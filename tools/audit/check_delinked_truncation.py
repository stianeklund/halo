#!/usr/bin/env python3
"""Detect truncated per-function delinked references.

Compares what compare_obj.first_function_insns returns against the
padding-stripped instruction count bounded by FUN_ symbols in the .obj.
A significant discrepancy means vc71_verify is scoring against a fraction
of the function (the first-RET truncation bug from lift-learnings §20).

The key distinction from counting ALL .obj instructions: per-function exports
often include neighbor functions. We only count instructions between the
target FUN_ symbol and the next FUN_ symbol, stripped of trailing NOP padding.

Usage:
    python3 tools/audit/check_delinked_truncation.py [--threshold 0.90] [--file <addr>.obj]
    python3 tools/audit/check_delinked_truncation.py --json
"""
import argparse
import json
import os
import re
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, "tools", "verify"))
import compare_obj

DELINKED_DIR = os.path.join(REPO_ROOT, "delinked", "functions")

_NOP_MNEMS = {"nop", "nopw", "nopl", "nopq", "int3"}


def count_bounded_insns(obj_path, target_aliases):
    """Count instructions from the target FUN_ symbol to the next FUN_ symbol,
    stripping trailing NOP/INT3 padding. This is the ground truth for how many
    instructions the function actually has."""
    result = subprocess.run(
        ["objdump", "-d", obj_path],
        capture_output=True, text=True, timeout=10
    )
    if result.returncode != 0:
        return None

    in_target = False
    insns = []
    for line in result.stdout.splitlines():
        m = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
        if m:
            sym = m.group(1)
            if sym in target_aliases:
                in_target = True
                continue
            elif in_target and sym.startswith("FUN_"):
                break
            continue
        if in_target and re.match(r"\s+[0-9a-f]+:", line):
            parts = line.strip().split(None)
            mnem = ""
            for p in parts:
                if not re.match(r"^[0-9a-f]+:?$", p):
                    mnem = p.lower()
                    break
            insns.append(mnem)

    while insns and insns[-1] in _NOP_MNEMS:
        insns.pop()
    return len(insns) if insns else None


def check_one(obj_path, threshold=0.90):
    fname = os.path.basename(obj_path)
    addr = fname.replace(".obj", "")
    aliases = {f"FUN_{addr}", f"FUN_00{addr}"}

    bounded = count_bounded_insns(obj_path, aliases)
    if bounded is None or bounded == 0:
        return {"addr": addr, "status": "no_symbol", "bounded": 0, "parsed": 0}

    insns = compare_obj.first_function_insns(obj_path, aliases)
    parsed = len(insns) if insns else 0

    if parsed == 0:
        return {"addr": addr, "status": "parse_failed", "bounded": bounded, "parsed": 0}

    ratio = parsed / bounded
    status = "ok" if ratio >= threshold else "truncated"
    return {
        "addr": addr,
        "status": status,
        "bounded": bounded,
        "parsed": parsed,
        "ratio": round(ratio, 3),
        "lost": bounded - parsed,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--threshold", type=float, default=0.90,
                        help="Ratio below which a ref is flagged (default: 0.90)")
    parser.add_argument("--file", help="Check a single .obj file instead of all")
    parser.add_argument("--json", action="store_true", help="Machine-readable output")
    parser.add_argument("--truncated-only", action="store_true",
                        help="Only show truncated refs")
    args = parser.parse_args()

    if args.file:
        path = args.file if os.path.isabs(args.file) else os.path.join(DELINKED_DIR, args.file)
        results = [check_one(path, args.threshold)]
    else:
        files = sorted(f for f in os.listdir(DELINKED_DIR) if f.endswith(".obj"))
        results = []
        for i, fname in enumerate(files):
            if (i + 1) % 100 == 0 and not args.json:
                print(f"  [{i+1}/{len(files)}]...", file=sys.stderr, flush=True)
            results.append(check_one(os.path.join(DELINKED_DIR, fname), args.threshold))

    truncated = [r for r in results if r["status"] == "truncated"]
    no_symbol = [r for r in results if r["status"] == "no_symbol"]
    parse_failed = [r for r in results if r["status"] == "parse_failed"]
    ok = [r for r in results if r["status"] == "ok"]

    if args.json:
        out = {"total_checked": len(results), "ok": len(ok),
               "truncated": len(truncated), "no_symbol": len(no_symbol),
               "parse_failed": len(parse_failed),
               "flagged": sorted(truncated, key=lambda x: x.get("ratio", 1))}
        print(json.dumps(out, indent=2))
        return

    if args.truncated_only:
        results = truncated

    print(f"Checked {len(results)} per-function delinked refs")
    print(f"  OK: {len(ok)}  |  Truncated: {len(truncated)}  "
          f"|  No symbol: {len(no_symbol)}  |  Parse failed: {len(parse_failed)}")

    if truncated:
        print(f"\nTRUNCATED (first_function_insns < {args.threshold:.0%} of bounded count):")
        for r in sorted(truncated, key=lambda x: x.get("ratio", 1)):
            print(f"  {r['addr']}: {r['parsed']}/{r['bounded']} insns "
                  f"({r['ratio']:.1%}, lost {r['lost']})")

    sys.exit(1 if truncated else 0)


if __name__ == "__main__":
    main()
