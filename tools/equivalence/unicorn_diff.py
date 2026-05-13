#!/usr/bin/env python3
"""Unicorn-Engine differential tester for pure x86 functions.

Compares MSVC-compiled delinked .obj (oracle) against clang-compiled lifted
.obj (candidate) by running both in separate Unicorn x86 emulators with
identical inputs and comparing CPU+FPU state at RET.

QUICK START
-----------
    # Basic smoke test (leaf function, 100 seeds):
    python3 tools/equivalence/unicorn_diff.py <func_name>

    # Non-leaf function (has external calls like csmemcpy, fabs):
    python3 tools/equivalence/unicorn_diff.py <func_name> --allow-stubs

    # FPU-heavy function with float output buffers:
    python3 tools/equivalence/unicorn_diff.py <func_name> --allow-stubs --float-tolerance 32

    # Detailed per-seed state dump:
    python3 tools/equivalence/unicorn_diff.py <func_name> --allow-stubs --verbose

    # Run self-test against built-in leaf function:
    python3 tools/equivalence/unicorn_diff.py --self-test

FLAGS
-----
    --seeds N          Number of test vectors (default: 100).
    --seed N           Base RNG seed for reproducibility (default: 0).
    --allow-stubs      Enable non-leaf emulation.  Patches DIR32 data
                       relocations, redirects REL32 calls to stub sentinels,
                       and emulates known callees (csmemcpy, fabs, _chkstk,
                       etc.) inline.  Required for any function that calls
                       other functions or references global data.
    --float-tolerance N
                       ULP (Unit in the Last Place) tolerance for float*
                       scratch buffer comparison.  When set, float pointer
                       params are compared element-by-element allowing up to
                       N ULP difference per float.  Non-float slots remain
                       byte-exact.  Auto-detects float* params from the
                       function declaration.  Typical values:
                         16  — tight, catches real bugs (recommended default)
                         32  — moderate, tolerates 1-2 extra rounding steps
                        256  — loose, for functions with long FPU chains
                       1024  — very loose, for extreme count=128 degenerates
    --float-params NAMES
                       Comma-separated param names to treat as float arrays.
                       Only needed when auto-detection guesses wrong.
    --verbose          Print per-seed register dump, FPU state, stub trace,
                       and scratch buffer diff for failures.
    --output-json PATH Write structured JSON result for CI/automation.
    --batch-classify   Scan all delinked/ .obj files and classify each
                       function as leaf / data_only / stubbable / non_leaf.
    --list-funcs OBJ   List function symbols in a .obj file.

HOW IT WORKS
------------
1. Locates the oracle (delinked/*.obj from the original XBE) and the lifted
   candidate (build/*.obj compiled from our C sources).
2. Parses the function declaration from kb.json to determine parameter types,
   calling convention, and return type.
3. Generates typed test seeds: corner-case values for scalars, float arrays
   for pointer params, sanitized for valid-path execution.
4. For each seed, runs both oracle and lifted in separate Unicorn x86 (i386)
   instances with identical memory layout:
     - Stack at 0x7FFE0000
     - Scratch buffer at 0x10000000 (1 KB per pointer param)
     - Globals region at 0x00500000 (DIR32 targets, seeded from known XBE data)
     - Stub sentinels at 0x40000000+ (intercepted calls)
5. Compares EAX/EDX (integer return), ST0 (float return), and scratch buffer.
   With --float-tolerance, float* scratch slots use ULP comparison instead of
   byte-exact.

GLOBALS SEEDING
---------------
The oracle COFF has DIR32 relocations like DAT_002533c8 that reference XBE
data addresses.  These are patched into the GLOBALS region (0x500000).
_KNOWN_GLOBAL_BYTES maps original XBE addresses to their canonical values
(0x2533c0=0.0f, 0x2533c8=1.0f, etc.).  After patching, _build_globals_seeds
writes the correct values into the GLOBALS slots so the oracle reads the same
data as the real binary.  The lifted code accesses these addresses directly
(via *(float*)0x2533c0 in C) and gets correct values from auto-mapped pages.

STUB EMULATION
--------------
External calls are redirected to sentinel addresses.  Known stubs:
  - _chkstk: stack probe, no-op in Unicorn (stack is pre-allocated)
  - csmemcpy / memcpy: reads src, writes dst, returns dst
  - fabs: x87 FABS instruction on ST0
  - _display_assert / system_exit: no-op (return 0)
All other stubs return 0/EAX or 0.0/ST0 depending on ABI.

INTERPRETING RESULTS
--------------------
  "4 passed, 0 failed"           — Oracle and lifted are equivalent for
                                   these seeds.  Genuine equivalence is
                                   likely (especially with --seeds 100+).
  "14 passed, 6 failed"          — Look at the failure details.  If the
                                   diff is in float scratch and ULP < 32,
                                   use --float-tolerance.  If EDX or EAX
                                   differs, there's a real logic bug.
  "0 passed, N failed, M errors"  — Errors mean the emulator crashed
                                   (unhandled stub, unmapped memory, stack
                                   overflow).  Check --verbose output for
                                   the crash address and missing stubs.

To add a new known global, edit _KNOWN_GLOBAL_BYTES near the top of this
file.  To add a new stub handler, see StubManager.execute_stub() in
stubs.py.
"""

import argparse
import json
import os
import re
import struct
import subprocess
import sys
import traceback
from pathlib import Path
from typing import Optional

# unicorn lives in the project venv; allow the script to find it even when
# invoked via system python (e.g. `rtk python3`).
_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_VENV_SP = _REPO_ROOT / ".venv" / "lib" / "python3.12" / "site-packages"
if _VENV_SP.exists() and str(_VENV_SP) not in sys.path:
  sys.path.insert(0, str(_VENV_SP))

try:
  import unicorn  # noqa: F401  (probe)
  _UNICORN_IMPORT_ERROR = ""
except ImportError as exc:
  _UNICORN_IMPORT_ERROR = str(exc)

# ---------------------------------------------------------------------------
# Path setup
# ---------------------------------------------------------------------------
_SCRIPT_DIR = Path(__file__).resolve().parent
_TOOLS_DIR = _SCRIPT_DIR.parent
_REPO_ROOT = _TOOLS_DIR.parent
sys.path.insert(0, str(_TOOLS_DIR))

KB_JSON = _REPO_ROOT / "kb.json"
OBJDIFF_JSON = _REPO_ROOT / "objdiff.json"
DELINKED_DIR = _REPO_ROOT / "delinked"
BUILD_DIR = _REPO_ROOT / "build"

# ---------------------------------------------------------------------------
# Unicorn memory layout constants
# ---------------------------------------------------------------------------
CODE_BASE      = 0x00400000   # where we map the function code
CODE_SIZE      = 0x00040000   # 256 KB — fits largest .text sections
STACK_BASE     = 0x00100000   # bottom of stack region
STACK_SIZE     = 0x00100000   # 1 MB stack
STACK_TOP      = STACK_BASE + STACK_SIZE
SCRATCH_BASE   = 0x10000000   # scratch buffer for pointer args
SCRATCH_SIZE   = 0x00010000   # 64 KB

FAKE_RET_ADDR  = 0xDEADC0DE   # fake return address pushed on stack
MAX_INSN       = 100_000      # hard limit on emulated instructions
TIMEOUT_MS     = 5_000        # 5 second timeout

# Frequently used scalar globals that appear as hardcoded absolute addresses in
# lifted C. Preload them so Unicorn sees the same canonical values as the XBE
# instead of faulting or reading synthetic zero pages.
#
# Values are loaded from known_globals.json (generated by extract_globals.py
# --json) when it exists, falling back to a minimal hardcoded set.
def _load_known_globals():
    """Load global bytes from known_globals.json, falling back to hardcoded defaults."""
    json_path = Path(__file__).resolve().parent / "known_globals.json"
    if json_path.exists():
        import json as _json
        raw = _json.loads(json_path.read_text(encoding="utf-8"))
        result = {}
        for addr_hex, val_hex in raw.items():
            result[int(addr_hex, 16)] = bytes.fromhex(val_hex)
        return result
    return {
        0x253394: struct.pack("<f", 30.0),
        0x253398: struct.pack("<f", 0.5),
        0x2533C0: struct.pack("<f", 0.0),
        0x2533C8: struct.pack("<f", 1.0),
        0x254CB8: struct.pack("<f", 1000.0),
    }


_KNOWN_GLOBAL_BYTES = _load_known_globals()

# ---------------------------------------------------------------------------
# kb.json helpers
# ---------------------------------------------------------------------------

def _load_kb() -> dict:
    with open(KB_JSON) as f:
        return json.load(f)


def _find_kb_entry(kb: dict, func_name: str) -> Optional[dict]:
    """Find a function entry in kb.json by name or hex address."""
    addr_query = func_name if func_name.startswith("0x") else None
    for obj in kb.get("objects", []):
        for fn in obj.get("functions", []):
            decl = fn.get("decl", "")
            # Match function name in decl
            m = re.search(r'\b(\w+)\s*\(', decl)
            fn_name = m.group(1) if m else ""
            if fn_name == func_name or fn.get("addr", "") == addr_query:
                return dict(fn, _obj_name=obj.get("name", ""),
                            _obj_source=obj.get("source", ""))
    return None


def _find_kb_entry_by_addr(kb: dict, addr: str) -> Optional[dict]:
    addr_norm = addr.lower().lstrip("0x")
    for obj in kb.get("objects", []):
        for fn in obj.get("functions", []):
            a = fn.get("addr", "").lower().lstrip("0x")
            if a == addr_norm:
                return dict(fn, _obj_name=obj.get("name", ""),
                            _obj_source=obj.get("source", ""))
    return None


# ---------------------------------------------------------------------------
# Object file discovery
# ---------------------------------------------------------------------------

def _load_objdiff() -> list[dict]:
    with open(OBJDIFF_JSON) as f:
        data = json.load(f)
    return data.get("units", [])


def _find_obj_paths(kb_entry: dict) -> tuple[Optional[Path], Optional[Path]]:
    """Return (delinked_path, build_path) for a kb.json entry.

    Searches objdiff.json units for a unit whose base_path matches the
    obj_name field, then returns both paths.  Also tries direct lookup
    in delinked/.
    """
    obj_name = kb_entry.get("_obj_name", "")
    # Strip decorations like "halo/" prefix or ".obj" suffix
    bare = obj_name.replace(".obj", "").replace("LIBCMT:", "")

    units = _load_objdiff()
    delinked_path = None
    build_path = None

    import re as _re
    # Match bare name at a word boundary to prevent "ai" matching "actors_ai..."
    _bare_pat = _re.compile(r'(?<![A-Za-z0-9_])' + _re.escape(bare) + r'\.obj')
    for unit in units:
        base = unit.get("base_path", "")
        target = unit.get("target_path", "")
        if bare and (_bare_pat.search(base) or _bare_pat.search(target)):
            dp = _REPO_ROOT / base
            tp = _REPO_ROOT / target
            if dp.exists():
                delinked_path = dp
            if tp.exists():
                build_path = tp
            if delinked_path and build_path:
                break

    # Direct fallback: look for <bare>.obj in delinked/
    if not delinked_path:
        candidate = DELINKED_DIR / f"{bare}.obj"
        if candidate.exists():
            delinked_path = candidate

    return delinked_path, build_path


def _find_build_obj_for_source(source_path: str) -> Optional[Path]:
    """Given a source file path, find the corresponding build .obj."""
    units = _load_objdiff()
    for unit in units:
        src = unit.get("metadata", {}).get("source_path", "")
        if src and (source_path.endswith(src) or src.endswith(source_path)):
            tp = _REPO_ROOT / unit.get("target_path", "")
            if tp.exists():
                return tp

    cmake_obj = BUILD_DIR / "CMakeFiles" / "halo.dir" / "src" / "halo" / f"{source_path}.obj"
    if cmake_obj.exists():
        return cmake_obj

    return None


def _function_aliases(func_name: str) -> set[str]:
    aliases = {func_name.lstrip("_")}
    entry = _find_kb_entry(_load_kb(), func_name)
    if not entry:
        return aliases

    decl = entry.get("decl", "")
    addr = entry.get("addr", "")
    m = re.search(r"\b(\w+)\s*\(", decl)
    if m:
        aliases.add(m.group(1))
    if addr:
        aliases.add(f"FUN_{int(addr, 16):08x}")
    return aliases


def _per_function_ref(func_name: str) -> Optional[Path]:
    for alias in _function_aliases(func_name):
        m = re.match(r"FUN_([0-9a-f]{8})$", alias, re.IGNORECASE)
        if not m:
            continue
        candidate = DELINKED_DIR / "functions" / f"{m.group(1).lower()}.obj"
        if candidate.exists():
            return candidate
    return None


def _compile_build_obj_for_source(source_path: str) -> tuple[Optional[Path], Optional[str]]:
    """Compile a standalone candidate .obj for a source file.

    This is used when a TU is not part of HALO_SOURCES yet, but we still want
    Unicorn/Z3 iteration against the current lifted C.
    """
    src_path = _REPO_ROOT / "src" / "halo" / source_path
    if not src_path.exists():
        return None, f"source file does not exist: {src_path}"

    gen_dir = BUILD_DIR / "generated"
    if not gen_dir.exists():
        return None, f"generated headers missing: {gen_dir}"

    out_dir = BUILD_DIR / "equivalence"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_obj = out_dir / (Path(source_path).name + ".obj")

    cmd = [
        "clang",
        "-Wall", "-Werror",
        "-target", "i386-pc-win32",
        "-march=pentium3",
        "-mno-sse",
        "-nostdlib",
        "-ffreestanding",
        "-fno-builtin",
        "-fno-exceptions",
        "-mstack-probe-size=65536",
        f"-I{_REPO_ROOT / 'src'}",
        f"-I{_REPO_ROOT / 'third_party' / 'xbox'}",
        f"-I{gen_dir}",
        "-include", str(_REPO_ROOT / "src" / "common.h"),
        "-c", str(src_path),
        "-o", str(out_obj),
    ]

    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        detail = proc.stderr.strip() or proc.stdout.strip() or "clang compile failed"
        return None, detail
    return out_obj, None


def _seed_known_globals(uc, base: int, size: int):
    end = base + size
    for addr, data in _KNOWN_GLOBAL_BYTES.items():
        if base <= addr and addr + len(data) <= end:
            uc.mem_write(addr, data)


def _build_globals_seeds(*slot_maps: dict) -> dict:
    """Build {slot_address: bytes} from DIR32 slot mappings + _KNOWN_GLOBAL_BYTES.

    Each slot_map has symbol_name -> slot_address.  Symbol names like
    DAT_002533c8 encode the original XBE address.  If that address is in
    _KNOWN_GLOBAL_BYTES, the slot gets seeded with the correct value.
    """
    import re
    seeds = {}
    for smap in slot_maps:
        for sym_name, slot_addr in smap.items():
            m = re.match(r'DAT_([0-9a-fA-F]{4,})', sym_name)
            if not m:
                continue
            orig_addr = int(m.group(1), 16)
            if orig_addr in _KNOWN_GLOBAL_BYTES:
                seeds[slot_addr] = _KNOWN_GLOBAL_BYTES[orig_addr]
    return seeds


# ---------------------------------------------------------------------------
# Unicorn emulation
# ---------------------------------------------------------------------------

def _run_function(code: bytes, abi: dict, arg_values: list,
                  verbose: bool = False, map_globals: bool = False,
                  stub_manager=None, globals_seeds: dict = None,
                  section_code: bytes = None,
                  func_offset: int = 0) -> "state.CPUState":
    """Run a function in a fresh Unicorn instance.

    Returns a CPUState with captured registers and scratch memory.
    If emulation fails, returns a CPUState with .error set.

    map_globals: if True, maps a zeroed globals region at 0x500000
    stub_manager: if set, installs a fetch-unmapped hook to intercept calls
    globals_seeds: dict of {address: bytes} to write into the globals region
                   after zero-initialization (seeds known global values).
    section_code: full .text section bytes (enables intra-object calls)
    func_offset: offset of the target function within section_code
    """
    import unicorn
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_FETCH_UNMAPPED, UC_HOOK_MEM_INVALID
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_EIP

    import sys
    sys.path.insert(0, str(_SCRIPT_DIR))
    from abi import setup_args, SCRATCH_BASE as SCBASE, SCRATCH_SIZE as SCSIZE
    import state as state_mod

    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    last_map_error = [""]
    last_unmapped_access = [""]
    stub_addrs = stub_manager.get_stub_addresses() if stub_manager else set()
    if verbose and stub_addrs:
        stub_pairs = []
        for addr in sorted(stub_addrs):
            name = ""
            if stub_manager is not None:
                name = stub_manager._stub_names.get(addr, "")
            stub_pairs.append(f"{addr:#x}:{name}")
        print("    [stub-map] " + ", ".join(stub_pairs))

    # Map memory regions
    uc.mem_map(CODE_BASE, CODE_SIZE)
    uc.mem_map(STACK_BASE, STACK_SIZE)
    uc.mem_map(SCRATCH_BASE, SCRATCH_SIZE)

    if map_globals:
        from stubs import GLOBALS_BASE, GLOBALS_SIZE
        uc.mem_map(GLOBALS_BASE, GLOBALS_SIZE)
        uc.mem_write(GLOBALS_BASE, b'\x00' * GLOBALS_SIZE)
        if globals_seeds:
            for addr, data in globals_seeds.items():
                uc.mem_write(addr, data)

    # Write function code at CODE_BASE
    if section_code is not None and len(section_code) <= CODE_SIZE:
        combined = bytearray(section_code)
        combined[func_offset:func_offset + len(code)] = code
        uc.mem_write(CODE_BASE, bytes(combined))
        entry_point = CODE_BASE + func_offset
    else:
        uc.mem_write(CODE_BASE, code)
        entry_point = CODE_BASE

    if stub_addrs:
        stub_page_base = min(stub_addrs) & ~0xFFFF
        stub_page_end = (max(stub_addrs) & ~0xFFFF) + 0x10000
        uc.mem_map(stub_page_base, stub_page_end - stub_page_base)
        uc.mem_write(stub_page_base, b"\xCC" * (stub_page_end - stub_page_base))
        for stub_addr in stub_addrs:
            uc.mem_write(stub_addr, stub_manager.get_stub_code(stub_addr))

    # Set up stack: ESP points just below STACK_TOP
    esp = STACK_TOP - 4
    uc.reg_write(UC_X86_REG_ESP, esp)
    uc.reg_write(UC_X86_REG_EBP, esp)

    # Zero scratch buffer
    uc.mem_write(SCRATCH_BASE, b'\x00' * SCRATCH_SIZE)

    # Set up arguments and fill scratch buffer
    scratch_writes = {}
    setup_args(uc, abi, arg_values, scratch_writes)
    for offset, data in scratch_writes.items():
        uc.mem_write(SCRATCH_BASE + offset, data)

    # Push fake return address (FAKE_RET_ADDR acts as termination sentinel)
    esp_after_args = uc.reg_read(UC_X86_REG_ESP)
    esp_after_args -= 4
    uc.mem_write(esp_after_args, struct.pack('<I', FAKE_RET_ADDR))
    uc.reg_write(UC_X86_REG_ESP, esp_after_args)

    entry_esp = esp_after_args  # ESP at function entry (after pushing ret addr)

    # Track instruction count
    insn_count = [0]
    stub_trace_count = [0]

    def hook_code(uc, address, size, user_data):
        insn_count[0] += 1
        if verbose and address in stub_addrs and stub_trace_count[0] < 64:
            symbol_name = ""
            if stub_manager is not None:
                symbol_name = stub_manager._stub_names.get(address, "")
            print(f"    [stub] {symbol_name or hex(address)} @ {address:#x}")
            stub_trace_count[0] += 1
        if address in stub_addrs and stub_manager is not None and stub_manager.should_intercept(address):
            if stub_manager.execute_stub(uc, address):
                cur_esp = uc.reg_read(UC_X86_REG_ESP)
                ret_addr_bytes = uc.mem_read(cur_esp, 4)
                ret_addr = struct.unpack('<I', bytes(ret_addr_bytes))[0]
                uc.reg_write(UC_X86_REG_ESP, cur_esp + 4)
                uc.reg_write(UC_X86_REG_EIP, ret_addr)

    uc.hook_add(UC_HOOK_CODE, hook_code)

    from unicorn import UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE_UNMAPPED

    known_pages = {addr & ~0xFFFF for addr in _KNOWN_GLOBAL_BYTES}

    def hook_mem_invalid(uc, access, address, size, value, user_data):
        if address in stub_addrs:
            stub_manager.execute_stub(uc, address)
            cur_esp = uc.reg_read(UC_X86_REG_ESP)
            ret_addr_bytes = uc.mem_read(cur_esp, 4)
            ret_addr = struct.unpack('<I', bytes(ret_addr_bytes))[0]
            uc.reg_write(UC_X86_REG_ESP, cur_esp + 4)
            uc.reg_write(UC_X86_REG_EIP, ret_addr)
            return True
        last_unmapped_access[0] = (
            f"invalid addr={address:#x} size={size} access={access} value={value:#x}"
        )
        return False

    uc.hook_add(UC_HOOK_MEM_INVALID, hook_mem_invalid)

    # Non-leaf support: handle unmapped memory access
    if map_globals:
        _mapped_regions = set()

        def hook_mem_unmapped(uc, access, address, size, value, user_data):
            # Auto-map a 64KB page for any unmapped read/write
            page_base = address & ~0xFFFF
            last_unmapped_access[0] = (
                f"page={page_base:#x} addr={address:#x} size={size} access={access}"
            )
            if page_base not in _mapped_regions:
                try:
                    uc.mem_map(page_base, 0x10000)
                    uc.mem_write(page_base, b'\x00' * 0x10000)
                    _seed_known_globals(uc, page_base, 0x10000)
                    _mapped_regions.add(page_base)
                    return True
                except Exception as exc:
                    last_map_error[0] = (
                        f"page={page_base:#x} addr={address:#x} size={size} access={access}: {exc}"
                    )
                    return False
            return False

        uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED, hook_mem_unmapped)
        uc.hook_add(UC_HOOK_MEM_WRITE_UNMAPPED, hook_mem_unmapped)
    else:
        def hook_known_globals(uc, access, address, size, value, user_data):
            from unicorn import UC_MEM_WRITE_UNMAPPED
            page_base = address & ~0xFFFF
            last_unmapped_access[0] = (
                f"known-page={page_base:#x} addr={address:#x} size={size} access={access}"
            )
            is_write = (access == UC_MEM_WRITE_UNMAPPED)
            if page_base not in known_pages and not is_write:
                return False
            try:
                uc.mem_map(page_base, 0x10000)
                uc.mem_write(page_base, b'\x00' * 0x10000)
                _seed_known_globals(uc, page_base, 0x10000)
                return True
            except Exception as exc:
                last_map_error[0] = (
                    f"known-page={page_base:#x} addr={address:#x} size={size} access={access}: {exc}"
                )
                return False

        uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED, hook_known_globals)
        uc.hook_add(UC_HOOK_MEM_WRITE_UNMAPPED, hook_known_globals)

    # Stub interception: handle fetch from sentinel addresses
    if stub_manager:
        def hook_fetch_unmapped(uc, access, address, size, value, user_data):
            if address in stub_addrs:
                stub_manager.execute_stub(uc, address)
                cur_esp = uc.reg_read(UC_X86_REG_ESP)
                ret_addr_bytes = uc.mem_read(cur_esp, 4)
                ret_addr = struct.unpack('<I', bytes(ret_addr_bytes))[0]
                uc.reg_write(UC_X86_REG_ESP, cur_esp + 4)
                uc.reg_write(UC_X86_REG_EIP, ret_addr)
                return True
            if address == FAKE_RET_ADDR:
                return False
            return False

        uc.hook_add(UC_HOOK_MEM_FETCH_UNMAPPED, hook_fetch_unmapped)

    err_msg = None
    try:
        uc.emu_start(entry_point, entry_point + len(code), timeout=TIMEOUT_MS * 1000,
                     count=MAX_INSN)
    except unicorn.UcError as e:
        err_str = str(e)
        cur_eip = uc.reg_read(UC_X86_REG_EIP)
        # UC_ERR_FETCH_UNMAPPED at FAKE_RET_ADDR means clean return
        if "fetch" in err_str.lower() or "Fetch" in err_str:
            if cur_eip != FAKE_RET_ADDR:
                err_msg = f"{err_str} [eip={cur_eip:#x}]"
        else:
            if last_map_error[0]:
                err_msg = f"{err_str} [{last_map_error[0]}]"
            elif last_unmapped_access[0]:
                err_msg = f"{err_str} [{last_unmapped_access[0]}]"
            else:
                err_msg = err_str

    s = state_mod.capture(uc, SCRATCH_BASE, SCRATCH_SIZE, entry_esp + 4)
    s.error = err_msg
    s.insn_count = insn_count[0]
    return s


# ---------------------------------------------------------------------------
# Relocation checker
# ---------------------------------------------------------------------------

def _check_relocations(func_slice, label: str) -> bool:
    """Return True if the function has no unresolvable external relocations.

    Relocations that reference symbols defined in the same .obj are safe
    (intra-object calls/data).  Only truly external symbols (not in
    defined_symbols and not section-relative) are rejected.
    """
    if not func_slice.relocs:
        return True

    defined = getattr(func_slice, 'defined_symbols', set())
    ok = True
    for r in func_slice.relocs:
        sym = r.symbol_name
        if sym.startswith(".text") or sym.startswith(".rdata"):
            continue
        if sym in defined:
            continue
        print(f"  [RELOC] {label}: '{sym}' at +0x{r.virtual_address:x} — external, cannot emulate")
        ok = False
    return ok


# ---------------------------------------------------------------------------
# Pure-leaf cache (consumed by tools/llm_auto_lift.py for selection scoring)
# ---------------------------------------------------------------------------

_LEAF_CACHE_PATH = _REPO_ROOT / "tools" / "equivalence" / "leaf_cache.json"


def _record_leaf_classification(addr: str, is_leaf: bool) -> None:
    """Persist a classification entry to leaf_cache.json.

    Uses the extended schema: each entry is a dict with at least a "class" key.
    Legacy string entries are upgraded on read.
    """
    if not addr:
        return
    norm = addr if addr.startswith("0x") else hex(int(addr, 0))
    norm = norm.lower()
    try:
        if _LEAF_CACHE_PATH.exists():
            data = json.loads(_LEAF_CACHE_PATH.read_text(encoding="utf-8"))
        else:
            data = {}
    except (OSError, json.JSONDecodeError):
        data = {}
    cat = "leaf" if is_leaf else "non_leaf"
    existing = data.get(norm)
    if isinstance(existing, dict):
        existing["class"] = cat
        data[norm] = existing
    else:
        data[norm] = {"class": cat}
    try:
        _LEAF_CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
        _LEAF_CACHE_PATH.write_text(
            json.dumps(dict(sorted(data.items())), indent=2) + "\n",
            encoding="utf-8",
        )
    except OSError:
        pass


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

SELF_TEST_FUNC = "vector3d_scale_add"


def _run_self_test(verbose: bool = False) -> int:
    """Run a smoke test against vector3d_scale_add.

    Returns 0 on success, 1 on failure.
    """
    print(f"[self-test] Running against '{SELF_TEST_FUNC}' ...")
    return run_diff(SELF_TEST_FUNC, num_seeds=20, base_seed=0xC0FFEE,
                    verbose=verbose, save_log=False)


# ---------------------------------------------------------------------------
# Main diff runner
# ---------------------------------------------------------------------------

def run_diff(func_name: str, num_seeds: int = 100, base_seed: int = 0,
             verbose: bool = False, save_log: bool = True,
             output_json: Optional[Path] = None,
             record_leaf: bool = True, z3_equiv: bool = False,
             allow_stubs: bool = False,
             float_tolerance_ulp: int = 0,
             float_tolerance_params: list = None,
             skip_esp: bool = False) -> int:
    """Run the differential test.  Returns 0 if all pass, 1 if any diverge."""

    sys.path.insert(0, str(_SCRIPT_DIR))
    from coff_loader import extract_function, CoffParseError
    from abi import parse_decl
    from seeds import generate_seeds
    import state as state_mod

    log_lines = []

    def log(msg: str = ""):
        print(msg)
        log_lines.append(msg)

    def finish(status: str, applicable: bool, reason: Optional[str],
               exit_code: int, **extra) -> int:
        payload = {
            "target": func_name,
            "status": status,
            "applicable": applicable,
            "reason": reason,
            "passed": 0,
            "failed": 0,
            "errors": 0,
            "seeds": 0,
            "log_path": None,
        }
        payload.update(extra)

        if save_log:
            log_dir = _REPO_ROOT / "artifacts" / "equivalence"
            log_dir.mkdir(parents=True, exist_ok=True)
            log_path = log_dir / f"{func_name}_smoke.log"
            with open(log_path, "w") as f:
                f.write('\n'.join(log_lines) + '\n')
            payload["log_path"] = str(log_path)
            print(f"\n  Log saved to: {log_path}")

        if output_json:
            output_json.parent.mkdir(parents=True, exist_ok=True)
            output_json.write_text(json.dumps(payload, indent=2) + "\n",
                                   encoding="utf-8")

        return exit_code

    log(f"=== unicorn_diff: {func_name} ===")

    if _UNICORN_IMPORT_ERROR:
        log("ERROR: unicorn not importable. Activate the project venv or install:")
        log(f"  {_REPO_ROOT}/.venv/bin/python3 -m pip install unicorn lief")
        return finish("not_applicable", False, "unicorn_unavailable", 2)

    # --- Locate kb.json entry ---
    kb = _load_kb()
    entry = _find_kb_entry(kb, func_name)
    if not entry:
        # Try as address
        entry = _find_kb_entry_by_addr(kb, func_name)
    if not entry:
        log(f"ERROR: '{func_name}' not found in kb.json")
        return finish("error", True, "missing_kb_entry", 1)

    decl = entry.get("decl", "")
    addr = entry.get("addr", "")
    obj_name = entry.get("_obj_name", "")
    log(f"  decl : {decl}")
    log(f"  addr : {addr}")
    log(f"  obj  : {obj_name}")

    if not decl:
        log("ERROR: no 'decl' in kb.json entry")
        return finish("error", True, "missing_decl", 1)

    # --- Parse ABI ---
    abi = parse_decl(decl)
    log(f"  conv : {abi['conv']}")
    log(f"  params: {[(p.name, p.c_type, p.reg) for p in abi['params']]}")
    log(f"  return: {abi['return_type']} (st0={abi['ret_st0']}, edx_eax={abi['ret_edx_eax']})")

    if float_tolerance_ulp > 0 and float_tolerance_params is None:
        float_tolerance_params = [
            p.name for p in abi['params']
            if p.is_pointer and 'float' in p.c_type
        ]

    float_tolerance_slot_indices = None
    if float_tolerance_ulp > 0 and float_tolerance_params:
        ptr_idx = 0
        float_tolerance_slot_indices = []
        for p in abi['params']:
            if p.is_pointer:
                if p.name in float_tolerance_params:
                    float_tolerance_slot_indices.append(ptr_idx)
                ptr_idx += 1
        if float_tolerance_params:
            log(f"  float-tolerance: {float_tolerance_ulp} ULP for {float_tolerance_params}")

    # --- Locate .obj files ---
    delinked_path, build_path = _find_obj_paths(entry)
    build_compiled_on_demand = False

    # Try source path from kb.json entry
    if not build_path and entry.get("_obj_source"):
        build_path = _find_build_obj_for_source(entry["_obj_source"])

    if not delinked_path:
        # Try matching address to a FUN_XXXXXXXX name in delinked/
        addr_sym = f"FUN_{int(addr, 16):08X}" if addr.startswith("0x") else None
        if addr_sym:
            for d in DELINKED_DIR.glob("*.obj"):
                try:
                    result = subprocess.run(
                        ["llvm-objdump", "-t", str(d)],
                        capture_output=True, text=True
                    )
                    if addr_sym in result.stdout:
                        delinked_path = d
                        break
                except Exception:
                    pass

    if not delinked_path:
        log(f"ERROR: cannot find delinked .obj for '{obj_name}'")
        log(f"  Searched delinked/ for {obj_name}.obj")
        return finish("not_applicable", False, "missing_delinked_reference", 2)

    if not build_path:
        compile_detail = None
        if entry.get("_obj_source"):
            build_path, compile_detail = _compile_build_obj_for_source(entry["_obj_source"])
        if build_path:
            build_compiled_on_demand = True
            log(f"  build   : {build_path} (compiled on demand)")
        else:
            log(f"ERROR: cannot find build .obj for '{obj_name}'")
            if compile_detail:
                log(f"  on-demand compile failed: {compile_detail}")
            log(f"  Run: python3 tools/build/build.py -q --target halo")
            return finish("not_applicable", False, "missing_build_object", 2)

    log(f"  delinked: {delinked_path}")
    if not build_compiled_on_demand:
        log(f"  build   : {build_path}")

    # --- Extract function slices ---
    # The delinked obj uses FUN_00XXXXXX naming; the build obj uses
    # _funcname (MSVC cdecl) or funcname.
    addr_int = int(addr, 16) if addr.startswith("0x") else int(addr, 16)
    delinked_sym = f"FUN_{addr_int:08x}"

    try:
        oracle_slice = extract_function(str(delinked_path), delinked_sym)
    except CoffParseError as e:
        # Try without leading underscore / with function name
        try:
            oracle_slice = extract_function(str(delinked_path), func_name)
        except CoffParseError as e2:
            per_func_ref = _per_function_ref(func_name)
            if per_func_ref:
                try:
                    oracle_slice = extract_function(str(per_func_ref), delinked_sym)
                    delinked_path = per_func_ref
                except CoffParseError as e3:
                    log(f"ERROR extracting oracle: {e}")
                    log(f"  (also tried '{func_name}': {e2})")
                    log(f"  (also tried split ref '{per_func_ref.name}': {e3})")
                    return finish("not_applicable", False, "oracle_extract_failed", 2)
            else:
                log(f"ERROR extracting oracle: {e}")
                log(f"  (also tried '{func_name}': {e2})")
                return finish("not_applicable", False, "oracle_extract_failed", 2)

    try:
        lifted_slice = extract_function(str(build_path), func_name)
    except CoffParseError as e:
        log(f"ERROR extracting lifted: {e}")
        return finish("not_applicable", False, "lifted_extract_failed", 2)

    log(f"  oracle code: {len(oracle_slice.code)} bytes, {len(oracle_slice.relocs)} relocs")
    log(f"  lifted code: {len(lifted_slice.code)} bytes, {len(lifted_slice.relocs)} relocs")

    from coff_loader import load_text_section
    oracle_text = load_text_section(str(delinked_path))
    lifted_text = load_text_section(str(build_path))

    if not oracle_slice.code:
        log("ERROR: oracle code is empty")
        return finish("error", True, "empty_oracle_code", 1)
    if not lifted_slice.code:
        log("ERROR: lifted code is empty")
        return finish("error", True, "empty_lifted_code", 1)

    # --- Check for external relocations ---
    oracle_ok = _check_relocations(oracle_slice, "oracle")
    lifted_ok = _check_relocations(lifted_slice, "lifted")
    is_leaf = oracle_ok and lifted_ok
    if record_leaf:
        _record_leaf_classification(addr, is_leaf)

    # Non-leaf: try stubs if allowed
    use_stubs = False
    stub_manager = None
    globals_seeds = None
    oracle_code_patched = oracle_slice.code
    lifted_code_patched = lifted_slice.code
    if not is_leaf and allow_stubs:
        from stubs import (classify_relocations, patch_dir32_relocs,
                           patch_rel32_calls, StubManager, GLOBALS_BASE, GLOBALS_SIZE)
        orc_cls = classify_relocations(oracle_slice.relocs,
                                       getattr(oracle_slice, 'defined_symbols', set()))
        lft_cls = classify_relocations(lifted_slice.relocs,
                                       getattr(lifted_slice, 'defined_symbols', set()))
        log(f"  oracle class: {orc_cls.category} ({orc_cls.reason})")
        log(f"  lifted class: {lft_cls.category} ({lft_cls.reason})")

        # Patch DIR32 relocations for both
        orc_defined = getattr(oracle_slice, 'defined_symbols', set())
        lft_defined = getattr(lifted_slice, 'defined_symbols', set())
        oracle_code_patched, orc_data_slots = patch_dir32_relocs(
            oracle_slice.code, oracle_slice.relocs, orc_defined, return_slots=True)
        oracle_code_patched = bytes(oracle_code_patched)
        lifted_code_patched, lft_data_slots = patch_dir32_relocs(
            lifted_slice.code, lifted_slice.relocs, lft_defined, return_slots=True)
        lifted_code_patched = bytes(lifted_code_patched)

        globals_seeds = _build_globals_seeds(orc_data_slots, lft_data_slots)
        shared_stub_sentinels = {}
        orc_stub_map = {}
        lft_stub_map = {}

        # Patch REL32 calls for oracle
        if orc_cls.call_count > 0:
            oracle_code_patched, orc_stub_map = patch_rel32_calls(
                bytes(oracle_code_patched), oracle_slice.relocs, orc_defined,
                symbol_sentinels=shared_stub_sentinels)

        # For lifted code, also patch REL32 if present
        if lft_cls.call_count > 0:
            lifted_code_patched, lft_stub_map = patch_rel32_calls(
                bytes(lifted_code_patched), lifted_slice.relocs, lft_defined,
                symbol_sentinels=shared_stub_sentinels)

        combined_stub_map = dict(orc_stub_map)
        combined_stub_map.update(lft_stub_map)
        if combined_stub_map:
            stub_mgr = StubManager(KB_JSON, DELINKED_DIR)
            n_prepared = stub_mgr.prepare_stubs(combined_stub_map)
            log(f"  stubs prepared: {n_prepared}/{len(combined_stub_map)}")
            stub_manager = stub_mgr
            use_stubs = True

        if orc_cls.category in ("data_only", "leaf"):
            use_stubs = True  # DIR32 patching alone is enough

    if not is_leaf and not use_stubs:
        log("ERROR: function has external relocations — cannot emulate without full linker.")
        log("  (This function calls other functions or references globals.)")
        log("  Solution: use --allow-stubs, or choose a pure leaf function.")
        return finish("not_applicable", False, "external_relocations", 2)

    # --- Z3 formal equivalence proof (optional) ---
    if z3_equiv and is_leaf:
        try:
            from z3_equiv import prove_equivalence
            log("\n  Attempting Z3 formal equivalence proof...")
            eq_result = prove_equivalence(oracle_slice.code, lifted_slice.code, abi)
            if eq_result.proven:
                log(f"  Z3 PROVEN EQUIVALENT")
                # Update cache
                if record_leaf:
                    try:
                        cache_data = json.loads(_LEAF_CACHE_PATH.read_text(encoding="utf-8"))
                    except (OSError, json.JSONDecodeError):
                        cache_data = {}
                    norm = addr.lower() if addr.startswith("0x") else hex(int(addr, 0)).lower()
                    entry = cache_data.get(norm, {})
                    if isinstance(entry, str):
                        entry = {"class": entry}
                    entry["z3_proven"] = True
                    cache_data[norm] = entry
                    _LEAF_CACHE_PATH.write_text(
                        json.dumps(dict(sorted(cache_data.items())), indent=2) + "\n",
                        encoding="utf-8")
                return finish("pass", True, None, 0,
                              passed=0, failed=0, errors=0, seeds=0,
                              z3_proven=True)
            elif eq_result.counterexample:
                log(f"  Z3 found divergence: {eq_result.counterexample}")
                log(f"  Falling through to Unicorn to confirm...")
            elif eq_result.not_applicable:
                log(f"  Z3 not applicable: {eq_result.reason}")
            elif eq_result.timeout:
                log(f"  Z3 timeout: {eq_result.reason}")
        except ImportError:
            pass
        except Exception as e:
            log(f"  Z3 equiv error: {e}")

    # --- Generate seeds (Z3 branch-coverage + random/corner) ---
    seed_safe_mode = not is_leaf
    z3_extra = []
    if seed_safe_mode:
        log("  seed mode: non-leaf valid-path execution (z3 branch seeds disabled)")
    else:
        try:
            from z3_seeds import extract_branch_seeds
            z3_extra = extract_branch_seeds(oracle_slice.code, abi)
            if z3_extra:
                log(f"  z3 branch seeds: {len(z3_extra)}")
        except ImportError:
            pass
        except Exception as e:
            log(f"  z3 seed warning: {e}")

    params = abi['params']
    seeds = generate_seeds(params, num_seeds=num_seeds, base_seed=base_seed,
                           z3_seeds=z3_extra, safe_mode=seed_safe_mode)
    log(f"\n  Running {len(seeds)} seeds...")
    log("")

    passed = 0
    failed = 0
    errors = 0
    first_diff = None

    for si, seed_vec in enumerate(seeds):
        seed_label = f"seed[{si:3d}]"

        # Run oracle
        try:
            oracle_state = _run_function(oracle_code_patched, abi, seed_vec,
                                         verbose=verbose, map_globals=use_stubs,
                                         stub_manager=stub_manager,
                                         globals_seeds=globals_seeds,
                                         section_code=oracle_text,
                                         func_offset=oracle_slice.section_offset)
        except Exception as exc:
            log(f"  {seed_label} ORACLE-ERROR: {exc}")
            errors += 1
            continue

        # Run lifted
        try:
            lifted_state = _run_function(lifted_code_patched, abi, seed_vec,
                                         verbose=verbose, map_globals=use_stubs,
                                         stub_manager=stub_manager,
                                         globals_seeds=globals_seeds,
                                         section_code=lifted_text,
                                         func_offset=lifted_slice.section_offset)
        except Exception as exc:
            log(f"  {seed_label} LIFTED-ERROR: {exc}")
            errors += 1
            continue

        if oracle_state.error:
            log(f"  {seed_label} ORACLE-CRASH: {oracle_state.error}")
            errors += 1
            continue
        if lifted_state.error:
            log(f"  {seed_label} LIFTED-CRASH: {lifted_state.error}")
            errors += 1
            continue

        # Determine what to compare based on return type
        ret_eax = not abi['ret_void'] and not abi['ret_st0'] and not abi.get('ret_is_ptr', False)
        diff = state_mod.compare(
            oracle_state, lifted_state,
            check_scratch=True,
            ret_eax=ret_eax,
            ret_eax_bits=abi.get('ret_bits', 32),
            ret_edx_eax=abi['ret_edx_eax'],
            ret_st0=abi['ret_st0'],
            check_st_count=1 if abi['ret_st0'] else 0,
            scratch_float_tolerance_ulp=float_tolerance_ulp,
            scratch_float_params=float_tolerance_slot_indices,
            st_tolerance_ulp=float_tolerance_ulp,
            check_esp=is_leaf if not skip_esp else False,
        )

        if diff.has_differences():
            failed += 1
            if first_diff is None:
                first_diff = (si, seed_vec, diff, oracle_state, lifted_state)
            if verbose:
                log(f"  {seed_label} FAIL: {diff.summary()}")
                log(state_mod.format_state_verbose(oracle_state, "oracle"))
                log(state_mod.format_state_verbose(lifted_state, "lifted"))
                if diff.scratch_differs:
                    for line in _summarize_scratch_diff(params, oracle_state.scratch_data,
                                                        lifted_state.scratch_data):
                        log(line)
            else:
                log(f"  {seed_label} FAIL: {diff.summary()}")
        else:
            passed += 1
            if verbose and si < 3:
                log(f"  {seed_label} PASS")
                log(state_mod.format_state_verbose(oracle_state, "oracle"))

    log("")
    log(f"=== RESULTS: {passed} passed, {failed} failed, {errors} errors / {len(seeds)} seeds ===")

    if first_diff and not verbose:
        si, seed_vec, diff, oracle_state, lifted_state = first_diff
        log(f"\nFirst divergence at seed[{si}]:")
        log(f"  inputs: {_format_inputs(params, seed_vec)}")
        log(f"  diff  : {diff.summary()}")
        log(state_mod.format_state_verbose(oracle_state, "oracle"))
        log(state_mod.format_state_verbose(lifted_state, "lifted"))
        if diff.scratch_differs:
            for line in _summarize_scratch_diff(params, oracle_state.scratch_data,
                                                lifted_state.scratch_data):
                log(line)

    if failed == 0 and errors == 0:
        return finish("pass", True, None, 0,
                      passed=passed, failed=failed, errors=errors,
                      seeds=len(seeds))

    reason = "emulation_error" if errors else "divergence"
    return finish("fail", True, reason, 1,
                  passed=passed, failed=failed, errors=errors,
                  seeds=len(seeds))


def _format_inputs(params, seed_vec) -> str:
    parts = []
    for p, v in zip(params, seed_vec):
        if isinstance(v, bytes):
            # Show first few floats
            floats = []
            for i in range(min(3, len(v) // 4)):
                fv, = struct.unpack_from('<f', v, i * 4)
                floats.append(f"{fv:.4g}")
            parts.append(f"{p.name}=[{', '.join(floats)}, ...]")
        elif isinstance(v, float):
            parts.append(f"{p.name}={v:.6g}")
        else:
            parts.append(f"{p.name}={v}")
    return ", ".join(parts)


def _summarize_scratch_diff(params, oracle_scratch: bytes, lifted_scratch: bytes) -> list[str]:
    from abi import POINTER_SLOT

    lines = []
    slot_index = 0
    for p in params:
        if not p.is_pointer:
            continue

        start = slot_index * POINTER_SLOT
        end = start + POINTER_SLOT
        slot_index += 1

        oracle_slot = oracle_scratch[start:end]
        lifted_slot = lifted_scratch[start:end]
        if oracle_slot == lifted_slot:
            continue

        diff_offsets = []
        i = 0
        limit = min(len(oracle_slot), len(lifted_slot))
        while i < limit and len(diff_offsets) < 4:
            if oracle_slot[i] != lifted_slot[i]:
                diff_offsets.append(i)
            i += 1

        if not diff_offsets:
            lines.append(f"    scratch {p.name}: size mismatch")
            continue

        parts = []
        for off in diff_offsets:
            o_word = oracle_slot[off:off + 4].hex()
            l_word = lifted_slot[off:off + 4].hex()
            parts.append(f"+0x{off:x} oracle={o_word} lifted={l_word}")
        lines.append(f"    scratch {p.name}: " + ", ".join(parts))

    return lines


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _run_batch_classify() -> int:
    """Classify all functions in delinked/ .obj files into leaf_cache.json.

    Iterates every delinked .obj, extracts all function symbols, and
    classifies each by its relocation profile.  No emulation is performed.
    """
    sys.path.insert(0, str(_SCRIPT_DIR))
    from coff_loader import load_coff, CoffParseError, IMAGE_SYM_CLASS_EXTERNAL, _canonical
    from stubs import classify_relocations, IMAGE_REL_I386_DIR32, IMAGE_REL_I386_REL32

    kb = _load_kb()

    addr_to_entry = {}
    for obj in kb.get("objects", []):
        for fn in obj.get("functions", []):
            a = fn.get("addr", "")
            if a:
                addr_to_entry[a.lower()] = fn

    cache = {}
    obj_files = sorted(DELINKED_DIR.glob("*.obj"))
    total_funcs = 0
    counts = {"leaf": 0, "data_only": 0, "stubbable": 0, "non_leaf": 0}

    for obj_path in obj_files:
        try:
            sections, symbols, _ = load_coff(str(obj_path))
        except CoffParseError:
            continue

        defined = {s.name for s in symbols if s.section_num > 0}

        with open(obj_path, "rb") as f:
            raw_data = f.read()

        for sym in symbols:
            if (sym.section_num <= 0
                    or sym.storage_class != IMAGE_SYM_CLASS_EXTERNAL
                    or not (sym.sym_type & 0x20)):
                continue

            sec_idx = sym.section_num - 1
            if sec_idx >= len(sections):
                continue

            section = sections[sec_idx]

            next_offset = len(section.data)
            for s2 in symbols:
                if (s2.section_num == sym.section_num
                        and s2.value > sym.value
                        and s2.value < next_offset
                        and s2.storage_class in (IMAGE_SYM_CLASS_EXTERNAL, 3)):
                    next_offset = s2.value

            from coff_loader import CoffReloc, RELOC_SIZE
            relocs = []
            off = section.reloc_offset
            for _ in range(section.num_relocs):
                if off + RELOC_SIZE > len(raw_data):
                    break
                va, si, rt = struct.unpack_from("<IIH", raw_data, off)
                off += RELOC_SIZE
                if sym.value <= va < next_offset:
                    sn = symbols[si].name if si < len(symbols) else f"SYM_{si}"
                    relocs.append(CoffReloc(va - sym.value, sn, rt))

            cls = classify_relocations(relocs, defined)

            canon = _canonical(sym.name)
            addr_hex = None
            m = re.match(r'FUN_([0-9a-fA-F]+)', canon)
            if m:
                addr_hex = "0x" + m.group(1).lower()

            if addr_hex:
                entry = {"class": cls.category}
                if cls.dir32_count > 0:
                    entry["dir32_count"] = cls.dir32_count
                if cls.call_count > 0:
                    entry["call_count"] = cls.call_count
                if cls.category == "non_leaf":
                    entry["reason"] = cls.reason
                cache[addr_hex] = entry
                counts[cls.category] = counts.get(cls.category, 0) + 1
                total_funcs += 1

    try:
        if _LEAF_CACHE_PATH.exists():
            existing = json.loads(_LEAF_CACHE_PATH.read_text(encoding="utf-8"))
        else:
            existing = {}
    except (OSError, json.JSONDecodeError):
        existing = {}

    for addr, old_val in existing.items():
        if isinstance(old_val, str):
            existing[addr] = {"class": old_val}

    existing.update(cache)

    _LEAF_CACHE_PATH.write_text(
        json.dumps(dict(sorted(existing.items())), indent=2) + "\n",
        encoding="utf-8",
    )

    print(f"Classified {total_funcs} functions from {len(obj_files)} .obj files:")
    for cat, cnt in sorted(counts.items()):
        print(f"  {cat}: {cnt}")
    print(f"Cache written to {_LEAF_CACHE_PATH} ({len(existing)} total entries)")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Differential emulation tester: oracle .obj vs. lifted .obj"
    )
    parser.add_argument("func_name", nargs="?",
                        help="Function name or hex address (e.g. vector3d_scale_add or 0x12f80)")
    parser.add_argument("--seeds", type=int, default=100,
                        help="Number of test seeds (default: 100)")
    parser.add_argument("--seed", type=lambda x: int(x, 0), default=0,
                        help="Base RNG seed (default: 0)")
    parser.add_argument("--verbose", action="store_true",
                        help="Print per-seed state dump")
    parser.add_argument("--self-test", action="store_true",
                        help=f"Run smoke test against {SELF_TEST_FUNC}")
    parser.add_argument("--list-funcs", metavar="OBJ",
                        help="List function symbols in a .obj file and exit")
    parser.add_argument("--output-json", type=Path, default=None,
                        help="Write structured result JSON to this path")
    parser.add_argument("--no-leaf-cache", action="store_true",
                        help="Do not update tools/equivalence/leaf_cache.json")
    parser.add_argument("--batch-classify", action="store_true",
                        help="Classify all functions in delinked/ .obj files and update leaf_cache.json")
    parser.add_argument("--z3-equiv", action="store_true",
                        help="Attempt Z3 formal equivalence proof before Unicorn testing")
    parser.add_argument("--allow-stubs", action="store_true",
                        help="Enable non-leaf emulation with callee stubbing and DIR32 patching")
    parser.add_argument("--float-tolerance", type=int, default=0, metavar="ULP",
                        help="Allow N ULP difference for float pointer params in scratch comparison")
    parser.add_argument("--float-params", type=str, default=None, metavar="NAMES",
                        help="Comma-separated param names treated as float arrays (default: auto-detect)")
    parser.add_argument("--skip-esp", action="store_true",
                        help="Skip ESP delta comparison (expected to differ for non-leaf functions)")
    args = parser.parse_args()

    if args.batch_classify:
        sys.exit(_run_batch_classify())

    if args.list_funcs:
        sys.path.insert(0, str(_SCRIPT_DIR))
        from coff_loader import list_functions
        fns = list_functions(args.list_funcs)
        for f in fns:
            print(f)
        return 0

    if args.self_test:
        sys.exit(_run_self_test(verbose=args.verbose))

    if not args.func_name:
        parser.print_help()
        sys.exit(1)

    sys.exit(run_diff(
        args.func_name,
        num_seeds=args.seeds,
        base_seed=args.seed,
        verbose=args.verbose,
        save_log=True,
        output_json=args.output_json,
        record_leaf=not args.no_leaf_cache,
        z3_equiv=args.z3_equiv,
        allow_stubs=args.allow_stubs,
        float_tolerance_ulp=args.float_tolerance,
        float_tolerance_params=args.float_params.split(",") if args.float_params else None,
        skip_esp=args.skip_esp,
    ))


if __name__ == "__main__":
    main()
