#!/usr/bin/env python3
"""Dump Xbox memory (xemu QMP or XBDM) and build unicorn-compatible state snapshots.

Usage:
    # Dump full 128MB from xemu (pauses VM, dumps, resumes):
    python3 tools/equivalence/dump_xemu_memory.py dump [--output PATH]

    # Dump from real Xbox via XBDM:
    python3 tools/equivalence/dump_xemu_memory.py dump --xbdm [--xbox-ip IP] [--output PATH]

    # Build a state snapshot for a target function from an existing dump:
    python3 tools/equivalence/dump_xemu_memory.py snapshot --dump PATH --target FUNC [--output PATH]

    # One-shot: dump + snapshot in one step:
    python3 tools/equivalence/dump_xemu_memory.py capture --target FUNC [--output PATH]

The Xbox maps physical memory at two virtual ranges:
    0x00000000-0x03FFFFFF  (identity, 64MB)
    0x80000000-0x83FFFFFF  (kernel mirror of same 64MB)
    0x04000000-0x07FFFFFF  (upper 64MB, identity)
    0x84000000-0x87FFFFFF  (kernel mirror of upper 64MB)

Dump sources:
    xemu (default): QMP pmemsave — physical memory dump, needs virt-to-phys mapping
    XBDM (--xbdm): getmem over RDCP — virtual memory reads, works on real hardware

*** WARNING — VERIFY EVERY DUMP on the Cerbios-debug-BIOS dev xemu (2026-06-07) ***
The identity mapping above does NOT reliably hold on the dev xemu (Cerbios 3.1.0
Hybrid Debug BIOS, kernel-irqchip=off). All three capture methods produced
UNUSABLE data in 2026-06-07 testing:
  - QMP `pmemsave` (default): physical dump; game VA is not identity-mapped, so
    virt_to_phys() below reads the WRONG bytes.
  - `--xbdm` getmem: returned an ALL-ZEROS 128MB file.
  - QMP `memsave` of an arbitrary paused state: read zeros at known globals.
The only proven capture (artifacts/snapshots/, see its README) used QMP `memsave`
of specific VA regions during a SPECIFIC active-gameplay paused state (the paused
CPU context must have the game's user address space live — not a menu/idle pause).
ALWAYS verify a dump before trusting it: read a known global and confirm it is
sane/non-zero, e.g. read_u32(dump, 0x31fc38) (fwd-vector ptr) ~ 0x0028xxxx, or look
for datum magic 0x64407440 in the object table. If they read 0, the dump is junk.

The snapshot tool reads the function's relocations to discover which globals/data
it touches, then extracts those regions from the dump (via virt_to_phys) into a
state_snapshot JSON that unicorn_diff can load directly.
"""

import argparse
import json
import os
import struct
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
ARTIFACTS = REPO_ROOT / "artifacts"
XBOX_RAM_SIZE = 128 * 1024 * 1024  # 128 MB


def virt_to_phys(vaddr: int) -> int | None:
    """Convert Xbox virtual address to physical offset in the dump.

    Returns None for addresses outside the direct-mapped ranges.
    """
    if 0x00000000 <= vaddr < 0x04000000:
        return vaddr
    if 0x80000000 <= vaddr < 0x84000000:
        return vaddr - 0x80000000
    if 0x04000000 <= vaddr < 0x08000000:
        return vaddr
    if 0x84000000 <= vaddr < 0x88000000:
        return vaddr - 0x80000000
    return None


def read_virt(dump: bytes, vaddr: int, size: int) -> bytes | None:
    """Read virtual memory from a physical dump."""
    phys = virt_to_phys(vaddr)
    if phys is None or phys + size > len(dump):
        return None
    return dump[phys:phys + size]


def read_u32(dump: bytes, vaddr: int) -> int | None:
    """Read a 32-bit little-endian value from virtual address."""
    raw = read_virt(dump, vaddr, 4)
    if raw is None:
        return None
    return struct.unpack("<I", raw)[0]


def dump_xemu(output: Path) -> bool:
    """Dump full xemu memory via QMP pmemsave."""
    output.parent.mkdir(parents=True, exist_ok=True)
    win_path = str(output).replace("/mnt/g/", "G:\\\\").replace("/", "\\\\")

    cmds = [
        f'pmemsave 0 {XBOX_RAM_SIZE} "{win_path}"',
    ]
    print(f"Dumping {XBOX_RAM_SIZE // (1024*1024)}MB from xemu to {output}")

    if output.exists() and output.stat().st_size == XBOX_RAM_SIZE:
        print(f"  Dump already exists ({output.stat().st_size} bytes)")
        return True

    print("  Run via xemu MCP or monitor:")
    for c in cmds:
        print(f"    {c}")
    return False


def _xbdm_read_range(sock, base: int, size: int, chunk: int = 1024,
                      label: str = "") -> bytearray:
    """Read a virtual memory range over an existing XBDM socket."""
    dump_data = bytearray()
    addr = base
    end = base + size
    last_pct = -1

    while addr < end:
        read_sz = min(chunk, end - addr)
        cmd = f"getmem addr=0x{addr:08x} length={read_sz}\r\n"
        sock.sendall(cmd.encode())

        response = b""
        while b"\r\n.\r\n" not in response:
            part = sock.recv(4096)
            if not part:
                break
            response += part

        lines = response.decode("ascii", errors="replace").split("\r\n")
        if not lines[0].startswith("203"):
            dump_data.extend(b'\x00' * read_sz)
            addr += read_sz
            continue

        hex_data = ""
        for line in lines[1:]:
            if line == ".":
                break
            hex_data += line.strip()

        if hex_data:
            dump_data.extend(bytes.fromhex(hex_data))
        else:
            dump_data.extend(b'\x00' * read_sz)

        addr += read_sz
        pct = (addr - base) * 100 // size
        if pct != last_pct and pct % 10 == 0:
            print(f"  {label} {pct}%")
            last_pct = pct

    return dump_data


def dump_xbdm(output: Path, xbox_ip: str = "192.168.1.20",
              chunk: int = 1024) -> bool:
    """Dump Xbox memory via XBDM getmem (virtual addresses, works on real hardware).

    Reads two virtual ranges:
      0x00010000-0x04000000  (XBE, globals, low heap — ~64MB)
      0x80000000-0x84000000  (kernel heap mirror — ~64MB)
    Saves as a JSON with hex regions keyed by base address.
    """
    import socket

    output.parent.mkdir(parents=True, exist_ok=True)

    ranges = [
        (0x00010000, 0x04000000, "user"),
        (0x80000000, 0x04000000, "kernel"),
    ]
    total_mb = sum(s for _, s, _ in ranges) // (1024 * 1024)
    print(f"Dumping {total_mb}MB from Xbox ({xbox_ip}) via XBDM...")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((xbox_ip, 731))
        banner = sock.recv(1024)
        if b"201" not in banner:
            print(f"  ERROR: unexpected XBDM banner: {banner}")
            return False
        print(f"  Connected: {banner.strip().decode()}")
    except Exception as e:
        print(f"  ERROR: cannot connect to {xbox_ip}:731 — {e}")
        return False

    # Send stop to freeze execution during dump
    sock.sendall(b"stop\r\n")
    resp = sock.recv(1024)
    print(f"  Paused: {resp.strip().decode()}")

    all_regions = {}
    try:
        for base, size, label in ranges:
            print(f"  Reading {label}: 0x{base:08x} - 0x{base + size:08x}")
            data = _xbdm_read_range(sock, base, size, chunk, label)
            all_regions[base] = data
            print(f"  {label}: {len(data)} bytes")

        # Resume
        sock.sendall(b"go\r\n")
        resp = sock.recv(1024)
        print(f"  Resumed: {resp.strip().decode()}")
    finally:
        sock.sendall(b"bye\r\n")
        sock.close()

    # Save as raw binary with a small sidecar JSON for region layout
    bin_path = output
    meta_path = output.with_suffix(".meta.json")

    with open(bin_path, "wb") as f:
        for base, size, label in ranges:
            f.write(all_regions[base])

    meta = {
        "source": f"xbdm:{xbox_ip}",
        "ranges": [{"base": f"0x{b:08x}", "size": s, "label": l}
                    for b, s, l in ranges],
    }
    with open(meta_path, "w") as f:
        json.dump(meta, f, indent=2)

    total = sum(len(d) for d in all_regions.values())
    print(f"  Wrote {total // (1024*1024)}MB to {bin_path}")
    return True


def get_function_memory_regions(dump: bytes, target: str) -> dict:
    """Discover memory regions a function needs from kb.json relocations
    and global address references."""
    kb_path = REPO_ROOT / "kb.json"
    with open(kb_path) as f:
        kb = json.load(f)

    func_info = None
    for obj in kb.get("objects", []):
        for func in obj.get("functions", []):
            if func.get("name") == target or func.get("addr") == target:
                func_info = func
                break

    if not func_info:
        print(f"  WARNING: {target} not found in kb.json, using default regions")

    regions = {}

    known_globals = [
        (0x5aa6d4, 4, "player_data"),
        (0x456b60, 4, "current_game_engine"),
        (0x456b24, 64, "variant_fields"),
        (0x5aa6d4, 4, "player_data_ptr"),
    ]

    for addr, size, name in known_globals:
        data = read_virt(dump, addr, size)
        if data:
            regions[f"0x{addr:08x}"] = data.hex()

    player_data_ptr = read_u32(dump, 0x5aa6d4)
    if player_data_ptr:
        table_size = 0x38 + 16 * 0xd4
        data = read_virt(dump, player_data_ptr, table_size)
        if data:
            regions[f"0x{player_data_ptr:08x}"] = data.hex()
            print(f"  player_data table at 0x{player_data_ptr:08x} ({table_size} bytes)")

    engine_ptr = read_u32(dump, 0x456b60)
    if engine_ptr:
        data = read_virt(dump, engine_ptr, 0x100)
        if data:
            regions[f"0x{engine_ptr:08x}"] = data.hex()
            print(f"  game_engine vtable at 0x{engine_ptr:08x}")

    return regions


def build_full_snapshot(dump: bytes, target: str, output: Path,
                        arg_overrides: dict | None = None) -> Path:
    """Build a comprehensive snapshot by mapping ALL non-zero pages."""
    PAGE = 4096
    regions = {}
    mapped_bytes = 0

    for base_virt, phys_start, size, label in [
        (0x00000000, 0x00000000, 0x04000000, "low 64MB"),
        (0x80000000, 0x00000000, 0x04000000, "kernel mirror"),
    ]:
        for offset in range(0, size, PAGE):
            phys = phys_start + offset
            if phys + PAGE > len(dump):
                break
            page = dump[phys:phys + PAGE]
            if page == b'\x00' * PAGE:
                continue
            vaddr = base_virt + offset
            regions[f"0x{vaddr:08x}"] = page.hex()
            mapped_bytes += PAGE

    print(f"  Mapped {mapped_bytes // 1024}KB across {len(regions)} pages")

    snapshot = {
        "description": f"Full xemu memory snapshot for {target}",
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "regions": regions,
    }
    if arg_overrides:
        snapshot["arg_overrides"] = arg_overrides

    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "w") as f:
        json.dump(snapshot, f)
    print(f"  Wrote {output} ({output.stat().st_size // 1024}KB)")
    return output


def build_targeted_snapshot(dump: bytes, target: str, output: Path,
                            arg_overrides: dict | None = None) -> Path:
    """Build a targeted snapshot with only the regions the function needs."""
    print(f"  Analyzing {target} memory dependencies...")
    regions = get_function_memory_regions(dump, target)

    snapshot = {
        "description": f"Targeted xemu snapshot for {target}",
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "regions": regions,
    }
    if arg_overrides:
        snapshot["arg_overrides"] = arg_overrides

    total = sum(len(v) // 2 for v in regions.values())
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "w") as f:
        json.dump(snapshot, f, indent=2)
    print(f"  Wrote {output} ({total} bytes in {len(regions)} regions)")
    return output


def main():
    parser = argparse.ArgumentParser(description="xemu memory dump + snapshot tool")
    sub = parser.add_subparsers(dest="command")

    p_dump = sub.add_parser("dump", help="Dump Xbox memory")
    p_dump.add_argument("--output", "-o", type=Path,
                        default=ARTIFACTS / "xbox_full_memory.bin")
    p_dump.add_argument("--xbdm", action="store_true",
                        help="Use XBDM (real Xbox) instead of xemu QMP")
    p_dump.add_argument("--xbox-ip", default="192.168.1.20",
                        help="Xbox IP for XBDM (default: 192.168.1.20)")

    p_snap = sub.add_parser("snapshot", help="Build snapshot from existing dump")
    p_snap.add_argument("--dump", "-d", type=Path, required=True)
    p_snap.add_argument("--target", "-t", required=True)
    p_snap.add_argument("--output", "-o", type=Path)
    p_snap.add_argument("--full", action="store_true",
                        help="Map all non-zero pages (large file, best coverage)")
    p_snap.add_argument("--arg", action="append", nargs=2, metavar=("NAME", "VALUE"),
                        help="Argument override (e.g. --arg dead_handle 0xec700000)")

    p_cap = sub.add_parser("capture", help="Dump + snapshot in one step")
    p_cap.add_argument("--target", "-t", required=True)
    p_cap.add_argument("--output", "-o", type=Path)
    p_cap.add_argument("--full", action="store_true")

    args = parser.parse_args()

    if args.command == "dump":
        if args.xbdm:
            dump_xbdm(args.output, xbox_ip=args.xbox_ip)
        else:
            dump_xemu(args.output)

    elif args.command == "snapshot":
        dump_path = args.dump
        if not dump_path.exists():
            print(f"ERROR: dump file not found: {dump_path}", file=sys.stderr)
            sys.exit(1)

        dump = dump_path.read_bytes()
        if len(dump) != XBOX_RAM_SIZE:
            print(f"WARNING: dump is {len(dump)} bytes, expected {XBOX_RAM_SIZE}")

        arg_overrides = {}
        if args.arg:
            for name, val in args.arg:
                arg_overrides[name] = int(val, 0) if val.startswith("0x") else int(val)

        out = args.output or (ARTIFACTS / f"snapshot_{args.target}.json")

        if args.full:
            build_full_snapshot(dump, args.target, out, arg_overrides or None)
        else:
            build_targeted_snapshot(dump, args.target, out, arg_overrides or None)

    elif args.command == "capture":
        print("Step 1: Ensure xemu dump exists")
        dump_path = ARTIFACTS / "xbox_full_memory.bin"
        if not dump_path.exists():
            print(f"  Dump not found at {dump_path}")
            print("  Pause xemu and run:")
            win_path = str(dump_path).replace("/mnt/g/", "G:\\\\").replace("/", "\\\\")
            print(f'  pmemsave 0 {XBOX_RAM_SIZE} "{win_path}"')
            sys.exit(1)

        dump = dump_path.read_bytes()
        out = args.output or (ARTIFACTS / f"snapshot_{args.target}.json")
        arg_overrides = {}

        if args.full:
            build_full_snapshot(dump, args.target, out, arg_overrides or None)
        else:
            build_targeted_snapshot(dump, args.target, out, arg_overrides or None)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
