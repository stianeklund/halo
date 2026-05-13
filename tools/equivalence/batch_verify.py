#!/usr/bin/env python3
"""Batch-verify ported functions using Z3 equivalence and Unicorn differential testing.

Iterates all ported functions that have both a delinked oracle and a build
candidate, runs unicorn_diff with --z3-equiv --allow-stubs, and reports results.

Usage:
    python3 tools/equivalence/batch_verify.py                # all verifiable
    python3 tools/equivalence/batch_verify.py --leaf-only     # pure leaves only
    python3 tools/equivalence/batch_verify.py --limit 50      # first 50
    python3 tools/equivalence/batch_verify.py --seeds 20      # fewer seeds (faster)
    python3 tools/equivalence/batch_verify.py --dry-run       # list candidates only
"""

import argparse
import csv
import json
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
KB_JSON = ROOT / "kb.json"
LEAF_CACHE = ROOT / "tools" / "equivalence" / "leaf_cache.json"
RESULTS_DIR = ROOT / "artifacts" / "batch_verify"


def load_candidates(leaf_only: bool = False, classes: set = None):
    """Find ported functions that exist in both leaf_cache and kb.json."""
    if classes is None:
        classes = {"leaf", "data_only", "stubbable"}
    if leaf_only:
        classes = {"leaf"}

    kb = json.loads(KB_JSON.read_text(encoding="utf-8"))
    cache = json.loads(LEAF_CACHE.read_text(encoding="utf-8"))

    cache_by_int = {}
    for addr_str, entry in cache.items():
        try:
            cache_by_int[int(addr_str, 16)] = entry
        except ValueError:
            continue

    candidates = []
    for obj in kb["objects"]:
        obj_name = obj.get("name", "")
        for fn in obj["functions"]:
            if not fn.get("ported"):
                continue
            addr_str = fn.get("addr", "")
            if not addr_str:
                continue
            try:
                addr_int = int(addr_str, 16)
            except ValueError:
                continue

            entry = cache_by_int.get(addr_int)
            if not entry:
                continue
            cls = entry.get("class", "") if isinstance(entry, dict) else entry
            if cls not in classes:
                continue

            decl = fn.get("decl", "")
            m = re.search(r'\b(\w+)\s*\(', decl)
            name = m.group(1) if m else addr_str

            candidates.append({
                "addr": addr_str,
                "name": name,
                "class": cls,
                "obj": obj_name,
                "decl": decl,
            })

    return candidates


def run_verify(name: str, seeds: int = 50, timeout: int = 60,
               float_tolerance: int = 0, skip_esp: bool = False) -> dict:
    """Run unicorn_diff on a single function. Returns structured result."""
    result_json = RESULTS_DIR / f"{name}.json"
    cmd = [
        sys.executable, str(ROOT / "tools" / "equivalence" / "unicorn_diff.py"),
        name,
        "--seeds", str(seeds),
        "--z3-equiv",
        "--allow-stubs",
        "--output-json", str(result_json),
        "--no-leaf-cache",
    ]
    if float_tolerance > 0:
        cmd.extend(["--float-tolerance", str(float_tolerance)])
    if skip_esp:
        cmd.append("--skip-esp")
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=timeout, cwd=str(ROOT))
        if result_json.exists():
            return json.loads(result_json.read_text(encoding="utf-8"))
        return {"status": "error", "reason": f"exit={proc.returncode}",
                "target": name}
    except subprocess.TimeoutExpired:
        return {"status": "error", "reason": "timeout", "target": name}
    except Exception as e:
        return {"status": "error", "reason": str(e), "target": name}


def main():
    parser = argparse.ArgumentParser(description="Batch-verify ported functions")
    parser.add_argument("--leaf-only", action="store_true",
                        help="Only verify pure leaf functions")
    parser.add_argument("--limit", type=int, default=0,
                        help="Max number of functions to verify (0=all)")
    parser.add_argument("--seeds", type=int, default=50,
                        help="Seeds per function (default: 50)")
    parser.add_argument("--timeout", type=int, default=60,
                        help="Timeout per function in seconds (default: 60)")
    parser.add_argument("--dry-run", action="store_true",
                        help="List candidates without running verification")
    parser.add_argument("--float-tolerance", type=int, default=32, metavar="ULP",
                        help="ULP tolerance for float params and ST0 (default: 32)")
    parser.add_argument("--skip-esp", action="store_true",
                        help="Force skip ESP delta even for leaf functions")
    parser.add_argument("--csv", action="store_true",
                        help="Write per-function results to results.csv")
    parser.add_argument("--classes", type=str, default="leaf,data_only,stubbable",
                        help="Comma-separated classes to verify")
    args = parser.parse_args()

    classes = set(args.classes.split(","))
    candidates = load_candidates(leaf_only=args.leaf_only, classes=classes)

    if args.limit > 0:
        candidates = candidates[:args.limit]

    print(f"Candidates: {len(candidates)}")

    if args.dry_run:
        for c in candidates:
            print(f"  {c['addr']:12s} {c['class']:10s} {c['obj']:30s} {c['name']}")
        return 0

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    csv_rows = []

    results = {"pass": 0, "fail": 0, "error": 0, "not_applicable": 0,
               "z3_proven": 0, "total": len(candidates)}
    failures = []
    proven = []
    t0 = time.time()

    for i, c in enumerate(candidates):
        name = c["name"]
        elapsed = time.time() - t0
        rate = (i / elapsed) if elapsed > 0 and i > 0 else 0
        eta = ((len(candidates) - i) / rate) if rate > 0 else 0
        print(f"[{i+1}/{len(candidates)}] {name:40s} ", end="", flush=True)

        result = run_verify(name, seeds=args.seeds, timeout=args.timeout,
                            float_tolerance=args.float_tolerance,
                            skip_esp=args.skip_esp)
        status = result.get("status", "error")

        if status == "pass":
            results["pass"] += 1
            if result.get("z3_proven"):
                results["z3_proven"] += 1
                proven.append(name)
                print(f"Z3 PROVEN")
            else:
                seeds_run = result.get("seeds", 0)
                print(f"PASS ({result.get('passed', 0)}/{seeds_run} seeds)")
        elif status == "fail":
            results["fail"] += 1
            failures.append((name, result.get("reason", "")))
            print(f"FAIL ({result.get('failed', 0)} diverged)")
        elif status == "not_applicable":
            results["not_applicable"] += 1
            print(f"N/A ({result.get('reason', '')})")
        else:
            results["error"] += 1
            print(f"ERROR ({result.get('reason', '')})")

        csv_rows.append({
            "addr": c["addr"],
            "name": name,
            "class": c["class"],
            "obj": c["obj"],
            "status": status,
            "reason": result.get("reason", ""),
            "seeds_passed": result.get("passed", 0),
            "seeds_total": result.get("seeds", 0),
            "z3_proven": "1" if result.get("z3_proven") else "0",
        })

    elapsed = time.time() - t0

    print(f"\n{'='*60}")
    print(f"BATCH VERIFY RESULTS ({elapsed:.1f}s)")
    print(f"{'='*60}")
    print(f"  Total:          {results['total']}")
    print(f"  Pass:           {results['pass']} ({results['z3_proven']} Z3 proven)")
    print(f"  Fail:           {results['fail']}")
    print(f"  Not applicable: {results['not_applicable']}")
    print(f"  Error:          {results['error']}")

    if failures:
        print(f"\nFAILURES:")
        for name, reason in failures:
            print(f"  {name}: {reason}")

    if proven:
        print(f"\nZ3 PROVEN EQUIVALENT ({len(proven)}):")
        for name in proven:
            print(f"  {name}")

    summary_path = RESULTS_DIR / "summary.json"
    summary_path.write_text(json.dumps({
        "results": results,
        "failures": [(n, r) for n, r in failures],
        "z3_proven": proven,
        "elapsed_seconds": elapsed,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"\nSummary: {summary_path}")

    if args.csv:
        csv_path = RESULTS_DIR / "results.csv"
        with open(csv_path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=[
                "addr", "name", "class", "obj", "status",
                "reason", "seeds_passed", "seeds_total", "z3_proven",
            ])
            writer.writeheader()
            writer.writerows(csv_rows)
        print(f"CSV: {csv_path}")

    return 1 if results["fail"] > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
