"""x86-to-Z3 abstract interpreter for i386 machine code.

Lifts x86 instructions into Z3 bitvector/floating-point expressions.
Used by z3_seeds.py (branch-condition extraction) and z3_equiv.py
(formal equivalence proofs).

Supports: GPR ops, EFLAGS, x87 FPU stack, basic memory model.
Does NOT support: SSE/AVX, segments, ring transitions.
"""

import struct
from dataclasses import dataclass, field
from typing import Optional

import capstone
from capstone import x86_const as X86
import z3

BV32 = z3.BitVecSort(32)
BV8 = z3.BitVecSort(8)
FP80 = z3.FPSort(15, 64)
FP64 = z3.FPSort(11, 53)
FP32 = z3.FPSort(8, 24)
RNE = z3.RNE()

_REG_NAMES_32 = {
    X86.X86_REG_EAX: "eax", X86.X86_REG_EBX: "ebx",
    X86.X86_REG_ECX: "ecx", X86.X86_REG_EDX: "edx",
    X86.X86_REG_ESI: "esi", X86.X86_REG_EDI: "edi",
    X86.X86_REG_ESP: "esp", X86.X86_REG_EBP: "ebp",
}

_REG_NAMES_16 = {
    X86.X86_REG_AX: "eax", X86.X86_REG_BX: "ebx",
    X86.X86_REG_CX: "ecx", X86.X86_REG_DX: "edx",
    X86.X86_REG_SI: "esi", X86.X86_REG_DI: "edi",
    X86.X86_REG_SP: "esp", X86.X86_REG_BP: "ebp",
}

_REG_NAMES_8L = {
    X86.X86_REG_AL: "eax", X86.X86_REG_BL: "ebx",
    X86.X86_REG_CL: "ecx", X86.X86_REG_DL: "edx",
}

_REG_NAMES_8H = {
    X86.X86_REG_AH: "eax", X86.X86_REG_BH: "ebx",
    X86.X86_REG_CH: "ecx", X86.X86_REG_DH: "edx",
}

_JCC_CONDS = {
    X86.X86_INS_JE:  lambda s: s.zf,
    X86.X86_INS_JNE: lambda s: z3.Not(s.zf),
    X86.X86_INS_JL:  lambda s: s.sf != s.of,
    X86.X86_INS_JGE: lambda s: s.sf == s.of,
    X86.X86_INS_JLE: lambda s: z3.Or(s.zf, s.sf != s.of),
    X86.X86_INS_JG:  lambda s: z3.And(z3.Not(s.zf), s.sf == s.of),
    X86.X86_INS_JB:  lambda s: s.cf,
    X86.X86_INS_JAE: lambda s: z3.Not(s.cf),
    X86.X86_INS_JBE: lambda s: z3.Or(s.cf, s.zf),
    X86.X86_INS_JA:  lambda s: z3.And(z3.Not(s.cf), z3.Not(s.zf)),
    X86.X86_INS_JS:  lambda s: s.sf,
    X86.X86_INS_JNS: lambda s: z3.Not(s.sf),
    X86.X86_INS_JP:  lambda s: s.pf,
    X86.X86_INS_JNP: lambda s: z3.Not(s.pf),
    X86.X86_INS_JO:  lambda s: s.of,
    X86.X86_INS_JNO: lambda s: z3.Not(s.of),
}

_SETCC_CONDS = {
    X86.X86_INS_SETE:  lambda s: s.zf,
    X86.X86_INS_SETNE: lambda s: z3.Not(s.zf),
    X86.X86_INS_SETL:  lambda s: s.sf != s.of,
    X86.X86_INS_SETGE: lambda s: s.sf == s.of,
    X86.X86_INS_SETLE: lambda s: z3.Or(s.zf, s.sf != s.of),
    X86.X86_INS_SETG:  lambda s: z3.And(z3.Not(s.zf), s.sf == s.of),
    X86.X86_INS_SETB:  lambda s: s.cf,
    X86.X86_INS_SETAE: lambda s: z3.Not(s.cf),
    X86.X86_INS_SETBE: lambda s: z3.Or(s.cf, s.zf),
    X86.X86_INS_SETA:  lambda s: z3.And(z3.Not(s.cf), z3.Not(s.zf)),
    X86.X86_INS_SETS:  lambda s: s.sf,
    X86.X86_INS_SETNS: lambda s: z3.Not(s.sf),
}

FPU_LOAD_IDS = {X86.X86_INS_FLD}
FPU_STORE_IDS = {X86.X86_INS_FST, X86.X86_INS_FSTP}
FPU_ARITH_IDS = {
    X86.X86_INS_FADD,  # capstone merges FADDP into FADD (same id); detect via mnemonic
    X86.X86_INS_FSUB, X86.X86_INS_FSUBP, X86.X86_INS_FSUBR, X86.X86_INS_FSUBRP,
    X86.X86_INS_FMUL, X86.X86_INS_FMULP,
    X86.X86_INS_FDIV, X86.X86_INS_FDIVP, X86.X86_INS_FDIVR, X86.X86_INS_FDIVRP,
}
FPU_MISC_IDS = {
    X86.X86_INS_FCHS, X86.X86_INS_FABS, X86.X86_INS_FSQRT,
    X86.X86_INS_FXCH,
    X86.X86_INS_FILD, X86.X86_INS_FISTP, X86.X86_INS_FIST,
    X86.X86_INS_FCOM, X86.X86_INS_FCOMP, X86.X86_INS_FCOMPP,
    X86.X86_INS_FUCOM, X86.X86_INS_FUCOMP, X86.X86_INS_FUCOMPP,
    X86.X86_INS_FNSTSW, X86.X86_INS_FNSTCW, X86.X86_INS_FLDCW,
    X86.X86_INS_FLDZ, X86.X86_INS_FLD1,
}
FPU_TRANSCENDENTAL_IDS = {
    X86.X86_INS_FSIN, X86.X86_INS_FCOS,
    X86.X86_INS_FPATAN, X86.X86_INS_FPTAN,
}
ALL_FPU_IDS = FPU_LOAD_IDS | FPU_STORE_IDS | FPU_ARITH_IDS | FPU_MISC_IDS | FPU_TRANSCENDENTAL_IDS


@dataclass
class BranchPoint:
    """A conditional branch discovered during lifting."""
    address: int
    condition: z3.BoolRef
    taken_target: int
    fallthrough_target: int


class LiftError(Exception):
    """Raised when the lifter encounters an unsupported construct."""
    pass


class X86State:
    """Symbolic x86 machine state backed by Z3 expressions."""

    def __init__(self, prefix: str = ""):
        self.prefix = prefix
        self.gprs = {
            "eax": z3.BitVec(f"{prefix}eax", 32),
            "ebx": z3.BitVec(f"{prefix}ebx", 32),
            "ecx": z3.BitVec(f"{prefix}ecx", 32),
            "edx": z3.BitVec(f"{prefix}edx", 32),
            "esi": z3.BitVec(f"{prefix}esi", 32),
            "edi": z3.BitVec(f"{prefix}edi", 32),
            "esp": z3.BitVec(f"{prefix}esp", 32),
            "ebp": z3.BitVec(f"{prefix}ebp", 32),
        }
        self.cf = z3.Bool(f"{prefix}cf")
        self.zf = z3.Bool(f"{prefix}zf")
        self.sf = z3.Bool(f"{prefix}sf")
        self.of = z3.Bool(f"{prefix}of")
        self.pf = z3.Bool(f"{prefix}pf")

        self.mem = z3.Array(f"{prefix}mem", BV32, BV8)

        self.fpu_stack: list = []  # list of Z3 FP80 expressions, index 0 = ST(0)
        self._fpu_fresh = 0  # counter for fresh FPU variables
        self._mem_fresh = 0

        self.has_transcendental = False
        self.unsupported_insns: list[str] = []

    def get_reg32(self, name: str):
        return self.gprs[name]

    def set_reg32(self, name: str, val):
        self.gprs[name] = val

    def get_reg16(self, name32: str):
        return z3.Extract(15, 0, self.gprs[name32])

    def set_reg16(self, name32: str, val):
        hi = z3.Extract(31, 16, self.gprs[name32])
        self.gprs[name32] = z3.Concat(hi, val)

    def get_reg8l(self, name32: str):
        return z3.Extract(7, 0, self.gprs[name32])

    def set_reg8l(self, name32: str, val):
        hi = z3.Extract(31, 8, self.gprs[name32])
        self.gprs[name32] = z3.Concat(hi, val)

    def get_reg8h(self, name32: str):
        return z3.Extract(15, 8, self.gprs[name32])

    def set_reg8h(self, name32: str, val):
        top = z3.Extract(31, 16, self.gprs[name32])
        lo = z3.Extract(7, 0, self.gprs[name32])
        self.gprs[name32] = z3.Concat(top, val, lo)

    def mem_read32(self, addr):
        b0 = z3.Select(self.mem, addr)
        b1 = z3.Select(self.mem, addr + 1)
        b2 = z3.Select(self.mem, addr + 2)
        b3 = z3.Select(self.mem, addr + 3)
        return z3.Concat(b3, b2, b1, b0)

    def mem_write32(self, addr, val):
        self.mem = z3.Store(self.mem, addr, z3.Extract(7, 0, val))
        self.mem = z3.Store(self.mem, addr + 1, z3.Extract(15, 8, val))
        self.mem = z3.Store(self.mem, addr + 2, z3.Extract(23, 16, val))
        self.mem = z3.Store(self.mem, addr + 3, z3.Extract(31, 24, val))

    def mem_read16(self, addr):
        b0 = z3.Select(self.mem, addr)
        b1 = z3.Select(self.mem, addr + 1)
        return z3.Concat(b1, b0)

    def mem_write16(self, addr, val):
        self.mem = z3.Store(self.mem, addr, z3.Extract(7, 0, val))
        self.mem = z3.Store(self.mem, addr + 1, z3.Extract(15, 8, val))

    def mem_read8(self, addr):
        return z3.Select(self.mem, addr)

    def mem_write8(self, addr, val):
        self.mem = z3.Store(self.mem, addr, val)

    def mem_read_fp32(self, addr):
        bv = self.mem_read32(addr)
        fp32 = z3.fpToFP(bv, FP32)
        return z3.fpToFP(RNE, fp32, FP80)

    def mem_read_fp64(self, addr):
        lo = self.mem_read32(addr)
        hi = self.mem_read32(addr + z3.BitVecVal(4, 32))
        bv64 = z3.Concat(hi, lo)
        fp64 = z3.fpToFP(bv64, FP64)
        return z3.fpToFP(RNE, fp64, FP80)

    def mem_write_fp32(self, addr, fpval):
        fp32 = z3.fpToFP(RNE, fpval, FP32)
        bv = z3.fpToIEEEBV(fp32)
        self.mem_write32(addr, bv)

    def mem_write_fp64(self, addr, fpval):
        fp64 = z3.fpToFP(RNE, fpval, FP64)
        bv = z3.fpToIEEEBV(fp64)
        lo = z3.Extract(31, 0, bv)
        hi = z3.Extract(63, 32, bv)
        self.mem_write32(addr, lo)
        self.mem_write32(addr + 4, hi)

    def fpu_push(self, val):
        self.fpu_stack.insert(0, val)
        if len(self.fpu_stack) > 8:
            self.fpu_stack = self.fpu_stack[:8]

    def fpu_pop(self):
        if not self.fpu_stack:
            self._fpu_fresh += 1
            return z3.FP(f"{self.prefix}fpu_fresh_{self._fpu_fresh}", FP80)
        return self.fpu_stack.pop(0)

    def fpu_st(self, i: int = 0):
        while len(self.fpu_stack) <= i:
            self._fpu_fresh += 1
            self.fpu_stack.append(z3.FP(f"{self.prefix}fpu_init_{self._fpu_fresh}", FP80))
        return self.fpu_stack[i]

    def fpu_set_st(self, i: int, val):
        while len(self.fpu_stack) <= i:
            self._fpu_fresh += 1
            self.fpu_stack.append(z3.FP(f"{self.prefix}fpu_init_{self._fpu_fresh}", FP80))
        self.fpu_stack[i] = val

    def fresh_mem_var(self, bits: int = 32):
        self._mem_fresh += 1
        return z3.BitVec(f"{self.prefix}mem_fresh_{self._mem_fresh}", bits)

    def update_flags_arith(self, result, op1, op2, bits: int = 32):
        zero = z3.BitVecVal(0, bits)
        self.zf = result == zero
        self.sf = z3.Extract(bits - 1, bits - 1, result) == z3.BitVecVal(1, 1)

        b0 = z3.Extract(0, 0, result)
        b1 = z3.Extract(1, 1, result)
        b2 = z3.Extract(2, 2, result)
        b3 = z3.Extract(3, 3, result)
        b4 = z3.Extract(4, 4, result)
        b5 = z3.Extract(5, 5, result)
        b6 = z3.Extract(6, 6, result)
        b7 = z3.Extract(7, 7, result)
        parity = b0 ^ b1 ^ b2 ^ b3 ^ b4 ^ b5 ^ b6 ^ b7
        self.pf = parity == z3.BitVecVal(0, 1)

    def update_flags_add(self, result, op1, op2, bits: int = 32):
        self.update_flags_arith(result, op1, op2, bits)
        self.cf = z3.ULT(result, op1)
        sign_op1 = z3.Extract(bits - 1, bits - 1, op1)
        sign_op2 = z3.Extract(bits - 1, bits - 1, op2)
        sign_res = z3.Extract(bits - 1, bits - 1, result)
        self.of = z3.And(sign_op1 == sign_op2, sign_res != sign_op1)

    def update_flags_sub(self, result, op1, op2, bits: int = 32):
        self.update_flags_arith(result, op1, op2, bits)
        self.cf = z3.ULT(op1, op2)
        sign_op1 = z3.Extract(bits - 1, bits - 1, op1)
        sign_op2 = z3.Extract(bits - 1, bits - 1, op2)
        sign_res = z3.Extract(bits - 1, bits - 1, result)
        self.of = z3.And(sign_op1 != sign_op2, sign_res != sign_op1)

    def update_flags_logic(self, result, bits: int = 32):
        self.update_flags_arith(result, result, result, bits)
        self.cf = z3.BoolVal(False)
        self.of = z3.BoolVal(False)


class X86Lifter:
    """Lift x86 machine code to Z3 expressions."""

    def __init__(self, state: X86State, code_base: int = 0):
        self.state = state
        self.code_base = code_base
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.branches: list[BranchPoint] = []

    def _read_operand(self, insn, op, bits: int = 32):
        """Read a capstone operand, returning a Z3 BitVec of the given width."""
        s = self.state
        if op.type == X86.X86_OP_REG:
            return self._read_reg(op.reg, bits)
        elif op.type == X86.X86_OP_IMM:
            return z3.BitVecVal(op.imm & ((1 << bits) - 1), bits)
        elif op.type == X86.X86_OP_MEM:
            addr = self._compute_mem_addr(op.mem)
            if bits == 32:
                return s.mem_read32(addr)
            elif bits == 16:
                return s.mem_read16(addr)
            elif bits == 8:
                return s.mem_read8(addr)
            else:
                return s.mem_read32(addr)
        raise LiftError(f"unsupported operand type {op.type} at 0x{insn.address:x}")

    def _write_operand(self, insn, op, val, bits: int = 32):
        s = self.state
        if op.type == X86.X86_OP_REG:
            self._write_reg(op.reg, val, bits)
        elif op.type == X86.X86_OP_MEM:
            addr = self._compute_mem_addr(op.mem)
            if bits == 32:
                s.mem_write32(addr, val)
            elif bits == 16:
                s.mem_write16(addr, val)
            elif bits == 8:
                s.mem_write8(addr, val)
        else:
            raise LiftError(f"cannot write to operand type {op.type} at 0x{insn.address:x}")

    def _read_reg(self, reg_id: int, bits: int = 32):
        s = self.state
        if reg_id in _REG_NAMES_32:
            v = s.get_reg32(_REG_NAMES_32[reg_id])
            return v if bits == 32 else z3.Extract(bits - 1, 0, v)
        if reg_id in _REG_NAMES_16:
            return s.get_reg16(_REG_NAMES_16[reg_id])
        if reg_id in _REG_NAMES_8L:
            return s.get_reg8l(_REG_NAMES_8L[reg_id])
        if reg_id in _REG_NAMES_8H:
            return s.get_reg8h(_REG_NAMES_8H[reg_id])
        raise LiftError(f"unsupported register {reg_id}")

    def _write_reg(self, reg_id: int, val, bits: int = 32):
        s = self.state
        if reg_id in _REG_NAMES_32:
            if bits < 32:
                val = z3.ZeroExt(32 - bits, val)
            s.set_reg32(_REG_NAMES_32[reg_id], val)
        elif reg_id in _REG_NAMES_16:
            s.set_reg16(_REG_NAMES_16[reg_id], val)
        elif reg_id in _REG_NAMES_8L:
            s.set_reg8l(_REG_NAMES_8L[reg_id], val)
        elif reg_id in _REG_NAMES_8H:
            s.set_reg8h(_REG_NAMES_8H[reg_id], val)
        else:
            raise LiftError(f"unsupported register write {reg_id}")

    def _compute_mem_addr(self, mem):
        s = self.state
        addr = z3.BitVecVal(mem.disp, 32)
        if mem.base != 0:
            addr = addr + self._read_reg(mem.base, 32)
        if mem.index != 0:
            idx = self._read_reg(mem.index, 32)
            if mem.scale > 1:
                idx = idx * z3.BitVecVal(mem.scale, 32)
            addr = addr + idx
        return addr

    def _op_size(self, insn, op_idx: int = 0) -> int:
        if op_idx < len(insn.operands):
            return insn.operands[op_idx].size * 8
        return 32

    def lift_instruction(self, insn) -> Optional[BranchPoint]:
        """Lift one capstone instruction. Returns BranchPoint if conditional branch."""
        s = self.state
        iid = insn.id
        ops = insn.operands

        if iid == X86.X86_INS_NOP or iid == X86.X86_INS_INT3:
            return None

        # --- MOV family ---
        if iid == X86.X86_INS_MOV:
            bits = self._op_size(insn, 0)
            val = self._read_operand(insn, ops[1], bits)
            self._write_operand(insn, ops[0], val, bits)
            return None

        if iid == X86.X86_INS_MOVSX or iid == X86.X86_INS_MOVSXD:
            dst_bits = self._op_size(insn, 0)
            src_bits = self._op_size(insn, 1)
            val = self._read_operand(insn, ops[1], src_bits)
            ext = z3.SignExt(dst_bits - src_bits, val)
            self._write_operand(insn, ops[0], ext, dst_bits)
            return None

        if iid == X86.X86_INS_MOVZX:
            dst_bits = self._op_size(insn, 0)
            src_bits = self._op_size(insn, 1)
            val = self._read_operand(insn, ops[1], src_bits)
            ext = z3.ZeroExt(dst_bits - src_bits, val)
            self._write_operand(insn, ops[0], ext, dst_bits)
            return None

        # --- LEA ---
        if iid == X86.X86_INS_LEA:
            addr = self._compute_mem_addr(ops[1].mem)
            self._write_operand(insn, ops[0], addr, 32)
            return None

        # --- PUSH / POP ---
        if iid == X86.X86_INS_PUSH:
            bits = self._op_size(insn, 0)
            val = self._read_operand(insn, ops[0], bits)
            if bits < 32:
                val = z3.SignExt(32 - bits, val)
            esp = s.get_reg32("esp") - z3.BitVecVal(4, 32)
            s.set_reg32("esp", esp)
            s.mem_write32(esp, val)
            return None

        if iid == X86.X86_INS_POP:
            esp = s.get_reg32("esp")
            val = s.mem_read32(esp)
            s.set_reg32("esp", esp + z3.BitVecVal(4, 32))
            bits = self._op_size(insn, 0)
            if bits < 32:
                val = z3.Extract(bits - 1, 0, val)
            self._write_operand(insn, ops[0], val, bits)
            return None

        # --- Arithmetic ---
        if iid in (X86.X86_INS_ADD, X86.X86_INS_ADC):
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            b = self._read_operand(insn, ops[1], bits)
            result = a + b
            if iid == X86.X86_INS_ADC:
                carry = z3.If(s.cf, z3.BitVecVal(1, bits), z3.BitVecVal(0, bits))
                result = result + carry
            s.update_flags_add(result, a, b, bits)
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid in (X86.X86_INS_SUB, X86.X86_INS_SBB):
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            b = self._read_operand(insn, ops[1], bits)
            result = a - b
            if iid == X86.X86_INS_SBB:
                borrow = z3.If(s.cf, z3.BitVecVal(1, bits), z3.BitVecVal(0, bits))
                result = result - borrow
            s.update_flags_sub(result, a, b, bits)
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_CMP:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            b = self._read_operand(insn, ops[1], bits)
            result = a - b
            s.update_flags_sub(result, a, b, bits)
            return None

        if iid == X86.X86_INS_INC:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            one = z3.BitVecVal(1, bits)
            result = a + one
            old_cf = s.cf
            s.update_flags_add(result, a, one, bits)
            s.cf = old_cf  # INC preserves CF
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_DEC:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            one = z3.BitVecVal(1, bits)
            result = a - one
            old_cf = s.cf
            s.update_flags_sub(result, a, one, bits)
            s.cf = old_cf  # DEC preserves CF
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_NEG:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            zero = z3.BitVecVal(0, bits)
            result = zero - a
            s.update_flags_sub(result, zero, a, bits)
            s.cf = a != zero
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_IMUL:
            if len(ops) == 1:
                bits = self._op_size(insn, 0)
                a = s.get_reg32("eax") if bits == 32 else z3.Extract(bits - 1, 0, s.get_reg32("eax"))
                b = self._read_operand(insn, ops[0], bits)
                a_ext = z3.SignExt(bits, a)
                b_ext = z3.SignExt(bits, b)
                result = a_ext * b_ext
                if bits == 32:
                    s.set_reg32("eax", z3.Extract(31, 0, result))
                    s.set_reg32("edx", z3.Extract(63, 32, result))
            elif len(ops) == 2:
                bits = self._op_size(insn, 0)
                a = self._read_operand(insn, ops[0], bits)
                b = self._read_operand(insn, ops[1], bits)
                result = a * b
                self._write_operand(insn, ops[0], result, bits)
            elif len(ops) == 3:
                bits = self._op_size(insn, 0)
                b = self._read_operand(insn, ops[1], bits)
                c = self._read_operand(insn, ops[2], bits)
                result = b * c
                self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_MUL:
            bits = self._op_size(insn, 0)
            a = s.get_reg32("eax") if bits == 32 else z3.Extract(bits - 1, 0, s.get_reg32("eax"))
            b = self._read_operand(insn, ops[0], bits)
            a_ext = z3.ZeroExt(bits, a)
            b_ext = z3.ZeroExt(bits, b)
            result = a_ext * b_ext
            if bits == 32:
                s.set_reg32("eax", z3.Extract(31, 0, result))
                s.set_reg32("edx", z3.Extract(63, 32, result))
            return None

        if iid in (X86.X86_INS_DIV, X86.X86_INS_IDIV):
            bits = self._op_size(insn, 0)
            divisor = self._read_operand(insn, ops[0], bits)
            if bits == 32:
                edx = s.get_reg32("edx")
                eax = s.get_reg32("eax")
                dividend = z3.Concat(edx, eax)
                if iid == X86.X86_INS_IDIV:
                    d_ext = z3.SignExt(32, divisor)
                    quot = dividend / d_ext  # signed
                    rem = z3.SRem(dividend, d_ext)
                else:
                    d_ext = z3.ZeroExt(32, divisor)
                    quot = z3.UDiv(dividend, d_ext)
                    rem = z3.URem(dividend, d_ext)
                s.set_reg32("eax", z3.Extract(31, 0, quot))
                s.set_reg32("edx", z3.Extract(31, 0, rem))
            return None

        if iid == X86.X86_INS_CDQ:
            eax = s.get_reg32("eax")
            sign = z3.Extract(31, 31, eax)
            edx = z3.If(sign == z3.BitVecVal(1, 1),
                        z3.BitVecVal(0xFFFFFFFF, 32),
                        z3.BitVecVal(0, 32))
            s.set_reg32("edx", edx)
            return None

        # --- Bitwise ---
        if iid == X86.X86_INS_AND:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            b = self._read_operand(insn, ops[1], bits)
            result = a & b
            s.update_flags_logic(result, bits)
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_OR:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            b = self._read_operand(insn, ops[1], bits)
            result = a | b
            s.update_flags_logic(result, bits)
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_XOR:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            b = self._read_operand(insn, ops[1], bits)
            result = a ^ b
            s.update_flags_logic(result, bits)
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_NOT:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            result = ~a
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_TEST:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            b = self._read_operand(insn, ops[1], bits)
            result = a & b
            s.update_flags_logic(result, bits)
            return None

        # --- Shifts ---
        if iid in (X86.X86_INS_SHL, X86.X86_INS_SAL):
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            cnt = self._read_operand(insn, ops[1], 8) if len(ops) > 1 else z3.BitVecVal(1, 8)
            cnt32 = z3.ZeroExt(bits - 8, cnt)
            result = a << cnt32
            s.update_flags_logic(result, bits)
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_SHR:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            cnt = self._read_operand(insn, ops[1], 8) if len(ops) > 1 else z3.BitVecVal(1, 8)
            cnt32 = z3.ZeroExt(bits - 8, cnt)
            result = z3.LShR(a, cnt32)
            s.update_flags_logic(result, bits)
            self._write_operand(insn, ops[0], result, bits)
            return None

        if iid == X86.X86_INS_SAR:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            cnt = self._read_operand(insn, ops[1], 8) if len(ops) > 1 else z3.BitVecVal(1, 8)
            cnt32 = z3.ZeroExt(bits - 8, cnt)
            result = a >> cnt32
            s.update_flags_logic(result, bits)
            self._write_operand(insn, ops[0], result, bits)
            return None

        # --- SETcc ---
        if iid in _SETCC_CONDS:
            cond = _SETCC_CONDS[iid](s)
            val = z3.If(cond, z3.BitVecVal(1, 8), z3.BitVecVal(0, 8))
            self._write_operand(insn, ops[0], val, 8)
            return None

        # --- CMOVcc ---
        if iid in (X86.X86_INS_CMOVE, X86.X86_INS_CMOVNE, X86.X86_INS_CMOVL,
                    X86.X86_INS_CMOVGE, X86.X86_INS_CMOVLE, X86.X86_INS_CMOVG,
                    X86.X86_INS_CMOVB, X86.X86_INS_CMOVAE, X86.X86_INS_CMOVBE,
                    X86.X86_INS_CMOVA, X86.X86_INS_CMOVS, X86.X86_INS_CMOVNS):
            cmov_map = {
                X86.X86_INS_CMOVE: _JCC_CONDS[X86.X86_INS_JE],
                X86.X86_INS_CMOVNE: _JCC_CONDS[X86.X86_INS_JNE],
                X86.X86_INS_CMOVL: _JCC_CONDS[X86.X86_INS_JL],
                X86.X86_INS_CMOVGE: _JCC_CONDS[X86.X86_INS_JGE],
                X86.X86_INS_CMOVLE: _JCC_CONDS[X86.X86_INS_JLE],
                X86.X86_INS_CMOVG: _JCC_CONDS[X86.X86_INS_JG],
                X86.X86_INS_CMOVB: _JCC_CONDS[X86.X86_INS_JB],
                X86.X86_INS_CMOVAE: _JCC_CONDS[X86.X86_INS_JAE],
                X86.X86_INS_CMOVBE: _JCC_CONDS[X86.X86_INS_JBE],
                X86.X86_INS_CMOVA: _JCC_CONDS[X86.X86_INS_JA],
                X86.X86_INS_CMOVS: _JCC_CONDS[X86.X86_INS_JS],
                X86.X86_INS_CMOVNS: _JCC_CONDS[X86.X86_INS_JNS],
            }
            bits = self._op_size(insn, 0)
            cond = cmov_map[iid](s)
            src = self._read_operand(insn, ops[1], bits)
            dst = self._read_operand(insn, ops[0], bits)
            result = z3.If(cond, src, dst)
            self._write_operand(insn, ops[0], result, bits)
            return None

        # --- XCHG ---
        if iid == X86.X86_INS_XCHG:
            bits = self._op_size(insn, 0)
            a = self._read_operand(insn, ops[0], bits)
            b = self._read_operand(insn, ops[1], bits)
            self._write_operand(insn, ops[0], b, bits)
            self._write_operand(insn, ops[1], a, bits)
            return None

        # --- Conditional branches ---
        if iid in _JCC_CONDS:
            cond = _JCC_CONDS[iid](s)
            target = ops[0].imm
            fallthrough = insn.address + insn.size
            bp = BranchPoint(insn.address, cond, target, fallthrough)
            self.branches.append(bp)
            return bp

        if iid == X86.X86_INS_JMP:
            return None  # unconditional, handled by control flow engine

        if iid in (X86.X86_INS_RET, X86.X86_INS_CALL):
            return None  # terminal or opaque

        # --- FPU ---
        if iid in ALL_FPU_IDS:
            return self._lift_fpu(insn)

        # --- REP string ops ---
        if iid in (X86.X86_INS_STOSB, X86.X86_INS_STOSD, X86.X86_INS_STOSW,
                    X86.X86_INS_MOVSB, X86.X86_INS_MOVSD, X86.X86_INS_MOVSW):
            return None  # model as no-op for branch analysis

        # --- LEAVE ---
        if iid == X86.X86_INS_LEAVE:
            ebp = s.get_reg32("ebp")
            s.set_reg32("esp", ebp)
            val = s.mem_read32(ebp)
            s.set_reg32("ebp", val)
            s.set_reg32("esp", ebp + z3.BitVecVal(4, 32))
            return None

        # --- CBW/CWDE ---
        if iid == X86.X86_INS_CWDE:
            ax = s.get_reg16("eax")
            s.set_reg32("eax", z3.SignExt(16, ax))
            return None

        if iid == X86.X86_INS_CBW:
            al = s.get_reg8l("eax")
            s.set_reg16("eax", z3.SignExt(8, al))
            return None

        # --- BSF/BSR ---
        if iid in (X86.X86_INS_BSF, X86.X86_INS_BSR):
            bits = self._op_size(insn, 0)
            src = self._read_operand(insn, ops[1], bits)
            s.zf = src == z3.BitVecVal(0, bits)
            result = s.fresh_mem_var(bits)
            self._write_operand(insn, ops[0], result, bits)
            return None

        # --- BT/BTS/BTC/BTR ---
        if iid == X86.X86_INS_BT:
            bits = self._op_size(insn, 0)
            base = self._read_operand(insn, ops[0], bits)
            bit_idx = self._read_operand(insn, ops[1], bits)
            masked = bit_idx & z3.BitVecVal(bits - 1, bits)
            shifted = z3.LShR(base, masked)
            s.cf = z3.Extract(0, 0, shifted) == z3.BitVecVal(1, 1)
            return None

        s.unsupported_insns.append(f"{insn.mnemonic} at 0x{insn.address:x}")
        return None

    def _lift_fpu(self, insn) -> Optional[BranchPoint]:
        s = self.state
        iid = insn.id
        ops = insn.operands

        # --- FLD ---
        if iid == X86.X86_INS_FLD:
            if ops[0].type == X86.X86_OP_REG:
                reg_idx = ops[0].reg - X86.X86_REG_ST0
                val = s.fpu_st(reg_idx)
                s.fpu_push(val)
            elif ops[0].type == X86.X86_OP_MEM:
                addr = self._compute_mem_addr(ops[0].mem)
                size = ops[0].size
                if size == 4:
                    val = s.mem_read_fp32(addr)
                elif size == 8:
                    fp64 = s.mem_read_fp64(addr)
                    val = z3.fpToFP(RNE, fp64, FP80)
                else:
                    val = s.mem_read_fp32(addr)
                s.fpu_push(val)
            return None

        # --- FLDZ / FLD1 ---
        if iid == X86.X86_INS_FLDZ:
            s.fpu_push(z3.FPVal(0.0, FP80))
            return None
        if iid == X86.X86_INS_FLD1:
            s.fpu_push(z3.FPVal(1.0, FP80))
            return None

        # --- FST / FSTP ---
        if iid in (X86.X86_INS_FST, X86.X86_INS_FSTP):
            st0 = s.fpu_st(0)
            if ops[0].type == X86.X86_OP_REG:
                reg_idx = ops[0].reg - X86.X86_REG_ST0
                s.fpu_set_st(reg_idx, st0)
            elif ops[0].type == X86.X86_OP_MEM:
                addr = self._compute_mem_addr(ops[0].mem)
                size = ops[0].size
                if size == 4:
                    s.mem_write_fp32(addr, st0)
                elif size == 8:
                    s.mem_write_fp64(addr, st0)
            if iid == X86.X86_INS_FSTP:
                s.fpu_pop()
            return None

        # --- FILD ---
        if iid == X86.X86_INS_FILD:
            addr = self._compute_mem_addr(ops[0].mem)
            size = ops[0].size
            if size == 4:
                ival = s.mem_read32(addr)
                fp = z3.fpSignedToFP(RNE, ival, FP80)
            elif size == 2:
                ival = s.mem_read16(addr)
                ival32 = z3.SignExt(16, ival)
                fp = z3.fpSignedToFP(RNE, ival32, FP80)
            else:
                ival = s.mem_read32(addr)
                fp = z3.fpSignedToFP(RNE, ival, FP80)
            s.fpu_push(fp)
            return None

        # --- FISTP / FIST ---
        if iid in (X86.X86_INS_FISTP, X86.X86_INS_FIST):
            st0 = s.fpu_st(0)
            addr = self._compute_mem_addr(ops[0].mem)
            size = ops[0].size
            bv = z3.fpToSBV(RNE, st0, z3.BitVecSort(32))
            if size == 4:
                s.mem_write32(addr, bv)
            elif size == 2:
                s.mem_write16(addr, z3.Extract(15, 0, bv))
            if iid == X86.X86_INS_FISTP:
                s.fpu_pop()
            return None

        # --- FPU arithmetic ---
        if iid in FPU_ARITH_IDS:
            return self._lift_fpu_arith(insn)

        # --- FCHS ---
        if iid == X86.X86_INS_FCHS:
            s.fpu_set_st(0, z3.fpNeg(s.fpu_st(0)))
            return None

        # --- FABS ---
        if iid == X86.X86_INS_FABS:
            s.fpu_set_st(0, z3.fpAbs(s.fpu_st(0)))
            return None

        # --- FSQRT ---
        if iid == X86.X86_INS_FSQRT:
            s.fpu_set_st(0, z3.fpSqrt(RNE, s.fpu_st(0)))
            return None

        # --- FXCH ---
        if iid == X86.X86_INS_FXCH:
            idx = 1
            if ops and ops[0].type == X86.X86_OP_REG:
                idx = ops[0].reg - X86.X86_REG_ST0
            a = s.fpu_st(0)
            b = s.fpu_st(idx)
            s.fpu_set_st(0, b)
            s.fpu_set_st(idx, a)
            return None

        # --- FCOM / FCOMP / FCOMPP / FUCOM* ---
        if iid in (X86.X86_INS_FCOM, X86.X86_INS_FCOMP, X86.X86_INS_FCOMPP,
                    X86.X86_INS_FUCOM, X86.X86_INS_FUCOMP, X86.X86_INS_FUCOMPP):
            st0 = s.fpu_st(0)
            if ops and ops[0].type == X86.X86_OP_REG:
                idx = ops[0].reg - X86.X86_REG_ST0
                other = s.fpu_st(idx)
            elif ops and ops[0].type == X86.X86_OP_MEM:
                addr = self._compute_mem_addr(ops[0].mem)
                if ops[0].size == 8:
                    fp64 = s.mem_read_fp64(addr)
                    other = z3.fpToFP(RNE, fp64, FP80)
                else:
                    other = s.mem_read_fp32(addr)
            else:
                other = s.fpu_st(1)
            # C3=ZF, C0=CF in FPU status word (set by FNSTSW then TEST)
            # We model the FPU comparison result as flags that get transferred by FNSTSW
            s._fpu_c0 = z3.fpLT(st0, other)
            s._fpu_c2 = z3.BoolVal(False)  # unordered — skip for now
            s._fpu_c3 = z3.fpEQ(st0, other)
            if iid in (X86.X86_INS_FCOMP, X86.X86_INS_FUCOMP):
                s.fpu_pop()
            elif iid in (X86.X86_INS_FCOMPP, X86.X86_INS_FUCOMPP):
                s.fpu_pop()
                s.fpu_pop()
            return None

        # --- FNSTSW ---
        if iid == X86.X86_INS_FNSTSW:
            c0 = getattr(s, '_fpu_c0', z3.BoolVal(False))
            c2 = getattr(s, '_fpu_c2', z3.BoolVal(False))
            c3 = getattr(s, '_fpu_c3', z3.BoolVal(False))
            # AX bits: C0=bit8, C2=bit10, C3=bit14
            ax = z3.BitVecVal(0, 16)
            ax = z3.If(c0, ax | z3.BitVecVal(0x0100, 16), ax)
            ax = z3.If(c2, ax | z3.BitVecVal(0x0400, 16), ax)
            ax = z3.If(c3, ax | z3.BitVecVal(0x4000, 16), ax)
            if ops[0].type == X86.X86_OP_REG:
                s.set_reg16("eax", ax)
            elif ops[0].type == X86.X86_OP_MEM:
                addr = self._compute_mem_addr(ops[0].mem)
                s.mem_write16(addr, ax)
            return None

        # --- FNSTCW / FLDCW ---
        if iid in (X86.X86_INS_FNSTCW, X86.X86_INS_FLDCW):
            return None  # rounding mode changes — ignore for now

        # --- Transcendentals ---
        if iid in FPU_TRANSCENDENTAL_IDS:
            s.has_transcendental = True
            s._fpu_fresh += 1
            fresh = z3.FP(f"{s.prefix}transcendental_{s._fpu_fresh}", FP80)
            if iid == X86.X86_INS_FPTAN:
                s.fpu_set_st(0, fresh)
                s.fpu_push(z3.FPVal(1.0, FP80))
            else:
                s.fpu_set_st(0, fresh)
            return None

        s.unsupported_insns.append(f"{insn.mnemonic} at 0x{insn.address:x}")
        return None

    def _lift_fpu_arith(self, insn) -> None:
        s = self.state
        iid = insn.id
        ops = insn.operands
        mnem = insn.mnemonic
        pop = (mnem.endswith('p') and mnem != 'fcomp' and mnem != 'fcompp'
               and mnem != 'fstp' and mnem != 'fistp')
        reverse = iid in (X86.X86_INS_FSUBR, X86.X86_INS_FSUBRP,
                          X86.X86_INS_FDIVR, X86.X86_INS_FDIVRP)

        if not ops:
            a, b = s.fpu_st(0), s.fpu_st(1)
            dst_idx = 1 if pop else 0
        elif len(ops) == 1 and ops[0].type == X86.X86_OP_MEM:
            addr = self._compute_mem_addr(ops[0].mem)
            if ops[0].size == 8:
                fp64 = s.mem_read_fp64(addr)
                b = z3.fpToFP(RNE, fp64, FP80)
            else:
                b = s.mem_read_fp32(addr)
            a = s.fpu_st(0)
            dst_idx = 0
        elif len(ops) == 2 and ops[0].type == X86.X86_OP_REG and ops[1].type == X86.X86_OP_REG:
            i0 = ops[0].reg - X86.X86_REG_ST0
            i1 = ops[1].reg - X86.X86_REG_ST0
            a = s.fpu_st(i0)
            b = s.fpu_st(i1)
            dst_idx = i0
        else:
            a, b = s.fpu_st(0), s.fpu_st(1)
            dst_idx = 1 if pop else 0

        if reverse:
            a, b = b, a

        base_mnem = mnem.rstrip('p') if pop else mnem
        if base_mnem in ('fadd',):
            result = z3.fpAdd(RNE, a, b)
        elif base_mnem in ('fsub', 'fsubr'):
            result = z3.fpSub(RNE, a, b)
        elif base_mnem in ('fmul',):
            result = z3.fpMul(RNE, a, b)
        elif base_mnem in ('fdiv', 'fdivr'):
            result = z3.fpDiv(RNE, a, b)
        else:
            result = a

        if pop:
            s.fpu_pop()
            if dst_idx > 0:
                dst_idx -= 1
        s.fpu_set_st(dst_idx, result)
        return None

    def lift_block(self, code: bytes, address: int = 0) -> list[BranchPoint]:
        """Lift a basic block of x86 code.

        Returns list of branch points encountered. Stops at RET, JMP,
        CALL, or end of code.
        """
        branches = []
        for insn in self.md.disasm(code, address):
            bp = self.lift_instruction(insn)
            if bp:
                branches.append(bp)
            if insn.id in (X86.X86_INS_RET, X86.X86_INS_JMP, X86.X86_INS_CALL):
                break
        return branches

    def lift_function(self, code: bytes, address: int = 0,
                      max_insns: int = 5000) -> list[BranchPoint]:
        """Lift an entire function following linear control flow.

        Follows unconditional jumps within the code region.
        At conditional branches, records the branch point but continues
        down the fallthrough path (for seed generation, the caller
        will fork the solver at each branch point).

        Returns all branch points discovered.
        """
        all_branches = []
        visited = set()
        pc = address
        code_end = address + len(code)
        count = 0

        while count < max_insns:
            if pc in visited or pc < address or pc >= code_end:
                break
            visited.add(pc)

            offset = pc - address
            remaining = code[offset:]
            if not remaining:
                break

            for insn in self.md.disasm(remaining, pc):
                count += 1
                if count > max_insns:
                    break

                bp = self.lift_instruction(insn)
                if bp:
                    all_branches.append(bp)
                    pc = insn.address + insn.size
                    break

                if insn.id == X86.X86_INS_RET:
                    return all_branches

                if insn.id == X86.X86_INS_JMP:
                    if insn.operands[0].type == X86.X86_OP_IMM:
                        pc = insn.operands[0].imm
                    else:
                        return all_branches
                    break

                if insn.id == X86.X86_INS_CALL:
                    pc = insn.address + insn.size
                    break

                pc = insn.address + insn.size
            else:
                break

        return all_branches
