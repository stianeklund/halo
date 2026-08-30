#!/usr/bin/env python3
"""Unit tests for compiler-profile corpus bookkeeping."""

import unittest

from compiler_profile import parse_case, score_case, summarize


class CompilerProfileTests(unittest.TestCase):
    def test_parse_case_requires_source_and_function(self):
        self.assertEqual(parse_case("src/halo/test.c:target"),
                         ("src/halo/test.c", "target"))
        for malformed in ("target", "src/halo/test.c:", "src/halo/test.h:target"):
            with self.subTest(malformed=malformed):
                with self.assertRaises(ValueError):
                    parse_case(malformed)

    def test_summary_ignores_unmeasured_rows(self):
        report = summarize([
            {"opt": "/O2", "official_pct": 90.0, "dp_lcs_pct": 92.0},
            {"opt": "/O2", "official_pct": 94.0, "dp_lcs_pct": 94.0},
            {"opt": "/O1", "error": "compiler unavailable"},
        ])
        self.assertEqual(report["/O2"]["cases"], 2)
        self.assertEqual(report["/O2"]["official_mean"], 92.0)
        self.assertNotIn("/O1", report)

    def test_missing_corpus_source_never_reuses_a_stale_score_context(self):
        result = score_case("src/halo/no_such_corpus_case.c", "target", "/O2")
        self.assertEqual(result["error"], "corpus source does not exist")


if __name__ == "__main__":
    unittest.main()
