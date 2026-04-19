#!/usr/bin/env python3
"""xemu-mon.py — talk to xemu's QEMU HMP monitor over TCP, return clean output.

Strips the line-editor's per-character ANSI/backspace echo so the output is
plain text suitable for grep/sed/automation.

Usage:
  tools/xemu-mon.py "info registers"
  tools/xemu-mon.py "info status" "info registers"
  tools/xemu-mon.py --repeat 5 --interval 1 "info registers"
  tools/xemu-mon.py --quit "info status"           # terminates xemu after
  echo "info registers" | tools/xemu-mon.py        # commands from stdin

Defaults to 127.0.0.1:4445 to match `tools/xemu.sh -m`.
"""

import argparse
import re
import socket
import sys
import time

from local_env import maybe_reexec_on_windows


maybe_reexec_on_windows(__file__)

PROMPT = b"(qemu) "
ANSI_CSI = re.compile(rb"\x1b\[[0-9;]*[a-zA-Z]")


def read_until_prompt(sock: socket.socket, timeout_secs: float = 5.0) -> bytes:
    sock.settimeout(timeout_secs)
    buf = b""
    while True:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            return buf
        if not chunk:
            return buf
        buf += chunk
        if PROMPT in buf:
            return buf


def clean(raw: bytes, command_echo: str = "") -> str:
    """Apply backspaces, strip ANSI, drop the trailing prompt and command echo.

    QEMU's HMP echoes typed characters using `ESC[D` (cursor-left) +
    `ESC[K` (erase-to-end) sequences instead of literal backspace, so after
    stripping ANSI we're left with concatenated prefixes like
    "iininfinfoinfo info rinfo registers" — a single line that *ends* with
    the full command. The cleanup drops the first non-empty line of the
    response if it ends with the echoed command.
    """
    raw = ANSI_CSI.sub(b"", raw)
    out = bytearray()
    for b in raw:
        if b == 0x08:  # literal backspace, just in case
            if out:
                out.pop()
        elif b != 0x0D:  # drop CR
            out.append(b)
    text = out.decode("utf-8", errors="replace")
    text = re.sub(r"\(qemu\)\s*$", "", text)
    if command_echo:
        lines = text.splitlines(keepends=True)
        for i, line in enumerate(lines):
            if line.strip():
                if line.rstrip().endswith(command_echo):
                    lines = lines[:i] + lines[i + 1:]
                break
        text = "".join(lines)
    return text.strip("\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=4445)
    ap.add_argument("--repeat", type=int, default=1,
                    help="repeat the command sequence N times")
    ap.add_argument("--interval", type=float, default=1.0,
                    help="seconds between repeats")
    ap.add_argument("--quit", action="store_true",
                    help='send "q" after the last sample (terminates xemu)')
    ap.add_argument("commands", nargs="*",
                    help="monitor commands; if empty, read from stdin")
    args = ap.parse_args()

    cmds = args.commands
    if not cmds:
        cmds = [line.strip() for line in sys.stdin if line.strip()]
    if not cmds:
        ap.error("no commands given")

    try:
        sock = socket.create_connection((args.host, args.port), timeout=5)
    except (ConnectionRefusedError, OSError) as e:
        print(f"error: cannot connect to {args.host}:{args.port}: {e}",
              file=sys.stderr)
        return 1

    read_until_prompt(sock, timeout_secs=2.0)

    try:
        for i in range(args.repeat):
            if args.repeat > 1:
                if i > 0:
                    time.sleep(args.interval)
                print(f"--- sample {i + 1} ---")
            for cmd in cmds:
                sock.sendall((cmd + "\n").encode())
                resp = read_until_prompt(sock, timeout_secs=5.0)
                print(clean(resp, cmd))
        if args.quit:
            sock.sendall(b"q\n")
    finally:
        try:
            sock.close()
        except OSError:
            pass

    return 0


if __name__ == "__main__":
    sys.exit(main())
