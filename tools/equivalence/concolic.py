"""Coverage-guided concolic feedback for unicorn_diff.

After Phase 1 (random seeds) produces low code coverage, this module
analyzes which branches were NOT taken, determines what memory values
would force the untaken direction, and generates memory overrides for
Phase 2 re-execution.

The key insight: most untaken branches in game functions gate on a
count/flag loaded from a dynamically allocated global struct.  Unicorn
zero-fills these pages, so count == 0 and the function early-exits.
By injecting non-zero values at the read addresses, we reach the
interesting code paths.
"""

import struct
from dataclasses import dataclass, field
from typing import Optional

try:
    import capstone
    from capstone import x86_const as X86
    _CAPSTONE_AVAILABLE = True
except ImportError:
    _CAPSTONE_AVAILABLE = False


_JCC_IDS = set()
if _CAPSTONE_AVAILABLE:
    _JCC_IDS = {
        X86.X86_INS_JE, X86.X86_INS_JNE,
        X86.X86_INS_JL, X86.X86_INS_JLE,
        X86.X86_INS_JG, X86.X86_INS_JGE,
        X86.X86_INS_JB, X86.X86_INS_JBE,
        X86.X86_INS_JA, X86.X86_INS_JAE,
        X86.X86_INS_JS, X86.X86_INS_JNS,
    }

_EQUALITY_JCC = set()
_LESS_JCC = set()
_GREATER_JCC = set()
_FPU_INS = set()
if _CAPSTONE_AVAILABLE:
    _EQUALITY_JCC = {X86.X86_INS_JE, X86.X86_INS_JNE}
    _LESS_JCC = {X86.X86_INS_JL, X86.X86_INS_JLE, X86.X86_INS_JB, X86.X86_INS_JBE}
    _GREATER_JCC = {X86.X86_INS_JG, X86.X86_INS_JGE, X86.X86_INS_JA, X86.X86_INS_JAE}
    _FPU_INS = {
        getattr(X86, 'X86_INS_COMISS', -1),
        getattr(X86, 'X86_INS_COMISD', -1),
        getattr(X86, 'X86_INS_UCOMISS', -1),
        getattr(X86, 'X86_INS_UCOMISD', -1),
        getattr(X86, 'X86_INS_FCOMI', -1),
        getattr(X86, 'X86_INS_FCOMPI', -1),
        getattr(X86, 'X86_INS_FCOM', -1),
        getattr(X86, 'X86_INS_FCOMP', -1),
        getattr(X86, 'X86_INS_FUCOMI', -1),
        getattr(X86, 'X86_INS_FUCOMPI', -1),
    }


@dataclass
class BranchInfo:
    """A conditional branch extracted from disassembly."""
    address: int
    target: int
    fallthrough: int
    jcc_id: int
    cmp_imm: Optional[int] = None
    cmp_op_size: int = 32
    has_mem_operand: bool = False
    mem_base_reg: int = 0
    mem_disp: int = 0
    is_fpu: bool = False


@dataclass
class UncoveredBranch:
    """A branch where one direction was never executed."""
    branch: BranchInfo
    untaken_is_target: bool


def disassemble_branches(code: bytes, code_base: int) -> list:
    """Disassemble code and extract all conditional branches with their
    preceding CMP/TEST flag-setters.

    Returns list of BranchInfo.
    """
    if not _CAPSTONE_AVAILABLE:
        return []

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    insns = list(md.disasm(code, code_base))

    branches = []
    for i, insn in enumerate(insns):
        if insn.id not in _JCC_IDS:
            continue

        if not insn.operands:
            continue
        target = insn.operands[0].imm
        fallthrough = insn.address + insn.size

        info = BranchInfo(
            address=insn.address,
            target=target,
            fallthrough=fallthrough,
            jcc_id=insn.id,
        )

        for j in range(i - 1, max(i - 5, -1), -1):
            prev = insns[j]
            if (prev.id == X86.X86_INS_CMP or prev.id in _FPU_INS) and len(prev.operands) >= 1:
                if prev.id in _FPU_INS:
                    info.is_fpu = True
                if len(prev.operands) == 2:
                    op0 = prev.operands[0]
                    op1 = prev.operands[1]
                    if op1.type == X86.X86_OP_IMM:
                        info.cmp_imm = op1.imm
                        info.cmp_op_size = op0.size * 8
                    elif op0.type == X86.X86_OP_IMM:
                        info.cmp_imm = op0.imm
                        info.cmp_op_size = op1.size * 8
                    if op0.type == X86.X86_OP_MEM:
                        info.has_mem_operand = True
                        info.mem_base_reg = op0.mem.base
                        info.mem_disp = op0.mem.disp
                    elif op1.type == X86.X86_OP_MEM:
                        info.has_mem_operand = True
                        info.mem_base_reg = op1.mem.base
                        info.mem_disp = op1.mem.disp
                elif len(prev.operands) == 1 and prev.operands[0].type == X86.X86_OP_MEM:
                    info.has_mem_operand = True
                    info.mem_base_reg = prev.operands[0].mem.base
                    info.mem_disp = prev.operands[0].mem.disp
                break
            elif prev.id == X86.X86_INS_TEST and len(prev.operands) == 2:
                op1 = prev.operands[1]
                if op1.type == X86.X86_OP_IMM:
                    info.cmp_imm = op1.imm
                    info.cmp_op_size = prev.operands[0].size * 8
                break
            elif prev.id in (X86.X86_INS_AND, X86.X86_INS_OR, X86.X86_INS_XOR,
                             X86.X86_INS_SUB, X86.X86_INS_ADD):
                break

        branches.append(info)

    return branches


def find_uncovered(branches: list, visited_pcs: dict,
                   code_base: int) -> list:
    """Find branches where one direction was never executed.

    Returns list of UncoveredBranch.
    """
    result = []
    for br in branches:
        target_hit = br.target in visited_pcs
        fall_hit = br.fallthrough in visited_pcs
        if target_hit and fall_hit:
            continue
        if not target_hit and not fall_hit:
            continue
        result.append(UncoveredBranch(
            branch=br,
            untaken_is_target=not target_hit,
        ))
    return result


def _value_for_branch(jcc_id: int, cmp_imm: int, untaken_is_target: bool,
                      op_size: int, is_fpu: bool = False) -> list:
    """Generate values that would force the untaken branch direction.

    Returns a list of candidate integer/float bit-pattern values to inject.
    """
    if is_fpu:
        # Standard float candidate values encoded as 32-bit uints
        float_vals = [0.0, 1.0, -1.0, 0.5, -0.5, 100.0, 1e-5, -100.0]
        return [struct.unpack('<I', struct.pack('<f', fv))[0] for fv in float_vals]

    if cmp_imm is None:
        return [1, 2, 5, 0xFF, 0x10]

    mask = (1 << op_size) - 1
    imm = cmp_imm & mask
    candidates = []

    if jcc_id in _EQUALITY_JCC:
        if untaken_is_target:
            candidates.append(imm)
        else:
            candidates.append((imm + 1) & mask)
            if imm > 0:
                candidates.append((imm - 1) & mask)
    elif jcc_id in _GREATER_JCC:
        if untaken_is_target:
            candidates.append((imm + 1) & mask)
            candidates.append((imm + 5) & mask)
        else:
            if imm > 0:
                candidates.append((imm - 1) & mask)
            candidates.append(0)
    elif jcc_id in _LESS_JCC:
        if untaken_is_target:
            if imm > 0:
                candidates.append((imm - 1) & mask)
            candidates.append(0)
        else:
            candidates.append((imm + 1) & mask)
            candidates.append((imm + 5) & mask)
    elif jcc_id in (X86.X86_INS_JS, X86.X86_INS_JNS) if _CAPSTONE_AVAILABLE else ():
        candidates.append(1)
        candidates.append(0x80 if op_size == 8 else 0x8000 if op_size == 16 else 0x80000000)

    if not candidates:
        candidates = [1, imm, (imm + 1) & mask]

    return candidates


def _is_spurious_address(addr: int) -> bool:
    """Reject injection targets that cause false positives.

    NULL-page reads (addr < 0x10000) and very-high addresses (>= 0x80000000)
    are auto-mapped artifacts, not real game data.  The GLOBALS region
    (0x500000-0x600000) holds DIR32-relocated oracle slots — injecting there
    changes oracle behavior without a matching effect on the lifted code,
    which reads constants at original XBE addresses.
    """
    if addr < 0x10000:
        return True
    if addr >= 0x80000000:
        return True
    if 0x500000 <= addr < 0x600000:
        return True
    return False


def load_value_corpus(path) -> dict:
    """Load a real-frame value corpus {int_addr: [int values]}.

    Produced by halorec_frame_sweep.py --emit-value-corpus: for each global the
    sweep saw read, the distinct real values that global actually held across the
    recording's frames. Keys may be hex strings ("0x...") or ints; values ints or
    hex strings. Returns {} on missing/empty.
    """
    import json
    from pathlib import Path
    if not path:
        return {}
    p = Path(path)
    if not p.exists():
        return {}
    raw = json.loads(p.read_text(encoding="utf-8"))
    out = {}
    for k, vals in raw.items():
        addr = int(k, 0) if isinstance(k, str) else int(k)
        out[addr] = [int(v, 0) if isinstance(v, str) else int(v) for v in vals]
    return out


def generate_memory_injections(uncovered: list, global_reads: dict,
                               code_base: int, value_corpus: dict = None) -> list:
    """Generate memory override dicts that might force untaken branches.

    For each uncovered branch:
    1. If the CMP uses a memory operand and we have a global_read near
       the branch, inject values that flip the condition.
    2. Fallback: for all zero-valued global reads, try small non-zero values.

    `value_corpus` (Step 4: real-frame leverage) maps a global address to the real
    values it held across recorded frames. When present for a target address, those
    feasible engine-produced values are injected ALONGSIDE the synthetic
    condition-flip values — grounding the residual-branch search in states the game
    actually reached instead of invented constants. Empty corpus => unchanged.

    Returns list of {int_address: bytes} override dicts.
    """
    if not uncovered:
        return []

    corpus = value_corpus or {}

    def _pack(addr, sz, val):
        fmt = {1: '<B', 2: '<H', 4: '<I'}.get(sz, '<I')
        try:
            return {addr: struct.pack(fmt, val & ((1 << (sz * 8)) - 1))}
        except struct.error:
            return None

    zero_reads = {addr: (sz, val) for addr, (sz, val) in global_reads.items()
                  if val == 0 and not _is_spurious_address(addr)}

    injections = []

    for ub in uncovered:
        br = ub.branch
        values = _value_for_branch(br.jcc_id, br.cmp_imm,
                                   ub.untaken_is_target, br.cmp_op_size,
                                   is_fpu=br.is_fpu)

        matched_reads = _find_relevant_reads(br, global_reads)
        if matched_reads:
            for read_addr, (read_sz, _) in matched_reads:
                if _is_spurious_address(read_addr):
                    continue
                # synthetic condition-flip values, then real observed values
                for val in list(values[:3]) + corpus.get(read_addr, []):
                    inj = _pack(read_addr, read_sz, val)
                    if inj is not None:
                        injections.append(inj)

    if not injections and zero_reads:
        for addr, (sz, _) in sorted(zero_reads.items())[:16]:
            # prefer real values for this global if the corpus has them
            candidates = corpus.get(addr) or (1, 2, 5, 0x10)
            for val in candidates:
                inj = _pack(addr, sz, val)
                if inj is not None:
                    injections.append(inj)
            if len(injections) >= 32:
                break

    seen = set()
    deduped = []
    for inj in injections:
        key = tuple(sorted((k, v) for k, v in inj.items()))
        if key not in seen:
            seen.add(key)
            deduped.append(inj)
    return deduped


def _find_relevant_reads(branch: BranchInfo, global_reads: dict) -> list:
    """Find global reads that likely feed the branch condition.

    Heuristic: reads within a small address window around the branch
    instruction, or reads from the memory operand's displacement.
    """
    results = []

    if not global_reads:
        return results

    all_reads = sorted(global_reads.items())

    if branch.has_mem_operand and branch.mem_disp != 0:
        disp = branch.mem_disp & 0xFFFFFFFF
        for addr, (sz, val) in all_reads:
            offset = (addr - disp) & 0xFFFFFFFF
            if offset < 0x100:
                results.append((addr, (sz, val)))

    if not results:
        for addr, (sz, val) in all_reads:
            if val == 0:
                results.append((addr, (sz, val)))
                if len(results) >= 4:
                    break

    return results
