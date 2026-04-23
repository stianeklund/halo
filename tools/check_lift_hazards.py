#!/usr/bin/env python3
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

Usage:
    python3 tools/check_lift_hazards.py
"""
import os
import re
import sys

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
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


def check_intrinsics():
    errors = []
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if not fname.endswith('.c'):
                continue
            fpath = os.path.join(dirpath, fname)
            with open(fpath, 'r', errors='replace') as f:
                in_block_comment = False
                for lineno, line in enumerate(f, 1):
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
                        relpath = os.path.relpath(fpath, ROOT_DIR)
                        errors.append(
                            f'  {relpath}:{lineno}: call to {name} '
                            f'(0x{addr}) — {fix}'
                        )
    return errors


def check_buffer_sizes():
    errors = []
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if not fname.endswith('.c'):
                continue
            fpath = os.path.join(dirpath, fname)
            with open(fpath, 'r', errors='replace') as f:
                lines = f.readlines()

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
                            relpath = os.path.relpath(fpath, ROOT_DIR)
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
    Returns list of stripped argument strings, or None if parse fails.
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
                return args
            current.append(ch)
        elif ch == ',' and depth == 1:
            arg = ''.join(current).strip()
            if arg:
                args.append(arg)
            current = []
        else:
            current.append(ch)
        i += 1
    return None


def check_duplicate_args():
    """Flag calls where the same non-trivial expression appears as multiple args.

    Lines containing ``dup-args-ok`` are suppressed (verified in-place or
    intentional same-arg calls).
    """
    errors = []
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if not fname.endswith('.c'):
                continue
            fpath = os.path.join(dirpath, fname)
            with open(fpath, 'r', errors='replace') as f:
                content = f.read()
            lines = content.split('\n')
            flat = content
            for m in FUNC_CALL_PATTERN.finditer(flat):
                func_name = m.group(1)
                paren_pos = m.end() - 1
                args = _extract_args(flat, paren_pos)
                if args is None or len(args) < 2:
                    continue
                seen = {}
                for i, arg in enumerate(args):
                    if arg in TRIVIAL_ARGS:
                        continue
                    if len(arg) < 3:
                        continue
                    if arg in seen:
                        lineno = flat[:m.start()].count('\n') + 1
                        if 'dup-args-ok' in lines[lineno - 1]:
                            break
                        relpath = os.path.relpath(fpath, ROOT_DIR)
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


def check_pointer_as_float():
    """Flag calls where a pointer expression is passed to a float parameter."""
    params_map = _parse_decl_params()
    if not params_map:
        return []
    errors = []
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if not fname.endswith('.c'):
                continue
            fpath = os.path.join(dirpath, fname)
            with open(fpath, 'r', errors='replace') as f:
                content = f.read()
            flat = content
            for m in FUNC_CALL_PATTERN.finditer(flat):
                func_name = m.group(1)
                if func_name not in params_map:
                    continue
                ptypes = params_map[func_name]
                paren_pos = m.end() - 1
                args = _extract_args(flat, paren_pos)
                if args is None:
                    continue
                for i, arg in enumerate(args):
                    if i >= len(ptypes):
                        break
                    if ptypes[i] in ('float', 'double') and PTR_EXPR.search(arg):
                        lineno = flat[:m.start()].count('\n') + 1
                        relpath = os.path.relpath(fpath, ROOT_DIR)
                        errors.append(
                            f'  {relpath}:{lineno}: {func_name}() '
                            f'arg {i+1} looks like a pointer but '
                            f'param is {ptypes[i]} — possible '
                            f'push-then-fstp artifact'
                        )
    return errors


def main():
    intrinsic_errors = check_intrinsics()
    buffer_errors = check_buffer_sizes()
    duplicate_errors = check_duplicate_args()
    ptr_float_errors = check_pointer_as_float()

    if intrinsic_errors:
        print(
            'ERROR: MSVC intrinsic addresses found in lifted code.\n'
            'These are compiler runtime helpers with non-standard ABIs.\n'
            'Use the equivalent C idiom instead:\n',
            file=sys.stderr,
        )
        for e in intrinsic_errors:
            print(e, file=sys.stderr)
        print(file=sys.stderr)

    if buffer_errors:
        print(
            'ERROR: undersized buffers passed to known callees.\n'
            'Verify the buffer size matches the callee\'s memset/init:\n',
            file=sys.stderr,
        )
        for e in buffer_errors:
            print(e, file=sys.stderr)
        print(file=sys.stderr)

    if duplicate_errors:
        print(
            'WARNING: duplicate arguments in function calls.\n'
            'Ghidra may have confused callee-saved registers (EBX/ESI/EDI)\n'
            'with a different variable. Verify against disassembly:\n',
            file=sys.stderr,
        )
        for e in duplicate_errors:
            print(e, file=sys.stderr)
        print(file=sys.stderr)

    if ptr_float_errors:
        print(
            'WARNING: pointer expression passed to float parameter.\n'
            'MSVC passes floats via PUSH <dummy>; FSTP [ESP]. Ghidra may\n'
            'have reported the dummy (a pointer) instead of the float:\n',
            file=sys.stderr,
        )
        for e in ptr_float_errors:
            print(e, file=sys.stderr)
        print(file=sys.stderr)

    if intrinsic_errors or buffer_errors:
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
