#!/usr/bin/env python3
"""Capture a burst of Xbox screenshots over XBDM/RDCP.

This wrapper invokes the working one-shot XBDM client once per frame, so each
capture starts on a fresh RDCP connection. The emitted `.bin` is the decoded
framebuffer only, padded to the expected framebuffer size.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[2]
RDCP_SCRIPT = ROOT_DIR / "tools" / "xbox" / "xbdm_rdcp.py"
DEFAULT_OUTPUT_DIR = ROOT_DIR / "xbdm" / "screenshots"
DEFAULT_COUNT = 1
DEFAULT_BINARY_LENGTH = 0x12C000
DEFAULT_TIMEOUT = 30.0
HEADER_FIELD_RE = re.compile(rb"([a-z]+)=0x([0-9a-fA-F]+)")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture a burst of XBDM screenshot payloads"
    )
    parser.add_argument(
        "--count",
        type=int,
        default=DEFAULT_COUNT,
        help=f"number of screenshots to capture (default: {DEFAULT_COUNT})",
    )
    parser.add_argument(
        "--output-dir",
        default=str(DEFAULT_OUTPUT_DIR),
        help=f"directory for captured files (default: {DEFAULT_OUTPUT_DIR})",
    )
    parser.add_argument(
        "--prefix",
        default="shot",
        help='output filename prefix (default: "shot")',
    )
    parser.add_argument(
        "--start-index",
        type=int,
        default=1,
        help="first capture index (default: 1)",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=0.0,
        help="delay in seconds between captures (default: 0)",
    )
    parser.add_argument(
        "--host",
        default=None,
        help="XBDM host override",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=None,
        help="XBDM port override",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT,
        help=f"socket timeout in seconds (default: {DEFAULT_TIMEOUT})",
    )
    parser.add_argument(
        "--binary-length",
        type=int,
        default=DEFAULT_BINARY_LENGTH,
        help=f"bytes to read from each screenshot payload (default: 0x{DEFAULT_BINARY_LENGTH:x})",
    )
    parser.add_argument(
        "--png",
        action="store_true",
        help="also write a PNG next to each captured binary payload",
    )
    parser.add_argument(
        "--png-only",
        action="store_true",
        help="write only PNG output and skip the .bin sidecar",
    )
    return parser.parse_args()


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def decode_screenshot_payload(payload: bytes) -> tuple[dict[str, int], bytes]:
    header, sep, body = payload.partition(b"\r\n")
    if not sep:
        raise RuntimeError("screenshot payload did not contain a header line")

    fields: dict[str, int] = {}
    for name, value in HEADER_FIELD_RE.findall(header):
        fields[name.decode("ascii")] = int(value, 16)

    if "width" not in fields or "height" not in fields:
        raise RuntimeError("screenshot header did not expose width/height")
    if "pitch" not in fields:
        raise RuntimeError("screenshot header did not expose pitch")

    expected = fields["pitch"] * fields["height"]
    if len(body) < expected:
        print(
            f"warning: screenshot pixel data short by {expected - len(body)} bytes; padding with zeroes",
            file=sys.stderr,
        )
        body = body + (b"\x00" * (expected - len(body)))
    elif len(body) > expected:
        body = body[:expected]

    return fields, body


def write_png(output_path: Path, framebuffer: bytes, fields: dict[str, int]) -> None:
    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError("Pillow is required for --png output") from exc

    width = fields["width"]
    height = fields["height"]
    pitch = fields["pitch"]

    image = Image.frombytes("RGBA", (width, height), framebuffer, "raw", "BGRA", pitch, 1)
    image.save(output_path)


def capture_one(args: argparse.Namespace) -> bytes:
    cmd = [
        sys.executable,
        str(RDCP_SCRIPT),
        "--timeout",
        str(args.timeout),
        "--binary-length",
        str(args.binary_length),
        "--output",
        "-",
    ]
    if args.host is not None:
        cmd.extend(["--host", args.host])
    if args.port is not None:
        cmd.extend(["--port", str(args.port)])
    cmd.append("screenshot")

    proc = subprocess.run(
        cmd,
        cwd=str(ROOT_DIR),
        capture_output=True,
    )
    if proc.returncode != 0:
        message = (
            proc.stderr.decode("utf-8", errors="replace").strip()
            or proc.stdout.decode("utf-8", errors="replace").strip()
            or "screenshot failed"
        )
        raise RuntimeError(message)
    return proc.stdout


def main() -> int:
    args = parse_args()

    if args.count <= 0:
        print("error: --count must be positive", file=sys.stderr)
        return 1
    if args.start_index <= 0:
        print("error: --start-index must be positive", file=sys.stderr)
        return 1
    if args.png_only and not args.png:
        args.png = True

    output_dir = Path(args.output_dir).expanduser().resolve()
    try:
        ensure_dir(output_dir)
        for offset in range(args.count):
            index = args.start_index + offset
            output_path = output_dir / f"{args.prefix}-{index:04d}.bin"
            payload = capture_one(args)
            fields, framebuffer = decode_screenshot_payload(payload)
            if not args.png_only:
                output_path.write_bytes(framebuffer)
            if args.png:
                png_path = output_path.with_suffix(".png")
                write_png(png_path, framebuffer, fields)
            if args.png_only:
                print(f"[{index}] saved PNG to {output_path.with_suffix('.png')}")
            else:
                print(f"[{index}] saved {len(framebuffer)} bytes to {output_path}")
            if args.interval > 0.0 and offset + 1 < args.count:
                time.sleep(args.interval)
        return 0
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
