#!/usr/bin/env python3
"""
Matching percentage tracking using objdiff.

Integrates with objdiff CLI to get actual binary matching percentages
for ported functions, distinguishing between "ported" (exists in source)
and "matching" (byte-accurate to original).

Usage:
    python3 tools/report/matching.py --unit actors     # Check specific unit
    python3 tools/report/matching.py --all-ported     # Check all ported units
    python3 tools/report/matching.py --update-cache   # Update matching cache
"""

import json
import os
import argparse
import subprocess
from pathlib import Path
from typing import Optional, Dict, List
from datetime import datetime


DEFAULT_CACHE_PATH = 'artifacts/progress/matching_cache.json'
DEFAULT_OBJDIFF_PATH = 'tools/objdiff-cli-linux-x86_64'
DEFAULT_OBJDIFF_JSON = 'objdiff.json'


class MatchingTracker:
    """Tracks matching percentages using objdiff."""
    
    def __init__(self, cache_path: str = DEFAULT_CACHE_PATH, 
                 objdiff_path: str = DEFAULT_OBJDIFF_PATH):
        self.cache_path = Path(cache_path)
        self.objdiff_path = Path(objdiff_path)
        self.cache = self._load_cache()
    
    def _load_cache(self) -> dict:
        """Load matching cache from disk."""
        if self.cache_path.exists():
            with open(self.cache_path) as f:
                return json.load(f)
        return {
            'version': '1.0.0',
            'last_updated': None,
            'units': {},
            'functions': {}
        }
    
    def save_cache(self):
        """Save cache to disk."""
        self.cache_path.parent.mkdir(parents=True, exist_ok=True)
        self.cache['last_updated'] = datetime.utcnow().isoformat() + 'Z'
        with open(self.cache_path, 'w') as f:
            json.dump(self.cache, f, indent=2)
    
    def _get_unit_config(self, unit_name: str) -> Optional[dict]:
        """Get unit configuration from objdiff.json."""
        objdiff_json = Path(DEFAULT_OBJDIFF_JSON)
        if not objdiff_json.exists():
            return None
        
        with open(objdiff_json) as f:
            config = json.load(f)
        
        for unit in config.get('units', []):
            if unit['name'] == unit_name or unit['name'].endswith(f'/{unit_name}'):
                return unit
        
        return None
    
    def _run_objdiff(self, base_path: str, target_path: str) -> Optional[dict]:
        """Run objdiff CLI and return results."""
        if not self.objdiff_path.exists():
            print(f"Warning: objdiff CLI not found at {self.objdiff_path}")
            return None
        
        # Check if target exists (built object)
        if not Path(target_path).exists():
            return None
        
        # Check if base exists (delinked reference)
        if not Path(base_path).exists():
            return None
        
        cmd = [
            str(self.objdiff_path),
            'diff',
            '--base', base_path,
            '--target', target_path,
            '--format', 'json'
        ]
        
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30
            )
            
            if result.returncode != 0:
                print(f"objdiff error: {result.stderr}")
                return None
            
            return json.loads(result.stdout)
        except subprocess.TimeoutExpired:
            print(f"objdiff timeout for {base_path}")
            return None
        except json.JSONDecodeError as e:
            print(f"Failed to parse objdiff output: {e}")
            return None
        except Exception as e:
            print(f"objdiff failed: {e}")
            return None
    
    def check_unit(self, unit_name: str, force: bool = False) -> Optional[dict]:
        """Check matching percentage for a single unit."""
        # Check cache first
        if not force and unit_name in self.cache['units']:
            cached = self.cache['units'][unit_name]
            # Check if cache is recent (within 24 hours)
            if 'timestamp' in cached:
                cache_time = datetime.fromisoformat(cached['timestamp'].replace('Z', '+00:00'))
                if (datetime.utcnow() - cache_time.replace(tzinfo=None)).total_seconds() < 86400:
                    return cached
        
        # Get unit config
        config = self._get_unit_config(unit_name)
        if not config:
            print(f"Unit {unit_name} not found in objdiff.json")
            return None
        
        # Run objdiff
        result = self._run_objdiff(config['base_path'], config['target_path'])
        if not result:
            return None
        
        # Extract overall match percentage
        match_percent = result.get('overall_match', 0)
        
        # Store in cache
        unit_data = {
            'name': unit_name,
            'match_percent': match_percent,
            'timestamp': datetime.utcnow().isoformat() + 'Z',
            'base_path': config['base_path'],
            'target_path': config['target_path'],
            'functions': result.get('functions', [])
        }
        
        self.cache['units'][unit_name] = unit_data
        
        # Also cache per-function data
        for func in result.get('functions', []):
            func_name = func.get('name', 'unknown')
            self.cache['functions'][f"{unit_name}::{func_name}"] = {
                'unit': unit_name,
                'name': func_name,
                'match_percent': func.get('match', 0),
                'timestamp': unit_data['timestamp']
            }
        
        return unit_data
    
    def check_all_ported(self, report: dict, force: bool = False) -> dict:
        """Check matching for all ported units in report."""
        units = report.get('units', [])
        ported_units = [u for u in units if u['summary']['ported'] > 0]
        
        results = {
            'checked': 0,
            'failed': 0,
            'units': {}
        }
        
        print(f"Checking {len(ported_units)} ported units...")
        
        for i, unit in enumerate(ported_units):
            unit_name = unit['name']
            print(f"[{i+1}/{len(ported_units)}] Checking {unit_name}...", end=' ')
            
            result = self.check_unit(unit_name, force=force)
            if result:
                print(f"{result['match_percent']:.1f}%")
                results['checked'] += 1
                results['units'][unit_name] = result
            else:
                print("FAILED")
                results['failed'] += 1
        
        return results
    
    def get_unit_matching(self, unit_name: str) -> Optional[float]:
        """Get cached matching percentage for a unit."""
        if unit_name in self.cache['units']:
            return self.cache['units'][unit_name]['match_percent']
        return None
    
    def get_function_matching(self, unit_name: str, func_name: str) -> Optional[float]:
        """Get cached matching percentage for a function."""
        key = f"{unit_name}::{func_name}"
        if key in self.cache['functions']:
            return self.cache['functions'][key]['match_percent']
        return None
    
    def get_summary(self) -> dict:
        """Get summary of matching status."""
        units = self.cache.get('units', {})
        
        if not units:
            return {
                'total_units_checked': 0,
                'avg_match_percent': 0,
                'fully_matching': 0,
                'partially_matching': 0
            }
        
        match_percents = [u['match_percent'] for u in units.values()]
        avg_match = sum(match_percents) / len(match_percents)
        
        fully_matching = sum(1 for m in match_percents if m >= 99)
        partially_matching = sum(1 for m in match_percents if 0 < m < 99)
        
        return {
            'total_units_checked': len(units),
            'avg_match_percent': round(avg_match, 2),
            'fully_matching': fully_matching,
            'partially_matching': partially_matching,
            'not_matching': len(units) - fully_matching - partially_matching
        }


def enhance_report_with_matching(report_path: str, cache_path: str = DEFAULT_CACHE_PATH):
    """Add matching data to an existing report."""
    with open(report_path) as f:
        report = json.load(f)
    
    tracker = MatchingTracker(cache_path=cache_path)
    
    # Add matching data to each unit
    for unit in report.get('units', []):
        unit_name = unit['name']
        match_percent = tracker.get_unit_matching(unit_name)
        if match_percent is not None:
            unit['match_percent'] = match_percent
        else:
            unit['match_percent'] = None
    
    # Add summary
    summary = tracker.get_summary()
    report['matching_summary'] = summary
    
    # Write back
    with open(report_path, 'w') as f:
        json.dump(report, f, indent=2)
    
    return report


def main():
    ap = argparse.ArgumentParser(description='Track matching percentages using objdiff')
    ap.add_argument('--cache', default=DEFAULT_CACHE_PATH,
                   help='Path to matching cache file')
    ap.add_argument('--objdiff', default=DEFAULT_OBJDIFF_PATH,
                   help='Path to objdiff CLI')
    ap.add_argument('--unit', metavar='NAME',
                   help='Check specific unit')
    ap.add_argument('--all-ported', metavar='REPORT_JSON',
                   help='Check all ported units from report')
    ap.add_argument('--force', action='store_true',
                   help='Force re-check (ignore cache)')
    ap.add_argument('--enhance-report', metavar='REPORT_JSON',
                   help='Add matching data to existing report')
    ap.add_argument('--show-summary', action='store_true',
                   help='Show matching summary')
    ap.add_argument('--clear-cache', action='store_true',
                   help='Clear the matching cache')
    
    args = ap.parse_args()
    
    tracker = MatchingTracker(cache_path=args.cache, objdiff_path=args.objdiff)
    
    if args.clear_cache:
        tracker.cache = {
            'version': '1.0.0',
            'last_updated': None,
            'units': {},
            'functions': {}
        }
        tracker.save_cache()
        print("✓ Cache cleared")
        return
    
    if args.unit:
        result = tracker.check_unit(args.unit, force=args.force)
        if result:
            print(f"\n{args.unit}: {result['match_percent']:.1f}% matching")
            tracker.save_cache()
        else:
            print(f"\nFailed to check {args.unit}")
    
    if args.all_ported:
        with open(args.all_ported) as f:
            report = json.load(f)
        
        results = tracker.check_all_ported(report, force=args.force)
        tracker.save_cache()
        
        print(f"\n✓ Checked {results['checked']} units ({results['failed']} failed)")
    
    if args.enhance_report:
        enhance_report_with_matching(args.enhance_report, args.cache)
        print(f"✓ Enhanced report with matching data")
    
    if args.show_summary or not any([args.unit, args.all_ported, args.enhance_report, args.clear_cache]):
        summary = tracker.get_summary()
        print("\n=== Matching Summary ===")
        print(f"Units checked: {summary['total_units_checked']}")
        print(f"Average match: {summary['avg_match_percent']:.1f}%")
        print(f"Fully matching (100%): {summary['fully_matching']}")
        print(f"Partially matching: {summary['partially_matching']}")


if __name__ == '__main__':
    main()
