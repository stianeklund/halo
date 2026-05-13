#!/usr/bin/env python3
"""Automated function discovery pipeline for kb.json.

Scans the binary for functions not yet documented in kb.json, filters out
library/compiler functions, classifies them by likely object file, and
optionally adds them to kb.json.

Usage:
    python3 tools/analysis/auto_discover.py --dry-run              # Preview only
    python3 tools/analysis/auto_discover.py --auto-add             # Add to kb.json
    python3 tools/analysis/auto_discover.py --min-xrefs 3          # Only high-usage fns
    python3 tools/analysis/auto_discover.py --from-frontier        # Only frontier fns
    python3 tools/analysis/auto_discover.py --json > discover.json # Machine-readable

Integration:
    # Add to .pre-commit hook to auto-discover before commits:
    python3 tools/analysis/auto_discover.py --auto-add --quiet
"""

import sys
import os
import json
import re
import argparse
import subprocess
from collections import defaultdict
from pathlib import Path
from typing import Optional

# Ensure repo tools are available
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from analysis.knowledge import KnowledgeBase, Function
from analysis.kb_meta import MetadataStore

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../.."))
KB_PATH = os.path.join(ROOT_DIR, 'kb.json')
CACHE_PATH = os.path.join(ROOT_DIR, 'build', 'function_sizes.json')

# Patterns for functions we NEVER want to auto-add
SKIP_PATTERNS = [
    # MSVC / CRT internals
    re.compile(r'^_'),                    # _chkstk, _ftol2, etc.
    re.compile(r'^__'),                   # __SEH_prolog, etc.
    re.compile(r'^@_'),                   # CRT mangled names
    
    # XDK / Xbox SDK
    re.compile(r'^D3D'),
    re.compile(r'^DirectSound'),
    re.compile(r'^IDirectSound'),
    re.compile(r'^Xapi'),
    re.compile(r'^Xe'),
    re.compile(r'^Hal'),
    re.compile(r'^Nt'),
    re.compile(r'^Ke'),
    re.compile(r'^Rtl'),
    re.compile(r'^Ob'),
    re.compile(r'^Mm'),
    re.compile(r'^Io'),
    re.compile(r'^Ps'),
    re.compile(r'^Cc'),
    re.compile(r'^FsRtl'),
    re.compile(r'^Ex'),
    re.compile(r'^Ki'),
    re.compile(r'^Vd'),
    re.compile(r'^SwitchToThread$'),
    re.compile(r'^GetLastError$'),
    re.compile(r'^SetLastError$'),
    re.compile(r'^Interlocked'),
    re.compile(r'^InitializeCriticalSection'),
    re.compile(r'^EnterCriticalSection'),
    re.compile(r'^LeaveCriticalSection'),
    re.compile(r'^DeleteCriticalSection'),
    re.compile(r'^QueryPerformanceCounter'),
    re.compile(r'^Sleep$'),
    re.compile(r'^OutputDebugString'),
    re.compile(r'^DebugBreak$'),
    
    # LIBCMT / standard library
    re.compile(r'^malloc$'),
    re.compile(r'^free$'),
    re.compile(r'^calloc$'),
    re.compile(r'^realloc$'),
    re.compile(r'^memcpy$'),
    re.compile(r'^memmove$'),
    re.compile(r'^memset$'),
    re.compile(r'^memcmp$'),
    re.compile(r'^strcpy$'),
    re.compile(r'^strncpy$'),
    re.compile(r'^strlen$'),
    re.compile(r'^strcmp$'),
    re.compile(r'^strncmp$'),
    re.compile(r'^strcat$'),
    re.compile(r'^strchr$'),
    re.compile(r'^sprintf$'),
    re.compile(r'^snprintf$'),
    re.compile(r'^sscanf$'),
    re.compile(r'^printf$'),
    re.compile(r'^fprintf$'),
    re.compile(r'^fopen$'),
    re.compile(r'^fclose$'),
    re.compile(r'^fread$'),
    re.compile(r'^fwrite$'),
    re.compile(r'^fseek$'),
    re.compile(r'^ftell$'),
    re.compile(r'^fgets$'),
    re.compile(r'^fputs$'),
    re.compile(r'^fflush$'),
    re.compile(r'^feof$'),
    re.compile(r'^ferror$'),
    re.compile(r'^time$'),
    re.compile(r'^rand$'),
    re.compile(r'^srand$'),
    re.compile(r'^qsort$'),
    re.compile(r'^bsearch$'),
    re.compile(r'^abs$'),
    re.compile(r'^labs$'),
    re.compile(r'^fabs$'),
    re.compile(r'^sqrt$'),
    re.compile(r'^sin$'),
    re.compile(r'^cos$'),
    re.compile(r'^tan$'),
    re.compile(r'^atan$'),
    re.compile(r'^atan2$'),
    re.compile(r'^pow$'),
    re.compile(r'^log$'),
    re.compile(r'^exp$'),
    re.compile(r'^floor$'),
    re.compile(r'^ceil$'),
    re.compile(r'^modf$'),
    re.compile(r'^fmod$'),
    re.compile(r'^isalnum$'),
    re.compile(r'^isalpha$'),
    re.compile(r'^isdigit$'),
    re.compile(r'^isspace$'),
    re.compile(r'^isupper$'),
    re.compile(r'^islower$'),
    re.compile(r'^isprint$'),
    re.compile(r'^isxdigit$'),
    re.compile(r'^toupper$'),
    re.compile(r'^tolower$'),
    re.compile(r'^errno$'),
    re.compile(r'^strerror$'),
    re.compile(r'^localtime$'),
    re.compile(r'^gmtime$'),
    re.compile(r'^mktime$'),
    re.compile(r'^time$'),
    re.compile(r'^clock$'),
    re.compile(r'^difftime$'),
    re.compile(r'^atexit$'),
    re.compile(r'^exit$'),
    re.compile(r'^abort$'),
    re.compile(r'^assert$'),
    re.compile(r'^setjmp$'),
    re.compile(r'^longjmp$'),
    re.compile(r'^signal$'),
    re.compile(r'^raise$'),
    re.compile(r'^localeconv$'),
    re.compile(r'^setlocale$'),
    re.compile(r'^wcslen$'),
    re.compile(r'^wcscpy$'),
    re.compile(r'^wcsncpy$'),
    re.compile(r'^wcscmp$'),
    re.compile(r'^wcsncmp$'),
    re.compile(r'^mbstowcs$'),
    re.compile(r'^wcstombs$'),
    re.compile(r'^mblen$'),
    re.compile(r'^mbtowc$'),
    re.compile(r'^wctomb$'),
    re.compile(r'^memcmp$'),
    re.compile(r'^memchr$'),
    re.compile(r'^memmove$'),
    re.compile(r'^strspn$'),
    re.compile(r'^strcspn$'),
    re.compile(r'^strpbrk$'),
    re.compile(r'^strstr$'),
    re.compile(r'^strtok$'),
    re.compile(r'^strerror$'),
    re.compile(r'^strcoll$'),
    re.compile(r'^strxfrm$'),
    re.compile(r'^atof$'),
    re.compile(r'^atoi$'),
    re.compile(r'^atol$'),
    re.compile(r'^strtod$'),
    re.compile(r'^strtol$'),
    re.compile(r'^strtoul$'),
    re.compile(r'^getenv$'),
    re.compile(r'^system$'),
    re.compile(r'^bsearch$'),
    re.compile(r'^qsort$'),
    re.compile(r'^rand$'),
    re.compile(r'^srand$'),
    re.compile(r'^clearerr$'),
    re.compile(r'^fgetc$'),
    re.compile(r'^fgetpos$'),
    re.compile(r'^fgetwc$'),
    re.compile(r'^fgetws$'),
    re.compile(r'^fileno$'),
    re.compile(r'^fputc$'),
    re.compile(r'^fputwc$'),
    re.compile(r'^fputws$'),
    re.compile(r'^fsetpos$'),
    re.compile(r'^ftell$'),
    re.compile(r'^getc$'),
    re.compile(r'^getwc$'),
    re.compile(r'^putc$'),
    re.compile(r'^putwc$'),
    re.compile(r'^remove$'),
    re.compile(r'^rename$'),
    re.compile(r'^rewind$'),
    re.compile(r'^setbuf$'),
    re.compile(r'^setvbuf$'),
    re.compile(r'^tmpfile$'),
    re.compile(r'^tmpnam$'),
    re.compile(r'^ungetc$'),
    re.compile(r'^ungetwc$'),
    re.compile(r'^perror$'),
    re.compile(r'^puts$'),
    re.compile(r'^putchar$'),
    re.compile(r'^getchar$'),
    re.compile(r'^gets$'),
    re.compile(r'^_assert$'),
    re.compile(r'^_controlfp$'),
    re.compile(r'^_control87$'),
    re.compile(r'^_clearfp$'),
    re.compile(r'^_statusfp$'),
    re.compile(r'^_fpreset$'),
    re.compile(r'^_cabs$'),
    re.compile(r'^_hypot$'),
    re.compile(r'^_j0$'),
    re.compile(r'^_j1$'),
    re.compile(r'^_jn$'),
    re.compile(r'^_y0$'),
    re.compile(r'^_y1$'),
    re.compile(r'^_yn$'),
    re.compile(r'^_logb$'),
    re.compile(r'^_nextafter$'),
    re.compile(r'^_scalb$'),
    re.compile(r'^_chgsign$'),
    re.compile(r'^_copysign$'),
    re.compile(r'^_finite$'),
    re.compile(r'^_fpclass$'),
    re.compile(r'^_isnan$'),
    re.compile(r'^_scalbf$'),
    re.compile(r'^_CIacos$'),
    re.compile(r'^_CIasin$'),
    re.compile(r'^_CIatan$'),
    re.compile(r'^_CIatan2$'),
    re.compile(r'^_CIcos$'),
    re.compile(r'^_CIcosh$'),
    re.compile(r'^_CIexp$'),
    re.compile(r'^_CIfmod$'),
    re.compile(r'^_CIlog$'),
    re.compile(r'^_CIlog10$'),
    re.compile(r'^_CIpow$'),
    re.compile(r'^_CIsin$'),
    re.compile(r'^_CIsinh$'),
    re.compile(r'^_CIsqrt$'),
    re.compile(r'^_CItan$'),
    re.compile(r'^_CItanh$'),
    re.compile(r'^_adj_fdiv_m16i$'),
    re.compile(r'^_adj_fdiv_m32$'),
    re.compile(r'^_adj_fdiv_m32i$'),
    re.compile(r'^_adj_fdiv_m64$'),
    re.compile(r'^_adj_fdivr_m16i$'),
    re.compile(r'^_adj_fdivr_m32$'),
    re.compile(r'^_adj_fdivr_m32i$'),
    re.compile(r'^_adj_fdivr_m64$'),
    re.compile(r'^_adj_fpatan$'),
    re.compile(r'^_adj_fprem$'),
    re.compile(r'^_adj_fprem1$'),
    re.compile(r'^_adj_fptan$'),
    re.compile(r'^_safe_fdiv$'),
    re.compile(r'^_safe_fdivr$'),
    re.compile(r'^_libm_sse2_'),
    
    # Compiler-generated / SEH
    re.compile(r'^__except_handler'),
    re.compile(r'^__global_unwind'),
    re.compile(r'^__local_unwind'),
    re.compile(r'^__rt_probe_read'),
    re.compile(r'^__security_check_cookie$'),
    re.compile(r'^__security_init_cookie$'),
    re.compile(r'^__report_gsfailure$'),
    re.compile(r'^__heap_alloc$'),
    re.compile(r'^__nh_malloc$'),
    re.compile(r'^__expand$'),
    re.compile(r'^__msize$'),
    re.compile(r'^__aligned_malloc$'),
    re.compile(r'^__aligned_free$'),
    re.compile(r'^__aligned_realloc$'),
    re.compile(r'^__aligned_msize$'),
    re.compile(r'^__calloc_impl$'),
    re.compile(r'^__recalloc$'),
    re.compile(r'^__freea$'),
    re.compile(r'^__malloca$'),
    re.compile(r'^__resetstkoflw$'),
    re.compile(r'^__except_handler3$'),
    re.compile(r'^__except_handler4$'),
    re.compile(r'^__seh_longjmp$'),
    re.compile(r'^__seh_longjmp_ex$'),
    
    # C++ / STL
    re.compile(r'^\?'),
    re.compile(r'^std::'),
]

# Name prefixes that associate with known object files
NAME_PREFIX_MAP = {
    "object_": "objects.obj",
    "unit_": "units.obj",
    "weapon_": "weapons.obj",
    "item_": "items.obj",
    "vehicle_": "vehicles.obj",
    "actor_": "actors.obj",
    "ai_": "ai.obj",
    "player_": "players.obj",
    "game_engine_": "game_engine.obj",
    "game_time_": "game_time.obj",
    "game_": "game.obj",
    "sound_": "sound_manager.obj",
    "random_": "random_math.obj",
    "matrix_": "real_math.obj",
    "real_math_": "real_math.obj",
    "cross_product": "vector_math.obj",
    "valid_real_normal": "vector_math.obj",
    "rotate_vector": "real_math.obj",
    "bsp3d_": "collision_bsp.obj",
    "cluster_partition_": "structures.obj",
    "decal_": "decals.obj",
    "structure_": "structures.obj",
    "scenario_": "scenario.obj",
    "model_animation_": "model_animations.obj",
    "device_group_": "devices.obj",
    "edit_text_": "edit_text.obj",
    "network_": "network.obj",
    "projectile_": "projectiles.obj",
    "effect_": "effects.obj",
    "damage_": "damage.obj",
    "hs_": "hs.obj",
    "tag_": "tags.obj",
    "collision_": "collision.obj",
    "rasterizer_": "rasterizer.obj",
}


def load_function_cache(path: str) -> dict:
    """Load function size cache from Ghidra export."""
    with open(path) as f:
        return json.load(f)


def load_kb_functions(kb_path: str) -> set[int]:
    """Load all function addresses already in kb.json."""
    with open(kb_path) as f:
        kb = json.load(f)
    
    known_addrs = set()
    for obj in kb.get('objects', []):
        for fn in obj.get('functions', []):
            addr = int(fn['addr'], 16)
            known_addrs.add(addr)
    return known_addrs


def is_library_function(name: str) -> bool:
    """Check if a function name matches known library patterns."""
    for pattern in SKIP_PATTERNS:
        if pattern.match(name):
            return True
    return False


def classify_by_address(addr: int, kb: KnowledgeBase) -> tuple[str, str]:
    """Classify an unknown function by address proximity to known objects.
    
    Returns (object_name, confidence) where confidence is 'high', 'medium', or 'low'.
    """
    # Get sorted list of known function addresses and their objects
    known = []
    for obj_name, symbols in kb.object_to_symbols.items():
        if obj_name == '<common>':
            continue
        for sym in symbols:
            if isinstance(sym, Function) and sym.addr is not None:
                known.append((sym.addr, obj_name))
    
    if not known:
        return '<common>', 'low'
    
    known.sort()
    
    # Find nearest neighbors
    # Binary search for position
    lo, hi = 0, len(known)
    while lo < hi:
        mid = (lo + hi) // 2
        if known[mid][0] < addr:
            lo = mid + 1
        else:
            hi = mid
    
    # Check neighbors
    before = known[lo - 1] if lo > 0 else None
    after = known[lo] if lo < len(known) else None
    
    if before and after:
        gap_before = addr - before[0]
        gap_after = after[0] - addr
        
        # If very close to a known function (< 0x100 bytes), high confidence
        if gap_before < 0x100:
            return before[1], 'high'
        if gap_after < 0x100:
            return after[1], 'high'
        
        # If in the same general region (< 0x1000 bytes), medium confidence
        if gap_before < 0x1000 and gap_after < 0x1000:
            # Assign to the closer one
            if gap_before < gap_after:
                return before[1], 'medium'
            else:
                return after[1], 'medium'
    
    elif before:
        gap = addr - before[0]
        if gap < 0x100:
            return before[1], 'high'
        if gap < 0x1000:
            return before[1], 'medium'
    
    elif after:
        gap = after[0] - addr
        if gap < 0x100:
            return after[1], 'high'
        if gap < 0x1000:
            return after[1], 'medium'
    
    return '<common>', 'low'


def get_called_functions_from_ported_code(kb: KnowledgeBase) -> dict[int, int]:
    """Find all functions called by ported code and count references.
    
    Returns: {addr: call_count}
    """
    import clang.cindex as clang
    
    called = defaultdict(int)
    known_names = set(kb.name_to_addr.keys())
    
    # Find all .c files in src/halo
    src_root = os.path.join(ROOT_DIR, 'src', 'halo')
    for root, _, files in os.walk(src_root):
        for filename in files:
            if not filename.endswith('.c'):
                continue
            path = os.path.join(root, filename)
            
            try:
                with open(path) as f:
                    text = f.read()
            except Exception:
                continue
            
            try:
                index = clang.Index.create()
                tu = index.parse(path, unsaved_files=[(path, text)], options=0)
            except Exception:
                continue
            
            def visit(cursor):
                if cursor.kind == clang.CursorKind.CALL_EXPR:
                    name = cursor.spelling
                    if name in known_names:
                        addr = kb.name_to_addr[name]
                        called[addr] += 1
                for child in cursor.get_children():
                    visit(child)
            
            visit(tu.cursor)
    
    return dict(called)


def discover_functions(cache_path: str, kb_path: str) -> list[dict]:
    """Discover undocumented functions.
    
    Returns list of dicts with keys:
        addr, size, name, proposed_object, confidence
    """
    # Load data
    cache = load_function_cache(cache_path)
    known_addrs = load_kb_functions(kb_path)
    kb = KnowledgeBase.deserialize()
    
    # Get all function addresses from cache
    all_addrs = {}
    for addr_str, finfo in cache.get('functions', {}).items():
        addr = int(addr_str, 16)
        size = finfo['size']
        all_addrs[addr] = size
    
    # Find unknown functions
    unknown = {}
    for addr, size in all_addrs.items():
        if addr not in known_addrs:
            unknown[addr] = size
    
    # Filter library functions and classify
    results = []
    for addr, size in unknown.items():
        name = f"FUN_{addr:08x}"
        
        # Skip library functions by name pattern
        if is_library_function(name):
            continue
        
        # Classify by address
        obj_name, confidence = classify_by_address(addr, kb)
        
        results.append({
            'addr': addr,
            'size': size,
            'name': name,
            'decl': f'void {name}(void);',
            'proposed_object': obj_name,
            'confidence': confidence,
        })
    
    # Sort by address
    results.sort(key=lambda r: r['addr'])
    return results


def apply_to_kb(kb_path: str, discoveries: list[dict], dry_run: bool = True) -> dict:
    """Add discovered functions to kb.json.
    
    Returns statistics about what was added.
    """
    with open(kb_path) as f:
        kb = json.load(f)
    
    # Build lookup of existing objects
    object_map = {}
    for obj in kb['objects']:
        object_map[obj['name']] = obj
    
    stats = {
        'added': 0,
        'skipped': 0,
        'by_object': defaultdict(int),
        'by_confidence': defaultdict(int),
    }
    
    for disc in discoveries:
        obj_name = disc['proposed_object']
        
        # Create object if needed
        if obj_name not in object_map:
            object_map[obj_name] = {
                'name': obj_name,
                'functions': [],
            }
            kb['objects'].append(object_map[obj_name])
        
        obj = object_map[obj_name]
        
        # Check if already exists
        existing_addrs = {int(fn['addr'], 16) for fn in obj.get('functions', [])}
        if disc['addr'] in existing_addrs:
            stats['skipped'] += 1
            continue
        
        # Add function
        obj.setdefault('functions', []).append({
            'decl': disc['decl'],
            'addr': f"0x{disc['addr']:x}",
        })
        
        stats['added'] += 1
        stats['by_object'][obj_name] += 1
        stats['by_confidence'][disc['confidence']] += 1
    
    if not dry_run:
        with open(kb_path, 'w') as f:
            json.dump(kb, f, indent=2)
    
    return stats


def print_report(discoveries: list[dict], stats: Optional[dict] = None, 
                 dry_run: bool = True, json_output: bool = False):
    """Print discovery report."""
    if json_output:
        output = {
            'discovered': len(discoveries),
            'dry_run': dry_run,
            'functions': discoveries,
        }
        if stats:
            output['stats'] = {
                'added': stats['added'],
                'skipped': stats['skipped'],
                'by_object': dict(stats['by_object']),
                'by_confidence': dict(stats['by_confidence']),
            }
        print(json.dumps(output, indent=2))
        return
    
    action = "Would add" if dry_run else "Added"
    
    print(f"\n{'='*60}")
    print(f"  Function Discovery Report")
    print(f"{'='*60}")
    print(f"\n  Discovered: {len(discoveries)} undocumented functions")
    
    if stats:
        print(f"  {action}: {stats['added']}")
        print(f"  Skipped (duplicates): {stats['skipped']}")
    
    if not discoveries:
        print(f"\n  No new functions to add!")
        return
    
    # Group by object
    by_object = defaultdict(list)
    for d in discoveries:
        by_object[d['proposed_object']].append(d)
    
    print(f"\n  By object:")
    for obj_name in sorted(by_object.keys()):
        fns = by_object[obj_name]
        conf_counts = defaultdict(int)
        for f in fns:
            conf_counts[f['confidence']] += 1
        conf_str = ', '.join(f"{c}={n}" for c, n in sorted(conf_counts.items()))
        print(f"    {obj_name:35s} {len(fns):>4d} ({conf_str})")
    
    print(f"\n  Top functions by cross-reference count:")
    sorted_by_xrefs = sorted(discoveries, key=lambda d: -d['xrefs'])[:20]
    for d in sorted_by_xrefs:
        print(f"    0x{d['addr']:06x}  {d['name']:<20s}  xrefs={d['xrefs']:<3d}  "
              f"obj={d['proposed_object']:<25s}  conf={d['confidence']}")
    
    print(f"\n  NOTE: Run with --auto-add to actually modify kb.json")


def main():
    ap = argparse.ArgumentParser(description='Auto-discover undocumented functions')
    ap.add_argument('--cache', default=CACHE_PATH,
                    help='Path to function_sizes.json')
    ap.add_argument('--kb', default=KB_PATH, help='Path to kb.json')
    ap.add_argument('--auto-add', action='store_true',
                    help='Actually modify kb.json (default: dry run)')
    ap.add_argument('--json', action='store_true',
                    help='Output machine-readable JSON')
    ap.add_argument('--quiet', '-q', action='store_true',
                    help='Minimal output')
    args = ap.parse_args()
    
    # If --auto-add is specified, disable dry-run
    dry_run = not args.auto_add
    
    # Validate cache exists
    if not os.path.exists(args.cache):
        if not args.quiet:
            print(f"Error: function size cache not found at {args.cache}", file=sys.stderr)
            print("Run ExportFunctionSizes.java in Ghidra first.", file=sys.stderr)
        sys.exit(1)
    
    # Discover
    discoveries = discover_functions(args.cache, args.kb)
    
    # Apply
    stats = None
    if discoveries:
        stats = apply_to_kb(args.kb, discoveries, dry_run=dry_run)
    
    # Report
    if not args.quiet:
        print_report(discoveries, stats, dry_run=dry_run, json_output=args.json)
    
    # Exit code
    if discoveries and not args.auto_add and not args.json:
        sys.exit(0)  # Normal exit, but nothing was modified


if __name__ == '__main__':
    main()
