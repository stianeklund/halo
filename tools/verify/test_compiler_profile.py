#!/usr/bin/env python3
"""Unit tests for compiler-profile corpus bookkeeping."""

import json
import tempfile
import unittest
from pathlib import Path

from compiler_profile import load_corpus, parse_case, score_case, summarize


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

    def test_load_corpus_requires_versioned_case_and_profile_lists(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "corpus.json"
            path.write_text(json.dumps({
                "schema": 1,
                "cases": ["src/halo/test.c:target"],
                "profiles": ["/O2", "/O1"],
            }), encoding="utf-8")
            self.assertEqual(load_corpus(path),
                             (["src/halo/test.c:target"], ["/O2", "/O1"]))
        cases, profiles = load_corpus(
            Path(__file__).with_name("compiler_profile_corpus.json"))
        self.assertGreaterEqual(len(cases), 3)
        self.assertIn("/O2", profiles)


if __name__ == "__main__":
    unittest.main()
