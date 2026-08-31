#!/usr/bin/env python3
"""Create a conservative reference-build to NTSC-2276 function map.

The reference symbol list is useful naming evidence, not proof that two
functions have the same body.  This tool only identifies unique,
non-placeholder name matches and labels every result ``NAME_ONLY``.  A caller
must additionally prove an unchanged body from both target binaries before
using a reference function as a source donor.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_KB = ROOT / "kb.json"
PLACEHOLDER_RE = re.compile(r"^(?:fun|code|sub|data|bss|rdata)_[0-9a-f]+$", re.I)
STDCALL_RE = re.compile(r"^_?(?P<name>[A-Za-z_$][A-Za-z0-9_$]*)@\d+$")
MAP_FUNCTION_RE = re.compile(
    r"^\s*(?P<section>[0-9A-Fa-f]+):(?P<offset>[0-9A-Fa-f]+)\s+"
    r"(?P<name>\S+)\s+(?P<va>[0-9A-Fa-f]+)\s+f\s+(?P<object>\S+)\s*$"
)


def normalize_name(name: str) -> str:
    """Normalize C calling-convention decoration without changing identity."""
    name = name.strip()
    match = STDCALL_RE.match(name)
    if match:
        name = match.group("name")
    elif name.startswith("_"):
        name = name[1:]
    return name


def is_meaningful_name(name: str) -> bool:
    """Reject address-derived and compiler-local placeholders."""
    return bool(name) and not PLACEHOLDER_RE.match(name)


def load_ntsc_functions(kb_path: Path) -> list[dict[str, Any]]:
    kb = json.loads(kb_path.read_text(encoding="utf-8"))
    functions = []
    for obj in kb.get("objects", []):
        for function in obj.get("functions", []):
            name = function.get("name", "")
            addr = function.get("addr")
            if not name or not addr:
                continue
            functions.append({
                "addr": addr.lower(),
                "name": name,
                "normalized_name": normalize_name(name),
                "decl": function.get("decl", ""),
                "ported": bool(function.get("ported")),
                "object": obj.get("name", ""),
                "source": obj.get("source", ""),
            })
    return functions


def load_reference_symbols(symbols_path: Path) -> list[dict[str, Any]]:
    symbols = json.loads(symbols_path.read_text(encoding="utf-8"))
    result = []
    for symbol in symbols:
        name = symbol.get("name")
        if not name:
            continue
        # PDB2.00 extraction includes named data records.  They cannot be a
        # function donor, while config/symbols.json has no section field and
        # remains valid input.
        if symbol.get("section") not in (None, ".text"):
            continue
        row = {
            "file_offset": symbol.get("file_offset"),
            "rva": symbol.get("rva"),
            "section": symbol.get("section"),
            "kind": symbol.get("kind"),
            "name": name,
            "normalized_name": normalize_name(name),
        }
        result.append(row)
    return result


def load_reference_map(map_path: Path) -> list[dict[str, Any]]:
    """Read function entries from an MSVC linker map without inferring bodies."""
    symbols = []
    for line in map_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = MAP_FUNCTION_RE.match(line)
        if not match:
            continue
        symbols.append({
            "file_offset": None,
            "va": "0x" + match.group("va").lower(),
            "object": match.group("object"),
            "name": match.group("name"),
            "normalized_name": normalize_name(match.group("name")),
        })
    return symbols


def group_by_name(rows: list[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[row["normalized_name"]].append(row)
    return grouped


def build_report(ntsc_functions: list[dict[str, Any]],
                 reference_symbols: list[dict[str, Any]]) -> dict[str, Any]:
    """Return only one-to-one, meaningful name correspondences.

    A matched name can still represent changed code, a changed signature, or
    coincidentally reused terminology.  The explicit evidence tier prevents
    consumers from mistaking this inventory for a cross-version byte match.
    """
    ntsc_by_name = group_by_name(ntsc_functions)
    reference_by_name = group_by_name(reference_symbols)
    candidates = []

    for name in sorted(set(ntsc_by_name) & set(reference_by_name)):
        ntsc_rows = ntsc_by_name[name]
        reference_rows = reference_by_name[name]
        if not is_meaningful_name(name):
            continue
        if len(ntsc_rows) != 1 or len(reference_rows) != 1:
            continue
        ntsc = ntsc_rows[0]
        reference = reference_rows[0]
        candidates.append({
            "status": "NAME_ONLY",
            "required_next_evidence": [
                "reference target-function bytes and boundary",
                "NTSC 2276 target-function bytes and boundary",
                "same ABI/call-site evidence",
            ],
            "reference": reference,
            "ntsc": ntsc,
        })

    unported = [candidate for candidate in candidates if not candidate["ntsc"]["ported"]]
    return {
        "schema_version": 1,
        "authority": {
            "target": "Halo Xbox debug build 2276",
            "rule": "Reference-build names are hypotheses; no row is a body or ABI match.",
        },
        "summary": {
            "reference_symbols": len(reference_symbols),
            "ntsc_functions": len(ntsc_functions),
            "unique_meaningful_name_matches": len(candidates),
            "unported_name_only_candidates": len(unported),
        },
        "candidates": candidates,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    reference_input = parser.add_mutually_exclusive_group(required=True)
    reference_input.add_argument("--reference-symbols", type=Path,
                                 help="config/symbols.json or extracted PDB symbols")
    reference_input.add_argument("--reference-map", type=Path,
                                 help="linker map from a reference build")
    parser.add_argument("--kb", type=Path, default=DEFAULT_KB,
                        help="NTSC 2276 kb.json (default: repository kb.json)")
    parser.add_argument("--out", required=True, type=Path,
                        help="report path")
    parser.add_argument("--unported-only", action="store_true",
                        help="write only NTSC functions not yet ported")
    args = parser.parse_args()

    if args.reference_map:
        reference_symbols = load_reference_map(args.reference_map)
    else:
        reference_symbols = load_reference_symbols(args.reference_symbols)
    report = build_report(load_ntsc_functions(args.kb), reference_symbols)
    if args.unported_only:
        report["candidates"] = [
            candidate for candidate in report["candidates"]
            if not candidate["ntsc"]["ported"]
        ]
        report["summary"]["written_candidates"] = len(report["candidates"])
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
