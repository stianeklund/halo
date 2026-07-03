#!/usr/bin/env python3
"""Sustained live probe of actor pathing state over XBDM (zero source change).

Waits for the level to load, then samples the actor array every ~2s and emits
ONLY meaningful transitions: changes in (action, cmdlist, moving, path_active)
per actor, and real position displacement. The key signal is whether
path_active (actor+0x4a8) EVER reads >=1 for a go_to actor.

Layout (from datum_get 0x119320): element = data + index*esize.
  actor_data ptr @ *0x6325a4 ; header esize@+0x22 count@+0x2e data@+0x34
  actor: salt@0 action@0x6c cmdlist@0x9c pos@0x12c moving@0x484 path@0x4a8
"""
import sys
import binascii
import subprocess
import struct
import time

ACTOR_DATA_PTR = 0x6325A4
DURATION = int(sys.argv[1]) if len(sys.argv) > 1 else 240


def rd(addr, length):
    out = subprocess.run(
        ["python3", "tools/xbox/xbdm_rdcp.py",
         f"getmem addr=0x{addr:x} length=0x{length:x}"],
        capture_output=True, text=True, timeout=30).stdout
    hx = "".join(c for c in out if c in "0123456789abcdefABCDEF.").replace("..", "00")
    try:
        return binascii.unhexlify(hx)
    except Exception:
        return b""


def u16(b, o): return int.from_bytes(b[o:o + 2], "little") if len(b) >= o + 2 else -1
def u32(b, o): return int.from_bytes(b[o:o + 4], "little") if len(b) >= o + 4 else 0
def f32(b, o): return struct.unpack("<f", b[o:o + 4])[0] if len(b) >= o + 4 else 0.0


def main():
    t_end = time.time() + DURATION
    prev = {}        # idx -> (action, cmdlist, moving, path)
    start_pos = {}   # idx -> (x,y,z)
    flip_seen = False
    loaded_announced = False
    while time.time() < t_end:
        arr_ptr = u32(rd(ACTOR_DATA_PTR, 4), 0)
        hdr = rd(arr_ptr, 0x40) if arr_ptr else b""
        esize = u16(hdr, 0x22)
        count = u16(hdr, 0x2e)
        data = u32(hdr, 0x34)
        if count <= 0 or esize <= 0 or data == 0:
            time.sleep(3)
            continue
        if not loaded_announced:
            print(f"[probe] level loaded: count={count} esize=0x{esize:x}")
            loaded_announced = True
        blob = rd(data, count * esize)
        for i in range(count):
            base = i * esize
            if base + 0x4ac > len(blob):
                continue
            if u16(blob, base) == 0:
                continue
            action = u16(blob, base + 0x6c)
            cmdlist = u16(blob, base + 0x9c)
            moving = blob[base + 0x484]
            path = blob[base + 0x4a8]
            pos = (f32(blob, base + 0x12c), f32(blob, base + 0x130), f32(blob, base + 0x134))
            key = (action, cmdlist, moving, path)
            if i not in start_pos:
                start_pos[i] = pos
            if path >= 1 and not flip_seen:
                print(f"[probe] *** PATH_ACTIVE>=1 actor[{i}] path={path} action={action} "
                      f"cmdlist={cmdlist} moving={moving} pos=({pos[0]:.2f},{pos[1]:.2f},{pos[2]:.2f})")
                flip_seen = True
            if prev.get(i) != key:
                disp = ((pos[0]-start_pos[i][0])**2 + (pos[1]-start_pos[i][1])**2 +
                        (pos[2]-start_pos[i][2])**2) ** 0.5
                print(f"[probe] actor[{i}] action={action} cmdlist={cmdlist} moving={moving} "
                      f"path={path} pos=({pos[0]:.2f},{pos[1]:.2f},{pos[2]:.2f}) disp={disp:.2f}")
                prev[i] = key
        time.sleep(2)
    # final displacement summary
    print(f"[probe] DONE flip_seen={flip_seen}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
