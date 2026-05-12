"""Z3 formal equivalence checker for small pure-leaf x86 functions.

Lifts both oracle (MSVC-delinked) and candidate (clang-built) machine
code into Z3 expressions via x86_to_z3, then checks if any input can
produce different outputs.  UNSAT = proven equivalent.

Scope: functions with <=50 instructions, no loops, no transcendentals.
Larger or more complex functions fall back to Unicorn statistical testing.
"""

import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

_SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(_SCRIPT_DIR))

try:
    import z3
    from x86_to_z3 import X86State, X86Lifter, FP80, FP32, BV32, RNE
    _Z3_AVAILABLE = True
except ImportError:
    _Z3_AVAILABLE = False

MAX_INSNS_FOR_EQUIV = 200
MAX_BRANCHES_FOR_EQUIV = 20


@dataclass
class EquivResult:
    proven: bool = False
    counterexample: Optional[dict] = None
    timeout: bool = False
    not_applicable: bool = False
    reason: str = ""


def _count_instructions(code: bytes) -> int:
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    return sum(1 for _ in md.disasm(code, 0))


def _setup_shared_inputs(abi: dict, prefix: str = "eq_"):
    """Create symbolic input variables shared by both oracle and candidate.

    Returns (input_constraints, input_vars_for_state_setup)
    where input_vars_for_state_setup is a dict mapping param index to
    the Z3 variable (for later extraction from counterexample models).
    """
    params = abi['params']
    shared_vars = {}

    for i, p in enumerate(params):
        name = f"{prefix}{p.name}_{i}"
        if p.is_pointer:
            n_floats = 16  # smaller buffer for equiv proofs
            float_vars = []
            for fi in range(n_floats):
                fv = z3.BitVec(f"{name}_f{fi}", 32)
                float_vars.append(fv)
            shared_vars[i] = ('pointer', float_vars)
        elif p.is_float:
            shared_vars[i] = ('float', z3.BitVec(name, 32))
        elif p.is_double:
            shared_vars[i] = ('double', z3.BitVec(name, 64))
        else:
            shared_vars[i] = ('int', z3.BitVec(name, 32))

    return shared_vars


def _apply_inputs_to_state(state: X86State, abi: dict, shared_vars: dict):
    """Write shared symbolic inputs into a state's registers/memory."""
    params = abi['params']
    scratch_ptr = 0x10000000
    slot_size = 0x400

    esp_val = 0x001FFFF0
    state.set_reg32("esp", z3.BitVecVal(esp_val, 32))
    state.set_reg32("ebp", z3.BitVecVal(esp_val, 32))

    fastcall_regs = ['ecx', 'edx'] if abi['conv'] == 'fastcall' else []
    fastcall_idx = 0
    stack_offset = 4

    for i, p in enumerate(params):
        kind, var = shared_vars[i]

        if kind == 'pointer':
            ptr_addr = scratch_ptr + i * slot_size
            float_vars = var
            for fi, fv in enumerate(float_vars):
                addr = z3.BitVecVal(ptr_addr + fi * 4, 32)
                state.mem_write32(addr, fv)

            if p.reg:
                state.set_reg32(p.reg.lower(), z3.BitVecVal(ptr_addr, 32))
            elif abi['conv'] == 'fastcall' and fastcall_idx < len(fastcall_regs):
                state.set_reg32(fastcall_regs[fastcall_idx], z3.BitVecVal(ptr_addr, 32))
                fastcall_idx += 1
            else:
                addr = z3.BitVecVal(esp_val + stack_offset, 32)
                state.mem_write32(addr, z3.BitVecVal(ptr_addr, 32))
                stack_offset += 4

        elif kind == 'float':
            bv = var
            if p.reg:
                state.set_reg32(p.reg.lower(), bv)
            else:
                addr = z3.BitVecVal(esp_val + stack_offset, 32)
                state.mem_write32(addr, bv)
                stack_offset += 4

        elif kind == 'double':
            bv = var
            lo = z3.Extract(31, 0, bv)
            hi = z3.Extract(63, 32, bv)
            addr = z3.BitVecVal(esp_val + stack_offset, 32)
            state.mem_write32(addr, lo)
            state.mem_write32(addr + z3.BitVecVal(4, 32), hi)
            stack_offset += 8

        elif kind == 'int':
            bv = var
            if p.reg:
                state.set_reg32(p.reg.lower(), bv)
            elif abi['conv'] == 'fastcall' and fastcall_idx < len(fastcall_regs) and not p.is_pointer:
                state.set_reg32(fastcall_regs[fastcall_idx], bv)
                fastcall_idx += 1
            else:
                addr = z3.BitVecVal(esp_val + stack_offset, 32)
                state.mem_write32(addr, bv)
                stack_offset += 4


def _extract_counterexample(model, abi: dict, shared_vars: dict) -> dict:
    """Extract concrete input values from a Z3 model."""
    result = {}
    for i, p in enumerate(abi['params']):
        kind, var = shared_vars[i]
        if kind == 'int':
            val = model.eval(var, model_completion=True)
            try:
                result[p.name] = val.as_long()
            except Exception:
                result[p.name] = 0
        elif kind == 'float':
            val = model.eval(var, model_completion=True)
            try:
                bits = val.as_long()
                result[p.name] = struct.unpack('<f', struct.pack('<I', bits & 0xFFFFFFFF))[0]
            except Exception:
                result[p.name] = 0.0
        elif kind == 'double':
            val = model.eval(var, model_completion=True)
            try:
                bits = val.as_long()
                result[p.name] = struct.unpack('<d', struct.pack('<Q', bits & 0xFFFFFFFFFFFFFFFF))[0]
            except Exception:
                result[p.name] = 0.0
        elif kind == 'pointer':
            float_vars = var
            floats = []
            for fv in float_vars[:4]:
                val = model.eval(fv, model_completion=True)
                try:
                    bits = val.as_long()
                    floats.append(struct.unpack('<f', struct.pack('<I', bits & 0xFFFFFFFF))[0])
                except Exception:
                    floats.append(0.0)
            result[p.name] = floats
    return result


def prove_equivalence(
    oracle_code: bytes,
    lifted_code: bytes,
    abi: dict,
    timeout_ms: int = 10_000,
) -> EquivResult:
    """Attempt to formally prove two functions produce identical outputs.

    Returns EquivResult with proven=True if UNSAT (proven equivalent),
    or a counterexample if SAT (divergence found).
    """
    if not _Z3_AVAILABLE:
        return EquivResult(not_applicable=True, reason="z3 not available")

    oracle_insns = _count_instructions(oracle_code)
    lifted_insns = _count_instructions(lifted_code)
    if oracle_insns > MAX_INSNS_FOR_EQUIV or lifted_insns > MAX_INSNS_FOR_EQUIV:
        return EquivResult(not_applicable=True,
                           reason=f"too many instructions (oracle={oracle_insns}, lifted={lifted_insns})")

    shared_vars = _setup_shared_inputs(abi)

    oracle_state = X86State("orc_")
    _apply_inputs_to_state(oracle_state, abi, shared_vars)
    oracle_lifter = X86Lifter(oracle_state, code_base=0)
    oracle_branches = oracle_lifter.lift_function(oracle_code, address=0)

    if oracle_state.has_transcendental:
        return EquivResult(not_applicable=True, reason="oracle uses transcendental FPU ops")
    if len(oracle_branches) > MAX_BRANCHES_FOR_EQUIV:
        return EquivResult(not_applicable=True,
                           reason=f"too many branches: {len(oracle_branches)}")

    lifted_state = X86State("lft_")
    _apply_inputs_to_state(lifted_state, abi, shared_vars)
    lifted_lifter = X86Lifter(lifted_state, code_base=0)
    lifted_branches = lifted_lifter.lift_function(lifted_code, address=0)

    if lifted_state.has_transcendental:
        return EquivResult(not_applicable=True, reason="lifted uses transcendental FPU ops")

    # Build the inequality assertion: some output differs
    diffs = []

    if not abi['ret_void']:
        if abi['ret_st0']:
            orc_st0 = oracle_state.fpu_st(0)
            lft_st0 = lifted_state.fpu_st(0)
            orc_bv = z3.fpToIEEEBV(z3.fpToFP(RNE, orc_st0, FP32))
            lft_bv = z3.fpToIEEEBV(z3.fpToFP(RNE, lft_st0, FP32))
            diffs.append(orc_bv != lft_bv)
        else:
            orc_eax = oracle_state.get_reg32("eax")
            lft_eax = lifted_state.get_reg32("eax")
            diffs.append(orc_eax != lft_eax)

        if abi['ret_edx_eax']:
            orc_edx = oracle_state.get_reg32("edx")
            lft_edx = lifted_state.get_reg32("edx")
            diffs.append(orc_edx != lft_edx)

    # Also check scratch buffer writes for pointer-out params
    for i, p in enumerate(abi['params']):
        if not p.is_pointer:
            continue
        kind, var = shared_vars[i]
        if kind != 'pointer':
            continue
        ptr_addr = 0x10000000 + i * 0x400
        for off in range(0, 16, 4):
            addr = z3.BitVecVal(ptr_addr + off, 32)
            orc_val = oracle_state.mem_read32(addr)
            lft_val = lifted_state.mem_read32(addr)
            diffs.append(orc_val != lft_val)

    if not diffs:
        return EquivResult(not_applicable=True, reason="no outputs to compare (void, no pointer params)")

    solver = z3.Solver()
    solver.set("timeout", timeout_ms)
    solver.add(z3.Or(*diffs))

    result = solver.check()

    if result == z3.unsat:
        return EquivResult(proven=True, reason="UNSAT: functions are equivalent")
    elif result == z3.sat:
        model = solver.model()
        ce = _extract_counterexample(model, abi, shared_vars)
        return EquivResult(proven=False, counterexample=ce,
                           reason="SAT: found diverging input")
    else:
        return EquivResult(timeout=True, reason=f"solver returned {result}")
