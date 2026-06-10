#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

if __name__ == "__main__":
    from audit.check_requirements import check_requirements
    check_requirements()

import bisect
import itertools
import json
import struct
import os
import logging
import hashlib
import argparse
from datetime import datetime

import pefile
from xbe import Xbe, XbeSection, XbeSectionHeader, XbeKernelImage

# Configure logging BEFORE importing modules that use logging
logging.basicConfig(
    level=getattr(logging, os.environ.get('LOG_LEVEL', 'INFO').upper()),
    format='%(levelname)s:%(name)s:%(message)s'
)

from internal import color
from analysis.knowledge import Function, KnowledgeBase


log = logging.getLogger(__name__)
root_dir = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../.."))
KB_REG_BASELINE_PATH = os.path.join(root_dir, 'tools', 'kb_reg_baseline.json')
KB_OVERLAY_ENV = 'HALO_KB_OVERLAY'


EXCEPTION_BUILD_AT_STRING_ADDR = 0x28b6f8
EXCEPTION_BUILD_AT_DATE_OFFSET = 38
EXCEPTION_BUILD_STRING_ADDR = 0x28b5d4
EXCEPTION_BUILD_STRING_DATE_OFFSET = 28
EXCEPTION_BUILD_TIMESTAMP_LENGTH = 20


def normalize_hex_addr(addr):
    if isinstance(addr, str):
        return hex(int(addr, 0))
    return hex(addr)


def format_register_args(reg_args):
    if not reg_args:
        return 'none'
    return ', '.join(f'arg{index}@<{reg}>' for index, reg in reg_args)


def format_register_arg_delta_lines(expected_reg_args, actual_reg_args):
    expected = {f'arg{index}@<{reg}>' for index, reg in expected_reg_args}
    actual = {f'arg{index}@<{reg}>' for index, reg in actual_reg_args}

    removed = sorted(expected - actual)
    added = sorted(actual - expected)
    if not removed and not added:
        return ['(no change)']

    lines = [f'- {entry}' for entry in removed]
    if added:
        lines.extend(f'+ {entry}' for entry in added)
    else:
        lines.append('+ none')
    return lines


def load_reg_annotation_baseline(baseline_path=KB_REG_BASELINE_PATH):
    with open(baseline_path) as f:
        baseline_raw = json.load(f)

    baseline_funcs = baseline_raw.get('functions')
    if not isinstance(baseline_funcs, dict):
        raise ValueError(f'{baseline_path} is missing a top-level "functions" object')

    migrated = []
    for key, val in baseline_raw.items():
        if key in ('functions', 'version'):
            continue
        if isinstance(val, str) and '@<' in val and key not in baseline_funcs:
            baseline_funcs[key] = val
            migrated.append(key)

    if migrated:
        baseline_raw['functions'] = baseline_funcs
        for key in migrated:
            del baseline_raw[key]
        with open(baseline_path, 'w') as f:
            json.dump(baseline_raw, f, indent=2)
            f.write('\n')
        log.warning('Auto-migrated %d baseline entr(y/ies) from top-level into functions dict: %s',
                     len(migrated), ', '.join(migrated))

    for addr, baseline_decl in baseline_funcs.items():
        if not isinstance(baseline_decl, str):
            raise ValueError(f'{baseline_path} entry {addr} must map to a declaration string')
        baseline_func = Function(baseline_decl, addr=normalize_hex_addr(addr))
        if not baseline_func.register_args:
            raise ValueError(f'{baseline_path} entry {addr} has no @<reg> annotation: {baseline_decl}')

    return baseline_funcs


def _overlay_selector_keys(sym):
    keys = {sym.name}
    if getattr(sym, 'addr', None) is not None:
        keys.add(hex(sym.addr))
        keys.add(f'0x{sym.addr:08x}')
        keys.add(f'FUN_{sym.addr:08x}')
    return {k.lower() for k in keys if k}


def apply_kb_overlay(kb, overlay_path):
    """Apply temporary metadata overrides without editing kb.json."""
    with open(overlay_path, encoding='utf-8') as f:
        overlay = json.load(f)

    functions = overlay.get('functions', overlay)
    if not isinstance(functions, dict):
        raise ValueError(f'{overlay_path}: expected object or functions object')

    by_selector = {}
    for sym in kb.symbols:
        if isinstance(sym, Function):
            for key in _overlay_selector_keys(sym):
                by_selector.setdefault(key, []).append(sym)

    applied = []
    for selector, value in functions.items():
        if isinstance(value, dict):
            ported = value.get('ported')
        else:
            ported = value
        if not isinstance(ported, bool):
            raise ValueError(f'{overlay_path}: {selector} must set boolean ported')

        matches = by_selector.get(str(selector).lower(), [])
        if not matches:
            raise ValueError(f'{overlay_path}: no function matches {selector}')
        for sym in matches:
            sym.ported = ported
            applied.append(f'{sym.name}:ported={ported}')

    log.info('Applied kb overlay %s: %s', overlay_path, ', '.join(applied))


def find_reg_annotation_mismatches(kb, baseline_path=KB_REG_BASELINE_PATH, baseline_funcs=None):
    if baseline_funcs is None:
        baseline_funcs = load_reg_annotation_baseline(baseline_path)

    current_symbols = {
        normalize_hex_addr(symbol.addr): symbol
        for symbol in kb.symbols
        if getattr(symbol, 'addr', None) is not None
    }

    mismatches = []
    for addr, baseline_decl in sorted(baseline_funcs.items(), key=lambda item: int(item[0], 16)):
        normalized_addr = normalize_hex_addr(addr)
        baseline_func = Function(baseline_decl, addr=normalized_addr)
        expected_reg_args = baseline_func.register_args

        current_symbol = current_symbols.get(normalized_addr)
        current_decl = current_symbol.decl if current_symbol is not None else None
        actual_reg_args = current_symbol.register_args if isinstance(current_symbol, Function) else []

        if current_decl is None or not isinstance(current_symbol, Function) or actual_reg_args != expected_reg_args:
            mismatches.append({
                'addr': normalized_addr,
                'name': baseline_func.name,
                'baseline_decl': baseline_decl,
                'expected_reg_args': expected_reg_args,
                'current_decl': current_decl,
                'actual_reg_args': actual_reg_args,
            })

    return mismatches


def validate_reg_annotation_baseline_completeness(kb, baseline_funcs, baseline_path=KB_REG_BASELINE_PATH):
    missing = []
    for symbol in kb.symbols:
        if not isinstance(symbol, Function) or symbol.addr is None or not symbol.register_args:
            continue
        normalized_addr = normalize_hex_addr(symbol.addr)
        if normalized_addr not in baseline_funcs:
            missing.append((normalized_addr, symbol.decl))

    if missing:
        detail = ', '.join(f'{addr} ({decl})' for addr, decl in missing)
        raise ValueError(
            f'{baseline_path} is missing {len(missing)} current @<reg> function(s): {detail}')


def validate_reg_arg_annotations(kb, baseline_path=KB_REG_BASELINE_PATH):
    try:
        baseline_funcs = load_reg_annotation_baseline(baseline_path)
        validate_reg_annotation_baseline_completeness(kb, baseline_funcs, baseline_path)
        mismatches = find_reg_annotation_mismatches(kb, baseline_path, baseline_funcs)
    except FileNotFoundError:
        log.error('Missing register-arg baseline: %s', baseline_path)
        exit(1)
    except json.JSONDecodeError as e:
        log.error('Invalid register-arg baseline %s: %s', baseline_path, e)
        exit(1)
    except ValueError as e:
        log.error('Register-arg baseline validation setup failed: %s', e)
        exit(1)

    if not mismatches:
        return

    log.error('Register-arg baseline mismatch detected — %d protected function(s):',
              len(mismatches))
    for mismatch in mismatches:
        log.error('  "%s" @ %s', mismatch['name'], mismatch['addr'])
        log.error('    EXPECTED: %s', format_register_args(mismatch['expected_reg_args']))
        log.error('    BASELINE: %s', mismatch['baseline_decl'])
        if mismatch['current_decl'] is None:
            log.error('    CURRENT: missing from kb.json')
        else:
            log.error('    CURRENT: %s', mismatch['current_decl'])
            log.error('    ACTUAL: %s', format_register_args(mismatch['actual_reg_args']))
        log.error('    DELTA:')
        for line in format_register_arg_delta_lines(
                mismatch['expected_reg_args'], mismatch['actual_reg_args']):
            log.error('      %s', line)
        log.error('    FIX: Restore the original @<reg> annotation layout from tools/kb_reg_baseline.json in kb.json.')
    exit(1)


def format_exception_build_timestamp(now: datetime) -> str:
    month = [
        'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
        'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
    ][now.month - 1]
    return f'{month} {now.day:02d} {now.year:04d} {now.hour:02d}:{now.minute:02d}:{now.second:02d}'


def patch_exception_build_timestamp_strings(xbe: Xbe):
    timestamp = format_exception_build_timestamp(datetime.now())
    encoded = timestamp.encode('ascii')
    assert len(encoded) == EXCEPTION_BUILD_TIMESTAMP_LENGTH

    write_to_vaddr(
        xbe,
        EXCEPTION_BUILD_AT_STRING_ADDR + EXCEPTION_BUILD_AT_DATE_OFFSET,
        encoded)
    write_to_vaddr(
        xbe,
        EXCEPTION_BUILD_STRING_ADDR + EXCEPTION_BUILD_STRING_DATE_OFFSET,
        encoded)
    log.info('Patched exception-screen build timestamp to "%s"', timestamp)


def strip_stdcall_decoration(name: str) -> str:
	"""Strip __stdcall/__fastcall name decoration:
	   _name@N -> name (stdcall)
	   @name@N -> name (fastcall)"""
	if name.startswith('@') and name.count('@') >= 2:
		return name[1:name.rindex('@')]
	if name.startswith('_') and '@' in name:
		return name[1:name.index('@')]
	return name

def round_up(value, round_to):
    value += (round_to - 1)
    value &= ~(round_to - 1)
    return value


# x86-32 register number (r/m or reg field bits) used for ModRM encoding.
REG32_BITS = {'eax': 0, 'ecx': 1, 'edx': 2, 'ebx': 3,
              'esp': 4, 'ebp': 5, 'esi': 6, 'edi': 7}
REG16_BITS = {'ax': 0, 'cx': 1, 'dx': 2, 'bx': 3,
              'sp': 4, 'bp': 5, 'si': 6, 'di': 7}
REG8_BITS = {'al': 0, 'cl': 1, 'dl': 2, 'bl': 3,
             'ah': 4, 'ch': 5, 'dh': 6, 'bh': 7}

# Full-register parent of a 16/8-bit sub-register. Used to pick which 32-bit
# register to widen into and to emit push-full for injection onto the stack.
REG_PARENT = {
    'eax': 'eax', 'ax': 'eax', 'al': 'eax', 'ah': 'eax',
    'ecx': 'ecx', 'cx': 'ecx', 'cl': 'ecx', 'ch': 'ecx',
    'edx': 'edx', 'dx': 'edx', 'dl': 'edx', 'dh': 'edx',
    'ebx': 'ebx', 'bx': 'ebx', 'bl': 'ebx', 'bh': 'ebx',
    'esi': 'esi', 'si': 'esi',
    'edi': 'edi', 'di': 'edi',
}


def encode_push_r32(reg32):
    return bytes([0x50 | REG32_BITS[reg32]])


def encode_pop_r32(reg32):
    return bytes([0x58 | REG32_BITS[reg32]])


def encode_movzx_r32_r16(dst32, src16):
    modrm = 0xC0 | (REG32_BITS[dst32] << 3) | REG16_BITS[src16]
    return bytes([0x0F, 0xB7, modrm])


def encode_movzx_r32_r8(dst32, src8):
    modrm = 0xC0 | (REG32_BITS[dst32] << 3) | REG8_BITS[src8]
    return bytes([0x0F, 0xB6, modrm])


def encode_mov_r32_r32(dst32, src32):
    # mov r/m32, r32 (89 /r)  with mod=11
    modrm = 0xC0 | (REG32_BITS[src32] << 3) | REG32_BITS[dst32]
    return bytes([0x89, modrm])


def encode_mov_r32_mesp(dst32, disp):
    if disp == 0:
        modrm = 0x04 | (REG32_BITS[dst32] << 3)
        return bytes([0x8B, modrm, 0x24])
    if 0 <= disp <= 0x7F:
        modrm = 0x44 | (REG32_BITS[dst32] << 3)
        return bytes([0x8B, modrm, 0x24, disp])
    modrm = 0x84 | (REG32_BITS[dst32] << 3)
    return bytes([0x8B, modrm, 0x24]) + struct.pack('<I', disp)


def encode_mov_mesp_r32(disp, src32):
    if disp == 0:
        modrm = 0x04 | (REG32_BITS[src32] << 3)
        return bytes([0x89, modrm, 0x24])
    if 0 <= disp <= 0x7F:
        modrm = 0x44 | (REG32_BITS[src32] << 3)
        return bytes([0x89, modrm, 0x24, disp])
    modrm = 0x84 | (REG32_BITS[src32] << 3)
    return bytes([0x89, modrm, 0x24]) + struct.pack('<I', disp)


def encode_push_mesp(disp):
    if disp == 0:
        return bytes([0xFF, 0x34, 0x24])
    if 0 <= disp <= 0x7F:
        return bytes([0xFF, 0x74, 0x24, disp])
    return bytes([0xFF, 0xB4, 0x24]) + struct.pack('<I', disp)


def encode_add_esp(imm):
    assert imm >= 0
    if imm <= 0x7F:
        return bytes([0x83, 0xC4, imm])
    return bytes([0x81, 0xC4]) + struct.pack('<I', imm)


def encode_call_rel32(src_addr_of_call, target_addr):
    # EIP after the 5-byte call is src_addr_of_call + 5; rel32 = target - (eip_after).
    rel32 = target_addr - (src_addr_of_call + 5)
    assert -(2 ** 31) <= rel32 < 2 ** 31
    return b'\xe8' + struct.pack('<i', rel32)


def encode_jmp_rel32(src_addr_of_jmp, target_addr):
    # EIP after the 5-byte jmp is src_addr_of_jmp + 5; rel32 = target - (eip_after).
    rel32 = target_addr - (src_addr_of_jmp + 5)
    assert -(2 ** 31) <= rel32 < 2 ** 31
    return b'\xe9' + struct.pack('<i', rel32)


def encode_ret_imm16(imm):
    assert 0 <= imm <= 0xFFFF
    return bytes([0xC2]) + struct.pack('<H', imm)


# Registers the C caller's compiler may keep live values in across a call.
DEACT_CALLEE_SAVED = ('ebx', 'esi', 'edi', 'ebp')


def decl_param_count(decl):
    """Count declared parameters in a kb.json decl (depth-aware comma split,
    so @<reg> annotations and nested function-pointer params don't miscount)."""
    open_paren = decl.find('(')
    close_paren = decl.rfind(')')
    assert open_paren >= 0 and close_paren >= 0
    params_src = decl[open_paren + 1:close_paren].strip()
    if not params_src or params_src == 'void':
        return 0
    depth = 0
    count = 1
    for ch in params_src:
        if ch == '(' or ch == '<':
            depth += 1
        elif ch == ')' or ch == '>':
            depth -= 1
        elif ch == ',' and depth == 0:
            count += 1
    return count


def generate_deactivation_redirect(sym, impl_addr, original_addr):
    """Emit a redirect to splat at the impl entry when ported=false.

    For non-@<reg> functions: a single 5-byte JMP rel32 to original_addr.

    For @<reg> functions the stub must marshal stack args into registers
    WITHOUT clobbering callee-saved registers (EBX/ESI/EDI/EBP): the C
    caller was compiled against a normal cdecl prototype and may keep live
    values in those registers across the call. (A bare `mov ebx,[esp+4];
    jmp original` corrupted FUN_00015520's `state` pointer across a call to
    dormant FUN_000153e0 @<ebx> — ACCESS_VIOLATION reading a datum handle.)

    Layout (CALL-based, not tail-JMP):
      0. PUSH each callee-saved register used for marshaling.
      1. Re-push the caller's entire arg block in reverse order, so the
         original sees the exact same stack-arg layout the caller built
         (register args' slots included, matching the old tail-JMP view).
      2. Load each register arg from the re-pushed block ([esp+4*i]).
      3. CALL original (returns here, not to the C caller).
      4. cdecl original: ADD ESP to drop the re-pushed block; restore saved
         registers; RET — the C caller cleans its own args.
         __stdcall original: its RET N already dropped the re-pushed block;
         restore saved registers; RET imm16 to drop the caller's arg block
         (the caller was compiled callee-cleans).
    """
    reg_args = sym.register_args
    if not reg_args:
        # Plain cdecl/stdcall: 5-byte JMP is enough.
        return encode_jmp_rel32(impl_addr, original_addr)

    param_count = decl_param_count(sym.decl)
    callee_cleans = '__stdcall' in sym.decl

    callee_saved_used = []
    for _, reg in reg_args:
        parent32 = REG_PARENT.get(reg, reg)
        if parent32 not in REG32_BITS:
            raise ValueError(f'Unsupported register {reg!r} in @<reg> annotation')
        if parent32 in DEACT_CALLEE_SAVED and parent32 not in callee_saved_used:
            callee_saved_used.append(parent32)

    code = bytearray()

    # 0. Save callee-saved marshal registers.
    for reg32 in callee_saved_used:
        code += encode_push_r32(reg32)
    save_bytes = 4 * len(callee_saved_used)

    # 1. Re-push the caller's arg block (rightmost first). After the saves,
    #    caller arg i lives at [esp + 4 + 4*i + save_bytes]; each push we
    #    emit shifts subsequent source offsets by another 4.
    pushed = 0
    for i in reversed(range(param_count)):
        disp = 4 + 4 * i + save_bytes + 4 * pushed
        code += encode_push_mesp(disp)
        pushed += 1

    # 2. Load register args from the re-pushed block: arg i at [esp + 4*i].
    #    Sources are stack slots, so load order can't clobber anything.
    #    16/8-bit register args: load the full 32-bit parent; the original
    #    reads only the low slice.
    for param_idx, reg in reg_args:
        parent32 = REG_PARENT.get(reg, reg)
        code += encode_mov_r32_mesp(parent32, 4 * param_idx)

    # 3. Call the original.
    call_site = impl_addr + len(code)
    code += encode_call_rel32(call_site, original_addr)

    # 4. Unwind and return to the C caller.
    if param_count and not callee_cleans:
        code += encode_add_esp(4 * param_count)
    for reg32 in reversed(callee_saved_used):
        code += encode_pop_r32(reg32)
    if callee_cleans and param_count:
        code += encode_ret_imm16(4 * param_count)
    else:
        code += b'\xc3'

    return bytes(code)


def _verify_staging_correctness(sym, reg_args, thunk_code, num_callee_saves=0):
    """Simulate the staging portion of a reverse thunk on a virtual register
    file and verify that each scratch register ends up holding the value that
    was originally in the corresponding source register.

    This catches any register clobbering bug regardless of the specific
    register combination — cycles, sub-register widening, arbitrary orderings.
    Runs at build time for every generated reverse thunk.

    num_callee_saves: number of callee-saved PUSH r32 instructions prepended
    before the staging moves.  These are skipped before simulation begins."""
    SCRATCH = ['eax', 'ecx', 'edx']
    ALL_REGS = {'eax': 0, 'ecx': 1, 'edx': 2, 'ebx': 3,
                'esp': 4, 'ebp': 5, 'esi': 6, 'edi': 7}
    PARENT = {'ax': 'eax', 'al': 'eax', 'ah': 'eax',
              'bx': 'ebx', 'bl': 'ebx', 'bh': 'ebx',
              'cx': 'ecx', 'cl': 'ecx', 'ch': 'ecx',
              'dx': 'edx', 'dl': 'edx', 'dh': 'edx',
              'si': 'esi', 'di': 'edi', 'bp': 'ebp'}

    # Initialize virtual registers with unique sentinel values.
    regs = {}
    for name in ALL_REGS:
        regs[name] = f'orig_{name}'

    # Walk thunk bytes, simulating MOV r32,r32 / MOVZX r32,r16 / XCHG r32,r32.
    # Stop at the first PUSH that is not a callee-save prefix (start of arg-push phase).
    r32_by_code = {v: k for k, v in ALL_REGS.items()}
    i = 0

    # Skip the callee-save PUSH r32 prefix (one per callee-save reg arg).
    for cs_idx in range(num_callee_saves):
        assert i < len(thunk_code) and 0x50 <= thunk_code[i] <= 0x57, (
            f'Expected PUSH r32 for callee-save #{cs_idx} in "{sym.decl}", '
            f'got 0x{thunk_code[i]:02x}')
        i += 1

    while i < len(thunk_code):
        b = thunk_code[i]
        if b == 0x50 | 0 or (0x50 <= b <= 0x57):
            break  # PUSH — staging is done
        if b == 0xFF:
            break  # PUSH [esp+disp] — also end of staging

        if b == 0x89 and i + 1 < len(thunk_code):
            # MOV r/m32, r32 — we only handle MOV r32,r32 (mod=11)
            modrm = thunk_code[i + 1]
            if (modrm >> 6) == 3:
                dst = r32_by_code[modrm & 7]
                src = r32_by_code[(modrm >> 3) & 7]
                regs[dst] = regs[src]
                i += 2; continue

        if b == 0x8B and i + 1 < len(thunk_code):
            # MOV r32, r/m32 — handle MOV r32,r32 (mod=11)
            modrm = thunk_code[i + 1]
            if (modrm >> 6) == 3:
                dst = r32_by_code[(modrm >> 3) & 7]
                src = r32_by_code[modrm & 7]
                regs[dst] = regs[src]
                i += 2; continue

        if b == 0x87 and i + 1 < len(thunk_code):
            # XCHG r/m32, r32 — handle r32,r32 (mod=11)
            modrm = thunk_code[i + 1]
            if (modrm >> 6) == 3:
                r1 = r32_by_code[modrm & 7]
                r2 = r32_by_code[(modrm >> 3) & 7]
                regs[r1], regs[r2] = regs[r2], regs[r1]
                i += 2; continue

        if b == 0x0F and i + 1 < len(thunk_code):
            b2 = thunk_code[i + 1]
            if b2 == 0xB7 and i + 2 < len(thunk_code):
                # MOVZX r32, r/m16 — handle r32,r16 (mod=11)
                modrm = thunk_code[i + 2]
                if (modrm >> 6) == 3:
                    dst = r32_by_code[(modrm >> 3) & 7]
                    src = r32_by_code[modrm & 7]
                    regs[dst] = regs[src]  # value transfer (widening)
                    i += 3; continue
            if b2 == 0xB6 and i + 2 < len(thunk_code):
                # MOVZX r32, r/m8 — handle r32,r8 (mod=11)
                modrm = thunk_code[i + 2]
                if (modrm >> 6) == 3:
                    dst = r32_by_code[(modrm >> 3) & 7]
                    src = r32_by_code[modrm & 7]
                    regs[dst] = regs[src]
                    i += 3; continue

        i += 1

    # Verify postcondition: each scratch register holds the original value of
    # its corresponding source register.  Only scratch-slot args (indices 0–2)
    # are staged; callee-save-slot args (indices 3+) are saved by PUSH at
    # thunk entry and are trivially correct.
    scratch_count = min(len(reg_args), len(SCRATCH))
    for idx in range(scratch_count):
        param_idx, src_reg = reg_args[idx]
        scratch = SCRATCH[idx]
        src32 = PARENT.get(src_reg, src_reg)
        expected = f'orig_{src32}'
        actual = regs[scratch]
        if actual != expected:
            raise ValueError(
                f'Reverse thunk staging verification FAILED for "{sym.decl}":\n'
                f'  Scratch {scratch} should hold {expected} but holds {actual}.\n'
                f'  This means register {src_reg} was clobbered before staging.'
            )


def generate_reverse_thunk(sym, impl_addr, rvthunk_addr):
    """Emit a naked register-to-cdecl trampoline for an implemented @<reg> function.

    Strategy for the first 3 register args: stage them through caller-saved
    scratch registers (EAX, ECX, EDX).  For a 4th–6th register arg (which must
    be a 32-bit callee-saved register such as EBX, ESI, or EDI): emit a PUSH
    r32 at thunk entry that simultaneously saves the arg value to the stack and
    satisfies the callee-save ABI contract for the original caller (the C impl
    preserves the register, so POP r32 at exit restores the original arg value,
    which is exactly what the caller placed there).

    Layout:
      0. PUSH each callee-save register arg (4th+ arg, in declaration order).
      1. Move/widen each scratch-slot register arg (1st–3rd) into EAX/ECX/EDX.
      2. Push full argument list in reverse declaration order.
         – Scratch-slot args: PUSH scratch_reg.
         – Callee-save-slot args: PUSH [ESP+disp] from the save block.
         – Stack args: PUSH [ESP+disp] from the original caller's frame.
      3. Call impl (rel32).
      4. Clean up the injected cdecl args.
      5. POP each callee-save register (reverse of step 0 order).
      6. Ret to the original caller using the untouched return address."""
    reg_args = sym.register_args
    assert reg_args, "generate_reverse_thunk called on non-register-arg function"

    # Slots 0–2: caller-saved scratch registers (free to clobber).
    # Slots 3–5: callee-saved registers saved by PUSH at thunk entry.
    # Only full 32-bit registers are supported for callee-save slots (no bx/si).
    SCRATCH_SLOTS = ['eax', 'ecx', 'edx']
    assert len(reg_args) <= 6, \
        f'rvthunk supports at most 6 register args, got {len(reg_args)}'

    # Classify each reg arg into a scratch slot or a callee-save slot.
    reg_param_to_scratch = {}    # param_idx → scratch_reg32
    reg_param_to_saved_slot = {} # param_idx → j (index in callee_save_pushes)
    callee_save_pushes = []      # canonical32 regs pushed at thunk entry, in order

    for i, (param_idx, src_reg) in enumerate(reg_args):
        if i < len(SCRATCH_SLOTS):
            reg_param_to_scratch[param_idx] = SCRATCH_SLOTS[i]
        else:
            assert src_reg in REG32_BITS, (
                f'Callee-save register arg must be a full 32-bit register '
                f'(got @<{src_reg}> in "{sym.decl}")')
            j = len(callee_save_pushes)
            callee_save_pushes.append(src_reg)
            reg_param_to_saved_slot[param_idx] = j

    K = len(callee_save_pushes)

    open_paren = sym.decl.find('(')
    close_paren = sym.decl.rfind(')')
    assert open_paren >= 0 and close_paren >= 0
    params_src = sym.decl[open_paren + 1:close_paren].strip()
    if not params_src or params_src == 'void':
        param_count = 0
    else:
        depth = 0
        buf = []
        params = []
        for ch in params_src:
            if ch == '(' or ch == '<':
                depth += 1
                buf.append(ch)
            elif ch == ')' or ch == '>':
                depth -= 1
                buf.append(ch)
            elif ch == ',' and depth == 0:
                params.append(''.join(buf))
                buf = []
            else:
                buf.append(ch)
        if buf:
            params.append(''.join(buf))
        param_count = len(params)

    stack_arg_count = param_count - len(reg_args)
    assert stack_arg_count >= 0

    code = bytearray()

    # 0. Push each callee-save register arg before any staging moves, so that
    #    their values are on the stack and safe from the staging writes below.
    for reg32 in callee_save_pushes:
        code += encode_push_r32(reg32)

    # 1. Stage each scratch-slot register arg into its scratch slot (EAX/ECX/EDX),
    #    widening as needed.  Must handle conflicts where a source register is
    #    clobbered by an earlier staging move (e.g. ECX→EAX then EAX→ECX loses
    #    EAX).  Strategy: topological sort the moves, breaking cycles with XCHG.

    def _canon32(reg):
        """Return the 32-bit parent of a register name."""
        _parent = {'ax': 'eax', 'al': 'eax', 'ah': 'eax',
                    'bx': 'ebx', 'bl': 'ebx', 'bh': 'ebx',
                    'cx': 'ecx', 'cl': 'ecx', 'ch': 'ecx',
                    'dx': 'edx', 'dl': 'edx', 'dh': 'edx',
                    'si': 'esi', 'di': 'edi', 'bp': 'ebp'}
        return _parent.get(reg, reg)

    moves = []  # (dst32, src_reg, src_reg_canonical32, needs_widen) — scratch-slot args only
    for i, (param_idx, src_reg) in enumerate(reg_args):
        if param_idx not in reg_param_to_scratch:
            continue  # callee-save slot; no staging move needed
        dst32 = reg_param_to_scratch[param_idx]
        src32 = _canon32(src_reg)
        needs_widen = src_reg not in REG32_BITS
        moves.append((dst32, src_reg, src32, needs_widen))

    # Build a dependency graph: move i depends on move j if move j's dst
    # clobbers move i's source.
    n = len(moves)
    emitted = [False] * n
    def _emit_move(idx):
        dst32, src_reg, src32, needs_widen = moves[idx]
        if needs_widen:
            if src_reg in REG16_BITS:
                code.extend(encode_movzx_r32_r16(dst32, src_reg))
            elif src_reg in REG8_BITS:
                code.extend(encode_movzx_r32_r8(dst32, src_reg))
            else:
                raise ValueError(f'Unsupported register {src_reg!r}')
        else:
            if src_reg != dst32:
                code.extend(encode_mov_r32_r32(dst32, src_reg))
        emitted[idx] = True

    changed = True
    while changed:
        changed = False
        for i in range(n):
            if emitted[i]:
                continue
            dst_i, _, src32_i, _ = moves[i]
            if dst_i == src32_i:
                _emit_move(i)
                changed = True
                continue
            blocked = False
            for j in range(n):
                if i == j or emitted[j]:
                    continue
                _, _, src32_j, _ = moves[j]
                if dst_i == src32_j:
                    blocked = True
                    break
            if not blocked:
                _emit_move(i)
                changed = True

    # Break remaining cycles with XCHG.
    # For a cycle of length N, N-1 XCHGs rotate all values into place.
    # Walk the cycle starting from any un-emitted move, chaining XCHGs
    # between the cycle head and each successive member.
    for i in range(n):
        if emitted[i]:
            continue
        # Trace the cycle: follow dst→src links among un-emitted moves.
        cycle = [i]
        cur = i
        while True:
            dst_cur = moves[cur][0]
            nxt = None
            for k in range(n):
                if not emitted[k] and k != cur and _canon32(moves[k][1]) == dst_cur:
                    nxt = k
                    break
            if nxt is None or nxt == i:
                break
            cycle.append(nxt)
            cur = nxt

        if len(cycle) == 1:
            _emit_move(i)
            continue

        for member in cycle:
            if moves[member][3]:
                raise ValueError(f'Cannot XCHG with sub-register {moves[member][1]!r} in cycle')

        # XCHG cycle[0] with each subsequent member.
        head_reg = moves[cycle[0]][0]
        for j in range(1, len(cycle)):
            other_reg = moves[cycle[j]][0]
            code.extend(b'\x87' + bytes([0xc0 | (REG32_BITS[head_reg] << 3) | REG32_BITS[other_reg]]))

        for member in cycle:
            emitted[member] = True

    # 2. Push full arg list in reverse declaration order.
    #    Three sources:
    #      a) Scratch-slot reg args: PUSH scratch_reg.
    #      b) Callee-save-slot reg args: PUSH [ESP+disp] from the save block at
    #         thunk entry.  Save block layout (top of stack = lowest address):
    #           [ESP + 4*(K-1-j) + 4*pushed_count] = callee_save_pushes[j]
    #      c) Stack args from original caller's frame:
    #           [ESP + 4*(1+K+src_slot+pushed_count)]
    #         where +1 skips the return address, +K skips the save block.
    stack_param_to_incoming_slot = {}
    incoming_slot = 0
    for param_idx in range(param_count):
        if param_idx not in reg_param_to_scratch and param_idx not in reg_param_to_saved_slot:
            stack_param_to_incoming_slot[param_idx] = incoming_slot
            incoming_slot += 1

    pushed_count = 0
    for param_idx in reversed(range(param_count)):
        if param_idx in reg_param_to_scratch:
            code += encode_push_r32(reg_param_to_scratch[param_idx])
        elif param_idx in reg_param_to_saved_slot:
            j = reg_param_to_saved_slot[param_idx]
            disp = 4 * (K - 1 - j + pushed_count)
            code += encode_push_mesp(disp)
        else:
            src_slot = stack_param_to_incoming_slot[param_idx]
            # +1 skips return address; +K skips the callee-save save block.
            disp = 4 * (1 + K + src_slot + pushed_count)
            code += encode_push_mesp(disp)
        pushed_count += 1

    # 3. Call impl. rel32 computed from address of the call instruction itself.
    call_site = rvthunk_addr + len(code)
    code += encode_call_rel32(call_site, impl_addr)

    # 4. Clean up injected args, preserving EAX return value.
    code += encode_add_esp(4 * param_count)

    # 5. Restore callee-saved registers (reverse of push order).
    for reg32 in reversed(callee_save_pushes):
        code += encode_pop_r32(reg32)

    # 6. Return to original caller.
    code += b'\xc3'  # ret

    # 7. Verify: simulate the staging code to prove register postconditions.
    _verify_staging_correctness(sym, reg_args, code, num_callee_saves=K)

    return bytes(code)


def _test_reverse_thunks():
    """Self-test reverse thunk byte shape and register-staging properties.

    This runs in the CMake patched_xbe path, so regressions in the thunk
    generator fail at build time rather than at runtime in the XBE.
    """
    from analysis.knowledge import Function

    def _rel32(call_site, target):
        return struct.pack('<i', target - (call_site + 5))

    def _call_offset(code):
        for i in range(len(code)):
            if code[i] == 0xE8 and i + 5 <= len(code):
                return i
        return None

    def _decl_param_count(decl):
        open_paren = decl.find('(')
        close_paren = decl.rfind(')')
        assert open_paren >= 0 and close_paren >= 0
        params_src = decl[open_paren + 1:close_paren].strip()
        if not params_src or params_src == 'void':
            return 0
        depth = 0
        count = 1
        for ch in params_src:
            if ch == '(' or ch == '<':
                depth += 1
            elif ch == ')' or ch == '>':
                depth -= 1
            elif ch == ',' and depth == 0:
                count += 1
        return count

    exact_cases = [
        (
            "void player_reset_action_result(int player_handle@<eax>);",
            0x401000,
            0x650000,
            (
                b'\x50'
                b'\xe8' + _rel32(0x650000 + 1, 0x401000) +
                b'\x83\xc4\x04'
                b'\xc3'
            ),
            "single @<eax> arg minimal thunk",
        ),
        (
            "void players_update_pvs(void *combined_pvs@<edi>, bool local_player_only);",
            0x4086c0,
            0x65f120,
            (
                b'\x89\xf8'
                b'\xff\x74\x24\x04'
                b'\x50'
                b'\xe8' + _rel32(0x65f120 + 7, 0x4086c0) +
                b'\x83\xc4\x08'
                b'\xc3'
            ),
            "single non-scratch register arg plus stack arg",
        ),
        (
            "void director_apply_replay_mode_for_player(char reset_flag@<al>, "
            "int16_t local_player_index@<si>, char mode_flags);",
            0x402000,
            0x651000,
            (
                b'\x0f\xb6\xc0'
                b'\x0f\xb7\xce'
                b'\xff\x74\x24\x04'
                b'\x51'
                b'\x50'
                b'\xe8' + _rel32(0x651000 + 12, 0x402000) +
                b'\x83\xc4\x0c'
                b'\xc3'
            ),
            "sub-register widening plus stack arg",
        ),
        (
            "void game_engine_hud_update_player(int player_handle@<ecx>, "
            "int hud_player@<eax>, int param3@<ebx>);",
            0x200000,
            0x300000,
            (
                b'\x89\xda'
                b'\x87\xc1'
                b'\x52'
                b'\x51'
                b'\x50'
                b'\xe8' + _rel32(0x300000 + 7, 0x200000) +
                b'\x83\xc4\x0c'
                b'\xc3'
            ),
            "regression: ECX/EAX cycle must preserve original EAX",
        ),
        # --- callee-save slot tests (4th+ register arg) ---
        (
            # FUN_00036890 shape: 4 reg args, no staging moves needed (all identity).
            # PUSH EBX saves d.  PUSH [ESP+0] retrieves d for cdecl arg3.
            "void FUN_00036890(int a@<eax>, int b@<ecx>, int c@<edx>, int d@<ebx>);",
            0x401000,
            0x650000,
            (
                b'\x53'                                    # PUSH EBX (save d)
                b'\xff\x34\x24'                           # PUSH [ESP+0] (d)
                b'\x52'                                    # PUSH EDX (c)
                b'\x51'                                    # PUSH ECX (b)
                b'\x50'                                    # PUSH EAX (a)
                b'\xe8' + _rel32(0x650000 + 7, 0x401000) +
                b'\x83\xc4\x10'                           # ADD ESP, 16
                b'\x5b'                                    # POP EBX
                b'\xc3'
            ),
            "4 reg args: 4th arg via callee-save PUSH/POP (FUN_00036890 shape)",
        ),
        (
            # callee-save slot + stack arg: displacement must add K=1 offset.
            "void f(int a@<eax>, int b@<ecx>, int c@<edx>, int d@<ebx>, int e);",
            0x401000,
            0x650000,
            (
                b'\x53'                                    # PUSH EBX (save d)
                b'\xff\x74\x24\x08'                       # PUSH [ESP+8] (e — skip saved EBX + retaddr)
                b'\xff\x74\x24\x04'                       # PUSH [ESP+4] (d — from save block)
                b'\x52'                                    # PUSH EDX (c)
                b'\x51'                                    # PUSH ECX (b)
                b'\x50'                                    # PUSH EAX (a)
                b'\xe8' + _rel32(0x650000 + 12, 0x401000) +
                b'\x83\xc4\x14'                           # ADD ESP, 20
                b'\x5b'                                    # POP EBX
                b'\xc3'
            ),
            "4 reg args + 1 stack arg: K=1 displacement offset",
        ),
        (
            # 3-cycle in scratch slots alongside a callee-save 4th arg.
            # Entry: EAX=c, ECX=a, EDX=b, EBX=d.
            # Scratch staging: XCHG EAX,EDX; XCHG EAX,ECX → EAX=a, ECX=b, EDX=c.
            "void f(int a@<ecx>, int b@<edx>, int c@<eax>, int d@<ebx>);",
            0x401000,
            0x650000,
            (
                b'\x53'                                    # PUSH EBX (save d)
                b'\x87\xc2'                               # XCHG EAX, EDX
                b'\x87\xc1'                               # XCHG EAX, ECX
                b'\xff\x34\x24'                           # PUSH [ESP+0] (d)
                b'\x52'                                    # PUSH EDX (c)
                b'\x51'                                    # PUSH ECX (b)
                b'\x50'                                    # PUSH EAX (a)
                b'\xe8' + _rel32(0x650000 + 11, 0x401000) +
                b'\x83\xc4\x10'                           # ADD ESP, 16
                b'\x5b'                                    # POP EBX
                b'\xc3'
            ),
            "3-cycle in scratch slots + callee-save 4th arg (XCHG interacts correctly)",
        ),
        (
            # ESI as the 4th callee-save arg (not EBX) — verifies parameterisation.
            "void f(int a@<eax>, int b@<ecx>, int c@<edx>, int d@<esi>);",
            0x401000,
            0x650000,
            (
                b'\x56'                                    # PUSH ESI (save d)
                b'\xff\x34\x24'                           # PUSH [ESP+0] (d)
                b'\x52'                                    # PUSH EDX (c)
                b'\x51'                                    # PUSH ECX (b)
                b'\x50'                                    # PUSH EAX (a)
                b'\xe8' + _rel32(0x650000 + 7, 0x401000) +
                b'\x83\xc4\x10'                           # ADD ESP, 16
                b'\x5e'                                    # POP ESI
                b'\xc3'
            ),
            "4 reg args: 4th arg in ESI (not EBX) — callee-save parameterisation",
        ),
    ]

    for decl, impl_addr, rvthunk_addr, expected, desc in exact_cases:
        sym = Function(decl, addr=0x100000)
        actual = generate_reverse_thunk(sym, impl_addr, rvthunk_addr)
        assert actual == expected, (
            f"FAIL: unexpected reverse thunk bytes for {desc}\n"
            f"  expected: {expected.hex(' ')}\n"
            f"  actual:   {actual.hex(' ')}"
        )
        log.info("PASS: exact bytes: %s", desc)

    property_cases = [
        ("int16_t unit_next_weapon_index(int unit_handle@<ebx>, int16_t weapon_index, int16_t direction);",
         "@<ebx> with 2 stack args"),
        ("void director_compute_camera_input(int handle@<eax>, int output);",
         "@<eax> with 1 stack arg"),
        ("int rumble_calculate(int handle@<eax>);",
         "@<eax> with 0 stack args"),
        ("int player_register_machine(int handle@<eax>, int machine);",
         "@<eax> with 1 stack arg"),
        ("void unit_set_animation(int unit_handle@<eax>, int anim_graph_tag_index@<edi>, int16_t animation_index@<bx>);",
         "@<eax>,@<edi>,@<bx> with 0 stack args"),
        ("void hs_report_compile_error(void *error_info, char *error_text@<esi>, char *script_element@<ebx>, void *source_start@<edi>);",
         "non-leading register args with one stack arg"),
        ("void game_engine_hud_update_player(int player_handle@<ecx>, int hud_player@<eax>, int param3@<ebx>);",
         "@<ecx>,@<eax>,@<ebx> cycle"),
        ("void test_two_swap(int a@<eax>, int b@<ecx>);",
         "minimal 2-element swap cycle"),
        ("void test_three_cycle(int a@<ecx>, int b@<edx>, int c@<eax>);",
         "3-element rotation cycle"),
        ("void FUN_00036890(int a@<eax>, int b@<ecx>, int c@<edx>, int d@<ebx>);",
         "4 reg args: 4th via callee-save EBX"),
        ("void f(int a@<eax>, int b@<ecx>, int c@<edx>, int d@<ebx>, int e);",
         "4 reg args + 1 stack arg: K=1 displacement"),
        ("void f(int a@<eax>, int b@<ecx>, int c@<edx>, int d@<esi>);",
         "4 reg args: 4th via callee-save ESI"),
    ]

    def _expected_post_call(sym_):
        """Build expected bytes after CALL: ADD ESP + POPs (callee-saves) + RET."""
        n_params = _decl_param_count(sym_.decl)
        result = encode_add_esp(4 * n_params)
        # Callee-save POPs in reverse push order (args at indices 3+ in order).
        _PARENT = {'ax': 'eax', 'bx': 'ebx', 'cx': 'ecx', 'dx': 'edx',
                   'si': 'esi', 'di': 'edi', 'bp': 'ebp'}
        saved_regs = [_PARENT.get(src_reg, src_reg)
                      for j, (_, src_reg) in enumerate(sym_.register_args) if j >= 3]
        for reg32 in reversed(saved_regs):
            result += encode_pop_r32(reg32)
        return result + b'\xc3'

    for decl, desc in property_cases:
        sym = Function(decl, addr=0x100000)
        code = generate_reverse_thunk(sym, 0x200000, 0x300000)
        call_offset = _call_offset(code)
        assert call_offset is not None, f"FAIL: no CALL found in thunk for {desc}"
        post_call = code[call_offset + 5:]
        expected_post = _expected_post_call(sym)
        assert post_call == expected_post, (
            f"FAIL: post-CALL code mismatch for {desc}\n"
            f"  expected: {expected_post.hex(' ')}\n"
            f"  actual:   {post_call.hex(' ')}"
        )
        log.info("PASS: properties: %s", desc)

    kb = KnowledgeBase.deserialize()
    checked = 0
    for sym in kb.symbols:
        if not isinstance(sym, Function) or not sym.register_args:
            continue
        generate_reverse_thunk(sym, 0x200000, 0x300000)
        checked += 1

    log.info("PASS: generated and verified %d current kb.json @<reg> thunk(s)", checked)

    # --- Deactivation redirect (ported=false impl-entry stub) tests ---
    deact_cases = [
        (
            "void plain_fn(int a, int b);",
            0x650000, 0x150000,
            b'\xe9' + _rel32(0x650000, 0x150000),
            "non-@<reg>: bare 5-byte JMP",
        ),
        (
            "bool FUN_000153e0(int actor_handle@<ebx>);",
            0x6497b0, 0x153e0,
            (
                b'\x53'                  # PUSH EBX (callee-saved marshal reg)
                b'\xff\x74\x24\x08'      # PUSH [ESP+8]      (re-push arg0)
                b'\x8b\x1c\x24'          # MOV EBX, [ESP]
                b'\xe8' + _rel32(0x6497b0 + 8, 0x153e0) +
                b'\x83\xc4\x04'          # ADD ESP, 4
                b'\x5b'                  # POP EBX
                b'\xc3'
            ),
            "@<ebx> arg: EBX preserved (FUN_00015520 flee-state regression)",
        ),
        (
            "void f(int h@<eax>);",
            0x650000, 0x150000,
            (
                b'\xff\x74\x24\x04'      # PUSH [ESP+4]
                b'\x8b\x04\x24'          # MOV EAX, [ESP]
                b'\xe8' + _rel32(0x650000 + 7, 0x150000) +
                b'\x83\xc4\x04'
                b'\xc3'
            ),
            "@<eax> arg: caller-saved, no PUSH/POP wrapper",
        ),
        (
            "void players_update_pvs(void *combined_pvs@<edi>, bool local_player_only);",
            0x650000, 0x150000,
            (
                b'\x57'                  # PUSH EDI
                b'\xff\x74\x24\x0c'      # PUSH [ESP+0xc]    (arg1)
                b'\xff\x74\x24\x0c'      # PUSH [ESP+0xc]    (arg0 after shift)
                b'\x8b\x3c\x24'          # MOV EDI, [ESP]
                b'\xe8' + _rel32(0x650000 + 12, 0x150000) +
                b'\x83\xc4\x08'
                b'\x5f'                  # POP EDI
                b'\xc3'
            ),
            "@<edi> + stack arg: arg block re-pushed, EDI preserved",
        ),
        (
            "int __stdcall f(int a@<ebx>, int b);",
            0x650000, 0x150000,
            (
                b'\x53'
                b'\xff\x74\x24\x0c'
                b'\xff\x74\x24\x0c'
                b'\x8b\x1c\x24'
                b'\xe8' + _rel32(0x650000 + 12, 0x150000) +
                b'\x5b'                  # POP EBX (original's RET N dropped re-push)
                b'\xc2\x08\x00'          # RET 8 (drop caller's arg block)
            ),
            "__stdcall @<ebx>: callee-clean both arg blocks",
        ),
    ]

    for decl, impl_addr, orig_addr, expected, desc in deact_cases:
        sym = Function(decl, addr=orig_addr)
        actual = generate_deactivation_redirect(sym, impl_addr, orig_addr)
        assert actual == expected, (
            f"FAIL: unexpected deactivation stub bytes for {desc}\n"
            f"  expected: {expected.hex(' ')}\n"
            f"  actual:   {actual.hex(' ')}"
        )
        log.info("PASS: deactivation stub: %s", desc)

    deact_checked = 0
    for sym in kb.symbols:
        if not isinstance(sym, Function) or not sym.register_args:
            continue
        generate_deactivation_redirect(sym, 0x650000, sym.addr or 0x150000)
        deact_checked += 1
    log.info("PASS: generated %d current kb.json @<reg> deactivation stub(s)", deact_checked)

    log.info("All reverse thunk self-tests passed.")


def ensure_unique_section_name(xbe, name):
    # FIXME: Section names don't actually need to be unique, but pyxbe uses a dict to track them, so we simply tack
    #        on a number to make it unique.
    base_name = name
    attempt = 1
    while name in xbe.sections:
        name = f'{base_name}_{attempt}'
        attempt += 1
    return name


# FIXME: Move to pyxbe
def write_to_vaddr(xbe: Xbe, vaddr: int, data: bytes):
    found, section = False, None
    for section in xbe.sections.values():
        min_addr = section.header.virtual_addr
        max_addr = min_addr + section.header.virtual_size
        if min_addr <= vaddr < max_addr:
            found = True
            break
    assert found, "Could not find section for patch offset"

    raw_offset = vaddr - section.header.virtual_addr
    new_data = bytearray(section.data)
    new_data[raw_offset:raw_offset + len(data)] = data
    section.data = bytes(new_data)


def main():
    ap = argparse.ArgumentParser(description='Patches re-implementation EXE into original Halo XBE')
    ap.add_argument('input_xbe', nargs='?', help='Original input XBE path')
    ap.add_argument('input_exe', nargs='?', help='Re-implementation EXE path')
    ap.add_argument('output_xbe', nargs='?', help='Output XBE path')
    ap.add_argument('--test-thunks', action='store_true',
                    help='Run reverse thunk self-tests and exit')
    ap.add_argument('--kb-overlay', default=os.environ.get(KB_OVERLAY_ENV, ''),
                    help='Temporary metadata override JSON. Also read from HALO_KB_OVERLAY.')
    args = ap.parse_args()

    if args.test_thunks:
        _test_reverse_thunks()
        return

    if not args.input_xbe or not args.input_exe or not args.output_xbe:
        ap.error('input_xbe, input_exe, and output_xbe are required')

    kb = KnowledgeBase.deserialize()
    if args.kb_overlay:
        apply_kb_overlay(kb, args.kb_overlay)

    if not os.path.isfile(args.input_xbe):
        log.error('Could not find input XBE %s', args.input_xbe)
        exit(1)
    log.info('Original XBE: %s', args.input_xbe)

    # Load xbe
    log.info('Verifying original XBE MD5')
    with open(args.input_xbe, 'rb') as f:
        md5 = hashlib.md5(f.read()).hexdigest()
        if md5 == kb.expected_md5:
            log.info('Ok')
        else:
            log.error('Incorrect MD5 hash of XBE')
            exit(1)

    log.info('Loading original XBE')
    xbe = Xbe.from_file(args.input_xbe)

    # Patch EXE into XBE
    log.info('Loading EXE %s', args.input_exe)
    pe = pefile.PE(args.input_exe)
    log.info('EXE image base %x', pe.OPTIONAL_HEADER.ImageBase)

    # Determine new base address for EXE
    base_addr = round_up(max(s.header.virtual_addr + s.header.virtual_size for s in xbe.sections.values()), 0x1000)

    # Gather EXE exports before mutating the XBE so ABI proofs run against the
    # original binary only.
    log.debug('All EXE exports:')
    export_name_to_addr = {}
    for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        name = exp.name.decode('ascii')
        addr = base_addr + exp.address
        log.debug('- %s @ %x', name, addr)
        export_name_to_addr[name] = addr

    validate_reg_arg_annotations(kb)

    pe_original_base = pe.OPTIONAL_HEADER.ImageBase
    if pe_original_base != base_addr:
        log.info('Rebasing EXE from %x to %x', pe_original_base, base_addr)
        if not pe.has_relocs():
            log.error('EXE was not built with relocation info')
            exit(1)
        pe.relocate_image(base_addr)

    exe_to_xbe_section_map = {}

    # Add EXE header, up to first section start, as a section
    first_section = sorted(pe.sections, key=lambda s: s.VirtualAddress)[0]
    pe_header_data = pe.trim()
    if len(pe_header_data) > first_section.VirtualAddress:
        pe_header_data = pe_header_data[:first_section.VirtualAddress]

    hdr = XbeSectionHeader()
    hdr.flags = XbeSectionHeader.Flags.PRELOAD
    hdr.virtual_addr = base_addr
    hdr.virtual_size = round_up(len(pe_header_data), 0x1000)
    hdr.raw_addr = 0
    hdr.raw_size = len(pe_header_data)

    vstart, size = pe_original_base, len(pe_header_data)
    name = ensure_unique_section_name(xbe, 'hdr.patch')
    log.info('Patching EXE header from [%x:%x) into XBE as section "%s" [%x:%x) length %x' % (
        vstart, vstart + size, name, hdr.virtual_addr, hdr.virtual_addr + hdr.virtual_size, size))
    xbe.sections[name] = XbeSection(name, hdr, pe_header_data)

    thunk_section_bounds = None

    for section in pe.sections:
        name = section.Name.rstrip(b'\x00').decode('ascii')
        if name == '.thunks':
            start = section.VirtualAddress + base_addr
            thunk_section_bounds = (start, start + section.Misc_VirtualSize)
        if name.startswith('/') or name in ['.reloc']:
            log.info('Skipping section "%s"' % name)
            continue
        new_name = ensure_unique_section_name(xbe, name + '.patch')

        hdr = XbeSectionHeader()
        hdr.flags = XbeSectionHeader.Flags.PRELOAD | XbeSectionHeader.Flags.WRITABLE  # FIXME: Writable for import resolve
        flag_text = 'r'
        if section.__dict__.get('IMAGE_SCN_MEM_WRITE', False):
            hdr.flags |= XbeSectionHeader.Flags.WRITABLE
            flag_text += 'w'
        if section.__dict__.get("IMAGE_SCN_MEM_EXECUTE", False):
            hdr.flags |= XbeSectionHeader.Flags.EXECUTABLE
            flag_text += 'x'
        hdr.virtual_addr = section.VirtualAddress + base_addr
        hdr.virtual_size = round_up(section.Misc_VirtualSize, 0x1000)
        hdr.raw_addr = 0
        hdr.raw_size = section.SizeOfRawData

        vstart = section.VirtualAddress + pe_original_base
        log.info('Patching EXE section into XBE:\n'
                 '    "%s" [%x:%x) length %x, raw length %x, flags %s (EXE)\n'
                 '  ->"%s" [%x:%x) length %x (XBE)' % (
                     name, vstart, vstart + section.Misc_VirtualSize, section.Misc_VirtualSize, section.SizeOfRawData,
                     flag_text,
                     new_name, hdr.virtual_addr, hdr.virtual_addr + hdr.virtual_size, hdr.virtual_size))
        data = section.get_data()[:]
        xbe.sections[new_name] = XbeSection(new_name, hdr, data)
        exe_to_xbe_section_map[name] = hdr.virtual_addr

    # Verify sections don't overlap
    for a, b in itertools.combinations(xbe.sections.values(), 2):
        a_start = a.header.virtual_addr
        a_end = a_start + a.header.virtual_size
        b_start = b.header.virtual_addr
        b_end = b_start + b.header.virtual_size
        if not (b_start >= a_end or a_start >= b_end):
            log.error('Section "%s" and "%s" overlap!', a.name, b.name)
            exit(1)

    # Process XBE
    xboxkrnl_import_table_base_addr = 0
    kernel_export_name_to_ordinal = {n: o for o, n in XbeKernelImage.exports.items()}
    existing_kernel_import_ordinals = {kernel_export_name_to_ordinal[n] for n in xbe.kern_imports}

    for de in pe.DIRECTORY_ENTRY_IMPORT:
        image_name = de.dll.decode('ascii')
        if image_name not in ('xboxkrnl.exe', 'halo.xbe'):
            log.error('Unexpected import from image "%s"', image_name)
            exit(1)
        log.info('Processing imports from image "%s"', image_name)

        # Print out xboxkrnl.exe imports (patched at runtime)
        if image_name == 'xboxkrnl.exe':
            for i in de.imports:
                flags = ' ** New **' if i.ordinal not in existing_kernel_import_ordinals else ''
                log.info('EXE imports ordinal %d (%s) at %x%s', i.ordinal, XbeKernelImage.exports[i.ordinal], i.address, flags)
            xboxkrnl_import_table_base_addr = min(i.address for i in de.imports)

        # Patch XBE imports now
        elif image_name == 'halo.xbe':
            for i in de.imports:
                name = i.name.decode('ascii')
                lookup_name = strip_stdcall_decoration(name)
                if lookup_name not in kb.name_to_addr:
                    log.error('Where is "%s" in the original XBE?', name)
                    exit(1)
                addr_of_original_function_in_xbe = kb.name_to_addr[lookup_name]
                log.info('Patching EXE import of XBE symbol "%s" at %x with %x', name, i.address, addr_of_original_function_in_xbe)
                write_to_vaddr(xbe, i.address, struct.pack('<I', addr_of_original_function_in_xbe))

    # Patch special exports
    special_exports = {
        'exe_base': base_addr,
        'exe_import_table': xboxkrnl_import_table_base_addr,
        'original_xbe_entry': xbe.entry_addr
    }
    for n, addr in special_exports.items():
        if n in export_name_to_addr:
            log.info('Patching EXE exported data "%s" at %x with %x', n, export_name_to_addr[n], addr)
            write_to_vaddr(xbe, export_name_to_addr[n], struct.pack('<I', addr))

    # Override the XBE entry point with our new handler
    entry_addr = pe.OPTIONAL_HEADER.AddressOfEntryPoint + base_addr
    log.info('Patching XBE with EXE entry point of %x', entry_addr)
    xbe.header.entry_addr = entry_addr ^ (Xbe.ENTRY_DEBUG if xbe.is_debug else Xbe.ENTRY_RETAIL)
    special_exports['_start'] = entry_addr

    # Hook all functions in the XBE that have been re-implemented
    patch_functions = [n for n in export_name_to_addr if n not in special_exports]

    # Map from (possibly decorated) export name to kb.json symbol name
    export_to_kb_name = {n: strip_stdcall_decoration(n) for n in patch_functions}

    # Resolve each export to its kb.json symbol so we can detect @<reg> funcs.
    name_to_symbol = {}
    for s in kb.symbols:
        name_to_symbol[s.name] = s

    # Guard: every ported=true function must appear in the EXE export table.
    # If a source file is missing from CMakeLists.txt the function compiles to
    # nothing, the XBE patch is silently skipped, and the original (crashing)
    # code runs.  Catch that here so the build fails loudly instead.
    ported_kb_names = {s.name for s in kb.symbols if getattr(s, 'ported', None) is True}
    exported_kb_names = set(export_to_kb_name.values())
    missing_exports = ported_kb_names - exported_kb_names
    if missing_exports:
        for name in sorted(missing_exports):
            log.error(
                'kb.json ported=true for "%s" but symbol absent from EXE exports '
                '— source file may be missing from CMakeLists.txt',
                name)
        exit(1)

    # First pass: build reverse thunks for implemented @<reg> functions. Each
    # rvthunk adapts the original register-arg ABI into cdecl before calling
    # the impl. Placed in a new .rvthunks XBE section after the EXE sections.
    rvthunks_base = round_up(
        max(s.header.virtual_addr + s.header.virtual_size for s in xbe.sections.values()),
        0x1000)
    rvthunks_bytes = bytearray()
    rvthunks_redirect = {}
    for n in patch_functions:
        sym = name_to_symbol.get(export_to_kb_name[n])
        if sym is None or not getattr(sym, 'requires_reg_thunk', False):
            continue
        impl_addr = export_name_to_addr[n]
        if thunk_section_bounds and thunk_section_bounds[0] <= impl_addr < thunk_section_bounds[1]:
            # Still just the forward-thunk weak symbol — no real impl.
            continue
        if getattr(sym, 'ported', None) is False:
            # Deactivated by kb.json; no original→our_impl path needed because
            # the original-address redirect is also skipped below.
            continue
        rvthunk_addr = rvthunks_base + len(rvthunks_bytes)
        rvthunks_bytes += generate_reverse_thunk(sym, impl_addr, rvthunk_addr)
        rvthunks_redirect[n] = rvthunk_addr
        log.info('Reverse thunk for "%s": impl %x, rvthunk %x', n, impl_addr, rvthunk_addr)

    if rvthunks_bytes:
        hdr = XbeSectionHeader()
        hdr.flags = XbeSectionHeader.Flags.PRELOAD | XbeSectionHeader.Flags.EXECUTABLE
        hdr.virtual_addr = rvthunks_base
        hdr.virtual_size = round_up(len(rvthunks_bytes), 0x1000)
        hdr.raw_addr = 0
        hdr.raw_size = len(rvthunks_bytes)
        name = ensure_unique_section_name(xbe, '.rvthunks')
        log.info('Adding .rvthunks section at %x, length %x', hdr.virtual_addr, hdr.raw_size)
        xbe.sections[name] = XbeSection(name, hdr, bytes(rvthunks_bytes))

    deactivated_count = 0
    sorted_export_addrs = sorted(set(export_name_to_addr.values()))
    for n in patch_functions:
        kb_name = export_to_kb_name[n]
        if kb_name not in kb.name_to_addr:
            log.error('Where is "%s" in the original XBE?', n)
            exit(1)
        addr_of_original_in_xbe = kb.name_to_addr[kb_name]
        addr_of_reimplementation = export_name_to_addr[n]
        if thunk_section_bounds and thunk_section_bounds[0] <= addr_of_reimplementation < thunk_section_bounds[1]:
            log.info('Skipping thunk "%s" export', n)
            continue
        sym = name_to_symbol.get(kb_name)
        if sym is not None and getattr(sym, 'ported', None) is False:
            # Deactivation: leave the original XBE bytes intact, and overwrite
            # our impl entry with a redirect to the original. Both original
            # callers (via untouched original_addr) and lifted callers (via
            # JMP at impl entry) end up at original code.
            redirect_bytes = generate_deactivation_redirect(
                sym, addr_of_reimplementation, addr_of_original_in_xbe)
            # The stub overwrites the impl body; make sure it fits before the
            # next exported function so we never splat a neighbor.
            nxt = bisect.bisect_right(sorted_export_addrs, addr_of_reimplementation)
            if nxt < len(sorted_export_addrs):
                room = sorted_export_addrs[nxt] - addr_of_reimplementation
                if len(redirect_bytes) > room:
                    log.error('Deactivation stub for "%s" (%d bytes) exceeds impl room '
                              '(%d bytes to next export at %x)',
                              n, len(redirect_bytes), room, sorted_export_addrs[nxt])
                    exit(1)
            log.info('Deactivating "%s" (kb.json ported=false): impl %x → JMP %x (%d bytes)',
                     n, addr_of_reimplementation, addr_of_original_in_xbe, len(redirect_bytes))
            write_to_vaddr(xbe, addr_of_reimplementation, redirect_bytes)
            deactivated_count += 1
            continue
        hook_target = rvthunks_redirect.get(n, addr_of_reimplementation)
        log.info('Patching "%s" at %x in XBE with redirect to %x%s',
                 n, addr_of_original_in_xbe, hook_target,
                 ' (via rvthunk)' if n in rvthunks_redirect else '')
        patch_bytes = b'\x68' + struct.pack('<I', hook_target) + b'\xc3'  # push addr, ret
        write_to_vaddr(xbe, addr_of_original_in_xbe, patch_bytes)
    if deactivated_count:
        log.info('%d function(s) deactivated via kb.json ported=false', deactivated_count)

    patch_exception_build_timestamp_strings(xbe)

    log.info('Generating patched XBE (%s)...', args.output_xbe)
    with open(args.output_xbe, 'wb') as f:
        f.write(xbe.pack())

    # Generate GDB init scripts
    log.info('Generating GDB init scripts')
    symbol_sections = ' '.join(f'-s {n} {addr:#x}' for n, addr in exe_to_xbe_section_map.items() if
                               n != '.text' and not n.startswith('/'))
    terminal_symbol_command = 'add-symbol-file build/halo ' + hex(exe_to_xbe_section_map['.text']) + ' ' + symbol_sections + '\n'
    clion_exe_path = os.path.abspath(args.input_exe)
    clion_symbol_command = 'add-symbol-file ' + clion_exe_path + ' ' + hex(exe_to_xbe_section_map['.text']) + ' ' + symbol_sections + '\n'
    with open(root_dir + '/.gdbinit', 'w') as f:
        f.write('set arch i386\n')
        f.write('file build/halo\n')
        f.write(terminal_symbol_command)
        f.write('target remote 127.0.0.1:1234\n')
        f.write('set disassembly-flavor intel\n')
        # FIXME: Add other symbols
    with open(root_dir + '/.gdbinit.clion', 'w') as f:
        f.write('set architecture i386\n')
        f.write('set disassembly-flavor intel\n')
        f.write('set breakpoint pending on\n')
        f.write('exec-file ' + clion_exe_path + '\n')
        f.write(clion_symbol_command)
        f.write('target remote 127.0.0.1:1234\n')
        # FIXME: Add other symbols

    log.info('Build complete')
    log.info('- Patched XBE: %s', args.output_xbe)
    from build_hash import print_build_hash
    print_build_hash(args.output_xbe)


if __name__ == '__main__':
    level = getattr(logging, os.environ.get("LOG_LEVEL", "INFO").upper(), logging.INFO)
    logging.basicConfig(level=level, handlers=[color.ColorLogHandler()])
    main()
