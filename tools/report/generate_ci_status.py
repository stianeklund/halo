#!/usr/bin/env python3
"""
Generate CI status dashboard page (ci.html) and compact summary (ci_summary.json).

Data sources:
  - GitHub Actions runs: gh run list (real CI, authoritative)
  - Snapshot/golden tests: artifacts/equivalence/ci_results.json
  - Equivalence coverage: tools/equivalence/leaf_cache.json
  - Runtime oracle: artifacts/runtime_oracle/*/summary.json
  - Local lift pipeline: artifacts/lift_runs/*/summary.json (labeled as local, NOT CI)

Usage:
  python3 tools/report/generate_ci_status.py [--output-dir artifacts/progress]
"""
import sys, os, json, subprocess, argparse
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def _fetch_gh_runs(limit=60):
    try:
        result = subprocess.run(
            ['gh', 'run', 'list', '--limit', str(limit),
             '--json', 'databaseId,conclusion,status,workflowName,createdAt,headSha,url'],
            capture_output=True, text=True, cwd=ROOT, timeout=20
        )
        if result.returncode == 0 and result.stdout.strip():
            return json.loads(result.stdout)
    except Exception:
        pass
    return []


def _load_leaf_cache():
    path = ROOT / 'tools' / 'equivalence' / 'leaf_cache.json'
    if not path.exists():
        return {}
    try:
        with open(path) as f:
            return json.load(f)
    except Exception:
        return {}


def _load_snapshot_results():
    path = ROOT / 'artifacts' / 'equivalence' / 'ci_results.json'
    if not path.exists():
        return None
    try:
        with open(path) as f:
            return json.load(f)
    except Exception:
        return None


def _load_runtime_oracle(limit=20):
    runs_dir = ROOT / 'artifacts' / 'runtime_oracle'
    if not runs_dir.exists():
        return []
    all_runs = []
    for d in sorted(runs_dir.iterdir(), reverse=True):
        p = d / 'summary.json'
        if not p.exists():
            continue
        try:
            with open(p) as f:
                data = json.load(f)
        except Exception:
            continue
        all_runs.append(data)
        if len(all_runs) >= limit * 3:
            break
    # Latest per target
    seen, out = set(), []
    for r in all_runs:
        t = r.get('target', r.get('run_id', ''))
        if t not in seen:
            seen.add(t)
            out.append(r)
        if len(out) >= limit:
            break
    return out


def _load_lift_runs(limit=30):
    runs_dir = ROOT / 'artifacts' / 'lift_runs'
    if not runs_dir.exists():
        return []
    all_runs = []
    for d in sorted(runs_dir.iterdir(), reverse=True):
        p = d / 'summary.json'
        if not p.exists():
            continue
        try:
            with open(p) as f:
                data = json.load(f)
        except Exception:
            continue
        all_runs.append(data)
        if len(all_runs) >= limit * 3:
            break
    # Latest per target
    seen, out = set(), []
    for r in all_runs:
        tgt = (r.get('target') or {}).get('name') or r.get('run_id', '')
        if tgt not in seen:
            seen.add(tgt)
            out.append(r)
        if len(out) >= limit:
            break
    return out


def _parse_lift_stages(run):
    stages = {s['name']: s for s in run.get('stages', [])}

    def ok(name):
        s = stages.get(name)
        return bool(s['ok']) if s and s.get('ran') else None

    def detail(name):
        s = stages.get(name)
        return s.get('details', '') if s else ''

    vc71_detail = detail('vc71_verify')
    vc71_pct = None
    if vc71_detail and '%' in vc71_detail:
        try:
            vc71_pct = float(vc71_detail.split('%')[0].strip())
        except ValueError:
            pass

    equiv_data = run.get('equivalence') or {}
    return {
        'build': ok('build'),
        'abi_audit': ok('abi_audit'),
        'vc71_verify': ok('vc71_verify'),
        'vc71_pct': vc71_pct,
        'hazard_scan': ok('hazard_scan'),
        'equivalence': ok('equivalence'),
        'equiv_confidence': equiv_data.get('confidence'),
        'equiv_coverage': equiv_data.get('coverage_pct'),
        'equiv_passed': equiv_data.get('passed'),
        'equiv_seeds': equiv_data.get('seeds'),
    }


# ---------------------------------------------------------------------------
# Summary JSON (for main-page cards)
# ---------------------------------------------------------------------------

def build_ci_summary(gh_runs, leaf_cache, snapshot_data):
    workflow_status = {}
    for r in gh_runs:
        wf = r.get('workflowName', '')
        if wf not in workflow_status:
            workflow_status[wf] = {
                'name': wf,
                'conclusion': r.get('conclusion'),
                'created_at': r.get('createdAt'),
                'head_sha': (r.get('headSha') or '')[:8],
                'url': r.get('url'),
            }

    conf_counts = {}
    cov_total, cov_count = 0.0, 0
    for entry in leaf_cache.values():
        conf = entry.get('confidence') or 'uncached'
        conf_counts[conf] = conf_counts.get(conf, 0) + 1
        cov = entry.get('coverage_pct')
        if cov is not None:
            cov_total += cov
            cov_count += 1

    snap_summary = None
    if snapshot_data:
        results = snapshot_data.get('results', [])
        total = len(results)
        passed = sum(1 for r in results if r.get('passed', 0) == r.get('total_seeds', 0) and not r.get('errors'))
        snap_summary = {'total': total, 'passed': passed}

    last = gh_runs[0] if gh_runs else None
    return {
        'generated': datetime.now(timezone.utc).isoformat(),
        'ci_runs': {
            'available': len(gh_runs) > 0,
            'total': len(gh_runs),
            'last_run_time': last.get('createdAt') if last else None,
            'last_commit': (last.get('headSha') or '')[:8] if last else None,
            'workflows': list(workflow_status.values()),
        },
        'equivalence': {
            'tested': cov_count,
            'high_confidence': conf_counts.get('high', 0),
            'moderate_confidence': conf_counts.get('moderate', 0),
            'weak_coverage': conf_counts.get('weak', 0),
            'avg_coverage': round(cov_total / cov_count, 1) if cov_count else None,
        },
        'snapshot': snap_summary,
    }


# ---------------------------------------------------------------------------
# HTML generation
# ---------------------------------------------------------------------------

_STYLE = '''
    <style>
        :root {
            --bg-primary: #0d1117;
            --bg-secondary: #161b22;
            --bg-tertiary: #21262d;
            --border: #30363d;
            --text-primary: #c9d1d9;
            --text-secondary: #8b949e;
            --accent-blue: #58a6ff;
            --accent-green: #238636;
            --green: #3fb950;
            --accent-yellow: #d29922;
            --accent-orange: #d4760a;
            --accent-red: #da3633;
            --red: #f85149;
        }
        * { box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            margin: 0; padding: 0;
            background: var(--bg-primary); color: var(--text-primary); line-height: 1.6;
        }
        .container { max-width: 1400px; margin: 0 auto; padding: 20px; }
        header {
            text-align: center; padding: 28px 0 16px;
            border-bottom: 1px solid var(--border); margin-bottom: 24px;
        }
        .header-row { display: flex; align-items: center; justify-content: center; gap: 16px; flex-wrap: wrap; }
        h1 { color: var(--accent-blue); margin: 0; font-size: 2em; font-weight: 700; }
        .subtitle { color: var(--text-secondary); font-size: 1em; margin-top: 4px; }
        .back-btn {
            display: inline-flex; align-items: center; gap: 6px;
            color: var(--accent-blue); text-decoration: none; font-size: 0.9em;
            padding: 5px 12px; border-radius: 6px; border: 1px solid var(--border);
            background: var(--bg-secondary); margin-bottom: 4px;
        }
        .back-btn:hover { background: var(--bg-tertiary); }
        h2 {
            color: var(--accent-blue); margin-top: 32px; margin-bottom: 14px;
            padding-bottom: 8px; border-bottom: 1px solid var(--border); font-size: 1.2em;
        }
        .card {
            background: var(--bg-secondary); border: 1px solid var(--border);
            border-radius: 12px; padding: 20px; margin-bottom: 14px;
        }
        .cards-row { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 14px; margin-bottom: 16px; }
        .stat-value { font-size: 2em; font-weight: 700; color: var(--accent-blue); margin: 6px 0 2px; }
        .stat-label { color: var(--text-secondary); font-size: 0.85em; text-transform: uppercase; letter-spacing: 0.5px; font-weight: 600; }
        .stat-sub { color: var(--text-secondary); font-size: 0.82em; margin-top: 6px; }
        /* Badge */
        .badge {
            display: inline-block; padding: 2px 9px; border-radius: 20px;
            font-size: 0.75em; font-weight: 700; text-transform: uppercase; letter-spacing: 0.4px;
        }
        .badge.success { background: rgba(35,134,54,0.2); color: var(--green); border: 1px solid rgba(35,134,54,0.35); }
        .badge.failure { background: rgba(218,54,51,0.2); color: var(--red); border: 1px solid rgba(218,54,51,0.35); }
        .badge.cancelled { background: rgba(139,148,158,0.2); color: var(--text-secondary); border: 1px solid var(--border); }
        .badge.skipped { background: rgba(139,148,158,0.15); color: var(--text-secondary); border: 1px solid var(--border); }
        .badge.pending { background: rgba(210,153,34,0.2); color: var(--accent-yellow); border: 1px solid rgba(210,153,34,0.35); }
        .badge.pass { background: rgba(35,134,54,0.15); color: var(--green); border: 1px solid rgba(35,134,54,0.3); }
        .badge.fail { background: rgba(218,54,51,0.15); color: var(--red); border: 1px solid rgba(218,54,51,0.3); }
        .badge.none { background: transparent; color: var(--text-secondary); border: 1px solid var(--border); opacity: 0.6; }
        .badge.high { background: rgba(35,134,54,0.15); color: var(--green); }
        .badge.moderate { background: rgba(88,166,255,0.15); color: var(--accent-blue); }
        .badge.weak { background: rgba(218,54,51,0.15); color: var(--red); }
        .badge.uncached { background: transparent; color: var(--text-secondary); opacity: 0.5; }
        /* Table */
        .table-wrap { overflow-x: auto; border: 1px solid var(--border); border-radius: 10px; }
        table { width: 100%; border-collapse: collapse; background: var(--bg-secondary); }
        th {
            background: var(--bg-tertiary); color: var(--text-secondary);
            font-weight: 600; font-size: 0.78em; text-transform: uppercase; letter-spacing: 0.5px;
            padding: 10px 12px; text-align: left; white-space: nowrap;
        }
        td { padding: 8px 12px; border-bottom: 1px solid var(--border); font-size: 0.88em; }
        tr:last-child td { border-bottom: none; }
        tr:hover td { background: rgba(88,166,255,0.04); }
        .mono { font-family: "SF Mono","Cascadia Code",monospace; font-size: 0.9em; }
        .num { text-align: right; font-variant-numeric: tabular-nums; }
        .dim { color: var(--text-secondary); }
        .cov-bar {
            display: inline-flex; align-items: center; gap: 5px;
        }
        .cov-track { width: 60px; height: 5px; background: var(--bg-tertiary); border-radius: 3px; overflow: hidden; }
        .cov-fill { height: 100%; border-radius: 3px; }
        .cov-fill.high { background: var(--green); }
        .cov-fill.moderate { background: var(--accent-blue); }
        .cov-fill.weak { background: var(--accent-yellow); }
        .cov-fill.low { background: var(--red); }
        .workflow-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(260px, 1fr)); gap: 12px; margin-bottom: 20px; }
        .wf-card {
            background: var(--bg-secondary); border: 1px solid var(--border); border-radius: 10px;
            padding: 14px 16px; display: flex; flex-direction: column; gap: 6px;
        }
        .wf-card.success { border-left: 3px solid var(--green); }
        .wf-card.failure { border-left: 3px solid var(--red); }
        .wf-card.cancelled, .wf-card.skipped { border-left: 3px solid var(--text-secondary); }
        .wf-name { font-weight: 600; font-size: 0.92em; }
        .wf-meta { color: var(--text-secondary); font-size: 0.78em; }
        .wf-link { color: var(--accent-blue); font-size: 0.78em; text-decoration: none; }
        .wf-link:hover { text-decoration: underline; }
        .meta { color: var(--text-secondary); font-size: 0.8em; margin-top: 32px; padding-top: 14px; border-top: 1px solid var(--border); text-align: center; }
        .empty-msg { color: var(--text-secondary); font-style: italic; padding: 16px 0; text-align: center; }
        .section-note { color: var(--text-secondary); font-size: 0.82em; margin-bottom: 12px; }
        .conf-bar { display: flex; height: 14px; border-radius: 7px; overflow: hidden; margin: 10px 0; }
        .conf-bar-seg { height: 100%; }
        @media (max-width: 768px) {
            h1 { font-size: 1.5em; }
            .cards-row { grid-template-columns: 1fr 1fr; }
            .workflow-grid { grid-template-columns: 1fr; }
        }
    </style>
'''


def _fmt_time(iso):
    if not iso:
        return '—'
    try:
        dt = datetime.fromisoformat(iso.replace('Z', '+00:00'))
        return dt.strftime('%Y-%m-%d %H:%M UTC')
    except Exception:
        return iso[:16]


def _conclusion_badge(conclusion):
    if not conclusion:
        return '<span class="badge pending">in progress</span>'
    c = conclusion.lower()
    label = {'success': 'Pass', 'failure': 'Fail', 'cancelled': 'Cancelled', 'skipped': 'Skipped'}.get(c, c)
    return f'<span class="badge {c}">{label}</span>'


def _bool_badge(val, ran=True):
    if val is None or not ran:
        return '<span class="badge none">—</span>'
    if val:
        return '<span class="badge pass">✓</span>'
    return '<span class="badge fail">✗</span>'


def _conf_badge(conf):
    if not conf:
        return '<span class="badge uncached">—</span>'
    c = (conf or 'uncached').lower()
    return f'<span class="badge {c}">{c}</span>'


def _cov_cell(pct):
    if pct is None:
        return '<span class="dim">—</span>'
    if pct >= 60:
        cls = 'high'
    elif pct >= 30:
        cls = 'moderate'
    elif pct >= 10:
        cls = 'weak'
    else:
        cls = 'low'
    bar = f'<div class="cov-track"><div class="cov-fill {cls}" style="width:{min(pct,100):.0f}%"></div></div>'
    return f'<div class="cov-bar">{bar}<span class="mono">{pct:.0f}%</span></div>'


def generate_html(out_path, gh_runs, lift_runs, leaf_cache, snapshot_data, oracle_runs):
    generated = datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')

    # ---- Section 1: Latest workflow status cards ----
    workflow_latest = {}
    for r in gh_runs:
        wf = r.get('workflowName', 'Unknown')
        if wf not in workflow_latest:
            workflow_latest[wf] = r

    WORKFLOW_ORDER = [
        '.github/workflows/main.yml',
        'Equivalence Tests',
        'Snapshot Verification',
        'Runtime Oracle Tests',
        'Progress Report',
    ]
    ordered_wf = []
    for name in WORKFLOW_ORDER:
        if name in workflow_latest:
            ordered_wf.append(workflow_latest[name])
    for name, r in workflow_latest.items():
        if name not in WORKFLOW_ORDER:
            ordered_wf.append(r)

    wf_cards_html = ''
    if ordered_wf:
        for r in ordered_wf:
            wf = r.get('workflowName', 'Unknown')
            conclusion = (r.get('conclusion') or 'pending').lower()
            sha = (r.get('headSha') or '')[:8]
            url = r.get('url', '')
            t = _fmt_time(r.get('createdAt'))
            badge = _conclusion_badge(r.get('conclusion'))
            link = f'<a class="wf-link" href="{url}" target="_blank">View run ↗</a>' if url else ''
            display_name = wf.replace('.github/workflows/', '')
            wf_cards_html += f'''
            <div class="wf-card {conclusion}">
                <div class="wf-name">{display_name}</div>
                <div>{badge}</div>
                <div class="wf-meta">{t} &middot; <span class="mono">{sha}</span></div>
                {link}
            </div>'''
    else:
        wf_cards_html = '<p class="empty-msg">No GitHub Actions runs found. Run <code>gh auth login</code> if authentication is needed.</p>'

    # ---- Section 2: Full CI run history table ----
    ci_table_html = ''
    if gh_runs:
        rows = ''
        # Group by commit, show latest run per workflow per commit
        for r in gh_runs[:40]:
            wf = r.get('workflowName', '').replace('.github/workflows/', '')
            sha = (r.get('headSha') or '')[:8]
            t = _fmt_time(r.get('createdAt'))
            badge = _conclusion_badge(r.get('conclusion'))
            url = r.get('url', '')
            link_cell = f'<a class="wf-link" href="{url}" target="_blank">↗</a>' if url else '—'
            rows += f'<tr><td class="mono dim">{t}</td><td>{wf}</td><td>{badge}</td><td class="mono dim">{sha}</td><td>{link_cell}</td></tr>'
        ci_table_html = f'''
        <div class="table-wrap">
          <table>
            <thead><tr>
              <th>Time</th><th>Workflow</th><th>Result</th><th>Commit</th><th>Link</th>
            </tr></thead>
            <tbody>{rows}</tbody>
          </table>
        </div>'''
    else:
        ci_table_html = '<p class="empty-msg">No GitHub Actions runs found.</p>'

    # ---- Section 3: Snapshot / Golden Tests ----
    snap_html = ''
    if snapshot_data:
        results = snapshot_data.get('results', [])
        snap_summary = snapshot_data.get('summary', {})
        total = snap_summary.get('total', len(results))
        passed_count = snap_summary.get('passed', sum(1 for r in results if r.get('passed', 0) == r.get('total_seeds', 0) and not r.get('errors')))

        status_class = 'pass' if passed_count == total and total > 0 else 'fail'
        rows = ''
        for r in sorted(results, key=lambda x: x.get('passed', 0) / max(x.get('total_seeds', 1), 1)):
            func = r.get('func', '?')
            seeds = r.get('total_seeds', 0)
            p = r.get('passed', 0)
            err = r.get('errors', 0)
            cov = r.get('coverage', 0.0)
            conf = r.get('confidence', '')
            ok = p == seeds and err == 0 and seeds > 0
            result_badge = '<span class="badge pass">Pass</span>' if ok else '<span class="badge fail">Fail</span>'
            rows += f'<tr><td class="mono">{func}</td><td>{result_badge}</td><td class="num">{p}/{seeds}</td><td class="num">{err}</td><td>{_cov_cell(cov)}</td><td>{_conf_badge(conf)}</td></tr>'

        snap_html = f'''
        <div class="cards-row">
          <div class="card">
            <div class="stat-label">Total functions</div>
            <div class="stat-value">{total}</div>
          </div>
          <div class="card">
            <div class="stat-label">Passed</div>
            <div class="stat-value" style="color:{'var(--green)' if passed_count == total else 'var(--red)'}">{passed_count}</div>
            <div class="stat-sub">{passed_count}/{total} passed</div>
          </div>
        </div>
        <div class="table-wrap">
          <table>
            <thead><tr>
              <th>Function</th><th>Result</th><th class="num">Seeds (pass/total)</th>
              <th class="num">Errors</th><th>Coverage</th><th>Confidence</th>
            </tr></thead>
            <tbody>{rows}</tbody>
          </table>
        </div>'''
    else:
        snap_html = '''<p class="empty-msg">No snapshot results found. The Snapshot Verification workflow writes
        <code>artifacts/equivalence/ci_results.json</code> when it runs on this machine.</p>'''

    # ---- Section 4: Equivalence coverage health ----
    conf_counts = {}
    cov_total, cov_count = 0.0, 0
    weak_fns = []
    for addr, entry in leaf_cache.items():
        conf = (entry.get('confidence') or 'uncached').lower()
        conf_counts[conf] = conf_counts.get(conf, 0) + 1
        cov = entry.get('coverage_pct')
        if cov is not None:
            cov_total += cov
            cov_count += 1
            if cov < 30:
                weak_fns.append({'addr': addr, 'confidence': conf, 'coverage': cov,
                                  'cls': entry.get('class', '?')})

    avg_cov = round(cov_total / cov_count, 1) if cov_count else None
    total_cached = sum(conf_counts.values())
    high_n = conf_counts.get('high', 0)
    mod_n = conf_counts.get('moderate', 0)
    weak_n = conf_counts.get('weak', 0)
    unc_n = conf_counts.get('uncached', 0)

    def pct(n): return f'{n / total_cached * 100:.0f}%' if total_cached else '0%'

    conf_bar = ''
    if total_cached:
        h_w = high_n / total_cached * 100
        m_w = mod_n / total_cached * 100
        wk_w = weak_n / total_cached * 100
        u_w = unc_n / total_cached * 100
        conf_bar = f'''
        <div class="conf-bar">
          <div class="conf-bar-seg" style="width:{h_w:.1f}%;background:var(--green)" title="High: {high_n}"></div>
          <div class="conf-bar-seg" style="width:{m_w:.1f}%;background:var(--accent-blue)" title="Moderate: {mod_n}"></div>
          <div class="conf-bar-seg" style="width:{wk_w:.1f}%;background:var(--accent-yellow)" title="Weak: {weak_n}"></div>
          <div class="conf-bar-seg" style="width:{u_w:.1f}%;background:var(--bg-tertiary)" title="Uncached: {unc_n}"></div>
        </div>
        <div style="display:flex;gap:16px;font-size:0.78em;color:var(--text-secondary);margin-top:4px">
          <span><span style="color:var(--green)">&#9632;</span> High ({high_n}, {pct(high_n)})</span>
          <span><span style="color:var(--accent-blue)">&#9632;</span> Moderate ({mod_n}, {pct(mod_n)})</span>
          <span><span style="color:var(--accent-yellow)">&#9632;</span> Weak ({weak_n}, {pct(weak_n)})</span>
          <span><span style="color:var(--bg-tertiary)">&#9632;</span> Uncached ({unc_n})</span>
        </div>'''

    weak_fns.sort(key=lambda x: x['coverage'])
    weak_rows = ''
    for f in weak_fns[:50]:
        weak_rows += f'<tr><td class="mono">{f["addr"]}</td><td><span class="badge {f["cls"] if f["cls"] in ("leaf","non_leaf","stubbable") else "none"}">{f["cls"]}</span></td><td>{_conf_badge(f["confidence"])}</td><td>{_cov_cell(f["coverage"])}</td></tr>'

    equiv_note = f'<p class="section-note">{total_cached} functions have equivalence data in leaf_cache.json &middot; avg coverage {avg_cov:.1f}%</p>' if avg_cov is not None else ''

    equiv_html = f'''
    <div class="cards-row">
      <div class="card">
        <div class="stat-label">Tested (any)</div>
        <div class="stat-value">{total_cached}</div>
      </div>
      <div class="card">
        <div class="stat-label">High Confidence</div>
        <div class="stat-value" style="color:var(--green)">{high_n}</div>
        <div class="stat-sub">{pct(high_n)} of tested</div>
      </div>
      <div class="card">
        <div class="stat-label">Avg Coverage</div>
        <div class="stat-value">{avg_cov:.1f}%</div>
      </div>
    </div>
    {conf_bar}
    {equiv_note}
    '''

    if weak_fns:
        equiv_html += f'''
    <h3 style="color:var(--text-secondary);font-size:0.95em;margin:16px 0 8px">Functions with &lt;30% coverage ({len(weak_fns)} total, showing first 50)</h3>
    <div class="table-wrap">
      <table>
        <thead><tr><th>Address</th><th>Class</th><th>Confidence</th><th>Coverage</th></tr></thead>
        <tbody>{weak_rows}</tbody>
      </table>
    </div>'''
    else:
        equiv_html += '<p class="dim" style="margin-top:8px">No functions with &lt;30% coverage.</p>'

    # ---- Section 5: Runtime Oracle ----
    if oracle_runs:
        o_rows = ''
        for r in oracle_runs:
            target = r.get('target', '?')
            ok = r.get('ok')
            err = r.get('error', '')
            t = _fmt_time(r.get('finished_utc') or r.get('started_utc'))
            badge = '<span class="badge pass">Pass</span>' if ok else f'<span class="badge fail">Fail</span>'
            err_cell = f'<span class="dim" style="font-size:0.82em">{err[:80]}</span>' if err else ''
            o_rows += f'<tr><td class="mono">{target}</td><td>{badge}</td><td class="dim">{t}</td><td>{err_cell}</td></tr>'
        oracle_html = f'''
        <div class="table-wrap">
          <table>
            <thead><tr><th>Target</th><th>Result</th><th>Finished</th><th>Error</th></tr></thead>
            <tbody>{o_rows}</tbody>
          </table>
        </div>'''
    else:
        oracle_html = '<p class="empty-msg">No runtime oracle results found.</p>'

    # ---- Section 6: Local Lift Pipeline Runs ----
    lift_html = ''
    if lift_runs:
        rows = ''
        for r in lift_runs:
            tgt = r.get('target') or {}
            name = tgt.get('name', r.get('run_id', '?'))
            obj = tgt.get('object', '?')
            t = r.get('started_utc', '')[:13].replace('T', ' ')
            stages = _parse_lift_stages(r)

            build_b = _bool_badge(stages['build'], stages['build'] is not None)
            abi_b = _bool_badge(stages['abi_audit'], stages['abi_audit'] is not None)
            vc71_b = _bool_badge(stages['vc71_verify'], stages['vc71_verify'] is not None)
            vc71_pct = f'<span class="mono">{stages["vc71_pct"]:.1f}%</span>' if stages['vc71_pct'] is not None else '<span class="dim">—</span>'
            eq_b = _bool_badge(stages['equivalence'], stages['equivalence'] is not None)
            conf = _conf_badge(stages['equiv_confidence'])
            cov = _cov_cell(stages['equiv_coverage'])

            rows += f'''<tr>
              <td class="dim">{t}</td>
              <td class="mono">{name}</td>
              <td class="dim" style="font-size:0.82em">{obj}</td>
              <td class="num">{build_b}</td>
              <td class="num">{abi_b}</td>
              <td class="num">{vc71_b}</td>
              <td class="num">{vc71_pct}</td>
              <td class="num">{eq_b}</td>
              <td>{conf}</td>
              <td>{cov}</td>
            </tr>'''
        lift_html = f'''
        <p class="section-note">Local <code>/lift</code> pipeline runs (latest per target, not GitHub Actions runs)</p>
        <div class="table-wrap">
          <table>
            <thead><tr>
              <th>Date</th><th>Target</th><th>Object</th>
              <th class="num">Build</th><th class="num">ABI</th>
              <th class="num">VC71</th><th class="num">Match %</th>
              <th class="num">Equiv</th><th>Confidence</th><th>Coverage</th>
            </tr></thead>
            <tbody>{rows}</tbody>
          </table>
        </div>'''
    else:
        lift_html = '<p class="empty-msg">No local lift pipeline runs found in artifacts/lift_runs/.</p>'

    html = f'''<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CI Status &mdash; Halo CE Xbox</title>
  {_STYLE}
</head>
<body>
  <div class="container">
    <header>
      <div class="header-row">
        <h1>CI Status</h1>
      </div>
      <div class="subtitle">Halo CE Xbox &mdash; Verification &amp; Pipeline Health</div>
    </header>

    <a class="back-btn" href="index.html">&#x2190; Back to Dashboard</a>

    <h2>GitHub Actions &mdash; Current Status</h2>
    <div class="workflow-grid">{wf_cards_html}</div>

    <h2>GitHub Actions &mdash; Run History</h2>
    {ci_table_html}

    <h2>Snapshot / Golden Tests</h2>
    {snap_html}

    <h2>Equivalence Coverage Health</h2>
    {equiv_html}

    <h2>Runtime Oracle</h2>
    {oracle_html}

    <h2>Local Lift Pipeline Runs</h2>
    {lift_html}

    <div class="meta">Generated {generated} &middot; Halo CE Xbox Decompilation</div>
  </div>
</body>
</html>'''

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(html)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description='Generate CI status page')
    ap.add_argument('--output-dir', default='artifacts/progress',
                    help='Output directory for ci.html and ci_summary.json')
    args = ap.parse_args()

    out_dir = ROOT / args.output_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    print('Fetching GitHub Actions run history...')
    gh_runs = _fetch_gh_runs(60)
    print(f'  {len(gh_runs)} runs fetched')

    print('Loading leaf cache...')
    leaf_cache = _load_leaf_cache()
    print(f'  {len(leaf_cache)} entries')

    print('Loading snapshot test results...')
    snapshot_data = _load_snapshot_results()
    if snapshot_data:
        results = snapshot_data.get('results', [])
        print(f'  {len(results)} functions')
    else:
        print('  not found')

    print('Loading runtime oracle runs...')
    oracle_runs = _load_runtime_oracle(20)
    print(f'  {len(oracle_runs)} runs (latest per target)')

    print('Loading local lift runs...')
    lift_runs = _load_lift_runs(25)
    print(f'  {len(lift_runs)} runs (latest per target)')

    ci_html_path = out_dir / 'ci.html'
    ci_summary_path = out_dir / 'ci_summary.json'

    print('Generating ci.html...')
    generate_html(ci_html_path, gh_runs, lift_runs, leaf_cache, snapshot_data, oracle_runs)
    print(f'  ✓ {ci_html_path}')

    print('Generating ci_summary.json...')
    summary = build_ci_summary(gh_runs, leaf_cache, snapshot_data)
    with open(ci_summary_path, 'w') as f:
        json.dump(summary, f, indent=2)
    print(f'  ✓ {ci_summary_path}')


if __name__ == '__main__':
    main()
