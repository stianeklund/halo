#!/usr/bin/env python3
"""Run the standard verification chain from docs/testing_plan.md."""

from __future__ import annotations

import sys
import os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import argparse
import subprocess
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent.parent


def run(name: str, cmd: list[str], cwd: Path | None = None) -> tuple[int, str, str]:
    result = subprocess.run(cmd, cwd=cwd or ROOT_DIR,
                          capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


def run_step(name: str, cmd: list[str], required: bool = True) -> bool:
    print(f"\n{'='*60}")
    print(f"STEP: {name}")
    print(f"CMD:  {' '.join(cmd)}")
    print('='*60)
    rc, stdout, stderr = run(name, cmd)
    if stdout:
        print(stdout)
    if stderr and rc != 0:
        print(stderr, file=sys.stderr)
    if rc != 0 and required:
        print(f"FAILED (exit {rc})")
        return False
    print(f"PASSED (exit {rc})")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the standard verification chain from docs/testing_plan.md",
    )
    parser.add_argument("--skip-build", action="store_true",
                        help="skip verify_option3 build stages")
    parser.add_argument("--skip-iso", action="store_true",
                        help="skip verify_option3 iso stages")
    parser.add_argument("--limit", type=int, default=0,
                        help="limit batch_verify to N candidates (0=discover only)")
    parser.add_argument("--batch-csv", action="store_true",
                        help="batch_verify writes CSV output")
    args = parser.parse_args()

    steps = [
        ("Phase 1: sync_agent_content --check",
         ["python3", "tools/analysis/sync_agent_content.py", "--check"]),
        ("Phase 1: audit_agent_content --strict",
         ["python3", "tools/audit/audit_agent_content.py", "--strict"]),
        ("Phase 2: py_compile check",
         ["python3", "-m", "py_compile",
          "tools/verify/verify_option3.py",
          "tools/verify/test_inventory.py",
          "tools/equivalence/batch_verify.py",
          "tools/verify/run_golden_tests.py",
          "tools/build/patch.py"]),
        ("Phase 2: verify_option3 smoke",
         ["python3", "tools/verify/verify_option3.py",
          "--target", "smoke", "--skip-build", "--skip-iso"]),
        ("Phase 3: test_inventory",
         ["python3", "tools/verify/test_inventory.py", "--no-write"]),
        ("Phase 4: batch_verify dry-run",
         ["python3", "tools/equivalence/batch_verify.py", "--dry-run"]),
    ]

    failed = []
    for name, cmd in steps:
        if not run_step(name, cmd):
            failed.append(name)

    if args.limit > 0:
        batch_cmd = [
            "python3", "tools/equivalence/batch_verify.py",
            "--limit", str(args.limit),
        ]
        if args.batch_csv:
            batch_cmd.append("--csv")
        batch_cmd.extend(["--baseline", "artifacts/batch_verify/summary.json",
                          "--fail-on-new"])
        if not run_step(f"Phase 4: batch_verify --limit {args.limit}", batch_cmd):
            failed.append("Phase 4: batch_verify")

    print("\n" + "="*60)
    print("VERIFICATION CHAIN COMPLETE")
    print("="*60)
    if failed:
        print(f"FAILED STEPS: {len(failed)}")
        for f in failed:
            print(f"  - {f}")
        return 1
    print("All steps PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())