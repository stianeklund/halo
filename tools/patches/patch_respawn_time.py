#!/usr/bin/env python3
"""Patch the minimum respawn time in a Halo CE Xbox XBE.

The engine hardcodes a 3-second (90-tick) minimum respawn floor in
game_engine_player_killed.  This script finds the clamp instruction
by byte signature and patches it to any desired value.

Use at your own risk, make a backup of your xbe first. :P

Usage:
    python3 tools/patches/patch_respawn_time.py <xbe_path> --seconds 0
    python3 tools/patches/patch_respawn_time.py <xbe_path> --seconds 5
    python3 tools/patches/patch_respawn_time.py <xbe_path> --seconds 3   # restore default
    python3 tools/patches/patch_respawn_time.py <xbe_path> --ticks 150   # raw tick value
    python3 tools/patches/patch_respawn_time.py <xbe_path> --dry-run     # show without patching
"""

import argparse
import shutil
import struct
import sys
from pathlib import Path

TICKS_PER_SECOND = 30
DEFAULT_MIN_TICKS = 0x5A  # 90 ticks = 3 seconds

# Instruction sequence for the respawn clamp:
#   83 F8 5A       cmp eax, 0x5a
#   7F 05          jg  +5
#   B8 5A 00 00 00 mov eax, 0x5a
#   89 46 2C       mov [esi+0x2c], eax
#
# We search for the full 13-byte pattern with the default 0x5A value,
# or a relaxed pattern that matches any immediate in the CMP/MOV pair
# (in case the XBE was already patched to a different value).
PATTERN_FIXED = bytes([
    0x83, 0xF8, 0x5A,
    0x7F, 0x05,
    0xB8, 0x5A, 0x00, 0x00, 0x00,
    0x89, 0x46, 0x2C,
])

# Offsets within the pattern
OFF_CMP_IMM = 2    # the immediate byte in CMP EAX, imm8
OFF_JG = 3         # the JG opcode (0x7F = jg, 0x7D = jge)
OFF_MOV_IMM = 6    # start of the 4-byte immediate in MOV EAX, imm32 (after B8 opcode)
OFF_STORE = 10     # MOV [ESI+0x2C], EAX (context anchor)


def find_clamp_sites(data: bytes) -> list[int]:
    """Find all respawn clamp sites by relaxed pattern matching."""
    sites = []

    # First pass: exact default pattern
    offset = 0
    while True:
        idx = data.find(PATTERN_FIXED, offset)
        if idx == -1:
            break
        sites.append(idx)
        offset = idx + 13

    # Second pass: relaxed (for already-patched XBEs)
    if not sites:
        offset = 0
        while True:
            idx = _find_relaxed(data, offset)
            if idx == -1:
                break
            sites.append(idx)
            offset = idx + 13

    return sorted(set(sites))


def _find_relaxed(data: bytes, start: int) -> int:
    """Find the clamp pattern with any immediate value."""
    # Search for the fixed parts: opcode structure + store instruction
    # 83 F8 ?? (7F|7D) 05 B8 ?? 00 00 00 89 46 2C
    anchor = bytes([0x89, 0x46, 0x2C])  # mov [esi+0x2c], eax
    pos = start
    while True:
        idx = data.find(anchor, pos)
        if idx == -1 or idx < 10:
            return -1
        # Check backwards for the CMP/JG/MOV structure
        base = idx - 10
        if (data[base] == 0x83 and
            data[base + 1] == 0xF8 and
            data[base + 3] in (0x7F, 0x7D) and
            data[base + 4] == 0x05 and
            data[base + 5] == 0xB8 and
            data[base + 8] == 0x00 and
            data[base + 9] == 0x00):
            return base
        pos = idx + 1


def format_hex(data: bytes, offset: int, length: int = 13) -> str:
    """Format bytes as hex with address."""
    chunk = data[offset:offset + length]
    return ' '.join(f'{b:02x}' for b in chunk)


def disassemble_site(data: bytes, offset: int) -> str:
    """Human-readable disassembly of the clamp site."""
    cmp_imm = data[offset + OFF_CMP_IMM]
    jmp_op = "jg" if data[offset + OFF_JG] == 0x7F else "jge"
    mov_imm = struct.unpack_from("<I", data, offset + OFF_MOV_IMM)[0]
    ticks = cmp_imm
    secs = ticks / TICKS_PER_SECOND
    lines = [
        f"  cmp  eax, 0x{cmp_imm:02x}        ; {ticks} ticks = {secs:.1f}s",
        f"  {jmp_op}   +5",
        f"  mov  eax, 0x{mov_imm:08x}  ; clamp value",
        f"  mov  [esi+0x2c], eax  ; store respawn_ticks",
    ]
    return '\n'.join(lines)


def patch(data: bytearray, offset: int, new_ticks: int) -> None:
    """Apply the respawn time patch at the given offset."""
    if new_ticks < 0 or new_ticks > 255:
        print(f"ERROR: tick value {new_ticks} out of range for CMP imm8 (0-255)",
              file=sys.stderr)
        sys.exit(1)

    data[offset + OFF_CMP_IMM] = new_ticks
    struct.pack_into("<I", data, offset + OFF_MOV_IMM, new_ticks)

    if new_ticks == 0:
        data[offset + OFF_JG] = 0x7D  # jge: skip when >= 0 (only patch negatives)
    else:
        data[offset + OFF_JG] = 0x7F  # jg: original behavior


def main():
    parser = argparse.ArgumentParser(
        description="Patch the minimum respawn time in a Halo CE Xbox XBE")
    parser.add_argument("xbe", type=Path, help="Path to default.xbe")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--seconds", type=float, help="Minimum respawn time in seconds")
    group.add_argument("--ticks", type=int, help="Minimum respawn time in game ticks (30/sec)")
    parser.add_argument("--dry-run", action="store_true", help="Show patch without writing")
    parser.add_argument("--no-backup", action="store_true", help="Skip creating .bak file")
    args = parser.parse_args()

    if not args.xbe.exists():
        print(f"ERROR: {args.xbe} not found", file=sys.stderr)
        sys.exit(1)

    if args.seconds is not None:
        new_ticks = int(args.seconds * TICKS_PER_SECOND)
    else:
        new_ticks = args.ticks

    if new_ticks < 0 or new_ticks > 255:
        print(f"ERROR: {new_ticks} ticks is out of range (0-255, i.e. 0-8.5 seconds)",
              file=sys.stderr)
        sys.exit(1)

    data = bytearray(args.xbe.read_bytes())
    sites = find_clamp_sites(data)

    if len(sites) == 0:
        print("ERROR: respawn clamp pattern not found in this XBE", file=sys.stderr)
        print("  This may not be a Halo CE Xbox executable, or the code section", file=sys.stderr)
        print("  has been modified in a way that changed the instruction layout.", file=sys.stderr)
        sys.exit(1)

    if len(sites) > 1:
        print(f"WARNING: found {len(sites)} clamp sites (expected 1):", file=sys.stderr)
        for s in sites:
            print(f"  file offset 0x{s:06x}: {format_hex(data, s)}", file=sys.stderr)
        print("  Patching all sites.", file=sys.stderr)

    new_secs = new_ticks / TICKS_PER_SECOND
    print(f"XBE: {args.xbe}")
    print(f"Target: {new_ticks} ticks ({new_secs:.1f} seconds)")
    print()

    for site in sites:
        va_approx = struct.unpack_from("<I", data, 0x104)[0] + site
        print(f"Patch site at file offset 0x{site:06x} (approx VA 0x{va_approx:06x}):")
        print(f"  Before: {format_hex(data, site)}")
        print(disassemble_site(data, site))
        print()

        patch(data, site, new_ticks)

        print(f"  After:  {format_hex(data, site)}")
        print(disassemble_site(data, site))
        print()

    if args.dry_run:
        print("(dry run — no files modified)")
        return

    if not args.no_backup:
        bak = args.xbe.with_suffix(".xbe.bak")
        if not bak.exists():
            shutil.copy2(args.xbe, bak)
            print(f"Backup: {bak}")
        else:
            print(f"Backup already exists: {bak}")

    args.xbe.write_bytes(data)
    print(f"Patched: {args.xbe}")


if __name__ == "__main__":
    main()
