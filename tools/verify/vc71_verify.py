#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Compile a source file with Visual C++ 7.1 and score it against the binary.

Compiles the source with CL.Exe (MSVC 13.10.3077, used as the closest available
comparison toolchain; cachebeta.xbe's exact compiler is unconfirmed) and runs
an instruction-level comparison against ONE canonical
reference per function, derived from two committed inputs:

  * the pristine XBE (halo-patched/cachebeta.xbe), for the bytes;
  * tools/verify/function_bounds.json, for where each function ends.

Those bytes are wrapped in a minimal COFF (tools/verify/xbe_reference.py) and
disassembled with the SAME llvm-objdump the candidate goes through, so the two
sides cannot disagree on a mnemonic spelling.

Ghidra's delinker and delinked/ are NOT part of scoring any more.  They remain
the oracle for the lanes that EXECUTE code and therefore need real relocations:
the permuter, unicorn_diff, z3, and objdiff.

Usage:
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c --function FUN_0009ac90
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c --show-diffs
    python3 tools/verify/vc71_verify.py --list  # show registered objdiff units
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c --no-cache
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c --rebuild-cache
"""

import argparse
import bisect
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
OBJDIFF_JSON = REPO_ROOT / "objdiff.json"
BUILD_DIR = REPO_ROOT / "build"
VC71_OUT_DIR = BUILD_DIR / "vc71"
_DEFAULT_OPT = "/O2"

# Machine-readable per-function score-context packs (diff ops, warning
# detail, DP-LCS score, classification) -- see _build_score_context().
# Default ON; disable with --no-score-context. Already covered by the
# repo-wide `artifacts` gitignore entry.
SCORE_CONTEXT_DIR = REPO_ROOT / "artifacts" / "score_context"

VC71_CL = r"C:\Program Files (x86)\RXDK\xbox\bin\vc71\CL.Exe"
VC71_CL_WSL = "/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71/CL.Exe"
RXDK_INC = r"C:\Program Files (x86)\RXDK\xbox\include"

COMPARE_SCRIPT = REPO_ROOT / "tools" / "verify" / "compare_obj.py"

# Generated header the VC71 compile includes (via xdk_common.h); produced from
# kb.json by knowledge.py.  See regen_decl_header for why this tool regenerates
# it rather than trusting whatever the last CMake build left behind.
DECL_H = BUILD_DIR / "generated" / "decl.h"
KNOWLEDGE_PY = REPO_ROOT / "tools" / "analysis" / "knowledge.py"

_kb_cache: dict | None = None


def _load_kb() -> dict:
    global _kb_cache
    if _kb_cache is None:
        with open(REPO_ROOT / "kb.json") as f:
            _kb_cache = json.load(f)
    return _kb_cache


def wsl_to_win(path: Path) -> str:
    """Convert a WSL path to a Windows path."""
    s = str(path.resolve())
    if s.startswith("/mnt/"):
        # /mnt/g/dev/halo/... -> G:\dev\halo\...
        drive = s[5].upper()
        remainder = s[7:]  # skip "/mnt/X/"
        return f"{drive}:\\{remainder}".replace("/", "\\") if remainder else f"{drive}:\\"
    try:
        res = subprocess.run(["wslpath", "-w", s], capture_output=True, text=True, check=True)
        return res.stdout.strip()
    except Exception:
        return s


def load_units() -> list[dict]:
    """Load objdiff.json units.

    Scoring no longer consults this registry -- every reference is derived from
    the XBE.  It survives only to back `--list`, which reports which delinked
    references exist for the lanes that still need them (permuter, unicorn, z3,
    objdiff).
    """
    with open(OBJDIFF_JSON) as f:
        data = json.load(f)
    units = []
    for u in data.get("units", []):
        src = u.get("metadata", {}).get("source_path")
        if src:
            units.append(u)
    return units


_alias_source: Path | None = None


def _set_alias_source(source: Path | None) -> None:
    """Set the default source TU for alias resolution.

    Alias/addr lookups deep inside the scoring path don't all thread the
    source parameter; the TU being scored is authoritative for resolving
    same-stem name collisions (e.g. _rasterizer_windows_end @ 0x155a40 vs the
    rasterizer.obj thunk rasterizer_windows_end @ 0x17c910), so stash it here
    and invalidate the name-keyed caches that depend on it.
    """
    global _alias_source
    if _alias_source != source:
        _alias_source = source
        _func_span_cache.clear()


def _source_comment_addr(fn: str, source: Path | None) -> tuple[int, str] | None:
    """(addr, spelling) from the house-style address comment above `fn`, else None.

        /* 0x155a40 */
        void _rasterizer_windows_end(void)

    Authoritative, and the only thing that separates two same-stem symbols:
    llvm-nm normalization strips ALL leading underscores from the candidate
    symbol, so an underscore-prefixed impl (_rasterizer_windows_end @ 0x155a40)
    would otherwise collide with the same-stem kb name (rasterizer_windows_end
    @ 0x17c910, the tail-call thunk) and be scored against the wrong bytes.
    """
    if not source:
        return None
    sp = Path(source)
    if not sp.exists():
        return None
    try:
        m = re.search(
            r"/\*\s*0x([0-9a-fA-F]{4,8})\s*\*/\s*\n[^\n(]*?\b(_{0,2}"
            + re.escape(fn) + r")\s*\(",
            sp.read_text(),
        )
    except Exception:
        return None
    return (int(m.group(1), 16), m.group(2)) if m else None


def function_aliases(fn: str, source: Path | None = None) -> set[str]:
    """Find all name aliases for a function (e.g. console_update -> FUN_000ff9e0)."""
    aliases = {fn}
    # CRT lifts retain this prefix to avoid colliding with host CRT declarations;
    # COFF's leading underscore is normalized away by the objdump symbol parse.
    if fn.startswith("crt_"):
        aliases.add(fn[4:])
    if source is None:
        source = _alias_source

    hit = _source_comment_addr(fn, source)
    if hit is not None:
        return {fn, hit[1], f"FUN_{hit[0]:08x}"}

    try:
        kb = _load_kb()
        if isinstance(kb, dict):
            if "objects" in kb:
                for obj in kb.get("objects", []):
                    for entry in obj.get("functions", []):
                        addr = entry.get("addr", "")
                        decl = entry.get("decl", "")
                        m = re.search(r"\b(\w+)\s*\(", decl)
                        if not (addr and m):
                            continue
                        declared = m.group(1)
                        fun_name = f"FUN_{int(addr, 16):08x}"
                        if fn == declared:
                            aliases.add(fun_name)
                        elif fn == fun_name:
                            aliases.add(declared)
            else:
                for addr_str, entry in kb.items():
                    if isinstance(entry, dict):
                        declared = entry.get("name")
                        decl = entry.get("decl", "")
                        if not declared and decl:
                            m = re.search(r"\b(\w+)\s*\(", decl)
                            if m:
                                declared = m.group(1)
                        if declared and addr_str.startswith("0x"):
                            try:
                                fun_name = f"FUN_{int(addr_str, 16):08x}"
                                if fn == declared:
                                    aliases.add(fun_name)
                                elif fn == fun_name:
                                    aliases.add(declared)
                            except ValueError:
                                pass
    except (OSError, ValueError, json.JSONDecodeError):
        pass

    if source:
        source_path = Path(source)
        if source_path.exists():
            try:
                txt = source_path.read_text()
                for m in re.finditer(r"/\*\s*(\w+)[^*]*?Address:\s*0x([0-9a-fA-F]+)", txt):
                    name, addr_hex = m.group(1), m.group(2)
                    fun_name = f"FUN_{int(addr_hex, 16):08x}"
                    if fn == name:
                        aliases.add(fun_name)
                    elif fn == fun_name:
                        aliases.add(name)
            except Exception:
                pass

    return aliases


_kb_starts_cache: list[int] | None = None


def _kb_func_starts() -> list[int]:
    """Sorted list of all function start addresses in kb.json (cached)."""
    global _kb_starts_cache
    if _kb_starts_cache is None:
        starts: set[int] = set()
        try:
            for obj in _load_kb().get("objects", []):
                for entry in obj.get("functions", []):
                    a = entry.get("addr")
                    if not a:
                        continue
                    try:
                        starts.add(int(a, 16))
                    except (ValueError, TypeError):
                        pass
        except (OSError, ValueError, json.JSONDecodeError):
            pass
        _kb_starts_cache = sorted(starts)
    return _kb_starts_cache


def _func_addr(function: str, source: Path | None = None) -> int | None:
    """Resolve a function name/alias to its start address, or None."""
    for alias in function_aliases(function, source) | {function}:
        m = re.match(r"FUN_0*([0-9a-fA-F]+)$", alias or "")
        if m:
            return int(m.group(1), 16)
    return None


def _decode_relative_jump_target(addr: int, code: bytes) -> int | None:
    """Decode a direct x86 near/short JMP target, or return no opinion."""
    if len(code) >= 5 and code[0] == 0xe9:
        disp = int.from_bytes(code[1:5], "little", signed=True)
        return addr + 5 + disp
    if len(code) >= 2 and code[0] == 0xeb:
        disp = int.from_bytes(code[1:2], "little", signed=True)
        return addr + 2 + disp
    return None


def _forwarding_target_info(addr: int) -> dict | None:
    """Describe a direct forwarding target without following or scoring it."""
    target = _decode_relative_jump_target(addr, _xbe_read(addr, 5) or b"")
    if target is None:
        return None
    name = None
    try:
        table = json.loads(
            (REPO_ROOT / "tools/verify/function_bounds.json").read_text())
        entry = table.get(f"0x{target:x}")
        if entry:
            name = entry.get("name")
    except (OSError, ValueError, json.JSONDecodeError):
        pass
    return {
        "addr": f"0x{target:08x}",
        "name": name,
        "body_score": None,
        "body_score_reason": "target body is not compiled in this verification lane",
    }


def _true_end_offset(addr: int, limit: int) -> int | None:
    """Byte size of the function at `addr`, read from the pristine XBE.

    A body closes at the first terminator (`ret`, or an unconditional `jmp`
    that is not an indirect/table dispatch) with NO outstanding branch target
    at or beyond it.  That second clause is what keeps this from cutting a
    function short at an interior `ret` when MSVC has placed a tail block out
    of line: FUN_00174510 looks finished at 0x174622 until you notice the
    `jl 0x174622` at 0x174608 proving the body continues.

    Returns None when capstone or the XBE is unavailable, or when no
    terminator is found within `limit` bytes -- callers must treat None as
    "no opinion" and fall back to the kb.json gap.
    """
    try:
        import capstone
    except ImportError:
        return None
    code = _xbe_read(addr, limit)
    if not code:
        return None
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    targets: set[int] = set()
    for ins in md.disasm(code, addr):
        if ins.mnemonic.startswith("j"):
            try:
                t = int(ins.op_str, 16)
                # Only an in-window target can be a later block of THIS function.
                # A branch to a distant address is a tail call / external jump and
                # must not hold the body open (`errors_dispose` is the 5-byte
                # thunk `jmp 0x92440`; counting its own target as outstanding
                # made it look like it never terminated).
                if addr <= t < addr + limit:
                    targets.add(t)
            except ValueError:
                pass  # indirect branch; the guard below just won't fire early
        end = ins.address + ins.size
        terminator = ins.mnemonic == "ret" or (
            ins.mnemonic == "jmp" and not ins.op_str.startswith("dword"))
        if terminator and not any(t >= end for t in targets):
            return end - addr
    return None


_func_span_cache: dict[str, int | None] = {}


def _func_span(function: str) -> int | None:
    """Byte span of a function, or None when its address cannot be resolved.

    Read from `tools/verify/function_bounds.json` -- the committed, CI-gated
    bounds table, which is the single authority for where a function ends.  The
    reference this function is scored against is cut from the SAME entry, so the
    span and the reference can no longer disagree.

    The table is what removed the listing-gap span class: kb.json's gap
    (distance to the next *listed* function) overshoots wherever the listing has
    a hole, and kb.json is not a full listing of the binary.  Measured:
    FUN_0015c2d0 is 102 bytes, its kb gap is 800, because its real neighbour at
    0x15c340 was never listed.

    An address absent from the table falls back to a run-time computation (with
    a warning from `xbe_reference`), so a freshly-added kb.json function is
    scored rather than dropped until the table is regenerated.

    None is preserved for symbols with no resolvable address -- callers use that
    to mean "not a kb.json-tracked function" (helper/thunk/static), which is a
    different condition from "tracked, size unknown".
    """
    if function in _func_span_cache:
        return _func_span_cache[function]
    addr = _func_addr(function)
    span: int | None = None
    if addr is not None:
        span = _addr_span(addr)
    _func_span_cache[function] = span
    return span


def _addr_span(addr: int) -> int | None:
    """Byte span at `addr` from the bounds table, else a computed fallback."""
    try:
        import xbe_reference as xr
        ext = xr.function_extent(addr)
        if ext is not None:
            return ext[0] - addr
        return None
    except Exception:
        pass
    # xbe_reference (or capstone, or the XBE) unavailable: degrade to the kb gap
    # capped by a local terminator scan, which is what the table encodes anyway.
    starts = _kb_func_starts()
    if not starts:
        return None
    i = bisect.bisect_right(starts, addr)
    kb_gap = (starts[i] - addr) if i < len(starts) else None
    if not kb_gap:
        return kb_gap
    true_size = _true_end_offset(addr, kb_gap)
    return min(kb_gap, true_size) if true_size else kb_gap


# ---------------------------------------------------------------------------
# Which functions a TU owns
# ---------------------------------------------------------------------------
#
# kb.json replaces the delinked object's symbol list as the answer to "which
# functions belong to this translation unit".  That list used to be the gate on
# what got scored, which is why a narrow export silently dropped a file's other
# functions; kb.json is the authority on ownership and is the same input the
# build and the patcher use.

_kb_source_index_cache: dict[str, dict[str, dict]] | None = None


def _kb_source_index() -> dict[str, dict[str, dict]]:
    """{kb.json source path -> {symbol -> record}} over every listed function.

    A function's owning TU is its own ``source_path``/``source``/``src``/``file``
    override when it has one, else its object's -- kb.json carries per-function
    overrides precisely because one .obj can be split across several .c files.
    Both the declared symbol and the ``FUN_<addr>`` spelling key the same
    record, so either resolves.  ``record`` is {addr, name, ported}.
    """
    global _kb_source_index_cache
    if _kb_source_index_cache is not None:
        return _kb_source_index_cache
    index: dict[str, dict[str, dict]] = {}
    try:
        kb = _load_kb()
        for obj in kb.get("objects", []) or []:
            obj_src = (obj.get("source") or obj.get("source_path")
                       or obj.get("src") or "")
            for entry in obj.get("functions", []) or []:
                addr = entry.get("addr")
                if not addr:
                    continue
                try:
                    a = int(addr, 16)
                except (ValueError, TypeError):
                    continue
                src = (entry.get("source_path") or entry.get("source")
                       or entry.get("src") or entry.get("file") or obj_src)
                if not src:
                    continue
                fun = f"FUN_{a:08x}"
                m = re.search(r"\b(\w+)\s*\(", entry.get("decl", "") or "")
                declared = m.group(1) if m else fun
                rec = {"addr": a, "name": declared,
                       "ported": bool(entry.get("ported"))}
                bucket = index.setdefault(str(src).replace("\\", "/"), {})
                bucket[declared] = rec
                bucket.setdefault(fun, rec)
    except Exception:
        pass
    _kb_source_index_cache = index
    return index


def _kb_functions_for_source(source: Path) -> dict[str, dict]:
    """The kb.json functions this TU owns, keyed by declared name and FUN_<addr>.

    kb source paths are src/halo-relative (``ai/actors.c``) while callers hold a
    repo-root-relative or absolute path, so match on a path-component suffix.
    A bare `endswith` would let ``profiles.c`` claim ``files.c``'s entries.
    """
    norm = str(source).replace("\\", "/")
    for key, fns in _kb_source_index().items():
        if norm == key or norm.endswith("/" + key):
            return fns
    return {}


def _undecorate(sym: str) -> str:
    """Strip MSVC's cdecl decoration -- exactly ONE leading underscore.

    ``lstrip("_")`` strips every leading underscore, so a C function whose name
    genuinely begins with one is over-stripped and can never be resolved: zlib's
    ``_tr_tally`` compiles to ``__tr_tally``, which ``lstrip`` reduced to
    ``tr_tally`` -- a name that exists in neither kb.json nor the source.  That
    silently dropped all four ``_tr_*`` functions from every score run.
    """
    return sym[1:] if sym.startswith("_") else sym


def _resolve_func_addr(fn: str, source: Path, tu_funcs: dict) -> int | None:
    """Address for a compiled symbol, most-specific evidence first.

    1. the house-style ``/* 0xADDR */`` comment above the definition;
    2. the kb.json functions THIS TU owns -- so a static helper whose name also
       exists in another TU is not resolved to that other TU's address and then
       scored against unrelated code;
    3. the global kb.json name <-> FUN_<addr> alias map.
    """
    hit = _source_comment_addr(fn, source)
    if hit is not None:
        return hit[0]
    rec = (tu_funcs.get(fn) or tu_funcs.get(_undecorate(fn))
           or tu_funcs.get(fn.lstrip("_")) or tu_funcs.get("_" + fn))
    if rec:
        return rec["addr"]
    return _func_addr(fn, source)


def _resolve_compiled_name(requested: str, compiled_funcs: dict,
                           source: Path) -> str | None:
    """The compiled symbol the caller means by `requested`, or None.

    Callers routinely pass ``FUN_<addr>`` for a function the source now names
    something else (lift_pipeline does, and goal-lift does).  The delinked
    reference used to bridge that by carrying the pre-rename symbol; with the
    reference derived per address, the bridge has to be explicit.
    """
    for want in (requested, _undecorate(requested), requested.lstrip("_")):
        if want in compiled_funcs:
            return want
    want = _undecorate(requested)
    aliases = function_aliases(want, source) | {want}
    for name in compiled_funcs:
        if name in aliases or name.rsplit("::", 1)[-1] in aliases:
            return name
        if function_aliases(name, source) & aliases:
            return name
    return None


def _trim_after_last_terminator(insns: list[str]) -> list[str]:
    """Drop everything after the last ret/jmp -- inline table data, never code.

    Backstop for a `table_data` bound, whose span deliberately covers the
    switch/index table MSVC emitted in .text after the final ret.
    """
    last = -1
    for i, insn in enumerate(insns):
        mnem = (insn.split(None, 1)[0] if insn else "").lower()
        if mnem in ("ret", "retl", "retw", "retq", "retn", "jmp", "jmpl"):
            last = i
    return insns[:last + 1] if last >= 0 else insns


_xbe_sections_cache: list[tuple[int, int, int, int]] | None = None


def _xbe_read(va: int, n: int) -> bytes | None:
    """Read n bytes at virtual address va from the pristine XBE."""
    global _xbe_sections_cache
    xbe_path = REPO_ROOT / "halo-patched" / "cachebeta.xbe"
    if not xbe_path.exists():
        return None
    try:
        import struct
        data = xbe_path.read_bytes()
        if _xbe_sections_cache is None:
            base = struct.unpack_from("<I", data, 0x104)[0]
            nsec = struct.unpack_from("<I", data, 0x11C)[0]
            shdr = struct.unpack_from("<I", data, 0x120)[0] - base
            _xbe_sections_cache = [struct.unpack_from("<IIII", data, shdr + i * 0x38 + 4)
                                   for i in range(nsec)]
        for vaddr, vsize, raw, _rawsz in _xbe_sections_cache:
            if vaddr <= va < vaddr + vsize:
                off = raw + va - vaddr
                return data[off:off + n]
    except Exception:
        return None
    return None



def _get_regarg_callees(source: Path) -> dict[str, str]:
    """Identify register-arg functions callable from this source file.

    Returns {func_name: cast_expr} where cast_expr is a C cast that
    generates the right call-site instruction sequence for VC71 comparison.
    Only includes functions with @<reg> annotations in kb.json.
    """
    kb = _load_kb()
    src_text = source.read_text()

    # Collect all register-arg functions from kb.json
    regarg_funcs: dict[str, list[tuple[int, str]]] = {}
    for obj in kb.get("objects", []):
        for fn in obj.get("functions", []):
            decl = fn.get("decl", "")
            if "@<" not in decl:
                continue
            m = re.search(r"\b(\w+)\s*\(", decl)
            if not m:
                continue
            name = m.group(1)
            # Parse @<reg> annotations to get (param_idx, reg) pairs
            params_str = decl[decl.index("(") + 1 : decl.rindex(")")]
            regs = []
            for i, p in enumerate(params_str.split(",")):
                rm = re.search(r"@<(\w+)>", p)
                if rm:
                    regs.append((i, rm.group(1).lower()))
            if regs:
                regarg_funcs[name] = regs

    # Only include functions that are CALLED from (not just defined in) this source
    result: dict[str, str] = {}
    for name, regs in regarg_funcs.items():
        if name not in src_text:
            continue

        # Find full declaration to extract return type and param info
        decl = ""
        for obj in kb.get("objects", []):
            for fn in obj.get("functions", []):
                d = fn.get("decl", "")
                m = re.search(r"\b" + re.escape(name) + r"\s*\(", d)
                if m:
                    decl = d
                    break
            if decl:
                break
        if not decl:
            continue

        # Extract return type (everything before the function name)
        ret_match = re.match(r"(.+?)\b" + re.escape(name) + r"\s*\(", decl)
        ret_type = ret_match.group(1).strip() if ret_match else "void"

        params_str = decl[decl.index("(") + 1 : decl.rindex(")")]
        params = [p.strip() for p in params_str.split(",") if p.strip()]
        stack_params = sum(1 for p in params if "@<" not in p and p != "void")

        # Single register-arg, no stack args: __fastcall(int) generates mov+call
        if len(regs) == 1 and stack_params == 0:
            result[name] = f"(({ret_type}(__fastcall*)(int)){name})"
        # All register-args, no stack args: void-cast enables tail-call/bare call
        elif stack_params == 0 and ret_type == "void":
            result[name] = f"((void(*)(void)){name})"
        # @<ecx>[, @<edx>] prefix followed by integer stack args: maps exactly
        # onto __fastcall (ecx, edx, remaining args pushed, callee-clean).
        # e.g. D3DDevice_SetTextureStageState(stage@<ecx>, state@<edx>, value)
        # -> mov edx, ...; mov/xor ecx, ...; push value; call (no add esp).
        # Float/double stack params are excluded (int cast would corrupt them).
        elif (
            stack_params > 0
            and len(regs) in (1, 2)
            and regs[0][0] == 0
            and regs[0][1] in _FASTCALL_ECX
            and (len(regs) == 1
                 or (regs[1][0] == 1 and regs[1][1] in _FASTCALL_EDX))
            and "float" not in params_str
            and "double" not in params_str
        ):
            base_ret = re.sub(r"__(stdcall|cdecl|fastcall)\b", "",
                              ret_type).strip()
            arg_types = ",".join(["int"] * len(params))
            result[name] = f"(({base_ret}(__fastcall*)({arg_types})){name})"

    return result


def _preprocess_regcall(source: Path, callees: dict[str, str]) -> Path:
    """Generate a VC71-specific source with register-arg calls using casts.

    For each callee in the map, replaces call-site invocations with the
    cast expression. Function definitions are preserved unchanged.
    Returns path to the preprocessed temp file.
    """
    lines = source.read_text().split("\n")
    out = []

    for line in lines:
        stripped = line.lstrip()
        # Skip function definitions — they start with a return type
        is_def = False
        for name in callees:
            if re.match(rf"^[\w\s\*]+\b{name}\s*\(", stripped):
                is_def = True
                break
        if is_def:
            out.append(line)
            continue

        # Replace call sites
        for name, cast in callees.items():
            idx = 0
            while True:
                pos = line.find(name + "(", idx)
                if pos < 0:
                    break
                # Find matching closing paren
                depth = 0
                start = pos + len(name)
                end = start
                for i in range(start, len(line)):
                    if line[i] == "(":
                        depth += 1
                    elif line[i] == ")":
                        depth -= 1
                        if depth == 0:
                            end = i + 1
                            break
                if end <= start:
                    break

                if "void(void)" in cast:
                    replacement = cast + "()"
                else:
                    args = line[start + 1 : end - 1]
                    replacement = cast + "(" + args + ")"
                line = line[:pos] + replacement + line[end:]
                idx = pos + len(replacement)
        out.append(line)

    tmp = source.parent / f".vc71_regcall_{source.name}"
    tmp.write_text("\n".join(out))
    return tmp


# Register sets whose kb.json @<reg> annotations map exactly onto __fastcall's
# argument registers (byte/word aliases included).
_FASTCALL_ECX = {"ecx", "cx", "cl"}
_FASTCALL_EDX = {"edx", "dx", "dl"}


def _get_fastcall_mappable() -> set[str]:
    """kb.json functions whose register-arg ABI maps exactly onto __fastcall.

    Returns the names of functions whose parameter list is exactly
    [@<ecx>] or [@<ecx>, @<edx>] (byte/word aliases included) with no stack
    parameters, or [@<ecx>, @<edx>, int-stack-params...] — for the latter,
    __fastcall consumes ecx and edx and pushes the remaining args with
    callee cleanup, which is exactly the original ABI (e.g.
    D3DDevice_SetTextureStageState(stage@<ecx>, state@<edx>, value)).
    For all of these, compiling as __fastcall makes VC71 read the argument
    registers directly (no cdecl stack-load prologue) and emit the
    original's call-site sequence (mov ecx[, edx][; push ...]; call).
    Functions with a single @<ecx> AND stack parameters are excluded:
    __fastcall would steal the first stack parameter into edx.  Float or
    double stack parameters are excluded (fastcall push conventions match,
    but keep parity with _get_regarg_callees' proven-safe subset).
    @<eax>/@<esi>/etc. cannot be expressed in VC71 at all (known permanent
    ceiling).
    """
    kb = _load_kb()
    result: set[str] = set()
    for obj in kb.get("objects", []):
        for fn in obj.get("functions", []):
            decl = fn.get("decl", "") or ""
            if "@<" not in decl:
                continue
            m = re.search(r"\b(\w+)\s*\(", decl)
            if not m:
                continue
            try:
                params_str = decl[decl.index("(") + 1 : decl.rindex(")")]
            except ValueError:
                continue
            params = [p.strip() for p in params_str.split(",")
                      if p.strip() and p.strip() != "void"]
            regs = []
            stack_params = 0
            ok = True
            for p in params:
                rm = re.search(r"@<(\w+)>", p)
                if rm:
                    if stack_params:
                        ok = False  # reg param after a stack param
                        break
                    regs.append(rm.group(1).lower())
                else:
                    stack_params += 1
                    if "float" in p or "double" in p:
                        ok = False
                        break
            if not ok or not regs:
                continue
            if (len(regs) == 1 and regs[0] in _FASTCALL_ECX
                    and stack_params == 0) or (
                len(regs) == 2
                and regs[0] in _FASTCALL_ECX
                and regs[1] in _FASTCALL_EDX
            ):
                result.add(m.group(1))
    return result


def _fastcall_sig_re(name: str) -> "re.Pattern[str]":
    """Match a top-level definition/prototype line of `name` (return type at
    column 0, then the name, then the parameter list opener)."""
    return re.compile(rf"^([\w][\w \t\*]*[ \t\*])({re.escape(name)})(\s*\()",
                      re.MULTILINE)


def _fastcall_sub(m: "re.Match[str]") -> str:
    """Rewrite a matched signature to __fastcall, dropping any explicit
    __stdcall/__cdecl so the keywords don't conflict (both are callee-clean
    vs __fastcall's callee-clean — the register ABI is what changes)."""
    prefix = re.sub(r"__(stdcall|cdecl)\b[ \t]*", "", m.group(1))
    return f"{prefix}__fastcall {m.group(2)}{m.group(3)}"


def _preprocess_fastcall_defs(source: Path, names: set[str],
                              orig_source: Path) -> Path:
    """Insert __fastcall into top-level definitions/prototypes of
    fastcall-mappable functions.  Returns the (possibly new) source path."""
    text = source.read_text()
    changed = False
    for name in names:
        if name not in text:
            continue
        new_text, n = _fastcall_sig_re(name).subn(_fastcall_sub, text)
        if n:
            text = new_text
            changed = True
    if not changed:
        return source
    tmp = orig_source.parent / f".vc71_fastcall_{orig_source.name}"
    tmp.write_text(text)
    return tmp


def regen_decl_header(quiet: bool = False) -> bool:
    """Regenerate build/generated/decl.h from kb.json before compiling.

    decl.h is a *compile input*, and this tool already refuses to trust a stale
    .obj (see obj_is_current / source_stamp).  The header had no such guard: only
    a CMake build regenerates it, so editing a prototype in kb.json and then
    running this script directly compiles the TU against the PREVIOUS header.

    That is not a soft failure.  A corrected prototype (say void(void) -> the
    real void *f(void*, void*, float, void*)) makes every call site in the lifted
    C a hard error against the old header, cl.exe returns non-zero, and the run
    reports zero scored functions for the whole TU -- which reads like a missing
    or broken reference, sending the investigation at the delinked object instead
    of the header.  Observed on particle_systems.c (C2440 at the
    point_physics_definition_interpolate call site, decl.h 12h older than
    kb.json); vc71_regression.py already pins the header for the same reason
    after it silently blanked network_game_globals on the dashboard.

    Cheap, idempotent, and deterministic for an unchanged kb.json.  Returns True
    on success; on failure warns and returns False so the run still proceeds
    against whatever header exists (a compile error then names the real cause).
    """
    DECL_H.parent.mkdir(parents=True, exist_ok=True)
    try:
        r = subprocess.run(
            [sys.executable, str(KNOWLEDGE_PY), "--gen-header", str(DECL_H)],
            capture_output=True, text=True, cwd=REPO_ROOT)
    except OSError as e:
        print(f"  ⚠ could not regenerate decl.h ({e}); using existing header",
              file=sys.stderr)
        return False
    if r.returncode != 0:
        tail = (r.stderr or r.stdout or "").strip().splitlines()[-3:]
        print("  ⚠ decl.h regeneration failed; using existing header:",
              file=sys.stderr)
        for line in tail:
            print(f"      {line}", file=sys.stderr)
        return False
    if not quiet:
        print("[decl] build/generated/decl.h pinned to kb.json", flush=True)
    return True


def _make_fastcall_decl_shadow(names: set[str]) -> Path | None:
    """Write a decl.h copy with __fastcall on mappable prototypes into a
    shadow include dir (searched before build/generated), so prototypes agree
    with the rewritten definitions and call sites compile as fastcall."""
    decl_path = BUILD_DIR / "generated" / "decl.h"
    if not decl_path.exists():
        return None
    text = decl_path.read_text()
    changed = False
    for name in names:
        if name not in text:
            continue
        new_text, n = _fastcall_sig_re(name).subn(_fastcall_sub, text)
        if n:
            text = new_text
            changed = True
    if not changed:
        return None
    shadow_dir = VC71_OUT_DIR / "fastcall_inc"
    shadow_dir.mkdir(parents=True, exist_ok=True)
    (shadow_dir / "decl.h").write_text(text)
    return shadow_dir


def obj_stamp_path(obj: Path) -> Path:
    """Sidecar recording WHICH source content produced this object."""
    return obj.with_name(obj.name + ".srcsha")


def source_stamp(source: Path, opt: str) -> str:
    """Identity of a compile input: source CONTENT plus the flags it was built
    with.  Content, not mtime — see obj_is_current."""
    try:
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
    except OSError:
        return ""
    return f"{digest}:{opt}"


def obj_is_current(obj: Path, source: Path, opt: str) -> bool:
    """Whether ``obj`` was compiled from the CURRENT bytes of ``source``.

    Reusing a stale object silently measures code that is no longer in the tree,
    in both directions:
      * a function deleted from the source keeps scoring from the old object --
        FUN_000f56b0 sat at exactly 81.8% across 13 attempts while having no
        definition at all, and the unmoving score read as a structural ceiling;
      * a function newly added is absent from the object, so `--function <new>`
        aborts with "not found in both objects" and the lift pipeline reports
        "VC71 compilation or comparison failed".

    This used to compare mtimes (obj >= source).  mtime is the wrong oracle for
    the question: this repo lives on a WSL2 /mnt/g DrvFs mount with coarse
    timestamps, so an edit landing in the same tick as the previous compile
    looks current; and anything that restores content while preserving times
    (`cp -p`, an archive extract, some checkout paths) makes a stale object look
    newer than the source it no longer matches.  Compare the source's CONTENT
    hash against a stamp written at compile time instead, plus the flags, since
    the same source at /O2 and /O2 /Ob1 are different objects.

    An object with no stamp (built before this existed, or by another tool) is
    NOT current: one extra compile is cheaper than one wrong score.
    """
    if not obj.exists() or not source.exists():
        return False
    want = source_stamp(source, opt)
    if not want:
        return False
    try:
        return obj_stamp_path(obj).read_text().strip() == want
    except OSError:
        return False


def compile_vc71(source: Path, output: Path, regcall_elide: bool = False, opt: str = "/O2") -> bool:
    """Compile a source file with VC++ 7.1 cl.exe. Returns True on success.

    When regcall_elide=True, preprocesses the source to cast register-arg
    callee invocations so VC71 generates matching call-site instruction
    sequences (mov+call instead of push+call+add).

    Always applies the __fastcall rewrite for @<ecx>[/@<edx>]-only functions
    (definitions, prototypes, and the decl.h shadow) — this models the
    kb.json-documented register ABI that VC71 cannot express via cdecl.
    """
    output.parent.mkdir(parents=True, exist_ok=True)

    actual_source = source
    if regcall_elide:
        callees = _get_regarg_callees(source)
        if callees:
            actual_source = _preprocess_regcall(source, callees)

    fastcall_names = _get_fastcall_mappable()
    shadow_inc = None
    if fastcall_names:
        actual_source = _preprocess_fastcall_defs(actual_source, fastcall_names, source)
        shadow_inc = _make_fastcall_decl_shadow(fastcall_names)

    src_win = wsl_to_win(actual_source)
    out_win = wsl_to_win(output)
    fi_win = wsl_to_win(REPO_ROOT / "src" / "xdk_common.h")
    gen_inc = wsl_to_win(BUILD_DIR / "generated")
    src_inc = wsl_to_win(REPO_ROOT / "src")
    tp_xbox_inc = wsl_to_win(REPO_ROOT / "third_party" / "xbox")

    cmd = [
        VC71_CL_WSL,
        "/nologo", "/c", "/TC",
        *opt.split(), "/Oy-", "/GF", "/Gy", "/Gd",
        "/W0", "/Zl", "/X",
        "/DMSVC", "/DXDK_BUILD", "/DHDATA=",
        f"/FI{fi_win}",
        *([f"/I{wsl_to_win(shadow_inc)}"] if shadow_inc else []),
        f"/I{gen_inc}",
        f"/I{src_inc}",
        f"/I{tp_xbox_inc}",
        f"/I{RXDK_INC}",
        f"/Fo{out_win}",
        src_win,
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
    finally:
        # Preprocessed temps live next to the source (so relative includes
        # resolve) — remove them so repo scans (raw-cast baseline, hazard
        # scan, grep) never see duplicated source under src/.
        if actual_source != source:
            actual_source.unlink(missing_ok=True)
        regcall_tmp = source.parent / f".vc71_regcall_{source.name}"
        if regcall_tmp != actual_source:
            regcall_tmp.unlink(missing_ok=True)

    if result.returncode != 0:
        diag = result.stdout + result.stderr
        print(f"VC71 compilation failed:\n{diag}", file=sys.stderr)
        return False

    if not output.exists():
        print(f"VC71 compilation produced no output at {output}", file=sys.stderr)
        return False

    # Record which source content this object is, so a later run can tell a
    # reusable object from a stale one (see obj_is_current).  Written last, so a
    # failed or interrupted compile leaves no stamp and the object is treated as
    # stale.  Stamp the ORIGINAL source, not the preprocessed temp: the temp is
    # derived from it and is deleted above.
    try:
        obj_stamp_path(output).write_text(source_stamp(source, opt) + "\n")
    except OSError:
        # A missing stamp only costs a recompile next time; never fail here.
        pass

    return True


_regdef_map_cache: dict[str, list[tuple[int, str]]] | None = None


def _regdef_params_for(fn: str, co) -> list[tuple[int, str]] | None:
    """(param_idx, reg) pairs for fn's OWN @<reg> params from kb.json, else None.

    Used to model the @reg-DEFINED prologue ceiling: cl.exe cannot express
    @<eax>/@<ebx>/@<esi>/@<edi>/@<ax> parameter conventions, so the candidate
    emits phantom-slot loads the reference never has.  compare_obj strips
    those (candidate-only, count-aware) when given these pairs.  @<ecx>-only
    functions are already compiled as __fastcall; for them the strip is a
    harmless no-op (no phantom loads exist).  Keyed by both the declared name
    and the FUN_<addr> alias so it resolves regardless of which symbol the
    reference carries.
    """
    global _regdef_map_cache
    if _regdef_map_cache is None:
        regdef_map: dict[str, list[tuple[int, str]]] = {}
        try:
            kb = _load_kb()
            for obj in kb.get("objects", []):
                for entry in obj.get("functions", []):
                    decl = entry.get("decl", "")
                    if "@<" not in decl:
                        continue
                    m = re.search(r"\b(\w+)\s*\(", decl)
                    if not m:
                        continue
                    regs = co.parse_regdef_from_decl(decl)
                    if not regs:
                        continue
                    regdef_map[m.group(1)] = regs
                    addr = entry.get("addr", "")
                    if addr:
                        try:
                            regdef_map[f"FUN_{int(addr, 16):08x}"] = regs
                        except ValueError:
                            pass
        except Exception:
            pass
        _regdef_map_cache = regdef_map
    return _regdef_map_cache.get(fn.lstrip("_"))


# --- Score-context pack (--no-score-context to disable) -----------------
#
# Machine-readable per-function diagnostics dumped to
# artifacts/score_context/<name>.json alongside every scored function:
# the official/operand-normalized/DP-LCS scores, frame-size probe, the
# same warning detail already computed for the console (FPU/LOADW/IMM/
# FCOM), an aligned mnemonic diff, and a best-effort classification into
# the recovery levers documented in docs/lift-learnings.md and
# .claude/skills/lift-score-improve/SKILL.md. Additive only: none of this
# touches the existing PASS/FAIL/warning console output other tooling
# parses (lift_pipeline, dashboards, permuter run.py).

_SUB_ESP_RE = re.compile(r'^subl?\s+\$(-?0x[0-9a-f]+|-?\d+)\s*,\s*%esp\b', re.IGNORECASE)
_ADD_ESP_RE = re.compile(r'^addl?\s+\$(-?0x[0-9a-f]+|-?\d+)\s*,\s*%esp\b', re.IGNORECASE)
_PROLOGUE_WINDOW = 40
_DIFF_OP_CAP = 300
_DIFF_EQUAL_CTX = 2
_PUSHL_MOVL_MIN = 3
_ANCHOR_COLLAPSE_GAP = 12.0


def _parse_signed_imm(tok: str) -> int:
    neg = tok.startswith("-")
    if neg:
        tok = tok[1:]
    val = int(tok, 16) if tok.lower().startswith("0x") else int(tok)
    return -val if neg else val


def _first_frame_bytes(insns: list[str]) -> int | None:
    """Bytes reserved by the first `sub esp, N` (or `add esp, -N`) seen in the
    prologue window. Returns None if neither idiom appears there (e.g. a
    leaf function with no locals, or a frame built a different way).
    """
    for insn in insns[:_PROLOGUE_WINDOW]:
        s = insn.strip()
        m = _SUB_ESP_RE.match(s)
        if m:
            return abs(_parse_signed_imm(m.group(1)))
        m = _ADD_ESP_RE.match(s)
        if m:
            v = _parse_signed_imm(m.group(1))
            if v < 0:
                return abs(v)
    return None


def _has_chkstk_call(insns: list[str]) -> bool:
    """Best-effort: does the function call _chkstk / an alloca-probe?

    Cheap substring check over already-disassembled text -- llvm-objdump
    usually keeps the symbol name on a call operand even for an unresolved
    extern reference. Skips (returns False) rather than raising when the
    symbol isn't textually present; this is advisory evidence, not a gate.
    """
    for insn in insns:
        low = insn.lower()
        if "call" in low and ("chkstk" in low or "alloca_probe" in low):
            return True
    return False


def _mk_diff_op(kind: str, i1: int, i2: int, j1: int, j2: int,
                 cand_insns: list[str], ref_insns: list[str]) -> dict:
    return {
        "kind": kind,
        "cand_range": [i1, i2],
        "ref_range": [j1, j2],
        "cand": [cand_insns[i].strip() for i in range(i1, i2)],
        "ref": [ref_insns[j].strip() for j in range(j1, j2)],
    }


def _build_diff_ops(cand_insns: list[str], ref_insns: list[str], opcodes,
                     cap: int = _DIFF_OP_CAP, ctx: int = _DIFF_EQUAL_CTX
                     ) -> tuple[list[dict], bool]:
    """Aligned diff ops for the score-context pack, from the same
    SequenceMatcher opcodes (mnemonic-sequence alignment) used for the
    official score. Long equal runs are collapsed to `ctx` lines of context
    on each end so a near-100%-match function doesn't dump its whole body;
    the total instruction payload (cand + ref lines, summed across all ops)
    is capped at `cap` and `truncated` is reported when that cap is hit.
    """
    ops: list[dict] = []
    total = 0
    truncated = False

    def _try_append(op: dict) -> bool:
        nonlocal total
        n = len(op["cand"]) + len(op["ref"])
        if total + n > cap:
            return False
        ops.append(op)
        total += n
        return True

    for tag, i1, i2, j1, j2 in opcodes:
        if tag == "equal" and (i2 - i1) > ctx * 2:
            head = _mk_diff_op("equal", i1, i1 + ctx, j1, j1 + ctx, cand_insns, ref_insns)
            tail = _mk_diff_op("equal", i2 - ctx, i2, j2 - ctx, j2, cand_insns, ref_insns)
            for op in (head, tail):
                if not _try_append(op):
                    truncated = True
                    break
            if truncated:
                break
        else:
            if not _try_append(_mk_diff_op(tag, i1, i2, j1, j2, cand_insns, ref_insns)):
                truncated = True
                break
    return ops, truncated


def _classify_score_context(scores: dict, warnings: dict, diff_ops: list[dict],
                             frame: dict) -> list[dict]:
    """Route diagnostics to the documented recovery levers. Pure function --
    no I/O, no recompilation. Each rule fires independently; several may fire
    on the same function.
    """
    rules: list[dict] = []

    if warnings.get("fpu"):
        rules.append({
            "rule": "fpu_operand_order",
            "evidence": f"{len(warnings['fpu'])} FPU-WARN detail line(s)",
            "action": "Check cross-product argument order and FSUB/FSUBR operand "
                      "direction (CLAUDE.md call-site pitfall #4; "
                      "lift-score-improve Step 3c).",
        })

    if warnings.get("loadw"):
        rules.append({
            "rule": "loadw_field_width",
            "evidence": f"{len(warnings['loadw'])} LOADW-WARN detail line(s)",
            "action": "Narrow field read as a wider type (int vs int16_t/int8_t); "
                      "verify the C type against disassembly. "
                      "docs/lift-learnings.md section 24.",
        })

    if warnings.get("imm"):
        rules.append({
            "rule": "imm_wrong_literal",
            "evidence": f"{len(warnings['imm'])} IMM-WARN detail line(s)",
            "action": "Wrong float/magic numeric literal; verify against the "
                      "disassembly immediate and prefer a named constant. "
                      "docs/lift-learnings.md immediate-constant section.",
        })

    if warnings.get("fcom"):
        rules.append({
            "rule": "fcom_bound_sense",
            "evidence": f"{len(warnings['fcom'])} FCOM-WARN detail line(s)",
            "action": "FPU comparison bound sense (<= lifted as <, >= as >, or "
                      "swapped/negated form); verify TEST AH,imm / Jcc against "
                      "the pristine disassembly. docs/lift-learnings.md section 38.",
        })

    cand_frame = frame.get("cand_frame_bytes")
    ref_frame = frame.get("ref_frame_bytes")
    if cand_frame is not None and ref_frame is not None and cand_frame != ref_frame:
        rules.append({
            "rule": "frame_mismatch",
            "evidence": f"cand `sub esp, {cand_frame:#x}` vs ref `sub esp, {ref_frame:#x}`",
            "action": "VC71 shape levers: volatile store/reload locals, never take "
                      "&param, mind else-block sinking, match assert-form "
                      "comparisons. docs/lift-learnings.md section 27; "
                      "lift-score-improve Step 3d.",
        })

    pushl_movl = 0
    for op in diff_ops:
        if op["kind"] != "replace":
            continue
        for c, r in zip(op["cand"], op["ref"]):
            cm = c.split()[0].lower() if c.split() else ""
            rm = r.split()[0].lower() if r.split() else ""
            if (cm.startswith("push") and rm.startswith("mov")) or \
               (cm.startswith("mov") and rm.startswith("push")):
                pushl_movl += 1
    if pushl_movl >= _PUSHL_MOVL_MIN:
        rules.append({
            "rule": "regarg_structural_ceiling",
            "evidence": f"{pushl_movl} replace-op pushl<->movl pair(s), likely at "
                        f"@<reg> call sites",
            "action": "@<reg>-arg structural ceiling (~1 mnemonic per reg-arg per "
                      "call site) -- document, don't chase further.",
        })

    dp = scores.get("dp_lcs_pct")
    off = scores.get("official_pct")
    if dp is not None and off is not None and (dp - off) > _ANCHOR_COLLAPSE_GAP:
        rules.append({
            "rule": "anchor_collapse",
            "evidence": f"dp_lcs {dp:.1f}% vs official {off:.1f}% (gap {dp - off:.1f}pp)",
            "action": "SequenceMatcher greedy-anchor collapse artifact -- keep "
                      "fixing the regions the diff shows, don't trust the "
                      "official-score cliff.",
        })

    if frame.get("ref_has_chkstk_call") and not frame.get("cand_has_chkstk_call"):
        rules.append({
            "rule": "chkstk_static_buffer",
            "evidence": "reference calls _chkstk/alloca_probe; candidate does not",
            "action": "Static-buffer ceiling: convert the large local from a plain "
                      "stack declaration to `static` (_chkstk is now a no-op stub, "
                      "so the linker error that motivated the workaround is gone). "
                      "docs/lift-learnings.md section 20; lift-score-improve Step 0.",
        })

    return rules


def _build_score_context(
    fn: str,
    compiled_insns: list[str],
    reference_insns: list[str],
    official_pct: float,
    fpu_warnings: list[str],
    loadw_warnings: list[str],
    imm_warnings: list[str],
    fcom_warnings: list[str],
    source: Path,
    ref_info: dict,
    regdef_params,
    co,
) -> dict:
    """Assemble the machine-readable score-context pack for one function.

    Post-processing only over already-disassembled instruction lists and
    already-computed warnings -- no recompilation, no re-disassembly. Extra
    work versus the console-only run: one operand-normalized rescoring pass
    (pure Python over the disassembled listings, same as the existing
    advisory `opnd` tag), one DP-LCS pass (guarded by a size cap), and one
    more SequenceMatcher pass for the diff opcodes (same cost class as the
    official score itself).
    """
    n_c, n_r = len(compiled_insns), len(reference_insns)
    scored_insns, n_stripped, preprocessing = co.select_regparam_candidate(
        compiled_insns, reference_insns, regdef_params, reg_normalize=False)

    raw_mnemonic_pct = co.compare_functions(
        compiled_insns, reference_insns, reg_normalize=False)[0]
    abi_modeled_mnemonic_pct = co.compare_functions(
        scored_insns, reference_insns, reg_normalize=False)[0]

    opnd_pct = co.compare_functions(
        scored_insns, reference_insns, reg_normalize=True)[0]

    c_seq = co.extract_mnemonic_sequence(scored_insns)
    r_seq = co.extract_mnemonic_sequence(reference_insns)
    dp_pct = co.dp_lcs_ratio(c_seq, r_seq)
    if dp_pct is not None:
        dp_pct *= 100.0

    opcodes = co.mnemonic_diff_opcodes(scored_insns, reference_insns)
    diff_ops, truncated = _build_diff_ops(scored_insns, reference_insns, opcodes)

    frame = {
        "cand_frame_bytes": _first_frame_bytes(compiled_insns),
        "ref_frame_bytes": _first_frame_bytes(reference_insns),
        "ref_has_chkstk_call": _has_chkstk_call(reference_insns),
        "cand_has_chkstk_call": _has_chkstk_call(compiled_insns),
    }

    scores = {
        "official_pct": official_pct,
        "raw_mnemonic_pct": raw_mnemonic_pct,
        "abi_modeled_mnemonic_pct": abi_modeled_mnemonic_pct,
        "abi_model": preprocessing,
        "operand_normalized_pct": opnd_pct,
        "dp_lcs_pct": dp_pct,
        "n_cand_insns": n_c,
        "n_scored_cand_insns": len(scored_insns),
        "n_ref_insns": n_r,
        "preprocessing": preprocessing,
        "regparam_loads_stripped": n_stripped,
    }

    warnings = {
        "fpu": list(fpu_warnings or []),
        "loadw": list(loadw_warnings or []),
        "imm": list(imm_warnings or []),
        "fcom": list(fcom_warnings or []),
    }

    classification = _classify_score_context(scores, warnings, diff_ops, frame)
    if ref_info.get("kind") == "thunk" and ref_info.get("n_insns") == 1:
        classification.insert(0, {
            "rule": "forwarding_reference",
            "evidence": "reference is a one-instruction forwarding entry",
            "action": "Verify the jump target and score forwarding entry and "
                      "target body separately before editing source.",
        })

    addr = _func_addr(fn)
    try:
        tu = str(source.relative_to(REPO_ROOT))
    except ValueError:
        tu = str(source)
    try:
        candidate_source_sha256 = hashlib.sha256(source.read_bytes()).hexdigest()
    except OSError:
        candidate_source_sha256 = None

    return {
        "schema": 2,
        "name": fn,
        "addr": f"0x{addr:08x}" if addr is not None else None,
        "tu": tu,
        # A score context is reusable only for the exact candidate source that
        # produced it. Packs without this field are legacy misses.
        "candidate_source_sha256": candidate_source_sha256,
        "reference": ref_info,
        "generated_at": datetime.datetime.now(datetime.timezone.utc)
            .strftime("%Y-%m-%dT%H:%M:%SZ"),
        "scores": scores,
        "frame": frame,
        "warnings": warnings,
        "diff": {"ops": diff_ops, "truncated": truncated},
        "classification": classification,
    }


def _write_score_context(pack: dict) -> Path:
    SCORE_CONTEXT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = SCORE_CONTEXT_DIR / f"{pack['name']}.json"
    with open(out_path, "w") as f:
        json.dump(pack, f, indent=1)
        f.write("\n")
    return out_path


# Mixed-optimization TUs: a handful of functions in an otherwise /O2 file were
# built with optimization disabled (MSVC #pragma optimize("",off)), so the
# reference spills every temp to EBP-relative slots, keeps a large frame, and
# emits JMP-to-next-instruction. One flag per file cannot score both groups, so
# these functions get a second compile at their own flag.
#
# ai.c: FUN_000425c0 has SUB ESP,0x40 with every value round-tripped through the
# stack, while ai_update next door is fully register-allocated. Measured across
# all 54 scored functions in the TU: exactly 2 gain at /Od, 50 lose (many by
# 40-80pp). FUN_000425c0 36.1% (/O2) -> 79.5% (/Od).
_PER_FUNCTION_OPT: dict[str, dict[str, str]] = {
    "ai/ai.c": {"FUN_000425c0": "/Od"},
}


def _per_function_opt_for(source: Path) -> dict[str, str]:
    """Per-function optimization overrides for a mixed-optimization TU."""
    key = str(source).replace("\\", "/")
    for tu, overrides in _PER_FUNCTION_OPT.items():
        if key.endswith(tu):
            return dict(overrides)
    return {}


def load_compare_obj():
    """Import compare_obj.py as a module (its CLI path sets up sys.path itself)."""
    import importlib.util
    spec = importlib.util.spec_from_file_location("compare_obj", str(COMPARE_SCRIPT))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def derive_reference(fn: str, source: Path, tu_funcs: dict, co):
    """(insns, meta) for fn's canonical reference, or (None, reason).

    THE reference path.  The bytes at [start, end) in the pristine XBE, where
    the bound comes from the committed tools/verify/function_bounds.json, are
    wrapped in a COFF and disassembled with the same llvm-objdump the candidate
    goes through.  `meta` is {addr, end, kind, provenance, obj, n_r, sha}.

    Module-level rather than a closure inside the scoring loop so the tests and
    tools/verify/validate_ref_migration.py measure exactly what a verify run
    measures, instead of a re-implementation that can drift from it.
    """
    addr = _resolve_func_addr(fn, source, tu_funcs)
    if addr is None:
        return None, ("no address: absent from kb.json for this TU and no "
                      "/* 0xADDR */ comment in the source")
    try:
        import xbe_reference as xr
    except Exception as exc:                       # pragma: no cover
        return None, f"xbe_reference unavailable ({exc})"
    ext = xr.function_extent(addr)
    if ext is None:
        return None, (f"no bound for 0x{addr:x} in function_bounds.json "
                      f"(regenerate: tools/verify/function_bounds.py)")
    end, kind, provenance = ext
    obj = xr.reference_object(addr)
    if obj is None:
        return None, f"could not synthesize a reference object for 0x{addr:x}"
    aliases = (set(function_aliases(fn, source))
               | {fn, f"FUN_{addr & 0xffffffff:08x}"})
    try:
        insns = co.first_function_insns(str(obj), aliases)
    except Exception as exc:                       # pragma: no cover
        return None, f"reference object unreadable ({exc})"
    if not insns:
        return None, "reference decoded to zero instructions"
    # Raw XBE bytes keep the real cross-reference addresses; a delinked object
    # and a VC71 candidate both store those as zeroed fields plus a relocation.
    # Undo that one asymmetry -- and ONLY that one -- so the two sides are
    # comparable without loosening the scoring metric.
    insns = [xr.normalize_synth_insn(i) for i in insns]
    if kind == "table_data":
        # A hand-verified `table_data` bound deliberately includes the switch /
        # index table MSVC placed in .text after the final ret, so the span
        # covers the whole function.  Those bytes are DATA and decode as
        # garbage; the reference must stop at the last real instruction.
        # first_function_insns already trims this in the common cases (a
        # data-decode marker, or a scaled indirect jmp proving a jump table
        # exists); this is the backstop for a table whose bytes happen to
        # decode as plausible instructions.
        insns = _trim_after_last_terminator(insns)
    return insns, {
        "addr": addr, "end": end, "kind": kind, "provenance": provenance,
        "obj": obj, "n_r": len(insns),
        # Identity of the exact instruction list that was scored against.
        # Hashed AFTER normalize_synth_insn (the reference-side rewrite) but
        # BEFORE the scorer's own normalization, which collapses immediates and
        # displacements -- a hash taken after that would be blind to a
        # reference carrying the wrong constants.
        "sha": hashlib.sha256("\n".join(insns).encode()).hexdigest()[:16],
    }


def run_compare_cached(
    compiled: Path,
    source: Path,
    extra_args: list[str],
    cache,
    no_cache: bool,
    quiet: bool = False,
    opt: str = "/O2",
    score_context: bool = True,
    per_fn_opt: dict[str, str] | None = None,
    regcall_elide: bool = False,
) -> int:
    """Score every function in `compiled` against its canonical reference.

    There is no `reference` object parameter any more: each function's reference
    is derived from the pristine XBE, bounded by the committed
    tools/verify/function_bounds.json entry (see `derive_reference`).

    Imports compare_obj as a module so results can be cached per function.
    """
    _set_alias_source(source)
    # Lazy import so compare_obj.py's sys.path setup runs in subprocess context
    # when invoked standalone, but we can reuse it as a library here.
    try:
        co = load_compare_obj()
    except Exception as exc:
        # There is no subprocess fallback any more: compare_obj.py's CLI takes
        # two object files, and the reference side is now built in-process from
        # the XBE rather than existing on disk as a delinked object.
        print(f"Could not import compare_obj as a module ({exc})", file=sys.stderr)
        return 1

    # Parse extra_args subset we need
    fn_filter = None
    threshold = 50.0
    show_diffs = False
    fpu_only = False
    loadw_only = False
    imm_only = False
    fcom_only = False
    reg_normalize = False
    regdef_override_str = None
    i = 0
    while i < len(extra_args):
        a = extra_args[i]
        if a in ("--function", "-f") and i + 1 < len(extra_args):
            fn_filter = extra_args[i + 1]; i += 2
        elif a in ("--threshold", "-t") and i + 1 < len(extra_args):
            threshold = float(extra_args[i + 1]); i += 2
        elif a in ("--show-diffs", "-d"):
            show_diffs = True; i += 1
        elif a == "--fpu-only":
            fpu_only = True; i += 1
        elif a == "--loadw-only":
            loadw_only = True; i += 1
        elif a == "--imm-only":
            imm_only = True; i += 1
        elif a == "--fcom-only":
            fcom_only = True; i += 1
        elif a in ("--reg-normalize", "-r"):
            reg_normalize = True; i += 1
        elif a == "--regdef-params" and i + 1 < len(extra_args):
            regdef_override_str = extra_args[i + 1]; i += 2
        else:
            i += 1

    compiled_funcs: dict[str, list[str]] = co.disassemble(str(compiled))

    # Mixed-optimization TU: recompile at the override flag and swap in only
    # those functions' bodies. Everything else keeps the primary-pass object.
    fn_opt: dict[str, str] = {}
    if per_fn_opt:
        by_flag: dict[str, list[str]] = {}
        for name, flag in per_fn_opt.items():
            if flag != opt:
                by_flag.setdefault(flag, []).append(name)
        for flag, names in sorted(by_flag.items()):
            alt_obj = compiled.with_name(
                f"{compiled.stem}.opt{flag.replace('/', '').replace(' ', '')}.obj"
            )
            if not compile_vc71(source, alt_obj, regcall_elide=regcall_elide,
                                opt=flag):
                print(f"[opt] per-function recompile at {flag} failed; "
                      f"{', '.join(sorted(names))} stay at {opt}",
                      file=sys.stderr)
                continue
            alt_funcs = co.disassemble(str(alt_obj))
            for name in names:
                if name in alt_funcs:
                    compiled_funcs[name] = alt_funcs[name]
                    fn_opt[name] = flag
                    if not quiet:
                        print(f"[opt] {name}: mixed-optimization TU -> "
                              f"scored at {flag}", flush=True)

    # ---- Reference derivation ------------------------------------------
    #
    # ONE canonical reference per function, derived from two committed inputs:
    # the pristine XBE and tools/verify/function_bounds.json.  The bytes at
    # [start, end) are wrapped in a COFF and disassembled with the SAME
    # llvm-objdump the candidate goes through, so neither side can pick up a
    # disassembler-spelling difference the other does not have.
    #
    # This replaces a four-rung selection ladder (whole-object slice -> sibling
    # range export -> per-function chunk -> synthesized fallback).  Every rung
    # was a way to measure the wrong bytes: a whole-object slice runs to the
    # next SYMBOL, so it absorbs alignment filler or the neighbouring function;
    # a per-function chunk could be stale; and when no rung produced a bounded
    # reference the function was DROPped, which emits no score line at all and
    # so reads as "this file has fewer functions" rather than as a measurement
    # gap.
    #
    # delinked/ and the Ghidra delinker are untouched.  The permuter, unicorn,
    # z3 and objdiff lanes EXECUTE the oracle and still need real relocations;
    # only scoring moved off them.
    tu_funcs = _kb_functions_for_source(source)
    reference_funcs: dict[str, list[str]] = {}
    ref_meta: dict[str, dict] = {}
    ref_failures: dict[str, str] = {}
    matched: set[str] = set()

    def _reference_for(fn: str):
        return derive_reference(fn, source, tu_funcs, co)

    if fn_filter:
        want = _resolve_compiled_name(fn_filter, compiled_funcs, source)
        if want is None:
            print(f"Function {fn_filter} not found in the compiled object")
            print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
            return 1
        insns, info = _reference_for(want)
        if insns is None:
            print(f"Function {want}: no reference could be derived — {info}")
            return 1
        reference_funcs[want] = insns
        ref_meta[want] = info
        matched = {want}
    else:
        for fn in compiled_funcs:
            insns, info = _reference_for(fn)
            if insns is None:
                ref_failures[fn] = info
                continue
            reference_funcs[fn] = insns
            ref_meta[fn] = info
            matched.add(fn)

    # Provenance, one machine-parseable line per scored function.  Emitted even
    # under --quiet, because vc71_regression parses a --quiet run.
    #
    #   SYNTHREF <fn>
    #       Kept for compatibility: vc71_regression stamps ref="synth" from it.
    #       Every reference is now derived, so this fires for every function.
    #   REFMETA <fn> addr=.. end=.. kind=.. n_r=.. sha=..
    #       Which bytes were scored against, and their identity.  `kind` comes
    #       straight from the bounds table, so a `no_terminator` bound (a
    #       function whose end could not be proven from a terminator) is visible
    #       downstream instead of looking like any other score.
    for fn in sorted(matched):
        m = ref_meta[fn]
        print(f"  SYNTHREF {fn}", flush=True)
        print(f"  REFMETA {fn} addr=0x{m['addr']:08x} end=0x{m['end']:08x} "
              f"kind={m['kind']} n_r={m['n_r']} sha={m['sha']}", flush=True)

    # Functions we could NOT score.  These produce no score line, so a gate that
    # only sees score lines would miss them entirely; emit one machine-parseable
    # DROP each.  Whole-file mode only -- a --function run scores exactly one
    # requested symbol and reports its own failure above.
    #
    # A DROP means exactly one thing and keeps meaning it: a COMPILED function
    # for which no reference could be derived.  Deliberately NOT extended to
    # "kb.json lists a ported function this object does not define" -- that is a
    # real condition, but vc71_regression already reports it from its own
    # _expected_ported_functions, and vc71_regression treats every DROP as a
    # hard failure under --strict.  Feeding a second, differently-derived
    # source-mapping heuristic into that gate risks failing CI on a stale
    # kb.json `source` field rather than on anything about the lift.
    if fn_filter is None:
        for fn in sorted(ref_failures):
            span = _func_span(fn)
            if span is None:
                continue  # not a kb.json-tracked function (helper/thunk/static)
            print(f"  DROP {fn}: no valid reference — {ref_failures[fn]} "
                  f"(span {span} bytes)", flush=True)

    if not matched:
        print("No functions could be scored in this translation unit")
        print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
        return 1

    any_fail = False
    any_fpu_warn = False
    any_loadw_warn = False
    any_imm_warn = False
    any_fcom_warn = False
    hits = 0
    misses = 0

    # @reg-DEFINED prologue modeling: explicit --regdef-params overrides the
    # kb.json-derived per-function lookup (see _regdef_params_for).
    regdef_override = None
    if regdef_override_str:
        regdef_override = []
        for tok in regdef_override_str.split(","):
            idx, _, reg = tok.partition(":")
            regdef_override.append((int(idx.strip()), reg.strip().lower()))

    for fn in sorted(matched):
        regdef = (regdef_override if regdef_override is not None
                  else _regdef_params_for(fn, co))
        cached_result = None
        # Every function now has one derived reference, so there is no
        # per-function reference override to bypass the cache for.  The cache
        # key covers the bounds entry the reference was cut from (see
        # vc71_cache.make_cache_key), so a moved bound invalidates on its own.
        # A mixed-optimization function is keyed on its own flag so it cannot
        # collide with a primary-pass entry.
        cache_opt = fn_opt.get(fn, opt)
        if not no_cache and cache is not None:
            cached_result = cache.get(fn, source, None, opt=cache_opt)

        if cached_result is not None:
            hits += 1
            pct = cached_result["match_pct"]
            fpu_warnings = cached_result["fpu_warnings"]
            loadw_warnings = cached_result.get("loadw_warnings") or []
            imm_warnings = cached_result.get("imm_warnings") or []
            fcom_warnings = cached_result.get("fcom_warnings") or []
            # diff_lines may be None if we didn't store diffs (e.g. not show_diffs)
            diffs = cached_result["diff_lines"] or []
            cache_tag = " [cache hit]"
        else:
            misses += 1
            pct, diffs, fpu_warnings, loadw_warnings, imm_warnings, fcom_warnings = co.compare_functions(
                compiled_funcs[fn], reference_funcs[fn],
                reg_normalize=reg_normalize,
                regdef_params=regdef,
            )
            cache_tag = ""
            if regdef and not quiet:
                n_stripped = co.strip_regparam_loads(
                    compiled_funcs[fn], reference_funcs[fn], regdef)[1]
                if n_stripped:
                    raw_pct = co.compare_functions(
                        compiled_funcs[fn], reference_funcs[fn],
                        reg_normalize=reg_normalize)[0]
                    print(f"  [REGPARM] {fn}: stripped {n_stripped} @<reg> "
                          f"phantom load(s); raw {raw_pct:.1f}% -> modeled {pct:.1f}%")
            # Store result; always save diff_lines so future --show-diffs works.
            if cache is not None and not no_cache:
                cache.put(fn, source, None, pct, fpu_warnings, diffs,
                          loadw_warnings=loadw_warnings, imm_warnings=imm_warnings,
                          fcom_warnings=fcom_warnings, opt=cache_opt)

        n_c = len(compiled_funcs[fn])
        n_r = len(reference_funcs[fn])
        metric_insns, abi_model_items, abi_model = co.select_regparam_candidate(
            compiled_funcs[fn], reference_funcs[fn], regdef,
            reg_normalize=False)
        raw_mnemonic_pct = co.compare_functions(
            compiled_funcs[fn], reference_funcs[fn],
            reg_normalize=False)[0]
        abi_modeled_mnemonic_pct = co.compare_functions(
            metric_insns, reference_funcs[fn], reg_normalize=False)[0]
        status = "PASS" if pct >= threshold else "FAIL"
        fpu_tag = " [FPU-WARN]" if fpu_warnings else ""
        loadw_tag = " [LOADW-WARN]" if loadw_warnings else ""
        imm_tag = " [IMM-WARN]" if imm_warnings else ""
        fcom_tag = " [FCOM-WARN]" if fcom_warnings else ""
        # A bound the generator could not close on a terminator: the reference
        # may run past the real body or stop short of it.  Tagged rather than
        # suppressed, so the score is still measured but is identifiable as
        # resting on a weaker bound.  See function_bounds.py's `kind` field.
        kind_tag = (" [BOUND-WARN:no_terminator]"
                    if ref_meta[fn]["kind"] == "no_terminator" else "")

        reg_tag = ""
        if reg_normalize:
            mnem_pct = co.compare_functions(
                compiled_funcs[fn], reference_funcs[fn], reg_normalize=False,
                regdef_params=regdef)[0]
            reg_tag = f" [struct:{mnem_pct:.1f}%]"

        only_mode = fpu_only or loadw_only or imm_only or fcom_only

        # Advisory operand-normalized score.  The primary `pct` above is a
        # mnemonic-only LCS, which is blind to operand-level bugs (swapped
        # arguments, wrong memory source, wrong register shape).  Recompute with
        # reg_normalize=True -- mnemonic + operand shape with canonical
        # registers -- and surface it alongside.  Pure Python over the already
        # disassembled listings; nothing is recompiled or re-disassembled.
        # ADVISORY ONLY: never gates, never cached, never affects the exit code,
        # and appended AFTER the "NN.N% match" token so existing parsers
        # (lift_pipeline.parse_match_percent*) still see the primary score first.
        opnd_tag = ""
        if not only_mode and not reg_normalize:
            opnd_pct = co.compare_functions(
                metric_insns, reference_funcs[fn], reg_normalize=True)[0]
            opnd_tag = f" | opnd {opnd_pct:.1f}% (operand-normalized)"

        abi_model_tag = ""
        if abi_model != "raw":
            abi_model_tag = (
                f" | raw {raw_mnemonic_pct:.1f}% | abi-modeled "
                f"{abi_modeled_mnemonic_pct:.1f}% "
                f"[{abi_model}:{abi_model_items}]"
            )

        if not only_mode:
            if quiet:
                print(f"  {status} {fn}: {pct:.1f}% match ({n_c}/{n_r} insns){reg_tag}{fpu_tag}{loadw_tag}{imm_tag}{fcom_tag}{kind_tag}{opnd_tag}{abi_model_tag}")
            else:
                print(f"  {status} {fn}: {pct:.1f}% match ({n_c}/{n_r} insns){reg_tag}{fpu_tag}{loadw_tag}{imm_tag}{fcom_tag}{kind_tag}{opnd_tag}{abi_model_tag}{cache_tag}")

        if fpu_warnings:
            any_fpu_warn = True
            if not loadw_only and not imm_only and not fcom_only:
                if fpu_only:
                    print(f"  {fn}:{fpu_tag}" + ("" if quiet else cache_tag))
                for w in fpu_warnings:
                    print(w)

        if loadw_warnings:
            any_loadw_warn = True
            # Detail lines are noisy (many benign codegen diffs across the tree),
            # so only expand them in the dedicated --loadw-only mode. In a normal
            # run the compact [LOADW-WARN] tag on the status line is the hint.
            if loadw_only:
                print(f"  {fn}:{loadw_tag}" + ("" if quiet else cache_tag))
                for w in loadw_warnings:
                    print(w)

        if imm_warnings:
            any_imm_warn = True
            # IMM-WARN is a low-false-positive detector (both sides are VC71
            # codegen, so a large inline-constant diff is a real source-literal
            # mismatch), so -- unlike LOADW -- expand its detail lines in a normal
            # run: the specific reference-vs-lift constants are the actionable hint.
            if not fpu_only and not loadw_only and not fcom_only:
                if imm_only:
                    print(f"  {fn}:{imm_tag}" + ("" if quiet else cache_tag))
                for w in imm_warnings:
                    print(w)

        if fcom_warnings:
            any_fcom_warn = True
            # Detail lines are dense in FPU-heavy TUs (45 of real_math's 171
            # functions carry structural comparison divergence), so -- like
            # LOADW -- only expand them in the dedicated --fcom-only mode; the
            # compact [FCOM-WARN] tag on the status line is the hint.
            if fcom_only:
                print(f"  {fn}:{fcom_tag}" + ("" if quiet else cache_tag))
                for w in fcom_warnings:
                    print(w)

        if status == "FAIL":
            any_fail = True

        if score_context:
            m = ref_meta[fn]
            try:
                obj_str = str(m["obj"].relative_to(REPO_ROOT))
            except ValueError:
                obj_str = str(m["obj"])
            # `per_function`/`synthesized` are kept (and are now always True)
            # so a consumer written against the old pack shape keeps working.
            ref_info = {
                "obj": obj_str, "per_function": True, "synthesized": True,
                "addr": f"0x{m['addr']:08x}", "end": f"0x{m['end']:08x}",
                "kind": m["kind"], "bound_provenance": m["provenance"],
                "n_insns": m["n_r"], "sha": m["sha"],
            }
            if m["kind"] == "thunk":
                ref_info["forwarding_target"] = _forwarding_target_info(m["addr"])

            pack = _build_score_context(
                fn, compiled_funcs[fn], reference_funcs[fn], pct,
                fpu_warnings, loadw_warnings, imm_warnings, fcom_warnings,
                source, ref_info, regdef, co,
            )
            ctx_path = _write_score_context(pack)
            if not only_mode and not quiet and pct < 100.0:
                try:
                    rel_ctx = ctx_path.relative_to(REPO_ROOT)
                except ValueError:
                    rel_ctx = ctx_path
                print(f"  score-context: {rel_ctx}")

        if show_diffs and diffs and not only_mode and not quiet:
            for d in diffs:
                print(d)

    if (hits or misses) and not quiet:
        total = hits + misses
        print(f"\n  Cache: {hits}/{total} hits ({100*hits//total if total else 0}%)")

    if any_fpu_warn and not loadw_only and not imm_only and not fcom_only:
        print("\nWARNING: FPU operand-order differences detected.")
        print("Check cross-product argument order and FSUB/FSUBR operand direction.")

    if any_loadw_warn and not fpu_only and not imm_only and not fcom_only:
        if loadw_only:
            print("\nWARNING: load-width (int vs int16_t/int8_t) differences detected.")
            print("A field the original narrows (movsx/movzx word/byte) is read wider in our lift,")
            print("or vice versa. Verify the C type against disassembly. See lift-learnings 'int vs int16_t'.")
        elif not quiet:
            print("\n[LOADW-WARN] load-width differences found; re-run with --loadw-only for details "
                  "(int vs int16_t/int8_t; see lift-learnings §24).")

    if any_imm_warn and not fpu_only and not loadw_only and not fcom_only:
        print("\nWARNING: immediate-constant differences detected.")
        print("A large inline constant (float bit-pattern or magic) differs between our lift and the")
        print("original. This is a likely source-literal mismatch -- verify the")
        print("numeric literal against the disassembly immediate. See lift-learnings 'immediate-constant'.")

    if any_fcom_warn and not fpu_only and not loadw_only and not imm_only:
        if fcom_only:
            print("\nWARNING: FPU-guard bound-sense differences detected.")
            print("A float comparison's TEST/Jcc guard shape differs between our lift and the original.")
            print("Both sides are VC71 codegen, so this is a source comparison-form mismatch (<= lifted")
            print("as <, >= as >, swapped operands, or positive vs negated form). Verify each bound")
            print("against the pristine disassembly's TEST AH,imm / Jcc pair. See lift-learnings")
            print("section 38 (the MP look-up assert class).")
        elif not quiet:
            print("\n[FCOM-WARN] FPU-guard bound-sense differences found; re-run with --fcom-only for "
                  "details (<= vs <; see lift-learnings section 38).")

    weak_bounds = sorted(fn for fn in matched
                         if ref_meta[fn]["kind"] == "no_terminator")
    if weak_bounds and not quiet:
        print(f"\n[BOUND-WARN] {len(weak_bounds)} function(s) scored against a "
              "reference whose end could not be proven from a terminator "
              "(function_bounds.json kind=no_terminator). Review the entry's "
              "`note` before trusting the score.")
        for fn in weak_bounds:
            m = ref_meta[fn]
            print(f"  {fn}: 0x{m['addr']:08x}-0x{m['end']:08x} "
                  f"({m['n_r']} insns)")

    return 1 if any_fail else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", nargs="?", help="Source file to verify")
    ap.add_argument("--function", "-f", help="Compare only this function")
    ap.add_argument("--show-diffs", "-d", action="store_true")
    ap.add_argument("--fpu-only", action="store_true", help="Only show FPU warnings")
    ap.add_argument("--loadw-only", action="store_true",
                    help="Only show load-width (int vs int16/int8) warnings")
    ap.add_argument("--imm-only", action="store_true",
                    help="Only show immediate-constant (wrong float/magic literal) warnings")
    ap.add_argument("--fcom-only", action="store_true",
                    help="Only show FPU-guard bound-sense (<= vs <) warnings")
    ap.add_argument("--threshold", "-t", type=float, default=50.0)
    ap.add_argument("--list", action="store_true", help="List available units")
    ap.add_argument("--skip-compile", action="store_true", help="Reuse existing VC71 .obj")
    ap.add_argument("--quiet", "-q", action="store_true",
                    help="Minimal output: match %% per function, FAIL, and FPU-WARN only")
    ap.add_argument("--no-cache", action="store_true",
                    help="Disable cache: always recompile and recompare")
    ap.add_argument("--rebuild-cache", action="store_true",
                    help="Drop existing cache entries for this source before run")
    ap.add_argument("--reg-normalize", "-r", action="store_true",
                    help="Use register-alias normalization for operand-level comparison")
    ap.add_argument("--opt", default="/O2",
                    help="MSVC optimization flag (default /O2 speed; use /O1 for size-optimized "
                         "prebuilt libs like XAPILIB/CRT that use compact push-imm8/leave idioms)")
    ap.add_argument("--regcall-elide", action="store_true",
                    help="Cast register-arg callee calls so VC71 generates matching "
                         "call-site sequences (mov+call instead of push+call+add)")
    ap.add_argument("--regdef-params", default=None,
                    help="Override @<reg>-DEFINED param modeling for the compared "
                         "function(s): 'idx:reg[,idx:reg]' e.g. '0:eax'. By default "
                         "this is derived per function from kb.json @<reg> annotations.")
    ap.add_argument("--no-synth-ref", action="store_true",
                    help=argparse.SUPPRESS)  # deprecated no-op; see below
    ap.add_argument("--no-score-context", action="store_true",
                    help="Disable machine-readable score-context pack output "
                         "(artifacts/score_context/<name>.json); on by default")
    ap.add_argument("--skip-decl-regen", action="store_true",
                    help="Do not regenerate build/generated/decl.h from kb.json "
                         "before compiling. Only for callers that already pinned "
                         "the header this process (e.g. a sharded batch run); a "
                         "stale header silently blanks a whole TU's scores")
    args = ap.parse_args()

    # --no-synth-ref used to force a DROP where no delinked reference bounded a
    # function.  Derivation from the pristine XBE plus the committed bounds
    # table is now the ONLY reference path, so there is nothing to fall back
    # from.  Accepted and ignored so an old command line still runs.
    if args.no_synth_ref:
        print("[deprecated] --no-synth-ref is a no-op: every reference is now "
              "derived from the pristine XBE and function_bounds.json",
              file=sys.stderr)

    if args.list:
        units = load_units()
        def list_key(unit: dict) -> tuple[str, str]:
            return (unit.get("metadata", {}).get("source_path", ""), unit.get("base_path", ""))

        for u in sorted(units, key=list_key):
            src = u.get("metadata", {}).get("source_path", "?")
            ref = u.get("base_path", "?")
            has_ref = "OK" if (REPO_ROOT / ref).exists() else "MISSING"
            print(f"  {src}  ref={ref} [{has_ref}]")
        return

    if not args.source:
        ap.print_help()
        sys.exit(1)

    source = Path(args.source)
    if not source.is_absolute():
        source = REPO_ROOT / source

    if not source.exists():
        print(f"Source file not found: {source}", file=sys.stderr)
        sys.exit(1)

    # Pin decl.h == kb.json before any compile.  --skip-compile still wants it:
    # the fastcall decl shadow is derived from this header.
    if not args.skip_decl_regen:
        regen_decl_header(quiet=args.quiet)

    # Size-optimized prebuilt library TUs (XAPILIB / CRT) were compiled /O1, so
    # they use compact idioms (push imm8/pop, leave) that /O2 never emits.
    # Auto-select /O1 for those when the caller didn't override it. Verified:
    # xbox_crt timer fns jump 81-82% (/O2) -> 100% (/O1).
    # Captured before the auto-select rules below rewrite args.opt.
    opt_was_default = args.opt == _DEFAULT_OPT

    _O1_TUS = ("cseries/xbox_crt.c",)
    if args.opt == "/O2" and any(str(source).replace("\\", "/").endswith(t) for t in _O1_TUS):
        args.opt = "/O1"
        if not args.quiet:
            print(f"[opt] size-optimized library TU detected -> using /O1", flush=True)

    # The original game code was built WITHOUT compiler auto-inlining: the
    # binary CALLs tiny same-TU helpers everywhere (e.g. 0x13c030 calls
    # 0x13d680; 0xb1760 calls 0xa95a0) instead of inlining them.  Our merged
    # TUs make /O2's implied /Ob2 inline those helpers into the candidate,
    # scrambling scores file-wide.  Auto-select /O2 /Ob1 for the big merged
    # game TUs.  Verified: FUN_0013c030 56.1% (/Ob2) -> 100.0% (/Ob1);
    # game_engine.c mean 84.9 -> 86.4 with 22 functions gaining >5pp vs
    # 4 dropping <7pp.
    _OB1_TUS = ("game/game_engine.c", "objects/objects.c", "units/units.c")
    if args.opt == "/O2" and any(str(source).replace("\\", "/").endswith(t) for t in _OB1_TUS):
        args.opt = "/O2 /Ob1"
        if not args.quiet:
            print(f"[opt] merged game TU detected -> using /O2 /Ob1", flush=True)

    obj_name = source.stem + ".obj"
    vc71_obj = VC71_OUT_DIR / obj_name

    # Set up cache (unless --no-cache)
    cache = None
    if not args.no_cache:
        try:
            from verify.vc71_cache import get_default_cache
            cache = get_default_cache()
        except ImportError:
            try:
                sys.path.insert(0, str(REPO_ROOT / "tools"))
                from verify.vc71_cache import get_default_cache
                cache = get_default_cache()
            except ImportError:
                pass

        if cache is not None and args.rebuild_cache:
            dropped = cache.invalidate(source_path=source)
            if not args.quiet:
                print(f"[cache] Dropped {dropped} stale entries for {source.name}", flush=True)

    # Decide whether to skip compile.  We can skip if:
    #   1. --skip-compile is set, OR
    #   2. cache is active AND every function in the object has a cache hit
    #      (we can only know this after checking the cache; we handle it below)
    need_compile = not args.skip_compile

    # Fast path: if cache is active and the compiled obj already exists,
    # try satisfying the entire run from cache before touching the compiler.
    # We still need the obj for disassembly in case of any miss — so we only
    # skip compile when ALL functions are cache hits.
    # Only reuse the object when it was compiled from the current source CONTENT
    # at the current flags — see obj_is_current for what a stale object costs and
    # why mtime could not answer this.  The SQLite result cache keys on the
    # source hash and was never the problem; only this object reuse is.
    obj_current = obj_is_current(vc71_obj, source, args.opt)
    if need_compile and cache is not None and obj_current and not args.no_cache:
        # We'll attempt cached compare first; compile only if there are misses.
        # The cached-compare path reads this object for disassembly on misses,
        # which is sound now that we know it matches the current source.
        need_compile = False  # tentative; compile_vc71 called below if obj absent
    elif (need_compile and cache is not None and vc71_obj.exists()
          and not args.no_cache and not args.quiet):
        print(f"[cache] {vc71_obj.name} was not built from the current "
              f"{source.name}; recompiling", flush=True)

    if need_compile:
        if not args.quiet:
            print(f"Compiling {source.name} with VC71 cl.exe...", flush=True)
        t0 = time.perf_counter()
        if not compile_vc71(source, vc71_obj, regcall_elide=args.regcall_elide, opt=args.opt):
            sys.exit(1)
        if not args.quiet:
            elapsed = time.perf_counter() - t0
            print(f"Compiled in {elapsed:.1f}s", flush=True)
    elif not args.skip_compile and vc71_obj.exists():
        if not args.quiet:
            print(f"Using cached VC71 object: {vc71_obj.name}", flush=True)
    elif args.skip_compile and vc71_obj.exists() and not obj_current:
        # Explicit user request, so it is honoured — but say plainly that the
        # scores below describe whatever source built that object, not the file
        # on disk.  Always printed, --quiet included: a silent stale measurement
        # is the exact failure this stamp exists to end.
        print(f"WARNING: --skip-compile is reusing {vc71_obj.name}, which was "
              f"NOT built from the current {source.name}; scores describe stale "
              f"code", file=sys.stderr, flush=True)

    if not vc71_obj.exists():
        if not args.quiet:
            print(f"Compiling {source.name} with VC71 cl.exe...", flush=True)
        t0 = time.perf_counter()
        if not compile_vc71(source, vc71_obj, regcall_elide=args.regcall_elide, opt=args.opt):
            sys.exit(1)
        if not args.quiet:
            print(f"Compiled in {time.perf_counter() - t0:.1f}s", flush=True)

    if not args.quiet:
        print("Comparing against references derived from the pristine XBE "
              "(bounds: tools/verify/function_bounds.json)...\n", flush=True)
    extra = []
    if args.function:
        extra += ["--function", args.function]
    if args.show_diffs:
        extra += ["--show-diffs"]
    if args.fpu_only:
        extra += ["--fpu-only"]
    if args.loadw_only:
        extra += ["--loadw-only"]
    if args.imm_only:
        extra += ["--imm-only"]
    if args.fcom_only:
        extra += ["--fcom-only"]
    if args.reg_normalize:
        extra += ["--reg-normalize"]
    if args.regdef_params:
        extra += ["--regdef-params", args.regdef_params]
    extra += ["--threshold", str(args.threshold)]

    # Only auto-apply per-function overrides when the caller didn't pin --opt;
    # an explicit flag means the user is deliberately measuring one setting.
    per_fn_opt = _per_function_opt_for(source) if opt_was_default else {}

    rc = run_compare_cached(
        vc71_obj, source, extra, cache, no_cache=args.no_cache,
        quiet=args.quiet, opt=args.opt,
        score_context=not args.no_score_context,
        per_fn_opt=per_fn_opt, regcall_elide=args.regcall_elide,
    )
    sys.exit(rc)


if __name__ == "__main__":
    main()
