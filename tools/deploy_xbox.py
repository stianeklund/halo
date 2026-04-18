#!/usr/bin/env python3
"""Deploy patched Halo build to a real Xbox via xbcp.exe (XDK file transfer).

Only uploads files that are newer than what's on the Xbox, using xbcp's -d flag.
Falls back to unconditional copy if the Xbox destination doesn't exist yet.

Usage:
    python tools/deploy_xbox.py [options]

Examples:
    python tools/deploy_xbox.py                        # deploy to default host
    python tools/deploy_xbox.py -x 192.168.1.42        # specify Xbox IP
    python tools/deploy_xbox.py -x 192.168.1.42 --full # full halo-patched dir
    python tools/deploy_xbox.py --dry-run              # show what would be copied

Requires: XDK installed at "C:\\Program Files (x86)\\RXDK\\xbox\\bin\\xbcp.exe"
          or XBCP_PATH env var pointing to xbcp.exe.
"""

import argparse
import os
import subprocess
import sys
import time

ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
HALO_PATCHED_DIR = os.path.join(ROOT_DIR, "halo-patched")
DEFAULT_XBCP = os.path.join(
    "C:", os.sep, "Program Files (x86)", "RXDK", "xbox", "bin", "xbcp.exe"
)
DEFAULT_XBOX_HOST = os.environ.get("XBOX_HOST", "")
DEFAULT_XBOX_DEST = os.environ.get("XBOX_DEST", "xE:\\GAMES\\halo-patched")


def to_windows_path(path: str) -> str:
    path = os.path.abspath(path)
    if sys.platform != "win32":
        path = path.replace("\\", "/")
        if (
            len(path) >= 7
            and path.startswith("/mnt/")
            and path[5].isalpha()
            and path[6] == "/"
        ):
            return f"{path[5].upper()}:{path[6:]}"
    return path


def find_xbcp() -> tuple[str, str]:
    """Return (executable_path, display_name).

    executable_path: Linux-format path for subprocess.run on WSL.
    display_name: human-readable Windows-style path for messages.
    """
    env_path = os.environ.get("XBCP_PATH", "")
    if env_path and os.path.isfile(env_path):
        return env_path, env_path
    if sys.platform == "win32" and os.path.isfile(DEFAULT_XBCP):
        return DEFAULT_XBCP, DEFAULT_XBCP
    wsl_path = "/mnt/c/Program Files (x86)/RXDK/xbox/bin/xbcp.exe"
    if os.path.isfile(wsl_path):
        return wsl_path, to_windows_path(wsl_path)
    print("error: cannot find xbcp.exe", file=sys.stderr)
    print(
        "  install the Xbox SDK or set XBCP_PATH to the full path of xbcp.exe",
        file=sys.stderr,
    )
    return "", ""


def get_modified_files(directory: str, since: float) -> list[str]:
    """Return files modified after `since` timestamp, relative to directory."""
    modified = []
    for root, _dirs, files in os.walk(directory):
        for fname in files:
            fpath = os.path.join(root, fname)
            try:
                mtime = os.path.getmtime(fpath)
            except OSError:
                continue
            if mtime > since:
                rel = os.path.relpath(fpath, directory)
                modified.append(rel)
    return sorted(modified)


def run_xbcp(
    xbcp_exe: str,
    xbcp_display: str,
    host: str,
    src: str,
    dest: str,
    recursive: bool = False,
    newer_only: bool = True,
    overwrite: bool = True,
    create_dirs: bool = True,
    quiet: bool = False,
    force_readonly: bool = True,
    dry_run: bool = False,
) -> int:
    args = [xbcp_exe]
    if host:
        args += ["-x", host]
    if recursive:
        args.append("/r")
    if newer_only:
        args.append("/d")
    if overwrite:
        args.append("/y")
    if create_dirs:
        args.append("/t")
    if quiet:
        args.append("/q")
    if force_readonly:
        args.append("/f")
    args += [src, dest]

    if dry_run:
        display_args = [xbcp_display] + args[1:]
        print(f"  [DRY-RUN] {' '.join(display_args)}")
        return 0

    try:
        result = subprocess.run(
            args, check=False, cwd=ROOT_DIR,
            capture_output=True, text=True,
        )
        if result.stdout:
            for line in result.stdout.strip().splitlines():
                print(f"  | {line}")
        if result.returncode != 0:
            if result.stderr:
                for line in result.stderr.strip().splitlines():
                    print(f"  | {line}", file=sys.stderr)
            print(f"  xbcp exited with code {result.returncode}", file=sys.stderr)
        return result.returncode
    except FileNotFoundError:
        print(f"error: failed to run {xbcp_display}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


def launch_xbe(xbox_dest: str, host: str, dry_run: bool) -> int:
    """Launch the deployed XBE on the Xbox via xbdm_rdcp.py magicboot."""
    xbe_xbox_path = xbox_dest.lstrip("x") + "\\default.xbe"
    rdcp_script = os.path.join(ROOT_DIR, "tools", "xbdm_rdcp.py")
    cmd = [
        sys.executable,
        rdcp_script,
        f"magicboot title={xbe_xbox_path} debug",
    ]
    if host:
        cmd += ["-x", host]
    if dry_run:
        print(f"  [DRY-RUN] {' '.join(cmd)}")
        return 0
    print(f"  launching {xbe_xbox_path}...")
    try:
        result = subprocess.run(cmd, cwd=ROOT_DIR, capture_output=True, text=True)
        if result.stdout:
            for line in result.stdout.strip().splitlines():
                print(f"  | {line}")
        if result.returncode != 0:
            if result.stderr:
                for line in result.stderr.strip().splitlines():
                    print(f"  | {line}", file=sys.stderr)
            print(f"  xbdm_rdcp exited with code {result.returncode}", file=sys.stderr)
            return result.returncode
        return 0
    except FileNotFoundError:
        print(f"error: failed to run {rdcp_script}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Deploy patched Halo build to Xbox via xbcp (XDK)"
    )
    parser.add_argument(
        "-x", "--xbox",
        default=DEFAULT_XBOX_HOST,
        help="Xbox name or IP address (or set XBOX_HOST env var)",
    )
    parser.add_argument(
        "--dest",
        default=DEFAULT_XBOX_DEST,
        help=f"Xbox destination path (default: {DEFAULT_XBOX_DEST})",
    )
    parser.add_argument(
        "--full",
        action="store_true",
        help="Deploy full halo-patched directory (XBE + maps + bink)",
    )
    parser.add_argument(
        "--xbe-only",
        action="store_true",
        help="Only deploy default.xbe (fastest)",
    )
    parser.add_argument(
        "--launch",
        action="store_true",
        help="Launch the XBE on the Xbox after deploying (via xbdm_rdcp magicboot)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show commands without executing",
    )
    parser.add_argument(
        "--since",
        type=float,
        default=0,
        help="Only deploy files modified in the last N seconds (default: auto-detect from build)",
    )
    args = parser.parse_args()

    xbcp_exe, xbcp_display = find_xbcp()
    if not xbcp_exe:
        return 1

    if not os.path.isdir(HALO_PATCHED_DIR):
        print(
            f"error: {HALO_PATCHED_DIR} does not exist. Run a build first.",
            file=sys.stderr,
        )
        return 1

    # Always rebuild the patched XBE before deploying to avoid shipping a
    # stale default.xbe.  The cmake step is a fast no-op when nothing changed.
    build_dir = os.path.join(ROOT_DIR, "build")
    if os.path.isdir(build_dir):
        print("building patched XBE...")
        rc = subprocess.call(
            ["cmake", "--build", build_dir, "--target", "patched_xbe"],
            cwd=ROOT_DIR,
        )
        if rc != 0:
            print("error: build failed", file=sys.stderr)
            return rc

    xbe_path = os.path.join(HALO_PATCHED_DIR, "default.xbe")
    if not os.path.isfile(xbe_path):
        print("error: default.xbe not found in halo-patched/", file=sys.stderr)
        return 1

    dest = args.dest
    host = args.xbox
    common_kwargs = dict(
        xbcp_exe=xbcp_exe,
        xbcp_display=xbcp_display,
        host=host,
        newer_only=True,
        overwrite=True,
        create_dirs=True,
        force_readonly=True,
        dry_run=args.dry_run,
    )

    xbe_src = to_windows_path(xbe_path)
    xbe_dest = f"{dest}\\default.xbe"

    print(f"deploying to {dest}" + (f" on {host}" if host else ""))

    if args.xbe_only:
        all_xbes = [f for f in os.listdir(HALO_PATCHED_DIR) if f.endswith('.xbe')]
        for xbe_name in sorted(all_xbes):
            xbe_file = os.path.join(HALO_PATCHED_DIR, xbe_name)
            print(f"  {xbe_name} ({os.path.getsize(xbe_file):,} bytes)")
            src = to_windows_path(xbe_file)
            d = f"{dest}\\{xbe_name}"
            rc = run_xbcp(src=src, dest=d, **common_kwargs)
            if rc != 0:
                print(f"  xbcp failed with exit code {rc}", file=sys.stderr)
                return rc
        print("done.")
        if args.launch:
            rc = launch_xbe(args.dest, host, args.dry_run)
            if rc != 0:
                return rc
        return 0

    since = args.since
    if since > 0:
        cutoff = time.time() - since
        modified = get_modified_files(HALO_PATCHED_DIR, cutoff)
        if not modified:
            print("no files modified in the specified window.")
            return 0
        print(f"  {len(modified)} file(s) modified in last {since:.0f}s")
        any_failed = False
        for rel in modified:
            src = to_windows_path(os.path.join(HALO_PATCHED_DIR, rel))
            d = f"{dest}\\{rel.replace(os.sep, '\\')}"
            print(f"  {rel}")
            rc = run_xbcp(src=src, dest=d, **common_kwargs)
            if rc != 0:
                print(f"    FAILED (exit {rc})", file=sys.stderr)
                any_failed = True
        if any_failed:
            return 1
        print("done.")
        if args.launch:
            rc = launch_xbe(args.dest, host, args.dry_run)
            if rc != 0:
                return rc
        return 0

    # Default: deploy XBE + anything that looks like it changed (maps, etc.)
    # First always push the XBE
    print(f"  default.xbe ({os.path.getsize(xbe_path):,} bytes)")
    rc = run_xbcp(src=xbe_src, dest=xbe_dest, **common_kwargs)
    if rc != 0:
        print(f"  xbcp failed with exit code {rc}", file=sys.stderr)
        return rc

    # Then push maps/ and bink/ if --full, or just maps/ by default
    dirs_to_deploy = []
    maps_dir = os.path.join(HALO_PATCHED_DIR, "maps")
    if os.path.isdir(maps_dir):
        dirs_to_deploy.append(("maps", maps_dir))

    if args.full:
        bink_dir = os.path.join(HALO_PATCHED_DIR, "bink")
        if os.path.isdir(bink_dir):
            dirs_to_deploy.append(("bink", bink_dir))

    for label, local_dir in dirs_to_deploy:
        print(f"  {label}/ (newer files only)")
        src = to_windows_path(local_dir)
        d = f"{dest}\\{label}"
        rc = run_xbcp(
            src=src + "\\*", 
            dest=d,
            recursive=True,
            **common_kwargs,
        )
        if rc != 0:
            print(f"    xbcp failed with exit code {rc}", file=sys.stderr)
            return rc

    print("done.")
    if args.launch:
        rc = launch_xbe(args.dest, host, args.dry_run)
        if rc != 0:
            return rc
    return 0

    # Default: deploy XBE + anything that looks like it changed (maps, etc.)
    # First always push the XBE
    print(f"  default.xbe ({os.path.getsize(xbe_path):,} bytes)")
    rc = run_xbcp(src=xbe_src, dest=xbe_dest, **common_kwargs)
    if rc != 0:
        print(f"  xbcp failed with exit code {rc}", file=sys.stderr)
        return rc

    # Then push maps/ and bink/ if --full, or just maps/ by default
    dirs_to_deploy = []
    maps_dir = os.path.join(HALO_PATCHED_DIR, "maps")
    if os.path.isdir(maps_dir):
        dirs_to_deploy.append(("maps", maps_dir))

    if args.full:
        bink_dir = os.path.join(HALO_PATCHED_DIR, "bink")
        if os.path.isdir(bink_dir):
            dirs_to_deploy.append(("bink", bink_dir))

    for label, local_dir in dirs_to_deploy:
        print(f"  {label}/ (newer files only)")
        src = to_windows_path(local_dir)
        d = f"{dest}\\{label}"
        rc = run_xbcp(
            src=src + "\\*", 
            dest=d,
            recursive=True,
            **common_kwargs,
        )
        if rc != 0:
            print(f"    xbcp failed with exit code {rc}", file=sys.stderr)
            return rc

    print("done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
