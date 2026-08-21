"""Standard-library tests for the source recovery manifest workflow."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

# Run as a script (`python3 tools/recovery/test_source_recovery.py`) sys.path[0] is
# tools/recovery and the repo root is NOT on the path, so `from tools.recovery ...`
# raised ModuleNotFoundError and the whole suite errored out without running a
# single test.  Same root cause the suite's own first test guards against inside
# source_recovery.py; it just never applied to the test module itself.
_ROOT = Path(__file__).resolve().parent.parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

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

    def test_plan_assigns_ladder_categories_and_risky_opt_out_by_default(self):
        manifest = self._plan()
        self.assertIs(manifest["allow_risky"], False)
        self.assertTrue(all(item["category"] in recovery.CATEGORIES for item in manifest["items"]))
        self.assertEqual(
            {item["category"] for item in manifest["inventory"]["raw_base_offset_dereferences"]},
            {"offset-to-field"})
        self.assertEqual(
            {item["category"] for item in manifest["inventory"]["decompiler_style_locals"]},
            {"local-renames"})
        self.assertEqual({item["category"] for item in manifest["inventory"]["fun_calls"]},
                         {"symbol-names"})
        recovery._validate_manifest(self._manifest())

    def test_plan_allow_risky_flag_is_recorded(self):
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"):
            manifest = recovery._plan(str(self.source), True)
        self.assertIs(manifest["allow_risky"], True)

    def test_pre_ladder_manifest_loads_checks_and_reports(self):
        """Backward compatibility: manifests planned before the ladder existed."""
        manifest = self._plan()
        manifest.pop("allow_risky")
        for item in manifest["items"]:
            item.pop("category", None)
        for category_items in manifest["inventory"].values():
            for item in category_items:
                item.pop("category", None)
        recovery._write_atomic(self.manifest_path, manifest)
        loaded = recovery._load(str(self.manifest_path))[1]
        self.assertNotIn("allow_risky", loaded)
        self.assertTrue(all("category" not in item for item in loaded["items"]))
        self.assertEqual(recovery._ladder_warnings(loaded), [])
        self.assertEqual(recovery._risky_failures(loaded), [])
        counts = recovery._ladder_counts(loaded)
        self.assertEqual(counts["uncategorized"]["pending"], len(loaded["items"]))
        report = recovery._report(loaded)
        self.assertIn("## Ladder (mandatory order)", report)
        self.assertIn("- Risky categories: not opted in", report)
        text, code = recovery._ladder(loaded)
        self.assertEqual(code, 0)
        self.assertIn("uncategorized", text)

    def test_invalid_category_and_category_divergence_rejected(self):
        self._plan()
        malformed = self._manifest()
        malformed["items"][0]["category"] = "not-a-ladder-step"
        with self.assertRaises(recovery.RecoveryError):
            recovery._validate_manifest(malformed)
        malformed = self._manifest()
        item_id = malformed["items"][0]["id"]
        malformed["items"][0]["category"] = "comments"
        for category_items in malformed["inventory"].values():
            for item in category_items:
                if item["id"] == item_id:
                    item["category"] = "const-enum"
        with self.assertRaises(recovery.RecoveryError):
            recovery._validate_manifest(malformed)
        malformed = self._manifest()
        malformed["allow_risky"] = "yes"
        with self.assertRaises(recovery.RecoveryError):
            recovery._validate_manifest(malformed)

    def test_ladder_order_violation_warns_but_never_fails(self):
        self._plan()
        manifest = self._manifest()
        offset_item = next(item for item in manifest["items"]
                           if item["category"] == "offset-to-field")
        recovery._set_status(self.manifest_path, manifest, offset_item["id"], "applied", None)
        updated = self._manifest()
        warnings = recovery._ladder_warnings(updated)
        self.assertEqual(len(warnings), 1, warnings)
        self.assertIn("offset-to-field", warnings[0])
        self.assertIn("local-renames", warnings[0])
        text, code = recovery._ladder(updated)
        self.assertEqual(code, 0)
        self.assertIn("WARN", text)
        self.assertIn("- Warning: ", recovery._report(updated))

    def test_risky_category_requires_manifest_opt_in(self):
        self._plan()
        manifest = self._manifest()
        item_id = manifest["items"][0]["id"]
        with self.assertRaises(recovery.RecoveryError):
            recovery._set_status(self.manifest_path, manifest, item_id, "applied",
                                 None, "control-flow")
        recovery._set_status(self.manifest_path, self._manifest(), item_id, "pending",
                             None, "control-flow")
        forced = self._manifest()
        for item in forced["items"]:
            if item["id"] == item_id:
                item["status"] = "applied"
        for category_items in forced["inventory"].values():
            for item in category_items:
                if item["id"] == item_id:
                    item["status"] = "applied"
        self.assertEqual(len(recovery._risky_failures(forced)), 1)
        self.assertEqual(recovery._ladder(forced)[1], 1)
        forced["allow_risky"] = True
        self.assertEqual(recovery._risky_failures(forced), [])
        recovery._set_status(self.manifest_path, forced, item_id, "applied", None)
        self.assertEqual(self._manifest()["items"][0]["status"], "applied")

    def test_check_records_ladder_warnings_and_risky_failures(self):
        self._plan()
        manifest = self._manifest()
        offset_item = next(item for item in manifest["items"]
                           if item["category"] == "offset-to-field")
        recovery._set_status(self.manifest_path, manifest, offset_item["id"], "applied", None)
        manifest = self._manifest()
        for item in manifest["items"]:
            if item["id"] == offset_item["id"]:
                item["category"] = "expr-simplify"
        for category_items in manifest["inventory"].values():
            for item in category_items:
                if item["id"] == offset_item["id"]:
                    item["category"] = "expr-simplify"
        recovery._write_atomic(self.manifest_path, manifest)
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"):
            result = recovery._check(self.manifest_path, self._manifest(), str(self.obj), True)
        self.assertFalse(result["ok"])
        self.assertTrue(any("risky category `expr-simplify`" in failure
                            for failure in result["failures"]), result["failures"])
        self.assertTrue(any("ladder order" in warning for warning in result["warnings"]),
                        result["warnings"])

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



class GroupedItemTests(unittest.TestCase):
    """Items are one per decision target, not one per occurrence.

    Grouping is what stops a category agent re-running the same evidence hunt
    for every textual hit on one address; the regression it guards is measured
    in the GROUP_KEY comment in source_recovery.py.
    """

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=recovery.ROOT)
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def _items(self, text):
        source = self.root / "sample.c"
        source.write_text(text, encoding="utf-8")
        inventory, _ = recovery._inventory(source)
        return [item for category in sorted(inventory) for item in inventory[category]]

    def _by_id(self, text):
        return {item["id"]: item for item in self._items(text)}

    def test_repeated_address_is_one_item_with_every_occurrence(self):
        items = self._by_id(
            "int f(void) { return *(int *)0x5AA730; }\n"
            "int g(void) { return *(int *)0x5aa730 + 1; }\n"
            "int h(void) { return *(int *)0x5aa730 + 2; }\n")
        grouped = items["absolute_address_dereferences:0x5aa730"]
        self.assertEqual(grouped["occurrence_count"], 3)
        self.assertEqual([site["line"] for site in grouped["occurrences"]], [1, 2, 3])
        # The flattened fields keep pointing at the first site, so consumers that
        # only read `line` behave exactly as before grouping.
        self.assertEqual(grouped["line"], 1)
        self.assertEqual(
            [i for i in items if i.startswith("absolute_address_dereferences")],
            ["absolute_address_dereferences:0x5aa730"])

    def test_base_offset_groups_on_base_and_offset_not_offset_alone(self):
        items = self._by_id(
            "int f(void) { return *(int *)(player + 0x20); }\n"
            "int g(void) { return *(int *)(player + 0x20); }\n"
            "int h(void) { return *(int *)(entry + 0x20); }\n")
        self.assertEqual(items["raw_base_offset_dereferences:player+0x20"]["occurrence_count"], 2)
        self.assertEqual(items["raw_base_offset_dereferences:entry+0x20"]["occurrence_count"], 1)

    def test_decompiler_locals_are_never_grouped(self):
        # Ghidra reuses `local_c` for unrelated variables in different functions,
        # so one name is several independent decisions.
        items = self._items(
            "int f(void) { int local_c; return local_c; }\n"
            "int g(void) { int local_c; return local_c; }\n")
        locals_ = [i for i in items if i["id"].startswith("decompiler_style_locals")]
        self.assertEqual(len(locals_), 4)
        self.assertTrue(all("occurrences" not in item for item in locals_))

    def test_comment_and_code_hits_on_one_address_stay_separate(self):
        items = self._by_id(
            "/* calls FUN_00123456() on entry */\n"
            "int f(void) { FUN_00123456(); return 0; }\n")
        self.assertEqual(items["fun_calls:FUN_00123456@comment"]["category"], "comments")
        self.assertEqual(items["fun_calls:FUN_00123456"]["category"], "symbol-names")
        # Both must survive as distinct ids, or the manifest fails validation.
        self.assertEqual(len([i for i in items if i.startswith("fun_calls:")]), 2)

    def test_grouped_manifest_round_trips_validation(self):
        source = self.root / "sample.c"
        source.write_text(
            "int f(void) { return *(int *)0x5aa730 + *(int *)0x5aa730; }\n", encoding="utf-8")
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"):
            manifest = recovery._plan(str(source))
        self.assertEqual(recovery._validate_manifest(manifest), manifest)

    def test_pre_grouping_manifest_still_validates(self):
        # Manifests planned before grouping have no key/occurrences and are
        # mid-ladder on disk; dropping them would strand real work.
        source = self.root / "sample.c"
        source.write_text("int f(void) { return *(int *)0x5aa730; }\n", encoding="utf-8")
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"):
            manifest = recovery._plan(str(source))
        for item in manifest["items"]:
            for field in ("key", "occurrences", "occurrence_count"):
                item.pop(field, None)
        self.assertEqual(recovery._validate_manifest(manifest), manifest)

    def test_occurrence_count_disagreement_is_rejected(self):
        source = self.root / "sample.c"
        source.write_text(
            "int f(void) { return *(int *)0x5aa730 + *(int *)0x5aa730; }\n", encoding="utf-8")
        with mock.patch.object(recovery, "_git_head", return_value="deadbeef"):
            manifest = recovery._plan(str(source))
        grouped = next(i for i in manifest["items"] if i.get("occurrence_count") == 2)
        grouped["occurrence_count"] = 99
        with self.assertRaises(recovery.RecoveryError):
            recovery._validate_manifest(manifest)

if __name__ == "__main__":
    unittest.main()
