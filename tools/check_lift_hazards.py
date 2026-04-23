#!/usr/bin/env python3
"""Build-time check for known Ghidra decompiler pitfalls in lifted code.

Two classes of hazard:

1. MSVC intrinsic calls: Ghidra decompiles compiler runtime helpers as normal
   function calls, but they have non-standard ABIs (modify ESP, use FPU stack,
   register-only args). Calling them from C corrupts the stack.

2. Undersized callee buffers: Ghidra may infer a smaller buffer size than the
   callee actually writes. Passing an undersized stack buffer causes overflow.

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


def main():
    intrinsic_errors = check_intrinsics()
    buffer_errors = check_buffer_sizes()

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

    if intrinsic_errors or buffer_errors:
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
