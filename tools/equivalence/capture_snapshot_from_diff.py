#!/usr/bin/env python3
"""Capture xemu memory snapshot regions inferred from unicorn_diff JSON output.

Typical usage:
  rtk python3 tools/equivalence/capture_snapshot_from_diff.py \
      artifacts/equivalence/FUN_0002a360.json --dry-run
  rtk python3 tools/equivalence/capture_snapshot_from_diff.py \
      artifacts/equivalence/FUN_0002a360.json
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

from state_snapshot import capture_from_xbdm, capture_from_xemu


ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_OUT_DIR = ROOT / "artifacts" / "snapshots"
DEFAULT_PAGE_SIZE = 0x10000


def parse_int(text: str) -> int:
    return int(text, 0)


def parse_region_spec(spec: str) -> tuple[int, int]:
    if ":" not in spec:
        raise ValueError(f"region must be ADDR:SIZE, got: {spec}")
    addr_text, size_text = spec.split(":", 1)
    addr = parse_int(addr_text)
    size = parse_int(size_text)
    if addr < 0 or size <= 0:
        raise ValueError(f"invalid region {spec}: addr>=0 and size>0 required")
    return addr, size


def page_base(addr: int, page_size: int) -> int:
    return addr & ~(page_size - 1)


def load_diff_json(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("diff JSON root must be an object")
    return data


def build_regions(
    diff_data: dict,
    page_size: int,
    include_global_read_pages: bool,
    extra_regions: list[tuple[int, int]],
) -> list[tuple[int, int]]:
    regions: dict[int, int] = {}

    for raw_page in diff_data.get("auto_mapped_pages", []):
        if not isinstance(raw_page, str):
            continue
        addr = parse_int(raw_page)
        regions[page_base(addr, page_size)] = max(
            regions.get(page_base(addr, page_size), 0), page_size
        )

    if include_global_read_pages:
        for item in diff_data.get("global_reads", []):
            if not isinstance(item, dict):
                continue
            addr_text = item.get("address")
            if not isinstance(addr_text, str):
                continue
            addr = parse_int(addr_text)
            base = page_base(addr, page_size)
            regions[base] = max(regions.get(base, 0), page_size)

    for addr, size in extra_regions:
        regions[addr] = max(regions.get(addr, 0), size)

    return sorted(regions.items())


def default_output_path(diff_json: Path, target: str) -> Path:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    stem = target if target else diff_json.stem
    safe_stem = "".join(ch if (ch.isalnum() or ch in "._-") else "_" for ch in stem)
    return DEFAULT_OUT_DIR / f"{safe_stem}-{stamp}.json"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture xemu snapshot regions inferred from unicorn_diff JSON"
    )
    parser.add_argument(
        "diff_json",
        type=Path,
        help="Path to unicorn_diff --output-json result",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output snapshot path (default: artifacts/snapshots/<target>-<timestamp>.json)",
    )
    parser.add_argument(
        "--page-size",
        type=parse_int,
        default=DEFAULT_PAGE_SIZE,
        help=f"Page size for auto/global capture (default: 0x{DEFAULT_PAGE_SIZE:x})",
    )
    parser.add_argument(
        "--no-global-read-pages",
        action="store_true",
        help="Ignore global_reads pages and only use auto_mapped_pages",
    )
    parser.add_argument(
        "--extra-region",
        action="append",
        default=[],
        metavar="ADDR:SIZE",
        help="Add extra region(s), e.g. --extra-region 0x456600:0x2000",
    )
    parser.add_argument(
        "--description",
        default="",
        help="Optional description stored in snapshot metadata",
    )
    parser.add_argument(
        "--backend",
        choices=("qmp", "xbdm"),
        default="qmp",
        help="Capture backend: qmp uses xemu pmemsave, xbdm uses RDCP getmem",
    )
    parser.add_argument(
        "--xbdm-host",
        default="localhost",
        help="XBDM host when --backend xbdm is used",
    )
    parser.add_argument(
        "--xbdm-port",
        type=int,
        default=731,
        help="XBDM port when --backend xbdm is used",
    )
    parser.add_argument(
        "--xbdm-timeout",
        type=float,
        default=5.0,
        help="XBDM socket timeout when --backend xbdm is used",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print planned regions without capturing",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()

    if args.page_size <= 0 or (args.page_size & (args.page_size - 1)) != 0:
        raise SystemExit("--page-size must be a positive power of two")

    if not args.diff_json.exists():
        raise SystemExit(f"diff JSON not found: {args.diff_json}")

    diff_data = load_diff_json(args.diff_json)
    target = str(diff_data.get("target", "")).strip()

    extra_regions = [parse_region_spec(spec) for spec in args.extra_region]
    regions = build_regions(
        diff_data,
        page_size=args.page_size,
        include_global_read_pages=not args.no_global_read_pages,
        extra_regions=extra_regions,
    )

    if not regions:
        raise SystemExit(
            "No capture regions found. Re-run unicorn_diff with --allow-stubs --mem-trace "
            "and --output-json, then retry."
        )

    total_bytes = sum(size for _, size in regions)
    print(f"target: {target or '<unknown>'}")
    print(f"diff:   {args.diff_json}")
    print(f"regions: {len(regions)}")
    print(f"bytes:  0x{total_bytes:x} ({total_bytes:,})")
    for addr, size in regions:
        print(f"  - 0x{addr:08x}:0x{size:x}")

    if args.dry_run:
        return 0

    output_path = args.output if args.output else default_output_path(args.diff_json, target)
    output_path = output_path.resolve()

    auto_pages = len(diff_data.get("auto_mapped_pages", []) or [])
    global_reads = len(diff_data.get("global_reads", []) or [])
    desc_parts = [
        args.description.strip(),
        f"source={args.diff_json.name}",
        f"target={target or 'unknown'}",
        f"backend={args.backend}",
        f"regions={len(regions)}",
        f"auto_mapped_pages={auto_pages}",
        f"global_reads={global_reads}",
    ]
    description = "; ".join(part for part in desc_parts if part)

    if args.backend == "xbdm":
        captured = capture_from_xbdm(
            regions,
            output_path=str(output_path),
            description=description,
            host=args.xbdm_host,
            port=args.xbdm_port,
            timeout=args.xbdm_timeout,
        )
    else:
        captured = capture_from_xemu(
            regions,
            output_path=str(output_path),
            description=description,
        )

    missing = [addr for addr, _ in regions if addr not in captured]
    captured_bytes = sum(len(data) for data in captured.values())

    print(f"saved: {output_path}")
    print(f"captured regions: {len(captured)}/{len(regions)}")
    print(f"captured bytes:   0x{captured_bytes:x} ({captured_bytes:,})")
    if missing:
        print("warning: missing region base(s):")
        for addr in missing:
            print(f"  - 0x{addr:08x}")
        return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
