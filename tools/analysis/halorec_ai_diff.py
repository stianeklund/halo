#!/usr/bin/env python3
"""Parse halo-memory-viewer .halorec recordings and extract a10 door-grunt AI
state time-series for patched-vs-unpatched differential analysis.

.halorec container (see crates/halo-viewer/src/replay.rs): gzip of
  "HMRC" | version:u32 | name_len:u32 | name | frame_count:u32
  then per frame: t:f64 | region_count:u32 | (addr:u32 len:u32 bytes)*

Regions are raw (base_address, bytes). The actor table is reached via
*(data_t**)0x6325a4 (stride 0x724). Field offsets are binary-derived from the
a10 gold-signature note. enc (actor+0x34) == 0xee6e0009 selects the scripted
door squad (a6/a10/a11); other encounters are pre-positioned non-regression
squads and are ignored.

Usage:
  halorec_ai_diff.py dump <file.halorec> [--all-enc] [--stride N]
  halorec_ai_diff.py diff <patched.halorec> <unpatched.halorec>
"""
import argparse
import gzip
import struct
import sys

ACTOR_DATA_PTR = 0x6325A4
ACTOR_STRIDE = 0x724
DOOR_ENC = 0xEE6E0009
NONE = 0xFFFFFFFF

# field offset, label, struct fmt (from gold-signature + decoder)
FIELDS = [
    ("enc",  0x34, "<I"),   # encounter/squad handle (door filter)
    ("unit", 0x18, "<I"),   # controlled unit (biped) handle
    ("st",   0x6A, "<B"),   # combat status 0..6 (0=inactive..3=fight..6=engage)
    ("act",  0x6C, "<H"),   # action (decoder's "action")
    ("agg",  0x6E, "<B"),   # aggression
    ("beh",  0xC0, "<B"),   # behavior
    ("aw",   0x268, "<h"),  # alertness/awareness continuum (word writer)
    ("mode", 0x46C, "<h"),  # move_src (cdb0 input)
    ("fp",   0x470, "<h"),  # firing-position index
    ("atd",  0x484, "<B"),  # at-destination flag
    ("patt", 0x4A4, "<B"),  # path computed this tick
    ("pact", 0x4A8, "<B"),  # path active (cdb0 commit result)
    ("ctgt", 0x610, "<I"),  # combat target handle (!=NONE => acquired)
]
POS_OFF = 0x12C


def parse_halorec(path):
    with open(path, "rb") as f:
        raw = gzip.decompress(f.read())
    pos = 0

    def take(n):
        nonlocal pos
        b = raw[pos:pos + n]
        if len(b) != n:
            raise ValueError("truncated recording")
        pos += n
        return b

    def u32():
        return struct.unpack("<I", take(4))[0]

    def f64():
        return struct.unpack("<d", take(8))[0]

    magic = take(4)
    if magic != b"HMRC":
        raise ValueError("bad magic %r" % magic)
    version = u32()
    name_len = u32()
    name = take(name_len).decode("utf-8", "replace")
    frame_count = u32()
    frames = []
    for _ in range(frame_count):
        t = f64()
        rc = u32()
        regions = []
        for _ in range(rc):
            addr = u32()
            ln = u32()
            regions.append((addr, take(ln)))
        frames.append((t, regions))
    return name, version, frames


import bisect


class FrameReader:
    """Resolve bytes at an absolute address from a frame's regions."""
    def __init__(self, regions):
        # sort by base for covering lookup via bisect
        self.regions = sorted(regions, key=lambda r: r[0])
        self.bases = [b for b, _ in self.regions]

    def read(self, addr, n):
        # rightmost region whose base <= addr
        i = bisect.bisect_right(self.bases, addr) - 1
        if i < 0:
            return None
        base, data = self.regions[i]
        if addr + n <= base + len(data):
            off = addr - base
            return data[off:off + n]
        return None

    def u32(self, addr):
        b = self.read(addr, 4)
        return struct.unpack("<I", b)[0] if b else None


def read_actors(fr, stride=ACTOR_STRIDE):
    ptr = fr.u32(ACTOR_DATA_PTR)
    if not ptr:
        return None, []
    hdr = fr.read(ptr, 0x38)
    if not hdr:
        return ptr, []
    magic = struct.unpack_from("<I", hdr, 0x28)[0]
    max_count = struct.unpack_from("<h", hdr, 0x20)[0]
    elem_size = struct.unpack_from("<h", hdr, 0x22)[0]
    cur_count = struct.unpack_from("<h", hdr, 0x2E)[0]
    data_addr = struct.unpack_from("<I", hdr, 0x34)[0]
    if elem_size <= 0:
        elem_size = stride
    n = cur_count if cur_count > 0 else max_count
    actors = []
    for slot in range(max(n, 0)):
        base = data_addr + slot * elem_size
        elem = fr.read(base, elem_size)
        if not elem:
            continue
        salt = struct.unpack_from("<H", elem, 0)[0]
        if salt == 0:
            continue
        handle = ((salt & 0xFFFF) << 16) | (slot & 0xFFFF)
        rec = {"slot": slot, "handle": handle}
        for label, off, fmt in FIELDS:
            try:
                rec[label] = struct.unpack_from(fmt, elem, off)[0]
            except struct.error:
                rec[label] = None
        try:
            rec["pos"] = struct.unpack_from("<fff", elem, POS_OFF)
        except struct.error:
            rec["pos"] = None
        actors.append(rec)
    return ptr, actors


def fmt_actor(t, a):
    ctgt = a["ctgt"]
    ctgt_s = "NONE" if ctgt == NONE else ("0x%08x" % ctgt if ctgt is not None else "?")
    pos = a["pos"]
    pos_s = "(%.2f,%.2f,%.2f)" % pos if pos else "?"
    return ("t=%6.2f slot=%2d h=0x%08x st=%s act=%s agg=%s beh=%s aw=%3s "
            "mode=%s fp=%s atd=%s patt=%s pact=%s ctgt=%s pos=%s") % (
        t, a["slot"], a["handle"], a["st"], a["act"], a["agg"], a["beh"],
        a["aw"], a["mode"], a["fp"], a["atd"], a["patt"], a["pact"], ctgt_s, pos_s)


def cmd_dump(args):
    name, ver, frames = parse_halorec(args.file)
    print("# %s  v%d  frames=%d" % (name, ver, len(frames)))
    # enc histogram across all frames (which squads are present)
    enc_hist = {}
    for t, regions in frames:
        _, actors = read_actors(FrameReader(regions), args.stride)
        for a in actors:
            enc_hist[a["enc"]] = enc_hist.get(a["enc"], 0) + 1
    print("# enc histogram (actor-frames):")
    for enc, c in sorted(enc_hist.items(), key=lambda kv: -kv[1]):
        mark = " <== DOOR SQUAD" if enc == DOOR_ENC else ""
        print("#   0x%08x : %d%s" % (enc & 0xFFFFFFFF, c, mark))
    print("# per-frame door-squad (enc=0x%08x)%s:" % (
        DOOR_ENC, " [--all-enc: showing ALL]" if args.all_enc else ""))
    for t, regions in frames:
        _, actors = read_actors(FrameReader(regions), args.stride)
        sel = actors if args.all_enc else [a for a in actors if a["enc"] == DOOR_ENC]
        for a in sorted(sel, key=lambda x: x["slot"]):
            print(fmt_actor(t, a))


def per_grunt_series(frames, stride):
    """handle -> list of (t, actorrec) for door-squad grunts."""
    series = {}
    for t, regions in frames:
        _, actors = read_actors(FrameReader(regions), stride)
        for a in actors:
            if a["enc"] != DOOR_ENC:
                continue
            series.setdefault(a["handle"], []).append((t, a))
    return series


def summarize(series):
    """Per-grunt: spawn t, peak aw, peak st, max pact, max mode, ctgt-acquired,
    net displacement."""
    out = []
    for h, seq in sorted(series.items()):
        ts = [t for t, _ in seq]
        aws = [a["aw"] for _, a in seq if a["aw"] is not None]
        sts = [a["st"] for _, a in seq if a["st"] is not None]
        modes = [a["mode"] for _, a in seq if a["mode"] is not None]
        pacts = [a["pact"] for _, a in seq if a["pact"] is not None]
        ctgt_acq = any(a["ctgt"] not in (None, NONE) for _, a in seq)
        p0 = seq[0][1]["pos"]
        pN = seq[-1][1]["pos"]
        disp = None
        if p0 and pN:
            disp = ((pN[0]-p0[0])**2 + (pN[1]-p0[1])**2 + (pN[2]-p0[2])**2) ** 0.5
        out.append({
            "h": h, "n": len(seq), "t0": ts[0], "t1": ts[-1],
            "peak_aw": max(aws) if aws else None,
            "end_aw": aws[-1] if aws else None,
            "peak_st": max(sts) if sts else None,
            "max_mode": max(modes) if modes else None,
            "max_pact": max(pacts) if pacts else None,
            "ctgt": ctgt_acq, "disp": disp,
        })
    return out


def cmd_diff(args):
    for tag, path in (("PATCHED", args.patched), ("UNPATCHED", args.unpatched)):
        name, ver, frames = parse_halorec(path)
        series = per_grunt_series(frames, args.stride)
        print("=" * 78)
        print("%s  %s  frames=%d  door-grunts=%d" % (tag, name, len(frames), len(series)))
        print("-" * 78)
        for s in summarize(series):
            print("  h=0x%08x n=%3d t=[%.1f,%.1f] peak_aw=%s end_aw=%s peak_st=%s "
                  "max_mode=%s max_pact=%s ctgt=%s disp=%s" % (
                s["h"], s["n"], s["t0"], s["t1"], s["peak_aw"], s["end_aw"],
                s["peak_st"], s["max_mode"], s["max_pact"], s["ctgt"],
                ("%.2f" % s["disp"]) if s["disp"] is not None else "?"))


# ---------------------------------------------------------------------------
# localize: first-divergent (frame, field) between two recordings
#
# This is the load-bearing a10 use of the *time series*: record the same
# scenario on two builds (e.g. unpatched-shooting vs patched-not-shooting), then
# walk each scripted door grunt's fields forward in time and report the FIRST
# (relative-time, field) where the two runs disagree. That onset localizes a
# behavioural regression in TIME -> the field that first diverged names the
# subsystem (awareness, firing-position, path-commit, ...) that broke.
#
# Two builds are NOT wall-clock synchronized and assign different datum salts to
# the same scripted actor, so we (advisor):
#   * anchor each recording at squad-spawn (first frame the door enc appears) and
#     compare on anchor-relative time, not absolute t;
#   * match grunts by datum SLOT (handle & 0xFFFF), not full handle.
# ---------------------------------------------------------------------------

def per_slot_series(frames, stride, enc=DOOR_ENC):
    """slot -> list of (t, actorrec) for the scripted squad, ordered by t."""
    series = {}
    for t, regions in frames:
        _, actors = read_actors(FrameReader(regions), stride)
        for a in actors:
            if enc is not None and a["enc"] != enc:
                continue
            series.setdefault(a["slot"], []).append((t, a))
    return series


def _anchor_t(frames, stride, enc=DOOR_ENC):
    """t of the first frame the scripted squad is present (else first frame t)."""
    for t, regions in frames:
        _, actors = read_actors(FrameReader(regions), stride)
        if any(enc is None or a["enc"] == enc for a in actors):
            return t
    return frames[0][0] if frames else 0.0


def _nearest(samples, rt):
    """(reltime, rec) in samples nearest reltime rt, plus the |dt| gap."""
    best, bdt = None, None
    for srt, rec in samples:
        dt = abs(srt - rt)
        if bdt is None or dt < bdt:
            best, bdt = (srt, rec), dt
    return best, bdt


def localize_divergence(framesA, framesB, stride, fields=None,
                        enc=DOOR_ENC, max_dt=None):
    """First-divergent (slot, field) onsets between two recordings.

    Returns a list of {slot, field, relt, a, b, dt} dicts, one per (slot, field)
    that ever diverges, at the earliest anchor-relative time it does. `max_dt`
    caps how far apart two samples may be and still be compared (default: 60%% of
    A's median sample gap), so cadence mismatch doesn't manufacture divergences.
    """
    flabels = [f[0] for f in FIELDS] if fields is None else fields
    aA, aB = _anchor_t(framesA, stride, enc), _anchor_t(framesB, stride, enc)
    sA = per_slot_series(framesA, stride, enc)
    sB = per_slot_series(framesB, stride, enc)

    if max_dt is None:
        gaps = []
        for samples in sA.values():
            ts = sorted(t for t, _ in samples)
            gaps += [b - a for a, b in zip(ts, ts[1:])]
        gaps.sort()
        med = gaps[len(gaps) // 2] if gaps else 0.0
        max_dt = 0.6 * med if med else 1e9

    onsets = []
    for slot in sorted(set(sA) & set(sB)):
        a_samples = sorted(((t - aA, rec) for t, rec in sA[slot]))
        b_samples = sorted(((t - aB, rec) for t, rec in sB[slot])) or None
        if not b_samples:
            continue
        first = {}  # field -> onset dict (earliest)
        for rt, arec in a_samples:
            (brt, brec), dt = _nearest(b_samples, rt)
            if dt > max_dt:
                continue
            for fld in flabels:
                if fld in first:
                    continue
                av, bv = arec.get(fld), brec.get(fld)
                if av != bv:
                    first[fld] = {"slot": slot, "field": fld, "relt": rt,
                                  "a": av, "b": bv, "dt": dt}
        onsets.extend(first.values())
    onsets.sort(key=lambda o: (o["relt"], o["slot"], o["field"]))
    return onsets


def _fmt_val(v):
    if v is None:
        return "?"
    if isinstance(v, int) and v > 0x10000:
        return "0x%08x" % (v & 0xFFFFFFFF)
    return str(v)


def cmd_localize(args):
    nameA, _, framesA = parse_halorec(args.a)
    nameB, _, framesB = parse_halorec(args.b)
    enc = None if args.all_enc else args.enc
    fields = args.fields.split(",") if args.fields else None
    onsets = localize_divergence(framesA, framesB, args.stride, fields, enc)
    print("=" * 78)
    print("A: %s (%d frames)   B: %s (%d frames)   enc=%s" % (
        nameA, len(framesA), nameB, len(framesB),
        "ALL" if enc is None else "0x%08x" % enc))
    print("anchor-relative; grunts matched by slot; %d divergent (slot,field)" %
          len(onsets))
    print("-" * 78)
    if not onsets:
        print("  no field divergence (recordings agree on every scripted grunt)")
        return
    for o in onsets:
        print("  relt=%6.2fs slot=%2d  %-5s  A=%-12s B=%-12s  (|dt|=%.2fs)" % (
            o["relt"], o["slot"], o["field"],
            _fmt_val(o["a"]), _fmt_val(o["b"]), o["dt"]))
    # earliest onset overall = the field to investigate first
    e = onsets[0]
    print("-" * 78)
    print("FIRST DIVERGENCE: field '%s' on slot %d at relt=%.2fs (A=%s B=%s)" % (
        e["field"], e["slot"], e["relt"], _fmt_val(e["a"]), _fmt_val(e["b"])))


def _perturb_actor_field(frames, frame_idx, slot, field_off, fmt, value, stride):
    """Return a deep copy of `frames` with one actor field overwritten in-memory.

    Used only by selftest to manufacture a known divergence onset.
    """
    import copy
    frames = copy.deepcopy(frames)
    t, regions = frames[frame_idx]
    fr = FrameReader(regions)
    ptr = fr.u32(ACTOR_DATA_PTR)
    hdr = fr.read(ptr, 0x38)
    data_addr = struct.unpack_from("<I", hdr, 0x34)[0]
    elem_size = struct.unpack_from("<h", hdr, 0x22)[0] or stride
    abs_addr = data_addr + slot * elem_size + field_off
    new_regions = []
    for base, data in regions:
        if base <= abs_addr < base + len(data) - (struct.calcsize(fmt) - 1):
            buf = bytearray(data)
            struct.pack_into(fmt, buf, abs_addr - base, value)
            new_regions.append((base, bytes(buf)))
        else:
            new_regions.append((base, data))
    frames[frame_idx] = (t, new_regions)
    return frames


def cmd_selftest(args):
    """Validate the detector: self-diff => 0 onsets; one perturbation => 1 onset."""
    name, _, frames = parse_halorec(args.file)
    # 1. self-diff must be empty (alignment + compare plumbing)
    onsets = localize_divergence(frames, frames, args.stride)
    assert onsets == [], "self-diff produced %d false onset(s): %r" % (
        len(onsets), onsets[:3])
    print("[selftest] self-diff: 0 onsets  OK")

    # 2. perturb one field for one door-squad slot at the last frame it is alive,
    #    expect exactly that (slot, field) to surface as an onset.
    slots = per_slot_series(frames, args.stride)
    assert slots, "no door-squad grunts in %s (try --stride/--enc)" % args.file
    slot = sorted(slots)[0]
    # frame index of this slot's last sample
    last_t = slots[slot][-1][0]
    fidx = max(i for i, (t, _) in enumerate(frames) if t == last_t)
    orig = slots[slot][-1][1]["aw"]
    pert = (orig or 0) ^ 0x7  # guaranteed different, fits the <h aw writer
    framesB = _perturb_actor_field(frames, fidx, slot, 0x268, "<h", pert,
                                   args.stride)
    onsets = localize_divergence(frames, framesB, args.stride)
    aw_onsets = [o for o in onsets if o["slot"] == slot and o["field"] == "aw"]
    assert len(aw_onsets) == 1, "expected 1 aw onset on slot %d, got %r" % (
        slot, onsets)
    print("[selftest] perturb slot %d aw %s->%s at frame %d: onset relt=%.2fs  OK"
          % (slot, _fmt_val(orig), _fmt_val(pert), fidx, aw_onsets[0]["relt"]))
    print("[selftest] PASS")


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("dump")
    d.add_argument("file")
    d.add_argument("--all-enc", action="store_true")
    d.add_argument("--stride", type=lambda x: int(x, 0), default=ACTOR_STRIDE)
    d.set_defaults(func=cmd_dump)
    f = sub.add_parser("diff")
    f.add_argument("patched")
    f.add_argument("unpatched")
    f.add_argument("--stride", type=lambda x: int(x, 0), default=ACTOR_STRIDE)
    f.set_defaults(func=cmd_diff)
    lo = sub.add_parser("localize", help="first-divergent (frame,field) between "
                                         "two recordings of the same scenario")
    lo.add_argument("a", help="recording A (e.g. unpatched)")
    lo.add_argument("b", help="recording B (e.g. patched)")
    lo.add_argument("--stride", type=lambda x: int(x, 0), default=ACTOR_STRIDE)
    lo.add_argument("--enc", type=lambda x: int(x, 0), default=DOOR_ENC,
                    help="scripted-squad encounter handle (default a10 door squad)")
    lo.add_argument("--all-enc", action="store_true", help="compare every squad")
    lo.add_argument("--fields", default=None,
                    help="comma list of fields to diff (default: all)")
    lo.set_defaults(func=cmd_localize)
    st = sub.add_parser("selftest", help="validate the localize detector")
    st.add_argument("file", help="any recording with the scripted door squad")
    st.add_argument("--stride", type=lambda x: int(x, 0), default=ACTOR_STRIDE)
    st.set_defaults(func=cmd_selftest)
    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
