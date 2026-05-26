#!/usr/bin/env python3
"""Print runtime oracle summary to stdout for GITHUB_STEP_SUMMARY."""
import json
import os
import sys
from pathlib import Path


def main() -> int:
    summary_path = os.environ.get("LATEST_SUMMARY")
    if not summary_path:
        return 0
    s = json.loads(Path(summary_path).read_text(encoding="utf-8"))
    r = s["results"]
    print("## Runtime Oracle Results")
    print()
    print("| Metric | Count |")
    print("|--------|-------|")
    print(f"| Total targets | {r['total']} |")
    print(f"| Pass | {r['pass']} |")
    print(f"| Fail | {r['fail']} |")
    print(f"| Error | {r['error']} |")
    for row in s.get("rows", []):
        status = row["status"].upper()
        detail = row.get("error", "") or row.get("reason", "")
        print(f"- **{status}** {row['target']} ({row.get('obj', '')}) {detail}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
