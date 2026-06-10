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

6. Void-return out-param (EAX implicit return): MSVC frequently ends void-like
   functions with MOV EAX,[EBP+8] (loading the first out-param pointer into EAX)
   before RET. Unported callers use that EAX as the function's return value.
   If the ported C implementation declares void, the caller gets garbage EAX and
   reads through a dangling pointer — causing wrong nav positions, wrong coords,
   or silent corruption. Functions in VOID_BUT_RETURNS_EAX must be declared with
   the matching pointer return type.

Usage:
    python3 tools/audit/check_lift_hazards.py
    python3 tools/audit/check_lift_hazards.py -q
    python3 tools/audit/check_lift_hazards.py --changed-only
    python3 tools/audit/check_lift_hazards.py --staged-only
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

# Functions that MSVC ends with "MOV EAX,[EBP+8]; RET", returning their first
# out-param pointer in EAX.  Unported callers (Ghidra shows them reading the
# return value as a pointer) depend on this.  Any of these declared void in
# decl.h will cause the caller to read through garbage EAX.
# Map: addr_hex -> (canonical_name, description)
VOID_BUT_RETURNS_EAX = {
    '0xa9380': (
        'game_engine_get_goal_position',
        'returns param_1 (goal pos float*) in EAX — FUN_000d6cc0 uses it as position pointer',
    ),
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
    r'^\s+(?:volatile\s+)?(?:unsigned\s+)?(?:char|uint8_t|int8_t)\s+\w+\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]',
)
LOCAL_ARRAY16_PATTERN = re.compile(
    r'^\s+(?:volatile\s+)?(?:unsigned\s+)?(?:short|int16_t|uint16_t)\s+\w+\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]',
)
LOCAL_ARRAY32_PATTERN = re.compile(
    r'^\s+(?:volatile\s+)?(?:unsigned\s+)?(?:int|int32_t|uint32_t|float)\s+\w+\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]',
)
# Matches struct-typed arrays: `some_struct_t name[N]`.  The size per element
# is resolved at runtime from cs() macros parsed out of project headers.
LOCAL_STRUCT_ARRAY_PATTERN = re.compile(
    r'^\s+(?:volatile\s+)?(\w+_t)\s+\w+\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]',
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


def check_void_eax_returns():
    """Check that functions in VOID_BUT_RETURNS_EAX are not declared void in decl.h.

    MSVC ends these functions with MOV EAX,[EBP+8] before RET, returning the
    first out-param pointer in EAX.  Unported callers use that EAX value as a
    pointer.  If the ported C implementation is declared void the caller reads
    through garbage EAX — causing silent corruption or wrong coordinates.
    """
    errors = []
    if not os.path.isfile(DECL_H):
        return errors

    with open(DECL_H, 'r', errors='replace') as f:
        decl_content = f.read()

    for addr, (name, desc) in VOID_BUT_RETURNS_EAX.items():
        # Match lines like: HFUNC void name( ...
        pattern = re.compile(
            r'\bvoid\s+' + re.escape(name) + r'\s*\('
        )
        if pattern.search(decl_content):
            errors.append(
                f'  decl.h: {name} ({addr}) declared void but must return '
                f'its first out-param as int* — {desc}'
            )
    return errors


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

NUMERIC_LITERAL_ARG = re.compile(
    r'^\(?\s*(?:unsigned\s+|signed\s+)?(?:int|long|short|char|float|double|void)?\s*\*?\s*\)?\s*'
    r'-?(?:0x[0-9a-fA-F]+|\d+(?:\.\d*)?|\.\d+)(?:[uUlLfF]*)\s*$'
)

FUNC_CALL_PATTERN = re.compile(
    r'\b(FUN_[0-9a-fA-F]+|[a-z_][a-z0-9_]*)\s*\('
)


def _blank_comments_and_literals(text):
    """Preserve positions while blanking comments and string/char literals."""
    out = []
    i = 0
    state = 'code'
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ''
        if state == 'code':
            if ch == '/' and nxt == '/':
                out.extend((' ', ' '))
                i += 2
                state = 'line_comment'
                continue
            if ch == '/' and nxt == '*':
                out.extend((' ', ' '))
                i += 2
                state = 'block_comment'
                continue
            if ch == '"':
                out.append(' ')
                i += 1
                state = 'string'
                continue
            if ch == "'":
                out.append(' ')
                i += 1
                state = 'char'
                continue
            out.append(ch)
            i += 1
            continue
        if state == 'line_comment':
            out.append('\n' if ch == '\n' else ' ')
            i += 1
            if ch == '\n':
                state = 'code'
            continue
        if state == 'block_comment':
            if ch == '*' and nxt == '/':
                out.extend((' ', ' '))
                i += 2
                state = 'code'
                continue
            out.append('\n' if ch == '\n' else ' ')
            i += 1
            continue
        if state in ('string', 'char'):
            quote = '"' if state == 'string' else "'"
            if ch == '\\' and nxt:
                out.extend((' ', '\n' if nxt == '\n' else ' '))
                i += 2
                continue
            out.append('\n' if ch == '\n' else ' ')
            i += 1
            if ch == quote:
                state = 'code'
            continue
    return ''.join(out)


def _is_trivial_duplicate_arg(arg):
    if arg in TRIVIAL_ARGS:
        return True
    if NUMERIC_LITERAL_ARG.match(arg):
        return True
    return False


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
    flat = _blank_comments_and_literals(content)
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
            if _is_trivial_duplicate_arg(arg):
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


KNOWN_2OUTPUT_CALLEES = {
    'FUN_00061df0',
}

_CALL_LAST_ARG_RE = re.compile(
    r'FUN_00061df0\s*\([^)]*,\s*(\w+)\s*\)'
)

def check_callee_output_size(filepath, content, lines):
    """Flag calls to known 2-output projection helpers where the output
    buffer is declared with more than 2 elements.

    FUN_00061df0 writes exactly 2 floats (3D→2D axis projection). If the
    output buffer is declared as float[3], subsequent code may index with [2]
    and read uninitialized data — or worse, miss stack-alias overwrites from
    the original MSVC layout (hazard #5).
    """
    errors = []
    array_decls = {}
    for i, line in enumerate(lines):
        m = re.search(r'float\s+(\w+)\s*\[\s*(\d+)\s*\]', line)
        if m:
            array_decls[m.group(1)] = (int(m.group(2)), i + 1)

    for i, line in enumerate(lines):
        m = _CALL_LAST_ARG_RE.search(line)
        if not m:
            continue
        out_arg = m.group(1)
        if out_arg in array_decls:
            arr_size, decl_line = array_decls[out_arg]
            if arr_size > 2:
                relpath = os.path.relpath(filepath, ROOT_DIR)
                errors.append(
                    f'  {relpath}:{i+1}: FUN_00061df0 outputs 2 floats but '
                    f'{out_arg} is declared as [{arr_size}] at line '
                    f'{decl_line} — should be [2] to prevent index-past-end '
                    f'and buffer-alias confusion'
                )
    return errors


X87_MATH_PATTERN = re.compile(
    r'\b((?:cos|sin|tan|fmod)f?)\s*\('
)
X87_MATH_ALLOWLIST = re.compile(
    r'\bx87_f(?:cos|sin|mod)\b'
)


def check_x87_math(filepath, content, lines):
    """Flag C math library calls that compile to wrong x87 instructions.

    clang -mno-sse compiles fmod() to FPREM1 (IEEE remainder) instead of FPREM,
    and cos()/sin() to library calls instead of inline FCOS/FSIN. Use the x87_*
    inline asm helpers in random_math.c instead.
    """
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
        if X87_MATH_ALLOWLIST.search(line):
            continue
        if re.match(r'^(?:float|double)\s+(?:cos|sin|tan|fmod)f?\s*\(', stripped):
            continue
        for m in X87_MATH_PATTERN.finditer(line):
            func = m.group(1)
            if func.startswith('fmod'):
                fix = 'compiles to FPREM1 (IEEE remainder) not FPREM — use x87_fmod()'
            else:
                fix = 'library call differs from hardware FCOS/FSIN — use x87_fcos_mul()/x87_fsin_mul()'
            relpath = os.path.relpath(filepath, ROOT_DIR)
            errors.append(
                f'  {relpath}:{lineno}: {func}() — {fix}'
            )
    return errors


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


# ---------------------------------------------------------------------------
# §13: CONCAT* macro survival (ERROR)
# ---------------------------------------------------------------------------
CONCAT_PATTERN = re.compile(r'\bCONCAT\d+\b')


def check_concat_survival(filepath, content, lines):
    """Flag any CONCAT* macro that survived the draft_decompiler rewrite pass.

    draft_decompiler.py rewrites CONCAT16/CONCAT22/CONCAT44 etc. into bitfield
    or shift/OR idioms.  If a CONCAT token reaches lifted C it means the draft
    pass was not run, or the output was edited by hand.  Every occurrence is a
    build error: the macro is undefined and the function is almost certainly
    wrong (see lift-learnings §13).
    """
    errors = []
    flat_lines = _blank_comments_and_literals(content).split('\n')
    for lineno, line in enumerate(flat_lines, 1):
        if CONCAT_PATTERN.search(line):
            relpath = os.path.relpath(filepath, ROOT_DIR)
            errors.append(
                f'  {relpath}:{lineno}: CONCAT token survived — run '
                f'draft_decompiler.py to rewrite into shift/OR (see lift-learnings §13)'
            )
    return errors


# ---------------------------------------------------------------------------
# §6: Float-as-pointer bit smuggling (WARN)
# ---------------------------------------------------------------------------
# var = (T*)(int)(float_expr)  — float bits stored in a pointer variable
_FLOAT_BIT_STORE = re.compile(
    r'(\w+)\s*=\s*\(\s*(?:short|char|int|uint8_t|int8_t|uint16_t|int16_t|uint32_t|int32_t)\s*\*\s*\)'
    r'\s*\(\s*(?:unsigned\s+)?int\s*\)\s*'
)
# (float)(int)var  — retrieves float bits back from that pointer variable
_FLOAT_FROM_INT_VAR = re.compile(r'\(float\)\s*\((?:unsigned\s+)?int\s*\)\s*(\w+)\b')


def check_float_int_bit_smuggling(filepath, content, lines):
    """Flag (float)(int)var where var was previously assigned via (T*)(int)(expr).

    Ghidra decompiles a float-through-register as pointer casts.  A lift that
    follows the pseudocode literally does an integer truncation instead of a
    bit-reinterpretation — producing completely wrong values (see §6).
    Suppress with /* hazard-ok: numeric-truncation */ on the same line.
    """
    errors = []
    flat_lines = _blank_comments_and_literals(content).split('\n')
    chunks = _split_functions(lines)
    for chunk_start, chunk_end in chunks:
        smuggled_vars = set()
        for i in range(chunk_start - 1, min(chunk_end, len(flat_lines))):
            m = _FLOAT_BIT_STORE.search(flat_lines[i])
            if m:
                smuggled_vars.add(m.group(1))
        if not smuggled_vars:
            continue
        for i in range(chunk_start - 1, min(chunk_end, len(flat_lines))):
            src_line = lines[i] if i < len(lines) else ''
            if 'hazard-ok' in src_line:
                continue
            for m in _FLOAT_FROM_INT_VAR.finditer(flat_lines[i]):
                if m.group(1) in smuggled_vars:
                    relpath = os.path.relpath(filepath, ROOT_DIR)
                    errors.append(
                        f'  {relpath}:{i + 1}: (float)(int){m.group(1)} — float bits '
                        f'smuggled through pointer cast; use memcpy(&dst,&src,4) or a '
                        f'union (see lift-learnings §6)'
                    )
    return errors


# ---------------------------------------------------------------------------
# §17: Address offset mis-rendered as value addition (WARN)
# ---------------------------------------------------------------------------
# *(T*)0xLITERAL + N  where N < 0x10 and literal is a non-trivial address
_ADDR_VALUE_ADD = re.compile(
    r'\*\(\s*\w+\s*\*\s*\)\s*(0x[0-9a-fA-F]{4,})\s*\+\s*([0-9]+)\b'
)


def check_addr_value_add(filepath, content, lines):
    """Flag *(T*)0xADDR + N patterns that may be *(T*)(0xADDR + N) instead.

    The former reads the value at 0xADDR and adds N (value add); the latter
    reads from address 0xADDR+N (address add / struct field access).  The two
    are NOT equivalent.  Check disasm: if a single movswl [0xADDR+N] appears
    with no trailing ADD instruction the +N belongs inside the cast.
    (see lift-learnings §17)
    Suppress with /* hazard-ok: counter-increment */ or similar.

    False-positive heuristics applied automatically:
    - Inner double-dereference:  *(T*)(*(T*)0xADDR + N)  — the +N adds to a
      runtime pointer value, not a literal address.  Detected by ')' following
      the match end.
    - Counter-increment pattern:  *(T*)0xADDR = *(T*)0xADDR + N  — the same
      literal address appears on the LHS; incrementing a global counter.
    """
    errors = []
    flat_lines = _blank_comments_and_literals(content).split('\n')
    for lineno, line in enumerate(flat_lines, 1):
        src_line = lines[lineno - 1] if lineno <= len(lines) else ''
        if 'hazard-ok' in src_line:
            continue
        for m in _ADDR_VALUE_ADD.finditer(line):
            const = int(m.group(2))
            if const >= 0x10:
                continue
            addr_str = m.group(1)
            # Skip: inner expression already inside a cast-paren (double-deref)
            # e.g. *(float *)(*(int *)0xADDR + 4) — +4 is address math, correct
            end = m.end()
            if end < len(line) and line[end:].lstrip().startswith(')'):
                continue
            # Skip: same literal address on LHS (counter increment)
            # e.g.  *(int *)0xADDR = *(int *)0xADDR + 1
            lhs_pat = re.search(
                r'\*\(\s*\w+\s*\*\s*\)\s*' + re.escape(addr_str) + r'\s*=',
                line[:m.start()]
            )
            if lhs_pat:
                continue
            addr = int(addr_str, 16)
            relpath = os.path.relpath(filepath, ROOT_DIR)
            errors.append(
                f'  {relpath}:{lineno}: *(T*)0x{addr:x} + {const} — '
                f'may be address-add mis-rendered as value-add; '
                f'check disasm for movswl/mov [0x{addr:x}+{const}] '
                f'(see lift-learnings §17; suppress with hazard-ok comment)'
            )
    return errors


# ---------------------------------------------------------------------------
# §4: Parameter corruption by loop (WARN)
# ---------------------------------------------------------------------------
_FUNC_SIG_PAT = re.compile(
    r'^(?:static\s+)?(?:void|int|short|char|float|unsigned\s+\w+|uint\w*|int\w*|data_t|bool|[A-Za-z_]\w*_t)\s*\*?\s*'
    r'(?:FUN_[0-9a-fA-F]+|\w+)\s*\(([^)]*)\)\s*\{'
)
_LOOP_KW = re.compile(r'\b(for|while|do)\b')
_PARAM_MUTATE_PAT = re.compile(r'\b(\w+)\s*(?:\+\+|--|[+\-*/%]=)')


def _extract_param_names_from_sig(sig_line):
    """Return set of bare parameter names from a function signature line."""
    m = _FUNC_SIG_PAT.match(sig_line.strip())
    if not m:
        return set()
    params_raw = m.group(1)
    names = set()
    for part in params_raw.split(','):
        part = part.strip().rstrip(')').strip()
        tokens = part.replace('*', ' ').split()
        skip = {'void', 'int', 'char', 'float', 'short', 'unsigned',
                'long', 'const', 'volatile', 'signed', 'double'}
        if tokens and tokens[-1] not in skip:
            names.add(tokens[-1])
    return names


def check_param_loop_corruption(filepath, content, lines):
    """Flag function parameters mutated inside a loop that are used after it.

    Ghidra generates ``param = param + 1`` inside a loop body when the
    original code used a separate copy register.  After the loop the param
    is advanced past the buffer end.  (see lift-learnings §4)
    Suppress with /* hazard-ok: intentional-param-advance */ on the loop line.
    """
    errors = []
    flat_lines = _blank_comments_and_literals(content).split('\n')
    i = 0
    while i < len(flat_lines):
        src_line = lines[i] if i < len(lines) else ''
        params = _extract_param_names_from_sig(src_line)
        if not params:
            i += 1
            continue
        body_start = i + 1
        depth = src_line.count('{') - src_line.count('}')
        j = body_start
        while j < len(flat_lines) and depth > 0:
            depth += flat_lines[j].count('{') - flat_lines[j].count('}')
            j += 1
        body_end = j

        k = body_start
        while k < body_end:
            src_k = lines[k] if k < len(lines) else ''
            if _LOOP_KW.search(flat_lines[k]) and 'hazard-ok' not in src_k:
                loop_depth = flat_lines[k].count('{') - flat_lines[k].count('}')
                loop_line = k
                mutated = set()
                k2 = k + 1
                if loop_depth <= 0:
                    loop_depth = 1
                while k2 < body_end and loop_depth > 0:
                    loop_depth += flat_lines[k2].count('{') - flat_lines[k2].count('}')
                    for mm in _PARAM_MUTATE_PAT.finditer(flat_lines[k2]):
                        v = mm.group(1)
                        if v in params:
                            mutated.add(v)
                    k2 += 1
                if mutated:
                    for k3 in range(k2, body_end):
                        for v in list(mutated):
                            if re.search(r'\b' + re.escape(v) + r'\b', flat_lines[k3]):
                                relpath = os.path.relpath(filepath, ROOT_DIR)
                                errors.append(
                                    f'  {relpath}:{k3 + 1}: parameter \'{v}\' used '
                                    f'after loop body (mutated ~line {loop_line + 1}) '
                                    f'— verify loop uses a copy register, not param '
                                    f'(see lift-learnings §4)'
                                )
                                mutated.discard(v)
                k = k2
                continue
            k += 1
        i = body_end
    return errors


# ---------------------------------------------------------------------------
# §8/§11: Discarded non-trivial function-call result (WARN)
# ---------------------------------------------------------------------------
_DISCARD_VAR_PAT = re.compile(r'^\s*\(void\)\s*(\w+)\s*;')
_CALL_ASSIGN_PAT = re.compile(
    r'\b(\w+)\s*=\s*(?:FUN_[0-9a-fA-F]+|[a-z_]\w*)\s*\('
)
_DISCARD_INLINE_PAT = re.compile(r'^\s*\(void\)\s*([a-zA-Z_]\w*)\s*\(')
_DISCARD_INLINE_EXEMPT = re.compile(
    r'\b(?:tag_get|tag_group_get|object_get_and_verify_type|datum_get)\s*\('
)


def check_discarded_result(filepath, content, lines):
    """Flag (void)var; where var was assigned from a function call nearby.

    The §8 accumulator-discard bug: an RNG/score result is computed then
    thrown away before the loop that depends on it.  Also flags
    ``(void)func(...)`` for non-exempt named callees.
    (see lift-learnings §8, §11)
    Suppress with /* hazard-ok: intentional-discard */ on the line.
    """
    errors = []
    flat_lines = _blank_comments_and_literals(content).split('\n')

    # Pattern 1: (void)var; after a recent function-call assignment
    for lineno, line in enumerate(flat_lines, 1):
        src_line = lines[lineno - 1] if lineno <= len(lines) else ''
        if 'hazard-ok' in src_line:
            continue
        m = _DISCARD_VAR_PAT.match(line)
        if not m:
            continue
        var = m.group(1)
        look_start = max(0, lineno - 6)
        for prev in flat_lines[look_start:lineno - 1]:
            mm = _CALL_ASSIGN_PAT.search(prev)
            if mm and mm.group(1) == var:
                relpath = os.path.relpath(filepath, ROOT_DIR)
                errors.append(
                    f'  {relpath}:{lineno}: (void){var}; discards a '
                    f'function-call result — verify original also ignores it '
                    f'(see lift-learnings §8/§11)'
                )
                break

    # Pattern 2: (void)callee(...) inline — only non-exempt named functions
    for lineno, line in enumerate(flat_lines, 1):
        src_line = lines[lineno - 1] if lineno <= len(lines) else ''
        if 'hazard-ok' in src_line:
            continue
        m = _DISCARD_INLINE_PAT.match(line)
        if not m:
            continue
        if _DISCARD_INLINE_EXEMPT.search(line):
            continue
        callee = m.group(1)
        if re.match(r'FUN_', callee):
            continue  # unknown return type
        relpath = os.path.relpath(filepath, ROOT_DIR)
        errors.append(
            f'  {relpath}:{lineno}: (void){callee}(...) — return value '
            f'discarded; verify original code also ignores it '
            f'(see lift-learnings §8/§11)'
        )
    return errors


def _collect_c_files(changed_only=False, staged_only=False):
    """Collect .c file paths under SRC_DIR.

    If staged_only is True, only return files staged in the index
    (git diff --cached), for use in pre-commit hooks.

    If changed_only is True, return the union of:
      - staged files (git diff --cached --name-only)
      - unstaged tracked changes (git diff --name-only HEAD)
      - untracked files in src/ (git ls-files -o --exclude-standard)
    This covers everything that differs from HEAD in any state — useful
    for manual hazard sweeps after editing.
    """
    def _git_lines(cmd):
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT_DIR)
            return r.stdout.strip().splitlines()
        except (subprocess.SubprocessError, FileNotFoundError):
            return []

    if staged_only:
        lines = _git_lines(['git', 'diff', '--cached', '--name-only', '--', 'src/'])
        changed = set()
        for line in lines:
            abspath = os.path.normpath(os.path.join(ROOT_DIR, line))
            changed.add(abspath)
    elif changed_only:
        changed = set()
        # staged
        for line in _git_lines(['git', 'diff', '--cached', '--name-only', '--', 'src/']):
            changed.add(os.path.normpath(os.path.join(ROOT_DIR, line)))
        # unstaged tracked
        for line in _git_lines(['git', 'diff', '--name-only', 'HEAD', '--', 'src/']):
            changed.add(os.path.normpath(os.path.join(ROOT_DIR, line)))
        # untracked
        for line in _git_lines(['git', 'ls-files', '-o', '--exclude-standard', '--', 'src/']):
            changed.add(os.path.normpath(os.path.join(ROOT_DIR, line)))
        if not changed:
            changed = None  # fall back to full scan if git unavailable
    else:
        changed = None

    c_files = []
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if not fname.endswith('.c'):
                continue
            if fname.startswith('.'):  # skip generated/hidden files (e.g. .vc71_regcall_*)
                continue
            fpath = os.path.join(dirpath, fname)
            if changed is not None and fpath not in changed:
                continue
            c_files.append(fpath)
    return c_files


def main():
    frame_audit = '--frame-size-audit' in sys.argv
    quiet = '-q' in sys.argv or '--quiet' in sys.argv or os.environ.get('LOG_LEVEL') == 'WARNING'
    changed_only = '--changed-only' in sys.argv
    staged_only = '--staged-only' in sys.argv

    c_files = _collect_c_files(changed_only=changed_only, staged_only=staged_only)

    params_map = _parse_decl_params()

    all_void_eax_errors = check_void_eax_returns()
    all_intrinsic_errors = []
    all_buffer_errors = []
    all_duplicate_errors = []
    all_ptr_float_errors = []
    all_alias_errors = []
    all_frame_errors = []
    all_output_size_errors = []
    all_x87_math_errors = []
    all_concat_errors = []
    all_float_smuggle_errors = []
    all_addr_value_add_errors = []
    all_param_loop_errors = []
    all_discard_result_errors = []

    for fpath in c_files:
        with open(fpath, 'r', errors='replace') as f:
            content = f.read()
        lines = content.split('\n')

        all_intrinsic_errors.extend(check_intrinsics(fpath, content, lines))
        all_buffer_errors.extend(check_buffer_sizes(fpath, content, lines))
        all_duplicate_errors.extend(check_duplicate_args(fpath, content, lines))
        all_ptr_float_errors.extend(check_pointer_as_float(fpath, content, lines, params_map))
        all_alias_errors.extend(check_buffer_alias(fpath, content, lines))
        all_output_size_errors.extend(check_callee_output_size(fpath, content, lines))
        all_x87_math_errors.extend(check_x87_math(fpath, content, lines))
        all_concat_errors.extend(check_concat_survival(fpath, content, lines))
        all_float_smuggle_errors.extend(check_float_int_bit_smuggling(fpath, content, lines))
        all_addr_value_add_errors.extend(check_addr_value_add(fpath, content, lines))
        all_param_loop_errors.extend(check_param_loop_corruption(fpath, content, lines))
        all_discard_result_errors.extend(check_discarded_result(fpath, content, lines))
        if frame_audit:
            all_frame_errors.extend(check_frame_sizes(fpath, content, lines))

    if quiet:
        counts = (
            f'void_eax: {len(all_void_eax_errors)}, '
            f'intrinsics: {len(all_intrinsic_errors)}, '
            f'buffer_sizes: {len(all_buffer_errors)}, '
            f'duplicate_args: {len(all_duplicate_errors)}, '
            f'pointer_as_float: {len(all_ptr_float_errors)}, '
            f'buffer_alias: {len(all_alias_errors)}, '
            f'callee_output_size: {len(all_output_size_errors)}, '
            f'x87_math: {len(all_x87_math_errors)}, '
            f'concat_survival: {len(all_concat_errors)}, '
            f'float_smuggling: {len(all_float_smuggle_errors)}, '
            f'addr_value_add: {len(all_addr_value_add_errors)}, '
            f'param_loop: {len(all_param_loop_errors)}, '
            f'discard_result: {len(all_discard_result_errors)}'
        )
        if frame_audit:
            counts += f', frame_sizes: {len(all_frame_errors)}'
        total = (len(all_void_eax_errors) + len(all_intrinsic_errors) + len(all_buffer_errors) +
                 len(all_duplicate_errors) + len(all_ptr_float_errors) +
                 len(all_alias_errors) + len(all_frame_errors) +
                 len(all_output_size_errors) + len(all_x87_math_errors) +
                 len(all_concat_errors) + len(all_float_smuggle_errors) +
                 len(all_addr_value_add_errors) + len(all_param_loop_errors) +
                 len(all_discard_result_errors))
        if total:
            print(counts, file=sys.stderr)
    else:
        if all_void_eax_errors:
            print(
                'ERROR: void-return functions that must return out-param in EAX.\n'
                'Unported callers use the EAX return value as a pointer.\n'
                'Change the C return type from void to int* and add "return param_1":\n',
                file=sys.stderr,
            )
            for e in all_void_eax_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

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

        if all_output_size_errors:
            print(
                'ERROR: callee output buffer oversized.\n'
                'A known 2-output function (e.g. FUN_00061df0, 3D→2D projection)\n'
                'is called with a buffer declared larger than 2 elements.\n'
                'This causes index-past-end reads and buffer-alias confusion:\n',
                file=sys.stderr,
            )
            for e in all_output_size_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_x87_math_errors:
            print(
                'WARNING: C math library calls that compile to wrong x87 instructions.\n'
                'clang -mno-sse compiles fmod() to FPREM1 (IEEE remainder, not C fmod)\n'
                'and cos()/sin() to library calls (not hardware FCOS/FSIN).\n'
                'Use the x87_* inline asm helpers instead:\n',
                file=sys.stderr,
            )
            for e in all_x87_math_errors:
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

        if all_concat_errors:
            print(
                'ERROR: CONCAT* macro survived in lifted C — draft_decompiler.py\n'
                'must be run first, or the output was hand-edited incorrectly.\n'
                'Rewrite as bitfield or shift/OR (see lift-learnings §13):\n',
                file=sys.stderr,
            )
            for e in all_concat_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_float_smuggle_errors:
            print(
                'WARNING: float bits smuggled through pointer cast.\n'
                'A variable assigned via (T*)(int)(float) is later read as\n'
                '(float)(int)var — numeric truncation, not bit-reinterpretation.\n'
                'Use memcpy(&dst, &src, 4) or a union (see lift-learnings §6):\n',
                file=sys.stderr,
            )
            for e in all_float_smuggle_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_addr_value_add_errors:
            print(
                'WARNING: *(T*)0xADDR + N may be *(T*)(0xADDR+N) address-add.\n'
                'Check disasm: movswl [0xADDR+N] with no trailing ADD = address,\n'
                'not value, add (see lift-learnings §17):\n',
                file=sys.stderr,
            )
            for e in all_addr_value_add_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_param_loop_errors:
            print(
                'WARNING: function parameter mutated inside loop body and used after.\n'
                'Ghidra can show param++ when the original used a copy register.\n'
                'Use a separate loop variable (see lift-learnings §4):\n',
                file=sys.stderr,
            )
            for e in all_param_loop_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

        if all_discard_result_errors:
            print(
                'WARNING: function-call result discarded with (void).\n'
                'Verify the original binary also ignores this return value;\n'
                'it may seed a loop accumulator or an assertion counter.\n'
                '(see lift-learnings §8/§11):\n',
                file=sys.stderr,
            )
            for e in all_discard_result_errors:
                print(e, file=sys.stderr)
            print(file=sys.stderr)

    if all_void_eax_errors or all_intrinsic_errors or all_buffer_errors \
            or all_output_size_errors or all_concat_errors:
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
