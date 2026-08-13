#!/usr/bin/env python3
"""Unit tests for the dashboard's floor/current VC71 score merge.

The report generator reads two score files: the committed raise-only floor
(``tools/verify/vc71_scores.json``, always complete, possibly stale) and the
honest current snapshot (``tools/verify/vc71_current.json``, present truth,
only as complete as its last populate pass).  Picking one wholesale means a
partial current snapshot blanks every function outside it -- the bug these
tests pin shut.
"""

import contextlib
import importlib.util
import io
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("generate_decomp_report.py")
SPEC = importlib.util.spec_from_file_location("generate_decomp_report",
                                              MODULE_PATH)
report = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(report)


def merge(floor, current, current_exists=True):
    """Call the merge with stderr swallowed; return the merged scores dict."""
    with contextlib.redirect_stderr(io.StringIO()):
        return report.merge_vc71_scores(floor, current,
                                        current_exists=current_exists)["scores"]


def doc(scores, version=2):
    return {"version": version, "scores": scores}


class TestMergeVc71Scores(unittest.TestCase):

    FLOOR = doc({"a": {"score": 10.0}, "b": {"score": 20.0}})

    def test_current_wins_where_it_measured(self):
        merged = merge(self.FLOOR, doc({"b": {"score": 99.0}}))
        self.assertEqual(merged["b"]["score"], 99.0)

    def test_partial_current_does_not_shadow_the_floor(self):
        """The regression this merge exists for: a one-TU refresh used to blank
        every other function on the dashboard."""
        merged = merge(self.FLOOR, doc({"b": {"score": 99.0}}))
        self.assertEqual(sorted(merged), ["a", "b"])
        self.assertEqual(merged["a"]["score"], 10.0)

    def test_current_may_lower_a_score(self):
        """Current is present truth.  The floor is raise-only, current is not:
        a real regression must be visible on the dashboard."""
        self.assertEqual(merge(self.FLOOR, doc({"a": {"score": 1.0}}))["a"]["score"],
                         1.0)

    def test_empty_current_falls_back_to_the_whole_floor(self):
        self.assertEqual(merge(self.FLOOR, doc({})), self.FLOOR["scores"])

    def test_missing_current_file(self):
        self.assertEqual(merge(self.FLOOR, {}, current_exists=False),
                         self.FLOOR["scores"])

    def test_missing_floor_file(self):
        merged = merge({}, doc({"b": {"score": 99.0}}))
        self.assertEqual(sorted(merged), ["b"])

    def test_both_missing(self):
        self.assertEqual(merge({}, {}, current_exists=False), {})

    def test_null_scores_key_is_not_a_crash(self):
        """Both writers can emit `scores: null` on a failed pass."""
        self.assertEqual(merge({"scores": None}, {"scores": None}), {})

    def test_warns_only_when_current_exists_but_is_empty(self):
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            report.merge_vc71_scores(self.FLOOR, doc({}), current_exists=True)
        self.assertIn("no scores", buf.getvalue())

        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            report.merge_vc71_scores(self.FLOOR, {}, current_exists=False)
        self.assertEqual(buf.getvalue(), "")

    def test_reports_the_mixed_provenance_of_a_scoped_refresh(self):
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            report.merge_vc71_scores(self.FLOOR, doc({"b": {"score": 99.0}}))
        self.assertIn("1 current + 1 from the committed floor", buf.getvalue())

    def test_inputs_are_not_mutated(self):
        floor = doc({"a": {"score": 10.0}})
        merge(floor, doc({"b": {"score": 99.0}}))
        self.assertEqual(sorted(floor["scores"]), ["a"])


if __name__ == "__main__":
    unittest.main()
