#!/usr/bin/env python3
"""Decompress Halo CE Xbox map files and optionally create z:\\ cache-slot files.

Halo CE Xbox maps are stored compressed on disc:
  [0x000, 0x800)  — 2 KiB header (uncompressed, contains 'head'/'foot')
  [0x800, EOF)    — zlib-compressed data stream (starts with 78 9c)

The game decompresses maps from d:\\ (DVD) to z:\\cache%03d.map (HDD) on first
load.  Each cache slot has a fixed capacity:
  Slots 0-1: 0x11600000 (276 MB, campaign)
  Slot   2 : 0x02300000 ( 35 MB, small MP)
  Slots 3-5: 0x02F00000 ( 47 MB, large MP)

During init, the game opens each z:\\cache*.map with OPEN_ALWAYS.  If the file
already exists AND its size matches the slot capacity, the header is read and
validated.  A valid pre-populated cache file skips decompression entirely.

Usage:
  # Decompress all maps to a directory (raw: header + decompressed data)
  python3 tools/xbox/decompress_maps.py --src halo-patched/maps --dst /tmp/maps

  # Create cache-slot files for specific maps, ready for z:\\ deployment
  python3 tools/xbox/decompress_maps.py --src halo-patched/maps --dst /tmp/cache \\
      --cache-slots wizard:3 bloodgulch:4

  # Dry-run: show sizes and slot assignments without writing
  python3 tools/xbox/decompress_maps.py --src halo-patched/maps --dry-run
"""

import argparse
import os
import struct
import sys
import zlib

HEADER_SIZE = 0x800
SLOT_SIZES = {
    0: 0x11600000,  # 289,406,976
    1: 0x11600000,
    2: 0x02300000,  #  36,700,160
    3: 0x02F00000,  #  49,283,072
    4: 0x02F00000,
    5: 0x02F00000,
}

HEAD_TAG = b'daeh'  # 'head' LE
FOOT_TAG = b'toof'  # 'foot' LE
ZLIB_MAGIC = b'\x78\x9c'
BUILD_STRING = b'01.10.12.2276'


def read_header(data):
    if len(data) < HEADER_SIZE:
        return None
    hdr = data[:HEADER_SIZE]
    if hdr[0:4] != HEAD_TAG:
        return None
    if hdr[0x7FC:0x800] != FOOT_TAG:
        return None
    version = struct.unpack_from('<I', hdr, 4)[0]
    file_size = struct.unpack_from('<I', hdr, 8)[0]
    name = hdr[0x20:0x40].split(b'\x00')[0].decode('ascii', errors='replace')
    build = hdr[0x40:0x60].split(b'\x00')[0].decode('ascii', errors='replace')
    return {
        'version': version,
        'file_size': file_size,
        'name': name,
        'build': build,
        'header_bytes': hdr,
    }


def decompress_map(path):
    with open(path, 'rb') as f:
        data = f.read()

    info = read_header(data)
    if info is None:
        return None, "invalid header (no head/foot tag)"

    if info['version'] != 5:
        return None, f"unexpected version {info['version']}"

    compressed = data[HEADER_SIZE:]
    if len(compressed) < 2:
        return None, "no data after header"

    if compressed[:2] != ZLIB_MAGIC:
        info['decompressed'] = compressed
        info['was_compressed'] = False
        return info, None

    try:
        raw = zlib.decompress(compressed)
    except zlib.error as e:
        return None, f"zlib error: {e}"

    expected = info['file_size'] - HEADER_SIZE
    if len(raw) != expected:
        return None, (f"size mismatch: got {len(raw)}, "
                      f"expected {expected} (header says {info['file_size']})")

    info['decompressed'] = raw
    info['was_compressed'] = True
    return info, None


def write_raw(info, dst_path):
    with open(dst_path, 'wb') as f:
        f.write(info['header_bytes'])
        f.write(info['decompressed'])
    return os.path.getsize(dst_path)


def write_cache_slot(info, dst_path, slot):
    capacity = SLOT_SIZES[slot]
    total = HEADER_SIZE + len(info['decompressed'])
    if total > capacity:
        return None, (f"decompressed size {total} exceeds "
                      f"slot {slot} capacity {capacity}")

    with open(dst_path, 'wb') as f:
        f.write(info['header_bytes'])
        f.write(info['decompressed'])
        padding = capacity - total
        if padding > 0:
            chunk = b'\x00' * min(padding, 1024 * 1024)
            written = 0
            while written < padding:
                n = min(len(chunk), padding - written)
                f.write(chunk[:n])
                written += n

    return os.path.getsize(dst_path), None


def main():
    parser = argparse.ArgumentParser(description='Decompress Halo CE Xbox maps')
    parser.add_argument('--src', required=True, help='Source maps directory')
    parser.add_argument('--dst', help='Destination directory')
    parser.add_argument('--cache-slots', nargs='*', metavar='MAP:SLOT',
                        help='Create cache-slot files (e.g. wizard:3 bloodgulch:4)')
    parser.add_argument('--dry-run', action='store_true',
                        help='Show sizes without writing')
    parser.add_argument('--map', help='Process only this map name')
    parser.add_argument('--with-placeholders', action='store_true',
                        help='Create zero-filled placeholder files for unused cache slots')
    args = parser.parse_args()

    slot_map = {}
    if args.cache_slots:
        for spec in args.cache_slots:
            name, slot_str = spec.split(':')
            slot = int(slot_str)
            if slot not in SLOT_SIZES:
                print(f"error: invalid slot {slot} (must be 0-5)", file=sys.stderr)
                return 1
            slot_map[name] = slot

    if args.dst and not args.dry_run:
        os.makedirs(args.dst, exist_ok=True)

    maps = sorted(f for f in os.listdir(args.src) if f.endswith('.map'))
    if args.map:
        maps = [f for f in maps if f.replace('.map', '') == args.map]

    ok_count = 0
    err_count = 0

    for fname in maps:
        path = os.path.join(args.src, fname)
        compressed_size = os.path.getsize(path)

        info, err = decompress_map(path)
        if err:
            print(f"  SKIP {fname}: {err}")
            err_count += 1
            continue

        name = info['name']
        decomp_size = HEADER_SIZE + len(info['decompressed'])
        ratio = compressed_size / decomp_size * 100

        status = "compressed" if info['was_compressed'] else "already raw"
        print(f"  {name:<20} {compressed_size:>12,} -> {decomp_size:>12,}  "
              f"({ratio:.1f}%)  [{status}]")

        if args.dry_run:
            if name in slot_map:
                slot = slot_map[name]
                cap = SLOT_SIZES[slot]
                fits = "OK" if decomp_size <= cap else "TOO BIG"
                print(f"    -> slot {slot}: capacity {cap:,}, {fits}")
            ok_count += 1
            continue

        if not args.dst:
            ok_count += 1
            continue

        if name in slot_map:
            slot = slot_map[name]
            dst_path = os.path.join(args.dst, f"cache{slot:03d}.map")
            size, slot_err = write_cache_slot(info, dst_path, slot)
            if slot_err:
                print(f"    ERROR: {slot_err}")
                err_count += 1
                continue
            print(f"    -> {dst_path} ({size:,} bytes, slot {slot})")
        else:
            dst_path = os.path.join(args.dst, fname)
            size = write_raw(info, dst_path)
            print(f"    -> {dst_path} ({size:,} bytes)")

        ok_count += 1

    if args.with_placeholders and args.dst and not args.dry_run:
        used_slots = set(slot_map.values())
        for slot in range(6):
            if slot in used_slots:
                continue
            dst_path = os.path.join(args.dst, f"cache{slot:03d}.map")
            if os.path.exists(dst_path):
                continue
            cap = SLOT_SIZES[slot]
            print(f"  placeholder slot {slot}: {dst_path} ({cap:,} bytes)")
            with open(dst_path, 'wb') as f:
                chunk = b'\x00' * min(cap, 1024 * 1024)
                written = 0
                while written < cap:
                    n = min(len(chunk), cap - written)
                    f.write(chunk[:n])
                    written += n

    print(f"\n{ok_count} OK, {err_count} errors")
    return 1 if err_count > 0 else 0


if __name__ == '__main__':
    sys.exit(main())
