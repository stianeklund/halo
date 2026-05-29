#!/usr/bin/env python3
"""
Generate decomp.dev-compatible progress report from existing project data.

This tool bridges our existing analysis infrastructure with decomp.dev format,
enabling external progress dashboards without requiring full decomp.dev deployment.
"""

import sys
import os
import json
import argparse
from datetime import datetime
from collections import defaultdict
from pathlib import Path

# Ensure tools/ is on path
_root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
sys.path.insert(0, _root_dir)
sys.path.insert(0, os.path.join(_root_dir, 'tools'))

from analysis.knowledge import KnowledgeBase
from analysis.kb_meta import MetadataStore


def load_function_sizes(cache_path: str) -> dict:
    """Load function size data from Ghidra export."""
    if not os.path.exists(cache_path):
        return {}
    with open(cache_path) as f:
        return json.load(f)


def _load_snapshot_data(results_path: str = None) -> dict:
    """Load snapshot verification results from game_state_verify.py output.
    Returns a dict keyed by function name: {passed, coverage, confidence, object}.
    """
    if results_path is None:
        root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
        results_path = os.path.join(root_dir, 'artifacts', 'equivalence', 'ci_results.json')
    if not os.path.exists(results_path):
        return {}
    with open(results_path, encoding='utf-8') as f:
        data = json.load(f)
    out = {}
    for r in data.get('results', []):
        name = r.get('func', '')
        if not name:
            continue
        total_seeds = r.get('total_seeds', 0)
        passed = r.get('passed', 0)
        errors = r.get('errors', 0)
        out[name] = {
            'snapshot_passed': passed == total_seeds and errors == 0 and total_seeds > 0,
            'snapshot_coverage': r.get('coverage', 0.0),
            'snapshot_confidence': r.get('confidence', 'unknown'),
            'snapshot_object': r.get('object', '?'),
        }
    return out


def _load_runtime_oracle_data(results_root: str = None) -> dict:
    """Load latest runtime-oracle result per target function."""
    if results_root is None:
        root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
        results_root = os.path.join(root_dir, 'artifacts', 'runtime_oracle')
    root = Path(results_root)
    if not root.exists():
        return {}

    latest = {}
    for summary_path in root.glob('*/summary.json'):
        try:
            with summary_path.open(encoding='utf-8') as f:
                data = json.load(f)
        except (OSError, json.JSONDecodeError):
            continue

        target = data.get('target', '')
        if not target:
            continue

        finished = data.get('finished_utc') or data.get('started_utc') or ''
        current = latest.get(target)
        if current and current.get('finished_utc', '') >= finished:
            continue

        latest[target] = {
            'runtime_oracle_tested': True,
            'runtime_oracle_passed': bool(data.get('ok')),
            'runtime_oracle_run_id': data.get('run_id', ''),
            'runtime_oracle_finished_utc': data.get('finished_utc'),
            'runtime_oracle_artifact': data.get('artifact_dir', str(summary_path.parent)),
            'runtime_oracle_summary': str(summary_path),
        }

    return latest


def compute_unit_stats(kb: KnowledgeBase, store: MetadataStore, 
                       function_cache: dict,
                       vc71_scores: dict = None,
                       leaf_cache: dict = None,
                       snapshot_data: dict = None,
                       runtime_oracle_data: dict = None) -> list[dict]:
    """Compute per-unit statistics in decomp.dev format."""

    if vc71_scores is None:
        vc71_scores = {}
    scores_data = vc71_scores.get('scores', {})
    
    if leaf_cache is None:
        leaf_cache = {}
    
    if snapshot_data is None:
        snapshot_data = {}

    if runtime_oracle_data is None:
        runtime_oracle_data = {}
    
    units = []
    drift = {
        'kb_ported_missing_meta': 0,
        'meta_ported_missing_kb': 0,
    }
    functions_data = function_cache.get('functions', {})
    
    tracked_match_sum = 0.0
    tracked_match_weighted_sum = 0.0
    tracked_scored_count = 0
    tracked_scored_weighted = 0
    
    tracked_equiv_tested = 0
    tracked_equiv_high_conf = 0
    tracked_equiv_avg_cov_sum = 0.0
    tracked_equiv_cov_count = 0
    
    tracked_snap_tested = 0
    tracked_snap_passed = 0
    tracked_snap_high_conf = 0
    tracked_snap_cov_sum = 0.0
    tracked_snap_cov_count = 0

    tracked_runtime_tested = 0
    tracked_runtime_passed = 0
    
    # Group functions by object file
    obj_to_funcs = defaultdict(list)
    
    for addr_str, symbol in kb.addr_to_symbol.items():
        addr = int(addr_str)
        obj = kb.symbol_to_object.get(symbol)
        if obj and symbol.name:
            obj_to_funcs[obj].append({
                'addr': addr,
                'name': symbol.name,
                'symbol': symbol
            })
    
    for obj_name, funcs in sorted(obj_to_funcs.items()):
        source = kb.object_to_source.get(obj_name, '?')
        
        unit_funcs = []
        total_bytes = 0
        ported_bytes = 0
        ported_count = 0
        match_scores = []
        match_weighted_sum = 0.0
        match_scored_bytes = 0
        
        for func_info in funcs:
            addr = func_info['addr']
            name = func_info['name']
            addr_hex = f'0x{addr:x}'
            
            # Get size from cache
            size = 0
            if addr_hex in functions_data:
                size = functions_data[addr_hex].get('size', 0)
            
            # Get status from kb_meta
            meta = store.symbols.get(f'{addr:#x}')
            status = meta.status if meta else 'unknown'
            is_ported = bool(func_info['symbol'].ported)
            meta_is_ported = status in ('ported', 'verified')
            if is_ported and not meta_is_ported:
                drift['kb_ported_missing_meta'] += 1
            elif meta_is_ported and not is_ported:
                drift['meta_ported_missing_kb'] += 1
            
            # Look up VC71 match score by function name
            match_pct = None
            if is_ported and name in scores_data:
                match_pct = scores_data[name].get('score')
            
            # Look up equivalence data from leaf_cache
            eq = leaf_cache.get(addr_hex, {})
            equiv_class = eq.get('class')
            equiv_coverage = eq.get('coverage_pct')
            equiv_confidence = eq.get('confidence')
            
            # Look up snapshot verification data
            snap = snapshot_data.get(name, {})
            snapshot_passed = snap.get('snapshot_passed', None)
            snapshot_coverage = snap.get('snapshot_coverage', None)
            snapshot_confidence = snap.get('snapshot_confidence', None)

            # Look up runtime-oracle data (latest run wins)
            runtime = (runtime_oracle_data.get(name) or
                       runtime_oracle_data.get(addr_hex) or
                       runtime_oracle_data.get(addr_hex.lower()) or
                       runtime_oracle_data.get(f'0x{addr:08x}') or
                       runtime_oracle_data.get(f'FUN_{addr:08X}'))
            runtime_tested = runtime.get('runtime_oracle_tested') if runtime else None
            runtime_passed = runtime.get('runtime_oracle_passed') if runtime else None
            runtime_run_id = runtime.get('runtime_oracle_run_id') if runtime else None
            runtime_finished_utc = runtime.get('runtime_oracle_finished_utc') if runtime else None
            runtime_artifact = runtime.get('runtime_oracle_artifact') if runtime else None
            runtime_summary = runtime.get('runtime_oracle_summary') if runtime else None
            
            func_entry = {
                'address': addr_hex,
                'name': name,
                'size': size,
                'status': status,
                'ported': is_ported,
                'match_percent': match_pct,
                'equiv_class': equiv_class,
                'equiv_coverage': equiv_coverage,
                'equiv_confidence': equiv_confidence,
                'snapshot_passed': snapshot_passed,
                'snapshot_coverage': snapshot_coverage,
                'snapshot_confidence': snapshot_confidence,
                'runtime_oracle_tested': runtime_tested,
                'runtime_oracle_passed': runtime_passed,
                'runtime_oracle_run_id': runtime_run_id,
                'runtime_oracle_finished_utc': runtime_finished_utc,
                'runtime_oracle_artifact': runtime_artifact,
                'runtime_oracle_summary': runtime_summary,
            }
            unit_funcs.append(func_entry)
            
            total_bytes += size
            if is_ported:
                ported_bytes += size
                ported_count += 1
                if match_pct is not None:
                    match_scores.append(match_pct)
                    match_weighted_sum += match_pct * size
                    match_scored_bytes += size
        
        # Skip empty units
        if not unit_funcs:
            continue
        
        match_avg = round(sum(match_scores) / len(match_scores), 1) if match_scores else None
        match_weighted = round(match_weighted_sum / max(match_scored_bytes, 1), 1) if match_scored_bytes > 0 else None
        
        if match_scores:
            tracked_match_sum += sum(match_scores)
            tracked_scored_count += len(match_scores)
            tracked_match_weighted_sum += match_weighted_sum
            tracked_scored_weighted += match_scored_bytes
        
        # Per-unit equivalence summary
        equiv_tested = sum(1 for f in unit_funcs if f['equiv_coverage'] is not None)
        equiv_high_conf = sum(1 for f in unit_funcs if f['equiv_confidence'] == 'high')
        equiv_cov_sum = sum(f['equiv_coverage'] for f in unit_funcs if f['equiv_coverage'] is not None)
        equiv_avg_cov = round(equiv_cov_sum / equiv_tested, 1) if equiv_tested > 0 else None
        equiv_classes = {}
        for f in unit_funcs:
            c = f['equiv_class'] or 'uncached'
            equiv_classes[c] = equiv_classes.get(c, 0) + 1
        
        tracked_equiv_tested += equiv_tested
        tracked_equiv_high_conf += equiv_high_conf
        if equiv_tested > 0:
            tracked_equiv_avg_cov_sum += equiv_cov_sum
            tracked_equiv_cov_count += equiv_tested
        
        # Per-unit snapshot verification summary
        snap_tested = sum(1 for f in unit_funcs if f['snapshot_passed'] is not None)
        snap_passed = sum(1 for f in unit_funcs if f['snapshot_passed'] is True)
        snap_cov_sum = sum(f['snapshot_coverage'] for f in unit_funcs if f['snapshot_coverage'] is not None)
        snap_avg_cov = round(snap_cov_sum / snap_tested, 1) if snap_tested > 0 else None
        snap_high_conf = sum(1 for f in unit_funcs if f['snapshot_confidence'] == 'high')
        
        tracked_snap_tested += snap_tested
        tracked_snap_passed += snap_passed
        tracked_snap_high_conf += snap_high_conf
        if snap_tested > 0:
            tracked_snap_cov_sum += snap_cov_sum
            tracked_snap_cov_count += snap_tested

        runtime_tested = sum(1 for f in unit_funcs if f['runtime_oracle_tested'])
        runtime_passed = sum(1 for f in unit_funcs if f['runtime_oracle_passed'] is True)

        tracked_runtime_tested += runtime_tested
        tracked_runtime_passed += runtime_passed
            
        unit = {
            'name': obj_name.replace('.obj', ''),
            'source_path': f'src/halo/{source}' if source != '?' else None,
            'obj_path': f'delinked/{obj_name}',
            'functions': sorted(unit_funcs, key=lambda x: x['address']),
            'summary': {
                'total': len(unit_funcs),
                'ported': ported_count,
                'percent': round(ported_count / len(unit_funcs) * 100, 2) if unit_funcs else 0,
                'bytes_total': total_bytes,
                'bytes_ported': ported_bytes,
                'bytes_percent': round(ported_bytes / total_bytes * 100, 2) if total_bytes else 0,
                'match_avg': match_avg,
                'match_weighted': match_weighted,
            },
            'equivalence': {
                'tested': equiv_tested,
                'high_confidence': equiv_high_conf,
                'avg_coverage': equiv_avg_cov,
                'classes': equiv_classes,
            },
            'snapshot': {
                'tested': snap_tested,
                'passed': snap_passed,
                'avg_coverage': snap_avg_cov,
                'high_confidence': snap_high_conf,
            },
            'runtime_oracle': {
                'tested': runtime_tested,
                'passed': runtime_passed,
            }
        }
        units.append(unit)
    
    # Compute overall match stats
    overall_match = None
    if tracked_scored_count > 0:
        overall_match = {
            'average': round(tracked_match_sum / tracked_scored_count, 1),
            'weighted': round(tracked_match_weighted_sum / max(tracked_scored_weighted, 1), 1),
            'scored_count': tracked_scored_count,
        }
    
    # Compute overall equivalence summary
    overall_equiv = None
    if tracked_equiv_tested > 0:
        overall_equiv = {
            'tested': tracked_equiv_tested,
            'high_confidence': tracked_equiv_high_conf,
            'avg_coverage': round(tracked_equiv_avg_cov_sum / tracked_equiv_cov_count, 1) if tracked_equiv_cov_count > 0 else None,
        }
    
    # Compute overall snapshot summary
    overall_snapshot = None
    if tracked_snap_tested > 0:
        overall_snapshot = {
            'tested': tracked_snap_tested,
            'passed': tracked_snap_passed,
            'high_confidence': tracked_snap_high_conf,
            'avg_coverage': round(tracked_snap_cov_sum / tracked_snap_cov_count, 1) if tracked_snap_cov_count > 0 else None,
        }
    
    # Sort by ported percentage (most complete first)
    units.sort(key=lambda x: x['summary']['percent'], reverse=True)
    overall_runtime_oracle = None
    if tracked_runtime_tested > 0:
        overall_runtime_oracle = {
            'tested': tracked_runtime_tested,
            'passed': tracked_runtime_passed,
        }

    return units, drift, overall_match, overall_equiv, overall_snapshot, overall_runtime_oracle


def generate_report(output_path: str) -> dict:
    """Generate full decomp.dev-compatible report."""
    
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
    cache_path = os.path.join(root_dir, 'build', 'function_sizes.json')
    vc71_path = os.path.join(root_dir, 'tools', 'verify', 'vc71_scores.json')
    leaf_cache_path = os.path.join(root_dir, 'tools', 'equivalence', 'leaf_cache.json')
    
    # Load knowledge base
    kb = KnowledgeBase.deserialize()
    store = MetadataStore(kb)
    store.load()
    
    # Load function sizes
    function_cache = load_function_sizes(cache_path)
    
    # Load VC71 match scores
    vc71_scores = {}
    if os.path.exists(vc71_path):
        with open(vc71_path) as f:
            vc71_scores = json.load(f)
    
    # Load equivalence leaf cache
    leaf_cache = {}
    if os.path.exists(leaf_cache_path):
        with open(leaf_cache_path) as f:
            leaf_cache = json.load(f)
    
    # Load snapshot verification results
    snapshot_data = _load_snapshot_data()

    # Load runtime-oracle verification results
    runtime_oracle_data = _load_runtime_oracle_data()
    
    # Compute unit stats
    units, drift, overall_match, overall_equiv, overall_snapshot, overall_runtime_oracle = compute_unit_stats(
        kb, store, function_cache, vc71_scores, leaf_cache, snapshot_data, runtime_oracle_data
    )
    
    # Compute overall stats
    total_funcs = sum(u['summary']['total'] for u in units)
    ported_funcs = sum(u['summary']['ported'] for u in units)
    total_bytes = sum(u['summary']['bytes_total'] for u in units)
    ported_bytes = sum(u['summary']['bytes_ported'] for u in units)
    
    # Get git info
    commit = 'unknown'
    branch = 'unknown'
    try:
        import subprocess
        result = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], 
                              capture_output=True, text=True, cwd=root_dir)
        if result.returncode == 0:
            commit = result.stdout.strip()
        result = subprocess.run(['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
                              capture_output=True, text=True, cwd=root_dir)
        if result.returncode == 0:
            branch = result.stdout.strip()
    except:
        pass
    
    report = {
        'project': {
            'name': 'halo-ce-xbox',
            'display_name': 'Halo: Combat Evolved (Xbox)',
            'version': '01.10.12.2276',
            'total_units': len(units),
            'total_functions': total_funcs
        },
        'summary': {
            'functions': {
                'total': total_funcs,
                'ported': ported_funcs,
                'percent': round(ported_funcs / total_funcs * 100, 2) if total_funcs else 0
            },
            'bytes': {
                'total': total_bytes,
                'ported': ported_bytes,
                'percent': round(ported_bytes / total_bytes * 100, 2) if total_bytes else 0
            },
            'match': overall_match,
            'equivalence': overall_equiv,
            'snapshot': overall_snapshot,
            'runtime_oracle': overall_runtime_oracle,
        },
        'meta': {
            'timestamp': datetime.now().astimezone().isoformat(),
            'commit': commit,
            'branch': branch,
            'tool_version': '1.0.0'
        },
        'consistency': {
            'ported_source_of_truth': 'kb.json',
            'kb_ported_missing_meta': drift['kb_ported_missing_meta'],
            'meta_ported_missing_kb': drift['meta_ported_missing_kb']
        }
    }
    
    # Include per-unit breakdown with per-function data
    report['units'] = units
    
    # Write output
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump(report, f, indent=2)
    
    return report


def generate_html(report: dict, output_path: str, history_path: str = None):
    """Generate a client-rendered HTML dashboard with search, byte accuracy, and SSE live updates."""

    import json

    report_json = json.dumps(report)

    history_data = None
    if history_path and os.path.exists(history_path):
        try:
            with open(history_path) as f:
                history_data = json.load(f)
        except Exception:
            pass
    history_json = json.dumps(history_data) if history_data else 'null'

    TEMPLATE = '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Halo CE Xbox - Decompilation Progress</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js">
    </script>
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
            --accent-yellow: #d29922;
            --accent-orange: #d4760a;
            --accent-red: #da3633;
            --live-pulse: #3fb950;
        }
        * { box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            margin: 0; padding: 0;
            background: var(--bg-primary);
            color: var(--text-primary);
            line-height: 1.6;
        }
        .container {
            max-width: 1400px; margin: 0 auto; padding: 20px;
        }
        header {
            text-align: center; padding: 36px 0 20px;
            border-bottom: 1px solid var(--border); margin-bottom: 24px;
        }
        .header-row {
            display: flex; align-items: center; justify-content: center; gap: 16px;
            flex-wrap: wrap;
        }
        h1 {
            color: var(--accent-blue); margin: 0;
            font-size: 2.2em; font-weight: 700;
        }
        .subtitle {
            color: var(--text-secondary); font-size: 1.05em; margin-top: 6px;
        }
        .live-badge {
            display: inline-flex; align-items: center; gap: 6px;
            padding: 4px 14px; border-radius: 20px;
            font-size: 0.78em; font-weight: 700; text-transform: uppercase;
            letter-spacing: 0.5px; border: 1px solid var(--border);
        }
        .live-badge.online {
            background: rgba(63, 185, 80, 0.1); color: var(--live-pulse);
            border-color: rgba(63, 185, 80, 0.3);
        }
        .live-badge.offline {
            background: rgba(139, 148, 158, 0.1); color: var(--text-secondary);
            border-color: var(--border);
        }
        .live-badge .dot {
            width: 8px; height: 8px; border-radius: 50%;
            background: currentColor;
        }
        .live-badge.online .dot {
            animation: pulse 2s ease-in-out infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.3; }
        }
        h2 {
            color: var(--accent-blue); margin-top: 36px; margin-bottom: 16px;
            padding-bottom: 8px; border-bottom: 1px solid var(--border);
            font-size: 1.3em;
        }
        .summary {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
            gap: 16px; margin: 20px 0;
        }
        .card {
            background: var(--bg-secondary); border: 1px solid var(--border);
            border-radius: 12px; padding: 22px;
            transition: transform 0.15s, box-shadow 0.15s;
        }
        .card:hover {
            transform: translateY(-1px);
            box-shadow: 0 4px 16px rgba(0,0,0,0.25);
        }
        .stat-value {
            font-size: 2.2em; font-weight: 700;
            color: var(--accent-blue); margin: 8px 0 4px;
        }
        .stat-label {
            color: var(--text-secondary); font-size: 0.9em;
            text-transform: uppercase; letter-spacing: 0.5px; font-weight: 600;
        }
        .stat-sub {
            color: var(--accent-yellow); font-size: 0.85em; margin-top: 8px;
        }
        .progress-bar {
            width: 100%; height: 22px; background: var(--bg-tertiary);
            border-radius: 11px; overflow: hidden; margin-top: 12px;
            position: relative;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, var(--accent-green), #2ea043);
            border-radius: 11px; transition: width 0.6s ease;
            display: flex; align-items: center; justify-content: flex-end;
            padding-right: 8px; min-width: 40px;
        }
        .progress-text {
            color: #fff; font-weight: 700; font-size: 0.8em;
        }
        .charts-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(420px, 1fr));
            gap: 16px; margin: 16px 0;
        }
        .chart-container {
            background: var(--bg-secondary); border: 1px solid var(--border);
            border-radius: 12px; padding: 20px; height: 360px;
            position: relative;
        }
        .chart-container.chart-full {
            grid-column: 1 / -1;
        }
        .chart-title {
            color: var(--text-secondary); font-size: 0.85em;
            text-transform: uppercase; letter-spacing: 0.5px;
            margin-bottom: 12px; font-weight: 600;
        }
        .table-controls {
            display: flex; align-items: center; gap: 12px;
            margin-bottom: 16px; flex-wrap: wrap;
        }
        .search-wrapper {
            position: relative; flex: 1; min-width: 200px; max-width: 400px;
        }
        .search-wrapper input {
            width: 100%; padding: 10px 14px 10px 36px;
            background: var(--bg-secondary); border: 1px solid var(--border);
            border-radius: 8px; color: var(--text-primary);
            font-size: 0.9em; outline: none; transition: border-color 0.2s;
        }
        .search-wrapper input:focus {
            border-color: var(--accent-blue);
        }
        .search-wrapper input::placeholder {
            color: var(--text-secondary); opacity: 0.7;
        }
        .search-icon {
            position: absolute; left: 12px; top: 50%;
            transform: translateY(-50%);
            color: var(--text-secondary); font-size: 0.9em;
            pointer-events: none;
        }
        .search-count {
            color: var(--text-secondary); font-size: 0.85em;
            white-space: nowrap;
        }
        .table-wrap {
            overflow-x: auto;
            border: 1px solid var(--border);
            border-radius: 12px;
        }
        table {
            width: 100%; border-collapse: collapse;
            background: var(--bg-secondary);
        }
        th {
            background: var(--bg-tertiary); color: var(--text-secondary);
            font-weight: 600; font-size: 0.82em;
            text-transform: uppercase; letter-spacing: 0.5px;
            padding: 12px 14px; text-align: left;
            cursor: pointer; user-select: none;
            position: sticky; top: 0; z-index: 1;
            white-space: nowrap;
        }
        th:hover { background: #30363d; }
        th .sort-arrow { color: var(--text-secondary); font-size: 0.8em; margin-left: 4px; }
        td {
            padding: 10px 14px; border-bottom: 1px solid var(--border);
            font-size: 0.9em;
        }
        tr:last-child td { border-bottom: none; }
        tr:hover { background: rgba(88, 166, 255, 0.04); }
        .unit-name {
            font-family: 'SF Mono', 'Cascadia Code', monospace;
            color: var(--accent-blue); font-weight: 500; font-size: 0.88em;
        }
        .source-path {
            color: var(--text-secondary); font-size: 0.82em; max-width: 280px;
            overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
        }
        .num { text-align: right; font-variant-numeric: tabular-nums; }
        .progress-cell {
            display: flex; align-items: center; gap: 8px; min-width: 120px;
        }
        .mini-bar {
            flex: 1; height: 7px; background: var(--bg-tertiary);
            border-radius: 4px; overflow: hidden; max-width: 80px;
        }
        .mini-fill {
            height: 100%; background: var(--accent-blue);
            border-radius: 4px; transition: width 0.4s ease;
        }
        .pct-complete { color: #3fb950; font-weight: 700; }
        .pct-partial { color: var(--accent-blue); font-weight: 600; }
        .pct-none { color: var(--text-secondary); }
        .match-indicator {
            display: inline-flex; align-items: center; gap: 6px;
        }
        .match-dot {
            width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0;
        }
        .match-dot.high { background: var(--accent-green); }
        .match-dot.ok { background: var(--accent-blue); }
        .match-dot.warn { background: var(--accent-yellow); }
        .match-dot.low { background: var(--accent-red); }
        .meta {
            color: var(--text-secondary); font-size: 0.82em;
            margin-top: 32px; padding-top: 16px;
            border-top: 1px solid var(--border); text-align: center;
        }
        @media (max-width: 768px) {
            h1 { font-size: 1.6em; }
            .summary { grid-template-columns: 1fr; }
            .charts-grid { grid-template-columns: 1fr; }
            .chart-container { height: 280px; }
            .search-wrapper { max-width: none; }
            th, td { padding: 8px 10px; font-size: 0.82em; }
            .source-path { max-width: 120px; }
        }
        /* ===== VIEW ROUTING ===== */
        .view { display: none; }
        .view.active { display: block; }
        .back-btn {
            display: inline-flex; align-items: center; gap: 6px;
            color: var(--accent-blue); cursor: pointer; font-size: 0.9em;
            padding: 6px 12px; border-radius: 6px;
            border: 1px solid var(--border); background: var(--bg-secondary);
            text-decoration: none; margin-bottom: 16px;
        }
        .back-btn:hover {
            background: var(--bg-tertiary);
        }
        .unit-title {
            font-family: 'SF Mono', 'Cascadia Code', monospace;
            font-size: 1.4em; color: var(--accent-blue); font-weight: 700;
        }
        .unit-meta-row {
            display: flex; gap: 24px; flex-wrap: wrap;
            margin: 12px 0 20px; font-size: 0.9em;
        }
        .unit-meta-item {
            color: var(--text-secondary);
        }
        .unit-meta-item strong {
            color: var(--text-primary);
        }
        .func-status {
            display: inline-block; padding: 2px 8px; border-radius: 4px;
            font-size: 0.78em; font-weight: 600; text-transform: uppercase;
            letter-spacing: 0.3px;
        }
        .func-status.ported {
            background: rgba(35, 134, 54, 0.15); color: #3fb950;
        }
        .func-status.unported {
            background: rgba(210, 153, 34, 0.15); color: var(--accent-yellow);
        }
        .func-name {
            font-family: 'SF Mono', 'Cascadia Code', monospace;
            font-size: 0.88em;
        }
        .func-address {
            font-family: 'SF Mono', 'Cascadia Code', monospace;
            font-size: 0.82em; color: var(--text-secondary);
        }
        td.num .match-dot {
            display: inline-block;
            width: 7px; height: 7px; border-radius: 50%; margin-right: 5px;
            vertical-align: middle;
        }
        .unit-name-link {
            cursor: pointer; color: var(--accent-blue); text-decoration: none;
        }
        .unit-name-link:hover {
            text-decoration: underline;
        }
        .detail-chart-container {
            background: var(--bg-secondary); border: 1px solid var(--border);
            border-radius: 12px; padding: 20px; height: 260px;
            margin-bottom: 20px; position: relative;
        }
        /* ===== EQUIVALENCE ===== */
        .equiv-badge {
            display: inline-block; padding: 2px 7px; border-radius: 4px;
            font-size: 0.72em; font-weight: 600; text-transform: uppercase;
            letter-spacing: 0.3px;
        }
        .equiv-badge.leaf { background: rgba(88, 166, 255, 0.15); color: var(--accent-blue); }
        .equiv-badge.non_leaf { background: rgba(139, 148, 158, 0.15); color: var(--text-secondary); }
        .equiv-badge.stubbable { background: rgba(210, 153, 34, 0.15); color: var(--accent-yellow); }
        .equiv-badge.data_only { background: rgba(212, 118, 10, 0.15); color: var(--accent-orange); }
        .equiv-badge.uncached { background: transparent; color: var(--text-secondary); opacity: 0.5; }
        .equiv-confidence {
            display: inline-block; padding: 2px 7px; border-radius: 4px;
            font-size: 0.72em; font-weight: 600; text-transform: uppercase;
            letter-spacing: 0.3px;
        }
        .equiv-confidence.high { background: rgba(35, 134, 54, 0.15); color: #3fb950; }
        .equiv-confidence.moderate { background: rgba(210, 153, 34, 0.15); color: var(--accent-yellow); }
        .equiv-confidence.weak { background: rgba(218, 54, 51, 0.15); color: var(--accent-red); }
        .equiv-confidence.runtime { background: rgba(31, 111, 235, 0.18); color: #79c0ff; }
        .equiv-cov-bar {
            display: inline-flex; align-items: center; gap: 5px;
            width: 80px;
        }
        .equiv-cov-track {
            flex: 1; height: 5px; background: var(--bg-tertiary);
            border-radius: 3px; overflow: hidden;
        }
        .equiv-cov-fill {
            height: 100%; border-radius: 3px;
            transition: width 0.3s ease;
        }
        .equiv-cov-fill.high { background: #3fb950; }
        .equiv-cov-fill.ok { background: var(--accent-blue); }
        .equiv-cov-fill.warn { background: var(--accent-yellow); }
        .equiv-cov-fill.low { background: var(--accent-red); }
        .equiv-cov-text {
            font-size: 0.78em; font-variant-numeric: tabular-nums; min-width: 32px;
        }
        /* ===== VERIFICATION COVERAGE SECTION ===== */
        .verif-two-col {
            display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 16px;
        }
        @media (max-width: 860px) { .verif-two-col { grid-template-columns: 1fr; } }
        .funnel-row {
            display: flex; align-items: center; gap: 10px; margin-bottom: 9px;
        }
        .funnel-label {
            color: var(--text-secondary); font-size: 0.78em; min-width: 110px;
            text-align: right; font-weight: 600; text-transform: uppercase; letter-spacing: 0.3px;
        }
        .funnel-bar-track {
            flex: 1; height: 18px; background: var(--bg-tertiary);
            border-radius: 4px; overflow: hidden; position: relative;
        }
        .funnel-bar-fill {
            height: 100%; border-radius: 4px; display: flex; align-items: center;
            padding-left: 7px; font-size: 0.72em; font-weight: 700; color: rgba(255,255,255,0.9);
            min-width: 3px; transition: width 0.6s ease;
        }
        .funnel-count {
            color: var(--text-primary); font-size: 0.78em; min-width: 140px;
            font-variant-numeric: tabular-nums;
        }
        .funnel-sub { color: var(--text-secondary); }
        .tu-heatmap-grid {
            display: flex; flex-wrap: wrap; gap: 3px; margin-top: 4px;
        }
        .tu-tile {
            width: 14px; height: 14px; border-radius: 2px; cursor: pointer;
            transition: transform 0.1s; flex-shrink: 0;
        }
        .tu-tile:hover { transform: scale(1.8); z-index: 10; }
        .tu-legend {
            display: flex; flex-wrap: wrap; gap: 10px; margin-top: 10px;
        }
        .tu-legend-item {
            display: flex; align-items: center; gap: 5px;
            font-size: 0.72em; color: var(--text-secondary);
        }
        .tu-legend-dot { width: 10px; height: 10px; border-radius: 2px; flex-shrink: 0; }
        .addr-strip-wrap {
            height: 56px; background: var(--bg-tertiary); border-radius: 6px;
            overflow: hidden; position: relative; margin-top: 4px;
        }
        #addrStripCanvas { display: block; width: 100%; height: 100%; }
        .addr-legend {
            display: flex; flex-wrap: wrap; gap: 12px; margin-top: 8px;
        }
        .addr-legend-item {
            display: flex; align-items: center; gap: 5px;
            font-size: 0.72em; color: var(--text-secondary);
        }
        .addr-legend-dot { width: 10px; height: 10px; border-radius: 2px; flex-shrink: 0; }
        .priority-rank { color: var(--text-secondary); font-weight: 700; }
        .priority-reason { display: flex; flex-wrap: wrap; gap: 4px; align-items: center; }
        .priority-reason-badge {
            display: inline-block; padding: 1px 6px; border-radius: 4px;
            font-size: 0.72em; font-weight: 600; text-transform: uppercase; letter-spacing: 0.3px;
        }
        .priority-reason-badge.vc71-great { background: rgba(35,134,54,0.15); color: #3fb950; }
        .priority-reason-badge.vc71-good { background: rgba(88,166,255,0.15); color: var(--accent-blue); }
        .priority-reason-badge.leaf { background: rgba(88,166,255,0.15); color: var(--accent-blue); }
        .priority-reason-badge.large { background: rgba(210,153,34,0.15); color: var(--accent-yellow); }
        .priority-reason-badge.size { background: rgba(139,148,158,0.15); color: var(--text-secondary); }
        .copy-cmd-btn {
            padding: 2px 8px; border-radius: 4px; border: 1px solid var(--border);
            background: var(--bg-tertiary); color: var(--text-secondary); cursor: pointer;
            font-size: 0.72em; font-weight: 600; white-space: nowrap;
            transition: background 0.15s, color 0.15s;
        }
        .copy-cmd-btn:hover { background: var(--accent-blue); color: #fff; border-color: var(--accent-blue); }
        .copy-cmd-btn.copied { background: var(--accent-green); color: #fff; border-color: var(--accent-green); }
        .score-btn {
            padding: 2px 8px; border-radius: 4px; border: 1px solid var(--border);
            background: var(--bg-tertiary); color: var(--text-secondary); cursor: pointer;
            font-size: 0.72em; font-weight: 600; white-space: nowrap;
            transition: background 0.15s, color 0.15s;
        }
        .score-btn:hover:not(:disabled) { background: var(--accent-blue); color: #fff; border-color: var(--accent-blue); }
        .score-btn:disabled { opacity: 0.5; cursor: default; }
        .score-btn.error { background: #da363322; color: #f85149; border-color: #f85149; }
        /* ===== TREEMAP ===== */
        .treemap-grid {
            display: grid; grid-template-columns: 1fr 220px; gap: 16px; margin-bottom: 16px;
        }
        @media (max-width: 860px) { .treemap-grid { grid-template-columns: 1fr; } }
        .treemap-wrap {
            position: relative; height: 420px; cursor: pointer;
        }
        #treemapCanvas { display: block; width: 100%; height: 100%; }
        .treemap-tooltip {
            position: fixed; padding: 8px 12px; background: #161b22;
            border: 1px solid var(--border); border-radius: 8px;
            color: var(--text-primary); font-size: 0.82em;
            pointer-events: none; z-index: 100; display: none;
            max-width: 280px; line-height: 1.5;
        }
        .donut-center {
            display: flex; flex-direction: column; align-items: center;
            justify-content: center;
        }
        .donut-pct {
            font-size: 1.8em; font-weight: 700; color: var(--accent-blue);
            margin-top: 8px;
        }
        .donut-label {
            color: var(--text-secondary); font-size: 0.82em; margin-top: 4px;
        }
        @media (prefers-reduced-motion: reduce) {
            .progress-fill, .live-badge.online .dot { animation: none; transition: none; }
        }
    </style>
</head>
<body>
    <div class="container">

        <!-- ===== OVERVIEW ===== -->
        <div id="view-overview" class="view active">
            <header>
                <div class="header-row">
                    <h1>Halo: Combat Evolved (Xbox)</h1>
                    <span class="live-badge offline" id="live-badge">
                        <span class="dot"></span>
                        <span id="live-text">Static</span>
                    </span>
                </div>
                <div class="subtitle">Decompilation Progress Dashboard</div>
            </header>

            <div class="summary" id="summary-cards"></div>

            <div class="charts-grid" id="charts-grid" style="margin-top:16px">
                <div class="chart-container" id="charts-section-heading" style="height:220px">
                    <div class="chart-title">Functions Ported Over Time</div>
                    <canvas id="progressChart"></canvas>
                </div>
            </div>

            <h2>Verification Coverage</h2>
            <div class="verif-two-col">
                <div class="card">
                    <div class="chart-title">Verification Pipeline</div>
                    <div id="verif-funnel"></div>
                </div>
                <div class="card">
                    <div class="chart-title">Unit Evidence Map &mdash; <span style="font-weight:400;text-transform:none;letter-spacing:0">click a tile to open unit</span></div>
                    <div class="tu-heatmap-grid" id="tu-heatmap"></div>
                    <div class="tu-legend" id="tu-legend"></div>
                </div>
            </div>
            <div class="card" style="margin-bottom:16px">
                <div class="chart-title">Binary Address Space &mdash; <span style="font-weight:400;text-transform:none;letter-spacing:0">every function plotted at its real address</span></div>
                <div class="addr-strip-wrap"><canvas id="addrStripCanvas"></canvas></div>
                <div class="addr-legend" id="addr-legend"></div>
            </div>
            <h2>Translation Units</h2>
            <div class="treemap-grid">
                <div class="card">
                    <div class="chart-title">Unit Size Map &mdash; <span style="font-weight:400;text-transform:none;letter-spacing:0">sized by bytes, colored by completion &mdash; click to drill down</span></div>
                    <div class="treemap-wrap"><canvas id="treemapCanvas"></canvas></div>
                </div>
                <div class="card donut-center">
                    <canvas id="overviewDonut" width="160" height="160" style="max-width:160px;max-height:160px"></canvas>
                    <div class="donut-pct" id="donut-pct"></div>
                    <div class="donut-label" id="donut-label"></div>
                </div>
            </div>
            <div id="treemap-tooltip" class="treemap-tooltip"></div>

            <h2>Per-Unit Breakdown</h2>
            <div class="table-controls">
                <div class="search-wrapper">
                    <span class="search-icon">&#x1F50D;</span>
                    <input type="text" id="unit-search" placeholder="Filter units by name or path..." autocomplete="off">
                </div>
                <span class="search-count" id="search-count"></span>
            </div>
            <div class="table-wrap">
                <table>
                    <thead>
                        <tr>
                            <th data-col="0">Unit Name <span class="sort-arrow"></span></th>
                            <th data-col="1">Source Path <span class="sort-arrow"></span></th>
                            <th data-col="2" class="num">Functions <span class="sort-arrow"></span></th>
                            <th data-col="3" class="num">Ported <span class="sort-arrow"></span></th>
                            <th data-col="4" class="num">Progress <span class="sort-arrow"></span></th>
                            <th data-col="5" class="num">Bytes <span class="sort-arrow"></span></th>
                            <th data-col="6" class="num">Match % <span class="sort-arrow"></span></th>
                        </tr>
                    </thead>
                    <tbody id="table-body"></tbody>
                </table>
            </div>

            <div class="meta" id="meta"></div>
        </div>

        <!-- ===== UNIT DETAIL ===== -->
        <div id="view-detail" class="view">
            <header>
                <div class="header-row">
                    <h1>Halo: Combat Evolved (Xbox)</h1>
                    <span class="live-badge offline" id="live-badge-detail">
                        <span class="dot"></span>
                        <span id="live-text-detail">Static</span>
                    </span>
                </div>
                <div class="subtitle">Translation Unit Detail</div>
            </header>

            <div id="detail-content">
                <a class="back-btn" href="#" id="detail-back">&#x2190; Back to Overview</a>
                <div class="unit-title" id="detail-unit-name"></div>
                <div class="unit-meta-row" id="detail-meta"></div>

                <div class="detail-chart-container">
                    <div class="chart-title">Match Score Distribution</div>
                    <canvas id="detailChart"></canvas>
                </div>

                    <h2>Functions</h2>
                    <div class="table-controls">
                        <div class="search-wrapper">
                            <span class="search-icon">&#x1F50D;</span>
                            <input type="text" id="func-search" placeholder="Filter functions..." autocomplete="off">
                        </div>
                        <span class="search-count" id="func-count"></span>
                    </div>
                    <div class="table-wrap">
                    <table>
                        <thead>
                            <tr>
                                <th data-fcol="0">Address <span class="sort-arrow"></span></th>
                                <th data-fcol="1">Function Name <span class="sort-arrow"></span></th>
                                <th data-fcol="2" class="num">Size <span class="sort-arrow"></span></th>
                                <th data-fcol="3" class="num">Status <span class="sort-arrow"></span></th>
                                <th data-fcol="4" class="num">Match % <span class="sort-arrow"></span></th>
                                <th data-fcol="5" class="num" title="Verified: VC71 &ge;90%, equiv high-conf, snapshot pass, or runtime oracle pass">Verified <span class="sort-arrow"></span></th>
                            </tr>
                        </thead>
                        <tbody id="func-table-body"></tbody>
                    </table>
                    </div>
                </div>

                <div style="margin-top: 20px;">
                    <a class="back-btn" href="#" id="detail-back-bottom">&#x2190; Back to Overview</a>
                </div>

            <div class="meta" id="meta-detail"></div>
        </div>

    </div>

    <script>
        /* ===== DATA ===== */
        var REPORT = __REPORT_JSON__;
        var HISTORY = __HISTORY_JSON__;

        /* ===== STATE ===== */
        var chartInstances = {};
        var sortCol = 4;
        var sortAsc = false;
        var filterText = '';
        var funcSortCol = -1;
        var funcSortAsc = true;
        var funcFilterText = '';
        var currentUnitName = null;

        /* ===== ROUTER ===== */
        function router() {
            var hash = window.location.hash || '#';
            if (hash.indexOf('#unit/') === 0) {
                var name = decodeURIComponent(hash.slice(6));
                showDetail(name);
            } else {
                showOverview();
            }
        }

        function goToUnit(name) {
            window.location.hash = '#unit/' + encodeURIComponent(name);
        }

        function goHome() {
            window.location.hash = '#';
        }

        function showOverview() {
            document.getElementById('view-overview').classList.add('active');
            document.getElementById('view-detail').classList.remove('active');
            render();
            updateLiveBadge('overview');
        }

        function showDetail(name) {
            document.getElementById('view-overview').classList.remove('active');
            document.getElementById('view-detail').classList.add('active');
            currentUnitName = name;
            funcFilterText = '';
            funcSortCol = -1;
            funcSortAsc = true;
            var fs = document.getElementById('func-search');
            if (fs) fs.value = '';
            renderUnitDetail(name);
            updateLiveBadge('detail');
        }

        function updateLiveBadge(view) {
            var liveEl = document.getElementById(view === 'detail' ? 'live-badge-detail' : 'live-badge');
            var textEl = document.getElementById(view === 'detail' ? 'live-text-detail' : 'live-text');
            // Sync detail badge state from overview badge
            var srcBadge = document.getElementById('live-badge');
            if (view === 'detail') {
                liveEl.className = srcBadge.className;
                textEl.textContent = srcBadge.querySelector('#live-text').textContent;
            }
        }

        function fetchJsonFresh(path) {
            var sep = path.indexOf('?') === -1 ? '?' : '&';
            return fetch(path + sep + '_ts=' + Date.now(), {cache: 'no-store'})
                .then(function(r) {
                    if (!r.ok) throw new Error('HTTP ' + r.status);
                    return r.json();
                });
        }

        function hydrateLiveSnapshot() {
            return Promise.allSettled([
                fetchJsonFresh('report.json').then(function(data) { REPORT = data; }),
                fetchJsonFresh('history.json').then(function(data) { HISTORY = data; })
            ]);
        }

        /* ===== SCORE BUTTON ===== */
        function scoreFunction(btn) {
            var unit = btn.getAttribute('data-unit');
            if (!unit) return;
            btn.disabled = true;
            btn.classList.remove('error');
            btn.textContent = '⏳ Scoring…';
            fetch('/api/score', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({unit: unit})
            }).then(function(r) { return r.json(); }).then(function(d) {
                if (d.ok) {
                    // Update REPORT in memory so re-renders show the score immediately
                    var scores = d.scores || {};
                    for (var i = 0; i < REPORT.units.length; i++) {
                        if (REPORT.units[i].name === unit) {
                            var funcs = REPORT.units[i].functions || [];
                            for (var j = 0; j < funcs.length; j++) {
                                if (funcs[j].name in scores) {
                                    funcs[j].match_percent = scores[funcs[j].name];
                                }
                            }
                            break;
                        }
                    }
                    renderUnitDetail(currentUnitName);
                } else {
                    btn.disabled = false;
                    btn.classList.add('error');
                    btn.textContent = d.error === 'no_reference' ? '⚠ No ref' : '⚠ Error';
                    btn.title = d.error || 'Scoring failed';
                }
            }).catch(function() {
                btn.disabled = false;
                btn.classList.add('error');
                btn.textContent = '⚠ Server offline';
                btn.title = 'progress_server.py is not running';
            });
        }

        /* ===== OVERVIEW RENDER ===== */
        function render() {
            renderSummary();
            renderCharts();
            renderVerifSection();
            renderTable();
            renderMeta();
        }

        function countVerified() {
            var count = 0;
            var byMethod = { vc71: 0, equiv: 0, snap: 0, oracle: 0 };
            for (var ui = 0; ui < REPORT.units.length; ui++) {
                var funcs = REPORT.units[ui].functions || [];
                for (var fi = 0; fi < funcs.length; fi++) {
                    var f = funcs[fi];
                    if (!f.ported) continue;
                    var verified = false;
                    if (f.match_percent !== null && f.match_percent !== undefined && f.match_percent >= 90) { byMethod.vc71++; verified = true; }
                    if (f.equiv_confidence === 'high') { byMethod.equiv++; verified = true; }
                    if (f.snapshot_passed === true) { byMethod.snap++; verified = true; }
                    if (f.runtime_oracle_passed === true) { byMethod.oracle++; verified = true; }
                    if (verified) count++;
                }
            }
            return { total: count, byMethod: byMethod };
        }

        function isVerified(f) {
            if (!f.ported) return false;
            if (f.match_percent !== null && f.match_percent !== undefined && f.match_percent >= 90) return true;
            if (f.equiv_confidence === 'high') return true;
            if (f.snapshot_passed === true) return true;
            if (f.runtime_oracle_passed === true) return true;
            return false;
        }

        function renderSummary() {
            var s = REPORT.summary;
            var u = REPORT.units;
            var totalUnits = REPORT.project.total_units;
            var vData = countVerified();

            var completedUnits = u.filter(function(unit) { return unit.summary.percent === 100 && unit.summary.total > 0; }).length;
            var completedPct = Math.round(completedUnits / totalUnits * 100);

            var verifiedPct = s.functions.ported > 0 ? (vData.total / s.functions.ported * 100).toFixed(1) : '0';
            var verifiedTip = 'Verified by at least one method:\\n';
            verifiedTip += 'VC71 \\u226590%: ' + vData.byMethod.vc71 + '\\n';
            verifiedTip += 'Equiv high-conf: ' + vData.byMethod.equiv + '\\n';
            verifiedTip += 'Snapshot pass: ' + vData.byMethod.snap + '\\n';
            verifiedTip += 'Runtime oracle pass: ' + vData.byMethod.oracle;

            document.getElementById('summary-cards').innerHTML =
                '<div class="card" title="Functions ported out of total.">' +
                    '<div class="stat-label">Overall Progress</div>' +
                    '<div class="stat-value">' + s.functions.percent.toFixed(1) + '%</div>' +
                    '<div class="stat-label">' + fmtNum(s.functions.ported) + ' / ' + fmtNum(s.functions.total) + ' functions</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(s.functions.percent, 2) + '%"><span class="progress-text">' + s.functions.percent.toFixed(1) + '%</span></div></div>' +
                '</div>' +
                '<div class="card" title="Source files where every function has been ported.">' +
                    '<div class="stat-label">Files Complete</div>' +
                    '<div class="stat-value">' + completedUnits + '</div>' +
                    '<div class="stat-label">' + completedUnits + ' / ' + totalUnits + ' source files fully ported</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(completedPct, 2) + '%"><span class="progress-text">' + completedPct + '%</span></div></div>' +
                '</div>' +
                (s.match ?
                '<div class="card" title="Byte-level accuracy of ported code compiled with MSVC 7.1 vs original Xbox binary.">' +
                    '<div class="stat-label">Match Quality</div>' +
                    '<div class="stat-value" style="color:' + matchColor(s.match.weighted) + '">' + s.match.weighted.toFixed(1) + '%</div>' +
                    '<div class="stat-label">Byte-weighted avg &middot; ' + fmtNum(s.match.scored_count) + ' scored</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(s.match.weighted, 2) + '%;background:linear-gradient(90deg,var(--accent-green),#2ea043)"><span class="progress-text">' + s.match.weighted.toFixed(1) + '%</span></div></div>' +
                '</div>' : '') +
                '<div class="card" title="' + escHtml(verifiedTip) + '">' +
                    '<div class="stat-label">Verified</div>' +
                    '<div class="stat-value" style="color:#3fb950">' + fmtNum(vData.total) + '</div>' +
                    '<div class="stat-label">' + verifiedPct + '% of ported &middot; hover for breakdown</div>' +
                    (s.functions.ported > 0 ? '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(vData.total / s.functions.ported * 100, 0.3) + '%;background:linear-gradient(90deg,#238636,#3fb950)"><span class="progress-text">' + verifiedPct + '%</span></div></div>' : '') +
                '</div>';
        }

        function matchColor(pct) {
            if (pct >= 95) return '#3fb950';
            if (pct >= 85) return '#58a6ff';
            if (pct >= 70) return '#d29922';
            return '#da3633';
        }

        function matchBadge(pct) {
            if (pct === null || pct === undefined) return 'none';
            if (pct >= 95) return 'high';
            if (pct >= 85) return 'ok';
            if (pct >= 70) return 'warn';
            return 'low';
        }

        function statusBadge(ported) {
            return ported ? '<span class="func-status ported">Ported</span>' : '<span class="func-status unported">Unported</span>';
        }

        function renderCharts() {
            var grid = document.getElementById('charts-grid');
            grid.style.display = '';

            var hasHistory = HISTORY && HISTORY.snapshots && HISTORY.snapshots.length >= 2;
            var progParent = document.getElementById('progressChart').parentNode;
            var heading = document.getElementById('charts-section-heading');
            var grid = document.getElementById('charts-grid');
            progParent.style.display = hasHistory ? '' : 'none';
            if (heading) heading.style.display = hasHistory ? '' : 'none';
            if (grid) grid.style.display = hasHistory ? '' : 'none';

            if (!hasHistory) {
                destroyChart('progressChart');
                return;
            }

            var snaps = HISTORY.snapshots.slice(-90);
            var labels = snaps.map(function(s) { return s.timestamp.slice(0, 10); });
            var funcs = snaps.map(function(s) { return s.summary.functions.ported; });

            destroyChart('progressChart');
            var ctx1 = document.getElementById('progressChart').getContext('2d');
            chartInstances.progressChart = new Chart(ctx1, {
                type: 'line',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Functions Ported',
                        data: funcs,
                        borderColor: '#58a6ff',
                        backgroundColor: 'rgba(88, 166, 255, 0.08)',
                        borderWidth: 2, tension: 0.35, fill: true,
                        pointRadius: 2, pointHoverRadius: 5
                    }]
                },
                options: chartOpts()
            });
        }

        function destroyChart(id) {
            if (chartInstances[id]) {
                chartInstances[id].destroy();
                delete chartInstances[id];
            }
        }

        function chartOpts() {
            return {
                responsive: true, maintainAspectRatio: false,
                interaction: { intersect: false, mode: 'index' },
                plugins: {
                    legend: { display: false },
                    tooltip: {
                        backgroundColor: '#161b22', borderColor: '#30363d', borderWidth: 1,
                        titleColor: '#c9d1d9', bodyColor: '#c9d1d9',
                        padding: 12, cornerRadius: 8
                    }
                },
                scales: {
                    x: {
                        grid: { color: '#21262d' },
                        ticks: { color: '#8b949e', maxRotation: 45 }
                    },
                    y: {
                        grid: { color: '#21262d' },
                        ticks: { color: '#8b949e' },
                        beginAtZero: true
                    }
                }
            };
        }

        /* ===== VERIFICATION COVERAGE RENDERERS ===== */
        function renderVerifSection() {
            renderVerifFunnel();
            renderTuHeatmap();
            renderAddrStrip();
            renderTreemap();
            renderOverviewDonut();
        }

        function renderVerifFunnel() {
            var s = REPORT.summary;
            var total = s.functions.total;
            var ported = s.functions.ported;
            var vc71 = s.match ? (s.match.scored_count || 0) : 0;
            var vData = countVerified();

            var steps = [
                { label: 'All functions',   count: total,       color: '#3d444d', pct: 100 },
                { label: 'Ported',          count: ported,      color: '#388bfd', pct: total > 0 ? ported / total * 100 : 0 },
                { label: 'Byte-matched',    count: vc71,        color: '#d29922', pct: total > 0 ? vc71 / total * 100 : 0 },
                { label: 'Verified',        count: vData.total, color: '#3fb950', pct: total > 0 ? vData.total / total * 100 : 0 }
            ];

            var html = '';
            for (var i = 0; i < steps.length; i++) {
                var st = steps[i];
                var sub = '';
                if (i === 1) sub = ' <span class="funnel-sub">(' + st.count + '/' + total + ')</span>';
                else if (i > 1) sub = ' <span class="funnel-sub">(' + (ported > 0 ? (st.count / ported * 100).toFixed(1) : '0') + '% of ported)</span>';
                var showLabel = st.pct > 12;
                html += '<div class="funnel-row">' +
                    '<div class="funnel-label">' + st.label + '</div>' +
                    '<div class="funnel-bar-track">' +
                        '<div class="funnel-bar-fill" style="width:' + Math.max(st.pct, 0.4) + '%;background:' + st.color + '">' +
                            (showLabel ? fmtNum(st.count) : '') +
                        '</div>' +
                    '</div>' +
                    '<div class="funnel-count">' + fmtNum(st.count) + sub + '</div>' +
                '</div>';
            }
            var el = document.getElementById('verif-funnel');
            if (el) el.innerHTML = html;
        }

        function tuEvidenceLevel(unit) {
            if (!unit.summary || !unit.summary.ported) return 'none';
            var funcs = unit.functions || [];
            var anyVerified = false;
            for (var i = 0; i < funcs.length; i++) {
                if (isVerified(funcs[i])) { anyVerified = true; break; }
            }
            if (anyVerified) return 'verified';
            if (unit.summary.match_weighted !== null && unit.summary.match_weighted !== undefined) return 'matched';
            return 'ported';
        }

        var EVIDENCE_COLORS = {
            none: '#21262d',
            ported: '#3d444d',
            matched: '#388bfd',
            verified: '#3fb950'
        };
        var EVIDENCE_LABELS = {
            none: 'Not started',
            ported: 'Ported, unverified',
            matched: 'Byte-matched (VC71)',
            verified: 'Verified'
        };

        function renderTuHeatmap() {
            var units = REPORT.units;
            var html = '';
            for (var i = 0; i < units.length; i++) {
                var u = units[i];
                var level = tuEvidenceLevel(u);
                var color = EVIDENCE_COLORS[level];
                var s = u.summary;
                var tip = u.name + '\\n' + EVIDENCE_LABELS[level] +
                    '\\n' + s.ported + '/' + s.total + ' ported';
                if (s.match_weighted !== null && s.match_weighted !== undefined) tip += '\\nVC71: ' + s.match_weighted.toFixed(1) + '%';
                html += '<div class="tu-tile" style="background:' + color + '" title="' + escHtml(tip) + '" onclick="goToUnit(\\'' + jsEsc(u.name) + '\\')"></div>';
            }
            var el = document.getElementById('tu-heatmap');
            if (el) el.innerHTML = html;

            var legEl = document.getElementById('tu-legend');
            if (legEl) {
                var legHtml = '';
                var keys = ['none', 'ported', 'matched', 'verified'];
                for (var j = 0; j < keys.length; j++) {
                    var k = keys[j];
                    legHtml += '<div class="tu-legend-item"><div class="tu-legend-dot" style="background:' + EVIDENCE_COLORS[k] + '"></div>' + EVIDENCE_LABELS[k] + '</div>';
                }
                legEl.innerHTML = legHtml;
            }
        }

        function renderAddrStrip() {
            var canvas = document.getElementById('addrStripCanvas');
            if (!canvas) return;
            var wrap = canvas.parentNode;
            var W = wrap.clientWidth || 1200;
            var H = 56;
            var dpr = window.devicePixelRatio || 1;
            canvas.width = Math.round(W * dpr);
            canvas.height = Math.round(H * dpr);
            canvas.style.width = W + 'px';
            canvas.style.height = H + 'px';
            var ctx = canvas.getContext('2d');
            ctx.scale(dpr, dpr);

            var fns = [];
            var minA = Infinity, maxA = 0;
            for (var ui = 0; ui < REPORT.units.length; ui++) {
                var funcs = REPORT.units[ui].functions || [];
                for (var fi = 0; fi < funcs.length; fi++) {
                    var f = funcs[fi];
                    var addr = parseInt(f.address, 16);
                    if (!addr || addr < 0x1000) continue;
                    if (addr < minA) minA = addr;
                    if (addr > maxA) maxA = addr;
                    fns.push(f);
                }
            }
            if (!fns.length || minA >= maxA) return;

            var range = maxA - minA;
            ctx.fillStyle = '#161b22';
            ctx.fillRect(0, 0, W, H);

            for (var i = 0; i < fns.length; i++) {
                var fn = fns[i];
                var addr = parseInt(fn.address, 16);
                var x = Math.floor((addr - minA) / range * (W - 1));
                var barW = Math.max(1, Math.round((fn.size || 1) / range * W));

                var col;
                if (!fn.ported) { col = '#2d333b'; }
                else if (isVerified(fn)) { col = '#3fb950'; }
                else if (fn.match_percent !== null && fn.match_percent !== undefined) { col = '#388bfd'; }
                else { col = '#444c56'; }

                ctx.fillStyle = col;
                ctx.fillRect(x, 0, barW, H);
            }

            var legendItems = [
                ['#2d333b', 'Unported'], ['#444c56', 'Ported'],
                ['#388bfd', 'Byte-matched'], ['#3fb950', 'Verified']
            ];
            var legEl = document.getElementById('addr-legend');
            if (legEl) {
                var lh = '';
                for (var li = 0; li < legendItems.length; li++) {
                    lh += '<div class="addr-legend-item"><div class="addr-legend-dot" style="background:' + legendItems[li][0] + '"></div>' + legendItems[li][1] + '</div>';
                }
                legEl.innerHTML = lh;
            }
        }

        /* ===== TREEMAP ===== */
        var treemapRects = [];

        function completionColor(pct) {
            if (pct >= 100) return '#238636';
            if (pct >= 75) return '#2ea043';
            if (pct >= 50) return '#d29922';
            if (pct >= 25) return '#d4760a';
            if (pct > 0) return '#da3633';
            return '#21262d';
        }

        function squarify(items, x, y, w, h, out) {
            if (!items.length || w <= 0 || h <= 0) return;
            if (items.length === 1) {
                out.push({ x: x, y: y, w: w, h: h, item: items[0] });
                return;
            }
            var totalVal = 0;
            for (var i = 0; i < items.length; i++) totalVal += items[i].value;
            if (totalVal <= 0) return;

            var vertical = h > w;
            var mainDim = vertical ? h : w;
            var otherDim = vertical ? w : h;

            var row = [];
            var rowVal = 0;
            var bestAspect = Infinity;
            var splitAt = 0;

            for (var i = 0; i < items.length; i++) {
                row.push(items[i]);
                rowVal += items[i].value;
                var rowFrac = rowVal / totalVal;
                var rowDim = mainDim * rowFrac;

                var worstAspect = 0;
                var subVal = 0;
                for (var j = 0; j < row.length; j++) {
                    subVal += row[j].value;
                    var cellDim = rowVal > 0 ? otherDim * (row[j].value / rowVal) : 0;
                    if (cellDim <= 0 || rowDim <= 0) continue;
                    var aspect = cellDim > rowDim ? cellDim / rowDim : rowDim / cellDim;
                    if (aspect > worstAspect) worstAspect = aspect;
                }
                if (worstAspect <= bestAspect) {
                    bestAspect = worstAspect;
                    splitAt = i + 1;
                } else {
                    break;
                }
            }

            var headItems = items.slice(0, splitAt);
            var tailItems = items.slice(splitAt);
            var headVal = 0;
            for (var i = 0; i < headItems.length; i++) headVal += headItems[i].value;
            var headFrac = totalVal > 0 ? headVal / totalVal : 0;

            if (vertical) {
                var headH = h * headFrac;
                var cx = x, accVal = 0;
                for (var i = 0; i < headItems.length; i++) {
                    var frac = headVal > 0 ? headItems[i].value / headVal : 0;
                    var cw = w * frac;
                    out.push({ x: cx, y: y, w: cw, h: headH, item: headItems[i] });
                    cx += cw;
                }
                if (tailItems.length) squarify(tailItems, x, y + headH, w, h - headH, out);
            } else {
                var headW = w * headFrac;
                var cy = y, accVal = 0;
                for (var i = 0; i < headItems.length; i++) {
                    var frac = headVal > 0 ? headItems[i].value / headVal : 0;
                    var ch = h * frac;
                    out.push({ x: x, y: cy, w: headW, h: ch, item: headItems[i] });
                    cy += ch;
                }
                if (tailItems.length) squarify(tailItems, x + headW, y, w - headW, h, out);
            }
        }

        function renderTreemap() {
            var canvas = document.getElementById('treemapCanvas');
            if (!canvas) return;
            var wrap = canvas.parentNode;
            var W = wrap.clientWidth || 900;
            var H = wrap.clientHeight || 420;
            var dpr = window.devicePixelRatio || 1;
            canvas.width = Math.round(W * dpr);
            canvas.height = Math.round(H * dpr);
            canvas.style.width = W + 'px';
            canvas.style.height = H + 'px';
            var ctx = canvas.getContext('2d');
            ctx.scale(dpr, dpr);

            var items = [];
            for (var i = 0; i < REPORT.units.length; i++) {
                var u = REPORT.units[i];
                var bytes = u.summary.bytes_total || 1;
                items.push({ value: bytes, unit: u });
            }
            items.sort(function(a, b) { return b.value - a.value; });

            treemapRects = [];
            squarify(items, 1, 1, W - 2, H - 2, treemapRects);

            ctx.fillStyle = '#0d1117';
            ctx.fillRect(0, 0, W, H);

            for (var i = 0; i < treemapRects.length; i++) {
                var r = treemapRects[i];
                var u = r.item.unit;
                var pct = u.summary.percent;
                ctx.fillStyle = completionColor(pct);
                ctx.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2);

                ctx.strokeStyle = '#0d1117';
                ctx.lineWidth = 1.5;
                ctx.strokeRect(r.x, r.y, r.w, r.h);

                if (r.w > 50 && r.h > 28) {
                    ctx.fillStyle = '#fff';
                    var fontSize = Math.min(11, Math.max(8, Math.min(r.w / 8, r.h / 3)));
                    ctx.font = '600 ' + fontSize + 'px -apple-system, sans-serif';
                    ctx.textBaseline = 'top';
                    var label = u.name.replace(/\\.obj$/, '');
                    if (ctx.measureText(label).width > r.w - 8) {
                        label = label.substring(0, Math.floor((r.w - 16) / (fontSize * 0.6))) + '\\u2026';
                    }
                    ctx.fillText(label, r.x + 4, r.y + 4);
                    if (r.h > 42) {
                        ctx.font = '700 ' + Math.max(9, fontSize) + 'px -apple-system, sans-serif';
                        ctx.fillText(pct.toFixed(0) + '%', r.x + 4, r.y + 4 + fontSize + 3);
                    }
                }
            }

            var tooltip = document.getElementById('treemap-tooltip');
            canvas.onmousemove = function(e) {
                var rect = canvas.getBoundingClientRect();
                var mx = (e.clientX - rect.left);
                var my = (e.clientY - rect.top);
                var hit = null;
                for (var i = 0; i < treemapRects.length; i++) {
                    var r = treemapRects[i];
                    if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) { hit = r; break; }
                }
                if (hit) {
                    var u = hit.item.unit;
                    var s = u.summary;
                    tooltip.innerHTML = '<strong>' + escHtml(u.name) + '</strong><br>' +
                        s.ported + ' / ' + s.total + ' functions (' + s.percent.toFixed(1) + '%)<br>' +
                        fmtNum(s.bytes_ported) + ' / ' + fmtNum(s.bytes_total) + ' bytes' +
                        (s.match_weighted != null ? '<br>Match: ' + s.match_weighted.toFixed(1) + '%' : '');
                    tooltip.style.display = 'block';
                    tooltip.style.left = (e.clientX + 12) + 'px';
                    tooltip.style.top = (e.clientY - 10) + 'px';
                    canvas.style.cursor = 'pointer';
                } else {
                    tooltip.style.display = 'none';
                    canvas.style.cursor = 'default';
                }
            };
            canvas.onmouseleave = function() { tooltip.style.display = 'none'; };
            canvas.onclick = function(e) {
                var rect = canvas.getBoundingClientRect();
                var mx = (e.clientX - rect.left);
                var my = (e.clientY - rect.top);
                for (var i = 0; i < treemapRects.length; i++) {
                    var r = treemapRects[i];
                    if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
                        tooltip.style.display = 'none';
                        goToUnit(r.item.unit.name);
                        return;
                    }
                }
            };
        }

        function renderOverviewDonut() {
            var ctx = document.getElementById('overviewDonut');
            if (!ctx) return;
            destroyChart('overviewDonut');
            var s = REPORT.summary;
            var ported = s.bytes.ported || 0;
            var remaining = (s.bytes.total || 1) - ported;
            var pctEl = document.getElementById('donut-pct');
            var lblEl = document.getElementById('donut-label');
            if (pctEl) pctEl.textContent = s.bytes.percent.toFixed(1) + '%';
            if (lblEl) lblEl.textContent = fmtNum(ported) + ' / ' + fmtNum(s.bytes.total) + ' bytes';

            chartInstances.overviewDonut = new Chart(ctx.getContext('2d'), {
                type: 'doughnut',
                data: {
                    labels: ['Ported', 'Remaining'],
                    datasets: [{
                        data: [ported, remaining],
                        backgroundColor: ['#238636', '#21262d'],
                        borderColor: ['#2ea043', '#30363d'],
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: false,
                    cutout: '65%',
                    plugins: {
                        legend: { display: false },
                        tooltip: {
                            backgroundColor: '#161b22', borderColor: '#30363d', borderWidth: 1,
                            titleColor: '#c9d1d9', bodyColor: '#c9d1d9',
                            padding: 10, cornerRadius: 6,
                            callbacks: {
                                label: function(c) { return c.label + ': ' + fmtNum(c.raw) + ' bytes'; }
                            }
                        }
                    }
                }
            });
        }

        function syncSortArrows() {
            var headers = document.querySelectorAll('th[data-col]');
            for (var i = 0; i < headers.length; i++) {
                var arrow = headers[i].querySelector('.sort-arrow');
                var c = parseInt(headers[i].getAttribute('data-col'));
                if (arrow) arrow.textContent = c === sortCol ? (sortAsc ? ' \\u25B2' : ' \\u25BC') : '';
            }
        }

        function renderTable() {
            syncSortArrows();
            var query = filterText.toLowerCase();
            var units = REPORT.units.filter(function(u) {
                return u.name.toLowerCase().indexOf(query) !== -1 ||
                       (u.source_path || '').toLowerCase().indexOf(query) !== -1;
            });

            if (sortCol >= 0) {
                units.sort(function(a, b) {
                    var va = getSortVal(a, sortCol);
                    var vb = getSortVal(b, sortCol);
                    if (typeof va === 'number') return sortAsc ? va - vb : vb - va;
                    return sortAsc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
                });
            }

            document.getElementById('search-count').textContent = units.length + ' / ' + REPORT.units.length + ' units';

            var html = '';
            for (var i = 0; i < units.length; i++) {
                var u = units[i];
                var s = u.summary;
                var name = u.name;
                var source = u.source_path || '?';
                var fp = s.percent;

                var pctClass = fp >= 100 ? 'pct-complete' : (fp > 0 ? 'pct-partial' : 'pct-none');

                var matchHtml = '';
                var sc = s.match_avg;
                if (sc !== null && sc !== undefined) {
                    var sw = s.match_weighted !== null && s.match_weighted !== undefined ? s.match_weighted : sc;
                    var mClass = sw >= 95 ? 'high' : (sw >= 85 ? 'ok' : (sw >= 70 ? 'warn' : 'low'));
                    var tip = 'VC71 byte-accuracy: compiled with MSVC 7.1 and compared to the original binary\\n';
                    tip += 'Average: ' + sc.toFixed(1) + '%  Byte-weighted: ' + sw.toFixed(1) + '%';
                    matchHtml = '<span class="match-indicator" title="' + tip + '"><span class="match-dot ' + mClass + '"></span>' + sw.toFixed(1) + '%</span>';
                } else {
                    matchHtml = '<span class="pct-none">\u2014</span>';
                }
                html += '<tr>' +
                    '<td class="unit-name"><a class="unit-name-link" onclick="goToUnit(\\'' + jsEsc(name) + '\\')">' + escHtml(name) + '</a></td>' +
                    '<td class="source-path" title="' + escHtml(source) + '">' + escHtml(source) + '</td>' +
                    '<td class="num">' + s.total + '</td>' +
                    '<td class="num">' + s.ported + '</td>' +
                    '<td class="num"><div class="progress-cell"><div class="mini-bar"><div class="mini-fill" style="width:' + Math.max(fp, 1) + '%"></div></div><span class="' + pctClass + '">' + fp.toFixed(1) + '%</span></div></td>' +
                    '<td class="num">' + fmtNum(s.bytes_ported) + ' / ' + fmtNum(s.bytes_total) + '</td>' +
                    '<td class="num">' + matchHtml + '</td>' +
                '</tr>';
            }
            document.getElementById('table-body').innerHTML = html;
        }

        function getSortVal(unit, col) {
            var s = unit.summary;
            switch (col) {
                case 0: return unit.name;
                case 1: return unit.source_path || '';
                case 2: return s.total;
                case 3: return s.ported;
                case 4: return s.percent;
                case 5: return s.bytes_total > 0 ? s.bytes_ported / s.bytes_total : 0;
                case 6: {
                    return s.match_weighted !== null && s.match_weighted !== undefined ? s.match_weighted : -1;
                }
                default: return '';
            }
        }

        function renderMeta() {
            var m = REPORT.meta;
            document.getElementById('meta').innerHTML =
                'Generated: ' + m.timestamp + ' &middot; ' +
                'Commit: ' + m.commit + ' (' + m.branch + ') &middot; ' +
                'Tool: generate_decomp_report.py ' + m.tool_version;
            document.getElementById('meta-detail').innerHTML =
                'Generated: ' + m.timestamp + ' &middot; ' +
                'Commit: ' + m.commit + ' (' + m.branch + ') &middot; ' +
                'Tool: generate_decomp_report.py ' + m.tool_version;
        }

        /* ===== UNIT DETAIL RENDER ===== */
        function renderUnitDetail(name) {
            // Find the unit
            var unit = null;
            for (var i = 0; i < REPORT.units.length; i++) {
                if (REPORT.units[i].name === name) {
                    unit = REPORT.units[i];
                    break;
                }
            }
            if (!unit) {
                document.getElementById('detail-content').innerHTML = '<div class="pct-none" style="text-align:center;padding:60px;">Unit not found: ' + escHtml(name) + '</div>';
                return;
            }

            var funcs = unit.functions || [];
            var s = unit.summary;

            // Header
            document.getElementById('detail-unit-name').textContent = unit.name;
            var eq = unit.equivalence || {};
            var snap = unit.snapshot || {};
            var golden = unit.runtime_oracle || {};
            document.getElementById('detail-meta').innerHTML =
                '<span class="unit-meta-item">Source: <strong>' + escHtml(unit.source_path || '?') + '</strong></span>' +
                '<span class="unit-meta-item">Functions: <strong>' + s.total + '</strong></span>' +
                '<span class="unit-meta-item">Ported: <strong>' + s.ported + '</strong> (' + s.percent.toFixed(1) + '%)</span>' +
                '<span class="unit-meta-item">Bytes: <strong>' + fmtNum(s.bytes_ported) + ' / ' + fmtNum(s.bytes_total) + '</strong></span>' +
                (s.match_weighted !== null && s.match_weighted !== undefined ?
                    '<span class="unit-meta-item">Match: <strong style="color:' + matchColor(s.match_weighted) + '">' + s.match_weighted.toFixed(1) + '%</strong></span>' : '') +
                (s.match_avg !== null && s.match_avg !== undefined ?
                    '<span class="unit-meta-item">Avg Match: <strong>' + s.match_avg.toFixed(1) + '%</strong></span>' : '') +
                (eq.tested > 0 ?
                    '<span class="unit-meta-item">Equiv: <strong>' + eq.tested + '</strong> tested &middot; <strong>' + (eq.avg_coverage !== null ? eq.avg_coverage.toFixed(1) + '%' : '?') + '</strong> avg cov &middot; <strong>' + eq.high_confidence + '</strong> high conf.</span>' : '') +
                (golden.tested > 0 ?
                    '<span class="unit-meta-item">Runtime Oracle: <strong style="color:#79c0ff">' + golden.passed + '</strong> / <strong>' + golden.tested + '</strong> passed</span>' : '') +
                (snap.tested > 0 ?
                    '<span class="unit-meta-item">Snapshot: <strong style="color:#a855f7">' + snap.tested + '</strong> tested &middot; <strong>' + snap.passed + '</strong> passed &middot; <strong>' + (snap.avg_coverage !== null ? snap.avg_coverage.toFixed(1) + '%' : '?') + '</strong> avg cov</span>' : '');

            // Match distribution chart (embedded Chart.js)
            renderDetailChart(funcs);

            // Function table
            renderFuncTable(funcs);
        }

        function renderDetailChart(funcs) {
            var ctx = document.getElementById('detailChart').getContext('2d');
            destroyChart('detailChart');

            var scored = funcs.filter(function(f) { return f.match_percent !== null && f.match_percent !== undefined && f.ported; });
            var parent = ctx.canvas.parentNode;
            if (scored.length < 2) {
                parent.style.display = 'none';
                return;
            }
            parent.style.display = '';

            // Bin match scores into ranges
            var bins = { '0-50%': 0, '50-70%': 0, '70-85%': 0, '85-95%': 0, '95-100%': 0 };
            var colors = ['#da3633', '#d4760a', '#d29922', '#58a6ff', '#3fb950'];
            var labels = ['0-50%', '50-70%', '70-85%', '85-95%', '95-100%'];

            for (var i = 0; i < scored.length; i++) {
                var p = scored[i].match_percent;
                if (p < 50) bins['0-50%']++;
                else if (p < 70) bins['50-70%']++;
                else if (p < 85) bins['70-85%']++;
                else if (p < 95) bins['85-95%']++;
                else bins['95-100%']++;
            }

            var data = labels.map(function(l) { return bins[l]; });

            chartInstances.detailChart = new Chart(ctx, {
                type: 'bar',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Functions',
                        data: data,
                        backgroundColor: colors.map(function(c) { return c + '99'; }),
                        borderColor: colors,
                        borderWidth: 1,
                        borderRadius: 4
                    }]
                },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    plugins: {
                        legend: { display: false },
                        tooltip: {
                            backgroundColor: '#161b22', borderColor: '#30363d', borderWidth: 1,
                            titleColor: '#c9d1d9', bodyColor: '#c9d1d9',
                            padding: 12, cornerRadius: 8,
                            callbacks: {
                                label: function(ctx) {
                                    var total = scored.length;
                                    return ctx.raw + ' functions (' + (ctx.raw / total * 100).toFixed(1) + '%)';
                                }
                            }
                        }
                    },
                    scales: {
                        x: {
                            grid: { color: '#21262d' },
                            ticks: { color: '#8b949e' }
                        },
                        y: {
                            grid: { color: '#21262d' },
                            ticks: { color: '#8b949e', precision: 0 },
                            beginAtZero: true
                        }
                    }
                }
            });
        }

        function renderFuncTable(funcs) {
            var query = funcFilterText.toLowerCase();
            var filtered = funcs.filter(function(f) {
                return f.name.toLowerCase().indexOf(query) !== -1;
            });

            if (funcSortCol >= 0) {
                filtered.sort(function(a, b) {
                    var va = getFuncSortVal(a, funcSortCol);
                    var vb = getFuncSortVal(b, funcSortCol);
                    if (typeof va === 'number') return funcSortAsc ? va - vb : vb - va;
                    return funcSortAsc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
                });
            }

            document.getElementById('func-count').textContent = filtered.length + ' / ' + funcs.length + ' funcs';

            var html = '';
            for (var i = 0; i < filtered.length; i++) {
                var f = filtered[i];
                var mClass = matchBadge(f.match_percent);
                var matchDisplay = f.match_percent !== null && f.match_percent !== undefined
                    ? '<span class="num"><span class="match-dot ' + mClass + '"></span>' + f.match_percent.toFixed(1) + '%</span>'
                    : (f.ported
                        ? '<button class="score-btn" data-unit="' + escHtml(currentUnitName) + '" data-func="' + escHtml(f.name) + '" onclick="scoreFunction(this)" title="Run objdiff to compute match%">&#x25B6; Score</button>'
                        : '<span class="pct-none">\u2014</span>');

                var vStatus = '<span class="pct-none">—</span>';
                if (isVerified(f)) {
                    var reasons = [];
                    if (f.match_percent != null && f.match_percent >= 90) reasons.push('VC71 ' + f.match_percent.toFixed(0) + '%');
                    if (f.equiv_confidence === 'high') reasons.push('Equiv');
                    if (f.snapshot_passed === true) reasons.push('Snap');
                    if (f.runtime_oracle_passed === true) reasons.push('Oracle');
                    vStatus = '<span class="func-status ported" title="' + escHtml(reasons.join(', ')) + '">✓ ' + reasons[0] + '</span>';
                } else if (f.ported) {
                    vStatus = '<span class="func-status unported">pending</span>';
                }

                html += '<tr>' +
                    '<td class="func-address">' + f.address + '</td>' +
                    '<td class="func-name">' + escHtml(f.name) + '</td>' +
                    '<td class="num">' + fmtNum(f.size) + '</td>' +
                    '<td class="num">' + statusBadge(f.ported) + '</td>' +
                    '<td class="num">' + matchDisplay + '</td>' +
                    '<td class="num">' + vStatus + '</td>' +
                '</tr>';
            }
            document.getElementById('func-table-body').innerHTML = html;
        }

        function getFuncSortVal(func, col) {
            switch (col) {
                case 0: return parseInt(func.address, 16) || 0;
                case 1: return func.name;
                case 2: return func.size;
                case 3: return func.ported ? 1 : 0;
                case 4: return func.match_percent !== null && func.match_percent !== undefined ? func.match_percent : -1;
                case 5: return isVerified(func) ? 1 : (func.ported ? 0 : -1);
                default: return '';
            }
        }

        /* ===== HELPERS ===== */
        function copyPqCmd(btn) {
            var cmd = btn.getAttribute('data-cmd');
            navigator.clipboard.writeText(cmd).then(function() {
                btn.textContent = 'Copied!';
                btn.classList.add('copied');
                setTimeout(function() { btn.textContent = 'Copy cmd'; btn.classList.remove('copied'); }, 1800);
            }).catch(function() {
                btn.textContent = cmd;
                setTimeout(function() { btn.textContent = 'Copy cmd'; }, 3000);
            });
        }

        function fmtNum(n) {
            return n.toString().replace(/\\B(?=(\\d{3})+(?!\\d))/g, ',');
        }

        function escHtml(s) {
            if (!s) return '';
            return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
        }

        function jsEsc(s) {
            if (!s) return '';
            return s.replace(/\\\\/g, '\\\\\\\\').replace(/'/g, "\\\\'").replace(/"/g, '&quot;');
        }

        /* ===== SSE / POLLING ===== */
        function connectSSE() {
            var badge = document.getElementById('live-badge');
            var text = document.getElementById('live-text');
            var badgeDetail = document.getElementById('live-badge-detail');
            var textDetail = document.getElementById('live-text-detail');

            if (window.EventSource) {
                var es = new EventSource('/events');
                var timeout = setTimeout(function() {
                    badge.className = 'live-badge offline';
                    text.textContent = 'Polling';
                    badgeDetail.className = 'live-badge offline';
                    textDetail.textContent = 'Polling';
                    startPolling();
                }, 3000);

                es.addEventListener('report', function(e) {
                    clearTimeout(timeout);
                    badge.className = 'live-badge online';
                    text.textContent = 'Live';
                    badgeDetail.className = 'live-badge online';
                    textDetail.textContent = 'Live';
                    try {
                        REPORT = JSON.parse(e.data);
                        router();
                    } catch(err) {}
                });

                es.addEventListener('history', function(e) {
                    try {
                        HISTORY = JSON.parse(e.data);
                        router();
                    } catch(err) {}
                });

                es.onerror = function() {
                    clearTimeout(timeout);
                    badge.className = 'live-badge offline';
                    text.textContent = 'Polling';
                    badgeDetail.className = 'live-badge offline';
                    textDetail.textContent = 'Polling';
                    startPolling();
                };
            } else {
                badge.className = 'live-badge offline';
                text.textContent = 'Polling';
                badgeDetail.className = 'live-badge offline';
                textDetail.textContent = 'Polling';
                startPolling();
            }
        }

        var pollTimer = null;
        function startPolling() {
            if (pollTimer) return;
            pollTimer = setInterval(function() {
                fetchJsonFresh('report.json').then(function(data) {
                    REPORT = data;
                    router();
                }).catch(function() {});
            }, 5000);
        }

        /* ===== EVENT BINDING ===== */
        document.addEventListener('DOMContentLoaded', function() {
            // Overview search
            var searchInput = document.getElementById('unit-search');
            if (searchInput) {
                searchInput.addEventListener('input', function() {
                    filterText = this.value;
                    renderTable();
                });
            }

            // Function search (detail view)
            var funcSearch = document.getElementById('func-search');
            if (funcSearch) {
                funcSearch.addEventListener('input', function() {
                    funcFilterText = this.value;
                    var unit = findUnit(currentUnitName);
                    if (unit) renderFuncTable(unit.functions || []);
                });
            }

            // Overview sort on header click
            var headers = document.querySelectorAll('th[data-col]');
            for (var i = 0; i < headers.length; i++) {
                headers[i].addEventListener('click', function() {
                    var col = parseInt(this.getAttribute('data-col'));
                    if (sortCol === col) {
                        sortAsc = !sortAsc;
                    } else {
                        sortCol = col;
                        sortAsc = true;
                    }
                    renderTable();
                });
            }

            // Function sort on header click (detail view)
            var fHeaders = document.querySelectorAll('th[data-fcol]');
            for (var i = 0; i < fHeaders.length; i++) {
                fHeaders[i].addEventListener('click', function() {
                    var col = parseInt(this.getAttribute('data-fcol'));
                    if (funcSortCol === col) {
                        funcSortAsc = !funcSortAsc;
                    } else {
                        funcSortCol = col;
                        funcSortAsc = true;
                    }
                    for (var j = 0; j < fHeaders.length; j++) {
                        var arrow = fHeaders[j].querySelector('.sort-arrow');
                        var c = parseInt(fHeaders[j].getAttribute('data-fcol'));
                        if (c === col) {
                            arrow.textContent = funcSortAsc ? ' \\u25B2' : ' \\u25BC';
                        } else {
                            arrow.textContent = '';
                        }
                    }
                    var unit = findUnit(currentUnitName);
                    if (unit) renderFuncTable(unit.functions || []);
                });
            }

            // Back buttons
            var backBtns = document.querySelectorAll('#detail-back, #detail-back-bottom');
            for (var i = 0; i < backBtns.length; i++) {
                backBtns[i].addEventListener('click', function(e) {
                    e.preventDefault();
                    goHome();
                });
            }

            // Hash change
            window.addEventListener('hashchange', router);

            // Redraw address strip on resize
            var resizeTimer = null;
            window.addEventListener('resize', function() {
                clearTimeout(resizeTimer);
                resizeTimer = setTimeout(function() { renderAddrStrip(); renderTreemap(); }, 200);
            });
        });

        function findUnit(name) {
            for (var i = 0; i < REPORT.units.length; i++) {
                if (REPORT.units[i].name === name) return REPORT.units[i];
            }
            return null;
        }

        /* ===== INIT ===== */
        hydrateLiveSnapshot().finally(function() {
            router();
            connectSSE();
        });
    </script>
</body>
</html>'''

    html = TEMPLATE.replace('__REPORT_JSON__', report_json)\
                   .replace('__HISTORY_JSON__', history_json)

    with open(output_path, 'w') as f:
        f.write(html)


def main():
    ap = argparse.ArgumentParser(description='Generate decomp.dev-style progress report')
    ap.add_argument('-o', '--output', default='artifacts/progress/report.json',
                    help='Output JSON file path')
    ap.add_argument('--html', metavar='PATH', 
                    help='Also generate HTML dashboard at specified path')
    ap.add_argument('--pretty', action='store_true',
                    help='Pretty print JSON output')
    ap.add_argument('--history', default='artifacts/progress/history.json',
                    help='Path to history.json for historical charts')
    args = ap.parse_args()
    
    print('Generating decomp.dev-compatible report...')
    
    report = generate_report(args.output)
    
    print(f'\n✓ Report written to: {args.output}')
    print(f'\nSummary:')
    print(f'  Functions: {report["summary"]["functions"]["ported"]:,} / {report["summary"]["functions"]["total"]:,} ({report["summary"]["functions"]["percent"]:.2f}%)')
    print(f'  Bytes: {report["summary"]["bytes"]["ported"]:,} / {report["summary"]["bytes"]["total"]:,} ({report["summary"]["bytes"]["percent"]:.2f}%)')
    print(f'  Units: {len(report["units"])}')
    
    if args.html:
        generate_html(report, args.html, args.history)
        print(f'\n✓ HTML dashboard written to: {args.html}')
    
    if args.pretty:
        print('\nJSON output:')
        print(json.dumps(report, indent=2)[:2000] + '...' if len(json.dumps(report)) > 2000 else '')


if __name__ == '__main__':
    main()
