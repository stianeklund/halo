#!/usr/bin/env python3
"""Triage batch_verify failures by parsing smoke logs and result JSONs.

Classifies each failure into actionable categories:
  - ftol2: oracle calls _ftol2 intrinsic (stubbed to return 0)
  - intrinsic: oracle calls other CRT intrinsic (_CIlog, _CIpow, etc.)
  - assert_path: divergence only on seeds that trigger assert/halt
  - stub_asymmetry: oracle/lifted call different stubs (name mismatch)
  - coverage_limited: <30% coverage, only early-exit path tested
  - genuine: both sides complete normally, results differ — investigate
  - unknown: doesn't fit other categories

Usage:
    python3 tools/equivalence/triage_failures.py
    python3 tools/equivalence/triage_failures.py --details
    python3 tools/equivalence/triage_failures.py --category genuine
"""

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SUMMARY = ROOT / "artifacts" / "batch_verify_full" / "summary.json"
RESULTS_DIR = ROOT / "artifacts" / "batch_verify_full"
SMOKE_DIR = ROOT / "artifacts" / "equivalence"

FTOL2_NAMES = {"FUN_001d9068", "_ftol2", "ftol2"}
CRT_INTRINSICS = {
    "FUN_001d9068": "_ftol2",
    "_ftol2": "_ftol2",
    "FUN_001d90e0": "_chkstk",
    "_chkstk": "_chkstk",
    "FUN_001d9030": "_ftol",
    "FUN_001dd5c8": "__SEH_prolog",
    "FUN_001dd601": "__SEH_epilog",
    "FUN_001dd620": "_allmul",
    "FUN_001dd660": "_aullshr",
    "FUN_001dd680": "_aullrem",
    "FUN_001dd770": "_aulldiv",
    "_CIlog": "_CIlog",
    "_CIpow": "_CIpow",
    "_CIlog10": "_CIlog10",
    "_CIsqrt": "_CIsqrt",
    "_CIatan2": "_CIatan2",
    "_CIsin": "_CIsin",
    "_CIcos": "_CIcos",
    "_CIfmod": "_CIfmod",
    "_CIacos": "_CIacos",
    "_CIasin": "_CIasin",
    "_CItan": "_CItan",
}
HALT_NAMES = {"FUN_001029a0", "_system_exit", "system_exit",
              "_halt_and_catch_fire", "halt_and_catch_fire"}
ASSERT_NAMES = {"FUN_0008d9f0", "_display_assert", "display_assert"}


def parse_smoke_log(path: Path) -> dict:
    """Extract triage-relevant signals from a smoke log."""
    if not path.exists():
        return {"exists": False}

    text = path.read_text(encoding="utf-8", errors="replace")
    info = {"exists": True, "path": str(path)}

    # Extract stub calls from [stub] lines
    stub_calls = []
    for m in re.finditer(r'\[stub\]\s+(\S+)\s+@\s+(0x[0-9a-f]+)', text):
        stub_calls.append(m.group(1))
    info["stub_calls"] = stub_calls
    info["unique_stubs"] = sorted(set(stub_calls))

    # Check for ftol2 calls
    info["has_ftol2"] = any(s in FTOL2_NAMES for s in stub_calls)

    # Check for CRT intrinsic calls
    intrinsics_hit = set()
    for s in stub_calls:
        if s in CRT_INTRINSICS:
            intrinsics_hit.add(CRT_INTRINSICS[s])
    info["intrinsics"] = sorted(intrinsics_hit)

    # Check for assert/halt stub calls
    info["has_assert"] = any(s in ASSERT_NAMES for s in stub_calls)
    info["has_halt"] = any(s in HALT_NAMES for s in stub_calls)

    # Extract FAIL lines with details
    fails = []
    for m in re.finditer(r'seed\[\s*(\d+)\]\s+FAIL:\s+(.*)', text):
        fails.append({"seed": int(m.group(1)), "diff": m.group(2).strip()})
    info["fails"] = fails

    # Extract oracle ESP_delta from first divergence
    esp_deltas = []
    for m in re.finditer(r'ESP_delta=(-?\d+)', text):
        esp_deltas.append(int(m.group(1)))
    info["esp_deltas"] = esp_deltas

    # Extract oracle INSN_count
    insn_counts = []
    for m in re.finditer(r'INSN_count=(\d+)', text):
        insn_counts.append(int(m.group(1)))
    info["insn_counts"] = insn_counts

    # Coverage
    m = re.search(r'coverage:.*?\(([\d.]+)%\)', text)
    if m:
        info["coverage"] = float(m.group(1))

    # Confidence
    m = re.search(r'confidence:\s+(\w+)', text)
    if m:
        info["confidence"] = m.group(1)

    # Oracle/lifted class
    m = re.search(r'oracle class:\s+(\w+)', text)
    if m:
        info["oracle_class"] = m.group(1)
    m = re.search(r'lifted class:\s+(\w+)', text)
    if m:
        info["lifted_class"] = m.group(1)

    # Extract reloc symbols (to detect asymmetric stubs)
    orc_relocs = []
    lft_relocs = []
    for m in re.finditer(r'\[RELOC\]\s+(oracle|lifted):\s+\'([^\']+)\'', text):
        if m.group(1) == "oracle":
            orc_relocs.append(m.group(2))
        else:
            lft_relocs.append(m.group(2))
    info["oracle_relocs"] = orc_relocs
    info["lifted_relocs"] = lft_relocs

    # CRASH lines
    crashes = re.findall(r'(ORACLE|LIFTED)-CRASH:\s+(.*)', text)
    info["crashes"] = crashes

    return info


def classify(row: dict, smoke: dict) -> tuple:
    """Return (category, detail) for a failure."""
    if not smoke.get("exists"):
        return "unknown", "no smoke log"

    coverage = smoke.get("coverage", 100)
    fails = smoke.get("fails", [])
    intrinsics = smoke.get("intrinsics", [])

    # ftol2: oracle calls _ftol2 and all fails show EAX divergence
    if smoke.get("has_ftol2"):
        eax_fails = [f for f in fails if "EAX:" in f["diff"]]
        if len(eax_fails) == len(fails) and fails:
            return "ftol2", "oracle _ftol2 stub returns 0"

    # Other CRT intrinsics
    non_ftol = [i for i in intrinsics if i != "_ftol2"]
    if non_ftol:
        return "intrinsic", f"oracle calls {', '.join(non_ftol)}"

    # Assert-path: oracle hits halt with ESP_delta != 0
    if smoke.get("has_halt") or smoke.get("has_assert"):
        esp = smoke.get("esp_deltas", [])
        insn = smoke.get("insn_counts", [])
        # If oracle ESP_delta is non-zero on failing seeds, it's an assert path
        abnormal_esp = any(e != 0 for e in esp)
        hit_max_insn = any(c >= 99999 for c in insn)
        if abnormal_esp or hit_max_insn:
            return "assert_path", "divergence on assert/halt path"

    # Coverage-limited: very low coverage means only early-exit tested
    if coverage < 30:
        if row.get("seeds_passed", 0) > row.get("seeds_total", 20) * 0.7:
            return "coverage_limited", f"{coverage:.0f}% coverage, mostly passing"

    # ftol2 even without explicit stub detection (oracle EAX=0 pattern)
    if smoke.get("has_ftol2") and intrinsics:
        return "ftol2", f"oracle _ftol2 + {', '.join(intrinsics)}"

    # Stub asymmetry: oracle and lifted have very different reloc counts
    orc_n = len(smoke.get("oracle_relocs", []))
    lft_n = len(smoke.get("lifted_relocs", []))
    if orc_n > 0 and lft_n > 0 and abs(orc_n - lft_n) > max(orc_n, lft_n) * 0.5:
        return "stub_asymmetry", f"oracle {orc_n} relocs vs lifted {lft_n}"

    # Scratch-only divergence with all seeds failing → likely .rdata or global issue
    if fails:
        scratch_only = all("scratch" in f["diff"] for f in fails)
        if scratch_only:
            return "scratch_divergence", "scratch buffer differs (possible constant/global)"

    # Genuine divergence: both complete normally, results differ
    if fails:
        pass_rate = row.get("seeds_passed", 0) / max(row.get("seeds_total", 1), 1)
        if pass_rate > 0.5:
            return "genuine", f"{len(fails)} seeds fail, {pass_rate:.0%} pass rate"
        return "genuine", f"{len(fails)}/{row.get('seeds_total', '?')} seeds fail"

    return "unknown", "unclassified"


def main():
    parser = argparse.ArgumentParser(description="Triage batch_verify failures")
    parser.add_argument("--details", action="store_true",
                        help="Show detail line for each failure")
    parser.add_argument("--category", type=str, default=None,
                        help="Filter to one category")
    parser.add_argument("--summary-path", type=Path, default=SUMMARY)
    args = parser.parse_args()

    summary = json.loads(args.summary_path.read_text(encoding="utf-8"))
    failures = [r for r in summary.get("rows", []) if r["status"] == "fail"]
    print(f"Triaging {len(failures)} failures...\n")

    categories = defaultdict(list)
    for row in failures:
        name = row["name"]
        smoke_path = SMOKE_DIR / f"{name}_smoke.log"
        smoke = parse_smoke_log(smoke_path)
        cat, detail = classify(row, smoke)
        categories[cat].append({
            "name": name,
            "addr": row.get("addr", ""),
            "obj": row.get("obj", ""),
            "detail": detail,
            "pass_rate": f"{row.get('seeds_passed', 0)}/{row.get('seeds_total', 0)}",
            "coverage": smoke.get("coverage", 0),
            "confidence": smoke.get("confidence", ""),
        })

    # Print summary
    print(f"{'Category':<22} {'Count':>5}  Description")
    print(f"{'-'*22} {'-'*5}  {'-'*50}")
    order = ["genuine", "ftol2", "intrinsic", "assert_path",
             "stub_asymmetry", "scratch_divergence", "coverage_limited", "unknown"]
    for cat in order:
        if cat not in categories:
            continue
        entries = categories[cat]
        desc = {
            "genuine": "Both sides complete, results differ — INVESTIGATE",
            "ftol2": "Oracle _ftol2 stub returns 0 — fix stub",
            "intrinsic": "Oracle calls CRT intrinsic — add stub",
            "assert_path": "Divergence on assert/halt path — low priority",
            "stub_asymmetry": "Oracle/lifted have different reloc patterns",
            "scratch_divergence": "Scratch buffer differs (constant/global)",
            "coverage_limited": "Low coverage, mostly passing — needs state",
            "unknown": "Could not classify",
        }.get(cat, "")
        marker = " ←" if cat == "genuine" else ""
        print(f"{cat:<22} {len(entries):>5}  {desc}{marker}")

    if args.category:
        entries = categories.get(args.category, [])
        if not entries:
            print(f"\nNo failures in category '{args.category}'")
            return
        print(f"\n{'='*70}")
        print(f"Category: {args.category} ({len(entries)} functions)")
        print(f"{'='*70}")
        for e in sorted(entries, key=lambda x: x["coverage"], reverse=True):
            print(f"  {e['name']:45s} {e['pass_rate']:>6s}  cov={e['coverage']:5.1f}%  {e['confidence']:8s}  {e['obj']}")
            if args.details:
                print(f"    → {e['detail']}")
    elif args.details:
        for cat in order:
            if cat not in categories:
                continue
            entries = categories[cat]
            print(f"\n--- {cat} ({len(entries)}) ---")
            for e in sorted(entries, key=lambda x: x["coverage"], reverse=True):
                print(f"  {e['name']:45s} {e['pass_rate']:>6s}  cov={e['coverage']:5.1f}%  {e['detail']}")

    # Actionable summary
    n_genuine = len(categories.get("genuine", []))
    n_ftol2 = len(categories.get("ftol2", []))
    n_intrinsic = len(categories.get("intrinsic", []))
    n_noise = sum(len(categories.get(c, []))
                  for c in ("assert_path", "coverage_limited", "stub_asymmetry", "unknown"))
    print(f"\n{'='*70}")
    print(f"ACTION ITEMS:")
    if n_ftol2:
        print(f"  1. Fix _ftol2 stub → resolves {n_ftol2} failures")
    if n_intrinsic:
        print(f"  2. Add CRT intrinsic stubs → resolves {n_intrinsic} failures")
    if n_genuine:
        print(f"  3. Investigate {n_genuine} genuine divergences (real bugs)")
    if n_noise:
        print(f"  ({n_noise} noise failures: assert paths, low coverage, stub asymmetry)")


if __name__ == "__main__":
    main()
