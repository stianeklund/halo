#!/usr/bin/env python3
"""Extract DIR32 relocation targets from ALL delinked .obj files.

Scans every section of every .obj in delinked/ for IMAGE_REL_I386_DIR32 (0x0006)
relocations whose target symbol name looks like an Xbox VA (hex suffix).
Outputs a _KNOWN_GLOBAL_BYTES dict snippet for unicorn_diff.py.

Usage:
    python3 tools/equivalence/extract_globals.py [--verbose] [--min-addr 0x10000]
"""
import re
import struct
import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
_TOOLS_DIR = _SCRIPT_DIR.parent
_REPO_ROOT = _TOOLS_DIR.parent
sys.path.insert(0, str(_SCRIPT_DIR))

import coff_loader

XBE_PATH = _REPO_ROOT / "halo-patched" / "cachebeta.xbe"
DELINKED_DIR = _REPO_ROOT / "delinked"

DIR32 = 0x0006  # IMAGE_REL_I386_DIR32
MIN_ADDR = 0x10000
MAX_ADDR = 0x6400000  # ~100 MB Xbox VA space; filter out constants like 0x80000000, 0xc0000090, 0xffffffff


def _load_xbe_sections() -> list:
    sections = []
    with open(XBE_PATH, "rb") as f:
        f.seek(0x104)
        base_addr = struct.unpack("<I", f.read(4))[0]
        f.seek(0x11C)
        section_count = struct.unpack("<I", f.read(4))[0]
        section_header_addr = struct.unpack("<I", f.read(4))[0]
        f.seek(section_header_addr - base_addr)
        for _ in range(section_count):
            sh = f.read(56)
            name = sh[:8].rstrip(b"\x00").decode("ascii", errors="replace")
            vaddr = struct.unpack_from("<I", sh, 4)[0]
            vsize = struct.unpack_from("<I", sh, 8)[0]
            raw_addr = struct.unpack_from("<I", sh, 12)[0]
            raw_size = struct.unpack_from("<I", sh, 16)[0]
            sections.append({
                "name": name,
                "vaddr": vaddr,
                "vsize": vsize,
                "raw_addr": raw_addr,
                "raw_size": raw_size
            })
    return sections


def _read_xbe_bytes(sections: list, va: int, size: int) -> bytes:
    for s in sections:
        if s["vaddr"] <= va < s["vaddr"] + s["vsize"]:
            file_off = s["raw_addr"] + (va - s["vaddr"])
            with open(XBE_PATH, "rb") as f:
                f.seek(file_off)
                return f.read(size)
    return None


def _va_from_symbol(name: str) -> int:
    m = re.search(r'([0-9a-fA-F]{5,8})$', name)
    if not m:
        return 0
    addr = int(m.group(1), 16)
    return addr if (MIN_ADDR <= addr <= MAX_ADDR) else 0


def main():
    verbose = "--verbose" in sys.argv
    out_path = None
    for i, a in enumerate(sys.argv):
        if a == "--output" and i + 1 < len(sys.argv):
            out_path = Path(sys.argv[i + 1])

    if not DELINKED_DIR.exists():
        print(f"Error: {DELINKED_DIR} not found.", file=sys.stderr)
        sys.exit(1)

    xbe_sections = _load_xbe_sections() if XBE_PATH.exists() else []

    obj_files = sorted(DELINKED_DIR.glob("*.obj"))
    print(f"# Scanning {len(obj_files)} delinked .obj files...", file=sys.stderr)

    global_addrs = set()
    addr_sources = {}
    errors = 0

    for obj_path in obj_files:
        try:
            sections, symbols, _strtab = coff_loader.load_coff(str(obj_path))
        except Exception as e:
            errors += 1
            if verbose:
                print(f"# SKIP {obj_path.name}: {e}", file=sys.stderr)
            continue

        for sec in sections:
            if sec.num_relocs == 0:
                continue
            with open(obj_path, "rb") as f:
                raw = f.read()
            off = sec.reloc_offset
            for _ in range(sec.num_relocs):
                if off + coff_loader.RELOC_SIZE > len(raw):
                    break
                _, sym_idx, rtype = struct.unpack_from("<IIH", raw, off)
                off += coff_loader.RELOC_SIZE
                if rtype != DIR32:
                    continue
                if sym_idx >= len(symbols):
                    continue
                sym_name = symbols[sym_idx].name
                addr = _va_from_symbol(sym_name)
                if addr and addr not in global_addrs:
                    global_addrs.add(addr)
                    addr_sources[addr] = f"{obj_path.name}:{sym_name}"

    print(f"# Parsed {len(obj_files)} .obj files ({errors} errors)", file=sys.stderr)
    print(f"# Found {len(global_addrs)} unique DIR32 relocation targets", file=sys.stderr)

    sorted_addrs = sorted(global_addrs)
    mapped = []
    unmapped = []

    for addr in sorted_addrs:
        if xbe_sections:
            b = _read_xbe_bytes(xbe_sections, addr, 4)
        else:
            b = None
        if b is not None and len(b) == 4:
            mapped.append((addr, b))
        else:
            unmapped.append(addr)

    lines = []
    lines.append("_KNOWN_GLOBAL_BYTES = {")
    for addr, b in mapped:
        hex_val = struct.unpack("<I", b)[0]
        try:
            f_val = struct.unpack("<f", b)[0]
            comment = f"{f_val:g}"
        except Exception:
            comment = ""
        src = addr_sources.get(addr, "")
        lines.append(f'    0x{addr:06x}: struct.pack("<I", 0x{hex_val:08x}),  # {comment}  [{src}]')
    lines.append("}")

    if unmapped:
        lines.append(f"\n# --- {len(unmapped)} UNMAPPED (runtime-initialized or BSS) ---")
        for addr in unmapped:
            src = addr_sources.get(addr, "")
            lines.append(f"# 0x{addr:06x}  [{src}]")

    text = "\n".join(lines) + "\n"

    if out_path:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text)
        print(f"# Wrote {len(mapped)} entries to {out_path}", file=sys.stderr)
    else:
        print(text)

    print(f"\n# Summary: {len(mapped)} mapped, {len(unmapped)} unmapped, {len(global_addrs)} total",
          file=sys.stderr)


if __name__ == "__main__":
    main()
