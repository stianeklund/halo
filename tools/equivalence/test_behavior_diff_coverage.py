#!/usr/bin/env python3
"""Pin behavior_diff's coverage accounting.

Regression guard for a silent false pass observed 2026-08-02: two viewer-made
`.halorec` recordings were diffed and the tool printed

    sustained divergences: 0
    CLEAN: behaviorally equivalent on the watch-list

having compared *nothing*. Neither recording carried the game-time region at
GAME_TIME_GLOBALS_PTR, so `frame_tick` returned None for every frame,
`slot_series` dropped every frame, and the empty onset list was reported as
equivalence. "No evidence of divergence" was rendered as "no divergence".

Both directions are pinned here, because a guard that only checks the failing
case can be satisfied by a tool that always reports INCONCLUSIVE:

  1. no tick region              -> exit 2, INCONCLUSIVE, not CLEAN
  2. tick region + matching data -> exit 0, CLEAN (guard is not over-eager)
  3. tick region + divergence    -> exit 3 (real signal still reported)
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import behavior_diff as bd          # noqa: E402
import halorec_to_snapshot as h2s   # noqa: E402
import trajectory_diff as td        # noqa: E402

ACTOR_STRIDE = 0x724
POOL_HDR = 0x80100000
POOL_DATA = 0x80200000
TIME_BLK = 0x80300000


def _pool_header(n_slots):
    """A data_t header the h2s/bd readers accept: max/es at 0x20, magic 0x28,
    cur 0x2E, data ptr 0x34."""
    h = bytearray(0x38)
    struct.pack_into("<hh", h, 0x20, n_slots, ACTOR_STRIDE)
    struct.pack_into("<I", h, 0x28, h2s.DATA_T_MAGIC)
    struct.pack_into("<h", h, 0x2E, n_slots)
    struct.pack_into("<I", h, 0x34, POOL_DATA)
    return bytes(h)


def _actor(combat_state):
    a = bytearray(ACTOR_STRIDE)
    struct.pack_into("<H", a, 0x00, 0x1234)      # salt != 0 => slot in use
    struct.pack_into("<I", a, 0x18, 0xFFFFFFFF)  # unit handle: NONE
    struct.pack_into("<I", a, 0x610, 0xFFFFFFFF)  # combat target: NONE
    struct.pack_into("<B", a, 0x6A, combat_state)
    return bytes(a)


def _frame(t, tick, combat_state):
    """One frame; tick=None omits the game-time region entirely."""
    regions = [
        (h2s.ACTOR_TABLE_PTR, struct.pack("<I", POOL_HDR)),
        (POOL_HDR, _pool_header(1)),
        (POOL_DATA, _actor(combat_state)),
    ]
    if tick is not None:
        blk = bytearray(0x10)
        struct.pack_into("<I", blk, 0x0C, tick)
        regions += [(td.GAME_TIME_GLOBALS_PTR, struct.pack("<I", TIME_BLK)),
                    (TIME_BLK, bytes(blk))]
    return h2s.Frame(t, regions)


CFG = {
    "window": 4,
    "min_run": 2,
    "pools": {"actors": {"fields": [
        {"label": "combat_state", "off": 0x6A, "fmt": "<B", "kind": "discrete"},
    ]}},
    "aggregates": [],
}


def _run(framesA, framesB):
    return bd.diff_behavior(framesA, framesB, CFG)


def main():
    failures = []

    # 1. No tick region anywhere -> nothing comparable. Must NOT read as clean.
    a = [_frame(i * 0.1, None, 3) for i in range(6)]
    b = [_frame(i * 0.1, None, 3) for i in range(6)]
    r = _run(a, b)
    if r["ticksA"] or r["ticksB"]:
        failures.append(f"1: expected 0 resolved ticks, got {r['ticksA']}/{r['ticksB']}")
    if r["compared_slots"]:
        failures.append(f"1: expected 0 slots compared, got {r['compared_slots']}")
    if r["onset_count"]:
        failures.append("1: unexpected onsets with no comparable data")

    # 2. Ticks present, data identical -> genuinely clean, guard must stay quiet.
    a = [_frame(i * 0.1, 100 + i, 3) for i in range(6)]
    b = [_frame(i * 0.1, 100 + i, 3) for i in range(6)]
    r = _run(a, b)
    if r["ticksA"] != 6 or r["ticksB"] != 6:
        failures.append(f"2: expected 6/6 resolved ticks, got {r['ticksA']}/{r['ticksB']}")
    if r["compared_slots"] != 1:
        failures.append(f"2: expected 1 slot compared, got {r['compared_slots']}")
    if r["onset_count"]:
        failures.append(f"2: expected clean, got {r['onset_count']} onsets")

    # 3. Ticks present, sustained divergence -> still reported.
    a = [_frame(i * 0.1, 100 + i, 3) for i in range(6)]
    b = [_frame(i * 0.1, 100 + i, 5) for i in range(6)]
    r = _run(a, b)
    if r["compared_slots"] != 1:
        failures.append(f"3: expected 1 slot compared, got {r['compared_slots']}")
    if r["onset_count"] != 1:
        failures.append(f"3: expected 1 onset, got {r['onset_count']}")

    # 4. End-to-end exit codes. The regression was main() RETURNING 0 on an
    #    empty comparison, so pinning diff_behavior's accounting alone would
    #    not have caught it -- drive the CLI the way ab_check does.
    import json
    import tempfile
    import hmrc
    with tempfile.TemporaryDirectory() as td_:
        d = Path(td_)
        cfg = d / "cfg.json"
        cfg.write_text(json.dumps(CFG))

        def rec(path, frames):
            hmrc.write_halorec(path, "t", [(f.t, f.regions) for f in frames])
            return str(path)

        no_tick = [_frame(i * 0.1, None, 3) for i in range(6)]
        same = [_frame(i * 0.1, 100 + i, 3) for i in range(6)]
        diff = [_frame(i * 0.1, 100 + i, 5) for i in range(6)]
        for label, fa, fb, want in (("no-tick", no_tick, no_tick, 2),
                                    ("clean", same, same, 0),
                                    ("divergent", same, diff, 3)):
            pa = rec(d / f"{label}_a.halorec", fa)
            pb = rec(d / f"{label}_b.halorec", fb)
            got = bd.main([pa, pb, "--config", str(cfg)])
            if got != want:
                failures.append(f"4/{label}: main() returned {got}, expected {want}")

    if failures:
        print("FAIL test_behavior_diff_coverage")
        for f in failures:
            print("  -", f)
        return 1
    print("PASS test_behavior_diff_coverage (4 cases)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
