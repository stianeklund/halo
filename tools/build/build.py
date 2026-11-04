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
    python3 tools/build/build.py --rng-trace        # + RNG draw trace
    python3 tools/build/build.py --aim-assist-debug # + aim-assist rings/telemetry
"""

import argparse
import os
import shutil
import subprocess
import sys


ROOT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "../..")
)
BUILD_DIR = os.path.join(ROOT_DIR, "build")
TOOLCHAIN_FILE = os.path.join(ROOT_DIR, "toolchains", "llvm.cmake")


def _build_jobs() -> int:
    """Recipes to run concurrently. Override with HALO_BUILD_JOBS."""
    override = os.environ.get("HALO_BUILD_JOBS")
    if override:
        try:
            return max(1, int(override))
        except ValueError:
            pass
    return os.cpu_count() or 1


def _report_quiet_failure(command, result) -> None:
    """Show output hidden by quiet mode when a command fails."""
    if result.returncode == 0:
        return
    print(
        "[build] command failed (exit %d): %s"
        % (result.returncode, " ".join(str(arg) for arg in command)),
        file=sys.stderr,
    )
    if result.stdout:
        print(result.stdout, file=sys.stderr, end="")


def _run_cmake_build(target: str = "", quiet: bool = False) -> int:
    command = ["cmake", "--build", BUILD_DIR]
    if target:
        command += ["--target", target]
    command += ["--parallel", str(_build_jobs())]
    if quiet:
        command += ["--", "--quiet"]

    env = os.environ.copy()
    if quiet:
        env["LOG_LEVEL"] = "WARNING"

    stdout = subprocess.PIPE if quiet else None
    result = subprocess.run(
        command,
        stdout=stdout,
        text=True,
        check=False,
        cwd=ROOT_DIR,
        env=env,
    )
    if quiet:
        _report_quiet_failure(command, result)
    return result.returncode


def _run_cmake_configure(extra_args: list[str] = None, quiet: bool = False) -> int:
    command = [
        "cmake", "-B", BUILD_DIR, "-S", ROOT_DIR,
        "-DCMAKE_TOOLCHAIN_FILE=" + TOOLCHAIN_FILE,
    ]
    if quiet:
        command.append("-Wno-dev")
    if extra_args:
        command.extend(extra_args)
    stdout = subprocess.PIPE if quiet else None
    result = subprocess.run(
        command,
        stdout=stdout,
        text=True,
        check=False,
        cwd=ROOT_DIR,
    )
    if quiet:
        _report_quiet_failure(command, result)
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
    cmd = [shutil.which("objcopy") or shutil.which("llvm-objcopy") or "objcopy"]
    for sec in (".text", ".rdata", ".data", ".thunks", ".reloc"):
        cmd += ["--change-section-vma", f"{sec}+{offset:#x}"]
    cmd += [src, dst]
    result = subprocess.run(cmd, check=False, cwd=ROOT_DIR,
                            stdout=subprocess.DEVNULL if quiet else None)
    if result.returncode != 0 and not quiet:
        print(f"warning: objcopy failed, build/halo.debug not updated", file=sys.stderr)


def _check_frame_pointer(quiet: bool = False) -> int:
    """Fail the build if a caller of a caller-EBP-reading helper lost its frame.

    Cheap post-link guard (see tools/audit/check_frame_pointer.py). Omitting
    -fno-omit-frame-pointer black-screens the game with no assert, so this is a
    hard error rather than a warning.
    """
    script = os.path.join(ROOT_DIR, "tools", "audit", "check_frame_pointer.py")
    exe = os.path.join(BUILD_DIR, "halo")
    if not os.path.isfile(script) or not os.path.isfile(exe):
        return 0
    result = subprocess.run([sys.executable, script, exe], check=False,
                            cwd=ROOT_DIR)
    if result.returncode == 2:
        # Missing pefile/capstone -- do not block the build on tooling gaps.
        if not quiet:
            print("warning: frame-pointer check could not run", file=sys.stderr)
        return 0
    return result.returncode


def _check_self_jumps(quiet: bool = False) -> int:
    """Fail the build on a `jmp $` self-loop the original binary lacks.

    A busy-wait over a non-volatile lvalue lets clang hoist the load and emit
    `jmp $` -- a silent freeze VC71 cannot see (see
    tools/audit/check_self_jumps.py).
    """
    script = os.path.join(ROOT_DIR, "tools", "audit", "check_self_jumps.py")
    exe = os.path.join(BUILD_DIR, "halo")
    if not os.path.isfile(script) or not os.path.isfile(exe):
        return 0
    result = subprocess.run([sys.executable, script, exe], check=False,
                            cwd=ROOT_DIR)
    if result.returncode == 2:
        if not quiet:
            print("warning: self-jump check could not run", file=sys.stderr)
        return 0
    return result.returncode


def _env_flag(name: str) -> bool:
    value = os.environ.get(name, "")
    return value.lower() in ("1", "true", "yes", "on")


def _ensure_generated_files(quiet: bool = False) -> None:
    """Ensure build/generated/decl.h and thunks match kb.json before building.

    Guards against mtime inversion after git branch switches or rebases
    where decl.h has a newer timestamp than an older-timestamped kb.json.
    """
    gen_dir = os.path.join(BUILD_DIR, "generated")
    stamp_file = os.path.join(gen_dir, ".kb.sha256")
    decl_h = os.path.join(gen_dir, "decl.h")
    def_file = os.path.join(gen_dir, "halo.xbe.def")
    thunks_c = os.path.join(gen_dir, "thunks.c")
    kb_path = os.path.join(ROOT_DIR, "kb.json")
    knowledge_py = os.path.join(ROOT_DIR, "tools", "analysis", "knowledge.py")

    if not (os.path.isfile(kb_path) and os.path.isfile(knowledge_py)):
        return

    import hashlib
    with open(kb_path, "rb") as f:
        kb_hash = hashlib.sha256(f.read()).hexdigest()
    with open(knowledge_py, "rb") as f:
        py_hash = hashlib.sha256(f.read()).hexdigest()
    current_hash = f"{kb_hash}_{py_hash}"

    saved_hash = ""
    if os.path.isfile(stamp_file):
        try:
            with open(stamp_file, "r") as f:
                saved_hash = f.read().strip()
        except OSError:
            pass

    if (
        saved_hash == current_hash
        and os.path.isfile(decl_h)
        and os.path.isfile(def_file)
        and os.path.isfile(thunks_c)
    ):
        return

    if not quiet:
        print("[build] Refreshing generated decl.h and thunks from kb.json...", flush=True)

    os.makedirs(gen_dir, exist_ok=True)
    cmd = [
        sys.executable,
        knowledge_py,
        "--gen-header", decl_h,
        "--gen-def", def_file,
        "--gen-thunks", thunks_c,
        "--gen-stamp", stamp_file,
    ]
    env = os.environ.copy()
    if quiet:
        env["LOG_LEVEL"] = "WARNING"
    subprocess.run(cmd, check=False, cwd=ROOT_DIR, env=env,
                   stdout=subprocess.DEVNULL if quiet else None)


def build(target: str = "", quiet: bool = False, test_harness: bool = False,
          retail64: bool | None = None, rng_trace: bool = False,
          aim_assist_debug: bool = False) -> int:
    if not os.path.isdir(BUILD_DIR):
        print(
            f"error: build directory not found: {BUILD_DIR}\n"
            "  run cmake -Bbuild -S. first",
            file=sys.stderr,
        )
        return 1

    _ensure_generated_files(quiet=quiet)

    configure_args = ["-DHALO_TEST_HARNESS=" + ("ON" if test_harness else "OFF")]
    # Always pass both states: a cache that kept ON would silently leave
    # tracing live in a "flag off" build.
    configure_args.append("-DHALO_RNG_TRACE=" + ("ON" if rng_trace else "OFF"))
    configure_args.append("-DHALO_AIM_ASSIST_DEBUG="
                          + ("ON" if aim_assist_debug else "OFF"))
    if retail64 is not None:
        configure_args.append("-DHALO_RETAIL64=" + ("ON" if retail64 else "OFF"))
    cfg_result = _run_cmake_configure(extra_args=configure_args, quiet=quiet)
    if cfg_result != 0:
        return cfg_result

    if target != "reverse_thunk_tests":
        thunk_result = _run_cmake_build("reverse_thunk_tests", quiet=quiet)
        if thunk_result != 0:
            return thunk_result

    build_result = _run_cmake_build(target, quiet=quiet)
    if build_result != 0:
        return build_result

    fp_result = _check_frame_pointer(quiet=quiet)
    if fp_result != 0:
        return fp_result

    sj_result = _check_self_jumps(quiet=quiet)
    if sj_result != 0:
        return sj_result

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
    parser.add_argument(
        "--rng-trace",
        action="store_true",
        help="Build with the diagnostic RNG draw trace enabled "
             "(HALO_RNG_TRACE=ON). See docs/rng-trace.md.",
    )
    parser.add_argument(
        "--aim-assist-debug",
        action="store_true",
        help="Build with the diagnostic aim-assist cone rings and magnetism "
             "telemetry (HALO_AIM_ASSIST_DEBUG=ON). See docs/aim-assist-debug.md.",
    )
    retail64_group = parser.add_mutually_exclusive_group()
    retail64_group.add_argument(
        "--retail64",
        dest="retail64",
        action="store_true",
        default=True if _env_flag("HALO_RETAIL64") else None,
        help="Build experimental 64MB retail-compatible runtime profile.",
    )
    retail64_group.add_argument(
        "--no-retail64",
        dest="retail64",
        action="store_false",
        help="Disable the experimental 64MB retail-compatible runtime profile.",
    )
    args = parser.parse_args()
    return build(target=args.target, quiet=args.quiet, test_harness=args.test,
                 retail64=args.retail64, rng_trace=args.rng_trace,
                 aim_assist_debug=args.aim_assist_debug)


if __name__ == "__main__":
    raise SystemExit(main())
