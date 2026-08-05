#!/usr/bin/env python3
"""Unit tests for tools/verify/score_improve.py."""

import importlib.util
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("score_improve.py")
SPEC = importlib.util.spec_from_file_location("score_improve", MODULE_PATH)
score_improve = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(score_improve)


def score(value, warnings=None):
    return {
        "score": value,
        "operand_score": value,
        "candidate_instructions": 10,
        "reference_instructions": 10,
        "warnings": warnings or {},
        "categories": [],
    }


class TestClassifyContext(unittest.TestCase):
    def test_classifies_explicit_diagnostics(self):
        context = {
            "warnings": {"fpu": ["difference"], "loadw": ["difference"]},
            "frame": {"cand_frame_bytes": 12, "ref_frame_bytes": 8},
            "scores": {"n_cand_insns": 10, "n_ref_insns": 9},
        }
        self.assertEqual(
            score_improve.classify_context(context, regparm=True),
            ["fpu_operand_order", "frame_layout", "instruction_count",
             "load_width", "register_abi"],
        )

    def test_classifies_clean_context_as_codegen_shape(self):
        self.assertEqual(score_improve.classify_context({}), ["codegen_shape"])


class TestCompare(unittest.TestCase):
    def baseline(self):
        return {"scores": {"target": score(80), "neighbor": score(90)}}

    def test_accepts_target_improvement_without_neighbor_regression(self):
        current = {"scores": {"target": score(80.5), "neighbor": score(90)}}
        report = score_improve.compare(self.baseline(), current, {"target"}, 0.01)
        self.assertTrue(report["passed"])
        self.assertEqual(report["improvements"], {"target": 0.5})

    def test_rejects_neutral_target(self):
        current = {"scores": {"target": score(80), "neighbor": score(90)}}
        report = score_improve.compare(self.baseline(), current, {"target"}, 0.01)
        self.assertFalse(report["passed"])
        self.assertIn("target", report["score_regressions"])

    def test_rejects_neighbor_score_or_warning_regression(self):
        current = {
            "scores": {
                "target": score(80.5),
                "neighbor": score(89.9, {"fpu": ["new warning"]}),
            }
        }
        report = score_improve.compare(self.baseline(), current, {"target"}, 0.01)
        self.assertFalse(report["passed"])
        self.assertIn("neighbor", report["score_regressions"])
        self.assertIn("neighbor", report["warning_regressions"])

    def test_rejects_missing_baseline_score(self):
        current = {"scores": {"target": score(80.5)}}
        report = score_improve.compare(self.baseline(), current, {"target"}, 0.01)
        self.assertFalse(report["passed"])
        self.assertEqual(report["missing"], ["neighbor"])


class TestParser(unittest.TestCase):
    def test_categorize_does_not_require_worktree(self):
        args = score_improve.build_parser().parse_args(
            ["categorize", "--context", "context.json"]
        )
        self.assertFalse(hasattr(args, "worktree"))


if __name__ == "__main__":
    unittest.main()
