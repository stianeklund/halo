#!/usr/bin/env python3
"""Deploy pre-decompressed cache files to Xbox z:\ partition via XBDM.

Halo CE Xbox maps are compressed on disc (d:\maps\*.map) and decompressed
to z:\cache000.map–cache005.map on the HDD during loading. Pre-populating
these cache files skips decompression entirely.

CRITICAL: All 6 cache slots must be present with exact sizes, or the init
code (FUN_001bd5f0) will delete slots when it encounters a size mismatch.

Usage:
  # Deploy wizard and bloodgulch with placeholders for other slots
  python3 tools/xbox/deploy_cache.py --src /tmp/halo_cache

  # Deploy specific slots only (dangerous — see above)
  python3 tools/xbox/deploy_cache.py --src /tmp/halo_cache --slots 3 4

  # Verify z:\ contents without uploading
  python3 tools/xbox/deploy_cache.py --verify-only

  # Generate cache files from compressed maps, then deploy
  python3 tools/xbox/decompress_maps.py --src halo-patched/maps --dst /tmp/halo_cache \\
      --cache-slots wizard:3 bloodgulch:4 --with-placeholders
  python3 tools/xbox/deploy_cache.py --src /tmp/halo_cache
"""

from __future__ import annotations

import argparse
import os
import sys
import time

_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from internal.local_env import load_repo_env, maybe_reexec_on_windows

load_repo_env("xbox.env")
maybe_reexec_on_windows(__file__)

from xbox.xbdm_rdcp import RdcpClient, RdcpError

DEFAULT_HOST = os.environ.get("XBDM_HOST", "localhost")
DEFAULT_PORT = int(os.environ.get("XBDM_PORT", "731"))
DEFAULT_TIMEOUT = float(os.environ.get("XBDM_TIMEOUT", "10.0"))

SLOT_SIZES = {
    0: 0x11600000,
    1: 0x11600000,
    2: 0x02300000,
    3: 0x02F00000,
    4: 0x02F00000,
    5: 0x02F00000,
}

TOTAL_BYTES = sum(SLOT_SIZES.values())


def verify_local_files(src_dir: str, slots: list[int] | None) -> list[tuple[int, str]]:
    target_slots = slots if slots else list(range(6))
    files = []
    errors = []

    for slot in target_slots:
        fname = f"cache{slot:03d}.map"
        path = os.path.join(src_dir, fname)
        if not os.path.isfile(path):
            errors.append(f"  missing: {path}")
            continue
        size = os.path.getsize(path)
        expected = SLOT_SIZES[slot]
        if size != expected:
            errors.append(f"  {fname}: {size:,} bytes (expected {expected:,})")
            continue
        files.append((slot, path))

    if errors:
        print("ERROR: local cache files have problems:", file=sys.stderr)
        for e in errors:
            print(e, file=sys.stderr)
        return []

    return files


def list_xbox_cache(client: RdcpClient) -> dict[str, int]:
    resp = client.send_command_and_read('dirlist name="z:\\"')
    if resp.code != 202:
        print(f"  z:\\ listing failed: {resp.code} {resp.message}")
        return {}

    files = {}
    for line in resp.lines:
        parts = {}
        for token in line.split():
            if "=" in token:
                k, v = token.split("=", 1)
                parts[k] = v.strip('"')
        name = parts.get("name", "")
        sizehi = int(parts.get("sizehi", "0"), 0)
        sizelo = int(parts.get("sizelo", "0"), 0)
        size = (sizehi << 32) | sizelo
        if name.lower().startswith("cache") and name.lower().endswith(".map"):
            files[name.lower()] = size
    return files


def deploy_file(client: RdcpClient, local_path: str, xbox_path: str,
                quiet: bool = False) -> bool:
    size = os.path.getsize(local_path)
    fname = os.path.basename(local_path)
    if not quiet:
        mb = size / (1024 * 1024)
        print(f"  uploading {fname} ({mb:.1f} MB) -> {xbox_path} ...", end="", flush=True)

    start = time.time()
    try:
        resp = client.send_file(local_path, xbox_path)
    except RdcpError as e:
        if not quiet:
            print(f" FAILED: {e}")
        return False

    elapsed = time.time() - start
    if resp.code == 200:
        rate = (size / (1024 * 1024)) / elapsed if elapsed > 0 else 0
        if not quiet:
            print(f" OK ({elapsed:.1f}s, {rate:.1f} MB/s)")
        return True
    else:
        if not quiet:
            print(f" FAILED: {resp.code} {resp.message}")
        return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Deploy cache files to Xbox z:\\")
    parser.add_argument("--src", help="Directory containing cache*.map files")
    parser.add_argument("--slots", nargs="*", type=int, metavar="N",
                        help="Deploy only these slot numbers (default: all 6)")
    parser.add_argument("--verify-only", action="store_true",
                        help="List z:\\ contents without uploading")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if not args.src and not args.verify_only:
        parser.error("--src is required (or use --verify-only)")

    if args.src:
        files = verify_local_files(args.src, args.slots)
        if not files:
            return 1

        if not args.slots:
            print(f"Deploying all 6 cache slots ({TOTAL_BYTES / (1024*1024):.0f} MB total)")
        else:
            total = sum(SLOT_SIZES[s] for s in args.slots)
            print(f"Deploying slots {args.slots} ({total / (1024*1024):.0f} MB)")
        for slot, path in files:
            size = os.path.getsize(path)
            print(f"  slot {slot}: {os.path.basename(path)} ({size:,} bytes)")

    if args.dry_run:
        print("\n[DRY-RUN] Would upload the above files to z:\\")
        return 0

    try:
        client = RdcpClient(args.host, args.port, timeout=args.timeout)
    except RdcpError as e:
        print(f"ERROR: cannot connect to Xbox at {args.host}:{args.port}: {e}",
              file=sys.stderr)
        return 1

    print(f"\nConnected to Xbox at {args.host}:{args.port}")

    if args.verify_only:
        print("\nz:\\ contents:")
        existing = list_xbox_cache(client)
        if not existing:
            print("  (empty or inaccessible)")
        for name, size in sorted(existing.items()):
            slot_num = -1
            try:
                slot_num = int(name.replace("cache", "").replace(".map", ""))
            except ValueError:
                pass
            expected = SLOT_SIZES.get(slot_num, 0)
            status = "OK" if size == expected else f"WRONG (expected {expected:,})"
            print(f"  {name}: {size:>12,} bytes [{status}]")
        client.close()
        return 0

    print("\nDeploying cache files to z:\\...")
    ok = 0
    fail = 0
    for slot, path in files:
        xbox_path = f"z:\\cache{slot:03d}.map"
        if deploy_file(client, path, xbox_path):
            ok += 1
        else:
            fail += 1

    client.close()
    print(f"\n{ok} deployed, {fail} failed")

    if fail > 0:
        return 1

    print("\nAll cache files deployed. Reboot the Xbox to pick up the new cache.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
