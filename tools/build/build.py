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


def _run_cmake_configure(extra_args: list[str] = None, quiet: bool = False) -> int:
    command = ["cmake", "-B", BUILD_DIR, "-S", ROOT_DIR]
    if quiet:
        command.append("-Wno-dev")
    if extra_args:
        command.extend(extra_args)
    stdout = subprocess.DEVNULL if quiet else None
    result = subprocess.run(command, stdout=stdout, check=False, cwd=ROOT_DIR)
    return result.returncode


def _generate_debug_elf(quiet: bool = False) -> None:
    """Generate build/halo.debug with section VMAs adjusted to XBE runtime addresses.

    The ELF is linked at 0x401000 but patch.py places our sections 0x242000
    higher in the XBE (0x643000 for .text). Adjusting the VMAs here lets GDB
    and CLion load the symbol file directly without add-symbol-file relocation.
    """
    src = os.path.join(BUILD_DIR, "halo")
    dst = os.path.join(BUILD_DIR, "halo.debug")
    if not os.path.isfile(src):
        return
    offset = 0x242000
    cmd = ["objcopy"]
    for sec in (".text", ".rdata", ".data", ".thunks", ".reloc"):
        cmd += ["--change-section-vma", f"{sec}+{offset:#x}"]
    cmd += [src, dst]
    result = subprocess.run(cmd, check=False, cwd=ROOT_DIR,
                            stdout=subprocess.DEVNULL if quiet else None)
    if result.returncode != 0 and not quiet:
        print(f"warning: objcopy failed, build/halo.debug not updated", file=sys.stderr)


def build(target: str = "", quiet: bool = False, test_harness: bool = False) -> int:
    if not os.path.isdir(BUILD_DIR):
        print(
            f"error: build directory not found: {BUILD_DIR}\n"
            "  run cmake -Bbuild -S. first",
            file=sys.stderr,
        )
        return 1

    harness_flag = "-DHALO_TEST_HARNESS=" + ("ON" if test_harness else "OFF")
    cfg_result = _run_cmake_configure(extra_args=[harness_flag], quiet=quiet)
    if cfg_result != 0:
        return cfg_result

    if target != "reverse_thunk_tests":
        thunk_result = _run_cmake_build("reverse_thunk_tests", quiet=quiet)
        if thunk_result != 0:
            return thunk_result

    build_result = _run_cmake_build(target, quiet=quiet)
    if build_result != 0:
        return build_result

    _generate_debug_elf(quiet=quiet)
    return 0


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
    parser.add_argument(
        "-t", "--test",
        action="store_true",
        help="Build with the in-engine test harness enabled (HALO_TEST_HARNESS=ON).",
    )
    args = parser.parse_args()
    return build(target=args.target, quiet=args.quiet, test_harness=args.test)


if __name__ == "__main__":
    raise SystemExit(main())
