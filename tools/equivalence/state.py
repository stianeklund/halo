"""CPU/FPU state capture and comparison for Unicorn emulation.

Captures EAX, EDX, the full x87 FPU register stack (ST0..ST7), and
memory writes to the scratch buffer after function emulation completes.

Comparison uses bit-pattern equality for floats (never IEEE 754 ==).
"""

import struct
from dataclasses import dataclass, field
from typing import Optional


def _float_ulp(a_bits: int, b_bits: int) -> int:
    """Return the ULP distance between two IEEE 754 single-precision values
    given as raw 32-bit bit patterns.  Handles sign changes correctly."""
    if (a_bits & 0x80000000) != (b_bits & 0x80000000):
        if a_bits == 0x80000000 and b_bits == 0:
            return 1
        if a_bits == 0 and b_bits == 0x80000000:
            return 1
        ax = a_bits if (a_bits & 0x80000000) == 0 else (~a_bits + 1) & 0xFFFFFFFF
        bx = b_bits if (b_bits & 0x80000000) == 0 else (~b_bits + 1) & 0xFFFFFFFF
        return ax + bx
    if a_bits < b_bits:
        return b_bits - a_bits
    return a_bits - b_bits


def _st80_to_f32_bits(b):
    """Convert 10-byte x87 extended to 32-bit float bit pattern."""
    if len(b) < 10:
        return 0
    mantissa = int.from_bytes(b[:8], 'little')
    exp_sign = int.from_bytes(b[8:10], 'little')
    sign = exp_sign >> 15
    exp = exp_sign & 0x7FFF
    if exp == 0:
        return sign << 31
    fexp = exp - 16383 + 127
    if fexp <= 0:
        return sign << 31
    if fexp >= 255:
        return 0x7F800000 | (sign << 31)
    fmant = (mantissa >> 40) & 0x7FFFFF
    return (sign << 31) | (fexp << 23) | fmant


def _st80_ulp_distance(a, b):
    """Return ULP distance between two 80-bit x87 extended precision values.

    Each value is 10 bytes LE: bytes[0:8] = 64-bit mantissa, bytes[8:10] = sign+exponent.
    Returns -1 if values have different signs (except +0/-0) or exponents differ by >1.
    """
    if len(a) < 10 or len(b) < 10:
        return -1
    ma = int.from_bytes(a[:8], 'little')
    ea_sign = int.from_bytes(a[8:10], 'little')
    sa = ea_sign >> 15
    ea = ea_sign & 0x7FFF
    mb = int.from_bytes(b[:8], 'little')
    eb_sign = int.from_bytes(b[8:10], 'little')
    sb = eb_sign >> 15
    eb = eb_sign & 0x7FFF
    if ea == 0 and ma == 0 and eb == 0 and mb == 0:
        return 0
    if sa != sb:
        return -1
    if ea == eb:
        return abs(ma - mb)
    if abs(ea - eb) == 1:
        if ea > eb:
            return (1 << 64) - mb + (ma - (1 << 63))
        else:
            return (1 << 64) - ma + (mb - (1 << 63))
    return -1


def _scratch_matches_float(oracle_data: bytes, lifted_data: bytes,
                           float_slot_indices: list,
                           max_ulp: int) -> bool:
    """Compare scratch buffers with ULP tolerance for float pointer params.

    float_slot_indices is a list of scratch slot indices (0-based) whose
    contents should be compared as float32 arrays with ULP tolerance.
    Other slots are compared byte-for-byte.
    """
    from abi import POINTER_SLOT
    if len(oracle_data) != len(lifted_data):
        return False
    n_slots = len(oracle_data) // POINTER_SLOT
    float_slots = set(float_slot_indices or [])
    for slot in range(n_slots):
        start = slot * POINTER_SLOT
        end = start + POINTER_SLOT
        if slot in float_slots:
            for i in range(start, min(end, len(oracle_data)), 4):
                if i + 4 > len(oracle_data):
                    break
                ob = struct.unpack_from('<I', oracle_data, i)[0]
                lb = struct.unpack_from('<I', lifted_data, i)[0]
                if ob != lb and _float_ulp(ob, lb) > max_ulp:
                    return False
        else:
            if oracle_data[start:end] != lifted_data[start:end]:
                return False
    return True


# ---------------------------------------------------------------------------
# Unicorn register IDs (lazy import)
# ---------------------------------------------------------------------------
def _get_regs():
    from unicorn.x86_const import (
        UC_X86_REG_EAX, UC_X86_REG_EDX,
        UC_X86_REG_ST0, UC_X86_REG_ST1, UC_X86_REG_ST2, UC_X86_REG_ST3,
        UC_X86_REG_ST4, UC_X86_REG_ST5, UC_X86_REG_ST6, UC_X86_REG_ST7,
        UC_X86_REG_FPSW, UC_X86_REG_FPCW,
        UC_X86_REG_ESP,
    )
    return {
        'EAX': UC_X86_REG_EAX,
        'EDX': UC_X86_REG_EDX,
        'ST0': UC_X86_REG_ST0,
        'ST1': UC_X86_REG_ST1,
        'ST2': UC_X86_REG_ST2,
        'ST3': UC_X86_REG_ST3,
        'ST4': UC_X86_REG_ST4,
        'ST5': UC_X86_REG_ST5,
        'ST6': UC_X86_REG_ST6,
        'ST7': UC_X86_REG_ST7,
        'FPSW': UC_X86_REG_FPSW,
        'FPCW': UC_X86_REG_FPCW,
        'ESP': UC_X86_REG_ESP,
    }


@dataclass
class MemoryWrite:
    """A single memory write observed during emulation."""
    address: int
    size: int
    value: int


@dataclass
class CPUState:
    """Captured register + memory state after emulation."""
    eax: int = 0
    edx: int = 0
    st: list[bytes] = field(default_factory=lambda: [b'\x00'*10]*8)
    fpsw: int = 0           # FP status word
    fpcw: int = 0           # FP control word
    esp_delta: int = 0      # bytes ESP changed relative to entry
    scratch_data: bytes = b''  # contents of scratch buffer after run
    error: Optional[str] = None
    insn_count: int = 0
    visited_pcs: dict = field(default_factory=dict)
    mem_writes: list = field(default_factory=list)
    global_reads: dict = field(default_factory=dict)
    auto_mapped_pages: set = field(default_factory=set)


def capture(uc, scratch_base: int, scratch_size: int, entry_esp: int) -> CPUState:
    """Read current Unicorn state into a CPUState."""
    regs = _get_regs()
    st = []
    for i in range(8):
        reg_id = regs[f'ST{i}']
        raw = uc.reg_read(reg_id)
        # Unicorn returns ST registers as an integer (80-bit long double)
        # packed into a Python int.  Convert to 10-byte little-endian.
        st.append(raw.to_bytes(10, 'little'))

    state = CPUState(
        eax=uc.reg_read(regs['EAX']),
        edx=uc.reg_read(regs['EDX']),
        st=st,
        fpsw=uc.reg_read(regs['FPSW']),
        fpcw=uc.reg_read(regs['FPCW']),
        esp_delta=uc.reg_read(regs['ESP']) - entry_esp,
        scratch_data=bytes(uc.mem_read(scratch_base, scratch_size)),
    )
    return state


def _fmt_st(b: bytes) -> str:
    """Format 10-byte x87 long double as hex."""
    return b.hex()


def _st_as_float(b: bytes) -> str:
    """Try to decode 10-byte x87 long double to a human-readable float string."""
    # x87 80-bit format: sign(1) exponent(15) mantissa(64)
    # Stored little-endian: bytes[0..7]=mantissa, bytes[8..9]=exponent+sign
    if len(b) < 10:
        return "?"
    try:
        mantissa = int.from_bytes(b[:8], 'little')
        exp_sign = int.from_bytes(b[8:10], 'little')
        sign = -1 if (exp_sign >> 15) else 1
        exp = exp_sign & 0x7FFF
        if exp == 0 and mantissa == 0:
            return "0.0"
        if exp == 0x7FFF:
            if mantissa == 0x8000000000000000:
                return "+inf" if sign > 0 else "-inf"
            return "NaN"
        # Convert: value = sign * mantissa * 2^(exp - 16383 - 63)
        power = exp - 16383 - 63
        val = sign * mantissa * (2.0 ** power)
        return f"{val:.8g}"
    except Exception:
        return "?"


@dataclass
class StateDiff:
    """Differences between two CPUState captures."""
    eax_differs: bool = False
    edx_differs: bool = False
    st_differs: list[int] = field(default_factory=list)  # indices of differing ST regs
    scratch_differs: bool = False
    esp_delta_differs: bool = False
    oracle_esp_delta: int = 0
    lifted_esp_delta: int = 0
    oracle: Optional[CPUState] = None
    lifted: Optional[CPUState] = None

    def has_differences(self) -> bool:
        return (self.eax_differs or self.edx_differs or
                bool(self.st_differs) or self.scratch_differs or
                self.esp_delta_differs)

    def summary(self) -> str:
        parts = []
        if self.eax_differs:
            parts.append(
                f"EAX: oracle=0x{self.oracle.eax:08x} lifted=0x{self.lifted.eax:08x}"
            )
        if self.edx_differs:
            parts.append(
                f"EDX: oracle=0x{self.oracle.edx:08x} lifted=0x{self.lifted.edx:08x}"
            )
        for si in self.st_differs:
            ob = self.oracle.st[si]
            lb = self.lifted.st[si]
            parts.append(
                f"ST{si}: oracle={_fmt_st(ob)} ({_st_as_float(ob)}) "
                f"lifted={_fmt_st(lb)} ({_st_as_float(lb)})"
            )
        if self.scratch_differs:
            parts.append("scratch buffer differs")
        if self.esp_delta_differs:
            parts.append(
                f"ESP delta: oracle={self.oracle_esp_delta} "
                f"lifted={self.lifted_esp_delta}"
            )
        return "; ".join(parts) if parts else "PASS"


def compare(oracle: CPUState, lifted: CPUState,
            check_scratch: bool = True,
            ret_eax: bool = True,
            ret_eax_bits: int = 32,
            ret_edx_eax: bool = False,
            ret_st0: bool = False,
            check_st_count: int = 0,
            scratch_float_tolerance_ulp: int = 0,
            scratch_float_params: list = None,
            st_tolerance_ulp: int = 0,
            st_compare_as_f32: bool = False,
            check_esp: bool = True) -> StateDiff:
    """Compare two CPUState objects and return a StateDiff.

    All comparisons are bit-pattern based (no floating-point ==).

    ret_eax         check EAX (True for int-returning functions)
    ret_eax_bits    number of meaningful low bits in EAX for integer returns
    ret_edx_eax     check EDX:EAX pair (int64 return)
    ret_st0         check ST0 only (float/double return)
    check_st_count  how many ST registers to check (0 = none, 8 = all)
                    Normally 1 for float-returning, 0 for void/int.
    check_scratch   compare scratch buffer byte-for-byte
    scratch_float_tolerance_ulp  if > 0, allow N ULP difference for float
                    values in scratch buffer slots listed in scratch_float_params.
    scratch_float_params  list of slot indices (ints) whose scratch slots
                    contain float arrays (compared with ULP tolerance).
    """
    diff = StateDiff(oracle=oracle, lifted=lifted)

    if ret_eax or ret_edx_eax:
        if ret_eax_bits >= 32:
            eax_mask = 0xFFFFFFFF
        elif ret_eax_bits <= 0:
            eax_mask = 0
        else:
            eax_mask = (1 << ret_eax_bits) - 1
        diff.eax_differs = (oracle.eax & eax_mask) != (lifted.eax & eax_mask)
    if ret_edx_eax:
        diff.edx_differs = oracle.edx != lifted.edx

    # FPU: only compare the number of ST regs that make sense for the ABI.
    # For void functions: 0 (callers don't read ST0).
    # For float/double functions: 1 (ST0 holds return value).
    # Don't compare ST1..ST7 because they may contain garbage from
    # unrelated prior operations that differ between oracle and lifted.
    for i in range(check_st_count):
        if oracle.st[i] != lifted.st[i]:
            if st_compare_as_f32:
                ob = _st80_to_f32_bits(oracle.st[i])
                lb = _st80_to_f32_bits(lifted.st[i])
                if ob == lb or (st_tolerance_ulp > 0 and _float_ulp(ob, lb) <= st_tolerance_ulp):
                    continue
            elif st_tolerance_ulp > 0:
                dist = _st80_ulp_distance(oracle.st[i], lifted.st[i])
                if 0 <= dist <= st_tolerance_ulp:
                    continue
            diff.st_differs.append(i)

    if check_scratch:
        if scratch_float_tolerance_ulp > 0 and scratch_float_params:
            diff.scratch_differs = not _scratch_matches_float(
                oracle.scratch_data, lifted.scratch_data,
                scratch_float_params,
                scratch_float_tolerance_ulp)
        else:
            diff.scratch_differs = oracle.scratch_data != lifted.scratch_data

    if check_esp and oracle.esp_delta != lifted.esp_delta:
        diff.esp_delta_differs = True
        diff.oracle_esp_delta = oracle.esp_delta
        diff.lifted_esp_delta = lifted.esp_delta

    return diff


@dataclass
class TraceDiff:
    """Differences in memory-write traces between oracle and lifted."""
    value_diffs: list = field(default_factory=list)
    oracle_only: list = field(default_factory=list)
    lifted_only: list = field(default_factory=list)

    def has_differences(self) -> bool:
        return bool(self.value_diffs or self.oracle_only or self.lifted_only)

    def summary(self) -> str:
        parts = []
        if self.value_diffs:
            parts.append(f"{len(self.value_diffs)} value diff(s)")
        if self.oracle_only:
            parts.append(f"{len(self.oracle_only)} oracle-only write(s)")
        if self.lifted_only:
            parts.append(f"{len(self.lifted_only)} lifted-only write(s)")
        return "; ".join(parts) if parts else "traces match"


def compare_mem_traces(oracle: CPUState, lifted: CPUState) -> TraceDiff:
    """Compare memory write traces between oracle and lifted.

    Builds final-value maps keyed by (address, size) and diffs them.
    """
    def _build_finals(writes):
        finals = {}
        for w in writes:
            finals[(w.address, w.size)] = w.value
        return finals

    orc = _build_finals(oracle.mem_writes)
    lft = _build_finals(lifted.mem_writes)
    all_keys = set(orc) | set(lft)

    value_diffs = []
    oracle_only = []
    lifted_only = []
    for key in sorted(all_keys):
        ov = orc.get(key)
        lv = lft.get(key)
        if ov is not None and lv is not None:
            if ov != lv:
                value_diffs.append((key[0], ov, lv))
        elif ov is not None:
            oracle_only.append(key[0])
        else:
            lifted_only.append(key[0])

    return TraceDiff(value_diffs=value_diffs, oracle_only=oracle_only,
                     lifted_only=lifted_only)


def format_state_verbose(state: CPUState, label: str) -> str:
    """Return a multi-line human-readable state dump."""
    lines = [f"  [{label}]"]
    lines.append(f"    EAX=0x{state.eax:08x}  EDX=0x{state.edx:08x}")
    for i, b in enumerate(state.st):
        lines.append(f"    ST{i}={_fmt_st(b)}  ({_st_as_float(b)})")
    lines.append(f"    FPSW=0x{state.fpsw:04x}  FPCW=0x{state.fpcw:04x}")
    lines.append(f"    ESP_delta={state.esp_delta}")
    lines.append(f"    INSN_count={state.insn_count}")
    if state.error:
        lines.append(f"    ERROR: {state.error}")
    return '\n'.join(lines)
