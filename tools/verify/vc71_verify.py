#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Compile a source file with Visual C++ 7.1 and compare against delinked reference.

Finds the matching delinked reference via objdiff.json, compiles the source with
CL.Exe (MSVC 13.10.3077 — the same compiler that built cachebeta.xbe),
and runs instruction-level comparison to flag FPU operand-order differences.

Usage:
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c --function FUN_0009ac90
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c --show-diffs
    python3 tools/verify/vc71_verify.py --list  # show available units
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c --no-cache
    python3 tools/verify/vc71_verify.py src/halo/effects/decals.c --rebuild-cache
"""

import argparse
import bisect
import datetime
import functools
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
DELINKED_DIR = REPO_ROOT / "delinked"

# Machine-readable per-function score-context packs (diff ops, warning
# detail, DP-LCS score, classification) -- see _build_score_context().
# Default ON; disable with --no-score-context. Already covered by the
# repo-wide `artifacts` gitignore entry.
SCORE_CONTEXT_DIR = REPO_ROOT / "artifacts" / "score_context"

# Fall back to a reference synthesized from the pristine XBE when no delinked
# reference bounds a function.  Purely additive: it is consulted only where the
# run would otherwise emit a DROP and produce no score line at all.  Disable
# with --no-synth-ref.  See tools/verify/xbe_reference.py.
SYNTH_REFS = True

VC71_CL = r"C:\Program Files (x86)\RXDK\xbox\bin\vc71\CL.Exe"
VC71_CL_WSL = "/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71/CL.Exe"
RXDK_INC = r"C:\Program Files (x86)\RXDK\xbox\include"

COMPARE_SCRIPT = REPO_ROOT / "tools" / "verify" / "compare_obj.py"

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
    return s


def load_units() -> list[dict]:
    """Load objdiff.json units.

    Multiple delinked references may exist for one source file, for example a
    full object plus one or more per-function exports. Keep all of them so a
    function-specific verify can choose a reference that actually contains the
    requested symbol.
    """
    with open(OBJDIFF_JSON) as f:
        data = json.load(f)
    units = []
    for u in data.get("units", []):
        src = u.get("metadata", {}).get("source_path")
        if src:
            units.append(u)
    return units


def find_units(source: str, units: list[dict]) -> list[dict]:
    """Find all objdiff units matching a source file path."""
    source = str(source).replace("\\", "/")
    matches = []
    for unit in units:
        key = unit.get("metadata", {}).get("source_path", "")
        if key and (source.endswith(key) or key.endswith(source)):
            matches.append(unit)
    return matches


def function_aliases(function: str | None) -> set[str]:
    """Return possible symbol names for a function across source/ref objects."""
    if not function:
        return set()

    fn = function.lstrip("_")
    aliases = {fn}

    try:
        kb = _load_kb()
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
    except (OSError, ValueError, json.JSONDecodeError):
        pass

    return aliases


def object_symbols(obj_path: Path) -> set[str]:
    """List normalized defined symbols in an object file."""
    result = subprocess.run(["llvm-nm", str(obj_path)], capture_output=True, text=True)
    if result.returncode != 0:
        return set()

    symbols = set()
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-2].upper() in {"T", "t"}:
            symbols.add(parts[-1].lstrip("_"))
    return symbols


def _per_function_ref(function: str) -> Path | None:
    """Return delinked/functions/<hex8>.obj if it exists for this function address.

    Also accepts an unpadded-hex filename (e.g. c0f50.obj for 0x000c0f50) —
    exporters have produced both forms, and a name-format mismatch silently
    skips VC71 scoring (goal-lift then records 0% and parks a good lift; see
    commits f8e29209/daa39ee6).
    """
    aliases = function_aliases(function)
    for alias in aliases:
        m = re.match(r"FUN_([0-9a-f]{8})$", alias, re.IGNORECASE)
        if m:
            hex8 = m.group(1).lower()
            for stem in (hex8, hex8.lstrip("0") or "0"):
                candidate = DELINKED_DIR / "functions" / f"{stem}.obj"
                if candidate.exists():
                    return candidate
    return None


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


def _func_addr(function: str) -> int | None:
    """Resolve a function name/alias to its start address, or None."""
    for alias in function_aliases(function) | {function}:
        m = re.match(r"FUN_0*([0-9a-fA-F]+)$", alias or "")
        if m:
            return int(m.group(1), 16)
    return None


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
    """Byte span of a function, or None when it is not tracked in kb.json.

    kb.json's gap (distance to the next *listed* function) overshoots wherever
    the listing has a hole -- and kb.json is not a full listing of the binary,
    so holes are common.  A function followed by an unlisted neighbour gets a
    span many times its real size, and `_ref_insns_valid` then rejects its
    CORRECT reference as "truncated" (measured: FUN_0015c2d0, 33 real insns,
    kb gap 800 bytes for a 102-byte function).

    So: take the binary's answer, capped by the kb gap.  The cap matters
    because the kb gap is a genuine upper bound (the next listed function is
    real code that cannot be part of this one), and it also bounds the
    disassembly window.

    None is preserved for functions absent from kb.json -- callers use that to
    mean "not a kb.json-tracked function" (helper/thunk/static), which is a
    different condition from "tracked, size unknown".
    """
    if function in _func_span_cache:
        return _func_span_cache[function]
    addr = _func_addr(function)
    starts = _kb_func_starts()
    span: int | None = None
    if addr is not None and starts:
        i = bisect.bisect_right(starts, addr)
        kb_gap = (starts[i] - addr) if i < len(starts) else None
        if kb_gap:
            true_size = _true_end_offset(addr, kb_gap)
            span = min(kb_gap, true_size) if true_size else kb_gap
        else:
            span = kb_gap
    _func_span_cache[function] = span
    return span


def _trim_trailing_padding(insns: list[str]) -> list[str]:
    """Drop inter-function alignment padding from the end of a symbol's slice.

    A symbol slice runs to the next symbol, so it absorbs whatever alignment
    filler the linker put after the function.  Measured 2026-07-29:
    delinked/bipeds.obj's FUN_001a0680 slice is 152 instructions where the
    function is 91 -- instruction 91 is the real `ret` (pristine XBE 0x1a0680
    +0xf3), followed by 61 instructions of `nop`-run + `ret` filler.  That
    padding is pure mismatch against a candidate that does not have it, so the
    function scored 61.5% against the bloated slice and 86.7% against a
    correctly-bounded per-function chunk.

    Keeps the last instruction that is neither `nop` nor `ret`, plus one
    trailing `ret` if present -- so a normal `pop ebp; ret` ending survives
    untouched, a multi-`ret` body is never cut (the last real instruction still
    follows every interior `ret`), and only genuine trailing filler is removed.
    """
    def _mnem(insn: str) -> str:
        return insn.split(None, 1)[0] if insn else ""

    def _is_pad(insn: str) -> bool:
        # "nop" also covers the multi-byte forms objdump prints with operands
        # ("nopw 0x0(%eax)", "nopl ..."), which a plain equality test misses.
        m = _mnem(insn)
        return m.startswith("nop") or m in ("ret", "retl", "retw", "retq")

    last_real = -1
    for i, insn in enumerate(insns):
        if not _is_pad(insn):
            last_real = i
    if last_real < 0:
        return insns  # nothing but nop/ret: an empty stub, leave it alone
    end = last_real + 1
    if end < len(insns) and _mnem(insns[end]) in ("ret", "retl", "retw", "retq"):
        end += 1
    return insns[:end]


_true_insn_cache: dict[int, int | None] = {}
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


def _true_insn_count(function: str) -> int | None:
    """Instruction count of a function as it exists in the pristine XBE.

    The binary is the source of truth for where a function ends, and nothing
    else here is: kb.json's span is the distance to the next *listed* function,
    which overshoots wherever the listing has a gap (FUN_000b97b0's span is
    480 bytes for a 196-byte function), and a reference slice runs to the next
    symbol in its own object, which overshoots into padding or the neighbouring
    function.  Used only to choose between already-valid references.

    Counts up to the first `ret` followed by `nop` padding, which is where MSVC
    ends a function.  Returns None when capstone or the XBE is unavailable, so
    every caller must treat None as "no opinion".
    """
    if function in _true_insn_cache:
        return _true_insn_cache[function]
    result = None
    try:
        import capstone
        addr = _func_addr(function)
        span = _func_span(function)
        if addr is not None and span:
            code = _xbe_read(addr, span + 32)
            if code:
                md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
                insns = list(md.disasm(code, addr))
                for i, insn in enumerate(insns):
                    if (insn.mnemonic == "ret" and i + 1 < len(insns)
                            and insns[i + 1].mnemonic.startswith("nop")):
                        result = i + 1
                        break
                else:
                    result = len(insns) or None
    except Exception:
        result = None
    _true_insn_cache[function] = result
    return result


def _closer_to_truth(a: int, b: int, truth: int | None) -> bool:
    """Whether length a is a better fit for the function than length b."""
    if truth is None:
        return False
    return abs(a - truth) < abs(b - truth)


def _slice_is_bloated(insns: list[str]) -> bool:
    """Whether a reference slice over-ran the function into alignment filler.

    Used only to pick BETWEEN references, never to edit the compared
    instruction lists.  Trimming the lists for scoring is the more complete fix
    but recalibrates every score in the committed baseline: padding present on
    both sides currently contributes free LCS matches, so symmetric trimming
    *lowers* honest scores (measured 2026-07-29: files.c file_open 85.2% ->
    80.6% with the reference unchanged).  That needs a deliberate baseline
    repopulate, not a drive-by change.
    """
    return len(_trim_trailing_padding(insns)) < len(insns)


def _ref_insns_valid(n_r: int, span: int | None) -> bool:
    """Whether a reference's instruction count plausibly matches the function's
    byte size.  Rejects both truncated and bloated references:

    - n_r * 15 (max x86 instruction length) < span  => truncated (too few insns
      to cover the function's bytes).
    - n_r > span                                     => bloated (more insns than
      bytes is impossible for real code; the reference swallowed neighbours).

    The bloat bound matters here because ~stale per-function chunks (pre-fix,
    0x2000-byte window) still exist on disk; without it the fallback could pick
    a bloated chunk over a good whole-object reference.  Mirrors the gate in
    vc71_regression.py.
    """
    if not n_r:
        return False
    if span and n_r * 15 < span:
        return False
    if span and n_r > span:
        return False
    return True


def choose_unit(source: str, units: list[dict], function: str | None) -> dict | None:
    """Choose the best delinked reference for a source/function pair."""
    matches = find_units(source, units)
    existing = []
    for unit in matches:
        ref = REPO_ROOT / unit.get("base_path", "")
        if ref.exists():
            existing.append(unit)

    aliases = function_aliases(function) if function else set()

    # A function-specific export is the authoritative reference for a targeted
    # comparison; do not let a whole-TU unit shadow it when both are present.
    per_func = _per_function_ref(function) if function else None
    if per_func:
        return {
            "base_path": str(per_func.relative_to(REPO_ROOT)),
            "metadata": {"source_path": str(source)},
        }

    # Among the references registered for THIS TU, prefer the one that actually
    # covers the most functions.  A partial range export or per-function chunk
    # carries one or two symbols, so choosing one silently DROPs every other
    # function in the file -- and a DROP produces no score line, so the loss is
    # invisible in the output.  Measured 2026-07-29: objects.c was scored
    # against a 2-symbol objects_FUN_00084a10.obj while a 754-symbol
    # objects.obj sat on disk, and actor_moving.c against 1 of 33.
    #
    # Ranked by symbol count rather than by name, because neither direction of
    # a name rule is safe alone: delinked/units.obj holds 4 symbols while the
    # units_batch*.obj slices hold more (so "prefer the exact stem" would LOSE
    # coverage), yet delinked/actors.obj is a candidate for actor_moving.c
    # while being a different TU (so "prefer the biggest" would pick a wrong
    # reference and fake the score).  Counting only same-TU names gets both.
    # Same-TU means the exact stem, or the stem followed only by address-range
    # / FUN_<addr> suffixes -- the forms the delinker emits for a slice of one
    # TU.  A bare "<stem>_<word>" is NOT accepted: sibling TUs share prefixes,
    # so files.c would otherwise adopt the 368-symbol files_windows.obj (a
    # different TU) over its own correct 17-symbol files.obj and report scores
    # against the wrong code.  This deliberately also skips units_new.obj /
    # objects_full.obj: they may well be wider exports of the same TU, but the
    # name cannot prove it, and a wrong reference fakes the score.
    stem = Path(source).stem.lower()
    _range_suffix = re.compile(r"^(?:_FUN_[0-9a-f]+|_[0-9a-f]{4,})+$", re.IGNORECASE)

    def _same_tu(base: str) -> bool:
        b = base.lower()
        if not b.endswith(".obj"):
            return False
        b = b[:-len(".obj")]
        if b == stem:
            return True
        if not b.startswith(stem):
            return False
        return bool(_range_suffix.match(b[len(stem):]))

    @functools.lru_cache(maxsize=None)
    def _n_symbols(base_path: str) -> int:
        return len(object_symbols(REPO_ROOT / base_path))

    def unit_priority(unit: dict) -> tuple[bool, bool, int, str]:
        name = unit.get("name", "")
        base_path = unit.get("base_path", "")
        base = Path(base_path).name
        # Prefer units whose name explicitly contains the function address (per-function export)
        name_matches = any(alias in name for alias in aliases if alias.startswith("FUN_"))
        # Then prefer full object files over chunk exports
        is_chunk = bool(re.match(r"[0-9a-f]{8}\.obj$", base, re.IGNORECASE))
        # Then the widest same-TU reference.  Chunks are per-function by
        # construction, so counting them is pointless and would cost hundreds
        # of llvm-nm calls per run (delinked/functions/*.obj); leave them at 0
        # so the is_chunk tier keeps them last regardless.
        width = -_n_symbols(base_path) if (_same_tu(base) and not is_chunk) else 0
        return (not name_matches, is_chunk, width, base)

    existing.sort(key=unit_priority)

    if not function:
        return existing[0] if existing else None

    for unit in existing:
        ref_path = REPO_ROOT / unit["base_path"]
        if object_symbols(ref_path) & aliases:
            return unit

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

    return True


def run_compare(compiled: Path, reference: Path, extra_args: list[str]) -> int:
    """Run compare_obj.py and return its exit code (legacy path, no caching)."""
    cmd = [sys.executable, str(COMPARE_SCRIPT), str(compiled), str(reference)] + extra_args
    return subprocess.run(cmd).returncode


def _build_rename_map(compiled_keys: set[str], matched: set[str]) -> dict[str, str]:
    """Build {declared_name -> FUN_xxx} rename map from kb.json."""
    rename_map: dict[str, str] = {}
    if not (compiled_keys - matched):
        return rename_map
    try:
        kb = _load_kb()
        for obj in kb.get("objects", []):
            for fn_entry in obj.get("functions", []):
                addr = fn_entry.get("addr", "")
                decl = fn_entry.get("decl", "")
                m = re.search(r"\b(\w+)\s*\(", decl)
                if m and addr:
                    declared_name = m.group(1)
                    fun_name = f"FUN_{int(addr, 16):08x}"
                    if declared_name != fun_name:
                        rename_map[declared_name] = fun_name
    except Exception:
        pass
    return rename_map


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

    opnd_pct = co.compare_functions(
        compiled_insns, reference_insns, reg_normalize=True)[0]

    c_seq = co.extract_mnemonic_sequence(compiled_insns)
    r_seq = co.extract_mnemonic_sequence(reference_insns)
    dp_pct = co.dp_lcs_ratio(c_seq, r_seq)
    if dp_pct is not None:
        dp_pct *= 100.0

    opcodes = co.mnemonic_diff_opcodes(compiled_insns, reference_insns)
    diff_ops, truncated = _build_diff_ops(compiled_insns, reference_insns, opcodes)

    frame = {
        "cand_frame_bytes": _first_frame_bytes(compiled_insns),
        "ref_frame_bytes": _first_frame_bytes(reference_insns),
        "ref_has_chkstk_call": _has_chkstk_call(reference_insns),
        "cand_has_chkstk_call": _has_chkstk_call(compiled_insns),
    }

    scores = {
        "official_pct": official_pct,
        "operand_normalized_pct": opnd_pct,
        "dp_lcs_pct": dp_pct,
        "n_cand_insns": n_c,
        "n_ref_insns": n_r,
    }

    warnings = {
        "fpu": list(fpu_warnings or []),
        "loadw": list(loadw_warnings or []),
        "imm": list(imm_warnings or []),
        "fcom": list(fcom_warnings or []),
    }

    classification = _classify_score_context(scores, warnings, diff_ops, frame)

    addr = _func_addr(fn)
    try:
        tu = str(source.relative_to(REPO_ROOT))
    except ValueError:
        tu = str(source)

    return {
        "name": fn,
        "addr": f"0x{addr:08x}" if addr is not None else None,
        "tu": tu,
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


def run_compare_cached(
    compiled: Path,
    reference: Path,
    source: Path,
    extra_args: list[str],
    cache,
    no_cache: bool,
    quiet: bool = False,
    opt: str = "/O2",
    score_context: bool = True,
) -> int:
    """Run per-function comparison with cache integration.

    Imports compare_obj as a module so results can be cached per function.
    Falls back to subprocess invocation if import fails.
    """
    # Lazy import so compare_obj.py's sys.path setup runs in subprocess context
    # when invoked standalone, but we can reuse it as a library here.
    try:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            "compare_obj", str(COMPARE_SCRIPT)
        )
        co = importlib.util.util if False else None  # sentinel
        co = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(co)
    except Exception as exc:
        print(f"[cache] Could not import compare_obj as module ({exc}); falling back to subprocess", file=sys.stderr)
        return run_compare(compiled, reference, extra_args)

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
    reference_funcs: dict[str, list[str]] = co.disassemble(str(reference))

    matched: set[str] = set(compiled_funcs.keys()) & set(reference_funcs.keys())

    # Delinked XDK objects can keep C++ namespace-qualified symbols while our
    # C source emits plain function names.
    namespace_map: dict[str, str] = {}
    for ref_name in reference_funcs:
        short_name = ref_name.rsplit("::", 1)[-1]
        if short_name != ref_name and short_name in compiled_funcs:
            namespace_map[short_name] = ref_name
            if ref_name not in matched:
                compiled_funcs[ref_name] = compiled_funcs[short_name]
                matched.add(ref_name)
    rename_map = _build_rename_map(set(compiled_funcs.keys()), matched)

    if fn_filter:
        fn = fn_filter.lstrip("_")
        if fn not in matched:
            namespace_name = namespace_map.get(fn)
            if namespace_name and namespace_name in reference_funcs and fn in compiled_funcs:
                compiled_funcs[namespace_name] = compiled_funcs[fn]
                matched = {namespace_name}
                fn = namespace_name
            else:
                old_name = rename_map.get(fn)
                if old_name and old_name in reference_funcs and fn in compiled_funcs:
                    compiled_funcs[old_name] = compiled_funcs[fn]
                    matched = {old_name}
                    fn = old_name
                else:
                    print(f"Function {fn} not found in both objects")
                    print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
                    print(f"  reference: {sorted(reference_funcs.keys())[:10]}")
                    return 1
        matched = {fn}
    else:
        for new_name, old_name in rename_map.items():
            if new_name in compiled_funcs and old_name in reference_funcs and new_name not in matched:
                compiled_funcs[old_name] = compiled_funcs[new_name]
                matched.add(old_name)

    # NB: the "no matching functions" bail-out is deferred until *after* the
    # per-function fallback below, so a TU whose whole-object reference has no
    # symbol overlap (missing/truncated) can still recover functions from valid
    # per-function chunks instead of returning empty here.

    # Per-function reference fallback: when a function's whole-object reference
    # is unusable — truncated (instruction count cannot span its kb.json byte
    # size) or dropped entirely (e.g. a last-function excluded by the BFT COFF
    # relocation-bug truncation workaround) — score it against its
    # function-aligned per-function chunk instead, if that chunk's symbol is
    # present and itself valid.  Gated on byte span in BOTH directions so a
    # stale (pre-fix, bloated) chunk is never preferred over a good reference.
    ref_overrides: set[str] = set()
    _chunk_truncation: dict[str, dict] = {}

    def _valid_chunk_ref(fn: str):
        """Instruction list from fn's per-function chunk, or None if unusable.

        Uses the chunk-aware, boundary-capped disassembly (co.first_function_insns)
        so a stale 0x2000-window chunk that packed following functions/stub slots
        under the same symbol collapses to its true first function — which then
        either scores honestly or fails the byte-span validity gate (and is
        quarantined for re-delink) rather than producing a false low against
        swallowed neighbours.
        """
        chunk = _per_function_ref(fn)
        if not chunk or not chunk.exists():
            return None
        aliases = set(function_aliases(fn)) | {fn}
        addr = _func_addr(fn)
        if addr is not None:
            aliases.add(f"FUN_{addr & 0xffffffff:08x}")
        try:
            cand = co.first_function_insns(str(chunk), aliases)
        except Exception:
            return None
        if not cand:
            return None
        if not _ref_insns_valid(len(cand), _func_span(fn)):
            return None
        try:
            bounded = co.count_bounded_insns(str(chunk), aliases)
        except Exception:
            bounded = None
        if bounded is not None and bounded > 0:
            ratio = len(cand) / bounded
            if ratio < 0.90:
                _chunk_truncation[fn] = {
                    "parsed": len(cand), "bounded": bounded,
                    "ratio": round(ratio, 3), "lost": bounded - len(cand),
                }
        return cand

    def _synth_ref(fn: str):
        """Instruction list from a reference synthesized out of the pristine XBE.

        FALLBACK ONLY.  Consulted where no delinked reference bounds the
        function -- the case that otherwise emits a DROP and no score at all.
        It cannot displace a delinked reference, so no existing score moves.

        Why this can stand in for a delinked reference: the scorer consumes
        instruction TEXT, and `compare_obj.normalize_instruction` already
        rewrites immediates and displacements before matching, so the symbols
        and relocations the delinker reconstructs are discarded anyway.
        Measured agreement against 1,464 existing chunks: 95.9%, and the
        disagreements are dominated by stale 0x2000-window chunks where the
        synthesized reference is the correct one.

        Runs the object through the same `co.first_function_insns` used for
        chunks (shared boundary/padding logic), then applies
        `normalize_synth_insn` to undo the one asymmetry raw bytes introduce:
        a delinked object stores cross-references as zeroed fields, so its
        absolute displacements print as nothing.
        """
        if not SYNTH_REFS:
            return None
        addr = _func_addr(fn)
        if addr is None:
            return None
        try:
            import xbe_reference as xr
            obj = xr.reference_object(addr)
        except Exception:
            return None          # capstone/XBE unavailable -- stay silent
        if obj is None:
            return None
        aliases = set(function_aliases(fn)) | {fn, f"FUN_{addr & 0xffffffff:08x}"}
        try:
            cand = co.first_function_insns(str(obj), aliases)
        except Exception:
            return None
        if not cand:
            return None
        cand = [xr.normalize_synth_insn(i) for i in cand]
        return cand if _ref_insns_valid(len(cand), _func_span(fn)) else None

    _tu_units_cache: list[dict] = []

    def _tu_units() -> list[dict]:
        """Registered units for this TU (loaded once, not once per function)."""
        if not _tu_units_cache:
            try:
                _tu_units_cache.extend(find_units(str(source), load_units()))
            except Exception:
                _tu_units_cache.append({})
        return [u for u in _tu_units_cache if u]

    _sib_objs_cache: list[Path] = []
    _sib_slices_cache: dict[str, dict] = {}

    def _sibling_objects() -> list[Path]:
        """Other whole/range references for this TU, disassembled at most once.

        Skips delinked/functions/<hex8>.obj -- those are per-function chunks
        already covered by _valid_chunk_ref, and a TU like objects.c registers
        ~150 of them, which would otherwise be disassembled per function.
        """
        if not _sib_objs_cache:
            for u in _tu_units():
                p = REPO_ROOT / u.get("base_path", "")
                if not p.exists():
                    continue
                if re.match(r"[0-9a-f]{8}\.obj$", p.name, re.IGNORECASE):
                    continue
                try:
                    if p.samefile(reference):
                        continue
                except OSError:
                    continue
                _sib_objs_cache.append(p)
        return _sib_objs_cache

    def _sib_slices(p: Path) -> dict:
        key = str(p)
        if key not in _sib_slices_cache:
            try:
                _sib_slices_cache[key] = co.disassemble(key)
            except Exception:
                _sib_slices_cache[key] = {}
        return _sib_slices_cache[key]

    def _sibling_refs(fn: str) -> list[list[str]]:
        """Slices of fn from OTHER registered references for the same TU.

        A narrow range export is often the only correctly-bounded reference for
        a function: FUN_000b97b0 is 68 instructions in the pristine XBE, and
        delinked/ has it at 68 (player_queues_b97b0_b9880.obj), 112
        (…_b9900.obj) and 154 (player_queues_new.obj).  Without this the
        widest-reference rule scores it against the 154-instruction slice.
        """
        out = []
        for p in _sibling_objects():
            slices = _sib_slices(p)
            for alias in set(function_aliases(fn)) | {fn}:
                s = slices.get(alias)
                if s and _ref_insns_valid(len(s), _func_span(fn)):
                    out.append(s)
                    break
        return out

    # (1) Replace a whole-object slice that does not bound the function well.
    # The pristine XBE decides: a slice is preferred when its instruction count
    # is closer to the function's real length.  This catches both failure modes
    # a byte-span gate cannot -- padding over-run (bipeds.obj FUN_001a0680 is
    # 152 instructions for a 91-instruction function, scoring 61.5% against the
    # filler vs 83.2% correctly bounded) and over-run into the neighbouring
    # function, which carries no padding to detect.
    for fn in list(matched):
        whole = reference_funcs.get(fn, [])
        truth = _true_insn_count(fn)
        span_ok = _ref_insns_valid(len(whole), _func_span(fn))
        # Nothing to do when the slice is already valid, unpadded, and either
        # matches the binary or the binary gave no opinion.
        if span_ok and not _slice_is_bloated(whole) and (
                truth is None or len(whole) == truth):
            continue
        alternates = _sibling_refs(fn)
        chunk = _valid_chunk_ref(fn)
        if chunk is not None:
            alternates.append(chunk)
        best, best_len = None, len(whole)
        for alt in alternates:
            if truth is not None:
                if _closer_to_truth(len(alt), best_len, truth):
                    best, best_len = alt, len(alt)
            elif _slice_is_bloated(whole) and not _slice_is_bloated(alt):
                # No binary opinion: only trust the unambiguous padding signal.
                best, best_len = alt, len(alt)
            elif not span_ok and len(alt) > best_len:
                best, best_len = alt, len(alt)
        if best is not None:
            reference_funcs[fn] = best
            ref_overrides.add(fn)

    # (2) Add candidate functions the whole-object reference dropped entirely.
    # A synthesized reference is the last resort here, after both the whole
    # object and the per-function chunk have failed to bound the function.
    synth_used: set[str] = set()
    for fn in list(compiled_funcs.keys()):
        if fn in matched:
            continue
        cand = _valid_chunk_ref(fn)
        if cand is None:
            cand = _synth_ref(fn)
            if cand is not None:
                synth_used.add(fn)
        if cand is not None:
            reference_funcs[fn] = cand
            matched.add(fn)
            ref_overrides.add(fn)

    chunk_overrides = ref_overrides - synth_used
    if chunk_overrides and not quiet:
        shown = ", ".join(sorted(chunk_overrides)[:6])
        more = " ..." if len(chunk_overrides) > 6 else ""
        print(f"[ref] {len(chunk_overrides)} function(s) scored against per-function "
              f"chunk (whole-object reference truncated): {shown}{more}", flush=True)
    # Always announce a synthesized reference, even under --quiet: it is a
    # different provenance from a Ghidra-delinked one and a score carrying it
    # should never look like a delinked-backed score.
    if synth_used:
        shown = ", ".join(sorted(synth_used)[:6])
        more = " ..." if len(synth_used) > 6 else ""
        print(f"[synth] {len(synth_used)} function(s) scored against a reference "
              f"synthesized from the pristine XBE (no delinked reference bounds "
              f"them): {shown}{more}", flush=True)
        # Machine-parseable, one per function and never truncated, so callers can
        # record provenance per entry.  The human line above elides after 6.
        # Same convention as the DROP lines below.
        for fn in sorted(synth_used):
            print(f"  SYNTHREF {fn}", flush=True)

    # Report compiled, kb.json-tracked functions we could NOT score against any
    # valid reference (whole-object truncated/absent AND no valid per-function
    # chunk).  These never produce a score line, so vc71_regression's gate — which
    # only sees scored functions — would miss them and leave the re-delink queue
    # incomplete.  Emit a machine-parseable DROP line per function so the runner
    # can record them.  Only in whole-file mode (a --function run scores exactly
    # one requested symbol; a miss there is already reported above).
    if fn_filter is None:
        for fn in sorted(compiled_funcs.keys()):
            # A renamed function is scored under whichever symbol its reference
            # carries: if the delinked ref still uses the pre-rename FUN_<addr>,
            # the rename bridge scores it under that name and adds *it* (not the
            # source's real name) to `matched`.  So a compiled `magnitude3d` whose
            # ref symbol is `FUN_00012f10` is NOT literally in `matched` yet is
            # not a drop — check the function's aliases too.
            if fn in matched or (function_aliases(fn) & matched):
                continue
            span = _func_span(fn)
            if span is None:
                continue  # not a kb.json-tracked function (helper/thunk/static)
            chunk = _per_function_ref(fn)
            if chunk and chunk.exists():
                reason = ("per-function chunk invalid after boundary cap "
                          "(stale/truncated) — re-delink")
            else:
                reason = ("no reference (whole-object truncated/absent, no "
                          "per-function chunk) — delink")
            print(f"  DROP {fn}: no valid reference — {reason} (span {span} bytes)",
                  flush=True)

    if not matched:
        print("No matching functions found between objects")
        print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
        print(f"  reference: {sorted(reference_funcs.keys())[:10]}")
        return 1

    any_fail = False
    any_fpu_warn = False
    any_loadw_warn = False
    any_imm_warn = False
    any_fcom_warn = False
    any_trunc_warn = False
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
        # Overridden functions were scored against a per-function chunk, not the
        # whole-object `reference` the cache key is derived from — bypass cache.
        if not no_cache and cache is not None and fn not in ref_overrides:
            cached_result = cache.get(fn, source, reference, opt=opt)

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
            # Skip overridden functions — their score is against a per-function
            # chunk, not the whole-object reference the cache key encodes.
            if cache is not None and not no_cache and fn not in ref_overrides:
                cache.put(fn, source, reference, pct, fpu_warnings, diffs,
                          loadw_warnings=loadw_warnings, imm_warnings=imm_warnings,
                          fcom_warnings=fcom_warnings, opt=opt)

        n_c = len(compiled_funcs[fn])
        n_r = len(reference_funcs[fn])
        status = "PASS" if pct >= threshold else "FAIL"
        fpu_tag = " [FPU-WARN]" if fpu_warnings else ""
        loadw_tag = " [LOADW-WARN]" if loadw_warnings else ""
        imm_tag = " [IMM-WARN]" if imm_warnings else ""
        fcom_tag = " [FCOM-WARN]" if fcom_warnings else ""
        trunc_info = _chunk_truncation.get(fn)
        trunc_tag = (f" [TRUNC-WARN:{trunc_info['parsed']}/{trunc_info['bounded']}]"
                     if trunc_info else "")

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
                compiled_funcs[fn], reference_funcs[fn], reg_normalize=True,
                regdef_params=regdef)[0]
            opnd_tag = f" | opnd {opnd_pct:.1f}% (operand-normalized)"

        if not only_mode:
            if quiet:
                print(f"  {status} {fn}: {pct:.1f}% match ({n_c}/{n_r} insns){reg_tag}{fpu_tag}{loadw_tag}{imm_tag}{fcom_tag}{trunc_tag}{opnd_tag}")
            else:
                print(f"  {status} {fn}: {pct:.1f}% match ({n_c}/{n_r} insns){reg_tag}{fpu_tag}{loadw_tag}{imm_tag}{fcom_tag}{trunc_tag}{opnd_tag}{cache_tag}")

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

        if trunc_info:
            any_trunc_warn = True

        if status == "FAIL":
            any_fail = True

        if score_context:
            if fn in synth_used:
                ref_info = {"obj": None, "per_function": False, "synthesized": True}
            elif fn in ref_overrides:
                per_ref = _per_function_ref(fn)
                try:
                    obj_str = str(per_ref.relative_to(REPO_ROOT)) if per_ref else None
                except ValueError:
                    obj_str = str(per_ref) if per_ref else None
                ref_info = {"obj": obj_str, "per_function": True, "synthesized": False}
            else:
                try:
                    obj_str = str(reference.relative_to(REPO_ROOT))
                except ValueError:
                    obj_str = str(reference)
                ref_info = {"obj": obj_str, "per_function": False, "synthesized": False}

            pack = _build_score_context(
                fn, compiled_funcs[fn], reference_funcs[fn], pct,
                fpu_warnings, loadw_warnings, imm_warnings, fcom_warnings,
                source, ref_info, co,
            )
            if trunc_info:
                pack["warnings"]["trunc"] = trunc_info
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
        print("original. Both sides are VC71 codegen, so this is a source-literal mismatch -- verify the")
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

    if any_trunc_warn and not quiet:
        trunc_fns = sorted(_chunk_truncation.keys())
        print(f"\n[TRUNC-WARN] {len(trunc_fns)} per-function delinked ref(s) appear "
              "internally truncated (first_function_insns returned <90% of the "
              "bounded instruction count). Score may be against a partial function "
              "-- re-export with a corrected address range.")
        for tfn in trunc_fns:
            ti = _chunk_truncation[tfn]
            print(f"  {tfn}: {ti['parsed']}/{ti['bounded']} insns "
                  f"({ti['ratio']:.1%}, lost {ti['lost']})")

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
                    help="Do not fall back to a reference synthesized from the "
                         "pristine XBE when no delinked reference bounds a "
                         "function; report the function as a DROP instead")
    ap.add_argument("--no-score-context", action="store_true",
                    help="Disable machine-readable score-context pack output "
                         "(artifacts/score_context/<name>.json); on by default")
    args = ap.parse_args()

    global SYNTH_REFS
    SYNTH_REFS = not args.no_synth_ref

    units = load_units()

    if args.list:
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

    # Size-optimized prebuilt library TUs (XAPILIB / CRT) were compiled /O1, so
    # they use compact idioms (push imm8/pop, leave) that /O2 never emits.
    # Auto-select /O1 for those when the caller didn't override it. Verified:
    # xbox_crt timer fns jump 81-82% (/O2) -> 100% (/O1).
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

    unit = choose_unit(str(source), units, args.function)
    if not unit:
        print(f"No usable objdiff.json unit found for {source}", file=sys.stderr)
        if args.function:
            aliases = ", ".join(sorted(function_aliases(args.function)))
            print(f"No existing delinked reference contains: {aliases}", file=sys.stderr)
        print("Run with --list to see available units")
        sys.exit(1)

    ref_path = REPO_ROOT / unit["base_path"]
    if not ref_path.exists():
        print(f"Delinked reference not found: {ref_path}", file=sys.stderr)
        print("Export it via: python3 tools/audit/batch_delink.py --object <name>")
        sys.exit(1)

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
            dropped = cache.invalidate(source_path=source, ref_path=ref_path)
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
    # Only reuse the object when it is at least as new as the source.  Reusing a
    # STALE object silently measures code that is no longer in the tree, in both
    # directions:
    #   * a function deleted from the source keeps scoring from the old object --
    #     FUN_000f56b0 sat at exactly 81.8% across 13 attempts while having no
    #     definition at all, and the unmoving score read as a structural ceiling.
    #   * a function newly added to the source is absent from the object, so
    #     --function <new fn> aborts with "not found in both objects" and the
    #     lift pipeline reports "VC71 compilation or comparison failed".
    # The SQLite result cache keys on the source hash and was never the problem;
    # only this object reuse is.  Compare mtimes rather than trusting existence.
    obj_is_current = (
        vc71_obj.exists()
        and source.exists()
        and vc71_obj.stat().st_mtime >= source.stat().st_mtime
    )
    if need_compile and cache is not None and obj_is_current and not args.no_cache:
        # We'll attempt cached compare first; compile only if there are misses.
        # The cached-compare path reads this object for disassembly on misses,
        # which is sound now that we know it matches the current source.
        need_compile = False  # tentative; compile_vc71 called below if obj absent
    elif (need_compile and cache is not None and vc71_obj.exists()
          and not args.no_cache and not args.quiet):
        print(f"[cache] {vc71_obj.name} is older than {source.name}; recompiling",
              flush=True)

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

    if not vc71_obj.exists():
        if not args.quiet:
            print(f"Compiling {source.name} with VC71 cl.exe...", flush=True)
        t0 = time.perf_counter()
        if not compile_vc71(source, vc71_obj, regcall_elide=args.regcall_elide, opt=args.opt):
            sys.exit(1)
        if not args.quiet:
            print(f"Compiled in {time.perf_counter() - t0:.1f}s", flush=True)

    if not args.quiet:
        print(f"Comparing against {ref_path.name}...\n", flush=True)
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

    rc = run_compare_cached(
        vc71_obj, ref_path, source, extra, cache, no_cache=args.no_cache,
        quiet=args.quiet, opt=args.opt,
        score_context=not args.no_score_context,
    )
    sys.exit(rc)


if __name__ == "__main__":
    main()
