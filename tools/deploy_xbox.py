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

The deployed XBE is automatically launched on the Xbox after deployment.

Requires: XDK installed at "C:\\Program Files (x86)\\RXDK\\xbox\\bin\\xbcp.exe"
          or XBCP_PATH env var pointing to xbcp.exe.
"""

import argparse
import fnmatch
import json
import os
import subprocess
import sys
import time

from build import build as run_build
from build_hash import print_build_hash
from local_env import build_windows_python_command, is_wsl, load_repo_env, to_windows_path


load_repo_env("xbox.env")

ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
HALO_PATCHED_DIR = os.path.join(ROOT_DIR, "halo-patched")
DEFAULT_XBCP = os.path.join(
    "C:", os.sep, "Program Files (x86)", "RXDK", "xbox", "bin", "xbcp.exe"
)
DEFAULT_XBOX_HOST = os.environ.get("XBOX_HOST", "")
DEFAULT_XBOX_DEST = os.environ.get("XBOX_DEST", "xE:\\GAMES\\halo-patched")

EXCLUDE_PATTERNS = [
    "*.id0",
    "*.id1",
    "*.id2",
    "*.nam",
    "*.til",
]


def is_excluded(filename: str) -> bool:
    return any(fnmatch.fnmatch(filename, pat) for pat in EXCLUDE_PATTERNS)


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
            if is_excluded(fname):
                continue
            fpath = os.path.join(root, fname)
            try:
                mtime = os.path.getmtime(fpath)
            except OSError:
                continue
            if mtime > since:
                rel = os.path.relpath(fpath, directory)
                modified.append(rel)
    return sorted(modified)


def get_source_dirs() -> list[str]:
    """Return list of directories containing build sources."""
    source_dirs = [
        os.path.join(ROOT_DIR, "src"),
        os.path.join(ROOT_DIR, "include"),
    ]
    build_cache = os.path.join(ROOT_DIR, "build", ".cmake_cache")
    if os.path.isfile(build_cache):
        source_dirs.append(build_cache)
    return [d for d in source_dirs if os.path.isdir(d)]


def is_build_current(xbe_path: str) -> bool:
    """Return True if xbe_path exists and is newer than all source files."""
    if not os.path.isfile(xbe_path):
        return False
    try:
        xbe_mtime = os.path.getmtime(xbe_path)
    except OSError:
        return False
    for src_dir in get_source_dirs():
        for root, _dirs, files in os.walk(src_dir):
            for fname in files:
                fpath = os.path.join(root, fname)
                try:
                    if os.path.getmtime(fpath) > xbe_mtime:
                        return False
                except OSError:
                    continue
    return True


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
    retries: int = 3,
) -> int:
    xbcp_args = []
    if host:
        xbcp_args += ["-x", host]
    if recursive:
        xbcp_args.append("/r")
    if newer_only:
        xbcp_args.append("/d")
    if overwrite:
        xbcp_args.append("/y")
    if create_dirs:
        xbcp_args.append("/t")
    if quiet:
        xbcp_args.append("/q")
    if force_readonly:
        xbcp_args.append("/f")
    xbcp_args += [src, dest]

    if is_wsl():
        args = ["cmd.exe", "/c", to_windows_path(xbcp_exe)] + xbcp_args
    else:
        args = [xbcp_exe] + xbcp_args

    if dry_run:
        display_args = [xbcp_display] + xbcp_args
        print(f"  [DRY-RUN] {' '.join(display_args)}")
        return 0

    last_error: int | None = None
    for attempt in range(retries):
        if attempt > 0:
            print(f"  retrying (attempt {attempt + 1}/{retries})...")
        try:
            result = subprocess.run(
                args, check=False, cwd=ROOT_DIR,
                capture_output=True, text=True,
            )
            if result.stdout:
                for line in result.stdout.strip().splitlines():
                    print(f"  | {line}")
            if result.returncode == 0:
                return 0
            last_error = result.returncode
            if result.stderr:
                for line in result.stderr.strip().splitlines():
                    print(f"  | {line}", file=sys.stderr)
            if attempt < retries - 1:
                time.sleep(1.0)
                continue
            print(f"  xbcp exited with code {last_error}", file=sys.stderr)
            return last_error
        except FileNotFoundError:
            print(f"error: failed to run {xbcp_display}", file=sys.stderr)
            return 1
        except OSError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1
    return last_error or 1


def launch_xbe(xbox_dest: str, host: str, dry_run: bool) -> int:
    """Launch the deployed XBE on the Xbox via xbdm_rdcp.py magicboot."""
    xbe_xbox_path = xbox_dest.lstrip("x") + "\\default.xbe"
    rdcp_script = os.path.join(ROOT_DIR, "tools", "xbdm_rdcp.py")
    cmd = build_windows_python_command(
        rdcp_script,
        [f"magicboot title={xbe_xbox_path} debug"],
    )
    if cmd is None:
        cmd = [
            sys.executable,
            rdcp_script,
            f"magicboot title={xbe_xbox_path} debug",
        ]
    if host:
        cmd += ["--host", host]
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


def query_remote_file_attributes(host: str, xbox_path: str) -> dict | None:
    rdcp_script = os.path.join(ROOT_DIR, "tools", "xbdm_rdcp.py")
    cmd = build_windows_python_command(
        rdcp_script,
        ["--json", f'getfileattributes name="{xbox_path}"'],
    )
    if cmd is None:
        cmd = [sys.executable, rdcp_script, "--json", f'getfileattributes name="{xbox_path}"']
    if host:
        cmd += ["--host", host]

    result = subprocess.run(
        cmd,
        cwd=ROOT_DIR,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return None

    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError:
        return None
    if not payload.get("ok"):
        return None
    return payload


def extract_remote_size(attributes: dict) -> int | None:
    lines = attributes.get("lines") or []
    if not lines:
        return None

    fields = {}
    for token in lines[0].split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = value

    try:
        size_hi = int(fields["sizehi"], 0)
        size_lo = int(fields["sizelo"], 0)
    except (KeyError, ValueError):
        return None
    return (size_hi << 32) | size_lo


def verify_uploaded_file(host: str, local_path: str, xbox_path: str) -> bool:
    attributes = query_remote_file_attributes(host, xbox_path)
    if attributes is None:
        print(f"  verify failed: could not query {xbox_path}", file=sys.stderr)
        return False

    remote_size = extract_remote_size(attributes)
    if remote_size is None:
        print(f"  verify failed: could not parse size for {xbox_path}", file=sys.stderr)
        return False

    local_size = os.path.getsize(local_path)
    if remote_size != local_size:
        print(
            f"  verify failed: {xbox_path} size mismatch "
            f"(local {local_size:,} vs remote {remote_size:,})",
            file=sys.stderr,
        )
        return False

    print(f"  verified {xbox_path} ({remote_size:,} bytes)")
    return True


def run_xemu_qmp_command(qmp_script: str, command: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, qmp_script, command],
        cwd=ROOT_DIR,
        capture_output=True,
        text=True,
    )


def prepare_xemu_for_deploy(qmp_script: str) -> None:
    print("preparing xemu...")

    eject_rc = run_xemu_qmp_command(qmp_script, "eject")
    if eject_rc.returncode != 0:
        print("  (no xemu instance found, skipping xemu prep)")
        return

    if eject_rc.stdout:
        for line in eject_rc.stdout.strip().splitlines():
            print(f"  | {line}")

    reset_rc = run_xemu_qmp_command(qmp_script, "reset")
    if reset_rc.returncode != 0:
        print("  (xemu reset failed after eject)")
        if reset_rc.stdout:
            for line in reset_rc.stdout.strip().splitlines():
                print(f"  | {line}")
        if reset_rc.stderr:
            for line in reset_rc.stderr.strip().splitlines():
                print(f"  | {line}", file=sys.stderr)
        return

    if reset_rc.stdout:
        for line in reset_rc.stdout.strip().splitlines():
            print(f"  | {line}")

    print("  | waiting 2.0s for xemu to settle after reset")
    time.sleep(2.0)


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
        "--dry-run",
        action="store_true",
        help="Show commands without executing",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip build step and only deploy existing halo-patched files",
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

    xbe_path = os.path.join(HALO_PATCHED_DIR, "default.xbe")
    if not args.skip_build and os.path.isdir(os.path.join(ROOT_DIR, "build")):
        if is_build_current(xbe_path):
            print("build unchanged, skipping rebuild...")
        else:
            print("building patched XBE...")
            rc = run_build(target="patched_xbe")
            if rc != 0:
                print("error: build failed", file=sys.stderr)
                return rc

    if not os.path.isfile(xbe_path):
        print("error: default.xbe not found in halo-patched/", file=sys.stderr)
        return 1

    # Reset xemu after eject so the guest stops running the old title.
    qmp_script = os.path.join(ROOT_DIR, "tools", "xemu_qmp.py")
    if os.path.isfile(qmp_script):
        prepare_xemu_for_deploy(qmp_script)

    print_build_hash(xbe_path)

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
        print(f"  default.xbe ({os.path.getsize(xbe_path):,} bytes)")
        src = to_windows_path(xbe_path)
        d = f"{dest}\\default.xbe"
        rc = run_xbcp(src=src, dest=d, **common_kwargs)
        if rc != 0:
            print(f"  xbcp failed with exit code {rc}", file=sys.stderr)
            return rc
        if not args.dry_run and not verify_uploaded_file(host, xbe_path, d.lstrip("x")):
            return 1
        print("done.")
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
    if not args.dry_run and not verify_uploaded_file(host, xbe_path, xbe_dest.lstrip("x")):
        return 1

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
    rc = launch_xbe(args.dest, host, args.dry_run)
    if rc != 0:
        return rc
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
