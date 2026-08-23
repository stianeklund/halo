#!/usr/bin/env python3
"""Unit tests for audit_object_provenance.py."""

import sys
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from audit.audit_object_provenance import build_report, parse_range


class ObjectProvenanceTests(unittest.TestCase):
    def test_parse_range(self):
        self.assertEqual(parse_range("draw:15d8b0-15ee80"),
                         ("draw", 0x15d8b0, 0x15ee80))

    def test_single_source_assigns_every_function_in_range(self):
        obj = {
            "name": "wrong.obj",
            "source": "wrong.c",
            "functions": [
                {"addr": "0x1000", "decl": "void FUN_00001000(void);"},
                {"addr": "0x1010", "decl": "void FUN_00001010(void);"},
                {"addr": "0x2000", "decl": "void FUN_00002000(void);"},
            ],
        }

        def probe(label, lo, hi):
            if label == "known":
                return [(4, "rasterizer\\xbox\\known.c")]
            return []

        report = build_report(obj, [("known", 0x1000, 0x1010),
                                    ("unknown", 0x2000, 0x2000)], probe)
        self.assertEqual([row["source_file"] for row in report["functions"][:2]],
                         ["rasterizer/xbox/known.c", "rasterizer/xbox/known.c"])
        self.assertEqual(report["functions"][0]["confidence"], "high")
        self.assertEqual(report["functions"][2]["confidence"], "unknown")
        self.assertEqual(len(report["attribution_mismatches"]), 2)
        self.assertFalse(report["check_passed"])

    def test_multiple_sources_stay_uncertain(self):
        obj = {"name": "mixed.obj", "functions": [
            {"addr": "0x1000", "decl": "void FUN_00001000(void);"},
        ]}
        report = build_report(
            obj, [("mixed", 0x1000, 0x1000)],
            lambda label, lo, hi: [(0, "one.c"), (4, "two.c")])
        self.assertEqual(report["functions"][0]["confidence"], "uncertain")
        self.assertEqual(report["mixed_source_ranges"], ["mixed"])
        self.assertFalse(report["check_passed"])


if __name__ == "__main__":
    unittest.main()
