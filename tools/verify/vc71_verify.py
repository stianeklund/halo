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

VC71_CL = r"C:\Program Files (x86)\RXDK\xbox\bin\vc71\CL.Exe"
VC71_CL_WSL = "/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71/CL.Exe"
RXDK_INC = r"C:\Program Files (x86)\RXDK\xbox\include"

COMPARE_SCRIPT = REPO_ROOT / "tools" / "verify" / "compare_obj.py"


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
        with open(REPO_ROOT / "kb.json") as f:
            kb = json.load(f)
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


def choose_unit(source: str, units: list[dict], function: str | None) -> dict | None:
    """Choose the best delinked reference for a source/function pair."""
    matches = find_units(source, units)
    existing = []
    for unit in matches:
        ref = REPO_ROOT / unit.get("base_path", "")
        if ref.exists():
            existing.append(unit)

    def full_object_first(unit: dict) -> tuple[bool, str]:
        base = Path(unit.get("base_path", "")).name
        return ("_FUN_" in base, base)

    existing.sort(key=full_object_first)

    if not function:
        return existing[0] if existing else None

    aliases = function_aliases(function)
    for unit in existing:
        ref_path = REPO_ROOT / unit["base_path"]
        if object_symbols(ref_path) & aliases:
            return unit

    return None


def compile_vc71(source: Path, output: Path) -> bool:
    """Compile a source file with VC++ 7.1 cl.exe. Returns True on success."""
    output.parent.mkdir(parents=True, exist_ok=True)

    src_win = wsl_to_win(source)
    out_win = wsl_to_win(output)
    fi_win = wsl_to_win(REPO_ROOT / "src" / "xdk_common.h")
    gen_inc = wsl_to_win(BUILD_DIR / "generated")
    src_inc = wsl_to_win(REPO_ROOT / "src")

    cmd = [
        VC71_CL_WSL,
        "/nologo", "/c", "/TC",
        "/O2", "/Oy-", "/GF", "/Gy", "/Gd",
        "/W0", "/Zl", "/X",
        "/DMSVC", "/DXDK_BUILD",
        f"/FI{fi_win}",
        f"/I{gen_inc}",
        f"/I{src_inc}",
        f"/I{RXDK_INC}",
        f"/Fo{out_win}",
        src_win,
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)
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
        with open(REPO_ROOT / "kb.json") as kf:
            kb = json.load(kf)
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


def run_compare_cached(
    compiled: Path,
    reference: Path,
    source: Path,
    extra_args: list[str],
    cache,
    no_cache: bool,
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
        else:
            i += 1

    compiled_funcs: dict[str, list[str]] = co.disassemble(str(compiled))
    reference_funcs: dict[str, list[str]] = co.disassemble(str(reference))

    matched: set[str] = set(compiled_funcs.keys()) & set(reference_funcs.keys())
    rename_map = _build_rename_map(set(compiled_funcs.keys()), matched)

    if fn_filter:
        fn = fn_filter.lstrip("_")
        if fn not in matched:
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

    if not matched:
        print("No matching functions found between objects")
        print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
        print(f"  reference: {sorted(reference_funcs.keys())[:10]}")
        return 1

    any_fail = False
    any_fpu_warn = False
    hits = 0
    misses = 0

    for fn in sorted(matched):
        cached_result = None
        if not no_cache and cache is not None:
            cached_result = cache.get(fn, source, reference)

        if cached_result is not None:
            hits += 1
            pct = cached_result["match_pct"]
            fpu_warnings = cached_result["fpu_warnings"]
            # diff_lines may be None if we didn't store diffs (e.g. not show_diffs)
            diffs = cached_result["diff_lines"] or []
            cache_tag = " [cache hit]"
        else:
            misses += 1
            pct, diffs, fpu_warnings = co.compare_functions(
                compiled_funcs[fn], reference_funcs[fn]
            )
            cache_tag = ""
            # Store result; always save diff_lines so future --show-diffs works
            if cache is not None and not no_cache:
                cache.put(fn, source, reference, pct, fpu_warnings, diffs)

        n_c = len(compiled_funcs[fn])
        n_r = len(reference_funcs[fn])
        status = "PASS" if pct >= threshold else "FAIL"
        fpu_tag = " [FPU-WARN]" if fpu_warnings else ""

        if not fpu_only:
            print(f"  {status} {fn}: {pct:.1f}% match ({n_c}/{n_r} insns){fpu_tag}{cache_tag}")

        if fpu_warnings:
            any_fpu_warn = True
            if not fpu_only:
                for w in fpu_warnings:
                    print(w)
            else:
                print(f"  {fn}:{fpu_tag}{cache_tag}")
                for w in fpu_warnings:
                    print(w)

        if status == "FAIL":
            any_fail = True

        if show_diffs and diffs and not fpu_only:
            for d in diffs[:60]:
                print(d)
            if len(diffs) > 60:
                print(f"  ... and {len(diffs) - 60} more diff lines")

    if hits or misses:
        total = hits + misses
        print(f"\n  Cache: {hits}/{total} hits ({100*hits//total if total else 0}%)")

    if any_fpu_warn:
        print("\nWARNING: FPU operand-order differences detected.")
        print("Check cross-product argument order and FSUB/FSUBR operand direction.")

    return 1 if any_fail else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", nargs="?", help="Source file to verify")
    ap.add_argument("--function", "-f", help="Compare only this function")
    ap.add_argument("--show-diffs", "-d", action="store_true")
    ap.add_argument("--fpu-only", action="store_true", help="Only show FPU warnings")
    ap.add_argument("--threshold", "-t", type=float, default=50.0)
    ap.add_argument("--list", action="store_true", help="List available units")
    ap.add_argument("--skip-compile", action="store_true", help="Reuse existing VC71 .obj")
    ap.add_argument("--no-cache", action="store_true",
                    help="Disable cache: always recompile and recompare")
    ap.add_argument("--rebuild-cache", action="store_true",
                    help="Drop existing cache entries for this source before run")
    args = ap.parse_args()

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
    if need_compile and cache is not None and vc71_obj.exists() and not args.no_cache:
        # We'll attempt cached compare first; compile only if there are misses.
        # The cached-compare path handles this transparently because it reads
        # the existing obj for disassembly on misses.
        need_compile = False  # tentative; compile_vc71 called below if obj absent

    if need_compile:
        print(f"Compiling {source.name} with VC71 cl.exe...", flush=True)
        t0 = time.perf_counter()
        if not compile_vc71(source, vc71_obj):
            sys.exit(1)
        elapsed = time.perf_counter() - t0
        print(f"Compiled in {elapsed:.1f}s", flush=True)
    elif not args.skip_compile and vc71_obj.exists():
        print(f"Using cached VC71 object: {vc71_obj.name}", flush=True)

    if not vc71_obj.exists():
        # Object missing and we skipped compile — need to compile now
        print(f"Compiling {source.name} with VC71 cl.exe...", flush=True)
        t0 = time.perf_counter()
        if not compile_vc71(source, vc71_obj):
            sys.exit(1)
        print(f"Compiled in {time.perf_counter() - t0:.1f}s", flush=True)

    print(f"Comparing against {ref_path.name}...\n", flush=True)
    extra = []
    if args.function:
        extra += ["--function", args.function]
    if args.show_diffs:
        extra += ["--show-diffs"]
    if args.fpu_only:
        extra += ["--fpu-only"]
    extra += ["--threshold", str(args.threshold)]

    rc = run_compare_cached(
        vc71_obj, ref_path, source, extra, cache, no_cache=args.no_cache
    )
    sys.exit(rc)


if __name__ == "__main__":
    main()
