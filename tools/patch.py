#!/usr/bin/env python3
if __name__ == "__main__":
    from check_requirements import check_requirements
    check_requirements()

import itertools
import struct
import subprocess
import os
import logging
import hashlib
import argparse

import pefile
from xbe import Xbe, XbeSection, XbeSectionHeader, XbeKernelImage

import color
from knowledge import KnowledgeBase


log = logging.getLogger(__name__)
root_dir = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))


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


def encode_add_esp_imm8(imm):
    assert -128 <= imm <= 127
    return bytes([0x83, 0xC4, imm & 0xFF])


def encode_call_rel32(src_addr_of_call, target_addr):
    # EIP after the 5-byte call is src_addr_of_call + 5; rel32 = target - (eip_after).
    rel32 = target_addr - (src_addr_of_call + 5)
    assert -(2 ** 31) <= rel32 < 2 ** 31
    return b'\xe8' + struct.pack('<i', rel32)


def generate_reverse_thunk(sym, impl_addr, rvthunk_addr):
    """Emit a naked register-to-cdecl trampoline for an implemented @<reg> function.

    Strategy: route every register arg through caller-saved scratch registers
    (EAX, ECX) so callee-saved regs (ESI/EDI/EBX/EBP) are never modified —
    cdecl requires we preserve them across the call. The original return
    address must stay on the stack throughout the call; keeping it in a
    caller-saved register like EDX is unsafe because the callee may clobber it.
    That clobber is normal lifted-C behavior: once we call a ported C body, the
    compiler is free to reuse EAX/ECX/EDX across its own subcalls and temporaries.

    Layout:
      1. Rotate the original return address underneath the existing stack args.
      2. Move/widen each register arg into its assigned scratch slot (EAX
         for arg 0, ECX for arg 1).
      3. Push scratch slots in reverse param order (so arg 0 ends up on top,
         which is where cdecl's first arg lives).
      4. Call impl (rel32).
      5. Clean up the N injected dwords with add esp.
      6. Rotate the original return address back to the top and ret."""
    reg_args = sym.register_args
    assert reg_args, "generate_reverse_thunk called on non-register-arg function"

    # Up to 2 register args supported.
    SCRATCH_FOR_ARG = ['eax', 'ecx']
    assert len(reg_args) <= len(SCRATCH_FOR_ARG), \
        f'rvthunk supports at most {len(SCRATCH_FOR_ARG)} register args'

    # Verify reg args occupy contiguous param indices starting at 0 — required
    # so injected args land in the right cdecl slots.
    for i, (param_idx, _) in enumerate(reg_args):
        assert param_idx == i, \
            'register args must be the first parameters in declaration order'

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

    # 1. Rotate the original ret address below the existing stack args.
    # Keep it on the stack instead of a caller-saved register so the ported C
    # body can freely use EAX/ECX/EDX without corrupting the eventual RET.
    if stack_arg_count > 0:
        code += encode_mov_r32_mesp('edx', 0)
        for i in range(stack_arg_count):
            code += encode_mov_r32_mesp('eax', 4 * (i + 1))
            code += encode_mov_mesp_r32(4 * i, 'eax')
        code += encode_mov_mesp_r32(4 * stack_arg_count, 'edx')

    # 2. Stage each register arg into its scratch slot, widening as needed.
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

    # 3. Push scratch slots in reverse so the lowest-index arg is on top.
    for i in reversed(range(len(reg_args))):
        code += encode_push_r32(SCRATCH_FOR_ARG[i])

    # 4. Call impl. rel32 computed from address of the call instruction itself.
    call_site = rvthunk_addr + len(code)
    code += encode_call_rel32(call_site, impl_addr)

    # 5. Clean up our injected args.
    code += encode_add_esp_imm8(4 * len(reg_args))

    # 6. Rotate the original ret address back to the top and return.
    if stack_arg_count > 0:
        code += encode_mov_r32_mesp('edx', 4 * stack_arg_count)
        for i in reversed(range(stack_arg_count)):
            code += encode_mov_r32_mesp('eax', 4 * i)
            code += encode_mov_mesp_r32(4 * (i + 1), 'eax')
        code += encode_mov_mesp_r32(0, 'edx')
    code += b'\xc3'  # ret

    return bytes(code)


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
    ap.add_argument('input_xbe', help='Original input XBE path')
    ap.add_argument('input_exe', help='Re-implementation EXE path')
    ap.add_argument('output_xbe', help='Output XBE path')
    args = ap.parse_args()

    if not os.path.isfile(args.input_xbe):
        log.error('Could not find input XBE %s', args.input_xbe)
        exit(1)
    log.info('Original XBE: %s', args.input_xbe)

    kb = KnowledgeBase.deserialize()

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
                if name not in kb.name_to_addr:
                    log.error('Where is "%s" in the original XBE?', name)
                    exit(1)
                addr_of_original_function_in_xbe = kb.name_to_addr[name]
                log.info('Patching EXE import of XBE symbol "%s" at %x with %x', name, i.address, addr_of_original_function_in_xbe)
                write_to_vaddr(xbe, i.address, struct.pack('<I', addr_of_original_function_in_xbe))

    # Gather EXE exports
    log.debug('All EXE exports:')
    export_name_to_addr = {}
    for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        name = exp.name.decode('ascii')
        addr = base_addr + exp.address
        log.debug('- %s @ %x', name, addr)
        export_name_to_addr[name] = addr

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


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, handlers=[color.ColorLogHandler()])
    main()
