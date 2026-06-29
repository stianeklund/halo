"""Convert a Halo Memory Viewer `.halorec` recording into a Unicorn
state-snapshot JSON (the `{"regions": {"0xADDR": "hex"}}` format consumed by
`state_snapshot.load_snapshot` / `unicorn_diff.py --state-snapshot`).

A `.halorec` is a gzipped `HMRC` container (see
`halo-memory-viewer/crates/halo-viewer/src/replay.rs`):

    magic "HMRC" | version u32=1 | name(u32 len + utf8) | frame_count u32
    per frame:   t f64 | region_count u32
                 per region: addr u32 | len u32 | bytes[len]

Region addresses are *guest virtual addresses* — exactly the address space the
Unicorn `--state-snapshot` path already maps (game-state heap at 0x80xxxxxx plus
XBE .data globals). So one recorded frame == one snapshot, byte for byte.

The viewer captures the full *used* (current_count) span of the standard data_t
pools, so a frame carries real object / actor / (in newer recordings) perception
-prop records — a deterministic, multi-frame oracle with no running emulator.

Usage:
    python3 halorec_to_snapshot.py REC.halorec -o snap.json            # last frame
    python3 halorec_to_snapshot.py REC.halorec --frame 40 -o snap.json
    python3 halorec_to_snapshot.py REC.halorec --t 12.5 -o snap.json
    python3 halorec_to_snapshot.py REC.halorec --handle 0xe38b0006 -o snap.json
    python3 halorec_to_snapshot.py REC.halorec --list-frames
"""

import argparse
import gzip
import json
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path

FORMAT_MAGIC = b"HMRC"
FORMAT_VERSION = 1

# data_t pool pointers (guest VAs). Header layout (from the lift repo / viewer):
#   +0x20 max_count i16 | +0x22 element_size i16 | +0x28 magic u32
#   +0x2E current_count i16 | +0x34 data_address u32
OBJECT_TABLE_PTR = 0x5A8D50
PLAYER_TABLE_PTR = 0x5AA6D4
ACTOR_TABLE_PTR = 0x6325A4
PROP_POOL_PTR = 0x5AB23C
DATA_T_MAGIC = 0x64407440


class Frame:
    __slots__ = ("t", "regions")

    def __init__(self, t, regions):
        self.t = t
        self.regions = regions  # list[(addr:int, bytes)]


def parse_halorec(path):
    """Return (name:str, version:int, frames:list[Frame]). Raises on bad data."""
    raw = gzip.open(path, "rb").read()
    pos = 0

    def take(n):
        nonlocal pos
        if pos + n > len(raw):
            raise ValueError("truncated recording data")
        b = raw[pos:pos + n]
        pos += n
        return b

    def u32():
        return struct.unpack("<I", take(4))[0]

    def f64():
        return struct.unpack("<d", take(8))[0]

    magic = take(4)
    if magic != FORMAT_MAGIC:
        raise ValueError(f"not a Halo recording (bad magic {magic!r})")
    version = u32()
    if version != FORMAT_VERSION:
        raise ValueError(f"unsupported recording version {version}")
    nlen = u32()
    name = take(nlen).decode("utf-8", "replace")
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
        frames.append(Frame(t, regions))
    return name, version, frames


def _reader(regions):
    """Build read(addr, n) -> bytes|None over a frame's regions (fully-covered only)."""
    iv = sorted(regions)

    def read(addr, n):
        for a, b in iv:
            if a <= addr and addr + n <= a + len(b):
                return b[addr - a:addr - a + n]
        return None

    return read


def _pool_header(read, ptr):
    """Resolve a data_t pool pointer to its header fields, or None if not captured.

    Returns dict {hdr, max, es, magic, cur, data} or None.
    """
    pv = read(ptr, 4)
    if pv is None:
        return None
    hdr = struct.unpack("<I", pv)[0]
    if hdr == 0:
        return None
    h = read(hdr, 0x38)
    if h is None:
        return None
    max_c, es = struct.unpack_from("<hh", h, 0x20)
    magic = struct.unpack_from("<I", h, 0x28)[0]
    cur = struct.unpack_from("<h", h, 0x2E)[0]
    data = struct.unpack_from("<I", h, 0x34)[0]
    return {"hdr": hdr, "max": max_c, "es": es, "magic": magic, "cur": cur, "data": data}


def _pool_used_coverage(regions, read, ptr):
    """(% of used span captured, used_bytes, header) for a pool, or (None, 0, None)."""
    hd = _pool_header(read, ptr)
    if hd is None or not (0 < hd["cur"] < 5000) or hd["es"] <= 0:
        return None, 0, hd
    used = hd["cur"] * hd["es"]
    lo, hi = hd["data"], hd["data"] + used
    cov = 0
    for a, b in sorted(regions):
        s = max(lo, a)
        e = min(hi, a + len(b))
        if e > s:
            cov += e - s
    return (100.0 * cov / used if used else 0.0), used, hd


def frame_handle_alive(regions, handle):
    """True if `handle`'s datum index slot carries a matching salt in any pool."""
    read = _reader(regions)
    salt = (handle >> 16) & 0xFFFF
    index = handle & 0xFFFF
    for ptr in (OBJECT_TABLE_PTR, ACTOR_TABLE_PTR, PLAYER_TABLE_PTR):
        hd = _pool_header(read, ptr)
        if hd is None or hd["es"] <= 0 or not (0 < hd["max"] <= 5000):
            continue
        if index >= hd["max"]:
            continue
        rec = read(hd["data"] + index * hd["es"], 2)
        if rec is not None and struct.unpack("<H", rec)[0] == salt:
            return True
    return False


def select_index(frames, frame=None, t=None, handle=None):
    """Resolve a frame selector to an index. Precedence: handle > t > frame > last."""
    n = len(frames)
    if n == 0:
        raise ValueError("recording has no frames")

    if handle is not None:
        # Latest frame where the handle is alive (squad present, steady state).
        alive = [i for i in range(n) if frame_handle_alive(frames[i].regions, handle)]
        if not alive:
            raise ValueError(f"handle {handle:#010x} not alive in any frame")
        return alive[-1]

    if t is not None:
        return min(range(n), key=lambda i: abs(frames[i].t - t))

    if frame is None or frame == "last":
        return n - 1
    if frame == "first":
        return 0
    # integer index (supports negative) or 0.0-1.0 fraction
    if isinstance(frame, str) and ("." in frame):
        frac = float(frame)
        return max(0, min(n - 1, int(round(frac * (n - 1)))))
    idx = int(frame)
    if idx < 0:
        idx += n
    return max(0, min(n - 1, idx))


def frame_regions_dict(frame):
    """{int_addr: bytes} for a frame; coalesces any duplicate addr (keep longest)."""
    out = {}
    for addr, b in frame.regions:
        prev = out.get(addr)
        if prev is None or len(b) > len(prev):
            out[addr] = b
    return out


def build_snapshot(path, frame=None, t=None, handle=None, args=None,
                   description=None, build_label=None):
    """Parse a recording, select a frame, return a load_snapshot-compatible dict."""
    name, _ver, frames = parse_halorec(path)
    idx = select_index(frames, frame=frame, t=t, handle=handle)
    fr = frames[idx]
    regions = frame_regions_dict(fr)

    desc = description if description is not None else (
        f"{name} (frame {idx}/{len(frames) - 1}, t={fr.t:.3f}s) "
        f"from {Path(path).name}"
    )
    out = {
        "description": desc,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "regions": {f"0x{a:08x}": regions[a].hex() for a in sorted(regions)},
    }
    if build_label:
        out["build_label"] = build_label
    if args:
        out["arg_overrides"] = dict(args)
    return out, idx, fr.t


def _parse_arg_value(v):
    try:
        return int(v, 0)
    except ValueError:
        return v


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("infile", help="path to a .halorec recording")
    ap.add_argument("-o", "--output", type=Path, default=None,
                    help="output snapshot JSON (default: stdout)")
    sel = ap.add_mutually_exclusive_group()
    sel.add_argument("--frame", default=None,
                     help="frame index (int, negatives ok), 'first', 'last', or 0.0-1.0 fraction")
    sel.add_argument("--t", type=float, default=None,
                     help="select frame nearest this timestamp (seconds)")
    sel.add_argument("--handle", type=lambda x: int(x, 0), default=None,
                     help="select latest frame where this datum handle is alive")
    ap.add_argument("--arg", nargs=2, action="append", metavar=("NAME", "VALUE"),
                    default=None, help="pin an arg override (repeatable); VALUE parsed as int if possible")
    ap.add_argument("--build-label", default=None, help="build_label field for the snapshot")
    ap.add_argument("--description", default=None, help="override the snapshot description")
    ap.add_argument("--list-frames", action="store_true",
                    help="print frame inventory + per-pool coverage and exit")
    a = ap.parse_args(argv)

    if a.list_frames:
        name, ver, frames = parse_halorec(a.infile)
        print(f"{Path(a.infile).name}: name={name!r} version={ver} frames={len(frames)}")
        if frames:
            print(f"  t range: {frames[0].t:.3f}s .. {frames[-1].t:.3f}s")
            mid = frames[len(frames) // 2]
            read = _reader(mid.regions)
            for label, ptr in (("objects", OBJECT_TABLE_PTR), ("actors", ACTOR_TABLE_PTR),
                               ("players", PLAYER_TABLE_PTR), ("props", PROP_POOL_PTR)):
                pct, used, hd = _pool_used_coverage(mid.regions, read, ptr)
                if hd is None:
                    print(f"  {label:8s}: ptr {ptr:#x} not captured")
                elif pct is None:
                    print(f"  {label:8s}: hdr@{hd['hdr']:#x} es={hd['es']:#x} cur={hd['cur']} (no used span)")
                else:
                    print(f"  {label:8s}: es={hd['es']:#x} cur={hd['cur']} magic={hd['magic']:#x} "
                          f"data@{hd['data']:#x} used={used}B cov={pct:.0f}%")
        return 0

    args = {k: _parse_arg_value(v) for k, v in a.arg} if a.arg else None
    snap, idx, t = build_snapshot(
        a.infile, frame=a.frame, t=a.t, handle=a.handle, args=args,
        description=a.description, build_label=a.build_label,
    )
    text = json.dumps(snap, indent=2) + "\n"
    if a.output:
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(text, encoding="utf-8")
        print(f"  [halorec] frame {idx} (t={t:.3f}s) -> {len(snap['regions'])} regions "
              f"-> {a.output}", file=sys.stderr)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
