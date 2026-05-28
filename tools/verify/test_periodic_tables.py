#!/usr/bin/env python3
"""Verify periodic/transition waveform tables on Xbox match golden reference.

Run after deploying a build to Xbox and loading into any map:
    python3 tools/verify/test_periodic_tables.py

Reads the 18 lookup tables (12 periodic + 6 transition) from Xbox memory via
XBDM and compares byte-by-byte against the golden reference captured from the
unpatched cachebeta.xbe.

A tolerance of ±1 per byte is allowed for float-to-int boundary rounding.
"""

import json
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
GOLDEN_DIR = ROOT / "tests" / "golden" / "periodic_tables"

PERIODIC_PTRS_ADDR = 0x46e3b8
TRANSITION_PTRS_ADDR = 0x46e3a0
TABLE_SIZE = 1024
TOLERANCE = 1

PERIODIC_NAMES = [
    "one", "zero", "cosine", "cosine_var", "triangle", "triangle_var",
    "slide", "slide_var", "gaussian", "stutter_4harm_a", "stutter_4harm_b",
    "square",
]
TRANSITION_NAMES = ["raw", "pow_a", "pow_b", "pow_c", "pow_d", "sine_wave"]


def read_mem(addr, length):
    result = subprocess.run(
        ["python3", str(ROOT / "tools" / "xbox" / "xbdm_rdcp.py"),
         "--json", "--binary-length", str(length),
         f"getmem addr=0x{addr:x} length={length}"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        print(f"XBDM read failed at 0x{addr:x}: {result.stderr}", file=sys.stderr)
        sys.exit(1)
    data = json.loads(result.stdout)
    return bytes.fromhex("".join(data["lines"]))


def read_pointers(base_addr, count):
    raw = read_mem(base_addr, count * 4)
    return [struct.unpack_from("<I", raw, i * 4)[0] for i in range(count)]


def compare_table(name, prefix, idx, table_addr):
    golden_path = GOLDEN_DIR / f"{prefix}_{idx:02d}_{name}.bin"
    if not golden_path.exists():
        return "SKIP", f"no golden reference at {golden_path.name}"

    golden = golden_path.read_bytes()
    actual = read_mem(table_addr, TABLE_SIZE)

    if golden == actual:
        return "PASS", "exact match"

    diffs = []
    for i in range(min(len(golden), len(actual))):
        delta = abs(golden[i] - actual[i])
        if delta > 0:
            diffs.append((i, golden[i], actual[i], delta))

    max_delta = max(d[3] for d in diffs)
    if max_delta <= TOLERANCE:
        return "PASS", f"{len(diffs)} bytes within ±{TOLERANCE}"

    return "FAIL", (
        f"{len(diffs)} bytes differ, max delta {max_delta}; "
        f"first: idx={diffs[0][0]} golden={diffs[0][1]} actual={diffs[0][2]}"
    )


def main():
    if not GOLDEN_DIR.exists():
        print(f"Golden reference directory not found: {GOLDEN_DIR}", file=sys.stderr)
        sys.exit(1)

    init_flag = read_mem(0x46e39c, 1)
    if init_flag[0] != 1:
        print("Tables not initialized (flag at 0x46e39c != 1). Load a map first.",
              file=sys.stderr)
        sys.exit(1)

    periodic_ptrs = read_pointers(PERIODIC_PTRS_ADDR, 12)
    transition_ptrs = read_pointers(TRANSITION_PTRS_ADDR, 6)

    passed = 0
    failed = 0
    skipped = 0

    print("Periodic tables:")
    for i, (addr, name) in enumerate(zip(periodic_ptrs, PERIODIC_NAMES)):
        status, detail = compare_table(name, "periodic", i, addr)
        print(f"  [{status}] periodic_{i:02d}_{name}: {detail}")
        if status == "PASS":
            passed += 1
        elif status == "FAIL":
            failed += 1
        else:
            skipped += 1

    print("Transition tables:")
    for i, (addr, name) in enumerate(zip(transition_ptrs, TRANSITION_NAMES)):
        status, detail = compare_table(name, "transition", i, addr)
        print(f"  [{status}] transition_{i:02d}_{name}: {detail}")
        if status == "PASS":
            passed += 1
        elif status == "FAIL":
            failed += 1
        else:
            skipped += 1

    print(f"\nResult: {passed} passed, {failed} failed, {skipped} skipped")
    sys.exit(1 if failed > 0 else 0)


if __name__ == "__main__":
    main()
