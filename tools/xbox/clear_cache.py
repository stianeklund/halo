#!/usr/bin/env python3
"""Clear Halo CE cache files from Xbox cache partitions via XBDM/RDCP.

This tool surgically removes only Halo CE related cache files from devkit cache
partitions (T: and U: for active title cache).

For devkits:
  T: - Persistent Data - Active Title (temporary data)
  U: - Saved Games - Active Title (save data)
  Z: - Not currently mounted (appears when game running)
"""

from __future__ import annotations

import argparse
import os
import sys

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

CACHE_PARTITIONS = ["P", "T", "U"]  # P: = All titles cache (has cache000.map, etc.)

# File extensions that Halo CE creates in cache
HALO_CACHE_EXTENSIONS = [
    ".map",      # Map files
    ".xbx",      # Xbox save metadata
]

# Known Halo file patterns
# Note: "halo" is too generic and matches non-Halo files like "TitleMeta.xbx"
HALO_FILE_PATTERNS = [
    "bloodgulch", "chillout", "damnation", "gephyrophobia",
    "hangemhigh", "ratrace", "prisoner", "wizard", "ui",
    "beavercreek", "boardingaction", "carousel", "dangercanyon",
    "deathisland", "derelict", "icefields", "infinity",
    "sidewinder", "timberland", "putput", "longest",
    "a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40",
]


def is_halo_cache_file(filename: str) -> bool:
    """Check if a filename matches Halo CE cache file patterns."""
    filename_lower = filename.lower()

    # Check for Halo map files (.map extension with Halo map name)
    if filename_lower.endswith(".map"):
        name_without_ext = filename_lower[:-4]  # Remove .map

        # Check for cache*.map files (cache000.map, cache001.map, etc.)
        # These are Halo's cached maps on P: drive
        if name_without_ext.startswith("cache"):
            return True

        # Check if it matches known Halo map names
        if any(name_without_ext.startswith(pattern.lower())
               for pattern in HALO_FILE_PATTERNS):
            return True

        # Unknown .map files in cache are likely Halo maps too
        # (cache partitions only contain cached game data)
        return True

    # Check for Halo save/cache metadata files in P:\ root
    # Include all .txt files (last_solo.txt, lastprof.txt, etc.)
    if filename_lower.endswith(".txt"):
        return True

    # Include specific metadata files
    if filename_lower == "savegame.bin":
        return True

    return False


def list_files_in_partition(client: RdcpClient, drive_letter: str) -> tuple[list[dict], int, str]:
    """List all files in a partition.
    
    Returns:
        Tuple of (files_list, error_code, error_message)
    """
    # IMPORTANT: Must use trailing backslash for XBDM
    drive_path = make_drive_path(drive_letter)
    command = f'dirlist name="{drive_path}"'
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


def delete_item(client: RdcpClient, drive_letter: str, item: dict, 
                dry_run: bool = False) -> bool:
    """Delete a file or directory from a partition."""
    drive_path = make_drive_path(drive_letter)
    full_path = f"{drive_path}{item['name']}"
    
    if dry_run:
        item_type = "directory" if item.get("is_dir") else "file"
        print(f"      [DRY-RUN] Would delete {item_type}: {full_path}")
        return True
    
    if item.get("is_dir"):
        command = f'delete name="{full_path}" dir'
    else:
        command = f'delete name="{full_path}"'
    
    response = client.command(command)
    
    if response.is_success:
        return True
    elif response.code == 402:  # Not found
        return True
    else:
        print(f"        Error: {response.code}- {response.message}",
              file=sys.stderr)
        return False


def clear_partition(client: RdcpClient, drive_letter: str,
                   dry_run: bool = False, verbose: bool = False) -> tuple[int, int, list[str]]:
    """Clear Halo cache from a single partition.
    
    Returns:
        Tuple of (deleted_count, failed_count, list_of_deleted_names)
    """
    drive_display = f"{drive_letter}:"
    
    if dry_run:
        print(f"  [DRY-RUN] Would scan {drive_display}")
    else:
        print(f"  Scanning {drive_display}...")
    
    files, error_code, error_msg = list_files_in_partition(client, drive_letter)
    
    if error_code == 402:  # File not found = empty partition
        if verbose:
            print(f"    {drive_display} is empty")
        return (0, 0, [])
    elif error_code != 0:
        print(f"    Error: {error_code}- {error_msg}")
        return (0, 1, [])
    
    if not files:
        if verbose:
            print(f"    {drive_display} is empty")
        return (0, 0, [])
    
    # Identify Halo cache files
    halo_items = [f for f in files if is_halo_cache_file(f["name"])]
    other_items = [f for f in files if not is_halo_cache_file(f["name"])]
    
    if not halo_items:
        if verbose:
            print(f"    No Halo cache files in {drive_display}")
        return (0, 0, [])
    
    print(f"    Found {len(halo_items)} Halo item(s) "
          f"({len(other_items)} other items preserved)")
    
    if dry_run:
        for item in halo_items:
            size_mb = item.get("size", 0) / (1024 * 1024)
            item_type = "dir" if item.get("is_dir") else "file"
            print(f"      [DRY-RUN] {item_type}: {item['name']} ({size_mb:.2f} MB)")
        return (len(halo_items), 0, [item['name'] for item in halo_items])
    
    deleted = 0
    failed = 0
    total_size = 0
    deleted_names = []
    
    for item in halo_items:
        if delete_item(client, drive_letter, item, dry_run):
            deleted += 1
            total_size += item.get("size", 0)
            deleted_names.append(item['name'])
            if verbose:
                print(f"      Deleted: {item['name']}")
        else:
            failed += 1
    
    if deleted > 0:
        size_mb = total_size / (1024 * 1024)
        print(f"    Deleted {deleted} item(s), freed {size_mb:.2f} MB")
        print("    Deleted items:")
        for name in deleted_names:
            print(f"      - {name}")
    
    return (deleted, failed, deleted_names)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Clear Halo CE cache from Xbox devkit cache partitions",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python tools/xbox/clear_cache.py              # Clear Halo cache
  python tools/xbox/clear_cache.py --dry-run    # Preview what would be deleted
  python tools/xbox/clear_cache.py -x 192.168.1.42

Clears Halo files from T: and U: partitions (active title cache on devkits).
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
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    print("Clearing Halo CE cache from Xbox")
    print(f"Target: {args.host}:{args.port}")
    print()

    if args.dry_run:
        print("[DRY-RUN] Preview mode - no files will be deleted\n")

    try:
        with RdcpClient(args.host, args.port, args.timeout) as client:
            print("Connected\n")

            total_deleted = 0
            total_failed = 0
            all_deleted_files = []

            for drive_letter in CACHE_PARTITIONS:
                deleted, failed, deleted_names = clear_partition(
                    client, drive_letter, args.dry_run, args.verbose
                )
                total_deleted += deleted
                total_failed += failed
                if deleted_names:
                    all_deleted_files.extend([(drive_letter, name) for name in deleted_names])

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
                    print(f"  {drive}:\\{name}")
                return 0

    except RdcpError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
