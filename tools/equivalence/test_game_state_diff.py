#!/usr/bin/env python3
"""Unit tests for game-state snapshot diff logic.

These tests don't require xemu — they test the in-memory comparison
engine that powers A/B regression detection.
"""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))

from game_state_replay import diff_snapshots


def _make_snapshot(description="test", cpu_data=None, gpu_data=None,
                   header_data=None, globals_data=None,
                   base_addr="0x80061000", cpu_size=None):
    """Build a snapshot dict matching the capture format."""
    if cpu_data is None:
        cpu_data = b"\x00" * 0x1000
    if gpu_data is None:
        gpu_data = b"\x00" * 0x1000
    if header_data is None:
        header_data = b"\x00" * 0x14c
    if globals_data is None:
        globals_data = b"\x00" * 0x20

    cs = cpu_size or f"0x{len(cpu_data):x}"

    return {
        "description": description,
        "captured_at": "2026-01-01T00:00:00",
        "globals_raw": {
            "game_state_globals": {
                "addr": "0x004ea990",
                "size": f"0x{len(globals_data):x}",
                "data": globals_data.hex(),
            },
            "xbox_game_state_globals": {
                "addr": "0x004ea9b0",
                "size": "0x14",
                "data": "00" * 20,
            },
        },
        "globals_parsed": {
            "base_address": base_addr,
            "cpu_allocation_size": cs,
            "gpu_allocation_size": "0x40000",
            "header_ptr": "0x80000000",
            "saved": True,
        },
        "header_raw": {
            "addr": "0x80000000",
            "size": f"0x{len(header_data):x}",
            "data": header_data.hex(),
        },
        "regions": {
            "cpu_pool": {
                "base": base_addr,
                "virtual_addr": base_addr,
                "size": cs,
                "data": cpu_data.hex(),
            },
            "gpu_pool": {
                "base": "0x00345000",
                "virtual_addr": "0x00345000",
                "size": f"0x{len(gpu_data):x}",
                "data": gpu_data.hex(),
            },
        },
    }


class TestDiffIdentical(unittest.TestCase):
    def test_identical_snapshots(self):
        data = b"\x00\x01\x02\x03" * 0x400
        a = _make_snapshot(description="baseline", cpu_data=data)
        b = _make_snapshot(description="candidate", cpu_data=data)

        report = diff_snapshots(a, b)

        self.assertEqual(report["regions"]["cpu_pool"]["status"], "identical")
        self.assertEqual(report["regions"]["gpu_pool"]["status"], "identical")
        self.assertEqual(report["globals_diff"]["game_state_globals"], "identical")

    def test_identical_empty(self):
        a = _make_snapshot()
        b = _make_snapshot()
        report = diff_snapshots(a, b)

        self.assertEqual(report["regions"]["cpu_pool"]["status"], "identical")
        self.assertEqual(report["regions"]["gpu_pool"]["status"], "identical")


class TestDiffDivergent(unittest.TestCase):
    def test_single_byte_diff(self):
        cpu_a = b"\x00" * 0x100
        cpu_b = bytearray(b"\x00" * 0x100)
        cpu_b[0x42] = 0xFF

        a = _make_snapshot(description="oracle", cpu_data=bytes(cpu_a))
        b = _make_snapshot(description="patched", cpu_data=bytes(cpu_b))

        report = diff_snapshots(a, b)

        self.assertEqual(report["regions"]["cpu_pool"]["status"], "divergent")
        self.assertEqual(report["regions"]["cpu_pool"]["num_diffs"], 1)
        self.assertEqual(report["regions"]["cpu_pool"]["total_diff_bytes"], 1)

        diff = report["regions"]["cpu_pool"]["diffs"][0]
        self.assertEqual(diff["offset"], 0x42)
        self.assertEqual(diff["length"], 1)

    def test_range_diff(self):
        cpu_a = b"\xAA" * 0x200
        cpu_b = bytearray(b"\xAA" * 0x200)
        for i in range(0x80, 0xC0):
            cpu_b[i] = 0xBB

        a = _make_snapshot(cpu_data=bytes(cpu_a))
        b = _make_snapshot(cpu_data=bytes(cpu_b))

        report = diff_snapshots(a, b)

        self.assertEqual(report["regions"]["cpu_pool"]["status"], "divergent")
        self.assertEqual(report["regions"]["cpu_pool"]["num_diffs"], 1)
        self.assertEqual(report["regions"]["cpu_pool"]["total_diff_bytes"], 0x40)

        diff = report["regions"]["cpu_pool"]["diffs"][0]
        self.assertEqual(diff["offset"], 0x80)
        self.assertEqual(diff["length"], 0x40)

    def test_multiple_diff_ranges(self):
        cpu_a = b"\x00" * 0x100
        cpu_b = bytearray(b"\x00" * 0x100)
        cpu_b[0x10] = 0xFF
        cpu_b[0x11] = 0xFF
        cpu_b[0x80] = 0xEE
        cpu_b[0x81] = 0xEE
        cpu_b[0x82] = 0xEE

        a = _make_snapshot(cpu_data=bytes(cpu_a))
        b = _make_snapshot(cpu_data=bytes(cpu_b))

        report = diff_snapshots(a, b)

        self.assertEqual(report["regions"]["cpu_pool"]["num_diffs"], 2)
        self.assertEqual(report["regions"]["cpu_pool"]["total_diff_bytes"], 5)

        offsets = [d["offset"] for d in report["regions"]["cpu_pool"]["diffs"]]
        self.assertIn(0x10, offsets)
        self.assertIn(0x80, offsets)

    def test_size_mismatch(self):
        a = _make_snapshot(cpu_data=b"\x00" * 0x100)
        b = _make_snapshot(cpu_data=b"\x00" * 0x200)

        report = diff_snapshots(a, b)
        self.assertEqual(report["regions"]["cpu_pool"]["status"], "divergent")

    def test_missing_region_in_candidate(self):
        a = _make_snapshot()
        b = _make_snapshot()
        del b["regions"]["cpu_pool"]

        report = diff_snapshots(a, b)
        self.assertEqual(report["regions"]["cpu_pool"]["status"],
                         "missing_in_candidate")

    def test_globals_divergent(self):
        gs_a = b"\x00" * 0x20
        gs_b = bytearray(b"\x00" * 0x20)
        gs_b[0x15] = 1  # 'saved' flag differs

        a = _make_snapshot(globals_data=bytes(gs_a))
        b = _make_snapshot(globals_data=bytes(gs_b))

        report = diff_snapshots(a, b)
        self.assertEqual(report["globals_diff"]["game_state_globals"],
                         "divergent")

    def test_gpu_pool_diff(self):
        gpu_a = b"\x00" * 0x100
        gpu_b = b"\xFF" * 0x100

        a = _make_snapshot(gpu_data=gpu_a)
        b = _make_snapshot(gpu_data=gpu_b)

        report = diff_snapshots(a, b)
        self.assertEqual(report["regions"]["gpu_pool"]["status"], "divergent")
        self.assertEqual(report["regions"]["gpu_pool"]["total_diff_bytes"], 0x100)


class TestDiffRealistic(unittest.TestCase):
    def test_initial_same_then_diverge(self):
        """Simulates two identical pools where only a small section diverges
        (e.g. one ported function writes a different AI state)."""
        pool_size = 0x50000
        base = bytearray(pool_size)
        # Fill with semi-random-looking data
        for i in range(pool_size):
            base[i] = (i * 7 + 13) & 0xFF

        candidate = bytearray(base)
        # Simulate a divergence at offset 0x12340: 16 bytes differ
        for i in range(0x12340, 0x12350):
            candidate[i] ^= 0xAA

        a = _make_snapshot(cpu_data=bytes(base), cpu_size=f"0x{pool_size:x}")
        b = _make_snapshot(cpu_data=bytes(candidate), cpu_size=f"0x{pool_size:x}")

        report = diff_snapshots(a, b)

        self.assertEqual(report["regions"]["cpu_pool"]["num_diffs"], 1)
        diff = report["regions"]["cpu_pool"]["diffs"][0]
        self.assertEqual(diff["offset"], 0x12340)
        self.assertEqual(diff["length"], 16)

    def test_snapshot_roundtrip_serialization(self):
        """Verify snapshot dict round-trips through JSON correctly."""
        cpu = bytes(i & 0xFF for i in range(0x1000))
        gpu = bytes((i * 3) & 0xFF for i in range(0x800))
        gs = bytes((i + 0x40) & 0xFF for i in range(0x20))
        header = bytes(i & 0xFF for i in range(0x14C))

        snap = _make_snapshot(
            description="roundtrip test",
            cpu_data=cpu, gpu_data=gpu, globals_data=gs, header_data=header
        )

        with tempfile.NamedTemporaryFile(mode="w", suffix=".json",
                                         delete=False, encoding="utf-8") as f:
            json.dump(snap, f, indent=2)
            tmp_path = f.name

        try:
            with open(tmp_path, encoding="utf-8") as f:
                loaded = json.load(f)

            loaded_cpu = bytes.fromhex(loaded["regions"]["cpu_pool"]["data"])
            loaded_gpu = bytes.fromhex(loaded["regions"]["gpu_pool"]["data"])
            loaded_gs = bytes.fromhex(loaded["globals_raw"]["game_state_globals"]["data"])
            loaded_hdr = bytes.fromhex(loaded["header_raw"]["data"])

            self.assertEqual(loaded_cpu, cpu)
            self.assertEqual(loaded_gpu, gpu)
            self.assertEqual(loaded_gs, gs)
            self.assertEqual(loaded_hdr, header)
        finally:
            os.unlink(tmp_path)


if __name__ == "__main__":
    unittest.main(verbosity=2)
