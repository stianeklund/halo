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


def previous_kb_summary_from_git():
    """Try to get summary from HEAD~1 via git show + python."""
    try:
        out = run(["git", "show", "HEAD~1:tools/analysis/kb_meta.py"])
        if not out:
            return None, None, None
        # Fallback: just return None and let caller handle it
        return None, None, None
    except Exception:
        return None, None, None


def _load_kb_functions(ref=None):
    """Load all function declarations from kb.json at a given git ref (or working tree)."""
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
                funcs[addr] = decl.replace(";", "").strip()
    return funcs


def compare_kb_json(old_ref=None, new_ref=None):
    """Compare two versions of kb.json and return (ports, renames)."""
    old_funcs = _load_kb_functions(old_ref)
    new_funcs = _load_kb_functions(new_ref)

    ports = []
    renames = []

    for addr, new_decl in new_funcs.items():
        if addr not in old_funcs:
            ports.append((addr, new_decl))
        elif old_funcs[addr] != new_decl:
            renames.append((addr, old_funcs[addr], new_decl))

    return ports, renames


def staged_kb_json_changes():
    """Parse staged kb.json changes against HEAD."""
    # Check if kb.json is staged
    staged = run(["git", "diff", "--cached", "--name-only"])
    if "kb.json" not in staged:
        return [], []
    return compare_kb_json(old_ref="HEAD")


def object_for_addr(addr):
    """Look up object name from kb_meta.py list output."""
    out = run([sys.executable, "tools/analysis/kb_meta.py", "list"])
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 5 and parts[0] == addr:
            return parts[3]
    return "<common>"


def kb_meta_change_count():
    """Count how many symbol blocks changed in staged kb_meta.json."""
    diff = run(["git", "diff", "--cached", "kb_meta.json"])
    return len(re.findall(r'^\+\s*"0x[0-9a-fA-F]+":\s*\{', diff, re.MULTILINE))


def source_files_changed():
    """List .c files changed in the stage."""
    diff = run(["git", "diff", "--cached", "--name-only"])
    return [l for l in diff.splitlines() if l.endswith(".c")]


def _find_latest_vc71_match():
    """Find the VC71 match % from the most recent lift run summary."""
    runs_dir = REPO_ROOT / "artifacts" / "lift_runs"
    if not runs_dir.exists():
        return None
    summaries = sorted(runs_dir.glob("*/summary.json"), reverse=True)
    for s in summaries[:3]:
        try:
            data = json.loads(s.read_text())
            for stage in data.get("stages", []):
                if stage.get("name") == "vc71_verify" and stage.get("ok"):
                    m = re.search(r'([\d.]+)%', stage.get("details", ""))
                    if m:
                        return m.group(1)
        except (json.JSONDecodeError, KeyError):
            continue
    return None


def generate_message(batch_name=None, since_ref=None, vc71_match=None):
    if since_ref:
        ports, renames = compare_kb_json(old_ref=since_ref, new_ref="HEAD")
        meta_diff = run(["git", "diff", since_ref, "HEAD", "--", "kb_meta.json"])
        meta_changes = len(re.findall(r'^\+\s*"0x[0-9a-fA-F]+":\s*\{', meta_diff, re.MULTILINE))
    else:
        ports, renames = staged_kb_json_changes()
        meta_changes = kb_meta_change_count()
    ported_after, total_after, pct_after = kb_summary()
    prev = previous_kb_summary()

    if vc71_match is None:
        vc71_match = _find_latest_vc71_match()

    match_tag = f" ({vc71_match}% VC71 match)" if vc71_match else ""
    lines = []
    if batch_name:
        lines.append(f"Port {batch_name}{match_tag}")
    else:
        lines.append(f"Port functions{match_tag}")
    lines.append("")

    if ports:
        lines.append("Functions ported:")
        for addr, decl in sorted(ports, key=lambda x: x[0]):
            name = decl.split("(")[0].split()[-1]
            obj = object_for_addr(addr)
            lines.append(f"- {name} @ {addr} ({obj})")
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

    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description="Generate standardized lift commit message")
    ap.add_argument("--batch-name", default=None, help="Short batch description (e.g. 'weapon helpers')")
    ap.add_argument("--since", default=None, help="Git ref to diff against instead of staged changes")
    ap.add_argument("--skip-abi-audit", action="store_true",
                    help="Skip ABI audit gate (emergency bypass)")
    ap.add_argument("--vc71-match", default=None,
                    help="VC71 match %% to include in commit title (auto-detected from latest lift run if omitted)")
    args = ap.parse_args()

    if args.since:
        ports, renames = compare_kb_json(old_ref=args.since, new_ref="HEAD")
    else:
        ports, renames = staged_kb_json_changes()

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
                           vc71_match=args.vc71_match)
    print(msg)


if __name__ == "__main__":
    raise SystemExit(main())
