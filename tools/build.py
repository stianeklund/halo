#!/usr/bin/env python3
"""Build the patched XBE via CMake.

Usage:
    python3 tools/build.py            # default: build all targets
    python3 tools/build.py --target halo  # build specific target
"""

import argparse
import os
import subprocess
import sys


ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
BUILD_DIR = os.path.join(ROOT_DIR, "build")


def build(target: str = "") -> int:
    if not os.path.isdir(BUILD_DIR):
        print(
            f"error: build directory not found: {BUILD_DIR}\n"
            "  run cmake -Bbuild -S. first",
            file=sys.stderr,
        )
        return 1

    command = ["cmake", "--build", BUILD_DIR]
    if target:
        command += ["--target", target]

    result = subprocess.run(command, check=False, cwd=ROOT_DIR)
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description="Build the patched XBE.")
    parser.add_argument(
        "--target",
        default="",
        help="CMake target to build (default: all)",
    )
    args = parser.parse_args()
    return build(target=args.target)


if __name__ == "__main__":
    raise SystemExit(main())
