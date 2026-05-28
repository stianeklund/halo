#!/usr/bin/env python3
"""Capture Xbox screenshots over XBDM/RDCP.

This wrapper invokes the one-shot XBDM RDCP client once per frame, so each
capture starts on a fresh RDCP connection.

The XBDM screenshot payload normally looks like this:

    pitch=0x00000a00 width=0x00000280 height=0x000001e0 format=0x00000012, framebuffersize=0x0012c000\r\n
    <raw framebuffer bytes>

For a normal 640x480 Xbox framebuffer, format 0x12 appears to be a 32-bit
BGRX/XRGB-style layout in memory:

    byte 0 = B
    byte 1 = G
    byte 2 = R
    byte 3 = unused / alpha-like byte, often 0

If that fourth byte is passed through as PNG alpha, the PNG can appear blank.
This script therefore writes PNGs as:

    R G B 255
"""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[2]

DEFAULT_OUTPUT_DIR = ROOT_DIR / "xbdm" / "screenshots"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_COUNT = 1
DEFAULT_PORT = 731

DEFAULT_TIMEOUT = 30.0

HEADER_FIELD_RE = re.compile(rb"([A-Za-z_]+)=0x([0-9a-fA-F]+)")
HEADER_START_RE = re.compile(rb"pitch=0x[0-9a-fA-F]+")
SUPPORTED_32BPP_FORMATS = {
    0x12,  # observed Xbox screenshot format, BGRX/X8R8G8B8-like in memory
}


def parse_int(value: str) -> int:
    """argparse helper that accepts decimal or 0x-prefixed integers."""
    try:
        return int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid integer: {value!r}") from exc


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture XBDM screenshot payloads and optionally convert them to PNG."
    )

    parser.add_argument(
        "--count",
        "--images",
        dest="count",
        type=int,
        default=DEFAULT_COUNT,
        help=f"number of screenshots to capture in sequence (default: {DEFAULT_COUNT})",
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
        default=DEFAULT_HOST,
        help=f"XBDM host or IP address (default: {DEFAULT_HOST}).",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=None,
        help="XBDM port override.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT,
        help=f"socket timeout in seconds (default: {DEFAULT_TIMEOUT})",
    )
    parser.add_argument(
        "--binary-length",
        type=parse_int,
        help=(
            "override bytes to read after the screenshot header. Accepts decimal or "
            "hex. By default, the script reads the header first and then auto-detects "
            "the framebuffer length from framebuffersize or pitch*height."
        ),
    )

    parser.add_argument(
        "--png",
        action="store_true",
        help="write PNG output instead of the default raw .bin output.",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="also write raw .bin output. With no flags, raw .bin is still the default.",
    )
    parser.add_argument(
        "--keep-response",
        action="store_true",
        help=(
            "write the full original XBDM response to .bin, including the text header. "
            "By default the .bin sidecar contains only decoded framebuffer bytes."
        ),
    )
    parser.add_argument(
        "--dump-header",
        action="store_true",
        help="print parsed screenshot header fields for each capture.",
    )

    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    if args.count <= 0:
        raise ValueError("--count must be positive")
    if args.start_index <= 0:
        raise ValueError("--start-index must be positive")
    if args.interval < 0:
        raise ValueError("--interval must not be negative")
    if args.timeout <= 0:
        raise ValueError("--timeout must be positive")
    if args.binary_length is not None and args.binary_length <= 0:
        raise ValueError("--binary-length must be positive")

def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def find_header(payload: bytes) -> tuple[int, int]:
    """Return (header_start, body_start) inside a screenshot payload."""
    match = HEADER_START_RE.search(payload)
    if not match:
        raise RuntimeError(
            "screenshot payload did not contain a recognizable 'pitch=0x...' header"
        )

    header_start = match.start()

    crlf = payload.find(b"\r\n", header_start)
    lf = payload.find(b"\n", header_start)

    if crlf != -1 and (lf == -1 or crlf <= lf):
        return header_start, crlf + 2

    if lf != -1:
        return header_start, lf + 1

    raise RuntimeError("screenshot payload header was found, but no line ending followed it")


def parse_header(header: bytes) -> dict[str, int]:
    fields: dict[str, int] = {}

    for name, value in HEADER_FIELD_RE.findall(header):
        key = name.decode("ascii", errors="strict").lower()
        fields[key] = int(value, 16)

    required = ("pitch", "width", "height")
    missing = [field for field in required if field not in fields]
    if missing:
        raise RuntimeError(
            "screenshot header missing required field(s): " + ", ".join(missing)
        )

    width = fields["width"]
    height = fields["height"]
    pitch = fields["pitch"]

    if width <= 0 or height <= 0 or pitch <= 0:
        raise RuntimeError(f"invalid screenshot dimensions in header: {fields}")

    if pitch < width * 4:
        raise RuntimeError(
            f"pitch is too small for 32-bit pixels: pitch={pitch}, width={width}"
        )

    return fields


def decode_screenshot_payload(payload: bytes) -> tuple[dict[str, int], bytes]:
    """Extract raw framebuffer bytes from an XBDM screenshot response."""
    header_start, body_start = find_header(payload)
    header = payload[header_start:body_start].strip()
    fields = parse_header(header)

    width = fields["width"]
    height = fields["height"]
    pitch = fields["pitch"]

    expected_from_pitch = pitch * height
    expected_from_header = fields.get("framebuffersize")

    if expected_from_header is not None:
        expected = expected_from_header
        if expected != expected_from_pitch:
            print(
                "warning: framebuffersize does not match pitch*height: "
                f"framebuffersize={expected}, pitch*height={expected_from_pitch}; "
                "using framebuffersize",
                file=sys.stderr,
            )
    else:
        expected = expected_from_pitch

    body = payload[body_start:]

    if len(body) < expected:
        print(
            f"warning: screenshot pixel data short by {expected - len(body)} byte(s); "
            "padding with zeroes",
            file=sys.stderr,
        )
        body += b"\x00" * (expected - len(body))
    elif len(body) > expected:
        body = body[:expected]

    # If framebuffersize was used and it is somehow not enough for pitch*height,
    # pad again so row conversion does not read past the buffer.
    minimum_for_rows = pitch * height
    if len(body) < minimum_for_rows:
        print(
            f"warning: framebuffer shorter than pitch*height by "
            f"{minimum_for_rows - len(body)} byte(s); padding with zeroes",
            file=sys.stderr,
        )
        body += b"\x00" * (minimum_for_rows - len(body))

    return fields, body


def expected_framebuffer_length(fields: dict[str, int]) -> int:
    expected_from_pitch = fields["pitch"] * fields["height"]
    expected_from_header = fields.get("framebuffersize")

    if expected_from_header is not None:
        return expected_from_header

    return expected_from_pitch


def parse_rdcp_status_line(line: bytes) -> tuple[int, str]:
    if len(line) < 5 or line[3:4] != b"-":
        raise RuntimeError(f"malformed RDCP response: {line!r}")

    try:
        code = int(line[:3])
    except ValueError as exc:
        raise RuntimeError(f"invalid RDCP status code: {line!r}") from exc

    return code, line[4:].decode("ascii", errors="replace")


def read_socket_line(fileobj) -> bytes:
    try:
        raw = fileobj.readline()
    except OSError as exc:
        raise RuntimeError(f"failed to read response line: {exc}") from exc

    if not raw:
        raise RuntimeError("connection closed while reading response")
    if raw.endswith(b"\r\n"):
        return raw[:-2]
    if raw.endswith(b"\n"):
        return raw[:-1]
    if raw.endswith(b"\r"):
        return raw[:-1]
    return raw


def read_socket_exact(fileobj, length: int) -> bytes:
    chunks = bytearray()
    remaining = length

    while remaining > 0:
        try:
            chunk = fileobj.read(remaining)
        except OSError as exc:
            raise RuntimeError(f"failed to read binary payload: {exc}") from exc

        if not chunk:
            raise RuntimeError(
                f"connection closed with {remaining} binary bytes remaining"
            )

        chunks.extend(chunk)
        remaining -= len(chunk)

    return bytes(chunks)


def framebuffer_to_rgba(framebuffer: bytes, fields: dict[str, int]) -> bytes:
    """Convert Xbox 32-bit BGRX/BGRA-like framebuffer bytes to PNG-friendly RGBA."""
    width = fields["width"]
    height = fields["height"]
    pitch = fields["pitch"]
    fmt = fields.get("format")

    if fmt is not None and fmt not in SUPPORTED_32BPP_FORMATS:
        print(
            f"warning: screenshot format 0x{fmt:x} is not explicitly known; "
            "assuming 32-bit B,G,R,X byte order",
            file=sys.stderr,
        )

    rgba = bytearray(width * height * 4)

    dst = 0
    for y in range(height):
        row_start = y * pitch
        row = framebuffer[row_start : row_start + width * 4]

        if len(row) < width * 4:
            row += b"\x00" * (width * 4 - len(row))

        for x in range(width):
            src = x * 4

            b = row[src + 0]
            g = row[src + 1]
            r = row[src + 2]

            rgba[dst + 0] = r
            rgba[dst + 1] = g
            rgba[dst + 2] = b
            rgba[dst + 3] = 255

            dst += 4

    return bytes(rgba)


def write_png(output_path: Path, framebuffer: bytes, fields: dict[str, int]) -> None:
    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError(
            "Pillow is required for --png output. Install it with: python -m pip install pillow"
        ) from exc

    width = fields["width"]
    height = fields["height"]

    rgba = framebuffer_to_rgba(framebuffer, fields)
    image = Image.frombytes("RGBA", (width, height), rgba)
    image.save(output_path)


def capture_one(args: argparse.Namespace) -> bytes:
    try:
        port = args.port if args.port is not None else DEFAULT_PORT
        with socket.create_connection((args.host, port), timeout=args.timeout) as sock:
            sock.settimeout(args.timeout)
            fileobj = sock.makefile("rwb", buffering=0)

            # Consume the initial 201 banner, then request the screenshot.
            read_socket_line(fileobj)
            fileobj.write(b"screenshot\r\n")

            status_line = read_socket_line(fileobj)
            code, message = parse_rdcp_status_line(status_line)
            if code != 203:
                raise RuntimeError(f"screenshot failed: {code}- {message}".rstrip())

            header_line = read_socket_line(fileobj)
            header = header_line.strip()
            fields = parse_header(header)
            body_length = args.binary_length
            if body_length is None:
                body_length = expected_framebuffer_length(fields)
            body = read_socket_exact(fileobj, body_length)
            return header_line + b"\r\n" + body
    except OSError as exc:
        raise RuntimeError(str(exc)) from exc


def print_header(index: int, fields: dict[str, int]) -> None:
    ordered = ["pitch", "width", "height", "format", "framebuffersize"]
    parts: list[str] = []

    for key in ordered:
        if key in fields:
            parts.append(f"{key}=0x{fields[key]:x}")

    for key in sorted(fields):
        if key not in ordered:
            parts.append(f"{key}=0x{fields[key]:x}")

    print(f"[{index}] header: " + " ".join(parts))


def main() -> int:
    try:
        args = parse_args()
        validate_args(args)

        output_dir = Path(args.output_dir).expanduser().resolve()
        ensure_dir(output_dir)

        for offset in range(args.count):
            index = args.start_index + offset
            base_path = output_dir / f"{args.prefix}-{index:04d}"
            bin_path = base_path.with_suffix(".bin")
            png_path = base_path.with_suffix(".png")

            response = capture_one(args)
            fields, framebuffer = decode_screenshot_payload(response)

            if args.dump_header:
                print_header(index, fields)

            write_raw = args.raw or not args.png

            if write_raw:
                if args.keep_response:
                    bin_path.write_bytes(response)
                    print(f"[{index}] saved full XBDM response, {len(response)} bytes, to {bin_path}")
                else:
                    bin_path.write_bytes(framebuffer)
                    print(f"[{index}] saved framebuffer, {len(framebuffer)} bytes, to {bin_path}")

            if args.png:
                write_png(png_path, framebuffer, fields)
                print(f"[{index}] saved PNG to {png_path}")

            if args.interval > 0.0 and offset + 1 < args.count:
                time.sleep(args.interval)

        return 0

    except (RuntimeError, ValueError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
