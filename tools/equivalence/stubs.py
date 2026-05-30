"""Relocation classification, patching, and callee stubbing for non-leaf emulation.

Provides:
  - classify_relocations(): categorize a function's relocations
  - patch_dir32_relocs(): rewrite DIR32 relocations to globals region
  - patch_rel32_calls(): rewrite CALL targets to sentinel addresses
  - StubManager: intercept calls via Unicorn hooks, run callees in sub-emulator
"""

import math
import struct
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

IMAGE_REL_I386_DIR32 = 0x0006
IMAGE_REL_I386_REL32 = 0x0014

GLOBALS_BASE = 0x00500000
GLOBALS_SIZE = 0x00100000  # 1 MB


def _st80_to_double(b: bytes) -> float:
    """Convert 10-byte x87 extended precision to Python float."""
    mantissa = int.from_bytes(b[:8], 'little')
    exp_sign = int.from_bytes(b[8:10], 'little')
    sign = -1 if (exp_sign >> 15) else 1
    exp = exp_sign & 0x7FFF
    if exp == 0 and mantissa == 0:
        return 0.0
    if exp == 0x7FFF:
        return float('inf') * sign if mantissa == (1 << 63) else float('nan')
    power = exp - 16383 - 63
    try:
        return sign * mantissa * (2.0 ** power)
    except OverflowError:
        return float('inf') * sign


def _write_st0_double(uc, val: float):
    """Write a Python float to Unicorn's ST0 as x87 80-bit extended."""
    from unicorn.x86_const import UC_X86_REG_ST0
    bits = struct.unpack('<Q', struct.pack('<d', val))[0]
    sign = (bits >> 63) & 1
    exp = (bits >> 52) & 0x7FF
    frac = bits & ((1 << 52) - 1)
    if exp == 0 and frac == 0:
        ext = sign << 79
    elif exp == 0x7FF:
        ext = (sign << 79) | (0x7FFF << 64) | (1 << 63) | (frac << 11)
    else:
        ext = (sign << 79) | ((exp - 1023 + 16383) << 64) | (1 << 63) | (frac << 11)
    uc.reg_write(UC_X86_REG_ST0, ext)


@dataclass
class RelocClassification:
    """Result of classifying a function's relocations."""
    category: str           # "leaf", "data_only", "stubbable", "non_leaf"
    dir32_count: int        # number of DIR32 (global data) relocations
    call_count: int         # number of external REL32 (call) relocations
    intra_obj_calls: int    # calls to functions defined in same .obj
    external_symbols: list  # names of unresolvable externals
    reason: str             # human-readable classification reason


def classify_relocations(relocs: list, defined_symbols: set) -> RelocClassification:
    """Classify a function's relocations to determine emulation strategy.

    Categories:
      leaf       - no external relocations at all
      data_only  - only DIR32 refs to global data (no calls)
      stubbable  - has external calls, but all callees are known
      non_leaf   - has unresolvable external references
    """
    if not relocs:
        return RelocClassification("leaf", 0, 0, 0, [], "no relocations")

    dir32_ext = 0
    rel32_ext = 0
    intra_obj = 0
    external = []

    for r in relocs:
        sym = r.symbol_name
        if sym.startswith(".text") or sym.startswith(".rdata"):
            continue
        if sym in defined_symbols:
            intra_obj += 1
            continue

        if r.reloc_type == IMAGE_REL_I386_DIR32:
            dir32_ext += 1
            external.append(sym)
        elif r.reloc_type == IMAGE_REL_I386_REL32:
            rel32_ext += 1
            external.append(sym)
        else:
            external.append(sym)

    if dir32_ext == 0 and rel32_ext == 0:
        return RelocClassification("leaf", 0, 0, intra_obj, [],
                                   "all relocations resolve within .obj")

    if rel32_ext == 0:
        return RelocClassification("data_only", dir32_ext, 0, intra_obj, external,
                                   f"{dir32_ext} global data reference(s)")

    return RelocClassification("stubbable", dir32_ext, rel32_ext, intra_obj, external,
                               f"{rel32_ext} external call(s), {dir32_ext} data ref(s)")


def patch_dir32_relocs(code: bytes, relocs: list, defined_symbols: set,
                       globals_base: int = GLOBALS_BASE,
                       return_slots: bool = False,
                       rdata_map: dict = None):
    """Rewrite DIR32 relocations to point into the globals memory region.

    Each unique external DIR32 symbol gets a 256-byte slot in the globals
    region (enough for most scalar/struct globals).  Section-relative
    relocations (.text, .rdata prefixed) are left untouched.

    Symbols in rdata_map (intra-object cross-section references like .rdata
    constants) also get globals slots, seeded with actual section data.

    Returns a mutable copy of the code with patched addresses.
    If return_slots is True, returns (patched, symbol_slots) where
    symbol_slots maps symbol_name -> slot_address.
    """
    patched = bytearray(code)
    slot_size = 256
    symbol_slots = {}
    rdata_seeds = {}
    next_slot = 0
    if rdata_map is None:
        rdata_map = {}

    for r in relocs:
        if r.reloc_type != IMAGE_REL_I386_DIR32:
            continue
        sym = r.symbol_name
        if sym.startswith(".text") or sym.startswith(".rdata"):
            continue

        is_rdata_ref = sym in rdata_map
        if sym in defined_symbols and not is_rdata_ref:
            continue

        if sym not in symbol_slots:
            symbol_slots[sym] = globals_base + next_slot * slot_size
            next_slot += 1
            if is_rdata_ref:
                rdata_seeds[symbol_slots[sym]] = rdata_map[sym][:slot_size]

        addr = symbol_slots[sym]
        off = r.virtual_address
        if off + 4 <= len(patched):
            struct.pack_into('<I', patched, off, addr)

    if return_slots:
        return patched, symbol_slots, rdata_seeds
    return patched


# Must be within signed-int32 range of CODE_BASE (0x00400000) to avoid
# rel32 displacement overflow when patching CALL instructions.
STUB_BASE = 0x40000000
STUB_SLOT = 0x4000  # 16KB per sentinel slot — enough for real callee code
MAX_RECURSION_DEPTH = 3


@dataclass
class CalleeStub:
    """Pre-loaded callee information for runtime stubbing."""
    name: str
    code: bytes
    abi: dict
    sentinel_addr: int
    has_real_code: bool = False  # True when code is patched oracle bytes, not a trampoline


def patch_rel32_calls(code: bytes, relocs: list, defined_symbols: set,
                      code_base: int = 0x00400000,
                      symbol_sentinels: Optional[dict[str, int]] = None,
                      include_defined: bool = False) -> tuple:
    """Rewrite REL32 call relocations to sentinel addresses.

    Returns (patched_code, stub_map) where stub_map is
    {sentinel_addr: symbol_name} for each redirected call.

    include_defined: when True, also redirect calls to symbols DEFINED in the
    same .obj (named siblings like FUN_xxxx) to sentinels.  Needed for callee
    code run standalone at a sentinel — it has no mapped .text section, so its
    intra-object sibling calls would otherwise keep their original (now wrong)
    displacements.  Section-relative ".text"/".rdata" relocs are still skipped
    (they carry an addend, not a single resolvable symbol).
    """
    patched = bytearray(code)
    stub_map = {}
    if symbol_sentinels is None:
        symbol_sentinels = {}
    next_idx = len(symbol_sentinels)

    for r in relocs:
        if r.reloc_type != IMAGE_REL_I386_REL32:
            continue
        sym = r.symbol_name
        if sym.startswith(".text") or sym.startswith(".rdata"):
            continue
        if sym in defined_symbols and not include_defined:
            continue

        sym_key = sym.lstrip("_")
        sentinel = symbol_sentinels.get(sym_key)
        if sentinel is None:
            sentinel = STUB_BASE + next_idx * STUB_SLOT
            symbol_sentinels[sym_key] = sentinel
            next_idx += 1
        call_addr = code_base + r.virtual_address
        # REL32: displacement = target - (call_addr + 4)
        # The +4 is because the displacement is relative to the end of the instruction
        disp = sentinel - (call_addr + 4)
        off = r.virtual_address
        if off + 4 <= len(patched):
            struct.pack_into('<i', patched, off, disp)
        stub_map[sentinel] = sym

    return bytes(patched), stub_map


class StubManager:
    """Manages callee stubs for non-leaf function emulation.

    Intercepts calls to external functions by patching their targets to
    sentinel addresses and hooking Unicorn's memory fetch errors.
    When a sentinel is hit, runs the callee's oracle code in a
    sub-emulator and copies the return value back.
    """

    def __init__(self, kb_path: Path, delinked_dir: Path):
        self.kb_path = kb_path
        self.delinked_dir = delinked_dir
        self._kb = None
        self._stubs: dict[int, CalleeStub] = {}
        self._stub_names: dict[int, str] = {}
        self._canonical_names: dict[int, str] = {}
        self._depth = 0
        # Real-callee sub-emulation state (populated by prepare_stubs when
        # real_callees is enabled): DIR32 slots the loaded callee code reads
        # from (caller seeds them) and any .rdata constant seeds.
        self._callee_dir32_slots: dict[str, int] = {}
        self._extra_rdata_seeds: dict[int, bytes] = {}
        self._real_code_count = 0

    def _load_kb(self):
        if self._kb is None:
            self._kb = json.loads(self.kb_path.read_text(encoding="utf-8"))
        return self._kb

    def _find_callee_in_kb(self, symbol_name: str) -> Optional[dict]:
        """Look up a callee symbol in kb.json."""
        kb = self._load_kb()
        canon = symbol_name.lstrip("_")
        # Try FUN_XXXXXXXX format
        m = re.match(r'FUN_([0-9a-fA-F]+)', canon)
        if m:
            addr = "0x" + m.group(1).lower().lstrip("0")
            if not addr.endswith("0"):
                addr = "0x" + m.group(1).lower()
        else:
            addr = None

        for obj in kb.get("objects", []):
            for fn in obj.get("functions", []):
                decl = fn.get("decl", "")
                fn_m = re.search(r'\b(\w+)\s*\(', decl)
                fn_name = fn_m.group(1) if fn_m else ""
                if fn_name == canon or (addr and fn.get("addr", "").lower() == addr):
                    return dict(fn, _obj_name=obj.get("name", ""))
        return None

    def _load_callee_code(self, symbol_name: str, kb_entry: dict):
        """Extract callee's oracle code from delinked .obj.

        Returns a FunctionSlice on success, or None if not found.
        """
        _SCRIPT_DIR = Path(__file__).resolve().parent
        sys.path.insert(0, str(_SCRIPT_DIR))
        from coff_loader import extract_function, CoffParseError

        obj_name = kb_entry.get("_obj_name", "").replace(".obj", "").replace("LIBCMT:", "")
        candidates = list(self.delinked_dir.glob(f"{obj_name}.obj"))
        if not candidates:
            candidates = list(self.delinked_dir.glob("*.obj"))

        # Delinked objects name functions FUN_<addr>, but a call site may use the
        # real name (lifted/clang obj) or the FUN_ form (delinked oracle).  Both
        # resolve to the same kb_entry — normalize to FUN_<addr> so the named
        # call site also finds the real oracle body (else it stays a trampoline
        # and diverges from the FUN_-named oracle side).
        fun_syms = []
        addr = kb_entry.get("addr", "")
        if addr:
            try:
                a = int(addr, 16)
                fun_syms = [f"FUN_{a:08x}", f"FUN_{a:08X}"]
            except ValueError:
                pass

        for try_sym in dict.fromkeys([symbol_name, symbol_name.lstrip("_")] + fun_syms):
            for obj_path in candidates:
                try:
                    fs = extract_function(str(obj_path), try_sym)
                    if fs.code:
                        return fs
                except (CoffParseError, Exception):
                    continue
        return None

    # Cap on total real-code callees loaded, to bound a runaway call graph.
    MAX_REAL_CALLEES = 96

    def _register_stub(self, sentinel_addr: int, symbol_name: str):
        """Create a trampoline CalleeStub (+ canonical name) for a sentinel.

        Returns (CalleeStub or None, kb_entry or None).  A stub is only created
        when kb.json has a parseable decl for the callee; otherwise the sentinel
        falls back to the synthetic ret-stub in get_stub_code.
        """
        from abi import parse_decl
        self._stub_names[sentinel_addr] = symbol_name
        kb_entry = self._find_callee_in_kb(symbol_name)
        decl = kb_entry.get("decl", "") if kb_entry else ""
        if not decl:
            return None, kb_entry
        fn_m = re.search(r'\b(\w+)\s*\(', decl)
        if fn_m:
            self._canonical_names[sentinel_addr] = fn_m.group(1)
        try:
            abi = parse_decl(decl)
        except Exception:
            return None, kb_entry
        stub = CalleeStub(name=symbol_name, code=b"", abi=abi,
                          sentinel_addr=sentinel_addr, has_real_code=False)
        self._stubs[sentinel_addr] = stub
        return stub, kb_entry

    def prepare_stubs(self, stub_map: dict, globals_base: int = None,
                      shared_sentinels: dict = None,
                      real_callees: bool = False) -> int:
        """Prepare callee stubs for all symbols in the stub_map.

        By default each callee is a return-0 trampoline (only its call site is
        exercised).  When ``real_callees`` is set, every non-intercept callee
        with loadable delinked oracle code is loaded and patched to run NATIVELY
        in place at its sentinel: its DIR32 globals are redirected to fresh
        seed slots and its REL32 calls to sentinels (recursively, bounded by
        ``MAX_RECURSION_DEPTH`` and ``MAX_REAL_CALLEES``).  This lets iterator
        and helper loops actually execute over snapshot data instead of
        early-exiting on a NULL/zero stub return.

        ``globals_base`` is the first free address in the globals region (past
        the caller's own oracle/lifted slots); ``shared_sentinels`` is the
        symbol->sentinel map shared with the caller's REL32 patching so the
        same callee always resolves to the same sentinel.

        Returns the number of top-level stubs prepared.  Callee DIR32 slots to
        seed are exposed via ``self._callee_dir32_slots`` (the caller runs them
        through _build_globals_seeds) and ``self._extra_rdata_seeds``.
        """
        prepared = 0
        for sentinel_addr, symbol_name in stub_map.items():
            stub, _ = self._register_stub(sentinel_addr, symbol_name)
            if stub is not None:
                prepared += 1

        if real_callees:
            self._load_real_callees(stub_map, globals_base, shared_sentinels)

        return prepared

    def _load_real_callees(self, top_stub_map: dict, globals_base: int,
                           shared_sentinels: dict) -> None:
        """BFS-load native oracle code for non-intercept callees.

        Mutates self._stubs (sets has_real_code + patched code), discovers
        nested callees and registers their sentinels, and accumulates the
        DIR32 slots their code reads into self._callee_dir32_slots.
        """
        from coff_loader import extract_function, CoffParseError  # noqa: F401

        if globals_base is None:
            globals_base = GLOBALS_BASE
        if shared_sentinels is None:
            shared_sentinels = {}
        glob_cursor = globals_base

        worklist = [(sa, sym, 0) for sa, sym in top_stub_map.items()]
        processed = set()
        while worklist:
            sentinel_addr, symbol_name, depth = worklist.pop(0)
            if sentinel_addr in processed:
                continue
            processed.add(sentinel_addr)
            if (self._real_code_count >= self.MAX_REAL_CALLEES
                    or depth >= MAX_RECURSION_DEPTH):
                continue
            # Math/memcpy/assert intrinsics stay as Python intercepts.
            name = self._resolve_name(sentinel_addr)
            if name in self._INTERCEPT_NAMES or name in self._FTOL2_ADDRS:
                continue
            stub = self._stubs.get(sentinel_addr)
            if stub is None:
                continue  # no decl/abi -> synthetic ret-stub
            kb_entry = self._find_callee_in_kb(symbol_name)
            fs = self._load_callee_code(symbol_name, kb_entry) if kb_entry else None
            if fs is None or not fs.code or len(fs.code) > STUB_SLOT:
                continue  # not found / too big for a sentinel slot -> trampoline
            defined = getattr(fs, 'defined_symbols', set())
            rdata = getattr(fs, 'rdata_map', {})
            # DIR32 -> fresh globals slots (each unique global one 256B slot).
            patched, slots, rdata_seeds = patch_dir32_relocs(
                fs.code, fs.relocs, defined,
                globals_base=glob_cursor, return_slots=True, rdata_map=rdata)
            glob_cursor += max(1, len(slots)) * 256
            self._callee_dir32_slots.update(slots)
            self._extra_rdata_seeds.update(rdata_seeds)
            # REL32 -> sentinels, relative to where this callee will be written.
            # include_defined: sibling (intra-object) calls must also redirect
            # to sentinels, since this callee has no mapped .text section here.
            patched2, new_map = patch_rel32_calls(
                bytes(patched), fs.relocs, defined,
                code_base=sentinel_addr, symbol_sentinels=shared_sentinels,
                include_defined=True)
            # Raw E8/E9 calls with NO reloc: the delinker encodes intra-XBE
            # calls as direct displacements to absolute VAs.  At the original
            # base they resolve; relocated to a sentinel they point to garbage.
            # Redirect them to sentinels too (target must be an EXACT known
            # function address — a strong guard against E8 bytes that are really
            # mid-instruction operands).
            callee_base = int(kb_entry.get("addr", "0"), 16) if kb_entry else 0
            if callee_base:
                reloc_offs = {r.virtual_address for r in fs.relocs}
                patched2, raw_map = self._redirect_raw_calls(
                    bytes(patched2), callee_base, reloc_offs,
                    sentinel_addr, shared_sentinels)
                new_map.update(raw_map)
            stub.code = bytes(patched2)
            stub.has_real_code = True
            self._real_code_count += 1
            # Enqueue nested callees discovered in this callee's body.
            for new_sentinel, new_sym in new_map.items():
                if new_sentinel in processed:
                    continue
                if new_sentinel not in self._stubs:
                    self._register_stub(new_sentinel, new_sym)
                worklist.append((new_sentinel, new_sym, depth + 1))

    def _kb_func_addrs(self) -> set:
        """Set of all kb.json function entry addresses (ints), cached."""
        cached = getattr(self, "_func_addr_set", None)
        if cached is not None:
            return cached
        addrs = set()
        for obj in self._load_kb().get("objects", []):
            for fn in obj.get("functions", []):
                a = fn.get("addr", "")
                if a:
                    try:
                        addrs.add(int(a, 16))
                    except ValueError:
                        pass
        self._func_addr_set = addrs
        return addrs

    def _redirect_raw_calls(self, code: bytes, callee_base: int,
                            reloc_offsets: set, sentinel_addr: int,
                            shared_sentinels: dict) -> tuple:
        """Redirect unrelocated E8/E9 calls/jumps to sentinels.

        Only redirects when the computed absolute target is an EXACT kb.json
        function entry address, so a mid-instruction 0xE8/0xE9 byte (whose
        "target" is essentially random) is left untouched.
        Returns (patched_code, {sentinel_addr: symbol_name}).
        """
        func_addrs = self._kb_func_addrs()
        patched = bytearray(code)
        new_map = {}
        next_idx = len(shared_sentinels)
        i = 0
        n = len(patched)
        while i + 5 <= n:
            op = patched[i]
            if op in (0xE8, 0xE9) and (i + 1) not in reloc_offsets:
                disp = int.from_bytes(patched[i + 1:i + 5], "little", signed=True)
                target_va = (callee_base + i + 5 + disp) & 0xFFFFFFFF
                if target_va in func_addrs:
                    sym = f"FUN_{target_va:08x}"
                    sym_key = sym.lstrip("_")
                    sentinel = shared_sentinels.get(sym_key)
                    if sentinel is None:
                        sentinel = STUB_BASE + next_idx * STUB_SLOT
                        shared_sentinels[sym_key] = sentinel
                        next_idx += 1
                    new_disp = sentinel - (sentinel_addr + i + 5)
                    patched[i + 1:i + 5] = (new_disp & 0xFFFFFFFF).to_bytes(4, "little")
                    new_map[sentinel] = sym
                    i += 5
                    continue
            i += 1
        return bytes(patched), new_map

    def has_stub(self, address: int) -> bool:
        return address in self._stub_names

    def get_stub_addresses(self) -> set:
        return set(self._stub_names.keys())

    _INTERCEPT_NAMES = frozenset((
        "csmemcpy", "memcpy", "csstrncpy", "csmemset", "memset",
        "crt_sprintf", "debug_string_to_display",
        "system_exit", "display_assert", "halt_and_catch_fire",
        "ciacos", "ciasin", "ciatan2", "cisin", "cicos",
        "cisqrt", "cilog", "cilog10", "cipow", "cifmod", "citan",
        "datum_get",
        "object_get_and_verify_type",
        "tag_get",
        "real_vector3d_valid", "valid_real_point3d",
        "valid_real_normal3d_perpendicular", "valid_real_vector3d",
    ))
    _FTOL2_ADDRS = frozenset(("fun_001d9068", "_ftol2", "ftol2"))
    # XBE address → _CI* intrinsic name for FUN_XXXXXXXX symbols
    _CRT_MATH_ADDRS = {
        "fun_001d94f0": "ciacos",
    }

    def _resolve_name(self, address: int) -> str:
        """Resolve a stub address to its effective intercept name."""
        raw = self._stub_names.get(address, "").lstrip("_").lower()
        if raw in self._CRT_MATH_ADDRS:
            return self._CRT_MATH_ADDRS[raw]
        canonical = self._canonical_names.get(address, "").lower()
        if canonical in self._CRT_MATH_ADDRS:
            return self._CRT_MATH_ADDRS[canonical]
        if canonical in self._INTERCEPT_NAMES:
            return canonical
        return raw

    def should_intercept(self, address: int) -> bool:
        # Named intercepts always take priority — even if oracle code was loaded.
        name = self._resolve_name(address)
        if name in self._INTERCEPT_NAMES or name in self._FTOL2_ADDRS:
            return True
        stub = self._stubs.get(address)
        # Real-code stubs execute natively via Unicorn; their RET pops the return addr.
        if stub is not None and stub.has_real_code:
            return False
        return False

    def get_stub_code(self, address: int) -> bytes:
        """Return machine code to write at a sentinel address.

        For real-code stubs, returns the oracle bytes directly — the page is
        pre-filled with 0xCC (INT3), so no padding is needed.
        For trampoline stubs, returns a tiny synthetic return sequence.
        """
        stub = self._stubs.get(address)

        if stub is not None and stub.has_real_code:
            return stub.code

        symbol_name = self._resolve_name(address)

        if symbol_name == "fabs":
            return b"\xDD\x44\x24\x04\xD9\xE1\xC3"

        if stub is not None:
            ret_st0 = stub.abi.get('ret_st0', False)
            ret_void = stub.abi.get('ret_void', True)
            conv = stub.abi.get('conv', 'cdecl')
            n_stack_params = sum(1 for p in stub.abi['params'] if not p.reg)
        else:
            ret_st0 = False
            ret_void = False
            conv = 'cdecl'
            n_stack_params = 0

        code = bytearray()
        if ret_st0:
            code += b"\xD9\xEE"  # FLDZ
        elif not ret_void:
            code += b"\x31\xC0"  # XOR EAX, EAX

        if conv == 'stdcall':
            code += b"\xC2" + int(n_stack_params * 4).to_bytes(2, "little")
        else:
            code += b"\xC3"  # RET

        return bytes(code)

    def execute_stub(self, uc, address: int) -> bool:
        """Execute a callee stub at the given sentinel address.

        Runs the callee's oracle code in a sub-emulator, copies the
        return value (EAX or ST0) back to the caller's Unicorn instance.

        Returns True if handled, False if the stub is not available.
        """
        if address not in self._stub_names or self._depth >= MAX_RECURSION_DEPTH:
            return False
        stub = self._stubs.get(address)
        self._depth += 1

        try:
            from unicorn.x86_const import (
                UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ESP,
                UC_X86_REG_ECX, UC_X86_REG_EBP,
                UC_X86_REG_ST0, UC_X86_REG_FPSW, UC_X86_REG_FPTAG,
            )
            from unicorn import UcError, UC_PROT_ALL

            def _safe_write(addr, data):
                """Write to emulator memory, auto-mapping the page if needed."""
                try:
                    uc.mem_write(addr, data)
                except UcError:
                    page = addr & ~0xFFFF
                    uc.mem_map(page, 0x10000)
                    uc.mem_write(page, b'\x00' * 0x10000)
                    uc.mem_write(addr, data)

            def _safe_read(addr, size):
                """Read from emulator memory, auto-mapping the page if needed."""
                try:
                    return bytes(uc.mem_read(addr, size))
                except UcError:
                    page = addr & ~0xFFFF
                    uc.mem_map(page, 0x10000)
                    uc.mem_write(page, b'\x00' * 0x10000)
                    return bytes(uc.mem_read(addr, size))

            # Read caller's current state
            caller_esp = uc.reg_read(UC_X86_REG_ESP)
            symbol_name = self._resolve_name(address)

            if symbol_name in ("csmemcpy", "memcpy"):
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                src = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                size = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if size > 0:
                    data = _safe_read(src, size)
                    _safe_write(dst, data)
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            if symbol_name == "csstrncpy":
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                src = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                n = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if n > 0:
                    data = _safe_read(src, n)
                    idx = data.find(b'\0')
                    if idx != -1:
                        data = data[:idx+1]
                    _safe_write(dst, data)
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            if symbol_name in ("csmemset", "memset"):
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                val = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little") & 0xFF
                n = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if n > 0:
                    _safe_write(dst, bytes([val] * n))
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            if symbol_name == "csstrncpy":
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                src = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                n = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if n > 0:
                    data = bytes(uc.mem_read(src, n))
                    idx = data.find(b'\0')
                    if idx != -1:
                        data = data[:idx+1]
                    uc.mem_write(dst, data)
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            if symbol_name in ("csmemset", "memset"):
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                val = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little") & 0xFF
                n = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if n > 0:
                    uc.mem_write(dst, bytes([val] * n))
                uc.reg_write(UC_X86_REG_EAX, dst)
                return True

            if symbol_name in ("crt_sprintf", "debug_string_to_display"):
                uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name in ("system_exit", "halt_and_catch_fire"):
                uc.emu_stop()
                return True

            if symbol_name in self._FTOL2_ADDRS:
                import struct as _struct
                from unicorn.x86_const import UC_X86_REG_FP0
                # UC_X86_REG_FP0..FP7 are the PHYSICAL x87 data registers; reading
                # one returns a (mantissa: uint64, exponent: uint16) tuple for the
                # full 80-bit value.  UC_X86_REG_ST0..ST7 are LOGICAL but only
                # return the 64-bit mantissa (no exponent word — unusable for decode).
                # Logical ST0 = physical R[TOP]; TOP = FPSW bits 13-11.
                fpsw = uc.reg_read(UC_X86_REG_FPSW)
                old_top = (fpsw >> 11) & 0x7
                fp = uc.reg_read(UC_X86_REG_FP0 + old_top)
                mantissa, exponent_word = fp
                # Reassemble as 10 bytes and use the existing robust helper.
                raw80 = _struct.pack('<QH', mantissa, exponent_word)
                try:
                    val = _st80_to_double(raw80)
                except (OverflowError, ValueError, ZeroDivisionError):
                    val = float('inf')
                try:
                    result = int(val)  # truncates toward zero, matching _ftol2
                except (OverflowError, ValueError):
                    result = 0x80000000  # MSVC _ftol2 saturates to INT_MIN on overflow
                uc.reg_write(UC_X86_REG_EAX, result & 0xFFFFFFFF)
                # Pop ST0 from the x87 stack: increment TOP in FPSW (bits 13-11)
                # and mark the vacated physical slot as empty (11b) in FPTAG.
                new_top = (old_top + 1) & 0x7
                uc.reg_write(UC_X86_REG_FPSW, (fpsw & ~(0x7 << 11)) | (new_top << 11))
                fptag = uc.reg_read(UC_X86_REG_FPTAG)
                uc.reg_write(UC_X86_REG_FPTAG, fptag | (0x3 << (old_top * 2)))
                return True

            if symbol_name == "display_assert":
                uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name == "datum_get":
                # datum_get(data_t *data @stack+4, int datum_handle @stack+8)
                # Returns pointer to entry if salt matches identifier, else NULL.
                try:
                    data_ptr = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                    handle = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                    import sys as _sys
                    _g0_b = bytes(uc.mem_read(0x500000, 4)); _g0 = int.from_bytes(_g0_b, "little"); from unicorn.x86_const import UC_X86_REG_ECX as _ECX_R, UC_X86_REG_EAX as _EAX_R; _ecx_v = uc.reg_read(_ECX_R); _eax_v = uc.reg_read(_EAX_R); print(f"[datum_get] data_ptr=0x{data_ptr:08x} handle=0x{handle:08x} glob0=0x{_g0:08x} ECX=0x{_ecx_v:08x} EAX=0x{_eax_v:08x} esp=0x{caller_esp:08x}", file=_sys.stderr)
                    if data_ptr:
                        identifier = (handle >> 16) & 0xFFFF
                        index = handle & 0xFFFF
                        current_count = int.from_bytes(_safe_read(data_ptr + 0x2e, 2), "little")
                        elem_size = int.from_bytes(_safe_read(data_ptr + 0x22, 2), "little")
                        arr_ptr = int.from_bytes(_safe_read(data_ptr + 0x34, 4), "little")
                        print(f"[datum_get] id={identifier} idx={index} count={current_count} sz={elem_size} arr=0x{arr_ptr:08x}", file=_sys.stderr)
                        if index < current_count and elem_size > 0 and arr_ptr:
                            entry_addr = arr_ptr + index * elem_size
                            salt = int.from_bytes(_safe_read(entry_addr, 2), "little")
                            print(f"[datum_get] entry=0x{entry_addr:08x} salt={salt}", file=_sys.stderr)
                            if salt != 0 and (identifier == 0 or identifier == salt):
                                uc.reg_write(UC_X86_REG_EAX, entry_addr)
                                return True
                except Exception as _e:
                    import sys as _sys
                    print(f"[datum_get] exception: {_e}", file=_sys.stderr)
                uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name == "object_get_and_verify_type":
                # Replicate datum_get + type check without calling the real function.
                # datum array pointer lives at 0x5A8D50 in the XBE's global data.
                # object_header_data_t entry layout (12 bytes):
                #   +0x00 uint16 salt, +0x02 uint16 object_type_byte (byte[3] used),
                #   +0x08 ptr object_data_t*
                # object_data_t.type (int16) is at obj_ptr+0x64.
                try:
                    datum_handle = int.from_bytes(_safe_read(caller_esp + 4, 4), "little")
                    type_mask = int.from_bytes(_safe_read(caller_esp + 8, 4), "little")
                    arr_ptr = int.from_bytes(_safe_read(0x5A8D50, 4), "little")
                    if arr_ptr:
                        max_elements = int.from_bytes(_safe_read(arr_ptr + 0x20, 2), "little")
                        elem_size = int.from_bytes(_safe_read(arr_ptr + 0x22, 2), "little")
                        data_ptr = int.from_bytes(_safe_read(arr_ptr + 0x34, 4), "little")
                        index = datum_handle & 0xFFFF
                        handle_salt = (datum_handle >> 16) & 0xFFFF
                        if (index < max_elements and elem_size >= 12
                                and data_ptr and handle_salt):
                            entry_addr = data_ptr + index * elem_size
                            entry_salt = int.from_bytes(_safe_read(entry_addr, 2), "little")
                            if entry_salt == handle_salt:
                                obj_ptr = int.from_bytes(
                                    _safe_read(entry_addr + 8, 4), "little")
                                if obj_ptr:
                                    raw16 = _safe_read(obj_ptr + 0x64, 2)
                                    obj_type = struct.unpack('<h', raw16)[0]
                                    if type_mask & (1 << (obj_type & 0x1f)):
                                        uc.reg_write(UC_X86_REG_EAX, obj_ptr)
                                        return True
                except Exception:
                    pass
                uc.reg_write(UC_X86_REG_EAX, 0)
                return True

            if symbol_name == "tag_get":
                # Return a pointer to a synthetic projectile tag block.
                # tag_get(group_tag, datum_handle) → void* tag_data
                # Projectile physics (FUN_000f9c40) uses these key offsets:
                #   +0x1c8 max_range (float): must be non-zero to avoid FUN_000f7e40
                #           intra-COFF call in the speed-clamp/deceleration section.
                #   +0x1b8 / +0x198 / +0x220: tag references (-1 = absent).
                _SYNTH_TAG_ADDR = 0x601000
                _SYNTH_TAG_SIZE = 0x280
                _SYNTH_SENTINEL = b'\xDE\xAD\xBE\xEF'
                try:
                    _tail = bytes(uc.mem_read(_SYNTH_TAG_ADDR + _SYNTH_TAG_SIZE - 4, 4))
                    _initialized = (_tail == _SYNTH_SENTINEL)
                except UcError:
                    _initialized = False
                if not _initialized:
                    import struct as _struct
                    _page = _SYNTH_TAG_ADDR & ~0xFFFF
                    try:
                        uc.mem_map(_page, 0x10000)
                    except UcError:
                        pass
                    uc.mem_write(_page, b'\x00' * 0x10000)
                    _tag_bytes = bytearray(_SYNTH_TAG_SIZE)
                    _struct.pack_into('<f',  _tag_bytes, 0x1c8, 60.0)   # max_range (non-zero)
                    _struct.pack_into('<f',  _tag_bytes, 0x1cc, 0.0)    # gravity_scale_normal
                    _struct.pack_into('<f',  _tag_bytes, 0x1d8, 0.0)    # gravity_scale_super
                    _struct.pack_into('<f',  _tag_bytes, 0x1e8, 0.0)    # min_speed
                    _struct.pack_into('<f',  _tag_bytes, 0x1ec, 0.0)    # lock_on_rate (no steering)
                    _struct.pack_into('<i',  _tag_bytes, 0x1b8, -1)     # effect_tag (absent)
                    _struct.pack_into('<i',  _tag_bytes, 0x198, -1)     # burst_centre_effect (absent)
                    _struct.pack_into('<i',  _tag_bytes, 0x220, -1)     # area_damage_tag (absent)
                    _tag_bytes[_SYNTH_TAG_SIZE - 4:] = _SYNTH_SENTINEL
                    uc.mem_write(_SYNTH_TAG_ADDR, bytes(_tag_bytes))
                uc.reg_write(UC_X86_REG_EAX, _SYNTH_TAG_ADDR)
                return True

            # real_vector3d_valid / valid_real_point3d / valid_real_normal3d_perpendicular /
            # valid_real_vector3d: cdecl (float* vec) -> bool
            # Return 1 if all three floats at the pointer are finite, 0 otherwise.
            _VECTOR3D_VALID = frozenset((
                "real_vector3d_valid", "valid_real_point3d",
                "valid_real_normal3d_perpendicular", "valid_real_vector3d",
            ))
            if symbol_name in _VECTOR3D_VALID:
                result = 0
                try:
                    vec_ptr = int.from_bytes(_safe_read(caller_esp + 4, 4), "little")
                    if vec_ptr:
                        raw = _safe_read(vec_ptr, 12)
                        x, y, z = struct.unpack('<fff', raw)
                        if math.isfinite(x) and math.isfinite(y) and math.isfinite(z):
                            result = 1
                except Exception:
                    pass
                uc.reg_write(UC_X86_REG_EAX, result)
                return True

            # _CI* CRT math intrinsics: arg(s) in ST0 (and ST1), result in ST0
            _CI_ONE_ARG = {
                "ciacos": math.acos, "ciasin": math.asin,
                "cisin": math.sin, "cicos": math.cos, "citan": math.tan,
                "cisqrt": math.sqrt, "cilog": math.log, "cilog10": math.log10,
            }
            _CI_TWO_ARG = {
                "ciatan2": math.atan2, "cipow": math.pow, "cifmod": math.fmod,
            }
            if symbol_name in _CI_ONE_ARG:
                import struct as _st
                st0_raw = uc.reg_read(UC_X86_REG_ST0)
                st0_bytes = st0_raw.to_bytes(10, 'little')
                val = _st80_to_double(st0_bytes)
                try:
                    result = _CI_ONE_ARG[symbol_name](val)
                except (ValueError, OverflowError):
                    result = 0.0
                _write_st0_double(uc, result)
                return True

            if symbol_name in _CI_TWO_ARG:
                import struct as _st
                from unicorn.x86_const import UC_X86_REG_ST1
                st0_raw = uc.reg_read(UC_X86_REG_ST0)
                st1_raw = uc.reg_read(UC_X86_REG_ST1)
                st0_val = _st80_to_double(st0_raw.to_bytes(10, 'little'))
                st1_val = _st80_to_double(st1_raw.to_bytes(10, 'little'))
                try:
                    result = _CI_TWO_ARG[symbol_name](st0_val, st1_val)
                except (ValueError, OverflowError, ZeroDivisionError):
                    result = 0.0
                _write_st0_double(uc, result)
                return True

            # For now, return 0 from all stubs.
            # Full sub-emulator execution is deferred to avoid complexity.
            # The key win here is that the calling function still executes
            # correctly through the call site, testing the pre-call setup
            # and post-call usage of the return value.
            if stub is not None:
                ret_st0 = stub.abi.get('ret_st0', False)
                ret_void = stub.abi.get('ret_void', True)
                conv = stub.abi.get('conv', 'cdecl')
                n_stack_params = sum(1 for p in stub.abi['params'] if not p.reg)
            else:
                ret_st0 = False
                ret_void = False
                conv = 'cdecl'
                n_stack_params = 0

            if ret_st0:
                # Push 0.0 onto FPU stack
                pass  # ST0 is already undefined; caller will use it as-is
            elif not ret_void:
                uc.reg_write(UC_X86_REG_EAX, 0)

            # Clean up stack based on calling convention
            if conv == 'stdcall':
                uc.reg_write(UC_X86_REG_ESP, caller_esp + n_stack_params * 4)

        finally:
            self._depth -= 1

        return True
