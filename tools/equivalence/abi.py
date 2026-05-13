"""ABI / calling-convention argument marshaling for x86 Unicorn emulation.

Handles:
  - __cdecl  (default): args pushed right-to-left on stack
  - __stdcall: same as cdecl for callers; callee cleans stack (we just check
    ESP delta after return)
  - __fastcall: first two args (int-sized) in ECX, EDX; rest on stack
  - @<reg> annotations from kb.json: named arg goes in the specified register

Pointer args (type contains '*') are placed in the scratch buffer.
Float args (type 'float') are pushed as 4-byte IEEE-754 little-endian words.
Double args are pushed as 8-byte little-endian words.
Int-sized args (int, uint32_t, int32_t, bool, int16_t, etc.) are pushed as
32-bit integers (sign-extended or zero-extended as appropriate — for testing
purposes we use a 32-bit word regardless).
"""

import re
import struct
from dataclasses import dataclass, field
from typing import Any


# ---------------------------------------------------------------------------
# Unicorn register IDs — imported lazily to keep module importable without UC
# ---------------------------------------------------------------------------
def _uc_regs():
    from unicorn.x86_const import (
        UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
        UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EBP,
    )
    return {
        "eax": UC_X86_REG_EAX, "ax": UC_X86_REG_EAX,
        "ebx": UC_X86_REG_EBX, "bx": UC_X86_REG_EBX,
        "ecx": UC_X86_REG_ECX, "cx": UC_X86_REG_ECX,
        "edx": UC_X86_REG_EDX, "dx": UC_X86_REG_EDX,
        "esi": UC_X86_REG_ESI, "si": UC_X86_REG_ESI,
        "edi": UC_X86_REG_EDI, "di": UC_X86_REG_EDI,
        "esp": UC_X86_REG_ESP,
        "ebp": UC_X86_REG_EBP,
    }


@dataclass
class Param:
    """Parsed parameter descriptor."""
    name: str
    c_type: str        # raw C type string
    reg: str           # register name from @<reg> annotation, or "" for stack
    is_float: bool
    is_double: bool
    is_pointer: bool
    is_int64: bool     # int64_t / uint64_t: pushed as two 32-bit words


def _strip_annotations(decl: str) -> str:
    """Remove @<reg> annotations from a declaration string."""
    return re.sub(r'\s*@\s*<\w+>', '', decl)


def _parse_params(decl: str) -> list[Param]:
    """Very lightweight C parameter parser.

    Extracts (name, type, @<reg>) tuples from a cdecl/stdcall/fastcall
    function declaration.  Handles common types used in this codebase:
    int, int16_t, int32_t, uint32_t, int64_t, uint64_t, float, double,
    bool, char, void*, and pointers.

    Does NOT handle function pointer parameters — those are treated as void*.
    """
    # Extract content inside the outermost parentheses
    m = re.search(r'\(([^)]*)\)\s*;?\s*$', decl)
    if not m:
        return []
    params_str = m.group(1).strip()
    if not params_str or params_str in ('void', ''):
        return []

    params = []
    # Split on commas that are NOT inside parentheses (for fn pointers)
    depth = 0
    parts = []
    current = []
    for ch in params_str:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            parts.append(''.join(current).strip())
            current = []
        else:
            current.append(ch)
    if current:
        parts.append(''.join(current).strip())

    for part in parts:
        part = part.strip()
        if not part:
            continue

        # Extract @<reg> annotation
        reg = ""
        reg_m = re.search(r'@\s*<(\w+)>', part)
        if reg_m:
            reg = reg_m.group(1).lower()
            part = re.sub(r'\s*@\s*<\w+>', '', part).strip()

        # Determine type categories
        c_type = part.strip()
        is_pointer = '*' in c_type or 'void *' in c_type
        is_float = bool(re.search(r'\bfloat\b', c_type)) and not is_pointer
        is_double = bool(re.search(r'\bdouble\b', c_type)) and not is_pointer
        is_int64 = bool(re.search(r'\bint64_t\b|\buint64_t\b|\blong long\b', c_type))

        # Extract parameter name (last word before optional array brackets)
        # Handle "void *name", "const float *name", "int name", etc.
        name_m = re.search(r'(\w+)\s*(?:\[\d*\])?\s*$', re.sub(r'\*', ' ', c_type))
        name = name_m.group(1) if name_m else f"p{len(params)}"

        params.append(Param(
            name=name,
            c_type=c_type,
            reg=reg,
            is_float=is_float,
            is_double=is_double,
            is_pointer=is_pointer,
            is_int64=is_int64,
        ))

    return params


def parse_decl(decl: str) -> dict:
    """Parse a kb.json decl string into ABI metadata.

    Returns a dict with:
        conv          calling convention: 'cdecl', 'stdcall', 'fastcall'
        params        list of Param objects
        return_type   'void', 'float', 'int64', or 'int'
        ret_st0       True if return value is in ST0 (float/double)
        ret_edx_eax   True if return value is EDX:EAX (int64)
    """
    conv = 'cdecl'
    if '__stdcall' in decl or 'WINAPI' in decl:
        conv = 'stdcall'
    elif '__fastcall' in decl:
        conv = 'fastcall'

    ret_m = re.match(r'\s*([\w\s\*]+?)\s+(?:__cdecl|__stdcall|__fastcall)?\s*\w+\s*\(', decl)
    ret_type_str = ret_m.group(1).strip() if ret_m else ''

    ret_st0 = bool(re.search(r'\bfloat\b|\bdouble\b', ret_type_str))
    ret_edx_eax = bool(re.search(r'\bint64_t\b|\buint64_t\b|\blong long\b', ret_type_str))
    ret_void = 'void' in ret_type_str and '*' not in ret_type_str
    ret_bits = 32

    if ret_edx_eax:
        ret_bits = 64
    elif ret_st0 or ret_void:
        ret_bits = 0
    elif re.search(r'\b(bool|char|int8_t|uint8_t)\b', ret_type_str):
        ret_bits = 8
    elif re.search(r'\b(short|int16_t|uint16_t)\b', ret_type_str):
        ret_bits = 16

    params = _parse_params(decl)

    return {
        'conv': conv,
        'params': params,
        'return_type': ret_type_str,
        'ret_st0': ret_st0,
        'ret_edx_eax': ret_edx_eax,
        'ret_void': ret_void,
        'ret_bits': ret_bits,
    }


# ---------------------------------------------------------------------------
# Argument setup in Unicorn
# ---------------------------------------------------------------------------

SCRATCH_BASE = 0x10000000   # base address of scratch buffer (mapped externally)
SCRATCH_SIZE  = 0x10000     # 64 KB

# Slot size inside scratch buffer per pointer argument
POINTER_SLOT = 0x400  # 1 KB per pointer param — enough for 256 floats


def setup_args(uc, abi: dict, arg_values: list, scratch_writes: dict):
    """Push arguments onto the Unicorn stack and set register args.

    abi         result from parse_decl()
    arg_values  list of Python int/float values, one per param
    scratch_writes  dict to collect {scratch_offset: bytes} for pointer args
                    (the caller maps and fills scratch before calling this)

    After this call:
    - ESP points to just below the fake return address that was pushed last
    - Register args are set in the appropriate UC registers
    - Pointer args point into the scratch buffer
    """
    from unicorn.x86_const import UC_X86_REG_ESP
    regs = _uc_regs()

    params = abi['params']
    conv = abi['conv']

    # Identify register-bound params (from @<reg> annotations or fastcall)
    # For fastcall: first two int args go in ECX, EDX in that order
    fastcall_regs = ['ecx', 'edx'] if conv == 'fastcall' else []
    fastcall_idx = 0

    stack_args = []   # (bytes_to_push,) in right-to-left order
    reg_args = {}     # reg_name -> value

    scratch_ptr = SCRATCH_BASE  # next available slot in scratch

    for i, (param, value) in enumerate(zip(params, arg_values)):
        if param.is_pointer:
            # Write a dummy scalar or use whatever was passed as bytes
            slot_base = scratch_ptr
            if isinstance(value, (bytes, bytearray)):
                slot_data = bytes(value)[:POINTER_SLOT]
                slot_data = slot_data.ljust(POINTER_SLOT, b'\x00')
            else:
                # Treat as an array of floats / ints
                slot_data = b'\x00' * POINTER_SLOT
            scratch_writes[slot_base - SCRATCH_BASE] = slot_data
            scratch_ptr += POINTER_SLOT
            packed = struct.pack('<I', slot_base)
        elif param.is_float:
            if isinstance(value, float):
                packed = struct.pack('<f', value)
            else:
                packed = struct.pack('<I', value & 0xFFFFFFFF)
        elif param.is_double:
            if isinstance(value, float):
                packed = struct.pack('<d', value)
            else:
                packed = struct.pack('<Q', value & 0xFFFFFFFFFFFFFFFF)
        elif param.is_int64:
            v = value & 0xFFFFFFFFFFFFFFFF
            # Push high word first (stack grows down), then low word
            packed_hi = struct.pack('<I', (v >> 32) & 0xFFFFFFFF)
            packed_lo = struct.pack('<I', v & 0xFFFFFFFF)
            # We'll handle int64 as two entries
            if param.reg:
                reg_args[param.reg] = v & 0xFFFFFFFF  # best-effort
            else:
                # Push high word, then low word (little-endian cdecl)
                stack_args.append(packed_hi)
                stack_args.append(packed_lo)
            continue
        else:
            # integer
            v = value & 0xFFFFFFFF
            packed = struct.pack('<I', v)

        if param.reg:
            # Register arg from @<reg> annotation
            reg_args[param.reg] = struct.unpack('<I', packed[:4])[0]
        elif conv == 'fastcall' and fastcall_idx < len(fastcall_regs) and not param.is_pointer:
            reg_args[fastcall_regs[fastcall_idx]] = struct.unpack('<I', packed[:4])[0]
            fastcall_idx += 1
        else:
            stack_args.append(packed)

    # Push stack args right-to-left (last param first)
    esp = uc.reg_read(UC_X86_REG_ESP)
    for packed in reversed(stack_args):
        esp -= len(packed)
        uc.mem_write(esp, packed)

    uc.reg_write(UC_X86_REG_ESP, esp)

    # Set register args
    for reg_name, val in reg_args.items():
        if reg_name in regs:
            uc.reg_write(regs[reg_name], val)
