#!/usr/bin/env python3
"""Probe live unit mouth-aperture (object + 0x298) over QMP `memsave`.

Lip-sync writes object+0x298 only through sound_object_apply_pitch_delta
(0x1ac2f0), reached only when sound_update_music's callback-identity test
matches.  The per-tick decay in FUN_001b3690 can only subtract.  So a
non-zero +0x298 on any unit is direct proof the lip-sync path executes;
all-zero across a talking scene is proof it does not.  See
docs/lift-learnings.md §54.

The aperture is a fast waveform (it tracks the permutation's mouth-data byte
per tick and decays between phonemes), so a sparse sampler reports an
essentially random point on it.  Sampling is therefore done with ONE bulk
`memsave` over the object pool per sample instead of one read per unit --
~4 reads/sample instead of ~85, which is the difference between 0.3 Hz and
~10 Hz and the difference between a meaningless peak and a converged one.

Usage:
    python3 tools/xbox/probe_mouth_aperture.py --seconds 20
    python3 tools/xbox/probe_mouth_aperture.py --samples 40 --interval 0.1
"""
import argparse
import json
import re
import socket
import struct
import sys
import time
from pathlib import Path

OBJECT_HEADER_TABLE_PTR = 0x5A8D50
DATUM_MAGIC = 0x64407440
TYPE_BIPED = 0
TYPE_VEHICLE = 1
MOUTH_APERTURE_OFF = 0x298
OBJECT_TYPE_OFF = 0x64          # int16 type, cross-checks the header byte
WINDOW_GAP = 0x10000            # merge unit objects closer than this
MAX_WINDOW = 0x40000            # never allocate more than 256 KB at a time
OUT_DIR = Path("/mnt/g/dev/halo/artifacts/mouth_probe")


def win_path(p: Path) -> str:
    m = re.match(r'/mnt/([a-zA-Z])/(.*)', str(p))
    return f"{m.group(1).upper()}:\\" + m.group(2).replace("/", "\\") if m else str(p)


class Qmp:
    """Minimal QMP client. One long-lived session; memsave is virtual."""

    def __init__(self, host="127.0.0.1", port=4444):
        self.s = socket.create_connection((host, port), timeout=15)
        self.f = self.s.makefile("rwb")
        self._readline()
        self._cmd({"execute": "qmp_capabilities"})

    def _readline(self):
        line = self.f.readline()
        if not line:
            raise RuntimeError("QMP connection closed")
        return json.loads(line)

    def _cmd(self, obj):
        self.f.write((json.dumps(obj) + "\r\n").encode())
        self.f.flush()
        while True:
            msg = self._readline()
            if "return" in msg or "error" in msg:
                return msg

    def hmp(self, line):
        return self._cmd({"execute": "human-monitor-command",
                          "arguments": {"command-line": line}})

    def read(self, vaddr, size, tag="r"):
        """memsave [vaddr, vaddr+size) and return exactly `size` bytes.

        Read in chunks into a preallocated buffer: this box routinely runs
        with <200 MB free (a telemetry daemon holds several GB), and
        Path.read_bytes() on a multi-hundred-KB dump was enough to raise
        ENOMEM mid-run.
        """
        OUT_DIR.mkdir(parents=True, exist_ok=True)
        binp = OUT_DIR / f"{tag}.bin"
        if binp.exists():
            binp.unlink()
        self.hmp(f"memsave 0x{vaddr:x} {size} {win_path(binp)}")
        for _ in range(200):
            if binp.exists() and binp.stat().st_size >= size:
                break
            time.sleep(0.005)
        last = None
        for attempt in range(4):
            try:
                buf = bytearray(size)
                if binp.exists():
                    with open(binp, "rb") as f:
                        got = 0
                        while got < size:
                            n = f.readinto(memoryview(buf)[got:got + 0x8000])
                            if not n:
                                break
                            got += n
                return bytes(buf)
            except OSError as exc:
                # ENOMEM: the host is momentarily out of memory (xemu spikes
                # during a magicboot, and this box runs with a few hundred MB
                # free). Back off rather than killing the run.
                last = exc
                time.sleep(0.25 * (attempt + 1))
        raise last

    def close(self):
        try:
            self.s.close()
        except OSError:
            pass


def _plausible_va(va):
    return 0x10000 <= va < 0xF0000000


def _plan_windows(vas):
    """Group sorted object VAs into a few small read windows.

    Each window must cover base..base+0x29c for every VA it holds.  Windows
    are merged while the next object is within WINDOW_GAP, and split once a
    window would exceed MAX_WINDOW.
    """
    need = MOUTH_APERTURE_OFF + 4
    out = []
    start = end = None
    for va in vas:
        if start is None:
            start, end = va, va + need
            continue
        if va <= end + WINDOW_GAP and (va + need - start) <= MAX_WINDOW:
            end = max(end, va + need)
        else:
            out.append((start, end - start))
            start, end = va, va + need
    if start is not None:
        out.append((start, end - start))
    return out


def sample(q):
    """One snapshot.

    Returns ((object_count, rows, type_mismatches), None) on success or
    (None, reason) when the context is not usable.  Each row is
    (header_index, type, object_va, aperture).
    """
    table_ptr = struct.unpack_from("<I", q.read(OBJECT_HEADER_TABLE_PTR, 4, "tp"), 0)[0]
    if not _plausible_va(table_ptr):
        return None, f"object table pointer looks wrong: 0x{table_ptr:08x}"

    hdr = q.read(table_ptr, 0x38, "dt")
    max_count, size = struct.unpack_from("<hh", hdr, 0x20)
    magic = struct.unpack_from("<I", hdr, 0x28)[0]
    count = struct.unpack_from("<h", hdr, 0x2E)[0]
    data_va = struct.unpack_from("<I", hdr, 0x34)[0]
    if magic != DATUM_MAGIC:
        return None, (f"datum magic 0x{magic:08x} != 0x{DATUM_MAGIC:08x} "
                      "(not an active-gameplay context)")
    if not (0 < size <= 0x40 and 0 < max_count <= 0x1000) or not _plausible_va(data_va):
        return None, f"implausible table header: max={max_count} size={size}"

    raw = q.read(data_va, max_count * size, "hd")
    units = []
    for i in range(max_count):
        e = raw[i * size:(i + 1) * size]
        if struct.unpack_from("<H", e, 0)[0] == 0:      # free slot
            continue
        otype = e[3]
        if otype not in (TYPE_BIPED, TYPE_VEHICLE):
            continue
        obj_va = struct.unpack_from("<I", e, 8)[0]
        if _plausible_va(obj_va):
            units.append((i, otype, obj_va))
    if not units:
        return (count, [], 0), None

    # A handful of clustered reads instead of one read per unit: the unit
    # objects sit near each other in the object pool, so merging everything
    # within WINDOW_GAP typically yields one or two small windows.
    windows = _plan_windows(sorted(u[2] for u in units))
    pages = {}
    for start, size in windows:
        pages[start] = q.read(start, size, f"w{start:08x}")

    def fetch(va, fmt, width):
        for start, blob in pages.items():
            off = va - start
            if 0 <= off <= len(blob) - width:
                return struct.unpack_from(fmt, blob, off)[0]
        return None

    rows, bad_type = [], 0
    for i, otype, obj_va in units:
        obj_type = fetch(obj_va + OBJECT_TYPE_OFF, "<h", 2)
        if obj_type is None or obj_type != otype:
            # Header byte and object+0x64 disagree: assumed layout is wrong.
            bad_type += 1
            continue
        ap = fetch(obj_va + MOUTH_APERTURE_OFF, "<f", 4)
        if ap is None:
            bad_type += 1
            continue
        rows.append((i, otype, obj_va, ap))
    return (count, rows, bad_type), None


def is_talking(ap):
    """Finite, positive, and inside the field's real [0,1] range."""
    return 1e-6 < ap < 1.5


def collect(q, seconds=None, samples=None, interval=0.1, verbose=True):
    """Poll and reduce to a signature dict."""
    peak = 0.0
    ok = talking = 0
    talkers = set()
    aps = []
    n = 0
    deadline = (time.time() + seconds) if seconds else None
    while True:
        if deadline is not None and time.time() >= deadline:
            break
        if samples is not None and n >= samples:
            break
        try:
            res, err = sample(q)
        except (OSError, RuntimeError, struct.error) as exc:
            n += 1
            if verbose:
                print(f"[{n:03d}] SKIP read failed: {exc}")
            time.sleep(max(interval, 0.25))
            continue
        n += 1
        if err:
            if verbose:
                print(f"[{n:03d}] SKIP {err}")
            time.sleep(interval)
            continue
        count, rows, bad = res
        ok += 1
        nz = [r for r in rows if is_talking(r[3])]
        if nz:
            talking += 1
            top = max(r[3] for r in nz)
            aps.append(top)
            peak = max(peak, top)
            talkers.update(r[0] for r in nz)
        if verbose:
            print(f"[{n:03d}] objects={count} units={len(rows)} "
                  f"type_mismatch={bad} talking={len(nz)} "
                  f"max={max((r[3] for r in nz), default=0.0):.4f}")
        if bad and verbose and ok == 1:
            print("      WARNING: header type byte disagrees with object+0x64 "
                  "— assumed layout may be wrong")
        time.sleep(interval)

    aps.sort()
    return {
        "ok_samples": ok,
        "talking_samples": talking,
        "talking_fraction": (talking / ok) if ok else 0.0,
        "peak_aperture": round(peak, 4),
        "median_talking_aperture": round(aps[len(aps) // 2], 4) if aps else 0.0,
        "distinct_talkers": len(talkers),
    }


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--seconds", type=float, help="poll for this long")
    g.add_argument("--samples", type=int, help="poll this many times")
    ap.add_argument("--interval", type=float, default=0.1)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=4444)
    ap.add_argument("--json", action="store_true", help="print only the signature")
    a = ap.parse_args()
    if a.seconds is None and a.samples is None:
        a.seconds = 20.0

    q = Qmp(a.host, a.port)
    try:
        sig = collect(q, seconds=a.seconds, samples=a.samples,
                      interval=a.interval, verbose=not a.json)
    finally:
        q.close()

    if a.json:
        print(json.dumps(sig, indent=2))
    else:
        print()
        if not sig["ok_samples"]:
            print("RESULT: no usable sample (not in active gameplay?)")
            return 2
        if sig["peak_aperture"] > 0.0:
            print(f"RESULT: lip-sync ACTIVE — peak {sig['peak_aperture']:.4f}, "
                  f"median-while-talking {sig['median_talking_aperture']:.4f}, "
                  f"talking in {sig['talking_samples']}/{sig['ok_samples']} "
                  f"samples, {sig['distinct_talkers']} distinct unit(s)")
            return 0
        print(f"RESULT: mouth aperture stayed 0.0 across {sig['ok_samples']} "
              f"samples — no lip-sync observed")
    return 0 if sig["peak_aperture"] > 0.0 else 1


if __name__ == "__main__":
    sys.exit(main())
