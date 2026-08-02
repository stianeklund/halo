#!/usr/bin/env python3
"""Rank kb.json objects (translation units) for readability/type-recovery campaigns.

Where tools/analysis/frontier.py answers "what should I LIFT next", this answers
"what should I RECOVER next" -- which already-implemented TU gives the most
readability payback per session while keeping a hard byte-match safety net.

The hard rule for a recovery campaign is: never risk a VC71 byte-match
regression, only improve. Everything below is in service of that rule.

Eligibility (hard gates, evaluated in this order; the first failure is the
reported reason):

  1. no_source_files   The object has no .c file it majority-owns, so there is
                       nothing to recover.
  2. no_delink         `Delink? != true` in tools/objects.csv. Without a delinked
                       reference object there is no byte oracle, so a recovery
                       edit cannot be proven codegen-neutral. Non-negotiable.
  3. not_fully_ported  Some function of the object still has `ported != true` in
                       kb.json. Recovery is for implemented code; a half-lifted
                       TU should finish lifting first (that is frontier.py's
                       job), and unported neighbours make whole-TU VC71
                       regression gating noisy.
  4. deactivated       A function of the object appears in
                       tools/audit/deactivation_allowlist.json. Those entries are
                       code that is deliberately switched off (bisect/dormant);
                       the TU's runtime behaviour is not fully exercised, so it
                       is a riskier campaign target.

Score (eligible objects only) -- all three factors are deterministic and every
constant is tunable at the top of this module:

    value  = (raw_fnptr_cast + fun_call + raw_offset_deref) / funcs
             Debt density: findings per function. How much there is to win.

    safety = |{f : min(vc71_score(f), vc71_opnd(f)) >= HIGH_MATCH}| / funcs
             Fraction of the TU's functions that currently sit at (near-)
             byte-identical match. Those are the functions whose match % is a
             sharp instrument: any codegen drift a recovery edit introduces shows
             up immediately, so the "only improve" rule is mechanically
             enforceable. Functions with NO vc71 score count against safety --
             an unmeasured function is an unguarded one.

             BOTH the mnemonic and the operand-normalized match must clear
             HIGH_MATCH. A function can carry the reference's instruction
             skeleton while its operands diverge, and such a function is not a
             sharp instrument -- it cannot detect an operand regression it
             already has. This excludes 74 of the 1312 functions that clear
             HIGH_MATCH on mnemonics alone (~5.6%).

             A high mnemonic score against a low operand score also flags a BAD
             DELINKED REFERENCE (truncated, stale, or whole-object section-0),
             which is likewise not something to gate a cleanup campaign on.

             CAVEAT (2026-08-02): opnd_percent is a LOWER BOUND, not a clean
             operand-fidelity measure. compare_obj.canonicalize_registers()
             aliases registers by global first-appearance order, so a candidate
             and reference that introduce register families in a different order
             score near-zero on operands even when the lift is correct (measured
             on FUN_00017ab0: 17.1% canonicalized vs 58.8% identity-mapped).
             Excluding such a function costs us a real gate. That is the
             conservative direction -- it under-counts safety rather than
             over-counting it -- so this test is still the right one to ship,
             but the exclusions are suspects to investigate, not proven defects.
             Only 7 of the 74 excluded here carry an @<reg> decl, the known
             systematic trigger.

             When no operand data exists at all the test degrades to
             mnemonic-only rather than zeroing every score.

    size   = log2(funcs + SIZE_LOG_OFFSET)
             Mild preference for smaller TUs; a campaign is a per-object session
             and a 237-function TU does not fit in one.

    score  = SCORE_SCALE * value * safety / size

The size penalty is deliberately mild (log2), so a tiny fully-recovered-adjacent
TU can out-rank a large one on density alone. `--min-funcs N` hides objects
below N functions from the ranking without touching the score, for when you want
a campaign-sized target rather than a four-line fix.

The `header?` column is INFORMATIONAL ONLY (never scored): whether the object's
subsystem already has a binary-proven recovered header (the 2026-07-24 sweep
recorded in .claude/skills/header-recovery/SKILL.md). A subsystem with a proven
header can move recovered structs into it; one without must leave them in
types.h, which caps how much of the debt is actually payable.

Usage:
    python3 tools/recovery/recovery_frontier.py --limit 15
    python3 tools/recovery/recovery_frontier.py --json artifacts/recovery.json
    python3 tools/recovery/recovery_frontier.py --debt artifacts/debt.json
    python3 tools/recovery/recovery_frontier.py --self-test
"""
import argparse
import csv
import json
import math
import os
import re
import sys

_TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _TOOLS_DIR not in sys.path:
    sys.path.insert(0, _TOOLS_DIR)

from audit.check_readability import report_by_object  # noqa: E402

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..'))
KB_FILE = os.path.join(ROOT_DIR, 'kb.json')
OBJECTS_CSV = os.path.join(ROOT_DIR, 'tools', 'objects.csv')
VC71_SCORES = os.path.join(ROOT_DIR, 'tools', 'verify', 'vc71_scores.json')
# Honest current slice.  Untracked / locally regenerated, so it may be absent;
# it is the only source with full operand-normalized coverage.
VC71_CURRENT = os.path.join(ROOT_DIR, 'tools', 'verify', 'vc71_current.json')
ALLOWLIST = os.path.join(ROOT_DIR, 'tools', 'audit', 'deactivation_allowlist.json')

# --- tunables --------------------------------------------------------------
HIGH_MATCH = 99.0        # vc71 % at/above which a function is a sharp gate
SIZE_LOG_OFFSET = 2.0    # log2(funcs + offset) size penalty
SCORE_SCALE = 100.0      # cosmetic multiplier so scores read as 0..N
DEBT_CATEGORIES = ('raw_fnptr_cast', 'fun_call', 'raw_offset_deref')

GATES = ('no_source_files', 'no_delink', 'not_fully_ported', 'deactivated')

# Binary-proven recovered headers, 2026-07-24 sweep (13 total).
# Directory-pinned (path proven from a __FILE__ string):
RECOVERED_HEADERS = (
    'src/halo/ai/actor_type_definitions.h',
    'src/halo/ai/encounters.h',
    'src/halo/ai/path.h',
    'src/halo/hs/hs_library_internal_compile.h',
    'src/halo/hs/hs_library_internal_runtime.h',
    'src/halo/objects/objects.h',
    'src/halo/objects/widgets/widget_types.h',
    'src/halo/sound/sound_classes.h',
    'src/halo/sound/sound_definitions.h',
    'src/halo/sound/sound_dsound.h',
)
# Short-name hits whose directory is not yet pinned; located on disk at runtime.
RECOVERED_HEADER_BASENAMES = (
    'bitmaps_inlines.h',
    'real_math.h',
    'reference_lists.h',
)


# --- inputs ----------------------------------------------------------------

def load_objects_csv(path=OBJECTS_CSV):
    """Return {object: {'delink': bool, 'addr_range': str, 'func_count': int}}."""
    out = {}
    with open(path, newline='') as f:
        for row in csv.DictReader(f):
            name = (row.get('Object') or '').strip()
            if not name:
                continue
            try:
                count = int(row.get('func_count') or 0)
            except ValueError:
                count = 0
            out[name] = {
                'delink': (row.get('Delink?') or '').strip().lower() == 'true',
                'addr_range': (row.get('addr_range') or '').strip(),
                'func_count': count,
            }
    return out


def _read_scores(path):
    """Return the raw {name: record} score map from a vc71 scores file."""
    try:
        with open(path) as f:
            data = json.load(f)
    except (OSError, ValueError):
        return {}
    scores = data.get('scores')
    return scores if isinstance(scores, dict) else {}


def load_vc71_scores(path=VC71_SCORES, current_path=VC71_CURRENT):
    """Return {function_name: {'score': float, 'opnd': float|None}}.

    ``score`` is the mnemonic-LCS match from the committed floor.  ``opnd`` is
    the operand-normalized match, which is what actually decides whether a
    function is a *sharp* gate: a lift can carry the reference's instruction
    skeleton while getting nearly every operand wrong (worst observed in-repo:
    89.7% mnemonic against 17.1% operand).  Such a function cannot detect an
    operand regression it already has.

    Operand coverage lives almost entirely in the honest current slice, so it
    is overlaid from there when present and back-filled from the floor.  A
    ``None`` opnd means unmeasured, which ``build_rows`` treats as unguarded
    whenever operand data exists at all -- see ``opnd_available``.
    """
    out = {}
    for name, rec in _read_scores(path).items():
        if isinstance(rec, dict) and isinstance(rec.get('score'), (int, float)):
            opnd = rec.get('opnd_percent')
            out[name] = {
                'score': float(rec['score']),
                'opnd': float(opnd) if isinstance(opnd, (int, float)) else None,
            }
    for name, rec in _read_scores(current_path).items():
        if not isinstance(rec, dict):
            continue
        opnd = rec.get('opnd_percent')
        if not isinstance(opnd, (int, float)):
            continue
        if name in out:
            out[name]['opnd'] = float(opnd)
        elif isinstance(rec.get('score'), (int, float)):
            out[name] = {'score': float(rec['score']), 'opnd': float(opnd)}
    return out


def opnd_available(vc71):
    """True when any function carries an operand score.

    Gates the strictness of the sharp-gate test.  Without this the tool would
    silently zero every safety term on a checkout where the untracked current
    slice has never been generated.
    """
    return any(rec.get('opnd') is not None for rec in vc71.values())


def is_sharp_gate(rec, require_opnd):
    """True when a function's match % is a sharp instrument for cleanup work.

    Requires BOTH the mnemonic and operand match to sit at/above HIGH_MATCH.
    When no operand data exists anywhere (``require_opnd`` false) this degrades
    to the historical mnemonic-only test rather than failing closed on every
    function.
    """
    if rec['score'] < HIGH_MATCH:
        return False
    if not require_opnd:
        return True
    return rec['opnd'] is not None and rec['opnd'] >= HIGH_MATCH


def load_allowlist_objects(path=ALLOWLIST):
    """Return {object: n_deactivated_entries} from the deactivation allowlist."""
    try:
        with open(path) as f:
            data = json.load(f)
    except (OSError, ValueError):
        return {}
    entries = data if isinstance(data, list) else data.get('entries', [])
    out = {}
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        obj = entry.get('object')
        if obj:
            out[obj] = out.get(obj, 0) + 1
    return out


def function_name(func):
    """Best-effort function name for a kb.json function entry."""
    if func.get('name'):
        return func['name']
    decl = (func.get('decl') or '').split('(')[0]
    idents = re.findall(r'[A-Za-z_][A-Za-z0-9_:]*', decl)
    return idents[-1] if idents else None


def load_kb_objects(path=KB_FILE):
    """Return {object: {'funcs': [names], 'ported': n, 'total': n}} from kb.json."""
    with open(path) as f:
        kb = json.load(f)
    out = {}
    for obj in kb.get('objects', []):
        name = obj.get('name') or '(unnamed)'
        funcs = obj.get('functions', [])
        rec = out.setdefault(name, {'funcs': [], 'ported': 0, 'total': 0})
        for func in funcs:
            rec['total'] += 1
            if func.get('ported'):
                rec['ported'] += 1
            fname = function_name(func)
            if fname:
                rec['funcs'].append(fname)
    return out


def recovered_header_dirs(root=ROOT_DIR):
    """Directories (repo-relative) known to own a binary-proven recovered header."""
    dirs = set()
    for rel in RECOVERED_HEADERS:
        dirs.add(os.path.dirname(rel))
    for base in RECOVERED_HEADER_BASENAMES:
        for dirpath, _, filenames in os.walk(os.path.join(root, 'src')):
            if base in filenames:
                dirs.add(os.path.relpath(dirpath, root).replace('\\', '/'))
    return dirs


def has_recovered_header(files, header_dirs):
    """True if any of the object's source files sits in a recovered-header dir."""
    for rel in files:
        if os.path.dirname(rel).replace('\\', '/') in header_dirs:
            return True
    return False


# --- scoring ---------------------------------------------------------------

def score_object(debt_total, funcs, high_match_funcs):
    """Return (score, value, safety, size_penalty). See module docstring."""
    if funcs <= 0:
        return 0.0, 0.0, 0.0, 1.0
    value = debt_total / float(funcs)
    safety = high_match_funcs / float(funcs)
    size_penalty = math.log2(funcs + SIZE_LOG_OFFSET)
    score = SCORE_SCALE * value * safety / size_penalty
    return score, value, safety, size_penalty


def classify(name, debt, csv_row, kb_row, allow_objects):
    """Return the first failing gate name, or None when eligible."""
    if debt is None or not debt.get('files'):
        return 'no_source_files'
    if csv_row is None or not csv_row['delink']:
        return 'no_delink'
    if kb_row is None or kb_row['total'] == 0 or kb_row['ported'] != kb_row['total']:
        return 'not_fully_ported'
    if name in allow_objects:
        return 'deactivated'
    return None


def build_rows(debt_report, csv_objects, kb_objects, vc71, allow_objects,
               header_dirs):
    eligible, ineligible = [], []
    require_opnd = opnd_available(vc71)
    debt_objects = debt_report.get('objects', {})
    names = set(debt_objects) | set(csv_objects) | set(kb_objects)

    for name in sorted(names):
        debt = debt_objects.get(name)
        csv_row = csv_objects.get(name)
        kb_row = kb_objects.get(name)
        gate = classify(name, debt, csv_row, kb_row, allow_objects)
        if gate is not None:
            ineligible.append({'object': name, 'gate': gate})
            continue

        counts = debt['counts']
        debt_total = sum(counts.get(c, 0) for c in DEBT_CATEGORIES)
        funcs = kb_row['total']
        scored = [vc71[f] for f in kb_row['funcs'] if f in vc71]
        high = sum(1 for rec in scored if is_sharp_gate(rec, require_opnd))
        # Mnemonic-only count, kept for reporting: the delta against `high` is
        # the set of functions that LOOK like gates but are operand-defective.
        high_mnemonic = sum(1 for rec in scored if rec['score'] >= HIGH_MATCH)
        score, value, safety, size_penalty = score_object(debt_total, funcs, high)

        eligible.append({
            'object': name,
            'files': sorted(debt['files']),
            'file_count': len(debt['files']),
            'funcs': funcs,
            'csv_func_count': csv_row['func_count'],
            'addr_range': csv_row['addr_range'],
            'measured_funcs': len(scored),
            'high_match_funcs': high,
            'high_match_pct': 100.0 * safety,
            'high_match_mnemonic_funcs': high_mnemonic,
            'opnd_defective_gates': high_mnemonic - high,
            'debt': {c: counts.get(c, 0) for c in DEBT_CATEGORIES},
            'debt_total': debt_total,
            'debt_per_func': value,
            'safety': safety,
            'size_penalty': size_penalty,
            'score': score,
            'recovered_header': has_recovered_header(debt['files'], header_dirs),
            'multi_object_files': [m['file'] for m in debt.get('multi_object_files', [])],
        })

    eligible.sort(key=lambda r: (-r['score'], r['object']))
    for rank, row in enumerate(eligible, 1):
        row['rank'] = rank
    ineligible.sort(key=lambda r: (GATES.index(r['gate']), r['object']))
    return eligible, ineligible


# --- output ----------------------------------------------------------------

def print_section(title):
    print(title)
    print('-' * len(title))


def print_table(rows, limit):
    hdr = (f"{'#':>3}  {'object':<32} {'files':>5} {'funcs':>5} {'%>=99':>6} "
           f"{'debt':>6} {'/func':>6}  {'hdr':<4} {'score':>7}")
    print(hdr)
    print('-' * len(hdr))
    for row in rows[:limit]:
        flag = '*' if row['multi_object_files'] else ' '
        print(f"{row['rank']:>3}  {row['object']:<32} "
              f"{row['file_count']:>4}{flag} {row['funcs']:>5} "
              f"{row['high_match_pct']:>5.0f}% {row['debt_total']:>6} "
              f"{row['debt_per_func']:>6.1f}  "
              f"{('yes' if row['recovered_header'] else '-'):<4} "
              f"{row['score']:>7.1f}")


def print_ineligible(ineligible):
    counts = {}
    for row in ineligible:
        counts[row['gate']] = counts.get(row['gate'], 0) + 1
    print(f'{len(ineligible)} object(s) excluded:')
    for gate in GATES:
        if gate in counts:
            print(f'  {gate:<18} {counts[gate]:>4}')


def run(args):
    debt_report = None
    if args.debt:
        with open(args.debt) as f:
            debt_report = json.load(f)
    else:
        debt_report = report_by_object()

    csv_objects = load_objects_csv()
    kb_objects = load_kb_objects()
    vc71 = load_vc71_scores()
    allow_objects = load_allowlist_objects()
    header_dirs = recovered_header_dirs()

    eligible, ineligible = build_rows(debt_report, csv_objects, kb_objects,
                                      vc71, allow_objects, header_dirs)

    hidden = 0
    if args.min_funcs > 0:
        keep = [r for r in eligible if r['funcs'] >= args.min_funcs]
        hidden = len(eligible) - len(keep)
        eligible = keep

    if args.json:
        payload = {
            'constants': {
                'high_match': HIGH_MATCH,
                'size_log_offset': SIZE_LOG_OFFSET,
                'score_scale': SCORE_SCALE,
            },
            'eligible': eligible,
            'ineligible': ineligible,
            'ineligible_by_gate': {
                gate: [r['object'] for r in ineligible if r['gate'] == gate]
                for gate in GATES
            },
        }
        with open(args.json, 'w') as f:
            json.dump(payload, f, indent=2, sort_keys=True)
            f.write('\n')
        print(f'recovery frontier written to {args.json} '
              f'({len(eligible)} eligible, {len(ineligible)} excluded)')
        return 0

    print_section('Recovery Frontier')
    print(f'score = {SCORE_SCALE:g} * (debt/func) '
          f'* (frac min(vc71, opnd) >= {HIGH_MATCH:g}%) '
          f'/ log2(funcs + {SIZE_LOG_OFFSET:g})')
    if not opnd_available(vc71):
        print('WARNING: no operand-normalized scores found; safety falls back '
              'to mnemonic-only and OVERSTATES how sharp the gates are. '
              'Regenerate tools/verify/vc71_current.json.')
    else:
        defective = sum(r['opnd_defective_gates'] for r in eligible)
        if defective:
            print(f'({defective} function(s) across eligible objects clear '
                  f'{HIGH_MATCH:g}% mnemonic but fail on operands -- not '
                  f'counted as gates; suspect lift defect or bad delinked ref)')
    print()
    print_table(eligible, args.limit)
    if len(eligible) > args.limit:
        print(f'... {len(eligible) - args.limit} more eligible object(s) '
              f'(--limit {len(eligible)} to see all)')
    if hidden:
        print(f'({hidden} eligible object(s) hidden by --min-funcs '
              f'{args.min_funcs})')
    print()
    print('* = object majority-owns a file shared with another object')
    print('hdr = subsystem has a binary-proven recovered header (informational)')
    print()
    print_section('Ineligible')
    print_ineligible(ineligible)
    return 0


# --- self-test -------------------------------------------------------------

def _self_test():
    import tempfile
    checks = []

    def check(name, passed):
        checks.append((name, bool(passed)))

    # objects.csv parsing
    with tempfile.NamedTemporaryFile('w', suffix='.csv', delete=False) as f:
        f.write('Object,Delink?,addr_range,func_count\n')
        f.write('a.obj,true,0x1000-0x2000,10\n')
        f.write('b.obj,false,0x2000-0x3000,5\n')
        f.write('c.obj,true,0x3000-0x4000,notanint\n')
        csv_path = f.name
    try:
        parsed = load_objects_csv(csv_path)
        check('csv: 3 rows', len(parsed) == 3)
        check('csv: delink true', parsed['a.obj']['delink'] is True)
        check('csv: delink false', parsed['b.obj']['delink'] is False)
        check('csv: func_count int', parsed['a.obj']['func_count'] == 10)
        check('csv: bad int -> 0', parsed['c.obj']['func_count'] == 0)
    finally:
        os.unlink(csv_path)

    # score math
    score, value, safety, size = score_object(100, 10, 10)
    check('score: value = debt/funcs', abs(value - 10.0) < 1e-9)
    check('score: safety = 1.0', abs(safety - 1.0) < 1e-9)
    check('score: size = log2(12)', abs(size - math.log2(12.0)) < 1e-9)
    check('score: formula', abs(score - (SCORE_SCALE * 10.0 / math.log2(12.0))) < 1e-9)
    check('score: zero funcs safe', score_object(50, 0, 0)[0] == 0.0)
    check('score: zero safety -> zero', score_object(100, 10, 0)[0] == 0.0)
    lo = score_object(100, 10, 10)[0]
    hi = score_object(100, 5, 5)[0]
    check('score: smaller TU wins at equal density',
          score_object(50, 5, 5)[0] > lo and hi > lo)
    check('score: more debt wins at equal size',
          score_object(200, 10, 10)[0] > lo)
    check('score: partial safety scales linearly',
          abs(score_object(100, 10, 5)[0] - lo / 2.0) < 1e-9)

    # sharp-gate test: operand match must clear HIGH_MATCH too
    sharp = {'score': 100.0, 'opnd': 100.0}
    skeleton = {'score': 99.5, 'opnd': 17.1}   # the FUN_00017ab0 signature
    unmeasured = {'score': 100.0, 'opnd': None}
    check('gate: both high -> sharp', is_sharp_gate(sharp, True))
    check('gate: low mnemonic -> not sharp',
          not is_sharp_gate({'score': 80.0, 'opnd': 100.0}, True))
    check('gate: high mnemonic + low operand -> NOT sharp',
          not is_sharp_gate(skeleton, True))
    check('gate: unmeasured operand -> not sharp when data exists',
          not is_sharp_gate(unmeasured, True))
    check('gate: degrades to mnemonic-only with no operand data anywhere',
          is_sharp_gate(unmeasured, False) and is_sharp_gate(skeleton, False))
    check('gate: availability probe',
          opnd_available({'a': sharp}) and not opnd_available({'a': unmeasured})
          and not opnd_available({}))

    # score loading: current-slice operand overlays the committed floor
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
        json.dump({'scores': {'A': {'score': 90.0},
                              'B': {'score': 80.0, 'opnd_percent': 70.0}}}, f)
        floor_path = f.name
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
        json.dump({'scores': {'A': {'score': 91.0, 'opnd_percent': 40.0},
                              'C': {'score': 60.0, 'opnd_percent': 55.0}}}, f)
        cur_path = f.name
    loaded = load_vc71_scores(floor_path, cur_path)
    check('load: floor score wins, current operand overlays',
          loaded['A']['score'] == 90.0 and loaded['A']['opnd'] == 40.0)
    check('load: floor-only operand preserved', loaded['B']['opnd'] == 70.0)
    check('load: current-only function admitted',
          loaded['C']['score'] == 60.0 and loaded['C']['opnd'] == 55.0)
    check('load: missing files are not fatal',
          load_vc71_scores('/nonexistent', '/nonexistent') == {})
    os.unlink(floor_path)
    os.unlink(cur_path)

    # eligibility
    debt_ok = {'files': ['src/halo/x/x.c'], 'counts': {c: 1 for c in DEBT_CATEGORIES}}
    csv_ok = {'delink': True, 'addr_range': '', 'func_count': 2}
    kb_ok = {'funcs': ['f1', 'f2'], 'ported': 2, 'total': 2}
    check('gate: eligible', classify('x.obj', debt_ok, csv_ok, kb_ok, {}) is None)
    check('gate: no_source_files (missing)',
          classify('x.obj', None, csv_ok, kb_ok, {}) == 'no_source_files')
    check('gate: no_source_files (empty)',
          classify('x.obj', {'files': [], 'counts': {}}, csv_ok, kb_ok, {})
          == 'no_source_files')
    check('gate: no_delink',
          classify('x.obj', debt_ok, {'delink': False, 'addr_range': '',
                                      'func_count': 2}, kb_ok, {}) == 'no_delink')
    check('gate: no_delink (absent from csv)',
          classify('x.obj', debt_ok, None, kb_ok, {}) == 'no_delink')
    check('gate: not_fully_ported',
          classify('x.obj', debt_ok, csv_ok,
                   {'funcs': ['f1', 'f2'], 'ported': 1, 'total': 2}, {})
          == 'not_fully_ported')
    check('gate: deactivated',
          classify('x.obj', debt_ok, csv_ok, kb_ok, {'x.obj': 3}) == 'deactivated')
    check('gate: order (delink beats ported)',
          classify('x.obj', debt_ok, {'delink': False, 'addr_range': '',
                                      'func_count': 2},
                   {'funcs': [], 'ported': 0, 'total': 2}, {'x.obj': 1})
          == 'no_delink')
    check('gate: every gate name is known',
          all(g in GATES for g in ('no_source_files', 'no_delink',
                                   'not_fully_ported', 'deactivated')))

    # function-name derivation
    check('name: from decl',
          function_name({'decl': 'void FUN_00113930(int s, int p);'})
          == 'FUN_00113930')
    check('name: explicit name wins',
          function_name({'name': 'real_thing', 'decl': 'int other(void);'})
          == 'real_thing')
    check('name: pointer return',
          function_name({'decl': 'struct foo_t *object_get(int handle);'})
          == 'object_get')
    check('name: namespaced',
          function_name({'decl': 'int D3D8::Res_IsBusy(void);'})
          == 'D3D8::Res_IsBusy')

    # header flag
    dirs = {'src/halo/ai', 'src/halo/sound'}
    check('header: match', has_recovered_header(['src/halo/ai/actors.c'], dirs))
    check('header: no match',
          not has_recovered_header(['src/halo/game/game.c'], dirs))

    # end-to-end row build on synthetic inputs
    eligible, ineligible = build_rows(
        {'objects': {
            'good.obj': {'files': ['src/halo/ai/good.c'],
                         'counts': {'raw_fnptr_cast': 0, 'fun_call': 10,
                                    'raw_offset_deref': 30},
                         'multi_object_files': []},
            'nodelink.obj': {'files': ['src/halo/x/nodelink.c'],
                             'counts': {c: 1 for c in DEBT_CATEGORIES},
                             'multi_object_files': []},
        }},
        {'good.obj': {'delink': True, 'addr_range': '0x1-0x2', 'func_count': 4},
         'nodelink.obj': {'delink': False, 'addr_range': '', 'func_count': 1}},
        {'good.obj': {'funcs': ['a', 'b', 'c', 'd'], 'ported': 4, 'total': 4},
         'nodelink.obj': {'funcs': ['z'], 'ported': 1, 'total': 1}},
        # 'd' is the skeleton case: perfect mnemonic, broken operands.
        {'a': {'score': 100.0, 'opnd': 100.0},
         'b': {'score': 99.0, 'opnd': 99.0},
         'c': {'score': 80.0, 'opnd': 80.0},
         'd': {'score': 100.0, 'opnd': 20.0}},
        {},
        {'src/halo/ai'})
    check('build: one eligible', len(eligible) == 1 and eligible[0]['object'] == 'good.obj')
    check('build: rank assigned', eligible[0]['rank'] == 1)
    check('build: debt total', eligible[0]['debt_total'] == 40)
    check('build: high match 2/4 (operand-defective d excluded)',
          eligible[0]['high_match_funcs'] == 2)
    check('build: mnemonic-only count still 3',
          eligible[0]['high_match_mnemonic_funcs'] == 3)
    check('build: operand-defective gate counted',
          eligible[0]['opnd_defective_gates'] == 1)
    check('build: header flag', eligible[0]['recovered_header'] is True)
    check('build: ineligible gate',
          len(ineligible) == 1 and ineligible[0]['gate'] == 'no_delink')

    for name, passed in checks:
        print('  %s %s' % ('ok  ' if passed else 'FAIL', name))
    failed = [n for n, p in checks if not p]
    print(f'{len(checks) - len(failed)}/{len(checks)} checks passed')
    return 1 if failed else 0


def main(argv=None):
    ap = argparse.ArgumentParser(
        description='Rank kb.json objects for readability/type-recovery campaigns.')
    ap.add_argument('--limit', type=int, default=20,
                    help='rows to print (default 20)')
    ap.add_argument('--json', metavar='PATH',
                    help='write the ranked data as JSON to PATH')
    ap.add_argument('--min-funcs', type=int, default=0, metavar='N',
                    help='hide eligible objects with fewer than N functions '
                         '(display filter only; does not change the score)')
    ap.add_argument('--debt', metavar='PATH',
                    help='reuse a check_readability.py --report-by-object --json '
                         'file instead of recomputing')
    ap.add_argument('--self-test', action='store_true',
                    help='run internal checks and exit non-zero on failure')
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()
    return run(args)


if __name__ == '__main__':
    sys.exit(main())
