#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Build-time check for known Ghidra decompiler pitfalls in lifted code.

Hazard classes:

1. MSVC intrinsic calls: Ghidra decompiles compiler runtime helpers as normal
   function calls, but they have non-standard ABIs (modify ESP, use FPU stack,
   register-only args). Calling them from C corrupts the stack.

2. Undersized callee buffers: Ghidra may infer a smaller buffer size than the
   callee actually writes. Passing an undersized stack buffer causes overflow.

3. Duplicate arguments: Ghidra loses track of callee-saved registers (EBX, ESI,
   EDI) in long functions and substitutes the wrong variable. This manifests as
   the same expression appearing as two different arguments in a single function
   call — a strong signal of register aliasing confusion.

4. Pointer-as-float: MSVC passes floats via PUSH <dummy>; FSTP [ESP]. Ghidra
   sees the dummy value (often a pointer/LEA) as the argument. This shows up as
   a pointer expression passed to a parameter declared float in the function's
   prototype.

5. Buffer-alias confusion: Ghidra names each stack offset as an independent
   local_XX, even when the offset falls inside a local buffer. After a call
   that takes a buffer pointer, subsequent reads from the buffer may be
   misattributed to a different variable (like a parameter pointer). This check
   flags functions where the same raw hex offset (e.g. + 0x4c) is accessed on
   both a local buffer and a different pointer — a strong signal that one of the
   accesses should be the buffer, not the pointer.

Usage:
    python3 tools/audit/check_lift_hazards.py
    python3 tools/audit/check_lift_hazards.py -q
    python3 tools/audit/check_lift_hazards.py --changed-only
    python3 tools/audit/check_lift_hazards.py --changed-only --cached
"""
import os
import re
import subprocess
import sys

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
SRC_DIR = os.path.join(ROOT_DIR, 'src')

MSVC_INTRINSICS = {
    '1d90e0': ('_chkstk', 'modifies ESP — declare locals normally or use static'),
    '1d9068': ('_ftol2', 'FPU-stack ABI — use (int)float_expr cast'),
    '1dd5c8': ('__SEH_prolog', 'restructures stack frame — use __try/__except or skip'),
    '1dd601': ('__SEH_epilog', 'non-standard return — paired with __SEH_prolog'),
    '1dd620': ('_allmul', '64-bit multiply — use (int64_t)a * b'),
    '1dd660': ('_aullshr', 'register-only ABI — use (uint64_t)val >> shift'),
    '1dd680': ('_aullrem', '64-bit modulo — use (uint64_t)a %% b'),
    '1dd770': ('_aulldiv', '64-bit divide — use (uint64_t)a / b'),
}

INTRINSIC_PATTERN = re.compile(
    r'0x(?:' + '|'.join(MSVC_INTRINSICS.keys()) + r')\b', re.IGNORECASE
)

KNOWN_BUFFER_SIZES = {
    'FUN_0013fc20': (0x88, 'object placement init — memsets 0x88 bytes'),
}

BUFFER_CALL_PATTERN = re.compile(
    r'\b(' + '|'.join(KNOWN_BUFFER_SIZES.keys()) + r')\s*\('
)

ARRAY_DECL_PATTERN = re.compile(
    r'(?:unsigned\s+)?char\s+(\w+)\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]'
)

SUB_ESP_PATTERN = re.compile(
    r'SUB\s+ESP\s*[,=]\s*(0x[0-9a-fA-F]+)', re.IGNORECASE
)
LOCAL_ARRAY_PATTERN = re.compile(
    r'^\s+(?:unsigned\s+)?(?:char|uint8_t|int8_t)\s+\w+\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]',
)
LOCAL_ARRAY16_PATTERN = re.compile(
    r'^\s+(?:unsigned\s+)?(?:short|int16_t|uint16_t)\s+\w+\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]',
)
LOCAL_ARRAY32_PATTERN = re.compile(
    r'^\s+(?:unsigned\s+)?(?:int|int32_t|uint32_t|float)\s+\w+\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]',
)
# Matches struct-typed arrays: `some_struct_t name[N]`.  The size per element
# is resolved at runtime from cs() macros parsed out of project headers.
LOCAL_STRUCT_ARRAY_PATTERN = re.compile(
    r'^\s+(\w+_t)\s+\w+\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]',
)
# cs(TypeName, size) static-assert macro — used to build the struct-size table.
CS_MACRO_PATTERN = re.compile(
    r'\bcs\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\)'
)
LOCAL_SCALAR_PATTERN = re.compile(
    r'^\s+(?:unsigned\s+)?(?:int|int32_t|uint32_t|float|void\s*\*|char\s*\*|int\s*\*|short\s*\*)\s+\w+\s*;',
)
LOCAL_SCALAR16_PATTERN = re.compile(
    r'^\s+(?:unsigned\s+)?(?:short|int16_t|uint16_t)\s+\w+\s*;',
)
LOCAL_SCALAR8_PATTERN = re.compile(
    r'^\s+(?:unsigned\s+)?(?:char|int8_t|uint8_t)\s+\w+\s*;',
)


def check_intrinsics(filepath, content, lines):
    errors = []
    in_block_comment = False
    for lineno, line in enumerate(lines, 1):
        stripped = line.lstrip()
        if in_block_comment:
            if '*/' in line:
                in_block_comment = False
            continue
        if stripped.startswith('//'):
            continue
        if stripped.startswith('/*'):
            if '*/' not in stripped:
                in_block_comment = True
            continue
        if stripped.startswith('* ') or stripped.startswith('*\t') or stripped == '*\n':
            continue
        for m in INTRINSIC_PATTERN.finditer(line):
            addr = m.group(0)[2:].lower()
            name, fix = MSVC_INTRINSICS[addr]
            relpath = os.path.relpath(filepath, ROOT_DIR)
            errors.append(
                f'  {relpath}:{lineno}: call to {name} '
                f'(0x{addr}) — {fix}'
            )
    return errors


def check_buffer_sizes(filepath, content, lines):
    errors = []

    buffers = {}
    for lineno, line in enumerate(lines, 1):
        for m in ARRAY_DECL_PATTERN.finditer(line):
            name = m.group(1)
            size_str = m.group(2)
            size = int(size_str, 16) if size_str.startswith('0x') else int(size_str)
            buffers[name] = (size, lineno)

    for lineno, line in enumerate(lines, 1):
        for m in BUFFER_CALL_PATTERN.finditer(line):
            callee = m.group(1)
            required_size, desc = KNOWN_BUFFER_SIZES[callee]
            rest = line[m.start():]
            for buf_name, (buf_size, decl_line) in buffers.items():
                if buf_name in rest and buf_size < required_size:
                    relpath = os.path.relpath(filepath, ROOT_DIR)
                    errors.append(
                        f'  {relpath}:{lineno}: {buf_name}[{buf_size:#x}] '
                        f'passed to {callee} which needs {required_size:#x} '
                        f'bytes ({desc})'
                    )
    return errors


DECL_H = os.path.join(ROOT_DIR, 'build', 'generated', 'decl.h')

TRIVIAL_ARGS = frozenset((
    '0', '1', '-1', '0.0f', '0.0', 'NULL', 'NONE', 'true', 'false',
))

FUNC_CALL_PATTERN = re.compile(
    r'\b(FUN_[0-9a-fA-F]+|[a-z_][a-z0-9_]*)\s*\('
)


def _extract_args(text, start):
    """Extract comma-separated arguments from text starting at '(' position.

    Handles nested parens and casts but not string literals with commas.
    Returns (args, end_pos) where end_pos is the closing ')' index, or
    (None, None) if parse fails.
    """
    depth = 0
    args = []
    current = []
    i = start
    while i < len(text):
        ch = text[i]
        if ch == '(':
            depth += 1
            if depth > 1:
                current.append(ch)
        elif ch == ')':
            depth -= 1
            if depth == 0:
                arg = ''.join(current).strip()
                if arg:
                    args.append(arg)
                return args, i
            current.append(ch)
        elif ch == ',' and depth == 1:
            arg = ''.join(current).strip()
            if arg:
                args.append(arg)
            current = []
        else:
            current.append(ch)
        i += 1
    return None, None


def check_duplicate_args(filepath, content, lines):
    """Flag calls where the same non-trivial expression appears as multiple args.

    Calls containing ``dup-args-ok`` are suppressed (verified in-place or
    intentional same-arg calls), including multiline call expressions.
    """
    errors = []
    flat = content
    for m in FUNC_CALL_PATTERN.finditer(flat):
        func_name = m.group(1)
        paren_pos = m.end() - 1
        args, end_pos = _extract_args(flat, paren_pos)
        if args is None or len(args) < 2:
            continue
        start_lineno = flat[:m.start()].count('\n') + 1
        end_lineno = flat[:end_pos].count('\n') + 1
        call_lines = lines[start_lineno - 1:end_lineno]
        if any('dup-args-ok' in line for line in call_lines):
            continue
        if func_name.startswith('_mm_'):
            continue
        seen = {}
        for i, arg in enumerate(args):
            if arg in TRIVIAL_ARGS:
                continue
            if len(arg) < 3:
                continue
            if arg in seen:
                lineno = start_lineno
                relpath = os.path.relpath(filepath, ROOT_DIR)
                errors.append(
                    f'  {relpath}:{lineno}: {func_name}() '
                    f'arg {seen[arg]+1} and {i+1} are both '
                    f'"{arg}" — possible register aliasing'
                )
                break
            seen[arg] = i
    return errors


def _parse_decl_params():
    """Parse decl.h to build a map of function name -> list of param type strings."""
    if not os.path.isfile(DECL_H):
        return {}
    decl_re = re.compile(
        r'HFUNC\s+[\w\s\*]+?\s+(\w+)\s*\(([^)]*)\)\s*;'
    )
    params_map = {}
    with open(DECL_H, 'r') as f:
        for line in f:
            m = decl_re.search(line)
            if not m:
                continue
            func = m.group(1)
            raw_params = m.group(2)
            if raw_params.strip() == 'void':
                continue
            ptypes = []
            for p in raw_params.split(','):
                p = p.strip()
                if 'float' in p and '*' not in p:
                    ptypes.append('float')
                elif 'double' in p and '*' not in p:
                    ptypes.append('double')
                elif '*' in p:
                    ptypes.append('ptr')
                else:
                    ptypes.append('int')
            params_map[func] = ptypes
    return params_map


PTR_EXPR = re.compile(
    r'^(?!\*)\([\w\s]*\*\s*\)|^&\w'
)


def check_pointer_as_float(filepath, content, lines, params_map):
    """Flag calls where a pointer expression is passed to a float parameter."""
    if not params_map:
        return []
    errors = []
    flat = content
    for m in FUNC_CALL_PATTERN.finditer(flat):
        func_name = m.group(1)
        if func_name not in params_map:
            continue
        ptypes = params_map[func_name]
        paren_pos = m.end() - 1
        args, _ = _extract_args(flat, paren_pos)
        if args is None:
            continue
        for i, arg in enumerate(args):
            if i >= len(ptypes):
                break
            if ptypes[i] in ('float', 'double') and PTR_EXPR.search(arg):
                lineno = flat[:m.start()].count('\n') + 1
                relpath = os.path.relpath(filepath, ROOT_DIR)
                errors.append(
                    f'  {relpath}:{lineno}: {func_name}() '
                    f'arg {i+1} looks like a pointer but '
                    f'param is {ptypes[i]} — possible '
                    f'push-then-fstp artifact'
                )
    return errors


BUF_WRITE_PATTERN = re.compile(
    r'\*\s*\(\s*[\w\s]*\*\s*\)\s*\(\s*(\w+)\s*\+\s*(0x[0-9a-fA-F]+)\s*\)\s*='
)

PARAM_CAST_READ_PATTERN = re.compile(
    r'=.*\*\s*\(\s*[\w\s]*\*\s*\)\s*\(\s*\(\s*char\s*\*\s*\)\s*(\w+)\s*\+\s*(0x[0-9a-fA-F]+)\s*\)'
)

FUNC_DEF_PATTERN = re.compile(r'^(?:void|int|char|float|short|unsigned|uint\w*|int\w*)\s')

MIN_BUF_SIZE = 0x40
MIN_OFFSET = 0x20
ALIAS_TOLERANCE = 16


FUNC_BOUNDARY = re.compile(r'^}')


def _split_functions(lines):
    """Split file lines into function chunks. Each chunk is (start_line, end_line)
    where line numbers are 1-based. Uses '}' at column 0 as the boundary."""
    chunks = []
    start = 1
    for i, line in enumerate(lines, 1):
        if FUNC_BOUNDARY.match(line) and i > start:
            chunks.append((start, i))
            start = i + 1
    if start <= len(lines):
        chunks.append((start, len(lines)))
    return chunks


def check_buffer_alias(filepath, content, lines):
    """Flag functions where a local buffer is written at an offset AND a
    different pointer is read at a nearby offset — likely buffer-alias
    confusion from Ghidra's independent local_XX naming.

    Only triggers for:
      - Buffers >= 0x40 bytes (small buffers rarely cause aliasing)
      - Offsets >= 0x20 (low offsets are normal struct field accesses)
      - Param read offset falls within the buffer's size
      - Buffer write and param read are in the same function scope
      - Offset difference <= 16 bytes (close enough to be suspicious)
    Suppressed by ``buf-alias-ok`` comment on the read line.
    """
    errors = []
    chunks = _split_functions(lines)

    for chunk_start, chunk_end in chunks:
        buffers = {}
        buf_write_offsets = {}
        param_read_sites = []

        for lineno in range(chunk_start, chunk_end + 1):
            if lineno > len(lines):
                break
            line = lines[lineno - 1]
            stripped = line.lstrip()
            if stripped.startswith('//'):
                continue
            if stripped.startswith('/*') or stripped.startswith('* ') \
               or stripped == '*\n' or stripped == '*':
                continue

            for m in ARRAY_DECL_PATTERN.finditer(line):
                name = m.group(1)
                size_str = m.group(2)
                size = int(size_str, 16) if size_str.startswith('0x') else int(size_str)
                if size >= MIN_BUF_SIZE:
                    buffers[name] = (size, lineno)

            for m in BUF_WRITE_PATTERN.finditer(line):
                var = m.group(1)
                off = int(m.group(2), 16)
                if var in buffers and off >= MIN_OFFSET and off < buffers[var][0]:
                    buf_write_offsets.setdefault(var, set()).add(off)

            if 'buf-alias-ok' in line:
                continue

            for m in PARAM_CAST_READ_PATTERN.finditer(line):
                var = m.group(1)
                off = int(m.group(2), 16)
                if var not in buffers and off >= MIN_OFFSET:
                    param_read_sites.append((var, off, lineno))

        for param_name, p_off, p_line in param_read_sites:
            for buf_name, b_offs in buf_write_offsets.items():
                buf_size = buffers[buf_name][0]
                if p_off >= buf_size:
                    continue
                for b_off in b_offs:
                    if abs(p_off - b_off) <= ALIAS_TOLERANCE:
                        relpath = os.path.relpath(filepath, ROOT_DIR)
                        errors.append(
                            f'  {relpath}:{p_line}: reads {param_name}+{p_off:#x} '
                            f'but {buf_name}[{buf_size:#x}] is written at '
                            f'+{b_off:#x} — verify this isn\'t a buffer field '
                            f'(suppress with buf-alias-ok comment)'
                        )
                        break

    seen = set()
    deduped = []
    for e in errors:
        if e not in seen:
            seen.add(e)
            deduped.append(e)
    return deduped


def _parse_int(s):
    return int(s, 16) if s.startswith('0x') else int(s)


def _find_function_body(lines, func_name):
    """Return (start_line, end_line) 0-indexed for function body, or None."""
    for i, line in enumerate(lines):
        if func_name + '(' in line and not line.lstrip().startswith(('*', '/', '#')):
            for j in range(i, min(i + 5, len(lines))):
                if '{' in lines[j]:
                    depth = 1
                    for k in range(j + 1, len(lines)):
                        depth += lines[k].count('{') - lines[k].count('}')
                        if depth == 0:
                            return (j + 1, k)
                    break
    return None


_struct_sizes_cache = None


def _get_struct_sizes():
    """Return {type_name: byte_size} for all cs()-asserted structs in src/ headers."""
    global _struct_sizes_cache
    if _struct_sizes_cache is not None:
        return _struct_sizes_cache
    sizes = {}
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if not fname.endswith('.h'):
                continue
            try:
                with open(os.path.join(dirpath, fname), 'r', errors='replace') as f:
                    for m in CS_MACRO_PATTERN.finditer(f.read()):
                        sizes[m.group(1)] = _parse_int(m.group(2))
            except OSError:
                pass
    _struct_sizes_cache = sizes
    return sizes


def _sum_locals(lines):
    """Sum declared local variable sizes (bytes) from function body lines."""
    total = 0
    struct_sizes = _get_struct_sizes()
    for line in lines:
        for m in LOCAL_ARRAY_PATTERN.finditer(line):
            total += _parse_int(m.group(1))
        for m in LOCAL_ARRAY16_PATTERN.finditer(line):
            total += _parse_int(m.group(1)) * 2
        for m in LOCAL_ARRAY32_PATTERN.finditer(line):
            total += _parse_int(m.group(1)) * 4
        for m in LOCAL_STRUCT_ARRAY_PATTERN.finditer(line):
            type_name = m.group(1)
            if type_name in struct_sizes:
                total += struct_sizes[type_name] * _parse_int(m.group(2))
        if LOCAL_SCALAR_PATTERN.match(line):
            total += 4
        elif LOCAL_SCALAR16_PATTERN.match(line):
            total += 4  # MSVC aligns shorts to 4
        elif LOCAL_SCALAR8_PATTERN.match(line):
            total += 4  # MSVC aligns chars to 4
    return total


def check_frame_sizes(filepath, content, lines):
    """Check ported functions where SUB ESP,N is documented in a comment.

    Compares the original MSVC stack frame size against the sum of declared
    local variables in the C source. Flags functions where the declared
    locals are significantly smaller than the original frame.
    """
    errors = []
    i = 0
    while i < len(lines):
        m = SUB_ESP_PATTERN.search(lines[i])
        if not m:
            i += 1
            continue
        frame_size = _parse_int(m.group(1))
        func_name = None
        j = i
        for j in range(i + 1, min(i + 30, len(lines))):
            stripped = lines[j].lstrip()
            if stripped and not stripped.startswith(('*', '/', '#', '\n')):
                fn_match = re.match(
                    r'(?:static\s+)?(?:void|int|short|char|float|unsigned|'
                    r'int16_t|uint32_t|int32_t|bool|data_t)\s*\*?\s*'
                    r'(FUN_[0-9a-fA-F]+|\w+)\s*\(', stripped
                )
                if fn_match:
                    func_name = fn_match.group(1)
                break
        if func_name:
            body = _find_function_body(lines, func_name)
            if body:
                start, end = body
                declared = _sum_locals(lines[start:end])
                gap = frame_size - declared
                if gap >= 8:
                    relpath = os.path.relpath(filepath, ROOT_DIR)
                    errors.append(
                        f'  {relpath}:{j+1}: {func_name} — '
                        f'original: {frame_size:#x} ({frame_size}), '
                        f'declared: {declared:#x} ({declared}), '
                        f'gap: {gap} bytes'
                    )
        i = j + 1

    return errors


def _collect_c_files(changed_only=False, cached=False):
    """Collect .c file paths under SRC_DIR.

    If changed_only is True, only return files appearing in git diff output.
    If cached is also True, use --cached to check staged files.
    """
    if changed_only:
        cmd = ['git', 'diff', '--name-only', 'HEAD']
        if cached:
            cmd = ['git', 'diff', '--name-only', '--cached']
        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, cwd=ROOT_DIR
            )
            changed = set()
            for line in result.stdout.strip().splitlines():
                abspath = os.path.normpath(os.path.join(ROOT_DIR, line))
                changed.add(abspath)
        except (subprocess.SubprocessError, FileNotFoundError):
            changed = None
    else:
        changed = None

    c_files = []
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if not fname.endswith('.c'):
                continue
            fpath = os.path.join(dirpath, fname)
            if changed is not None and fpath not in changed:
                continue
            c_files.append(fpath)
    return c_files


def main():
    frame_audit = '--frame-size-audit' in sys.argv
    quiet = '-q' in sys.argv or '--quiet' in sys.argv
    changed_only = '--changed-only' in sys.argv
    cached = '--cached' in sys.argv

    c_files = _collect_c_files(changed_only=changed_only, cached=cached)

    params_map = _parse_decl_params()

    all_intrinsic_errors = []
    all_buffer_errors = []
    all_duplicate_errors = []
    all_ptr_float_errors = []
    all_alias_errors = []
    all_frame_errors = []

    for fpath in c_files:
        with open(fpath, 'r', errors='replace') as f:
            content = f.read()
        lines = content.split('\n')

        all_intrinsic_errors.extend(check_intrinsics(fpath, content, lines))
        all_buffer_errors.extend(check_buffer_sizes(fpath, content, lines))
        all_duplicate_errors.extend(check_duplicate_args(fpath, content, lines))
        all_ptr_float_errors.extend(check_pointer_as_float(fpath, content, lines, params_map))
        all_alias_errors.extend(check_buffer_alias(fpath, content, lines))
        if frame_audit:
            all_frame_errors.extend(check_frame_sizes(fpath, content, lines))

    if quiet:
        counts = (
            f'intrinsics: {len(all_intrinsic_errors)}, '
            f'buffer_sizes: {len(all_buffer_errors)}, '
            f'duplicate_args: {len(all_duplicate_errors)}, '
            f'pointer_as_float: {len(all_ptr_float_errors)}, '
            f'buffer_alias: {len(all_alias_errors)}'
        )
        if frame_audit:
            counts += f', frame_sizes: {len(all_frame_errors)}'
        print(counts, file=sys.stderr)
    else:
        if all_intrinsic_errors:
            print(
                'ERROR: MSVC intrinsic addresses found in lifted code.\n'
                'These are compiler runtime helpers with non-standard ABIs.\n'
                'Use the equivalent C idiom instead:\n',
                file=sys.stderr,
            )
            for e in all_intrinsic_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_buffer_errors:
            print(
                'ERROR: undersized buffers passed to known callees.\n'
                'Verify the buffer size matches the callee\'s memset/init:\n',
                file=sys.stderr,
            )
            for e in all_buffer_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_duplicate_errors:
            print(
                'WARNING: duplicate arguments in function calls.\n'
                'Ghidra may have confused callee-saved registers (EBX/ESI/EDI)\n'
                'with a different variable. Verify against disassembly:\n',
                file=sys.stderr,
            )
            for e in all_duplicate_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_ptr_float_errors:
            print(
                'WARNING: pointer expression passed to float parameter.\n'
                'MSVC passes floats via PUSH <dummy>; FSTP [ESP]. Ghidra may\n'
                'have reported the dummy (a pointer) instead of the float:\n',
                file=sys.stderr,
            )
            for e in all_ptr_float_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_alias_errors:
            print(
                'WARNING: possible buffer-alias confusion.\n'
                'A local buffer and a different pointer are accessed at nearby\n'
                'offsets. Ghidra may have shown a buffer field as a separate\n'
                'local_XX variable. Verify against disassembly [EBP±N]:\n',
                file=sys.stderr,
            )
            for e in all_alias_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_frame_errors:
            print(
                'WARNING: stack frame size mismatch in ported functions.\n'
                'The original MSVC binary allocates more stack space than our C\n'
                'code declares. Unported callees may depend on this layout\n'
                '(MSVC stack overlap hazard):\n',
                file=sys.stderr,
            )
            for e in all_frame_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

    if all_intrinsic_errors or all_buffer_errors:
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
