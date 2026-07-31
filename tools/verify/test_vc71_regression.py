#!/usr/bin/env python3
"""Unit tests for vc71_regression check-mode evidence gates."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


MODULE_PATH = Path(__file__).with_name("vc71_regression.py")
SPEC = importlib.util.spec_from_file_location("vc71_regression", MODULE_PATH)
vc71 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vc71)


SOURCE = "tools/verify/test_vc71_regression.py"


class TestStrictCheck(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.baseline_path = Path(self.temp_dir.name) / "scores.json"
        self.args = SimpleNamespace(
            source=None, threshold=100.0, quiet=True, strict=True,
        )

    def tearDown(self):
        self.temp_dir.cleanup()

    def write_baseline(self, scores):
        self.baseline_path.write_text(json.dumps({"scores": scores}))

    def run_check(self, scores, runner=None, source=None):
        self.write_baseline(scores)
        args = SimpleNamespace(
            source=source, threshold=self.args.threshold,
            quiet=self.args.quiet, strict=self.args.strict,
        )
        with patch.object(vc71, "BASELINE_PATH", self.baseline_path):
            with patch.object(vc71, "run_vc71_verify", runner or self.runner()):
                with patch.object(vc71, "_func_span", return_value=10):
                    return vc71.cmd_check(args)

    @staticmethod
    def runner(results=None, status="ok", drops=None):
        results = results or {}
        drops = drops or []

        def run(_source, drops_out=None, meta_out=None):
            if drops_out is not None:
                drops_out.extend(drops)
            if meta_out is not None:
                meta_out["status"] = status
            return results

        return run

    def test_empty_baseline_fails_strict(self):
        self.assertNotEqual(self.run_check({}), 0)

    def test_source_filter_without_matches_fails_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        self.assertNotEqual(self.run_check(scores, source=["other.c"]), 0)

    def test_missing_source_fails_strict(self):
        scores = {"fn": {"score": 90, "source": "missing.c"}}
        self.assertNotEqual(self.run_check(scores), 0)

    def test_runner_failures_fail_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        for status in ("subprocess_failed", "compile_failed", "parse_failed"):
            with self.subTest(status=status):
                self.assertNotEqual(
                    self.run_check(scores, self.runner(status=status)), 0
                )

    def test_runner_exception_fails_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}

        def fail(*_args, **_kwargs):
            raise OSError("compiler unavailable")

        self.assertNotEqual(self.run_check(scores, fail), 0)

    def test_missing_parsed_function_and_zero_checked_fail(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        self.assertNotEqual(self.run_check(scores, self.runner()), 0)

    def test_invalid_reference_fails_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        result = {"fn": {"score": 90, "n_r": 0}}
        self.assertNotEqual(self.run_check(scores, self.runner(result)), 0)

    def test_truncated_reference_fails_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        result = {"fn": {"score": 90, "n_r": 1}}
        with patch.object(vc71, "_func_span", return_value=100):
            self.write_baseline(scores)
            with patch.object(vc71, "BASELINE_PATH", self.baseline_path):
                with patch.object(vc71, "run_vc71_verify", self.runner(result)):
                    self.assertNotEqual(vc71.cmd_check(self.args), 0)

    def test_missing_reference_drop_fails_strict(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        drop = {"function": "fn", "reason": "missing", "span_bytes": 10}
        result = {"fn": {"score": 90, "n_r": 10}}
        self.assertNotEqual(
            self.run_check(scores, self.runner(result, drops=[drop])), 0
        )

    def test_improvement_passes_and_any_regression_fails(self):
        scores = {"fn": {"score": 90, "source": SOURCE}}
        improved = {"fn": {"score": 91, "n_r": 10}}
        regressed = {"fn": {"score": 89.9, "n_r": 10}}
        self.assertEqual(self.run_check(scores, self.runner(improved)), 0)
        self.assertNotEqual(self.run_check(scores, self.runner(regressed)), 0)

    def test_strict_flag_is_explicit_parser_option(self):
        args = vc71.build_parser().parse_args(["check", "--strict"])
        self.assertTrue(args.strict)


class TestLegacyCheckSkipsEvidenceGaps(unittest.TestCase):
    def test_empty_baseline_and_missing_result_still_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            baseline = Path(temp) / "scores.json"
            baseline.write_text(json.dumps({"scores": {}}))
            args = SimpleNamespace(source=None, threshold=2.0, quiet=True, strict=False)
            with patch.object(vc71, "BASELINE_PATH", baseline):
                self.assertEqual(vc71.cmd_check(args), 0)

            baseline.write_text(json.dumps({
                "scores": {"fn": {"score": 90, "source": SOURCE}},
            }))
            runner = TestStrictCheck.runner()
            with patch.object(vc71, "BASELINE_PATH", baseline):
                with patch.object(vc71, "run_vc71_verify", runner):
                    with patch.object(vc71, "_func_span", return_value=10):
                        self.assertEqual(vc71.cmd_check(args), 0)


if __name__ == "__main__":
    unittest.main()
