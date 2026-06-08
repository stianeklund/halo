#!/usr/bin/env python3
"""Assemble per-target biped equivalence snapshots from the ACTIVE-COMBAT
capture in /mnt/g/dev/halo/artifacts/combat_capture/ (main repo, durable).

Distinct from _assemble_biped_snaps.py (which used the held-scene capture in
artifacts/biped_capture/cur_*.bin).  This combat capture has 468 live objects
(68 bipeds) and is the input the final equivalence push runs against.

File map (combat_capture -> VA region):
  gamestate.bin   -> 0x800b0000 (0x310000): object table + datums + tag table
  heap_809_0..7   -> 0x80900000 + i*0x40000 (tag bodies + game_globals heap)
  g_2b4.bin       -> 0x2b4000  (kill-Z @0x2b4d1c = -2000)
  g_31f.bin       -> 0x31f000  (fwd-vec ptr @0x31fc38 = 0x28caa8)
  g_4e4.bin       -> 0x4e4000  (tags_loaded)
  g_4e5.bin       -> 0x4e5000  (tag count struct)
  g_505.bin       -> 0x505000  (tag instances ptr @0x5054f0 = 0x803a6024)
  g_506.bin       -> 0x506000  (game_globals ptr @0x5064d4)
  g_5a8.bin       -> 0x5a8000  (object-table ptr @0x5a8d50 = 0x800b9370)
  g_250.bin       -> 0x250000  (thresholds / rdata consts)

Carries the rdata const page 0x28c000 from the prior assembled snapshot (the
combat capture did not grab it; it is scene-invariant .rdata).

Sets "real_tag_get": true so the gated tag_get stub (env BIPED_REAL_TAGS=1)
uses real tag bodies resolved via *0x5054f0.

Object-array datum chain (verified against this capture):
  arr  = *0x5a8d50            = 0x800b9370
  elem = u16(arr+0x22)        = 12
  data = u32(arr+0x34)        = 0x800b93a8
  obj  = u32(data + (h&0xffff)*elem + 8)
  class= u16(obj+0x64)        (biped == 0)
Verified biped handle 0xe26f0000 (idx 0) -> obj 0x800bf3f8.
"""
import json
import os
import struct

WT = "/mnt/g/dev/halo/.claude/worktrees/scalable-coalescing-lobster"
CC = "/mnt/g/dev/halo/artifacts/combat_capture"
ART = os.path.join(WT, "artifacts")

HANDLE = 0xE26F0000  # biped idx 0, verified class==0, obj 0x800bf3f8


def rd(name):
    return open(os.path.join(CC, name), "rb").read()


def regions():
    r = {}
    r[0x800B0000] = rd("gamestate.bin")
    for i in range(8):
        va = 0x80900000 + i * 0x40000
        r[va] = rd(f"heap_809_{i}.bin")
    r[0x2B4000] = rd("g_2b4.bin")
    r[0x31F000] = rd("g_31f.bin")
    r[0x4E4000] = rd("g_4e4.bin")
    r[0x4E5000] = rd("g_4e5.bin")
    r[0x505000] = rd("g_505.bin")
    r[0x506000] = rd("g_506.bin")
    r[0x5A8000] = rd("g_5a8.bin")
    r[0x250000] = rd("g_250.bin")
    return r


def carry_rdata(r):
    """Carry scene-invariant .rdata pages the combat capture did not grab,
    from the prior assembled held-scene snapshot."""
    prev_path = os.path.join(ART, "snapshot_FUN_001a0680.json")
    if not os.path.exists(prev_path):
        return r
    prev = json.load(open(prev_path))
    for va_hex, hexd in prev["regions"].items():
        va = int(va_hex, 16)
        if va in (0x28C000,):
            if va not in r:
                r[va] = bytes.fromhex(hexd)
    return r


# Per-target arg overrides (same scalar gates as the held-scene assembler;
# scalars are scene-invariant function inputs, not capture-dependent).
TARGETS = {
    "FUN_001a0680": {"unit_handle": HANDLE},
    "FUN_001a0b30": {"unit_handle": HANDLE},
    "FUN_001a0e00": {"unit_handle": HANDLE, "eax": HANDLE, "threshold": 0x3DCCCCCD},
    "FUN_001a1a10": {"unit_handle": HANDLE, "edi": HANDLE, "scale": 0x40000000,
                     "direction": 0x28CAA8, "eax": 0x28CAA8},
    "FUN_001a2290": {"unit_handle": HANDLE, "edi": HANDLE},
    "FUN_001a2b10": {"unit_handle": HANDLE, "edi": HANDLE},
}


def main():
    base = carry_rdata(regions())
    region_json = {f"0x{va:08x}": data.hex() for va, data in sorted(base.items())}
    total = sum(len(d) for d in base.values())
    for func, args in TARGETS.items():
        snap = {
            "description": f"COMBAT biped equiv {func}; handle 0x{HANDLE:08x} "
                           f"(verified class==0, obj 0x800bf3f8); active-combat "
                           f"capture 468 objects / 68 bipeds",
            "captured_at": "2026-06-07-combat",
            "real_tag_get": True,
            "arg_overrides": args,
            "regions": region_json,
        }
        out = os.path.join(ART, f"snapshot_{func}_combat.json")
        json.dump(snap, open(out, "w"))
        print(f"  wrote {out}: {len(base)} regions, {total} bytes, args={args}")


if __name__ == "__main__":
    main()
