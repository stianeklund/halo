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
