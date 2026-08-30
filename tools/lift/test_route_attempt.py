#!/usr/bin/env python3
"""Unit tests for deterministic lift routing."""

import unittest

from route_attempt import route_attempt


class RouteAttemptTests(unittest.TestCase):
    def test_failed_safety_gate_precedes_score_policy(self):
        result = route_attempt(92.0, {}, {"stages": [
            {"name": "hazard_scan", "ran": True, "ok": False},
        ]})
        self.assertEqual(result["route"], "repair_gate_failure")

    def test_score_bands_are_disjoint(self):
        self.assertEqual(route_attempt(None, {})["route"], "measure")
        self.assertEqual(route_attempt(64.9, {})["route"], "semantic_relift")
        self.assertEqual(route_attempt(84.9, {})["route"], "codegen_optimize")
        self.assertEqual(route_attempt(85.0, {})["route"], "permute")
        self.assertEqual(route_attempt(98.0, {})["route"], "behavior_validate")

    def test_classifier_routes_atlas_and_known_ceiling(self):
        atlas = route_attempt(74.0, {"classification": [{"rule": "loadw_field_width"}]})
        self.assertEqual(atlas["route"], "atlas_fix")
        ceiling = route_attempt(74.0, {"classification": [
            {"rule": "regarg_structural_ceiling"},
        ]})
        self.assertEqual(ceiling["route"], "park_structural")

    def test_score_context_can_supply_score(self):
        result = route_attempt(None, {"scores": {"official_pct": 87.5}})
        self.assertEqual(result["route"], "permute")


if __name__ == "__main__":
    unittest.main()
