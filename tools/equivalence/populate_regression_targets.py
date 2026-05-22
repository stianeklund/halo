#!/usr/bin/env python3
"""Auto-populate regression_targets.json from leaf_cache and batch_verify results.

Adds ported functions with high/moderate Unicorn confidence to the regression
suite so they're covered by the pre-commit hook.

Usage:
    python3 tools/equivalence/populate_regression_targets.py              # dry-run
    python3 tools/equivalence/populate_regression_targets.py --apply      # write
    python3 tools/equivalence/populate_regression_targets.py --from-batch # also scan batch_verify results
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TARGETS_FILE = Path(__file__).resolve().parent / "regression_targets.json"
LEAF_CACHE = Path(__file__).resolve().parent / "leaf_cache.json"
KB_JSON = ROOT / "kb.json"
BATCH_DIR = ROOT / "artifacts" / "batch_verify"

CONFIDENCE_THRESHOLD = {"high", "moderate"}
MIN_COVERAGE_PCT = 50
DEFAULT_SEEDS = 20


def load_kb_index():
    kb = json.loads(KB_JSON.read_text(encoding="utf-8"))
    index = {}
    for obj in kb["objects"]:
        for fn in obj["functions"]:
            if not fn.get("ported"):
                continue
            addr = fn.get("addr", "")
            if not addr:
                continue
            decl = fn.get("decl", "")
            m = re.search(r'\b(\w+)\s*\(', decl)
            name = m.group(1) if m else f"FUN_{int(addr, 16):08x}"
            obj_name = obj["name"]
            if not obj_name.endswith(".obj"):
                obj_name += ".obj"
            index[addr] = {
                "name": name,
                "obj": obj_name,
                "decl": decl,
            }
    return index


def discover_from_cache(kb_index):
    if not LEAF_CACHE.exists():
        return []
    cache = json.loads(LEAF_CACHE.read_text(encoding="utf-8"))
    candidates = []
    for addr_hex, entry in cache.items():
        if not isinstance(entry, dict):
            continue
        conf = entry.get("confidence", "")
        if conf not in CONFIDENCE_THRESHOLD:
            continue
        cov = entry.get("coverage_pct", 0)
        if cov < MIN_COVERAGE_PCT:
            continue
        addr_key = addr_hex if addr_hex.startswith("0x") else f"0x{addr_hex}"
        kb_entry = kb_index.get(addr_key)
        if not kb_entry:
            continue
        candidates.append({
            "addr": addr_key,
            "name": kb_entry["name"],
            "obj": kb_entry["obj"],
            "seeds": DEFAULT_SEEDS,
            "flags": ["--allow-stubs"],
            "reason": f"auto: {conf} confidence, {entry.get('coverage_pct', 0):.0f}% coverage",
            "_source": "leaf_cache",
        })
    return candidates


def discover_from_batch(kb_index):
    if not BATCH_DIR.exists():
        return []
    summary = BATCH_DIR / "summary.json"
    if not summary.exists():
        return []
    data = json.loads(summary.read_text(encoding="utf-8"))
    candidates = []
    for row in data.get("rows", []):
        if row.get("status") != "pass":
            continue
        seeds_passed = row.get("seeds_passed", 0)
        seeds_total = row.get("seeds_total", 0)
        if seeds_total < 10 or seeds_passed < seeds_total:
            continue
        addr = row.get("addr", "")
        kb_entry = kb_index.get(addr)
        if not kb_entry:
            continue
        candidates.append({
            "addr": addr,
            "name": kb_entry["name"],
            "obj": kb_entry["obj"],
            "seeds": DEFAULT_SEEDS,
            "flags": ["--allow-stubs"],
            "reason": f"auto: batch pass {seeds_passed}/{seeds_total} seeds",
            "_source": "batch_verify",
        })
    return candidates


def main():
    parser = argparse.ArgumentParser(
        description="Auto-populate regression_targets.json")
    parser.add_argument("--apply", action="store_true",
                        help="Write changes to regression_targets.json")
    parser.add_argument("--from-batch", action="store_true",
                        help="Also scan batch_verify results for passing functions")
    args = parser.parse_args()

    data = json.loads(TARGETS_FILE.read_text(encoding="utf-8"))
    existing = {t["addr"] for t in data.get("targets", [])}

    kb_index = load_kb_index()
    new_targets = []

    for c in discover_from_cache(kb_index):
        if c["addr"] not in existing:
            new_targets.append(c)
            existing.add(c["addr"])

    if args.from_batch:
        for c in discover_from_batch(kb_index):
            if c["addr"] not in existing:
                new_targets.append(c)
                existing.add(c["addr"])

    if not new_targets:
        print("No new targets to add.")
        return 0

    print(f"Found {len(new_targets)} new regression targets:")
    for t in new_targets:
        print(f"  {t['addr']:12s} {t['name']:30s} {t['obj']:25s} ({t['reason']})")

    if not args.apply:
        print(f"\nDry run. Use --apply to write to {TARGETS_FILE.name}")
        return 0

    for t in new_targets:
        clean = {k: v for k, v in t.items() if not k.startswith("_")}
        data["targets"].append(clean)

    TARGETS_FILE.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"\nWrote {len(data['targets'])} targets to {TARGETS_FILE.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
