#!/usr/bin/env python3
"""Cross-check IDA-exported names against PAL prototype symbol corpus.

This does name-level corroboration only (no address mapping across builds).
Use output to raise/lower confidence before applying renames in Ghidra.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path

_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)


def normalize_name(name: str) -> str:
    s = name.strip()
    if not s:
        return s
    # Keep C++ mangled as-is for exact matching.
    if s.startswith("?"):
        return s
    # Remove common callconv decorations.
    s = re.sub(r"^@", "", s)
    s = re.sub(r"^_", "", s)
    s = re.sub(r"@[0-9]+$", "", s)
    # IDA style @name@N
    if s.startswith("@") and s.count("@") >= 2:
        parts = s.split("@")
        if len(parts) >= 3 and parts[-1].isdigit() and parts[1]:
            s = parts[1]
    return s


def subsystem_for(name: str) -> str:
    n = name.lower()
    if any(k in n for k in ["network", "packet", "socket", "winsock"]):
        return "network"
    if any(k in n for k in ["sound", "audio", "music"]):
        return "sound"
    if any(k in n for k in ["render", "rasterizer", "d3d", "shader"]):
        return "render"
    if any(k in n for k in ["ui", "shell", "screen", "widget"]):
        return "ui"
    if any(k in n for k in ["game", "object", "player", "weapon", "scenario"]):
        return "game"
    return "misc"


def main() -> int:
    ap = argparse.ArgumentParser(description="Match PAL symbol corpus against IDA export names")
    ap.add_argument("--ida-functions", required=True)
    ap.add_argument("--pal-symbols", required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    ida = json.loads(Path(args.ida_functions).read_text(encoding="utf-8"))
    pal = json.loads(Path(args.pal_symbols).read_text(encoding="utf-8"))

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    pal_exact: set[str] = set()
    pal_norm: dict[str, list[str]] = {}
    for row in pal:
        name = row["name"]
        pal_exact.add(name)
        n = normalize_name(name)
        pal_norm.setdefault(n, []).append(name)

    matches = []
    unmatched = []
    for fn in ida:
        ida_name = fn.get("name", "")
        if not ida_name or ida_name.startswith("sub_"):
            continue
        ida_norm = normalize_name(ida_name)
        exact = ida_name in pal_exact
        normalized = ida_norm in pal_norm
        row = {
            "ea_start": fn.get("ea_start"),
            "ida_name": ida_name,
            "ida_normalized": ida_norm,
            "exact_match": exact,
            "normalized_match": normalized,
            "pal_candidates": pal_norm.get(ida_norm, [])[:10],
            "subsystem": subsystem_for(ida_name),
        }
        if exact or normalized:
            matches.append(row)
        else:
            unmatched.append(row)

    matches.sort(key=lambda r: (0 if r["exact_match"] else 1, r["subsystem"], r["ida_name"]))
    unmatched.sort(key=lambda r: (r["subsystem"], r["ida_name"]))

    by_subsystem: dict[str, int] = {}
    for m in matches:
        by_subsystem[m["subsystem"]] = by_subsystem.get(m["subsystem"], 0) + 1

    summary = {
        "ida_named_functions": len([f for f in ida if f.get("name") and not f["name"].startswith("sub_")]),
        "pal_symbol_names": len(pal),
        "match_count": len(matches),
        "exact_match_count": len([m for m in matches if m["exact_match"]]),
        "normalized_match_count": len([m for m in matches if not m["exact_match"] and m["normalized_match"]]),
        "subsystem_breakdown": by_subsystem,
    }

    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    (out_dir / "matched_names.json").write_text(json.dumps(matches, indent=2), encoding="utf-8")
    (out_dir / "unmatched_names.json").write_text(json.dumps(unmatched, indent=2), encoding="utf-8")

    csv_lines = ["ea_start,ida_name,ida_normalized,match_type,subsystem,pal_candidates"]
    for m in matches:
        t = "exact" if m["exact_match"] else "normalized"
        cands = "|".join(m["pal_candidates"]) if m["pal_candidates"] else ""
        csv_lines.append(
            f"{m['ea_start']},{m['ida_name']},{m['ida_normalized']},{t},{m['subsystem']},{cands}"
        )
    (out_dir / "matched_names.csv").write_text("\n".join(csv_lines) + "\n", encoding="utf-8")

    print(json.dumps(summary, indent=2))
    print(f"[+] wrote {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
