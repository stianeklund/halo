#!/usr/bin/env python3
"""Freeze the current batch_verify failure set as the --fail-on-new baseline.

The nightly gates on NEW divergences only: a failure already in this baseline
is known and does not fail the build. That makes refreshing it a deliberate,
reviewable act -- regenerating after a batch that introduced a regression
would silently bless it, which is the one thing this file must not do.

So: only refresh from a summary you have looked at, and review the diff. The
script prints what would be added and removed and refuses to write when the
additions look like a regression, unless you pass --accept-new.

Usage:
    python3 tools/equivalence/freeze_batch_baseline.py            # dry-run diff
    python3 tools/equivalence/freeze_batch_baseline.py --write    # no new fails
    python3 tools/equivalence/freeze_batch_baseline.py --write --accept-new
"""
import argparse
import datetime
import json
import os
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SUMMARY = ROOT / "artifacts" / "batch_verify" / "summary.json"
BASELINE = ROOT / "tools" / "equivalence" / "batch_verify_baseline.json"
FAIL_STATUSES = {"fail", "error"}


def _key(row):
    return f"{row['name']}|{row['status']}|{row.get('reason', '')}"


def _load_rows(path):
    if not path.exists():
        return []
    return json.loads(path.read_text()).get("rows", [])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--summary", type=Path, default=SUMMARY)
    ap.add_argument("--write", action="store_true", help="write the baseline")
    ap.add_argument("--accept-new", action="store_true",
                    help="write even when the summary adds failures")
    args = ap.parse_args()

    if not args.summary.exists():
        print(f"ERROR: no summary at {args.summary}", file=sys.stderr)
        return 2

    rows = [r for r in _load_rows(args.summary) if r.get("status") in FAIL_STATUSES]
    fails = [{"name": r["name"], "status": r["status"],
              "reason": r.get("reason", "")} for r in rows]

    old = {_key(r) for r in _load_rows(BASELINE)}
    new = {_key(r) for r in fails}
    added, removed = sorted(new - old), sorted(old - new)

    print(f"summary:  {args.summary}  ({len(fails)} failing rows)")
    print(f"baseline: {BASELINE}  ({len(old)} entries)")
    print(f"status histogram: {dict(Counter(r['status'] for r in fails))}")
    print(f"\n+{len(added)} added / -{len(removed)} removed")
    for k in added[:20]:
        print(f"  + {k}")
    if len(added) > 20:
        print(f"  ... and {len(added) - 20} more")
    for k in removed[:10]:
        print(f"  - {k}")
    if len(removed) > 10:
        print(f"  ... and {len(removed) - 10} more")

    if not args.write:
        print("\n(dry run — pass --write to update)")
        return 0

    if added and not args.accept_new:
        print(f"\nREFUSING to write: {len(added)} new failure(s) would be "
              f"blessed as known. Investigate them, or pass --accept-new if "
              f"they are genuinely expected.")
        return 1

    payload = {
        "_comment": (
            "Frozen baseline of KNOWN equivalence divergences for "
            "batch_verify.py --fail-on-new. Only failures ABSENT from this "
            "file (or whose status/reason string CHANGED) fail the nightly. "
            "Refresh with tools/equivalence/freeze_batch_baseline.py, and "
            "review the printed diff -- refreshing after a regression would "
            "silently bless it."
        ),
        "source_summary_mtime": datetime.datetime.fromtimestamp(
            os.path.getmtime(args.summary)).isoformat(),
        "rows": sorted(fails, key=lambda r: (r["name"], r["status"])),
    }
    BASELINE.write_text(json.dumps(payload, indent=2) + "\n")
    print(f"\nwritten: {BASELINE} ({len(fails)} entries)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
