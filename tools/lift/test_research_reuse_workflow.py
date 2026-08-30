#!/usr/bin/env python3
"""Static acceptance checks for the goal-lift research-reuse cutover."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class WorkflowCutoverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.goal = (ROOT / ".claude" / "workflows" / "goal-lift.js").read_text(encoding="utf-8")
        cls.auto_session = (ROOT / ".claude" / "workflows" / "auto-session.js").read_text(encoding="utf-8")
        cls.analyst = (ROOT / ".claude" / "agents" / "auto-lift-analyst.md").read_text(encoding="utf-8")
        cls.reviewer = (ROOT / ".claude" / "agents" / "auto-lift-reviewer.md").read_text(encoding="utf-8")

    def test_no_unconditional_force_or_opus_research_call(self):
        self.assertNotIn("cache-context --target ${t.addr} --force", self.goal)
        self.assertNotIn("agent(researchPrompt(", self.goal)
        self.assertNotIn("schema: BRIEF_SCHEMA", self.goal)
        self.assertIn("agent(bundlePrompt(t)", self.goal)
        self.assertIn("phase: 'Research', ...M.mechanical, schema: BUNDLE_SCHEMA", self.goal)

    def test_no_broken_skill_paths(self):
        self.assertNotIn(".claude/skills/lift-arg-hazards", self.goal)
        self.assertNotIn(".claude/skills/lift-frame-hazards", self.goal)
        self.assertIn(".claude/skills/auto-lift-checklist/SKILL.md", self.goal)

    def test_automated_profiles_are_memoryless(self):
        self.assertNotIn("memory: project", self.analyst)
        self.assertNotIn("memory: project", self.reviewer)
        self.assertIn("agentType: 'auto-lift-analyst'", self.goal)
        self.assertIn("agentType: 'auto-lift-reviewer'", self.goal)

    def test_legacy_failure_file_is_not_a_skip_gate(self):
        self.assertNotIn("skip_prior_fail", self.goal)
        self.assertNotIn("t.prior_fail === true", self.goal)
        self.assertIn(
            "a1.capped === true && a1.cap_confidence === 'high'", self.goal)

    def test_existing_safety_gates_remain(self):
        for required in ("check_lift_hazards.py", "audit_reg_abi.py",
                         "lift_pipeline.py", "vc71_score", "gateThenCommit"):
            self.assertIn(required, self.goal, required)
        self.assertIn("auto_reintegrate.py", self.auto_session)


if __name__ == "__main__":
    unittest.main()
