"""Z3-based coverage-guided seed generator for unicorn_diff.

Two strategies:
  1. Lightweight: disassemble the function, identify CMP/TEST+Jcc patterns,
     and solve for the comparison values directly using Z3 bitvectors.
     Works on all functions regardless of FPU complexity.
  2. Full symbolic: lift the entire function to Z3 via x86_to_z3 and solve
     for branch conditions. Only used for simple GPR-only functions where
     the Z3 Array memory model doesn't blow up.
"""

import math
import struct
import sys
from pathlib import Path
from typing import Optional

_SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(_SCRIPT_DIR))

try:
    import z3
    import capstone
    from capstone import x86_const as X86
    _Z3_AVAILABLE = True
except ImportError:
    _Z3_AVAILABLE = False


def _extract_cmp_branch_pairs(code: bytes) -> list[dict]:
    """Disassemble code and extract CMP/TEST + Jcc pairs.

    Returns a list of dicts describing each branch:
      {type: 'cmp_imm'|'test_imm'|'cmp_reg'|'fpu_cmp',
       jcc: capstone instruction ID,
       imm: int (if applicable),
       address: int}
    """
    if not _Z3_AVAILABLE:
        return []

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    insns = list(md.disasm(code, 0))
    pairs = []

    for i, insn in enumerate(insns):
        if insn.id not in (X86.X86_INS_JE, X86.X86_INS_JNE, X86.X86_INS_JL,
                           X86.X86_INS_JLE, X86.X86_INS_JG, X86.X86_INS_JGE,
                           X86.X86_INS_JB, X86.X86_INS_JBE, X86.X86_INS_JA,
                           X86.X86_INS_JAE, X86.X86_INS_JS, X86.X86_INS_JNS,
                           X86.X86_INS_JP, X86.X86_INS_JNP):
            continue

        # Look back for the flag-setting instruction
        for j in range(i - 1, max(i - 5, -1), -1):
            prev = insns[j]
            if prev.id == X86.X86_INS_CMP and len(prev.operands) == 2:
                op0 = prev.operands[0]
                op1 = prev.operands[1]
                if op1.type == X86.X86_OP_IMM:
                    pairs.append({
                        'type': 'cmp_imm',
                        'jcc': insn.id,
                        'imm': op1.imm,
                        'op_size': op0.size * 8,
                        'address': insn.address,
                    })
                elif op0.type == X86.X86_OP_IMM:
                    pairs.append({
                        'type': 'cmp_imm',
                        'jcc': insn.id,
                        'imm': op0.imm,
                        'op_size': op1.size * 8,
                        'address': insn.address,
                    })
                break
            elif prev.id == X86.X86_INS_TEST and len(prev.operands) == 2:
                op1 = prev.operands[1]
                if op1.type == X86.X86_OP_IMM:
                    pairs.append({
                        'type': 'test_imm',
                        'jcc': insn.id,
                        'imm': op1.imm,
                        'op_size': prev.operands[0].size * 8,
                        'address': insn.address,
                    })
                break
            elif prev.id == X86.X86_INS_FNSTSW:
                pairs.append({
                    'type': 'fpu_cmp',
                    'jcc': insn.id,
                    'address': insn.address,
                })
                break
            elif prev.id in (X86.X86_INS_AND, X86.X86_INS_OR, X86.X86_INS_XOR,
                             X86.X86_INS_SUB, X86.X86_INS_ADD):
                break

    return pairs


def _solve_cmp_pair(pair: dict) -> list:
    """For a CMP+Jcc pair, generate concrete values that satisfy each direction."""
    if not _Z3_AVAILABLE:
        return []

    results = []
    bits = pair.get('op_size', 32)
    imm = pair.get('imm', 0) & ((1 << bits) - 1)

    if pair['type'] == 'cmp_imm':
        x = z3.BitVec('x', bits)
        imm_bv = z3.BitVecVal(imm, bits)
        result = x - imm_bv

        jcc = pair['jcc']
        zero = z3.BitVecVal(0, bits)
        sign_bit = bits - 1

        cond_map = {
            X86.X86_INS_JE:  x == imm_bv,
            X86.X86_INS_JNE: x != imm_bv,
            X86.X86_INS_JL:  z3.Extract(sign_bit, sign_bit, result) != z3.Extract(sign_bit, sign_bit, z3.If(
                z3.And(z3.Extract(sign_bit, sign_bit, x) != z3.Extract(sign_bit, sign_bit, imm_bv),
                       z3.Extract(sign_bit, sign_bit, result) != z3.Extract(sign_bit, sign_bit, x)),
                z3.BitVecVal(1, bits), z3.BitVecVal(0, bits))),
            X86.X86_INS_JB:  z3.ULT(x, imm_bv),
            X86.X86_INS_JBE: z3.ULE(x, imm_bv),
            X86.X86_INS_JA:  z3.UGT(x, imm_bv),
            X86.X86_INS_JAE: z3.UGE(x, imm_bv),
        }
        # Simplified conditions for the common cases
        simple_map = {
            X86.X86_INS_JE:  x == imm_bv,
            X86.X86_INS_JNE: x != imm_bv,
            X86.X86_INS_JB:  z3.ULT(x, imm_bv),
            X86.X86_INS_JBE: z3.ULE(x, imm_bv),
            X86.X86_INS_JA:  z3.UGT(x, imm_bv),
            X86.X86_INS_JAE: z3.UGE(x, imm_bv),
            X86.X86_INS_JL:  x < imm_bv,   # signed
            X86.X86_INS_JLE: x <= imm_bv,
            X86.X86_INS_JG:  x > imm_bv,
            X86.X86_INS_JGE: x >= imm_bv,
        }

        cond = simple_map.get(jcc)
        if cond is None:
            return results

        solver = z3.Solver()
        solver.set('timeout', 500)

        # Taken direction
        solver.push()
        solver.add(cond)
        if solver.check() == z3.sat:
            m = solver.model()
            results.append(m.eval(x, model_completion=True).as_long())
        solver.pop()

        # Not-taken direction
        solver.push()
        solver.add(z3.Not(cond))
        if solver.check() == z3.sat:
            m = solver.model()
            results.append(m.eval(x, model_completion=True).as_long())
        solver.pop()

    elif pair['type'] == 'test_imm':
        x = z3.BitVec('x', bits)
        mask = z3.BitVecVal(imm, bits)
        result = x & mask

        jcc = pair['jcc']
        if jcc == X86.X86_INS_JE:
            cond = result == z3.BitVecVal(0, bits)
        elif jcc == X86.X86_INS_JNE:
            cond = result != z3.BitVecVal(0, bits)
        elif jcc == X86.X86_INS_JS:
            cond = z3.Extract(bits - 1, bits - 1, result) == z3.BitVecVal(1, 1)
        elif jcc == X86.X86_INS_JNS:
            cond = z3.Extract(bits - 1, bits - 1, result) == z3.BitVecVal(0, 1)
        elif jcc == X86.X86_INS_JP:
            # Parity of low byte
            byte = z3.Extract(7, 0, result)
            parity = z3.BitVecVal(0, 1)
            for bi in range(8):
                parity = parity ^ z3.Extract(bi, bi, byte)
            cond = parity == z3.BitVecVal(0, 1)
        elif jcc == X86.X86_INS_JNP:
            byte = z3.Extract(7, 0, result)
            parity = z3.BitVecVal(0, 1)
            for bi in range(8):
                parity = parity ^ z3.Extract(bi, bi, byte)
            cond = parity == z3.BitVecVal(1, 1)
        else:
            return results

        solver = z3.Solver()
        solver.set('timeout', 500)
        solver.push()
        solver.add(cond)
        if solver.check() == z3.sat:
            results.append(solver.model().eval(x, model_completion=True).as_long())
        solver.pop()
        solver.push()
        solver.add(z3.Not(cond))
        if solver.check() == z3.sat:
            results.append(solver.model().eval(x, model_completion=True).as_long())
        solver.pop()

    elif pair['type'] == 'fpu_cmp':
        # FPU comparison: inject boundary floats
        results.extend([
            0,
            0x7F800000,   # +inf
            0xFF800000,   # -inf
            0x7FC00000,   # NaN
            0x3F800000,   # 1.0
            0xBF800000,   # -1.0
            0x00000001,   # smallest denorm
            0x00800000,   # smallest normal
        ])

    return results


def _values_to_seeds(values: list[int], params: list, is_float: bool = False) -> list[list]:
    """Convert solver-produced values into complete seed vectors.

    For each solved value, creates a seed vector with that value for every
    parameter (or a representative placement). This is a coarse approximation
    but catches most branch-coverage cases.
    """
    seeds = []
    for val in values:
        seed = []
        for p in params:
            if p.is_pointer:
                if is_float:
                    blob = struct.pack('<f', _bits_to_float(val)) * 64
                else:
                    blob = struct.pack('<I', val & 0xFFFFFFFF) * 64
                seed.append(blob)
            elif p.is_float:
                if is_float:
                    seed.append(_bits_to_float(val))
                else:
                    seed.append(struct.unpack('<f', struct.pack('<I', val & 0xFFFFFFFF))[0])
            elif p.is_double:
                seed.append(float(val))
            else:
                seed.append(val & 0xFFFFFFFF)
        seeds.append(seed)
    return seeds


def _bits_to_float(bits: int) -> float:
    try:
        return struct.unpack('<f', struct.pack('<I', bits & 0xFFFFFFFF))[0]
    except Exception:
        return 0.0


def extract_branch_seeds(
    code: bytes,
    abi: dict,
    max_paths: int = 64,
    timeout_ms: int = 2000,
) -> list[list]:
    """Generate seed vectors that target branch coverage.

    Uses lightweight CMP/TEST+Jcc pattern extraction rather than full
    symbolic execution, which is fast and works even for FPU-heavy code.

    Returns a list of seed vectors (same format as seeds.generate_seeds output).
    Returns empty list if Z3 is unavailable or the function has no branches.
    """
    if not _Z3_AVAILABLE:
        return []

    pairs = _extract_cmp_branch_pairs(code)
    if not pairs:
        return []

    all_seeds = []
    params = abi['params']

    for pair in pairs:
        if len(all_seeds) >= max_paths:
            break
        values = _solve_cmp_pair(pair)
        is_fpu = pair['type'] == 'fpu_cmp'
        seeds = _values_to_seeds(values, params, is_float=is_fpu)
        all_seeds.extend(seeds)

    return all_seeds[:max_paths]
