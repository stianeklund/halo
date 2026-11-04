#!/usr/bin/env python3
"""Toggle and dump the aim-assist diagnostics of a --aim-assist-debug build.

DIAGNOSTIC TOOLING.  Two PE exports (src/halo/game/aim_assist_debug.h):
  halo_aim_assist_debug  4 toggle bytes: rings, text, console, trace
  halo_aim_assist_trace  per-tick ring of 128-byte records
  halo_aim_test_script   scripted pad input (halo-cea-jukkis --input format)
Their VAs are resolved the way tools/xbox/rng_trace_dump.py resolves
`halo_rng_trace`.  Bridged boxes need HALO_WINDOWS_REEXEC=1.

Usage:
    tools/xbox/aim_assist_debug.py --host 10.0.0.21                  # show
    tools/xbox/aim_assist_debug.py --host 10.0.0.21 --rings 0 --console 1
    tools/xbox/aim_assist_debug.py --host 10.0.0.21 --dump a.csv     # whole ring
    tools/xbox/aim_assist_debug.py --host 10.0.0.21 --tail 30        # last 30
    tools/xbox/aim_assist_debug.py --csv a.csv --tail 30             # offline
    tools/xbox/aim_assist_debug.py --host 10.0.0.21 --script flick.input.txt --tick-scale 4
    tools/xbox/aim_assist_debug.py --host 10.0.0.21 --script-status
See docs/aim-assist-debug.md.
"""
from __future__ import annotations

import argparse
import csv
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from rng_trace_dump import XBDM_PORT, XbdmError, getmem, xbdm_connect  # noqa: E402
from symbolize_exception import (  # noqa: E402
    DEFAULT_ORIGINAL_XBE,
    DEFAULT_PE,
    load_pe_symbols,
    normalize_export_name,
    resolve_runtime_base,
)

TOGGLES_SYMBOL = "halo_aim_assist_debug"
TRACE_SYMBOL = "halo_aim_assist_trace"
SCRIPT_SYMBOL = "halo_aim_test_script"
TOGGLES = ("rings", "text", "console", "trace")

TRACE_MAGIC = 0x54414941
TRACE_VERSION = 2
HEADER = struct.Struct("<4I")
RECORD = struct.Struct("<iBBHi29f")
assert RECORD.size == 0x80

KINDS = {1: "tick", 2: "shot"}
FLAG_MIXED, FLAG_MAGNETISM, FLAG_SEARCH_AXIS, FLAG_SCRIPTED = 0x1, 0x2, 0x4, 0x8

SCRIPT_MAGIC = 0x50534941
SCRIPT_CAPACITY = 4096
SCRIPT_HEADER = struct.Struct("<IIiIIiII")
POSE = struct.Struct("<I5f")  # enabled, x, y, z, yaw, pitch (radians)
assert POSE.size == 0x18
SCRIPT_POSES = 4
KEYFRAME = struct.Struct("<iBBHhhhh")
assert KEYFRAME.size == 0x10
KEYFRAMES_OFFSET = SCRIPT_HEADER.size + SCRIPT_POSES * POSE.size
SCRIPT_STATES = {0: "idle", 1: "armed", 2: "running", 3: "done"}
FLOAT_FIELDS = (
    "autoaim_level", "magnetism", "aim_yaw", "aim_pitch",
    "aim_velocity_yaw", "aim_velocity_pitch", "stick_yaw", "stick_pitch",
    "look_yaw", "look_pitch", "game_speed", "input_scale", "magnetism_scale",
    "clamped_velocity_yaw", "clamped_velocity_pitch", "mixed_yaw",
    "mixed_pitch", "final_yaw", "final_pitch", "delta_time",
    "search_i", "search_j", "search_k", "facing_i", "facing_j", "facing_k",
    "pos_x", "pos_y", "pos_z",
)
# Radian-valued fields, written to the CSV in degrees.
ANGLE_FIELDS = frozenset({
    "aim_yaw", "aim_pitch", "aim_velocity_yaw", "aim_velocity_pitch",
    "look_yaw", "look_pitch", "clamped_velocity_yaw", "clamped_velocity_pitch",
    "mixed_yaw", "mixed_pitch", "final_yaw", "final_pitch",
})
COLUMNS = ("seq", "tick", "kind", "player", "mixed", "player_magnetism",
           "scripted", "target") + FLOAT_FIELDS + ("search_vs_facing_deg", "shot_bend_deg")


def find_exports(pe_path: Path, original_xbe: Path,
                 runtime_base: str = "auto") -> dict[str, int]:
    base, _ = resolve_runtime_base(runtime_base, original_xbe)
    pe = load_pe_symbols(pe_path, base)
    if pe is None:
        raise XbdmError(f"PE not found: {pe_path}")
    found = {}
    for sym in pe.symbols:
        name = normalize_export_name(sym.raw_name)
        if name in (TOGGLES_SYMBOL, TRACE_SYMBOL, SCRIPT_SYMBOL):
            found[name] = sym.address
    if TOGGLES_SYMBOL not in found:
        raise XbdmError(f"'{TOGGLES_SYMBOL}' is not exported by the PE -- this "
                        "build was not compiled with --aim-assist-debug")
    return found


def setmem(sock, addr: int, data: bytes) -> None:
    sock.sendall(f"setmem addr=0x{addr:08x} data={data.hex()}\r\n".encode())
    reply = sock.recv(1024).decode(errors="replace")
    if not reply.startswith("200"):
        raise XbdmError(f"setmem 0x{addr:08x} failed: {reply.strip()}")


def load_script(path: Path, tick_scale: float) -> list[tuple]:
    """Parse a halo-cea-jukkis --input script (tick port lx ly rx ry buttons).

    Ticks are divided by tick_scale (4 turns 120 Hz PC ticks into 30 Hz Xbox
    ticks) and rounded.  A keyframe that would land on the same tick as the
    previous one for its port moves one tick later, so a short press is never
    swallowed by its own release."""
    keys = []
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        f = line.split()
        if len(f) != 7:
            raise XbdmError(f"malformed script line: {line!r}")
        keys.append((int(f[0]), int(f[1]), int(f[2]), int(f[3]), int(f[4]),
                     int(f[5]), int(f[6], 16)))
    keys.sort(key=lambda k: k[0])
    last = {}
    out = []
    for tick, port, lx, ly, rx, ry, buttons in keys:
        if not 0 <= port < 4:
            raise XbdmError(f"port {port} out of range")
        t = int(math.floor(tick / tick_scale + 0.5))
        if port in last and t <= last[port]:
            t = last[port] + 1
        last[port] = t
        out.append((t, port, lx, ly, rx, ry, buttons))
    out.sort(key=lambda k: k[0])
    if len(out) > SCRIPT_CAPACITY:
        raise XbdmError(f"{len(out)} keyframes, capacity {SCRIPT_CAPACITY}")
    return out


def clamp16(v: int) -> int:
    return max(-32768, min(32767, v))


def parse_pose(text: str) -> tuple[int, tuple]:
    """LP,x,y,z,yaw_deg[,pitch_deg] -> (local player, pose)."""
    parts = [float(v) for v in text.split(",")]
    if len(parts) not in (5, 6):
        raise argparse.ArgumentTypeError("expected LP,x,y,z,yaw[,pitch]")
    pitch = parts[5] if len(parts) == 6 else 0.0
    return int(parts[0]), (parts[1], parts[2], parts[3],
                           math.radians(parts[4]), math.radians(pitch))


def meet_pose(target: tuple, distance: float) -> tuple:
    """A pose `distance` wu in front of `target`, facing back at it."""
    x, y, z, yaw, _ = target
    return (x + distance * math.cos(yaw), y + distance * math.sin(yaw), z,
            yaw + math.pi, 0.0)


def upload_script(sock, va: int, keys: list[tuple], ports: int,
                  end_tick: int, poses: dict[int, tuple]) -> None:
    setmem(sock, va + 4, struct.pack("<I", 0))  # idle while writing
    setmem(sock, va + SCRIPT_HEADER.size, b"".join(
        POSE.pack(1, *poses[lp]) if lp in poses else POSE.pack(0, 0, 0, 0, 0, 0)
        for lp in range(SCRIPT_POSES)))
    blob = b"".join(KEYFRAME.pack(t, p, 0, b & 0xffff, clamp16(lx), clamp16(ly),
                                  clamp16(rx), clamp16(ry))
                    for t, p, lx, ly, rx, ry, b in keys)
    base = va + KEYFRAMES_OFFSET
    for off in range(0, len(blob), 128):
        setmem(sock, base + off, blob[off:off + 128])
    setmem(sock, va + 12, struct.pack("<I", len(keys)))
    setmem(sock, va + 20, struct.pack("<iI", end_tick, ports))
    setmem(sock, va + 4, struct.pack("<I", 1))  # armed


def script_status(sock, va: int) -> str:
    magic, state, start, count, cursor, end, ports, _ = SCRIPT_HEADER.unpack(
        getmem(sock, va, SCRIPT_HEADER.size))
    if magic != SCRIPT_MAGIC:
        raise XbdmError(f"script magic {magic:#x}, expected {SCRIPT_MAGIC:#x}")
    return (f"script {SCRIPT_STATES.get(state, state)} start_tick {start} "
            f"keyframes {cursor}/{count} end_tick {end} ports {ports:#x}")


def angle_between(a, b) -> float:
    cross = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
             a[0] * b[1] - a[1] * b[0])
    na = math.sqrt(sum(x * x for x in a))
    nb = math.sqrt(sum(x * x for x in b))
    if na < 1e-6 or nb < 1e-6:
        return -1.0
    return math.degrees(math.atan2(math.sqrt(sum(x * x for x in cross)),
                                   sum(x * y for x, y in zip(a, b))))


def direction(yaw: float, pitch: float) -> tuple[float, float, float]:
    return (math.cos(yaw) * math.cos(pitch), math.sin(yaw) * math.cos(pitch),
            math.sin(pitch))


def decode(blob: bytes) -> list[dict]:
    magic, version, capacity, write_index = HEADER.unpack_from(blob, 0)
    if magic != TRACE_MAGIC:
        raise XbdmError("trace is empty (no tick recorded since boot)")
    if version != TRACE_VERSION:
        raise XbdmError(f"trace version {version}, tool expects {TRACE_VERSION}")
    count = min(write_index, capacity)
    rows = []
    for seq in range(write_index - count, write_index):
        off = HEADER.size + (seq % capacity) * RECORD.size
        values = RECORD.unpack_from(blob, off)
        tick, kind, player, flags, target = values[:5]
        floats = values[5:5 + len(FLOAT_FIELDS)]
        row = {"seq": seq, "tick": tick, "kind": KINDS.get(kind, str(kind)),
               "player": player, "mixed": int(bool(flags & FLAG_MIXED)),
               "player_magnetism": int(bool(flags & FLAG_MAGNETISM)),
               "scripted": int(bool(flags & FLAG_SCRIPTED)),
               "target": f"{target & 0xffffffff:08x}"}
        for name, value in zip(FLOAT_FIELDS, floats):
            row[name] = math.degrees(value) if name in ANGLE_FIELDS else value
        row["search_vs_facing_deg"] = (
            angle_between(floats[20:23], floats[23:26])
            if flags & FLAG_SEARCH_AXIS else -1.0)
        row["shot_bend_deg"] = (
            angle_between(direction(floats[2], floats[3]),
                          direction(floats[17], floats[18]))
            if kind == 2 else 0.0)
        rows.append(row)
    return rows


def read_csv(path: Path) -> list[dict]:
    with path.open(newline="") as fh:
        return list(csv.DictReader(fh))


def write_csv(path: Path, rows: list[dict]) -> None:
    with path.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=COLUMNS)
        w.writeheader()
        for row in rows:
            w.writerow({k: (f"{v:.6f}" if isinstance(v, float) else v)
                        for k, v in row.items()})


def fmt(row: dict) -> str:
    f = {k: (float(v) if k in FLOAT_FIELDS
             or k in ("search_vs_facing_deg", "shot_bend_deg") else v)
         for k, v in row.items()}
    if f["kind"] == "shot":
        return (f"t{f['tick']} p{f['player']} SHOT target {f['target']} "
                f"bend {f['shot_bend_deg']:.3f}deg "
                f"aim {f['aim_yaw']:+.3f}/{f['aim_pitch']:+.3f} -> "
                f"{f['final_yaw']:+.3f}/{f['final_pitch']:+.3f} "
                f"search-vs-view {f['search_vs_facing_deg']:.3f}deg")
    line = (f"t{f['tick']} p{f['player']} tgt {f['target']} "
            f"aa {f['autoaim_level']:.3f} mag {f['magnetism']:.3f} "
            f"stick {f['stick_yaw']:+.3f}/{f['stick_pitch']:+.3f} "
            f"vel {f['aim_velocity_yaw']:+.4f}/{f['aim_velocity_pitch']:+.4f} "
            f"look {f['look_yaw']:+.4f}/{f['look_pitch']:+.4f}")
    if str(f["mixed"]) == "1":
        line += (f" MIX in {f['input_scale']:.3f} mag {f['magnetism_scale']:.3f}"
                 f" clamped {f['clamped_velocity_yaw']:+.4f}/"
                 f"{f['clamped_velocity_pitch']:+.4f}")
    line += (f" final {f['final_yaw']:+.4f}/{f['final_pitch']:+.4f} "
             f"axis {f['search_vs_facing_deg']:.3f}")
    return line


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=XBDM_PORT)
    ap.add_argument("--timeout", type=float, default=30.0)
    ap.add_argument("--pe", default=str(DEFAULT_PE))
    ap.add_argument("--original-xbe", default=str(DEFAULT_ORIGINAL_XBE))
    ap.add_argument("--runtime-base", default="auto")
    for name in TOGGLES:
        ap.add_argument(f"--{name}", type=int, choices=(0, 1), default=None)
    ap.add_argument("--dump", metavar="CSV",
                    help="read the whole trace ring and write it as CSV")
    ap.add_argument("--tail", type=int, metavar="N",
                    help="print the last N records as readable lines")
    ap.add_argument("--csv", metavar="CSV",
                    help="format an existing --dump file instead of the box")
    ap.add_argument("--script", metavar="FILE",
                    help="upload a jukkis --input script and arm it; it starts "
                         "on the next game tick")
    ap.add_argument("--tick-scale", type=float, default=1.0,
                    help="divide script ticks by this (4 for 120 Hz PC scripts)")
    ap.add_argument("--ports", type=lambda v: int(v, 0), default=None,
                    help="bit mask of scripted ports (default: ports in script)")
    ap.add_argument("--end-tick", type=int, default=None,
                    help="relative tick the script ends after (default: last "
                         "keyframe)")
    ap.add_argument("--pose", type=parse_pose, action="append", default=[],
                    metavar="LP,X,Y,Z,YAW[,PITCH]",
                    help="at script start, put local player LP's unit here "
                         "facing YAW degrees (repeatable)")
    ap.add_argument("--meet", type=float, metavar="DIST",
                    help="pose local player 0 DIST wu in front of local "
                         "player 1's --pose, facing it (jukkis TEST_MEET)")
    ap.add_argument("--script-status", action="store_true")
    ap.add_argument("--script-stop", action="store_true",
                    help="stop a running script and give the pads back")
    args = ap.parse_args()

    if args.csv:
        for row in read_csv(Path(args.csv))[-(args.tail or 0):]:
            print(fmt(row))
        return 0

    try:
        exports = find_exports(Path(args.pe), Path(args.original_xbe),
                               args.runtime_base)
        toggles_va = exports[TOGGLES_SYMBOL]
        sock = xbdm_connect(args.host, args.port, args.timeout)
        try:
            if args.script or args.script_status or args.script_stop:
                script_va = exports.get(SCRIPT_SYMBOL)
                if script_va is None:
                    raise XbdmError(f"'{SCRIPT_SYMBOL}' is not exported by "
                                    "this build")
                if args.script_stop:
                    setmem(sock, script_va + 4, struct.pack("<I", 3))
                if args.script:
                    keys = load_script(Path(args.script), args.tick_scale)
                    ports = args.ports
                    if ports is None:
                        ports = 0
                        for k in keys:
                            ports |= 1 << k[1]
                    end_tick = (args.end_tick if args.end_tick is not None
                                else (keys[-1][0] if keys else 0))
                    poses = dict(args.pose)
                    if args.meet is not None:
                        if 1 not in poses:
                            raise XbdmError("--meet needs --pose 1,...")
                        poses[0] = meet_pose(poses[1], args.meet)
                    upload_script(sock, script_va, keys, ports, end_tick,
                                  poses)
                    print(f"armed {len(keys)} keyframes, ports {ports:#x}, "
                          f"end_tick {end_tick}", file=sys.stderr)
                    for lp, (x, y, z, yaw, pitch) in sorted(poses.items()):
                        print(f"  pose lp{lp} ({x:.3f}, {y:.3f}, {z:.3f}) "
                              f"yaw {math.degrees(yaw):.1f} pitch "
                              f"{math.degrees(pitch):.1f}", file=sys.stderr)
                print(script_status(sock, script_va), file=sys.stderr)
            for i, name in enumerate(TOGGLES):
                value = getattr(args, name)
                if value is not None:
                    setmem(sock, toggles_va + i, bytes([value]))
            state = getmem(sock, toggles_va, len(TOGGLES))
            rows = None
            if args.dump or args.tail:
                trace_va = exports[TRACE_SYMBOL]
                header = getmem(sock, trace_va, HEADER.size)
                capacity = HEADER.unpack(header)[2] or 1
                blob = getmem(sock, trace_va,
                              HEADER.size + capacity * RECORD.size)
                rows = decode(blob)
        finally:
            sock.close()
    except XbdmError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"{TOGGLES_SYMBOL} @ 0x{toggles_va:08x}: " +
          " ".join(f"{n}={state[i]}" for i, n in enumerate(TOGGLES)),
          file=sys.stderr)
    if rows is not None:
        print(f"{len(rows)} records", file=sys.stderr)
        if args.dump:
            write_csv(Path(args.dump), rows)
            print(f"wrote {args.dump}", file=sys.stderr)
        if args.tail:
            for row in rows[-args.tail:]:
                print(fmt(row))
    return 0


if __name__ == "__main__":
    sys.exit(main())
