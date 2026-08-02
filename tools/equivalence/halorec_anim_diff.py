#!/usr/bin/env python3
"""Per-biped ANIMATION-state summary and A/B diff over `.halorec` trajectories.

Animation advances every game tick regardless of controller input, which makes it
the one behavioural signal a ZERO-INPUT boot can observe -- unlike AI turning or
combat, which need a player to react to. So a plain core-boot on both builds is a
sufficient A/B experiment for it.

Requires a recording captured with object BODIES followed
(`capture_trajectory.py --include-object-bodies`): the object pool's element is
only a 12-byte datum entry holding a pointer at +0x08, and every animation field
lives in the body it points at:

    body +0x7c  u32   animation-graph datum handle ('antr'), 0xffffffff = none
    body +0x80  i16   animation index
    body +0x82  i16   animation frame index
    body +0x253 i8    unit animation state (when captured with body size >=0x254)

Bipeds are reached through the ACTOR pool (actor +0x18 = unit object handle), so
series are keyed by actor SLOT -- the same identity the rest of the A/B harness
uses, and stable across builds even though datum salts are not.

    halorec_anim_diff.py A.halorec                      # summarize one run
    halorec_anim_diff.py A.halorec B.halorec            # compare two runs
    halorec_anim_diff.py AA1 AA2 --vs AB                # A/A noise floor, then A/B
"""
import argparse
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from halorec_to_snapshot import (   # noqa: E402
    ACTOR_TABLE_PTR, DATA_T_MAGIC, OBJECT_TABLE_PTR, parse_halorec,
)

GAME_TIME_GLOBALS_PTR = 0x45708C
ACTOR_UNIT_HANDLE_OFF = 0x18
ACTOR_CONTROL_FACING_OFF = 0x6FC
ACTOR_BASE_FACING_OFF = 0x174
ACTOR_DESIRED_FACING_OFF = 0x5A4
ANIM_GRAPH_OFF = 0x7C
ANIM_INDEX_OFF = 0x80
ANIM_FRAME_OFF = 0x82
UNIT_ANIM_STATE_OFF = 0x253
FORWARD_OFF = 0x24
DESIRED_FACING_OFF = 0x1D4
OBJECT_ENTRY_PTR_OFF = 0x08
HANDLE_NONE = 0xFFFFFFFF


# ---------------------------------------------------------------- extraction

def _reader(regions):
    """read(addr, n) over a {addr: bytes} frame map; None unless fully covered."""
    iv = sorted(regions.items())

    def read(addr, n):
        for a, b in iv:
            if a <= addr and addr + n <= a + len(b):
                return b[addr - a:addr - a + n]
        return None
    return read


def _pool(read, ptr):
    pv = read(ptr, 4)
    if pv is None:
        return None
    hdr = struct.unpack("<I", pv)[0]
    h = read(hdr, 0x38) if hdr else None
    if h is None:
        return None
    max_c, es = struct.unpack_from("<hh", h, 0x20)
    magic = struct.unpack_from("<I", h, 0x28)[0]
    cur = struct.unpack_from("<h", h, 0x2E)[0]
    data = struct.unpack_from("<I", h, 0x34)[0]
    if magic != DATA_T_MAGIC or es <= 0:
        return None
    return {"max": max_c, "es": es, "cur": cur, "data": data}


def frame_tick(read):
    pv = read(GAME_TIME_GLOBALS_PTR, 4)
    if pv is None:
        return None
    gt = struct.unpack("<I", pv)[0]
    blk = read(gt + 0x0C, 4) if gt else None
    return struct.unpack("<I", blk)[0] if blk else None


def object_anim(read, obj_pool, index, salt=None):
    """Animation state and standing-turn inputs for an object slot, or None."""
    if obj_pool is None or not (0 <= index < obj_pool["max"]):
        return None
    e = read(obj_pool["data"] + index * obj_pool["es"], obj_pool["es"])
    if e is None or obj_pool["es"] < OBJECT_ENTRY_PTR_OFF + 4:
        return None
    ent_salt = struct.unpack_from("<H", e, 0)[0]
    if ent_salt == 0 or (salt is not None and ent_salt != salt):
        return None
    body_ptr = struct.unpack_from("<I", e, OBJECT_ENTRY_PTR_OFF)[0]
    body = read(body_ptr, ANIM_FRAME_OFF + 2)
    if body is None:
        return None
    graph = struct.unpack_from("<I", body, ANIM_GRAPH_OFF)[0]
    if graph == HANDLE_NONE:
        return None
    state_blob = read(body_ptr + UNIT_ANIM_STATE_OFF, 1)
    unit_state = struct.unpack("<b", state_blob)[0] if state_blob else None
    forward_blob = read(body_ptr + FORWARD_OFF, 8)
    desired_blob = read(body_ptr + DESIRED_FACING_OFF, 8)
    turn_dot = turn_cross = None
    if forward_blob and desired_blob:
        forward_x, forward_y = struct.unpack("<ff", forward_blob)
        desired_x, desired_y = struct.unpack("<ff", desired_blob)
        desired_length = math.hypot(desired_x, desired_y)
        if desired_length:
            desired_x /= desired_length
            desired_y /= desired_length
        else:
            desired_x, desired_y = forward_x, forward_y
        turn_dot = desired_x * forward_x + desired_y * forward_y
        turn_cross = desired_x * forward_y - desired_y * forward_x
    return (graph,
            struct.unpack_from("<h", body, ANIM_INDEX_OFF)[0],
            struct.unpack_from("<h", body, ANIM_FRAME_OFF)[0],
            unit_state, turn_dot, turn_cross)


def extract_series(path):
    """samples[slot] rows also carry unit_state and standing-turn dot/cross."""
    _name, _ver, frames = parse_halorec(path)
    samples = {}
    stats = {"frames": len(frames), "with_bodies": 0, "ticks": []}
    for fr in frames:
        regions = {}
        for a, b in fr.regions:
            if a not in regions or len(b) > len(regions[a]):
                regions[a] = b
        read = _reader(regions)
        tick = frame_tick(read)
        if tick is None:
            continue
        stats["ticks"].append(tick)
        obj = _pool(read, OBJECT_TABLE_PTR)
        act = _pool(read, ACTOR_TABLE_PTR)
        if act is None or obj is None:
            continue
        got_any = False
        n = act["cur"] if 0 < act["cur"] <= act["max"] else act["max"]
        for slot in range(n):
            actor_addr = act["data"] + slot * act["es"]
            rec = read(actor_addr,
                       ACTOR_UNIT_HANDLE_OFF + 4)
            if rec is None or struct.unpack_from("<H", rec, 0)[0] == 0:
                continue
            handle = struct.unpack_from("<I", rec, ACTOR_UNIT_HANDLE_OFF)[0]
            if handle == HANDLE_NONE:
                continue
            a = object_anim(read, obj, handle & 0xFFFF, (handle >> 16) & 0xFFFF)
            if a is None:
                continue
            facing_blob = read(actor_addr + ACTOR_CONTROL_FACING_OFF, 12)
            actor_facing = (struct.unpack("<fff", facing_blob) if facing_blob
                            else (None, None, None))
            desired_blob = read(actor_addr + ACTOR_DESIRED_FACING_OFF, 12)
            actor_desired = (struct.unpack("<fff", desired_blob) if desired_blob
                             else (None, None, None))
            base_blob = read(actor_addr + ACTOR_BASE_FACING_OFF, 12)
            actor_base = (struct.unpack("<fff", base_blob) if base_blob
                          else (None, None, None))
            got_any = True
            samples.setdefault(slot, []).append(
                (tick,) + a + actor_facing + actor_desired + actor_base)
        if got_any:
            stats["with_bodies"] += 1
    for s in samples.values():
        s.sort()
    return samples, stats


# ------------------------------------------------------------------ analysis

def summarize_slot(series):
    """Per-slot animation behaviour summary."""
    anims = {}
    regress = 0          # frame went DOWN while the animation index held
    advances = 0
    max_frame = -1
    neg_frame = 0
    prev = None
    states = set()
    for row in series:
        tick, graph, anim, frame, state = row[:5]
        anims.setdefault(anim, {"max": -1, "min": 1 << 30, "n": 0,
                                "states": set()})
        d = anims[anim]
        d["max"] = max(d["max"], frame)
        d["min"] = min(d["min"], frame)
        d["n"] += 1
        if state is not None:
            d["states"].add(state)
        max_frame = max(max_frame, frame)
        if frame < 0:
            neg_frame += 1
        if state is not None:
            states.add(state)
        if prev is not None and prev[1] == anim and prev[0] == graph:
            if frame > prev[2]:
                advances += 1
            elif frame < prev[2]:
                regress += 1          # a wrap, or a genuine reset
        prev = (graph, anim, frame)
    return {
        "samples": len(series),
        "anims": anims,
        "anim_set": sorted(anims),
        "max_frame": max_frame,
        "advances": advances,
        "wraps_or_resets": regress,
        "negative_frames": neg_frame,
        "graphs": sorted({row[1] for row in series}),
        "states": sorted(states),
    }


def print_summary(label, samples, stats):
    ticks = stats["ticks"]
    span = f"{min(ticks)}..{max(ticks)}" if ticks else "n/a"
    print(f"\n=== {label}")
    print(f"  frames={stats['frames']} with-animated-actors={stats['with_bodies']} "
          f"tick span {span}")
    if ticks and len(ticks) > 1:
        gaps = [b - a for a, b in zip(ticks, ticks[1:]) if b > a]
        if gaps:
            print(f"  sampling: median {sorted(gaps)[len(gaps)//2]} ticks, "
                  f"min {min(gaps)}, max {max(gaps)}")
    print(f"  {'slot':>4} {'n':>4} {'anim indices':<24} {'states':<14} {'maxfrm':>6} "
          f"{'adv':>4} {'wrap':>4}  per-anim max frame")
    for slot in sorted(samples):
        s = summarize_slot(samples[slot])
        per = " ".join(f"{a}:{d['max']}/s{sorted(d['states'])}"
                       for a, d in sorted(s["anims"].items()))
        states = str(s["states"]) if s["states"] else "-"
        print(f"  {slot:>4} {s['samples']:>4} {str(s['anim_set']):<24} {states:<14} "
              f"{s['max_frame']:>6} {s['advances']:>4} {s['wraps_or_resets']:>4}  {per}")


def compare(a_samples, b_samples, label_a, label_b, window=6):
    """-> (rows, totals). One row per slot with the per-slot divergence metrics."""
    rows = []
    for slot in sorted(set(a_samples) | set(b_samples)):
        sa = a_samples.get(slot, [])
        sb = b_samples.get(slot, [])
        ra = summarize_slot(sa) if sa else None
        rb = summarize_slot(sb) if sb else None
        if ra is None or rb is None:
            rows.append({"slot": slot, "only_in": label_a if rb is None else label_b,
                         "anim_only_a": [], "anim_only_b": [],
                         "maxfrm_a": ra["max_frame"] if ra else None,
                         "maxfrm_b": rb["max_frame"] if rb else None,
                         "maxfrm_delta": None, "mismatch": None, "compared": 0})
            continue
        # tick-aligned animation-index comparison
        idx_b = {row[0]: (row[1], row[2], row[3], row[4]) for row in sb}
        ticks_b = sorted(idx_b)
        mism = comp = 0
        for row in sa:
            t, an = row[0], row[2]
            near = min(ticks_b, key=lambda x: abs(x - t)) if ticks_b else None
            if near is None or abs(near - t) > window:
                continue
            comp += 1
            if idx_b[near][1] != an:
                mism += 1
        rows.append({
            "slot": slot, "only_in": None,
            "anim_only_a": sorted(set(ra["anim_set"]) - set(rb["anim_set"])),
            "anim_only_b": sorted(set(rb["anim_set"]) - set(ra["anim_set"])),
            "maxfrm_a": ra["max_frame"], "maxfrm_b": rb["max_frame"],
            "maxfrm_delta": rb["max_frame"] - ra["max_frame"],
            "mismatch": mism, "compared": comp,
        })
    totals = {
        "slots": len(rows),
        "slots_only_one_side": sum(1 for r in rows if r["only_in"]),
        "slots_anim_set_differs": sum(1 for r in rows
                                      if r["anim_only_a"] or r["anim_only_b"]),
        "max_maxfrm_delta": max([abs(r["maxfrm_delta"]) for r in rows
                                 if r["maxfrm_delta"] is not None] or [0]),
        "mismatch_rate": (sum(r["mismatch"] or 0 for r in rows) /
                          max(1, sum(r["compared"] or 0 for r in rows))),
    }
    return rows, totals


def print_compare(rows, totals, label_a, label_b):
    print(f"\n--- {label_a}  vs  {label_b}")
    print(f"  {'slot':>4} {'anim only A':<16} {'anim only B':<16} "
          f"{'maxfrmA':>7} {'maxfrmB':>7} {'d':>5} {'animIdx mismatch':>18}")
    for r in rows:
        if r["only_in"]:
            print(f"  {r['slot']:>4} {'(slot only in ' + r['only_in'] + ')':<60}")
            continue
        rate = (f"{r['mismatch']}/{r['compared']}" if r["compared"] else "-")
        flag = ""
        if r["anim_only_a"] or r["anim_only_b"] or abs(r["maxfrm_delta"]) > 8:
            flag = "  <<"
        print(f"  {r['slot']:>4} {str(r['anim_only_a']):<16} {str(r['anim_only_b']):<16} "
              f"{r['maxfrm_a']:>7} {r['maxfrm_b']:>7} {r['maxfrm_delta']:>5} "
              f"{rate:>18}{flag}")
    print(f"  TOTALS slots={totals['slots']} "
          f"one-sided={totals['slots_only_one_side']} "
          f"anim-set-differs={totals['slots_anim_set_differs']} "
          f"max|maxfrm delta|={totals['max_maxfrm_delta']} "
          f"animIdx mismatch rate={totals['mismatch_rate']:.3f}")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("recs", nargs="+", type=Path,
                    help="1 recording to summarize, or 2 to compare (A then B)")
    ap.add_argument("--vs", type=Path, default=None,
                    help="third recording: recs[0]/recs[1] is the A/A noise floor, "
                         "recs[0] vs this is the A/B under test")
    ap.add_argument("--window", type=int, default=6,
                    help="tick tolerance when aligning samples (default 6)")
    ap.add_argument("--slot", type=int, default=None,
                    help="print the raw per-tick series for one actor slot")
    ap.add_argument("--transitions-only", action="store_true",
                    help="with --slot, print only animation/state transitions")
    a = ap.parse_args(argv)

    loaded = [(p, *extract_series(p)) for p in a.recs]
    if a.vs:
        loaded.append((a.vs, *extract_series(a.vs)))
    for p, s, st in loaded:
        print_summary(p.name, s, st)

    if a.slot is not None:
        for p, s, _st in loaded:
            print(f"\n  raw slot {a.slot} in {p.name}: "
                   f"(tick, graph, anim, frame, unit-state, turn-dot, turn-cross, "
                   f"actor-facing)")
            previous = None
            for row in s.get(a.slot, []):
                current = (row[2], row[4])
                if a.transitions_only and current == previous:
                    continue
                previous = current
                state = "-" if row[4] is None else str(row[4])
                turn_dot = "-" if row[5] is None else f"{row[5]:+.6f}"
                turn_cross = "-" if row[6] is None else f"{row[6]:+.6f}"
                facing = ("-" if row[7] is None else
                          f"({row[7]:+.4f},{row[8]:+.4f},{row[9]:+.4f})")
                desired = ("-" if row[10] is None else
                           f"({row[10]:+.4f},{row[11]:+.4f},{row[12]:+.4f})")
                base = ("-" if row[13] is None else
                        f"({row[13]:+.4f},{row[14]:+.4f},{row[15]:+.4f})")
                print(f"    {row[0]:>7} {row[1]:#010x} {row[2]:>4} "
                      f"{row[3]:>5} {state:>4} {turn_dot:>10} {turn_cross:>10} "
                      f"out={facing} desired={desired} base={base}")

    if len(loaded) >= 2:
        aa_rows, aa_tot = compare(loaded[0][1], loaded[1][1],
                                  loaded[0][0].name, loaded[1][0].name, a.window)
        print_compare(aa_rows, aa_tot, loaded[0][0].name, loaded[1][0].name)
    if len(loaded) >= 3:
        ab_rows, ab_tot = compare(loaded[0][1], loaded[2][1],
                                  loaded[0][0].name, loaded[2][0].name, a.window)
        print_compare(ab_rows, ab_tot, loaded[0][0].name, loaded[2][0].name)
        print("\n=== NOISE FLOOR vs SIGNAL")
        for k in ("slots_only_one_side", "slots_anim_set_differs",
                  "max_maxfrm_delta", "mismatch_rate"):
            av, bv = aa_tot[k], ab_tot[k]
            verdict = "SIGNAL" if bv > av else "within noise"
            print(f"  {k:<26} A/A={av!s:<8} A/B={bv!s:<8} {verdict}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
