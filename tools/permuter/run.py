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
"""


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def find_delinked_reference(source: Path) -> Path | None:
    """Locate the delinked .obj reference for a source file via objdiff.json."""
    try:
        with open(OBJDIFF_JSON) as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"[run.py] Cannot load {OBJDIFF_JSON}: {e}", file=sys.stderr)
        return None

    src_str = str(source).replace("\\", "/")
    for unit in data.get("units", []):
        key = unit.get("metadata", {}).get("source_path", "")
        if key and (src_str.endswith(key) or key.endswith(src_str)):
            base = unit.get("base_path", "")
            candidate = REPO_ROOT / base
            if candidate.exists():
                return candidate
    return None


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
            "-D__attribute__(x)=",
            str(source),
        ],
        capture_output=True, text=True,
    )
    if proc.returncode != 0:
        print(f"[run.py] cpp warning: {proc.stderr[:200]}", file=sys.stderr)

    src = proc.stdout

    # Find the function
    m = re.search(rf'^(?:void|int|char|float|short|unsigned|static)\s+{re.escape(func_name)}\s*\(',
                  src, re.MULTILINE)
    if not m:
        m = re.search(rf'^[\w\s\*]+{re.escape(func_name)}\s*\(', src, re.MULTILINE)
    if not m:
        print(f"[run.py] Function {func_name} not found in preprocessed source",
              file=sys.stderr)
        return None

    start = m.start()
    brace_pos = src.find('{', start)
    if brace_pos == -1:
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

    # Extract file-scope static definitions that appear before the function.
    # Walk line by line, collecting top-level 'static ...' declarations
    # (which may span multiple lines due to array initializers).
    preamble = src[:start]
    static_defs = []
    lines = preamble.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        if re.match(r'^static\b', line):
            # Collect lines until we see the closing ';' at top level
            block = [line]
            depth = line.count('{') - line.count('}')
            j = i + 1
            while j < len(lines) and (depth > 0 or not block[-1].rstrip().endswith(';')):
                next_line = lines[j]
                block.append(next_line)
                depth += next_line.count('{') - next_line.count('}')
                j += 1
            static_defs.append('\n'.join(block))
            i = j
        else:
            i += 1

    return "\n\n".join(static_defs), func_body


def build_base_c(func_name: str, func_body: str, file_statics: str = "") -> str:
    """Construct a minimal base.c suitable for pycparser + VC71 compilation."""
    return f"""\
/* permuter base.c for {func_name} — auto-generated by tools/permuter/run.py */

{PYCPARSER_TYPEDEFS}

{file_statics}

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


def get_lcs_score(func_name: str, compiled_obj: Path, ref_obj: Path) -> float | None:
    """Get LCS match % for a function between compiled and reference objects."""
    spec = importlib.util.spec_from_file_location("compare_obj", str(COMPARE_OBJ))
    co = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(co)

    cand_funcs = co.disassemble(str(compiled_obj))
    ref_funcs = co.disassemble(str(ref_obj))

    fn = func_name.lstrip("_")
    if fn in cand_funcs and fn in ref_funcs:
        pct, _, _ = co.compare_functions(cand_funcs[fn], ref_funcs[fn])
        return pct
    return None


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

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
    args = ap.parse_args()

    source = Path(args.source)
    if not source.is_absolute():
        source = REPO_ROOT / source
    if not source.exists():
        print(f"[run.py] Source not found: {source}", file=sys.stderr)
        sys.exit(1)

    func_name = args.function.lstrip("_")
    print(f"[run.py] Target function : {func_name}")
    print(f"[run.py] Source file     : {source}")

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
    ref_coff = find_delinked_reference(source)
    if not ref_coff:
        print("[run.py] ERROR: No delinked reference found. "
              "Run batch_delink.py to export the reference object.", file=sys.stderr)
        sys.exit(1)
    print(f"[run.py] Reference COFF  : {ref_coff}")

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
    print(f"[run.py] Work dir        : {work_dir}")

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
        print(f"[run.py] base.c          : {len(base_c_content)} chars, {func_name}")

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
            print(f"[run.py] Initial LCS     : {init_pct:.1f}% (LCS loss={init_score})")
        else:
            print("[run.py] Initial LCS     : (could not compute)")

        # ------------------------------------------------------------------
        # Run permuter
        # ------------------------------------------------------------------
        permuter_py = PERMUTER_DIR / "permuter.py"
        cmd = [sys.executable, str(permuter_py)]
        if args.threads > 1:
            cmd += [f"-j{args.threads}"]
        cmd += ["--best-only", str(work_dir)]

        print(f"\n[run.py] Running permuter for {args.time}s, {args.threads} thread(s)...")
        print(f"[run.py] Command: {' '.join(cmd)}\n")
        print("-" * 60)

        try:
            result = subprocess.run(cmd, timeout=args.time,
                                    env={**os.environ, "TMPDIR": str(WIN_TMPDIR)})
        except subprocess.TimeoutExpired:
            print(f"\n[run.py] Permuter stopped after {args.time}s timeout.")
        except KeyboardInterrupt:
            print("\n[run.py] Interrupted.")

        # Check for output directories (improvements found)
        outputs = sorted(work_dir.glob("output-*"))
        if outputs:
            best_dir = min(outputs, key=lambda p: int(p.name.split("-")[1]))
            best_score = int(best_dir.name.split("-")[1])
            print(f"\n[run.py] Best permuter score: {best_score}")
            print(f"[run.py] Best output dir: {best_dir}")

            # Compile best candidate and report the repo's acceptance metric.
            best_src = best_dir / "source.c"
            best_obj = work_dir / "best_candidate.o"
            if best_src.exists():
                r = subprocess.run(
                    [str(COMPILE_SH), str(best_src), "-o", str(best_obj)],
                    env={**os.environ, "TMPDIR": str(WIN_TMPDIR)},
                    capture_output=True,
                )
                if r.returncode == 0 and best_obj.exists():
                    best_pct = get_lcs_score(func_name, best_obj, target_o)
                    if best_pct is not None:
                        print(f"[run.py] Best candidate LCS: {best_pct:.1f}%")
                        if init_pct is not None:
                            print(f"[run.py] Improvement: {init_pct:.1f}% → {best_pct:.1f}% "
                                  f"(+{best_pct - init_pct:.1f}pp)")
                            if best_pct < init_pct:
                                print("[run.py] WARNING: permuter penalty score improved but LCS regressed; "
                                      "do not apply this candidate.", file=sys.stderr)
        else:
            print("\n[run.py] No improvements found in this run.")

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
