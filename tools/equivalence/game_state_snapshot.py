#!/usr/bin/env python3
"""Game-state snapshot capture from a running xemu instance.

Captures the Halo CE Xbox game-state pool (CPU + GPU), save header, and
relevant globals into a JSON snapshot file.  The snapshot is a faithful
byte-level capture that can be re-injected for A/B testing.

Usage:
  rtk python3 tools/equivalence/game_state_snapshot.py capture \\
      --description "cryo chamber, about to enter hallway"

The captured snapshot can then be loaded into a different XBE build
(patched vs unpatched) for regression comparison.

Physical address model:
  Xbox identity-maps the first 64 MB of physical RAM to virtual 0x80000000.
  The game-state pool is at physical 0x00061000 (virtual 0x80061000).
  System globals (game_state_globals at 0x4ea990) live in Xbox system RAM
  which xemu maps differently; we read them via GDB RSP using virtual
  addresses and use pmemsave for bulk captures.
"""

from __future__ import annotations

import argparse
import json
import os
import socket
import struct
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_OUT_DIR = ROOT / "artifacts" / "snapshots"

# ── known addresses (Xbox virtual → identity-mapped physical) ──────────
# Game-state pool: Xbox XPhysicalAlloc at physical 0x00061000, size 0x345000
GAME_STATE_PHYS_BASE = 0x00061000
GAME_STATE_POOL_SIZE = 0x345000  # CPU 0x305000 + GPU 0x40000 (whole alloc)
# Game-state globals struct (0x20 bytes) at virtual 0x4ea990
GS_GLOBALS_ADDR = 0x4EA990
# Xbox-specific game state globals (0x14 bytes) at virtual 0x4ea9b0
XBOX_GS_GLOBALS_ADDR = 0x4EA9B0
# Save header size
GS_HEADER_SIZE = 0x14C

# ── struct offsets (must match types.h:game_state_globals_t) ───────────
# game_state_globals_t at 0x4ea990:
#   +0x00: log_file (void*)
#   +0x04: base_address (char*)
#   +0x08: cpu_allocation_size (int)
#   +0x0c: gpu_allocation_size (uint32_t)
#   +0x10: checksum (uint32_t)
#   +0x14: locked (bool)
#   +0x15: saved (bool, 1 byte + 2 pad)
#   +0x18: unk_18 (int32_t)
#   +0x1c: header (char*)


def checksum_byte(b: int) -> int:
    """GDB RSP checksum: sum of ASCII values of all chars, mod 256."""
    return sum(ord(c) for c in b) & 0xFF


def gdb_send_recv(sock: socket.socket, packet: str) -> str:
    """Send a GDB RSP packet and receive the response."""
    chk = checksum_byte(packet)
    wire = f"${packet}#{chk:02x}"
    sock.sendall(wire.encode("ascii"))

    # read ACK for our request
    ack = sock.recv(1)
    if ack != b"+":
        raise RuntimeError(f"GDB no ACK, got: {ack!r}")

    # read response(s) — may be interleaved with notifications
    while True:
        prefix = sock.recv(1)
        if not prefix:
            raise RuntimeError("GDB connection closed")
        if prefix == b"%":
            # notification packet — read and discard
            _buf = b""
            while True:
                c = sock.recv(1)
                if not c:
                    raise RuntimeError("GDB connection closed")
                if c == b"#":
                    break
                _buf += c
            sock.recv(2)  # checksum
            continue
        if prefix == b"$":
            break
        raise RuntimeError(f"GDB unexpected prefix: {prefix!r}")

    # read response data
    buf = b""
    while True:
        c = sock.recv(1)
        if not c:
            raise RuntimeError("GDB connection closed")
        if c == b"#":
            break
        buf += c

    # read checksum (2 hex digits)
    csum_hex = sock.recv(2)
    expected_csum = checksum_byte(buf.decode("ascii"))
    got_csum = int(csum_hex, 16)
    if expected_csum != got_csum:
        raise RuntimeError(f"GDB checksum mismatch: {got_csum:02x} != {expected_csum:02x}")
    # send ACK
    sock.sendall(b"+")
    return buf.decode("ascii")


def gdb_connect(timeout: float = 3.0) -> socket.socket:
    """Connect to xemu's GDB stub on port 1234."""
    host = os.environ.get("XEMU_GDB_HOST", "localhost")
    port = int(os.environ.get("XEMU_GDB_PORT", "1234"))
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.settimeout(timeout)
    return sock


def gdb_read_mem(sock: socket.socket, addr: int, size: int) -> bytes:
    """Read memory from the guest via GDB RSP `m` packet.
    
    Always uses chunked reads (max 4096 bytes per packet) to avoid
    hitting GDB packet size limits on large reads.
    """
    if size <= 0:
        return b""
    result = bytearray()
    offset = 0
    while offset < size:
        chunk = min(size - offset, 4096)
        pkt = f"m{addr + offset:x},{chunk:x}"
        r = gdb_send_recv(sock, pkt)
        if r.startswith("E"):
            raise RuntimeError(f"GDB read error at 0x{addr + offset:x}: {r}")
        try:
            result.extend(bytes.fromhex(r))
        except ValueError as e:
            raise RuntimeError(
                f"GDB hex parse error at 0x{addr + offset:x}: {e}") from e
        offset += chunk
    return bytes(result)


def gdb_write_mem(sock: socket.socket, addr: int, data: bytes) -> None:
    """Write memory to the guest via GDB RSP `M` packet."""
    if not data:
        return
    offset = 0
    while offset < len(data):
        chunk = min(len(data) - offset, 4096)  # Keep packets reasonable
        chunk_data = data[offset:offset + chunk]
        hex_data = chunk_data.hex()
        packet = f"M{addr + offset:x},{chunk:x}:{hex_data}"
        resp = gdb_send_recv(sock, packet)
        if resp != "OK":
            raise RuntimeError(f"GDB write error at 0x{addr + offset:x}: {resp}")
        offset += chunk


def qmp_connect(host: str = "localhost", timeout: float = 3.0) -> "QmpSession":
    """Connect to xemu's QMP.  Import xemu_qmp lazily."""
    qmp_mod_path = ROOT / "tools" / "xbox" / "xemu_qmp.py"
    import importlib.util
    spec = importlib.util.spec_from_file_location("xemu_qmp", str(qmp_mod_path))
    qmp = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(qmp)
    session = qmp.discover_session(host)
    if session is None:
        raise RuntimeError("No running xemu QMP session found")
    return session


def qmp_pmemsave(session, phys_addr: int, size: int) -> bytes:
    """Save physical memory from xemu via QMP pmemsave."""
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        cmd = f"pmemsave {phys_addr:#x} {size:#x} {tmp_path}"
        session.command("human-monitor-command", {"command-line": cmd})
        with open(tmp_path, "rb") as f:
            data = f.read()
        return data[:size]
    finally:
        try:
            os.unlink(tmp_path)
        except OSError:
            pass


def parse_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def parse_ptr(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def read_globals_via_gdb() -> tuple[bytes, bytes]:
    """Read game_state_globals and xbox_game_state_globals via GDB RSP."""
    sock = gdb_connect()
    try:
        gs = gdb_read_mem(sock, GS_GLOBALS_ADDR, 0x20)
        xgs = gdb_read_mem(sock, XBOX_GS_GLOBALS_ADDR, 0x14)
    finally:
        sock.close()
    return gs, xgs


def read_data_via_gdb(addr: int, size: int) -> bytes:
    """Read arbitrary data via GDB RSP."""
    sock = gdb_connect()
    try:
        return gdb_read_mem(sock, addr, size)
    finally:
        sock.close()


def capture_snapshot(description: str = "",
                     output_path: Optional[str] = None) -> dict:
    """Capture a full game-state snapshot from a running xemu instance.

    Automatically pauses xemu via QMP for a consistent point-in-time
    capture, then resumes after.
    """
    result: dict = {
        "description": description,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "regions": {},
    }

    # pause for a consistent snapshot
    qmp = None
    try:
        qmp = qmp_connect()
        print("[*] Pausing xemu...")
        qmp.command("stop")
    except RuntimeError:
        pass  # QMP not available; capture anyway

    try:
        # Step 1: read the game_state_globals struct to learn addresses/sizes
        print("[*] Reading game_state_globals (0x4ea990) via GDB RSP...")
        gs_data, xgs_data = read_globals_via_gdb()

        base_addr = parse_ptr(gs_data, 0x04)
        cpu_size = parse_u32(gs_data, 0x08)
        gpu_size = parse_u32(gs_data, 0x0C)
        header_ptr = parse_ptr(gs_data, 0x1C)
        saved = gs_data[0x15]
        checksum_stored = parse_u32(gs_data, 0x10)
        unk_18 = parse_u32(gs_data, 0x18)
        log_file = parse_ptr(gs_data, 0x00)

        result["globals_raw"] = {
            "game_state_globals": {
                "addr": f"0x{GS_GLOBALS_ADDR:08x}",
                "size": "0x20",
                "data": gs_data.hex(),
            },
            "xbox_game_state_globals": {
                "addr": f"0x{XBOX_GS_GLOBALS_ADDR:08x}",
                "size": "0x14",
                "data": xgs_data.hex(),
            },
        }
        result["globals_parsed"] = {
            "base_address": f"0x{base_addr:08x}",
            "cpu_allocation_size": f"0x{cpu_size:x}",
            "gpu_allocation_size": f"0x{gpu_size:x}",
            "header_ptr": f"0x{header_ptr:08x}",
            "saved": bool(saved),
            "checksum": f"0x{checksum_stored:08x}",
            "unk_18": f"0x{unk_18:08x}",
            "log_file": f"0x{log_file:08x}" if log_file else "NULL",
        }

        print(f"    base_address:    0x{base_addr:08x}")
        print(f"    cpu_size:        0x{cpu_size:x}")
        print(f"    gpu_size:        0x{gpu_size:x}")
        print(f"    header_ptr:      0x{header_ptr:08x}")

        # Step 2: read the save header
        print(f"[*] Reading save header (0x{GS_HEADER_SIZE:x} bytes at 0x{header_ptr:08x})...")
        header_data = read_data_via_gdb(header_ptr, GS_HEADER_SIZE)
        result["header_raw"] = {
            "addr": f"0x{header_ptr:08x}",
            "size": f"0x{GS_HEADER_SIZE:x}",
            "data": header_data.hex(),
        }

        # Step 3: dump CPU pool via GDB RSP (base_addr has CPU data)
        print(f"[*] Dumping CPU pool (0x{cpu_size:x} bytes at 0x{base_addr:08x})...")
        cpu_data = read_data_via_gdb(base_addr, cpu_size)
        result["regions"]["cpu_pool"] = {
            "base": f"0x{base_addr:08x}",
            "virtual_addr": f"0x{base_addr:08x}",
            "size": f"0x{cpu_size:x}",
            "data": cpu_data.hex(),
        }

        # Step 4: dump GPU pool (fixed at virtual 0x345000 per game code)
        gpu_addr = 0x345000
        gpu_region_size = 0x40000  # GAME_STATE_GPU_SIZE
        print(f"[*] Dumping GPU pool (0x{gpu_region_size:x} bytes at 0x{gpu_addr:08x})...")
        gpu_data = read_data_via_gdb(gpu_addr, gpu_region_size)
        result["regions"]["gpu_pool"] = {
            "base": f"0x{gpu_addr:08x}",
            "virtual_addr": f"0x{gpu_addr:08x}",
            "size": f"0x{gpu_region_size:x}",
            "data": gpu_data.hex(),
        }

        # Step 5: capture additional global regions needed by game_state fns
        # Callback table at 0x32eaa0 (64 bytes covers pre-save, pre-revert,
        # and 13 post-load callback pointers)
        callbacks_addr = 0x32EAA0
        callbacks_size = 0x40
        print(f"[*] Reading callback table ({callbacks_size} bytes at 0x{callbacks_addr:08x})...")
        cb_data = read_data_via_gdb(callbacks_addr, callbacks_size)
        result["regions"]["callbacks"] = {
            "base": f"0x{callbacks_addr:08x}",
            "virtual_addr": f"0x{callbacks_addr:08x}",
            "size": f"0x{callbacks_size:x}",
            "data": cb_data.hex(),
        }

        # Scenario index at 0x326a08 (8 bytes)
        scenario_addr = 0x326A08
        scenario_size = 0x8
        print(f"[*] Reading scenario index ({scenario_size} bytes at 0x{scenario_addr:08x})...")
        sc_data = read_data_via_gdb(scenario_addr, scenario_size)
        result["regions"]["scenario_index"] = {
            "base": f"0x{scenario_addr:08x}",
            "virtual_addr": f"0x{scenario_addr:08x}",
            "size": f"0x{scenario_size:x}",
            "data": sc_data.hex(),
        }

        # Map type at 0x31fa94 (8 bytes — covers adjacent fields too)
        map_type_addr = 0x31FA94
        map_type_size = 0x8
        print(f"[*] Reading map type ({map_type_size} bytes at 0x{map_type_addr:08x})...")
        mt_data = read_data_via_gdb(map_type_addr, map_type_size)
        result["regions"]["map_type"] = {
            "base": f"0x{map_type_addr:08x}",
            "virtual_addr": f"0x{map_type_addr:08x}",
            "size": f"0x{map_type_size:x}",
            "data": mt_data.hex(),
        }

        total_captured = cpu_size + GS_HEADER_SIZE + gpu_region_size
        print(f"[✓] Snapshot captured: {total_captured:#x} bytes total")

        if output_path:
            out = Path(output_path)
            out.parent.mkdir(parents=True, exist_ok=True)
            with open(out, "w", encoding="utf-8") as f:
                json.dump(result, f, indent=2)
                f.write("\n")
            print(f"[✓] Written to {out}")

        return result
    finally:
        if qmp is not None:
            print("[*] Resuming xemu...")
            qmp.command("cont")
            qmp.close()


def gdb_write_mem(sock: socket.socket, addr: int, data: bytes) -> None:
    """Write memory to the guest via GDB RSP `M` packet."""
    if not data:
        return
    offset = 0
    while offset < len(data):
        chunk = min(len(data) - offset, 4096)
        chunk_data = data[offset:offset + chunk]
        hex_data = chunk_data.hex()
        packet = f"M{addr + offset:x},{chunk:x}:{hex_data}"
        resp = gdb_send_recv(sock, packet)
        if resp != "OK":
            raise RuntimeError(f"GDB write error at 0x{addr + offset:x}: {resp}")
        offset += chunk


_GDB_REG_EIP = 8
_GDB_REG_ESP = 4
_CALLBACK_FN = 0x1BF790  # game_state_call_after_load_procs


def gdb_read_reg(sock: socket.socket, reg: int) -> int:
    """Read a single GDB register value (p packet)."""
    resp = gdb_send_recv(sock, packet=f"p{reg:x}")
    if resp.startswith("E"):
        raise RuntimeError(f"GDB register read error: {resp}")
    return int(resp, 16)


def gdb_write_reg(sock: socket.socket, reg: int, value: int) -> None:
    """Write a single GDB register value (P packet)."""
    resp = gdb_send_recv(sock, f"P{reg:x}={value:x}")
    if resp != "OK":
        raise RuntimeError(f"GDB register write error: {resp}")


def gdb_invoke_callbacks(sock: socket.socket) -> None:
    """Set up a GDB-assisted call to game_state_call_after_load_procs.

    Overwrites [ESP] with the current EIP as the return address, then
    sets EIP to the callback function.  When callbacks complete, the
    `ret` pops the saved EIP and execution resumes.

    The original [ESP] value is lost, but the injected pool data and
    post-load callbacks fully reinitialize the game state.
    """
    old_eip = gdb_read_reg(sock, _GDB_REG_EIP)
    old_esp = gdb_read_reg(sock, _GDB_REG_ESP)

    return_addr_bytes = struct.pack("<I", old_eip)
    gdb_write_mem(sock, old_esp, return_addr_bytes)
    gdb_write_reg(sock, _GDB_REG_EIP, _CALLBACK_FN)

    print(f"    EIP {old_eip:#x} → {_CALLBACK_FN:#x}, "
          f"ESP={old_esp:#x} (return addr written to [ESP])")


def inject_snapshot(snapshot_path: str) -> None:
    """Inject a previously captured snapshot into a running xemu instance.

    Automatically pauses xemu via QMP before writing and resumes after.
    Writes CPU pool, GPU pool, header, and globals via GDB RSP.
    """
    with open(snapshot_path, encoding="utf-8") as f:
        snap = json.load(f)

    regions = snap.get("regions", {})
    globals_raw = snap.get("globals_raw", {})
    header_raw = snap.get("header_raw", {})
    parsed = snap.get("globals_parsed", {})

    cpu_info = regions.get("cpu_pool")
    gpu_info = regions.get("gpu_pool")
    if not cpu_info or not gpu_info:
        raise RuntimeError("Snapshot missing cpu_pool or gpu_pool regions")

    base_addr = int(parsed.get("base_address", "0"), 16)
    cpu_data = bytes.fromhex(cpu_info["data"])
    gpu_data = bytes.fromhex(gpu_info["data"])
    gs_data = bytes.fromhex(globals_raw["game_state_globals"]["data"])
    xgs_data = bytes.fromhex(globals_raw.get("xbox_game_state_globals", {}).get("data", ""))

    gpu_base = int(gpu_info.get("virtual_addr", gpu_info.get("base", "0")), 16)
    header_addr = int(parsed.get("header_ptr", "0"), 16)
    header_data = bytes.fromhex(header_raw.get("data", ""))

    print(f"[*] Injecting snapshot: {snap.get('description', '(none)')}")
    print(f"    CPU pool: {len(cpu_data):#x} bytes → 0x{base_addr:08x}")
    print(f"    GPU pool: {len(gpu_data):#x} bytes → 0x{gpu_base:08x}")
    if header_data:
        print(f"    Header:   {len(header_data):#x} bytes → 0x{header_addr:08x}")

    # pause xemu via QMP for a clean injection window
    qmp = None
    try:
        qmp = qmp_connect()
        print("[*] Pausing xemu...")
        qmp.command("stop")
    except RuntimeError:
        print("[!] Could not pause via QMP (no QMP session?); injecting while running.")

    try:
        sock = gdb_connect()
        try:
            print("[*] Writing game_state_globals...")
            gdb_write_mem(sock, GS_GLOBALS_ADDR, gs_data)
            if xgs_data:
                gdb_write_mem(sock, XBOX_GS_GLOBALS_ADDR, xgs_data)

            if header_data and header_addr:
                print("[*] Writing save header...")
                gdb_write_mem(sock, header_addr, header_data)

            print(f"[*] Writing CPU pool ({len(cpu_data):#x} bytes)...")
            gdb_write_mem(sock, base_addr, cpu_data)

            print(f"[*] Writing GPU pool ({len(gpu_data):#x} bytes)...")
            gdb_write_mem(sock, gpu_base, gpu_data)

            print("[*] Dispatching post-load callbacks...")
            gdb_invoke_callbacks(sock)

        finally:
            sock.close()
    finally:
        if qmp is not None:
            print("[*] Resuming xemu...")
            qmp.command("cont")
            qmp.close()

    print("[✓] Snapshot injected.")


def convert_to_unicorn_snapshot(snapshot_path: str,
                               output_path: Optional[str] = None) -> dict:
    """Convert a game_state snapshot to a unicorn_diff-compatible
    flat state_snapshot format {int_addr: bytes}.

    Maps the CPU pool, GPU pool, header, and globals as flat address
    ranges suitable for --state-snapshot injection into Unicorn.
    """
    with open(snapshot_path, encoding="utf-8") as f:
        snap = json.load(f)

    regions_out: dict[int, bytes] = {}

    # globals
    globals_raw = snap.get("globals_raw", {})
    for key, entry in globals_raw.items():
        addr = int(entry["addr"], 16)
        data = bytes.fromhex(entry["data"])
        regions_out[addr] = data

    # header
    header_raw = snap.get("header_raw", {})
    if header_raw:
        addr = int(header_raw["addr"], 16)
        data = bytes.fromhex(header_raw["data"])
        regions_out[addr] = data

    # CPU pool
    cpu_info = snap.get("regions", {}).get("cpu_pool", {})
    if cpu_info:
        addr = int(cpu_info.get("virtual_addr", cpu_info.get("base", "0")), 16)
        data = bytes.fromhex(cpu_info["data"])
        regions_out[addr] = data

    # GPU pool
    gpu_info = snap.get("regions", {}).get("gpu_pool", {})
    if gpu_info:
        addr = int(gpu_info.get("virtual_addr", gpu_info.get("base", "0")), 16)
        data = bytes.fromhex(gpu_info["data"])
        regions_out[addr] = data

    result = {
        "description": f"converted from {snapshot_path}",
        "captured_at": snap.get("captured_at", ""),
        "regions": {f"0x{a:08x}": d.hex() for a, d in sorted(regions_out.items())},
    }

    if output_path:
        out = Path(output_path)
        out.parent.mkdir(parents=True, exist_ok=True)
        with open(out, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2)
            f.write("\n")
        print(f"[✓] Unicorn snapshot written to {out}")
        total = sum(len(d) for d in regions_out.values())
        print(f"    {len(regions_out)} regions, {total:#x} bytes total")

    return result
    """Load a previously captured snapshot."""
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# ── CLI ──────────────────────────────────────────────────────────────────


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture and replay Halo CE Xbox game-state snapshots",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    # capture
    cap = sub.add_parser("capture", help="Capture game state from running xemu")
    cap.add_argument("--description", default="",
                     help="Human-readable snapshot description")
    cap.add_argument("--output", default=None,
                     help="Output path (default: artifacts/snapshots/<timestamp>.json)")
    cap.add_argument("--gdb-host", default="localhost",
                     help="xemu GDB stub host")
    cap.add_argument("--gdb-port", type=int, default=1234,
                     help="xemu GDB stub port")
    cap.set_defaults(func=cmd_capture)

    # inject
    inj = sub.add_parser("inject", help="Inject a snapshot into running xemu")
    inj.add_argument("snapshot", help="Path to snapshot JSON file")
    inj.add_argument("--gdb-host", default="localhost",
                     help="xemu GDB stub host")
    inj.add_argument("--gdb-port", type=int, default=1234,
                     help="xemu GDB stub port")
    inj.set_defaults(func=cmd_inject)

    # info
    info = sub.add_parser("info", help="Show snapshot metadata")
    info.add_argument("snapshot", help="Path to snapshot JSON file")
    info.set_defaults(func=cmd_info)

    # convert (to unicorn state_snapshot format)
    conv = sub.add_parser("convert", help="Convert to unicorn --state-snapshot format")
    conv.add_argument("snapshot", help="Path to game-state snapshot JSON")
    conv.add_argument("--output", default=None,
                      help="Output path (default: <snapshot>_unicorn.json)")
    conv.set_defaults(func=cmd_convert)

    return parser


def cmd_capture(args) -> int:
    # set env vars for GDB connection
    if args.gdb_host:
        os.environ["XEMU_GDB_HOST"] = args.gdb_host
    if args.gdb_port:
        os.environ["XEMU_GDB_PORT"] = str(args.gdb_port)

    output = args.output
    if output is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
        output = str(DEFAULT_OUT_DIR / f"snapshot-{stamp}.json")

    capture_snapshot(description=args.description, output_path=output)
    return 0


def cmd_inject(args) -> int:
    if args.gdb_host:
        os.environ["XEMU_GDB_HOST"] = args.gdb_host
    if args.gdb_port:
        os.environ["XEMU_GDB_PORT"] = str(args.gdb_port)

    inject_snapshot(args.snapshot)
    return 0


def cmd_convert(args) -> int:
    output = args.output
    if output is None:
        output = str(Path(args.snapshot).with_suffix("")) + "_unicorn.json"
    convert_to_unicorn_snapshot(args.snapshot, output_path=output)
    return 0


def cmd_info(args) -> int:
    snap = load_snapshot(args.snapshot)
    parsed = snap.get("globals_parsed", {})
    regions = snap.get("regions", {})

    print(f"Description: {snap.get('description', '(none)')}")
    print(f"Captured:    {snap.get('captured_at', 'unknown')}")
    print(f"Base addr:   {parsed.get('base_address', '?')}")
    print(f"CPU size:    {parsed.get('cpu_allocation_size', '?')}")
    print(f"GPU size:    {parsed.get('gpu_allocation_size', '?')}")
    print(f"Header ptr:  {parsed.get('header_ptr', '?')}")
    print(f"Saved flag:  {parsed.get('saved', '?')}")
    print()
    for name, info in regions.items():
        print(f"  {name}: {info['base']} ({info['size']} = {int(info['size'], 16)} bytes)")
    return 0


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
