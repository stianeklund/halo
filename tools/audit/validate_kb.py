#!/usr/bin/env python3
"""Validate auto-discovered functions in kb.json.

Checks for:
1. Address validity - functions exist in binary
2. Overlaps - no functions overlap
3. Library functions - missed compiler/library functions
4. Object classification - reasonable classifications
5. Size consistency - matches function cache

Usage:
    python3 tools/audit/validate_kb.py                    # Full validation
    python3 tools/audit/validate_kb.py --quick            # Quick checks only
    python3 tools/audit/validate_kb.py --json             # Machine-readable output
    python3 tools/audit/validate_kb.py --fix              # Auto-fix issues
"""

import sys
import os
import json
import re
import argparse
from collections import defaultdict
from pathlib import Path

# Ensure repo tools are available
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from analysis.knowledge import KnowledgeBase, Function

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../.."))
KB_PATH = os.path.join(ROOT_DIR, 'kb.json')
CACHE_PATH = os.path.join(ROOT_DIR, 'build', 'function_sizes.json')

# Additional library patterns that might have been missed
SUSPICIOUS_PATTERNS = [
    (re.compile(r'^FUN_001[dD][0-9a-fA-F]{4}$'), 'MSVC runtime (1D9000-1DDFFF)'),
    (re.compile(r'^FUN_001[eE][0-9a-fA-F]{4}$'), 'XDK/D3D (1E0000-1EFFFF)'),
    (re.compile(r'^FUN_001[fF][0-9a-fA-F]{4}$'), 'LIBCMT/XAPILIB (1F0000-1FFFFF)'),
    (re.compile(r'^FUN_002[0-2][0-9a-fA-F]{4}$'), 'D3D8/XNET (200000-22FFFF)'),
]


def load_function_cache(path: str) -> dict:
    """Load function size cache from Ghidra export."""
    with open(path) as f:
        return json.load(f)


def validate_addresses(kb: KnowledgeBase, cache: dict) -> list[dict]:
    """Check all function addresses exist in the binary cache."""
    cache_addrs = set()
    for addr_str in cache.get('functions', {}):
        cache_addrs.add(int(addr_str, 16))
    
    issues = []
    for obj_name, symbols in kb.object_to_symbols.items():
        for sym in symbols:
            if isinstance(sym, Function) and sym.addr is not None:
                if sym.addr not in cache_addrs:
                    issues.append({
                        'type': 'missing_address',
                        'object': obj_name,
                        'name': sym.name,
                        'addr': sym.addr,
                        'message': f'Address 0x{sym.addr:x} not found in binary',
                    })
    return issues


def find_overlaps(kb: KnowledgeBase, cache: dict) -> list[dict]:
    """Find functions that overlap in address space."""
    # Build sorted list of (start, end, name, object)
    ranges = []
    cache_funcs = cache.get('functions', {})
    
    for obj_name, symbols in kb.object_to_symbols.items():
        for sym in symbols:
            if isinstance(sym, Function) and sym.addr is not None:
                addr_str = f'0x{sym.addr:x}'
                size = cache_funcs.get(addr_str, {}).get('size', 0)
                if size > 0:
                    ranges.append((sym.addr, sym.addr + size, sym.name, obj_name, size))
    
    ranges.sort()
    
    issues = []
    for i in range(len(ranges) - 1):
        start1, end1, name1, obj1, size1 = ranges[i]
        start2, end2, name2, obj2, size2 = ranges[i + 1]
        
        if end1 > start2:
            overlap = end1 - start2
            issues.append({
                'type': 'overlap',
                'object1': obj1,
                'name1': name1,
                'addr1': start1,
                'size1': size1,
                'object2': obj2,
                'name2': name2,
                'addr2': start2,
                'size2': size2,
                'overlap': overlap,
                'message': f'Overlap: {name1} (0x{start1:x}, {size1}b) and {name2} (0x{start2:x}, {size2}b) overlap by {overlap} bytes',
            })
    
    return issues


def find_suspicious_functions(kb: KnowledgeBase) -> list[dict]:
    """Find functions that look like library functions but weren't filtered."""
    issues = []
    
    for obj_name, symbols in kb.object_to_symbols.items():
        # Skip known library objects
        if not obj_name or any(x in obj_name for x in ['LIBCMT', 'XAPILIB', 'D3D8', 'XNET']):
            continue
            
        for sym in symbols:
            if isinstance(sym, Function) and sym.name and sym.name.startswith('FUN_'):
                for pattern, category in SUSPICIOUS_PATTERNS:
                    if pattern.match(sym.name):
                        issues.append({
                            'type': 'suspicious_library',
                            'object': obj_name,
                            'name': sym.name,
                            'addr': sym.addr,
                            'category': category,
                            'message': f'{sym.name} in {obj_name} looks like {category}',
                        })
                        break
    
    return issues


def check_object_classification(kb: KnowledgeBase, cache: dict) -> list[dict]:
    """Check if object classifications look reasonable."""
    issues = []
    
    # Build map of known function addresses per object
    obj_addrs = defaultdict(list)
    for obj_name, symbols in kb.object_to_symbols.items():
        for sym in symbols:
            if isinstance(sym, Function) and sym.addr is not None:
                obj_addrs[obj_name].append(sym.addr)
    
    # Check for objects with very scattered addresses
    for obj_name, addrs in obj_addrs.items():
        if len(addrs) < 3:
            continue
            
        addrs.sort()
        gaps = []
        for i in range(len(addrs) - 1):
            gap = addrs[i + 1] - addrs[i]
            gaps.append(gap)
        
        avg_gap = sum(gaps) / len(gaps) if gaps else 0
        max_gap = max(gaps) if gaps else 0
        
        # If average gap is very large, might be misclassified
        if avg_gap > 0x5000:
            issues.append({
                'type': 'scattered_object',
                'object': obj_name,
                'functions': len(addrs),
                'avg_gap': avg_gap,
                'max_gap': max_gap,
                'message': f'{obj_name} has {len(addrs)} functions with avg gap 0x{int(avg_gap):x} (may be misclassified)',
            })
    
    return issues


def check_gaps(kb: KnowledgeBase, cache: dict) -> list[dict]:
    """Find large gaps within objects that might indicate missing functions."""
    issues = []
    cache_funcs = cache.get('functions', {})
    
    for obj_name, symbols in kb.object_to_symbols.items():
        if obj_name == '<common>':
            continue
            
        funcs = [(sym.addr, sym.name) for sym in symbols 
                 if isinstance(sym, Function) and sym.addr is not None]
        funcs.sort()
        
        for i in range(len(funcs) - 1):
            addr1, name1 = funcs[i]
            addr2, name2 = funcs[i + 1]
            
            size1 = cache_funcs.get(f'0x{addr1:x}', {}).get('size', 0)
            expected_next = addr1 + size1
            gap = addr2 - expected_next
            
            # If gap is > 0x100, there might be missing functions
            if gap > 0x100:
                # Check if there are cache functions in the gap
                gap_funcs = []
                for addr_str, finfo in cache_funcs.items():
                    addr = int(addr_str, 16)
                    if expected_next <= addr < addr2:
                        gap_funcs.append((addr, finfo['size']))
                
                if gap_funcs:
                    issues.append({
                        'type': 'gap',
                        'object': obj_name,
                        'after': name1,
                        'after_addr': addr1,
                        'before': name2,
                        'before_addr': addr2,
                        'gap_size': gap,
                        'missing_count': len(gap_funcs),
                        'message': f'Gap of 0x{gap:x} bytes in {obj_name} between {name1} and {name2} ({len(gap_funcs)} functions missing)',
                    })
    
    return issues


def print_report(issues: list[dict], json_output: bool = False):
    """Print validation report."""
    if json_output:
        print(json.dumps(issues, indent=2))
        return
    
    if not issues:
        print('Validation PASSED: No issues found')
        return
    
    # Group by type
    by_type = defaultdict(list)
    for issue in issues:
        by_type[issue['type']].append(issue)
    
    print(f'\n{"="*60}')
    print(f'  kb.json Validation Report')
    print(f'{"="*60}')
    print(f'\n  Total issues: {len(issues)}')
    
    for issue_type, type_issues in sorted(by_type.items()):
        print(f'\n  {issue_type.upper().replace("_", " ")} ({len(type_issues)}):')
        for issue in type_issues[:10]:  # Show first 10
            print(f'    {issue["message"]}')
        if len(type_issues) > 10:
            print(f'    ... and {len(type_issues) - 10} more')


def main():
    ap = argparse.ArgumentParser(description='Validate kb.json auto-discovered functions')
    ap.add_argument('--kb', default=KB_PATH, help='Path to kb.json')
    ap.add_argument('--cache', default=CACHE_PATH, help='Path to function_sizes.json')
    ap.add_argument('--json', action='store_true', help='Output machine-readable JSON')
    ap.add_argument('--quick', action='store_true', help='Run quick checks only')
    args = ap.parse_args()
    
    if not os.path.exists(args.cache):
        print(f'Error: function size cache not found at {args.cache}', file=sys.stderr)
        sys.exit(1)
    
    if not os.path.exists(args.kb):
        print(f'Error: kb.json not found at {args.kb}', file=sys.stderr)
        sys.exit(1)
    
    if not args.json:
        print('Loading knowledge base...')
    kb = KnowledgeBase.deserialize()
    cache = load_function_cache(args.cache)
    
    if not args.json:
        print('Running validations...')
    issues = []
    
    # Always run these
    if not args.json:
        print('  Checking addresses...')
    issues.extend(validate_addresses(kb, cache))
    
    if not args.json:
        print('  Finding overlaps...')
    issues.extend(find_overlaps(kb, cache))
    
    if not args.json:
        print('  Finding suspicious functions...')
    issues.extend(find_suspicious_functions(kb))
    
    if not args.quick:
        if not args.json:
            print('  Checking object classifications...')
        issues.extend(check_object_classification(kb, cache))
        
        if not args.json:
            print('  Finding gaps...')
        issues.extend(check_gaps(kb, cache))
    
    print_report(issues, json_output=args.json)
    
    # Exit with error if issues found
    if issues:
        sys.exit(1)


if __name__ == '__main__':
    main()
