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
    python3 tools/equivalence/batch_verify.py --baseline artifacts/batch_verify/summary.json
"""

import argparse
import csv
import json
import re
import subprocess
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
KB_JSON = ROOT / "kb.json"
LEAF_CACHE = ROOT / "tools" / "equivalence" / "leaf_cache.json"
RESULTS_DIR = ROOT / "artifacts" / "batch_verify"
FAIL_STATUSES = {"fail", "error"}


DELINKED_DIR = ROOT / "delinked"


def _has_delinked_ref(addr_str: str, obj_name: str) -> bool:
    """Check if a delinked oracle exists for a function address."""
    try:
        addr_int = int(addr_str, 16)
    except ValueError:
        return False
    sym_upper = f"FUN_{addr_int:08X}"
    sym_lower = f"FUN_{addr_int:08x}"
    addr_no_0x = addr_str.replace("0x", "").lstrip("0") or "0"
    for d in DELINKED_DIR.glob("*.obj"):
        # Use whole-word boundary check on the zero-padded symbol form to
        # avoid substring false-positives (e.g. "3c3a0" matching
        # "objects_FUN_0013c3a0" when the target is at 0x3c3a0).
        if sym_lower in d.stem or sym_upper in d.stem:
            return True
        # Allow bare hex addr only when it is word-bounded (preceded by '_' or
        # start-of-stem).
        stem = d.stem
        idx = stem.find(addr_no_0x)
        while idx != -1:
            before = stem[idx - 1] if idx > 0 else "_"
            if not before.isalnum():
                return True
            idx = stem.find(addr_no_0x, idx + 1)
    # obj_name already includes ".obj" (e.g. "actors.obj") — use it directly.
    bare_name = obj_name if obj_name.endswith(".obj") else f"{obj_name}.obj"
    if (DELINKED_DIR / bare_name).exists():
        return True
    return False


def load_candidates(leaf_only: bool = False, classes: set = None,
                    discover: bool = False):
    """Find ported functions that can be verified.

    Default mode: only functions already in leaf_cache.json.
    Discovery mode (--discover): all ported functions with a delinked oracle,
    regardless of leaf_cache presence.  Cached entries still get their class
    label; uncached ones are tagged 'uncached'.
    """
    if classes is None:
        classes = {"leaf", "data_only", "stubbable"}
    if leaf_only:
        classes = {"leaf"}

    kb = json.loads(KB_JSON.read_text(encoding="utf-8"))
    cache = {}
    if LEAF_CACHE.exists():
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

            if discover:
                if not entry and not _has_delinked_ref(addr_str, obj_name):
                    continue
                cls = (entry.get("class", "uncached") if isinstance(entry, dict)
                       else entry) if entry else "uncached"
            else:
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


def load_json(path: Path) -> dict:
    if not path:
        return {}
    if not path.exists():
        raise FileNotFoundError(path)
    return json.loads(path.read_text(encoding="utf-8"))


def load_targets(path: Path):
    """Load an explicit target list restricting which candidates to verify.

    Accepts a JSON array whose items are either strings (a function name or a
    hex address like "0x1234") or objects with "addr"/"name" keys. Returns
    (addr_rank, name_rank): maps from address-int / name to position in the
    file, so candidates can be filtered to the list and run in its order
    (used to drive low-VC71-first ordering).
    """
    raw = load_json(path)
    items = raw.get("targets", raw) if isinstance(raw, dict) else raw
    addr_rank: dict[int, int] = {}
    name_rank: dict[str, int] = {}
    for i, item in enumerate(items):
        addr = name = None
        if isinstance(item, str):
            if item.lower().startswith("0x"):
                addr = item
            else:
                name = item
        elif isinstance(item, dict):
            addr = item.get("addr")
            name = item.get("name")
        if addr:
            try:
                addr_rank.setdefault(int(addr, 16), i)
            except ValueError:
                pass
        if name:
            name_rank.setdefault(name, i)
    return addr_rank, name_rank


def load_allowlist(path: Path) -> dict[str, set[str]]:
    if not path:
        return {}
    raw = load_json(path)
    entries = raw.get("targets", raw)
    allowlist = {}

    def values(value):
        if isinstance(value, str):
            return [value]
        if isinstance(value, list):
            return [str(item) for item in value]
        return []

    for name, value in entries.items():
        if isinstance(value, str):
            allowlist[name] = {value}
        elif isinstance(value, list):
            allowlist[name] = {str(item) for item in value}
        elif isinstance(value, dict):
            allowlist[name] = set(values(value.get("statuses", [])))
            allowlist[name].update(values(value.get("reasons", [])))
        else:
            allowlist[name] = {"*"}
    return allowlist


def is_allowlisted(row: dict, allowlist: dict[str, set[str]]) -> bool:
    allowed = allowlist.get(row["name"]) or allowlist.get(row["addr"])
    if not allowed:
        return False
    if "*" in allowed:
        return True
    return row["status"] in allowed or row.get("reason", "") in allowed


def failure_key(row: dict) -> str:
    return f"{row['name']}|{row['status']}|{row.get('reason', '')}"


def baseline_failure_keys(path: Path) -> set[str]:
    if not path:
        return set()
    raw = load_json(path)
    rows = raw.get("rows", [])
    if rows:
        return {failure_key(row) for row in rows
                if row.get("status") in FAIL_STATUSES}
    return {f"{name}|fail|{reason}" for name, reason in raw.get("failures", [])}


def summarize_by_object(rows: list[dict]) -> dict:
    by_object: dict[str, Counter] = defaultdict(Counter)
    for row in rows:
        by_object[row["obj"]][row["status"]] += 1
        by_object[row["obj"]]["total"] += 1
    return {obj: dict(counts) for obj, counts in sorted(by_object.items())}


def run_verify(name: str, output_dir: Path, seeds: int = 50, timeout: int = 60,
               float_tolerance: int = 0, skip_esp: bool = False,
               update_leaf_cache: bool = False) -> dict:
    """Run unicorn_diff on a single function. Returns structured result.

    update_leaf_cache: when True, let unicorn_diff persist its class/confidence/
    coverage to leaf_cache.json (the canonical store the dashboard reads). The
    default keeps the regression-batch behavior of NOT touching the cache.
    """
    result_json = output_dir / f"{name}.json"
    cmd = [
        sys.executable, str(ROOT / "tools" / "equivalence" / "unicorn_diff.py"),
        name,
        "--seeds", str(seeds),
        "--z3-equiv",
        "--allow-stubs",
        "--mem-trace",
        "--output-json", str(result_json),
    ]
    if not update_leaf_cache:
        cmd.append("--no-leaf-cache")
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
    parser.add_argument("--output-dir", type=Path, default=RESULTS_DIR,
                        help="Artifact directory (default: artifacts/batch_verify)")
    parser.add_argument("--baseline", type=Path, default=None,
                        help="Previous summary.json to compare against")
    parser.add_argument("--allowlist", type=Path, default=None,
                        help="JSON allowlist for known failures or not_applicable reasons")
    parser.add_argument("--fail-on-new", action="store_true",
                        help="Exit non-zero when baseline comparison finds new failures")
    parser.add_argument("--skip-existing", action="store_true",
                        help="Skip functions that already have a result JSON in --output-dir")
    parser.add_argument("--discover", action="store_true",
                        help="Test all ported functions with delinked refs, not just leaf_cache entries")
    parser.add_argument("--targets", type=Path, default=None,
                        help="JSON list of function names/addresses to restrict to "
                             "(runs in file order; implies --discover so uncached targets are included)")
    parser.add_argument("--update-leaf-cache", action="store_true",
                        help="Persist class/confidence/coverage to leaf_cache.json "
                             "(the store the dashboard reads). Off by default.")
    args = parser.parse_args()

    classes = set(args.classes.split(","))
    # An explicit target list may include uncached functions, so force discovery.
    discover = args.discover or bool(args.targets)
    candidates = load_candidates(leaf_only=args.leaf_only, classes=classes,
                                 discover=discover)

    if args.targets:
        addr_rank, name_rank = load_targets(args.targets)

        def _target_rank(c):
            try:
                ai = int(c["addr"], 16)
            except ValueError:
                ai = None
            ranks = [r for r in (addr_rank.get(ai) if ai is not None else None,
                                 name_rank.get(c["name"])) if r is not None]
            return min(ranks) if ranks else None

        # Keep only candidates named in the target list, in the list's order.
        ranked = [(c, _target_rank(c)) for c in candidates]
        candidates = [c for c, r in sorted(
            (rc for rc in ranked if rc[1] is not None), key=lambda rc: rc[1])]

    if args.limit > 0:
        candidates = candidates[:args.limit]

    print(f"Candidates: {len(candidates)}")

    if args.dry_run:
        for c in candidates:
            print(f"  {c['addr']:12s} {c['class']:10s} {c['obj']:30s} {c['name']}")
        return 0

    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    existing = set()
    if args.skip_existing:
        for p in output_dir.glob("*.json"):
            if p.name == "summary.json":
                continue
            existing.add(p.stem)

    csv_rows = []
    results = {"pass": 0, "fail": 0, "error": 0, "not_applicable": 0,
               "z3_proven": 0, "total": len(candidates)}
    failures = []
    error_failures = []
    proven = []
    rows = []
    skipped = 0
    interrupted = False
    t0 = time.time()

    def _write_summary():
        elapsed = time.time() - t0
        allowlist = load_allowlist(args.allowlist)
        allowlisted = [row for row in rows if is_allowlisted(row, allowlist)]
        current_failure_rows = [row for row in rows
                                if row["status"] in FAIL_STATUSES
                                and not is_allowlisted(row, allowlist)]
        baseline_keys = baseline_failure_keys(args.baseline)
        new_failure_rows = [row for row in current_failure_rows
                            if failure_key(row) not in baseline_keys]
        comparison = {
            "baseline": str(args.baseline) if args.baseline else "",
            "new_failures": new_failure_rows,
            "known_failures": [row for row in current_failure_rows
                               if failure_key(row) in baseline_keys],
            "allowlisted": allowlisted,
        }
        summary_path = output_dir / "summary.json"
        summary_path.write_text(json.dumps({
            "results": results,
            "failures": [(n, r) for n, r in failures],
            "errors": [(n, r) for n, r in error_failures],
            "z3_proven": proven,
            "rows": rows,
            "by_object": summarize_by_object(rows),
            "comparison": comparison,
            "elapsed_seconds": elapsed,
            "interrupted": interrupted,
            "skipped": skipped,
        }, indent=2) + "\n", encoding="utf-8")
        return summary_path, elapsed, comparison

    import signal

    def _sigint_handler(signum, frame):
        nonlocal interrupted
        interrupted = True
        print("\n\nInterrupted — writing summary...")

    prev_handler = signal.signal(signal.SIGINT, _sigint_handler)

    try:
        for i, c in enumerate(candidates):
            if interrupted:
                break

            name = c["name"]

            if name in existing:
                result_path = output_dir / f"{name}.json"
                try:
                    result = json.loads(result_path.read_text(encoding="utf-8"))
                except (OSError, json.JSONDecodeError):
                    result = None
                if result:
                    status = result.get("status", "error")
                    results[status] = results.get(status, 0) + 1
                    if result.get("z3_proven"):
                        results["z3_proven"] += 1
                        proven.append(name)
                    if status == "fail":
                        failures.append((name, result.get("reason", "")))
                    elif status == "error":
                        error_failures.append((name, result.get("reason", "")))
                    rows.append({
                        "addr": c["addr"], "name": name, "class": c["class"],
                        "obj": c["obj"], "status": status,
                        "reason": result.get("reason", ""),
                        "error_details": result.get("error_details", []),
                        "seeds_passed": result.get("passed", 0),
                        "seeds_total": result.get("seeds", 0),
                        "z3_proven": "1" if result.get("z3_proven") else "0",
                    })
                    csv_rows.append(rows[-1])
                    skipped += 1
                    continue

            elapsed = time.time() - t0
            rate = ((i - skipped) / elapsed) if elapsed > 0 and (i - skipped) > 0 else 0
            eta = ((len(candidates) - i) / rate) if rate > 0 else 0
            print(f"[{i+1}/{len(candidates)}] {name:40s} ", end="", flush=True)

            result = run_verify(name, output_dir, seeds=args.seeds, timeout=args.timeout,
                                float_tolerance=args.float_tolerance,
                                skip_esp=args.skip_esp,
                                update_leaf_cache=args.update_leaf_cache)
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
                error_failures.append((name, result.get("reason", "")))
                print(f"ERROR ({result.get('reason', '')})")

            row = {
                "addr": c["addr"],
                "name": name,
                "class": c["class"],
                "obj": c["obj"],
                "status": status,
                "reason": result.get("reason", ""),
                "error_details": result.get("error_details", []),
                "seeds_passed": result.get("passed", 0),
                "seeds_total": result.get("seeds", 0),
                "z3_proven": "1" if result.get("z3_proven") else "0",
            }
            csv_rows.append(row)
            rows.append(row)
    finally:
        signal.signal(signal.SIGINT, prev_handler)

    summary_path, elapsed, comparison = _write_summary()

    print(f"\n{'='*60}")
    print(f"BATCH VERIFY RESULTS ({elapsed:.1f}s)")
    print(f"{'='*60}")
    tested = results["pass"] + results["fail"] + results["error"] + results["not_applicable"]
    print(f"  Total:          {results['total']} ({tested} tested, {skipped} reused)")
    print(f"  Pass:           {results['pass']} ({results['z3_proven']} Z3 proven)")
    print(f"  Fail:           {results['fail']}")
    print(f"  Error:          {results['error']}")
    print(f"  Not applicable: {results['not_applicable']}")
    if interrupted:
        print(f"  (interrupted — {results['total'] - tested - skipped} remaining)")

    if failures:
        print(f"\nFAILURES:")
        for name, reason in failures:
            print(f"  {name}: {reason}")

    if proven:
        print(f"\nZ3 PROVEN EQUIVALENT ({len(proven)}):")
        for name in proven:
            print(f"  {name}")

    if args.baseline:
        print(f"\nBASELINE COMPARISON:")
        print(f"  New failures:   {len(comparison['new_failures'])}")
        print(f"  Known failures: {len(comparison['known_failures'])}")
        print(f"  Allowlisted:    {len(comparison['allowlisted'])}")

    print(f"\nSummary: {summary_path}")

    if args.csv:
        csv_path = output_dir / "results.csv"
        with open(csv_path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=[
                "addr", "name", "class", "obj", "status",
                "reason", "seeds_passed", "seeds_total", "z3_proven",
            ])
            writer.writeheader()
            writer.writerows(csv_rows)
        print(f"CSV: {csv_path}")

    if args.fail_on_new and comparison.get("new_failures"):
        return 1
    return 1 if results["fail"] > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
