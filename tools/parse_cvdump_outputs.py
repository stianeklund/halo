#!/usr/bin/env python3
"""Parse cvdump text outputs into machine-readable symbol/type corpora."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


PUB_RE = re.compile(r"^S_PUB32:\s*\[([0-9A-Fa-f]{4}):([0-9A-Fa-f]{8})\],\s*Flags:\s*([0-9A-Fa-f]{8}),\s*(.+)$")
TYPE_ENUM_RE = re.compile(r"enum name\s*=\s*([^,]+),")
TYPE_STRUCT_RE = re.compile(r"(?:class|struct|union) name\s*=\s*([^,]+),")


def classify_name(name: str) -> str:
    if name.startswith("?"):
        return "msvc_mangled"
    if name.startswith("@") and name.count("@") >= 2:
        return "stdcall_decorated"
    if name.startswith("_") and "@" in name and name.rsplit("@", 1)[-1].isdigit():
        return "stdcall_underscore"
    if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name):
        return "identifier"
    return "other"


def parse_publics(path: Path) -> list[dict[str, object]]:
    out: list[dict[str, object]] = []
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        m = PUB_RE.match(line.strip())
        if not m:
            continue
        sec_hex, off_hex, flags_hex, name = m.groups()
        name = name.strip()
        out.append(
            {
                "name": name,
                "section": int(sec_hex, 16),
                "offset": int(off_hex, 16),
                "flags": int(flags_hex, 16),
                "source": "cvdump_publics",
                "confidence": "high",
                "kind": classify_name(name),
            }
        )
    return out


def parse_types(path: Path) -> dict[str, list[str]]:
    enums: set[str] = set()
    structs: set[str] = set()
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        s = line.strip()
        m = TYPE_ENUM_RE.search(s)
        if m:
            enums.add(m.group(1).strip())
        m = TYPE_STRUCT_RE.search(s)
        if m:
            structs.add(m.group(1).strip())
    return {"enums": sorted(enums), "composites": sorted(structs)}


def main() -> int:
    ap = argparse.ArgumentParser(description="Parse cvdump outputs")
    ap.add_argument("--publics", required=True)
    ap.add_argument("--types", required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    publics_path = Path(args.publics)
    types_path = Path(args.types)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    publics = parse_publics(publics_path)
    type_info = parse_types(types_path)

    # De-dup by name keeping first occurrence (usually fine for corroboration).
    seen = set()
    unique_symbols = []
    for row in publics:
        n = row["name"]
        if n in seen:
            continue
        seen.add(n)
        unique_symbols.append(row)

    summary = {
        "public_rows": len(publics),
        "unique_symbol_names": len(unique_symbols),
        "enum_count": len(type_info["enums"]),
        "composite_type_count": len(type_info["composites"]),
    }

    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    (out_dir / "symbol_names.json").write_text(json.dumps(unique_symbols, indent=2), encoding="utf-8")
    (out_dir / "types.json").write_text(json.dumps(type_info, indent=2), encoding="utf-8")
    (out_dir / "symbol_names.txt").write_text("\n".join(r["name"] for r in unique_symbols) + "\n", encoding="utf-8")

    print(json.dumps(summary, indent=2))
    print(f"[+] wrote {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
