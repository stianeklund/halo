#!/usr/bin/env python3
"""
vc71_regression.py — Track VC71 match scores and detect regressions.

STATUS: Active.  Generates and updates tools/verify/vc71_scores.json which is
committed to the repo.  The scores file is consumed by tools/analysis/frontier.py,
tools/llm_auto_lift.py (liftability scoring), and tools/equivalence/batch_equivalence.py
(priority queue).  No auto-callers; run manually after bulk lifts.

Manages tools/verify/vc71_scores.json (committed to repo), which records the
expected minimum VC71 match percentage for each ported function. Used to catch
edits that silently degrade byte-match quality.

Commands:
    update --source src/halo/game/game.c [...]
        Run vc71_verify on the given source file(s), update the stored
        floor scores. Raises stored score when a function improves; never
        lowers it automatically (use --force to lower).

    check [--source src/...] [--threshold N]
        Run vc71_verify and compare against stored floors.
        Exits 1 if any function dropped more than --threshold pp (default 2).
        Limits to specific files when --source is given.

    show
        Print the current baseline in a human-readable table.

    populate
        Re-derive scores for every ported function in kb.json by scanning
        all relevant source files (slow, but useful for initial population
        or after bulk changes). Writes new floors for any function not yet
        in the baseline.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
BASELINE_PATH = Path(__file__).parent / "vc71_scores.json"
VC71_VERIFY = REPO_ROOT / "tools" / "verify" / "vc71_verify.py"

_LINE_RE = re.compile(
    r"(?:PASS|FAIL)\s+(\S+):\s+([\d.]+)%\s+match\s+\((\d+)/(\d+)\s+insns\)"
)


# ---------------------------------------------------------------------------
# Baseline I/O
# ---------------------------------------------------------------------------

def load_baseline() -> dict[str, dict]:
    """Return {fn_name: {"score": float, "source": str}} from the baseline file."""
    if not BASELINE_PATH.exists():
        return {}
    try:
        data = json.loads(BASELINE_PATH.read_text())
        return data.get("scores", {})
    except (json.JSONDecodeError, OSError):
        return {}


def save_baseline(scores: dict[str, dict]) -> None:
    data = {"version": 1, "scores": scores}
    BASELINE_PATH.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")


# ---------------------------------------------------------------------------
# vc71_verify runner
# ---------------------------------------------------------------------------

def run_vc71_verify(source: Path, no_cache: bool = True) -> dict[str, dict]:
    """Run vc71_verify on a source file; return {fn_name: {score, n_c, n_r}}.

    Defaults to --no-cache so stale .obj files from previous compilations do
    not mask source changes: vc71_verify's fast-path will read a stale .obj
    when the source hash misses the SQLite cache, producing phantom cache hits.
    The compile step is fast (~0.1s/file) so the accuracy cost is acceptable.
    """
    cmd = [sys.executable, str(VC71_VERIFY), str(source), "--quiet"]
    if no_cache:
        cmd.append("--no-cache")
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT)
    out = {}
    for line in (result.stdout + result.stderr).splitlines():
        m = _LINE_RE.search(line)
        if m:
            out[m.group(1)] = {
                "score": float(m.group(2)),
                "n_c": int(m.group(3)),
                "n_r": int(m.group(4)),
            }
    return out


# ---------------------------------------------------------------------------
# update command
# ---------------------------------------------------------------------------

def cmd_update(args) -> int:
    if not args.source:
        print("Error: --source is required for update.", file=sys.stderr)
        return 1

    baseline = load_baseline()
    total_changed = 0

    for src_str in args.source:
        src = Path(src_str)
        if not src.is_absolute():
            src = REPO_ROOT / src
        if not src.exists():
            print(f"  SKIP {src_str} (not found)", file=sys.stderr)
            continue

        print(f"Verifying {src.relative_to(REPO_ROOT)} ...", flush=True)
        results = run_vc71_verify(src)
        src_rel = str(src.relative_to(REPO_ROOT))

        for fn_name, info in sorted(results.items()):
            new_score = info["score"]
            old_entry = baseline.get(fn_name)
            old_score = old_entry["score"] if old_entry else None

            if old_score is None:
                baseline[fn_name] = {"score": new_score, "source": src_rel}
                print(f"  + {fn_name}: {new_score:.1f}% (new)")
                total_changed += 1
            elif new_score > old_score + 0.1:
                # Improvement: always raise the floor
                baseline[fn_name] = {"score": new_score, "source": src_rel}
                print(f"  ↑ {fn_name}: {old_score:.1f}% → {new_score:.1f}%")
                total_changed += 1
            elif new_score < old_score - 0.1:
                if args.force:
                    baseline[fn_name] = {"score": new_score, "source": src_rel}
                    print(f"  ↓ {fn_name}: {old_score:.1f}% → {new_score:.1f}% (forced lower)")
                    total_changed += 1
                else:
                    print(f"  ! {fn_name}: {old_score:.1f}% → {new_score:.1f}%"
                          f" (drop; use --force to lower floor)")
            # else: unchanged within tolerance, nothing to do

    save_baseline(baseline)
    if total_changed:
        print(f"\nBaseline updated: {total_changed} function(s) changed → {BASELINE_PATH.name}")
    else:
        print("\nBaseline unchanged.")
    return 0


# ---------------------------------------------------------------------------
# check command
# ---------------------------------------------------------------------------

def cmd_check(args) -> int:
    threshold = args.threshold
    sources_filter = set(args.source) if args.source else None

    baseline = load_baseline()
    if not baseline:
        print("Baseline is empty. Run 'update' first.")
        return 0

    # Group baseline entries by source file
    by_source: dict[str, list[str]] = {}
    for fn_name, entry in baseline.items():
        src = entry.get("source", "")
        if sources_filter:
            # Accept both relative and absolute forms in the filter
            if src not in sources_filter and str(REPO_ROOT / src) not in sources_filter:
                continue
        by_source.setdefault(src, []).append(fn_name)

    if not by_source:
        print("No baseline entries match the given --source filter.")
        return 0

    regressions = []
    improvements = []
    skipped = 0

    for src_rel, fn_names in sorted(by_source.items()):
        src_path = REPO_ROOT / src_rel
        if not src_path.exists():
            skipped += len(fn_names)
            if not args.quiet:
                print(f"  SKIP {src_rel} (file not found)")
            continue

        results = run_vc71_verify(src_path)

        for fn_name in fn_names:
            current = results.get(fn_name)
            if current is None:
                # Function not found in compiled output — may be unported or renamed
                continue
            baseline_score = baseline[fn_name]["score"]
            current_score = current["score"]
            delta = current_score - baseline_score
            if delta < -threshold:
                regressions.append((fn_name, baseline_score, current_score, src_rel))
            elif delta > threshold and not args.quiet:
                improvements.append((fn_name, baseline_score, current_score, src_rel))

    if improvements:
        print(f"Improvements ({len(improvements)}):")
        for fn, base, curr, src in improvements:
            print(f"  ↑ {fn}: {base:.1f}% → {curr:.1f}% in {src}")
        print()

    if regressions:
        print(f"REGRESSIONS ({len(regressions)}):")
        for fn, base, curr, src in regressions:
            drop = base - curr
            print(f"  ✗ {fn}: {base:.1f}% → {curr:.1f}% (-{drop:.1f}pp) in {src}")
        print(
            "\nHint: investigate the change, fix the regression, then re-run:\n"
            "  python3 tools/verify/vc71_regression.py update --source <file>"
        )
        return 1

    checked = sum(len(v) for v in by_source.values()) - skipped
    print(f"OK — no regressions ({checked} functions in {len(by_source)} source file(s)).")
    return 0


# ---------------------------------------------------------------------------
# show command
# ---------------------------------------------------------------------------

def cmd_show(args) -> int:
    baseline = load_baseline()
    if not baseline:
        print("Baseline is empty.")
        return 0

    by_source: dict[str, list] = {}
    for fn_name, entry in baseline.items():
        by_source.setdefault(entry.get("source", "?"), []).append(
            (fn_name, entry["score"])
        )

    total = len(baseline)
    perfect = 0
    above90 = 0

    for src in sorted(by_source):
        fns = sorted(by_source[src], key=lambda x: x[1])
        print(f"\n{src}:")
        for fn_name, score in fns:
            marker = " ✓" if score >= 99.9 else ""
            print(f"  {fn_name:<44s} {score:6.1f}%{marker}")
            if score >= 99.9:
                perfect += 1
            if score >= 90.0:
                above90 += 1

    print(
        f"\nTotal: {total} functions | "
        f"{perfect} at 100% | {above90} at ≥90%"
    )
    return 0


# ---------------------------------------------------------------------------
# populate command
# ---------------------------------------------------------------------------

def cmd_populate(args) -> int:
    """Scan objdiff.json to find all source files with ported functions and update."""
    objdiff = REPO_ROOT / "objdiff.json"
    if not objdiff.exists():
        print("objdiff.json not found.", file=sys.stderr)
        return 1

    data = json.loads(objdiff.read_text())
    units = data.get("units", [])

    # Collect source files that have a delinked reference
    src_files = []
    for unit in units:
        src = unit.get("metadata", {}).get("source_path", "")
        ref = unit.get("base_path", "")
        if src and ref and (REPO_ROOT / ref).exists():
            full = REPO_ROOT / src
            if full.exists() and str(full) not in src_files:
                src_files.append(str(full))

    if not src_files:
        print("No source files with delinked references found in objdiff.json.")
        return 1

    print(f"Populating baseline from {len(src_files)} source files...")
    # Reuse cmd_update with a fake args object
    class _Args:
        source = src_files
        force = False
    return cmd_update(_Args())


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = ap.add_subparsers(dest="command")

    p_update = sub.add_parser("update", help="Update baseline for source file(s)")
    p_update.add_argument("--source", "-s", nargs="+",
                          help="Source .c file(s) to run vc71_verify on")
    p_update.add_argument("--force", action="store_true",
                          help="Allow lowering stored floors (normally disallowed)")

    p_check = sub.add_parser("check", help="Check for regressions against baseline")
    p_check.add_argument("--source", "-s", nargs="+",
                         help="Limit check to these source files")
    p_check.add_argument("--threshold", "-t", type=float, default=2.0,
                         help="Acceptable drop in pp before flagging (default: 2.0)")
    p_check.add_argument("--quiet", "-q", action="store_true",
                         help="Suppress improvement messages")

    sub.add_parser("show", help="Display current baseline")

    sub.add_parser("populate", help="Populate baseline for all objdiff.json source files")

    args = ap.parse_args()

    if args.command == "update":
        sys.exit(cmd_update(args))
    elif args.command == "check":
        sys.exit(cmd_check(args))
    elif args.command == "show":
        sys.exit(cmd_show(args))
    elif args.command == "populate":
        sys.exit(cmd_populate(args))
    else:
        ap.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
