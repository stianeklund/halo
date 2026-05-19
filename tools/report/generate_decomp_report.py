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


def compute_unit_stats(kb: KnowledgeBase, store: MetadataStore, 
                       function_cache: dict) -> list[dict]:
    """Compute per-unit statistics in decomp.dev format."""
    
    units = []
    drift = {
        'kb_ported_missing_meta': 0,
        'meta_ported_missing_kb': 0,
    }
    functions_data = function_cache.get('functions', {})
    
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
            
            func_entry = {
                'address': addr_hex,
                'name': name,
                'size': size,
                'status': status,
                'ported': is_ported,
                'match_percent': None  # Would need objdiff integration
            }
            unit_funcs.append(func_entry)
            
            total_bytes += size
            if is_ported:
                ported_bytes += size
                ported_count += 1
        
        # Skip empty units
        if not unit_funcs:
            continue
            
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
                'bytes_percent': round(ported_bytes / total_bytes * 100, 2) if total_bytes else 0
            }
        }
        units.append(unit)
    
    # Sort by ported percentage (most complete first)
    units.sort(key=lambda x: x['summary']['percent'], reverse=True)
    return units, drift


def generate_report(output_path: str, with_functions: bool = False) -> dict:
    """Generate full decomp.dev-compatible report."""
    
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
    cache_path = os.path.join(root_dir, 'build', 'function_sizes.json')
    
    # Load knowledge base
    kb = KnowledgeBase.deserialize()
    store = MetadataStore(kb)
    store.load()
    
    # Load function sizes
    function_cache = load_function_sizes(cache_path)
    
    # Compute unit stats
    units, drift = compute_unit_stats(kb, store, function_cache)
    
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
            }
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
    
    # Include per-unit breakdown (always)
    report['units'] = [
        {k: v for k, v in u.items() if k != 'functions'}  # Exclude function list for brevity
        for u in units
    ]
    
    # Include per-function data if requested
    if with_functions:
        report['units_with_functions'] = units
    
    # Write output
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump(report, f, indent=2)
    
    return report


def generate_html(report: dict, output_path: str, history_path: str = None):
    """Generate an enhanced HTML dashboard with historical charts."""
    
    units = report.get('units', [])
    summary = report['summary']
    
    # Load historical data if available
    history_data = None
    if history_path and os.path.exists(history_path):
        try:
            with open(history_path) as f:
                history_data = json.load(f)
        except:
            pass
    
    # Prepare chart data
    chart_data = {'labels': [], 'functions': [], 'velocity': []}
    eta_info = None
    velocity_info = None
    
    if history_data and history_data.get('snapshots'):
        snapshots = history_data['snapshots'][-90:]  # Last 90 snapshots
        for snap in snapshots:
            chart_data['labels'].append(snap['timestamp'][:10])
            chart_data['functions'].append(snap['summary']['functions']['ported'])
        
        # Calculate velocity
        if len(snapshots) >= 2:
            recent = snapshots[-7:] if len(snapshots) >= 7 else snapshots
            funcs_diff = recent[-1]['summary']['functions']['ported'] - recent[0]['summary']['functions']['ported']
            days = max(1, len(recent))
            velocity_info = funcs_diff / days
            
            # Simple ETA
            remaining = summary['functions']['total'] - summary['functions']['ported']
            if velocity_info > 0:
                days_remaining = int(remaining / velocity_info)
                from datetime import datetime, timedelta
                eta_date = (datetime.now() + timedelta(days=days_remaining)).strftime('%Y-%m-%d')
                eta_info = {'days': days_remaining, 'date': eta_date}
    
    # Build HTML with Chart.js
    html = f'''<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Halo CE Xbox - Decompilation Progress</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
    <style>
        :root {{
            --bg-primary: #0d1117;
            --bg-secondary: #161b22;
            --bg-tertiary: #21262d;
            --border: #30363d;
            --text-primary: #c9d1d9;
            --text-secondary: #8b949e;
            --accent-blue: #58a6ff;
            --accent-green: #238636;
            --accent-yellow: #d29922;
        }}
        
        * {{
            box-sizing: border-box;
        }}
        
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            margin: 0;
            padding: 0;
            background: var(--bg-primary);
            color: var(--text-primary);
            line-height: 1.6;
        }}
        
        .container {{
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
        }}
        
        header {{
            text-align: center;
            padding: 40px 0;
            border-bottom: 1px solid var(--border);
            margin-bottom: 30px;
        }}
        
        h1 {{
            color: var(--accent-blue);
            margin: 0 0 10px 0;
            font-size: 2.5em;
        }}
        
        .subtitle {{
            color: var(--text-secondary);
            font-size: 1.1em;
        }}
        
        h2 {{
            color: var(--accent-blue);
            margin-top: 40px;
            margin-bottom: 20px;
            padding-bottom: 10px;
            border-bottom: 1px solid var(--border);
        }}
        
        .summary {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }}
        
        .card {{
            background: var(--bg-secondary);
            border: 1px solid var(--border);
            border-radius: 12px;
            padding: 24px;
            transition: transform 0.2s, box-shadow 0.2s;
        }}
        
        .card:hover {{
            transform: translateY(-2px);
            box-shadow: 0 4px 20px rgba(0,0,0,0.3);
        }}
        
        .stat-value {{
            font-size: 2.5em;
            font-weight: bold;
            color: var(--accent-blue);
            margin: 10px 0;
        }}
        
        .stat-label {{
            color: var(--text-secondary);
            font-size: 0.95em;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }}
        
        .progress-bar {{
            width: 100%;
            height: 24px;
            background: var(--bg-tertiary);
            border-radius: 12px;
            overflow: hidden;
            margin-top: 15px;
            position: relative;
        }}
        
        .progress-fill {{
            height: 100%;
            background: linear-gradient(90deg, var(--accent-green), #2ea043);
            border-radius: 12px;
            transition: width 0.5s ease;
            display: flex;
            align-items: center;
            justify-content: flex-end;
            padding-right: 10px;
        }}
        
        .progress-text {{
            color: white;
            font-weight: bold;
            font-size: 0.85em;
        }}
        
        .chart-container {{
            background: var(--bg-secondary);
            border: 1px solid var(--border);
            border-radius: 12px;
            padding: 24px;
            margin: 20px 0;
            position: relative;
            height: 400px;
        }}
        
        .chart-title {{
            color: var(--text-secondary);
            font-size: 0.9em;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin-bottom: 15px;
        }}
        
        .velocity-badge {{
            display: inline-block;
            background: var(--accent-green);
            color: white;
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.85em;
            font-weight: bold;
            margin-left: 10px;
        }}
        
        table {{
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
            background: var(--bg-secondary);
            border: 1px solid var(--border);
            border-radius: 12px;
            overflow: hidden;
        }}
        
        th, td {{
            padding: 14px 16px;
            text-align: left;
            border-bottom: 1px solid var(--border);
        }}
        
        th {{
            background: var(--bg-tertiary);
            color: var(--text-secondary);
            font-weight: 600;
            cursor: pointer;
            user-select: none;
            font-size: 0.9em;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }}
        
        th:hover {{
            background: #30363d;
        }}
        
        tr:hover {{
            background: rgba(88, 166, 255, 0.05);
        }}
        
        tr:last-child td {{
            border-bottom: none;
        }}
        
        .unit-name {{
            font-family: 'SF Mono', Monaco, monospace;
            color: var(--accent-blue);
            font-weight: 500;
        }}
        
        .source-path {{
            color: var(--text-secondary);
            font-size: 0.85em;
        }}
        
        .percent {{
            font-weight: bold;
            color: var(--accent-blue);
        }}
        
        .complete {{
            color: #3fb950;
        }}
        
        .none {{
            color: var(--text-secondary);
        }}
        
        .progress-cell {{
            width: 150px;
        }}
        
        .mini-bar {{
            width: 100%;
            height: 8px;
            background: var(--bg-tertiary);
            border-radius: 4px;
            overflow: hidden;
            display: inline-block;
            vertical-align: middle;
            margin-right: 8px;
        }}
        
        .mini-fill {{
            height: 100%;
            background: var(--accent-blue);
            border-radius: 4px;
        }}
        
        .meta {{
            color: var(--text-secondary);
            font-size: 0.85em;
            margin-top: 40px;
            padding-top: 20px;
            border-top: 1px solid var(--border);
            text-align: center;
        }}
        
        .grid-2 {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(500px, 1fr));
            gap: 20px;
        }}
        
        @media (max-width: 768px) {{
            .grid-2 {{
                grid-template-columns: 1fr;
            }}
            
            h1 {{
                font-size: 1.8em;
            }}
            
            .summary {{
                grid-template-columns: 1fr;
            }}
        }}
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>Halo: Combat Evolved (Xbox)</h1>
            <div class="subtitle">Decompilation Progress Dashboard</div>
        </header>
        
        <div class="summary">
            <div class="card">
                <div class="stat-label">Overall Progress</div>
                <div class="stat-value">{summary['functions']['percent']:.1f}%</div>
                <div class="stat-label">{summary['functions']['ported']:,} / {summary['functions']['total']:,} functions</div>
                <div class="progress-bar">
                    <div class="progress-fill" style="width: {summary['functions']['percent']}%; min-width: 50px;">
                        <span class="progress-text">{summary['functions']['percent']:.1f}%</span>
                    </div>
                </div>
            </div>
            
            <div class="card">
                <div class="stat-label">Byte Coverage</div>
                <div class="stat-value">{summary['bytes']['percent']:.1f}%</div>
                <div class="stat-label">{summary['bytes']['ported']:,} / {summary['bytes']['total']:,} bytes</div>
                <div class="progress-bar">
                    <div class="progress-fill" style="width: {summary['bytes']['percent']}%; min-width: 50px;">
                        <span class="progress-text">{summary['bytes']['percent']:.1f}%</span>
                    </div>
                </div>
            </div>
            
            <div class="card">
                <div class="stat-label">Translation Units</div>
                <div class="stat-value">{len(units)}</div>
                <div class="stat-label">Object files tracked</div>
                {f'<div style="margin-top: 10px; color: var(--accent-yellow);">Velocity: {velocity_info:.1f} funcs/day</div>' if velocity_info else ''}
                {f'<div style="margin-top: 5px; font-size: 0.9em;">ETA: {eta_info["date"]}</div>' if eta_info else ''}
            </div>
        </div>
        
        {'<h2>📈 Historical Progress</h2>' if chart_data['labels'] else ''}
        {'<div class="grid-2">' if chart_data['labels'] else ''}
            {'<div class="chart-container"><div class="chart-title">Functions Ported Over Time</div><canvas id="progressChart"></canvas></div>' if chart_data['labels'] else ''}
            {'<div class="chart-container"><div class="chart-title">Daily Velocity (Functions/Day)</div><canvas id="velocityChart"></canvas></div>' if chart_data['labels'] else ''}
        {'</div>' if chart_data['labels'] else ''}
        
        <h2>📊 Per-Unit Breakdown</h2>
        <table id="units-table">
            <thead>
                <tr>
                    <th onclick="sortTable(0)">Unit Name ↕</th>
                    <th onclick="sortTable(1)">Source Path ↕</th>
                    <th onclick="sortTable(2)">Functions ↕</th>
                    <th onclick="sortTable(3)">Ported ↕</th>
                    <th onclick="sortTable(4)">Progress ↕</th>
                    <th onclick="sortTable(5)">Bytes ↕</th>
                </tr>
            </thead>
            <tbody>
'''
    
    for unit in units:
        s = unit['summary']
        name = unit['name']
        source = unit.get('source_path', '?')
        percent = s['percent']
        
        if percent == 100:
            percent_class = 'complete'
        elif percent == 0:
            percent_class = 'none'
        else:
            percent_class = 'percent'
        
        html += f'''                <tr>
                    <td class="unit-name">{name}</td>
                    <td class="source-path">{source}</td>
                    <td>{s['total']}</td>
                    <td>{s['ported']}</td>
                    <td class="progress-cell">
                        <div class="mini-bar">
                            <div class="mini-fill" style="width: {percent}%"></div>
                        </div>
                        <span class="{percent_class}">{percent:.1f}%</span>
                    </td>
                    <td>{s['bytes_ported']:,} / {s['bytes_total']:,}</td>
                </tr>
'''
    
    # Chart.js initialization
    chart_script = ''
    if chart_data['labels']:
        # Calculate velocity data
        velocity_data = [0]
        for i in range(1, len(chart_data['functions'])):
            velocity_data.append(chart_data['functions'][i] - chart_data['functions'][i-1])
        
        chart_script = f'''
        <script>
            const progressCtx = document.getElementById('progressChart').getContext('2d');
            new Chart(progressCtx, {{
                type: 'line',
                data: {{
                    labels: {json.dumps(chart_data['labels'])},
                    datasets: [{{
                        label: 'Functions Ported',
                        data: {json.dumps(chart_data['functions'])},
                        borderColor: '#58a6ff',
                        backgroundColor: 'rgba(88, 166, 255, 0.1)',
                        borderWidth: 2,
                        tension: 0.4,
                        fill: true,
                        pointRadius: 3,
                        pointHoverRadius: 6
                    }}]
                }},
                options: {{
                    responsive: true,
                    maintainAspectRatio: false,
                    interaction: {{
                        intersect: false,
                        mode: 'index'
                    }},
                    plugins: {{
                        legend: {{
                            display: false
                        }},
                        tooltip: {{
                            backgroundColor: '#161b22',
                            borderColor: '#30363d',
                            borderWidth: 1,
                            titleColor: '#c9d1d9',
                            bodyColor: '#c9d1d9',
                            padding: 12,
                            cornerRadius: 8
                        }}
                    }},
                    scales: {{
                        x: {{
                            grid: {{
                                color: '#21262d'
                            }},
                            ticks: {{
                                color: '#8b949e',
                                maxRotation: 45
                            }}
                        }},
                        y: {{
                            grid: {{
                                color: '#21262d'
                            }},
                            ticks: {{
                                color: '#8b949e'
                            }},
                            beginAtZero: true
                        }}
                    }}
                }}
            }});
            
            const velocityCtx = document.getElementById('velocityChart').getContext('2d');
            new Chart(velocityCtx, {{
                type: 'bar',
                data: {{
                    labels: {json.dumps(chart_data['labels'][1:] if len(chart_data['labels']) > 1 else [])},
                    datasets: [{{
                        label: 'Functions/Day',
                        data: {json.dumps(velocity_data[1:] if len(velocity_data) > 1 else [])},
                        backgroundColor: 'rgba(35, 134, 54, 0.7)',
                        borderColor: '#238636',
                        borderWidth: 1,
                        borderRadius: 4
                    }}]
                }},
                options: {{
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {{
                        legend: {{
                            display: false
                        }},
                        tooltip: {{
                            backgroundColor: '#161b22',
                            borderColor: '#30363d',
                            borderWidth: 1,
                            titleColor: '#c9d1d9',
                            bodyColor: '#c9d1d9',
                            padding: 12,
                            cornerRadius: 8
                        }}
                    }},
                    scales: {{
                        x: {{
                            grid: {{
                                color: '#21262d'
                            }},
                            ticks: {{
                                color: '#8b949e',
                                maxRotation: 45
                            }}
                        }},
                        y: {{
                            grid: {{
                                color: '#21262d'
                            }},
                            ticks: {{
                                color: '#8b949e'
                            }},
                            beginAtZero: true
                        }}
                    }}
                }}
            }});
        </script>
'''
    
    html += f'''            </tbody>
        </table>
        
        <div class="meta">
            Generated: {report['meta']['timestamp']} | 
            Commit: {report['meta']['commit']} ({report['meta']['branch']}) | 
            Tool: generate_decomp_report.py {report['meta']['tool_version']}
        </div>
    </div>
    
    <script>
        let sortDir = {{}};
        
        function sortTable(colIndex) {{
            const table = document.getElementById('units-table');
            const tbody = table.tBodies[0];
            const rows = Array.from(tbody.rows);
            
            sortDir[colIndex] = !sortDir[colIndex];
            
            rows.sort((a, b) => {{
                let aVal = a.cells[colIndex].textContent.trim();
                let bVal = b.cells[colIndex].textContent.trim();
                
                // Try numeric sort
                const aNum = parseFloat(aVal.replace(/,/g, '').replace('%', ''));
                const bNum = parseFloat(bVal.replace(/,/g, '').replace('%', ''));
                
                if (!isNaN(aNum) && !isNaN(bNum)) {{
                    return sortDir[colIndex] ? aNum - bNum : bNum - aNum;
                }}
                
                return sortDir[colIndex] ? aVal.localeCompare(bVal) : bVal.localeCompare(aVal);
            }});
            
            rows.forEach(row => tbody.appendChild(row));
        }}
    </script>
    {chart_script}
</body>
</html>
'''
    
    with open(output_path, 'w') as f:
        f.write(html)


def main():
    ap = argparse.ArgumentParser(description='Generate decomp.dev-style progress report')
    ap.add_argument('-o', '--output', default='artifacts/progress/report.json',
                    help='Output JSON file path')
    ap.add_argument('--html', metavar='PATH', 
                    help='Also generate HTML dashboard at specified path')
    ap.add_argument('--with-functions', action='store_true',
                    help='Include per-function data (large file)')
    ap.add_argument('--pretty', action='store_true',
                    help='Pretty print JSON output')
    ap.add_argument('--history', default='artifacts/progress/history.json',
                    help='Path to history.json for historical charts')
    args = ap.parse_args()
    
    print('Generating decomp.dev-compatible report...')
    
    report = generate_report(args.output, args.with_functions)
    
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
