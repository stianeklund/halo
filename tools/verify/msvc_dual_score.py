#!/usr/bin/env python3
"""Score one Halo translation unit with VC71 and VC6 SP5 + Processor Pack.

The two compiler lanes use the same source preprocessing and the same canonical
XBE-derived references as vc71_verify.py.  A final compiler-to-compiler score
then shows whether the generated instruction sequences differ independently of
the original-binary score.

Run from WSL, like vc71_verify.py:
    python3 tools/verify/msvc_dual_score.py src/halo/bitmaps/targa_file.c -f targa_export
"""

import argparse
import os
import sys
from contextlib import contextmanager
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parent.parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from verify import vc71_verify as verifier


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_VC6_CL = REPO_ROOT / "toolchains" / "vc6-sp5-processor-pack" / "bin" / "CL.EXE"
OUT_ROOT = REPO_ROOT / "build" / "compiler-score"


def _select_opt(source: Path, requested: str) -> str:
    """Mirror VC71's TU-level default optimization selection."""
    if requested != "/O2":
        return requested
    normalized = str(source).replace("\\", "/")
    if normalized.endswith("cseries/xbox_crt.c"):
        return "/O1"
    if normalized.endswith(("game/game_engine.c", "objects/objects.c", "units/units.c")):
        return "/O2 /Ob1"
    return requested


@contextmanager
def _compiler_context(cl_path: Path, out_dir: Path):
    """Reuse vc71_verify's preprocessing with a different cl.exe and output."""
    old_cl = verifier.VC71_CL
    old_cl_wsl = verifier.VC71_CL_WSL
    old_out_dir = verifier.VC71_OUT_DIR
    verifier.VC71_CL = str(cl_path)
    verifier.VC71_CL_WSL = str(cl_path)
    verifier.VC71_OUT_DIR = out_dir
    try:
        yield
    finally:
        verifier.VC71_CL = old_cl
        verifier.VC71_CL_WSL = old_cl_wsl
        verifier.VC71_OUT_DIR = old_out_dir


def _compile(label: str, cl_path: Path, source: Path, output: Path,
             opt: str, regcall_elide: bool) -> bool:
    print(f"Compiling {source.name} with {label}...", flush=True)
    with _compiler_context(cl_path, output.parent):
        return verifier.compile_vc71(source, output,
                                     regcall_elide=regcall_elide, opt=opt)


def _score_xbe(label: str, compiled: Path, source: Path, args, opt: str) -> int:
    print(f"\n=== {label} vs pristine XBE ===", flush=True)
    extra = ["--threshold", str(args.threshold)]
    if args.function:
        extra += ["--function", args.function]
    if args.show_diffs:
        extra += ["--show-diffs"]
    if args.reg_normalize:
        extra += ["--reg-normalize"]
    return verifier.run_compare_cached(
        compiled, source, extra, cache=None, no_cache=True, quiet=False,
        opt=opt, score_context=False, per_fn_opt=None,
        regcall_elide=args.regcall_elide,
    )


def _matched_functions(vc71_funcs: dict[str, list[str]],
                       vc6_funcs: dict[str, list[str]],
                       source: Path, wanted: str | None) -> list[str]:
    common = set(vc71_funcs) & set(vc6_funcs)
    if wanted is None:
        return sorted(common)

    aliases = verifier.function_aliases(wanted.lstrip("_"), source)
    matches = sorted(common & aliases)
    if matches:
        return matches
    raise ValueError(
        f"{wanted} was not found in both compiler outputs; "
        f"shared symbols include {sorted(common)[:10]}")


def _score_compiler_diff(vc71_obj: Path, vc6_obj: Path, source: Path, args) -> int:
    """Report VC6-as-candidate vs VC71 instruction similarity, never an oracle."""
    co = verifier.load_compare_obj()
    vc71_funcs = co.disassemble(str(vc71_obj))
    vc6_funcs = co.disassemble(str(vc6_obj))
    try:
        names = _matched_functions(vc71_funcs, vc6_funcs, source, args.function)
    except ValueError as exc:
        print(f"\nCompiler-to-compiler comparison skipped: {exc}", file=sys.stderr)
        return 1
    if not names:
        print("\nCompiler-to-compiler comparison skipped: no shared functions", file=sys.stderr)
        return 1

    print("\n=== VC6 SP5 + PP vs VC71 (not an XBE accuracy score) ===")
    for name in names:
        mnemonic_pct, diffs, *_ = co.compare_functions(
            vc6_funcs[name], vc71_funcs[name], reg_normalize=False)
        operand_pct = co.compare_functions(
            vc6_funcs[name], vc71_funcs[name], reg_normalize=True)[0]
        print(f"  {name}: {mnemonic_pct:.1f}% mnemonic "
              f"({len(vc6_funcs[name])}/{len(vc71_funcs[name])} insns) "
              f"| operand {operand_pct:.1f}%")
        if args.show_diffs and diffs:
            print("    VC6 (-) / VC71 (+):")
            for line in diffs[:args.max_diffs]:
                print(line)
            if len(diffs) > args.max_diffs:
                print(f"    ... {len(diffs) - args.max_diffs} more diff lines")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source", help="C source file relative to the repo or absolute")
    ap.add_argument("--function", "-f", help="Compare only this function")
    ap.add_argument("--threshold", "-t", type=float, default=50.0,
                    help="XBE score pass threshold (default: 50)")
    ap.add_argument("--show-diffs", "-d", action="store_true",
                    help="Show XBE and compiler-to-compiler instruction diffs")
    ap.add_argument("--max-diffs", type=int, default=80,
                    help="Maximum direct compiler diff lines per function (default: 80)")
    ap.add_argument("--reg-normalize", "-r", action="store_true",
                    help="Use register-normalized score in each XBE lane")
    ap.add_argument("--regcall-elide", action="store_true",
                    help="Apply vc71_verify's proven register-call preprocessing")
    ap.add_argument("--opt", default="/O2", help="MSVC optimization flags (default: /O2)")
    ap.add_argument("--vc6-cl", type=Path, default=DEFAULT_VC6_CL,
                    help="VC6 Processor Pack CL.EXE path")
    ap.add_argument("--skip-decl-regen", action="store_true",
                    help="Reuse build/generated/decl.h instead of regenerating it")
    args = ap.parse_args()

    if os.name == "nt":
        ap.error("run this from WSL: python3 tools/verify/msvc_dual_score.py ...")

    source = Path(args.source)
    if not source.is_absolute():
        source = REPO_ROOT / source
    source = source.resolve()
    vc6_cl = args.vc6_cl.resolve()
    if not source.is_file():
        ap.error(f"source file not found: {source}")
    if not vc6_cl.is_file():
        ap.error(f"VC6 compiler not found: {vc6_cl}")
    if args.max_diffs < 0:
        ap.error("--max-diffs must be non-negative")

    if not args.skip_decl_regen:
        verifier.regen_decl_header()

    opt = _select_opt(source, args.opt)
    stem = source.stem + ".obj"
    vc71_obj = OUT_ROOT / "vc71" / stem
    vc6_obj = OUT_ROOT / "vc6-sp5-processor-pack" / stem

    if not _compile("VC71", Path(verifier.VC71_CL_WSL), source, vc71_obj,
                    opt, args.regcall_elide):
        return 1
    if not _compile("VC6 SP5 + Processor Pack", vc6_cl, source, vc6_obj,
                    opt, args.regcall_elide):
        return 1

    vc71_rc = _score_xbe("VC71", vc71_obj, source, args, opt)
    vc6_rc = _score_xbe("VC6 SP5 + Processor Pack", vc6_obj, source, args, opt)
    diff_rc = _score_compiler_diff(vc71_obj, vc6_obj, source, args)
    return 1 if vc71_rc or vc6_rc or diff_rc else 0


if __name__ == "__main__":
    raise SystemExit(main())
