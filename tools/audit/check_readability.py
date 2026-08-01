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
  --report-by-object
                    Aggregate the per-file findings per kb.json OBJECT (the
                    translation unit a recovery campaign is scoped to) and print
                    a debt table sorted by total findings. Read-only; touches no
                    baseline. `--json [path]` writes/prints the machine-readable
                    form (consumed by tools/recovery/recovery_frontier.py).
  --json            Machine-readable output.

Usage:
    python3 tools/audit/check_readability.py --check
    python3 tools/audit/check_readability.py --changed-only
    python3 tools/audit/check_readability.py --report-by-object
    python3 tools/audit/check_readability.py --report-by-object --json debt.json
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
KB_FILE = os.path.join(ROOT_DIR, 'kb.json')
VC71_SCORES_FILE = os.path.join(ROOT_DIR, 'tools', 'verify', 'vc71_scores.json')

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
    # .h as well as .c: header recovery (the header-recovery skill) moves real
    # code out of .c files into recovered headers -- hs_library_internal_runtime.h
    # alone took 75 raw offset derefs with it. Scanning only .c would let that
    # debt vanish from the ratchet and silently lower the baseline, making a
    # pure file move look like a recovery win.
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if fname.endswith('.c') or fname.endswith('.h'):
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


# ---------------------------------------------------------------------------
# --report-by-object: attribute per-file debt to the kb.json object (TU) that
# owns the code, so a recovery campaign can be scoped to one translation unit.
# ---------------------------------------------------------------------------

def _norm_source(path):
    """Normalize a kb.json source reference to a repo-relative path.

    kb.json stores three flavours: 'ai/actors.c' (relative to src/halo),
    'src/halo/ai/actors.c' (repo-relative) and 'halo/items/weapons.c'
    (relative to src). Prefer whichever candidate exists on disk.
    """
    if not path:
        return None
    path = path.replace('\\', '/').lstrip('./')
    if path.startswith('src/'):
        return path
    candidates = ('src/halo/' + path, 'src/' + path)
    for cand in candidates:
        if os.path.exists(os.path.join(ROOT_DIR, cand)):
            return cand
    return candidates[0]


def _function_name(func):
    """Best-effort function name for a kb.json function entry."""
    if func.get('name'):
        return func['name']
    decl = (func.get('decl') or '').split('(')[0]
    idents = re.findall(r'[A-Za-z_][A-Za-z0-9_:]*', decl)
    return idents[-1] if idents else None


def _load_vc71_sources():
    """function name -> source file, from tools/verify/vc71_scores.json."""
    try:
        with open(VC71_SCORES_FILE) as f:
            data = json.load(f)
    except (OSError, ValueError):
        return {}
    out = {}
    for name, rec in (data.get('scores') or {}).items():
        if isinstance(rec, dict) and rec.get('source'):
            out[name] = rec['source']
    return out


def object_function_files(kb=None):
    """Return {source_path: {object_name: n_functions}} derived from kb.json.

    Resolution order per function: its own source_path/src/file/source, then the
    file vc71_scores.json compiled it from, then the owning object's source.
    """
    if kb is None:
        with open(KB_FILE) as f:
            kb = json.load(f)
    vc71 = _load_vc71_sources()
    files = {}
    for obj in kb.get('objects', []):
        name = obj.get('name') or '(unnamed)'
        obj_src = _norm_source(obj.get('source_path') or obj.get('src')
                               or obj.get('source'))
        for func in obj.get('functions', []):
            path = _norm_source(func.get('source_path') or func.get('src')
                                or func.get('file') or func.get('source'))
            if not path:
                fname = _function_name(func)
                if fname and fname in vc71:
                    path = _norm_source(vc71[fname])
            if not path:
                path = obj_src
            if not path:
                continue
            files.setdefault(path, {})
            files[path][name] = files[path].get(name, 0) + 1
    return files


def file_object_map(kb=None):
    """Return (owner, detail) where owner maps source_path -> object name.

    A .c file can host functions from several objects (kb.json models the
    original TU split, we model the file layout). The majority owner takes the
    file's findings; `detail` keeps the full breakdown so multi-object files can
    be reported -- they complicate per-object campaigns.
    """
    detail = object_function_files(kb)
    owner = {}
    for path, objs in detail.items():
        owner[path] = max(sorted(objs), key=lambda o: objs[o])
    return owner, detail


def report_by_object(kb=None):
    """Aggregate readability findings per kb.json object."""
    owner, detail = file_object_map(kb)
    objects = {}
    unmapped = {'files': [], 'counts': {k: 0 for k in PATTERNS}}

    for fpath in sorted(_iter_c_files()):
        rel = os.path.relpath(fpath, ROOT_DIR).replace('\\', '/')
        counts = count_file(fpath)
        obj = owner.get(rel)
        if obj is None:
            if any(counts.values()):
                unmapped['files'].append(rel)
                for cat, n in counts.items():
                    unmapped['counts'][cat] += n
            continue
        rec = objects.setdefault(obj, {
            'object': obj,
            'files': [],
            'multi_object_files': [],
            'funcs': 0,
            'counts': {k: 0 for k in PATTERNS},
        })
        rec['files'].append(rel)
        rec['funcs'] += detail[rel].get(obj, 0)
        for cat, n in counts.items():
            rec['counts'][cat] += n
        if len(detail[rel]) > 1:
            rec['multi_object_files'].append(
                {'file': rel, 'objects': dict(sorted(detail[rel].items()))})

    for rec in objects.values():
        rec['total'] = sum(rec['counts'].values())
        rec['debt_per_func'] = (rec['total'] / rec['funcs']) if rec['funcs'] else 0.0

    multi_files = sorted(p for p, objs in detail.items() if len(objs) > 1
                         and os.path.exists(os.path.join(ROOT_DIR, p)))
    return {
        'objects': objects,
        'unmapped': unmapped,
        'multi_object_files': [
            {'file': p, 'objects': dict(sorted(detail[p].items()))}
            for p in multi_files
        ],
    }


def mode_report_by_object(json_path, as_json):
    report = report_by_object()
    rows = sorted(report['objects'].values(),
                  key=lambda r: (-r['total'], -r['debt_per_func'], r['object']))
    payload = {
        'objects': {r['object']: r for r in rows},
        'ranked': [r['object'] for r in rows],
        'unmapped': report['unmapped'],
        'multi_object_files': report['multi_object_files'],
    }

    if json_path:
        with open(json_path, 'w') as f:
            json.dump(payload, f, indent=2, sort_keys=True)
            f.write('\n')
        print(f'readability debt by object written to {json_path} '
              f'({len(rows)} objects)')
        return 0
    if as_json:
        print(json.dumps(payload, indent=2, sort_keys=True))
        return 0

    hdr = (f"{'object':<34} {'files':>5} {'funcs':>5} {'cast':>5} {'FUN_':>6} "
           f"{'deref':>6} {'total':>6} {'/func':>6}")
    print('Readability debt by object')
    print('-' * len(hdr))
    print(hdr)
    print('-' * len(hdr))
    tot = {k: 0 for k in PATTERNS}
    for r in rows:
        c = r['counts']
        for k in tot:
            tot[k] += c[k]
        flag = ' *' if r['multi_object_files'] else ''
        print(f"{r['object']:<34} {len(r['files']):>5} {r['funcs']:>5} "
              f"{c['raw_fnptr_cast']:>5} {c['fun_call']:>6} "
              f"{c['raw_offset_deref']:>6} {r['total']:>6} "
              f"{r['debt_per_func']:>6.1f}{flag}")
    print('-' * len(hdr))
    print(f"{'TOTAL (' + str(len(rows)) + ' objects)':<34} {'':>5} {'':>5} "
          f"{tot['raw_fnptr_cast']:>5} {tot['fun_call']:>6} "
          f"{tot['raw_offset_deref']:>6} {sum(tot.values()):>6}")

    if report['multi_object_files']:
        print(f"\n* multi-object files ({len(report['multi_object_files'])}) "
              '-- findings attributed to the majority owner:')
        for m in report['multi_object_files']:
            share = ', '.join(f'{o}={n}' for o, n in m['objects'].items())
            print(f"    {m['file']}: {share}")

    um = report['unmapped']
    if um['files']:
        print(f"\nunmapped files with findings ({len(um['files'])}): "
              + ', '.join(f'{k}={v}' for k, v in sorted(um['counts'].items())))
        for rel in um['files'][:10]:
            print(f'    {rel}')
        if len(um['files']) > 10:
            print(f"    ... and {len(um['files']) - 10} more")
    return 0


def _arg_value(argv, flag):
    """Return the token after `flag` if it is not another flag, else None."""
    if flag not in argv:
        return None
    i = argv.index(flag)
    if i + 1 < len(argv) and not argv[i + 1].startswith('-'):
        return argv[i + 1]
    return None


def main():
    argv = sys.argv[1:]
    as_json = '--json' in argv

    if '--report-by-object' in argv:
        return mode_report_by_object(_arg_value(argv, '--json'), as_json)
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
