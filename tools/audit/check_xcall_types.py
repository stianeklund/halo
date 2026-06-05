#!/usr/bin/env python3
"""Audit XCALL raw-cast macros against kb.json declarations.

Catches the bug class where an XCALL function-pointer cast uses the wrong
return type or parameter type vs the function's actual signature in kb.json.
On x86:
  - float return reads ST(0); int/uint32_t return reads EAX. Wrong type =
    garbage from the wrong register.
  - float param is pushed via FLD+FSTP[ESP]; int param is pushed via PUSH.
    Casting (int)float_var truncates the float before pushing.

Usage:
    python3 tools/audit/check_xcall_types.py [src/path/to/file.c ...]
    python3 tools/audit/check_xcall_types.py --all          # scan all src/*.c
    python3 tools/audit/check_xcall_types.py --changed-only  # git diff files

Exit code: number of mismatches found (0 = clean).
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KB_PATH = os.path.join(ROOT, 'kb.json')

XCALL_PATTERN = re.compile(
    r'#define\s+\w+\([^)]*\)\s+XCALL\(\s*(0x[0-9a-fA-F]+)\s*,\s*'
    r'([\w\s\*]+)\s*\(\s*\*\s*\)\s*\(([^)]*)\)\s*\)'
)

FLOAT_TYPES = {'float', 'double', 'float10'}
INT_TYPES = {'int', 'unsigned int', 'uint32_t', 'unsigned', 'long',
             'unsigned long', 'short', 'unsigned short', 'int16_t',
             'uint16_t', 'char', 'unsigned char', 'uint8_t', 'int8_t',
             'bool'}
PTR_PATTERN = re.compile(r'\*')


def normalize_type(t):
    """Normalize a C type string for comparison."""
    t = t.strip()
    t = re.sub(r'\s+', ' ', t)
    t = t.replace('unsigned int', 'uint32_t').replace('signed int', 'int')
    return t


def is_float_type(t):
    t = normalize_type(t)
    return t in FLOAT_TYPES


def is_int_type(t):
    t = normalize_type(t)
    return t in INT_TYPES or t.startswith('int') or t.startswith('uint')


def is_ptr_type(t):
    return '*' in t


def type_class(t):
    """Classify a type as 'float', 'int', 'ptr', or 'void'."""
    t = normalize_type(t)
    if t == 'void':
        return 'void'
    if is_ptr_type(t):
        return 'ptr'
    if is_float_type(t):
        return 'float'
    if is_int_type(t):
        return 'int'
    return 'unknown'


def parse_xcall_macros(filepath):
    """Extract XCALL macro definitions from a C file."""
    macros = []
    with open(filepath, 'r', errors='replace') as f:
        content = f.read()
    for m in XCALL_PATTERN.finditer(content):
        addr = m.group(1).lower()
        ret_type = m.group(2).strip()
        params_str = m.group(3).strip()
        lineno = content[:m.start()].count('\n') + 1

        params = []
        if params_str and params_str != 'void':
            for p in params_str.split(','):
                p = p.strip()
                p = re.sub(r'\w+$', '', p).strip()
                if not p:
                    p = params_str.split(',')[len(params)].strip()
                params.append(p)

        macros.append({
            'addr': addr,
            'ret': ret_type,
            'params': params,
            'line': lineno,
            'file': filepath,
            'raw': m.group(0)[:80],
        })
    return macros


def parse_kb_decls():
    """Build addr → declaration map from kb.json."""
    with open(KB_PATH) as f:
        kb = json.load(f)
    decls = {}
    for key, val in kb.items():
        if isinstance(val, dict) and 'addr' in val and 'decl' in val:
            decls[val['addr'].lower()] = val['decl']
    for obj in kb.get('objects', []):
        for fn in obj.get('functions', []):
            if 'addr' in fn and 'decl' in fn:
                decls[fn['addr'].lower()] = fn['decl']
        for d in obj.get('data', []):
            if 'addr' in d and 'decl' in d:
                decls[d['addr'].lower()] = d['decl']
    return decls


DECL_PATTERN = re.compile(
    r'^\s*([\w\s\*]+?)\s+\w+\s*\(([^)]*)\)\s*;'
)


def parse_decl(decl_str):
    """Parse a kb.json decl string into (return_type, [param_types])."""
    decl_str = re.sub(r'@<\w+>', '', decl_str)
    m = DECL_PATTERN.match(decl_str)
    if not m:
        return None, []
    ret = m.group(1).strip()
    params_str = m.group(2).strip()
    if not params_str or params_str == 'void':
        return ret, []
    params = []
    for p in params_str.split(','):
        p = p.strip()
        p = re.sub(r'\s+\w+$', '', p).strip()
        params.append(p)
    return ret, params


def check_file(filepath, kb_decls):
    """Check all XCALL macros in a file against kb.json."""
    macros = parse_xcall_macros(filepath)
    errors = []
    for mac in macros:
        addr = mac['addr']
        if addr not in kb_decls:
            continue
        kb_ret, kb_params = parse_decl(kb_decls[addr])
        if kb_ret is None:
            continue

        relpath = os.path.relpath(mac['file'], ROOT)
        xcall_ret_class = type_class(mac['ret'])
        kb_ret_class = type_class(kb_ret)

        if xcall_ret_class != kb_ret_class and kb_ret_class != 'void':
            severity = 'ERROR' if (
                (xcall_ret_class == 'float') != (kb_ret_class == 'float')
            ) else 'WARN'
            errors.append(
                f'  {severity} {relpath}:{mac["line"]}: {addr} return type '
                f'XCALL={mac["ret"]}({xcall_ret_class}) vs '
                f'kb.json={kb_ret}({kb_ret_class}) — '
                f'{"float↔int = wrong register (ST0 vs EAX)!" if severity == "ERROR" else "type class mismatch"}'
            )

        xcall_params = mac['params']
        for i, (xp, kp) in enumerate(zip(xcall_params, kb_params)):
            xc = type_class(xp)
            kc = type_class(kp)
            if xc != kc and kc != 'unknown' and xc != 'unknown':
                severity = 'ERROR' if (
                    (xc == 'float') != (kc == 'float')
                ) else 'WARN'
                errors.append(
                    f'  {severity} {relpath}:{mac["line"]}: {addr} param {i+1} '
                    f'XCALL={xp}({xc}) vs kb.json={kp}({kc}) — '
                    f'{"float↔int = truncation/wrong push!" if severity == "ERROR" else "type class mismatch"}'
                )
    return errors


def collect_files(args):
    """Collect C files to scan."""
    if '--all' in args:
        result = []
        for dirpath, _, filenames in os.walk(os.path.join(ROOT, 'src')):
            for fn in filenames:
                if fn.endswith('.c'):
                    result.append(os.path.join(dirpath, fn))
        return result
    if '--changed-only' in args:
        try:
            out = subprocess.check_output(
                ['git', 'diff', '--name-only', '--diff-filter=ACMR', 'HEAD', '--', 'src/'],
                cwd=ROOT, text=True
            )
            return [os.path.join(ROOT, f.strip()) for f in out.strip().split('\n')
                    if f.strip().endswith('.c')]
        except subprocess.CalledProcessError:
            return []
    files = [a for a in args if a.endswith('.c')]
    return [os.path.join(ROOT, f) if not os.path.isabs(f) else f for f in files]


def main():
    args = sys.argv[1:]
    if not args:
        args = ['--all']

    kb_decls = parse_kb_decls()
    files = collect_files(args)
    all_errors = []
    for fpath in files:
        if os.path.exists(fpath):
            all_errors.extend(check_file(fpath, kb_decls))

    if all_errors:
        print(f'XCALL type mismatches ({len(all_errors)}):')
        for e in all_errors:
            print(e)
    else:
        print('XCALL type audit: clean')

    error_count = sum(1 for e in all_errors if 'ERROR' in e)
    return error_count


if __name__ == '__main__':
    sys.exit(main())
