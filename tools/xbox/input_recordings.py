#!/usr/bin/env python3
"""Manage the local per-level Halo Xbox input recording corpus."""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "input-recordings"
MANIFEST = CORPUS / "manifest.json"
PACKET_SIZE = 0x28
CONTROLLERS_PER_TICK = 4

LEVEL_MAPS = {
    "a10": "levels\\a10\\a10",
    "a30": "levels\\a30\\a30",
    "a50": "levels\\a50\\a50",
    "b30": "levels\\b30\\b30",
    "b40": "levels\\b40\\b40",
    "c10": "levels\\c10\\c10",
    "c20": "levels\\c20\\c20",
    "c40": "levels\\c40\\c40",
    "d20": "levels\\d20\\d20",
    "d40": "levels\\d40\\d40",
}


def slugify(text: str) -> str:
    slug = re.sub(r"[^a-zA-Z0-9]+", "-", text.strip().lower()).strip("-")
    return slug or "recording"


def load_manifest() -> dict:
    if not MANIFEST.exists():
        return {"version": 1, "recordings": []}
    with MANIFEST.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("version") != 1 or not isinstance(data.get("recordings"), list):
        raise ValueError(f"unsupported manifest format: {MANIFEST}")
    return data


def save_manifest(data: dict) -> None:
    CORPUS.mkdir(parents=True, exist_ok=True)
    with MANIFEST.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
        handle.write("\n")


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def read_source_bytes(path: Path, strip_rdcp_prefix: bool) -> bytes:
    data = path.read_bytes()
    if not strip_rdcp_prefix:
        return data
    if len(data) < 4:
        raise ValueError("RDCP-prefixed file is shorter than 4 bytes")
    reported = int.from_bytes(data[:4], "little")
    payload = data[4:]
    if reported != len(payload):
        raise ValueError(
            f"RDCP prefix reports {reported} bytes, payload has {len(payload)}"
        )
    return payload


def estimate_ticks(size: int) -> int:
    tick_bytes = PACKET_SIZE * CONTROLLERS_PER_TICK
    return size // tick_bytes


def cmd_add(args: argparse.Namespace) -> int:
    source = Path(args.source)
    if not source.is_file():
        print(f"error: source not found: {source}", file=sys.stderr)
        return 1

    level = args.level.lower()
    map_name = args.map_name or LEVEL_MAPS.get(level)
    if not map_name:
        print(f"error: unknown level '{args.level}', pass --map-name", file=sys.stderr)
        return 1

    rec_id = args.id or f"{_dt.datetime.utcnow():%Y%m%d}-{level}-{slugify(args.title)}"
    rel_dir = Path("levels") / level / rec_id
    out_dir = CORPUS / rel_dir
    out_state = out_dir / "state.data"
    if out_dir.exists() and not args.allow_overwrite:
        print(f"error: recording already exists: {out_dir}", file=sys.stderr)
        return 1

    try:
        payload = read_source_bytes(source, args.strip_rdcp_prefix)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    out_dir.mkdir(parents=True, exist_ok=True)
    out_state.write_bytes(payload)
    digest = sha256_file(out_state)
    size = out_state.stat().st_size

    entry = {
        "id": rec_id,
        "level": level,
        "map_name": map_name,
        "title": args.title,
        "purpose": args.purpose,
        "path": str(out_state.relative_to(ROOT)).replace("\\", "/"),
        "state_data_sha256": digest,
        "state_data_bytes": size,
        "packet_size": PACKET_SIZE,
        "controllers_per_tick": CONTROLLERS_PER_TICK,
        "estimated_ticks": estimate_ticks(size),
        "build": args.build,
        "difficulty": args.difficulty,
        "start_condition": args.start_condition,
        "created_utc": _dt.datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
        "known_good": args.known_good,
        "notes": args.notes,
    }

    with (out_dir / "recording.json").open("w", encoding="utf-8") as handle:
        json.dump(entry, handle, indent=2, sort_keys=True)
        handle.write("\n")

    manifest = load_manifest()
    recordings = [r for r in manifest["recordings"] if r.get("id") != rec_id]
    if len(recordings) != len(manifest["recordings"]) and not args.allow_overwrite:
        print(f"error: manifest already contains id: {rec_id}", file=sys.stderr)
        shutil.rmtree(out_dir)
        return 1
    recordings.append(entry)
    recordings.sort(key=lambda r: (r.get("level", ""), r.get("id", "")))
    manifest["recordings"] = recordings
    save_manifest(manifest)

    print(json.dumps({"ok": True, "recording": entry}, indent=2, sort_keys=True))
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    rows = manifest["recordings"]
    if args.level:
        rows = [r for r in rows if r.get("level") == args.level.lower()]
    if args.json:
        print(json.dumps({"recordings": rows}, indent=2, sort_keys=True))
        return 0
    for rec in rows:
        print(
            f"{rec.get('id')}\t{rec.get('level')}\t{rec.get('estimated_ticks', '?')} ticks\t{rec.get('title')}"
        )
    return 0


def cmd_validate(_args: argparse.Namespace) -> int:
    manifest = load_manifest()
    ok = True
    for rec in manifest["recordings"]:
        path = ROOT / rec["path"]
        if not path.is_file():
            print(f"missing: {rec['id']} {path}", file=sys.stderr)
            ok = False
            continue
        size = path.stat().st_size
        digest = sha256_file(path)
        if size != rec.get("state_data_bytes"):
            print(f"size mismatch: {rec['id']} {size} != {rec.get('state_data_bytes')}", file=sys.stderr)
            ok = False
        if digest != rec.get("state_data_sha256"):
            print(f"sha mismatch: {rec['id']} {digest} != {rec.get('state_data_sha256')}", file=sys.stderr)
            ok = False
    print(json.dumps({"ok": ok, "recordings": len(manifest["recordings"])}))
    return 0 if ok else 1


def cmd_init(args: argparse.Namespace) -> int:
    level = args.level.lower()
    map_name = args.map_name or LEVEL_MAPS.get(level)
    if not map_name:
        print(f"error: unknown level '{args.level}', pass --map-name", file=sys.stderr)
        return 1
    print(f"map_name {map_name}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    add = sub.add_parser("add", help="Promote a state.data capture into the corpus")
    add.add_argument("--level", required=True, help="Level key, e.g. a10")
    add.add_argument("--map-name", default="", help="Override map name")
    add.add_argument("--source", required=True, help="Source state.data or RDCP raw file")
    add.add_argument("--strip-rdcp-prefix", action="store_true", help="Strip 4-byte getfile size prefix")
    add.add_argument("--id", default="", help="Stable recording id")
    add.add_argument("--title", required=True)
    add.add_argument("--purpose", required=True)
    add.add_argument("--build", default="")
    add.add_argument("--difficulty", default="")
    add.add_argument("--start-condition", default="")
    add.add_argument("--notes", default="")
    add.add_argument("--known-good", action="store_true")
    add.add_argument("--allow-overwrite", action="store_true")
    add.set_defaults(func=cmd_add)

    ls = sub.add_parser("list", help="List corpus recordings")
    ls.add_argument("--level", default="")
    ls.add_argument("--json", action="store_true")
    ls.set_defaults(func=cmd_list)

    val = sub.add_parser("validate", help="Validate corpus files against manifest")
    val.set_defaults(func=cmd_validate)

    init = sub.add_parser("init-line", help="Print an init.txt map_name line")
    init.add_argument("--level", required=True)
    init.add_argument("--map-name", default="")
    init.set_defaults(func=cmd_init)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
