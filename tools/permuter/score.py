#!/usr/bin/env python3
"""score.py — LCS-based score bridge between decomp-permuter and compare_obj.py.

Prints a single integer score to stdout: 0 = perfect match, higher = worse.
The score is derived from our LCS instruction-ratio: score = round((1 - ratio) * 1000).

Usage:
    python3 tools/permuter/score.py <candidate.o> <target.o> [--function FUNC]
"""

import argparse
import importlib.util
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
COMPARE_OBJ = REPO_ROOT / "tools" / "verify" / "compare_obj.py"


def load_compare_obj():
    spec = importlib.util.spec_from_file_location("compare_obj", str(COMPARE_OBJ))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def score_objects(cand_path: str, target_path: str, function: str | None = None) -> int:
    co = load_compare_obj()
    cand_funcs = co.disassemble(cand_path)
    target_funcs = co.disassemble(target_path)

    if not cand_funcs or not target_funcs:
        return 10**6  # compile produced no functions

    if function:
        fn = function.lstrip("_")
        # Try direct match first
        if fn in cand_funcs and fn in target_funcs:
            pct, _, _ = co.compare_functions(cand_funcs[fn], target_funcs[fn])
            return round((100.0 - pct) * 10)

        # Try rename: find the first function in target that matches one in cand
        for c_fn in cand_funcs:
            if c_fn in target_funcs:
                pct, _, _ = co.compare_functions(cand_funcs[c_fn], target_funcs[c_fn])
                return round((100.0 - pct) * 10)

        return 10**6  # function not found

    # Whole-object: average over matched functions
    matched = set(cand_funcs.keys()) & set(target_funcs.keys())
    if not matched:
        # Try first function in each (handles name mismatch)
        c_fn = next(iter(cand_funcs))
        t_fn = next(iter(target_funcs))
        pct, _, _ = co.compare_functions(cand_funcs[c_fn], target_funcs[t_fn])
        return round((100.0 - pct) * 10)

    total_score = 0
    for fn in matched:
        pct, _, _ = co.compare_functions(cand_funcs[fn], target_funcs[fn])
        total_score += round((100.0 - pct) * 10)
    return total_score // len(matched)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("candidate", help="Candidate compiled .o file")
    ap.add_argument("target", help="Target (reference) .o file")
    ap.add_argument("--function", "-f", help="Compare only this function")
    args = ap.parse_args()

    score = score_objects(args.candidate, args.target, args.function)
    print(score)


if __name__ == "__main__":
    main()
