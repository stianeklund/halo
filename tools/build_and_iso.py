#!/usr/bin/env python3

import os
import subprocess
import sys

from build import build


ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
ISO_PATH = os.path.join(ROOT_DIR, "halo-patched.iso")
DEFAULT_ISO_RETRIES = 2
DEFAULT_ISO_RETRY_DELAY = 2.0


def run(command: list[str]) -> int:
    completed = subprocess.run(command, check=False, cwd=ROOT_DIR)
    return completed.returncode


def main() -> int:
    build_result = build(quiet=True)
    if build_result != 0:
        print(f"build failed with exit code {build_result}", file=sys.stderr)
        return build_result

    iso_result = run(
        [
            sys.executable,
            os.path.join("tools", "build_iso.py"),
            "--retries",
            str(DEFAULT_ISO_RETRIES),
            "--retry-delay",
            str(DEFAULT_ISO_RETRY_DELAY),
        ]
    )
    if iso_result != 0:
        return iso_result

    xemu_result = run(
        [
            sys.executable,
            os.path.join("tools", "xemu_qmp.py"),
            "--launch-if-missing",
            "load-iso",
            ISO_PATH,
            "--reset",
        ]
    )
    if xemu_result != 0:
        print("warning: ISO built, but xemu load/reset failed", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
