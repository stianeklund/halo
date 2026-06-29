#!/usr/bin/env python3
"""Offline contract tests for ab_check's deploy + liveness gate (no box needed).

The whole point of the gate is that a failed deploy or a stale/unpatched running
image must produce exit 2 (INCONCLUSIVE) and run NO behavioral diff -- a wrong
CLEAN/DIVERGENT verdict on an unverified build is worse than the gap. These tests
monkeypatch deploy_candidate / liveness_gate / capture_run so they run in
milliseconds with no xemu, locking that contract for the CI tripwire.

    python3 -m unittest tools.equivalence.test_ab_check
"""
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))
import ab_check  # noqa: E402


def _argv(out_dir):
    return ["--level", "a10", "--scenario", "x", "--out-dir", out_dir]


def _boom(*a, **k):
    raise AssertionError("capture_run ran — the abort path did NOT short-circuit")


class AbCheckGateContract(unittest.TestCase):
    def setUp(self):
        # Save/restore the module hooks we monkeypatch.
        self._saved = (ab_check.deploy_candidate, ab_check.liveness_gate,
                       ab_check.capture_run)
        ab_check.capture_run = _boom  # any capture in these tests is a failure

    def tearDown(self):
        (ab_check.deploy_candidate, ab_check.liveness_gate,
         ab_check.capture_run) = self._saved

    def test_deploy_failure_is_inconclusive_and_skips_capture_and_gate(self):
        ab_check.deploy_candidate = lambda host, args: 1
        ab_check.liveness_gate = lambda host, port: (_ for _ in ()).throw(
            AssertionError("gate ran after a failed deploy"))
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(ab_check.main(_argv(d)), 2)

    def test_gate_failure_is_inconclusive_and_skips_capture(self):
        ab_check.deploy_candidate = lambda host, args: 0
        ab_check.liveness_gate = lambda host, port: 1
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(ab_check.main(_argv(d)), 2)

    def test_gate_qmp_unreachable_rc2_is_inconclusive(self):
        # verify_toggles_live returns 2 when QMP is unreachable; still an abort.
        ab_check.deploy_candidate = lambda host, args: 0
        ab_check.liveness_gate = lambda host, port: 2
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(ab_check.main(_argv(d)), 2)


if __name__ == "__main__":
    unittest.main()
