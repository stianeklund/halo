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
"""

import argparse
import json
import os
import re
import subprocess
import sys
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
    """Run compare_obj.py and return its exit code."""
    cmd = [sys.executable, str(COMPARE_SCRIPT), str(compiled), str(reference)] + extra_args
    return subprocess.run(cmd).returncode


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

    if not args.skip_compile:
        print(f"Compiling {source.name} with VC71 cl.exe...", flush=True)
        if not compile_vc71(source, vc71_obj):
            sys.exit(1)

    if not vc71_obj.exists():
        print(f"VC71 object not found: {vc71_obj}", file=sys.stderr)
        sys.exit(1)

    print(f"Comparing against {ref_path.name}...\n", flush=True)
    extra = []
    if args.function:
        extra += ["--function", args.function]
    if args.show_diffs:
        extra += ["--show-diffs"]
    if args.fpu_only:
        extra += ["--fpu-only"]
    extra += ["--threshold", str(args.threshold)]

    rc = run_compare(vc71_obj, ref_path, extra)
    sys.exit(rc)


if __name__ == "__main__":
    main()
