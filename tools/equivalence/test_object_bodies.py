#!/usr/bin/env python3
"""Self-test for qmp_capture's object-body following (pure helpers only).

The object pool's element is a 12-byte datum ENTRY holding a pointer at +0x08 to
the object BODY elsewhere in the heap, so capturing the pool captures no object
state. These pins cover the two pure functions that turn the entry array into a
small set of memsave regions: liveness/pointer-validity filtering, and the
gap-merge that keeps a frame to ~20 round-trips instead of ~460.
"""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import qmp_capture as qc  # noqa: E402


def entry(salt, ptr, es=12, ptr_off=8):
    e = bytearray(es)
    struct.pack_into("<H", e, 0, salt)
    struct.pack_into("<I", e, ptr_off, ptr)
    return bytes(e)


class TestObjectBodyPtrs(unittest.TestCase):
    def test_live_entries_only(self):
        data = entry(0xE26F, 0x800BF3F8) + entry(0, 0x80112233) + entry(0xE271, 0x800C01A0)
        self.assertEqual(qc.object_body_ptrs(data, 12), [0x800BF3F8, 0x800C01A0])

    def test_rejects_out_of_heap_pointers(self):
        data = entry(1, 0x00000000) + entry(2, 0x7FFFFFFF) + entry(3, 0x84000000) \
            + entry(4, 0x80000000) + entry(5, 0x83FFFFFF)
        self.assertEqual(qc.object_body_ptrs(data, 12), [0x80000000, 0x83FFFFFF])

    def test_count_bounds_the_scan(self):
        data = entry(1, 0x80001000) + entry(2, 0x80002000) + entry(3, 0x80003000)
        self.assertEqual(qc.object_body_ptrs(data, 12, count=2),
                         [0x80001000, 0x80002000])
        # count larger than the buffer must not read past it
        self.assertEqual(len(qc.object_body_ptrs(data, 12, count=99)), 3)

    def test_degenerate_element_size(self):
        self.assertEqual(qc.object_body_ptrs(b"\x00" * 32, 0), [])
        self.assertEqual(qc.object_body_ptrs(b"\x00" * 32, 8), [])   # ptr_off+4 > es


class TestMergeBodySpecs(unittest.TestCase):
    def test_empty(self):
        self.assertEqual(qc.merge_body_specs([]), [])

    def test_merges_near_adjacent_and_splits_on_big_hole(self):
        # gap 0x18c between windows -> merge; 0x2c10 -> split
        ptrs = [0x800BF3F8, 0x800BF684, 0x800C2300]
        specs = qc.merge_body_specs(ptrs, size=0x100, merge_gap=0x800)
        self.assertEqual(specs, [(0x800BF3F8, 0x800BF684 + 0x100 - 0x800BF3F8),
                                 (0x800C2300, 0x100)])

    def test_max_region_caps_a_run(self):
        ptrs = [0x80000000 + i * 0x100 for i in range(64)]     # perfectly contiguous
        specs = qc.merge_body_specs(ptrs, size=0x100, merge_gap=0x800,
                                    max_region=0x1000)
        self.assertTrue(all(sz <= 0x1000 for _, sz in specs))
        self.assertEqual(sum(sz for _, sz in specs), 64 * 0x100)

    def test_covers_every_body_and_is_sorted(self):
        ptrs = [0x80005000, 0x80001000, 0x80001280, 0x80009000]
        specs = qc.merge_body_specs(ptrs, size=0x100, merge_gap=0x400)
        self.assertEqual(specs, sorted(specs))
        for p in ptrs:
            self.assertTrue(any(a <= p and p + 0x100 <= a + n for a, n in specs),
                            f"body {p:#x} not covered by {specs}")

    def test_duplicate_pointers_collapse(self):
        self.assertEqual(qc.merge_body_specs([0x80001000, 0x80001000], size=0x40),
                         [(0x80001000, 0x40)])

    def test_wasted_bytes_bounded_by_merge_gap(self):
        gap = 0x200
        ptrs = [0x80001000, 0x80001000 + 0x100 + gap]
        specs = qc.merge_body_specs(ptrs, size=0x100, merge_gap=gap)
        self.assertEqual(len(specs), 1)
        self.assertEqual(specs[0][1], 2 * 0x100 + gap)


if __name__ == "__main__":
    unittest.main(verbosity=2)
