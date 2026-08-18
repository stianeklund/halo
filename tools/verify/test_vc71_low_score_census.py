#!/usr/bin/env python3
"""Unit tests for VC71 census generation and score-context metric inputs."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


HERE = Path(__file__).resolve().parent


def _load(name):
    path = HERE / (name + ".py")
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


census = _load("vc71_low_score_census")
compare_obj = _load("compare_obj")
vc71_verify = _load("vc71_verify")


class TestScoreContextInputs(unittest.TestCase):
    def test_regparam_metrics_share_official_selected_candidate(self):
        cand = ["movl 0x8(%ebp), %eax", "addl $0x1, %eax", "retl"]
        ref = ["addl $0x1, %eax", "retl"]
        official = compare_obj.compare_functions(
            cand, ref, regdef_params=[(0, "eax")])[0]
        pack = vc71_verify._build_score_context(
            "test_function", cand, ref, official, [], [], [], [],
            census.ROOT / "src/test.c",
            {"kind": "auto", "n_insns": 2}, [(0, "eax")], compare_obj)
        self.assertEqual(pack["scores"]["official_pct"], 100.0)
        self.assertEqual(pack["scores"]["raw_mnemonic_pct"], 80.0)
        self.assertEqual(pack["scores"]["abi_modeled_mnemonic_pct"], 100.0)
        self.assertEqual(pack["scores"]["abi_model"], "regparam_stripped")
        self.assertEqual(pack["schema"], 2)
        self.assertEqual(pack["scores"]["dp_lcs_pct"], 100.0)
        self.assertEqual(pack["scores"]["preprocessing"], "regparam_stripped")
        self.assertEqual(pack["scores"]["regparam_loads_stripped"], 1)

    def test_classifies_static_regarg_helper_ceiling(self):
        scores = {"regparam_loads_stripped": 1}
        diff_ops = [
            {"kind": "delete", "cand": [
                "pushl\t%ebp", "movl\t%esp, %ebp", "pushl\t%esi"]},
            {"kind": "delete", "cand": ["popl\t%esi", "popl\t%ebp"]},
        ]
        rules = vc71_verify._classify_score_context(
            scores, {}, diff_ops, {})
        self.assertIn("regarg_static_helper_ceiling",
                      [rule["rule"] for rule in rules])

        rules = vc71_verify._classify_score_context(
            scores, {}, [{"kind": "delete", "cand": ["pushl\t%esi"]}], {})
        self.assertNotIn("regarg_static_helper_ceiling",
                         [rule["rule"] for rule in rules])

    def test_forwarding_reference_is_explicit(self):
        pack = vc71_verify._build_score_context(
            "test_thunk", ["retl"], ["jmp 0x10"], 0.0,
            [], [], [], [], census.ROOT / "src/test.c",
            {"kind": "thunk", "n_insns": 1}, None, compare_obj)
        self.assertEqual(pack["classification"][0]["rule"],
                         "forwarding_reference")

    def test_relative_jump_decoder(self):
        self.assertEqual(vc71_verify._decode_relative_jump_target(
            0x1000, b"\xe9\x0b\x00\x00\x00"), 0x1010)
        self.assertEqual(vc71_verify._decode_relative_jump_target(
            0x1000, b"\xeb\xfe"), 0x1000)
        self.assertIsNone(vc71_verify._decode_relative_jump_target(
            0x1000, b"\xc3"))


class TestCensus(unittest.TestCase):
    def test_counts_join_gaps_rules_and_one_instruction_rows(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            floor = root / "scores.json"
            contexts = root / "contexts"
            contexts.mkdir()
            floor.write_text(json.dumps({"version": 2, "scores": {
                "perfect": {"score": 100.0, "n_r": 1, "kind": "auto"},
                "low": {"score": 20.0, "n_r": 1, "kind": "thunk"},
                "unclassified": {"score": 60.0, "n_r": 10, "kind": "auto"},
            }}))
            (contexts / "perfect.json").write_text(json.dumps({
                "name": "perfect", "scores": {
                    "official_pct": 100.0, "dp_lcs_pct": 100.0},
                "classification": []}))
            (contexts / "low.json").write_text(json.dumps({
                "name": "low", "scores": {
                    "official_pct": 20.0, "dp_lcs_pct": 35.0},
                "classification": [
                    {"rule": "anchor_collapse"},
                    {"rule": "regarg_structural_ceiling"},
                ]}))
            (contexts / "unclassified.json").write_text(json.dumps({
                "name": "unclassified", "scores": {
                    "official_pct": 60.0, "dp_lcs_pct": 60.0},
                "classification": []}))

            report = census.build_census(floor, contexts)
            self.assertEqual(report["join"]["joined_rows"], 3)
            self.assertEqual(report["inputs"]["context_schema_counts"], {"1": 3})
            self.assertEqual(report["low_score"]["count"], 2)
            self.assertEqual(report["low_score"]["without_rule"], 1)
            self.assertEqual(report["low_score"]["with_multiple_rules"], 1)
            self.assertEqual(report["metric_gap"]["low_score_count"], 1)
            self.assertEqual(
                report["one_instruction_references"]["below_low_threshold"], 1)
            self.assertEqual(
                report["regarg_structural_ceiling"]["below_70"], 1)


if __name__ == "__main__":
    unittest.main()
