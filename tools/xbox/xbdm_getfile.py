#!/usr/bin/env python3
"""Retrieve a file from the Xbox/xemu HDD over XBDM (RDCP `getfile`).

Why this exists
---------------
Grabbing a Halo `core_save` dump (or any file) off the running xemu/Xbox HDD is a
one-liner over XBDM, but the wire format has a gotcha that wastes a lot of time if
you rediscover it each session:

    getfile name=STRING
        Retrieve the entire contents of the named file. The received data is
        prefixed with a 32-bit little-endian value: the number of bytes that
        follow.

So after the `203- binary response follows` status line the payload is:

    [4-byte LE length N][N bytes of file data]

If you (wrongly) read a fixed size instead of honoring that length prefix you end
up with the 4-byte prefix glued to the front AND the tail truncated. This tool
reads the length prefix, then exactly N bytes, and writes the CLEAN file.

Usage
-----
    # Grab the patched-build Halo core dump (default path):
    python3 tools/xbox/xbdm_getfile.py --core -o artifacts/equivalence/core_patched.bin

    # Grab an arbitrary file:
    python3 tools/xbox/xbdm_getfile.py 'E:\\GAMES\\halo-patched\\core\\core.bin' -o out.bin

    # List a directory (to discover paths / sizes) instead of fetching:
    python3 tools/xbox/xbdm_getfile.py --list 'E:\\GAMES\\halo-patched\\core'

Host/port default to localhost:731 (override with --host/--port or XBDM_HOST/
XBDM_PORT). The xemu XBDM stub must be reachable (the same one xbdm_screenshot.py
uses). Nothing here touches QMP or the gdbstub, so it is safe to run while the
game is live — no stop/cont, no CPU halt.

Note on the Halo core dump format (for reference): once fetched, byte 0 is the
magic 0x1fd7a64f, followed by the scenario tag path (e.g. "levels\\c40\\c40",
Assault on the Control Room) padded to 0xE0-ish, then the engine build string
(e.g. "01.10.12.2276"), then the game-state region (object datums carry the
0x64407440 "d@t@"-style magic). It is the game_state pool, NOT the tag cache.
"""
from __future__ import annotations

import argparse
import os
import socket
import struct
import sys

DEFAULT_HOST = os.environ.get("XBDM_HOST", "localhost")
DEFAULT_PORT = int(os.environ.get("XBDM_PORT", "731"))
DEFAULT_CORE_PATH = r"E:\GAMES\halo-patched\core\core.bin"


def _connect(host: str, port: int) -> socket.socket:
    s = socket.create_connection((host, port), timeout=15)
    s.settimeout(15)
    return s


def _readline(f) -> bytes:
    raw = f.readline()
    if not raw:
        raise RuntimeError("connection closed while reading status line")
    return raw.rstrip(b"\r\n")


def getfile(host: str, port: int, xbox_path: str, out_path: str) -> int:
    """Fetch xbox_path -> out_path. Returns the number of bytes written."""
    s = _connect(host, port)
    f = s.makefile("rwb")
    try:
        _readline(f)  # greeting: "201- connected"
        f.write(f'getfile name="{xbox_path}"\r\n'.encode("ascii"))
        f.flush()
        status = _readline(f)
        if not status.startswith(b"203"):
            # surface the error body (e.g. "402- file not found")
            raise RuntimeError(f"getfile failed: {status.decode('ascii', 'replace')}")
        length = struct.unpack("<I", f.read(4))[0]
        data = bytearray()
        remaining = length
        while remaining > 0:
            chunk = f.read(remaining)
            if not chunk:
                raise RuntimeError(
                    f"connection closed with {remaining}/{length} bytes remaining"
                )
            data += chunk
            remaining -= len(chunk)
        os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
        with open(out_path, "wb") as out:
            out.write(data)
        return length
    finally:
        f.close()
        s.close()


def dirlist(host: str, port: int, xbox_dir: str) -> str:
    s = _connect(host, port)
    f = s.makefile("rwb")
    try:
        _readline(f)  # greeting
        f.write(f'dirlist name="{xbox_dir}"\r\n'.encode("ascii"))
        f.flush()
        status = _readline(f)
        if not status.startswith(b"202"):
            raise RuntimeError(f"dirlist failed: {status.decode('ascii', 'replace')}")
        lines = []
        while True:
            line = _readline(f)
            if line == b".":
                break
            lines.append(line.decode("ascii", "replace"))
        return "\n".join(lines)
    finally:
        f.close()
        s.close()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("path", nargs="?", help="Xbox file path (use \\\\ or quote backslashes)")
    ap.add_argument("--core", action="store_true",
                    help=f"shortcut for {DEFAULT_CORE_PATH}")
    ap.add_argument("-o", "--output", help="local output path (for fetch)")
    ap.add_argument("--list", metavar="DIR", dest="list_dir",
                    help="list an Xbox directory instead of fetching a file")
    ap.add_argument("--host", default=DEFAULT_HOST)
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    args = ap.parse_args()

    if args.list_dir:
        print(dirlist(args.host, args.port, args.list_dir))
        return 0

    xbox_path = DEFAULT_CORE_PATH if args.core else args.path
    if not xbox_path:
        ap.error("provide a file path, --core, or --list DIR")
    out = args.output or os.path.basename(xbox_path.replace("\\", "/"))
    n = getfile(args.host, args.port, xbox_path, out)
    print(f"wrote {out} ({n} bytes) from {xbox_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
