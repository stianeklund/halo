#!/usr/bin/env python3
"""Focused stdlib tests for tools/auto_lift_opencode.py.

Run:
    python3 -m unittest discover -s tools -p test_auto_lift_opencode.py -v
"""

import importlib.util
import json
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "auto_lift_opencode_under_test", ROOT / "tools" / "auto_lift_opencode.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def result(returncode=0, stdout="", stderr=""):
    return SimpleNamespace(returncode=returncode, stdout=stdout, stderr=stderr)


def selector_rows():
    return [
        {
            "lane": "auto-lift",
            "prior_fail": False,
            "parked_status": "parked",
            "target": {"name": "first", "object_name": "wanted.obj", "addr": "0x10"},
        },
        {
            "lane": "cache-context",
            "prior_fail": False,
            "parked_status": None,
            "target": {"name": "second", "object_name": "wanted.obj", "addr": "0x20"},
        },
        {
            "lane": "manual-lift",
            "prior_fail": False,
            "target": {"name": "manual", "object_name": "wanted.obj", "addr": "0x30"},
        },
        {
            "lane": "auto-lift",
            "prior_fail": True,
            "target": {"name": "failed", "object_name": "wanted.obj", "addr": "0x40"},
        },
        {
            "lane": "auto-lift",
            "prior_fail": False,
            "parked_status": "capped_confirmed",
            "target": {"name": "capped", "object_name": "wanted.obj", "addr": "0x50"},
        },
        {
            "lane": "auto-lift",
            "prior_fail": False,
            "target": {"name": "other", "object_name": "other.obj", "addr": "0x60"},
        },
    ]


class CandidateTests(unittest.TestCase):
    def test_filter_keeps_order_and_only_allowed_lanes(self):
        candidates = MODULE.filter_candidates(selector_rows(), ("wanted.obj",))
        self.assertEqual([candidate.name for candidate in candidates], ["first", "second"])

    def test_parse_args_defaults_and_object_allowlist(self):
        args = MODULE.parse_args(["--objects", "wanted.obj, other.obj", "--criteria", "prefer leaves"])
        self.assertEqual(args.goal, 1)
        self.assertEqual(args.stop_on_fail, 3)
        self.assertEqual(args.objects, ("wanted.obj", "other.obj"))
        self.assertEqual(args.criteria, "prefer leaves")
        self.assertEqual(args.agent, "build")

    def test_batch_alias_sets_goal(self):
        args = MODULE.parse_args(["--batch", "4"])
        self.assertEqual(args.goal, 4)


class GuardTests(unittest.TestCase):
    def test_protected_status_ignores_untracked_ci_and_artifacts(self):
        raw = "?? ci/snapshot.json\0?? artifacts/run.json\0 M src/halo/items/items.c\0"
        changes = MODULE.protected_tracked_changes(raw)
        self.assertEqual(len(changes), 1)
        self.assertEqual(changes[0].paths, ("src/halo/items/items.c",))

    def test_start_guard_refuses_main_and_protected_tracked_changes(self):
        main_state = MODULE.RepositoryState("main", "abc", ())
        with self.assertRaisesRegex(MODULE.DriverError, "allow-main"):
            MODULE.validate_start_state(main_state, allow_main=False)

        dirty_state = MODULE.RepositoryState(
            "topic",
            "abc",
            (MODULE.StatusEntry("M ", ("kb.json",)),),
        )
        with self.assertRaisesRegex(MODULE.DriverError, "protected tracked changes"):
            MODULE.validate_start_state(dirty_state, allow_main=True)

    def test_attempt_requires_new_head_and_clean_protected_state(self):
        before = MODULE.RepositoryState("topic", "before", ())
        no_commit = MODULE.assess_attempt(before, before, 0)
        self.assertFalse(no_commit.committed)
        self.assertFalse(no_commit.retryable)

        clean_failure = MODULE.assess_attempt(before, before, 1)
        self.assertFalse(clean_failure.committed)
        self.assertTrue(clean_failure.retryable)

        committed = MODULE.assess_attempt(
            before, MODULE.RepositoryState("topic", "after", ()), 0
        )
        self.assertTrue(committed.committed)


class CommandTests(unittest.TestCase):
    def test_command_composition_includes_model_and_guarded_prompt(self):
        candidate = MODULE.Candidate("FUN_00000010", "items.obj", "0x10", "auto-lift", {})
        self.assertEqual(
            MODULE.selector_command(),
            ["python3", "tools/llm_auto_lift.py", "select", "--limit", "60", "--json"],
        )
        command = MODULE.opencode_command(candidate, "lift-agent", "model-x", "small only")
        self.assertEqual(command[:6], ["opencode", "run", "--agent", "lift-agent", "-m", "model-x"])
        prompt = command[-1]
        self.assertIn("C89", prompt)
        self.assertIn("/lift rules", prompt)
        self.assertIn("classify_liftability.py", prompt)
        self.assertIn(
            "rtk python3 tools/lift_pipeline.py --target FUN_00000010 --no-metadata-update --verify-policy goal90",
            prompt,
        )
        self.assertIn("mktemp /tmp/halo-commit-msg.XXXXXX", prompt)
        self.assertIn("git checkout", prompt)
        self.assertIn("git reset", prompt)
        self.assertIn("small only", prompt)

    def test_dry_run_selects_without_invoking_opencode(self):
        calls = []

        def fake_run(command, **kwargs):
            calls.append((command, kwargs))
            if command[:3] == ["git", "branch", "--show-current"]:
                return result(stdout="topic\n")
            if command[:3] == ["git", "rev-parse", "HEAD"]:
                return result(stdout="abc\n")
            if command[:3] == ["git", "status", "--porcelain=v1"]:
                return result(stdout="?? ci/snapshot.json\0")
            if command == MODULE.selector_command():
                return result(stdout=json.dumps(selector_rows()))
            self.fail("unexpected command: " + repr(command))

        args = MODULE.parse_args(["--dry-run", "--objects", "wanted.obj"])
        output = []
        with patch.object(MODULE.subprocess, "run", side_effect=fake_run):
            summary = MODULE.run_driver(args, root=Path("/repo"), emit=output.append)

        self.assertEqual(summary.reason, "dry-run")
        self.assertEqual(summary.committed, 0)
        self.assertTrue(any(line == "01 first wanted.obj auto-lift" for line in output))
        self.assertFalse(any(command[:2] == ["opencode", "run"] for command, _ in calls))
        self.assertIn((MODULE.selector_command(), {
            "cwd": "/repo", "text": True, "capture_output": True, "check": False,
        }), calls)

    def test_real_run_invokes_opencode_once_and_counts_only_new_head(self):
        calls = []
        state = {"head_reads": 0}

        def fake_run(command, **kwargs):
            calls.append((command, kwargs))
            if command[:3] == ["git", "branch", "--show-current"]:
                return result(stdout="topic\n")
            if command[:3] == ["git", "rev-parse", "HEAD"]:
                state["head_reads"] += 1
                return result(stdout=("after\n" if state["head_reads"] >= 3 else "before\n"))
            if command[:3] == ["git", "status", "--porcelain=v1"]:
                return result(stdout="")
            if command == MODULE.selector_command():
                return result(stdout=json.dumps(selector_rows()[:1]))
            if command[:2] == ["opencode", "run"]:
                return result()
            self.fail("unexpected command: " + repr(command))

        args = MODULE.parse_args(["--agent", "lift-agent", "--model", "model-x"])
        with patch.object(MODULE.subprocess, "run", side_effect=fake_run):
            summary = MODULE.run_driver(args, root=Path("/repo"), emit=lambda _: None)

        self.assertEqual(summary.committed, 1)
        self.assertEqual(summary.reason, "goal-reached")
        opencode_calls = [call for call in calls if call[0][:2] == ["opencode", "run"]]
        self.assertEqual(len(opencode_calls), 1)
        self.assertEqual(opencode_calls[0][0][:6], [
            "opencode", "run", "--agent", "lift-agent", "-m", "model-x",
        ])
        self.assertFalse(opencode_calls[0][1]["capture_output"])


if __name__ == "__main__":
    unittest.main()
