#!/usr/bin/env python3
"""Clear Halo CE cache files from Xbox cache partitions via XBDM/RDCP.

This tool surgically removes only Halo CE related cache files from devkit cache
partitions (P:, T:, U:, and Z:).

For devkits:
  P: - All titles cache (cache000.map, etc.)
  T: - Persistent Data - Active Title (temporary data)
  U: - Saved Games - Active Title (save data)
  Z: - Title state files (last_solo.txt); z:\\saved is preserved
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

CACHE_PARTITIONS = ["P", "T", "U", "Z"]

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

# Directories to skip entirely (save data, not cache)
SKIP_DIRECTORIES = {"saved"}


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

    # Check for Xbox save metadata files
    if filename_lower.endswith(".xbx"):
        return True

    # Include all .txt files (last_solo.txt, lastprof.txt, etc.)
    if filename_lower.endswith(".txt"):
        return True

    # Include specific metadata files
    if filename_lower == "savegame.bin":
        return True

    return False


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


def delete_item(client: RdcpClient, full_path: str, is_dir: bool = False,
                dry_run: bool = False) -> bool:
    """Delete a file or directory from the Xbox."""
    if dry_run:
        item_type = "directory" if is_dir else "file"
        print(f"      [DRY-RUN] Would delete {item_type}: {full_path}")
        return True
    
    if is_dir:
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


def clear_path_recursive(client: RdcpClient, drive_letter: str,
                        current_path: str,
                        dry_run: bool = False, verbose: bool = False) -> tuple[int, int, list[str], int]:
    """Recursively clear Halo cache from a path.
    
    Returns:
        Tuple of (deleted_count, failed_count, list_of_deleted_names, total_size)
    """
    files, error_code, error_msg = list_files_in_path(client, current_path)
    
    if error_code == 402:  # File not found = empty or missing
        return (0, 0, [], 0)
    elif error_code != 0:
        if verbose:
            print(f"    Error listing {current_path}: {error_code}- {error_msg}")
        return (0, 1, [], 0)
    
    if not files:
        return (0, 0, [], 0)
    
    deleted = 0
    failed = 0
    total_size = 0
    deleted_names = []
    
    for item in files:
        item_name = item["name"]
        item_lower = item_name.lower()
        full_path = f"{current_path}{item_name}"
        
        if item.get("is_dir"):
            # Skip protected directories (save data)
            if item_lower in SKIP_DIRECTORIES:
                if verbose:
                    print(f"    Skipping protected directory: {full_path}")
                continue
            
            # Recurse into subdirectory
            sub_path = f"{full_path}\\"
            sub_deleted, sub_failed, sub_names, sub_size = clear_path_recursive(
                client, drive_letter, sub_path, dry_run, verbose
            )
            deleted += sub_deleted
            failed += sub_failed
            deleted_names.extend(sub_names)
            total_size += sub_size
            
            # After clearing contents, try to remove the directory itself
            # if it is now empty and appears to be Halo-specific
            # (only do this if the directory name matches a Halo pattern)
            if is_halo_cache_file(item_name + ".map") or item_lower in HALO_FILE_PATTERNS:
                if delete_item(client, full_path, is_dir=True, dry_run=dry_run):
                    deleted += 1
                    deleted_names.append(item_name)
                else:
                    failed += 1
        else:
            if is_halo_cache_file(item_name):
                if dry_run:
                    size_mb = item.get("size", 0) / (1024 * 1024)
                    print(f"      [DRY-RUN] file: {full_path} ({size_mb:.2f} MB)")
                    deleted += 1
                    total_size += item.get("size", 0)
                    deleted_names.append(item_name)
                else:
                    if delete_item(client, full_path, is_dir=False, dry_run=dry_run):
                        deleted += 1
                        total_size += item.get("size", 0)
                        deleted_names.append(item_name)
                        if verbose:
                            print(f"      Deleted: {full_path}")
                    else:
                        failed += 1
    
    return (deleted, failed, deleted_names, total_size)


def clear_partition(client: RdcpClient, drive_letter: str,
                   dry_run: bool = False, verbose: bool = False) -> tuple[int, int, list[str]]:
    """Clear Halo cache from a single partition.
    
    Returns:
        Tuple of (deleted_count, failed_count, list_of_deleted_names)
    """
    drive_display = f"{drive_letter}:"
    drive_path = make_drive_path(drive_letter)
    
    if dry_run:
        print(f"  [DRY-RUN] Would scan {drive_display}")
    else:
        print(f"  Scanning {drive_display}...")
    
    deleted, failed, deleted_names, total_size = clear_path_recursive(
        client, drive_letter, drive_path, dry_run, verbose
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

Clears Halo files from P:, T:, U:, and Z: partitions (devkit cache).
The z:\\saved directory is always preserved (contains save data).
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
