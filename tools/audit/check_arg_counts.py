#!/usr/bin/env python3
"""Call-site arg-count audit using stack-cleanup cross-check (§7).

For each CALL in a disassembly, the following ADD ESP,N instruction tells us
how many bytes the caller cleaned up — and therefore how many cdecl stack
arguments the callee received (N/4 for 32-bit).

Algorithm:
  For each CALL instruction:
    1. Scan forward for ADD ESP,N (single cleanup).
    2. Compute cleanup_args = N / 4.
    3. Look up the callee in kb.json; compute declared_stack_args by counting
       cdecl parameters that are NOT @<reg>-annotated.
    4. Skip stdcall callees (they self-clean; no caller ADD ESP follows).
    5. Flag when cleanup_args != declared_stack_args.
    6. Special-case §7: a callee with 0 declared args whose prior pushes are
       "swallowed" by the NEXT CALL's cleanup → "inner 0-arg getter swallowed
       outer's args" warning.

Usage:
  rtk python3 tools/audit/check_arg_counts.py <disasm_text_or_json>
  rtk python3 tools/audit/check_arg_counts.py --json artifacts/auto_lift/context_cache/FUN_XXX.json
  rtk python3 tools/audit/check_arg_counts.py --changed-only   # pre-commit use (reads cached disasm)
  rtk python3 tools/audit/check_arg_counts.py --addr 0x1234    # single function
"""

import json
import os
import re
import subprocess
import sys
from typing import Optional

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KB_JSON = os.path.join(ROOT, 'kb.json')
CACHE_DIR = os.path.join(ROOT, 'artifacts', 'auto_lift', 'context_cache')

_DISASM_LINE_RE = re.compile(r'^\s*([0-9a-fA-F]{8}):\s+([A-Z][A-Z0-9]*)\s*(.*?)\s*$')
_ADD_ESP_RE     = re.compile(r'^ESP\s*,\s*(0x[0-9a-fA-F]+|\d+)$', re.IGNORECASE)
_CALL_TARGET_RE = re.compile(r'(?:dword\s+ptr\s+\[)?(0x[0-9a-fA-F]+)', re.IGNORECASE)
_PUSH_SCAN_STOP = {'CALL', 'RET', 'RETN', 'JMP', 'JA', 'JB', 'JE', 'JNE',
                   'JG', 'JGE', 'JL', 'JLE', 'JZ', 'JNZ', 'POP'}


def _load_kb():
    """Return flat {addr_hex_no0x: {name, decl, ported, reg_args}} dict."""
    with open(KB_JSON, encoding='utf-8') as f:
        kb = json.load(f)

    fns = {}

    def _walk(node):
        if isinstance(node, dict):
            if 'addr' in node and 'decl' in node:
                addr = node['addr']
                h = addr.lstrip('0x') if addr else None
                if h:
                    # Count reg args from decl annotations in kb
                    reg_args = len(re.findall(r'@<\w+>', node.get('decl', '')))
                    fns[h.lower().lstrip('0') or '0'] = {
                        'name': node.get('name', ''),
                        'decl': node.get('decl', ''),
                        'ported': node.get('ported', False),
                        'reg_args': reg_args,
                    }
            for v in node.values():
                _walk(v)
        elif isinstance(node, list):
            for item in node:
                _walk(item)

    _walk(kb)
    return fns


def _is_stdcall(decl: str) -> bool:
    return '__stdcall' in decl or 'WINAPI' in decl or 'RET 0x' in decl.upper()


def _count_declared_stack_args(decl: str, reg_args: int) -> Optional[int]:
    """Return number of expected stack arguments (total params minus reg args).

    Returns None if the decl is too complex to parse reliably.
    """
    # Extract parameter list between outermost parens
    m = re.search(r'\(([^)]*)\)', decl)
    if not m:
        return None
    params_raw = m.group(1).strip()
    if not params_raw or params_raw == 'void':
        return max(0, -reg_args)  # edge case
    total = len([p for p in params_raw.split(',') if p.strip()])
    return max(0, total - reg_args)


def _parse_disasm(text: str):
    """Return list of (addr, mnemonic, operands) from Ghidra disasm text."""
    insns = []
    for line in text.splitlines():
        m = _DISASM_LINE_RE.match(line.strip())
        if m:
            insns.append((m.group(1), m.group(2).upper(), m.group(3).strip()))
    return insns


def audit_call_sites(disasm_text: str, kb: dict, source_name: str = '') -> list:
    """Analyse disassembly for arg-count mismatches.  Returns list of findings."""
    insns = _parse_disasm(disasm_text)
    findings = []

    # Build list of (index, addr, mnemonic, operands) for easy scanning
    indexed = list(enumerate(insns))

    for idx, (addr, mnem, ops) in indexed:
        if mnem != 'CALL':
            continue

        # --- Step 1: find ADD ESP,N after the CALL ---
        cleanup_bytes = None
        for fwd_idx in range(idx + 1, min(idx + 6, len(insns))):
            fa, fm, fo = insns[fwd_idx]
            if fm == 'ADD':
                m = _ADD_ESP_RE.match(fo)
                if m:
                    val = m.group(1)
                    cleanup_bytes = int(val, 16) if val.startswith('0x') else int(val)
                    break
            elif fm in ('CALL', 'RET', 'RETN', 'JMP'):
                break
            # NOP, MOV, LEA etc. are transparent

        if cleanup_bytes is None:
            continue  # no cleanup → either stdcall or no stack args

        cleanup_args = cleanup_bytes // 4

        # --- Step 2: look up callee ---
        callee_m = _CALL_TARGET_RE.match(ops)
        if not callee_m:
            continue  # indirect call; can't look up

        callee_addr_hex = callee_m.group(1).lower().lstrip('0x') or '0'
        callee_info = kb.get(callee_addr_hex) or kb.get(callee_addr_hex.lstrip('0') or '0')
        if not callee_info:
            continue  # unknown callee

        decl = callee_info['decl']
        if _is_stdcall(decl):
            continue  # stdcall self-cleans; our ADD ESP is noise

        reg_args = callee_info['reg_args']
        declared_stack = _count_declared_stack_args(decl, reg_args)
        if declared_stack is None:
            continue

        if cleanup_args == declared_stack:
            continue  # match — OK

        name = callee_info['name'] or f'FUN_{callee_addr_hex}'
        issue = (
            f'CALL 0x{addr} → {name}: '
            f'stack-cleanup={cleanup_args} args, decl={declared_stack} stack args '
            f'(total params={declared_stack + reg_args}, reg_args={reg_args})'
        )

        # --- Step 3: §7 special case — 0-arg getter swallowing outer args ---
        if declared_stack == 0 and reg_args == 0:
            # Count pushes before this CALL
            prior_pushes = 0
            for bwd_idx in range(idx - 1, max(idx - 15, -1), -1):
                bm = insns[bwd_idx][1]
                if bm == 'PUSH':
                    prior_pushes += 1
                elif bm in _PUSH_SCAN_STOP:
                    break

            if prior_pushes > 0:
                issue = (
                    f'§7 LIKELY: CALL 0x{addr} → {name} is a 0-arg getter '
                    f'but {prior_pushes} push(es) precede it — those args likely '
                    f'belong to the NEXT call (cleanup={cleanup_args}); '
                    f'Ghidra mis-grouped them'
                )
                findings.append({'level': 'WARN_HIGH', 'msg': issue, 'call_addr': addr})
                continue

        findings.append({'level': 'WARN', 'msg': issue, 'call_addr': addr})

    return findings


def _load_disasm_from_cache(addr_str: str) -> Optional[str]:
    try:
        h = int(addr_str, 16)
    except (ValueError, TypeError):
        return None
    path = os.path.join(CACHE_DIR, f'FUN_{h:08x}.json')
    if not os.path.isfile(path):
        return None
    with open(path, encoding='utf-8') as f:
        data = json.load(f)
    disasm = data.get('disassembly', '')
    if isinstance(disasm, dict):
        disasm = disasm.get('result', '')
    return disasm or None


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__[:80])
    parser.add_argument('input', nargs='?', help='Disassembly text file or JSON cache pack')
    parser.add_argument('--json', metavar='FILE', help='Ghidra context cache JSON')
    parser.add_argument('--addr', metavar='ADDR', help='Single function address (uses cache)')
    parser.add_argument('--changed-only', action='store_true',
                        help='Audit functions changed in the current git diff (uses cache)')
    parser.add_argument('--quiet', action='store_true',
                        help='Only print findings, no summary header')
    args = parser.parse_args()

    kb = _load_kb()

    tasks = []  # list of (name, disasm_text)

    if args.addr:
        disasm = _load_disasm_from_cache(args.addr)
        if not disasm:
            print(f'No cached context for {args.addr}', file=sys.stderr)
            return 1
        tasks.append((args.addr, disasm))

    elif args.changed_only:
        # Find changed functions via git diff and look up their disasm in cache
        try:
            result = subprocess.run(
                ['git', 'diff', '--name-only', 'HEAD'],
                capture_output=True, text=True, cwd=ROOT,
            )
            changed_files = result.stdout.strip().splitlines()
        except subprocess.SubprocessError:
            changed_files = []

        # Find all FUN_ addresses referenced in changed files
        changed_addrs = set()
        for cf in changed_files:
            fp = os.path.join(ROOT, cf)
            if not cf.endswith('.c') or not os.path.isfile(fp):
                continue
            with open(fp, 'r', errors='replace') as f:
                for m in re.finditer(r'FUN_([0-9a-fA-F]{6,8})', f.read()):
                    changed_addrs.add('0x' + m.group(1).lstrip('0') or '0')

        for addr in sorted(changed_addrs):
            disasm = _load_disasm_from_cache(addr)
            if disasm:
                tasks.append((addr, disasm))

    elif args.json or args.input:
        path = args.json or args.input
        with open(path, encoding='utf-8') as f:
            raw = json.load(f) if path.endswith('.json') else {'disassembly': f.read()}
        disasm = raw.get('disassembly', raw) if isinstance(raw, dict) else raw
        if isinstance(disasm, dict):
            disasm = disasm.get('result', '')
        tasks.append((path, disasm or ''))

    else:
        parser.print_help(sys.stderr)
        return 2

    all_findings = []
    for name, disasm in tasks:
        findings = audit_call_sites(disasm, kb, source_name=name)
        for f in findings:
            f['source'] = name
        all_findings.extend(findings)

    if not args.quiet:
        print(f'[check_arg_counts] {len(tasks)} function(s) audited, '
              f'{len(all_findings)} finding(s)')

    high = [f for f in all_findings if f['level'] == 'WARN_HIGH']
    normal = [f for f in all_findings if f['level'] == 'WARN']

    if high:
        print('\n§7 HIGH-PRIORITY (inner getter swallowed outer args):')
        for f in high:
            print(f"  [{f['source']}] {f['msg']}")

    if normal:
        print('\nArg-count mismatches:')
        for f in normal:
            print(f"  [{f['source']}] {f['msg']}")

    return 1 if all_findings else 0


if __name__ == '__main__':
    sys.exit(main())
