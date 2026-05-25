#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Deploy patched Halo build to a real Xbox via xbcp.exe (XDK file transfer).

Only uploads files that are newer than what's on the Xbox, using xbcp's -d flag.
Falls back to unconditional copy if the Xbox destination doesn't exist yet.

Usage:
    python tools/xbox/deploy_xbox.py [options]

Examples:
    python tools/xbox/deploy_xbox.py                        # deploy to default host
    python tools/xbox/deploy_xbox.py -x 192.168.1.42        # specify Xbox IP
    python tools/xbox/deploy_xbox.py -x 192.168.1.42 --full # full halo-patched dir
    python tools/xbox/deploy_xbox.py --dry-run              # show what would be copied

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

from build.build import build as run_build
from build.build_hash import print_build_hash
from internal.local_env import build_windows_python_command, is_wsl, load_repo_env, to_windows_path


load_repo_env("xbox.env")

ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "../..")
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
                if attempt > 0:
                    print(f"  xbcp succeeded on attempt {attempt + 1}/{retries}")
                return 0
            last_error = result.returncode
            if result.stderr:
                for line in result.stderr.strip().splitlines():
                    print(f"  | {line}", file=sys.stderr)
            if attempt < retries - 1:
                print(
                    f"  xbcp attempt {attempt + 1}/{retries} failed; "
                    "waiting 1.0s before retry"
                )
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
    rdcp_script = os.path.join(ROOT_DIR, "tools", "xbox", "xbdm_rdcp.py")
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


def run_rdcp_command(host: str, command: str) -> subprocess.CompletedProcess[str]:
    rdcp_script = os.path.join(ROOT_DIR, "tools", "xbox", "xbdm_rdcp.py")
    cmd = build_windows_python_command(rdcp_script, [command])
    if cmd is None:
        cmd = [sys.executable, rdcp_script, command]
    if host:
        cmd += ["--host", host]
    return subprocess.run(
        cmd,
        cwd=ROOT_DIR,
        capture_output=True,
        text=True,
        check=False,
    )


def wait_for_xbdm(host: str, timeout_seconds: float = 15.0) -> bool:
    deadline = time.time() + timeout_seconds
    last_error = ""
    while time.time() < deadline:
        result = run_rdcp_command(host, "isstopped")
        if result.returncode == 0:
            print("  | XBDM reachable again")
            return True
        last_error = (result.stderr or result.stdout or "").strip()
        time.sleep(0.5)

    print(
        f"  warning: XBDM did not come back within {timeout_seconds:.1f}s",
        file=sys.stderr,
    )
    if last_error:
        for line in last_error.splitlines():
            print(f"  | {line}", file=sys.stderr)
    return False


def prepare_xbdm_for_xbe_replace(host: str, dry_run: bool) -> bool:
    target = host or "localhost"
    if dry_run:
        print(f"  [DRY-RUN] reboot XBDM target {target} before XBE retry")
        return True

    print(f"  rebooting XBDM target {target} to release default.xbe...")
    result = run_rdcp_command(host, "reboot")
    if result.stdout:
        for line in result.stdout.strip().splitlines():
            print(f"  | {line}")
    if result.stderr:
        for line in result.stderr.strip().splitlines():
            print(f"  | {line}", file=sys.stderr)

    print("  | waiting for XBDM to return after reboot")
    return wait_for_xbdm(host)


def upload_via_xbdm(local_path: str, xbox_path: str, host: str) -> int:
    """Upload a file via XBDM sendfile. Returns 0 on success."""
    rdcp_script = os.path.join(ROOT_DIR, "tools", "xbox", "xbdm_rdcp.py")
    src = to_windows_path(local_path) if is_wsl() else local_path
    cmd = build_windows_python_command(rdcp_script, ["--sendfile", src, xbox_path, "--timeout", "30"])
    if cmd is None:
        cmd = [sys.executable, rdcp_script, "--sendfile", local_path, xbox_path, "--timeout", "30"]
    if host:
        cmd += ["--host", host]
    try:
        result = subprocess.run(
            cmd, cwd=ROOT_DIR, capture_output=True, text=True, check=False,
        )
        if result.stdout:
            for line in result.stdout.strip().splitlines():
                print(f"  | {line}")
        if result.returncode != 0 and result.stderr:
            for line in result.stderr.strip().splitlines():
                print(f"  | {line}", file=sys.stderr)
        return result.returncode
    except (FileNotFoundError, OSError):
        return 1


def deploy_default_xbe(
    xbe_path: str,
    xbe_src: str,
    xbe_dest: str,
    host: str,
    dry_run: bool,
    common_kwargs: dict,
) -> int:
    xbox_dest_path = xbe_dest.lstrip("x")
    if not dry_run:
        rc = upload_via_xbdm(xbe_path, xbox_dest_path, host)
        if rc == 0:
            print("  upload completed via XBDM; verifying remote file size...")
            if verify_uploaded_file(host, xbe_path, xbox_dest_path):
                return 0
            print("  XBDM verify failed, falling back to xbcp...",
                  file=sys.stderr)
        else:
            if prepare_xbdm_for_xbe_replace(host, dry_run):
                rc = upload_via_xbdm(xbe_path, xbox_dest_path, host)
                if rc == 0:
                    print("  upload completed via XBDM after reboot; verifying...")
                    if verify_uploaded_file(host, xbe_path, xbox_dest_path):
                        return 0
            print("  XBDM sendfile failed, falling back to xbcp...",
                  file=sys.stderr)
    rc = run_xbcp(src=xbe_src, dest=xbe_dest, **common_kwargs)
    if rc != 0 and prepare_xbdm_for_xbe_replace(host, dry_run):
        print("  retrying default.xbe upload after XBDM reboot...")
        rc = run_xbcp(src=xbe_src, dest=xbe_dest, **common_kwargs)
    if rc != 0:
        print(f"  xbcp failed with exit code {rc}", file=sys.stderr)
        return rc
    if not dry_run:
        print("  upload completed; verifying remote file size...")
    if not dry_run and not verify_uploaded_file(host, xbe_path, xbe_dest.lstrip("x")):
        return 1
    return 0


def query_remote_file_attributes(host: str, xbox_path: str) -> dict | None:
    rdcp_script = os.path.join(ROOT_DIR, "tools", "xbox", "xbdm_rdcp.py")
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
    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError:
        return None
    return payload


def remote_file_missing(attributes: dict) -> bool:
    if attributes.get("ok"):
        return False

    if int(attributes.get("code", 0) or 0) == 402:
        return True

    message = str(attributes.get("message", "")).lower()
    return any(
        phrase in message
        for phrase in (
            "not found",
            "does not exist",
            "no such file",
            "cannot find",
            "missing",
        )
    )


def delete_remote_file(host: str, xbox_path: str, dry_run: bool) -> int:
    """Delete a file on the Xbox via XBDM.

    Returns the XBDM response code:
      0   — success (file confirmed gone)
      402 — file was already absent (treated as success by callers)
      414 — access denied (title still holds the file open)
      -1  — local error (subprocess / JSON parse failure)
      other non-zero — XBDM reported an unexpected error
    """
    rdcp_script = os.path.join(ROOT_DIR, "tools", "xbox", "xbdm_rdcp.py")
    cmd = build_windows_python_command(
        rdcp_script,
        ["--json", f'delete name="{xbox_path}"'],
    )
    if cmd is None:
        cmd = [
            sys.executable,
            rdcp_script,
            "--json",
            f'delete name="{xbox_path}"',
        ]
    if host:
        cmd += ["--host", host]

    if dry_run:
        print(f"  [DRY-RUN] {' '.join(cmd)}")
        return 0

    print(f"  deleting {xbox_path}...")
    try:
        result = subprocess.run(
            cmd,
            cwd=ROOT_DIR,
            capture_output=True,
            text=True,
            check=False,
        )
    except FileNotFoundError:
        print(f"error: failed to run {rdcp_script}", file=sys.stderr)
        return -1
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return -1

    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError:
        if result.stderr:
            for line in result.stderr.strip().splitlines():
                print(f"  | {line}", file=sys.stderr)
        print(
            f"  verify failed: could not parse delete response for {xbox_path}",
            file=sys.stderr,
        )
        return -1

    code = int(payload.get("code", 0) or 0)
    message = str(payload.get("message", "")).strip()
    if code == 402:
        filename = xbox_path.split("\\")[-1]
        print(f"  {filename} already absent")
        return 402

    if not payload.get("ok"):
        print(f"  delete failed: {code}- {message}", file=sys.stderr)
        return code

    attributes = query_remote_file_attributes(host, xbox_path)
    if attributes is None:
        print(
            f"  verify failed: could not confirm deletion of {xbox_path}",
            file=sys.stderr,
        )
        return -1
    if remote_file_missing(attributes):
        print(f"  deleted {xbox_path}")
        return 0

    if attributes.get("ok"):
        print(f"  verify failed: {xbox_path} still exists", file=sys.stderr)
        return -1

    print(
        f"  verify failed: could not confirm deletion of {xbox_path}",
        file=sys.stderr,
    )
    return -1


def _send_xbdm_reboot(host: str, dry_run: bool) -> bool:
    """Send a warm reboot via XBDM to stop the running title and free file locks."""
    rdcp_script = os.path.join(ROOT_DIR, "tools", "xbox", "xbdm_rdcp.py")
    cmd = build_windows_python_command(rdcp_script, ["--json", "reboot warm"])
    if cmd is None:
        cmd = [sys.executable, rdcp_script, "--json", "reboot warm"]
    if host:
        cmd += ["--host", host]
    if dry_run:
        print(f"  [DRY-RUN] {' '.join(cmd)}")
        return True
    print("  rebooting via XBDM to release file locks...")
    try:
        result = subprocess.run(cmd, cwd=ROOT_DIR, capture_output=True, text=True, check=False)
        if result.stdout:
            for line in result.stdout.strip().splitlines():
                print(f"  | {line}")
        return result.returncode == 0
    except OSError:
        return False


def unlock_console(host: str, qmp_script: str, dry_run: bool) -> bool:
    """Eject+reset via QMP (xemu) or reboot via XBDM to free locked files."""
    print("  unlocking console (access denied on delete)...")
    if should_prepare_xemu(host) and os.path.isfile(qmp_script):
        eject_rc = run_xemu_qmp_command(qmp_script, "eject")
        if eject_rc.returncode == 0:
            reset_rc = run_xemu_qmp_command(qmp_script, "reset")
            if reset_rc.returncode == 0:
                print("  | waiting 2.0s for xemu to settle after reset")
                time.sleep(2.0)
                return True
    if _send_xbdm_reboot(host, dry_run):
        print("  | waiting for XBDM to return after reboot")
        return wait_for_xbdm(host)
    return False


def delete_state_files(
    host: str,
    paths: list,
    dry_run: bool,
    qmp_script: str,
) -> bool:
    """Delete state files, unlocking the console via QMP/XBDM if access is denied.

    On a 414 (access denied) response the console is ejected/rebooted and the
    failed paths are retried once so a still-running title cannot block the deploy.
    """
    failed_paths = []
    for path in paths:
        code = delete_remote_file(host, path, dry_run)
        if code == 414:
            failed_paths.append(path)
        elif code not in (0, 402):
            return False

    if failed_paths:
        if not unlock_console(host, qmp_script, dry_run):
            print("  warning: unlock failed; retrying deletes anyway", file=sys.stderr)
        for path in failed_paths:
            code = delete_remote_file(host, path, dry_run)
            if code not in (0, 402):
                return False

    return True


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

    print(f"  verified size match for {xbox_path} ({remote_size:,} bytes)")
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


def should_prepare_xemu(host: str) -> bool:
    normalized = host.strip().lower()
    return normalized in {"", "127.0.0.1", "::1", "localhost"}


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
    elf_path = os.path.join(ROOT_DIR, "build", "halo")
    if os.path.isdir(os.path.join(ROOT_DIR, "build")):
        if args.skip_build:
            # --skip-build skips compile but still patches XBE if the ELF is newer
            if os.path.isfile(elf_path) and (
                not os.path.isfile(xbe_path)
                or os.path.getmtime(elf_path) > os.path.getmtime(xbe_path)
            ):
                print("ELF newer than XBE, patching XBE...")
                rc = run_build(target="patched_xbe", quiet=True)
                if rc != 0:
                    print("error: XBE patch failed", file=sys.stderr)
                    return rc
        elif is_build_current(xbe_path):
            print("build unchanged, skipping rebuild...")
        else:
            print("building patched XBE...")
            rc = run_build(target="patched_xbe", quiet=True)
            if rc != 0:
                print("error: build failed", file=sys.stderr)
                return rc

    if not os.path.isfile(xbe_path):
        print("error: default.xbe not found in halo-patched/", file=sys.stderr)
        return 1

    host = args.xbox

    # Reset xemu after eject so the guest stops running the old title.
    qmp_script = os.path.join(ROOT_DIR, "tools", "xbox", "xemu_qmp.py")
    if should_prepare_xemu(host) and os.path.isfile(qmp_script):
        prepare_xemu_for_deploy(qmp_script)

    print_build_hash(xbe_path)

    dest = args.dest
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
    debug_txt_dest = f"{dest.lstrip('x')}\\debug.txt"
    gamestate_txt_dest = f"{dest.lstrip('x')}\\gamestate.txt"
    stabbed_txt_dest = f"{dest.lstrip('x')}\\stabbed.txt"
    crashdump_dest = "E:\\crashdump.xdmp"

    print(f"deploying to {dest}" + (f" on {host}" if host else ""))

    if args.xbe_only:
        print(f"  default.xbe ({os.path.getsize(xbe_path):,} bytes)")
        rc = deploy_default_xbe(
            xbe_path=xbe_path,
            xbe_src=xbe_src,
            xbe_dest=xbe_dest,
            host=host,
            dry_run=args.dry_run,
            common_kwargs=common_kwargs,
        )
        if rc != 0:
            return rc
        if not delete_state_files(host, [debug_txt_dest, gamestate_txt_dest, stabbed_txt_dest, crashdump_dest], args.dry_run, qmp_script):
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
        if not delete_state_files(host, [debug_txt_dest, gamestate_txt_dest, stabbed_txt_dest, crashdump_dest], args.dry_run, qmp_script):
            return 1
        print("done.")
        rc = launch_xbe(args.dest, host, args.dry_run)
        if rc != 0:
            return rc
        return 0

    # Default: deploy XBE + anything that looks like it changed (maps, etc.)
    # First always push the XBE
    print(f"  default.xbe ({os.path.getsize(xbe_path):,} bytes)")
    rc = deploy_default_xbe(
        xbe_path=xbe_path,
        xbe_src=xbe_src,
        xbe_dest=xbe_dest,
        host=host,
        dry_run=args.dry_run,
        common_kwargs=common_kwargs,
    )
    if rc != 0:
        return rc
    if not delete_state_files(host, [debug_txt_dest, gamestate_txt_dest, stabbed_txt_dest, crashdump_dest], args.dry_run, qmp_script):
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
