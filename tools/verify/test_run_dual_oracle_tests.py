#!/usr/bin/env python3
"""Unit tests for dual-oracle runtime runner argument plumbing."""

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


MODULE_PATH = Path(__file__).with_name("run_dual_oracle_tests.py")
sys.path.insert(0, str(MODULE_PATH.parent))
SPEC = importlib.util.spec_from_file_location("run_dual_oracle_tests", MODULE_PATH)
dual_oracle = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dual_oracle)


class TestDualOracleDeploymentArguments(unittest.TestCase):
    def test_main_passes_deployment_namespace(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            artifact_root = Path(temp_dir)
            captured = {}

            def deploy(label, artifact_dir, args):
                captured["label"] = label
                captured["artifact_dir"] = artifact_dir
                captured["args"] = args

            result = {"parsed": {"assertions": {"fail": 0}}}
            argv = [
                "run_dual_oracle_tests.py",
                "--target", "scalars_interpolate",
                "--run-id", "argument-contract",
                "--skip-build",
                "--xbox-host", "192.0.2.10",
                "--backend", "xbdm",
            ]
            with patch.object(dual_oracle, "ARTIFACT_ROOT", artifact_root), \
                 patch.object(dual_oracle, "write_overlay"), \
                 patch.object(dual_oracle, "build_dual_oracle"), \
                 patch.object(dual_oracle, "deploy_variant", side_effect=deploy), \
                 patch.object(dual_oracle, "capture_output", return_value=result), \
                 patch.object(dual_oracle, "restore_harness_off", return_value={"ok": True}) as restore, \
                 patch.object(sys, "argv", argv):
                self.assertEqual(dual_oracle.main(), 0)

            args = captured["args"]
            self.assertEqual(captured["label"], "dual_oracle")
            self.assertEqual(captured["artifact_dir"], artifact_root / "argument-contract")
            self.assertTrue(args.skip_build)
            self.assertFalse(args.skip_deploy)
            self.assertEqual(args.xbox_host, "192.0.2.10")
            self.assertFalse(args.xemu)
            self.assertEqual(args.backend, "xbdm")
            restore.assert_called_once_with(
                artifact_root / "argument-contract", True, False, "192.0.2.10")


if __name__ == "__main__":
    unittest.main()
