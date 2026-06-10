#!/usr/bin/env python3
"""dump_caller_regsetup.py: For a given callee (hex addr or kb.json name), find
every CALL site to it in the pristine XBE and print the register/stack setup
window so engineers can verify each ported C caller matches the original.

This tool mechanises the evidence-gathering step documented in lift-learnings.md
§10 ("Caller-Site Register Order Contradicts Thunk Convention"). The value →
register comparison against C call sites remains manual review.

Usage:
    python3 tools/audit/dump_caller_regsetup.py 0xacef0
    python3 tools/audit/dump_caller_regsetup.py game_engine_hud_update_player
    python3 tools/audit/dump_caller_regsetup.py 0xacef0 --window 12
    python3 tools/audit/dump_caller_regsetup.py 0xacef0 --program-xbe halo-patched/cachebeta.xbe
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

try:
    import capstone
    from capstone import x86_const as X86
    HAS_CAPSTONE = True
except ImportError:
    HAS_CAPSTONE = False

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
KB_PATH   = REPO_ROOT / "kb.json"
DEFAULT_XBE = REPO_ROOT / "halo-patched" / "cachebeta.xbe"

# 32-bit parent of any sub-register name capstone may return.
_REG32: dict[str, str] = {
    "al": "eax", "ah": "eax", "ax": "eax", "eax": "eax",
    "cl": "ecx", "ch": "ecx", "cx": "ecx", "ecx": "ecx",
    "dl": "edx", "dh": "edx", "dx": "edx", "edx": "edx",
    "bl": "ebx", "bh": "ebx", "bx": "ebx", "ebx": "ebx",
    "si": "esi",                              "esi": "esi",
    "di": "edi",                              "edi": "edi",
}
_TRACKED = set(_REG32.values())  # {"eax","ecx","edx","ebx","esi","edi"}


# ---------------------------------------------------------------------------
# XBE parsing — self-contained; does not import any shared module.
# ---------------------------------------------------------------------------

def _load_xbe(path: Path):
    """Return (raw_bytes, base_va, sections).

    sections = list of (va, vsize, raw_off, raw_size).
    Mirrors the parsing in tools/audit/check_callee_reg_args.py.
    """
    data = path.read_bytes()
    base       = struct.unpack_from("<I", data, 0x104)[0]
    n_sects    = struct.unpack_from("<I", data, 0x11C)[0]
    hdrs_va    = struct.unpack_from("<I", data, 0x120)[0]
    hdr_off    = hdrs_va - base
    sections: list[tuple[int, int, int, int]] = []
    for i in range(n_sects):
        off     = hdr_off + i * 0x38
        va      = struct.unpack_from("<I", data, off + 0x04)[0]
        vsize   = struct.unpack_from("<I", data, off + 0x08)[0]
        raw_off = struct.unpack_from("<I", data, off + 0x0C)[0]
        raw_size= struct.unpack_from("<I", data, off + 0x10)[0]
        sections.append((va, vsize, raw_off, raw_size))
    return data, base, sections


def _read_va(data: bytes, sections, va: int, length: int) -> bytes | None:
    """Read `length` bytes from virtual address `va` out of the XBE image."""
    for (sec_va, sec_vsize, raw_off, raw_size) in sections:
        if sec_va <= va < sec_va + sec_vsize:
            off_in_sec = va - sec_va
            file_off   = raw_off + off_in_sec
            avail      = raw_size - off_in_sec
            if avail <= 0:
                return None
            return data[file_off : file_off + min(length, avail)]
    return None


# ---------------------------------------------------------------------------
# kb.json helpers
# ---------------------------------------------------------------------------

def _load_kb(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _resolve_target(kb: dict, arg: str) -> tuple[int, str | None, str | None]:
    """Return (callee_va, callee_name_or_None, callee_decl_or_None).

    `arg` is either a hex address (0xABCDEF or ABCDEF) or a function name.
    Walks the full kb tree — addresses at top level and nested in object lists.
    """
    # Normalise to a hex string with leading 0x for top-level key lookup.
    addr_int: int | None = None
    name_given: str | None = None

    # Try parsing as hex address first.
    stripped = arg.strip()
    try:
        addr_int = int(stripped, 16)
    except ValueError:
        # Not a hex number — treat as name.
        name_given = stripped

    def _scan(node) -> tuple[int, str | None, str | None] | None:
        """DFS over kb tree, return (va, name, decl) on first match."""
        if isinstance(node, dict):
            if "addr" in node:
                entry_va = int(node["addr"], 16)
                entry_name = node.get("name")
                entry_decl = node.get("decl")
                if addr_int is not None and entry_va == addr_int:
                    return entry_va, entry_name, entry_decl
                if name_given is not None and entry_name == name_given:
                    return entry_va, entry_name, entry_decl
                # Also try matching name embedded in decl when "name" key absent.
                if name_given is not None and entry_decl:
                    # e.g. "void game_engine_hud_update_player(..." → extract symbol
                    import re
                    m = re.search(r'\b(\w+)\s*\(', entry_decl)
                    if m and m.group(1) == name_given:
                        return entry_va, entry_name or name_given, entry_decl
            for v in node.values():
                r = _scan(v)
                if r:
                    return r
        elif isinstance(node, list):
            for item in node:
                r = _scan(item)
                if r:
                    return r
        return None

    # Fast path: top-level hex key lookup.
    if addr_int is not None:
        hex_key = hex(addr_int)
        if hex_key in kb:
            entry = kb[hex_key]
            if isinstance(entry, dict):
                return addr_int, entry.get("name"), entry.get("decl")

    # Full DFS scan (handles nested objects list and name-only match).
    result = _scan(kb)
    if result:
        return result

    if addr_int is not None:
        # Address not in kb.json at all — that's OK; still disassemble.
        return addr_int, None, None

    raise SystemExit(f"ERROR: '{arg}' not found in kb.json as an address or name.")


def _build_function_map(kb: dict) -> list[tuple[int, str | None, bool]]:
    """Return sorted list of (va, name_or_None, ported) for all function entries."""
    result: list[tuple[int, str | None, bool]] = []

    def _walk(node) -> None:
        if isinstance(node, dict):
            if "addr" in node and "decl" in node:
                try:
                    va = int(node["addr"], 16)
                except ValueError:
                    pass
                else:
                    # Skip data entries (no parens in decl means data, not function).
                    if "(" in node.get("decl", ""):
                        result.append((va, node.get("name"), bool(node.get("ported", False))))
            for v in node.values():
                _walk(v)
        elif isinstance(node, list):
            for item in node:
                _walk(item)

    _walk(kb)
    result.sort(key=lambda t: t[0])
    return result


def _containing_function(func_map: list[tuple[int, str | None, bool]], va: int):
    """Return (fn_va, fn_name, fn_ported) for the function that contains `va`."""
    import bisect
    addrs = [t[0] for t in func_map]
    idx = bisect.bisect_right(addrs, va) - 1
    if idx < 0:
        return None, None, None
    return func_map[idx]


# ---------------------------------------------------------------------------
# Disassembly and register-setup analysis
# ---------------------------------------------------------------------------

def _disasm_function(data: bytes, sections, func_va: int,
                     max_bytes: int = 0x4000) -> list:
    """Return capstone instructions for the function starting at `func_va`."""
    code = _read_va(data, sections, func_va, max_bytes)
    if not code:
        return []
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    return list(md.disasm(code, func_va))


def _find_call_sites(insns, callee_va: int) -> list[int]:
    """Return sorted list of instruction indices where CALL targets callee_va."""
    sites = []
    for i, insn in enumerate(insns):
        if insn.mnemonic.lower() == "call":
            # capstone encodes `call imm32` with the target as an int in operands.
            ops = insn.op_str.strip()
            try:
                target = int(ops, 16)
                if target == callee_va:
                    sites.append(i)
            except ValueError:
                pass
    return sites


def _decode_reg_write(insn) -> str | None:
    """Return e.g. 'MOV EAX,EBX' describing what the instruction writes, or None."""
    # We use raw mnemonic+op_str from capstone for display.
    return f"{insn.mnemonic.upper()} {insn.op_str.upper()}"


_CS_AC_WRITE = 2  # capstone CS_AC_WRITE flag


def _last_reg_write_in_window(window_insns) -> dict[str, tuple[str, int]]:
    """For each tracked register, find the last instruction in the window that
    writes it. Returns {reg: (description, addr)}.

    Uses operand-level access flags (CS_AC_WRITE) rather than insn.regs_write,
    because capstone only populates regs_write for implicit side-effects (flags,
    ESP on PUSH, etc.) — not for explicit destination operands in MOV, XOR, etc.
    """
    reg_last: dict[str, tuple[str, int]] = {}
    for insn in window_insns:
        desc = f"{insn.mnemonic.upper()} {insn.op_str.upper()}"
        # Check operands for explicit writes (covers MOV, XOR, LEA, etc.)
        for op in insn.operands:
            if op.type == X86.X86_OP_REG and (op.access & _CS_AC_WRITE):
                rname = insn.reg_name(op.value.reg).lower()
                parent = _REG32.get(rname)
                if parent in _TRACKED:
                    reg_last[parent] = (desc, insn.address)
        # Also check implicit writes (PUSH side-effects on ESP, etc.) — these
        # rarely matter for our tracked GPRs but keep for completeness.
        for rid in insn.regs_write:
            rname = insn.reg_name(rid).lower()
            parent = _REG32.get(rname)
            if parent in _TRACKED and parent not in reg_last:
                reg_last[parent] = (desc, insn.address)
    return reg_last


def _push_sequence(window_insns) -> list[tuple[str, int]]:
    """Return the sequence of PUSH instructions since the last CALL in the window.

    Each entry is (operand_text, address). The sequence is in push order
    (first entry = first push = last cdecl argument).
    """
    # Find index of last CALL in the window (so we only count pushes after it).
    last_call_idx = -1
    for i, insn in enumerate(window_insns):
        if insn.mnemonic.lower() == "call":
            last_call_idx = i

    pushes: list[tuple[str, int]] = []
    for insn in window_insns[last_call_idx + 1:]:
        if insn.mnemonic.lower() == "push":
            pushes.append((insn.op_str.upper(), insn.address))
    return pushes


# ---------------------------------------------------------------------------
# Output formatting
# ---------------------------------------------------------------------------

def _print_site(site_idx: int, total: int, call_insn, window_insns,
                callee_decl: str | None,
                fn_va: int | None, fn_name: str | None, fn_ported: bool | None,
                window_size: int) -> None:
    """Print the full analysis block for one call site."""

    call_va = call_insn.address

    # Header
    print(f"{'=' * 72}")
    print(f"Call site {site_idx}/{total}  @  0x{call_va:x}")

    # Containing function info
    if fn_va is not None:
        ported_str = "ported=true" if fn_ported else "ported=false"
        name_str = fn_name if fn_name else f"FUN_{fn_va:08x}"
        print(f"  in: {name_str}  (0x{fn_va:x}, {ported_str})")
    else:
        print(f"  in: (unknown function)")

    # Callee decl for reference
    if callee_decl:
        print(f"  callee decl: {callee_decl}")

    print()

    # Raw disassembly window
    print(f"  --- last {len(window_insns)} instructions before CALL ---")
    for insn in window_insns:
        marker = " >>" if insn.address == call_va else "   "
        print(f"  {marker} 0x{insn.address:08x}:  {insn.mnemonic:<8}  {insn.op_str}")
    # Print the CALL itself (not in window)
    print(f"   >> 0x{call_insn.address:08x}:  {call_insn.mnemonic:<8}  {call_insn.op_str}")

    print()

    # Decoded summary — last write per register
    reg_writes = _last_reg_write_in_window(window_insns)
    print(f"  --- decoded register loads (last write per reg in window) ---")
    for reg in ("eax", "ecx", "edx", "ebx", "esi", "edi"):
        if reg in reg_writes:
            desc, addr = reg_writes[reg]
            print(f"    {reg.upper():<3}  <-  {desc}  @ 0x{addr:x}")
        else:
            print(f"    {reg.upper():<3}  <-  (no write seen in window)")

    print()

    # Push sequence
    pushes = _push_sequence(window_insns)
    if pushes:
        print(f"  --- PUSH sequence (first push = last cdecl arg) ---")
        n_pushes = len(pushes)
        for rank, (operand, addr) in enumerate(pushes):
            arg_pos = n_pushes - rank  # 1-based; first push = highest arg index
            print(f"    push #{rank + 1}  arg[{arg_pos}]  {operand}  @ 0x{addr:x}")
    else:
        print(f"  --- PUSH sequence: (no pushes found in window) ---")

    print()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Dump register/stack setup at every original CALL site to a callee."
    )
    parser.add_argument(
        "callee",
        help="Callee address (0xABCDEF) or kb.json function name."
    )
    parser.add_argument(
        "--window", type=int, default=12,
        help="Number of instructions before each CALL to show (default: 12)."
    )
    parser.add_argument(
        "--program-xbe", type=Path, default=DEFAULT_XBE,
        dest="xbe_path",
        help=f"Path to pristine XBE (default: {DEFAULT_XBE})."
    )
    args = parser.parse_args()

    if not HAS_CAPSTONE:
        sys.exit("ERROR: capstone not installed. Run: pip install capstone")

    xbe_path: Path = args.xbe_path
    if not xbe_path.exists():
        sys.exit(f"ERROR: XBE not found: {xbe_path}")
    if not KB_PATH.exists():
        sys.exit(f"ERROR: kb.json not found: {KB_PATH}")

    # ---- Load data -------------------------------------------------------
    kb = _load_kb(KB_PATH)
    callee_va, callee_name, callee_decl = _resolve_target(kb, args.callee)
    func_map = _build_function_map(kb)

    print(f"Target callee: {callee_name or hex(callee_va)}  (0x{callee_va:x})")
    if callee_decl:
        print(f"  decl: {callee_decl}")
    print(f"XBE: {xbe_path}")
    print()

    xbe_data, base_va, sections = _load_xbe(xbe_path)

    # ---- Scan every known function for CALL sites ------------------------
    # Build a sorted list of function start VAs to derive function boundaries.
    func_vas = sorted(t[0] for t in func_map)

    all_sites: list[tuple[int, list, object, int | None, str | None, bool | None]] = []
    # each entry: (call_va, window_insns, call_insn, fn_va, fn_name, fn_ported)

    for fn_va, fn_name, fn_ported in func_map:
        # Skip obviously-SDK entries (addresses above game .text).
        if fn_va >= 0x1d0000:
            continue
        # Skip self-calls: a callee calling itself is internal recursion,
        # not a site that a ported C *caller* needs to match.
        if fn_va == callee_va:
            continue

        # Estimate function size from the gap to the next known function.
        import bisect
        idx = bisect.bisect_left(func_vas, fn_va)
        if idx + 1 < len(func_vas):
            max_bytes = min(func_vas[idx + 1] - fn_va, 0x8000)
        else:
            max_bytes = 0x4000

        insns = _disasm_function(xbe_data, sections, fn_va, max_bytes)
        site_indices = _find_call_sites(insns, callee_va)

        for site_idx in site_indices:
            call_insn = insns[site_idx]
            window_start = max(0, site_idx - args.window)
            window_insns = insns[window_start:site_idx]
            all_sites.append((
                call_insn.address,
                window_insns,
                call_insn,
                fn_va,
                fn_name,
                fn_ported,
            ))

    # Sort by call site address for deterministic output.
    all_sites.sort(key=lambda t: t[0])

    total = len(all_sites)
    print(f"Found {total} call site(s) to 0x{callee_va:x}.")
    print()

    if total == 0:
        print("(No CALL imm32 sites found. The callee may be called only indirectly,")
        print(" or the XBE image may differ from expectation.)")
        return

    for i, (call_va, window_insns, call_insn, fn_va, fn_name, fn_ported) in enumerate(all_sites, 1):
        _print_site(
            site_idx=i,
            total=total,
            call_insn=call_insn,
            window_insns=window_insns,
            callee_decl=callee_decl,
            fn_va=fn_va,
            fn_name=fn_name,
            fn_ported=fn_ported,
            window_size=args.window,
        )

    print(f"{'=' * 72}")
    print(f"Total: {total} call site(s) found.")


if __name__ == "__main__":
    main()
