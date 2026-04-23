#!/usr/bin/env python3
"""Fetch `debug.txt` from an Xbox over XBDM/RDCP.

Default mode copies the full file into the repo. Passing a non-zero line count
prints the last N lines instead, so `-5` or `-10` are convenient shortcuts.
"""

from __future__ import annotations

import argparse
import os
import re
import struct
import sys

from local_env import load_repo_env, maybe_reexec_on_windows


load_repo_env("xbox.env")
maybe_reexec_on_windows(__file__)

from xbdm_rdcp import RdcpClient, RdcpError, parse_int


ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
DEFAULT_REMOTE_PATH = r"E:\GAMES\halo-patched\debug.txt"
DEFAULT_OUTPUT_PATH = os.path.join(ROOT_DIR, "xbdm", "debug.txt")
DEFAULT_CHUNK_SIZE = 4096


def normalize_argv(argv: list[str]) -> list[str]:
    if len(argv) > 1 and re.fullmatch(r"-?\d+", argv[1]):
        return [argv[0], "--lines", argv[1], *argv[2:]]
    return argv


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Copy or tail Xbox debug.txt via XBDM"
    )
    parser.add_argument(
        "--lines",
        type=int,
        default=0,
        help="tail this many lines instead of copying the full file; -10 and -5 work too",
    )
    parser.add_argument(
        "--remote",
        default=DEFAULT_REMOTE_PATH,
        help=f"remote file path on the Xbox (default: {DEFAULT_REMOTE_PATH})",
    )
    parser.add_argument(
        "--output",
        help=f"output file path for full copies or tail text (default: {DEFAULT_OUTPUT_PATH})",
    )
    parser.add_argument(
        "--chunk-size",
        type=parse_int,
        default=DEFAULT_CHUNK_SIZE,
        help=f"initial tail read size in bytes (default: {DEFAULT_CHUNK_SIZE})",
    )
    parser.add_argument("--host", default=None, help="XBDM host override")
    parser.add_argument("--port", type=int, default=None, help="XBDM port override")
    parser.add_argument(
        "--timeout",
        type=float,
        default=None,
        help="socket timeout in seconds",
    )
    if argv is None:
        argv = sys.argv[1:]
    return parser.parse_args(normalize_argv([sys.argv[0], *argv])[1:])


def build_file_command(command: str, remote_path: str, **kwargs: int) -> str:
    parts = [command, f'name="{remote_path}"']
    for key, value in kwargs.items():
        parts.append(f"{key}={value}")
    return " ".join(parts)


def parse_size(lines: list[str]) -> int:
    if not lines:
        raise RdcpError("getfileattributes returned no data")

    fields: dict[str, str] = {}
    for line in lines:
        for token in line.replace(",", " ").split():
            if "=" not in token:
                continue
            key, value = token.split("=", 1)
            fields[key.lower()] = value

    for key in ("size", "filesize", "file_size", "length"):
        if key in fields:
            return int(fields[key], 0)

    for key, value in fields.items():
        if key.endswith("size") or key.endswith("length"):
            try:
                return int(value, 0)
            except ValueError:
                continue

    if "sizehi" in fields and "sizelo" in fields:
        try:
            size_hi = int(fields["sizehi"], 0)
            size_lo = int(fields["sizelo"], 0)
        except ValueError as exc:
            raise RdcpError("getfileattributes sizehi/sizelo were not numeric") from exc
        return (size_hi << 32) | size_lo

    raise RdcpError(f"getfileattributes did not expose a size field: {lines[0]}")


def size_lines_from_response(response) -> list[str]:
    if response.lines is not None:
        return response.lines
    if response.message:
        return [response.message]
    return []


def read_remote_size(client: RdcpClient, remote_path: str) -> int:
    response = client.command(build_file_command("getfileattributes", remote_path))
    if not response.is_success:
        raise RdcpError(
            f"getfileattributes failed: {response.code}- {response.message}"
        )
    return parse_size(size_lines_from_response(response))


def read_remote_file(
    client: RdcpClient,
    remote_path: str,
    offset: int,
    size: int,
) -> bytes:
    actual_size = max(0, size)
    response = client.command(
        build_file_command("getfile", remote_path, offset=offset, size=actual_size),
        binary_length=actual_size + 4,
    )
    if not response.is_success:
        raise RdcpError(f"getfile failed: {response.code}- {response.message}")
    if response.binary is None:
        raise RdcpError("getfile did not return binary data")
    if len(response.binary) < 4:
        raise RdcpError("getfile returned too few bytes")

    reported_size = struct.unpack_from("<I", response.binary)[0]
    payload = response.binary[4:4 + reported_size]
    if reported_size != len(response.binary) - 4:
        raise RdcpError(
            f"getfile payload size mismatch: reported {reported_size}, received {len(response.binary) - 4}"
        )
    return payload


def read_tail_text(
    client: RdcpClient,
    remote_path: str,
    tail_lines: int,
    chunk_size: int,
) -> str:
    file_size = read_remote_size(client, remote_path)
    if file_size == 0:
        return ""

    target_lines = abs(tail_lines)
    read_size = min(file_size, max(chunk_size, target_lines * 256))

    while True:
        offset = max(0, file_size - read_size)
        payload = read_remote_file(client, remote_path, offset, read_size)
        text = payload.decode("utf-8", errors="replace")

        if offset > 0:
            first_newline = text.find("\n")
            if first_newline == -1:
                if read_size >= file_size:
                    break
                read_size = min(file_size, read_size * 2)
                continue
            text = text[first_newline + 1 :]

        lines = text.splitlines()
        if len(lines) >= target_lines or read_size >= file_size:
            return "\n".join(lines[-target_lines:])

        read_size = min(file_size, read_size * 2)

    return "\n".join(text.splitlines()[-target_lines:])


def write_full_copy(output_path: str, payload: bytes) -> str:
    if output_path == "-":
        sys.stdout.buffer.write(payload)
        return "-"

    abs_path = os.path.abspath(output_path)
    parent_dir = os.path.dirname(abs_path)
    if parent_dir:
        os.makedirs(parent_dir, exist_ok=True)
    with open(abs_path, "wb") as handle:
        handle.write(payload)
    return abs_path


def write_tail_text(output_path: str, text: str) -> str:
    if output_path == "-":
        if text:
            sys.stdout.write(text)
            if not text.endswith("\n"):
                sys.stdout.write("\n")
        return "-"

    abs_path = os.path.abspath(output_path)
    parent_dir = os.path.dirname(abs_path)
    if parent_dir:
        os.makedirs(parent_dir, exist_ok=True)
    with open(abs_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
        if text and not text.endswith("\n"):
            handle.write("\n")
    return abs_path


def main() -> int:
    args = parse_args()
    host = args.host if args.host is not None else os.environ.get("XBDM_HOST", "localhost")
    port = args.port if args.port is not None else int(os.environ.get("XBDM_PORT", "731"))
    timeout = args.timeout if args.timeout is not None else float(os.environ.get("XBDM_TIMEOUT", "5.0"))

    try:
        with RdcpClient(host, port, timeout) as client:
            if args.lines == 0:
                size = read_remote_size(client, args.remote)
                payload = read_remote_file(client, args.remote, 0, size)
                output_path = write_full_copy(args.output or DEFAULT_OUTPUT_PATH, payload)
                if output_path == "-":
                    return 0
                print(f"saved {len(payload)} bytes to {output_path}")
                return 0

            tail_text = read_tail_text(client, args.remote, args.lines, args.chunk_size)
            output_path = write_tail_text(args.output or "-", tail_text)
            if output_path != "-":
                print(f"saved tail to {output_path}")
            return 0
    except RdcpError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
