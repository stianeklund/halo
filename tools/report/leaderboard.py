#!/usr/bin/env python3
"""
Generate a leaderboard of translation units by various metrics.

Shows:
- Units closest to completion
- Best targets to work on (frontier analysis)
- Recently completed units
- Largest remaining chunks
"""

import sys
import os
import json
import argparse
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../..'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../..', 'tools'))


def load_report(path: str) -> dict:
    """Load a report JSON file."""
    with open(path) as f:
        return json.load(f)


def show_closest_to_completion(units: list, n: int = 10):
    """Show units closest to 100% completion."""
    # Filter units that are >0% but <100%
    in_progress = [u for u in units if 0 < u['summary']['percent'] < 100]
    
    # Sort by percentage descending, then by total functions
    in_progress.sort(key=lambda x: (x['summary']['percent'], x['summary']['total']), 
                    reverse=True)
    
    print("=" * 80)
    print(f"TOP {n} UNITS CLOSEST TO COMPLETION")
    print("=" * 80)
    print(f"{'Unit Name':<40} {'Funcs':>8} {'Ported':>8} {'Remain':>8} {'%':>8}")
    print("-" * 80)
    
    for unit in in_progress[:n]:
        s = unit['summary']
        remaining = s['total'] - s['ported']
        print(f"{unit['name']:<40} {s['total']:>8} {s['ported']:>8} {remaining:>8} "
              f"{s['percent']:>7.1f}%")
    
    print()


def show_best_targets(units: list, n: int = 10):
    """Show best targets for next work (using frontier-style scoring)."""
    # Score based on: partially ported (has infrastructure) + has remaining functions
    # + smaller total size (easier to finish)
    
    def score_unit(unit):
        s = unit['summary']
        if s['percent'] == 0:
            return -1  # Skip unstarted
        if s['percent'] == 100:
            return -1  # Skip complete
        
        # Prefer units with:
        # - Higher existing progress (momentum)
        # - Fewer remaining functions (quick win)
        # - Reasonable total size
        remaining = s['total'] - s['ported']
        return (s['percent'] * 0.5) + ((100 - remaining) * 0.5)
    
    scored = [(u, score_unit(u)) for u in units]
    scored = [(u, s) for u, s in scored if s > 0]
    scored.sort(key=lambda x: x[1], reverse=True)
    
    print("=" * 80)
    print(f"TOP {n} RECOMMENDED TARGETS (Best Bang for Buck)")
    print("=" * 80)
    print(f"{'Unit Name':<40} {'Funcs':>8} {'Ported':>8} {'Remain':>8} {'%':>8}")
    print("-" * 80)
    
    for unit, score in scored[:n]:
        s = unit['summary']
        remaining = s['total'] - s['ported']
        print(f"{unit['name']:<40} {s['total']:>8} {s['ported']:>8} {remaining:>8} "
              f"{s['percent']:>7.1f}%")
    
    print()


def show_largest_remaining(units: list, n: int = 10):
    """Show units with most remaining bytes (biggest impact potential)."""
    remaining_bytes = []
    for unit in units:
        s = unit['summary']
        if s['percent'] < 100:
            bytes_remaining = s['bytes_total'] - s['bytes_ported']
            remaining_bytes.append((unit, bytes_remaining))
    
    remaining_bytes.sort(key=lambda x: x[1], reverse=True)
    
    print("=" * 80)
    print(f"TOP {n} UNITS BY REMAINING SIZE (Biggest Impact Potential)")
    print("=" * 80)
    print(f"{'Unit Name':<40} {'Total KB':>10} {'Remain KB':>10} {'Funcs':>8} {'%':>8}")
    print("-" * 80)
    
    for unit, remaining in remaining_bytes[:n]:
        s = unit['summary']
        total_kb = s['bytes_total'] / 1024
        remain_kb = remaining / 1024
        print(f"{unit['name']:<40} {total_kb:>9.1f} {remain_kb:>9.1f} "
              f"{s['total']:>8} {s['percent']:>7.1f}%")
    
    print()


def show_quick_wins(units: list, n: int = 10):
    """Show small units that can be completed quickly."""
    # Units with <= 5 total functions and some progress
    candidates = []
    for unit in units:
        s = unit['summary']
        if 1 <= s['total'] <= 5 and 0 < s['percent'] < 100:
            remaining = s['total'] - s['ported']
            candidates.append((unit, remaining))
    
    candidates.sort(key=lambda x: x[1])
    
    print("=" * 80)
    print(f"TOP {n} QUICK WINS (Small Units to Complete)")
    print("=" * 80)
    print(f"{'Unit Name':<40} {'Total':>8} {'Ported':>8} {'Remain':>8} {'%':>8}")
    print("-" * 80)
    
    for unit, remaining in candidates[:n]:
        s = unit['summary']
        print(f"{unit['name']:<40} {s['total']:>8} {s['ported']:>8} {remaining:>8} "
              f"{s['percent']:>7.1f}%")
    
    print()


def show_completed_units(units: list, n: int = 15):
    """Show recently completed units."""
    completed = [u for u in units if u['summary']['percent'] == 100]
    completed.sort(key=lambda x: x['name'])
    
    print("=" * 80)
    print(f"COMPLETED UNITS ({len(completed)} total, showing first {n})")
    print("=" * 80)
    print(f"{'Unit Name':<50} {'Funcs':>8} {'Source':<20}")
    print("-" * 80)
    
    for unit in completed[:n]:
        s = unit['summary']
        source = unit.get('source_path', '?') or '?'
        if source.startswith('src/halo/'):
            source = source[9:]  # Remove prefix
        print(f"{unit['name']:<50} {s['total']:>8} {source:<20}")
    
    if len(completed) > n:
        print(f"\n  ... and {len(completed) - n} more")
    
    print()


def show_category_breakdown(units: list):
    """Show breakdown by source directory category."""
    categories = defaultdict(lambda: {'total': 0, 'ported': 0, 'units': 0})
    
    for unit in units:
        source = unit.get('source_path', '?') or '?'
        if source.startswith('src/halo/'):
            parts = source[9:].split('/')
            category = parts[0] if parts else 'unknown'
        else:
            category = 'unknown'
        
        s = unit['summary']
        categories[category]['total'] += s['total']
        categories[category]['ported'] += s['ported']
        categories[category]['units'] += 1
    
    # Sort by ported percentage
    cat_list = []
    for name, data in categories.items():
        pct = (data['ported'] / data['total'] * 100) if data['total'] else 0
        cat_list.append((name, data, pct))
    
    cat_list.sort(key=lambda x: x[2], reverse=True)
    
    print("=" * 80)
    print("PROGRESS BY CATEGORY")
    print("=" * 80)
    print(f"{'Category':<25} {'Units':>8} {'Funcs':>10} {'Ported':>10} {'%':>8}")
    print("-" * 80)
    
    for name, data, pct in cat_list:
        print(f"{name:<25} {data['units']:>8} {data['total']:>10} "
              f"{data['ported']:>10} {pct:>7.1f}%")
    
    print()


def show_size_distribution(units: list):
    """Show distribution of unit sizes."""
    sizes = [u['summary']['total'] for u in units]
    
    buckets = {
        'Tiny (1-2 funcs)': 0,
        'Small (3-10)': 0,
        'Medium (11-50)': 0,
        'Large (51-100)': 0,
        'Huge (100+)': 0
    }
    
    for size in sizes:
        if size <= 2:
            buckets['Tiny (1-2 funcs)'] += 1
        elif size <= 10:
            buckets['Small (3-10)'] += 1
        elif size <= 50:
            buckets['Medium (11-50)'] += 1
        elif size <= 100:
            buckets['Large (51-100)'] += 1
        else:
            buckets['Huge (100+)'] += 1
    
    print("=" * 80)
    print("UNIT SIZE DISTRIBUTION")
    print("=" * 80)
    
    for label, count in buckets.items():
        pct = count / len(units) * 100
        bar_len = int(pct / 2)
        bar = '█' * bar_len
        print(f"{label:<20} {count:>5} ({pct:>5.1f}%) {bar}")
    
    print(f"\nTotal units: {len(units)}")
    print()


def main():
    ap = argparse.ArgumentParser(description='Generate progress leaderboard')
    ap.add_argument('--report', default='artifacts/progress/report.json',
                   help='Path to progress report JSON')
    ap.add_argument('--closest', type=int, metavar='N', 
                   help='Show N units closest to completion')
    ap.add_argument('--targets', type=int, metavar='N',
                   help='Show N recommended targets')
    ap.add_argument('--largest', type=int, metavar='N',
                   help='Show N units by remaining size')
    ap.add_argument('--quick-wins', type=int, metavar='N',
                   help='Show N quick win targets')
    ap.add_argument('--completed', type=int, metavar='N',
                   help='Show N completed units')
    ap.add_argument('--category', action='store_true',
                   help='Show breakdown by category')
    ap.add_argument('--sizes', action='store_true',
                   help='Show size distribution')
    ap.add_argument('--all', action='store_true',
                   help='Show all sections (default)')
    
    args = ap.parse_args()
    
    if not os.path.exists(args.report):
        print(f"Error: Report not found at {args.report}")
        print("Generate one with: python3 tools/report/generate_decomp_report.py")
        sys.exit(1)
    
    report = load_report(args.report)
    units = report.get('units', [])
    
    # If no specific flags, show all
    show_all = args.all or not any([
        args.closest, args.targets, args.largest, args.quick_wins,
        args.completed, args.category, args.sizes
    ])
    
    print()
    print("🏆 HALO CE XBOX - DECOMPILATION LEADERBOARD")
    print(f"Generated: {report['meta']['timestamp']}")
    print(f"Commit: {report['meta']['commit']}")
    print()
    
    if show_all or args.closest:
        show_closest_to_completion(units, args.closest or 10)
    
    if show_all or args.targets:
        show_best_targets(units, args.targets or 10)
    
    if show_all or args.largest:
        show_largest_remaining(units, args.largest or 10)
    
    if show_all or args.quick_wins:
        show_quick_wins(units, args.quick_wins or 10)
    
    if show_all or args.completed:
        show_completed_units(units, args.completed or 15)
    
    if show_all or args.category:
        show_category_breakdown(units)
    
    if show_all or args.sizes:
        show_size_distribution(units)


if __name__ == '__main__':
    main()
