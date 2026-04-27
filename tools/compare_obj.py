#!/usr/bin/env python3
"""Compare compiled object against delinked reference at instruction level.

Uses basic-block-aware matching: extracts instruction mnemonics per basic block
(split at labels/branches), computes LCS-based similarity, and flags FPU
operand-order differences that indicate potential cross-product or subtraction
argument swaps.

Usage:
    python3 tools/compare_obj.py <compiled.obj> <reference.obj> [--function FUN_XXXXXXXX]
    python3 tools/compare_obj.py <compiled.obj> <reference.obj> --show-diffs
"""

import argparse
import re
import subprocess
import sys
from collections import OrderedDict
from difflib import SequenceMatcher


def disassemble(obj_path: str) -> dict[str, list[str]]:
    """Disassemble object, return {symbol: [instruction_lines]} for FUN_ symbols."""
    result = subprocess.run(
        ["llvm-objdump", "-d", "--no-show-raw-insn", "--no-leading-addr", obj_path],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"llvm-objdump failed on {obj_path}: {result.stderr}", file=sys.stderr)
        sys.exit(1)

    functions: dict[str, list[str]] = OrderedDict()
    current_func = None
    current_lines: list[str] = []

    for raw_line in result.stdout.splitlines():
        line = raw_line.rstrip()
        m = re.match(r'^(?:[0-9a-f]+ )?<([^>]+)>:', line)
        if m:
            sym = m.group(1)
            if sym.startswith("LAB_"):
                if current_func:
                    current_lines.append("LABEL")
                continue
            if current_func and current_lines:
                functions[current_func] = current_lines
            current_func = sym.lstrip("_")
            current_lines = []
            continue

        stripped = line.strip()
        if not stripped or stripped.startswith("..."):
            continue
        # Extract just the mnemonic + operands (skip leading address)
        parts = stripped.split(None, 1)
        if len(parts) >= 1:
            insn = stripped
            # Remove leading hex address if present
            if re.match(r'^[0-9a-f]+:', parts[0]):
                insn = parts[1] if len(parts) > 1 else ""
            if insn:
                current_lines.append(insn)

    if current_func and current_lines:
        functions[current_func] = current_lines

    return functions


def normalize(insn: str) -> str:
    """Normalize instruction for matching: keep mnemonic + operand shape."""
    # Replace specific addresses/immediates with placeholders
    s = re.sub(r'\$0x[0-9a-f]+', '$IMM', insn)
    s = re.sub(r'\b0x[0-9a-f]+\b', 'IMM', s)
    s = re.sub(r'\b[0-9a-f]{5,}\b', 'IMM', s)
    # Normalize register-relative addressing: keep displacement sign + register
    s = re.sub(r'-?0x[0-9a-f]+\(', 'OFF(', s)
    s = re.sub(r'-?\d+\(', 'OFF(', s)
    return s.strip()


def mnemonic(insn: str) -> str:
    """Extract just the instruction mnemonic."""
    return insn.split()[0] if insn.split() else insn


def extract_mnemonic_sequence(insns: list[str]) -> list[str]:
    """Get ordered mnemonic sequence for LCS matching."""
    return [mnemonic(i) for i in insns]


def lcs_ratio(a: list[str], b: list[str]) -> float:
    """Longest common subsequence ratio via SequenceMatcher."""
    if not a and not b:
        return 1.0
    if not a or not b:
        return 0.0
    return SequenceMatcher(None, a, b).ratio()


FPU_MNEMONICS = {'fld', 'flds', 'fldl', 'fldt', 'fild', 'fst', 'fstp',
                 'fmul', 'fmuls', 'fmulp', 'fdiv', 'fdivs', 'fdivp', 'fdivr', 'fdivrs', 'fdivrp',
                 'fadd', 'fadds', 'faddp', 'fsub', 'fsubs', 'fsubp', 'fsubr', 'fsubrs', 'fsubrp',
                 'fabs', 'fchs', 'fsqrt', 'fsin', 'fcos', 'fxch', 'fcom', 'fcomp', 'fcompp',
                 'fucom', 'fucomp', 'fucompp', 'fnstsw', 'fnstcw', 'fldcw'}


def extract_fpu_blocks(insns: list[str]) -> list[list[str]]:
    """Extract contiguous FPU instruction sequences (basic blocks of FPU ops)."""
    blocks = []
    current = []
    for insn in insns:
        m = mnemonic(insn).lower()
        if m in FPU_MNEMONICS:
            current.append(insn)
        else:
            if len(current) >= 3:
                blocks.append(current)
            current = []
    if len(current) >= 3:
        blocks.append(current)
    return blocks


def compare_fpu_blocks(compiled_blocks: list[list[str]], ref_blocks: list[list[str]]) -> list[str]:
    """Compare FPU blocks between compiled and reference, flag operand swaps."""
    warnings = []
    # Match blocks by position (rough heuristic)
    for i, (cb, rb) in enumerate(zip(compiled_blocks, ref_blocks)):
        c_mnems = [mnemonic(x).lower() for x in cb]
        r_mnems = [mnemonic(x).lower() for x in rb]
        if c_mnems == r_mnems:
            # Same mnemonic sequence — check if operands differ
            for j, (ci, ri) in enumerate(zip(cb, rb)):
                if normalize(ci) != normalize(ri):
                    cm = mnemonic(ci).lower()
                    if cm.startswith(('fsub', 'fmul', 'fld', 'fadd', 'fdiv')):
                        warnings.append(f"    FPU block {i}, insn {j}: {ci.strip()}  vs  {ri.strip()}")
        elif set(c_mnems) == set(r_mnems) and sorted(c_mnems) == sorted(r_mnems):
            # Same mnemonics but reordered — likely operand swap
            warnings.append(f"    FPU block {i}: same ops, different order (possible operand swap)")
            for ci, ri in zip(cb[:6], rb[:6]):
                warnings.append(f"      compiled:  {ci.strip()}")
                warnings.append(f"      reference: {ri.strip()}")
    return warnings


def compare_functions(compiled: list[str], reference: list[str]) -> tuple[float, list[str], list[str]]:
    """Compare two functions. Returns (match_pct, diff_summary, fpu_warnings)."""
    c_mnems = extract_mnemonic_sequence(compiled)
    r_mnems = extract_mnemonic_sequence(reference)

    ratio = lcs_ratio(c_mnems, r_mnems)

    # Generate unified diff summary
    diffs = []
    sm = SequenceMatcher(None, c_mnems, r_mnems)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'replace':
            for k in range(min(i2 - i1, j2 - j1)):
                ci = i1 + k
                rj = j1 + k
                diffs.append(f"  [{ci:4d}] - {compiled[ci].strip()}")
                diffs.append(f"         + {reference[rj].strip()}")
        elif tag == 'delete':
            for ci in range(i1, i2):
                diffs.append(f"  [{ci:4d}] - {compiled[ci].strip()}")
        elif tag == 'insert':
            for rj in range(j1, j2):
                diffs.append(f"         + {reference[rj].strip()}")

    # FPU-specific comparison
    c_fpu = extract_fpu_blocks(compiled)
    r_fpu = extract_fpu_blocks(reference)
    fpu_warnings = compare_fpu_blocks(c_fpu, r_fpu)

    return ratio * 100, diffs, fpu_warnings


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("compiled", help="Compiled .obj file")
    ap.add_argument("reference", help="Delinked reference .obj file")
    ap.add_argument("--function", "-f", help="Compare only this function")
    ap.add_argument("--threshold", "-t", type=float, default=50.0,
                    help="Minimum match %% to pass (default: 50)")
    ap.add_argument("--show-diffs", "-d", action="store_true",
                    help="Show instruction-level diffs")
    ap.add_argument("--fpu-only", action="store_true",
                    help="Only report FPU operand warnings")
    args = ap.parse_args()

    compiled_funcs = disassemble(args.compiled)
    reference_funcs = disassemble(args.reference)

    matched = set(compiled_funcs.keys()) & set(reference_funcs.keys())
    if args.function:
        fn = args.function.lstrip("_")
        if fn not in matched:
            print(f"Function {fn} not found in both objects")
            print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
            print(f"  reference: {sorted(reference_funcs.keys())[:10]}")
            sys.exit(1)
        matched = {fn}

    if not matched:
        print("No matching functions found between objects")
        print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
        print(f"  reference: {sorted(reference_funcs.keys())[:10]}")
        sys.exit(1)

    any_fail = False
    any_fpu_warn = False

    for fn in sorted(matched):
        pct, diffs, fpu_warnings = compare_functions(compiled_funcs[fn], reference_funcs[fn])
        n_c = len(compiled_funcs[fn])
        n_r = len(reference_funcs[fn])
        status = "PASS" if pct >= args.threshold else "FAIL"
        fpu_tag = " [FPU-WARN]" if fpu_warnings else ""

        if not args.fpu_only:
            print(f"  {status} {fn}: {pct:.1f}% match ({n_c}/{n_r} insns){fpu_tag}")

        if fpu_warnings:
            any_fpu_warn = True
            if not args.fpu_only:
                for w in fpu_warnings:
                    print(w)
            else:
                print(f"  {fn}:{fpu_tag}")
                for w in fpu_warnings:
                    print(w)

        if status == "FAIL":
            any_fail = True

        if args.show_diffs and diffs and not args.fpu_only:
            for d in diffs[:60]:
                print(d)
            if len(diffs) > 60:
                print(f"  ... and {len(diffs) - 60} more diff lines")

    if any_fpu_warn:
        print("\nWARNING: FPU operand-order differences detected.")
        print("Check cross-product argument order and FSUB/FSUBR operand direction.")

    sys.exit(1 if any_fail else 0)


if __name__ == "__main__":
    main()
