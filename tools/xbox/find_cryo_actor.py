#!/usr/bin/env python3
"""Scan live actor_data array over XBDM; dump cryo-relevant state.

Layout from datum_get(0x119320): element = data + index*element_size.
  actor_data ptr  = *0x6325a4
  header: element_size @+0x22 (u16), count @+0x2e (u16), data @+0x34 (u32)
  element[0] (u16) = salt (nonzero => valid slot)
Actor fields (from FUN_00018b90 / FUN_0002a3f0 / guard init):
  +0x58  actor-type tag index (u32)
  +0x6c  current action (u16)
  +0x9c  scripted command-list/prop index (u16)   (cryo_explosion_1 == 30)
  +0x12c position x,y,z (3*f32)
  +0x484 is_moving (u8)
  +0x4a8 path_active (u8)
"""
import sys
import binascii
import subprocess
import struct

ACTOR_DATA_PTR = 0x6325A4
SCAN_SLOTS = int(sys.argv[1]) if len(sys.argv) > 1 else 16


def read_mem(addr, length):
    out = subprocess.run(
        ["python3", "tools/xbox/xbdm_rdcp.py",
         f"getmem addr=0x{addr:x} length=0x{length:x}"],
        capture_output=True, text=True, timeout=30,
    ).stdout
    hexstr = "".join(c for c in out if c in "0123456789abcdefABCDEF.")
    return binascii.unhexlify(hexstr.replace("..", "00"))


def u16(b, o):
    return int.from_bytes(b[o:o + 2], "little")


def u32(b, o):
    return int.from_bytes(b[o:o + 4], "little")


def f32(b, o):
    return struct.unpack("<f", b[o:o + 4])[0]


def main():
    arr = u32(read_mem(ACTOR_DATA_PTR, 4), 0)
    hdr = read_mem(arr, 0x40)
    esize = u16(hdr, 0x22)
    count = u16(hdr, 0x2e)
    data = u32(hdr, 0x34)
    print(f"actor_data @0x{arr:08x} esize=0x{esize:x} count={count} data=0x{data:08x}")
    n = max(count, SCAN_SLOTS)
    blob = read_mem(data, n * esize)
    for i in range(n):
        base = i * esize
        if base + 0x4ac > len(blob):
            break
        salt = u16(blob, base + 0)
        if salt == 0:
            continue
        cmdlist = u16(blob, base + 0x9c)
        action = u16(blob, base + 0x6c)
        tag = u32(blob, base + 0x58)
        px, py, pz = f32(blob, base + 0x12c), f32(blob, base + 0x130), f32(blob, base + 0x134)
        moving = blob[base + 0x484]
        path = blob[base + 0x4a8]
        mark = "  <== cryo_explosion_1" if cmdlist == 30 else ""
        print(f"[{i}] salt=0x{salt:04x} tag=0x{tag:08x} action={action} "
              f"cmdlist={cmdlist} pos=({px:.2f},{py:.2f},{pz:.2f}) "
              f"moving={moving} path_active={path}{mark}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
