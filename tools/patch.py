#!/usr/bin/env python3
if __name__ == "__main__":
    from check_requirements import check_requirements
    check_requirements()

import itertools
import json
import struct
import os
import logging
import hashlib
import argparse
from bisect import bisect_right
from datetime import datetime

import pefile
from xbe import Xbe, XbeSection, XbeSectionHeader, XbeKernelImage

import color
from knowledge import Function, KnowledgeBase


log = logging.getLogger(__name__)
root_dir = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
KB_REG_BASELINE_PATH = os.path.join(root_dir, 'tools', 'kb_reg_baseline.json')


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


REG_ANNOTATION_REMOVAL_REQUIREMENTS = [
    '1. The function itself must be reimplemented and exported by this build.',
    '2. Every original-XBE code reference to the old address must come only from functions that are also patched to C in this build.',
    '3. No raw non-code/data references to the old address may remain in the original XBE.',
    '4. If the build cannot prove (1)-(3) automatically, keep the original @<reg> annotation in kb.json.',
]


def find_reg_annotation_mismatches(kb, baseline_path=KB_REG_BASELINE_PATH):
    with open(baseline_path) as f:
        baseline_raw = json.load(f)

    baseline_funcs = baseline_raw.get('functions')
    if not isinstance(baseline_funcs, dict):
        raise ValueError(f'{baseline_path} is missing a top-level "functions" object')

    current_symbols = {
        normalize_hex_addr(symbol.addr): symbol
        for symbol in kb.symbols
        if getattr(symbol, 'addr', None) is not None
    }

    mismatches = []
    for addr, baseline_decl in sorted(baseline_funcs.items(), key=lambda item: int(item[0], 16)):
        if not isinstance(baseline_decl, str):
            raise ValueError(f'{baseline_path} entry {addr} must map to a declaration string')

        normalized_addr = normalize_hex_addr(addr)
        baseline_func = Function(baseline_decl, addr=normalized_addr)
        expected_reg_args = baseline_func.register_args
        if not expected_reg_args:
            raise ValueError(
                f'{baseline_path} entry {addr} has no @<reg> annotation: {baseline_decl}')

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


def build_function_index(kb):
    functions = sorted(
        [symbol for symbol in kb.symbols if isinstance(symbol, Function) and symbol.addr is not None],
        key=lambda symbol: symbol.addr,
    )
    return [symbol.addr for symbol in functions], functions


def find_containing_function(source_addr, function_starts, functions):
    idx = bisect_right(function_starts, source_addr) - 1
    if idx < 0:
        return None
    return functions[idx]


def import_capstone():
    try:
        from capstone import Cs, CS_ARCH_X86, CS_GRP_CALL, CS_GRP_JUMP, CS_MODE_32
        from capstone.x86 import X86_OP_IMM
    except ImportError:
        log.error('capstone is required for register-arg ABI proof. Please install project requirements via requirements.txt.')
        exit(1)
    return Cs, CS_ARCH_X86, CS_GRP_CALL, CS_GRP_JUMP, CS_MODE_32, X86_OP_IMM


def scan_original_xbe_references(xbe, target_addrs):
    normalized_targets = [normalize_hex_addr(addr) for addr in target_addrs]
    code_refs = {addr: [] for addr in normalized_targets}
    data_refs = {addr: [] for addr in normalized_targets}
    if not normalized_targets:
        return code_refs, data_refs

    Cs, CS_ARCH_X86, CS_GRP_CALL, CS_GRP_JUMP, CS_MODE_32, X86_OP_IMM = import_capstone()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True

    target_values = {int(addr, 16): addr for addr in normalized_targets}
    for section in xbe.sections.values():
        is_executable = bool(section.header.flags & XbeSectionHeader.Flags.EXECUTABLE)
        if is_executable:
            for insn in md.disasm(section.data, section.header.virtual_addr):
                matches = set()
                for operand in getattr(insn, 'operands', []):
                    if operand.type != X86_OP_IMM:
                        continue
                    immediate = operand.imm & 0xFFFFFFFF
                    if immediate in target_values:
                        matches.add(immediate)
                for immediate in matches:
                    ref_kind = 'call' if insn.group(CS_GRP_CALL) else 'jump' if insn.group(CS_GRP_JUMP) else 'imm'
                    code_refs[target_values[immediate]].append({
                        'source_addr': normalize_hex_addr(insn.address),
                        'kind': ref_kind,
                        'text': f'{insn.mnemonic} {insn.op_str}'.strip(),
                        'section_name': section.name,
                    })
            continue

        for target_value, target_addr in target_values.items():
            pattern = struct.pack('<I', target_value)
            search_from = 0
            while True:
                match_index = section.data.find(pattern, search_from)
                if match_index < 0:
                    break
                data_refs[target_addr].append({
                    'address': normalize_hex_addr(section.header.virtual_addr + match_index),
                    'section_name': section.name,
                })
                search_from = match_index + 1

    return code_refs, data_refs


def assess_reg_annotation_mismatch(mismatch, kb, patch_function_names, code_refs, data_refs):
    function_starts, functions = build_function_index(kb)
    patched_names = set(patch_function_names)

    blockers = []
    supporting_refs = []
    if mismatch['name'] not in patched_names:
        blockers.append({
            'kind': 'target_not_patched',
            'message': 'function is not exported from the current EXE build',
        })

    for ref in code_refs:
        source_addr = int(ref['source_addr'], 16)
        owner = find_containing_function(source_addr, function_starts, functions)
        if owner is None:
            blockers.append({
                'kind': 'unknown_code_ref',
                'ref': ref,
            })
            continue

        ref_info = {
            'kind': ref['kind'],
            'ref': ref,
            'owner_name': owner.name,
            'owner_addr': normalize_hex_addr(owner.addr),
        }
        if owner.name in patched_names:
            supporting_refs.append(ref_info)
        else:
            blockers.append({
                'kind': 'unpatched_code_ref',
                'ref': ref,
                'owner_name': owner.name,
                'owner_addr': normalize_hex_addr(owner.addr),
            })

    for ref in data_refs:
        blockers.append({
            'kind': 'data_ref',
            'ref': ref,
        })

    return {
        'safe': not blockers,
        'supporting_refs': supporting_refs,
        'blockers': blockers,
    }


def format_reg_annotation_blocker(blocker):
    if blocker['kind'] == 'target_not_patched':
        return blocker['message']

    if blocker['kind'] == 'unknown_code_ref':
        ref = blocker['ref']
        return f"{ref['kind']} reference at {ref['source_addr']} ({ref['text']}) could not be mapped to a known function"

    if blocker['kind'] == 'unpatched_code_ref':
        ref = blocker['ref']
        return (f"{ref['kind']} reference from {blocker['owner_name']} @ {blocker['owner_addr']} "
                f"via {ref['source_addr']} ({ref['text']})")

    if blocker['kind'] == 'data_ref':
        ref = blocker['ref']
        return f"raw data reference in section {ref['section_name']} at {ref['address']}"

    return str(blocker)


def validate_reg_arg_annotations(kb, xbe, patch_function_names, baseline_path=KB_REG_BASELINE_PATH):
    try:
        mismatches = find_reg_annotation_mismatches(kb, baseline_path)
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

    code_refs_by_addr, data_refs_by_addr = scan_original_xbe_references(
        xbe, [mismatch['addr'] for mismatch in mismatches])

    failures = []
    for mismatch in mismatches:
        verdict = assess_reg_annotation_mismatch(
            mismatch,
            kb,
            patch_function_names,
            code_refs_by_addr.get(mismatch['addr'], []),
            data_refs_by_addr.get(mismatch['addr'], []),
        )
        if verdict['safe']:
            log.info('Original register-arg ABI removal proven safe for "%s" @ %s (%d original code refs, no data refs)',
                     mismatch['name'], mismatch['addr'], len(verdict['supporting_refs']))
            continue
        failures.append((mismatch, verdict))

    if failures:
        log.error('Register-arg baseline mismatch detected — %d function(s) cannot drop the original ABI yet:',
                  len(failures))
        for mismatch, verdict in failures:
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
            log.error('    REQUIREMENTS:')
            for requirement in REG_ANNOTATION_REMOVAL_REQUIREMENTS:
                log.error('      %s', requirement)
            log.error('    BLOCKERS:')
            for blocker in verdict['blockers']:
                log.error('      - %s', format_reg_annotation_blocker(blocker))
            log.error('    FIX: Restore the original @<reg> annotations in kb.json, or satisfy the requirements above so the build can prove the old ABI is no longer reachable.')
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


def generate_reverse_thunk(sym, impl_addr, rvthunk_addr):
    """Emit a naked register-to-cdecl trampoline for an implemented @<reg> function.

    Strategy: stage every register arg through caller-saved scratch registers
    (EAX, ECX, EDX) so callee-saved regs (ESI/EDI/EBX/EBP) are never modified.
    Then rebuild a full cdecl argument list on top of the current stack by
    pushing parameters right-to-left. Register params are pushed from staged
    scratch slots; stack params are copied from their original incoming stack
    locations. This supports any register-param indices, including non-leading
    ones such as `f(a, b@<esi>, c@<ebx>, d)`.

    Layout:
      1. Move/widen each register arg into its assigned scratch slot.
      2. Push full argument list in reverse declaration order.
      3. Call impl (rel32).
      4. Clean up the injected cdecl args.
      5. Ret to the original caller using the untouched return address."""
    reg_args = sym.register_args
    assert reg_args, "generate_reverse_thunk called on non-register-arg function"

    # Up to 3 register args supported via EAX/ECX/EDX scratch staging.
    SCRATCH_FOR_ARG = ['eax', 'ecx', 'edx']
    assert len(reg_args) <= len(SCRATCH_FOR_ARG), \
        f'rvthunk supports at most {len(SCRATCH_FOR_ARG)} register args'

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

    # 1. Stage each register arg into its scratch slot, widening as needed.
    for i, (_, src_reg) in enumerate(reg_args):
        dst32 = SCRATCH_FOR_ARG[i]
        if src_reg in REG32_BITS:
            if src_reg != dst32:
                code += encode_mov_r32_r32(dst32, src_reg)
        elif src_reg in REG16_BITS:
            code += encode_movzx_r32_r16(dst32, src_reg)
        elif src_reg in REG8_BITS:
            code += encode_movzx_r32_r8(dst32, src_reg)
        else:
            raise ValueError(f'Unsupported register {src_reg!r}')

    # 2. Push full arg list in reverse declaration order.
    reg_param_to_scratch = {param_idx: SCRATCH_FOR_ARG[i] for i, (param_idx, _) in enumerate(reg_args)}
    stack_param_to_incoming_slot = {}
    incoming_slot = 0
    for param_idx in range(param_count):
        if param_idx not in reg_param_to_scratch:
            stack_param_to_incoming_slot[param_idx] = incoming_slot
            incoming_slot += 1

    pushed_count = 0
    for param_idx in reversed(range(param_count)):
        if param_idx in reg_param_to_scratch:
            code += encode_push_r32(reg_param_to_scratch[param_idx])
        else:
            src_slot = stack_param_to_incoming_slot[param_idx]
            # +1 skips return address from original caller.
            disp = 4 * (1 + src_slot + pushed_count)
            code += encode_push_mesp(disp)
        pushed_count += 1

    # 3. Call impl. rel32 computed from address of the call instruction itself.
    call_site = rvthunk_addr + len(code)
    code += encode_call_rel32(call_site, impl_addr)

    # 4. Clean up injected args, preserving EAX return value.
    code += encode_add_esp(4 * param_count)

    # 5. Return to original caller.
    code += b'\xc3'  # ret

    return bytes(code)


def _test_reverse_thunks():
    """Self-test: verify generated thunks don't clobber EAX return values
    or register arg sources. Run via `python tools/patch.py --test-thunks`."""
    from knowledge import Function

    # Decode all MOV r32,[esp+disp] and MOV [esp+disp],r32 instructions in
    # the thunk bytes to extract which registers are used as scratch.
    def extract_mov_esp_regs(code_bytes):
        r32_names = {v: k for k, v in REG32_BITS.items()}
        regs_used = set()
        i = 0
        while i < len(code_bytes):
            if code_bytes[i] in (0x89, 0x8B) and i + 2 < len(code_bytes):
                modrm = code_bytes[i + 1]
                mod = (modrm >> 6) & 3
                rm = modrm & 7
                reg = (modrm >> 3) & 7
                if rm == 4 and mod in (0, 1, 2):
                    sib = code_bytes[i + 2] if i + 2 < len(code_bytes) else 0
                    if sib == 0x24:
                        if code_bytes[i] == 0x8B:
                            regs_used.add(r32_names[reg])
                        else:
                            regs_used.add(r32_names[reg])
            i += 1
        return regs_used

    cases = [
        # (decl, description, check)
        ("int16_t unit_next_weapon_index(int unit_handle@<ebx>, int16_t weapon_index, int16_t direction);",
         "@<ebx> with 2 stack args — backward rotation must not use EAX"),
        ("void director_compute_camera_input(int handle@<eax>, int output);",
         "@<eax> with 1 stack arg — forward rotation must not use EAX"),
        ("int rumble_calculate(int handle@<eax>);",
         "@<eax> with 0 stack args — no rotation needed"),
        ("int player_register_machine(int handle@<eax>, int machine);",
         "@<eax> with 1 stack arg — forward rotation must not use EAX"),
        ("void unit_set_animation(int unit_handle@<eax>, int anim_graph_tag_index@<edi>, int16_t animation_index@<bx>);",
         "@<eax>,@<edi>,@<bx> with 0 stack args — 3 reg args, EDX as third scratch"),
        ("void hs_report_compile_error(void *error_info, char *error_text@<esi>, char *script_element@<ebx>, void *source_start@<edi>);",
         "non-leading register args with one stack arg"),
    ]

    for decl, desc in cases:
        sym = Function(decl, addr=0x100000)
        code = generate_reverse_thunk(sym, 0x200000, 0x300000)

        # Find the CALL instruction (E8 xx xx xx xx) — everything after it is
        # the backward rotation + ret. The backward rotation must not use EAX.
        call_offset = None
        for i in range(len(code)):
            if code[i] == 0xE8 and i + 5 <= len(code):
                call_offset = i
                break
        assert call_offset is not None, f"No CALL found in thunk for: {desc}"

        # After CALL: add esp, N (3 bytes) then the backward rotation
        post_call = code[call_offset + 5:]

        # The backward rotation section starts after the `add esp, imm8`
        if len(post_call) > 3 and post_call[0] == 0x83 and post_call[1] == 0xC4:
            rotate_back = post_call[3:]
        else:
            rotate_back = post_call

        # Check: EAX must not appear as a scratch in the backward rotation.
        # Scan for MOV r32,[esp+disp] where r32 is EAX (opcode 8B with reg=0).
        for i in range(len(rotate_back)):
            if rotate_back[i] == 0x8B and i + 2 < len(rotate_back):
                modrm = rotate_back[i + 1]
                reg = (modrm >> 3) & 7
                rm = modrm & 7
                if reg == REG32_BITS['eax'] and rm == 4:
                    assert False, (
                        f"FAIL: backward rotation uses EAX as scratch — "
                        f"this would destroy the return value. Case: {desc}"
                    )

        # Check: if the function has @<eax>, the forward rotation (before CALL)
        # must not use EAX as scratch either.
        reg_arg_sources = {REG_PARENT[r] for _, r in sym.register_args}
        if 'eax' in reg_arg_sources:
            pre_call = code[:call_offset]
            for i in range(len(pre_call)):
                if pre_call[i] == 0x8B and i + 2 < len(pre_call):
                    modrm = pre_call[i + 1]
                    reg = (modrm >> 3) & 7
                    rm = modrm & 7
                    mod = (modrm >> 6) & 3
                    if reg == REG32_BITS['eax'] and rm == 4 and mod in (0, 1, 2):
                        sib = pre_call[i + 2] if i + 2 < len(pre_call) else 0
                        if sib == 0x24:
                            assert False, (
                                f"FAIL: forward rotation uses EAX as scratch "
                                f"for @<eax> function. Case: {desc}"
                            )

        log.info("PASS: %s", desc)

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
    args = ap.parse_args()

    if args.test_thunks:
        _test_reverse_thunks()
        return

    if not args.input_xbe or not args.input_exe or not args.output_xbe:
        ap.error('input_xbe, input_exe, and output_xbe are required')

    kb = KnowledgeBase.deserialize()

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

    patch_function_names = {name for name in export_name_to_addr if name not in {'exe_base', 'exe_import_table', 'original_xbe_entry', '_start'}}
    validate_reg_arg_annotations(kb, xbe, patch_function_names)

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
                lookup_name = name
                # Strip stdcall decoration: _name@N -> name
                if lookup_name.startswith('_') and '@' in lookup_name:
                    lookup_name = lookup_name[1:lookup_name.index('@')]
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

    # Resolve each export to its kb.json symbol so we can detect @<reg> funcs.
    name_to_symbol = {}
    for s in kb.symbols:
        name_to_symbol[s.name] = s

    # First pass: build reverse thunks for implemented @<reg> functions. Each
    # rvthunk adapts the original register-arg ABI into cdecl before calling
    # the impl. Placed in a new .rvthunks XBE section after the EXE sections.
    rvthunks_base = round_up(
        max(s.header.virtual_addr + s.header.virtual_size for s in xbe.sections.values()),
        0x1000)
    rvthunks_bytes = bytearray()
    rvthunks_redirect = {}
    for n in patch_functions:
        sym = name_to_symbol.get(n)
        if sym is None or not getattr(sym, 'requires_reg_thunk', False):
            continue
        impl_addr = export_name_to_addr[n]
        if thunk_section_bounds and thunk_section_bounds[0] <= impl_addr < thunk_section_bounds[1]:
            # Still just the forward-thunk weak symbol — no real impl.
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

    for n in patch_functions:
        if n not in kb.name_to_addr:
            log.error('Where is "%s" in the original XBE?', n)
            exit(1)
        addr_of_original_in_xbe = kb.name_to_addr[n]
        addr_of_reimplementation = export_name_to_addr[n]
        if thunk_section_bounds and thunk_section_bounds[0] <= addr_of_reimplementation < thunk_section_bounds[1]:
            log.info('Skipping thunk "%s" export', n)
            continue
        hook_target = rvthunks_redirect.get(n, addr_of_reimplementation)
        log.info('Patching "%s" at %x in XBE with redirect to %x%s',
                 n, addr_of_original_in_xbe, hook_target,
                 ' (via rvthunk)' if n in rvthunks_redirect else '')
        patch_bytes = b'\x68' + struct.pack('<I', hook_target) + b'\xc3'  # push addr, ret
        write_to_vaddr(xbe, addr_of_original_in_xbe, patch_bytes)

    patch_exception_build_timestamp_strings(xbe)

    log.info('Generating patched XBE (%s)...', args.output_xbe)
    with open(args.output_xbe, 'wb') as f:
        f.write(xbe.pack())

    # Generate GDB init script
    log.info('Generating GDB init script')
    with open(root_dir + '/.gdbinit', 'w') as f:
        f.write('set arch i386\n')
        f.write('add-symbol-file build/halo ' + hex(exe_to_xbe_section_map['.text']) + ' ' +
                ' '.join(f'-s {n} {addr:#x}' for n, addr in exe_to_xbe_section_map.items() if
                         n != '.text' and not n.startswith('/')) + '\n')
        f.write('layout src\n')
        f.write('target remote 127.0.0.1:1234\n')
        f.write('set disassembly-flavor intel\n')
        # FIXME: Add other symbols

    log.info('Build complete')
    log.info('- Patched XBE: %s', args.output_xbe)
    from build_hash import print_build_hash
    print_build_hash(args.output_xbe)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, handlers=[color.ColorLogHandler()])
    main()
