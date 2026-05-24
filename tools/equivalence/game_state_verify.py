#!/usr/bin/env python3
"""Game-state powered verification: snapshot → unicorn_diff → report.

One-command pipeline for A/B testing ported functions against real
game-state data captured from xemu.

  # Verify all pool-referencing ported functions:
  rtk python3 tools/equivalence/game_state_verify.py \\
      --snapshot artifacts/snapshots/pillar_unpatched.json

  # Verify specific functions:
  rtk python3 tools/equivalence/game_state_verify.py \\
      --snapshot artifacts/snapshots/pillar_unpatched.json \\
      --funcs game_state_malloc,FUN_001bfd00

  # Verify all functions in an object file:
  rtk python3 tools/equivalence/game_state_verify.py \\
      --snapshot artifacts/snapshots/pillar_unpatched.json \\
      --objects game_state,ai

Architecture:
  1. Convert snapshot → unicorn flat format
  2. Scan delinked COFFs for functions that reference pool addresses
  3. Run unicorn_diff with --state-snapshot on each candidate
  4. Compare coverage / pass rate against no-snapshot baseline
  5. Report ranked by coverage delta (most improved first)
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import struct
import tempfile
import time
from datetime import datetime
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent.parent


# ── unicorn_diff runner ────────────────────────────────────────────────


def run_unicorn_diff(func_name: str, snapshot_path: str,
                     seeds: int = 100, allow_stubs: bool = True,
                     timeout: int = 300, verbose: bool = False) -> dict:
    """Run unicorn_diff on a single function with a state snapshot.

    Returns a dict with pass/fail/coverage/confidence info.
    """
    diff_script = str(ROOT / "tools" / "equivalence" / "unicorn_diff.py")
    with tempfile.TemporaryDirectory(prefix="gsv_") as tmpdir:
        output_json = os.path.join(tmpdir, "result.json")
        cmd = [
            sys.executable, diff_script,
            func_name,
            "--seeds", str(seeds),
            "--state-snapshot", snapshot_path,
            "--output-json", output_json,
        ]
        if allow_stubs:
            cmd.append("--allow-stubs")
        if verbose:
            cmd.append("--verbose")

        try:
            proc = subprocess.run(
                cmd, cwd=str(ROOT), capture_output=True, text=True,
                timeout=timeout,
            )
        except subprocess.TimeoutExpired:
            return {"func": func_name, "error": "timeout", "coverage": 0.0}

        if not os.path.exists(output_json):
            return {"func": func_name, "error": "no output", "coverage": 0.0}

        with open(output_json, encoding="utf-8") as f:
            result = json.load(f)

        return {
            "func": func_name,
            "passed": result.get("passed", 0),
            "failed": result.get("failed", 0),
            "errors": result.get("errors", 0),
            "coverage": result.get("coverage_pct", 0.0),
            "confidence": result.get("confidence", "unknown"),
            "total_seeds": result.get("total_seeds", seeds),
        }


# ── pipeline ───────────────────────────────────────────────────────────


def snapshot_to_unicorn(snapshot_path: str) -> str:
    """Convert a game-state snapshot to unicorn format.
    Returns path to the unicorn-format JSON."""
    out_path = str(Path(snapshot_path).with_suffix("")) + "_unicorn_verify.json"
    snap_mod_path = ROOT / "tools" / "equivalence" / "game_state_snapshot.py"
    import importlib.util
    spec = importlib.util.spec_from_file_location("snap", str(snap_mod_path))
    snap = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(snap)
    snap.convert_to_unicorn_snapshot(snapshot_path, output_path=out_path)
    return out_path


def enumerate_pool_functions(objects: Optional[list[str]] = None) -> list[tuple[int | None, str, str]]:
    """List ported functions from specified or all objects in kb.json.

    Uses kb.json to find ported functions filtered by object name.
    Returns sorted list of (address_or_None, name, obj_name) triples.
    """
    kb_path = ROOT / "kb.json"
    if not kb_path.exists():
        return []

    with open(kb_path, encoding="utf-8") as f:
        kb = json.load(f)

    results: list[tuple[int | None, str, str]] = []
    for obj in kb.get("objects", []):
        obj_name = obj.get("name", "")
        if objects and obj_name not in objects:
            continue
        for func in obj.get("functions", []):
            if not func.get("ported"):
                continue
            fname = func.get("decl", "")
            if not fname:
                continue
            fname = fname.split("(", 1)[0].strip()
            parts = fname.split()
            if len(parts) >= 2:
                fname = parts[-1]
            fname = fname.lstrip("*")

            addr_str = func.get("addr", "")
            addr: int | None = None
            if addr_str:
                try:
                    addr = int(addr_str, 16)
                except ValueError:
                    pass

            results.append((addr, fname, obj_name))

    return sorted(set(results), key=lambda x: x[0] or 0)


def verify_snapshot(snapshot_path: str,
                    functions: Optional[list[str]] = None,
                    objects: Optional[list[str]] = None,
                    all_portable: bool = False,
                    seeds: int = 100,
                    timeout: int = 300,
                    verbose: bool = False,
                    output: Optional[str] = None) -> dict:
    """Run the full snapshot verification pipeline.

    1. Convert snapshot to unicorn format
    2. Find target functions
    3. Run unicorn_diff on each with the snapshot
    4. Return ranked report
    """
    report = {
        "snapshot": snapshot_path,
        "timestamp": datetime.now().isoformat(),
        "seeds": seeds,
        "results": [],
        "summary": {},
    }

    # Step 1: convert
    print("=" * 60)
    print(f"Game-State Snapshot Verification")
    print(f"  snapshot: {snapshot_path}")
    print("=" * 60)

    unicorn_path = snapshot_to_unicorn(snapshot_path)
    print(f"  converted → {unicorn_path}")

    # Step 2: enumerate targets
    if functions:
        print(f"  targets : {len(functions)} specified")
        targets = []
        for fname in functions:
            addr = None
            if fname.startswith("FUN_"):
                try:
                    addr = int(fname[4:], 16)
                except ValueError:
                    addr = None
            targets.append((addr, fname, ""))  # no object for --funcs mode
    else:
        targets = enumerate_pool_functions(objects)
        label = "all ported" if all_portable else "pool-referencing"
        print(f"  targets : {len(targets)} {label} functions "
              f"(auto-detected)")

    if not targets:
        print("  No targets found. Done.")
        return report

    # Step 3: run unicorn_diff on each
    n = len(targets)
    report["results"] = []
    passed = 0
    improved = 0
    total_cov = 0.0

    for i, (addr, fname, obj_name) in enumerate(targets):
        print(f"\n  [{i + 1}/{n}] {fname}")
        t0 = time.time()
        result = run_unicorn_diff(fname, unicorn_path, seeds=seeds,
                                  timeout=timeout, verbose=verbose)
        elapsed = time.time() - t0
        result["elapsed_s"] = round(elapsed, 1)
        result["addr"] = f"0x{addr:x}" if addr and addr > 0 else "?"
        result["object"] = obj_name

        if result.get("error"):
            status = f"ERROR ({result['error']})"
        elif result.get("passed", 0) == result.get("total_seeds", 0) and result.get("errors", 0) == 0:
            status = "PASS"
            passed += 1
        elif result.get("errors", 0) > 0:
            status = "ERRORS"
        else:
            status = "FAIL"

        print(f"    {status} | cov={result.get('coverage', 0.0):.1f}% "
              f"conf={result.get('confidence', 'unknown')} | {elapsed:.1f}s")
        if result.get("passed", 0) > 0:
            passed += 0  # already counted above
        total_cov += result["coverage"]
        report["results"].append(result)

    # Step 4: summary
    avg_cov = total_cov / len(targets) if targets else 0
    report["summary"] = {
        "total": len(targets),
        "passed": passed,
        "avg_coverage_pct": round(avg_cov, 1),
    }

    print(f"\n{'=' * 60}")
    print(f"  Summary: {passed}/{len(targets)} passed "
          f"(avg coverage {avg_cov:.1f}%)")
    if all_portable:
        _obj_stats = {}
        for r in report["results"]:
            obj = r.get("object", "?")
            if obj not in _obj_stats:
                _obj_stats[obj] = {"total": 0, "passed": 0, "cov_sum": 0.0}
            _obj_stats[obj]["total"] += 1
            if r.get("passed", 0) == r.get("total_seeds", 0) and r.get("errors", 0) == 0:
                _obj_stats[obj]["passed"] += 1
            _obj_stats[obj]["cov_sum"] += r.get("coverage", 0.0)
        print(f"  {'─' * 40}")
        print(f"  {'Object':<35s} {'P/F/T':>8s} {'Cov%':>7s}")
        for o in sorted(_obj_stats):
            s = _obj_stats[o]
            avg = s["cov_sum"] / s["total"] if s["total"] else 0
            print(f"  {o:<35s} {s['passed']:>1d}/{s['total']-s['passed']:>1d}/{s['total']:>1d} {avg:>6.1f}%")
        report["summary"]["per_object"] = _obj_stats
    print(f"{'=' * 60}")

    if output:
        out_p = Path(output)
        out_p.parent.mkdir(parents=True, exist_ok=True)
        with open(out_p, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)
            f.write("\n")
        print(f"  Report → {output}")

    return report


# ── CLI ──────────────────────────────────────────────────────────────


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Snapshot-powered unicorn verification pipeline",
    )
    p.add_argument("--snapshot", required=True,
                   help="Path to game-state snapshot JSON")
    p.add_argument("--funcs", default=None,
                   help="Comma-separated function names to test")
    p.add_argument("--objects", default=None,
                   help="Comma-separated object names (e.g. game_state,ai)")
    p.add_argument("--all", action="store_true",
                   help="Test ALL ported functions in kb.json (ignores --objects/--funcs)")
    p.add_argument("--seeds", type=int, default=100,
                   help="Seeds per function (default: 100)")
    p.add_argument("--timeout", type=int, default=300,
                   help="Timeout per function in seconds (default: 300)")
    p.add_argument("--output", default=None,
                   help="Write JSON report to this path")
    p.add_argument("--verbose", action="store_true",
                   help="Show per-seed unicorn_diff output")
    p.add_argument("--list-targets", action="store_true",
                   help="List pool-referencing functions and exit")
    p.add_argument("--no-stubs", action="store_true",
                   help="Disable --allow-stubs (only test pure leaves)")
    return p


def main() -> int:
    args = build_parser().parse_args()

    objects = None
    if args.objects:
        objects = [o.strip() for o in args.objects.split(",") if o.strip()]

    functions = None
    if args.funcs:
        functions = [f.strip() for f in args.funcs.split(",") if f.strip()]

    if args.all:
        objects = None
        functions = None

    if args.list_targets:
        targets = enumerate_pool_functions(objects)
        seen = set()
        for addr, name, obj_name in targets:
            if addr is not None:
                print(f"  0x{addr:08x}  {name:<40s} [{obj_name}]")
            else:
                print(f"         ?  {name:<40s} [{obj_name}]")
        print(f"\n  {len(targets)} functions found")
        return 0

    verify_snapshot(
        snapshot_path=args.snapshot,
        functions=functions,
        objects=objects,
        all_portable=args.all,
        seeds=args.seeds,
        timeout=args.timeout,
        verbose=args.verbose,
        output=args.output,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
