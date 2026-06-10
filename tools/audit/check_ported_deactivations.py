#!/usr/bin/env python3
"""Audit kb.json for active port deactivations (ported: false on a function
that has a source implementation).

`ported: false` is a real switch: it tells `tools/build/patch.py` to skip the
original-address redirect and to write a JMP at our impl entry that tail-calls
the original. This is intended for bisection — temporarily reverting one port
at a time without deleting source.

A deactivation is "active" if a source impl exists for the function; otherwise
the field just describes a function we never lifted (the common case).

An allowlist at tools/audit/deactivation_allowlist.json records the set of
long-standing deactivations that were present when gating was first introduced
(2026-06-10). --check only fails on deactivations NOT in the allowlist so that
historical dormant/bisect deactivations don't keep CI permanently red.

Modes:
  --report           Print all active deactivations and exit 0 (default).
  --check            Fail (exit 1) only on deactivations NOT in the allowlist.
                     Suitable for CI gates and the pre-commit hook.
  --update-allowlist Regenerate the allowlist from the current kb.json state.
                     LOUD WARNING: additions need a justification comment.

Options:
  --kb PATH          Use a specific kb.json instead of the default.
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
KB_JSON = os.path.join(ROOT, 'kb.json')
SRC_DIR = os.path.join(ROOT, 'src')
ALLOWLIST_PATH = os.path.join(ROOT, 'tools', 'audit', 'deactivation_allowlist.json')

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


def _load_allowlist():
    """Return set of (addr, name) tuples from the allowlist file.

    Returns an empty set if the file does not exist (first-run, no allowlist).
    """
    if not os.path.isfile(ALLOWLIST_PATH):
        return set()
    try:
        with open(ALLOWLIST_PATH) as f:
            entries = json.load(f)
        # Key by addr; name is a secondary check for moved entries.
        return {e['addr'] for e in entries}
    except (OSError, json.JSONDecodeError, KeyError):
        return set()


def find_active_deactivations(kb_path=None):
    """Return list of dicts for functions with ported=false that have a src impl."""
    kb_path = kb_path or KB_JSON
    with open(kb_path) as f:
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


def _print_deactivations(active, label='active'):
    print('=' * 72, file=sys.stderr)
    print(f'WARNING: {len(active)} {label} port deactivation(s) detected in kb.json.',
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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument('--report', action='store_true', default=True,
                      help='Print all active deactivations and exit 0 (default)')
    mode.add_argument('--check', action='store_true',
                      help='Exit 1 only on deactivations NOT in the allowlist (for CI/hook)')
    mode.add_argument('--update-allowlist', action='store_true',
                      help='Regenerate allowlist from current kb.json state')
    ap.add_argument('--kb', metavar='PATH',
                    help='Path to a specific kb.json (default: repo kb.json)')
    args = ap.parse_args()

    kb_path = args.kb if args.kb else None

    if args.update_allowlist:
        active = find_active_deactivations(kb_path)
        entries = [
            {
                'addr': e['addr'],
                'name': e['name'],
                'object': e['object'],
                'reason': 'FILL IN JUSTIFICATION',
                'since': '2026-06-10',
            }
            for e in active
        ]
        with open(ALLOWLIST_PATH, 'w') as f:
            json.dump(entries, f, indent=2)
            f.write('\n')
        print(f'!!! UPDATED allowlist with {len(entries)} entries: {ALLOWLIST_PATH}', file=sys.stderr)
        print('!!! Review every NEW entry and replace "FILL IN JUSTIFICATION" with a real reason.', file=sys.stderr)
        print('!!! New allowlist entries must not be committed without justification.', file=sys.stderr)
        return 0

    active = find_active_deactivations(kb_path)

    if args.check:
        allowlist = _load_allowlist()
        new_deactivations = [e for e in active if e['addr'] not in allowlist]
        if new_deactivations:
            _print_deactivations(new_deactivations, label='NON-ALLOWLISTED')
            print('', file=sys.stderr)
            print('These deactivations are NOT in the allowlist and must not be committed.', file=sys.stderr)
            print(f'Allowlist: {ALLOWLIST_PATH}', file=sys.stderr)
            print('If this deactivation is intentional bisect work:', file=sys.stderr)
            print('  HALO_ALLOW_DEACTIVATIONS=1 git commit  (or --no-verify to bypass the hook)', file=sys.stderr)
            return 1
        # Report allowlisted count as informational, but exit 0.
        if active:
            print(f'INFO: {len(active)} allowlisted deactivation(s) present (all in allowlist — OK).', file=sys.stderr)
        return 0

    # Default: --report, print everything and exit 0.
    if not active:
        return 0
    _print_deactivations(active)
    return 0


if __name__ == '__main__':
    sys.exit(main())
