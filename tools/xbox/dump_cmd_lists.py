#!/usr/bin/env python3
"""One-off diagnostic: walk live scenario AI command lists over XBDM.

Reads scenario @ *0x5064e4, command-list block @ scenario+0x438 ({count,addr}),
each entry 0x60 bytes (name@0, atoms block @+0x30 {count,addr}), atom 0x20 bytes
(type = int16 @0). Prints each list's name and atom types; detail for a target.
"""
import sys
import binascii
import subprocess

SCENARIO_PTR = 0x5064E4
CMDLIST_OFF = 0x438
ENTRY_SIZE = 0x60
ATOMS_OFF = 0x30
ATOM_SIZE = 0x20

TARGET = sys.argv[1] if len(sys.argv) > 1 else "cryo"


def read_mem(addr, length):
    """Read memory via the proven xbdm_rdcp.py CLI (one connect per call)."""
    out = subprocess.run(
        ["python3", "tools/xbox/xbdm_rdcp.py",
         f"getmem addr=0x{addr:x} length=0x{length:x}"],
        capture_output=True, text=True, timeout=20,
    ).stdout
    hexstr = "".join(c for c in out if c in "0123456789abcdefABCDEF.")
    hexstr = hexstr.replace("..", "00")
    return binascii.unhexlify(hexstr)


def u32(b, off):
    return int.from_bytes(b[off:off + 4], "little")


def i16(b, off):
    return int.from_bytes(b[off:off + 2], "little", signed=True)


def cstr(b, off, maxlen=32):
    raw = b[off:off + maxlen]
    nul = raw.find(b"\x00")
    if nul >= 0:
        raw = raw[:nul]
    return raw.decode("ascii", "replace")


def main():
    scen = u32(read_mem(SCENARIO_PTR, 4), 0)
    print(f"scenario = 0x{scen:08x}")
    blk = read_mem(scen + CMDLIST_OFF, 8)
    count, addr = u32(blk, 0), u32(blk, 4)
    print(f"command lists: count={count} addr=0x{addr:08x}")
    if count <= 0 or count > 1000:
        print("!! implausible count, aborting")
        return 1
    arr = read_mem(addr, count * ENTRY_SIZE)
    if True:
        for i in range(count):
            base = i * ENTRY_SIZE
            name = cstr(arr, base + 0)
            flags = u32(arr, base + 0x20)
            acount = u32(arr, base + ATOMS_OFF)
            aaddr = u32(arr, base + ATOMS_OFF + 4)
            is_target = TARGET.lower() in name.lower()
            if not is_target:
                continue
            print(f"\n=== [{i}] {name}  flags=0x{flags:x} atoms={acount} @0x{aaddr:08x} ===")
            if acount <= 0 or acount > 256 or aaddr < 0x10000:
                print("   (no/invalid atoms)")
                continue
            atoms = read_mem(aaddr, acount * ATOM_SIZE)
            for a in range(acount):
                ab = a * ATOM_SIZE
                atype = i16(atoms, ab + 0)
                a1 = i16(atoms, ab + 2)
                # dump first 0x20 bytes hex for inspection
                raw = binascii.hexlify(atoms[ab:ab + ATOM_SIZE]).decode()
                print(f"   atom[{a}] type={atype} field2={a1}  raw={raw}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
