#!/usr/bin/env python3
"""Inventory current Halo verification coverage and blockers."""

import argparse
import json
import re
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent.parent
KB_JSON = ROOT / "kb.json"
LEAF_CACHE = ROOT / "tools" / "equivalence" / "leaf_cache.json"
DELINKED_MANIFEST = ROOT / "delinked" / "manifest.json"
FUNCTION_BOUNDS = ROOT / "tools" / "verify" / "function_bounds.json"
BATCH_VERIFY_DIR = ROOT / "artifacts" / "batch_verify"
OUTPUT_DIR = ROOT / "artifacts" / "test_inventory"
EQUIV_CLASSES = {"leaf", "data_only", "stubbable"}


def load_json(path: Path, default):
    if not path.exists():
        return default
    return json.loads(path.read_text(encoding="utf-8"))


def function_name(decl: str, addr: str) -> str:
    match = re.search(r"\b(\w+)\s*\(", decl or "")
    return match.group(1) if match else addr


def normalize_addr(addr: str) -> str:
    try:
        return f"0x{int(addr, 16):08x}"
    except (TypeError, ValueError):
        return addr or ""


def fun_name_from_addr(addr: str) -> str:
    try:
        return f"FUN_{int(addr, 16):08x}"
    except (TypeError, ValueError):
        return ""


def manifest_keys() -> set[str]:
    raw = load_json(DELINKED_MANIFEST, {})
    return {key.replace("LIBCMT:", "") for key in raw.keys()}


_BOUNDS_CACHE: set[int] | None = None


def bounds_addrs() -> set[int]:
    """Addresses the committed VC71 bounds table can derive a reference for.

    VC71 scoring reads the pristine XBE bounded by tools/verify/function_bounds.json
    (tools/verify/xbe_reference.py), so membership here — not the presence of a
    Ghidra export — is what "byte-matchable" means.
    """
    global _BOUNDS_CACHE
    if _BOUNDS_CACHE is not None:
        return _BOUNDS_CACHE
    _BOUNDS_CACHE = set()
    table = load_json(FUNCTION_BOUNDS, {})
    for key, entry in table.items():
        if key == "_meta" or not isinstance(entry, dict):
            continue
        try:
            start = int(key, 16)
            if int(entry["end"], 16) > start:
                _BOUNDS_CACHE.add(start)
        except (KeyError, TypeError, ValueError):
            continue
    return _BOUNDS_CACHE


def is_vc71_scoreable(addr: str) -> bool:
    """True when VC71 can score this function (a bound exists for its address)."""
    try:
        return int(addr, 16) in bounds_addrs()
    except (TypeError, ValueError):
        return False


def has_delinked_reference(obj_name: str, addr: str, manifest: set[str]) -> bool:
    """True when a Ghidra-delinked object exists for this function.

    This is the EQUIVALENCE-lane precondition, not the VC71 one: unicorn_diff
    executes the oracle and needs an object with real relocations. For "can this
    be byte-matched?" use is_vc71_scoreable().
    """
    bare = obj_name.replace("LIBCMT:", "")
    if bare in manifest:
        return True
    if (ROOT / "delinked" / bare).exists():
        return True
    if bare.endswith(".obj") and (ROOT / "delinked" / bare).exists():
        return True
    try:
        func_ref = ROOT / "delinked" / "functions" / f"{int(addr, 16):08x}.obj"
    except (TypeError, ValueError):
        return False
    return func_ref.exists()


def load_leaf_classes() -> dict[int, str]:
    raw = load_json(LEAF_CACHE, {})
    classes = {}
    for addr, entry in raw.items():
        try:
            addr_int = int(addr, 16)
        except ValueError:
            continue
        if isinstance(entry, dict):
            classes[addr_int] = entry.get("class", "unknown")
        else:
            classes[addr_int] = str(entry)
    return classes


def batch_failure_artifacts() -> dict[str, dict]:
    artifacts = {}
    if not BATCH_VERIFY_DIR.exists():
        return artifacts
    for path in sorted(BATCH_VERIFY_DIR.glob("*.json")):
        if path.name == "summary.json":
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        status = data.get("status")
        if status in {"fail", "error", "not_applicable"}:
            target = data.get("target") or path.stem
            artifacts[target] = {
                "status": status,
                "reason": data.get("reason", ""),
                "path": str(path),
            }
    return artifacts


def inventory() -> tuple[dict, dict, dict]:
    kb = load_json(KB_JSON, {})
    leaf_classes = load_leaf_classes()
    manifest = manifest_keys()
    prior_artifacts = batch_failure_artifacts()

    totals = Counter()
    class_counts = Counter()
    lane_counts = Counter()
    blocked_reasons = Counter()
    by_object: dict[str, Counter] = defaultdict(Counter)
    blocked = []
    runtime_targets = []
    rows = []

    for obj in kb.get("objects", []):
        obj_name = obj.get("name", "")
        for fn in obj.get("functions", []):
            totals["functions"] += 1
            addr = fn.get("addr", "")
            decl = fn.get("decl", "")
            name = function_name(decl, addr)
            ported = bool(fn.get("ported"))
            if ported:
                totals["ported"] += 1

            try:
                cls = leaf_classes.get(int(addr, 16), "unclassified")
            except (TypeError, ValueError):
                cls = "unclassified"
            class_counts[cls] += 1

            has_ref = has_delinked_reference(obj_name, addr, manifest)
            scoreable = is_vc71_scoreable(addr)
            if ported and scoreable:
                totals["vc71_scoreable"] += 1
            prior = prior_artifacts.get(name)
            if not prior:
                prior = prior_artifacts.get(fun_name_from_addr(addr))

            if not ported:
                lane = "unported"
                reason = ""
            elif cls in EQUIV_CLASSES and has_ref:
                lane = "unicorn_z3"
                reason = ""
                totals["unicorn_z3_verifiable"] += 1
            elif cls in EQUIV_CLASSES:
                lane = "blocked"
                reason = "missing_delinked_reference"
            elif cls == "non_leaf":
                lane = "runtime_oracle"
                reason = "non_leaf"
            else:
                lane = "runtime_oracle"
                reason = "unclassified"

            lane_counts[lane] += 1
            by_object[obj_name][lane] += 1
            by_object[obj_name]["total"] += 1

            row = {
                "addr": normalize_addr(addr),
                "name": name,
                "object": obj_name,
                "ported": ported,
                "class": cls,
                # VC71 byte-match readiness (bounds table).  Distinct from
                # has_delinked_reference, which gates the equivalence lane.
                "vc71_scoreable": scoreable,
                "has_delinked_reference": has_ref,
                "recommended_lane": lane,
                "blocked_reason": reason,
                "prior_failure_artifact": prior or None,
            }
            rows.append(row)

            if reason:
                blocked_reasons[reason] += 1
                if lane == "blocked":
                    blocked.append(row)
                else:
                    runtime_targets.append(row)

    summary = {
        "total_functions": totals["functions"],
        "ported_functions": totals["ported"],
        "vc71_scoreable_functions": totals["vc71_scoreable"],
        "unicorn_z3_verifiable_functions": totals["unicorn_z3_verifiable"],
        "class_counts": dict(sorted(class_counts.items())),
        "lane_counts": dict(sorted(lane_counts.items())),
        "blocked_reasons": dict(sorted(blocked_reasons.items())),
        "prior_failure_artifacts": len(prior_artifacts),
    }
    by_object_json = {obj: dict(counts) for obj, counts in sorted(by_object.items())}
    report = {
        "summary": summary,
        "blocked": blocked,
        "targets_needing_runtime_oracle": runtime_targets,
        "rows": rows,
    }
    return summary, by_object_json, report


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Report verification coverage from kb.json, leaf_cache, delinked refs, and prior artifacts.")
    parser.add_argument("--output-dir", type=Path, default=OUTPUT_DIR,
                        help="Directory for summary.json/by_object.json/blocked_report.json")
    parser.add_argument("--no-write", action="store_true",
                        help="Print the summary without writing artifacts.")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    summary, by_object, report = inventory()

    print("Test inventory")
    print(f"  Total functions:              {summary['total_functions']}")
    print(f"  Ported functions:             {summary['ported_functions']}")
    print(f"  VC71-scoreable (ported):      {summary['vc71_scoreable_functions']}")
    print(f"  Unicorn/Z3 verifiable:        {summary['unicorn_z3_verifiable_functions']}")
    print(f"  Classes:                      {summary['class_counts']}")
    print(f"  Lanes:                        {summary['lane_counts']}")
    print(f"  Blocked reasons:              {summary['blocked_reasons']}")
    print(f"  Prior failure artifacts:      {summary['prior_failure_artifacts']}")

    if not args.no_write:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        (args.output_dir / "summary.json").write_text(
            json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        (args.output_dir / "by_object.json").write_text(
            json.dumps(by_object, indent=2) + "\n", encoding="utf-8")
        (args.output_dir / "blocked_report.json").write_text(
            json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"  Wrote:                        {args.output_dir}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
