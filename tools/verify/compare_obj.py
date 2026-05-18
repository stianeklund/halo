#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Compare compiled object against delinked reference at instruction level.

Uses basic-block-aware matching: extracts instruction mnemonics per basic block
(split at labels/branches), computes LCS-based similarity, and flags FPU
operand-order differences that indicate potential cross-product or subtraction
argument swaps.

Usage:
    python3 tools/verify/compare_obj.py <compiled.obj> <reference.obj> [--function FUN_XXXXXXXX]
    python3 tools/verify/compare_obj.py <compiled.obj> <reference.obj> --show-diffs
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
            if sym.startswith("LAB_") or sym.startswith("switchD_") or sym.startswith("$L"):
                continue
            if current_func and current_lines:
                functions[current_func] = current_lines
            current_func = re.sub(r'@\d+$', '', sym.lstrip("_"))
            current_lines = []
            continue

        stripped = line.strip()
        if not stripped or stripped.startswith("...") or stripped.startswith("Disassembly of section"):
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

    for fn in functions:
        lines = functions[fn]
        while lines and lines[-1].strip().split()[0].startswith('nop'):
            lines.pop()
        functions[fn] = _trim_trailing_thunks(lines)

    return functions


_RET_MNEMS = {'ret', 'retl', 'retw', 'retq', 'retn'}
_THUNK_BODY_MNEMS = {'push', 'pushl', 'pushw', 'call', 'calll', 'callw',
                     'add', 'addl', 'nop', 'pop', 'popl'}

def _trim_trailing_thunks(insns: list) -> list:
    """Strip NOP padding and push/call/ret thunk stubs after the real function RET.

    The delinked reference sometimes bundles MSVC thunk stubs under the preceding
    symbol when there is no next-symbol boundary.  These inflate the reference
    instruction count and cause false low-match scores.

    Strategy: scan forward for the first RET that is followed by nothing but
    [nop*] [push+call+ret]+ sequences consuming the entire remainder.  That RET
    is the true function epilogue; everything after it is thunks.
    """
    if not insns:
        return insns
    n = len(insns)

    for i, insn in enumerate(insns):
        if mnemonic(insn).lower() not in _RET_MNEMS:
            continue

        # Skip alignment NOPs following this ret
        j = i + 1
        while j < n and mnemonic(insns[j]).lower() == 'nop':
            j += 1

        if j >= n:
            continue  # nothing meaningful after this ret — not a thunk boundary

        # Try to consume everything from j to end as push+call+ret thunk stubs
        pos = j
        thunks_found = 0
        while pos < n:
            if mnemonic(insns[pos]).lower() not in {'push', 'pushl', 'pushw'}:
                break  # unexpected — not a thunk start
            sub = pos + 1
            found_call = found_ret = False
            while sub < n:
                sm = mnemonic(insns[sub]).lower()
                if sm in {'call', 'calll', 'callw'}:
                    found_call = True
                elif sm in _RET_MNEMS and found_call:
                    found_ret = True
                    pos = sub + 1
                    while pos < n and mnemonic(insns[pos]).lower() == 'nop':
                        pos += 1
                    break
                elif sm not in _THUNK_BODY_MNEMS:
                    break
                sub += 1
            if not (found_call and found_ret):
                break
            thunks_found += 1

        if pos >= n and thunks_found > 0:
            # All remaining instructions were valid thunk stubs — trim them
            return insns[:i + 1]

    return insns


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


# --- Register-alias normalization ---

_REG_FAMILIES = {
    'eax': ['eax', 'ax', 'al', 'ah'],
    'ecx': ['ecx', 'cx', 'cl', 'ch'],
    'edx': ['edx', 'dx', 'dl', 'dh'],
    'ebx': ['ebx', 'bx', 'bl', 'bh'],
    'esi': ['esi', 'si'],
    'edi': ['edi', 'di'],
}
_FIXED_REGS = frozenset(['esp', 'sp', 'ebp', 'bp'])

_REG_TO_FAMILY = {}
_SUBREG_SUFFIX = {}
for _base, _members in _REG_FAMILIES.items():
    for _r in _members:
        _REG_TO_FAMILY[_r] = _base
        if _r == _base:
            _SUBREG_SUFFIX[_r] = ''
        elif _r.endswith('x') or _r in ('si', 'di'):
            _SUBREG_SUFFIX[_r] = 'w'
        elif _r.endswith('l'):
            _SUBREG_SUFFIX[_r] = 'l'
        elif _r.endswith('h'):
            _SUBREG_SUFFIX[_r] = 'h'

_REG_PATTERN = re.compile(r'%(' + '|'.join(
    sorted(_REG_TO_FAMILY.keys(), key=len, reverse=True)) + r')\b')


def canonicalize_registers(insns: list[str]) -> list[str]:
    """Canonicalize GP registers to positional aliases based on first-appearance order.

    Fixed registers (ESP, EBP) are preserved. All others are mapped to R0, R1, ...
    based on the order their family first appears in the instruction stream.
    Sub-registers keep a size suffix: R0l (low byte), R0h (high byte), R0w (16-bit).
    """
    family_order = {}
    next_idx = [0]

    def _replace_reg(m):
        reg = m.group(1).lower()
        if reg in _FIXED_REGS:
            return m.group(0)
        family = _REG_TO_FAMILY.get(reg)
        if family is None:
            return m.group(0)
        if family not in family_order:
            family_order[family] = next_idx[0]
            next_idx[0] += 1
        idx = family_order[family]
        suffix = _SUBREG_SUFFIX.get(reg, '')
        return '%R' + str(idx) + suffix

    result = []
    for insn in insns:
        result.append(_REG_PATTERN.sub(_replace_reg, insn))
    return result


def normalize_instruction(insn: str) -> str:
    """Full instruction normalization: mnemonic + operand shape with canonical registers."""
    # Strip disassembler label annotations: <symbol+0xNN> or <LAB_xxx>
    s = re.sub(r'\s*<[^>]+>', '', insn)
    s = re.sub(r'\$0x[0-9a-f]+', '$IMM', s)
    s = re.sub(r'\b0x[0-9a-f]+\b', 'IMM', s)
    s = re.sub(r'\b[0-9a-f]{5,}\b', 'IMM', s)
    s = re.sub(r'-?0x[0-9a-f]+\(', 'OFF(', s)
    s = re.sub(r'-?\d+\(', 'OFF(', s)
    return s.strip()


def extract_normalized_sequence(insns: list[str]) -> list[str]:
    """Canonicalize registers then normalize each instruction for LCS."""
    canonical = canonicalize_registers(insns)
    return [normalize_instruction(i) for i in canonical]


def extract_mnemonic_sequence(insns: list[str]) -> list[str]:
    """Get ordered mnemonic sequence for LCS matching."""
    return [mnemonic(i) for i in insns]


def lcs_ratio(a: list[str], b: list[str]) -> float:
    """Longest common subsequence ratio via SequenceMatcher."""
    if not a and not b:
        return 1.0
    if not a or not b:
        return 0.0
    return SequenceMatcher(None, a, b, autojunk=False).ratio()


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


def compare_functions(compiled: list[str], reference: list[str],
                      reg_normalize: bool = False) -> tuple[float, list[str], list[str]]:
    """Compare two functions. Returns (match_pct, diff_summary, fpu_warnings).

    When reg_normalize=True, uses full normalized instructions with register
    canonicalization for LCS instead of mnemonic-only. This catches operand-level
    bugs (argument order swaps, wrong memory sources) while being tolerant of
    benign register allocation differences between compilers.
    """
    if reg_normalize:
        c_seq = extract_normalized_sequence(compiled)
        r_seq = extract_normalized_sequence(reference)
    else:
        c_seq = extract_mnemonic_sequence(compiled)
        r_seq = extract_mnemonic_sequence(reference)

    ratio = lcs_ratio(c_seq, r_seq)

    # Generate unified diff summary (always show raw instructions for readability)
    diffs = []
    sm = SequenceMatcher(None, c_seq, r_seq, autojunk=False)
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
    ap.add_argument("--reg-normalize", "-r", action="store_true",
                    help="Use register-alias normalization for LCS (canonical operands)")
    args = ap.parse_args()

    compiled_funcs = disassemble(args.compiled)
    reference_funcs = disassemble(args.reference)

    matched = set(compiled_funcs.keys()) & set(reference_funcs.keys())
    # Build rename map: when a function was renamed from FUN_xxx, map
    # the new name to the old FUN_xxx name for matching against references.
    rename_map = {}
    if compiled_funcs.keys() - matched:
        try:
            import json
            kb_path = os.path.join(os.path.dirname(os.path.dirname(_tools_dir)), 'kb.json')
            if not os.path.exists(kb_path):
                kb_path = os.path.join(_tools_dir, '..', 'kb.json')
            with open(kb_path) as kf:
                kb = json.load(kf)
            for obj in kb.get('objects', []):
                for fn_entry in obj.get('functions', []):
                    addr = fn_entry.get('addr', '')
                    decl = fn_entry.get('decl', '')
                    # Extract function name from declaration
                    import re as _re
                    m = _re.search(r'\b(\w+)\s*\(', decl)
                    if m and addr:
                        declared_name = m.group(1)
                        fun_name = f"FUN_{int(addr, 16):08x}"
                        if declared_name != fun_name:
                            rename_map[declared_name] = fun_name
        except Exception:
            pass

    if args.function:
        fn = args.function.lstrip("_")
        if fn not in matched:
            # Try rename fallback: look up old FUN_xxx name in reference
            old_name = rename_map.get(fn)
            if old_name and old_name in reference_funcs and fn in compiled_funcs:
                compiled_funcs[old_name] = compiled_funcs[fn]
                matched = {old_name}
                fn = old_name
            else:
                print(f"Function {fn} not found in both objects")
                print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
                print(f"  reference: {sorted(reference_funcs.keys())[:10]}")
                sys.exit(1)
        matched = {fn}

    # Apply rename map for unmatched compiled functions only for whole-object
    # comparisons. --function must stay restricted to the requested symbol.
    if not args.function:
        for new_name, old_name in rename_map.items():
            if new_name in compiled_funcs and old_name in reference_funcs and new_name not in matched:
                compiled_funcs[old_name] = compiled_funcs[new_name]
                matched.add(old_name)

    if not matched:
        print("No matching functions found between objects")
        print(f"  compiled:  {sorted(compiled_funcs.keys())[:10]}")
        print(f"  reference: {sorted(reference_funcs.keys())[:10]}")
        sys.exit(1)

    any_fail = False
    any_fpu_warn = False

    for fn in sorted(matched):
        pct, diffs, fpu_warnings = compare_functions(compiled_funcs[fn], reference_funcs[fn],
                                                      reg_normalize=args.reg_normalize)
        n_c = len(compiled_funcs[fn])
        n_r = len(reference_funcs[fn])
        status = "PASS" if pct >= args.threshold else "FAIL"
        fpu_tag = " [FPU-WARN]" if fpu_warnings else ""
        trunc_tag = " [REF-TRUNCATED?]" if n_r > 0 and n_c > n_r * 1.4 else ""

        # When reg-normalize is on, also compute mnemonic % to show the gap
        reg_tag = ""
        if args.reg_normalize:
            mnem_pct, _, _ = compare_functions(compiled_funcs[fn], reference_funcs[fn],
                                              reg_normalize=False)
            reg_tag = f" [struct:{mnem_pct:.1f}%]"

        if not args.fpu_only:
            print(f"  {status} {fn}: {pct:.1f}% match ({n_c}/{n_r} insns){reg_tag}{fpu_tag}{trunc_tag}")

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
