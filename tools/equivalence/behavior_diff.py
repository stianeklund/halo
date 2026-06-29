#!/usr/bin/env python3
"""Tolerant behavioral diff of two `.halorec` trajectories — the A/B regression
oracle (docs/ab-trajectory-testing.md).

Unlike trajectory_diff (strict byte-equality at the exact tick — the right tool
only for the A/A determinism gate), this asks the question that actually matters
for a lift: *does the patched build show the SAME BEHAVIOR around the SAME TIME as
the faithful build?* It is robust to the two things that are expected and benign:

  * 1-ULP x87 float drift (clang vs MSVC) that compounds over time, and
  * sub-frame capture-phase jitter (QMP pauses mid-frame, so per-tick counters
    and positions wobble by a tick) — the noise that tripped the byte-diff.

How:
  * align by NEAREST tick within +/-W (not exact); absolute tick (core boots reset
    the counter to the core's saved tick, so two boots share a timeline, and W
    absorbs anchor/cadence/phase jitter);
  * match entities by datum SLOT, not handle (salts differ across builds);
  * DISCRETE fields (AI state, awareness, flags) must agree; a divergence is
    reported only when it is SUSTAINED (>= min-run consecutive samples), so a
    one-sample blip from phase/FP is ignored;
  * CONTINUOUS fields (position) compared at a value tolerance (eps); a sustained
    excursion beyond eps is the signal;
  * HANDLE fields compared by LIVENESS (NONE vs not), never by raw value;
  * AGGREGATE invariants (alive count, in-combat count) compared as scalar series.

The earliest sustained onset names the time + field + entity to investigate.
Clean (no onsets) means the builds are behaviorally equivalent on the watch-list.

    python3 behavior_diff.py FAITHFUL.halorec PATCHED.halorec [--window 4] [--config cfg.json]
"""
import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import halorec_to_snapshot as h2s   # noqa: E402
import trajectory_diff as td        # noqa: E402  (frame_tick, POOLS)

HANDLE_NONE = 0xFFFFFFFF

# Default watch-list. Offsets from the a10 gold-signature (halorec_ai_diff.FIELDS);
# the volatile per-tick counters (+0x4a, +0x7c) are deliberately NOT listed.
#   kind: discrete | continuous | vec | handle_live
DEFAULT_CONFIG = {
    "window": 4,            # +/- tick tolerance for nearest-sample matching
    "min_run": 2,           # consecutive mismatched samples before reporting (sustained)
    "pools": {
        "actors": {
            "fields": [
                {"label": "combat_state", "off": 0x6A, "fmt": "<B", "kind": "discrete"},
                {"label": "action",       "off": 0x6C, "fmt": "<H", "kind": "discrete"},
                {"label": "aggression",   "off": 0x6E, "fmt": "<B", "kind": "discrete"},
                {"label": "behavior",     "off": 0xC0, "fmt": "<B", "kind": "discrete"},
                {"label": "awareness",    "off": 0x268, "fmt": "<h", "kind": "discrete"},
                {"label": "move_mode",    "off": 0x46C, "fmt": "<h", "kind": "discrete"},
                {"label": "firing_pos",   "off": 0x470, "fmt": "<h", "kind": "discrete"},
                {"label": "at_dest",      "off": 0x484, "fmt": "<B", "kind": "discrete"},
                {"label": "path_active",  "off": 0x4A8, "fmt": "<B", "kind": "discrete"},
                {"label": "unit",         "off": 0x18, "fmt": "<I", "kind": "handle_live"},
                {"label": "combat_tgt",   "off": 0x610, "fmt": "<I", "kind": "handle_live"},
                {"label": "pos",          "off": 0x12C, "fmt": "<fff", "kind": "vec", "eps": 0.5},
            ],
        },
    },
    "aggregates": [
        {"label": "actors_alive", "pool": "actors", "reduce": "count"},
        {"label": "actors_in_combat", "pool": "actors", "off": 0x6A, "fmt": "<B",
         "reduce": "count_ge", "value": 3},
        {"label": "objects_alive", "pool": "objects", "reduce": "count"},
    ],
}


def _pool_records(reader, ptr):
    """[(slot, salt, rec_bytes)] for used, salt-nonzero slots; or []."""
    hd = h2s._pool_header(reader, ptr)
    if hd is None or hd["magic"] != h2s.DATA_T_MAGIC or hd["es"] <= 0:
        return []
    n = hd["cur"] if 0 < hd["cur"] <= hd["max"] else hd["max"]
    if not (0 < n <= 5000):
        return []
    out = []
    for slot in range(n):
        rec = reader(hd["data"] + slot * hd["es"], hd["es"])
        if rec is None:
            continue
        salt = struct.unpack_from("<H", rec, 0)[0]
        if salt == 0:
            continue
        out.append((slot, salt, rec))
    return out


def _field_value(rec, f):
    try:
        v = struct.unpack_from(f["fmt"], rec, f["off"])
    except struct.error:
        return None
    kind = f["kind"]
    if kind == "vec":
        return tuple(v)
    val = v[0]
    if kind == "handle_live":
        return val != HANDLE_NONE
    return val


def _values_match(f, a, b):
    if a is None or b is None:
        return a == b
    kind = f["kind"]
    if kind == "continuous":
        return abs(a - b) <= f.get("eps", 0.0)
    if kind == "vec":
        d2 = sum((x - y) ** 2 for x, y in zip(a, b))
        return d2 <= f.get("eps", 0.0) ** 2
    return a == b


def slot_series(frames, ptr, fields):
    """{slot: [(tick, {label: value})]} sorted by tick."""
    series = {}
    for fr in frames:
        rd = h2s._reader(fr.regions)
        tk = td.frame_tick(rd)
        if tk is None:
            continue
        for slot, _salt, rec in _pool_records(rd, ptr):
            vals = {f["label"]: _field_value(rec, f) for f in fields}
            series.setdefault(slot, []).append((tk, vals))
    for s in series.values():
        s.sort()
    return series


def aggregate_series(frames, agg):
    """[(tick, scalar)] for one aggregate spec, sorted by tick."""
    ptr = td.POOLS[agg["pool"]]
    out = []
    for fr in frames:
        rd = h2s._reader(fr.regions)
        tk = td.frame_tick(rd)
        if tk is None:
            continue
        recs = _pool_records(rd, ptr)
        if agg["reduce"] == "count":
            val = len(recs)
        elif agg["reduce"] == "count_ge":
            thr = agg["value"]
            val = sum(1 for _s, _salt, rec in recs
                      if (lambda x: x is not None and x >= thr)(
                          _field_value(rec, {"off": agg["off"], "fmt": agg["fmt"],
                                             "kind": "discrete"})))
        else:
            val = len(recs)
        out.append((tk, val))
    out.sort()
    return out


def _nearest(samples, t, window):
    """(tick, value) in samples nearest t within +/-window, or None."""
    best, bdt = None, None
    for st, v in samples:
        dt = abs(st - t)
        if dt <= window and (bdt is None or dt < bdt):
            best, bdt = (st, v), dt
    return best


def _sustained_onset(a_samples, b_samples, match_fn, window, min_run):
    """First tick where a/b disagree for >= min_run consecutive comparable A-samples.

    Returns (onset_tick, a_val, b_val, dt) or None. Reconvergence resets the run.
    """
    run = 0
    run_start = None
    for t, av in a_samples:
        nb = _nearest(b_samples, t, window)
        if nb is None:
            run = 0
            run_start = None
            continue
        bt, bv = nb
        if match_fn(av, bv):
            run = 0
            run_start = None
        else:
            if run == 0:
                run_start = (t, av, bv, abs(bt - t))
            run += 1
            if run >= min_run:
                return run_start
    return None


def diff_behavior(framesA, framesB, config=None):
    """Tolerant behavioral diff. Returns a structured result with onsets."""
    cfg = config or DEFAULT_CONFIG
    window = cfg.get("window", 4)
    min_run = cfg.get("min_run", 2)
    onsets = []

    for pool_name, pcfg in cfg.get("pools", {}).items():
        ptr = td.POOLS[pool_name]
        fields = pcfg["fields"]
        sA = slot_series(framesA, ptr, fields)
        sB = slot_series(framesB, ptr, fields)
        for slot in sorted(set(sA) & set(sB)):
            for f in fields:
                aseq = [(t, v[f["label"]]) for t, v in sA[slot]]
                bseq = [(t, v[f["label"]]) for t, v in sB[slot]]
                hit = _sustained_onset(
                    aseq, bseq, lambda a, b, _f=f: _values_match(_f, a, b),
                    window, min_run)
                if hit:
                    t, av, bv, dt = hit
                    onsets.append({"scope": pool_name, "slot": slot,
                                   "field": f["label"], "tick": t,
                                   "a": av, "b": bv, "dt": dt})

    for agg in cfg.get("aggregates", []):
        aA = aggregate_series(framesA, agg)
        aB = aggregate_series(framesB, agg)
        tol = agg.get("tol", 0)
        hit = _sustained_onset(
            aA, aB, lambda a, b, _t=tol: a is not None and b is not None and abs(a - b) <= _t,
            window, min_run)
        if hit:
            t, av, bv, dt = hit
            onsets.append({"scope": "aggregate", "slot": None,
                           "field": agg["label"], "tick": t,
                           "a": av, "b": bv, "dt": dt})

    onsets.sort(key=lambda o: (o["tick"], o["scope"], o["slot"] or 0, o["field"]))
    return {"window": window, "min_run": min_run,
            "framesA": len(framesA), "framesB": len(framesB),
            "onset_count": len(onsets), "onsets": onsets}


def _fmt(v):
    if isinstance(v, tuple):
        return "(" + ",".join(f"{x:.2f}" for x in v) + ")"
    if isinstance(v, bool):
        return "live" if v else "NONE"
    if isinstance(v, int) and v > 0x10000:
        return f"0x{v & 0xFFFFFFFF:08x}"
    return str(v)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("faithful", help="recording A (cachebeta golden / faithful)")
    ap.add_argument("patched", help="recording B (default.xbe / candidate)")
    ap.add_argument("--window", type=int, default=None,
                    help="tick tolerance for nearest-sample matching (default 4)")
    ap.add_argument("--min-run", type=int, default=None,
                    help="consecutive mismatched samples before reporting (default 2)")
    ap.add_argument("--config", type=Path, default=None,
                    help="watch-list JSON (default: built-in a10 AI watch-list)")
    ap.add_argument("--json", type=Path, default=None)
    a = ap.parse_args(argv)

    cfg = json.loads(a.config.read_text()) if a.config else dict(DEFAULT_CONFIG)
    if a.window is not None:
        cfg["window"] = a.window
    if a.min_run is not None:
        cfg["min_run"] = a.min_run

    _, _, framesA = h2s.parse_halorec(a.faithful)
    _, _, framesB = h2s.parse_halorec(a.patched)
    res = diff_behavior(framesA, framesB, cfg)

    print("=" * 78)
    print(f"FAITHFUL: {Path(a.faithful).name} ({res['framesA']} frames)")
    print(f"PATCHED:  {Path(a.patched).name} ({res['framesB']} frames)")
    print(f"window=+/-{res['window']} ticks  min_run={res['min_run']}  "
          f"sustained divergences: {res['onset_count']}")
    print("-" * 78)
    if not res["onsets"]:
        print("CLEAN: behaviorally equivalent on the watch-list (no sustained divergence).")
    else:
        for o in res["onsets"]:
            slot = "" if o["slot"] is None else f"slot={o['slot']:<3d} "
            print(f"  tick={o['tick']:<6d} {o['scope']:9s} {slot}{o['field']:14s} "
                  f"faithful={_fmt(o['a'])}  patched={_fmt(o['b'])}  (|dt|={o['dt']})")
        e = res["onsets"][0]
        print("-" * 78)
        print(f"FIRST SUSTAINED DIVERGENCE: '{e['field']}' "
              + (f"on {e['scope']} slot {e['slot']}" if e["slot"] is not None else f"({e['scope']})")
              + f" at tick {e['tick']} (faithful={_fmt(e['a'])} patched={_fmt(e['b'])})")

    if a.json:
        a.json.write_text(json.dumps(res, indent=2) + "\n")
        print(f"  [json] -> {a.json}")
    return 0 if res["onset_count"] == 0 else 3


if __name__ == "__main__":
    sys.exit(main())
