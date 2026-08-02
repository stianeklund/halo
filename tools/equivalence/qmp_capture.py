"""Atomic live-memory capture from xemu over raw QMP (:4444).

The proven-viable capture primitive for A/B trajectory testing
(docs/ab-trajectory-testing.md). Per the reference recipe
(reference_xemu_qmp_memsave_capture, re-confirmed 2026-06-29):

  * raw QMP :4444 with a RESPONSE-MATCHED client (skip async STOP/RESUME events),
  * `memsave` (VIRTUAL); never `pmemsave` (physical, wrong bytes on this
    Cerbios / kernel-irqchip=off setup — the heap lives at 0x80xxxxxx),
  * HMP `memsave` command-line needs DOUBLED backslashes in the Windows path,
  * `stop` -> memsave the whole region set -> `cont`: atomic by construction
    (nothing advances while paused; pausing does not perturb tick-determinism),
  * verify the datum magic before trusting any capture.

This module is transport only — it hands back raw {addr: bytes} regions. Pool
semantics live in halorec_to_snapshot / halorec_ai_diff; the HMRC writer in hmrc.
"""

import json
import os
import socket
import struct
import time
from pathlib import Path

QMP_HOST = "127.0.0.1"
QMP_PORT = 4444

# data_t pool pointers (guest VAs) + header layout (matches halorec_to_snapshot).
OBJECT_TABLE_PTR = 0x5A8D50
PLAYER_TABLE_PTR = 0x5AA6D4
ACTOR_TABLE_PTR = 0x6325A4
PROP_POOL_PTR = 0x5AB23C
DATA_T_MAGIC = 0x64407440
DATA_T_HDR_LEN = 0x38            # max@0x20 es@0x22 magic@0x28 cur@0x2E data@0x34

GAME_TIME_GLOBALS_PTR = 0x45708C  # *ptr + 0x0C = game tick
HANDLE_NONE = 0xFFFFFFFF

# The object pool's element is only a 12-byte datum ENTRY (es=0xc): salt@0x00,
# then a pointer at +0x08 to the object BODY, which lives elsewhere in the heap.
# Capturing the pool alone therefore captures no object state at all (animation
# index/frame, position, ... all live in the body). Follow the pointers.
OBJECT_BODY_PTR_OFF = 0x08
DEFAULT_OBJECT_BODY_SIZE = 0x100   # covers +0x7c 'antr' / +0x80 anim / +0x82 frame
# Bodies are densely packed in one heap arena (observed a10: 460 bodies, median
# gap 0x28c, total span ~0x83000), and a memsave costs ~13ms of round-trip
# REGARDLESS of size (4B: 13.6ms, 576KB: 17.3ms) -- so the only thing that makes
# a frame cheap is issuing FEW reads. Merging holes up to 0x2000 collapses those
# 460 bodies into 1-2 reads (1.15s/frame -> 0.25s/frame); the wasted bytes are
# bounded per boundary and gzip away in the container.
DEFAULT_BODY_MERGE_GAP = 0x2000
DEFAULT_BODY_MAX_REGION = 0x100000
HEAP_LO = 0x80000000
HEAP_HI = 0x84000000

POOL_PTRS = {
    "objects": OBJECT_TABLE_PTR,
    "players": PLAYER_TABLE_PTR,
    "actors": ACTOR_TABLE_PTR,
    "props": PROP_POOL_PTR,
}

_DEFAULT_SCRATCH = Path(__file__).resolve().parent.parent.parent / "tmp" / "qmp_capture"


def wsl_to_win(path: Path) -> str:
    """/mnt/g/dev/... -> G:\\dev\\... (single-backslash Windows path)."""
    p = path.resolve()
    parts = p.parts
    if len(parts) >= 3 and parts[0] == "/" and parts[1] == "mnt" and len(parts[2]) == 1:
        drive = parts[2].upper()
        rest = "\\".join(parts[3:])
        return f"{drive}:\\{rest}"
    raise ValueError(f"not a /mnt/<drive>/ path, cannot map to Windows: {path}")


class QMPError(RuntimeError):
    pass


class QMPCapture:
    """Response-matched QMP client with atomic memsave-based region capture."""

    def __init__(self, host=QMP_HOST, port=QMP_PORT, scratch=None, settle_s=1.0):
        self.s = socket.create_connection((host, port), timeout=10)
        self.f = self.s.makefile("rwb", buffering=0)
        self._readline()                       # greeting
        self.execute("qmp_capabilities")
        self.scratch = Path(scratch) if scratch else _DEFAULT_SCRATCH
        self.scratch.mkdir(parents=True, exist_ok=True)
        self.settle_s = settle_s
        self._n = 0
        self._gt_ptr = None            # cached game-time globals ptr (stable per replay)

    # -- raw protocol -------------------------------------------------------
    def _readline(self):
        line = self.f.readline()
        if not line:
            raise QMPError("QMP connection closed")
        return json.loads(line.decode("utf-8", "replace"))

    def execute(self, cmd, **args):
        msg = {"execute": cmd}
        if args:
            msg["arguments"] = args
        self.f.write((json.dumps(msg) + "\r\n").encode())
        while True:                            # skip async events, match the return
            obj = self._readline()
            if "return" in obj:
                return obj["return"]
            if "error" in obj:
                raise QMPError(f"{cmd} -> {obj['error']}")

    def hmp(self, line):
        return self.execute("human-monitor-command", **{"command-line": line})

    def status(self):
        return self.execute("query-status").get("status")

    def stop(self):
        self.execute("stop")

    def cont(self):
        self.execute("cont")

    # -- memory -------------------------------------------------------------
    def _memsave_file(self, va, size):
        """Issue one memsave; return the host path it was written to (not yet read)."""
        self._n += 1
        wsl = self.scratch / f"ms_{self._n}.bin"
        try:
            wsl.unlink()                       # avoid reading stale bytes
        except FileNotFoundError:
            pass
        win = wsl_to_win(wsl).replace("\\", "\\\\")   # HMP unescapes \\ -> \
        self.hmp(f'memsave 0x{va:x} {size} "{win}"')
        return wsl, size

    def _read_file(self, wsl, size):
        deadline = time.monotonic() + self.settle_s
        while True:
            try:
                data = wsl.read_bytes()
                if len(data) >= size:
                    try:
                        wsl.unlink()
                    except OSError:
                        pass
                    return data[:size]
            except FileNotFoundError:
                pass
            if time.monotonic() > deadline:
                raise QMPError(f"memsave produced no/short file: {wsl}")
            time.sleep(0.01)

    def read_mem(self, va, size):
        """Single virtual read (no stop — caller must pause for a coherent set)."""
        wsl, n = self._memsave_file(va, size)
        return self._read_file(wsl, n)

    def read_u32(self, va):
        return struct.unpack("<I", self.read_mem(va, 4))[0]

    def tick(self):
        """Fast game-tick poll: caches the game-time ptr -> one round-trip/poll."""
        if self._gt_ptr is None:
            gt = self.read_u32(GAME_TIME_GLOBALS_PTR)
            if not (0x80000000 <= gt < 0x84000000):
                return None
            self._gt_ptr = gt
        return struct.unpack("<I", self.read_mem(self._gt_ptr + 0x0C, 4))[0]

    def capture_regions(self, specs):
        """Atomically capture [(va, size), ...] inside one stop/cont window.

        Returns {va: bytes}. memsave returns once the file is written, so we
        issue every memsave while paused (coherent), then cont, then read the
        files back (content already reflects the frozen state).
        """
        self.stop()
        try:
            pending = [(va, *self._memsave_file(va, size)) for va, size in specs]
        finally:
            self.cont()
        return {va: self._read_file(wsl, n) for va, wsl, n in pending}

    def close(self):
        try:
            self.s.close()
        except OSError:
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        # best-effort: never leave the guest paused on the way out
        try:
            self.cont()
        except Exception:
            pass
        self.close()


# -- pool resolution (needs a paused read each step) ------------------------

def resolve_pool(cap, ptr):
    """Return {hdr, max, es, magic, cur, data} for a data_t pool, or None."""
    hdr_ptr = cap.read_u32(ptr)
    if not (0x80000000 <= hdr_ptr < 0x84000000):
        return None
    h = cap.read_mem(hdr_ptr, DATA_T_HDR_LEN)
    max_c, es = struct.unpack_from("<hh", h, 0x20)
    magic = struct.unpack_from("<I", h, 0x28)[0]
    cur = struct.unpack_from("<h", h, 0x2E)[0]
    data = struct.unpack_from("<I", h, 0x34)[0]
    return {"hdr": hdr_ptr, "max": max_c, "es": es, "magic": magic,
            "cur": cur, "data": data}


def pool_region_specs(cap, ptr):
    """Region (va,size) specs to fully capture a pool's USED span: ptr, hdr, data.

    Returns [] if the pool can't be resolved (not in gameplay / not present).
    """
    hd = resolve_pool(cap, ptr)
    if hd is None or hd["magic"] != DATA_T_MAGIC or hd["es"] <= 0:
        return []
    specs = [(ptr, 4), (hd["hdr"], DATA_T_HDR_LEN)]
    n = hd["cur"] if 0 < hd["cur"] <= hd["max"] else hd["max"]
    if 0 < n <= 5000 and 0x80000000 <= hd["data"] < 0x84000000:
        specs.append((hd["data"], n * hd["es"]))
    return specs


def object_body_ptrs(pool_data, es, count=None, ptr_off=OBJECT_BODY_PTR_OFF):
    """Body pointers held by the live entries of a datum-entry pool's data span.

    An entry is live when its salt (u16 @ +0x00) is non-zero; the body pointer at
    `ptr_off` must land in the game heap. Pure function -- takes the already-read
    element array so the caller can reuse the bytes it captured while paused.
    """
    if es <= 0 or ptr_off + 4 > es:
        return []
    n = len(pool_data) // es if count is None else min(count, len(pool_data) // es)
    out = []
    for i in range(n):
        base = i * es
        if struct.unpack_from("<H", pool_data, base)[0] == 0:
            continue                       # free slot
        p = struct.unpack_from("<I", pool_data, base + ptr_off)[0]
        if HEAP_LO <= p < HEAP_HI:
            out.append(p)
    return out


def merge_body_specs(ptrs, size=DEFAULT_OBJECT_BODY_SIZE,
                     merge_gap=DEFAULT_BODY_MERGE_GAP,
                     max_region=DEFAULT_BODY_MAX_REGION):
    """Coalesce [ptr, ptr+size) windows into as few (va, size) reads as possible.

    Two windows merge when the hole between them is <= `merge_gap` (the wasted
    bytes are bounded by that, and they compress away in the gzip container);
    a merged run is split once it would exceed `max_region`. Pure function.
    """
    if not ptrs or size <= 0:
        return []
    spans = sorted((p, p + size) for p in set(ptrs))
    out = []
    lo, hi = spans[0]
    for s, e in spans[1:]:
        if s - hi <= merge_gap and (max(hi, e) - lo) <= max_region:
            hi = max(hi, e)
        else:
            out.append((lo, hi - lo))
            lo, hi = s, e
    out.append((lo, hi - lo))
    return out


def object_body_specs(cap, ptr=OBJECT_TABLE_PTR, size=DEFAULT_OBJECT_BODY_SIZE,
                      merge_gap=DEFAULT_BODY_MERGE_GAP,
                      max_region=DEFAULT_BODY_MAX_REGION):
    """(va, size) specs covering every live object BODY. Caller should be paused."""
    hd = resolve_pool(cap, ptr)
    if hd is None or hd["magic"] != DATA_T_MAGIC or hd["es"] <= 0:
        return []
    n = hd["cur"] if 0 < hd["cur"] <= hd["max"] else hd["max"]
    if not (0 < n <= 5000 and HEAP_LO <= hd["data"] < HEAP_HI):
        return []
    data = cap.read_mem(hd["data"], n * hd["es"])
    return merge_body_specs(object_body_ptrs(data, hd["es"], n),
                            size, merge_gap, max_region)


def gametime_region_specs(cap):
    """Specs for the game-time globals so a frame's tick is recoverable offline."""
    gt = cap.read_u32(GAME_TIME_GLOBALS_PTR)
    specs = [(GAME_TIME_GLOBALS_PTR, 4)]
    if 0x80000000 <= gt < 0x84000000:
        specs.append((gt, 0x10))
    return specs


def read_tick(cap):
    """Current game tick (free-running poll; single field, monotonic)."""
    gt = cap.read_u32(GAME_TIME_GLOBALS_PTR)
    if not (0x80000000 <= gt < 0x84000000):
        return None
    return struct.unpack_from("<I", cap.read_mem(gt + 0x0C, 4), 0)[0]


def capture_full_frame(cap, pools=("objects", "players", "actors", "props"),
                       object_bodies=False,
                       body_size=DEFAULT_OBJECT_BODY_SIZE,
                       body_merge_gap=DEFAULT_BODY_MERGE_GAP,
                       body_max_region=DEFAULT_BODY_MAX_REGION):
    """Coherently capture game-time + the used span of each pool in ONE pause.

    Pool pointers/headers are resolved *while paused*, so the captured data
    span can't be torn by a realloc between resolve and read. Returns
    (regions {va: bytes}, tick). Regions are exactly what the HMRC readers and
    the viewer expect, so the frame is offline-resolvable and viewer-loadable.

    `object_bodies=True` additionally follows every live object entry's +0x08
    body pointer and captures the bodies (merged into few reads) in the SAME
    pause -- without it a frame carries no object state at all, only the 12-byte
    datum entries. Off by default so existing callers are unaffected.
    """
    regions = {}
    tick = None
    cap.stop()
    try:
        gt = struct.unpack("<I", cap.read_mem(GAME_TIME_GLOBALS_PTR, 4))[0]
        regions[GAME_TIME_GLOBALS_PTR] = struct.pack("<I", gt)
        if 0x80000000 <= gt < 0x84000000:
            blk = cap.read_mem(gt, 0x10)
            regions[gt] = blk
            tick = struct.unpack_from("<I", blk, 0x0C)[0]
        for name in pools:
            ptr = POOL_PTRS[name]
            hdr_ptr = cap.read_u32(ptr)
            regions[ptr] = struct.pack("<I", hdr_ptr)
            if not (0x80000000 <= hdr_ptr < 0x84000000):
                continue
            h = cap.read_mem(hdr_ptr, DATA_T_HDR_LEN)
            regions[hdr_ptr] = h
            es = struct.unpack_from("<h", h, 0x22)[0]
            magic = struct.unpack_from("<I", h, 0x28)[0]
            cur = struct.unpack_from("<h", h, 0x2E)[0]
            max_c = struct.unpack_from("<h", h, 0x20)[0]
            data = struct.unpack_from("<I", h, 0x34)[0]
            if magic != DATA_T_MAGIC or es <= 0:
                continue
            n = cur if 0 < cur <= max_c else max_c
            if 0 < n <= 5000 and 0x80000000 <= data < 0x84000000:
                blob = cap.read_mem(data, n * es)
                regions[data] = blob
                if object_bodies and name == "objects":
                    specs = merge_body_specs(
                        object_body_ptrs(blob, es, n),
                        body_size, body_merge_gap, body_max_region)
                    for bva, bsz in specs:
                        regions[bva] = cap.read_mem(bva, bsz)
    finally:
        cap.cont()
    return regions, tick


def magic_ok(cap):
    """Verify-datum-magic gate: objtable resolves with the data_t magic."""
    hd = resolve_pool(cap, OBJECT_TABLE_PTR)
    return hd is not None and hd["magic"] == DATA_T_MAGIC


def player_spawned(cap):
    """True if player slot 0's unit handle (+0x34) is live (gameplay, not lobby)."""
    hd = resolve_pool(cap, PLAYER_TABLE_PTR)
    if hd is None or hd["es"] <= 0 or hd["data"] == 0:
        return False
    rec = cap.read_mem(hd["data"], min(hd["es"], 0x40))
    if len(rec) < 0x38:
        return False
    return struct.unpack_from("<I", rec, 0x34)[0] != HANDLE_NONE
