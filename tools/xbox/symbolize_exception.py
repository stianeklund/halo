#!/usr/bin/env python3
"""Symbolize Halo Xbox exception addresses without using stale build/halo.map.

Compiled reimplementation code is appended into the XBE at runtime. The export
RVAs in build/halo are authoritative for those frames; build/halo.map is not.
Original-code frames are resolved against kb.json as a best-effort fallback.
"""
from __future__ import annotations

import argparse
import bisect
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PE = ROOT / "build" / "halo"
DEFAULT_KB = ROOT / "kb.json"
DEFAULT_ORIGINAL_XBE = ROOT / "halo-patched" / "cachebeta.xbe"
FALLBACK_RUNTIME_BASE = 0x642000

ADDRESS_TOKEN_RE = re.compile(r"0x[0-9a-fA-F]{5,8}\b")
FRAME_RE = re.compile(
    r"^\s*(\[[0-9]+\]|#[0-9]+)\s*[:=]?\s*"
    r"(0x[0-9a-fA-F]{5,8}|[0-9a-fA-F]{5,8})\b",
    re.MULTILINE,
)
REGISTER_RE = re.compile(
    r"\b(EIP|ESP|EBP|EAX|EBX|ECX|EDX|ESI|EDI|CR2)\b\s*[:= ]\s*"
    r"(0x[0-9a-fA-F]{5,8}|[0-9a-fA-F]{5,8})\b",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class AddressHit:
    address: int
    label: str
    line: int | None = None


@dataclass(frozen=True)
class ExportSymbol:
    address: int
    rva: int
    raw_name: str
    name: str


@dataclass(frozen=True)
class PeSection:
    name: str
    start: int
    end: int
    executable: bool


@dataclass(frozen=True)
class KbFunction:
    address: int
    name: str
    object_name: str
    decl: str
    ported: Any


class PeSymbolIndex:
    def __init__(self, runtime_base: int, symbols: list[ExportSymbol], sections: list[PeSection]):
        self.runtime_base = runtime_base
        self.symbols = sorted(symbols, key=lambda sym: sym.address)
        self.addresses = [sym.address for sym in self.symbols]
        self.sections = sorted(sections, key=lambda sec: sec.start)

    def section_for(self, address: int) -> PeSection | None:
        for section in self.sections:
            if section.start <= address < section.end:
                return section
        return None

    def contains(self, address: int) -> bool:
        return self.section_for(address) is not None

    def nearest(self, address: int) -> ExportSymbol | None:
        index = bisect.bisect_right(self.addresses, address) - 1
        if index < 0:
            return None
        return self.symbols[index]


class KbSymbolIndex:
    def __init__(self, functions: list[KbFunction]):
        self.functions = sorted(functions, key=lambda fn: fn.address)
        self.addresses = [fn.address for fn in self.functions]

    @classmethod
    def from_data(cls, data: dict[str, Any]) -> "KbSymbolIndex":
        functions: list[KbFunction] = []
        for obj in data.get("objects", []):
            object_name = obj.get("name") or "?"
            for fn in obj.get("functions", []) or []:
                raw_addr = fn.get("addr")
                if not raw_addr:
                    continue
                try:
                    address = int(str(raw_addr), 16)
                except ValueError:
                    continue
                decl = fn.get("decl") or ""
                name = fn.get("name") or name_from_decl(decl) or f"FUN_{address:08x}"
                functions.append(
                    KbFunction(
                        address=address,
                        name=name,
                        object_name=object_name,
                        decl=decl,
                        ported=fn.get("ported"),
                    )
                )
        return cls(functions)

    def nearest(self, address: int) -> KbFunction | None:
        index = bisect.bisect_right(self.addresses, address) - 1
        if index < 0:
            return None
        return self.functions[index]


def parse_address(token: str) -> int:
    token = token.strip().rstrip(",;:)]}")
    if token.lower().startswith("0x"):
        return int(token, 16)
    if re.search(r"[a-fA-F]", token) or len(token) >= 5:
        return int(token, 16)
    return int(token, 0)


def name_from_decl(decl: str) -> str | None:
    cleaned = re.sub(r"@<[^>]+>", "", decl)
    match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", cleaned)
    return match.group(1) if match else None


def normalize_export_name(raw_name: str) -> str:
    name = raw_name
    if name.startswith("@"):
        match = re.match(r"@([^@]+)@[0-9]+$", name)
        if match:
            return match.group(1)
    match = re.match(r"_([^@]+)@[0-9]+$", name)
    if match:
        return match.group(1)
    return name.lstrip("_")


def round_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def runtime_base_from_original_xbe(path: Path) -> tuple[int | None, str]:
    if not path.exists():
        return None, f"{path} not found"
    try:
        from xbe import Xbe  # type: ignore

        xbe = Xbe.from_file(str(path))
        max_end = max(
            section.header.virtual_addr + section.header.virtual_size
            for section in xbe.sections.values()
        )
        return round_up(max_end, 0x1000), f"computed from {path}"
    except Exception as exc:  # pragma: no cover - depends on optional local binary/parser state
        return None, f"failed to parse {path}: {exc}"


def resolve_runtime_base(value: str, original_xbe: Path) -> tuple[int, str]:
    if value != "auto":
        return parse_address(value), "command line"
    base, source = runtime_base_from_original_xbe(original_xbe)
    if base is not None:
        return base, source
    return FALLBACK_RUNTIME_BASE, f"fallback ({source})"


def load_pe_symbols(path: Path, runtime_base: int) -> PeSymbolIndex | None:
    if not path.exists():
        print(f"warning: PE not found: {path}", file=sys.stderr)
        return None
    import pefile  # type: ignore

    pe = pefile.PE(str(path), fast_load=True)
    pe.parse_data_directories(
        directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]]
    )

    symbols: list[ExportSymbol] = []
    for export in getattr(pe, "DIRECTORY_ENTRY_EXPORT", []).symbols:
        if not export.name:
            continue
        raw_name = export.name.decode("ascii", errors="replace")
        symbols.append(
            ExportSymbol(
                address=runtime_base + export.address,
                rva=export.address,
                raw_name=raw_name,
                name=normalize_export_name(raw_name),
            )
        )

    sections: list[PeSection] = []
    for section in pe.sections:
        raw_name = section.Name.rstrip(b"\x00").decode("ascii", errors="replace")
        size = max(section.Misc_VirtualSize, section.SizeOfRawData)
        if size <= 0:
            continue
        executable = bool(section.Characteristics & 0x20000000)
        sections.append(
            PeSection(
                name=raw_name,
                start=runtime_base + section.VirtualAddress,
                end=runtime_base + section.VirtualAddress + size,
                executable=executable,
            )
        )
    return PeSymbolIndex(runtime_base, symbols, sections)


def load_kb_symbols(path: Path) -> KbSymbolIndex | None:
    if not path.exists():
        print(f"warning: kb.json not found: {path}", file=sys.stderr)
        return None
    with path.open("r", encoding="utf-8") as f:
        return KbSymbolIndex.from_data(json.load(f))


def extract_address_hits(text: str, all_hex: bool = False) -> list[AddressHit]:
    hits: list[AddressHit] = []
    seen: set[tuple[int, str, int | None]] = set()

    def add(address: int, label: str, line: int | None) -> None:
        key = (address, label, line)
        if key in seen:
            return
        seen.add(key)
        hits.append(AddressHit(address, label, line))

    line_starts = [0]
    for match in re.finditer(r"\n", text):
        line_starts.append(match.end())

    def line_for(offset: int) -> int:
        return bisect.bisect_right(line_starts, offset)

    for match in REGISTER_RE.finditer(text):
        add(parse_address(match.group(2)), match.group(1).upper(), line_for(match.start()))
    for match in FRAME_RE.finditer(text):
        add(parse_address(match.group(2)), match.group(1), line_for(match.start()))
    if all_hex:
        for match in ADDRESS_TOKEN_RE.finditer(text):
            add(parse_address(match.group(0)), "hex", line_for(match.start()))
    return hits


def resolve_address(
    address: int,
    pe_index: PeSymbolIndex | None,
    kb_index: KbSymbolIndex | None,
    runtime_base: int,
) -> dict[str, Any]:
    if address >= 0x80000000:
        return {
            "address": address,
            "space": "kernel-or-hardware",
            "confidence": "range",
            "symbol": None,
            "offset": None,
            "detail": "outside game XBE user address range",
        }

    if pe_index and pe_index.contains(address):
        section = pe_index.section_for(address)
        symbol = pe_index.nearest(address)
        if symbol:
            offset = address - symbol.address
            return {
                "address": address,
                "space": "compiled",
                "confidence": "exact-export" if offset == 0 else "nearest-export",
                "symbol": symbol.name,
                "raw_symbol": symbol.raw_name,
                "symbol_address": symbol.address,
                "offset": offset,
                "section": section.name if section else None,
                "section_executable": section.executable if section else None,
                "rva": address - pe_index.runtime_base,
                "detail": "PE export table",
            }
        return {
            "address": address,
            "space": "compiled",
            "confidence": "inside-pe-no-export",
            "symbol": None,
            "offset": None,
            "section": section.name if section else None,
            "section_executable": section.executable if section else None,
            "rva": address - pe_index.runtime_base,
            "detail": "inside PE image but before first export",
        }

    if address >= runtime_base:
        nearest = pe_index.nearest(address) if pe_index else None
        return {
            "address": address,
            "space": "compiled?",
            "confidence": "outside-pe-image",
            "symbol": nearest.name if nearest else None,
            "raw_symbol": nearest.raw_name if nearest else None,
            "symbol_address": nearest.address if nearest else None,
            "offset": address - nearest.address if nearest else None,
            "rva": address - runtime_base,
            "detail": "above runtime base but outside known PE sections",
        }

    if kb_index:
        fn = kb_index.nearest(address)
        if fn:
            offset = address - fn.address
            return {
                "address": address,
                "space": "original",
                "confidence": "exact-kb" if offset == 0 else "nearest-kb-start",
                "symbol": fn.name,
                "symbol_address": fn.address,
                "offset": offset,
                "object": fn.object_name,
                "ported": fn.ported,
                "decl": fn.decl,
                "detail": "kb.json function table",
            }

    return {
        "address": address,
        "space": "unknown",
        "confidence": "unresolved",
        "symbol": None,
        "offset": None,
        "detail": "no matching PE or kb.json symbol",
    }


def format_symbol(result: dict[str, Any]) -> str:
    symbol = result.get("symbol")
    if symbol is None:
        return "<unresolved>"
    offset = result.get("offset")
    if offset in (None, 0):
        return str(symbol)
    return f"{symbol}+0x{offset:x}"


def format_result(hit: AddressHit, result: dict[str, Any]) -> str:
    label = hit.label
    if hit.line is not None:
        label = f"{label}:L{hit.line}"
    extra: list[str] = []
    if result.get("space") == "compiled":
        extra.append(f"section={result.get('section')}")
        extra.append(f"rva=0x{result.get('rva'):x}")
    if result.get("space") == "original":
        extra.append(f"object={result.get('object')}")
        extra.append(f"ported={result.get('ported')}")
    detail = " ".join(extra)
    if detail:
        detail = "  " + detail
    return (
        f"{label:10} 0x{hit.address:08x}  {result['space']:<13} "
        f"{result['confidence']:<17} {format_symbol(result)}{detail}"
    )


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Symbolize Halo Xbox exception EIP/stack addresses.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("addresses", nargs="*", help="addresses to symbolize")
    parser.add_argument("--file", "-f", type=Path, help="exception text file; use '-' for stdin")
    parser.add_argument("--stdin", action="store_true", help="read exception text from stdin")
    parser.add_argument("--all-hex", action="store_true", help="also symbolize every 0x... token in text")
    parser.add_argument("--pe", type=Path, default=DEFAULT_PE, help="fresh compiled PE")
    parser.add_argument("--kb", type=Path, default=DEFAULT_KB, help="kb.json for original-code frames")
    parser.add_argument("--no-kb", action="store_true", help="skip kb.json original-code resolution")
    parser.add_argument(
        "--base",
        default="auto",
        help="runtime base for build/halo export RVAs, or 'auto' to derive from original XBE",
    )
    parser.add_argument(
        "--original-xbe",
        type=Path,
        default=DEFAULT_ORIGINAL_XBE,
        help="unpatched XBE used to compute the appended PE base when --base=auto",
    )
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    return parser


def collect_hits(args: argparse.Namespace) -> list[AddressHit]:
    hits = [AddressHit(parse_address(addr), "arg") for addr in args.addresses]
    text_parts: list[str] = []
    if args.file:
        if str(args.file) == "-":
            text_parts.append(sys.stdin.read())
        else:
            text_parts.append(args.file.read_text(encoding="utf-8", errors="replace"))
    if args.stdin:
        text_parts.append(sys.stdin.read())
    for text in text_parts:
        hits.extend(extract_address_hits(text, all_hex=args.all_hex))
    return hits


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    hits = collect_hits(args)
    if not hits:
        parser.error("provide addresses, --file, or --stdin")

    runtime_base, base_source = resolve_runtime_base(args.base, args.original_xbe)
    pe_index = load_pe_symbols(args.pe, runtime_base)
    kb_index = None if args.no_kb else load_kb_symbols(args.kb)

    rows = []
    for hit in hits:
        result = resolve_address(hit.address, pe_index, kb_index, runtime_base)
        row = dict(result)
        row["label"] = hit.label
        row["line"] = hit.line
        rows.append(row)

    if args.json:
        print(json.dumps({"runtime_base": runtime_base, "base_source": base_source, "frames": rows}, indent=2))
        return 0

    print(f"runtime_base=0x{runtime_base:x} ({base_source})")
    print(f"pe={args.pe}")
    if not args.no_kb:
        print(f"kb={args.kb}")
    for hit, row in zip(hits, rows):
        print(format_result(hit, row))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
