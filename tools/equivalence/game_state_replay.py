#!/usr/bin/env python3
"""Replay and compare game-state snapshots across Halo CE Xbox XBE builds.

Uses the game's own core-save/load pipeline (debug feature built into the
retail binary) to create named save-game files on the Xbox HDD, then loads
them back on a different XBE build for A/B comparison.

Workflow:
  1. Play to interesting state on oracle XBE
  2. Trigger game_state_save_core("test_snap") via GDB + pending flag
  3. The game writes d:\\test_snap.bin (persists on xemu HDD)
  4. Launch candidate XBE (same xemu HDD image)
  5. Trigger game_state_load_core("test_snap") via GDB + pending flag
  6. Game restores state + runs 13 post-load callbacks
  7. Advance N frames with xbox_pad.py inputs
  8. Capture resulting game state via game_state_snapshot.py
  9. Diff oracle vs candidate pool dumps

Key global addresses (Xbox virtual, identity-mapped in xemu):
  0x46dd55: core_name[] global buffer (256 bytes)
  0x46da3d: game_state_save_core_pending (uint8_t)
  0x46da3e: game_state_load_core_pending (uint8_t)
  0x4ea990: game_state_globals_t
"""

from __future__ import annotations

import argparse
import json
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_OUT_DIR = ROOT / "artifacts" / "replay"

# ── known Xbox globals ───────────────────────────────────────────────
CORE_NAME_ADDR = 0x46DD55          # char core_name[256]
SAVE_CORE_PENDING = 0x46DA3D       # uint8_t
LOAD_CORE_PENDING = 0x46DA3E       # uint8_t
GAME_STATE_SAVE_PENDING = 0x46DA2B
GAME_STATE_REVERT_PENDING = 0x46DA26

GS_GLOBALS_ADDR = 0x4EA990         # game_state_globals_t
GS_GLOBALS_SIZE = 0x20
GS_HEADER_SIZE = 0x14C

# ── GDB RSP helpers ──────────────────────────────────────────────────

def _gdb_checksum(data: str) -> int:
    return sum(ord(c) for c in data) & 0xFF


def _gdb_send_recv(sock: socket.socket, packet: str) -> str:
    chk = _gdb_checksum(packet)
    wire = f"${packet}#{chk:02x}"
    sock.sendall(wire.encode("ascii"))

    ack = sock.recv(1)
    if ack != b"+":
        raise RuntimeError(f"GDB no ACK, got: {ack!r}")

    # read responses, skipping any notification packets
    while True:
        prefix = sock.recv(1)
        if not prefix:
            raise RuntimeError("GDB connection closed")
        if prefix == b"%":
            _buf = b""
            while True:
                c = sock.recv(1)
                if not c:
                    raise RuntimeError("GDB connection closed")
                if c == b"#":
                    break
                _buf += c
            sock.recv(2)
            continue
        if prefix == b"$":
            break
        raise RuntimeError(f"GDB unexpected prefix: {prefix!r}")

    buf = b""
    while True:
        c = sock.recv(1)
        if not c:
            raise RuntimeError("GDB connection closed")
        if c == b"#":
            break
        buf += c

    csum_hex = sock.recv(2)
    got = int(csum_hex, 16)
    expected = _gdb_checksum(buf.decode("ascii"))
    if got != expected:
        raise RuntimeError(f"GDB checksum mismatch: {got:02x} != {expected:02x}")
    sock.sendall(b"+")
    return buf.decode("ascii")


def gdb_connect(timeout: float = 5.0) -> socket.socket:
    host = os.environ.get("XEMU_GDB_HOST", "localhost")
    port = int(os.environ.get("XEMU_GDB_PORT", "1234"))
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.settimeout(timeout)
    return sock


def gdb_write_u8(sock: socket.socket, addr: int, value: int) -> None:
    """Write a single byte via GDB RSP."""
    hex_data = f"{value & 0xFF:02x}"
    packet = f"M{addr:x},1:{hex_data}"
    resp = _gdb_send_recv(sock, packet)
    if resp != "OK":
        raise RuntimeError(f"GDB write error at 0x{addr:x}: {resp}")


def gdb_write_str(sock: socket.socket, addr: int, text: str, max_len: int = 256) -> None:
    """Write a null-terminated string via GDB RSP."""
    encoded = text.encode("ascii", errors="replace") + b"\x00"
    if len(encoded) > max_len:
        encoded = encoded[:max_len]
    hex_data = encoded.hex()
    packet = f"M{addr:x},{len(encoded):x}:{hex_data}"
    resp = _gdb_send_recv(sock, packet)
    if resp != "OK":
        raise RuntimeError(f"GDB write error at 0x{addr:x}: {resp}")


def gdb_read_u8(sock: socket.socket, addr: int) -> int:
    """Read a single byte via GDB RSP."""
    packet = f"m{addr:x},1"
    resp = _gdb_send_recv(sock, packet)
    if resp.startswith("E"):
        raise RuntimeError(f"GDB read error at 0x{addr:x}: {resp}")
    return int(resp, 16)


def gdb_read_mem(sock: socket.socket, addr: int, size: int) -> bytes:
    """Read memory from guest via GDB RSP (chunked, max 4096 bytes/packet)."""
    if size <= 0:
        return b""
    result = bytearray()
    offset = 0
    while offset < size:
        chunk = min(size - offset, 4096)
        pkt = f"m{addr + offset:x},{chunk:x}"
        resp = _gdb_send_recv(sock, pkt)
        if resp.startswith("E"):
            raise RuntimeError(f"GDB read error at 0x{addr + offset:x}: {resp}")
        try:
            result.extend(bytes.fromhex(resp))
        except ValueError as e:
            raise RuntimeError(
                f"GDB hex parse error at 0x{addr + offset:x}: {e}") from e
        offset += chunk
    return bytes(result)


def gdb_write_mem(sock: socket.socket, addr: int, data: bytes) -> None:
    """Write memory via GDB RSP."""
    if not data:
        return
    offset = 0
    while offset < len(data):
        chunk = min(len(data) - offset, 4096)
        chunk_data = data[offset:offset + chunk]
        hex_data = chunk_data.hex()
        pkt = f"M{addr + offset:x},{chunk:x}:{hex_data}"
        resp = _gdb_send_recv(sock, pkt)
        if resp != "OK":
            raise RuntimeError(f"GDB write error at 0x{addr + offset:x}: {resp}")
        offset += chunk


# ── QMP helpers ──────────────────────────────────────────────────────

def qmp_session():
    qmp_mod = ROOT / "tools" / "xbox" / "xemu_qmp.py"
    import importlib.util
    spec = importlib.util.spec_from_file_location("xemu_qmp", str(qmp_mod))
    qmp = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(qmp)
    session = qmp.discover_session()
    if session is None:
        raise RuntimeError("No running xemu QMP session found")
    return session


def qmp_hmp(session, cmd: str) -> str:
    result = session.command("human-monitor-command", {"command-line": cmd})
    return str(result) if result else ""


# ── Replay operations ────────────────────────────────────────────────


def trigger_core_save(name: str, timeout: float = 10.0) -> None:
    """Trigger game_state_save_core(name) by writing the core_name global
    and setting the save_core_pending flag.  Waits for the flag to clear."""
    sock = gdb_connect()
    try:
        gdb_write_str(sock, CORE_NAME_ADDR, name)
        gdb_write_u8(sock, SAVE_CORE_PENDING, 1)
        # verify writes took effect
        written_flag = gdb_read_u8(sock, SAVE_CORE_PENDING)
        if written_flag != 1:
            raise RuntimeError(
                f"Failed to set save_core_pending: wrote 1, read back {written_flag}")
        # check the core_name was written (first few bytes)
        verify = gdb_read_mem(sock, CORE_NAME_ADDR, min(len(name) + 1, 64))
        read_name = verify.split(b"\x00", 1)[0].decode("ascii", errors="replace")
        if read_name != name:
            raise RuntimeError(
                f"core_name write failed: wrote '{name}', read '{read_name}'")
    finally:
        sock.close()

    print(f"[*] Triggered core save: '{name}'")
    print(f"    Waiting for save to complete (timeout={timeout}s)...")
    deadline = time.time() + timeout
    last_flag = None
    while time.time() < deadline:
        sock = gdb_connect()
        try:
            flag = gdb_read_u8(sock, SAVE_CORE_PENDING)
        except Exception as e:
            print(f"    (GDB read error: {e})")
            time.sleep(0.5)
            continue
        finally:
            sock.close()
        if flag == 0:
            print("    Save completed.")
            return
        if flag != last_flag:
            print(f"    pending flag = 0x{flag:02x}")
            last_flag = flag
        time.sleep(0.3)
    raise RuntimeError(f"Core save timed out after {timeout}s "
                       f"(flag still 0x{last_flag:02x} — is the game in a level, not paused?)")


def trigger_core_load(name: str, timeout: float = 15.0) -> None:
    """Trigger game_state_load_core(name) by writing the core_name global
    and setting the load_core_pending flag.  Waits for the flag to clear."""
    sock = gdb_connect()
    try:
        gdb_write_str(sock, CORE_NAME_ADDR, name)
        gdb_write_u8(sock, LOAD_CORE_PENDING, 1)
        written_flag = gdb_read_u8(sock, LOAD_CORE_PENDING)
        if written_flag != 1:
            raise RuntimeError(
                f"Failed to set load_core_pending: wrote 1, read back {written_flag}")
        verify = gdb_read_mem(sock, CORE_NAME_ADDR, min(len(name) + 1, 64))
        read_name = verify.split(b"\x00", 1)[0].decode("ascii", errors="replace")
        if read_name != name:
            raise RuntimeError(
                f"core_name write failed: wrote '{name}', read '{read_name}'")
    finally:
        sock.close()

    print(f"[*] Triggered core load: '{name}'")
    print(f"    Waiting for load to complete (timeout={timeout}s)...")
    deadline = time.time() + timeout
    last_flag = None
    while time.time() < deadline:
        sock = gdb_connect()
        try:
            flag = gdb_read_u8(sock, LOAD_CORE_PENDING)
        except Exception as e:
            print(f"    (GDB read error: {e})")
            time.sleep(0.5)
            continue
        finally:
            sock.close()
        if flag == 0:
            print("    Load completed.")
            return
        if flag != last_flag:
            print(f"    pending flag = 0x{flag:02x}")
            last_flag = flag
        time.sleep(0.3)
    raise RuntimeError(f"Core load timed out after {timeout}s "
                       f"(flag still 0x{last_flag:02x} — is the game in a level, not paused?)")


def wait_for_main_loop(timeout: float = 120.0) -> None:
    """Wait for the game to enter the main loop (e.g. after loading a map).
    
    Polls game_state_revert_pending and game_state_save_pending as a
    proxy signal: when the main loop is active, these update each tick.
    """
    print(f"[*] Waiting for game main loop (timeout={timeout}s)...")
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            sock = gdb_connect(timeout=2.0)
        except (OSError, socket.timeout):
            time.sleep(1.0)
            continue
        try:
            # Check that the save pending flag is accessible (game_started)
            _ = gdb_read_u8(sock, GAME_STATE_SAVE_PENDING)
            flag = gdb_read_u8(sock, SAVE_CORE_PENDING)
        except RuntimeError:
            sock.close()
            time.sleep(1.0)
            continue
        finally:
            sock.close()
        if flag == 0:
            print("    Game appears to be in main loop.")
            return
        time.sleep(1.0)
    raise RuntimeError(f"Timed out waiting for main loop after {timeout}s")


def capture_current_state(output_path: Optional[str] = None,
                          description: str = "") -> dict:
    """Capture the current game state from xemu using the snapshot tool."""
    # Import our snapshot module
    snap_mod_path = ROOT / "tools" / "equivalence" / "game_state_snapshot.py"
    import importlib.util
    spec = importlib.util.spec_from_file_location("game_state_snapshot",
                                                  str(snap_mod_path))
    snap = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(snap)
    return snap.capture_snapshot(description=description,
                                 output_path=output_path)


def diff_snapshots(baseline: dict, candidate: dict) -> dict:
    """Compare two snapshot dicts and return a diff report."""
    report = {
        "baseline_description": baseline.get("description", ""),
        "candidate_description": candidate.get("description", ""),
        "regions": {},
        "globals_diff": {},
        "layout_diff": {},
    }

    # Compare parsed globals (allocation sizes, pointers)
    b_parsed = baseline.get("globals_parsed", {})
    c_parsed = candidate.get("globals_parsed", {})
    layout_keys = ["base_address", "cpu_allocation_size", "gpu_allocation_size",
                   "header_ptr"]
    for key in layout_keys:
        bv = b_parsed.get(key, "")
        cv = c_parsed.get(key, "")
        if bv != cv:
            report["layout_diff"][key] = {"baseline": bv, "candidate": cv}

    baseline_regions = baseline.get("regions", {})
    candidate_regions = candidate.get("regions", {})

    for region_name in baseline_regions:
        b_info = baseline_regions[region_name]
        c_info = candidate_regions.get(region_name)

        b_data = bytes.fromhex(b_info["data"])
        if c_info is None:
            report["regions"][region_name] = {
                "status": "missing_in_candidate",
                "baseline_size": len(b_data),
            }
            continue

        c_data = bytes.fromhex(c_info["data"])

        if b_data == c_data:
            report["regions"][region_name] = {
                "status": "identical",
                "size": len(b_data),
            }
            continue

        # Find differing byte ranges
        diffs = []
        differ_start = None
        min_len = min(len(b_data), len(c_data))
        for i in range(min_len):
            if b_data[i] != c_data[i]:
                if differ_start is None:
                    differ_start = i
            else:
                if differ_start is not None:
                    diffs.append({
                        "offset": differ_start,
                        "length": i - differ_start,
                        "baseline": b_data[differ_start:i].hex(),
                        "candidate": c_data[differ_start:i].hex(),
                    })
                    differ_start = None
        if differ_start is not None:
            diffs.append({
                "offset": differ_start,
                "length": min_len - differ_start,
                "baseline": b_data[differ_start:min_len].hex(),
                "candidate": c_data[differ_start:min_len].hex(),
            })

        size_msg = ""
        if len(b_data) != len(c_data):
            size_msg = f" (size diff: baseline={len(b_data)}, candidate={len(c_data)})"

        report["regions"][region_name] = {
            "status": "divergent",
            "size": f"baseline={len(b_data)} candidate={len(c_data)}",
            "num_diffs": len(diffs),
            "total_diff_bytes": sum(d["length"] for d in diffs),
            "diffs": diffs[:20],  # cap at 20 diff ranges for readability
        }
        print(f"  {region_name}: {len(diffs)} diff ranges, "
              f"{report['regions'][region_name]['total_diff_bytes']} bytes differ"
              f"{size_msg}")

    # Compare globals
    b_globals = baseline.get("globals_raw", {})
    c_globals = candidate.get("globals_raw", {})
    for key in b_globals:
        b_entry = b_globals[key]
        c_entry = c_globals.get(key)
        if c_entry is None:
            report["globals_diff"][key] = "missing_in_candidate"
        elif b_entry["data"] != c_entry["data"]:
            report["globals_diff"][key] = "divergent"
        else:
            report["globals_diff"][key] = "identical"

    return report


# ── CLI ──────────────────────────────────────────────────────────────


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Replay and compare Halo CE Xbox game-state snapshots",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    # trigger-save
    save_cmd = sub.add_parser("trigger-save",
                              help="Trigger a core save on the running xemu")
    save_cmd.add_argument("--name", default="test_snap",
                          help="Core save name (default: test_snap)")
    save_cmd.add_argument("--timeout", type=float, default=10.0,
                          help="Seconds to wait for save completion")
    save_cmd.set_defaults(func=cmd_trigger_save)

    # trigger-load
    load_cmd = sub.add_parser("trigger-load",
                              help="Trigger a core load on the running xemu")
    load_cmd.add_argument("--name", default="test_snap",
                          help="Core save name to load (default: test_snap)")
    load_cmd.add_argument("--timeout", type=float, default=15.0,
                          help="Seconds to wait for load completion")
    load_cmd.set_defaults(func=cmd_trigger_load)

    # snapshot-after
    snap_cmd = sub.add_parser("capture-after",
                              help="Capture game state after a save/load operation")
    snap_cmd.add_argument("--description", default="",
                          help="Snapshot description")
    snap_cmd.add_argument("--output", default=None,
                          help="Output path for snapshot JSON")
    snap_cmd.set_defaults(func=cmd_capture_after)
    # status check
    status_cmd = sub.add_parser("status",
                                help="Check game state flags (main loop active, etc.)")
    status_cmd.set_defaults(func=cmd_status)

    # diff

    diff_cmd = sub.add_parser("diff",
                              help="Diff two snapshot JSON files")
    diff_cmd.add_argument("baseline", help="Baseline (oracle) snapshot JSON")
    diff_cmd.add_argument("candidate", help="Candidate (patched) snapshot JSON")
    diff_cmd.add_argument("--output", default=None,
                          help="Output path for diff report JSON")
    diff_cmd.add_argument("--quiet", action="store_true",
                          help="Only print summary line")
    diff_cmd.set_defaults(func=cmd_diff)

    # full compare workflow
    compare_cmd = sub.add_parser("compare",
                                 help="Full A/B comparison: trigger load on candidate, capture, diff")
    compare_cmd.add_argument("--name", default="test_snap",
                             help="Core save name to load")
    compare_cmd.add_argument("--baseline-snapshot", required=True,
                             help="Baseline (oracle) snapshot to compare against")
    compare_cmd.add_argument("--output", default=None,
                             help="Output path for candidate snapshot")
    compare_cmd.add_argument("--diff-output", default=None,
                             help="Output path for diff report JSON")
    compare_cmd.add_argument("--timeout", type=float, default=15.0,
                             help="Seconds to wait for load completion")
    compare_cmd.set_defaults(func=cmd_compare)

    # wait
    wait_cmd = sub.add_parser("wait",
                              help="Wait for game to enter main loop")
    wait_cmd.add_argument("--timeout", type=float, default=120.0,
                          help="Seconds to wait")
    wait_cmd.set_defaults(func=cmd_wait)

    return parser


def cmd_status(args) -> int:
    """Print current game state flags for diagnostics."""
    sock = gdb_connect()
    try:
        save_pending = gdb_read_u8(sock, GAME_STATE_SAVE_PENDING)
        revert_pending = gdb_read_u8(sock, GAME_STATE_REVERT_PENDING)
        save_core = gdb_read_u8(sock, SAVE_CORE_PENDING)
        load_core = gdb_read_u8(sock, LOAD_CORE_PENDING)
        saved_flag = gdb_read_u8(sock, 0x4ea995)  # game_state_globals.saved
    finally:
        sock.close()

    print(f"save_pending:    0x{save_pending:02x} (auto-save checkpoint)")
    print(f"revert_pending:  0x{revert_pending:02x} (revert to checkpoint)")
    print(f"save_core_pend:  0x{save_core:02x} (debug core save)")
    print(f"load_core_pend:  0x{load_core:02x} (debug core load)")
    print(f"game_saved:      0x{saved_flag:02x} (valid checkpoint exists)")

    if save_core == 0 and load_core == 0 and save_pending == 0:
        print("\nGame appears to be in main loop (no pending operations).")
    else:
        print("\nGame has pending operations — wait for them to complete.")

    return 0


def cmd_trigger_save(args) -> int:
    trigger_core_save(args.name, args.timeout)
    return 0


def cmd_trigger_load(args) -> int:
    trigger_core_load(args.name, args.timeout)
    return 0


def cmd_capture_after(args) -> int:
    output = args.output
    if output is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
        output = str(DEFAULT_OUT_DIR / f"capture-{stamp}.json")
    capture_current_state(output_path=output, description=args.description)
    return 0


def cmd_diff(args) -> int:
    with open(args.baseline, encoding="utf-8") as f:
        baseline = json.load(f)
    with open(args.candidate, encoding="utf-8") as f:
        candidate = json.load(f)

    report = diff_snapshots(baseline, candidate)

    total_diffs = sum(
        r.get("total_diff_bytes", 0)
        for r in report["regions"].values()
    )
    all_identical = all(
        r["status"] == "identical"
        for r in report["regions"].values()
    )

    if not args.quiet:
        # show layout differences
        layout = report.get("layout_diff", {})
        if layout:
            print(f"\nLayout differences:")
            for key, vals in layout.items():
                print(f"  {key}: baseline={vals['baseline']} candidate={vals['candidate']}")

        # show first diff ranges for each diverging region
        for name, region in report["regions"].items():
            if region["status"] == "divergent":
                offsets = [d["offset"] for d in region["diffs"][:5]]
                print(f"\n{name} first diff offsets: {[f'0x{o:x}' for o in offsets]}")

        print(f"\nSummary: {'IDENTICAL' if all_identical else 'DIVERGENT'} "
              f"({total_diffs} bytes differ)")

    if args.output:
        out_path = Path(args.output)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)
            f.write("\n")
        if not args.quiet:
            print(f"Diff report written to {out_path}")

    return 0 if all_identical else 1


def cmd_compare(args) -> int:
    """Full comparison: load save on current XBE, capture, diff vs baseline."""
    # Load the core save
    trigger_core_load(args.name, args.timeout)

    # Capture the result
    output = args.output
    if output is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
        output = str(DEFAULT_OUT_DIR / f"candidate-{stamp}.json")

    print("[*] Capturing candidate state...")
    candidate = capture_current_state(output_path=output,
                                      description="candidate after load")

    # Load baseline
    with open(args.baseline_snapshot, encoding="utf-8") as f:
        baseline = json.load(f)

    # Diff
    diff_output = args.diff_output
    if diff_output is None:
        diff_output = str(Path(output).with_suffix(".diff.json"))

    report = diff_snapshots(baseline, candidate)
    with open(diff_output, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
        f.write("\n")

    all_identical = all(
        r["status"] == "identical"
        for r in report["regions"].values()
    )
    total_diffs = sum(
        r.get("total_diff_bytes", 0)
        for r in report["regions"].values()
    )
    print(f"\nComparison: {'IDENTICAL' if all_identical else 'DIVERGENT'} "
          f"({total_diffs} bytes differ)")
    print(f"Candidate snapshot: {output}")
    print(f"Diff report: {diff_output}")

    return 0 if all_identical else 1


def cmd_wait(args) -> int:
    wait_for_main_loop(args.timeout)
    return 0


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
