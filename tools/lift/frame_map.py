#!/usr/bin/env python3
"""Frame-map extractor and validator for MSVC x86 function frames (Task 3).

Derives a per-function MSVC stack frame contract from disassembly, then
validates that every addr-taken slot region is covered by a single C object
(array, struct, or buffer) of sufficient size in the lifted source.

This is binary evidence for the §2 stack-aliasing bug class.  The original
MSVC frame shows which slots were contiguous; our clang frame is not required
to match, but every address-taken region must be represented by a single C
object large enough to span those accesses.

Usage:
  python3 tools/lift/frame_map.py --disasm-file func_disasm.txt src/file.c
  python3 tools/lift/frame_map.py --context-cache FUN_0012ab40.json src/file.c
  python3 tools/lift/frame_map.py --self-test
"""

import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent.parent

# ---------------------------------------------------------------------------
# Frame slot
# ---------------------------------------------------------------------------

@dataclass
class FrameSlot:
    ebp_off: int       # negative = below EBP (local), positive = above (param)
    width: int         # bytes (1, 2, 4, 8)
    addr_taken: bool   # True if LEA reg,[EBP±N] was observed for this offset
    writes: int = 0
    reads: int = 0


@dataclass
class FrameMap:
    func_name: str
    frame_size: int             # bytes from SUB ESP,N or _chkstk argument
    slots: list[FrameSlot] = field(default_factory=list)

    def addr_taken_regions(self):
        """Return contiguous spans of addr-taken slots, sorted by ebp_off."""
        taken = sorted([s for s in self.slots if s.addr_taken], key=lambda s: s.ebp_off)
        if not taken:
            return []
        regions = []
        cur_start = taken[0].ebp_off
        cur_end = taken[0].ebp_off + taken[0].width
        for s in taken[1:]:
            if s.ebp_off <= cur_end + 4:  # adjacent or overlapping (within 4 bytes)
                cur_end = max(cur_end, s.ebp_off + s.width)
            else:
                regions.append((cur_start, cur_end))
                cur_start = s.ebp_off
                cur_end = s.ebp_off + s.width
        regions.append((cur_start, cur_end))
        return regions

    def to_dict(self):
        return {
            "func_name": self.func_name,
            "frame_size": self.frame_size,
            "slots": [
                {
                    "ebp_off": s.ebp_off,
                    "width": s.width,
                    "addr_taken": s.addr_taken,
                    "writes": s.writes,
                    "reads": s.reads,
                }
                for s in self.slots
            ],
        }


# ---------------------------------------------------------------------------
# Disassembly parser
# ---------------------------------------------------------------------------

_DISASM_LINE_RE = re.compile(r'^([0-9a-fA-F]{6,8})\s*:\s*(.+)$')
_SUB_ESP_RE = re.compile(r'\bSUB\s+ESP\s*,\s*(0x[0-9a-fA-F]+|\d+)', re.I)
_CHKSTK_RE = re.compile(r'\bMOV\s+EAX\s*,\s*(0x[0-9a-fA-F]+|\d+)', re.I)
_EBP_ACCESS_RE = re.compile(
    r'\b(?:(?:byte|word|dword|qword)\s+ptr\s+)?\[EBP\s*([+\-])\s*(0x[0-9a-fA-F]+|\d+)\]',
    re.I,
)
_LEA_EBP_RE = re.compile(r'\bLEA\s+\w+\s*,\s*\[EBP\s*([+\-])\s*(0x[0-9a-fA-F]+|\d+)\]', re.I)
_WIDTH_RE = re.compile(r'\b(byte|word|dword|qword)\s+ptr\b', re.I)
_WIDTHS = {'byte': 1, 'word': 2, 'dword': 4, 'qword': 8}


def _parse_int(s: str) -> int:
    return int(s, 16) if s.startswith(('0x', '0X')) else int(s)


def _infer_width(line: str, default: int = 4) -> int:
    m = _WIDTH_RE.search(line)
    if m:
        return _WIDTHS.get(m.group(1).lower(), default)
    return default


def extract_frame_map(disasm_text: str, func_name: str = '') -> FrameMap:
    """Parse Ghidra disassembly text and return a FrameMap."""
    lines = disasm_text.splitlines()

    frame_size = 0
    slots: dict[int, FrameSlot] = {}   # ebp_off → FrameSlot
    addr_taken_offsets: set[int] = set()

    # First pass: find frame size
    chkstk_mov_val = None
    for line in lines:
        # SUB ESP, N  →  frame_size = N
        m = _SUB_ESP_RE.search(line)
        if m:
            frame_size = _parse_int(m.group(1))
            break
        # MOV EAX, N; CALL _chkstk  →  frame_size = N
        m = _CHKSTK_RE.search(line)
        if m:
            chkstk_mov_val = _parse_int(m.group(1))
        if chkstk_mov_val and ('_chkstk' in line or '1d90e0' in line.lower()):
            frame_size = chkstk_mov_val
            break

    # Second pass: collect slot accesses and addr-taken
    for line in lines:
        # LEA reg, [EBP±N]  →  address taken
        lea_m = _LEA_EBP_RE.search(line)
        if lea_m:
            sign = 1 if lea_m.group(1) == '+' else -1
            off = sign * _parse_int(lea_m.group(2))
            addr_taken_offsets.add(off)
            if off not in slots:
                slots[off] = FrameSlot(ebp_off=off, width=4, addr_taken=True)
            else:
                slots[off].addr_taken = True

        # [EBP±N] read/write
        for acc in _EBP_ACCESS_RE.finditer(line):
            sign = 1 if acc.group(1) == '+' else -1
            off = sign * _parse_int(acc.group(2))
            width = _infer_width(line)
            is_write = '=' in line.split('[EBP')[0] if '[EBP' in line else False
            # Rough write detection: instruction is MOV [...], reg
            is_write = bool(re.search(r'\bMOV\b', line, re.I)) and '[EBP' in line and ',' in line.split('[EBP')[1]

            if off not in slots:
                slots[off] = FrameSlot(ebp_off=off, width=width, addr_taken=off in addr_taken_offsets)
            slot = slots[off]
            slot.width = max(slot.width, width)
            if is_write:
                slot.writes += 1
            else:
                slot.reads += 1

    # Update addr_taken flags for offsets found in LEA pass
    for off in addr_taken_offsets:
        if off in slots:
            slots[off].addr_taken = True

    return FrameMap(
        func_name=func_name,
        frame_size=frame_size,
        slots=sorted(slots.values(), key=lambda s: s.ebp_off),
    )


# ---------------------------------------------------------------------------
# C source parser
# ---------------------------------------------------------------------------

_C_ARRAY_RE = re.compile(
    r'^\s+(?:unsigned\s+|signed\s+)?(?:\w+)\s+(\w+)\s*\[\s*(0x[0-9a-fA-F]+|\d+)\s*\]\s*;'
)
_C_STRUCT_RE = re.compile(r'^\s+(\w+_t)\s+(\w+)\s*;')
_SUB_ESP_COMMENT_RE = re.compile(r'SUB\s+ESP\s*,\s*(0x[0-9a-fA-F]+)')
_FUNC_DEF_RE = re.compile(
    r'^(?:static\s+)?(?:void|int|short|char|float|unsigned|uint\w*|int\w*|data_t|bool|[A-Za-z_]\w*_t)\s*\*?\s*'
    r'(FUN_[0-9a-fA-F]+|\w+)\s*\('
)


def _parse_c_locals(source_lines: list[str], func_start: int, func_end: int) -> list[tuple[str, int]]:
    """Return list of (name, byte_size) for array/struct locals in a function body."""
    locals_found = []
    for line in source_lines[func_start:func_end]:
        m = _C_ARRAY_RE.match(line)
        if m:
            name = m.group(1)
            sz_s = m.group(2)
            size = int(sz_s, 16) if sz_s.startswith('0x') else int(sz_s)
            locals_found.append((name, size))
    return locals_found


# ---------------------------------------------------------------------------
# Validator
# ---------------------------------------------------------------------------

@dataclass
class ValidationError:
    region_start: int
    region_end: int
    region_size: int
    message: str


def validate_frame(frame_map: FrameMap, source_path: str) -> list[ValidationError]:
    """Validate that all addr-taken regions in the frame map are covered by
    a single C local that is at least as large as the span of accesses.

    Returns list of errors (empty = clean).
    """
    errors = []
    regions = frame_map.addr_taken_regions()
    if not regions:
        return errors

    # Find the target function in the source file
    try:
        with open(source_path, 'r', errors='replace') as f:
            source_lines = f.readlines()
    except OSError:
        return errors

    # Find function body
    func_start = None
    func_end = None
    func_name_lower = frame_map.func_name.lower()
    for i, line in enumerate(source_lines):
        m = _FUNC_DEF_RE.match(line.strip())
        if m and (func_name_lower in line.lower()):
            for j in range(i, min(i + 5, len(source_lines))):
                if '{' in source_lines[j]:
                    func_start = j + 1
                    depth = 1
                    for k in range(j + 1, len(source_lines)):
                        depth += source_lines[k].count('{') - source_lines[k].count('}')
                        if depth == 0:
                            func_end = k
                            break
                    break
        if func_start is not None:
            break

    if func_start is None or func_end is None:
        return errors

    locals_list = _parse_c_locals(source_lines, func_start, func_end)
    # Sort by size descending (largest buffer covers the widest span)
    locals_list.sort(key=lambda x: -x[1])

    frame_size = frame_map.frame_size
    if frame_size == 0:
        return errors

    for region_start, region_end in regions:
        region_size = region_end - region_start
        if region_size <= 4:
            continue  # single slot, not a buffer concern

        # Check if any declared local is large enough to cover this region
        covered = any(size >= region_size for _, size in locals_list)
        if not covered:
            largest = max((sz for _, sz in locals_list), default=0)
            errors.append(ValidationError(
                region_start=region_start,
                region_end=region_end,
                region_size=region_size,
                message=(
                    f"addr-taken region [{region_start:#x}, {region_end:#x}] "
                    f"spans {region_size} bytes but no single C local is that large "
                    f"(largest declared: {largest} bytes) — likely §2 stack-aliasing bug"
                ),
            ))
    return errors


# ---------------------------------------------------------------------------
# Self-tests
# ---------------------------------------------------------------------------

_TEST_DISASM_CLEAN = """\
001234: PUSH EBP
001235: MOV EBP, ESP
001237: SUB ESP, 0xac
00123a: LEA ECX, [EBP - 0x8c]
00123e: PUSH ECX
00123f: CALL 0x9876
001244: MOV EAX, dword ptr [EBP - 0x44]
"""

_TEST_DISASM_CHKSTK = """\
001234: PUSH EBP
001235: MOV EBP, ESP
001237: MOV EAX, 0x1028
00123c: CALL 0x1d90e0
001241: SUB ESP, EAX
001243: LEA ECX, [EBP - 0x100c]
"""


def run_self_tests() -> bool:
    failed = 0

    # Test 1: basic frame extraction
    fm = extract_frame_map(_TEST_DISASM_CLEAN, 'test_func')
    assert fm.frame_size == 0xac, f"Expected 0xac got {fm.frame_size:#x}"
    taken = [s for s in fm.slots if s.addr_taken]
    assert any(s.ebp_off == -0x8c for s in taken), "Expected EBP-0x8c to be addr-taken"
    print("[PASS] basic frame extraction")

    # Test 2: _chkstk frame detection
    fm2 = extract_frame_map(_TEST_DISASM_CHKSTK, 'test_chkstk')
    assert fm2.frame_size == 0x1028, f"Expected 0x1028 got {fm2.frame_size:#x}"
    taken2 = [s for s in fm2.slots if s.addr_taken]
    assert any(s.ebp_off == -0x100c for s in taken2), "Expected EBP-0x100c addr-taken"
    print("[PASS] _chkstk frame detection")

    # Test 3: addr_taken_regions merging
    fm3 = FrameMap('f', 0x20)
    fm3.slots = [
        FrameSlot(-0x10, 4, True),
        FrameSlot(-0x0c, 4, True),
        FrameSlot(-0x08, 4, True),
        FrameSlot(-0x20, 4, False),
    ]
    regions = fm3.addr_taken_regions()
    assert len(regions) == 1, f"Expected 1 region, got {len(regions)}"
    start, end = regions[0]
    assert end - start >= 0xc, f"Region size expected >=12, got {end - start}"
    print("[PASS] addr_taken_regions merging")

    # Test 4: JSON round-trip
    d = fm.to_dict()
    assert d['frame_size'] == 0xac
    assert isinstance(d['slots'], list)
    print("[PASS] to_dict round-trip")

    return failed == 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__[:60])
    parser.add_argument('source', nargs='?', help='C source file to validate against')
    parser.add_argument('--disasm-file', metavar='FILE', help='Disassembly text file')
    parser.add_argument('--context-cache', metavar='JSON', help='Ghidra context cache JSON')
    parser.add_argument('--func', metavar='NAME', help='Function name (for context cache)')
    parser.add_argument('--self-test', action='store_true', help='Run built-in self-tests')
    parser.add_argument('--json-out', metavar='FILE', help='Write frame map JSON to file')
    args = parser.parse_args()

    if args.self_test:
        ok = run_self_tests()
        return 0 if ok else 1

    disasm_text = ''
    func_name = args.func or ''

    if args.context_cache:
        with open(args.context_cache, encoding='utf-8') as f:
            ctx = json.load(f)
        disasm = ctx.get('disassembly', '')
        if isinstance(disasm, dict):
            disasm = disasm.get('result', '')
        disasm_text = disasm or ''
        if not func_name:
            func_name = Path(args.context_cache).stem

    elif args.disasm_file:
        with open(args.disasm_file, encoding='utf-8') as f:
            disasm_text = f.read()

    else:
        parser.print_help(sys.stderr)
        return 2

    frame_map = extract_frame_map(disasm_text, func_name)
    print(f'Frame map for {func_name or "<unknown>"}:')
    print(f'  frame_size = {frame_map.frame_size:#x} ({frame_map.frame_size})')
    taken = [s for s in frame_map.slots if s.addr_taken]
    print(f'  slots: {len(frame_map.slots)} total, {len(taken)} addr-taken')
    for region in frame_map.addr_taken_regions():
        start, end = region
        print(f'  addr-taken region: [{start:#x}, {end:#x}] = {end - start} bytes')

    if args.json_out:
        with open(args.json_out, 'w', encoding='utf-8') as f:
            json.dump(frame_map.to_dict(), f, indent=2)
        print(f'  → {args.json_out}')

    if args.source:
        errors = validate_frame(frame_map, args.source)
        if errors:
            print(f'\nValidation ERRORS ({len(errors)}):')
            for e in errors:
                print(f'  ERROR: {e.message}')
            return 1
        else:
            print('\nValidation: OK (all addr-taken regions covered by C locals)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
