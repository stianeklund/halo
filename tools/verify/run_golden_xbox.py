#!/usr/bin/env python3
"""Build, deploy, and capture golden test harness results from Xbox.

Single command:
    python3 tools/verify/run_golden_xbox.py

Workflow:
    1. Build with HALO_TEST_HARNESS=ON
    2. Deploy to Xbox via XBDM
    3. Wait for RUN|END in debug.txt
    4. Print results summary
    5. Restore normal (non-harness) build and redeploy
"""

import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
BUILD_DIR = ROOT / "build"
DEBUG_TXT = ROOT / "xbdm" / "debug.txt"
PULL_SCRIPT = ROOT / "tools" / "xbox" / "xbdm_debug_txt.py"
DEPLOY_SCRIPT = ROOT / "tools" / "xbox" / "deploy_xbox.py"
TOOLCHAIN = ROOT / "toolchains" / "llvm.cmake"


def run(cmd, quiet=False, **kwargs):
    if not quiet:
        print(f"  > {' '.join(str(c) for c in cmd)}")
    return subprocess.run(cmd, cwd=str(ROOT), **kwargs)


def cmake_configure(test_harness: bool, quiet=False):
    harness = "ON" if test_harness else "OFF"
    cmd = [
        "cmake", "-B", str(BUILD_DIR), "-S", str(ROOT),
        f"-DHALO_TEST_HARNESS={harness}",
        f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN}",
    ]
    if quiet:
        cmd.append("-Wno-dev")
    stdout = subprocess.DEVNULL if quiet else None
    r = run(cmd, quiet=quiet, stdout=stdout)
    if r.returncode != 0:
        print("cmake configure failed", file=sys.stderr)
        sys.exit(1)


def cmake_build(target="halo", quiet=False):
    cmd = ["cmake", "--build", str(BUILD_DIR), "--target", target]
    if quiet:
        cmd += ["--", "--quiet"]
    stdout = subprocess.DEVNULL if quiet else None
    r = run(cmd, quiet=quiet, stdout=stdout)
    if r.returncode != 0:
        print("cmake build failed", file=sys.stderr)
        sys.exit(1)


def build(test_harness: bool):
    label = "TEST_HARNESS" if test_harness else "normal"
    print(f"\n=== Building ({label}) ===")
    cmake_configure(test_harness, quiet=True)
    cmake_build("reverse_thunk_tests", quiet=True)
    cmake_build("halo", quiet=True)


def patch_xbe():
    print("  patching XBE...")
    r = run(["cmake", "--build", str(BUILD_DIR), "--target", "patched_xbe"],
            quiet=True, capture_output=True)
    if r.returncode != 0:
        print("XBE patching failed", file=sys.stderr)
        sys.exit(1)


def deploy():
    print("\n=== Deploying to Xbox ===")
    r = run([sys.executable, str(DEPLOY_SCRIPT), "--xbe-only", "--skip-build"])
    if r.returncode != 0:
        print(f"Deploy failed (exit {r.returncode})", file=sys.stderr)
        sys.exit(1)


def wait_for_results(timeout=120):
    print(f"\n=== Waiting for test harness (timeout {timeout}s) ===")
    deadline = time.time() + timeout
    while time.time() < deadline:
        time.sleep(4)
        r = run([sys.executable, str(PULL_SCRIPT)],
                quiet=True, capture_output=True, text=True)
        if r.returncode != 0:
            continue
        if DEBUG_TXT.exists():
            text = DEBUG_TXT.read_text(encoding="utf-8", errors="replace")
            if "RUN|END" in text:
                return text
    print("Timed out waiting for test harness output", file=sys.stderr)
    if DEBUG_TXT.exists():
        return DEBUG_TXT.read_text(encoding="utf-8", errors="replace")
    return None


def print_results(text):
    print("\n=== Test Harness Results ===")
    passed = 0
    failed = 0
    failures = []
    cases = []
    for line in text.splitlines():
        line = line.strip()
        if "ASSERT|PASS|" in line:
            passed += 1
        elif "ASSERT|FAIL|" in line:
            failed += 1
            failures.append(line)
        elif "CASE|" in line or "VALUE|" in line:
            cases.append(line)
        elif "RUN|END" in line:
            print(f"  {line}")

    print(f"\n  {passed} passed, {failed} failed")
    if failures:
        print("\n  Failures:")
        for f in failures:
            print(f"    {f}")
    if cases:
        print("\n  Dump cases:")
        for c in cases:
            print(f"    {c}")


def main():
    build(test_harness=True)
    deploy()
    text = wait_for_results()
    if text:
        print_results(text)

    print("\n=== Restoring normal build ===")
    build(test_harness=False)
    deploy()
    print("\nDone.")


if __name__ == "__main__":
    main()
