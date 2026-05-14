#!/usr/bin/env python3
"""
Historical progress tracking for decomp.dev-style reporting.

Manages a JSON-based history database with:
- Daily snapshots of progress
- Velocity calculations (functions/day, bytes/day)
- ETA projections
- Trend data for visualization
"""

import json
import os
import argparse
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional
from collections import defaultdict


DEFAULT_HISTORY_PATH = 'artifacts/progress/history.json'


class HistoryManager:
    """Manages historical progress data."""
    
    def __init__(self, history_path: str = DEFAULT_HISTORY_PATH):
        self.history_path = Path(history_path)
        self.data = self._load()
    
    def _load(self) -> dict:
        """Load history from disk or create new."""
        if self.history_path.exists():
            with open(self.history_path) as f:
                return json.load(f)
        return {
            'version': '1.0.0',
            'snapshots': [],
            'trends': {
                'daily': [],
                'weekly': []
            },
            'metadata': {
                'created': datetime.now().astimezone().isoformat(),
                'last_updated': None
            }
        }
    
    def save(self):
        """Save history to disk."""
        self.history_path.parent.mkdir(parents=True, exist_ok=True)
        self.data['metadata']['last_updated'] = datetime.now().astimezone().isoformat()
        with open(self.history_path, 'w') as f:
            json.dump(self.data, f, indent=2)
    
    def add_snapshot(self, report: dict, commit: Optional[str] = None) -> bool:
        """Add a new snapshot from a report. Returns True if added, False if duplicate."""
        if commit is None:
            commit = report.get('meta', {}).get('commit', 'unknown')
        
        # Check for duplicate (same commit within last hour)
        for snap in reversed(self.data['snapshots'][-5:]):
            if snap.get('commit') == commit:
                snap_time = datetime.fromisoformat(snap['timestamp'].replace('Z', '+00:00'))
                if datetime.now().astimezone().replace(tzinfo=None) - snap_time.replace(tzinfo=None) < timedelta(hours=1):
                    return False
        
        snapshot = {
            'timestamp': datetime.now().astimezone().isoformat(),
            'commit': commit,
            'summary': report['summary'],
            'units': [
                {
                    'name': u['name'],
                    'ported': u['summary']['ported'],
                    'total': u['summary']['total'],
                    'percent': u['summary']['percent'],
                    'bytes_ported': u['summary'].get('bytes_ported', 0),
                    'bytes_total': u['summary'].get('bytes_total', 0)
                }
                for u in report.get('units', [])
            ]
        }
        
        self.data['snapshots'].append(snapshot)
        self._calculate_trends()
        return True
    
    def _calculate_trends(self):
        """Calculate velocity and trend data."""
        snapshots = self.data['snapshots']
        if len(snapshots) < 2:
            return
        
        daily = []
        for i in range(1, len(snapshots)):
            curr = snapshots[i]
            prev = snapshots[i-1]
            
            curr_time = datetime.fromisoformat(curr['timestamp'].replace('Z', '+00:00'))
            prev_time = datetime.fromisoformat(prev['timestamp'].replace('Z', '+00:00'))
            days_diff = (curr_time - prev_time).total_seconds() / 86400
            
            if days_diff > 0:
                funcs_gained = curr['summary']['functions']['ported'] - prev['summary']['functions']['ported']
                bytes_gained = curr['summary']['bytes']['ported'] - prev['summary']['bytes']['ported']
                
                daily.append({
                    'date': curr['timestamp'][:10],
                    'functions_gained': funcs_gained,
                    'bytes_gained': bytes_gained,
                    'functions_velocity': round(funcs_gained / days_diff, 2),
                    'bytes_velocity': round(bytes_gained / days_diff, 2)
                })
        
        self.data['trends']['daily'] = daily
        self._calculate_weekly_trends()
    
    def _calculate_weekly_trends(self):
        """Calculate 7-day rolling averages."""
        daily = self.data['trends']['daily']
        if len(daily) < 7:
            return
        
        weekly = []
        for i in range(6, len(daily)):
            window = daily[i-6:i+1]
            avg_funcs = sum(d['functions_gained'] for d in window) / 7
            avg_bytes = sum(d['bytes_gained'] for d in window) / 7
            
            weekly.append({
                'date': daily[i]['date'],
                'functions_velocity_7d': round(avg_funcs, 2),
                'bytes_velocity_7d': round(avg_bytes, 2)
            })
        
        self.data['trends']['weekly'] = weekly
    
    def get_velocity(self, days: int = 7) -> dict:
        """Get average velocity over last N days."""
        snapshots = self.data['snapshots']
        if len(snapshots) < 2:
            return {'functions_per_day': 0, 'bytes_per_day': 0, 'samples': 0}
        
        now = datetime.now().astimezone().replace(tzinfo=None)
        cutoff = now - timedelta(days=days)
        
        recent = []
        for snap in snapshots:
            snap_time = datetime.fromisoformat(snap['timestamp'].replace('Z', '+00:00'))
            if snap_time.replace(tzinfo=None) >= cutoff:
                recent.append(snap)
        
        if len(recent) < 2:
            recent = snapshots[-7:] if len(snapshots) >= 7 else snapshots
        
        if len(recent) < 2:
            return {'functions_per_day': 0, 'bytes_per_day': 0, 'samples': len(recent)}
        
        first = recent[0]
        last = recent[-1]
        
        first_time = datetime.fromisoformat(first['timestamp'].replace('Z', '+00:00'))
        last_time = datetime.fromisoformat(last['timestamp'].replace('Z', '+00:00'))
        days_diff = (last_time - first_time).total_seconds() / 86400
        
        if days_diff < 1:
            days_diff = 1
        
        funcs_diff = last['summary']['functions']['ported'] - first['summary']['functions']['ported']
        bytes_diff = last['summary']['bytes']['ported'] - first['summary']['bytes']['ported']
        
        return {
            'functions_per_day': round(funcs_diff / days_diff, 2),
            'bytes_per_day': round(bytes_diff / days_diff, 2),
            'period_days': round(days_diff, 1),
            'samples': len(recent)
        }
    
    def get_eta(self) -> Optional[dict]:
        """Calculate estimated time to completion."""
        velocity = self.get_velocity(days=14)
        
        if not self.data['snapshots']:
            return None
        
        latest = self.data['snapshots'][-1]
        remaining_funcs = latest['summary']['functions']['total'] - latest['summary']['functions']['ported']
        remaining_bytes = latest['summary']['bytes']['total'] - latest['summary']['bytes']['ported']
        
        funcs_per_day = velocity['functions_per_day']
        
        if funcs_per_day <= 0:
            return {
                'days_to_complete': None,
                'estimated_date': None,
                'remaining_functions': remaining_funcs,
                'remaining_bytes': remaining_bytes,
                'velocity_functions_per_day': funcs_per_day,
                'velocity_bytes_per_day': velocity['bytes_per_day'],
                'confidence': 'low',
                'note': 'No recent progress detected'
            }
        
        days_to_complete = remaining_funcs / funcs_per_day
        estimated_date = datetime.now().astimezone().replace(tzinfo=None) + timedelta(days=days_to_complete)
        
        confidence = 'low'
        if velocity['samples'] >= 7:
            confidence = 'medium'
        if velocity['samples'] >= 14 and days_to_complete < 365:
            confidence = 'high'
        
        return {
            'days_to_complete': round(days_to_complete, 0),
            'estimated_date': estimated_date.strftime('%Y-%m-%d'),
            'remaining_functions': remaining_funcs,
            'remaining_bytes': remaining_bytes,
            'velocity_functions_per_day': funcs_per_day,
            'velocity_bytes_per_day': velocity['bytes_per_day'],
            'confidence': confidence
        }
    
    def get_graph_data(self) -> dict:
        """Generate data for visualization charts."""
        snapshots = self.data['snapshots']
        
        if not snapshots:
            return {'labels': [], 'functions': [], 'bytes': [], 'velocity': []}
        
        recent = snapshots[-90:] if len(snapshots) > 90 else snapshots
        
        labels = []
        functions = []
        bytes_data = []
        
        for snap in recent:
            labels.append(snap['timestamp'][:10])
            functions.append(snap['summary']['functions']['ported'])
            bytes_data.append(snap['summary']['bytes']['ported'])
        
        velocity = []
        for i, snap in enumerate(recent):
            if i == 0:
                velocity.append(0)
            else:
                prev = recent[i-1]
                funcs_gained = snap['summary']['functions']['ported'] - prev['summary']['functions']['ported']
                velocity.append(funcs_gained)
        
        return {
            'labels': labels,
            'functions': functions,
            'bytes': bytes_data,
            'velocity': velocity,
            'total_functions': recent[-1]['summary']['functions']['total'],
            'total_bytes': recent[-1]['summary']['bytes']['total']
        }
    
    def get_summary(self) -> dict:
        """Get a summary of historical progress."""
        snapshots = self.data['snapshots']
        
        if not snapshots:
            return {
                'total_snapshots': 0,
                'first_snapshot': None,
                'latest_snapshot': None
            }
        
        first = snapshots[0]
        latest = snapshots[-1]
        
        return {
            'total_snapshots': len(snapshots),
            'first_snapshot': first['timestamp'],
            'latest_snapshot': latest['timestamp'],
            'velocity_7d': self.get_velocity(days=7),
            'velocity_30d': self.get_velocity(days=30),
            'eta': self.get_eta(),
            'progress_since_start': {
                'functions': latest['summary']['functions']['ported'] - first['summary']['functions']['ported'],
                'bytes': latest['summary']['bytes']['ported'] - first['summary']['bytes']['ported']
            }
        }


def main():
    ap = argparse.ArgumentParser(description='Manage historical progress data')
    ap.add_argument('--history', default=DEFAULT_HISTORY_PATH,
                   help='Path to history.json file')
    ap.add_argument('--add-snapshot', metavar='REPORT_JSON',
                   help='Add snapshot from report JSON')
    ap.add_argument('--commit', help='Git commit hash for snapshot')
    ap.add_argument('--show-summary', action='store_true',
                   help='Show historical summary')
    ap.add_argument('--show-velocity', action='store_true',
                   help='Show velocity metrics')
    ap.add_argument('--show-eta', action='store_true',
                   help='Show ETA to completion')
    ap.add_argument('--show-graph-data', action='store_true',
                   help='Output graph data as JSON')
    ap.add_argument('--prune', type=int, metavar='DAYS',
                   help='Remove snapshots older than N days')
    
    args = ap.parse_args()
    
    manager = HistoryManager(args.history)
    
    if args.add_snapshot:
        with open(args.add_snapshot) as f:
            report = json.load(f)
        
        added = manager.add_snapshot(report, args.commit)
        if added:
            manager.save()
            print(f"✓ Added snapshot for commit {report.get('meta', {}).get('commit', 'unknown')}")
        else:
            print("⚠ Snapshot already exists for this commit (skipped)")
    
    if args.prune:
        cutoff = datetime.now().astimezone().replace(tzinfo=None) - timedelta(days=args.prune)
        original_count = len(manager.data['snapshots'])
        manager.data['snapshots'] = [
            s for s in manager.data['snapshots']
            if datetime.fromisoformat(s['timestamp'].replace('Z', '+00:00')).replace(tzinfo=None) >= cutoff
        ]
        removed = original_count - len(manager.data['snapshots'])
        manager.save()
        print(f"✓ Pruned {removed} old snapshots")
    
    if args.show_summary or not any([args.add_snapshot, args.prune, args.show_velocity, 
                                     args.show_eta, args.show_graph_data]):
        summary = manager.get_summary()
        print("\n=== Historical Progress Summary ===")
        print(f"Total snapshots: {summary['total_snapshots']}")
        if summary['first_snapshot']:
            print(f"First snapshot: {summary['first_snapshot']}")
            print(f"Latest snapshot: {summary['latest_snapshot']}")
            print(f"\nProgress since start:")
            print(f"  Functions: +{summary['progress_since_start']['functions']:,}")
            print(f"  Bytes: +{summary['progress_since_start']['bytes']:,}")
    
    if args.show_velocity:
        v7 = manager.get_velocity(7)
        v30 = manager.get_velocity(30)
        print("\n=== Velocity Metrics ===")
        print(f"Last 7 days: {v7['functions_per_day']:.2f} funcs/day ({v7['samples']} samples)")
        print(f"Last 30 days: {v30['functions_per_day']:.2f} funcs/day ({v30['samples']} samples)")
    
    if args.show_eta:
        eta = manager.get_eta()
        if eta:
            print("\n=== ETA to Completion ===")
            if eta['estimated_date']:
                print(f"Estimated completion: {eta['estimated_date']}")
                print(f"Days remaining: {eta['days_to_complete']}")
                print(f"Confidence: {eta['confidence']}")
            else:
                print(f"Note: {eta['note']}")
            print(f"Velocity: {eta['velocity_functions_per_day']:.2f} funcs/day")
            print(f"Remaining: {eta['remaining_functions']:,} functions")
    
    if args.show_graph_data:
        data = manager.get_graph_data()
        print(json.dumps(data, indent=2))


if __name__ == '__main__':
    main()
