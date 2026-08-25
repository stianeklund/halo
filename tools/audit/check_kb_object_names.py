#!/usr/bin/env python3
"""
check_kb_object_names.py — kb.json object names must stay unique.

Nothing else in the repo enforces this, and a duplicate is silently
destructive rather than loudly wrong:

  * tools/audit/batch_delink.py:683 derives the export path from the object
    name (`out_dir / name.replace(":", "/")`).  Two objects sharing a name
    write the SAME file.  Objects export in kb array order, so whichever is
    later wins and the other's delinked reference is overwritten -- while
    objdiff.json still points at the path, now holding the wrong object's
    code.  A 2-function object can replace a 46-function reference.
  * tools/analysis/fun_pipeline.py:96 and tools/audit/batch_delink.py:444
    build `{obj["name"]: obj}` maps: last wins, so function moves land in the
    wrong object and are written back to kb.json with no error.
  * tools/analysis/classify_common.py:163,166 key addr_to_obj and
    obj_addr_ranges by name, so one object's address range disappears from
    the <common> classification heuristics.
  * tools/audit/audit_object_provenance.py:56 returns the FIRST name match,
    so an audit silently reports on the wrong object's functions.

Two duplicates predate this check and are allowlisted below.  The gate exists
to stop NEW ones: the obvious way to act on a proven misattribution is to
rename the object to the TU it really came from, and that target name usually
already exists -- which is exactly how the delinked reference gets eaten.
The remedy for those is a merge, not a rename.

Usage:
  python3 tools/audit/check_kb_object_names.py            # report
  python3 tools/audit/check_kb_object_names.py --check    # exit 1 on new dups
"""

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
KB_PATH = REPO_ROOT / "kb.json"

# Duplicates present before this gate existed.  Do not extend without a reason:
# every entry here is an object whose delinked export races another's.
ALLOWLIST = {
    "<xdk_stubs>",
    "scenario.obj",
}


def duplicates(kb_path: Path):
    kb = json.loads(kb_path.read_text(encoding="utf-8"))
    names = [o.get("name") for o in kb.get("objects", []) if o.get("name")]
    counts = Counter(names)
    where = defaultdict(list)
    for i, obj in enumerate(kb.get("objects", [])):
        nm = obj.get("name")
        if nm and counts[nm] > 1:
            where[nm].append((i, len(obj.get("functions") or []),
                              obj.get("source")))
    return where


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if a non-allowlisted duplicate exists")
    ap.add_argument("--kb", default=str(KB_PATH))
    args = ap.parse_args()

    dups = duplicates(Path(args.kb))
    new = {n: v for n, v in dups.items() if n not in ALLOWLIST}

    for name, entries in sorted(dups.items()):
        tag = "allowlisted" if name in ALLOWLIST else "NEW"
        print("%-16s %s  (%d objects)" % (tag, name, len(entries)))
        for idx, nfuncs, source in entries:
            print("      objects[%d]  %3d functions  source=%s"
                  % (idx, nfuncs, source))

    if not dups:
        print("kb.json object names are unique.")

    if new:
        print()
        print("ERROR: %d new duplicate object name(s)." % len(new))
        print("batch_delink.py writes both to one path; the later one in the kb")
        print("array overwrites the other's delinked reference while objdiff")
        print("still points at it.  If you meant to relabel an object whose")
        print("proven TU already exists, MERGE the two objects instead of")
        print("renaming into a collision.")
        if args.check:
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
