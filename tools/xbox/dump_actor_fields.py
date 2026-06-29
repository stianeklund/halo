#!/usr/bin/env python3
"""Live-dump an AI actor's escalation/perception fields over XBDM getmem.

Companion to dump_actor_props.py. Reads the actor record (table *0x6325a4,
stride 0x724) for --actor and prints the fields the awareness-escalation and
perception-knowledge paths branch on. Use --stop to freeze the CPU for a
consistent snapshot while the game HOLDS the broken aw=3 state.

  rtk python3 tools/xbox/dump_actor_fields.py --stop
"""
import argparse
import socket
import struct

PORT = 731
ACTOR_TABLE_PTR = 0x6325A4
ACTOR_STRIDE = 0x724

# (offset, struct-fmt, label)
FIELDS = [
    (0x06, "<b", "f0x06_blind"),
    (0x3E, "<h", "team_0x3e"),
    (0x58, "<i", "actr_tag_idx"),
    (0x5C, "<i", "actv_tag_idx"),
    (0x6C, "<h", "act_0x6c"),
    (0x6E, "<h", "agg_0x6e"),
    (0x15D, "<b", "f0x15d"),
    (0x161, "<b", "f0x161"),
    (0x1CA, "<b", "f0x1ca"),
    (0x1ED, "<b", "f0x1ed"),
    (0x202, "<b", "f0x202"),
    (0x268, "<h", "AWARENESS_0x268"),
    (0x270, "<i", "PRIMARY_PROP_0x270"),
    (0x378, "<b", "f0x378"),
    (0x3A8, "<h", "f0x3a8"),
    (0x3AC, "<i", "f0x3ac"),
    (0x60C, "<b", "engage_0x60c"),
]


def connect(host):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)
    s.connect((host, PORT))
    banner = s.recv(1024)
    if b"201" not in banner:
        raise SystemExit(f"bad XBDM banner: {banner!r}")
    return s


def getmem(s, addr, length, chunk=1024):
    out = bytearray()
    off = 0
    while off < length:
        n = min(chunk, length - off)
        s.sendall(f"getmem addr=0x{addr + off:08x} length={n}\r\n".encode())
        resp = b""
        while b"\r\n.\r\n" not in resp:
            part = s.recv(4096)
            if not part:
                break
            resp += part
        lines = resp.decode("ascii", "replace").split("\r\n")
        if not (lines[0].startswith("202") or lines[0].startswith("203")):
            out.extend(b"\x00" * n)
        else:
            hx = ""
            for ln in lines[1:]:
                if ln == ".":
                    break
                hx += ln.strip()
            out.extend(bytes.fromhex(hx) if hx else b"\x00" * n)
        off += n
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--actor", type=lambda x: int(x, 0), default=0xE38B0006,
                    help="actor handle (default a6 = 0xe38b0006)")
    ap.add_argument("--stop", action="store_true",
                    help="freeze CPU (stop/go) around the read")
    args = ap.parse_args()

    slot = args.actor & 0xFFFF
    s = connect(args.host)
    try:
        if args.stop:
            s.sendall(b"stop\r\n")
            s.recv(1024)

        at = struct.unpack_from("<I", getmem(s, ACTOR_TABLE_PTR, 4), 0)[0]
        print(f"actor table ptr *0x{ACTOR_TABLE_PTR:x} = 0x{at:08x}")
        if at in (0, 0xFFFFFFFF):
            print("  null actor table — not in-game?")
            return
        hdr = getmem(s, at, 0x38)
        data = struct.unpack_from("<I", hdr, 0x34)[0]
        base = data + slot * ACTOR_STRIDE
        print(f"  data=0x{data:08x} slot={slot} actor_base=0x{base:08x}")

        rec = getmem(s, base, 0x620)
        salt = struct.unpack_from("<H", rec, 0)[0]
        print(f"  salt=0x{salt:04x} (expect 0x{args.actor >> 16:04x})")
        print(f"\n-- actor 0x{args.actor:08x}")
        for off, fmt, name in FIELDS:
            v = struct.unpack_from(fmt, rec, off)[0]
            print(f"     +0x{off:03x} {name:18} = {v} (0x{v & 0xFFFFFFFF:x})")
    finally:
        if args.stop:
            s.sendall(b"go\r\n")
            s.recv(1024)
        s.sendall(b"bye\r\n")
        s.close()


if __name__ == "__main__":
    main()
