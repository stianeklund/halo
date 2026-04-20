#!/usr/bin/env python3

import argparse
import fnmatch
import os
import shutil
import subprocess
import sys
import tempfile
import time

from build_hash import print_build_hash
from local_env import build_windows_python_command


ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
SOURCE_DIR = "halo-patched"
OUTPUT_ISO = "halo-patched.iso"
EXTRACT_XISO = os.path.join(ROOT_DIR, "tools", "extract-xiso.exe")
XEMU_QMP = os.path.join(ROOT_DIR, "tools", "xemu_qmp.py")
DEFAULT_RETRIES = 2
DEFAULT_RETRY_DELAY = 2.0

EXCLUDE_PATTERNS = [
    "*.id0",
    "*.id1",
    "*.id2",
    "*.nam",
    "*.til",
]


def eject_from_xemu() -> None:
    iso_path = os.path.join(ROOT_DIR, OUTPUT_ISO)
    if not os.path.exists(iso_path):
        return
    try:
        command = build_windows_python_command(XEMU_QMP, ["eject"])
        if command is None:
            command = [sys.executable, XEMU_QMP, "eject"]
        subprocess.run(
            command,
            check=False,
            capture_output=True,
            cwd=ROOT_DIR,
            timeout=5,
        )
    except Exception:
        pass


def is_excluded(filename: str) -> bool:
    return any(fnmatch.fnmatch(filename, pat) for pat in EXCLUDE_PATTERNS)


def run_extract_xiso() -> int:
    source_abs = os.path.join(ROOT_DIR, SOURCE_DIR)
    excluded_files: list[str] = []
    for root, _dirs, files in os.walk(source_abs):
        for fname in files:
            if is_excluded(fname):
                excluded_files.append(os.path.join(root, fname))

    tmp_dir = None
    moved: list[tuple[str, str]] = []
    try:
        if excluded_files:
            tmp_dir = tempfile.mkdtemp(prefix="build_iso_excl_")
            for i, fpath in enumerate(excluded_files):
                tmp_name = f"{i}_{os.path.basename(fpath)}"
                tmp_path = os.path.join(tmp_dir, tmp_name)
                shutil.move(fpath, tmp_path)
                moved.append((fpath, tmp_path))

        command = [EXTRACT_XISO, "-c", SOURCE_DIR, OUTPUT_ISO]
        try:
            completed = subprocess.run(command, check=False, cwd=ROOT_DIR)
        except PermissionError:
            print("Permission denied. Close xemu and retry.", file=sys.stderr)
            return 1

        return completed.returncode
    finally:
        for orig_path, tmp_path in moved:
            if os.path.exists(tmp_path):
                shutil.move(tmp_path, orig_path)
        if tmp_dir and os.path.isdir(tmp_dir):
            shutil.rmtree(tmp_dir, ignore_errors=True)


def build_iso(retries: int, retry_delay: float) -> int:
    eject_from_xemu()

    for attempt in range(retries + 1):
        result = run_extract_xiso()
        if result == 0:
            iso_path = os.path.join(ROOT_DIR, OUTPUT_ISO)
            print_build_hash(os.path.join(ROOT_DIR, SOURCE_DIR, "default.xbe"))
            print_build_hash(iso_path)
            return 0

        if attempt == retries:
            print(f"extract-xiso failed with exit code {result}", file=sys.stderr)
            return result

        print(
            f"extract-xiso failed with exit code {result}; "
            f"ejecting media from xemu and retrying "
            f"({attempt + 1}/{retries})",
            file=sys.stderr,
        )
        eject_from_xemu()
        time.sleep(retry_delay)

    return 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Create halo-patched.iso with extract-xiso.",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=DEFAULT_RETRIES,
        help=f"retry extract-xiso this many times after ejecting xemu media (default: {DEFAULT_RETRIES})",
    )
    parser.add_argument(
        "--retry-delay",
        type=float,
        default=DEFAULT_RETRY_DELAY,
        help=f"seconds to wait after an eject before retrying (default: {DEFAULT_RETRY_DELAY})",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()

    if not os.path.exists(EXTRACT_XISO):
        print(f"missing tool: {EXTRACT_XISO}", file=sys.stderr)
        return 1

    if not os.path.isdir(os.path.join(ROOT_DIR, SOURCE_DIR)):
        print(
            f"missing source directory: {os.path.join(ROOT_DIR, SOURCE_DIR)}",
            file=sys.stderr,
        )
        return 1

    if args.retries < 0:
        print("--retries must be >= 0", file=sys.stderr)
        return 1

    if args.retry_delay < 0:
        print("--retry-delay must be >= 0", file=sys.stderr)
        return 1

    return build_iso(args.retries, args.retry_delay)


if __name__ == "__main__":
    raise SystemExit(main())
