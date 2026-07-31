"""Standard-library tests for the source recovery manifest workflow."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.recovery import source_recovery as recovery


class SourceRecoveryTests(unittest.TestCase):
    def test_script_path_bootstraps_repo_imports(self):
        script = recovery.ROOT / "tools" / "recovery" / "source_recovery.py"
        result = subprocess.run(
            [sys.executable, str(script), "--self-test"],
            cwd=recovery.ROOT,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("recovery guard imports", result.stdout)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=recovery.ROOT)
        self.root = Path(self.temp.name)
        self.source = self.root / "sample.c"
        self.source.write_text(
            "int f(void) {\n"
            "  int local_4;\n"
            "  FUN_00123456();\n"
            "  XCALL(foo);\n"
            "  return *(int *)(base + 0x10) + *(int *)0x12345678 + 7;\n"
            "}\n", encoding="utf-8")
        self.obj = self.root / "sample.obj"
        self.obj.write_bytes(b"candidate")
        self.manifest_path = self.root / "manifest.json"

    def tearDown(self):
        self.temp.cleanup()

    def _manifest(self):
        with self.manifest_path.open(encoding="utf-8") as stream:
            return json.load(stream)

    def _plan(self):
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"):
            manifest = recovery._plan(str(self.source))
        recovery._write_atomic(self.manifest_path, manifest)
        return manifest

    def test_deterministic_planning_and_categories(self):
        first = self._plan()
        second = self._plan()
        self.assertEqual(first, second)
        self.assertGreater(len(first["inventory"]["fun_calls"]), 0)
        self.assertGreater(len(first["inventory"]["absolute_address_dereferences"]), 0)
        self.assertGreater(len(first["inventory"]["raw_base_offset_dereferences"]), 0)
        self.assertTrue(all(item["status"] == "pending" for item in first["items"]))

    def test_stale_source_rejected(self):
        self._plan()
        self.source.write_text("changed\n", encoding="utf-8")
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"):
            with self.assertRaises(recovery.RecoveryError):
                recovery._capture(self.manifest_path, self._manifest(), str(self.obj))

    def test_missing_capture_rejected(self):
        self._plan()
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"):
            result = recovery._check(self.manifest_path, self._manifest(), str(self.obj), True)
        self.assertFalse(result["ok"])
        self.assertIn("no captured baseline", result["failures"])

    def test_unchanged_capture_check_success_with_mocked_vc71(self):
        self._plan()
        manifest = self._manifest()
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"), \
                mock.patch("tools.recovery.coff_candidate_guard.capture_object", return_value={"schema": 1, "kind": "coff-candidate-neutrality", "sections": [], "assertion_metadata": []}), \
                mock.patch("tools.recovery.assert_metadata_guard.capture_sources", return_value={"schema": 1, "kind": "assert-metadata", "assertions": []}), \
                mock.patch("tools.recovery.coff_candidate_guard.compare_snapshots", return_value={"ok": True, "errors": []}), \
            mock.patch("tools.recovery.assert_metadata_guard.compare_snapshots", return_value={"ok": True, "errors": []}), \
                mock.patch.object(recovery, "_run_vc71", return_value=0):
            recovery._capture(self.manifest_path, manifest, str(self.obj))
            recovery._write_atomic(self.manifest_path, manifest)
            result = recovery._check(self.manifest_path, self._manifest(), str(self.obj), False)
        self.assertTrue(result["ok"], repr(result))

    def test_changed_source_and_object_reach_guards_and_record_current_state(self):
        self._plan()
        manifest = self._manifest()
        valid_coff = {"schema": 1, "kind": "coff-candidate-neutrality", "sections": [], "assertion_metadata": []}
        valid_assertion = {"schema": 1, "kind": "assert-metadata", "assertions": []}
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"), \
                mock.patch("tools.recovery.coff_candidate_guard.capture_object", return_value=valid_coff), \
                mock.patch("tools.recovery.assert_metadata_guard.capture_sources", return_value=valid_assertion):
            recovery._capture(self.manifest_path, manifest, str(self.obj))
            recovery._write_atomic(self.manifest_path, manifest)
        self.source.write_text("int changed(void) { return 1; }\n", encoding="utf-8")
        self.obj.write_bytes(b"rebuilt-with-different-metadata")
        with mock.patch.object(recovery, "_git_head", return_value="edited-head"), \
                mock.patch("tools.recovery.coff_candidate_guard.capture_object", return_value=valid_coff), \
                mock.patch("tools.recovery.assert_metadata_guard.capture_sources", return_value=valid_assertion), \
                mock.patch("tools.recovery.coff_candidate_guard.compare_snapshots", return_value={"ok": True, "errors": []}) as coff_compare, \
                mock.patch("tools.recovery.assert_metadata_guard.compare_snapshots", return_value={"ok": True, "errors": []}), \
                mock.patch.object(recovery, "_run_vc71", return_value=0):
            result = recovery._check(self.manifest_path, self._manifest(), str(self.obj), False)
        self.assertTrue(result["ok"], repr(result))
        self.assertEqual(result["current_git_head"], "edited-head")
        self.assertEqual(result["current_source_sha256"], recovery._sha256(self.source))
        self.assertEqual(result["baseline_object_sha256"], manifest["baseline"]["object_sha256"])
        self.assertEqual(result["current_object_sha256"], recovery._sha256(self.obj))
        self.assertNotEqual(result["baseline_object_sha256"], result["current_object_sha256"])
        self.assertTrue(coff_compare.called)

    def test_corrective_coff_delta_is_observed_and_requires_vc71(self):
        self._plan()
        manifest = self._manifest()
        valid_coff = {"schema": 1, "kind": "coff-candidate-neutrality", "sections": [], "assertion_metadata": []}
        valid_assertion = {"schema": 1, "kind": "assert-metadata", "assertions": []}
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"), \
                mock.patch("tools.recovery.coff_candidate_guard.capture_object", return_value=valid_coff), \
                mock.patch("tools.recovery.assert_metadata_guard.capture_sources", return_value=valid_assertion):
            recovery._capture(self.manifest_path, manifest, str(self.obj))
            recovery._write_atomic(self.manifest_path, manifest)
        self.obj.write_bytes(b"corrective-object")
        coff_delta = {"ok": False, "errors": ["code bytes changed"]}
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"), \
                mock.patch("tools.recovery.coff_candidate_guard.capture_object", return_value=valid_coff), \
                mock.patch("tools.recovery.assert_metadata_guard.capture_sources", return_value=valid_assertion), \
                mock.patch("tools.recovery.coff_candidate_guard.compare_snapshots", return_value=coff_delta), \
                mock.patch("tools.recovery.assert_metadata_guard.compare_snapshots", return_value={"ok": True, "errors": []}), \
                mock.patch.object(recovery, "_run_vc71", return_value=0) as vc71:
            result = recovery._check(self.manifest_path, self._manifest(), str(self.obj), False, "corrective")
        self.assertTrue(result["ok"], repr(result))
        self.assertEqual(result["mode"], "corrective")
        self.assertEqual(result["observations"]["changes"], ["COFF: code bytes changed"])
        vc71.assert_called_once()

    def test_assertion_delta_blocks_both_modes_and_vc71_still_runs(self):
        self._plan()
        manifest = self._manifest()
        valid_coff = {"schema": 1, "kind": "coff-candidate-neutrality", "sections": [], "assertion_metadata": []}
        valid_assertion = {"schema": 1, "kind": "assert-metadata", "assertions": []}
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"), \
                mock.patch("tools.recovery.coff_candidate_guard.capture_object", return_value=valid_coff), \
                mock.patch("tools.recovery.assert_metadata_guard.capture_sources", return_value=valid_assertion):
            recovery._capture(self.manifest_path, manifest, str(self.obj))
            recovery._write_atomic(self.manifest_path, manifest)
        for mode in ("neutral", "corrective"):
            with mock.patch.object(recovery, "_git_head", return_value="deadbeef"), \
                    mock.patch("tools.recovery.coff_candidate_guard.capture_object", return_value=valid_coff), \
                    mock.patch("tools.recovery.assert_metadata_guard.capture_sources", return_value=valid_assertion), \
                    mock.patch("tools.recovery.coff_candidate_guard.compare_snapshots", return_value={"ok": True, "errors": []}), \
                    mock.patch("tools.recovery.assert_metadata_guard.compare_snapshots", return_value={"ok": False, "errors": ["assertion metadata changed"]}), \
                    mock.patch.object(recovery, "_run_vc71", return_value=0) as vc71:
                result = recovery._check(self.manifest_path, self._manifest(), str(self.obj), False, mode)
            self.assertFalse(result["ok"], repr(result))
            self.assertIn("assertion: assertion metadata changed", result["failures"])
            vc71.assert_called_once()

    def test_coff_and_assertion_divergence_fails(self):
        self._plan()
        manifest = self._manifest()
        valid_coff = {"schema": 1, "kind": "coff-candidate-neutrality", "sections": [], "assertion_metadata": []}
        valid_assertion = {"schema": 1, "kind": "assert-metadata", "assertions": []}
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"), \
                mock.patch("tools.recovery.coff_candidate_guard.capture_object", return_value=valid_coff), \
                mock.patch("tools.recovery.assert_metadata_guard.capture_sources", return_value=valid_assertion):
            recovery._capture(self.manifest_path, manifest, str(self.obj))
            recovery._write_atomic(self.manifest_path, manifest)
            with mock.patch("tools.recovery.coff_candidate_guard.compare_snapshots", return_value={"ok": False, "errors": ["code changed"]}), \
                    mock.patch("tools.recovery.assert_metadata_guard.compare_snapshots", return_value={"ok": False, "errors": ["assertion metadata changed"]}), \
                    mock.patch.object(recovery, "_run_vc71", return_value=0) as vc71:
                result = recovery._check(self.manifest_path, self._manifest(), str(self.obj), False)
        self.assertFalse(result["ok"])
        self.assertEqual(len(result["failures"]), 2, repr(result))
        vc71.assert_called_once()

    def test_status_transitions_and_park_reason(self):
        self._plan()
        item_id = self._manifest()["items"][0]["id"]
        with self.assertRaises(recovery.RecoveryError):
            recovery._set_status(self.manifest_path, self._manifest(), item_id, "parked", None)
        recovery._set_status(self.manifest_path, self._manifest(), item_id, "parked", "needs binary evidence")
        self.assertEqual(self._manifest()["items"][0]["status"], "parked")
        recovery._set_status(self.manifest_path, self._manifest(), item_id, "applied", None)
        updated = self._manifest()
        self.assertEqual(updated["items"][0]["status"], "applied")
        inventory_copy = [item for category in updated["inventory"].values() for item in category
                          if item["id"] == item_id]
        self.assertEqual(len(inventory_copy), 1)
        self.assertEqual(inventory_copy[0]["status"], "applied")

    def test_malformed_manifest_and_report(self):
        self._plan()
        malformed = self._manifest()
        malformed["items"].append(dict(malformed["items"][0]))
        with self.assertRaises(recovery.RecoveryError):
            recovery._validate_manifest(malformed)
        malformed = self._manifest()
        item_id = malformed["items"][0]["id"]
        for category in malformed["inventory"].values():
            for item in category:
                if item["id"] == item_id:
                    item["status"] = "applied"
        with self.assertRaises(recovery.RecoveryError):
            recovery._validate_manifest(malformed)
        report = recovery._report(self._manifest())
        self.assertIn("# Source Recovery Report", report)
        self.assertIn("pending", report)

    def test_report_explains_skipped_gate_as_last_check_reason(self):
        manifest = self._plan()
        manifest["checks"].append({
            "ok": False,
            "failures": [],
            "skipped": ["vc71_regression.py check --strict"],
        })
        report = recovery._report(manifest)
        self.assertIn("- Last check: not passed (skipped gates)", report)
        self.assertIn("- Failures: none recorded", report)
        self.assertIn("- Skipped gates: vc71_regression.py check --strict", report)


if __name__ == "__main__":
    unittest.main()
