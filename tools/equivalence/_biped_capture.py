#!/usr/bin/env python3
"""Live biped equivalence capture helper (memsave-only).

WORKING METHOD (hard-won, do not deviate):
  - raw QMP over 127.0.0.1:4444
  - QMP `memsave` (VIRTUAL) is the ONLY reliable read path on this xemu.
    pmemsave/xp/XBDM getmem all read wrong/zero. Use memsave exclusively.
  - Always `cont` at the end. Keep pauses short.

All output bins go to the WORKTREE artifacts dir.
"""
import os
import sys
import struct
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
_TOOLS = os.path.dirname(_HERE)
if _TOOLS not in sys.path:
    sys.path.insert(0, _TOOLS)

from xbox.xemu_qmp import discover_session  # noqa: E402

WORKTREE = "/mnt/g/dev/halo/.claude/worktrees/scalable-coalescing-lobster"
ART = os.path.join(WORKTREE, "artifacts", "biped_capture")
os.makedirs(ART, exist_ok=True)


def win_path(p):
    # /mnt/g/... -> G:\...
    if p.startswith("/mnt/") and len(p) > 6 and p[6] == "/":
        drive = p[5].upper()
        rest = p[7:].replace("/", "\\")
        return f"{drive}:\\{rest}"
    return p


class Cap:
    def __init__(self):
        self.s = discover_session("127.0.0.1", 4444)
        if self.s is None:
            raise SystemExit("ERROR: no xemu QMP on 127.0.0.1:4444")
        self.paused = False

    def hmp(self, cmd):
        return self.s.command("human-monitor-command", {"command-line": cmd})

    def stop(self):
        if not self.paused:
            self.s.command("stop")
            self.paused = True

    def cont(self):
        if self.paused:
            self.s.command("cont")
            self.paused = False

    def memsave(self, va, size, name):
        """Capture [va, va+size) to ART/name. Returns bytes or None on error."""
        out = os.path.join(ART, name)
        wp = win_path(out)
        try:
            self.hmp(f"memsave 0x{va:x} {size} {wp}")
        except RuntimeError as e:
            return None, str(e)
        # small settle
        for _ in range(20):
            if os.path.exists(out) and os.path.getsize(out) >= size:
                break
            time.sleep(0.05)
        if not os.path.exists(out):
            return None, "file not created"
        data = open(out, "rb").read()
        return data, None

    def read_mem(self, va, size):
        """Capture to a scratch file and return bytes (no persistent name)."""
        out = os.path.join(ART, f"_scratch_{va:08x}_{size:x}.bin")
        wp = win_path(out)
        try:
            self.hmp(f"memsave 0x{va:x} {size} {wp}")
        except RuntimeError as e:
            return None
        for _ in range(20):
            if os.path.exists(out) and os.path.getsize(out) >= size:
                break
            time.sleep(0.05)
        if not os.path.exists(out):
            return None
        return open(out, "rb").read()[:size]

    def u32(self, va):
        b = self.read_mem(va, 4)
        if b is None or len(b) < 4:
            return None
        return struct.unpack_from("<I", b, 0)[0]

    def u16(self, va):
        b = self.read_mem(va, 2)
        if b is None or len(b) < 2:
            return None
        return struct.unpack_from("<H", b, 0)[0]

    def close(self):
        try:
            self.cont()
        finally:
            self.s.close()
