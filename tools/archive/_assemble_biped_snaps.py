#!/usr/bin/env python3
"""Assemble per-target biped equivalence snapshots from the current held-scene
captures in artifacts/biped_capture/.

Produces artifacts/snapshot_<func>_live.json for each of the 5 targets with:
  - fresh gamestate (object table + records + tag table)  [base 0x800b0000]
  - heap chunks (bipd/antr tag bodies + game_globals)     [0x80900000..0x80b00000]
  - missing global pages (kill-Z, tag ptrs, game_globals ptr, flags)
  - rdata const pages carried over from the prior snapshot
  - arg_overrides for the chosen biped handle 0xe26f0000 + per-target scalars
Sets "real_tag_get": true so the (gated) tag_get stub uses real tag bodies.
"""
import json
import os
import struct

WT = "/mnt/g/dev/halo/.claude/worktrees/scalable-coalescing-lobster"
CAP = os.path.join(WT, "artifacts", "biped_capture")
ART = os.path.join(WT, "artifacts")

HANDLE = 0xE26F0000  # biped idx=0, tag 'bipd' verified, datum 0x800bf3f8


def rd(name):
    return open(os.path.join(CAP, name), "rb").read()


def regions():
    r = {}
    # fresh gamestate (object table + tag table)
    r[0x800B0000] = rd("cur_gamestate.bin")
    # heap chunks: tag bodies + game_globals
    for off in range(0, 0x200000, 0x40000):
        va = 0x80900000 + off
        r[va] = rd(f"cur_heap_{va:08x}.bin")
    # newly captured global pages
    # 0x5a8000 = object table ptr (0x5a8d50); 0x505000 = tag instances ptr;
    # 0x4e4000 = tags_loaded; 0x4e5000 = tag count struct ptr;
    # 0x506000 = game_globals ptr; 0x5aa000 = mp/flag bytes; 0x2b4000 = kill-Z.
    for va in (0x2B4000, 0x505000, 0x4E4000, 0x4E5000, 0x506000, 0x5A8000, 0x5AA000):
        r[va] = rd(f"cur_glob_{va:06x}.bin")
    return r


def carry_rdata(r):
    """Carry the rdata const pages (0x250000, 0x28c000) + dir globals
    (0x31f000) from the prior assembled snapshot, which the live capture
    did not re-grab (they are static .rdata/.data, scene-invariant)."""
    prev = json.load(open(os.path.join(ART, "snapshot_FUN_001a0680.json")))
    for va_hex, hexd in prev["regions"].items():
        va = int(va_hex, 16)
        if va in (0x250000, 0x28C000, 0x31F000):
            if va not in r:
                r[va] = bytes.fromhex(hexd)
    return r


TARGETS = {
    "FUN_001a0680": {"unit_handle": HANDLE},
    "FUN_001a0b30": {"unit_handle": HANDLE},
    # threshold is the 1st (float) arg, unit_handle in EAX. Real bipd phase
    # boundaries are 1.5/5.0/8.0 ticks -> 0.05/0.167/0.267 s. Pick 0.1f
    # (0x3dcccccd): fVar1<=threshold<fVar2 exercises the first (if) branch.
    "FUN_001a0e00": {"unit_handle": HANDLE, "eax": HANDLE, "threshold": 0x3DCCCCCD},
    # scale=2.0f, direction = forward-vec ptr value 0x28caa8 (from 0x31fc50),
    # out_point/out_vec are caller stack buffers (seeded by harness).
    "FUN_001a1a10": {"unit_handle": HANDLE, "edi": HANDLE, "scale": 0x40000000,
                      "direction": 0x28CAA8, "eax": 0x28CAA8},
    "FUN_001a2290": {"unit_handle": HANDLE, "edi": HANDLE},
}


def main():
    base = carry_rdata(regions())
    region_json = {f"0x{va:08x}": data.hex() for va, data in sorted(base.items())}
    total = sum(len(d) for d in base.values())
    for func, args in TARGETS.items():
        snap = {
            "description": f"LIVE biped equiv {func}; handle 0x{HANDLE:08x} "
                           f"(verified 'bipd'); held scene 2026-06-07",
            "captured_at": "2026-06-07",
            "real_tag_get": True,
            "arg_overrides": args,
            "regions": region_json,
        }
        out = os.path.join(ART, f"snapshot_{func}_live.json")
        json.dump(snap, open(out, "w"))
        print(f"  wrote {out}: {len(base)} regions, {total} bytes, args={args}")


if __name__ == "__main__":
    main()
