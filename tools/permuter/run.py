#!/usr/bin/env python3
"""run.py — Adapter driver: run decomp-permuter against a VC71/MSVC target function.

Assembles the permuter input directory for a given function, then invokes
permuter.py with our compile.sh adapter and COFF reference object.

Usage:
    python3 tools/permuter/run.py --function FUN_0014b220 --source src/halo/physics/collision_features.c
    python3 tools/permuter/run.py --function FUN_0014b220 --source src/... --time 60 --threads 4
    python3 tools/permuter/run.py --function FUN_0014b220 --source src/... --work-dir /path/to/dir
    python3 tools/permuter/run.py --target FUN_0014b220 --attempts 100   # auto-resolves --source via kb.json

Steps performed:
    1. Extract and preprocess the target function into base.c (with pycparser-compat typedefs)
    2. Copy the reference COFF to target.o.  That reference is the one vc71_verify
       scores against: the pristine XBE bytes for the function, bounded by the
       committed tools/verify/function_bounds.json (tools/verify/xbe_reference.py).
       --delinked-ref opts into the Ghidra delinker's object instead.
    3. Write compile.sh symlink + settings.toml into a temp work dir
    4. Run permuter.py -j<threads> --best-only <workdir>

Key constraints handled:
    - VC71 CL.Exe is a Windows process; it cannot write to /tmp.
      TMPDIR is set to build/vc71/permuter_tmp (a Windows-accessible path).
    - pycparser runs with cpp -nostdinc so cannot see types.h.
      base.c includes explicit typedef declarations for uint32_t, bool, etc.
    - pycparser cannot parse bool as a type without a declaration.
      Any stub forward-declarations with 'bool' return type are rewritten to
      'unsigned char'.
"""

import argparse
import importlib.util
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
PERMUTER_DIR = REPO_ROOT / "third_party" / "decomp-permuter"
COMPILE_SH = REPO_ROOT / "tools" / "permuter" / "compile.sh"
COMPARE_OBJ = REPO_ROOT / "tools" / "verify" / "compare_obj.py"
XBE_REFERENCE = REPO_ROOT / "tools" / "verify" / "xbe_reference.py"
LIFT_PIPELINE = REPO_ROOT / "tools" / "lift_pipeline.py"
OBJDIFF_JSON = REPO_ROOT / "objdiff.json"
DELINKED_DIR = REPO_ROOT / "delinked"
BUILD_VC71 = REPO_ROOT / "build" / "vc71"
# When running from a worktree outside /mnt/ (e.g. /tmp/...), VC71 cl.exe cannot
# access those paths. Fall back to the main repo's build dir for staging. When
# REPO_ROOT is already on /mnt this is a no-op, preserving existing behavior.
_MAIN_REPO = Path("/mnt/g/dev/halo")
_FALLBACK_BUILD_VC71 = _MAIN_REPO / "build" / "vc71"
if not str(BUILD_VC71).startswith("/mnt/") and _FALLBACK_BUILD_VC71.parent.exists():
    BUILD_VC71 = _FALLBACK_BUILD_VC71
WIN_TMPDIR = BUILD_VC71 / "permuter_tmp"

# Symbols already defined by the force-included xdk_common.h.
# Any static const with one of these names in func_statics would cause C2370.
_XDK_COMMON_H = REPO_ROOT / "src" / "xdk_common.h"
_STATIC_CONST_RE = re.compile(r'^\s*static\s+const\s+\w[\w\s\*]*\s+(\w+)\s*=')
try:
    _XDK_COMMON_SYMBOLS: set = {
        m.group(1)
        for line in _XDK_COMMON_H.read_text(errors='replace').splitlines()
        if (m := _STATIC_CONST_RE.match(line))
    }
except OSError:
    _XDK_COMMON_SYMBOLS: set = set()


def _typedef_names_in_text(txt: str) -> set:
    """Collect typedef'd identifiers defined in a C text (mirrors strip_dup_typedefs)."""
    names = {m.group(1) for m in re.finditer(r"\}\s*(\w+)\s*;", txt)}
    names |= {m.group(1) for m in re.finditer(r"typedef\s+[^;{}]+?\b(\w+)\s*;", txt)}
    return names


# Typedef names supplied by the VC71 /FI environment (types.h via xdk_common.h).
# File-scope type statics whose name is NOT here are file-local types (e.g. a
# struct typedef'd inside the target .c) and must stay visible to VC71 — the
# #ifndef TYPES_H guard hides them when /FI defines TYPES_H (C2065 in base.c
# pre-compile, aborting the permuter before any iteration).
_FI_TYPEDEF_NAMES: set = set()
for _hdr in (REPO_ROOT / "src" / "types.h", _XDK_COMMON_H):
    try:
        _FI_TYPEDEF_NAMES |= _typedef_names_in_text(_hdr.read_text(errors="replace"))
    except OSError:
        pass

# Typedefs needed by pycparser (which runs with cpp -nostdinc, so sees no system headers)
PYCPARSER_TYPEDEFS = """\
/* Types needed by pycparser (cpp -nostdinc cannot see types.h) */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef unsigned char bool;
/* Bungie cseries primitive aliases (src/types.h). The cpp-expanded game
 * structs in type_statics use these (e.g. `real desired_angles_yaw;`) but
 * cpp emits them in include order, which can place a struct that USES `real`
 * ahead of types.h's `typedef float real;`. VC71 never trips on this because
 * types.h is force-included via /FI, but the bare `cpp -nostdinc` pycparser
 * path has no such predefinition and aborts with a syntax error at the first
 * use -> 0 iterations and a vacuous "no improvements found". Declaring them
 * up front here makes the forward use parse. */
typedef uint8_t boolean;
typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef float real;
typedef struct { real i; real j; } real_vector2d;
typedef struct { float x; float y; float z; } vector3_t;
typedef struct { float x; float y; float z; float w; } vector4_t;
typedef struct { short index; short salt; } datum_handle_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef unsigned char _BYTE;
typedef unsigned short _WORD;
typedef unsigned int _DWORD;
typedef struct data_s data_t; /* opaque: pointer-only use in extracted bodies */
typedef struct data_iter_s data_iter_t;
typedef unsigned short wchar_t;
"""


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

_co_module = None
_xr_module = None


def _load_compare_obj():
    """Load tools/verify/compare_obj.py once and cache the module."""
    global _co_module
    if _co_module is None:
        spec = importlib.util.spec_from_file_location("compare_obj", str(COMPARE_OBJ))
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        _co_module = mod
    return _co_module


_lp_module = None


def _load_lift_pipeline():
    """Load tools/lift_pipeline.py once and cache the module (mirrors _load_compare_obj)."""
    global _lp_module
    if _lp_module is None:
        spec = importlib.util.spec_from_file_location("lift_pipeline", str(LIFT_PIPELINE))
        mod = importlib.util.module_from_spec(spec)
        # lift_pipeline.py uses @dataclass, whose decorator resolves annotations
        # via sys.modules[cls.__module__] -- register before exec_module or it
        # crashes with "'NoneType' object has no attribute '__dict__'".
        sys.modules[spec.name] = mod
        spec.loader.exec_module(mod)
        _lp_module = mod
    return _lp_module


def _resolve_target_to_function_and_source(target: str) -> tuple[str, Path] | None:
    """Resolve --target (a function name or hex address) to (func_name, source_path).

    Reuses lift_pipeline.load_kb_index() -- the same kb.json object/function walk
    the lift pipeline itself uses to map a target to its TU -- instead of
    duplicating that logic here. Accepts a lifted name (e.g. "real_reciprocal"),
    a decl-literal FUN_ name (e.g. "FUN_0010da90"), or a bare hex address
    (e.g. "0x10da90", "10da90").
    """
    lp = _load_lift_pipeline()
    by_name, by_addr = lp.load_kb_index()

    hit = by_name.get(target.strip())
    if hit is None:
        addr_str = target.strip()
        if addr_str.upper().startswith("FUN_"):
            addr_str = addr_str[4:]
        addr_str = addr_str.lower().removeprefix("0x")
        try:
            hit = by_addr.get(f"0x{int(addr_str, 16):x}")
        except ValueError:
            hit = None
    if hit is None or not hit.source_path:
        return None
    source = REPO_ROOT / hit.source_path
    if not source.exists():
        return None
    return hit.name, source


def _load_xbe_reference():
    """Load tools/verify/xbe_reference.py once and cache the module."""
    global _xr_module
    if _xr_module is None:
        spec = importlib.util.spec_from_file_location(
            "xbe_reference", str(XBE_REFERENCE))
        mod = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = mod
        spec.loader.exec_module(mod)
        _xr_module = mod
    return _xr_module


def _target_address(func_name: str, source: Path | None = None) -> int | None:
    """Resolve a function name to its absolute VA in the pristine XBE."""
    fn = func_name
    alias = (fn if re.match(r"FUN_[0-9a-fA-F]{8}$", fn)
             else _resolve_ref_name(fn, source))
    if not alias:
        return None
    m = re.match(r"FUN_([0-9a-fA-F]{8})$", alias)
    if not m:
        return None
    return int(m.group(1), 16)


def find_synth_reference(source: Path, func_name: str) -> Path | None:
    """THE reference: the pristine XBE bytes for this function, in a COFF.

    Identical to what vc71_verify scores against (tools/verify/xbe_reference.py,
    bounded by the committed tools/verify/function_bounds.json), so the
    permuter's in-search objective is the same metric the pipeline reports.
    Resolving through objdiff.json/delinked/ instead would optimize a DIFFERENT
    reference than the one that decides whether the lift lands.
    """
    addr = _target_address(func_name, source)
    if addr is None:
        return None
    try:
        xr = _load_xbe_reference()
    except Exception as exc:                       # pragma: no cover
        print(f"[run.py] xbe_reference unavailable ({exc})", file=sys.stderr)
        return None
    try:
        return xr.reference_object(addr)
    except Exception as exc:                       # pragma: no cover
        print(f"[run.py] could not synthesize reference for 0x{addr:x}: {exc}",
              file=sys.stderr)
        return None


def _per_function_chunk(func_name: str) -> Path | None:
    """Return delinked/functions/<hex8>.obj for this function if it exists.

    Mirrors vc71_verify._per_function_ref: resolve the lifted name to its
    FUN_<addr> alias via kb.json, then look up the per-function chunk export.
    """
    fn = func_name
    alias = fn if re.match(r"FUN_[0-9a-fA-F]{8}$", fn) else _resolve_ref_name(fn)
    if not alias:
        return None
    m = re.match(r"FUN_([0-9a-fA-F]{8})$", alias)
    if not m:
        return None
    cand = DELINKED_DIR / "functions" / f"{m.group(1).lower()}.obj"
    return cand if cand.exists() else None


def _ref_has_function(ref_obj: Path, func_name: str, source: Path | None = None) -> bool:
    """True if the reference object contains the function (by lifted name or
    FUN_<addr> alias). A missing symbol means the TU reference is truncated
    for this target and a per-function chunk should be used instead."""
    try:
        funcs = _load_compare_obj().disassemble(str(ref_obj))
    except Exception:
        return False
    fn = func_name
    if fn in funcs:
        return True
    alias = _resolve_ref_name(fn, source)
    return bool(alias and alias in funcs)


def find_delinked_reference(source: Path, func_name: str | None = None) -> Path | None:
    """Locate the best delinked .obj reference for a source file via objdiff.json.

    OPT-IN FALLBACK (--delinked-ref, or no bounds entry for the target).  This is
    no longer what vc71_verify scores against -- see find_synth_reference -- so a
    run using it optimizes a different reference than the pipeline reports.

    Resolution order:
      1. per-function object registered in objdiff.json (name contains func_name)
      2. whole-TU object, if it actually contains the target function's symbol
      3. per-function chunk export (delinked/functions/<addr>.obj)
      4. whole-TU object even without the symbol (legacy last resort)
    Previously only 1+4 existed, so functions scoreable only via chunks failed
    every LCS lookup ('LCS lookup failed, skipping' on all candidates).
    """
    try:
        with open(OBJDIFF_JSON) as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"[run.py] Cannot load {OBJDIFF_JSON}: {e}", file=sys.stderr)
        data = {}

    src_str = str(source).replace("\\", "/")
    matches = []
    for unit in data.get("units", []):
        key = unit.get("metadata", {}).get("source_path", "")
        if key and (src_str.endswith(key) or key.endswith(src_str)):
            base = unit.get("base_path", "")
            candidate = REPO_ROOT / base
            if candidate.exists():
                matches.append((unit.get("name", ""), candidate))

    if func_name:
        # 1. per-function obj registered in objdiff.json.
        # Units are named either by the lifted name or by the FUN_<addr>
        # alias (e.g. "halo/actions_FUN_0001cfa0" for
        # actor_action_can_stop_conversing), so try both spellings. Matching
        # only the lifted name silently fell through to step 2/4 and handed
        # back matches[0] -- which, when the whole-TU object is absent, is a
        # DIFFERENT function's per-function unit. The permuter then scored
        # every candidate against the wrong function's reference.
        alias = _resolve_ref_name(func_name, source)
        for name, cand in matches:
            if func_name in name or (alias and alias in name):
                return cand
        # 2. whole-TU object that actually carries the symbol.
        # Only the unit whose name has no FUN_<addr> suffix is a whole-TU
        # object; a per-function unit for another function is not a valid
        # fallback and must not be returned as "the TU".
        tu = next((c for n, c in matches
                   if not re.search(r"FUN_[0-9a-fA-F]{8}", n)), None)
        if tu is not None and _ref_has_function(tu, func_name, source):
            return tu
        # 3. per-function chunk fallback
        chunk = _per_function_chunk(func_name)
        if chunk is not None:
            print(f"[run.py] TU reference {'missing' if tu is None else 'lacks symbol'};"
                  f" using per-function chunk {chunk.name}")
            return chunk
        # 4. legacy: hand back the TU object anyway (caller will fail lookup)
        return tu

    return matches[0][1] if matches else None


def copy_reference_coff(coff: Path, target: Path) -> bool:
    """Copy the delinked COFF i386 object into the permuter work dir."""
    try:
        shutil.copy2(coff, target)
    except OSError as e:
        print(f"[run.py] reference copy failed: {e}", file=sys.stderr)
        return False
    return True


def extract_function_body(source: Path, func_name: str) -> tuple[str, str] | None:
    """Preprocess source and extract a single function body plus file-scope statics.

    Returns (file_scope_statics, function_body) or None on failure.
    File-scope statics (e.g. static const arrays used by the function) are
    extracted separately so they can be placed before the function in base.c.
    """
    # Run cpp to expand macros and includes, producing plain C
    proc = subprocess.run(
        [
            "cpp", "-P",
            f"-I{REPO_ROOT / 'src'}",
            f"-I{REPO_ROOT / 'build' / 'generated'}",
            "-DMSVC", "-DXDK_BUILD",
            "-D_M_IX86=1", "-D_MSC_VER=1310",
            "-D__attribute__(x)=",
            "-D__declspec(x)=",
            str(source),
        ],
        capture_output=True, text=True,
    )
    if proc.returncode != 0:
        print(f"[run.py] cpp warning: {proc.stderr[:200]}", file=sys.stderr)

    src = proc.stdout

    # Find the function definition (not a forward declaration).
    # Try each match of the function name in order; skip any match where a
    # semicolon appears before the opening brace (that's a declaration, not a
    # definition).
    patterns = [
        re.compile(rf'^(?:void|int|char|float|short|unsigned|static)\s+{re.escape(func_name)}\s*\(',
                   re.MULTILINE),
        re.compile(rf'^[\w\s\*]+{re.escape(func_name)}\s*\(', re.MULTILINE),
    ]
    start = None
    brace_pos = None
    for pat in patterns:
        for m in pat.finditer(src):
            candidate = m.start()
            next_brace = src.find('{', candidate)
            next_semi = src.find(';', candidate)
            if next_brace == -1:
                continue
            # Skip forward declarations: ; before {
            if next_semi != -1 and next_semi < next_brace:
                continue
            start = candidate
            brace_pos = next_brace
            break
        if start is not None:
            break
    if start is None:
        print(f"[run.py] Function {func_name} not found in preprocessed source",
              file=sys.stderr)
        return None
    if brace_pos is None:
        print(f"[run.py] No opening brace found for {func_name}", file=sys.stderr)
        return None

    # Find matching closing brace
    level = 0
    i = brace_pos
    while i < len(src):
        if src[i] == '{':
            level += 1
        elif src[i] == '}':
            level -= 1
            if level == 0:
                break
        i += 1

    func_body = src[start:i + 1]

    # Extract file-scope declarations that the target function actually uses.
    # 1. Collect all identifiers referenced in the function body.
    # 2. Walk the preamble for typedef/struct/static/extern blocks.
    # 3. Keep only blocks whose defined name appears in the function's identifiers.
    preamble = src[:start]
    _C_KEYWORDS = {
        'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
        'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if',
        'int', 'long', 'register', 'return', 'short', 'signed', 'sizeof',
        'static', 'struct', 'switch', 'typedef', 'union', 'unsigned', 'void',
        'volatile', 'while', 'bool', 'true', 'false', 'NULL',
    }
    func_no_strings = re.sub(r'"[^"]*"', '""', func_body)
    func_ids = set(re.findall(r'\b[A-Za-z_]\w*\b', func_no_strings)) - _C_KEYWORDS

    all_blocks = []  # list of (block_text, defined_names)
    lines = preamble.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        if re.match(r'^(typedef|struct|union|enum|static|extern|__declspec)\b', line):
            block = [line]
            depth = line.count('{') - line.count('}')
            entered_body = depth > 0
            j = i + 1
            while j < len(lines):
                if entered_body and depth == 0:
                    break
                if not entered_body and block[-1].rstrip().endswith(';'):
                    break
                next_line = lines[j]
                block.append(next_line)
                depth += next_line.count('{') - next_line.count('}')
                if depth > 0:
                    entered_body = True
                j += 1
            block_text = '\n'.join(block)
            block_ids = set(re.findall(r'\b[A-Za-z_]\w*\b', block_text)) - _C_KEYWORDS
            all_blocks.append((block_text, block_ids))
            i = j
        else:
            i += 1

    # Keep blocks that define names used by the function.  Then iteratively
    # resolve transitive dependencies (a kept typedef may reference another).
    kept = []
    resolved = set(func_ids)
    changed = True
    while changed:
        changed = False
        remaining = []
        for block_text, block_ids in all_blocks:
            if block_ids & resolved:
                kept.append(block_text)
                resolved |= block_ids
                changed = True
            else:
                remaining.append((block_text, block_ids))
        all_blocks = remaining

    return "\n\n".join(kept), func_body


def _generate_implicit_decls(func_body: str, file_statics: str) -> str:
    """Generate implicit int f() declarations for called-but-undeclared identifiers.

    pycparser needs every called identifier to have a visible declaration.
    VC71 gets declarations via /FI, but the base.c extracted from cpp may lack
    them when the source file has no #include directives.
    """
    _C_KEYWORDS = {
        'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
        'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if',
        'int', 'long', 'register', 'return', 'short', 'signed', 'sizeof',
        'static', 'struct', 'switch', 'typedef', 'union', 'unsigned', 'void',
        'volatile', 'while', 'bool', 'true', 'false', 'NULL',
    }
    call_pattern = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
    called = set(call_pattern.findall(func_body)) - _C_KEYWORDS
    combined = file_statics + "\n" + func_body
    sig_pattern = re.compile(r'\b(?:void|int|char|float|short|unsigned|long|bool|boolean|byte|word|dword|real)\s+\**\s*([A-Za-z_]\w*)\s*\(')
    declared = set(sig_pattern.findall(combined))
    undeclared = called - declared
    if not undeclared:
        return ""
    lines = []
    for name in sorted(undeclared):
        lines.append(f"int {name}();")
    return "\n".join(lines)


def build_base_c(func_name: str, func_body: str, file_statics: str = "") -> str:
    """Construct a minimal base.c suitable for pycparser + VC71 compilation."""
    statics = re.sub(r'__declspec\s*\([^)]*\)\s*', '', file_statics)
    # pycparser cannot parse MSVC __cdecl calling convention specifier. Strip it
    # so extern declarations like `extern void *__cdecl memcpy(...);` are
    # converted to `extern void *memcpy(...);` for pycparser compatibility.
    statics = re.sub(r'\b__cdecl\s+', '', statics)
    # pycparser cannot parse MSVC __asm { ... } blocks or GCC __asm__ blocks in inline helpers.
    statics = re.sub(r'\b__asm\s*\{[^}]*\}', '{ /* asm */ }', statics)
    statics = re.sub(r'\b__asm__\s*__volatile__\s*\([^;]*\);', '/* asm */;', statics)

    # Split statics into two buckets:
    #   type_statics  — typedef/struct/union/enum blocks that conflict with the
    #                   force-included xdk_common.h/types.h when TYPES_H is set.
    #   func_statics  — static function/variable definitions that must always be
    #                   visible (e.g. noinline helpers called by the target).
    _TYPE_PREFIXES = re.compile(
        r'^(typedef|struct\s+\w+\s*\{|union\s+\w+\s*\{|enum\s+\w+\s*\{)', re.MULTILINE
    )
    type_statics_lines = []
    func_statics_lines = []
    current_block: list[str] = []
    is_type_block = False
    brace_depth = 0

    for line in statics.splitlines(keepends=True):
        stripped = line.lstrip()
        if not current_block:
            if not stripped.strip():
                # Blank line between blocks — don't start a new block
                continue
            # Start of a new top-level block
            is_type_block = bool(_TYPE_PREFIXES.match(stripped))
        current_block.append(line)
        brace_depth += line.count('{') - line.count('}')
        # A block ends at a semicolon on a zero-brace line (or a trailing semi)
        if brace_depth <= 0 and stripped.rstrip().endswith(';'):
            if is_type_block:
                # Guard only typedefs that collide with the /FI environment
                # (types.h/xdk_common.h) or PYCPARSER_TYPEDEFS. File-local
                # types must stay UNguarded so VC71 (which defines TYPES_H
                # via /FI) still sees them.
                block_text = "".join(current_block)
                mname = re.search(r"(\w+)\s*;\s*$", block_text.rstrip())
                name = mname.group(1) if mname else None
                guard_names = _FI_TYPEDEF_NAMES | _typedef_names_in_text(PYCPARSER_TYPEDEFS)
                if name and name not in guard_names and '__int64' not in block_text:
                    func_statics_lines.extend(current_block)
                else:
                    type_statics_lines.extend(current_block)
            else:
                func_statics_lines.extend(current_block)
            current_block = []
            brace_depth = 0

    # Flush any unterminated block into func_statics (safety net)
    func_statics_lines.extend(current_block)

    # Drop single-line static const declarations whose symbol is already
    # defined by the force-included xdk_common.h (would cause C2370).
    if _XDK_COMMON_SYMBOLS:
        filtered = []
        for line in func_statics_lines:
            m = _STATIC_CONST_RE.match(line)
            if m and m.group(1) in _XDK_COMMON_SYMBOLS:
                continue
            filtered.append(line)
        func_statics_lines = filtered

    type_statics = "".join(type_statics_lines)
    func_statics = "".join(func_statics_lines)

    # pycparser (C99) cannot parse the MSVC `__int64` keyword. Headers expanded
    # by cpp can leak `typedef __int64 int64_t;` (and the unsigned variant) into
    # type_statics, which sits inside the #ifndef TYPES_H guard alongside
    # PYCPARSER_TYPEDEFS — so pycparser sees both the friendly `long long`
    # typedef and the `__int64` one and aborts with a syntax error (0 iterations,
    # vacuous "no improvements"). These are redundant with PYCPARSER_TYPEDEFS for
    # the pycparser path, and VC71 never sees type_statics (TYPES_H is defined),
    # so drop the offending typedef statements from the guarded block.
    type_statics = re.sub(
        r'(?m)^[ \t]*typedef\b[^;\n]*\b__int64\b[^;\n]*;[ \t]*\n?', '', type_statics
    )

    # PYCPARSER_TYPEDEFS and conflicting type statics are guarded so they are
    # only active in bare pycparser runs (where TYPES_H is not yet defined).
    # Static function definitions always appear outside the guard.
    return f"""\
/* permuter base.c for {func_name} — auto-generated by tools/permuter/run.py */

#ifndef TYPES_H
{PYCPARSER_TYPEDEFS}
{type_statics}
#endif

{func_statics}

{func_body}
"""


def _fix_struct_scope_issue(base_c_path: Path) -> None:
    """Fix base.c compilation issues from typedef/struct scoping.

    The generated base.c has two main issues:
    1. TIFF-specific function pointer typedefs (tiff_*_method_t) are inside
       #ifndef TYPES_H but needed by struct definitions outside the guard
    2. Some structs reference other structs defined later

    This function:
    - Moves TIFF-specific function pointer typedefs outside the guard
    - Keeps generic typedefs (data_t, etc) inside the guard to avoid conflicts with types.h
    - Adds forward declarations for referenced structs
    """
    import re
    content = base_c_path.read_text()
    lines = content.split('\n')

    ifndef_idx = None
    endif_idx = None

    # Find #ifndef TYPES_H and #endif
    for i, line in enumerate(lines):
        if '#ifndef TYPES_H' in line:
            ifndef_idx = i
        elif '#endif' in line and ifndef_idx is not None:
            endif_idx = i
            break

    if ifndef_idx is None or endif_idx is None:
        return

    # Move only TIFF-specific function pointer typedefs outside the guard
    tiff_typedefs_to_move = []
    output_lines = []
    i = 0

    while i < len(lines):
        line = lines[i]

        if i == endif_idx:
            output_lines.append(line)
            # After #endif, insert TIFF-specific typedefs before remaining content
            output_lines.extend(tiff_typedefs_to_move)
            i += 1
        elif i < endif_idx and line.strip().startswith('typedef') and 'tiff_' in line and ('*' in line or '_method_t' in line):
            # This is a TIFF-specific typedef we want to move out
            if line.strip().endswith(';'):
                # Single-line typedef (function pointers)
                tiff_typedefs_to_move.append(line)
                i += 1
            else:
                # Multi-line typedef
                typedef_lines = [line]
                i += 1
                while i < endif_idx and ';' not in lines[i]:
                    typedef_lines.append(lines[i])
                    i += 1
                if i < endif_idx:
                    typedef_lines.append(lines[i])
                    i += 1
                tiff_typedefs_to_move.extend(typedef_lines)
        else:
            output_lines.append(line)
            i += 1

    # Append remaining lines after the endif-inserted typedefs
    if i < len(lines):
        output_lines.extend(lines[i:])

    # Now add forward declarations for structs that are referenced before defined
    # Look for usage patterns like "tiff_bitstate_t *" before the struct definition
    forward_decls_needed = set()
    for line in output_lines:
        if 'tiff_bitstate_t *' in line:
            forward_decls_needed.add('typedef struct tiff_bitstate_s tiff_bitstate_t;')

    # Insert forward declarations after typedef definitions but before struct definitions
    if forward_decls_needed:
        # Find the right insertion point (after function pointer typedefs, before struct definitions)
        insert_idx = -1
        for i, line in enumerate(output_lines):
            if 'typedef struct tiff' in line and '{' in line:
                insert_idx = i
                break
        if insert_idx > 0:
            for decl in forward_decls_needed:
                output_lines.insert(insert_idx, decl)
                insert_idx += 1

    base_c_path.write_text('\n'.join(output_lines))


def compile_base(work_dir: Path) -> bool:
    """Pre-compile base.c to verify the setup before permuter starts."""
    base_c = work_dir / "base.c"
    _fix_struct_scope_issue(base_c)
    base_o = work_dir / "base.o"
    result = subprocess.run(
        [str(COMPILE_SH), str(base_c), "-o", str(base_o)],
        capture_output=True,
    )
    if result.returncode != 0 or not base_o.exists():
        print("[run.py] Pre-compile of base.c FAILED.", file=sys.stderr)
        # Show VC71 errors
        result2 = subprocess.run(
            [str(COMPILE_SH), str(base_c), "-o", str(base_o)],
            env={**os.environ, "TMPDIR": str(WIN_TMPDIR)},
        )
        return False
    print(f"[run.py] Pre-compile OK: {base_o} ({base_o.stat().st_size} bytes)")
    return True


def _resolve_ref_name(func_name: str, source: Path | None = None) -> str | None:
    """Map a lifted function name to its FUN_<addr> delinked symbol via kb.json or source comments."""
    kb_path = REPO_ROOT / "kb.json"
    if kb_path.exists():
        try:
            kb = json.loads(kb_path.read_text())
            if isinstance(kb, dict):
                if "objects" in kb:
                    name_re = re.compile(r"\b" + re.escape(func_name) + r"\s*\(")
                    for obj in kb.get("objects", []):
                        for fn in obj.get("functions", []):
                            decl = fn.get("decl", "")
                            addr = fn.get("addr", "")
                            if addr and name_re.search(decl):
                                raw = int(addr, 16)
                                return f"FUN_{raw:08x}"
                else:
                    for addr_str, entry in kb.items():
                        if isinstance(entry, dict):
                            declared = entry.get("name")
                            decl = entry.get("decl", "")
                            if declared == func_name or (decl and re.search(r"\b" + re.escape(func_name) + r"\s*\(", decl)):
                                if addr_str.startswith("0x"):
                                    return f"FUN_{int(addr_str, 16):08x}"
        except Exception:
            pass

    if source and source.exists():
        try:
            txt = source.read_text()
            for m in re.finditer(r"/\*\s*(\w+)[^*]*?Address:\s*0x([0-9a-fA-F]+)", txt):
                name, addr_hex = m.group(1), m.group(2)
                if name == func_name:
                    return f"FUN_{int(addr_hex, 16):08x}"
        except Exception:
            pass

    return None


def _resolve_ref_insns(co, func_name: str, ref_obj: Path, ref_kind: str,
                       source: Path | None = None):
    """Resolve the reference instruction list for func_name from ref_obj.

    `ref_kind` is "synth" (XBE-derived, one symbol), "chunk" (per-function
    delinked export) or "tu" (whole-object delinked export).
    """
    fn = func_name
    delinked_name = _resolve_ref_name(fn, source)

    ref_insns = None
    if ref_kind == "tu":
        ref_funcs = co.disassemble(str(ref_obj))
        if fn in ref_funcs:
            ref_insns = ref_funcs[fn]
        elif delinked_name and delinked_name in ref_funcs:
            ref_insns = ref_funcs[delinked_name]
    if ref_insns is None:
        # synth/chunk reference (or TU lookup failed): boundary-capped
        # first-function read, exactly as vc71_verify.derive_reference does.
        aliases = {fn, f"_{fn}"}
        if delinked_name:
            aliases.add(delinked_name)
        try:
            ref_insns = co.first_function_insns(str(ref_obj), aliases)
        except Exception:
            ref_insns = None
    if ref_insns and ref_kind == "synth":
        # Raw XBE bytes keep the real cross-reference addresses; a VC71
        # candidate stores those as zeroed fields plus a relocation.  Undo that
        # one asymmetry -- the same rewrite vc71_verify applies -- or every
        # call/global reference scores as a mismatch and the permuter optimizes
        # a baseline the pipeline never reports.
        try:
            xr = _load_xbe_reference()
            ref_insns = [xr.normalize_synth_insn(i) for i in ref_insns]
        except Exception as exc:                   # pragma: no cover
            print(f"[run.py] WARNING: synth normalization unavailable ({exc}); "
                  "baseline will not agree with vc71_verify", file=sys.stderr)
    return ref_insns


def _audit_candidate(base_c: Path, cand_c: Path, func_name: str) -> str:
    """Semantic audit of one candidate.  Returns REJECT / OK / UNKNOWN.

    The permuter's objective function knows nothing about meaning, so a
    behaviour-changing candidate can and does outscore the faithful base.  This
    calls tools/permuter/audit_candidate.py, which detects the two classes that
    were actually measured on units.c (2026-08-13):

      LOST_DEF    -- a local loses an assignment but keeps its later reads, so
                     those reads observe a stale value
      UNDEF_PATH  -- every assignment to a local sits inside a branch that
                     leaves the function, yet it is read outside that branch

    UNKNOWN means the audit could not parse the candidate; treat it exactly
    like REJECT until a human has read the diff.
    """
    script = Path(__file__).resolve().parent / "audit_candidate.py"
    if not script.exists() or not base_c.exists() or not cand_c.exists():
        return "UNKNOWN"
    try:
        r = subprocess.run(
            [sys.executable, str(script), "--base", str(base_c),
             "--cand", str(cand_c), "--function", func_name, "--quiet"],
            capture_output=True, text=True, timeout=60,
        )
    except (OSError, subprocess.SubprocessError):
        return "UNKNOWN"
    if r.returncode == 1:
        return "REJECT"
    if r.returncode == 0:
        return "OK"
    return "UNKNOWN"


def get_lcs_score(func_name: str, compiled_obj: Path, ref_obj: Path,
                  ref_kind: str = "synth", source: Path | None = None) -> float | None:
    """Get LCS match % for a function between compiled and reference objects."""
    co = _load_compare_obj()

    cand_funcs = co.disassemble(str(compiled_obj))

    fn = func_name.lstrip("_@")
    cand_fn = fn if fn in cand_funcs else None
    if not cand_fn:
        return None

    ref_insns = _resolve_ref_insns(co, func_name, ref_obj, ref_kind, source)
    if ref_insns:
        pct, *_ = co.compare_functions(cand_funcs[cand_fn], ref_insns)
        return pct
    return None


def write_ref_mnemonics_file(func_name: str, ref_obj: Path, ref_kind: str,
                             dest_path: Path, source: Path | None = None) -> bool:
    """Precompute the reference mnemonic sequence once and write it to dest_path."""
    co = _load_compare_obj()
    ref_insns = _resolve_ref_insns(co, func_name, ref_obj, ref_kind, source)
    if not ref_insns:
        return False
    mnemonics = co.extract_mnemonic_sequence(ref_insns)
    dest_path.write_text("\n".join(mnemonics) + "\n")
    return True


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

_quiet = False  # set after arg parse; used by _log


def _log(*a, **kw):
    if not _quiet:
        print(*a, **kw)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--target",
                    help="Function name or hex address (e.g. FUN_0010da90 or 0x10da90). "
                         "Auto-resolves --source via kb.json. Mutually exclusive with "
                         "--function/--source.")
    ap.add_argument("--function", "-f",
                    help="Function name to permute (e.g. FUN_0014b220). Requires --source.")
    ap.add_argument("--source", "-s",
                    help="Source .c file containing the function. Requires --function.")
    ap.add_argument("--time", "-t", type=int, default=None,
                    help="Permuter runtime in seconds (default: 60, or derived from "
                         "--attempts if given)")
    ap.add_argument("--attempts", type=int, default=None,
                    help="Approximate bound on candidate iterations. Only takes effect "
                         "when --time is not given: time = max(30, ceil(attempts * 0.5)) "
                         "seconds (warm VC71 compile+score is ~0.15s/candidate). "
                         "Approximate; bounds wall-clock, not an exact iteration count -- "
                         "Guard 1 reports the actual iteration count achieved.")
    ap.add_argument("--threads", "-j", type=int, default=1,
                    help="Number of parallel permuter threads (default: 1)")
    ap.add_argument("--work-dir", default=None,
                    help="Use an existing work dir (skips setup)")
    ap.add_argument("--keep", action="store_true",
                    help="Keep the work directory after the run")
    ap.add_argument("--output-dir", default=None,
                    help="Save work dir to this path after run")
    ap.add_argument("--quiet", "-q", action="store_true",
                    help="Suppress diagnostic noise; only print final summary and errors")
    ap.add_argument("--delinked-ref", action="store_true",
                    help="Score against the Ghidra-delinked object instead of the "
                         "XBE-derived reference vc71_verify uses. Opt-in only: the "
                         "delinked object carries real relocations, but it is a "
                         "DIFFERENT reference than the pipeline reports, so the "
                         "in-search baseline need not agree with the VC71 score.")
    args = ap.parse_args()

    global _quiet
    _quiet = args.quiet

    if args.target:
        if args.function or args.source:
            ap.error("--target is mutually exclusive with --function/--source")
        resolved = _resolve_target_to_function_and_source(args.target)
        if resolved is None:
            print(f"[run.py] Could not resolve --target {args.target!r} to a "
                  "kb.json function with a valid source_path.", file=sys.stderr)
            sys.exit(1)
        args.function, resolved_source = resolved
        args.source = str(resolved_source)
        _log(f"[run.py] Resolved --target : {args.target} -> function={args.function} "
             f"source={resolved_source.relative_to(REPO_ROOT)}")
    elif not args.function or not args.source:
        ap.error("either --target, or both --function and --source, are required")

    if args.time is None:
        if args.attempts is not None:
            args.time = max(30, math.ceil(args.attempts * 0.5))
            _log(f"[run.py] --attempts {args.attempts} mapped to --time {args.time}s (approximate)")
        else:
            args.time = 60

    source = Path(args.source)
    if not source.is_absolute():
        source = REPO_ROOT / source
    if not source.exists():
        print(f"[run.py] Source not found: {source}", file=sys.stderr)
        sys.exit(1)

    func_name = args.function
    _log(f"[run.py] Target function : {func_name}")
    _log(f"[run.py] Source file     : {source}")

    # ------------------------------------------------------------------
    # VC71 requires temp files on Windows-accessible drive paths.
    # Set TMPDIR before any tempfile usage.
    # ------------------------------------------------------------------
    WIN_TMPDIR.mkdir(parents=True, exist_ok=True)
    os.environ["TMPDIR"] = str(WIN_TMPDIR)
    tempfile.tempdir = str(WIN_TMPDIR)

    # ------------------------------------------------------------------
    # Locate the reference.  PRIMARY is the XBE-derived object vc71_verify
    # scores against; --delinked-ref opts into the Ghidra delinker instead
    # (real relocations, but a different reference than the pipeline reports,
    # so its baseline need not agree with the pipeline's score).
    # ------------------------------------------------------------------
    ref_coff = None
    ref_kind = "synth"
    if not args.delinked_ref:
        ref_coff = find_synth_reference(source, func_name)
        if not ref_coff:
            print("[run.py] WARNING: no XBE-derived reference for this function "
                  "(missing from tools/verify/function_bounds.json?); falling "
                  "back to the delinked reference — its baseline will NOT match "
                  "vc71_verify's score.", file=sys.stderr)
    if not ref_coff:
        ref_coff = find_delinked_reference(source, func_name)
        ref_kind = "chunk" if (ref_coff and ref_coff.parent.name == "functions") else "tu"
    if not ref_coff:
        print("[run.py] ERROR: No reference found. Regenerate the bounds table "
              "(tools/verify/function_bounds.py) or export a delinked reference.",
              file=sys.stderr)
        sys.exit(1)
    _log(f"[run.py] Reference COFF  : {ref_coff} ({ref_kind})")

    # ------------------------------------------------------------------
    # Set up work directory (must be on Windows-accessible path)
    # ------------------------------------------------------------------
    cleanup = False
    if args.work_dir:
        work_dir = Path(args.work_dir)
        work_dir.mkdir(parents=True, exist_ok=True)
    else:
        work_dir = Path(tempfile.mkdtemp(prefix="permuter_", dir=WIN_TMPDIR))
        cleanup = not args.keep
    _log(f"[run.py] Work dir        : {work_dir}")

    try:
        # Copy reference COFF into the work dir. The permuter scorer now uses
        # llvm-objdump on COFF directly, matching compare_obj.py's pipeline.
        target_o = work_dir / "target.o"
        if not copy_reference_coff(ref_coff, target_o):
            sys.exit(1)

        # Extract and preprocess the target function
        result = extract_function_body(source, func_name)
        if not result:
            print(f"[run.py] Could not extract {func_name} from {source}", file=sys.stderr)
            sys.exit(1)
        file_statics, func_body = result

        # Build minimal base.c (with pycparser-compatible typedefs)
        # Do NOT add extern declarations for game functions: they are already
        # declared in build/generated/decl.h which is included by xdk_common.h.
        # Adding extra externs causes C2371 "redefinition" errors.
        # DO include file-scope statics (e.g. lookup tables) used by the function.
        base_c_content = build_base_c(func_name, func_body, file_statics)

        base_c = work_dir / "base.c"
        base_c.write_text(base_c_content)
        _log(f"[run.py] base.c          : {len(base_c_content)} chars, {func_name}")

        # Write compile.sh symlink
        compile_sh_link = work_dir / "compile.sh"
        if compile_sh_link.exists() or compile_sh_link.is_symlink():
            compile_sh_link.unlink()
        compile_sh_link.symlink_to(COMPILE_SH)

        # [halo] Precompute the reference mnemonic sequence ONCE (using the
        # same chunk-aware extraction find_delinked_reference/get_lcs_score
        # already use) and hand it to the in-search scorer via a file, so the
        # permuter's own candidate ranking uses the repo's real acceptance
        # metric (mnemonic-LCS) instead of upstream's difflib penalty scorer.
        # If resolution fails for any reason, fall back to default upstream
        # scoring rather than crash the run.
        ref_mnemonics_path = work_dir / "ref_mnemonics.txt"
        has_lcs_ref = write_ref_mnemonics_file(func_name, target_o, ref_kind,
                                               ref_mnemonics_path, source)
        if not has_lcs_ref:
            _log("[run.py] WARNING: could not precompute reference mnemonics; "
                 "falling back to upstream difflib scoring for in-search ranking.")

        # Write settings.toml
        settings_f = work_dir / "settings.toml"
        settings_lines = [
            f'func_name = "{func_name}"\n',
            f'compiler_type = "msvc"\n',
            f'objdump_command = "llvm-objdump -d --no-show-raw-insn --no-leading-addr"\n',
        ]
        if has_lcs_ref:
            settings_lines.append('score_algorithm = "lcs"\n')
            settings_lines.append(f'ref_mnemonics_file = "{ref_mnemonics_path.name}"\n')
        settings_f.write_text("".join(settings_lines))

        # Pre-compile sanity check
        if not compile_base(work_dir):
            print("[run.py] Fix base.c compilation errors before running the permuter.",
                  file=sys.stderr)
            sys.exit(1)

        # Get initial score via vc71_verify
        base_o = work_dir / "base.o"
        init_pct = get_lcs_score(func_name, base_o, target_o,
                                 ref_kind=ref_kind, source=source)
        init_score = None
        if init_pct is not None:
            init_score = round((100.0 - init_pct) * 10)
            _log(f"[run.py] Initial LCS     : {init_pct:.1f}% (LCS loss={init_score})")
        else:
            _log("[run.py] Initial LCS     : (could not compute)")

        # ------------------------------------------------------------------
        # Run permuter
        # ------------------------------------------------------------------
        permuter_py = PERMUTER_DIR / "permuter.py"
        cmd = [sys.executable, str(permuter_py)]
        if args.threads > 1:
            cmd += [f"-j{args.threads}"]
        cmd += ["--best-only", str(work_dir)]

        _log(f"\n[run.py] Running permuter for {args.time}s, {args.threads} thread(s)...")
        _log(f"[run.py] Command: {' '.join(cmd)}\n")
        _log("-" * 60)

        # [halo] Always capture child stdout/stderr (never gate this on -q):
        # deliverable 3 needs to parse iteration counts and the printed base
        # score regardless of quiet mode, and the VACUOUS RUN diagnostic must
        # itself be visible even under -q. When not quiet, replay the captured
        # output to our own stdout/stderr afterwards so interactive runs still
        # see it (buffered rather than streamed live -- see final report).
        child_stdout = ""
        child_stderr = ""
        try:
            result = subprocess.run(cmd, timeout=args.time,
                                    env={**os.environ, "TMPDIR": str(WIN_TMPDIR)},
                                    capture_output=True, text=True)
            child_stdout = result.stdout or ""
            child_stderr = result.stderr or ""
        except subprocess.TimeoutExpired as exc:
            _log(f"\n[run.py] Permuter stopped after {args.time}s timeout.")
            child_stdout = exc.stdout or "" if isinstance(exc.stdout, str) else (exc.stdout or b"").decode("utf-8", "replace")
            child_stderr = exc.stderr or "" if isinstance(exc.stderr, str) else (exc.stderr or b"").decode("utf-8", "replace")
        except KeyboardInterrupt:
            _log("\n[run.py] Interrupted.")

        if not _quiet:
            if child_stdout:
                sys.stdout.write(child_stdout)
            if child_stderr:
                sys.stderr.write(child_stderr)

        child_combined = child_stdout + child_stderr

        # [halo] Guard 1: vacuous-run detection. Parse permuter.py's own
        # "iteration N, ... score = S" progress lines (main.py's post_score())
        # and bail loudly if zero real iterations ran -- otherwise "no
        # candidates" looks identical to "the search never started" (e.g.
        # pycparser choking on the extracted function, or the subprocess
        # crashing before the first iteration).
        iter_nums = [int(m) for m in re.findall(r"iteration (\d+),", child_combined)]
        max_iteration = max(iter_nums) if iter_nums else 0
        if max_iteration == 0:
            tail = "\n".join(child_combined.splitlines()[-20:])
            print("\n[run.py] VACUOUS RUN: 0 candidate iterations -- "
                  "do NOT trust \"no improvement\"", file=sys.stderr)
            print(f"[run.py] Command was: {' '.join(cmd)}", file=sys.stderr)
            print("[run.py] Last output from permuter subprocess:", file=sys.stderr)
            print(tail if tail else "(no output captured)", file=sys.stderr)
            sys.exit(3)
        _log(f"[run.py] Iterations ran  : {max_iteration}")

        # [halo] Guard 2: baseline agreement. When the in-search LCS scorer is
        # active, permuter.py prints "[name] base score = N" at startup
        # (main.py run_inner(), after Permuter construction). That N must
        # equal our own independently-computed init_score, or the in-search
        # scorer and get_lcs_score() are looking at different references --
        # the doctrine that "printed baseline LCS must equal vc71_verify's
        # independent score" (see docs/lift-learnings.md / permuter-campaign).
        if has_lcs_ref and init_score is not None:
            base_score_matches = re.findall(r"base score = (-?\d+)", child_combined)
            if base_score_matches:
                printed_base_score = int(base_score_matches[0])
                if printed_base_score != init_score:
                    print("\n[run.py] BASELINE MISMATCH: permuter's printed "
                          f"base score ({printed_base_score}) != run.py's own "
                          f"LCS-derived base score ({init_score}). The "
                          "in-search scorer and get_lcs_score() likely resolved "
                          "different references -- do not trust in-search "
                          "ranking for this run.", file=sys.stderr)
                    sys.exit(4)
                _log(f"[run.py] Baseline agree  : permuter base score "
                     f"{printed_base_score} == {init_score} (OK)")
            else:
                _log("[run.py] WARNING: could not find permuter's printed "
                     "base score line to cross-check against init_score.")

        # ------------------------------------------------------------------
        # LCS-gated candidate selection
        # ------------------------------------------------------------------
        # The permuter's penalty score can diverge from the repo's LCS
        # instruction-match metric.  We compile every output candidate,
        # compute the repo LCS for each, and select by:
        #   1. Highest LCS first (must exceed baseline)
        #   2. Lowest permuter penalty as tie-breaker
        #   3. Equal-LCS candidates labelled as manual-inspection only
        # ------------------------------------------------------------------
        outputs = sorted(work_dir.glob("output-*"))
        if not outputs:
            print("\n[run.py] No improvements found in this run.")
        else:
            _log(f"\n[run.py] Scoring {len(outputs)} candidate(s) by LCS...")

            candidates = []
            for out_dir in outputs:
                perm_penalty = int(out_dir.name.split("-")[1])
                src = out_dir / "source.c"
                if not src.exists():
                    continue
                obj_file = work_dir / f"candidate_{perm_penalty}.o"
                r = subprocess.run(
                    [str(COMPILE_SH), str(src), "-o", str(obj_file)],
                    env={**os.environ, "TMPDIR": str(WIN_TMPDIR)},
                    capture_output=True,
                )
                if r.returncode != 0 or not obj_file.exists():
                    _log(f"  penalty={perm_penalty}: compile failed, skipping")
                    continue
                lcs = get_lcs_score(func_name, obj_file, target_o,
                                    ref_kind=ref_kind, source=source)
                if lcs is None:
                    _log(f"  penalty={perm_penalty}: LCS lookup failed, skipping")
                    continue
                candidates.append((lcs, perm_penalty, out_dir, obj_file))
                is_best = init_pct is None or lcs > init_pct
                label = "NEW BEST" if is_best else ""
                # In quiet mode only log candidates that beat the baseline
                if not _quiet or is_best:
                    print(f"  penalty={perm_penalty:>6d}  LCS={lcs:5.1f}%  {label}")

            if not candidates:
                print("[run.py] No candidates compiled successfully.")
            else:
                candidates.sort(key=lambda c: (-c[0], c[1]))
                best_lcs, best_penalty, best_dir, best_obj = candidates[0]

                print(f"\n[run.py] Best permuter penalty: {best_penalty}")
                print(f"[run.py] Best LCS            : {best_lcs:.1f}%")
                _log(f"[run.py] Best output dir     : {best_dir}")

                if init_pct is not None:
                    delta = best_lcs - init_pct
                    print(f"[run.py] Baseline            : {init_pct:.1f}%")
                    if delta > 0:
                        print(f"[run.py] Result: IMPROVED by {delta:.1f}pp")
                    elif delta == 0:
                        print("[run.py] Result: EQUAL to baseline — manual inspection only")
                    else:
                        print(f"[run.py] Result: REGRESSED by {abs(delta):.1f}pp — do not apply")

                # Semantic audit of every candidate.  A higher LCS is NOT
                # evidence of correctness: measured 2026-08-13 on units.c, two
                # of three best-ranked candidates changed behaviour while
                # scoring higher, and BOTH passed the campaign's VC71 and
                # equivalence gates.  See tools/permuter/audit_candidate.py and
                # docs/lift-learnings.md section 44.
                audits = {}
                for _, penalty, d, _obj in candidates:
                    audits[d.name] = _audit_candidate(
                        base_c=work_dir / "base.c",
                        cand_c=d / "source.c",
                        func_name=func_name,
                    )

                rejected = [n for n, a in audits.items() if a == "REJECT"]
                if rejected:
                    print(f"[run.py] AUDIT: {len(rejected)} of {len(audits)} "
                          f"candidate(s) REJECTED for semantic change "
                          f"({', '.join(sorted(rejected))})")
                    print("[run.py] Run audit_candidate.py on the winner before "
                          "applying it.")

                # Write a summary file for downstream tooling
                summary = work_dir / "lcs_results.txt"
                with open(summary, "w") as sf:
                    sf.write(f"baseline_lcs={init_pct}\n")
                    for rank, (lcs, penalty, d, _) in enumerate(candidates, 1):
                        delta_str = f"{lcs - init_pct:+.1f}" if init_pct else "n/a"
                        verdict = "IMPROVED" if init_pct and lcs > init_pct else (
                            "EQUAL" if init_pct and lcs == init_pct else (
                            "REGRESSED" if init_pct and lcs < init_pct else "UNKNOWN"))
                        sf.write(f"rank={rank} lcs={lcs:.1f} penalty={penalty} "
                                 f"delta={delta_str} verdict={verdict} "
                                 f"audit={audits.get(d.name, 'UNKNOWN')} "
                                 f"dir={d.name}\n")
                _log(f"[run.py] Summary written to: {summary}")

        # ------------------------------------------------------------------
        # Save or clean up
        # ------------------------------------------------------------------
        if args.output_dir:
            out = Path(args.output_dir)
            if out.exists():
                shutil.rmtree(out)
            shutil.copytree(work_dir, out)
            print(f"[run.py] Work dir saved to: {out}")

    finally:
        if cleanup and work_dir.exists():
            shutil.rmtree(work_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
