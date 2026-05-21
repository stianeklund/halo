#!/usr/bin/env python3
"""Fast Unicorn regression test runner for high-risk ported functions.

Runs unicorn_diff on a curated list of targets from regression_targets.json.
Designed to be fast enough for pre-commit / CI use.

Usage:
    python3 tools/equivalence/regression_test.py          # run all targets
    python3 tools/equivalence/regression_test.py --quick   # fewer seeds (5)
    python3 tools/equivalence/regression_test.py --dry-run  # list targets only
    python3 tools/equivalence/regression_test.py --target FUN_000b4da0  # one target
"""

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TARGETS_FILE = Path(__file__).resolve().parent / "regression_targets.json"
UNICORN_DIFF = ROOT / "tools" / "equivalence" / "unicorn_diff.py"


def load_targets(filter_name=None):
    data = json.loads(TARGETS_FILE.read_text(encoding="utf-8"))
    targets = data.get("targets", [])
    if filter_name:
        targets = [t for t in targets if t["name"] == filter_name or t["addr"] == filter_name]
    return targets


def check_prerequisites(target):
    delinked_dir = ROOT / "delinked"
    obj_path = delinked_dir / target["obj"]
    if obj_path.exists():
        return None
    addr = target.get("addr", "").replace("0x", "")
    for d in delinked_dir.glob("*.obj"):
        if addr and addr in d.stem:
            return None
    return f"missing delinked oracle for {target['name']} (checked {target['obj']} and *{addr}*.obj)"


def run_target(target, seed_override=None):
    name = target["name"]
    seeds = seed_override or target.get("seeds", 20)
    flags = target.get("flags", [])

    venv_python = ROOT / ".venv" / "bin" / "python3"
    if not venv_python.exists():
        git_common = Path(subprocess.run(
            ["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
            capture_output=True, text=True, cwd=str(ROOT)
        ).stdout.strip()).parent / ".venv" / "bin" / "python3"
        if git_common.exists():
            venv_python = git_common
    python = str(venv_python) if venv_python.exists() else sys.executable

    cmd = [
        python, str(UNICORN_DIFF),
        name,
        "--seeds", str(seeds),
    ] + flags

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=120,
            cwd=str(ROOT),
        )
        output = result.stdout + result.stderr

        if "RESULTS:" in output:
            for line in output.splitlines():
                if "RESULTS:" in line:
                    if "0 failed" in line and "0 errors" in line:
                        return "pass", line.strip()
                    else:
                        return "fail", line.strip()

        if result.returncode != 0:
            last_lines = output.strip().splitlines()[-3:]
            return "error", "; ".join(last_lines)

        return "error", "no RESULTS line in output"

    except subprocess.TimeoutExpired:
        return "error", "timeout (120s)"


def main():
    parser = argparse.ArgumentParser(description="Unicorn regression test runner")
    parser.add_argument("--quick", action="store_true", help="Use 5 seeds per target")
    parser.add_argument("--dry-run", action="store_true", help="List targets only")
    parser.add_argument("--target", help="Run a single target by name or address")
    args = parser.parse_args()

    targets = load_targets(args.target)
    if not targets:
        print("No targets found.")
        return 1

    if args.dry_run:
        print(f"{'Addr':<12} {'Name':<20} {'Obj':<20} {'Seeds':<6} Reason")
        print("-" * 80)
        for t in targets:
            skip = check_prerequisites(t)
            status = f"SKIP: {skip}" if skip else t.get("reason", "")
            print(f"{t['addr']:<12} {t['name']:<20} {t['obj']:<20} {t.get('seeds', 20):<6} {status}")
        return 0

    seed_override = 5 if args.quick else None
    passed = failed = skipped = errors = 0
    t0 = time.time()

    print(f"Running {len(targets)} regression target(s)...")
    print()

    for t in targets:
        skip = check_prerequisites(t)
        if skip:
            print(f"  SKIP  {t['name']:<24} {skip}")
            skipped += 1
            continue

        status, detail = run_target(t, seed_override)
        seeds_used = seed_override or t.get("seeds", 20)

        if status == "pass":
            print(f"  PASS  {t['name']:<24} {seeds_used} seeds — {detail}")
            passed += 1
        elif status == "fail":
            print(f"  FAIL  {t['name']:<24} {seeds_used} seeds — {detail}")
            failed += 1
        else:
            print(f"  ERR   {t['name']:<24} {detail}")
            errors += 1

    elapsed = time.time() - t0
    print()
    print(f"Done in {elapsed:.1f}s: {passed} passed, {failed} failed, {errors} errors, {skipped} skipped")

    return 1 if (failed > 0 or errors > 0) else 0


if __name__ == "__main__":
    sys.exit(main())
