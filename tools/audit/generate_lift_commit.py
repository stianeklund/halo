#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""
generate_lift_commit.py — produce a standardized commit message for lift batches.

Usage:
    python3 tools/generate_lift_commit.py [--batch-name <name>]

Run after `git add` but before `git commit`. Reads staged changes in:
  - src/**/*.c      (to find newly ported functions)
  - kb.json         (to find renames/address changes)
  - kb_meta.json    (to count metadata updates)

Auto-stages cross-TU call-site fixups: when a lift renames a callee, callers in
OTHER TUs must adopt the new name in the same commit or HEAD fails to build.
The lift step stages only the primary TU + kb.json, so those fixup edits are
left unstaged; this tool detects the rename and stages the pending .c edits that
adopt the new name (and warns on any dangling caller with no pending fix).

Gates on ABI audit: refuses to generate a commit message if any newly ported
function with register args fails audit_reg_abi.py.

Outputs a commit message to stdout. Pipe it:
    python3 tools/generate_lift_commit.py > /tmp/commit_msg.txt
    git commit -F /tmp/commit_msg.txt
"""

import argparse
import json
import logging
import os
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
TOOLS = REPO_ROOT / "tools"
sys.path.insert(0, str(TOOLS))


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT).stdout


def _load_kb_reg_args():
    """Return {addr: [(param_index, reg), ...]} for functions with register args."""
    from analysis.knowledge import KnowledgeBase, Function
    if not logging.root.handlers:
        logging.basicConfig(level=logging.ERROR)
    kb = KnowledgeBase.deserialize()
    result = {}
    for sym in kb.symbols:
        if not isinstance(sym, Function):
            continue
        if sym.register_args and sym.addr is not None:
            addr = hex(int(str(sym.addr), 0)).lower()
            result[addr] = sym.register_args
    return result


def run_abi_audit(function_addrs):
    """Run audit_reg_abi.py on each address. Returns (pass_count, fail_count, messages)."""
    passes = 0
    failures = 0
    messages = []
    reg_map = _load_kb_reg_args()

    for addr in function_addrs:
        norm = hex(int(str(addr), 0)).lower()
        if norm not in reg_map:
            passes += 1
            continue
        proc = subprocess.run(
            [sys.executable, str(TOOLS / "audit" / "audit_reg_abi.py"), "--target", norm],
            capture_output=True, text=True, cwd=str(REPO_ROOT),
        )
        if proc.returncode != 0:
            failures += 1
            messages.append(f"ABI FAIL {norm}: {proc.stderr.strip() or proc.stdout.strip()}")
        else:
            passes += 1
            stdout = proc.stdout.strip()
            if "warning:" in stdout.lower():
                messages.append(f"ABI WARN {norm}: {stdout}")

    return passes, failures, messages


def kb_summary():
    """Return (ported, total, percent) from kb_meta.py summary."""
    out = run([sys.executable, "tools/analysis/kb_meta.py", "summary"])
    ported = total = 0
    for line in out.splitlines():
        if line.startswith("ported symbols:"):
            ported = int(line.split(":")[1].strip())
        if line.startswith("total symbols:"):
            total = int(line.split(":")[1].strip())
    pct = (ported / total * 100) if total else 0.0
    return ported, total, pct


def previous_kb_summary():
    """Return (ported, total, percent) from HEAD~1 kb_meta.json."""
    try:
        prev = run(["git", "show", "HEAD~1:kb_meta.json"])
        data = json.loads(prev)
        syms = data.get("symbols", {})
        ported = sum(
            1 for v in syms.values()
            if isinstance(v, dict) and v.get("status") == "ported"
        )
        total = len(syms)
        pct = (ported / total * 100) if total else 0.0
        return ported, total, pct
    except Exception:
        return None, None, None


def _load_kb_functions(ref=None):
    """Load {addr: (decl, ported)} from kb.json at a given git ref (or working tree)."""
    if ref:
        raw = run(["git", "show", f"{ref}:kb.json"])
    else:
        raw = (REPO_ROOT / "kb.json").read_text()
    data = json.loads(raw)
    funcs = {}
    for obj in data.get("objects", []):
        for fn in obj.get("functions", []):
            addr = fn.get("addr", "")
            decl = fn.get("decl", "")
            if addr and decl:
                funcs[addr] = (decl.replace(";", "").strip(), bool(fn.get("ported")))
    return funcs


def compare_kb_json(old_ref=None, new_ref=None):
    """Compare two versions of kb.json and return (ports, renames).

    A "port" is either a brand-new kb entry OR an existing entry whose
    `ported` flag flipped to true — the latter is how every modern lift
    registers (the address was catalogued long before it was lifted).
    """
    old_funcs = _load_kb_functions(old_ref)
    new_funcs = _load_kb_functions(new_ref)

    ports = []
    renames = []

    for addr, (new_decl, new_ported) in new_funcs.items():
        if addr not in old_funcs:
            ports.append((addr, new_decl))
            continue
        old_decl, old_ported = old_funcs[addr]
        if new_ported and not old_ported:
            # The lift itself: any accompanying decl fix is part of the port,
            # not a separate rename.
            ports.append((addr, new_decl))
        elif old_decl != new_decl:
            renames.append((addr, old_decl, new_decl))

    return ports, renames


def staged_kb_json_changes():
    """Parse staged kb.json changes against HEAD."""
    # Check if kb.json is staged
    staged = run(["git", "diff", "--cached", "--name-only"])
    if "kb.json" not in staged:
        return [], []
    return compare_kb_json(old_ref="HEAD")


_obj_cache = {}

def object_for_addr(addr):
    """Look up object name from kb_meta.py list output."""
    if not _obj_cache:
        out = run([sys.executable, "tools/analysis/kb_meta.py", "list"])
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 5:
                # Store both object (parts[3]) and source if we can find it
                # For now just match the existing return behavior but cached
                _obj_cache[parts[0]] = parts[3]
    return _obj_cache.get(addr, "<common>")


def kb_meta_change_count():
    """Count how many symbol blocks changed in staged kb_meta.json."""
    diff = run(["git", "diff", "--cached", "kb_meta.json"])
    return len(re.findall(r'^\+\s*"0x[0-9a-fA-F]+":\s*\{', diff, re.MULTILINE))


def source_files_changed(since_ref=None):
    """List changed .c files — staged by default, or since a ref."""
    if since_ref:
        diff = run(["git", "diff", "--name-only", since_ref, "HEAD"])
    else:
        diff = run(["git", "diff", "--cached", "--name-only"])
    return [l for l in diff.splitlines() if l.endswith(".c")]


_VC71_LINE_RE = re.compile(
    r"(?:PASS|FAIL)\s+(\S+):\s+([\d.]+)%\s+match\s+\((\d+)/(\d+)\s+insns\)"
)


def _port_fn_names(ports) -> list:
    """Candidate symbol names for each ported function: decl name + FUN_<addr>."""
    names = []
    for addr, decl in ports:
        cand = []
        m = re.search(r"\b(\w+)\s*\(", decl or "")
        if m:
            cand.append(m.group(1))
        try:
            cand.append(f"FUN_{int(addr, 16):08x}")
        except (ValueError, TypeError):
            pass
        if cand:
            names.append(cand)
    return names


def _run_vc71_on_staged_sources(ports=None, since_ref=None) -> dict:
    """Run vc71_verify on staged .c files; return {fn_name: {score, n_c, n_r}}.

    When the staged ports are known, verify each ported function individually
    (--function): far faster than a whole-file pass on a large TU and it reuses
    the lift pipeline's cache entry for that exact function. Falls back to a
    whole-file pass when no ports are given.

    Uses the existing vc71 cache — changed files will miss and recompute
    automatically since the cache key includes the source hash.
    """
    scores: dict = {}
    fn_names = sorted({n for cands in _port_fn_names(ports or []) for n in cands})
    for src in source_files_changed(since_ref):
        src_path = REPO_ROOT / src
        if not src_path.exists():
            continue
        runs = [["--function", fn] for fn in fn_names] or [[]]
        for extra in runs:
            result = subprocess.run(
                [sys.executable,
                 str(REPO_ROOT / "tools" / "verify" / "vc71_verify.py"),
                 str(src_path), "--quiet", *extra],
                capture_output=True, text=True, cwd=REPO_ROOT,
            )
            for line in (result.stdout + result.stderr).splitlines():
                m = _VC71_LINE_RE.search(line)
                if m:
                    scores[m.group(1)] = {
                        "score": float(m.group(2)),
                        "n_c":   int(m.group(3)),
                        "n_r":   int(m.group(4)),
                    }
    return scores


def _lookup_vc71_score(addr: str, decl: str, scores: dict):
    """Return the vc71 score entry for a ported function, or None."""
    try:
        fun_key = f"FUN_{int(addr, 16):08x}"
        if fun_key in scores:
            return scores[fun_key]
    except (ValueError, TypeError):
        pass
    m = re.search(r"\b(\w+)\s*\(", decl)
    if m and m.group(1) in scores:
        return scores[m.group(1)]
    return None


_TIMESTAMP_RUN_RE = re.compile(r'^\d{8}-\d{6}$')
_EQUIV_RESULTS_RE = re.compile(
    r'RESULTS:\s*(\d+)\s+passed,\s*(\d+)\s+(?:failed|diverged),\s*(\d+)\s+errors\s*/\s*(\d+)\s+seeds'
)

def _find_latest_vc71_match(ports=None):
    """Find the VC71 match % from the most recent timestamped lift run summary.

    When staged ports are known, only accept a summary whose target_pick stage
    names one of them — otherwise a recent run for a DIFFERENT (e.g. reverted)
    candidate gets its score stamped onto this commit.
    """
    runs_dir = REPO_ROOT / "artifacts" / "lift_runs"
    if not runs_dir.exists():
        return None
    tokens = {n.lower() for cands in _port_fn_names(ports or []) for n in cands}
    tokens |= {str(addr).lower() for addr, _ in (ports or [])}
    summaries = sorted(
        (s for s in runs_dir.glob("*/summary.json")
         if _TIMESTAMP_RUN_RE.match(s.parent.name)),
        reverse=True,
    )
    for s in summaries[:10]:
        try:
            data = json.loads(s.read_text())
            if tokens:
                picked = " ".join(
                    stage.get("details", "") for stage in data.get("stages", [])
                    if stage.get("name") == "target_pick"
                ).lower()
                if not any(tok in picked for tok in tokens):
                    continue
            for stage in data.get("stages", []):
                if stage.get("name") == "vc71_verify" and stage.get("ok"):
                    m = re.search(r'([\d.]+)%', stage.get("details", ""))
                    if m:
                        return m.group(1)
        except (json.JSONDecodeError, KeyError):
            continue
    return None


def _find_latest_equivalence(ports=None):
    """Find passed/total seed counts from the most recent equivalence smoke log.

    When staged ports are known, only accept a log whose filename names one of
    them (logs are per-function, e.g. FUN_0018d360_smoke.log) — never attribute
    another candidate's equivalence run to this commit.
    """
    equiv_dir = REPO_ROOT / "artifacts" / "equivalence"
    if not equiv_dir.exists():
        return None
    tokens = {n.lower() for cands in _port_fn_names(ports or []) for n in cands}
    logs = sorted(equiv_dir.glob("*_smoke.log"), key=lambda p: p.stat().st_mtime, reverse=True)
    for log in logs[:5]:
        if tokens and not any(tok in log.stem.lower() for tok in tokens):
            continue
        try:
            text = log.read_text(errors="replace")
            m = _EQUIV_RESULTS_RE.search(text)
            if m:
                passed, failed, errors, seeds = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))
                if seeds > 0 and failed == 0 and errors == 0 and passed == seeds:
                    return f"{passed}/{seeds}"
        except OSError:
            continue
    return None


def _build_match_tag(vc71_match, equivalence):
    """Format the verification tag for the commit title."""
    parts = []
    if vc71_match:
        parts.append(f"{vc71_match}% VC71")
    if equivalence:
        parts.append(f"{equivalence} equiv")
    if not parts:
        return ""
    return " (" + ", ".join(parts) + ")"


def generate_message(batch_name=None, since_ref=None, vc71_match=None,
                     equivalence=None, ports_renames=None):
    if ports_renames is not None:
        ports, renames = ports_renames
    elif since_ref:
        ports, renames = compare_kb_json(old_ref=since_ref, new_ref="HEAD")
    else:
        ports, renames = staged_kb_json_changes()

    if since_ref:
        meta_diff = run(["git", "diff", since_ref, "HEAD", "--", "kb_meta.json"])
        meta_changes = len(re.findall(r'^\+\s*"0x[0-9a-fA-F]+":\s*\{', meta_diff, re.MULTILINE))
    else:
        meta_changes = kb_meta_change_count()
    ported_after, total_after, pct_after = kb_summary()
    prev = previous_kb_summary()

    # --- Fresh VC71 scores from staged source files -------------------------
    vc71_scores = _run_vc71_on_staged_sources(ports, since_ref)

    if vc71_match is None:
        if ports and vc71_scores:
            # Compute aggregate from the newly ported functions
            port_scores = [
                _lookup_vc71_score(addr, decl, vc71_scores)["score"]
                for addr, decl in ports
                if _lookup_vc71_score(addr, decl, vc71_scores) is not None
            ]
            if port_scores:
                avg = sum(port_scores) / len(port_scores)
                vc71_match = f"{avg:.1f}"
        if vc71_match is None:
            vc71_match = _find_latest_vc71_match(ports)  # fallback, port-scoped

    if equivalence is None:
        equivalence = _find_latest_equivalence(ports)

    match_tag = _build_match_tag(vc71_match, equivalence)

    # Identify objects affected by ports/renames to include in subject
    affected_addrs = [addr for addr, _ in ports] + [addr for addr, _, _ in renames]
    objs = sorted(list(set(object_for_addr(addr) for addr in affected_addrs)))
    obj_tag = ""
    if objs:
        if len(objs) == 1:
            obj_tag = f" ({objs[0]})"
        else:
            obj_tag = f" ({len(objs)} objects)"

    lines = []
    if batch_name:
        lines.append(f"Port {batch_name}{obj_tag}{match_tag}")
    else:
        lines.append(f"Port functions{obj_tag}{match_tag}")
    lines.append("")

    if ports:
        lines.append("Functions ported:")
        for addr, decl in sorted(ports, key=lambda x: x[0]):
            name = decl.split("(")[0].split()[-1]
            obj = object_for_addr(addr)
            score_info = _lookup_vc71_score(addr, decl, vc71_scores)
            if score_info:
                score_tag = (
                    f" [{score_info['score']:.1f}% VC71,"
                    f" {score_info['n_c']}/{score_info['n_r']} insns]"
                )
            else:
                score_tag = ""
            lines.append(f"- {name} @ {addr} ({obj}){score_tag}")
        lines.append("")

    if renames:
        lines.append("Functions renamed:")
        for addr, old, new in sorted(renames, key=lambda x: x[0]):
            obj = object_for_addr(addr)
            old_name = old.split("(")[0].split()[-1]
            new_name = new.split("(")[0].split()[-1]
            lines.append(f"- {old_name} -> {new_name} @ {addr} ({obj})")
        lines.append("")

    if meta_changes:
        lines.append("kb_meta.json updates:")
        lines.append(f"- {meta_changes} symbol(s) updated")
        lines.append("")

    if prev[0] is not None and prev[1] == total_after:
        # Same total count, safe to show delta
        lines.append(
            f"Coverage: {prev[2]:.1f}% -> {pct_after:.1f}% "
            f"({ported_after}/{total_after} symbols)"
        )
    else:
        # Total changed (likely kb.json regeneration) — just show current
        lines.append(
            f"Coverage: {pct_after:.1f}% ({ported_after}/{total_after} symbols)"
        )

    # NOTE: the VC71 regression floor is refreshed exactly once per commit by the
    # pre-commit hook (tools/hooks/pre-commit-vc71-regression.sh), which also
    # re-stages vc71_scores.json. Message generation no longer runs `update` to
    # avoid a redundant second compile+diff pass on every commit.

    return "\n".join(lines)


def _decl_name(decl):
    """Extract the function name from a decl string."""
    m = re.search(r"\b(\w+)\s*\(", decl or "")
    return m.group(1) if m else None


def git_add(files):
    """Stage the given paths (bypasses the rtk shell hook). Returns staged list."""
    files = [f for f in files if f]
    if not files:
        return []
    subprocess.run(["git", "add", "--", *files],
                   capture_output=True, text=True, cwd=str(REPO_ROOT))
    return files


def callee_name_changes(ports, renames, old_ref="HEAD"):
    """[(old_name, new_name), ...] for every ported/renamed fn whose decl NAME
    changed vs old_ref.

    A name-changing lift is classified as a `port` by compare_kb_json (the decl
    fix is folded into the port, so it never reaches `renames`); the old name is
    recovered here from old_ref's kb.json.
    """
    old_funcs = _load_kb_functions(old_ref)
    changes = []
    for addr, new_decl in ports:
        old = old_funcs.get(addr)
        if not old:
            continue
        on, nn = _decl_name(old[0]), _decl_name(new_decl)
        if on and nn and on != nn:
            changes.append((on, nn))
    for addr, old_decl, new_decl in renames:
        on, nn = _decl_name(old_decl), _decl_name(new_decl)
        if on and nn and on != nn:
            changes.append((on, nn))
    return changes


def stage_cross_tu_callsites(name_changes):
    """Stage cross-TU call-site fixups when a lift renames a callee.

    When a lift renames a callee, its call sites in OTHER TUs must adopt the new
    name in the SAME commit — otherwise the callee's old name no longer resolves
    and HEAD fails to build. The lift step stages only the primary TU + kb.json,
    so those cross-TU fixup edits sit unstaged and are lost from the commit. The
    workflow's pre-commit build passes anyway because it validates the full
    working tree, not the staged subset — masking the break until the next
    checkout.

    Stage any unstaged working-tree .c edit that adopts a renamed callee's new
    name. Also report any committed .c file that still *calls* an old name with
    no pending fix (a genuine dangling caller — build will break, and there is
    no working-tree edit to stage).

    Returns (staged, dangling).
    """
    if not name_changes:
        return [], []
    unstaged = [l for l in run(["git", "diff", "--name-only", "--", "*.c"]).splitlines()
                if l.endswith(".c")]
    to_stage = set()
    dangling = set()
    for old_name, new_name in name_changes:
        new_re = re.compile(r"^\+.*(?<![\w])" + re.escape(new_name) + r"(?![\w])",
                            re.MULTILINE)
        for f in unstaged:
            if new_re.search(run(["git", "diff", "--", f])):
                to_stage.add(f)
        # Committed files still *calling* the old name with no pending edit.
        grep = run(["git", "grep", "-lE", re.escape(old_name) + r"\s*\(", "--", "*.c"])
        for f in grep.splitlines():
            if f and f not in unstaged and f not in to_stage:
                dangling.add((f, old_name, new_name))
    staged = git_add(sorted(to_stage))
    return staged, sorted(dangling)


def main():
    ap = argparse.ArgumentParser(description="Generate standardized lift commit message")
    ap.add_argument("--batch-name", default=None, help="Short batch description (e.g. 'weapon helpers')")
    ap.add_argument("--since", default=None, help="Git ref to diff against instead of staged changes")
    ap.add_argument("--skip-abi-audit", action="store_true",
                    help="Skip ABI audit gate (emergency bypass)")
    ap.add_argument("--vc71-match", default=None,
                    help="VC71 match %% to include in commit title (auto-detected from latest lift run if omitted)")
    ap.add_argument("--equivalence", default=None,
                    help="Equivalence result to include in commit title, e.g. '100/100' "
                         "(auto-detected from latest artifacts/equivalence/*_smoke.log if omitted)")
    args = ap.parse_args()

    if args.since:
        ports, renames = compare_kb_json(old_ref=args.since, new_ref="HEAD")
    else:
        ports, renames = staged_kb_json_changes()

    # Stage cross-TU call-site fixups for any renamed callee (staged-commit mode
    # only — --since builds a message over an already-committed range and has no
    # working-tree edits to stage). Runs BEFORE the ABI audit and message so the
    # commit reflects the full change and HEAD stays buildable.
    if not args.since:
        name_changes = callee_name_changes(ports, renames)
        staged, dangling = stage_cross_tu_callsites(name_changes)
        for f in staged:
            print(f"staged cross-TU call-site fixup: {f}", file=sys.stderr)
        for f, on, nn in dangling:
            print(
                f"WARNING: {f} still calls {on}( but the callee was renamed to "
                f"{nn} and no working-tree fix is pending — HEAD may not build.",
                file=sys.stderr,
            )

    if ports and not args.skip_abi_audit:
        port_addrs = [addr for addr, _ in ports]
        passes, failures, abi_messages = run_abi_audit(port_addrs)
        for msg in abi_messages:
            print(msg, file=sys.stderr)
        if failures > 0:
            print(
                f"\nABI audit FAILED for {failures} function(s). "
                f"Fix register annotations or pass --skip-abi-audit to bypass.",
                file=sys.stderr,
            )
            return 1
        if abi_messages:
            print(f"ABI audit: {passes} passed with warnings (see above)", file=sys.stderr)
        else:
            print(f"ABI audit: {passes} function(s) passed", file=sys.stderr)

    # Drift gate: kb.json @<reg> annotations vs tools/kb_reg_baseline.json.
    # Hard-fails if the parser detects drift, missing entries, or stale ones.
    # Use --skip-abi-audit to bypass in emergencies.
    if not args.skip_abi_audit:
        drift_proc = subprocess.run(
            [sys.executable, str(TOOLS / "audit" / "extract_reg_args.py"), "--check"],
            capture_output=True, text=True, cwd=str(REPO_ROOT),
        )
        if drift_proc.returncode != 0:
            print(drift_proc.stdout, file=sys.stderr)
            print(
                "\nkb_reg_baseline drift detected. "
                "Run `tools/audit/extract_reg_args.py --check` for the full "
                "list, or `--apply` to merge missing entries. "
                "Pass --skip-abi-audit to bypass.",
                file=sys.stderr,
            )
            return 1

    # Run sync-ported to catch any drift before committing
    sync_proc = subprocess.run(
        [sys.executable, str(TOOLS / "analysis" / "kb_meta.py"), "sync-ported", "--dry-run"],
        capture_output=True, text=True, cwd=str(REPO_ROOT),
    )
    if "would mark" in sync_proc.stdout:
        print("WARNING: kb_meta.json has untracked ported functions:", file=sys.stderr)
        print(sync_proc.stdout.strip(), file=sys.stderr)
        print(
            "Run: python3 tools/analysis/kb_meta.py sync-ported   (then stage kb_meta.json)",
            file=sys.stderr,
        )

    msg = generate_message(batch_name=args.batch_name, since_ref=args.since,
                           vc71_match=args.vc71_match,
                           equivalence=args.equivalence,
                           ports_renames=(ports, renames))
    print(msg)


if __name__ == "__main__":
    raise SystemExit(main())
