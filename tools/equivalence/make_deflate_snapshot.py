#!/usr/bin/env python3
"""Build a synthetic deflate_state snapshot for Unicorn equivalence testing.

zlib functions that operate on a live ``deflate_state`` (e.g.
``deflateSetDictionary`` at 0x110d40) early-exit under the harness's default
zero-fill: the very first guards read ``strm->state`` (strm+0x1c) and
``state->status`` (state+4 == 0x2a) and bail to Z_STREAM_ERROR before any real
logic runs.  To exercise the real body we must seed a *coherent* deflate_state:
consistent internal pointers and sizes so every array index stays in-bounds.

The equivalence oracle only needs the oracle and candidate to read the SAME
seeded state -- the contents need not be the values a real game session would
hold, only self-consistent.  This is the same "seed the gate so the real path
is reached" technique proven on the transition_function_evaluate cluster
(table-flag seeding), generalised from a single flag to a full state struct.

Memory layout (each structure on its own 64 KB page, well clear of the
harness regions CODE 0x400000, STACK 0x100000, GLOBALS 0x500000,
SCRATCH 0x10000000):

    strm    0x02000000   z_stream
    state   0x02010000   deflate_state
    window  0x02020000   sliding window         (w_size bytes)
    prev    0x02040000   prev-match chain        (w_size ushorts)
    head    0x02060000   hash head table         (hash_size ushorts)
    dict    0x02080000   dictionary buffer       (>= max dictLength)

deflate_state field offsets touched by FUN_00110d40 (verified against
src/halo/math/real_math.c:4094 and the cachebeta disasm):

    strm+0x1c   state pointer            (must be non-zero)
    strm+0x30   adler                    (read + written by adler32)
    state+0x04  status                   (must be 0x2a == INIT_STATE)
    state+0x24  w_size
    state+0x2c  w_mask
    state+0x30  window pointer
    state+0x38  prev pointer
    state+0x3c  head pointer
    state+0x40  ins_h                    (read + written)
    state+0x4c  hash_mask
    state+0x50  hash_shift
    state+0x54  block_start              (written)
    state+0x64  strstart                 (written)

w_size is deliberately small (0x400 == windowBits 10, a valid zlib value) so
the ``more = w_size - MIN_LOOKAHEAD(0x106)`` truncation threshold is 762.  A
dictLength *range* that straddles 762 then covers BOTH the no-truncation and
the truncation branch without forcing 32K-iteration hash loops.

Usage:
    python3 tools/equivalence/make_deflate_snapshot.py \
        -o artifacts/snapshots/deflate_state.json
"""

import argparse
import struct
from pathlib import Path

# --- region base addresses (64 KB-page isolated) ---------------------------
STRM_ADDR   = 0x02000000
STATE_ADDR  = 0x02010000
WINDOW_ADDR = 0x02020000
PREV_ADDR   = 0x02040000
HEAD_ADDR   = 0x02060000
DICT_ADDR   = 0x02080000

# --- self-consistent deflate_state parameters ------------------------------
W_SIZE     = 0x0400        # windowBits 10
W_MASK     = W_SIZE - 1    # 0x3ff
HASH_BITS  = 15
HASH_SIZE  = 1 << HASH_BITS  # 0x8000
HASH_MASK  = HASH_SIZE - 1   # 0x7fff
HASH_SHIFT = 5
STATUS_INIT = 0x2a         # zlib INIT_STATE
ADLER_INIT  = 1

WINDOW_BYTES = 0x8000      # >= w_size, page-rounded
PREV_BYTES   = 0x10000     # >= w_size * 2
HEAD_BYTES   = 0x10000     # == hash_size * 2
DICT_BYTES   = 0x800       # >= max dictLength (1500), pattern-filled

# dictLength range straddling the truncation threshold (more = 762)
DICTLEN_MIN = 64
DICTLEN_MAX = 1500


def _put(buf: bytearray, off: int, val: int) -> None:
    """Write a little-endian uint32 at byte offset ``off``."""
    struct.pack_into("<I", buf, off, val & 0xFFFFFFFF)


def build_regions(status: int = STATUS_INIT) -> dict:
    """Return {int_address: bytes} for every seeded region.

    ``status`` seeds state+4. The default 0x2a (INIT_STATE) reaches the real
    body; any other value drives every seed down the Z_STREAM_ERROR (-2) guard
    so the error-exit path can be covered too (return-value diversity).
    """
    # z_stream (only the two fields the function touches need real values)
    strm = bytearray(0x40)
    _put(strm, 0x1c, STATE_ADDR)
    _put(strm, 0x30, ADLER_INIT)

    # deflate_state
    state = bytearray(0x70)
    _put(state, 0x04, status)
    _put(state, 0x24, W_SIZE)
    _put(state, 0x2c, W_MASK)
    _put(state, 0x30, WINDOW_ADDR)
    _put(state, 0x38, PREV_ADDR)
    _put(state, 0x3c, HEAD_ADDR)
    _put(state, 0x40, 0)          # ins_h
    _put(state, 0x4c, HASH_MASK)
    _put(state, 0x50, HASH_SHIFT)
    _put(state, 0x54, 0)          # block_start
    _put(state, 0x64, 0)          # strstart

    # buffers: window/prev/head start zeroed (head[ins_h] read-before-write is
    # 0 first time on both sides -> consistent); dict is a fixed pattern so
    # adler32 + csmemcpy read identical bytes on oracle and candidate.
    window = bytes(WINDOW_BYTES)
    prev = bytes(PREV_BYTES)
    head = bytes(HEAD_BYTES)
    dict_buf = (bytes(range(256)) * ((DICT_BYTES // 256) + 1))[:DICT_BYTES]

    return {
        STRM_ADDR: bytes(strm),
        STATE_ADDR: bytes(state),
        WINDOW_ADDR: window,
        PREV_ADDR: prev,
        HEAD_ADDR: head,
        DICT_ADDR: dict_buf,
    }


def build_snapshot(status: int = STATUS_INIT) -> dict:
    regions = build_regions(status=status)
    out = {
        "description": (
            "synthetic coherent deflate_state for deflateSetDictionary "
            "(0x110d40) equivalence: status=0x2a, w_size=0x400 "
            "(more=762), w_mask=0x3ff, hash_mask=0x7fff, hash_shift=5. "
            "Seeds both no-truncation and truncation branches via a "
            "dictLength range straddling 762. Contents self-consistent, "
            "not real game state."
        ),
        "build_label": "synthetic",
        "regions": {f"0x{addr:08x}": data.hex() for addr, data in sorted(regions.items())},
        # cdecl args: param_1=strm ptr, param_2=dict ptr (fixed scalars),
        # param_3=dictLength (range -> some seeds cross the truncation point).
        "arg_overrides": {
            "param_1": STRM_ADDR,
            "param_2": DICT_ADDR,
            "param_3": [DICTLEN_MIN, DICTLEN_MAX],
        },
    }
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--output", default="artifacts/snapshots/deflate_state.json",
                    help="output snapshot JSON path")
    ap.add_argument("--status", type=lambda s: int(s, 0), default=STATUS_INIT,
                    help="deflate_state status (state+4); default 0x2a reaches the "
                         "body, any other value drives the Z_STREAM_ERROR(-2) guard")
    args = ap.parse_args()

    import json
    snap = build_snapshot(status=args.status)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(snap, f, indent=2)
        f.write("\n")

    nbytes = sum(len(bytes.fromhex(h)) for h in snap["regions"].values())
    print(f"wrote {out_path}  ({len(snap['regions'])} regions, {nbytes} bytes)")
    print(f"  arg_overrides: {snap['arg_overrides']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
