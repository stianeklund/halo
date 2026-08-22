#!/usr/bin/env python3
"""Generate the synthetic state snapshots for the bink_playback.c sound-import
cluster: bink_memory_callback_alloc (0x1c5ab0), FUN_001c6900 (AIFF/AIFC COMM
parse) and FUN_001c6c00 (sound-file container sniff dispatch).

All three are non-leaf and read nothing but globals and stubbed file I/O, so a
zero-filled run only ever reaches the early-exit path (FUN_001c6900 measured
5.7% coverage / one return value before these snapshots existed).  The
snapshots below drive each branch family explicitly; see the per-file
"description" for what each one covers.

    python3 tools/equivalence/make_bink_sound_snapshots.py
    python3 tools/equivalence/unicorn_diff.py FUN_001c6900 --seeds 20 \
        --allow-stubs --mem-trace \
        --state-snapshot artifacts/equivalence/aiff_comm_snap_44100.json

FUN_001c6c00 additionally needs BIPED_SIBLING_RESOLVE=1 (it calls same-TU
siblings) and --no-stub-arg-trace (the oracle stubs those siblings while the
candidate runs them for real, so the two sides issue a different number of
inner calls -- see lift-synthetic-equivalence rule 6).

BYTE ORDER
    FUN_00118be0, the definition-driven byte swapper, is a stubbed no-op on
    both sides, so every multi-byte field below is written in POST-swap
    (little-endian) form -- exactly what the parser compares against.  These
    images are therefore not valid on-disk AIFF; they are what the parser sees
    after the swap.
"""

import json
import struct
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent.parent / "artifacts" / "equivalence"

# --- chunk / container tags, as the parser compares them (post-swap) ---------
FORM, AIFF, AIFC = 0x464F524D, 0x41494646, 0x41494643
RIFF, WAVE = 0x52494646, 0x57415645
COMM, SSND, MARK, FVER = 0x434F4D4D, 0x53534E44, 0x4D41524B, 0x46564552
NONE = 0x4E4F4E45          # compressionType 'NONE'
SOWT = 0x74776F73          # compressionType 'sowt' -- rejected

# 80-bit IEEE-754 extended encodings of the three rates the engine accepts,
# stored byte-by-byte at 0x1c69ae-0x1c6a19 in the original.
RATE = {
    11025: bytes([0x40, 0x0C, 0xAC, 0x44, 0, 0, 0, 0, 0, 0]),
    22050: bytes([0x40, 0x0D, 0xAC, 0x44, 0, 0, 0, 0, 0, 0]),
    44100: bytes([0x40, 0x0E, 0xAC, 0x44, 0, 0, 0, 0, 0, 0]),
    8000:  bytes([0x40, 0x0B, 0xFA, 0x00, 0, 0, 0, 0, 0, 0]),  # not accepted
}


def _write(name, doc):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(doc, indent=2) + "\n")
    print(OUT / name)


def _snap(name, desc, regions=None, args=None, stub_returns=None,
          stub_writes=None):
    doc = {
        "description": desc,
        "build_label": "synthetic",
        "verified": True,
        "regions": regions or {},
        "arg_overrides": args or {},
    }
    if stub_returns:
        doc["stub_returns"] = stub_returns
    if stub_writes:
        doc["stub_writes"] = stub_writes
    _write(name, doc)


# ---------------------------------------------------------------------------
# bink_memory_callback_alloc (0x1c5ab0)
# ---------------------------------------------------------------------------
# Globals, named from the assert text pushed at 0x1c5c60:
#   0x4eae24 bink_globals.memory_pool_base   (pointer)
#   0x4eae28 bink_globals.memory_pool_offset
#   0x4eae2c bink_globals.memory_pool_size
#   0x4eae30 block-table count
#   0x4eacd0 16-entry dword block table
# They are contiguous at 0x4eae24, so one 16-byte region seeds all four; the
# oracle's DAT_004eaeXX DIR32 relocs then identity-relocate onto the same
# addresses the candidate reaches through absolute immediates.  Without that,
# candidate and oracle write to different addresses and --mem-trace reports a
# divergence on every seed (observed: 100/100 before these regions existed).

def _bink_regions(base, offset, size, count, table=()):
    tbl = bytearray(64)
    for i, v in enumerate(table):
        struct.pack_into("<I", tbl, i * 4, v)
    return {
        "0x004eae24": struct.pack("<IIII", base, offset, size, count).hex(),
        "0x004eacd0": bytes(tbl).hex(),
        # bink_playback_trace's "free MB" checkpoint scalar (written by the
        # function).  8 bytes so the identity-relocation seed window is filled.
        "0x0032eb9c": (b"\x00" * 8).hex(),
        # csprintf destination used by the "needs more memory" assert; mapped
        # so both sides hand the csprintf stub the SAME pointer.
        "0x005ab100": (b"\x00" * 64).hex(),
    }


# bink_memory_pool_is_empty (0x1c5a80) is a same-TU sibling: the oracle stubs
# it, the candidate runs the real body.  Where the function calls it, the stub
# return is pinned to what the real body computes for that block table.
def _pool_empty(value):
    return {"bink_memory_pool_is_empty": value, "FUN_001c5a80": value}


POOL = 0x00700000

_snap("bink_alloc_snap_success.json",
      "bink_memory_callback_alloc: valid 1MB pool, empty block table, size fits",
      _bink_regions(POOL, 0, 0x00100000, 0), {"size": [1, 0x2000]})

_snap("bink_alloc_snap_overrun.json",
      "bink_memory_callback_alloc: pool too small for the request (0x2f7 assert)",
      _bink_regions(POOL, 0x0F00, 0x1000, 0), {"size": [0x2000, 0x8000]})

_snap("bink_alloc_snap_pool_reset.json",
      "bink_memory_callback_alloc: stale count with empty table -> pool reset branch",
      _bink_regions(POOL, 0x0800, 0x00100000, 1), {"size": [1, 0x2000]},
      _pool_empty(1))

_snap("bink_alloc_snap_confused.json",
      "bink_memory_callback_alloc: stale count, non-empty table -> 0x2df assert",
      _bink_regions(POOL, 0x0800, 0x00100000, 3, [0, POOL + 0x100, POOL + 0x200]),
      {"size": [1, 0x2000]}, _pool_empty(0))

_snap("bink_alloc_snap_blocks_full.json",
      "bink_memory_callback_alloc: block table full (count==0x10)",
      _bink_regions(POOL, 0x0800, 0x00100000, 0x10,
                    [POOL + i * 0x1000 for i in range(16)]),
      {"size": [1, 0x2000]}, _pool_empty(0))

# The pre-check at 0x1c5b4a is UNSIGNED (ja) while the post-store check at
# 0x1c5ba8 is SIGNED (jle); a pool size of 0x80000000 passes the first and
# fails the second, which is the only way into the 0x312 arm.
_snap("bink_alloc_snap_needs_more_memory.json",
      "bink_memory_callback_alloc: signed/unsigned split -> 'needs more memory' (0x312)",
      _bink_regions(POOL, 0x0800, 0x80000000, 0), {"size": [1, 0x2000]},
      _pool_empty(0))

# 0x31f needs offset+size == pool size EXACTLY (any more fails the unsigned
# pre-check), so size is pinned rather than ranged.
_snap("bink_alloc_snap_offset_at_limit.json",
      "bink_memory_callback_alloc: allocation lands exactly on the pool end -> 0x31f assert",
      _bink_regions(POOL, 0xF000, 0x10000, 0), {"size": 0x1000}, _pool_empty(0))

_snap("bink_alloc_snap_clamp.json",
      "bink_memory_callback_alloc: offset+0x3000 overshoots the pool -> tail clamp",
      _bink_regions(POOL, 0x000FE000, 0x00100000, 0), {"size": [1, 0x1000]},
      _pool_empty(0))


# ---------------------------------------------------------------------------
# FUN_001c6900 -- AIFF/AIFC COMM parse
# ---------------------------------------------------------------------------
# Driven by the SEQUENCED form of snapshot "stub_writes": one entry per
# file_read_from_position call, so a specific chunk walk (and a specific read
# failure) can be scripted.

def _chunk(cid, size):
    return struct.pack("<II", cid, size)


def _comm_payload(rate_bits, channels=2, sample_size=16, compression=0):
    """22-byte COMM payload: +0 numChannels(u16), +2 numSampleFrames(u32,
    unread), +6 sampleSize(u16), +8 sampleRate(80-bit ext), +18
    compressionType."""
    b = bytearray(22)
    struct.pack_into("<H", b, 0, channels)
    struct.pack_into("<I", b, 2, 0x1000)
    struct.pack_into("<H", b, 6, sample_size)
    b[8:18] = rate_bits
    struct.pack_into("<I", b, 18, compression)
    return bytes(b)


def _comm_snap(name, desc, reads, returns=None):
    _snap(name, desc,
          stub_returns={"file_open": 1, "file_close": 1,
                        "file_read_from_position":
                            returns or [1] * len(reads) + [0]},
          stub_writes={"file_read_from_position":
                       [{"arg": 3, "data": d.hex()} for d in reads]})


# Every path walks one non-COMM chunk of ODD size first (5 -> padded to 6), so
# the skip/word-padding arm of the walk loop is exercised too.
FVER_HDR = _chunk(FVER, 5)

for hz in (11025, 22050, 44100):
    _comm_snap(f"aiff_comm_snap_{hz}.json",
               f"AIFF COMM walk -> {hz} Hz arm, 18-byte COMM (chunk size 0x12) -> success",
               [FVER_HDR, _chunk(COMM, 0x12), _comm_payload(RATE[hz])])

_comm_snap("aiff_comm_snap_22050_none.json",
           "AIFC COMM walk -> 22050 Hz arm, 0x16-byte COMM with compressionType "
           "'NONE' -> success via the second half of the success test",
           [FVER_HDR, _chunk(COMM, 0x16),
            _comm_payload(RATE[22050], 1, 8, NONE)])

_comm_snap("aiff_comm_snap_bad_compression.json",
           "AIFC COMM, 44100 Hz, compressionType 'sowt' -> reject (rate/channels/"
           "sampleSize still stored through format_out)",
           [FVER_HDR, _chunk(COMM, 0x16),
            _comm_payload(RATE[44100], 2, 16, SOWT)])

_comm_snap("aiff_comm_snap_bad_rate.json",
           "AIFF COMM with an unsupported 8000 Hz rate -> stores -1 and fails",
           [FVER_HDR, _chunk(COMM, 0x12), _comm_payload(RATE[8000])])

_comm_snap("aiff_comm_snap_payload_read_fails.json",
           "AIFF COMM header found, COMM payload read fails -> false",
           [FVER_HDR, _chunk(COMM, 0x12), _comm_payload(RATE[11025])],
           returns=[1, 1, 0])

_comm_snap("aiff_comm_snap_no_comm.json",
           "AIFF chunk walk over 3 non-COMM chunks then a failed read -> false",
           [FVER_HDR, _chunk(SSND, 8), _chunk(MARK, 7), FVER_HDR],
           returns=[1, 1, 1, 0])

# file_open failure: the only path through the second (no-file_close) epilogue.
_snap("aiff_comm_snap_open_fails.json",
      "AIFF COMM parse: file_open fails -> false via the no-file_close epilogue",
      stub_returns={"file_open": 0, "file_close": 1,
                    "file_read_from_position": 0})


# ---------------------------------------------------------------------------
# FUN_001c6c00 -- sound-file container sniff dispatch
# ---------------------------------------------------------------------------
# Uses the POSITIONAL (image-backed) form of "stub_writes": the read is served
# out of a synthetic file image at the callee's own offset/size arguments, so
# it does not matter how many times the read is called or in what order.  That
# is required here -- FUN_001c6880 and FUN_001c6900 are same-TU siblings the
# candidate runs for real while the oracle stubs them, so the two sides issue a
# DIFFERENT number of file reads and a call-index-sequenced write would
# desynchronise.
#
# The oracle's sibling stub returns are pinned to what the candidate's real
# bodies compute from the image; running instead with --real-callees leaves
# them at 0 (the per-function delinked ref carries no sibling bodies) and
# produces a textbook sibling-asymmetry false divergence, oracle=0 lifted=1.

def _aiff_image(rate_bits, comm_size=0x12, type_tag=AIFF):
    img = bytearray(0x40)
    img[0x00:0x04] = struct.pack("<I", FORM)
    struct.pack_into("<I", img, 0x04, 0x34)      # form size (unread)
    img[0x08:0x0C] = struct.pack("<I", type_tag)
    img[0x0C:0x10] = struct.pack("<I", COMM)     # first chunk, read at 0xc
    struct.pack_into("<I", img, 0x10, comm_size)
    img[0x14:0x2A] = _comm_payload(rate_bits)    # payload, read at 0x14
    return bytes(img)


def _riff_image():
    img = bytearray(0x40)
    img[0x00:0x04] = struct.pack("<I", RIFF)
    struct.pack_into("<I", img, 0x04, 0x34)
    img[0x08:0x0C] = struct.pack("<I", WAVE)
    return bytes(img)


def _import_snap(name, desc, image, siblings, args=None):
    _snap(name, desc, args=args,
          stub_returns=dict({"file_open": 1, "file_close": 1,
                             "file_read_from_position": 1}, **siblings),
          stub_writes={"file_read_from_position":
                       [{"image": image.hex(), "offset_arg": 1,
                         "size_arg": 2, "arg": 3}]})


AIFF_OK, AIFF_BAD = _aiff_image(RATE[11025]), _aiff_image(RATE[8000])

_import_snap("sound_import_snap_aiff_ok.json",
             "FUN_001c6c00: valid AIFF/COMM 11025 -> true via the AIFF branch",
             AIFF_OK, {"FUN_001c6880": 1, "FUN_001c6900": 1,
                       "FUN_001c6d20": 0, "FUN_001c6d90": 0})

# Exercises BOTH delegations in one run: the AIFF branch fails on the rate, the
# RIFF sniff then fails on the same image.
_import_snap("sound_import_snap_aiff_bad_rate.json",
             "FUN_001c6c00: AIFF container, unsupported 8000 Hz rate -> both "
             "branches fail -> false",
             AIFF_BAD, {"FUN_001c6880": 1, "FUN_001c6900": 0,
                        "FUN_001c6d20": 0, "FUN_001c6d90": 0})

# FUN_001c6d20 (game_sound.c) and FUN_001c6d90 (unported) live in other TUs, so
# both sides stub them and these pins are symmetric.
_import_snap("sound_import_snap_wave_ok.json",
             "FUN_001c6c00: RIFF/WAVE container -> true via the WAVE branch",
             _riff_image(), {"FUN_001c6880": 0, "FUN_001c6900": 0,
                             "FUN_001c6d20": 1, "FUN_001c6d90": 1})

_import_snap("sound_import_snap_wave_parse_fails.json",
             "FUN_001c6c00: RIFF/WAVE sniff ok but 'fmt ' parse fails -> false",
             _riff_image(), {"FUN_001c6880": 0, "FUN_001c6900": 0,
                             "FUN_001c6d20": 1, "FUN_001c6d90": 0})

_import_snap("sound_import_snap_unknown.json",
             "FUN_001c6c00: unrecognised container -> both sniffs fail -> false",
             bytes(0x40), {"FUN_001c6880": 0, "FUN_001c6900": 0,
                           "FUN_001c6d20": 0, "FUN_001c6d90": 0})

# The two parameter asserts.  A JSON null arg_override is the only way to pin a
# pointer parameter to NULL (an integer 0 still gets a scratch slot).  Both
# sides stop inside the guard because system_exit is __noreturn.
for key in ("info", "file"):
    _import_snap(f"sound_import_snap_null_{key}.json",
                 f"FUN_001c6c00: {key} == NULL -> '{key}' assert "
                 f"(sound_import.c line 0x{0x12 if key == 'info' else 0x13:x})",
                 AIFF_OK, {"FUN_001c6880": 1, "FUN_001c6900": 1,
                           "FUN_001c6d20": 0, "FUN_001c6d90": 0},
                 args={key: None})
