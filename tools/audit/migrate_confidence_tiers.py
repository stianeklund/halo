#!/usr/bin/env python3
"""One-shot migration of leaf_cache.json confidence tiers (Phase 0.4).

Entries recorded below COVERAGE_FLOOR_PCT carry a confidence tier that no
evidence supports -- 129 of them sit at literally 0.0% coverage. Downstream
consumers (the selector's eq_high_conf boost, batch_verify summaries, the
dashboard) cannot distinguish those from measured verdicts.

This rewrites any sub-floor entry's `confidence` to "none", leaving
`coverage_pct` untouched so the history is still visible.

It writes leaf_cache.json DIRECTLY rather than going through
unicorn_diff._record_confidence, because that function is best-of/monotonic by
design (a re-measurement must never downgrade a recorded entry) and would
therefore refuse exactly the demotions this migration exists to perform.

Usage:
    python3 tools/audit/migrate_confidence_tiers.py           # report only
    python3 tools/audit/migrate_confidence_tiers.py --apply   # rewrite cache
"""

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LEAF_CACHE = ROOT / "tools" / "equivalence" / "leaf_cache.json"

sys.path.insert(0, str(ROOT / "tools" / "equivalence"))
try:
    from unicorn_diff import COVERAGE_FLOOR_PCT
except ImportError:          # unicorn not installed; the constant is all we need
    COVERAGE_FLOOR_PCT = 10.0


def histogram(cache: dict) -> Counter:
    return Counter(e.get("confidence") for e in cache.values()
                   if isinstance(e, dict) and "confidence" in e)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true",
                    help="write the migrated cache back to disk")
    args = ap.parse_args()

    try:
        cache = json.loads(LEAF_CACHE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        print(f"ERROR: cannot read {LEAF_CACHE}: {e}", file=sys.stderr)
        return 2

    before = histogram(cache)
    demoted = []
    for addr, entry in cache.items():
        if not isinstance(entry, dict):
            continue
        conf, cov = entry.get("confidence"), entry.get("coverage_pct")
        if conf in (None, "none") or not isinstance(cov, (int, float)):
            continue
        if cov < COVERAGE_FLOOR_PCT:
            demoted.append((addr, conf, cov))
            entry["confidence"] = "none"

    after = histogram(cache)

    print(f"Coverage floor: {COVERAGE_FLOOR_PCT:.1f}%\n")
    print("  tier      before   after")
    for tier in ("high", "moderate", "weak", "none"):
        print(f"  {tier:<9} {before.get(tier, 0):>6}  {after.get(tier, 0):>6}")
    print(f"\n  demoted to 'none': {len(demoted)}")

    zero = [d for d in demoted if d[2] == 0.0]
    if zero:
        print(f"    of which at exactly 0.0% coverage: {len(zero)}")
    by_tier = Counter(d[1] for d in demoted)
    for tier, n in sorted(by_tier.items()):
        print(f"    was '{tier}': {n}")

    if args.apply and demoted:
        LEAF_CACHE.write_text(
            json.dumps(dict(sorted(cache.items())), indent=2) + "\n",
            encoding="utf-8")
        print(f"\nWrote {LEAF_CACHE.relative_to(ROOT)}")
    elif demoted:
        print("\nRe-run with --apply to write the migration.")
    else:
        print("\nNothing to migrate.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
