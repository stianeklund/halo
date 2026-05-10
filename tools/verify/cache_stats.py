#!/usr/bin/env python3
"""Print statistics about the vc71_verify per-function cache.

Usage:
    python3 tools/verify/cache_stats.py
    python3 tools/verify/cache_stats.py --purge   # drop all entries
"""
import sys
import os

_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import argparse
from pathlib import Path

from verify.vc71_cache import Vc71Cache, CACHE_DB


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--purge", action="store_true",
                    help="Drop all cache entries (cache file remains)")
    ap.add_argument("--db", default=str(CACHE_DB),
                    help="Path to cache database (default: %(default)s)")
    args = ap.parse_args()

    db_path = Path(args.db)
    cache = Vc71Cache(db_path)

    if not db_path.exists():
        print(f"Cache database not found: {db_path}")
        print("Run vc71_verify.py on a source file to populate it.")
        sys.exit(0)

    if args.purge:
        dropped = cache.invalidate()
        print(f"Purged {dropped} entries from {db_path}")
        sys.exit(0)

    stats = cache.stats()
    total = stats["total_entries"]

    print(f"VC71 verify cache: {db_path}")
    print(f"  Total entries : {total}")
    if total:
        print(f"  Oldest entry  : {stats['oldest']}")
        print(f"  Newest entry  : {stats['newest']}")
        print(f"  DB size       : {db_path.stat().st_size / 1024:.1f} KB")
        print()
        print("  Top source files by cached function count:")
        for src, cnt in stats["top_sources"]:
            short = src.replace(str(Path(src).parent.parent.parent), "")
            print(f"    {cnt:4d}  {short}")
    else:
        print("  (empty)")

    cache.close()


if __name__ == "__main__":
    main()
