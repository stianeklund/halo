#!/usr/bin/env python3
"""Readability ratchet + per-file advisory for lifted C (readable-lift Phase 3).

Tracks the readability debt the readable-lift initiative pays down, so that
recovery wins are locked in (baseline can only ratchet DOWN) and new lifts get
per-file feedback on how "un-recovered" they still read.

Categories:
  raw_fnptr_cast   ((T(*)(A))0xADDR)(...)   HARD -- bypasses kb.json/thunks and
                                             hides calling-convention bugs. A raw
                                             cast is NEVER necessary (add the
                                             callee to kb.json instead), so this
                                             stays hard-gated -- but by the
                                             long-standing tools/audit/check_raw_casts.py
                                             (baseline tools/raw_cast_baseline.txt).
                                             Reported here for the --changed-only
                                             view only; not owned by this baseline.
  fun_call         FUN_<addr>(...)          SOFT -- a call to (or definition of)
                                             an un-named function. Legitimately
                                             grows as new code is lifted, so it is
                                             tracked and ratcheted-down, never
                                             hard-blocked.
  raw_offset_deref *(T *)(ident + 0xNN)     SOFT -- an un-recovered struct field
                                             access. Same rationale as fun_call.

Modes:
  --check           Global ratchet over src/. Auto-lowers the soft baseline on any
                    decrease (locks the win). WARNS if a soft category grew but
                    exits 0 (non-blocking) -- normal lifts add FUN_ calls and
                    offset derefs before recovery pays them down. Raw fn-ptr casts
                    are NOT gated here; run check_raw_casts.py for that hard gate.
  --changed-only    Per-file, line-numbered findings across ALL categories for the
                    files you have touched (git staged + unstaged-tracked +
                    untracked). Developer/agent feedback loop. Exits 1 if any
                    findings so they are noticed before committing.
  --update          Rewrite the soft baseline to the current counts.
  --json            Machine-readable output.

Usage:
    python3 tools/audit/check_readability.py --check
    python3 tools/audit/check_readability.py --changed-only
    python3 tools/audit/check_readability.py --update
"""
import json
import os
import re
import subprocess
import sys

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
SRC_DIR = os.path.join(ROOT_DIR, 'src')
BASELINE_FILE = os.path.join(ROOT_DIR, 'tools', 'readability_baseline.json')

# raw_fnptr_cast pattern is kept byte-identical to check_raw_casts.py so the two
# tools never disagree on what a raw cast is.
PATTERNS = {
    'raw_fnptr_cast': re.compile(r'\(\(.*\(\*\).*\)0x[0-9a-fA-F]'),
    'fun_call': re.compile(r'\bFUN_[0-9a-fA-F]{4,}\s*\('),
    'raw_offset_deref': re.compile(
        r'\*\([A-Za-z_][\w ]*\*\)\([A-Za-z_]\w* \+ 0x[0-9a-fA-F]+\)'),
}

# Categories the ratchet baseline owns. raw_fnptr_cast is HARD-gated elsewhere
# (check_raw_casts.py) so it is intentionally NOT ratcheted here.
SOFT_CATEGORIES = ('fun_call', 'raw_offset_deref')


def _iter_c_files():
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if fname.endswith('.c'):
                yield os.path.join(dirpath, fname)


def count_file(fpath):
    """Return {category: count} for one file."""
    counts = {k: 0 for k in PATTERNS}
    with open(fpath, 'r', errors='replace') as f:
        for line in f:
            for cat, pat in PATTERNS.items():
                counts[cat] += len(pat.findall(line))
    return counts


def findings_file(fpath):
    """Return list of (lineno, category, text) for one file."""
    out = []
    with open(fpath, 'r', errors='replace') as f:
        for lineno, line in enumerate(f, 1):
            for cat, pat in PATTERNS.items():
                if pat.search(line):
                    out.append((lineno, cat, line.strip()))
    return out


def count_all():
    total = {k: 0 for k in PATTERNS}
    for fpath in _iter_c_files():
        for cat, n in count_file(fpath).items():
            total[cat] += n
    return total


def read_baseline():
    if not os.path.exists(BASELINE_FILE):
        return None
    with open(BASELINE_FILE) as f:
        return json.load(f)


def write_baseline(counts):
    data = {cat: counts[cat] for cat in SOFT_CATEGORIES}
    with open(BASELINE_FILE, 'w') as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write('\n')


def changed_c_files():
    """Union of staged, unstaged-tracked, and untracked .c files under src/."""
    files = set()
    cmds = (
        ['git', 'diff', '--name-only', '--diff-filter=ACMR', 'HEAD'],
        ['git', 'ls-files', '--others', '--exclude-standard'],
    )
    for cmd in cmds:
        try:
            out = subprocess.run(cmd, cwd=ROOT_DIR, capture_output=True,
                                 text=True, check=False).stdout
        except Exception:
            continue
        for rel in out.splitlines():
            rel = rel.strip()
            if rel.endswith('.c') and (rel.startswith('src/') or '/src/' in rel):
                files.add(os.path.join(ROOT_DIR, rel))
    return sorted(p for p in files if os.path.exists(p))


def mode_check(as_json):
    current = count_all()
    baseline = read_baseline()

    if baseline is None:
        write_baseline(current)
        msg = {cat: current[cat] for cat in SOFT_CATEGORIES}
        if as_json:
            print(json.dumps({'initialized': msg}))
        else:
            print(f'readability baseline initialized: {msg}')
        return 0

    lowered, grew = {}, {}
    for cat in SOFT_CATEGORIES:
        base = baseline.get(cat)
        cur = current[cat]
        if base is None or cur < base:
            lowered[cat] = (base, cur)
        elif cur > base:
            grew[cat] = (base, cur)

    # Ratchet is a per-category low-water-mark: lock any decrease, keep the lower
    # target for categories that grew (so recovery still has a goal to beat).
    if lowered:
        new_baseline = {cat: min(baseline.get(cat, current[cat]), current[cat])
                        for cat in SOFT_CATEGORIES}
        write_baseline(new_baseline)

    if as_json:
        print(json.dumps({'current': {c: current[c] for c in SOFT_CATEGORIES},
                          'baseline': baseline, 'lowered': lowered, 'grew': grew}))
        return 0

    for cat, (base, cur) in lowered.items():
        print(f'  {cat}: {base} -> {cur} (baseline lowered, win locked)')
    for cat, (base, cur) in grew.items():
        print(f'  WARN {cat}: {base} -> {cur} (+{cur - base}) -- consider '
              f'naming the callee / recovering the struct field')
    if not lowered and not grew:
        print('  readability debt unchanged: '
              + ', '.join(f'{c}={current[c]}' for c in SOFT_CATEGORIES))
    # Soft categories never block.
    return 0


def mode_changed_only(as_json):
    files = changed_c_files()
    result = {}
    total = 0
    for fpath in files:
        fnd = findings_file(fpath)
        if fnd:
            rel = os.path.relpath(fpath, ROOT_DIR)
            result[rel] = fnd
            total += len(fnd)

    if as_json:
        print(json.dumps({rel: [{'line': l, 'category': c, 'text': t}
                                 for (l, c, t) in v]
                          for rel, v in result.items()}))
        return 1 if total else 0

    if not result:
        print('readability: no raw-cast / FUN_ / offset-deref findings in '
              'touched files')
        return 0

    for rel, fnd in result.items():
        print(f'\n{rel}:')
        for lineno, cat, text in fnd:
            tag = 'HARD' if cat == 'raw_fnptr_cast' else 'soft'
            print(f'  {rel}:{lineno}  [{tag}:{cat}]  {text[:100]}')
    print(f'\n{total} readability finding(s) in {len(result)} touched file(s). '
          'raw_fnptr_cast is a hard gate (check_raw_casts.py); fun_call and '
          'raw_offset_deref are advisory -- name the callee or recover the '
          'struct field where you can.')
    return 1


def main():
    argv = sys.argv[1:]
    as_json = '--json' in argv

    if '--update' in argv:
        write_baseline(count_all())
        print(f'readability baseline updated: {read_baseline()}')
        return 0
    if '--changed-only' in argv:
        return mode_changed_only(as_json)
    # default / --check
    return mode_check(as_json)


if __name__ == '__main__':
    sys.exit(main())
