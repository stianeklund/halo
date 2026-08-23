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


def pool_header(maximum, element_size, current, data):
    header = bytearray(qc.DATA_T_HDR_LEN)
    struct.pack_into("<hh", header, 0x20, maximum, element_size)
    struct.pack_into("<I", header, 0x28, qc.DATA_T_MAGIC)
    struct.pack_into("<h", header, 0x2E, current)
    struct.pack_into("<I", header, 0x34, data)
    return bytes(header)


class FakeCapture:
    def __init__(self):
        self.memory = {}
        self.reads = []
        self.stops = 0
        self.continues = 0

    def map(self, address, data):
        for offset, value in enumerate(data):
            self.memory[address + offset] = value

    def read_mem(self, address, size):
        self.reads.append((address, size))
        return bytes(self.memory.get(address + offset, 0) for offset in range(size))

    def read_u32(self, address):
        return struct.unpack("<I", self.read_mem(address, 4))[0]

    def stop(self):
        self.stops += 1

    def cont(self):
        self.continues += 1


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

    def test_full_fidelity_sizes_item_objects_from_type_byte(self):
        rows = []
        for slot, (kind, size) in enumerate(((2, 0x27C), (3, 0x1F4),
                                               (4, 0x1F4), (5, 0x228))):
            rows.append(entry(0x1000 + slot, 0x80001000 + slot * 0x1000))
            row = bytearray(rows[-1])
            row[3] = kind
            rows[-1] = bytes(row)
        self.assertEqual(
            qc.object_body_specs_from_entries(b"".join(rows), 12),
            [(0x80001000, 0x27C), (0x80002000, 0x1F4),
             (0x80003000, 0x1F4), (0x80004000, 0x228)])


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


class TestSparsePoolCapture(unittest.TestCase):
    def test_dense_prefix_avoids_tail_read(self):
        cap = FakeCapture()
        data = 0x80100000
        cap.map(data, struct.pack("<HH", 1, 2))
        hd = {"max": 4, "cur": 2, "es": 2, "data": data}
        self.assertEqual(qc.capture_pool_data(cap, hd), struct.pack("<HH", 1, 2))
        self.assertEqual(cap.reads, [(data, 4)])

    def test_hole_in_prefix_retains_sparse_tail(self):
        cap = FakeCapture()
        data = 0x80100000
        blob = struct.pack("<HHHH", 1, 0, 0, 4)
        cap.map(data, blob)
        hd = {"max": 4, "cur": 2, "es": 2, "data": data}
        self.assertEqual(qc.capture_pool_data(cap, hd), blob)
        self.assertEqual(cap.reads, [(data, 4), (data + 4, 4)])


class TestFocusedCapture(unittest.TestCase):
    def test_selected_props_weapons_and_relations_are_retained(self):
        cap = FakeCapture()
        time_block = 0x80001000
        cap.map(qc.GAME_TIME_GLOBALS_PTR, struct.pack("<I", time_block))
        tick_data = bytearray(0x10)
        struct.pack_into("<I", tick_data, 0x0C, 1234)
        cap.map(time_block, tick_data)

        object_header, actor_header, prop_header = 0x80002000, 0x80003000, 0x80004000
        object_data, actor_data, prop_data = 0x80100000, 0x80200000, 0x80300000
        cap.map(qc.OBJECT_TABLE_PTR, struct.pack("<I", object_header))
        cap.map(qc.ACTOR_TABLE_PTR, struct.pack("<I", actor_header))
        cap.map(qc.PROP_POOL_PTR, struct.pack("<I", prop_header))
        cap.map(object_header, pool_header(5, 12, 4, object_data))
        cap.map(actor_header, pool_header(6, 0x724, 1, actor_data))
        cap.map(prop_header, pool_header(8, 0x138, 1, prop_data))

        handles = [0, 0x11110001, 0x22220002, 0x33330003, 0x44440004]
        body_ptrs = [0, 0x81000000, 0x81001000, 0x81002000, 0x81003000]
        object_rows = []
        for slot in range(5):
            row = bytearray(entry(handles[slot] >> 16, body_ptrs[slot]))
            row[3] = 2 if slot == 3 else 0
            object_rows.append(bytes(row))
        cap.map(object_data, b"".join(object_rows))

        actor_rows = bytearray(6 * 0x724)
        actor = memoryview(actor_rows)[5 * 0x724:6 * 0x724]
        struct.pack_into("<H", actor, 0, 0x5555)
        struct.pack_into("<I", actor, qc.ACTOR_UNIT_HANDLE_OFF, handles[1])
        struct.pack_into("<I", actor, qc.ACTOR_PRIMARY_PROP_OFF, 0x77770007)
        struct.pack_into("<I", actor, qc.ACTOR_COMBAT_TARGET_OFF, handles[2])
        cap.map(actor_data, actor_rows)

        prop_rows = bytearray(8 * 0x138)
        struct.pack_into("<H", prop_rows, 7 * 0x138, 0x7777)
        cap.map(prop_data, prop_rows)

        unit = bytearray(0x480)
        struct.pack_into("<I", unit, 0x2A8, handles[3])
        struct.pack_into("<I", unit, qc.OBJECT_PARENT_OFF, handles[4])
        for off in (0x2AC, 0x2B0, 0x2B4, qc.OBJECT_FIRST_CHILD_OFF,
                    qc.OBJECT_NEXT_SIBLING_OFF):
            struct.pack_into("<I", unit, off, qc.HANDLE_NONE)
        cap.map(body_ptrs[1], unit)
        for ptr in body_ptrs[2:]:
            body = bytearray(0x480)
            for off in (qc.OBJECT_PARENT_OFF, qc.OBJECT_FIRST_CHILD_OFF,
                        qc.OBJECT_NEXT_SIBLING_OFF):
                struct.pack_into("<I", body, off, qc.HANDLE_NONE)
            cap.map(ptr, body)

        regions, tick = qc.capture_focused_frame(
            cap, actor_slots=[5], pools=("objects", "actors"),
            include_perception=True, include_linked_object_body=True,
            include_weapon_bodies=True, include_object_relations=True)

        self.assertEqual(tick, 1234)
        self.assertEqual(cap.stops, 1)
        self.assertEqual(cap.continues, 1)
        self.assertEqual(len(regions[object_data]), 5 * 12)
        self.assertEqual(len(regions[actor_data]), 6 * 0x724)
        self.assertEqual(len(regions[prop_data]), 8 * 0x138)
        for ptr in body_ptrs[1:]:
            self.assertIsNotNone(qc._read_from_regions(regions, ptr, 0xD0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
