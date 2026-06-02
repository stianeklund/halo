#!/usr/bin/env python3
"""run.py — Adapter driver: run decomp-permuter against a VC71/MSVC target function.

Assembles the permuter input directory for a given function, then invokes
permuter.py with our compile.sh adapter and COFF reference object.

Usage:
    python3 tools/permuter/run.py --function FUN_0014b220 --source src/halo/physics/collision_features.c
    python3 tools/permuter/run.py --function FUN_0014b220 --source src/... --time 60 --threads 4
    python3 tools/permuter/run.py --function FUN_0014b220 --source src/... --work-dir /path/to/dir

Steps performed:
    1. Extract and preprocess the target function into base.c (with pycparser-compat typedefs)
    2. Copy delinked reference COFF to target.o
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
OBJDIFF_JSON = REPO_ROOT / "objdiff.json"
DELINKED_DIR = REPO_ROOT / "delinked"
BUILD_VC71 = REPO_ROOT / "build" / "vc71"
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
typedef struct { float x; float y; float z; } vector3_t;
typedef struct { float x; float y; float z; float w; } vector4_t;
typedef struct { short index; short salt; } datum_handle_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef unsigned char _BYTE;
typedef unsigned short _WORD;
typedef unsigned int _DWORD;
"""


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def find_delinked_reference(source: Path, func_name: str | None = None) -> Path | None:
    """Locate the best delinked .obj reference for a source file via objdiff.json.

    Prefers a per-function delinked object (name contains func_name) over a
    whole-TU object, so the permuter scores against the tightest available ref.
    """
    try:
        with open(OBJDIFF_JSON) as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"[run.py] Cannot load {OBJDIFF_JSON}: {e}", file=sys.stderr)
        return None

    src_str = str(source).replace("\\", "/")
    matches = []
    for unit in data.get("units", []):
        key = unit.get("metadata", {}).get("source_path", "")
        if key and (src_str.endswith(key) or key.endswith(src_str)):
            base = unit.get("base_path", "")
            candidate = REPO_ROOT / base
            if candidate.exists():
                matches.append((unit.get("name", ""), candidate))

    if not matches:
        return None

    # Prefer the per-function obj if func_name is provided
    if func_name:
        for name, cand in matches:
            if func_name in name:
                return cand

    # Fall back to first existing match
    return matches[0][1]


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
    sig_pattern = re.compile(r'\b(?:void|int|char|float|short|unsigned|long)\s+\**\s*([A-Za-z_]\w*)\s*\(')
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
            target = type_statics_lines if is_type_block else func_statics_lines
            target.extend(current_block)
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



def compile_base(work_dir: Path) -> bool:
    """Pre-compile base.c to verify the setup before permuter starts."""
    base_c = work_dir / "base.c"
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


def _resolve_ref_name(func_name: str) -> str | None:
    """Map a lifted function name to its FUN_<addr> delinked symbol via kb.json."""
    kb_path = REPO_ROOT / "kb.json"
    if not kb_path.exists():
        return None
    try:
        kb = json.loads(kb_path.read_text())
        # Match the function name as a whole identifier immediately followed by
        # '(' so that e.g. "actor_look_secondary" does not spuriously match
        # "actor_look_secondary_stop" (a substring) and resolve to the wrong addr.
        name_re = re.compile(r"\b" + re.escape(func_name) + r"\s*\(")
        for obj in kb.get("objects", []):
            for fn in obj.get("functions", []):
                decl = fn.get("decl", "")
                addr = fn.get("addr", "")
                if addr and name_re.search(decl):
                    raw = int(addr, 16)
                    return f"FUN_{raw:08x}"
    except Exception:
        pass
    return None


def get_lcs_score(func_name: str, compiled_obj: Path, ref_obj: Path) -> float | None:
    """Get LCS match % for a function between compiled and reference objects."""
    spec = importlib.util.spec_from_file_location("compare_obj", str(COMPARE_OBJ))
    co = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(co)

    cand_funcs = co.disassemble(str(compiled_obj))
    ref_funcs = co.disassemble(str(ref_obj))

    fn = func_name.lstrip("_")
    cand_fn = fn if fn in cand_funcs else None
    ref_fn = fn if fn in ref_funcs else None
    if ref_fn is None:
        delinked_name = _resolve_ref_name(fn)
        if delinked_name and delinked_name in ref_funcs:
            ref_fn = delinked_name
    if cand_fn and ref_fn:
        pct, _, _ = co.compare_functions(cand_funcs[cand_fn], ref_funcs[ref_fn])
        return pct
    return None


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
    ap.add_argument("--function", "-f", required=True,
                    help="Function name to permute (e.g. FUN_0014b220)")
    ap.add_argument("--source", "-s", required=True,
                    help="Source .c file containing the function")
    ap.add_argument("--time", "-t", type=int, default=60,
                    help="Permuter runtime in seconds (default: 60)")
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
    args = ap.parse_args()

    global _quiet
    _quiet = args.quiet

    source = Path(args.source)
    if not source.is_absolute():
        source = REPO_ROOT / source
    if not source.exists():
        print(f"[run.py] Source not found: {source}", file=sys.stderr)
        sys.exit(1)

    func_name = args.function.lstrip("_")
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
    # Locate delinked reference
    # ------------------------------------------------------------------
    ref_coff = find_delinked_reference(source, func_name)
    if not ref_coff:
        print("[run.py] ERROR: No delinked reference found. "
              "Run batch_delink.py to export the reference object.", file=sys.stderr)
        sys.exit(1)
    _log(f"[run.py] Reference COFF  : {ref_coff}")

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

        # Write settings.toml
        settings_f = work_dir / "settings.toml"
        settings_f.write_text(
            f'func_name = "{func_name}"\n'
            f'compiler_type = "base"\n'
            f'objdump_command = "llvm-objdump -d --no-show-raw-insn --no-leading-addr"\n'
        )

        # Pre-compile sanity check
        if not compile_base(work_dir):
            print("[run.py] Fix base.c compilation errors before running the permuter.",
                  file=sys.stderr)
            sys.exit(1)

        # Get initial score via vc71_verify
        base_o = work_dir / "base.o"
        init_pct = get_lcs_score(func_name, base_o, target_o)
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

        try:
            result = subprocess.run(cmd, timeout=args.time,
                                    env={**os.environ, "TMPDIR": str(WIN_TMPDIR)},
                                    capture_output=_quiet)
        except subprocess.TimeoutExpired:
            _log(f"\n[run.py] Permuter stopped after {args.time}s timeout.")
        except KeyboardInterrupt:
            _log("\n[run.py] Interrupted.")

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
                lcs = get_lcs_score(func_name, obj_file, target_o)
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
                                 f"delta={delta_str} verdict={verdict} dir={d.name}\n")
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
