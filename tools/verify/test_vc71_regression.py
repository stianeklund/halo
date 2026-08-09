#!/usr/bin/env python3
"""Unit tests for vc71_regression check-mode evidence gates."""

import importlib.util
import json
import subprocess
import sys
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


class TestFuncSpanDelegatesWhenImported(unittest.TestCase):
    """_func_span must reach vc71_verify however this module was imported.

    Regression test for the import-path bug: `from vc71_verify import _func_span`
    resolves only when sys.path[0] is tools/verify (the __main__/CLI path).  When
    tools/recovery/source_recovery.py imports this as `tools.verify.vc71_regression`
    with only the repo root on sys.path, that raised ImportError and _func_span
    silently fell back to the kb.json listing gap -- which overshoots wherever the
    listing has a hole, so a correct short reference reads as truncated and the
    function is reported "invalid delinked reference".  Observed on the hud_weapon
    recovery run: FUN_000d8b70/FUN_000d8b80 scored a 16-byte kb gap against a true
    span of 1, failing a check the CLI passed.

    The delegated answer now comes from the committed bounds table
    (tools/verify/function_bounds.json), which is also where the reference itself
    is cut from -- so a gate that disagrees with vc71_verify about a function's
    size is disagreeing about which bytes were scored.

    Delegation is asserted via sys.modules rather than a span value, so the test
    does not depend on kb.json contents: if the delegating import had failed,
    vc71_verify would never be loaded at all.  The values are then pinned against
    the table directly, which catches the subtler failure where both sides
    delegate but the table has stopped being the authority.
    """

    def test_import_as_package_module_still_delegates(self):
        repo_root = Path(__file__).resolve().parent.parent.parent
        probe = (
            "import sys\n"
            "sys.path.insert(0, %r)\n"
            "from tools.verify import vc71_regression as vr\n"
            "assert 'vc71_verify' not in sys.modules, 'precondition: not yet loaded'\n"
            "vr._func_span('FUN_00000000')\n"
            "assert 'vc71_verify' in sys.modules, "
            "'_func_span fell back to the kb gap instead of delegating'\n"
            "import vc71_verify\n"
            "for fn in ('FUN_000d8b70', 'FUN_000d8b80'):\n"
            "    assert vr._func_span(fn) == vc71_verify._func_span(fn), fn\n"
            "print('ok')\n"
        ) % str(repo_root)
        proc = subprocess.run(
            [sys.executable, "-c", probe],
            cwd=str(repo_root), capture_output=True, text=True,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_span_is_the_committed_bound(self):
        """Both sides must report end-start from function_bounds.json."""
        repo_root = Path(__file__).resolve().parent.parent.parent
        sys.path.insert(0, str(repo_root / "tools" / "verify"))
        sys.path.insert(0, str(repo_root / "tools" / "audit"))
        import function_bounds as fb
        import vc71_verify as vv
        table = fb.load_table()
        # A listing-gap case (kb gap 800 for a 102-byte function) and a thunk,
        # the two shapes the old kb-gap rule got wrong in opposite directions.
        for addr in (0x15C2D0, 0x18E300, 0x1A0680):
            entry = table.get(hex(addr))
            self.assertIsNotNone(entry, f"0x{addr:x} missing from the table")
            want = int(entry["end"], 16) - addr
            self.assertEqual(vv._func_span(f"FUN_{addr:08x}"), want)
            self.assertEqual(vc71._func_span(f"FUN_{addr:08x}"), want)


if __name__ == "__main__":
    unittest.main()
