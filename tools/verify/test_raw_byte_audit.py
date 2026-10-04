#!/usr/bin/env python3
"""Focused tests for strict raw-byte audit COFF extraction."""

import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("raw_byte_audit", HERE / "raw_byte_audit.py")
raw = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(raw)


def coff(code, symbols, relocations=(), include_aux=True):
    """Build a small i386 .text COFF object for extraction tests."""
    text_offset = 60
    reloc_offset = text_offset + len(code)
    symbol_offset = reloc_offset + 10 * len(relocations)
    symbol_count = len(symbols) * (2 if include_aux else 1)
    string_table = b"\x04\x00\x00\x00"
    header = struct.pack("<HHIIIHH", 0x14C, 1, 0, symbol_offset, symbol_count, 0, 0)
    section = struct.pack("<8sIIIIIIHHI", b".text\0\0\0", 0, 0, len(code), text_offset,
                          reloc_offset if relocations else 0, 0, len(relocations), 0, 0x60000020)
    reloc = b"".join(struct.pack("<IIH", offset, 0, 6) for offset in relocations)
    symbol_data = b"".join(
        struct.pack("<8sIhHBB", name.encode().ljust(8, b"\0"), value,
                    1, raw.IMAGE_SYM_DTYPE_FUNCTION, 2, 1 if include_aux else 0)
        + (struct.pack("<IIIIH", 0, size, 0, 0, 0) if include_aux else b"")
        for name, value, size in symbols)
    return header + section + code + reloc + symbol_data + string_table


class TestExtractCoffFunction(unittest.TestCase):
    def _extract(self, image, function):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "candidate.obj"
            path.write_bytes(image)
            return raw.extract_coff_function(path, function)

    def test_exact_function_span_ends_at_next_function(self):
        code, provenance = self._extract(
            coff(b"\x55\xc3\x90\xc3", [("target", 0, 2), ("next", 2, 2)]), "target")
        self.assertEqual(code, b"\x55\xc3")
        self.assertEqual(provenance["coff_offset"], 0)
        self.assertEqual(provenance["coff_end"], 2)

    def test_relocation_inside_target_is_not_comparable(self):
        with self.assertRaisesRegex(raw.NotComparable, "relocation"):
            self._extract(coff(b"\xe8\0\0\0\0\xc3", [("target", 0, 6)], [1]), "target")

    def test_relocation_after_target_does_not_change_target_bytes(self):
        code, _ = self._extract(
            coff(b"\xc3\xe8\0\0\0\0", [("target", 0, 1), ("next", 1, 5)], [2]), "target")
        self.assertEqual(code, b"\xc3")

    def test_missing_symbol_is_not_comparable(self):
        with self.assertRaisesRegex(raw.NotComparable, "0 COFF function symbols"):
            self._extract(coff(b"\xc3", [("other", 0, 1)]), "target")

    def test_single_function_section_without_aux_size_is_exact_extent(self):
        code, provenance = self._extract(
            coff(b"\x55\xc3", [("target", 0, 2)], include_aux=False), "target")
        self.assertEqual(code, b"\x55\xc3")
        self.assertEqual(provenance["coff_end"], 2)

    def test_eligible_functions_flattens_address_and_object_records(self):
        old_root = raw.ROOT
        try:
            with tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                source = root / "src/halo/test.c"
                source.parent.mkdir(parents=True)
                source.write_text("void target(void) {}\n", encoding="utf-8")
                (root / "kb.json").write_text(json.dumps({
                    "0x1000": {"addr": "0x1000", "ported": True, "name": "target",
                               "source_path": "src/halo/test.c"},
                    "objects": [{"functions": [{"addr": "0x1000", "ported": True,
                                                   "name": "target", "source_path": "src/halo/test.c"}]}]
                }), encoding="utf-8")
                raw.ROOT = root
                items = raw._eligible_functions()
        finally:
            raw.ROOT = old_root
        self.assertEqual([(item["address"], item["function"]) for item in items], [(0x1000, "target")])

    def test_summary_aggregates_verdicts_and_unaudited_bytes(self):
        records = [
            {"verdict": "raw-byte exact", "address": "0x00001000", "function": "exact",
             "reference": {"length": 4}},
            {"verdict": "bytes differ", "address": "0x00001004", "function": "diff",
             "reference": {"length": 6}},
            {"verdict": "not comparable", "address": "0x0000100a", "function": "nc",
             "reference": {"length": 3}},
        ]
        eligible = [{"address": 0x100d, "function": "un audited", "source": Path("source.c")}]
        old = raw.xref.function_extent
        raw.xref.function_extent = lambda address: (address + 5, "auto", "table")
        try:
            with tempfile.TemporaryDirectory() as temp:
                raw.ROOT = Path(temp)
                summary = raw._write_summary(records, ["populate"], 1, 0, eligible)
        finally:
            raw.xref.function_extent = old
        self.assertEqual(summary["totals"]["raw-byte exact"]["functions"], 1)
        self.assertEqual(summary["totals"]["bytes differ"]["original_bytes"], 6)
        self.assertEqual(summary["totals"]["not comparable"]["functions"], 1)
        self.assertEqual(summary["totals"]["not audited"]["original_bytes"], 5)


class SymbolDecoration(unittest.TestCase):
    def names(self, *symbols):
        return [{"name": symbol} for symbol in symbols]

    def test_leading_underscore_c_name_does_not_collide_with_its_sibling(self):
        matches = self.names("__TIFFVSetField", "_TIFFVSetField")
        self.assertTrue(all(raw._same_symbol(m["name"], "_TIFFVSetField") for m in matches))
        self.assertEqual(raw.prefer_exact_decoration(matches, "_TIFFVSetField"),
                         [{"name": "__TIFFVSetField"}])


if __name__ == "__main__":
    unittest.main()
