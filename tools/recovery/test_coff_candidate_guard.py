"""In-memory tests for the candidate neutrality guard."""

import struct
import tempfile
import unittest
from pathlib import Path

from tools.recovery.coff_candidate_guard import (
    GuardError, capture_object, compare_snapshots, _self_test,
)


def _minimal_object_with_bss():
    """Build a 2-section i386 COFF: .text with bytes, .bss uninitialized.

    .bss carries SizeOfRawData=0x400 with PointerToRawData=0, exactly as clang
    and MSVC emit it.  That is legal and must not read as truncation.
    """
    text = b"\x90\x90\x90\x90"
    header_size = 20
    section_table = header_size + 2 * 40
    text_offset = section_table
    sym_ptr = text_offset + len(text)
    header = struct.pack("<HHIIIHH", 0x014c, 2, 0, sym_ptr, 0, 0, 0)
    text_hdr = struct.pack("<8sIIIIIIHHI", b".text", 0, 0, len(text), text_offset,
                           0, 0, 0, 0, 0x60500020)
    bss_hdr = struct.pack("<8sIIIIIIHHI", b".bss", 0, 0, 0x400, 0,
                          0, 0, 0, 0, 0xC0300080)
    return header + text_hdr + bss_hdr + text + struct.pack("<I", 4)


class CandidateGuardTests(unittest.TestCase):
    def test_required_cases(self):
        self.assertEqual(_self_test(), 0)

    def test_malformed_baseline_is_rejected(self):
        with self.assertRaises(GuardError):
            compare_snapshots({}, {})

    def test_referenced_data_change_is_rejected(self):
        target = {"name": "literal", "section": ".rdata#0", "value": 0,
                  "data": {"section": ".rdata#0", "value": 0,
                            "bytes_hex": "6f6c6400"}}
        section = {"id": ".text#0", "name": ".text", "characteristics": 32,
                   "raw_size": 4, "executable": True, "raw_bytes_hex": "e800000000",
                   "relocations": [{"offset": 1, "type": 6, "target": target}]}
        before = {"schema": 1, "kind": "coff-candidate-neutrality",
                  "sections": [section], "assertion_metadata": []}
        changed_target = dict(target, data=dict(target["data"], bytes_hex="6e657700"))
        changed = dict(before, sections=[dict(section, relocations=[dict(
            section["relocations"][0], target=changed_target)])])
        self.assertFalse(compare_snapshots(before, changed)["ok"])

    def test_bss_without_file_backing_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bss.obj"
            path.write_bytes(_minimal_object_with_bss())
            snapshot = capture_object(str(path))
        names = [section["name"] for section in snapshot["sections"]]
        self.assertEqual(names, [".text", ".bss"])
        self.assertTrue(compare_snapshots(snapshot, snapshot)["ok"])

    def test_initialized_section_without_file_backing_still_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.obj"
            raw = bytearray(_minimal_object_with_bss())
            # Reclassify .bss as initialized data: PointerToRawData == 0 is now
            # a genuine truncation and must be rejected.
            struct.pack_into("<I", raw, 20 + 40 + 36, 0xC0300040)
            path.write_bytes(bytes(raw))
            with self.assertRaises(GuardError):
                capture_object(str(path))


if __name__ == "__main__":
    unittest.main()
