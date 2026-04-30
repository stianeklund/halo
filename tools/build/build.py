#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Build the patched XBE via CMake.

Usage:
    python3 tools/build/build.py                    # default: build all targets
    python3 tools/build/build.py --target halo      # build specific target
    python3 tools/build/build.py -q --target halo   # warnings/errors only
"""

import argparse
import os
import subprocess
import sys


ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "../..")
)
BUILD_DIR = os.path.join(ROOT_DIR, "build")


def _run_cmake_build(target: str = "", quiet: bool = False) -> int:
    command = ["cmake", "--build", BUILD_DIR]
    if target:
        command += ["--target", target]
    if quiet:
        command += ["--", "--quiet"]

    env = os.environ.copy()
    if quiet:
        env["LOG_LEVEL"] = "WARNING"

    stdout = subprocess.DEVNULL if quiet else None
    result = subprocess.run(command, stdout=stdout, check=False, cwd=ROOT_DIR, env=env)
    return result.returncode


def build(target: str = "", quiet: bool = False) -> int:
    if not os.path.isdir(BUILD_DIR):
        print(
            f"error: build directory not found: {BUILD_DIR}\n"
            "  run cmake -Bbuild -S. first",
            file=sys.stderr,
        )
        return 1

    if target != "reverse_thunk_tests":
        thunk_result = _run_cmake_build("reverse_thunk_tests", quiet=quiet)
        if thunk_result != 0:
            return thunk_result

    return _run_cmake_build(target, quiet=quiet)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build the patched XBE.")
    parser.add_argument(
        "--target",
        default="",
        help="CMake target to build (default: all)",
    )
    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Only show warnings and errors.",
    )
    args = parser.parse_args()
    return build(target=args.target, quiet=args.quiet)


if __name__ == "__main__":
    raise SystemExit(main())
