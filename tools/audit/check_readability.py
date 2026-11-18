#!/usr/bin/env python3
"""
Legacy VC71 score fields describe mnemonic similarity, NOT raw byte accuracy.

Readability ratchet + per-file advisory for lifted C (readable-lift Phase 3).

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
  untyped_producer *(T *)(p + 0xNN) where p HARD -- p came from an untyped cast
                   came from `(char *)f(...)`      of a call, so the struct type was
                                                   lost at f's kb.json RETURN decl,
                                                   not at this line. Typing that one
                                                   decl types every caller at once
                                                   and is codegen-neutral (a pointer
                                                   return is EAX either way). Gated
                                                   on ADDED lines only, so existing
                                                   sites never block an edit.

Modes:
  --check           Global ratchet over src/. Auto-lowers the soft baseline on any
                    decrease (locks the win). WARNS if a soft category grew but
                    exits 0 (non-blocking) -- normal lifts add FUN_ calls and
                    offset derefs before recovery pays them down. Raw fn-ptr casts
                    are NOT gated here; run check_raw_casts.py for that hard gate.
  --untyped-producer
                    Repo-wide census ranking producers by the offset-deref debt
                    downstream of their untyped return. The kb.json worklist,
                    ordered by leverage. Read-only.
  --untyped-producer-added
                    Report only sites on lines ADDED by the staged diff.
                    Exits 1 if any -- this is the pre-commit hard gate.
  --self-test       Unit-check the scope walker.
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

# Same shape as raw_offset_deref, but capturing the base identifier so we can
# look up its declared type and decide whether the struct is already recovered.
OFFSET_BASE_RE = re.compile(
    r'\*\([A-Za-z_][\w ]*\*\)\(([A-Za-z_]\w*) \+ 0x[0-9a-fA-F]+\)')

# `} foo_t;` / `} foo_t, *pfoo;` -- the tail of a typedef'd struct/union.
STRUCT_TYPEDEF_RE = re.compile(r'^\}\s*([A-Za-z_]\w*)\s*[,;]', re.MULTILINE)

# A local/param declared as a pointer to a recovered struct: `foo_t *p`,
# `const foo_t *p`, `foo_t * restrict p`. Deliberately NOT matching `char *`,
# `void *` or any non-typedef'd type -- those are the un-recovered cases that
# raw_offset_deref already tracks as soft debt.
TYPED_PTR_DECL_RE = re.compile(
    r'\b(?:const\s+)?([A-Za-z_]\w*_t)\s*\*\s*(?:restrict\s*)?([A-Za-z_]\w*)')

# `char *obj = (char *)object_get_and_verify_type(h);` and the bare-assignment
# form `obj = (char *)object_get(h);` -- an untyped pointer produced by a call.
UNTYPED_PRODUCER_RE = re.compile(
    r'(?:\b(?:char|void|uint8_t|int8_t|unsigned\s+char)\s*\*\s*)?'
    r'\b([A-Za-z_]\w*)\s*=\s*'
    r'\(\s*(?:const\s+)?(?:char|void|uint8_t|int8_t|unsigned\s+char)\s*\*\s*\)\s*'
    r'([A-Za-z_]\w*)\s*\(')

# Categories the ratchet baseline owns. raw_fnptr_cast is HARD-gated elsewhere
# (check_raw_casts.py) so it is intentionally NOT ratcheted here.
SOFT_CATEGORIES = ('fun_call', 'raw_offset_deref')


def _iter_c_files():
    # .h as well as .c: header recovery (readability-ladder §8) moves real
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


# ---------------------------------------------------------------------------
# untyped_producer: an accessor whose result is held as char*/void* and then
# raw-offset-dereffed by its callers.
#
# This is where struct types are actually lost. A raw offset on a base that is
# ALREADY declared `foo_t *` does not exist and cannot exist -- `p + 0x10` on a
# struct pointer scales by sizeof, so the C type system rules it out. What the
# tree has instead is `char *obj = (char *)object_get_and_verify_type(h)`: the
# producer's kb.json decl returns void*/char*, so every caller holds an untyped
# pointer and raw offsets are the only spelling available.
#
# Typing ONE producer decl therefore types every one of its callers at once, and
# a pointer return is EAX either way, so it is codegen-neutral. This census ranks
# producers by how much offset-deref debt each one is upstream of -- the kb.json
# worklist, ordered by leverage.
# ---------------------------------------------------------------------------

_known_structs_cache = None


def known_struct_types():
    """Set of typedef'd struct/union names defined anywhere under src/."""
    global _known_structs_cache
    if _known_structs_cache is not None:
        return _known_structs_cache
    names = set()
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fname in filenames:
            if not fname.endswith('.h'):
                continue
            with open(os.path.join(dirpath, fname), 'r', errors='replace') as f:
                names.update(STRUCT_TYPEDEF_RE.findall(f.read()))
    _known_structs_cache = {n for n in names if n.endswith('_t')}
    return _known_structs_cache


def _blank_noncode(lines):
    """Return `lines` with block comments, // comments, string and char literals
    blanked out, preserving line count and column positions.

    Brace counting and declaration scanning both run on this, so a `{` inside a
    comment or a `'{'` literal can never open a scope. Getting this wrong is not
    cosmetic: a single unbalanced brace in a comment keeps one function's scope
    alive for thousands of lines and turns every later `char *obj` deref into a
    false struct_bypass report.
    """
    out = []
    in_block = False
    for raw in lines:
        buf = []
        i, n = 0, len(raw)
        while i < n:
            c = raw[i]
            if in_block:
                if raw.startswith('*/', i):
                    in_block = False
                    buf.append('  ')
                    i += 2
                else:
                    buf.append(' ')
                    i += 1
                continue
            if raw.startswith('/*', i):
                in_block = True
                buf.append('  ')
                i += 2
                continue
            if raw.startswith('//', i):
                buf.append(' ' * (n - i))
                break
            if c in ('"', "'"):
                quote = c
                buf.append(' ')
                i += 1
                while i < n:
                    if raw[i] == '\\':
                        buf.append('  ')
                        i += 2
                        continue
                    if raw[i] == quote:
                        buf.append(' ')
                        i += 1
                        break
                    buf.append(' ')
                    i += 1
                continue
            buf.append(c)
            i += 1
        out.append(''.join(buf))
    return out


def _scan_decls(text, known, out):
    """Add every `foo_t *p` declaration in `text` to the `out` scope map."""
    for typ, ident in TYPED_PTR_DECL_RE.findall(text):
        if typ in known:
            out.setdefault(ident, typ)


def struct_bypass_findings(fpath):
    """[(lineno, base_ident, producer_callee, line_text)] for one file.

    Finds raw offset derefs whose base pointer came from an untyped cast of a
    call -- i.e. the producer's return type is where the struct was lost.

    The identifier->producer map is FUNCTION-scoped, not file-scoped. units.c
    alone declares `unit` as `char *` in 101 functions; a file-wide map would
    attribute every one of them to whichever producer happened to be seen first.
    Scope resets at each top-level `{`.
    """
    with open(fpath, 'r', errors='replace') as f:
        raw_lines = f.read().splitlines()
    code = _blank_noncode(raw_lines)

    out = []
    depth = 0
    scope = {}          # ident -> producer callee name
    cond_stack = []     # depth at each open #if, so #else can rewind to it

    for idx, line in enumerate(code):
        lineno = idx + 1
        # Preprocessor directives do not open or close C scopes; a
        # `#define X { ... }` would otherwise unbalance the whole file.
        stripped = line.lstrip()
        is_directive = stripped.startswith('#')

        if is_directive:
            # Sibling #if/#else branches are mutually exclusive, but a linear
            # scan sees BOTH. objects.c's MSVC-vs-clang asm guard opens a brace
            # in each arm and closes it once; counting both leaves depth
            # permanently +1 and leaks one function's scope over the next 3000
            # lines. Rewind to the #if's depth at each sibling branch.
            directive = (stripped[1:].lstrip().split(None, 1) or [''])[0]
            if directive in ('if', 'ifdef', 'ifndef'):
                cond_stack.append(depth)
            elif directive in ('else', 'elif'):
                if cond_stack:
                    depth = cond_stack[-1]
            elif directive == 'endif':
                if cond_stack:
                    cond_stack.pop()

        if depth > 0:
            for ident, callee in UNTYPED_PRODUCER_RE.findall(line):
                scope[ident] = callee
            for base in OFFSET_BASE_RE.findall(line):
                if base in scope:
                    out.append((lineno, base, scope[base],
                                raw_lines[idx].strip()))

        if is_directive:
            continue
        opens = line.count('{')
        closes = line.count('}')
        if depth == 0 and opens:
            scope = {}
        depth += opens - closes
        if depth <= 0:
            depth = 0
            scope = {}
    return out


def _self_test_struct_bypass():
    """Guard the scope walker against the comment / char-literal / #if-#else
    leaks that made the first version report 1107 sites, 1007 of them false."""
    import tempfile
    src = """
/* A comment with an unbalanced { brace and a 'quote. */
void a(int h)
{
  char *obj = (char *)object_get(h);
  x = *(int *)(obj + 0x10);     // FINDING: producer object_get
}

char brace_literal(void) { return '{'; }

void b(int h)
{
  char *obj = local_buffer;
  y = *(int *)(obj + 0x10);     // not a finding: no producer call
}

void c(int h)
{
  char *g = (char *)widget_get(h);
#if defined(_MSC_VER) && !defined(__clang__)
  if (p) {
#else
  if (q) {
#endif
    z = *(int *)(g + 0x4);      // FINDING: producer widget_get
  }
}
"""
    failures = 0
    with tempfile.NamedTemporaryFile('w', suffix='.c', delete=False) as f:
        f.write(src)
        path = f.name
    try:
        found = struct_bypass_findings(path)
    finally:
        os.unlink(path)

    want = [('obj', 'object_get'), ('g', 'widget_get')]
    got = [(f[1], f[2]) for f in found]
    if got != want:
        print(f'  FAIL untyped_producer: expected {want}, got {got} ({found})')
        failures += 1
    else:
        print('  ok   untyped_producer scope walker '
              '(comment / char-literal / non-call / #if-#else cases)')
    return failures


def _added_lines_by_file():
    """{repo-relative path: set(line numbers added by the staged diff)}."""
    try:
        out = subprocess.run(
            ['git', 'diff', '--cached', '--unified=0', '--diff-filter=ACMR'],
            cwd=ROOT_DIR, capture_output=True, text=True, check=False).stdout
    except Exception:
        return {}
    added, cur, lineno = {}, None, 0
    for line in out.splitlines():
        if line.startswith('+++ b/'):
            cur = line[6:].strip()
            continue
        if line.startswith('@@'):
            m = re.search(r'\+(\d+)', line)
            lineno = int(m.group(1)) if m else 0
            continue
        if cur and line.startswith('+') and not line.startswith('+++'):
            added.setdefault(cur, set()).add(lineno)
            lineno += 1
    return added


def mode_untyped_producer(as_json, added_only):
    """Rank the producers whose untyped return is upstream of offset-deref debt.

    `--added` narrows to lines the staged diff adds, which is the pre-commit
    gate: a NEW offset deref on a pointer from an already-known producer means
    the decl should have been typed first.
    """
    added = _added_lines_by_file() if added_only else None
    if added_only:
        files = sorted(os.path.join(ROOT_DIR, r) for r in added
                       if (r.endswith('.c') or r.endswith('.h'))
                       and os.path.exists(os.path.join(ROOT_DIR, r)))
    else:
        files = sorted(_iter_c_files())

    by_producer = {}
    total = 0
    for fpath in files:
        rel = os.path.relpath(fpath, ROOT_DIR)
        fnd = struct_bypass_findings(fpath)
        if added_only:
            allowed = added.get(rel, set())
            fnd = [f for f in fnd if f[0] in allowed]
        for lineno, base, callee, text in fnd:
            rec = by_producer.setdefault(callee, {'sites': 0, 'files': set(),
                                                  'offsets': set(),
                                                  'examples': []})
            rec['sites'] += 1
            rec['files'].add(rel)
            m = re.search(r'\+ (0x[0-9a-fA-F]+)\)', text)
            if m:
                rec['offsets'].add(m.group(1).lower())
            if len(rec['examples']) < 3:
                rec['examples'].append(f'{rel}:{lineno}  {text[:90]}')
            total += 1

    if as_json:
        print(json.dumps({k: {'sites': v['sites'],
                              'files': sorted(v['files']),
                              'offsets': sorted(v['offsets']),
                              'examples': v['examples']}
                          for k, v in by_producer.items()}, indent=2))
        return 1 if (added_only and total) else 0

    if not by_producer:
        print('untyped_producer: none'
              + (' on added lines' if added_only else ' repo-wide'))
        return 0

    ranked = sorted(by_producer.items(), key=lambda kv: -kv[1]['sites'])
    print(f'{"producer":<44} {"sites":>6} {"files":>6} {"offsets":>8}')
    print('-' * 68)
    for callee, rec in ranked:
        print(f'{callee:<44} {rec["sites"]:>6} {len(rec["files"]):>6} '
              f'{len(rec["offsets"]):>8}')
    print('-' * 68)
    print(f'{"TOTAL":<44} {total:>6} '
          f'{len(set().union(*(r["files"] for r in by_producer.values()))):>6}')

    print('\nTop producers, with examples:')
    for callee, rec in ranked[:5]:
        print(f'\n  {callee}  ({rec["sites"]} sites, '
              f'{len(rec["offsets"])} distinct offsets)')
        for ex in rec['examples']:
            print(f'      {ex}')

    if added_only:
        print(f'\npre-commit BLOCKED: {total} added line(s) raw-offset-deref a '
              'pointer from a known producer.')
        print('Type the producer\'s return in kb.json (a pointer return is EAX '
              'either way, so it is codegen-neutral), then use `p->field`. '
              'Bypass with --no-verify.')
        return 1
    print('\nEach producer is ONE kb.json decl. Typing its return types every '
          'caller at once.')
    return 0


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
    if '--self-test' in argv:
        return 1 if _self_test_struct_bypass() else 0
    if '--untyped-producer-added' in argv:
        return mode_untyped_producer(as_json, added_only=True)
    if '--untyped-producer' in argv:
        return mode_untyped_producer(as_json, added_only=False)
    if '--changed-only' in argv:
        return mode_changed_only(as_json)
    # default / --check
    return mode_check(as_json)


if __name__ == '__main__':
    sys.exit(main())
