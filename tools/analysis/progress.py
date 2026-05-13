#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

if __name__ == "__main__":
    from audit.check_requirements import check_requirements
    check_requirements()

import argparse
import json
import os
import sys
from collections import defaultdict

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../.."))
KB_PATH = os.path.join(ROOT_DIR, 'kb.json')
KB_META_PATH = os.path.join(ROOT_DIR, 'kb_meta.json')
DEFAULT_CACHE = os.path.join(ROOT_DIR, 'build', 'function_sizes.json')


def bar(pct, width=40):
    filled = int(width * pct / 100)
    return '[' + '#' * filled + '-' * (width - filled) + ']'


def fmt_bytes(n):
    if n >= 1024 * 1024:
        return f'{n / 1024 / 1024:.1f} MB'
    if n >= 1024:
        return f'{n / 1024:.1f} KB'
    return f'{n:,} bytes'


def load_function_cache(path):
    with open(path) as f:
        return json.load(f)


def load_meta(meta_path):
    with open(meta_path) as f:
        meta = json.load(f)
    result = {}
    for addr_str, info in meta.get('symbols', {}).items():
        if info.get('kind') == 'function':
            addr = int(addr_str, 16)
            result[addr] = {
                'status': info.get('status', 'unknown'),
                'name': info.get('name', ''),
            }
    return result


def load_kb_objects(kb_path):
    with open(kb_path) as f:
        kb = json.load(f)
    obj_map = {}
    for obj in kb.get('objects', []):
        name = obj.get('name', '')
        source = obj.get('source', '')
        funcs = {}
        for fn in obj.get('functions', []):
            addr = int(fn['addr'], 16)
            funcs[addr] = fn.get('decl', '')
        obj_map[name] = {
            'source': source,
            'functions': funcs,
        }
    return obj_map


def compute(cache_path, kb_path, meta_path):
    cache = load_function_cache(cache_path)
    meta_addrs = load_meta(meta_path)
    obj_map = load_kb_objects(kb_path)

    text_size = cache['text_section_size']
    total_funcs = cache['total_functions']
    total_func_bytes = cache['total_function_bytes']
    functions = cache['functions']

    addr_to_obj = {}
    for obj_name, obj_info in obj_map.items():
        for addr in obj_info['functions']:
            addr_to_obj[addr] = obj_name

    ported_count = 0
    ported_bytes = 0
    declared_count = 0
    declared_bytes = 0
    ported_not_found = []
    obj_stats = defaultdict(lambda: {
        'ported_funcs': 0, 'ported_bytes': 0,
        'declared_funcs': 0, 'declared_bytes': 0,
    })

    for addr_str, finfo in functions.items():
        addr = int(addr_str, 16)
        size = finfo['size']
        obj_name = addr_to_obj.get(addr, '')

        if obj_name:
            declared_count += 1
            declared_bytes += size
            obj_stats[obj_name]['declared_funcs'] += 1
            obj_stats[obj_name]['declared_bytes'] += size

        if addr in meta_addrs:
            s = meta_addrs[addr]['status']
            if s in ('ported', 'verified'):
                ported_count += 1
                ported_bytes += size
                if obj_name:
                    obj_stats[obj_name]['ported_funcs'] += 1
                    obj_stats[obj_name]['ported_bytes'] += size

    for addr, info in meta_addrs.items():
        if info['status'] in ('ported', 'verified') and str(hex(addr)) not in functions:
            ported_not_found.append(addr)

    return {
        'text_size': text_size,
        'total_func_bytes': total_func_bytes,
        'total_funcs': total_funcs,
        'ported_count': ported_count,
        'ported_bytes': ported_bytes,
        'declared_count': declared_count,
        'declared_bytes': declared_bytes,
        'ported_not_found': ported_not_found,
        'obj_stats': dict(obj_stats),
        'obj_map': obj_map,
        'meta_addrs': meta_addrs,
    }


def print_report(s, by_object=False):
    text_size = s['text_size']
    total_funcs = s['total_funcs']
    total_func_bytes = s['total_func_bytes']
    ported_count = s['ported_count']
    ported_bytes = s['ported_bytes']
    declared_count = s['declared_count']
    declared_bytes = s['declared_bytes']
    ported_not_found = s['ported_not_found']
    obj_stats = s['obj_stats']
    obj_map = s['obj_map']
    meta_addrs = s['meta_addrs']

    print('Halo: Combat Evolved — Reimplementation Progress')
    print()
    print(f'  .text section:        {fmt_bytes(text_size):>12s}')
    print(f'  Total functions:      {total_funcs:>12,}')
    print(f'  Total function bytes: {fmt_bytes(total_func_bytes):>12s}  ({total_func_bytes * 100 / text_size:.1f}% of .text)')
    print()
    print(f'  Byte coverage:  {bar(ported_bytes * 100 / text_size)} {ported_bytes * 100 / text_size:.2f}%')
    print(f'    {fmt_bytes(ported_bytes)} ported / {fmt_bytes(text_size)} total')
    print()
    print(f'  Function count: {bar(ported_count * 100 / total_funcs)} {ported_count * 100 / total_funcs:.1f}%')
    print(f'    {ported_count:,} ported / {total_funcs:,} total')
    print()
    print(f'  Declared in kb.json:  {declared_count:>12,}  ({declared_count * 100 / total_funcs:.1f}% of total)')
    print(f'  Declared bytes:       {fmt_bytes(declared_bytes):>12s}')
    if declared_count:
        print(f'  Ported in kb_meta:    {ported_count:>12,}  ({ported_count * 100 / declared_count:.1f}% of declared)')
    print()

    if ported_not_found:
        print(f'  WARNING: {len(ported_not_found)} ported addresses not found in cache:')
        for a in sorted(ported_not_found)[:10]:
            name = meta_addrs.get(a, {}).get('name', '?')
            print(f'    0x{a:x} ({name})')
        if len(ported_not_found) > 10:
            print(f'    ... and {len(ported_not_found) - 10} more')
        print()

    if by_object:
        print('Per-object breakdown:')
        print(f'  {"object":<35s} {"funcs":>7s} {"bytes":>10s} {"pct":>7s}')
        print(f'  {"-" * 35} {"-" * 7} {"-" * 10} {"-" * 7}')
        sorted_objs = sorted(obj_stats.items(),
                             key=lambda x: x[1]['ported_bytes'],
                             reverse=True)
        for obj_name, stats in sorted_objs:
            if stats['declared_funcs'] == 0:
                continue
            pct = stats['ported_bytes'] * 100 / text_size
            declared_in_obj = len(obj_map.get(obj_name, {}).get('functions', {}))
            print(f'  {obj_name:<35s} {stats["ported_funcs"]:>3d}/{declared_in_obj:<3d}'
                  f' {fmt_bytes(stats["ported_bytes"]):>10s} {pct:>6.2f}%')
        print()


def find_cache_file(cache_path):
    """Find the function size cache file, checking multiple locations."""
    # Check the specified/default path first
    if os.path.exists(cache_path):
        return cache_path
    
    # Check alternative locations
    alternatives = [
        os.path.join(ROOT_DIR, 'function_sizes.json'),
        os.path.join(os.getcwd(), 'function_sizes.json'),
        os.path.join(os.getcwd(), 'build', 'function_sizes.json'),
    ]
    
    for alt in alternatives:
        if os.path.exists(alt):
            return alt
    
    return None


def main():
    ap = argparse.ArgumentParser(description='Report reimplementation progress')
    ap.add_argument('--cache', default=DEFAULT_CACHE,
                    help='Path to function_sizes.json cache from Ghidra')
    ap.add_argument('--kb', default=KB_PATH)
    ap.add_argument('--meta', default=KB_META_PATH)
    ap.add_argument('--by-object', action='store_true',
                    help='Show per-object breakdown')
    ap.add_argument('--json', action='store_true',
                    help='Output machine-readable JSON only')
    args = ap.parse_args()

    # Ensure build directory exists (for when Ghidra script runs)
    build_dir = os.path.dirname(DEFAULT_CACHE)
    if not os.path.exists(build_dir):
        os.makedirs(build_dir, exist_ok=True)

    # Try to find the cache file
    cache_path = find_cache_file(args.cache)
    
    if cache_path is None:
        print(f'Error: function size cache not found at {args.cache}', file=sys.stderr)
        print(f'', file=sys.stderr)
        print(f'To generate this file:', file=sys.stderr)
        print(f'  1. Open the XBE in Ghidra', file=sys.stderr)
        print(f'  2. Go to Window -> Script Manager', file=sys.stderr)
        print(f'  3. Click the script directories icon (+) and add:', file=sys.stderr)
        print(f'     {os.path.join(ROOT_DIR, "ghidra_scripts")}', file=sys.stderr)
        print(f'  4. Run: ExportFunctionSizes.java', file=sys.stderr)
        print(f'  5. The file will be saved to: {DEFAULT_CACHE}', file=sys.stderr)
        print(f'', file=sys.stderr)
        print(f'Or specify a custom path with --cache', file=sys.stderr)
        sys.exit(1)

    s = compute(cache_path, args.kb, args.meta)

    if args.json:
        print(json.dumps({
            'text_size': s['text_size'],
            'ported_bytes': s['ported_bytes'],
            'ported_count': s['ported_count'],
            'total_functions': s['total_funcs'],
            'declared_count': s['declared_count'],
            'byte_coverage_pct': round(s['ported_bytes'] * 100 / s['text_size'], 2),
            'function_coverage_pct': round(s['ported_count'] * 100 / s['total_funcs'], 2),
        }, indent=2))
    else:
        print_report(s, by_object=args.by_object)
        
        # Suggest auto-discovery if many functions are undeclared
        undeclared = s['total_funcs'] - s['declared_count']
        if undeclared > 1000:
            print()
            print(f'Note: {undeclared:,} functions are not yet in kb.json')
            print(f'Run the following to auto-discover and add them:')
            print(f'  python3 tools/analysis/auto_discover.py --auto-add')


if __name__ == '__main__':
    main()
