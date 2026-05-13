"""COFF object file loader for x86 (i386) .obj files.

Parses COFF symbol tables to locate functions by name, extracts their code
bytes, and enumerates any relocations within the function body.  Supports both
MSVC-style delinked .obj files (symbols like FUN_00012f80, no leading
underscore) and clang/MSVC-cross style .obj files (symbols with leading
underscore).

Only the IMAGE_FILE_MACHINE_I386 (0x014c) variant is handled.
"""

import struct
import sys
from dataclasses import dataclass, field
from typing import Optional


# ---------------------------------------------------------------------------
# COFF layout constants
# ---------------------------------------------------------------------------
COFF_HEADER_FMT = "<HHIIIHH"  # Machine, NumSections, TimeDateStamp, SymTabPtr,
COFF_HEADER_SIZE = struct.calcsize(COFF_HEADER_FMT)  # NumSymbols, OptHdrSize, Characteristics

SECTION_HEADER_FMT = "<8sIIIIIIHHI"
SECTION_HEADER_SIZE = struct.calcsize(SECTION_HEADER_FMT)

SYMBOL_ENTRY_SIZE = 18  # fixed in COFF spec

IMAGE_FILE_MACHINE_I386 = 0x014C

# Symbol storage class values we care about
IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3

RELOC_SIZE = 10  # 4 (VirtualAddress) + 4 (SymbolTableIndex) + 2 (Type)


@dataclass
class CoffSection:
    name: str
    virtual_size: int
    virtual_address: int
    raw_size: int
    raw_offset: int          # file offset of the raw data
    reloc_offset: int        # file offset of the relocation table
    num_relocs: int
    characteristics: int
    data: bytes = field(default_factory=bytes)


@dataclass
class CoffSymbol:
    name: str
    value: int          # offset within its section (for section symbols)
    section_num: int    # 1-based; 0 = external, -1 = absolute, -2 = debug
    sym_type: int
    storage_class: int


@dataclass
class CoffReloc:
    """A single COFF relocation entry relative to the start of its section."""
    virtual_address: int   # offset from section start
    symbol_name: str
    reloc_type: int


@dataclass
class FunctionSlice:
    """Extracted machine code for one function, with relocation info."""
    name: str               # canonical name (leading _ stripped, or as-is)
    raw_name: str           # exact symbol name as found in the table
    code: bytes             # raw machine code bytes
    relocs: list[CoffReloc] # relocations that fall inside this function
    defined_symbols: set = field(default_factory=set)  # symbols defined in this .obj
    section_offset: int = 0  # offset of this function within its section


class CoffParseError(Exception):
    pass


def _read_coff_string(data: bytes, offset: int, string_table: bytes) -> str:
    """Read an 8-byte COFF name field; resolve long names via string table."""
    field_bytes = data[offset:offset + 8]
    # If first 4 bytes are zero, the next 4 are an offset into the string table
    if field_bytes[:4] == b"\x00\x00\x00\x00":
        str_offset = struct.unpack_from("<I", field_bytes, 4)[0]
        end = string_table.index(b"\x00", str_offset)
        return string_table[str_offset:end].decode("ascii", errors="replace")
    else:
        # Inline name — strip null padding
        return field_bytes.rstrip(b"\x00").decode("ascii", errors="replace")


def load_coff(path: str) -> tuple[list[CoffSection], list[CoffSymbol], bytes]:
    """Parse a COFF .obj file.

    Returns (sections, symbols, string_table_bytes).
    Raises CoffParseError on format errors.
    """
    with open(path, "rb") as f:
        data = f.read()

    if len(data) < COFF_HEADER_SIZE:
        raise CoffParseError(f"{path}: too small to be a COFF file")

    machine, num_sections, _ts, sym_table_ptr, num_symbols, opt_hdr_size, chars = \
        struct.unpack_from(COFF_HEADER_FMT, data, 0)

    if machine != IMAGE_FILE_MACHINE_I386:
        raise CoffParseError(
            f"{path}: unexpected machine type 0x{machine:04x} (expected 0x014c i386)"
        )

    # --- String table (immediately after symbol table) ---
    string_table_offset = sym_table_ptr + num_symbols * SYMBOL_ENTRY_SIZE
    if string_table_offset + 4 <= len(data):
        str_table_size = struct.unpack_from("<I", data, string_table_offset)[0]
        string_table = data[string_table_offset:string_table_offset + str_table_size]
    else:
        string_table = b"\x00\x00\x00\x00"

    # --- Section headers ---
    section_hdr_start = COFF_HEADER_SIZE + opt_hdr_size
    sections: list[CoffSection] = []
    for i in range(num_sections):
        off = section_hdr_start + i * SECTION_HEADER_SIZE
        (raw_name, virt_size, virt_addr, raw_size, raw_off,
         reloc_off, line_off, num_relocs, num_line_nums, characteristics) = \
            struct.unpack_from(SECTION_HEADER_FMT, data, off)

        # Section name: 8-byte field; long names start with /
        name_str = raw_name.rstrip(b"\x00").decode("ascii", errors="replace")
        if name_str.startswith("/"):
            str_off = int(name_str[1:])
            end = string_table.index(b"\x00", str_off)
            name_str = string_table[str_off:end].decode("ascii", errors="replace")

        sec = CoffSection(
            name=name_str,
            virtual_size=virt_size,
            virtual_address=virt_addr,
            raw_size=raw_size,
            raw_offset=raw_off,
            reloc_offset=reloc_off,
            num_relocs=num_relocs,
            characteristics=characteristics,
            data=data[raw_off:raw_off + raw_size] if raw_off and raw_size else b"",
        )
        sections.append(sec)

    # --- Symbol table ---
    symbols: list[CoffSymbol] = []
    i = 0
    while i < num_symbols:
        off = sym_table_ptr + i * SYMBOL_ENTRY_SIZE
        name = _read_coff_string(data, off, string_table)
        value, section_num, sym_type, storage_class, num_aux = \
            struct.unpack_from("<IHHBB", data, off + 8)
        sym = CoffSymbol(
            name=name,
            value=value,
            section_num=section_num,
            sym_type=sym_type,
            storage_class=storage_class,
        )
        symbols.append(sym)
        for _ in range(num_aux):
            symbols.append(sym)
        i += 1 + num_aux  # skip auxiliary records

    return sections, symbols, string_table


def _canonical(name: str) -> str:
    """Strip a leading underscore from a COFF symbol name (MSVC cdecl decoration)."""
    return name.lstrip("_")


def _section_relocs(section: CoffSection, symbols: list[CoffSymbol], raw_data: bytes) -> list[CoffReloc]:
    """Parse relocation entries for a section."""
    relocs = []
    off = section.reloc_offset
    for _ in range(section.num_relocs):
        if off + RELOC_SIZE > len(raw_data):
            break
        virt_addr, sym_idx, reloc_type = struct.unpack_from("<IIH", raw_data, off)
        off += RELOC_SIZE
        sym_name = symbols[sym_idx].name if sym_idx < len(symbols) else f"SYM_{sym_idx}"
        relocs.append(CoffReloc(
            virtual_address=virt_addr,
            symbol_name=sym_name,
            reloc_type=reloc_type,
        ))
    return relocs


def extract_function(obj_path: str, func_name: str) -> FunctionSlice:
    """Extract code bytes and relocations for a named function from a COFF .obj.

    func_name can be given with or without leading underscore; both the
    canonical form and the leading-underscore form are tried.

    Raises CoffParseError if the symbol is not found or has no associated code.
    """
    with open(obj_path, "rb") as f:
        raw_data = f.read()

    sections, symbols, _strtab = load_coff(obj_path)

    # Build canonical name lookup: try both "_name" and "name"
    candidates = {func_name, "_" + func_name, func_name.lstrip("_")}

    target_sym: Optional[CoffSymbol] = None
    for sym in symbols:
        if sym.name in candidates or _canonical(sym.name) in candidates:
            if sym.section_num > 0:  # defined in a section
                target_sym = sym
                break

    if target_sym is None:
        available = [s.name for s in symbols if s.section_num > 0
                     and s.storage_class in (IMAGE_SYM_CLASS_EXTERNAL, IMAGE_SYM_CLASS_STATIC)
                     and "FUN_" in s.name or s.storage_class == IMAGE_SYM_CLASS_EXTERNAL]
        raise CoffParseError(
            f"Symbol '{func_name}' not found in {obj_path}.\n"
            f"Available external symbols: {available[:20]}"
        )

    sec_idx = target_sym.section_num - 1  # 0-based
    if sec_idx >= len(sections):
        raise CoffParseError(
            f"Symbol '{func_name}' points to section {target_sym.section_num} "
            f"but only {len(sections)} sections exist"
        )

    section = sections[sec_idx]
    func_offset = target_sym.value  # offset within section

    # Determine function end: find the next *function* symbol in the same section
    # that comes after our function's start offset.  Only EXTERNAL symbols with
    # function type (sym_type 0x20) are used as boundaries — this avoids
    # LAB_/switchD static labels truncating the extracted slice prematurely.
    # Fall back to the end of the section if no such symbol exists.
    next_offset = len(section.data)
    for sym in symbols:
        if (sym.section_num == target_sym.section_num
                and sym.value > func_offset
                and sym.value < next_offset
                and sym.storage_class == IMAGE_SYM_CLASS_EXTERNAL
                and sym.sym_type == 0x20):
            next_offset = sym.value

    code = section.data[func_offset:next_offset]
    # Strip trailing NOPs
    while code.endswith(b"\x90"):
        code = code[:-1]

    # Collect relocations that fall inside [func_offset, next_offset)
    all_relocs = _section_relocs(section, symbols, raw_data)
    func_relocs = [
        CoffReloc(r.virtual_address - func_offset, r.symbol_name, r.reloc_type)
        for r in all_relocs
        if func_offset <= r.virtual_address < next_offset
    ]

    defined = {s.name for s in symbols if s.section_num > 0}

    return FunctionSlice(
        name=_canonical(target_sym.name),
        raw_name=target_sym.name,
        code=code,
        relocs=func_relocs,
        defined_symbols=defined,
        section_offset=func_offset,
    )


def load_text_section(obj_path: str) -> Optional[bytes]:
    """Return the raw .text section data from a COFF .obj, or None."""
    sections, _, _ = load_coff(obj_path)
    for s in sections:
        if s.name == '.text':
            return s.data
    return None


def list_functions(obj_path: str) -> list[str]:
    """Return canonical names of all external function symbols defined in a COFF obj."""
    _sections, symbols, _strtab = load_coff(obj_path)
    result = []
    for sym in symbols:
        if (sym.section_num > 0
                and sym.storage_class == IMAGE_SYM_CLASS_EXTERNAL
                and sym.sym_type & 0x20):  # type 0x20 = function
            result.append(_canonical(sym.name))
    return result
