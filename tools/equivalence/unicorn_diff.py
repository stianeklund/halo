#!/usr/bin/env python3
"""Unicorn-Engine differential tester for pure x86 functions.

Compares MSVC-compiled delinked .obj (oracle) against clang-compiled lifted
.obj (candidate) by running both in separate Unicorn x86 emulators with
identical inputs and comparing CPU+FPU state at RET.

Usage:
    python3 tools/equivalence/unicorn_diff.py <func_name>
    python3 tools/equivalence/unicorn_diff.py <func_name> --seeds 100
    python3 tools/equivalence/unicorn_diff.py <func_name> --seed 0xdeadbeef
    python3 tools/equivalence/unicorn_diff.py <func_name> --verbose
    python3 tools/equivalence/unicorn_diff.py --self-test
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
CODE_SIZE      = 0x00010000   # 64 KB — enough for any single function
STACK_BASE     = 0x00100000   # bottom of stack region
STACK_SIZE     = 0x00100000   # 1 MB stack
STACK_TOP      = STACK_BASE + STACK_SIZE
SCRATCH_BASE   = 0x10000000   # scratch buffer for pointer args
SCRATCH_SIZE   = 0x00010000   # 64 KB

FAKE_RET_ADDR  = 0xDEADC0DE   # fake return address pushed on stack
MAX_INSN       = 100_000      # hard limit on emulated instructions
TIMEOUT_MS     = 5_000        # 5 second timeout


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
                return dict(fn, _obj_name=obj.get("name", ""))
    return None


def _find_kb_entry_by_addr(kb: dict, addr: str) -> Optional[dict]:
    addr_norm = addr.lower().lstrip("0x")
    for obj in kb.get("objects", []):
        for fn in obj.get("functions", []):
            a = fn.get("addr", "").lower().lstrip("0x")
            if a == addr_norm:
                return dict(fn, _obj_name=obj.get("name", ""))
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

    for unit in units:
        base = unit.get("base_path", "")
        target = unit.get("target_path", "")
        if bare and (bare in base or bare in target):
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
    return None


# ---------------------------------------------------------------------------
# Unicorn emulation
# ---------------------------------------------------------------------------

def _run_function(code: bytes, abi: dict, arg_values: list,
                  verbose: bool = False) -> "state.CPUState":
    """Run a function in a fresh Unicorn instance.

    Returns a CPUState with captured registers and scratch memory.
    If emulation fails, returns a CPUState with .error set.
    """
    import unicorn
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn import UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EBP

    import sys
    sys.path.insert(0, str(_SCRIPT_DIR))
    from abi import setup_args, SCRATCH_BASE as SCBASE, SCRATCH_SIZE as SCSIZE
    import state as state_mod

    uc = Uc(UC_ARCH_X86, UC_MODE_32)

    # Map memory regions
    uc.mem_map(CODE_BASE, CODE_SIZE)
    uc.mem_map(STACK_BASE, STACK_SIZE)
    uc.mem_map(SCRATCH_BASE, SCRATCH_SIZE)

    # Write function code at CODE_BASE
    uc.mem_write(CODE_BASE, code)

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

    def hook_code(uc, address, size, user_data):
        insn_count[0] += 1

    uc.hook_add(UC_HOOK_CODE, hook_code)

    err_msg = None
    try:
        uc.emu_start(CODE_BASE, CODE_BASE + len(code), timeout=TIMEOUT_MS * 1000,
                     count=MAX_INSN)
    except unicorn.UcError as e:
        err_str = str(e)
        # UC_ERR_FETCH_UNMAPPED at FAKE_RET_ADDR means clean return
        if "fetch" in err_str.lower() or "Fetch" in err_str:
            pass  # normal termination via fake return address
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
    """Return True if the function has no external relocations.

    External relocations point to symbols not defined in the same .obj
    (section_num == 0 in COFF means undefined external).  We cannot handle
    these without a full linker, so we reject functions with them.
    """
    if not func_slice.relocs:
        return True

    # Classify: built-in symbols we can safely ignore (typically self-calls
    # within the same section that got encoded as relative offsets)
    ok = True
    for r in func_slice.relocs:
        sym = r.symbol_name
        # Relative self-calls within the object appear as relocations but
        # resolve to the same .obj — these are fine.  External references
        # to DAT_, FUN_, or library functions are not fine.
        if not (sym.startswith(".text") or sym.startswith(".rdata")):
            print(f"  [RELOC] {label}: '{sym}' at +0x{r.virtual_address:x} — external, cannot emulate")
            ok = False
    return ok


# ---------------------------------------------------------------------------
# Pure-leaf cache (consumed by tools/llm_auto_lift.py for selection scoring)
# ---------------------------------------------------------------------------

_LEAF_CACHE_PATH = _REPO_ROOT / "tools" / "equivalence" / "leaf_cache.json"


def _record_leaf_classification(addr: str, is_leaf: bool) -> None:
    """Persist a `addr -> "leaf" | "non_leaf"` entry to leaf_cache.json.

    Used by `llm_auto_lift.py select` to reward Unicorn-eligible candidates
    without paying the cost of COFF parsing on the hot path. The cache is
    populated as a side-effect of real `unicorn_diff` runs, so entries
    represent verified evidence rather than heuristics.
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
    data[norm] = "leaf" if is_leaf else "non_leaf"
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
             record_leaf: bool = True) -> int:
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

    # --- Locate .obj files ---
    delinked_path, build_path = _find_obj_paths(entry)

    # Try source path from kb.json entry
    if not build_path and entry.get("source"):
        build_path = _find_build_obj_for_source(entry["source"])

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
        log(f"ERROR: cannot find build .obj for '{obj_name}'")
        log(f"  Run: python3 tools/build/build.py -q --target halo")
        return finish("not_applicable", False, "missing_build_object", 2)

    log(f"  delinked: {delinked_path}")
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
    if not is_leaf:
        log("ERROR: function has external relocations — cannot emulate without full linker.")
        log("  (This function calls other functions or references globals.)")
        log("  Solution: choose a pure leaf function, or implement callee stubs (Stage 2).")
        return finish("not_applicable", False, "external_relocations", 2)

    # --- Generate seeds ---
    params = abi['params']
    seeds = generate_seeds(params, num_seeds=num_seeds, base_seed=base_seed)
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
            oracle_state = _run_function(oracle_slice.code, abi, seed_vec, verbose=verbose)
        except Exception as exc:
            log(f"  {seed_label} ORACLE-ERROR: {exc}")
            errors += 1
            continue

        # Run lifted
        try:
            lifted_state = _run_function(lifted_slice.code, abi, seed_vec, verbose=verbose)
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
        ret_eax = not abi['ret_void'] and not abi['ret_st0']
        diff = state_mod.compare(
            oracle_state, lifted_state,
            check_scratch=True,
            ret_eax=ret_eax,
            ret_edx_eax=abi['ret_edx_eax'],
            ret_st0=abi['ret_st0'],
            check_st_count=1 if abi['ret_st0'] else 0,
        )

        if diff.has_differences():
            failed += 1
            if first_diff is None:
                first_diff = (si, seed_vec, diff, oracle_state, lifted_state)
            if verbose:
                log(f"  {seed_label} FAIL: {diff.summary()}")
                log(state_mod.format_state_verbose(oracle_state, "oracle"))
                log(state_mod.format_state_verbose(lifted_state, "lifted"))
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


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

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
    args = parser.parse_args()

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
    ))


if __name__ == "__main__":
    main()
