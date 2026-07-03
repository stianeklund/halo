#!/usr/bin/env python3
"""Live-dump perception props owned by an actor, over XBDM getmem.

Walks the perception-prop table at *0x5ab23c (data_t header: max_count@0x20,
elem_size@0x22, magic@0x28, cur_count@0x2e, data_addr@0x34), finds every prop
whose owner_actor_index (prop+0x04) == --actor, and prints the fields the
scorer / knowledge / engagement-level functions branch on. prop+0x24 (type) is
printed first because the scorer (0x2fd10) and knowledge fn (0x2fc20) both
early-exit unless type in {2,3}.

Run while the game HOLDS the broken aw=3 state at the a10 door (the plateau is
stable for 30+s, so timing is forgiving). Use --stop to freeze the CPU during
the read for a consistent snapshot.

  rtk python3 tools/xbox/dump_actor_props.py --stop
"""
import argparse
import socket
import struct

PORT = 731
PROP_TABLE_PTR = 0x5AB23C

# (offset, struct-fmt, label) — fields read/written along the promotion path
FIELDS = [
    (0x04, "<i", "owner_actor"),
    (0x08, "<i", "f0x08"),
    (0x0C, "<i", "orphan_idx"),
    (0x10, "<h", "f0x10"),
    (0x18, "<i", "unit_handle"),
    (0x24, "<h", "TYPE_0x24"),
    (0x50, "<f", "weight_0x50"),
    (0x60, "<b", "friendly_0x60"),
    (0x61, "<b", "ally_0x61"),
    (0x64, "<b", "ack_0x64"),
    (0x66, "<h", "f0x66"),
    (0x9C, "<h", "f0x9c"),
    (0xA4, "<b", "knowledge_0xa4"),
    (0xB8, "<b", "f0xb8"),
    (0x11C, "<f", "dist_0x11c"),
    (0x127, "<b", "f0x127"),
    (0x135, "<b", "f0x135"),
    (0x136, "<b", "f0x136"),
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
        # XBDM getmem returns 202 (multiline hex) on this box; some stacks use
        # 203. Accept both — anything else is a real error, so zero-fill.
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
                    help="owner actor handle (default a6 = 0xe38b0006)")
    ap.add_argument("--stop", action="store_true",
                    help="freeze CPU (stop/go) around the read")
    ap.add_argument("--all", action="store_true",
                    help="dump every live prop, not just --actor's")
    args = ap.parse_args()

    s = connect(args.host)
    try:
        if args.stop:
            s.sendall(b"stop\r\n")
            s.recv(1024)

        tp = struct.unpack_from("<I", getmem(s, PROP_TABLE_PTR, 4), 0)[0]
        print(f"prop table ptr *0x{PROP_TABLE_PTR:x} = 0x{tp:08x}")
        if tp in (0, 0xFFFFFFFF):
            print("  null prop table — not in-game?")
            return

        hdr = getmem(s, tp, 0x38)
        max_count = struct.unpack_from("<h", hdr, 0x20)[0]
        elem = struct.unpack_from("<h", hdr, 0x22)[0]
        magic = struct.unpack_from("<I", hdr, 0x28)[0]
        cur = struct.unpack_from("<h", hdr, 0x2E)[0]
        data = struct.unpack_from("<I", hdr, 0x34)[0]
        print(f"  hdr: max={max_count} elem=0x{elem:x} cur={cur} "
              f"magic=0x{magic:08x} data=0x{data:08x}")
        if elem <= 0 or max_count <= 0 or data in (0, 0xFFFFFFFF):
            print("  bad/empty prop table header")
            return

        region = getmem(s, data, max_count * elem)
        found = 0
        for slot in range(max_count):
            e = region[slot * elem:(slot + 1) * elem]
            if len(e) < min(elem, 0x140):
                continue
            salt = struct.unpack_from("<H", e, 0)[0]
            if salt == 0:
                continue
            owner = struct.unpack_from("<i", e, 4)[0]
            if not args.all and owner != args.actor:
                continue
            handle = ((salt & 0xFFFF) << 16) | (slot & 0xFFFF)
            found += 1
            print(f"\n-- prop slot={slot} handle=0x{handle:08x} "
                  f"owner=0x{owner & 0xFFFFFFFF:08x}")
            for off, fmt, name in FIELDS:
                try:
                    v = struct.unpack_from(fmt, e, off)[0]
                    if fmt == "<f":
                        print(f"     +0x{off:03x} {name:15} = {v:.4f}")
                    else:
                        print(f"     +0x{off:03x} {name:15} = {v} "
                              f"(0x{v & 0xFFFFFFFF:x})")
                except struct.error:
                    print(f"     +0x{off:03x} {name:15} = <oob>")
        print(f"\n{found} prop(s) "
              f"{'total' if args.all else f'owned by 0x{args.actor:08x}'}")
    finally:
        if args.stop:
            s.sendall(b"go\r\n")
            s.recv(1024)
        s.sendall(b"bye\r\n")
        s.close()


if __name__ == "__main__":
    main()
