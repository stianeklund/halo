"""Standard-library tests for the /recover-goal queue and ledger."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.recovery import recovery_goal as goal


class RecoveryGoalSelfTestTests(unittest.TestCase):
    def test_script_path_runs_self_test(self):
        script = goal.ROOT / "tools" / "recovery" / "recovery_goal.py"
        result = subprocess.run(
            [sys.executable, str(script), "--self-test"],
            cwd=goal.ROOT,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("0 failure(s)", result.stdout)


class LedgerTransitionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.path = self.root / "goal_ledger.json"
        self.ledger = goal._empty_ledger()
        patcher = mock.patch.object(goal, "_today", return_value="2026-08-01")
        self.addCleanup(patcher.stop)
        patcher.start()

    def tearDown(self):
        self.temp.cleanup()

    def test_missing_ledger_is_empty_schema(self):
        self.assertEqual(goal._load_ledger(self.path), {"schema": 1, "objects": {}})

    def test_full_lifecycle_records_dates_commits_and_categories(self):
        goal.transition_start(self.ledger, "hud_weapon.obj")
        entry = goal.transition_finish(self.ledger, "hud_weapon.obj",
                                      ["abc1234", "def5678"],
                                      ["comments", "local-renames"])
        self.assertEqual(entry, {
            "status": "done",
            "reason": None,
            "started": "2026-08-01",
            "finished": "2026-08-01",
            "commits": ["abc1234", "def5678"],
            "categories_done": ["comments", "local-renames"],
        })

    def test_park_overwrites_in_progress_and_reparking_updates_reason(self):
        goal.transition_start(self.ledger, "errors.obj")
        goal.transition_park(self.ledger, "errors.obj", "no COFF baseline")
        self.assertEqual(goal._status_of(self.ledger, "errors.obj"), "parked")
        entry = goal.transition_park(self.ledger, "errors.obj", "manifest errors")
        self.assertEqual(entry["reason"], "manifest errors")
        self.assertEqual(entry["status"], "parked")

    def test_park_without_start_is_allowed(self):
        entry = goal.transition_park(self.ledger, "hud.obj", "capture failed")
        self.assertEqual(entry["status"], "parked")
        self.assertEqual(entry["reason"], "capture failed")

    def test_parked_object_can_be_restarted_and_keeps_commit_history(self):
        goal.transition_start(self.ledger, "hud.obj")
        goal.transition_finish(self.ledger, "hud.obj", ["aaaaaaa"], ["comments"])
        goal.transition_start(self.ledger, "hud.obj", force=True)
        goal.transition_park(self.ledger, "hud.obj", "second pass blocked")
        entry = self.ledger["objects"]["hud.obj"]
        self.assertEqual(entry["commits"], ["aaaaaaa"])
        self.assertEqual(entry["categories_done"], ["comments"])

    def test_finish_is_additive_and_deduplicates_commits(self):
        goal.transition_start(self.ledger, "hud.obj")
        goal.transition_finish(self.ledger, "hud.obj", ["aaaaaaa"], ["comments"])
        goal.transition_start(self.ledger, "hud.obj", force=True)
        entry = goal.transition_finish(self.ledger, "hud.obj",
                                       ["aaaaaaa", "bbbbbbb"], ["comments", "const-enum"])
        self.assertEqual(entry["commits"], ["aaaaaaa", "bbbbbbb"])
        self.assertEqual(entry["categories_done"], ["comments", "const-enum"])

    def test_cannot_finish_what_was_not_started(self):
        with self.assertRaises(goal.GoalError):
            goal.transition_finish(self.ledger, "hud.obj", ["abc1234"])

    def test_cannot_finish_a_done_or_parked_object(self):
        goal.transition_start(self.ledger, "a.obj")
        goal.transition_finish(self.ledger, "a.obj", ["abc1234"])
        goal.transition_park(self.ledger, "b.obj", "blocked")
        for name in ("a.obj", "b.obj"):
            with self.assertRaises(goal.GoalError):
                goal.transition_finish(self.ledger, name, ["abc1234"])

    def test_finish_requires_commits_or_explicit_no_commits(self):
        goal.transition_start(self.ledger, "a.obj")
        with self.assertRaises(goal.GoalError):
            goal.transition_finish(self.ledger, "a.obj", [])
        entry = goal.transition_finish(self.ledger, "a.obj", [], None, no_commits=True)
        self.assertEqual(entry["commits"], [])
        self.assertEqual(entry["status"], "done")

    def test_finish_rejects_a_value_that_is_not_a_commit_sha(self):
        goal.transition_start(self.ledger, "a.obj")
        with self.assertRaises(goal.GoalError):
            goal.transition_finish(self.ledger, "a.obj", ["hud_weapon.obj"])
        self.assertEqual(goal._status_of(self.ledger, "a.obj"), "in_progress")

    def test_start_refuses_done_and_in_progress_without_force(self):
        goal.transition_start(self.ledger, "a.obj")
        with self.assertRaises(goal.GoalError):
            goal.transition_start(self.ledger, "a.obj")
        goal.transition_finish(self.ledger, "a.obj", ["abc1234"])
        with self.assertRaises(goal.GoalError):
            goal.transition_start(self.ledger, "a.obj")
        self.assertEqual(goal.transition_start(self.ledger, "a.obj", force=True)["status"],
                         "in_progress")

    def test_park_requires_a_reason_and_refuses_a_done_object(self):
        goal.transition_start(self.ledger, "a.obj")
        with self.assertRaises(goal.GoalError):
            goal.transition_park(self.ledger, "a.obj", "  ")
        goal.transition_finish(self.ledger, "a.obj", ["abc1234"])
        with self.assertRaises(goal.GoalError):
            goal.transition_park(self.ledger, "a.obj", "changed my mind")
        self.assertEqual(
            goal.transition_park(self.ledger, "a.obj", "reverted", force=True)["status"],
            "parked")

    def test_object_names_must_be_kb_objects(self):
        for bad in ("src/halo/interface/hud_weapon.c", "hud_weapon", "", "hud weapon.obj"):
            with self.assertRaises(goal.GoalError):
                goal._object_name(bad)
        self.assertEqual(goal._object_name(" hud_weapon.obj "), "hud_weapon.obj")


class LedgerPersistenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / "goal_ledger.json"

    def tearDown(self):
        self.temp.cleanup()

    def test_round_trip(self):
        ledger = goal._empty_ledger()
        goal.transition_start(ledger, "a.obj")
        goal._write_atomic(self.path, ledger)
        self.assertEqual(goal._load_ledger(self.path), ledger)

    def test_corrupt_and_invalid_ledgers_raise_goal_error(self):
        self.path.write_text("{not json", encoding="utf-8")
        with self.assertRaises(goal.GoalError):
            goal._load_ledger(self.path)
        for bad in ([], {"objects": {}}, {"schema": 99, "objects": {}},
                    {"schema": 1, "objects": []},
                    {"schema": 1, "objects": {"a.obj": {"status": "wat"}}},
                    {"schema": 1, "objects": {"a.obj": {"status": "done",
                                                        "commits": "abc"}}}):
            with self.assertRaises(goal.GoalError):
                goal._validate_ledger(bad)


class FrontierSelectionTests(unittest.TestCase):
    ROWS = [
        {"object": "small.obj", "funcs": 4, "score": 400.0, "rank": 1},
        {"object": "a.obj", "funcs": 20, "score": 300.0, "rank": 2},
        {"object": "big.obj", "funcs": 40, "score": 200.0, "rank": 3},
        {"object": "tiny.obj", "funcs": 30, "score": 1.0, "rank": 4},
    ]

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.frontier = self.root / "frontier.json"
        # Deliberately unsorted on disk: ordering must come from the score.
        self.frontier.write_text(json.dumps({"eligible": list(reversed(self.ROWS))}),
                                 encoding="utf-8")
        self.ledger = goal._empty_ledger()

    def tearDown(self):
        self.temp.cleanup()

    def test_rows_are_score_ordered_and_min_funcs_filters(self):
        self.assertEqual([r["object"] for r in goal.frontier_rows(str(self.frontier))],
                         ["small.obj", "a.obj", "big.obj", "tiny.obj"])
        self.assertEqual([r["object"] for r in goal.frontier_rows(str(self.frontier), 10)],
                         ["a.obj", "big.obj", "tiny.obj"])

    def test_next_skips_taken_objects(self):
        rows = goal.frontier_rows(str(self.frontier))
        goal.transition_park(self.ledger, "small.obj", "blocked")
        goal.transition_start(self.ledger, "a.obj")
        row, stats = goal.select_next(rows, self.ledger)
        self.assertEqual(row["object"], "big.obj")
        self.assertEqual(stats["taken"], 2)

    def test_min_score_filters(self):
        rows = goal.frontier_rows(str(self.frontier))
        row, _stats = goal.select_next(rows, self.ledger, 500.0)
        self.assertIsNone(row)
        row, _stats = goal.select_next(rows, self.ledger, 100.0)
        self.assertEqual(row["object"], "small.obj")

    def test_exhaustion_reports_counted_reasons(self):
        for name in ("small.obj", "a.obj", "big.obj"):
            goal.transition_park(self.ledger, name, "blocked")
        row, stats = goal.select_next(goal.frontier_rows(str(self.frontier)),
                                      self.ledger, 10.0)
        self.assertIsNone(row)
        self.assertEqual(stats, {"eligible": 4, "taken": 3, "below_min_score": 1})

    def test_empty_frontier_is_exhaustion_not_an_error(self):
        row, stats = goal.select_next([], self.ledger)
        self.assertIsNone(row)
        self.assertEqual(stats["eligible"], 0)

    def test_malformed_and_missing_frontier_json_raise_goal_error(self):
        self.frontier.write_text("{}", encoding="utf-8")
        with self.assertRaises(goal.GoalError):
            goal.frontier_rows(str(self.frontier))
        with self.assertRaises(goal.GoalError):
            goal.frontier_rows(str(self.root / "absent.json"))

    def test_frontier_subprocess_failure_is_a_clean_error(self):
        failed = subprocess.CompletedProcess([], 1, stdout="", stderr="boom\n")
        with mock.patch.object(goal.subprocess, "run", return_value=failed):
            with self.assertRaises(goal.GoalError) as caught:
                goal.frontier_rows(None, 10)
        self.assertIn("recovery frontier failed", str(caught.exception))

    def test_missing_frontier_tool_is_a_clean_error(self):
        with mock.patch.object(goal, "FRONTIER", self.root / "nope.py"):
            with self.assertRaises(goal.GoalError) as caught:
                goal.frontier_rows(None, 0)
        self.assertIn("not found", str(caught.exception))


class CommandLineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.path = self.root / "goal_ledger.json"
        self.frontier = self.root / "frontier.json"
        self.frontier.write_text(json.dumps({"eligible": [
            {"object": "hud_weapon.obj", "funcs": 11, "score": 104.5, "rank": 2,
             "debt_total": 78, "debt_per_func": 7.1, "high_match_pct": 55.0,
             "high_match_funcs": 6, "measured_funcs": 11,
             "files": ["src/halo/interface/hud_weapon.c"],
             "debt": {"fun_calls": 40, "decompiler_style_locals": 38}},
            {"object": "errors.obj", "funcs": 26, "score": 15.8, "rank": 9},
        ]}), encoding="utf-8")

    def tearDown(self):
        self.temp.cleanup()

    def _run(self, *argv):
        return goal.main(["--ledger", str(self.path), *argv])

    def test_next_start_finish_and_status_flow(self):
        self.assertEqual(self._run("next", "--frontier-json", str(self.frontier)), 0)
        self.assertEqual(self._run("start", "hud_weapon.obj"), 0)
        self.assertEqual(self._run("finish", "hud_weapon.obj", "--commit", "abc1234",
                                   "--category", "comments"), 0)
        entry = json.loads(self.path.read_text(encoding="utf-8"))["objects"]["hud_weapon.obj"]
        self.assertEqual(entry["status"], "done")
        self.assertEqual(entry["commits"], ["abc1234"])
        self.assertEqual(entry["categories_done"], ["comments"])
        self.assertEqual(self._run("status"), 0)

    def test_next_advances_past_a_finished_object(self):
        self._run("start", "hud_weapon.obj")
        self._run("finish", "hud_weapon.obj", "--commit", "abc1234")
        self.assertEqual(self._run("next", "--frontier-json", str(self.frontier),
                                   "--json"), 0)

    def test_next_exit_code_3_when_exhausted(self):
        self._run("park", "hud_weapon.obj", "--reason", "blocked")
        self._run("park", "errors.obj", "--reason", "blocked")
        self.assertEqual(self._run("next", "--frontier-json", str(self.frontier)), 3)
        self.assertEqual(self._run("next", "--frontier-json", str(self.frontier),
                                   "--json"), 3)

    def test_next_min_funcs_and_min_score_are_honoured(self):
        self.assertEqual(self._run("next", "--frontier-json", str(self.frontier),
                                   "--min-funcs", "20"), 0)
        self.assertEqual(self._run("next", "--frontier-json", str(self.frontier),
                                   "--min-score", "200"), 3)

    def test_invalid_transitions_exit_2_without_writing_the_ledger(self):
        self.assertEqual(self._run("finish", "hud_weapon.obj", "--commit", "abc1234"), 2)
        self.assertFalse(self.path.exists())
        self.assertEqual(self._run("park", "hud_weapon.obj", "--reason", ""), 2)
        self.assertEqual(self._run("start", "src/halo/hud.c"), 2)
        self.assertFalse(self.path.exists())

    def test_finish_without_commits_exits_2_unless_explicit(self):
        self._run("start", "hud_weapon.obj")
        self.assertEqual(self._run("finish", "hud_weapon.obj"), 2)
        self.assertEqual(self._run("finish", "hud_weapon.obj", "--no-commits"), 0)

    def test_status_json_is_the_ledger(self):
        self._run("start", "hud_weapon.obj")
        self.assertEqual(self._run("status", "--json"), 0)
        self.assertEqual(json.loads(self.path.read_text(encoding="utf-8"))["schema"], 1)

    def test_corrupt_ledger_exits_2(self):
        self.path.write_text("{not json", encoding="utf-8")
        self.assertEqual(self._run("status"), 2)


if __name__ == "__main__":
    unittest.main()
