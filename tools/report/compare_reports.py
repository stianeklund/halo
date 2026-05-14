#!/usr/bin/env python3
"""
Compare two progress reports to show delta/changes.

Usage:
    python3 tools/report/compare_reports.py report_v1.json report_v2.json
    python3 tools/report/compare_reports.py --git HEAD~10 HEAD
"""

import sys
import os
import json
import argparse
from datetime import datetime
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../..'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../..', 'tools'))


def load_report(path: str) -> dict:
    """Load a report JSON file."""
    with open(path) as f:
        return json.load(f)


def format_delta(value: int, suffix: str = '') -> str:
    """Format a delta value with color."""
    if value > 0:
        return f"+{value:,}{suffix}"
    elif value < 0:
        return f"{value:,}{suffix}"
    else:
        return f"±0{suffix}"


def format_percent_delta(value: float) -> str:
    """Format a percentage delta."""
    if value > 0:
        return f"+{value:.2f}%"
    elif value < 0:
        return f"{value:.2f}%"
    else:
        return "±0.00%"


def compare_units(old_units: list, new_units: list) -> dict:
    """Compare unit-by-unit progress."""
    old_by_name = {u['name']: u for u in old_units}
    new_by_name = {u['name']: u for u in new_units}
    
    changes = {
        'completed': [],      # Units that reached 100%
        'started': [],        # Units that went from 0% to >0%
        'improved': [],       # Units with progress increase
        'regressed': [],      # Units with progress decrease (shouldn't happen normally)
        'new': [],            # New units added
        'removed': []         # Units removed
    }
    
    # Check for new and improved units
    for name, new_unit in new_by_name.items():
        new_pct = new_unit['summary']['percent']
        
        if name not in old_by_name:
            changes['new'].append(new_unit)
            continue
            
        old_unit = old_by_name[name]
        old_pct = old_unit['summary']['percent']
        
        if old_pct == 0 and new_pct > 0:
            changes['started'].append({
                'name': name,
                'old': old_unit['summary'],
                'new': new_unit['summary']
            })
        elif old_pct < 100 and new_pct == 100:
            changes['completed'].append({
                'name': name,
                'old': old_unit['summary'],
                'new': new_unit['summary']
            })
        elif new_pct > old_pct:
            changes['improved'].append({
                'name': name,
                'old': old_unit['summary'],
                'new': new_unit['summary']
            })
        elif new_pct < old_pct:
            changes['regressed'].append({
                'name': name,
                'old': old_unit['summary'],
                'new': new_unit['summary']
            })
    
    # Check for removed units
    for name, old_unit in old_by_name.items():
        if name not in new_by_name:
            changes['removed'].append(old_unit)
    
    return changes


def print_comparison(old_report: dict, new_report: dict, changes: dict):
    """Print a formatted comparison."""
    old_summary = old_report['summary']
    new_summary = new_report['summary']
    
    print("=" * 60)
    print("PROGRESS COMPARISON REPORT")
    print("=" * 60)
    print()
    
    # Header info
    old_commit = old_report['meta'].get('commit', 'unknown')[:8]
    new_commit = new_report['meta'].get('commit', 'unknown')[:8]
    old_time = old_report['meta'].get('timestamp', 'unknown')
    new_time = new_report['meta'].get('timestamp', 'unknown')
    
    print(f"From: {old_commit} ({old_time})")
    print(f"To:   {new_commit} ({new_time})")
    print()
    
    # Overall stats
    print("-" * 60)
    print("OVERALL PROGRESS")
    print("-" * 60)
    
    func_delta = new_summary['functions']['ported'] - old_summary['functions']['ported']
    func_pct_delta = new_summary['functions']['percent'] - old_summary['functions']['percent']
    
    bytes_delta = new_summary['bytes']['ported'] - old_summary['bytes']['ported']
    bytes_pct_delta = new_summary['bytes']['percent'] - old_summary['bytes']['percent']
    
    print(f"  Functions: {old_summary['functions']['ported']:,} → {new_summary['functions']['ported']:,} "
          f"({format_delta(func_delta)} / {format_percent_delta(func_pct_delta)})")
    print(f"  Bytes:     {old_summary['bytes']['ported']:,} → {new_summary['bytes']['ported']:,} "
          f"({format_delta(bytes_delta)} / {format_percent_delta(bytes_pct_delta)})")
    print()
    
    # Unit changes
    print("-" * 60)
    print("TRANSLATION UNIT CHANGES")
    print("-" * 60)
    
    if changes['completed']:
        print(f"\n✅ Completed ({len(changes['completed'])} units):")
        for item in changes['completed'][:10]:
            name = item['name']
            funcs = item['new']['ported']
            print(f"   • {name:<40} ({funcs} functions)")
        if len(changes['completed']) > 10:
            print(f"   ... and {len(changes['completed']) - 10} more")
    
    if changes['started']:
        print(f"\n🚀 Started ({len(changes['started'])} units):")
        for item in changes['started'][:5]:
            name = item['name']
            funcs = item['new']['ported']
            print(f"   • {name:<40} ({funcs} functions)")
    
    if changes['improved']:
        print(f"\n📈 Improved ({len(changes['improved'])} units):")
        # Sort by function delta
        improved = sorted(changes['improved'], 
                         key=lambda x: x['new']['ported'] - x['old']['ported'],
                         reverse=True)
        for item in improved[:10]:
            name = item['name']
            old_funcs = item['old']['ported']
            new_funcs = item['new']['ported']
            delta = new_funcs - old_funcs
            old_pct = item['old']['percent']
            new_pct = item['new']['percent']
            print(f"   • {name:<40} {old_funcs}→{new_funcs} (+{delta}) "
                  f"[{old_pct:.1f}%→{new_pct:.1f}%]")
        if len(changes['improved']) > 10:
            print(f"   ... and {len(changes['improved']) - 10} more")
    
    if changes['new']:
        print(f"\n🆕 New Units ({len(changes['new'])}):")
        for unit in changes['new'][:5]:
            name = unit['name']
            total = unit['summary']['total']
            print(f"   • {name:<40} ({total} functions)")
    
    if changes['regressed']:
        print(f"\n⚠️  Regressed ({len(changes['regressed'])} units):")
        for item in changes['regressed'][:5]:
            name = item['name']
            print(f"   • {name}")
    
    total_changes = (len(changes['completed']) + len(changes['started']) + 
                    len(changes['improved']) + len(changes['new']))
    
    if total_changes == 0:
        print("\n  No changes detected.")
    
    print()
    print("=" * 60)


def generate_github_summary(old_report: dict, new_report: dict, changes: dict):
    """Generate GitHub Actions step summary format."""
    old_summary = old_report['summary']
    new_summary = new_report['summary']
    
    func_delta = new_summary['functions']['ported'] - old_summary['functions']['ported']
    bytes_delta = new_summary['bytes']['ported'] - old_summary['bytes']['ported']
    
    summary_file = os.environ.get('GITHUB_STEP_SUMMARY')
    if not summary_file:
        return
    
    with open(summary_file, 'a') as f:
        f.write("## 📊 Progress Comparison\n\n")
        f.write("| Metric | Before | After | Delta |\n")
        f.write("|--------|--------|-------|-------|\n")
        f.write(f"| Functions | {old_summary['functions']['ported']:,} | "
                f"{new_summary['functions']['ported']:,} | "
                f"{'+' if func_delta >= 0 else ''}{func_delta:,} |\n")
        f.write(f"| Bytes | {old_summary['bytes']['ported']:,} | "
                f"{new_summary['bytes']['ported']:,} | "
                f"{'+' if bytes_delta >= 0 else ''}{bytes_delta:,} |\n")
        f.write(f"| Coverage | {old_summary['functions']['percent']:.2f}% | "
                f"{new_summary['functions']['percent']:.2f}% | "
                f"{'+' if func_delta >= 0 else ''}{new_summary['functions']['percent'] - old_summary['functions']['percent']:.2f}% |\n")
        f.write("\n")
        
        if changes['completed']:
            f.write(f"### ✅ Completed Units ({len(changes['completed'])})\n")
            for item in changes['completed'][:5]:
                f.write(f"- **{item['name']}**: {item['new']['ported']} functions\n")
            f.write("\n")
        
        if changes['improved']:
            f.write(f"### 📈 Most Improved\n")
            improved = sorted(changes['improved'], 
                            key=lambda x: x['new']['ported'] - x['old']['ported'],
                            reverse=True)[:5]
            for item in improved:
                delta = item['new']['ported'] - item['old']['ported']
                f.write(f"- **{item['name']}**: +{delta} functions\n")
            f.write("\n")


def generate_git_reports(base_ref: str, head_ref: str, output_dir: str):
    """Generate reports for two git refs."""
    import subprocess
    import tempfile
    
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
    
    # Save current state
    current_branch = subprocess.run(
        ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
        capture_output=True, text=True, cwd=root_dir
    ).stdout.strip()
    
    try:
        # Generate base report
        subprocess.run(['git', 'checkout', base_ref], cwd=root_dir, 
                      capture_output=True, check=True)
        base_path = os.path.join(output_dir, 'base_report.json')
        subprocess.run([
            'python3', 'tools/report/generate_decomp_report.py',
            '-o', base_path
        ], cwd=root_dir, check=True)
        
        # Generate head report
        subprocess.run(['git', 'checkout', head_ref], cwd=root_dir,
                      capture_output=True, check=True)
        head_path = os.path.join(output_dir, 'head_report.json')
        subprocess.run([
            'python3', 'tools/report/generate_decomp_report.py',
            '-o', head_path
        ], cwd=root_dir, check=True)
        
        return base_path, head_path
        
    finally:
        # Restore original state
        subprocess.run(['git', 'checkout', current_branch], 
                      cwd=root_dir, capture_output=True)


def main():
    ap = argparse.ArgumentParser(description='Compare two progress reports')
    ap.add_argument('old_report', nargs='?', help='Path to old report JSON')
    ap.add_argument('new_report', nargs='?', help='Path to new report JSON')
    ap.add_argument('--git', action='store_true', 
                   help='Compare two git refs instead of files')
    ap.add_argument('--base-ref', default='HEAD~1',
                   help='Base git ref (default: HEAD~1)')
    ap.add_argument('--head-ref', default='HEAD',
                   help='Head git ref (default: HEAD)')
    ap.add_argument('--json', action='store_true',
                   help='Output JSON instead of text')
    ap.add_argument('--github-summary', action='store_true',
                   help='Write GitHub Actions step summary')
    
    args = ap.parse_args()
    
    if args.git:
        # Generate reports from git refs
        import tempfile
        with tempfile.TemporaryDirectory() as tmpdir:
            print(f"Generating reports for {args.base_ref} and {args.head_ref}...")
            old_path, new_path = generate_git_reports(
                args.base_ref, args.head_ref, tmpdir
            )
            old_report = load_report(old_path)
            new_report = load_report(new_path)
    elif args.old_report and args.new_report:
        old_report = load_report(args.old_report)
        new_report = load_report(args.new_report)
    else:
        ap.error("Either provide two report files or use --git")
    
    # Compare
    changes = compare_units(old_report.get('units', []), 
                           new_report.get('units', []))
    
    if args.json:
        output = {
            'old_commit': old_report['meta'].get('commit'),
            'new_commit': new_report['meta'].get('commit'),
            'function_delta': new_report['summary']['functions']['ported'] - 
                             old_report['summary']['functions']['ported'],
            'byte_delta': new_report['summary']['bytes']['ported'] - 
                         old_report['summary']['bytes']['ported'],
            'changes': changes
        }
        print(json.dumps(output, indent=2))
    else:
        print_comparison(old_report, new_report, changes)
    
    if args.github_summary:
        generate_github_summary(old_report, new_report, changes)


if __name__ == '__main__':
    main()
