#!/usr/bin/env python3
"""Offline tests for symbolize_exception.py."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "xbox"))
import symbolize_exception as s  # noqa: E402


class ParseContract(unittest.TestCase):
    def test_extracts_registers_and_frames_without_all_hex_noise(self):
        text = """EXCEPTION halt in c:\\halo\\SOURCE\\tag_files\\tag_groups.c,#3089
EIP=0x0072a2ac EAX=0x00010000
[0] 0x0072a2ac
[1] 72a2b0
index 0x10000 count 1
"""
        hits = s.extract_address_hits(text)
        got = [(hit.label, hit.address) for hit in hits]
        self.assertIn(("EIP", 0x72A2AC), got)
        self.assertIn(("EAX", 0x10000), got)
        self.assertIn(("[0]", 0x72A2AC), got)
        self.assertIn(("[1]", 0x72A2B0), got)
        self.assertNotIn(("hex", 0x10000), got)

    def test_all_hex_includes_generic_hex_tokens(self):
        hits = s.extract_address_hits("index 0x10000", all_hex=True)
        self.assertEqual([(hit.label, hit.address) for hit in hits], [("hex", 0x10000)])


class SymbolContract(unittest.TestCase):
    def test_compiled_address_uses_nearest_export(self):
        pe = s.PeSymbolIndex(
            0x642000,
            [
                s.ExportSymbol(0x642100, 0x100, "_first@4", "first"),
                s.ExportSymbol(0x642200, 0x200, "_second@4", "second"),
            ],
            [s.PeSection(".text", 0x642000, 0x643000, True)],
        )
        result = s.resolve_address(0x642123, pe, None, 0x642000)
        self.assertEqual(result["space"], "compiled")
        self.assertEqual(result["symbol"], "first")
        self.assertEqual(result["offset"], 0x23)
        self.assertEqual(result["confidence"], "nearest-export")

    def test_original_address_uses_kb_nearest_start(self):
        kb = s.KbSymbolIndex.from_data(
            {
                "objects": [
                    {
                        "name": "actions.obj",
                        "functions": [
                            {
                                "addr": "0x1c270",
                                "decl": "char *encounter_get_squad(char *encounter, int16_t squad_index);",
                                "ported": True,
                            }
                        ],
                    }
                ]
            }
        )
        result = s.resolve_address(0x1C28A, None, kb, 0x642000)
        self.assertEqual(result["space"], "original")
        self.assertEqual(result["symbol"], "encounter_get_squad")
        self.assertEqual(result["offset"], 0x1A)
        self.assertEqual(result["object"], "actions.obj")

    def test_normalizes_stdcall_and_fastcall_exports(self):
        self.assertEqual(s.normalize_export_name("_foo@12"), "foo")
        self.assertEqual(s.normalize_export_name("@bar@8"), "bar")


if __name__ == "__main__":
    unittest.main()
