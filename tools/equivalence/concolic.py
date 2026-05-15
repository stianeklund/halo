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
if _CAPSTONE_AVAILABLE:
    _EQUALITY_JCC = {X86.X86_INS_JE, X86.X86_INS_JNE}
    _LESS_JCC = {X86.X86_INS_JL, X86.X86_INS_JLE, X86.X86_INS_JB, X86.X86_INS_JBE}
    _GREATER_JCC = {X86.X86_INS_JG, X86.X86_INS_JGE, X86.X86_INS_JA, X86.X86_INS_JAE}


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
            if prev.id == X86.X86_INS_CMP and len(prev.operands) == 2:
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
                      op_size: int) -> list:
    """Generate values that would force the untaken branch direction.

    Returns a list of candidate integer values to inject.
    """
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


def generate_memory_injections(uncovered: list, global_reads: dict,
                               code_base: int) -> list:
    """Generate memory override dicts that might force untaken branches.

    For each uncovered branch:
    1. If the CMP uses a memory operand and we have a global_read near
       the branch, inject values that flip the condition.
    2. Fallback: for all zero-valued global reads, try small non-zero values.

    Returns list of {int_address: bytes} override dicts.
    """
    if not uncovered:
        return []

    zero_reads = {addr: (sz, val) for addr, (sz, val) in global_reads.items()
                  if val == 0}

    injections = []

    for ub in uncovered:
        br = ub.branch
        values = _value_for_branch(br.jcc_id, br.cmp_imm,
                                   ub.untaken_is_target, br.cmp_op_size)

        matched_reads = _find_relevant_reads(br, global_reads)
        if matched_reads:
            for read_addr, (read_sz, _) in matched_reads:
                for val in values[:3]:
                    fmt = {1: '<B', 2: '<H', 4: '<I'}.get(read_sz, '<I')
                    try:
                        data = struct.pack(fmt, val & ((1 << (read_sz * 8)) - 1))
                    except struct.error:
                        continue
                    injections.append({read_addr: data})

    if not injections and zero_reads:
        for addr, (sz, _) in sorted(zero_reads.items())[:16]:
            fmt = {1: '<B', 2: '<H', 4: '<I'}.get(sz, '<I')
            for val in (1, 2, 5, 0x10):
                try:
                    data = struct.pack(fmt, val & ((1 << (sz * 8)) - 1))
                except struct.error:
                    continue
                injections.append({addr: data})
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
