#!/usr/bin/env python3
r"""Clear Halo CE cache files from Xbox cache partitions via XBDM/RDCP.

This tool surgically removes only Halo CE related cache files from devkit cache
partitions (P:, T:, U:, and Z:). The retail binary's confirmed runtime writes
use z:\ paths; P:/T:/U: are scanned conservatively for devkit-visible cache
aliases.

For devkits:
  P: - All titles cache (cache000.map, etc.; scanned conservatively)
  T: - Persistent Data - Active Title (temporary data)
  U: - Saved Games - Active Title (save data)
  Z: - Title cache/state files (cacheNNN.map, last_solo.txt, etc.)
       z:\saved is preserved unless --deep is passed
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

# Devkit cache partitions that Halo uses
# Note: Trailing backslash is REQUIRED for XBDM
def make_drive_path(letter: str) -> str:
    return f"{letter}:\\"

CACHE_PARTITIONS = ["P", "T", "U", "Z"]

# Known Halo map names for conservative devkit partition scanning.
# Note: "halo" is too generic and matches non-Halo files like "TitleMeta.xbx".
HALO_FILE_PATTERNS = [
    "bloodgulch", "chillout", "damnation", "gephyrophobia",
    "hangemhigh", "ratrace", "prisoner", "wizard", "ui",
    "beavercreek", "boardingaction", "carousel", "dangercanyon",
    "deathisland", "derelict", "icefields", "infinity",
    "sidewinder", "timberland", "putput", "longest",
    "a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40",
]

# Confirmed root z:\ files written by the Halo CE Xbox binary.
Z_ROOT_FILES = {
    "last_solo.txt",
    "lastprof.txt",
    "lastmpvr.txt",
    "lastmpmp.txt",
    "savegame.bin",
}


def is_cache_slot_map(filename: str) -> bool:
    """Return true for cache000.map through cache019.map."""
    filename_lower = filename.lower()
    if not (filename_lower.startswith("cache") and filename_lower.endswith(".map")):
        return False
    slot_text = filename_lower[5:-4]
    return len(slot_text) == 3 and slot_text.isdigit() and int(slot_text) < 20


def is_halo_map_file(filename: str) -> bool:
    """Check if a map filename matches known Halo CE cache/map patterns."""
    filename_lower = filename.lower()

    if not filename_lower.endswith(".map"):
        return False
    if is_cache_slot_map(filename_lower):
        return True

    name_without_ext = filename_lower[:-4]
    return any(name_without_ext.startswith(pattern.lower())
               for pattern in HALO_FILE_PATTERNS)


def is_halo_cache_path(drive_letter: str, current_path: str, filename: str,
                       deep: bool) -> bool:
    """Return true if this exact Xbox path is a confirmed Halo cache target."""
    drive = drive_letter.upper()
    filename_lower = filename.lower()
    full_path = f"{current_path}{filename}".lower()

    if drive == "Z":
        if full_path.startswith("z:\\saved\\"):
            if not deep:
                return False
            saved_rel = full_path[len("z:\\saved\\"):]
            if saved_rel == "hdmu.map":
                return True
            if (saved_rel.startswith("player_profiles\\default_profile\\") and
                    filename_lower.endswith(".sav")):
                return True
            if (saved_rel.startswith("playlists\\default_playlist\\") and
                    filename_lower == "blam.lst"):
                return True
            return False

        if filename_lower in Z_ROOT_FILES:
            return True
        return is_cache_slot_map(filename_lower)

    if drive in ("P", "T", "U"):
        return is_halo_map_file(filename_lower)

    return False


def should_recurse_directory(drive_letter: str, current_path: str, dirname: str,
                             deep: bool) -> bool:
    """Decide whether recursion is safe for this directory."""
    if drive_letter.upper() == "Z":
        full_path = f"{current_path}{dirname}".lower()
        if full_path == "z:\\saved":
            return deep
    return True


def list_files_in_path(client: RdcpClient, path: str) -> tuple[list[dict], int, str]:
    """List all files/directories at a given XBDM path.
    
    Returns:
        Tuple of (files_list, error_code, error_message)
    """
    # IMPORTANT: Must use trailing backslash for XBDM directories
    command = f'dirlist name="{path}"'
    response = client.command(command)
    
    if not response.is_success:
        return [], response.code, response.message
    
    if response.lines is None:
        return [], 0, ""
    
    files = []
    for line in response.lines:
        if line.strip() in (".", ".."):
            continue
            
        file_info = {"name": "", "size": 0, "is_dir": False}
        
        parts = line.split()
        for part in parts:
            if part.startswith("name="):
                name = part[5:].strip('"')
                if name not in (".", ".."):
                    file_info["name"] = name
            elif part.startswith("sizehi="):
                try:
                    size_hi = int(part[7:], 0)
                    file_info["size"] = (file_info["size"] & 0xFFFFFFFF) | (size_hi << 32)
                except ValueError:
                    pass
            elif part.startswith("sizelo="):
                try:
                    size_lo = int(part[7:], 0)
                    file_info["size"] = (file_info["size"] & 0xFFFFFFFF00000000) | size_lo
                except ValueError:
                    pass
            elif part == "directory":
                file_info["is_dir"] = True
        
        if file_info["name"]:
            files.append(file_info)
    
    return files, 0, ""


def reboot_and_reconnect(client: RdcpClient, timeout_seconds: float = 20.0) -> bool:
    """Reboot the Xbox and reconnect XBDM. Returns True on success."""
    print("\n  Files locked by running title - rebooting Xbox...")
    try:
        client.command("reboot")
    except RdcpError:
        pass
    client.close()

    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        time.sleep(1.0)
        try:
            client.connect()
            print("  XBDM reachable again after reboot")
            return True
        except RdcpError:
            pass

    print(
        f"  warning: XBDM did not come back within {timeout_seconds:.1f}s",
        file=sys.stderr,
    )
    return False


def delete_item(client: RdcpClient, full_path: str, is_dir: bool = False,
                dry_run: bool = False) -> tuple[bool, bool]:
    """Delete a file or directory from the Xbox.

    Returns:
        (success, needs_reboot) - needs_reboot is True when a 414 (locked)
        error indicates the title must be stopped first.
    """
    if dry_run:
        item_type = "directory" if is_dir else "file"
        print(f"      [DRY-RUN] Would delete {item_type}: {full_path}")
        return True, False

    if is_dir:
        command = f'delete name="{full_path}" dir'
    else:
        command = f'delete name="{full_path}"'

    response = client.command(command)

    if response.is_success:
        return True, False
    elif response.code == 402:  # Not found
        return True, False
    elif response.code == 414:  # File is locked / in use
        print(f"        Locked: {full_path} (414 - file in use by running title)")
        return False, True
    else:
        print(f"        Error: {response.code}- {response.message}",
              file=sys.stderr)
        return False, False


def clear_path_recursive(client: RdcpClient, drive_letter: str,
                        current_path: str,
                        dry_run: bool = False, verbose: bool = False,
                        deep: bool = False) -> tuple[int, int, list[str], int, bool]:
    """Recursively clear Halo cache from a path.

    Returns:
        Tuple of (deleted_count, failed_count, list_of_deleted_names, total_size,
                  hit_locked) - hit_locked is True if any file returned 414.
    """
    files, error_code, error_msg = list_files_in_path(client, current_path)

    if error_code == 402:
        return (0, 0, [], 0, False)
    elif error_code != 0:
        if verbose:
            print(f"    Error listing {current_path}: {error_code}- {error_msg}")
        return (0, 1, [], 0, False)

    if not files:
        return (0, 0, [], 0, False)

    deleted = 0
    failed = 0
    total_size = 0
    deleted_names = []
    hit_locked = False

    for item in files:
        item_name = item["name"]
        full_path = f"{current_path}{item_name}"

        if item.get("is_dir"):
            if not should_recurse_directory(drive_letter, current_path, item_name, deep):
                if verbose:
                    print(f"    Skipping protected directory: {full_path}")
                continue

            sub_path = f"{full_path}\\"
            sub_deleted, sub_failed, sub_names, sub_size, sub_locked = clear_path_recursive(
                client, drive_letter, sub_path, dry_run, verbose, deep
            )
            deleted += sub_deleted
            failed += sub_failed
            deleted_names.extend(sub_names)
            total_size += sub_size
            hit_locked = hit_locked or sub_locked
        else:
            if is_halo_cache_path(drive_letter, current_path, item_name, deep):
                if dry_run:
                    size_mb = item.get("size", 0) / (1024 * 1024)
                    print(f"      [DRY-RUN] file: {full_path} ({size_mb:.2f} MB)")
                    deleted += 1
                    total_size += item.get("size", 0)
                    deleted_names.append(full_path)
                else:
                    ok, locked = delete_item(client, full_path, is_dir=False, dry_run=dry_run)
                    hit_locked = hit_locked or locked
                    if ok:
                        deleted += 1
                        total_size += item.get("size", 0)
                        deleted_names.append(full_path)
                        if verbose:
                            print(f"      Deleted: {full_path}")
                    else:
                        failed += 1

    return (deleted, failed, deleted_names, total_size, hit_locked)


def clear_partition(client: RdcpClient, drive_letter: str,
                   dry_run: bool = False, verbose: bool = False,
                   deep: bool = False) -> tuple[int, int, list[str], bool]:
    """Clear Halo cache from a single partition.

    Returns:
        Tuple of (deleted_count, failed_count, list_of_deleted_names, hit_locked)
    """
    drive_display = f"{drive_letter}:"
    drive_path = make_drive_path(drive_letter)

    if dry_run:
        print(f"  [DRY-RUN] Would scan {drive_display}")
    else:
        print(f"  Scanning {drive_display}...")

    deleted, failed, deleted_names, total_size, hit_locked = clear_path_recursive(
        client, drive_letter, drive_path, dry_run, verbose, deep
    )

    if deleted > 0:
        size_mb = total_size / (1024 * 1024)
        print(f"    Deleted {deleted} item(s), freed {size_mb:.2f} MB")
        if verbose:
            print("    Deleted items:")
            for name in deleted_names:
                print(f"      - {name}")
    elif verbose:
        print(f"    No Halo cache files in {drive_display}")

    return (deleted, failed, deleted_names, hit_locked)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Clear Halo CE cache from Xbox devkit cache partitions",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python tools/xbox/clear_cache.py              # Clear Halo cache
  python tools/xbox/clear_cache.py --dry-run    # Preview what would be deleted
  python tools/xbox/clear_cache.py --deep       # Include generated z:\\saved files
  python tools/xbox/clear_cache.py -x 192.168.1.42

Clears Halo files from P:, T:, U:, and Z: partitions (devkit cache).
The z:\\saved directory is preserved unless --deep is passed.
"""
    )
    parser.add_argument("-x", "--host", default=DEFAULT_HOST,
                       help=f"Xbox host (default: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                       help=f"XBDM port (default: {DEFAULT_PORT})")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT,
                       help=f"Timeout (default: {DEFAULT_TIMEOUT})")
    parser.add_argument("--dry-run", action="store_true",
                       help="Preview without deleting")
    parser.add_argument("-v", "--verbose", action="store_true",
                       help="Verbose output")
    parser.add_argument("--deep", action="store_true",
                       help="Also remove confirmed generated files under z:\\saved")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    print("Clearing Halo CE cache from Xbox")
    print(f"Target: {args.host}:{args.port}")
    print()

    if args.dry_run:
        print("[DRY-RUN] Preview mode - no files will be deleted\n")
    if args.deep:
        print("Deep mode: confirmed generated z:\\saved files are included\n")

    try:
        client = RdcpClient(args.host, args.port, args.timeout)
        client.connect()
        print("Connected\n")

        total_deleted = 0
        total_failed = 0
        all_deleted_files = []
        any_locked = False

        for drive_letter in CACHE_PARTITIONS:
            deleted, failed, deleted_names, hit_locked = clear_partition(
                client, drive_letter, args.dry_run, args.verbose, args.deep
            )
            total_deleted += deleted
            total_failed += failed
            any_locked = any_locked or hit_locked
            if deleted_names:
                all_deleted_files.extend([(drive_letter, name) for name in deleted_names])

        if any_locked and not args.dry_run:
            if reboot_and_reconnect(client):
                print("  Retrying cache clear after reboot...\n")
                for drive_letter in CACHE_PARTITIONS:
                    deleted, failed, deleted_names, _ = clear_partition(
                        client, drive_letter, args.dry_run, args.verbose, args.deep
                    )
                    total_deleted += deleted
                    total_failed = max(0, total_failed - deleted)
                    if deleted_names:
                        all_deleted_files.extend(
                            [(drive_letter, name) for name in deleted_names]
                        )
            else:
                print(
                    "  warning: could not reboot - locked files were NOT deleted",
                    file=sys.stderr,
                )

        client.close()

        print()
        if total_failed > 0:
            print(f"Warning: Deleted {total_deleted} item(s), {total_failed} failed")
            return 1
        elif total_deleted == 0:
            print("No Halo cache files found (already clean)")
            return 0
        else:
            print(f"Deleted {total_deleted} Halo cache item(s):")
            for drive, name in all_deleted_files:
                print(f"  {name}")
            return 0

    except RdcpError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
