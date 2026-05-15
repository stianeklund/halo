#!/usr/bin/env python3
"""Triage batch_verify failures by parsing smoke logs and result JSONs.

Classifies each failure into actionable categories:
  - genuine: both sides complete normally, results differ — investigate
  - dirty_eax: oracle uses mov ax (16-bit), upper EAX bits are stale
  - insn_limit: one/both sides hit 100K instruction limit
  - exec_asymmetry: >10x instruction count ratio with different ESP
  - stack_divergence: both sides have different abnormal ESP deltas
  - ftol2: oracle calls _ftol2 intrinsic (stubbed to return 0)
  - intrinsic: oracle calls other CRT intrinsic (_CIlog, _CIpow, etc.)
  - assert_path: divergence only on seeds that trigger assert/halt
  - stub_asymmetry: oracle/lifted call different reloc patterns
  - scratch_divergence: only scratch buffer differs (constant/global)
  - coverage_limited: <30% coverage, only early-exit path tested
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

    # Stubs prepared ratio
    m = re.search(r'stubs prepared:\s*(\d+)/(\d+)', text)
    if m:
        info["stubs_prepared"] = int(m.group(1))
        info["stubs_total"] = int(m.group(2))

    # Per-side INSN_count and ESP_delta from first divergence block
    m = re.search(r'\[oracle\].*?INSN_count=(\d+)', text, re.DOTALL)
    if m:
        info["oracle_insn"] = int(m.group(1))
    m = re.search(r'\[lifted\].*?INSN_count=(\d+)', text, re.DOTALL)
    if m:
        info["lifted_insn"] = int(m.group(1))
    m = re.search(r'\[oracle\].*?ESP_delta=(-?\d+)', text, re.DOTALL)
    if m:
        info["oracle_esp"] = int(m.group(1))
    m = re.search(r'\[lifted\].*?ESP_delta=(-?\d+)', text, re.DOTALL)
    if m:
        info["lifted_esp"] = int(m.group(1))

    return info


def _is_dirty_eax(fails: list) -> bool:
    """Check if EAX divergence is only in upper bits (mov ax/mov al artifact).

    Matches both mov ax (low 16 match) and mov al (low 8 match) patterns.
    Also catches the case where oracle EAX is constant across all seeds
    (same stale upper bits, consistent return value).
    """
    eax_diffs = [f for f in fails if "EAX:" in f["diff"]]
    if not eax_diffs:
        return False
    for f in eax_diffs:
        m = re.search(r'EAX: oracle=(0x[0-9a-f]+) lifted=(0x[0-9a-f]+)', f["diff"])
        if not m:
            return False
        orc = int(m.group(1), 16)
        lft = int(m.group(2), 16)
        if orc == lft:
            return False
        # mov ax: low 16 bits match
        if (orc & 0xFFFF) == (lft & 0xFFFF):
            continue
        # mov al: low 8 bits match, upper 24 differ
        if (orc & 0xFF) == (lft & 0xFF):
            continue
        return False
    return True


def _is_stub_residual(fails: list) -> bool:
    """Check if oracle EAX is random garbage while lifted EAX is 0 or small.

    Pattern: stub callee leaves its return value in EAX, oracle doesn't clear
    it before returning. Lifted code uses XOR EAX,EAX. Detectable because
    oracle EAX values vary wildly across seeds while lifted is constant 0.
    """
    eax_diffs = [f for f in fails if "EAX:" in f["diff"]]
    if len(eax_diffs) < 3:
        return False
    orc_vals = set()
    lft_vals = set()
    for f in eax_diffs:
        m = re.search(r'EAX: oracle=(0x[0-9a-f]+) lifted=(0x[0-9a-f]+)', f["diff"])
        if not m:
            return False
        orc_vals.add(int(m.group(1), 16))
        lft_vals.add(int(m.group(2), 16))
    # Oracle has many distinct values (garbage), lifted has 1-2 (constant)
    return len(orc_vals) >= 3 and len(lft_vals) <= 2


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
        abnormal_esp = any(e != 0 for e in esp)
        hit_max_insn = any(c >= 99999 for c in insn)
        if abnormal_esp or hit_max_insn:
            return "assert_path", "divergence on assert/halt path"

    # --- New false-positive detectors ---

    orc_insn = smoke.get("oracle_insn", 0)
    lft_insn = smoke.get("lifted_insn", 0)
    orc_esp = smoke.get("oracle_esp", 0)
    lft_esp = smoke.get("lifted_esp", 0)

    # Dirty EAX: oracle uses mov ax (16-bit), upper bits are stale pointer
    if fails and _is_dirty_eax(fails):
        only_eax = all(
            f["diff"].startswith("EAX:") and ";" not in f["diff"]
            for f in fails
        )
        if only_eax:
            return "dirty_eax", "oracle mov ax leaves upper 16 bits dirty"

    # Stub residual: oracle EAX is random garbage from a stubbed callee,
    # lifted EAX is consistently 0 (proper XOR EAX,EAX)
    if fails and _is_stub_residual(fails):
        eax_count = sum(1 for f in fails if "EAX:" in f["diff"])
        if eax_count >= len(fails) * 0.5:
            return "stub_residual", "oracle EAX is callee stub garbage, lifted returns 0"

    # Leaf mismatch: oracle is "stubbable" but lifted is "leaf" (same-TU callees
    # compiled into one .obj). Lifted runs real callee code with garbage input
    # and crashes/diverges. Detectable: different class + lifted ESP abnormal.
    orc_class = smoke.get("oracle_class", "")
    lft_class = smoke.get("lifted_class", "")
    if orc_class == "stubbable" and lft_class == "leaf":
        if lft_esp != 0 or lft_insn >= 100000:
            return "leaf_mismatch", "lifted is leaf (same-TU callees), crashes on garbage input"

    # Instruction limit: one or both sides hit 100K — didn't complete normally
    if orc_insn >= 100000 or lft_insn >= 100000:
        # Asymmetric: one side completes, other hits limit
        if orc_insn >= 100000 and lft_insn < 100000 and lft_insn > 0:
            ratio = orc_insn / max(lft_insn, 1)
            if ratio > 10:
                return "insn_limit", f"oracle hit 100K insns, lifted only {lft_insn}"
        if lft_insn >= 100000 and orc_insn < 100000 and orc_insn > 0:
            ratio = lft_insn / max(orc_insn, 1)
            if ratio > 10:
                return "insn_limit", f"lifted hit 100K insns, oracle only {orc_insn}"
        # Both hit limit
        if orc_insn >= 100000 and lft_insn >= 100000:
            if orc_esp != 0 or lft_esp != 0:
                return "insn_limit", "both hit 100K insns, ESP abnormal"

    # Asymmetric execution: >10x instruction ratio without hitting limit
    if orc_insn > 0 and lft_insn > 0:
        ratio = max(orc_insn, lft_insn) / max(min(orc_insn, lft_insn), 1)
        if ratio > 10 and (orc_esp != lft_esp):
            return "exec_asymmetry", f"insn ratio {orc_insn}:{lft_insn}, ESP {orc_esp}:{lft_esp}"

    # Abnormal ESP on both sides with non-zero deltas (neither returned normally)
    if orc_esp != 0 and lft_esp != 0 and orc_esp != lft_esp:
        # Both have stack issues — likely both took different abnormal paths
        if abs(orc_esp - lft_esp) > 8:
            return "stack_divergence", f"ESP oracle={orc_esp} lifted={lft_esp}"

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

    # Stale: summary says fail but smoke log shows no failures (re-run passed)
    if not fails and smoke.get("exists"):
        return "stale", "smoke log shows no failures (batch result outdated)"

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
    order = ["genuine", "dirty_eax", "stub_residual", "leaf_mismatch",
             "insn_limit", "exec_asymmetry", "stack_divergence",
             "ftol2", "intrinsic", "assert_path",
             "stub_asymmetry", "scratch_divergence", "coverage_limited",
             "stale", "unknown"]
    for cat in order:
        if cat not in categories:
            continue
        entries = categories[cat]
        desc = {
            "genuine": "Both sides complete normally, results differ — INVESTIGATE",
            "dirty_eax": "Oracle uses mov ax (16-bit), upper EAX bits stale",
            "stub_residual": "Oracle EAX is callee stub garbage, lifted returns 0",
            "leaf_mismatch": "Lifted is leaf (same-TU callees), crashes on garbage",
            "insn_limit": "One/both sides hit 100K instruction limit",
            "exec_asymmetry": "Wildly different execution length (>10x ratio)",
            "stack_divergence": "Both sides have different abnormal ESP",
            "ftol2": "Oracle _ftol2 stub returns 0 — fix stub",
            "intrinsic": "Oracle calls CRT intrinsic — add stub",
            "assert_path": "Divergence on assert/halt path — low priority",
            "stub_asymmetry": "Oracle/lifted have different reloc patterns",
            "scratch_divergence": "Scratch buffer differs (constant/global)",
            "coverage_limited": "Low coverage, mostly passing — needs state",
            "stale": "Smoke log passes now — batch result outdated",
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
    n_infra = sum(len(categories.get(c, []))
                  for c in ("dirty_eax", "stub_residual", "leaf_mismatch",
                            "insn_limit", "exec_asymmetry", "stack_divergence"))
    n_noise = sum(len(categories.get(c, []))
                  for c in ("assert_path", "coverage_limited", "stub_asymmetry", "unknown"))
    print(f"\n{'='*70}")
    print(f"ACTION ITEMS:")
    if n_genuine:
        print(f"  1. Investigate {n_genuine} genuine divergences (real bugs)")
    if n_ftol2:
        print(f"  2. Fix _ftol2 stub → resolves {n_ftol2} failures")
    if n_intrinsic:
        print(f"  3. Add CRT intrinsic stubs → resolves {n_intrinsic} failures")
    if n_infra:
        print(f"  ({n_infra} test infra issues: dirty EAX, instruction limits, execution asymmetry)")
    if n_noise:
        print(f"  ({n_noise} noise: assert paths, low coverage, stub asymmetry)")


if __name__ == "__main__":
    main()
