#!/usr/bin/env python3

import os
import subprocess
import sys


ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
SOURCE_DIR = "halo-patched"
OUTPUT_ISO = "halo-patched.iso"
EXTRACT_XISO = os.path.join(ROOT_DIR, "tools", "extract-xiso.exe")
XEMU_QMP = os.path.join(ROOT_DIR, "tools", "xemu_qmp.py")


def eject_from_xemu() -> None:
    iso_path = os.path.join(ROOT_DIR, OUTPUT_ISO)
    if not os.path.exists(iso_path):
        return
    try:
        subprocess.run(
            [sys.executable, XEMU_QMP, "eject"],
            check=False,
            capture_output=True,
            cwd=ROOT_DIR,
            timeout=5,
        )
    except Exception:
        pass


def main() -> int:
    if not os.path.exists(EXTRACT_XISO):
        print(f"missing tool: {EXTRACT_XISO}", file=sys.stderr)
        return 1

    if not os.path.isdir(os.path.join(ROOT_DIR, SOURCE_DIR)):
        print(
            f"missing source directory: {os.path.join(ROOT_DIR, SOURCE_DIR)}",
            file=sys.stderr,
        )
        return 1

    eject_from_xemu()

    command = [EXTRACT_XISO, "-c", SOURCE_DIR, OUTPUT_ISO]

    try:
        completed = subprocess.run(command, check=False, cwd=ROOT_DIR)
    except PermissionError:
        print("Permission denied. Close xemu and retry.", file=sys.stderr)
        return 1

    if completed.returncode != 0:
        print(f"extract-xiso failed with exit code {completed.returncode}",
              file=sys.stderr)
        return completed.returncode

    print(OUTPUT_ISO)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
