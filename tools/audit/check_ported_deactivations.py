#!/usr/bin/env python3
"""Audit kb.json for active port deactivations (ported: false on a function
that has a source implementation).

`ported: false` is a real switch: it tells `tools/build/patch.py` to skip the
original-address redirect and to write a JMP at our impl entry that tail-calls
the original. This is intended for bisection — temporarily reverting one port
at a time without deleting source.

A deactivation is "active" if a source impl exists for the function; otherwise
the field just describes a function we never lifted (the common case).

Modes:
  --report    Print active deactivations and exit 0 (warn, don't block).
  --check     Same, but exit 1 if any active deactivations are present —
              suitable for CI lanes that should not ship deactivations.
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
KB_JSON = os.path.join(ROOT, 'kb.json')
SRC_DIR = os.path.join(ROOT, 'src')

_NAME_RE = re.compile(r'(\w+)\s*\(')
_REG_RE = re.compile(r'@<\w+>')


def _name_from_decl(decl):
    cleaned = _REG_RE.sub('', decl)
    m = _NAME_RE.search(cleaned)
    return m.group(1) if m else None


def _build_source_impl_set():
    """Return the set of function names that have a definition in src/.

    Cheap heuristic: grep for `<name>(...) {` at the start of a line. Misses
    a few corner cases but the false-negative rate is tiny in this codebase.
    """
    impls = set()
    func_def_re = re.compile(r'^[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\([^;]*\)\s*\{', re.MULTILINE)
    for root, _, files in os.walk(SRC_DIR):
        for f in files:
            if not f.endswith('.c'):
                continue
            path = os.path.join(root, f)
            try:
                with open(path, encoding='utf-8') as fh:
                    text = fh.read()
            except OSError:
                continue
            for m in func_def_re.finditer(text):
                impls.add(m.group(1))
    return impls


def find_active_deactivations():
    with open(KB_JSON) as f:
        kb = json.load(f)
    impls = _build_source_impl_set()
    active = []
    for obj in kb.get('objects', []):
        for func in obj.get('functions', []) or []:
            if func.get('ported') is not False:
                continue
            decl = func.get('decl', '')
            name = _name_from_decl(decl)
            if name and name in impls:
                active.append({
                    'name': name,
                    'addr': func.get('addr', ''),
                    'object': obj.get('name', '?'),
                })
    return active


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group()
    g.add_argument('--report', action='store_true', default=True,
                   help='Print and exit 0 (default)')
    g.add_argument('--check', action='store_true',
                   help='Print and exit non-zero if any deactivations are active')
    args = ap.parse_args()

    active = find_active_deactivations()
    if not active:
        return 0

    print('=' * 72, file=sys.stderr)
    print(f'WARNING: {len(active)} active port deactivation(s) detected in kb.json.',
          file=sys.stderr)
    print('', file=sys.stderr)
    print('These functions have a source impl but `ported: false` — the patch is', file=sys.stderr)
    print('skipped at the original address and our impl entry tail-calls original.', file=sys.stderr)
    print('Intended for bisection. Do not ship a release with these set.', file=sys.stderr)
    print('', file=sys.stderr)
    for entry in active:
        print(f'  {entry["addr"]}  {entry["name"]:<40}  ({entry["object"]})', file=sys.stderr)
    print('', file=sys.stderr)
    print('To re-activate: set `"ported": true` on each entry above and rebuild.', file=sys.stderr)
    print('=' * 72, file=sys.stderr)

    return 1 if args.check else 0


if __name__ == '__main__':
    sys.exit(main())
