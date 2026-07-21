#!/usr/bin/env python3
"""Detect file-scope declarations stranded below their first use.

`tools/analysis/maintain.py` reorders function bodies by kb.json address but
glues header / inter-function lines to the following function's extent, so a
file-scope `#include`, `#define`, `typedef`, or per-function `#pragma clang
diagnostic push/pop` pair can end up *below* a lower-address function that needs
it. `maintain.py --check` only validates function order, so it passes while the
tree no longer compiles (implicit-function-declaration / undeclared-identifier /
"pragma pop could not pop"). This detector makes that failure visible.

It flags:
  * a col-0 file-scope `#define NAME` / `typedef ... NAME` whose first real
    *code* use (comments, strings, other directives and `extern` decls ignored)
    appears on an earlier line than the definition, and
  * a per-file `#pragma clang diagnostic push/pop` stack that underflows (orphan
    pop) or ends unbalanced (unclosed push).

Exit status is non-zero when anything is flagged. Read-only.

Usage:
  check_stranded_decls.py [FILE ...]     # default: all of src/halo/**/*.c
  check_stranded_decls.py --self-test    # verify the detector fires + stays quiet
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

_DEF_MACRO = re.compile(r'^#define\s+([A-Za-z_]\w*)')
_TYPEDEF_FNPTR = re.compile(r'^typedef\b.*\(\s*\*\s*([A-Za-z_]\w*)\s*\)')
_TYPEDEF_SIMPLE = re.compile(r'^typedef\b.*?\b([A-Za-z_]\w*)\s*;\s*$')
_PRAGMA_DIAG = re.compile(r'^\s*#pragma\s+clang\s+diagnostic\s+(push|pop)\b')


def strip_comments_strings(text):
    """Blank out /* */, // comments and "..." / '...' literals; keep newlines."""
    out = []
    i, n, state = 0, len(text), 'code'
    while i < n:
        c, two = text[i], text[i:i + 2]
        if state == 'code':
            if two == '/*':
                state = 'block'; out.append('  '); i += 2; continue
            if two == '//':
                state = 'line'; out.append('  '); i += 2; continue
            if c == '"':
                state = 'str'; out.append(' '); i += 1; continue
            if c == "'":
                state = 'chr'; out.append(' '); i += 1; continue
            out.append(c); i += 1
        elif state == 'block':
            if two == '*/':
                state = 'code'; out.append('  '); i += 2; continue
            out.append('\n' if c == '\n' else ' '); i += 1
        elif state == 'line':
            if c == '\n':
                state = 'code'; out.append('\n'); i += 1; continue
            out.append(' '); i += 1
        else:  # str / chr
            if c == '\\':
                out.append('  '); i += 2; continue
            if (state == 'str' and c == '"') or (state == 'chr' and c == "'"):
                state = 'code'
            out.append(' '); i += 1
    return ''.join(out)


def _is_ignored_use_line(raw_line):
    """Lines that mention a symbol but are not a real *use* of it."""
    s = raw_line.lstrip()
    return (s.startswith('#') or            # directives: #define/#undef/#pragma/#if
            s.startswith('typedef') or
            s.startswith('extern'))          # forward decl / intrinsic shim


def _match_def(line):
    """Return (name, kind) if `line` defines a macro/typedef, else None.
    Accepts leading whitespace so function-local (indented) defs are seen too."""
    s = line.lstrip()
    m = _DEF_MACRO.match(s)
    if m:
        return m.group(1), 'macro'
    if s.startswith('typedef ') and s.rstrip().endswith(';'):
        mt = _TYPEDEF_FNPTR.match(s) or _TYPEDEF_SIMPLE.match(s)
        if mt:
            return mt.group(1), 'typedef'
    return None


def find_stranded_in_text(raw):
    """Return list of (kind, name, defline, useline) for one file's text.

    A name is stranded only if its earliest *code* use precedes EVERY definition
    of that name (any scope). This is conservative: a function-local redefinition
    (an indented typedef/#define that shadows a file-scope one) covers uses within
    its own scope, so such names are not flagged — matching what the compiler
    accepts. Only names that have at least one file-scope (col-0) definition are
    reported, since those are the ones the reorder can strand.
    """
    lines = raw.split('\n')
    clean = strip_comments_strings(raw).split('\n')
    findings = []

    all_def_lines = {}       # name -> [line, ...] across all scopes
    file_scope_def = {}      # name -> (first col-0 defline, kind)
    for idx, ln in enumerate(lines, 1):
        d = _match_def(ln)
        if not d:
            continue
        name, kind = d
        all_def_lines.setdefault(name, []).append(idx)
        # col-0 (no leading whitespace) == file scope
        if ln[:1] not in (' ', '\t') and name not in file_scope_def:
            file_scope_def[name] = (idx, kind)

    for name, (defline, kind) in file_scope_def.items():
        earliest_def = min(all_def_lines[name])
        pat = re.compile(r'\b' + re.escape(name) + r'\b')
        for i, cln in enumerate(clean, 1):
            if i >= earliest_def:
                break
            if i - 1 < len(lines) and _is_ignored_use_line(lines[i - 1]):
                continue
            if pat.search(cln):
                findings.append((kind, name, defline, i))
                break
    return findings


def find_pragma_imbalance(raw):
    """Return list of human-readable pragma-stack problems for one file."""
    depth, problems = 0, []
    for idx, ln in enumerate(raw.split('\n'), 1):
        m = _PRAGMA_DIAG.match(ln)
        if not m:
            continue
        if m.group(1) == 'push':
            depth += 1
        else:
            depth -= 1
            if depth < 0:
                problems.append(f'orphan diagnostic pop at line {idx} (no matching push)')
                depth = 0
    if depth > 0:
        problems.append(f'{depth} unclosed diagnostic push at EOF')
    return problems


def check_file(path):
    try:
        raw = open(path, encoding='utf-8', errors='replace').read()
    except (FileNotFoundError, IsADirectoryError):
        return []
    msgs = []
    for kind, name, defline, useline in find_stranded_in_text(raw):
        msgs.append(f'{path}: file-scope {kind} {name!r} defined@{defline} but '
                    f'first used@{useline} (stranded below its use)')
    for prob in find_pragma_imbalance(raw):
        msgs.append(f'{path}: {prob}')
    return msgs


def iter_default_sources():
    base = os.path.join(ROOT, 'src', 'halo')
    for dirpath, _, files in os.walk(base):
        for f in files:
            if f.endswith('.c'):
                yield os.path.join(dirpath, f)


def self_test():
    ok = True
    bad = ('#include "x.h"\n'
           'void a(void){ q(FOO); }\n'
           '#define FOO 1\n')
    r = find_stranded_in_text(bad)
    if not any(n == 'FOO' for _, n, _, _ in r):
        print('SELF-TEST FAIL: stranded macro not detected'); ok = False

    # extern-decl-before-#define shim must NOT be flagged
    shim = ('extern int __cdecl abs(int);\n'
            '#define abs __builtin_abs\n'
            'int f(void){ return abs(-1); }\n')
    if find_stranded_in_text(shim):
        print('SELF-TEST FAIL: extern shim false positive'); ok = False

    # comment mention must NOT be flagged
    cmt = ('/* uses FOO here */\n'
           '#define FOO 1\n'
           'int f(void){ return FOO; }\n')
    if find_stranded_in_text(cmt):
        print('SELF-TEST FAIL: comment mention false positive'); ok = False

    # function-local typedef shadowing a later file-scope one must NOT be flagged
    shadow = ('int g(void){\n'
              '  typedef int (*T)(void *);\n'
              '  return ((T)0)(0);\n'
              '}\n'
              'typedef char (*T)(float *);\n'
              'int h(void){ return ((T)0)(0); }\n')
    if find_stranded_in_text(shadow):
        print('SELF-TEST FAIL: local-shadow false positive'); ok = False

    if find_pragma_imbalance('#pragma clang diagnostic pop\n') != \
            ['orphan diagnostic pop at line 1 (no matching push)']:
        print('SELF-TEST FAIL: orphan pop not detected'); ok = False
    if find_pragma_imbalance('#pragma clang diagnostic push\n'
                             '#pragma clang diagnostic pop\n'):
        print('SELF-TEST FAIL: balanced pragmas flagged'); ok = False

    print('self-test: PASS' if ok else 'self-test: FAIL')
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('files', nargs='*', help='C files to check (default: src/halo/**/*.c)')
    ap.add_argument('--self-test', action='store_true', help='verify the detector itself')
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    targets = args.files or list(iter_default_sources())
    all_msgs = []
    for path in targets:
        all_msgs.extend(check_file(path))

    for msg in all_msgs:
        print(msg, file=sys.stderr)
    if all_msgs:
        print(f'{len(all_msgs)} stranded-declaration issue(s) found. '
              f'Hoist file-scope #include/#define/typedef to the top of the file, '
              f'and consolidate scrambled diagnostic push/pop blocks.',
              file=sys.stderr)
        return 1
    print(f'check_stranded_decls: clean ({len(targets)} file(s))')
    return 0


if __name__ == '__main__':
    sys.exit(main())
