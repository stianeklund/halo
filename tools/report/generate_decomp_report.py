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

            <h2 id="charts-section-heading">Historical Progress</h2>
            <div class="charts-grid" id="charts-grid">
                <div class="chart-container">
                    <div class="chart-title">Functions Ported Over Time</div>
                    <canvas id="progressChart"></canvas>
                </div>
                <div class="chart-container">
                    <div class="chart-title">Daily Velocity</div>
                    <canvas id="velocityChart"></canvas>
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
            <div class="card" style="margin-bottom:16px">
                <div class="chart-title">Equivalence Test Priority Queue &mdash; <span style="font-weight:400;text-transform:none;letter-spacing:0">top candidates for unicorn_diff.py</span></div>
                <div id="priority-queue-content"></div>
            </div>

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
                                <th data-fcol="5" class="num">Equiv Cov <span class="sort-arrow"></span></th>
                                <th data-fcol="6" class="num">Confidence <span class="sort-arrow"></span></th>
                                <th data-fcol="7">Class <span class="sort-arrow"></span></th>
                                <th data-fcol="8" class="num" title="Snapshot verification pass/fail">Snap <span class="sort-arrow"></span></th>
                                <th data-fcol="9" class="num" title="Snapshot coverage %">Snap Cov <span class="sort-arrow"></span></th>
                                <th data-fcol="10" class="num" title="Runtime oracle golden test pass/fail">Oracle <span class="sort-arrow"></span></th>
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
        var pqSortCol = -1;
        var pqSortAsc = true;

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

        function renderSummary() {
            var s = REPORT.summary;
            var u = REPORT.units;
            var totalUnits = REPORT.project.total_units;

            var vel = '';
            if (HISTORY && HISTORY.snapshots && HISTORY.snapshots.length >= 2) {
                var snaps = HISTORY.snapshots;
                var recent = snaps.length >= 7 ? snaps.slice(-7) : snaps;
                var fDiff = recent[recent.length - 1].summary.functions.ported - recent[0].summary.functions.ported;
                var days = Math.max(1, recent.length);
                var v = fDiff / days;
                var rem = s.functions.total - s.functions.ported;
                if (v > 0) {
                    var dRem = Math.ceil(rem / v);
                    var d = new Date();
                    d.setDate(d.getDate() + dRem);
                    vel = '<div class="stat-sub">Velocity: ' + v.toFixed(1) + ' funcs/day &middot; ETA: ' + d.toISOString().slice(0, 10) + ' (' + dRem + ' days)</div>';
                }
            }

            document.getElementById('summary-cards').innerHTML =
                '<div class="card" title="Functions ported out of total. Measures how many functions have been lifted from assembly to C.">' +
                    '<div class="stat-label">Overall Progress</div>' +
                    '<div class="stat-value">' + s.functions.percent.toFixed(1) + '%</div>' +
                    '<div class="stat-label">' + fmtNum(s.functions.ported) + ' / ' + fmtNum(s.functions.total) + ' functions</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(s.functions.percent, 2) + '%"><span class="progress-text">' + s.functions.percent.toFixed(1) + '%</span></div></div>' +
                    vel +
                '</div>' +
                (function() {
                    var completedUnits = u.filter(function(unit) { return unit.summary.percent === 100 && unit.summary.total > 0; }).length;
                    var completedPct = Math.round(completedUnits / totalUnits * 100);
                    return '<div class="card" title="Source files where every function has been ported. A fully-reimplemented translation unit.">' +
                        '<div class="stat-label">Files Complete</div>' +
                        '<div class="stat-value">' + completedUnits + '</div>' +
                        '<div class="stat-label">' + completedUnits + ' / ' + totalUnits + ' source files fully ported</div>' +
                        '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(completedPct, 2) + '%"><span class="progress-text">' + completedPct + '%</span></div></div>' +
                    '</div>';
                })() +
                (s.match ? 
                '<div class="card" title="Byte-level accuracy of ported code when compiled with MSVC 7.1 and compared against the original Xbox binary.">' +
                    '<div class="stat-label">Match Quality</div>' +
                    '<div class="stat-value" style="color:' + matchColor(s.match.weighted) + '">' + s.match.weighted.toFixed(1) + '%</div>' +
                    '<div class="stat-label">Byte-weighted avg &middot; ' + fmtNum(s.match.scored_count) + ' functions scored</div>' +
                    '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(s.match.weighted, 2) + '%;background:linear-gradient(90deg,var(--accent-green),#2ea043)"><span class="progress-text">' + s.match.weighted.toFixed(1) + '%</span></div></div>' +
                '</div>' : '') +
                (s.equivalence ?
                '<div class="card" title="Unicorn-Engine equivalence verification. Tests ported functions against original Xbox binary bytecode in a sandboxed x86 emulator.">' +
                    '<div class="stat-label">Equivalence Verified</div>' +
                    '<div class="stat-value" style="color:' + (s.equivalence.tested > 0 ? '#58a6ff' : 'var(--text-secondary)') + '">' + fmtNum(s.equivalence.tested || 0) + '</div>' +
                    '<div class="stat-label">' + (s.equivalence.tested > 0 ? (s.equivalence.tested / s.functions.ported * 100).toFixed(1) + '% of ported' : 'none tested yet') + ' &middot; ' + fmtNum(s.equivalence.high_confidence || 0) + ' high conf.</div>' +
                    (s.equivalence.tested > 0 ? '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(s.equivalence.tested / s.functions.ported * 100, 0.3) + '%;background:linear-gradient(90deg,#1f6feb,#388bfd)"><span class="progress-text">' + (s.equivalence.tested / s.functions.ported * 100).toFixed(1) + '%</span></div></div>' : '') +
                '</div>' : '') +
                (s.snapshot ?
                '<div class="card" title="Game-state snapshot verification. Functions tested with real xemu-captured game memory against the original Xbox binary.">' +
                    '<div class="stat-label">Snapshot Verified</div>' +
                    '<div class="stat-value" style="color:' + (s.snapshot.tested > 0 ? '#a855f7' : 'var(--text-secondary)') + '">' + fmtNum(s.snapshot.passed || 0) + ' / ' + fmtNum(s.snapshot.tested || 0) + '</div>' +
                    '<div class="stat-label">' + (s.snapshot.tested > 0 ? (s.snapshot.tested / s.functions.ported * 100).toFixed(1) + '% of ported' : 'none tested yet') + ' &middot; ' + fmtNum(s.snapshot.passed || 0) + ' passed</div>' +
                    (s.snapshot.tested > 0 ? '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(s.snapshot.passed / s.snapshot.tested * 100, 0.3) + '%;background:linear-gradient(90deg,#7c3aed,#a855f7)"><span class="progress-text">' + (s.snapshot.tested > 0 ? (s.snapshot.passed / s.snapshot.tested * 100).toFixed(1) : '0') + '% pass rate</span></div></div>' : '') +
                '</div>' : '') +
                (s.runtime_oracle ?
                '<div class="card" title="Runtime golden tests captured from the Xbox harness. Latest run per target function wins.">' +
                    '<div class="stat-label">Runtime Oracle</div>' +
                    '<div class="stat-value" style="color:' + (s.runtime_oracle.tested > 0 ? '#79c0ff' : 'var(--text-secondary)') + '">' + fmtNum(s.runtime_oracle.passed || 0) + ' / ' + fmtNum(s.runtime_oracle.tested || 0) + '</div>' +
                    '<div class="stat-label">' + (s.runtime_oracle.tested > 0 ? (s.runtime_oracle.tested / s.functions.ported * 100).toFixed(1) + '% of ported' : 'none tested yet') + ' &middot; ' + fmtNum(s.runtime_oracle.passed || 0) + ' passed</div>' +
                    (s.runtime_oracle.tested > 0 ? '<div class="progress-bar"><div class="progress-fill" style="width:' + Math.max(s.runtime_oracle.passed / s.runtime_oracle.tested * 100, 0.3) + '%;background:linear-gradient(90deg,#1f6feb,#79c0ff)"><span class="progress-text">' + (s.runtime_oracle.tested > 0 ? (s.runtime_oracle.passed / s.runtime_oracle.tested * 100).toFixed(1) : '0') + '% pass rate</span></div></div>' : '') +
                '</div>' : '');
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
            var velParent = document.getElementById('velocityChart').parentNode;
            var heading = document.getElementById('charts-section-heading');
            var grid = document.getElementById('charts-grid');
            progParent.style.display = hasHistory ? '' : 'none';
            velParent.style.display = hasHistory ? '' : 'none';
            if (heading) heading.style.display = hasHistory ? '' : 'none';
            if (grid) grid.style.display = hasHistory ? '' : 'none';

            if (!hasHistory) {
                destroyChart('progressChart');
                destroyChart('velocityChart');
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

            var velData = [0];
            for (var i = 1; i < funcs.length; i++) { velData.push(funcs[i] - funcs[i - 1]); }
            destroyChart('velocityChart');
            var ctx2 = document.getElementById('velocityChart').getContext('2d');
            chartInstances.velocityChart = new Chart(ctx2, {
                type: 'bar',
                data: {
                    labels: labels.slice(1),
                    datasets: [{
                        label: 'Functions/Day',
                        data: velData.slice(1),
                        backgroundColor: 'rgba(35, 134, 54, 0.6)',
                        borderColor: '#238636',
                        borderWidth: 1, borderRadius: 3
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
            renderPriorityQueue();
        }

        function renderVerifFunnel() {
            var s = REPORT.summary;
            var total = s.functions.total;
            var ported = s.functions.ported;
            var vc71 = s.match ? (s.match.scored_count || 0) : 0;
            var eqTested = s.equivalence ? (s.equivalence.tested || 0) : 0;
            var eqHigh = s.equivalence ? (s.equivalence.high_confidence || 0) : 0;
            var snapTested = s.snapshot ? (s.snapshot.tested || 0) : 0;
            var snapPassed = s.snapshot ? (s.snapshot.passed || 0) : 0;
            var goldenTested = s.runtime_oracle ? (s.runtime_oracle.tested || 0) : 0;
            var goldenPassed = s.runtime_oracle ? (s.runtime_oracle.passed || 0) : 0;

            var steps = [
                { label: 'All functions',   count: total,      color: '#3d444d', pct: 100 },
                { label: 'Ported',          count: ported,     color: '#388bfd', pct: ported / total * 100 },
                { label: 'VC71 scored',     count: vc71,       color: '#d29922', pct: vc71 / total * 100 },
                { label: 'Equiv tested',    count: eqTested,   color: '#d4760a', pct: eqTested / total * 100 },
                { label: 'Snap verified',   count: snapTested, color: '#8b5cf6', pct: snapTested / total * 100 },
                { label: 'Snap passed',     count: snapPassed, color: '#a855f7', pct: snapPassed / total * 100 },
                { label: 'Oracle tested',   count: goldenTested, color: '#1f6feb', pct: goldenTested / total * 100 },
                { label: 'Oracle passed',   count: goldenPassed, color: '#79c0ff', pct: goldenPassed / total * 100 },
                { label: 'High conf.',      count: eqHigh,     color: '#3fb950', pct: eqHigh / total * 100 }
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
            if (!unit.summary || !unit.summary.ported) return 'no_ported';
            var eq = unit.equivalence || {};
            var snap = unit.snapshot || {};
            var golden = unit.runtime_oracle || {};
            if (eq.high_confidence > 0) return 'eq_high';
            if (golden.passed > 0) return 'oracle_pass';
            if (golden.tested > 0) return 'oracle_some';
            if (snap.passed > 0) return 'snap_pass';
            if (snap.tested > 0) return 'snap_some';
            if (eq.tested > 0) return 'eq_some';
            if (unit.summary.match_weighted !== null && unit.summary.match_weighted !== undefined) return 'vc71';
            return 'unverified';
        }

        var EVIDENCE_COLORS = {
            no_ported: '#21262d',
            unverified: '#3d444d',
            vc71: '#388bfd',
            eq_some: '#d4760a',
            eq_high: '#3fb950',
            oracle_some: '#1f6feb',
            oracle_pass: '#79c0ff',
            snap_some: '#7c3aed',
            snap_pass: '#a855f7'
        };
        var EVIDENCE_LABELS = {
            no_ported: 'No ported functions',
            unverified: 'Ported, no verification',
            vc71: 'VC71 byte-match scored',
            eq_some: 'Equiv tested',
            eq_high: 'Equiv high-confidence',
            oracle_some: 'Runtime oracle tested',
            oracle_pass: 'Runtime oracle passed',
            snap_some: 'Snapshot tested',
            snap_pass: 'Snapshot verified & passes'
        };

        function renderTuHeatmap() {
            var units = REPORT.units;
            var html = '';
            for (var i = 0; i < units.length; i++) {
                var u = units[i];
                var level = tuEvidenceLevel(u);
                var color = EVIDENCE_COLORS[level];
                var s = u.summary;
                var eq = u.equivalence || {};
                var tip = u.name + '\\n' + EVIDENCE_LABELS[level] +
                    '\\n' + s.ported + '/' + s.total + ' ported';
                if (s.match_weighted !== null && s.match_weighted !== undefined) tip += '\\nVC71: ' + s.match_weighted.toFixed(1) + '%';
                if (eq.tested > 0) tip += '\\nEquiv: ' + eq.tested + ' tested, ' + (eq.avg_coverage || 0).toFixed(1) + '% cov, ' + eq.high_confidence + ' high';
                var golden = u.runtime_oracle || {};
                if (golden.tested > 0) tip += '\\nRuntime oracle: ' + golden.tested + ' tested, ' + golden.passed + ' passed';
                var snap = u.snapshot || {};
                if (snap.tested > 0) tip += '\\nSnapshot: ' + snap.tested + ' tested, ' + snap.passed + ' passed, ' + (snap.avg_coverage || 0).toFixed(1) + '% cov';
                html += '<div class="tu-tile" style="background:' + color + '" title="' + escHtml(tip) + '" onclick="goToUnit(\\'' + jsEsc(u.name) + '\\')"></div>';
            }
            var el = document.getElementById('tu-heatmap');
            if (el) el.innerHTML = html;

            var legEl = document.getElementById('tu-legend');
            if (legEl) {
                var legHtml = '';
                var keys = ['no_ported', 'unverified', 'vc71', 'eq_some', 'eq_high', 'oracle_some', 'oracle_pass', 'snap_some', 'snap_pass'];
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
                else if (fn.runtime_oracle_passed === true) { col = '#79c0ff'; }
                else if (fn.runtime_oracle_tested) { col = '#1f6feb'; }
                else if (fn.equiv_confidence === 'high') { col = '#3fb950'; }
                else if (fn.equiv_confidence === 'moderate') { col = '#d29922'; }
                else if (fn.equiv_coverage !== null && fn.equiv_coverage !== undefined) { col = '#d4760a'; }
                else if (fn.match_percent !== null && fn.match_percent !== undefined) {
                    col = fn.match_percent >= 95 ? '#2ea043' : fn.match_percent >= 85 ? '#388bfd' : '#6e5030';
                } else { col = '#444c56'; }

                ctx.fillStyle = col;
                ctx.fillRect(x, 0, barW, H);
            }

            var legendItems = [
                ['#2d333b', 'Unported'], ['#444c56', 'Ported/unscored'],
                ['#1f6feb', 'Runtime oracle tested'], ['#79c0ff', 'Runtime oracle passed'],
                ['#6e5030', 'VC71 <85%'], ['#388bfd', 'VC71 85-95%'], ['#2ea043', 'VC71 ≥95%'],
                ['#d4760a', 'Equiv tested'], ['#d29922', 'Equiv moderate'], ['#3fb950', 'Equiv high conf.']
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

        function renderPriorityQueue() {
            var candidates = [];
            for (var ui = 0; ui < REPORT.units.length; ui++) {
                var unit = REPORT.units[ui];
                var funcs = unit.functions || [];
                for (var fi = 0; fi < funcs.length; fi++) {
                    var f = funcs[fi];
                    if (!f.ported) continue;
                    if (f.equiv_coverage !== null && f.equiv_coverage !== undefined) continue;
                    var score = (f.size || 0) / 80.0;
                    if (f.match_percent !== null && f.match_percent !== undefined) score += f.match_percent * 0.4;
                    if (f.equiv_class === 'leaf') score += 20;
                    var reasons = [];
                    if (f.match_percent !== null && f.match_percent !== undefined) {
                        if (f.match_percent >= 95) reasons.push('VC71 ' + f.match_percent.toFixed(0) + '%');
                        else if (f.match_percent >= 85) reasons.push('near-match');
                    }
                    if (f.equiv_class === 'leaf') reasons.push('leaf');
                    if ((f.size || 0) >= 200) reasons.push('large');
                    candidates.push({f: f, unit: unit.name, score: score, reasons: reasons});
                }
            }
            candidates.sort(function(a, b) { return b.score - a.score; });
            var top = candidates.slice(0, 25);

            var el = document.getElementById('priority-queue-content');
            if (!el) return;
            if (!top.length) {
                el.innerHTML = '<div style="color:var(--text-secondary);padding:12px 0">No untested ported functions found.</div>';
                return;
            }

            // Apply sort if a column is selected
            if (pqSortCol >= 0) {
                candidates.sort(function(a, b) {
                    var va, vb;
                    switch (pqSortCol) {
                        case 0: va = b.score; vb = a.score; break; // rank (desc by default = lower number first)
                        case 1: va = a.f.name; vb = b.f.name; break;
                        case 2: va = a.unit; vb = b.unit; break;
                        case 3: va = a.f.size || 0; vb = b.f.size || 0; break;
                        case 4: va = a.f.match_percent !== null && a.f.match_percent !== undefined ? a.f.match_percent : -1;
                                vb = b.f.match_percent !== null && b.f.match_percent !== undefined ? b.f.match_percent : -1; break;
                        case 5: va = a.f.equiv_class || ''; vb = b.f.equiv_class || ''; break;
                        default: va = b.score; vb = a.score;
                    }
                    if (typeof va === 'number') return pqSortAsc ? va - vb : vb - va;
                    return pqSortAsc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
                });
            }
            top = candidates.slice(0, 25);

            var pqArrow = function(col) {
                if (pqSortCol !== col) return '<span class="sort-arrow"></span>';
                return '<span class="sort-arrow"> ' + (pqSortAsc ? '\\u25B2' : '\\u25BC') + '</span>';
            };
            var reasonBadge = function(tag) {
                if (tag.indexOf('VC71') === 0 && parseFloat(tag.slice(5)) >= 95) return '<span class="priority-reason-badge vc71-great">' + escHtml(tag) + '</span>';
                if (tag.indexOf('VC71') === 0) return '<span class="priority-reason-badge vc71-good">' + escHtml(tag) + '</span>';
                if (tag === 'leaf') return '<span class="priority-reason-badge leaf">leaf</span>';
                if (tag === 'near-match') return '<span class="priority-reason-badge vc71-good">near-match</span>';
                if (tag === 'large') return '<span class="priority-reason-badge large">large</span>';
                return '<span class="priority-reason-badge size">' + escHtml(tag) + '</span>';
            };
            var html = '<div class="table-wrap"><table id="priority-table"><thead><tr>' +
                '<th data-pqcol="0"># ' + pqArrow(0) + '</th>' +
                '<th data-pqcol="1">Function ' + pqArrow(1) + '</th>' +
                '<th data-pqcol="2">Unit ' + pqArrow(2) + '</th>' +
                '<th data-pqcol="3" class="num">Size ' + pqArrow(3) + '</th>' +
                '<th data-pqcol="4" class="num">VC71 ' + pqArrow(4) + '</th>' +
                '<th data-pqcol="5">Class ' + pqArrow(5) + '</th>' +
                '<th>Why</th>' +
                '<th>Command</th>' +
                '</tr></thead><tbody>';
            for (var i = 0; i < top.length; i++) {
                var c = top[i];
                var f = c.f;
                var mp = f.match_percent !== null && f.match_percent !== undefined;
                var isLeaf = f.equiv_class === 'leaf';
                var cmd = 'python3 tools/equivalence/unicorn_diff.py ' + f.address + ' --seeds 100' + (isLeaf ? '' : ' --allow-stubs --float-tolerance 32');
                var badges = c.reasons.length ? c.reasons.map(reasonBadge).join('') : reasonBadge('size');
                html += '<tr>' +
                    '<td class="priority-rank">' + (i + 1) + '</td>' +
                    '<td class="func-name" style="color:var(--accent-blue)">' + escHtml(f.name) + '</td>' +
                    '<td class="source-path"><a class="unit-name-link" onclick="goToUnit(\\'' + jsEsc(c.unit) + '\\')">' + escHtml(c.unit) + '</a></td>' +
                    '<td class="num">' + fmtNum(f.size || 0) + '</td>' +
                    '<td class="num">' + (mp ? '<span style="color:' + matchColor(f.match_percent) + '">' + f.match_percent.toFixed(1) + '%</span>' : '<span style="color:var(--text-secondary)">—</span>') + '</td>' +
                    '<td>' + (f.equiv_class ? '<span class="equiv-badge ' + f.equiv_class + '">' + f.equiv_class + '</span>' : '<span style="color:var(--text-secondary)">—</span>') + '</td>' +
                    '<td class="priority-reason">' + badges + '</td>' +
                    '<td><button class="copy-cmd-btn" data-cmd="' + escHtml(cmd) + '" onclick="copyPqCmd(this)">Copy cmd</button></td>' +
                '</tr>';
            }
            html += '</tbody></table></div>';
            el.innerHTML = html;

            // Wire up sort headers for priority table
            var pqHeaders = document.querySelectorAll('#priority-table th[data-pqcol]');
            for (var hi = 0; hi < pqHeaders.length; hi++) {
                pqHeaders[hi].addEventListener('click', (function(hh) {
                    return function() {
                        var col = parseInt(hh.getAttribute('data-pqcol'));
                        if (pqSortCol === col) { pqSortAsc = !pqSortAsc; }
                        else { pqSortCol = col; pqSortAsc = (col === 0); }
                        renderPriorityQueue();
                    };
                })(pqHeaders[hi]));
            }
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

            document.getElementById('func-count').textContent = filtered.length + ' / ' + funcs.length + ' functions';

            var html = '';
            for (var i = 0; i < filtered.length; i++) {
                var f = filtered[i];
                var mClass = matchBadge(f.match_percent);
                var matchDisplay = f.match_percent !== null && f.match_percent !== undefined
                    ? '<span class="num"><span class="match-dot ' + mClass + '"></span>' + f.match_percent.toFixed(1) + '%</span>'
                    : (f.ported
                        ? '<button class="score-btn" data-unit="' + escHtml(currentUnitName) + '" data-func="' + escHtml(f.name) + '" onclick="scoreFunction(this)" title="Run objdiff to compute match%">&#x25B6; Score</button>'
                        : '<span class="pct-none">\u2014</span>');

                // Equivalence coverage
                var covDisplay = '<span class="pct-none">\u2014</span>';
                if (f.equiv_coverage !== null && f.equiv_coverage !== undefined) {
                    var covClass = f.equiv_coverage >= 90 ? 'high' : (f.equiv_coverage >= 60 ? 'ok' : (f.equiv_coverage >= 30 ? 'warn' : 'low'));
                    covDisplay = '<span class="equiv-cov-bar">' +
                        '<span class="equiv-cov-track"><span class="equiv-cov-fill ' + covClass + '" style="width:' + f.equiv_coverage + '%"></span></span>' +
                        '<span class="equiv-cov-text" style="color:' + matchColor(f.equiv_coverage) + '">' + f.equiv_coverage.toFixed(1) + '%</span></span>';
                }

                // Equivalence confidence
                var confDisplay = '<span class="pct-none">\u2014</span>';
                if (f.equiv_confidence) {
                    confDisplay = '<span class="equiv-confidence ' + f.equiv_confidence + '">' + f.equiv_confidence + '</span>';
                }

                // Equivalence class
                var classDisplay = '<span class="pct-none">\u2014</span>';
                if (f.equiv_class) {
                    classDisplay = '<span class="equiv-badge ' + f.equiv_class + '">' + f.equiv_class + '</span>';
                }

                // Snapshot verification
                var snapDisplay = '<span class="pct-none">\u2014</span>';
                if (f.snapshot_passed !== null && f.snapshot_passed !== undefined) {
                    snapDisplay = f.snapshot_passed
                        ? '<span class="equiv-confidence high" style="background:#2ea043">passed</span>'
                        : '<span class="equiv-confidence low" style="background:#da3633">failed</span>';
                }

                // Snapshot coverage
                var snapCovDisplay = '<span class="pct-none">\u2014</span>';
                if (f.snapshot_coverage !== null && f.snapshot_coverage !== undefined) {
                    var snapCovClass = f.snapshot_coverage >= 90 ? 'high' : (f.snapshot_coverage >= 60 ? 'ok' : (f.snapshot_coverage >= 30 ? 'warn' : 'low'));
                    snapCovDisplay = '<span class="equiv-cov-bar">' +
                        '<span class="equiv-cov-track"><span class="equiv-cov-fill ' + snapCovClass + '" style="width:' + f.snapshot_coverage + '%"></span></span>' +
                        '<span class="equiv-cov-text" style="color:' + matchColor(f.snapshot_coverage) + '">' + f.snapshot_coverage.toFixed(1) + '%</span></span>';
                }

                var runtimeDisplay = '<span class="pct-none">—</span>';
                if (f.runtime_oracle_tested) {
                    var runtimeTitle = 'Runtime oracle';
                    if (f.runtime_oracle_run_id) runtimeTitle += ' run ' + f.runtime_oracle_run_id;
                    if (f.runtime_oracle_finished_utc) runtimeTitle += ' at ' + f.runtime_oracle_finished_utc;
                    runtimeDisplay = f.runtime_oracle_passed
                        ? '<span class="equiv-confidence runtime" title="' + escHtml(runtimeTitle) + '">passed</span>'
                        : '<span class="equiv-confidence weak" title="' + escHtml(runtimeTitle) + '">failed</span>';
                }

                html += '<tr>' +
                    '<td class="func-address">' + f.address + '</td>' +
                    '<td class="func-name">' + escHtml(f.name) + '</td>' +
                    '<td class="num">' + fmtNum(f.size) + '</td>' +
                    '<td class="num">' + statusBadge(f.ported) + '</td>' +
                    '<td class="num">' + matchDisplay + '</td>' +
                    '<td class="num">' + covDisplay + '</td>' +
                    '<td class="num">' + confDisplay + '</td>' +
                    '<td>' + classDisplay + '</td>' +
                    '<td class="num">' + snapDisplay + '</td>' +
                    '<td class="num">' + snapCovDisplay + '</td>' +
                    '<td class="num">' + runtimeDisplay + '</td>' +
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
                case 5: return func.equiv_coverage !== null && func.equiv_coverage !== undefined ? func.equiv_coverage : -1;
                case 6: {
                    var conf = func.equiv_confidence;
                    if (conf === 'high') return 3;
                    if (conf === 'moderate') return 2;
                    if (conf === 'low') return 1;
                    return 0;
                }
                case 7: return func.equiv_class || '';
                case 8: return func.snapshot_passed === true ? 1 : (func.snapshot_passed === false ? 0 : -1);
                case 9: return func.snapshot_coverage !== null && func.snapshot_coverage !== undefined ? func.snapshot_coverage : -1;
                case 10: return func.runtime_oracle_passed === true ? 1 : (func.runtime_oracle_tested ? 0 : -1);
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
                fetch('report.json').then(function(r) { return r.json(); }).then(function(data) {
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
                resizeTimer = setTimeout(renderAddrStrip, 200);
            });
        });

        function findUnit(name) {
            for (var i = 0; i < REPORT.units.length; i++) {
                if (REPORT.units[i].name === name) return REPORT.units[i];
            }
            return null;
        }

        /* ===== INIT ===== */
        router();
        connectSSE();
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
