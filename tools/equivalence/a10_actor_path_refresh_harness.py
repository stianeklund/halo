#!/usr/bin/env python3
"""A10 actor_path_refresh snapshot harness.

This helper keeps the a10 grunt regression workflow deterministic:

  1. inspect  - validate a flat Unicorn snapshot and list door actors
  2. prepare  - add actor_path_refresh arg_overrides for one actor handle
  3. run      - invoke unicorn_diff with the prepared snapshot

It intentionally does not capture memory itself. Capture must happen from the
faithful New Game a10 state; this script only validates and drives the replay.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import struct
import subprocess
import sys
from pathlib import Path
from typing import Any, Optional


ROOT = Path(__file__).resolve().parents[2]
TARGET = "actor_path_refresh"
ACTOR_TABLE_PTR = 0x006325A4
OBJECT_TABLE_PTR = 0x005A8D50
ZERO_VECTOR_PTR = 0x0031FC38
GLOBAL_SCENARIO_PTR = 0x005064E4
GLOBAL_BSP_PTR = 0x005064E0
DATA_T_MAGIC = 0x64407440
DOOR_ENCOUNTER = 0xEE6E0009
ACTOR_SIZE_EXPECTED = 0x724
PATH_REFRESH_DATA_WINDOWS = (
    0x00253390,
    0x002533C0,
    0x00255D10,
    0x00331F50,
    0x005AB230,
)


class SparseMemory:
    def __init__(self, regions: dict[str, str]) -> None:
        self._regions: list[tuple[int, int, bytes]] = []
        for addr_hex, hex_bytes in regions.items():
            base = int(addr_hex, 16)
            data = bytes.fromhex(hex_bytes)
            self._regions.append((base, base + len(data), data))
        self._regions.sort(key=lambda item: item[0])

    def read(self, addr: int, size: int) -> Optional[bytes]:
        out = bytearray()
        cur = addr
        end = addr + size
        while cur < end:
            hit = None
            for base, limit, data in self._regions:
                if base <= cur < limit:
                    hit = (base, limit, data)
                    break
            if hit is None:
                return None
            base, limit, data = hit
            take = min(end, limit) - cur
            out.extend(data[cur - base:cur - base + take])
            cur += take
        return bytes(out)

    def contains(self, addr: int, size: int = 1) -> bool:
        return self.read(addr, size) is not None

    def u8(self, addr: int) -> Optional[int]:
        data = self.read(addr, 1)
        return None if data is None else data[0]

    def u16(self, addr: int) -> Optional[int]:
        data = self.read(addr, 2)
        return None if data is None else struct.unpack("<H", data)[0]

    def s16(self, addr: int) -> Optional[int]:
        data = self.read(addr, 2)
        return None if data is None else struct.unpack("<h", data)[0]

    def u32(self, addr: int) -> Optional[int]:
        data = self.read(addr, 4)
        return None if data is None else struct.unpack("<I", data)[0]

    def f32(self, addr: int) -> Optional[float]:
        data = self.read(addr, 4)
        return None if data is None else struct.unpack("<f", data)[0]


def load_snapshot(path: Path) -> tuple[dict[str, Any], SparseMemory]:
    with path.open(encoding="utf-8") as f:
        snap = json.load(f)
    regions = snap.get("regions")
    if not isinstance(regions, dict):
        raise SystemExit(f"{path}: expected flat snapshot JSON with a regions object")
    return snap, SparseMemory(regions)


def hex_or_none(value: Optional[int]) -> str:
    if value is None:
        return "NA"
    return f"0x{value:08x}"


def value_or_na(value: Any) -> str:
    if value is None:
        return "NA"
    if isinstance(value, float):
        if not math.isfinite(value):
            return str(value)
        return f"{value:.3f}"
    return str(value)


def yes_no(value: bool) -> str:
    return "yes" if value else "no"


def pointer_status(mem: SparseMemory, ptr_addr: int) -> dict[str, Any]:
    ptr = mem.u32(ptr_addr)
    return {
        "root": ptr_addr,
        "root_mapped": ptr is not None,
        "ptr": ptr,
        "target_mapped": ptr is not None and mem.contains(ptr, 4),
    }


def replay_readiness_warnings(mem: SparseMemory) -> list[str]:
    warnings = []
    scenario = pointer_status(mem, GLOBAL_SCENARIO_PTR)
    bsp = pointer_status(mem, GLOBAL_BSP_PTR)

    if not scenario["root_mapped"]:
        warnings.append("missing global_scenario root at 0x005064e4")
    elif not scenario["target_mapped"]:
        warnings.append(
            "global_scenario points to "
            f"{hex_or_none(scenario['ptr'])}, but that target is not mapped")

    if not bsp["root_mapped"]:
        warnings.append("missing global_bsp root at 0x005064e0")
    elif bsp["ptr"] and not bsp["target_mapped"]:
        warnings.append(
            f"global_bsp points to {hex_or_none(bsp['ptr'])}, "
            "but that target is not mapped")

    for addr in PATH_REFRESH_DATA_WINDOWS:
        if not mem.contains(addr, 4):
            warnings.append(
                f"missing actor_path_refresh data window at 0x{addr:08x}")

    return warnings


def data_table(mem: SparseMemory, ptr_addr: int, label: str) -> dict[str, Any]:
    table = mem.u32(ptr_addr)
    if table is None:
        raise SystemExit(f"missing {label} pointer at 0x{ptr_addr:08x}")

    name_bytes = mem.read(table, 32)
    size = mem.u16(table + 0x22)
    magic = mem.u32(table + 0x28)
    current_count = mem.u16(table + 0x2E)
    data = mem.u32(table + 0x34)
    maximum_count = mem.u16(table + 0x20)
    name = b""
    if name_bytes is not None:
        name = name_bytes.split(b"\0", 1)[0]

    return {
        "ptr": table,
        "name": name.decode("ascii", "replace"),
        "maximum_count": maximum_count,
        "size": size,
        "magic": magic,
        "current_count": current_count,
        "data": data,
    }


def require_actor_table(mem: SparseMemory) -> dict[str, Any]:
    table = data_table(mem, ACTOR_TABLE_PTR, "actor_data")
    if table["magic"] != DATA_T_MAGIC:
        raise SystemExit(
            f"actor_data magic mismatch at {hex_or_none(table['ptr'])}: "
            f"{hex_or_none(table['magic'])}, expected 0x{DATA_T_MAGIC:08x}")
    if table["size"] != ACTOR_SIZE_EXPECTED:
        raise SystemExit(
            f"actor_data element size is {value_or_na(table['size'])}, "
            f"expected 0x{ACTOR_SIZE_EXPECTED:x}")
    if table["data"] is None:
        raise SystemExit("actor_data header is missing data pointer")
    return table


def actor_base(table: dict[str, Any], actor_handle: int) -> int:
    index = actor_handle & 0xFFFF
    return int(table["data"]) + index * int(table["size"])


def actor_handle_from_slot(mem: SparseMemory, table: dict[str, Any],
                           index: int) -> Optional[int]:
    base = int(table["data"]) + index * int(table["size"])
    salt = mem.u16(base)
    if not salt:
        return None
    return (salt << 16) | index


def actor_fields(mem: SparseMemory, base: int) -> dict[str, Any]:
    x = mem.f32(base + 0x12C)
    y = mem.f32(base + 0x130)
    z = mem.f32(base + 0x134)
    dx = mem.f32(base + 0x488)
    dy = mem.f32(base + 0x48C)
    dz = mem.f32(base + 0x490)
    d2 = None
    if None not in (x, y, z, dx, dy, dz):
        d2 = (dx - x) * (dx - x) + (dy - y) * (dy - y) + (dz - z) * (dz - z)

    return {
        "encounter": mem.u32(base + 0x34),
        "action": mem.u8(base + 0x6A),
        "behavior": mem.u8(base + 0xC0),
        "awareness": mem.s16(base + 0x268),
        "move_source": mem.s16(base + 0x46C),
        "firing_position": mem.s16(base + 0x470),
        "sticky": mem.u8(base + 0x484),
        "dest_x": dx,
        "dest_y": dy,
        "dest_z": dz,
        "path_target": mem.u8(base + 0x4A4),
        "path_active": mem.s16(base + 0x4A8),
        "check_dest": mem.s16(base + 0x494),
        "stored_distance": mem.f32(base + 0x498),
        "distance_sq": d2,
        "pos_x": x,
        "pos_y": y,
        "pos_z": z,
    }


def print_actor(handle: int, fields: dict[str, Any]) -> None:
    print(
        f"  idx=0x{handle & 0xffff:03x} handle=0x{handle:08x} "
        f"enc={hex_or_none(fields['encounter'])} "
        f"msrc={value_or_na(fields['move_source'])} "
        f"aw={value_or_na(fields['awareness'])} "
        f"action={value_or_na(fields['action'])} "
        f"beh={value_or_na(fields['behavior'])} "
        f"fp={value_or_na(fields['firing_position'])} "
        f"patt={value_or_na(fields['path_target'])} "
        f"pact={value_or_na(fields['path_active'])} "
        f"sticky={value_or_na(fields['sticky'])} "
        f"chk={value_or_na(fields['check_dest'])} "
        f"dstore={value_or_na(fields['stored_distance'])} "
        f"d2={value_or_na(fields['distance_sq'])}")


def inspect_snapshot(args: argparse.Namespace) -> int:
    _snap, mem = load_snapshot(args.snapshot)
    actor_tbl = require_actor_table(mem)
    object_tbl = data_table(mem, OBJECT_TABLE_PTR, "object_data")
    zero_vec = mem.u32(ZERO_VECTOR_PTR)
    scenario = pointer_status(mem, GLOBAL_SCENARIO_PTR)
    bsp = pointer_status(mem, GLOBAL_BSP_PTR)
    warnings = replay_readiness_warnings(mem)

    print(f"snapshot: {args.snapshot}")
    print(
        f"roots: actor_data={hex_or_none(actor_tbl['ptr'])} "
        f"object_data={hex_or_none(object_tbl['ptr'])} "
        f"zero_vec={hex_or_none(zero_vec)}")
    print(
        f"scenario roots: global_scenario={hex_or_none(scenario['ptr'])} "
        f"target_mapped={yes_no(scenario['target_mapped'])} "
        f"global_bsp={hex_or_none(bsp['ptr'])} "
        f"target_mapped={yes_no(bsp['target_mapped'])}")
    if warnings:
        print("replay readiness: incomplete")
        for warning in warnings:
            print(f"  warning: {warning}")
    else:
        print("replay readiness: complete")
    print(
        f"actor table: name={actor_tbl['name']!r} "
        f"size=0x{actor_tbl['size']:x} current={actor_tbl['current_count']} "
        f"max={actor_tbl['maximum_count']} data={hex_or_none(actor_tbl['data'])}")

    max_count = int(actor_tbl["maximum_count"] or 0)
    shown = 0
    for index in range(max_count):
        handle = actor_handle_from_slot(mem, actor_tbl, index)
        if handle is None:
            continue
        fields = actor_fields(mem, actor_base(actor_tbl, handle))
        if args.encounter is not None and fields["encounter"] != args.encounter:
            continue
        if args.move_source is not None and fields["move_source"] != args.move_source:
            continue
        print_actor(handle, fields)
        shown += 1
        if args.limit and shown >= args.limit:
            break

    if shown == 0:
        print("no matching actors")
    return 0


def validate_actor_for_prepare(mem: SparseMemory, table: dict[str, Any],
                               actor_handle: int,
                               require_encounter: Optional[int],
                               require_msrc3: bool) -> dict[str, Any]:
    index = actor_handle & 0xFFFF
    live_handle = actor_handle_from_slot(mem, table, index)
    if live_handle != actor_handle:
        raise SystemExit(
            f"actor handle 0x{actor_handle:08x} does not match snapshot slot "
            f"0x{index:04x}; live={hex_or_none(live_handle)}")

    fields = actor_fields(mem, actor_base(table, actor_handle))
    if require_encounter is not None and fields["encounter"] != require_encounter:
        raise SystemExit(
            f"actor 0x{actor_handle:08x} encounter is "
            f"{hex_or_none(fields['encounter'])}, expected "
            f"0x{require_encounter:08x}")
    if require_msrc3 and fields["move_source"] != 3:
        raise SystemExit(
            f"actor 0x{actor_handle:08x} move_source is "
            f"{value_or_na(fields['move_source'])}, expected 3. "
            "Use --allow-any-msrc only for plumbing checks, not for the "
            "door-grunt move-commit oracle.")
    return fields


def prepare_snapshot(args: argparse.Namespace) -> int:
    snap, mem = load_snapshot(args.snapshot)
    table = require_actor_table(mem)
    warnings = replay_readiness_warnings(mem)
    fields = validate_actor_for_prepare(
        mem,
        table,
        args.actor_handle,
        args.require_encounter,
        not args.allow_any_msrc,
    )

    prepared = dict(snap)
    overrides = dict(prepared.get("arg_overrides") or {})
    overrides.update({
        "actor_handle": args.actor_handle,
        "store_distance": args.store_distance,
        "override_path": args.override_path,
    })
    prepared["arg_overrides"] = overrides
    prepared["a10_actor_path_refresh"] = {
        "actor_handle": f"0x{args.actor_handle:08x}",
        "actor_index": args.actor_handle & 0xFFFF,
        "encounter": hex_or_none(fields["encounter"]),
        "move_source": fields["move_source"],
        "path_active": fields["path_active"],
        "path_target": fields["path_target"],
        "distance_sq": fields["distance_sq"],
        "replay_warnings": warnings,
    }
    prepared["description"] = (
        prepared.get("description") or "a10 actor_path_refresh snapshot")

    if args.out.exists() and not args.force:
        raise SystemExit(f"{args.out} exists; pass --force to overwrite")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(prepared, indent=2) + "\n", encoding="utf-8")

    print(f"prepared: {args.out}")
    print_actor(args.actor_handle, fields)
    if warnings:
        print("replay readiness: incomplete")
        for warning in warnings:
            print(f"  warning: {warning}")
    print(f"arg_overrides: {overrides}")
    return 0


def run_unicorn(args: argparse.Namespace) -> int:
    if not args.snapshot.exists():
        raise SystemExit(f"snapshot not found: {args.snapshot}")

    cmd = [
        sys.executable,
        "tools/equivalence/unicorn_diff.py",
        TARGET,
        "--state-snapshot",
        str(args.snapshot),
        "--allow-stubs",
        "--mem-trace",
        "--float-tolerance",
        "32",
        "--seeds",
        str(args.seeds),
        "--output-json",
        str(args.output_json),
        "--max-insn",
        str(args.max_insn),
    ]
    if args.real_callees:
        cmd.append("--real-callees")
    if args.quiet:
        cmd.append("--quiet")

    env = os.environ.copy()
    env.setdefault("BIPED_SIBLING_RESOLVE", "1")
    if args.heap_compare:
        env["BIPED_HEAP_COMPARE"] = "1"

    print("running:", " ".join(cmd))
    proc = subprocess.run(cmd, cwd=str(ROOT), env=env)
    return int(proc.returncode)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate and run a10 actor_path_refresh snapshot equivalence")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_inspect = sub.add_parser("inspect")
    p_inspect.add_argument("snapshot", type=Path)
    p_inspect.add_argument("--encounter", type=lambda x: int(x, 0),
                           default=DOOR_ENCOUNTER)
    p_inspect.add_argument("--move-source", type=lambda x: int(x, 0),
                           default=None)
    p_inspect.add_argument("--limit", type=int, default=0)
    p_inspect.set_defaults(func=inspect_snapshot)

    p_prepare = sub.add_parser("prepare")
    p_prepare.add_argument("snapshot", type=Path)
    p_prepare.add_argument("--actor-handle", type=lambda x: int(x, 0),
                           required=True)
    p_prepare.add_argument("--out", type=Path, required=True)
    p_prepare.add_argument("--store-distance", type=lambda x: int(x, 0),
                           default=1)
    p_prepare.add_argument("--override-path", type=lambda x: int(x, 0),
                           default=0)
    p_prepare.add_argument("--require-encounter", type=lambda x: int(x, 0),
                           default=DOOR_ENCOUNTER)
    p_prepare.add_argument("--allow-any-msrc", action="store_true")
    p_prepare.add_argument("--force", action="store_true")
    p_prepare.set_defaults(func=prepare_snapshot)

    p_run = sub.add_parser("run")
    p_run.add_argument("snapshot", type=Path)
    p_run.add_argument("--output-json", type=Path,
                       default=ROOT / "artifacts/equivalence/original/a10_actor_path_refresh.json")
    p_run.add_argument("--seeds", type=int, default=1)
    p_run.add_argument("--max-insn", type=int, default=2000000)
    p_run.add_argument("--real-callees", dest="real_callees",
                       action="store_true", default=True)
    p_run.add_argument("--no-real-callees", dest="real_callees",
                       action="store_false")
    p_run.add_argument("--heap-compare", action="store_true", default=True)
    p_run.add_argument("--quiet", action="store_true")
    p_run.set_defaults(func=run_unicorn)

    return parser


def main() -> int:
    args = build_parser().parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
