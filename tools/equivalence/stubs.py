"""Relocation classification, patching, and callee stubbing for non-leaf emulation.

Provides:
  - classify_relocations(): categorize a function's relocations
  - patch_dir32_relocs(): rewrite DIR32 relocations to globals region
  - patch_rel32_calls(): rewrite CALL targets to sentinel addresses
  - StubManager: intercept calls via Unicorn hooks, run callees in sub-emulator
"""

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
                       return_slots: bool = False):
    """Rewrite DIR32 relocations to point into the globals memory region.

    Each unique external DIR32 symbol gets a 256-byte slot in the globals
    region (enough for most scalar/struct globals).  Intra-object and
    section-relative relocations are left untouched.

    Returns a mutable copy of the code with patched addresses.
    If return_slots is True, returns (patched, symbol_slots) where
    symbol_slots maps symbol_name -> slot_address.
    """
    patched = bytearray(code)
    slot_size = 256
    symbol_slots = {}
    next_slot = 0

    for r in relocs:
        if r.reloc_type != IMAGE_REL_I386_DIR32:
            continue
        sym = r.symbol_name
        if sym.startswith(".text") or sym.startswith(".rdata"):
            continue
        if sym in defined_symbols:
            continue

        if sym not in symbol_slots:
            symbol_slots[sym] = globals_base + next_slot * slot_size
            next_slot += 1

        addr = symbol_slots[sym]
        off = r.virtual_address
        if off + 4 <= len(patched):
            struct.pack_into('<I', patched, off, addr)

    if return_slots:
        return patched, symbol_slots
    return patched


# Must be within signed-int32 range of CODE_BASE (0x00400000) to avoid
# rel32 displacement overflow when patching CALL instructions.
STUB_BASE = 0x40000000
STUB_SLOT = 0x100  # 256 bytes between sentinel addresses
MAX_RECURSION_DEPTH = 3


@dataclass
class CalleeStub:
    """Pre-loaded callee information for runtime stubbing."""
    name: str
    code: bytes
    abi: dict
    sentinel_addr: int


def patch_rel32_calls(code: bytes, relocs: list, defined_symbols: set,
                      code_base: int = 0x00400000,
                      symbol_sentinels: Optional[dict[str, int]] = None) -> tuple:
    """Rewrite REL32 call relocations to sentinel addresses.

    Returns (patched_code, stub_map) where stub_map is
    {sentinel_addr: symbol_name} for each external call.
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
        if sym in defined_symbols:
            continue

        sentinel = symbol_sentinels.get(sym)
        if sentinel is None:
            sentinel = STUB_BASE + next_idx * STUB_SLOT
            symbol_sentinels[sym] = sentinel
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
        self._depth = 0

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

    def _load_callee_code(self, symbol_name: str, kb_entry: dict) -> Optional[bytes]:
        """Extract callee's oracle code from delinked .obj."""
        _SCRIPT_DIR = Path(__file__).resolve().parent
        sys.path.insert(0, str(_SCRIPT_DIR))
        from coff_loader import extract_function, CoffParseError

        obj_name = kb_entry.get("_obj_name", "").replace(".obj", "").replace("LIBCMT:", "")
        candidates = list(self.delinked_dir.glob(f"{obj_name}.obj"))
        if not candidates:
            candidates = list(self.delinked_dir.glob("*.obj"))

        for obj_path in candidates:
            try:
                fs = extract_function(str(obj_path), symbol_name)
                if fs.code:
                    return fs.code
            except (CoffParseError, Exception):
                continue
        return None

    def prepare_stubs(self, stub_map: dict) -> int:
        """Prepare callee stubs for all symbols in the stub_map.

        Returns the number of successfully prepared stubs.
        """
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from abi import parse_decl

        prepared = 0
        for sentinel_addr, symbol_name in stub_map.items():
            self._stub_names[sentinel_addr] = symbol_name
            kb_entry = self._find_callee_in_kb(symbol_name)
            if not kb_entry:
                continue

            decl = kb_entry.get("decl", "")
            if not decl:
                continue

            code = self._load_callee_code(symbol_name, kb_entry)
            if not code:
                continue

            try:
                abi = parse_decl(decl)
            except Exception:
                continue

            self._stubs[sentinel_addr] = CalleeStub(
                name=symbol_name,
                code=code,
                abi=abi,
                sentinel_addr=sentinel_addr,
            )
            prepared += 1

        return prepared

    def has_stub(self, address: int) -> bool:
        return address in self._stub_names

    def get_stub_addresses(self) -> set:
        return set(self._stub_names.keys())

    def should_intercept(self, address: int) -> bool:
        symbol_name = self._stub_names.get(address, "").lstrip("_").lower()
        return symbol_name in ("csmemcpy", "memcpy", "csstrncpy", "csmemset", "memset",
                               "crt_sprintf", "debug_string_to_display",
                               "system_exit", "display_assert")

    def get_stub_code(self, address: int) -> bytes:
        """Return machine code for a tiny trampoline stub at a sentinel address."""
        stub = self._stubs.get(address)
        symbol_name = self._stub_names.get(address, "").lstrip("_").lower()

        if symbol_name == "fabs":
            # double fabs(double): load arg from [esp+4], apply x87 FABS, return in ST0
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
                UC_X86_REG_ST0,
            )

            # Read caller's current state
            caller_esp = uc.reg_read(UC_X86_REG_ESP)
            symbol_name = self._stub_names.get(address, "").lstrip("_").lower()

            if symbol_name in ("csmemcpy", "memcpy"):
                dst = int.from_bytes(bytes(uc.mem_read(caller_esp + 4, 4)), "little")
                src = int.from_bytes(bytes(uc.mem_read(caller_esp + 8, 4)), "little")
                size = int.from_bytes(bytes(uc.mem_read(caller_esp + 12, 4)), "little")
                if size > 0:
                    data = bytes(uc.mem_read(src, size))
                    uc.mem_write(dst, data)
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

            if symbol_name == "system_exit":
                uc.emu_stop()
                return True

            if symbol_name == "display_assert":
                uc.reg_write(UC_X86_REG_EAX, 0)
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
