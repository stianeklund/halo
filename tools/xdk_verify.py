#!/usr/bin/env python3
"""Compile a source file with the original XDK compiler and compare against delinked reference.

Finds the matching delinked reference via objdiff.json, compiles the source with
RXDK's CL.Exe (MSVC 13.10.3077 — the same compiler that built cachebeta.xbe),
and runs instruction-level comparison to flag FPU operand-order differences.

Usage:
    python3 tools/xdk_verify.py src/halo/effects/decals.c
    python3 tools/xdk_verify.py src/halo/effects/decals.c --function FUN_0009ac90
    python3 tools/xdk_verify.py src/halo/effects/decals.c --show-diffs
    python3 tools/xdk_verify.py --list  # show available units
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
OBJDIFF_JSON = REPO_ROOT / "objdiff.json"
BUILD_DIR = REPO_ROOT / "build"
XDK_OUT_DIR = BUILD_DIR / "xdk"
DELINKED_DIR = REPO_ROOT / "delinked"

XDK_CL = r"C:\Program Files (x86)\RXDK\xbox\bin\vc71\CL.Exe"
XDK_CL_WSL = "/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71/CL.Exe"
XDK_INC = r"C:\Program Files (x86)\RXDK\xbox\include"

COMPARE_SCRIPT = REPO_ROOT / "tools" / "compare_obj.py"


def wsl_to_win(path: Path) -> str:
    """Convert a WSL path to a Windows path."""
    s = str(path.resolve())
    if s.startswith("/mnt/"):
        # /mnt/g/dev/halo/... -> G:\dev\halo\...
        drive = s[5].upper()
        remainder = s[7:]  # skip "/mnt/X/"
        return f"{drive}:\\{remainder}".replace("/", "\\") if remainder else f"{drive}:\\"
    return s


def load_units() -> dict[str, dict]:
    """Load objdiff.json units, keyed by source_path."""
    with open(OBJDIFF_JSON) as f:
        data = json.load(f)
    units = {}
    for u in data.get("units", []):
        src = u.get("metadata", {}).get("source_path")
        if src:
            units[src] = u
    return units


def find_unit(source: str, units: dict) -> dict | None:
    """Find the objdiff unit matching a source file path."""
    source = str(source).replace("\\", "/")
    for key, unit in units.items():
        if source.endswith(key) or key.endswith(source):
            return unit
    return None


def compile_xdk(source: Path, output: Path) -> bool:
    """Compile a source file with the XDK cl.exe. Returns True on success."""
    output.parent.mkdir(parents=True, exist_ok=True)

    src_win = wsl_to_win(source)
    out_win = wsl_to_win(output)
    fi_win = wsl_to_win(REPO_ROOT / "src" / "xdk_common.h")
    gen_inc = wsl_to_win(BUILD_DIR / "generated")
    src_inc = wsl_to_win(REPO_ROOT / "src")

    cmd = [
        XDK_CL_WSL,
        "/nologo", "/c", "/TP",
        "/O2", "/Oy-", "/GF", "/Gy", "/Gd",
        "/W0", "/Zl", "/X",
        "/DMSVC", "/DXDK_BUILD",
        f"/FI{fi_win}",
        f"/I{gen_inc}",
        f"/I{src_inc}",
        f"/I{XDK_INC}",
        f"/Fo{out_win}",
        src_win,
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"XDK compilation failed:\n{result.stderr}", file=sys.stderr)
        return False

    if not output.exists():
        print(f"XDK compilation produced no output at {output}", file=sys.stderr)
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
    ap.add_argument("--skip-compile", action="store_true", help="Reuse existing XDK .obj")
    args = ap.parse_args()

    units = load_units()

    if args.list:
        for src, u in sorted(units.items()):
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

    unit = find_unit(str(source), units)
    if not unit:
        print(f"No objdiff.json unit found for {source}", file=sys.stderr)
        print("Run with --list to see available units")
        sys.exit(1)

    ref_path = REPO_ROOT / unit["base_path"]
    if not ref_path.exists():
        print(f"Delinked reference not found: {ref_path}", file=sys.stderr)
        print("Export it via: mcp__ghidra-live__export_delinked_object")
        sys.exit(1)

    obj_name = source.stem + ".obj"
    xdk_obj = XDK_OUT_DIR / obj_name

    if not args.skip_compile:
        print(f"Compiling {source.name} with XDK cl.exe...", flush=True)
        if not compile_xdk(source, xdk_obj):
            sys.exit(1)

    if not xdk_obj.exists():
        print(f"XDK object not found: {xdk_obj}", file=sys.stderr)
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

    rc = run_compare(xdk_obj, ref_path, extra)
    sys.exit(rc)


if __name__ == "__main__":
    main()
