#!/usr/bin/env python3
"""Minimal RDCP/XBDM client for scripted debug queries.

This client speaks the text-based RDCP protocol used by XBDM on TCP 731.
It is intended to be easy to invoke from OpenCode, Claude, or shell scripts.
"""

from __future__ import annotations

import argparse
import json
import os
import socket
import sys
from dataclasses import dataclass

from local_env import load_repo_env, maybe_reexec_on_windows


load_repo_env("xbox.env")
maybe_reexec_on_windows(__file__)


DEFAULT_HOST = os.environ.get("XBDM_HOST", "localhost")
DEFAULT_PORT = int(os.environ.get("XBDM_PORT", "731"))
DEFAULT_TIMEOUT = float(os.environ.get("XBDM_TIMEOUT", "5.0"))


class RdcpError(RuntimeError):
    pass


@dataclass
class RdcpResponse:
    code: int
    message: str
    lines: list[str] | None = None
    binary: bytes | None = None

    @property
    def is_success(self) -> bool:
        return 200 <= self.code < 300

    def to_json(self, output_path: str | None = None) -> dict:
        data = {
            "code": self.code,
            "message": self.message,
            "ok": self.is_success,
        }
        if self.lines is not None:
            data["lines"] = self.lines
            data["text"] = "\n".join(self.lines)
        if self.binary is not None:
            data["binary_length"] = len(self.binary)
        if output_path is not None:
            data["output_path"] = output_path
        return data


class RdcpClient:
    def __init__(self, host: str, port: int = DEFAULT_PORT,
                 timeout: float = DEFAULT_TIMEOUT):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock: socket.socket | None = None
        self.fileobj = None
        self.banner: RdcpResponse | None = None

    def __enter__(self) -> "RdcpClient":
        self.connect()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def connect(self) -> RdcpResponse:
        if self.sock is not None:
            raise RdcpError("client is already connected")
        try:
            self.sock = socket.create_connection(
                (self.host, self.port), timeout=self.timeout
            )
        except OSError as exc:
            raise RdcpError(
                f"failed to connect to {self.host}:{self.port}: {exc}"
            ) from exc

        try:
            self.sock.settimeout(self.timeout)
            self.fileobj = self.sock.makefile("rwb", buffering=0)
            self.banner = self.read_response()
            return self.banner
        except Exception:
            self.close()
            raise

    def close(self) -> None:
        if self.fileobj is not None:
            try:
                self.fileobj.close()
            except OSError:
                pass
            self.fileobj = None
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    def send_command(self, command: str) -> None:
        if self.fileobj is None:
            raise RdcpError("client is not connected")
        payload = command.rstrip("\r\n") + "\r\n"
        try:
            self.fileobj.write(payload.encode("ascii"))
        except UnicodeEncodeError as exc:
            raise RdcpError("RDCP commands must be ASCII") from exc
        except OSError as exc:
            raise RdcpError(f"failed to send command: {exc}") from exc

    def read_response(self, binary_length: int | None = None) -> RdcpResponse:
        line = self._readline()
        if len(line) < 5 or line[3:4] != b"-":
            raise RdcpError(f"malformed RDCP response: {line!r}")

        code_text = line[:3]
        try:
            code = int(code_text)
        except ValueError as exc:
            raise RdcpError(f"invalid RDCP status code: {line!r}") from exc

        message = line[4:].decode("ascii", errors="replace")
        if code == 202:
            return RdcpResponse(code=code, message=message,
                                lines=self._read_multiline())
        if code == 203:
            if binary_length is None:
                raise RdcpError(
                    "203 response requires --binary-length or binary_length"
                )
            return RdcpResponse(code=code, message=message,
                                binary=self._read_exact(binary_length))
        return RdcpResponse(code=code, message=message)

    def command(self, command: str,
                binary_length: int | None = None) -> RdcpResponse:
        self.send_command(command)
        return self.read_response(binary_length=binary_length)

    def _readline(self) -> bytes:
        if self.fileobj is None:
            raise RdcpError("client is not connected")
        try:
            raw = self.fileobj.readline()
        except OSError as exc:
            raise RdcpError(f"failed to read response line: {exc}") from exc
        if not raw:
            raise RdcpError("connection closed while reading response")
        if raw.endswith(b"\r\n"):
            return raw[:-2]
        if raw.endswith(b"\n"):
            return raw[:-1]
        if raw.endswith(b"\r"):
            return raw[:-1]
        return raw

    def _read_multiline(self) -> list[str]:
        lines = []
        while True:
            line = self._readline().decode("ascii", errors="replace")
            if line == ".":
                return lines
            lines.append(line)

    def _read_exact(self, length: int) -> bytes:
        if length < 0:
            raise RdcpError("binary_length must be non-negative")
        if self.fileobj is None:
            raise RdcpError("client is not connected")

        chunks = bytearray()
        remaining = length
        while remaining > 0:
            try:
                chunk = self.fileobj.read(remaining)
            except OSError as exc:
                raise RdcpError(f"failed to read binary payload: {exc}") from exc
            if not chunk:
                raise RdcpError(
                    f"connection closed with {remaining} binary bytes remaining"
                )
            chunks.extend(chunk)
            remaining -= len(chunk)
        return bytes(chunks)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send a single RDCP/XBDM command and print the response"
    )
    parser.add_argument(
        "command",
        help="RDCP command to send, for example 'threads' or 'getmem addr=0x1000 length=0x40'",
    )
    parser.add_argument(
        "--host",
        default=DEFAULT_HOST,
        help=f"XBDM host or IP address (default: {DEFAULT_HOST})",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"XBDM TCP port (default: {DEFAULT_PORT})",
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
        help="number of bytes to read after a 203 response; accepts decimal or 0x-prefixed hex",
    )
    parser.add_argument(
        "--output",
        help="save a 203 binary payload to this path; use '-' for stdout",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="print structured JSON metadata instead of plain text",
    )
    parser.add_argument(
        "--show-banner",
        action="store_true",
        help="include the initial 201 banner in stdout or JSON output",
    )
    return parser.parse_args()


def parse_int(value: str) -> int:
    return int(value, 0)


def save_binary_payload(payload: bytes, output: str) -> tuple[str | None, bool]:
    if output == "-":
        sys.stdout.buffer.write(payload)
        return None, True

    output_path = os.path.abspath(output)
    with open(output_path, "wb") as handle:
        handle.write(payload)
    return output_path, False


def emit_plain_response(response: RdcpResponse, output_path: str | None) -> None:
    if response.lines is not None:
        for line in response.lines:
            print(line)
        return

    if response.binary is not None:
        if output_path is not None:
            print(f"saved {len(response.binary)} bytes to {output_path}")
            return
        sys.stdout.buffer.write(response.binary)
        return

    text = f"{response.code}- {response.message}".rstrip()
    print(text)


def main() -> int:
    args = parse_args()

    if args.json and args.output == "-":
        print("error: --json cannot be combined with --output -",
              file=sys.stderr)
        return 1

    try:
        with RdcpClient(args.host, args.port, args.timeout) as client:
            response = client.command(
                args.command,
                binary_length=args.binary_length,
            )

            output_path = None
            wrote_binary_to_stdout = False
            if response.binary is not None and args.output is not None:
                output_path, wrote_binary_to_stdout = save_binary_payload(
                    response.binary, args.output
                )

            binary_to_stdout = (
                response.binary is not None
                and (wrote_binary_to_stdout or output_path is None)
                and not args.json
            )
            if binary_to_stdout and args.show_banner:
                print("error: --show-banner cannot be combined with binary stdout output",
                      file=sys.stderr)
                return 1

            if args.json:
                payload = response.to_json(output_path=output_path)
                if args.show_banner and client.banner is not None:
                    payload["banner"] = client.banner.to_json()
                json.dump(payload, sys.stdout, indent=2, sort_keys=True)
                sys.stdout.write("\n")
            else:
                if args.show_banner and client.banner is not None:
                    print(f"{client.banner.code}- {client.banner.message}".rstrip())
                if not wrote_binary_to_stdout:
                    emit_plain_response(response, output_path)

            return 0 if response.is_success else 1
    except RdcpError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
