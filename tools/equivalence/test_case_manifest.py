import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ab_check
from capture_profile import normalize_profile, pools_for_profile
import hmrc
from case_manifest import write_case


class CaptureProfileTest(unittest.TestCase):
    def test_profile_defaults_and_aliases(self):
        self.assertEqual(normalize_profile("AiCore"), "ai-core")
        self.assertEqual(pools_for_profile("ai-core"),
                         ("objects", "players", "actors"))
        self.assertEqual(pools_for_profile("full"),
                         ("objects", "players", "actors", "props"))


class CaseManifestTest(unittest.TestCase):
    def test_recipe_resolves_source_anchor_and_focused_window(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "source.halocase.json"
            source.write_text(json.dumps({
                "schema_version": 1,
                "fixture": {"level": "a10", "scenario": "fixture",
                            "input_hash": "abc"},
                "capture": {"alignment_window": 4,
                            "minimum_sustained_run": 2,
                            "gameplay_anchors": {"faithful": 1000}},
            }))
            recipe = root / "focused.halocapture.json"
            recipe.write_text(json.dumps({
                "schema_version": 1,
                "source_case": source.name,
                "onset_tick": 1042,
                "window_before": 30,
                "window_after": 60,
                "actor_slots": [7],
                "include_perception": True,
                "include_linked_object_body": True,
                "include_weapon_bodies": True,
                "include_object_relations": True,
                "quantum": 1,
                "max_relation_nodes": 16,
            }))
            loaded = ab_check._load_recipe(recipe)
            self.assertEqual(loaded["onset_relative"], 42)
            self.assertEqual(loaded["tick_start"], 12)
            self.assertEqual(loaded["tick_end"], 102)
            self.assertEqual(loaded["source_case"], source.resolve())
            self.assertIn("--include-focused-weapon-bodies",
                          loaded["capture_args"])

    def test_recipe_run_emits_parented_child_case(self):
        saved = (ab_check.capture_run, ab_check._diff_behavior,
                 ab_check._coverage)
        seen_args = []

        def fake_capture(level, scenario, xbe, host, out, ticks, quantum,
                         no_wait_spawn, profile, capture_args=()):
            seen_args.append((ticks, quantum, profile, tuple(capture_args)))
            hmrc.write_halorec(out, "fixture", [])
            return {"anchor_tick": 200}

        def fake_diff(golden, candidate, cfg):
            return {"window": 4, "min_run": 2, "framesA": 1, "framesB": 1,
                    "onset_count": 0, "onsets": []}

        coverage = ab_check._empty_coverage("ai-core")
        coverage["missing_required"] = []
        coverage.update({"ticks": True, "object_index": True,
                         "players": True, "actors": True})
        try:
            ab_check.capture_run = fake_capture
            ab_check._diff_behavior = fake_diff
            ab_check._coverage = lambda *args: coverage
            with tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                source = root / "source.halocase.json"
                source.write_text(json.dumps({
                    "schema_version": 1,
                    "fixture": {"level": "a10", "scenario": "fixture"},
                    "capture": {"gameplay_anchors": {"faithful": 1000}},
                }))
                recipe = root / "focused.halocapture.json"
                recipe.write_text(json.dumps({
                    "schema_version": 1,
                    "source_case": source.name,
                    "onset_tick": 1042,
                    "actor_slots": [7],
                    "include_perception": True,
                    "include_linked_object_body": True,
                    "include_weapon_bodies": True,
                    "include_object_relations": True,
                    "quantum": 1,
                    "max_relation_nodes": 16,
                }))
                rc = ab_check.main([
                    "--recipe", str(recipe), "--no-deploy",
                    "--out-dir", str(root),
                ])
                self.assertEqual(rc, 0)
                self.assertEqual(len(seen_args), 2)
                self.assertTrue(all(item[0:3] == (102, 1, "ai-core")
                                    for item in seen_args))
                self.assertTrue(all("--tick-start" in item[3]
                                    for item in seen_args))
                child = root / "a10_fixture_focused_t42.halocase.json"
                data = json.loads(child.read_text())
                self.assertEqual(data["parent_case"], source.name)
                self.assertEqual(data["capture"]["tick_start"], 12)
                self.assertEqual(data["capture"]["tick_end"], 102)
                self.assertEqual(data["capture"]["gameplay_anchors"]["faithful"], 200)
        finally:
            (ab_check.capture_run, ab_check._diff_behavior,
             ab_check._coverage) = saved

    def test_ab_check_defaults_to_ai_core_and_emits_report_and_case(self):
        saved = (ab_check.capture_run, ab_check._diff_behavior,
                 ab_check._coverage)
        seen_profiles = []

        def fake_capture(level, scenario, xbe, host, out, ticks, quantum,
                         no_wait_spawn, profile):
            seen_profiles.append(profile)
            hmrc.write_halorec(out, "fixture", [])

        def fake_diff(golden, candidate, cfg):
            return {"window": 4, "min_run": 2, "framesA": 1, "framesB": 1,
                    "onset_count": 0, "onsets": []}

        coverage = ab_check._empty_coverage("ai-core")
        coverage["missing_required"] = []
        coverage["ticks"] = True
        coverage["object_index"] = True
        coverage["players"] = True
        coverage["actors"] = True
        try:
            ab_check.capture_run = fake_capture
            ab_check._diff_behavior = fake_diff
            ab_check._coverage = lambda *args: coverage
            with tempfile.TemporaryDirectory() as temp:
                out_dir = Path(temp)
                rc = ab_check.main([
                    "--level", "a10", "--scenario", "fixture",
                    "--no-deploy", "--out-dir", str(out_dir),
                ])
                self.assertEqual(rc, 0)
                self.assertEqual(seen_profiles, ["ai-core", "ai-core"])
                report = out_dir / "a10_fixture_behavior.json"
                case = out_dir / "a10_fixture.halocase.json"
                self.assertTrue(report.is_file())
                self.assertTrue(case.is_file())
                self.assertEqual(json.loads(report.read_text())["coverage"], coverage)
                self.assertEqual(json.loads(case.read_text())["verdict"], "CLEAN")
        finally:
            (ab_check.capture_run, ab_check._diff_behavior,
             ab_check._coverage) = saved

    def test_relative_hashed_artifacts_and_verdict(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            faithful = root / "faithful.halorec"
            candidate = root / "candidate.halorec"
            report = root / "behavior.json"
            faithful.write_bytes(b"faithful")
            candidate.write_bytes(b"candidate")
            report.write_text("{}\n")
            case = root / "nested" / "run.halocase.json"
            write_case(
                case,
                level="a10",
                scenario="fixture",
                profile="ai-core",
                backend="xemu-qmp",
                ticks=300,
                quantum=1,
                alignment_window=4,
                minimum_sustained_run=2,
                faithful=faithful,
                candidate=candidate,
                behavior_report=report,
                faithful_build="cachebeta.xbe",
                candidate_build="default.xbe",
                candidate_verification="verified",
                verdict="CLEAN",
                coverage={"missing_required": [], "missing_fields": {}},
            )
            data = json.loads(case.read_text())
            self.assertEqual(data["schema_version"], 1)
            self.assertFalse(Path(data["faithful"]["path"]).is_absolute())
            self.assertEqual(len(data["faithful"]["hash"]), 64)
            self.assertEqual(data["candidate"]["verification_status"], "verified")
            self.assertEqual(data["verdict"], "CLEAN")


if __name__ == "__main__":
    unittest.main()
