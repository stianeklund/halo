#!/usr/bin/env python3
"""Batch-discover void-declared functions that implicitly return EAX (§16).

MSVC frequently ends functions with MOV EAX,[EBP+8] (return param_1 pointer)
before RET.  Unported callers use that EAX value as a pointer.  When the
ported C body is declared void the caller gets garbage EAX — silent corruption.

Algorithm:
  1. Enumerate all ported functions whose kb.json decl starts with "void ".
  2. For each: check the cached Ghidra context pack for a disassembly tail.
     If no cache entry is available, note it as "needs-ghidra" (no network call).
  3. Scan the last TAIL_LINES of disassembly for the tell-tale patterns:
     a. MOV EAX,[EBP+8]   — returns param_1 (first stack argument)
     b. MOV EAX,[EBP+0xC] — returns param_2 (rarer but also a §16 candidate)
     c. LEA EAX,[EBP-N]   — returns address of a local (needs manual review)
     d. MOV EAX,ECX/EAX/ESI/EDI before RET — may return a register-held value
  4. For confirmed hits: print the address, name, ported status, tail evidence,
     and a recommended fix (change decl to the return type, add return param_1;).

The known entry (0xa9380, game_engine_get_goal_position) serves as a positive
control — it must appear in the output.

Usage:
  rtk python3 tools/audit/find_void_eax_returns.py
  rtk python3 tools/audit/find_void_eax_returns.py --ported-only
  rtk python3 tools/audit/find_void_eax_returns.py --tsv > /tmp/hits.tsv
"""

import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KB_JSON = os.path.join(ROOT, 'kb.json')
CACHE_DIR = os.path.join(ROOT, 'artifacts', 'auto_lift', 'context_cache')

# How many disassembly lines to inspect before RET
TAIL_LINES = 8

# Patterns that indicate the function returns via EAX
_MOV_EAX_PARAM1 = re.compile(r'MOV\s+EAX,dword ptr \[EBP \+ 0x8\]')
_MOV_EAX_PARAM2 = re.compile(r'MOV\s+EAX,dword ptr \[EBP \+ 0xc\]')
_LEA_EAX_LOCAL  = re.compile(r'LEA\s+EAX,\[EBP [+\-]')
_MOV_EAX_REG    = re.compile(r'MOV\s+EAX,(ECX|EBX|ESI|EDI|ESP)')
_RET_INSN       = re.compile(r'\bRET\b')


def _iter_kb_functions():
    """Yield {addr, name, decl, ported} dicts from kb.json."""
    with open(KB_JSON, encoding='utf-8') as f:
        kb = json.load(f)

    def _walk(node):
        if isinstance(node, dict):
            if 'decl' in node and 'addr' in node:
                yield node
            for v in node.values():
                yield from _walk(v)
        elif isinstance(node, list):
            for item in node:
                yield from _walk(item)

    seen = set()
    for fn in _walk(kb):
        addr = fn.get('addr')
        if addr and addr not in seen:
            seen.add(addr)
            yield fn


def _cache_path(addr_str):
    """Return path to a context cache JSON for an address string like '0x1234'."""
    try:
        h = int(addr_str, 16)
    except (ValueError, TypeError):
        return None
    name = f'FUN_{h:08x}.json'
    return os.path.join(CACHE_DIR, name)


def _get_disasm_from_cache(addr_str):
    """Return disassembly string from cache pack, or None if not cached."""
    path = _cache_path(addr_str)
    if not path or not os.path.isfile(path):
        return None
    try:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
        disasm = data.get('disassembly', '')
        # The value may be a nested JSON string ({"result": "..."})
        if isinstance(disasm, dict):
            disasm = disasm.get('result', '')
        return disasm or None
    except (json.JSONDecodeError, OSError):
        return None


_EPILOGUE   = re.compile(r'MOV\s+ESP,EBP|LEAVE\b|POP\s+EBP')
_CALL_INSN  = re.compile(r'\bCALL\b')
_MOV_EAX_ANY = re.compile(r'MOV\s+EAX,')


def _classify_tail(disasm):
    """Return list of pattern labels for EAX-return evidence immediately before RET.

    Only counts a pattern if it appears AFTER the last CALL in the function and
    is within the epilogue window (MOV ESP,EBP / POP EBP ... RET).  This avoids
    false positives where MOV EAX,[EBP+8] is used to set up a CALL argument.
    """
    lines = [l for l in disasm.split('\\n') if l.strip()]
    # Find the last RET
    ret_idx = None
    for i in reversed(range(len(lines))):
        if _RET_INSN.search(lines[i]):
            ret_idx = i
            break
    if ret_idx is None:
        return []

    # Find the last CALL before the RET
    last_call_idx = -1
    for i in range(ret_idx):
        if _CALL_INSN.search(lines[i]):
            last_call_idx = i

    # Find epilogue start (MOV ESP,EBP or LEAVE or POP EBP)
    epi_idx = ret_idx
    for i in reversed(range(max(0, ret_idx - 4), ret_idx)):
        if _EPILOGUE.search(lines[i]):
            epi_idx = i
            break

    # Scan instructions between last CALL and epilogue
    window_start = max(last_call_idx + 1, ret_idx - TAIL_LINES, 0)
    window = lines[window_start:epi_idx]

    _EAX_AS_PTR = re.compile(r'\[EAX')  # EAX used as base for memory access after load

    found = []
    for idx, line in enumerate(window):
        pat = None
        if _MOV_EAX_PARAM1.search(line):
            pat = 'MOV_EAX_PARAM1'
        elif _MOV_EAX_PARAM2.search(line):
            pat = 'MOV_EAX_PARAM2'
        elif _LEA_EAX_LOCAL.search(line):
            pat = 'LEA_EAX_LOCAL'
        elif _MOV_EAX_REG.search(line):
            m = _MOV_EAX_REG.search(line)
            pat = f'MOV_EAX_REG({m.group(1)})'

        if pat:
            # Skip if EAX is subsequently used as a pointer base (store target)
            rest = window[idx + 1:]
            eax_reused = any(_EAX_AS_PTR.search(l) for l in rest)
            if not eax_reused:
                found.append(pat)
    return found


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__[:80])
    parser.add_argument('--ported-only', action='store_true',
                        help='Only report ported=true functions (default: all void decls)')
    parser.add_argument('--tsv', action='store_true',
                        help='TSV output: addr TAB name TAB ported TAB patterns')
    parser.add_argument('--all-void', action='store_true',
                        help='Include functions with no cache entry (marked needs-ghidra)')
    args = parser.parse_args()

    hits = []
    no_cache = []
    checked = 0

    for fn in _iter_kb_functions():
        decl = fn.get('decl', '')
        if not decl.startswith('void '):
            continue
        ported = fn.get('ported', False)
        if args.ported_only and not ported:
            continue

        addr = fn.get('addr', '')
        name = fn.get('name') or f'FUN_{int(addr, 16):08x}' if addr else '?'

        disasm = _get_disasm_from_cache(addr)
        if disasm is None:
            no_cache.append((addr, name, ported))
            continue

        checked += 1
        patterns = _classify_tail(disasm)
        if patterns:
            hits.append((addr, name, ported, patterns))

    # Sort by address
    hits.sort(key=lambda x: int(x[0], 16) if x[0].startswith('0x') else 0)

    if args.tsv:
        print('addr\tname\tported\tpatterns')
        for addr, name, ported, pats in hits:
            print(f'{addr}\t{name}\t{ported}\t{",".join(pats)}')
        return 0

    print(f'[void-EAX scan] Checked {checked} functions with cached context.')
    print(f'  Hits: {len(hits)}')
    print(f'  No-cache (skipped): {len(no_cache)}')
    if args.all_void and no_cache:
        print(f'\n  Functions needing Ghidra check (no cache entry):')
        for addr, name, ported in no_cache:
            print(f'    {addr}  {name}  ported={ported}')
    print()

    if not hits:
        print('No void-EAX return candidates found in cached functions.')
        return 0

    # Positive control check
    known = {h[0] for h in hits}
    control_addr = '0xa9380'
    if control_addr not in known:
        print(f'WARNING: positive control {control_addr} '
              f'(game_engine_get_goal_position) NOT found — cache may be '
              f'absent for this function; run with --all-void to confirm.\n',
              file=sys.stderr)

    for addr, name, ported, pats in hits:
        flag = '✓' if any('PARAM1' in p or 'PARAM2' in p for p in pats) else '?'
        print(f'{flag} {addr}  {name}')
        print(f'     ported={ported}  patterns={",".join(pats)}')
        if 'MOV_EAX_PARAM1' in pats:
            print(f'     FIX: change decl return type from void to int* (or the '
                  f'actual pointer type); add "return param_1;" to C body')
        elif 'MOV_EAX_PARAM2' in pats:
            print(f'     FIX: change decl return type; returns param_2 in EAX')
        elif 'LEA_EAX_LOCAL' in pats:
            print(f'     FIX (manual): returns address of local; check callers '
                  f'to determine the type')
        else:
            print(f'     REVIEW: register-return; verify callers use EAX value')
        print()

    return 0


if __name__ == '__main__':
    sys.exit(main())
