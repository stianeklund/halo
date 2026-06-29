#!/usr/bin/env python3
"""Diff two `.halorec` trajectories pool-by-pool, aligned by relative game tick.

The comparison half of A/B trajectory testing (docs/ab-trajectory-testing.md):

  * A/A (same build twice): expect ZERO divergence on the exact-tick overlap.
    This is the determinism gate — if it isn't clean, the harness is unsound and
    nothing built on top is trustworthy.
  * A/B (cachebeta golden vs default.xbe): divergences localize a regression in
    time (relative tick) and structure (pool / slot / byte offset).

Alignment: each run is anchored at its own first captured tick (the engine's
gameplay-ready event is deterministic, but the absolute tick counter need not be),
and frames are matched on EQUAL relative tick. Determinism makes equal-tick frames
byte-identical, so only the exact-tick intersection is comparable; frames a run
captured alone are reported as unmatched, never as divergence.

Pools are resolved per frame (ptr -> header -> used data span) and compared
record-by-record, so the comparison is independent of where the heap placed the
pool. Datum salt (record +0x00) is compared too, so a slot reused by a different
entity shows up.

    python3 trajectory_diff.py A.halorec B.halorec
    python3 trajectory_diff.py A.halorec B.halorec --json out.json --max-report 40
"""
import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import halorec_to_snapshot as h2s   # noqa: E402  (parse_halorec/_reader/_pool_header)

GAME_TIME_GLOBALS_PTR = 0x45708C

POOLS = {
    "objects": h2s.OBJECT_TABLE_PTR,
    "players": h2s.PLAYER_TABLE_PTR,
    "actors": h2s.ACTOR_TABLE_PTR,
    "props": h2s.PROP_POOL_PTR,
}

# Intra-tick-volatile per-record counters (range [offset, offset+size) per pool).
# QMP capture pauses at an arbitrary instruction *within* a frame, not at a frame
# boundary, so a per-tick counter the engine bumps mid-frame can be caught one
# ahead/behind even in two deterministic runs at the same tick. These offsets were
# identified by the A/A gate as the ONLY fields that ever differed cachebeta-vs-
# itself; everything else was byte-identical. Masking them is what makes the
# byte-diff a determinism gate rather than a phase detector. (A future in-guest
# tick-hook capture would be frame-phase-locked and need no mask.)
DEFAULT_VOLATILE = {
    "actors": [(0x4A, 2)],
    "props": [(0x26, 2)],
}


def _mask(rec, ranges):
    """Zero out volatile byte ranges in a record copy (None passes through)."""
    if rec is None or not ranges:
        return rec
    buf = bytearray(rec)
    for off, size in ranges:
        for i in range(off, min(off + size, len(buf))):
            buf[i] = 0
    return bytes(buf)


def frame_tick(reader):
    """Recover the game tick from a frame's captured game-time region."""
    gt = reader(GAME_TIME_GLOBALS_PTR, 4)
    if gt is None:
        return None
    ptr = struct.unpack("<I", gt)[0]
    blk = reader(ptr, 0x10)
    if blk is None:
        return None
    return struct.unpack_from("<I", blk, 0x0C)[0]


def index_by_tick(frames):
    """{tick: (reader, frame)} keeping the first frame at each tick."""
    out = {}
    for fr in frames:
        rd = h2s._reader(fr.regions)
        tk = frame_tick(rd)
        if tk is None or tk in out:
            continue
        out[tk] = (rd, fr)
    return out


def _pool_records(reader, ptr):
    """(header dict, [record_bytes per used slot]) for a pool, or (None, [])."""
    hd = h2s._pool_header(reader, ptr)
    if hd is None or hd["magic"] != h2s.DATA_T_MAGIC or hd["es"] <= 0:
        return None, []
    n = hd["cur"] if 0 < hd["cur"] <= hd["max"] else hd["max"]
    if not (0 < n <= 5000):
        return hd, []
    recs = []
    for slot in range(n):
        recs.append(reader(hd["data"] + slot * hd["es"], hd["es"]))
    return hd, recs


def _first_byte_diff(a, b):
    """Offset of the first differing byte between two equal-meaning records."""
    if a is None or b is None:
        return 0 if a != b else None
    m = min(len(a), len(b))
    for i in range(m):
        if a[i] != b[i]:
            return i
    return m if len(a) != len(b) else None


def diff_frame(rdA, rdB, pools, volatile=None):
    """Per-pool divergences at one matched tick: list of dicts."""
    if volatile is None:
        volatile = DEFAULT_VOLATILE
    div = []
    for name in pools:
        ptr = POOLS[name]
        ranges = volatile.get(name)
        hdA, recsA = _pool_records(rdA, ptr)
        hdB, recsB = _pool_records(rdB, ptr)
        if (hdA is None) != (hdB is None):
            div.append({"pool": name, "kind": "presence",
                        "a": hdA is not None, "b": hdB is not None})
            continue
        if hdA is None:
            continue
        if hdA["cur"] != hdB["cur"]:
            div.append({"pool": name, "kind": "count",
                        "a": hdA["cur"], "b": hdB["cur"]})
        for slot in range(min(len(recsA), len(recsB))):
            off = _first_byte_diff(_mask(recsA[slot], ranges), _mask(recsB[slot], ranges))
            if off is not None:
                ra, rb = recsA[slot], recsB[slot]
                div.append({
                    "pool": name, "kind": "record", "slot": slot, "offset": off,
                    "a": ra[off:off + 4].hex() if ra else None,
                    "b": rb[off:off + 4].hex() if rb else None,
                })
    return div


def diff_trajectories(framesA, framesB, pools=("objects", "players", "actors", "props")):
    """Align by relative tick, diff matched frames. Returns a structured result."""
    idxA, idxB = index_by_tick(framesA), index_by_tick(framesB)
    if not idxA or not idxB:
        return {"error": "a recording has no tick-bearing frames",
                "framesA": len(framesA), "framesB": len(framesB)}
    aA, aB = min(idxA), min(idxB)
    relA = {tk - aA: v for tk, v in idxA.items()}
    relB = {tk - aB: v for tk, v in idxB.items()}
    common = sorted(set(relA) & set(relB))

    frames_out = []
    clean = 0
    first = None
    for rel in common:
        rdA = relA[rel][0]
        rdB = relB[rel][0]
        div = diff_frame(rdA, rdB, pools)
        if not div:
            clean += 1
        else:
            if first is None:
                first = {"rel": rel, **div[0]}
            frames_out.append({"rel": rel, "divergences": div})
    return {
        "anchorA": aA, "anchorB": aB,
        "framesA": len(framesA), "framesB": len(framesB),
        "ticks_A": len(relA), "ticks_B": len(relB),
        "matched": len(common),
        "rel_span": [common[0], common[-1]] if common else None,
        "clean_matched": clean,
        "divergent_matched": len(frames_out),
        "first_divergence": first,
        "divergent_frames": frames_out,
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a", help="recording A (e.g. cachebeta golden / run 1)")
    ap.add_argument("b", help="recording B (e.g. default.xbe / run 2)")
    ap.add_argument("--pools", default="objects,players,actors,props",
                    help="comma list of pools to compare")
    ap.add_argument("--json", type=Path, default=None, help="write full result JSON")
    ap.add_argument("--max-report", type=int, default=20,
                    help="max divergent frames to print")
    a = ap.parse_args(argv)

    _, _, framesA = h2s.parse_halorec(a.a)
    _, _, framesB = h2s.parse_halorec(a.b)
    pools = tuple(p.strip() for p in a.pools.split(",") if p.strip())
    res = diff_trajectories(framesA, framesB, pools)

    if "error" in res:
        sys.exit(f"error: {res['error']} (A={res['framesA']} B={res['framesB']} frames)")

    print("=" * 78)
    print(f"A: {Path(a.a).name} ({res['framesA']} frames, {res['ticks_A']} ticks)")
    print(f"B: {Path(a.b).name} ({res['framesB']} frames, {res['ticks_B']} ticks)")
    print(f"matched on exact rel-tick: {res['matched']}"
          + (f"  (rel {res['rel_span'][0]}..{res['rel_span'][1]})" if res['rel_span'] else ""))
    overlap = (100.0 * res["matched"] / min(res["ticks_A"], res["ticks_B"])
               if min(res["ticks_A"], res["ticks_B"]) else 0.0)
    print(f"overlap: {overlap:.0f}% of the smaller run's ticks")
    print(f"clean matched frames: {res['clean_matched']}/{res['matched']}   "
          f"divergent: {res['divergent_matched']}")
    print("-" * 78)
    if res["divergent_matched"] == 0:
        print("CLEAN: every matched frame is byte-identical across pools.")
    else:
        for fr in res["divergent_frames"][:a.max_report]:
            head = fr["divergences"][0]
            extra = f" (+{len(fr['divergences']) - 1} more)" if len(fr["divergences"]) > 1 else ""
            if head["kind"] == "record":
                print(f"  rel={fr['rel']:4d} {head['pool']:8s} slot={head['slot']:<3d} "
                      f"+0x{head['offset']:03x}  A={head['a']} B={head['b']}{extra}")
            else:
                print(f"  rel={fr['rel']:4d} {head['pool']:8s} {head['kind']}: "
                      f"A={head['a']} B={head['b']}{extra}")
        e = res["first_divergence"]
        print("-" * 78)
        print(f"FIRST DIVERGENCE: rel={e['rel']} pool={e['pool']} kind={e['kind']}"
              + (f" slot={e.get('slot')} +0x{e.get('offset', 0):x}" if e["kind"] == "record" else ""))

    if a.json:
        a.json.write_text(json.dumps(res, indent=2) + "\n")
        print(f"  [json] -> {a.json}")
    # exit 0 = clean, 3 = divergence (CI-friendly)
    return 0 if res["divergent_matched"] == 0 else 3


if __name__ == "__main__":
    sys.exit(main())
