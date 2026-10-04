#!/usr/bin/env python3
"""Synthetic soundness tests for the raw-XBE structural lane."""

import importlib.util
import argparse
import contextlib
import io
import struct
import tempfile
import pathlib
import unittest
from pathlib import Path
from unittest import mock

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("raw_xbe_structural", HERE / "raw_xbe_structural.py")
raw = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(raw)


def relocation(offset, reloc_type=raw.IMAGE_REL_I386_REL32, symbol="FUN_00001010"):
    return {"offset": offset, "absolute_offset": offset, "symbol_index": 0,
            "type": reloc_type, "type_name": raw.RELOCATION_NAMES.get(reloc_type, "UNKNOWN"),
            "symbol": {"name": symbol}}


def resolved(target=0x1020):
    return {target: {"logical_target": target}}


class TestStructuralComparison(unittest.TestCase):
    def test_rel32_operand_is_masked_but_non_relocation_bytes_are_literal(self):
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(2)], reference, 0x1000, resolved(0x1021))
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["confidence"], "high")
        self.assertTrue(result["relocations"][0]["comparable"])

    def test_rel32_linked_displacement_differs_but_logical_target_matches(self):
        # COFF candidate field A=0x10; linked raw displacement is S+A-P=0x1b.
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\x10\x00\x00\x00\xc3"
        result = raw.compare(candidate, [relocation(1)], reference, 0x1000)
        self.assertEqual(result["verdict"], "structural exact")

    def test_known_wrong_addend_is_structural_differ(self):
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\x11\x00\x00\x00\xc3"
        result = raw.compare(candidate, [relocation(1, symbol="FUN_00001020")], reference, 0x1000)
        self.assertEqual(result["verdict"], "structural differ")
        self.assertEqual(result["identity_evidence"]["status"], "mismatch")
        self.assertTrue(result["identity_evidence"]["required_for_structural_match"])
        self.assertEqual(result["aligned_byte_match"]["mismatched_relocation_bytes"], 4)
        self.assertEqual(result["aligned_byte_match"]["byte_accuracy"],
                         result["aligned_byte_match"]["byte_accuracy_upper_bound"])

    def test_injected_identity_mismatch_is_structural_differ(self):
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\x10\x00\x00\x00\xc3"
        result = raw.compare(candidate, [relocation(1)], reference, 0x1000,
                             {0x1020: {"logical_target": 0x9999}})
        self.assertEqual(result["verdict"], "structural differ")
        self.assertEqual(result["identity_evidence"]["status"], "mismatch")
        self.assertEqual(result["aligned_byte_match"]["mismatched_relocations"], 1)

    def test_metrics_report_only_unmasked_byte_accuracy(self):
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(2)], reference, 0x1000,
                             resolved(0x1021))
        self.assertEqual(result["matching_non_relocation_bytes"], 3)
        self.assertEqual(result["byte_counts"]["relocation_operand"], 4)
        self.assertEqual(result["byte_counts"]["non_relocation"], 3)
        self.assertEqual(result["byte_accuracy"], 1.0)
        self.assertTrue(result["relocation_shape_evidence"]["matched"])

    def test_shape_mismatch_refuses_masking(self):
        # The function still scores, but nothing is masked and the score is
        # flagged as a lower bound.  Refusing to mask can only lower it.
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(3)], reference, 0x1000)
        self.assertEqual(result["relocation_shape_evidence"]["status"], "mismatch")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])
        self.assertEqual(result["unmasked_relocations"], 1)

    def test_relocation_location_mismatch_is_not_exact(self):
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(3)], reference, 0x1000,
                             resolved(0x1007))
        self.assertNotEqual(result["verdict"], "structural exact")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])

    def test_relocation_type_mismatch_is_never_masked(self):
        reference = b"\x90\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\x90\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(2, raw.IMAGE_REL_I386_DIR32)],
                             reference, 0x1000, resolved())
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])

    def test_relocation_count_mismatch_does_not_create_exactness(self):
        # Identical bytes, but the candidate declares a relocation the reference
        # cannot account for, so nothing may be masked on its behalf.
        reference = b"\x90\xc3"
        candidate = b"\x90\xc3"
        result = raw.compare(candidate, [relocation(0)], reference, 0x1000, resolved())
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])
        self.assertEqual(result["unmasked_relocations"], 1)

    def test_relocation_not_on_an_operand_field_is_never_masked(self):
        # Offset 2 is inside the jcc opcode, not its displacement field.
        reference = b"\x90\x0f\x80\x00\x00\x00\x00\xc3"
        candidate = b"\x90\x0f\x80\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(2)], reference, 0x1000, resolved())
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])

    def test_jcc_rel32_displacement_is_supported(self):
        # A near-jcc displacement is as unambiguous as a call's; the earlier
        # opcode whitelist simply did not list it.
        reference = b"\x90\x0f\x80\x00\x00\x00\x00\xc3"
        candidate = b"\x90\x0f\x80\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(3)], reference, 0x1000,
                             resolved(0x1007))
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 4)

    def test_push_imm32_absolute_is_supported(self):
        reference = b"\x68\x20\x10\x00\x00\xc3"
        candidate = b"\x68\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(1, raw.IMAGE_REL_I386_DIR32)],
                             reference, 0x1000, resolved())
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 4)

    def test_dir32_relocation_on_a_branch_field_is_refused(self):
        # A DIR32 must never be masked over a relative branch displacement.
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(1, raw.IMAGE_REL_I386_DIR32)],
                             reference, 0x1000, resolved())
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertTrue(result["accuracy_is_lower_bound"])

    def test_size_mismatch_counts_against_byte_accuracy(self):
        # A truncated candidate whose prefix matches must not read as 100%.
        reference = b"\x90\x90\x90\x90\xc3"
        candidate = b"\x90\x90"
        result = raw.compare(candidate, [], reference, 0x1000)
        self.assertEqual(result["verdict"], "structural differ")
        self.assertEqual(result["byte_counts"]["non_relocation"], 5)
        self.assertEqual(result["matching_non_relocation_bytes"], 2)
        self.assertAlmostEqual(result["byte_accuracy"], 2 / 5)

    def test_aligned_bytes_resynchronize_after_shorter_instruction(self):
        reference = b"\x81\xec\xd0\x00\x00\x00\x53\xc3"
        candidate = b"\x83\xec\x68\x53\xc3"
        result = raw.compare(candidate, [], reference, 0x1000)
        aligned = result["aligned_byte_match"]
        self.assertEqual(aligned["status"], "scored")
        self.assertEqual(aligned["matching_bytes"], 3)
        self.assertEqual(aligned["compared_bytes"], 8)
        self.assertAlmostEqual(aligned["byte_accuracy"], 3 / 8)
        self.assertGreater(aligned["byte_accuracy"], result["byte_accuracy"])

    def test_aligned_bytes_penalize_insert_and_resynchronize(self):
        reference = b"\x53\xc3"
        candidate = b"\x90\x53\xc3"
        aligned = raw.compare(candidate, [], reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["matching_bytes"], 2)
        self.assertEqual(aligned["compared_bytes"], 3)
        self.assertFalse(aligned["accuracy_is_lower_bound"])
        self.assertEqual(aligned["candidate_only_instructions"], 1)

    def test_dp_alignment_disambiguates_repeated_push_run(self):
        reference = b"\x51\x50\x53\xc3"
        candidate = b"\x50\x53\xc3"
        aligned = raw.compare(candidate, [], reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["method"], "weighted_global_dp_v1")
        self.assertEqual(aligned["matching_bytes"], 3)
        self.assertEqual(aligned["compared_bytes"], 4)
        self.assertEqual(aligned["reference_only_instructions"], 1)
        self.assertEqual(aligned["normalized_exact_instructions"], 3)

    def test_dp_alignment_disambiguates_repeated_mov_run(self):
        reference = b"\x89\xc9\x89\xc0\x89\xdb\xc3"
        candidate = b"\x89\xc0\x89\xdb\xc3"
        aligned = raw.compare(candidate, [], reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["matching_bytes"], 5)
        self.assertEqual(aligned["compared_bytes"], 7)
        self.assertEqual(aligned["reference_only_instructions"], 1)
        self.assertEqual(aligned["normalized_exact_instructions"], 3)

    def test_aligned_bytes_keep_wrong_immediate_visible(self):
        reference = b"\x68\x01\x00\x00\x00\xc3"
        candidate = b"\x68\x02\x00\x00\x00\xc3"
        aligned = raw.compare(candidate, [], reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["matching_bytes"], 5)
        self.assertEqual(aligned["compared_bytes"], 6)

    def test_aligned_relocation_masks_at_independent_instruction_offsets(self):
        reference = b"\x90\xe8\x1a\x00\x00\x00\xc3"
        candidate = b"\xe8\x00\x00\x00\x00\xc3"
        aligned = raw.compare(
            candidate, [relocation(1, symbol="FUN_00001020")],
            reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["resolved_relocations"], 1)
        self.assertEqual(aligned["masked_relocation_bytes"], 4)
        self.assertEqual(aligned["matching_bytes"], 2)
        self.assertEqual(aligned["compared_bytes"], 3)

    def test_aligned_relocation_masks_at_independent_operand_offsets(self):
        reference = b"\x8b\x05\x20\x10\x00\x00\xc3"
        candidate = b"\xa1\x00\x00\x00\x00\xc3"
        aligned = raw.compare(
            candidate,
            [relocation(1, raw.IMAGE_REL_I386_DIR32, "FUN_00001020")],
            reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["resolved_relocations"], 1)
        self.assertEqual(aligned["masked_relocation_bytes"], 4)
        self.assertEqual(aligned["matching_bytes"], 1)
        self.assertEqual(aligned["compared_bytes"], 3)

    def test_aligned_relocation_does_not_mask_unresolved_target(self):
        reference = b"\x90\xe8\x1a\x00\x00\x00\xc3"
        candidate = b"\xe8\x00\x00\x00\x00\xc3"
        aligned = raw.compare(
            candidate, [relocation(1, symbol="unknown_target")],
            reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["resolved_relocations"], 0)
        self.assertEqual(aligned["masked_relocation_bytes"], 0)
        self.assertEqual(aligned["unresolved_relocations"], 1)
        self.assertTrue(aligned["accuracy_is_lower_bound"])
        self.assertEqual(aligned["uncertain_relocation_bytes"], 4)
        self.assertAlmostEqual(aligned["byte_accuracy"], 2 / 7)
        self.assertAlmostEqual(aligned["byte_accuracy_upper_bound"], 6 / 7)

    def test_wrong_function_target_counts_four_fixed_mismatch_bytes(self):
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\x00\x00\x00\x00\xc3"
        result = raw.compare(candidate, [relocation(1, symbol="FUN_00001030")],
                             reference, 0x1000)
        aligned = result["aligned_byte_match"]
        self.assertEqual(result["verdict"], "structural differ")
        self.assertEqual(aligned["mismatched_relocations"], 1)
        self.assertEqual(aligned["mismatched_relocation_bytes"], 4)
        self.assertEqual(aligned["uncertain_relocation_bytes"], 0)
        self.assertEqual(aligned["byte_accuracy"], aligned["byte_accuracy_upper_bound"])
        self.assertAlmostEqual(aligned["byte_accuracy"], 2 / 6)
        self.assertEqual(aligned["normalized_exact_instructions"], 1)

    def test_wrong_global_target_counts_four_fixed_mismatch_bytes(self):
        reference = b"\xa1\x20\x10\x00\x00\xc3"
        candidate = b"\xa1\x00\x00\x00\x00\xc3"
        result = raw.compare(
            candidate, [relocation(1, raw.IMAGE_REL_I386_DIR32, "FUN_00001024")],
            reference, 0x1000)
        aligned = result["aligned_byte_match"]
        self.assertEqual(result["verdict"], "structural differ")
        self.assertEqual(aligned["mismatched_relocation_bytes"], 4)
        self.assertEqual(aligned["uncertain_relocation_bytes"], 0)
        self.assertAlmostEqual(aligned["byte_accuracy"], 2 / 6)

    def test_mixed_known_and_unknown_targets_are_not_comparable(self):
        reference = b"\xe8\x1b\x00\x00\x00\xe8\x16\x00\x00\x00\xc3"
        candidate = b"\xe8\x00\x00\x00\x00\xe8\x00\x00\x00\x00\xc3"
        result = raw.compare(candidate, [
            relocation(1, symbol="FUN_00001020"),
            relocation(6, symbol="unknown_target"),
        ], reference, 0x1000)
        aligned = result["aligned_byte_match"]
        self.assertEqual(result["verdict"], "not comparable")
        self.assertEqual(result["identity_evidence"]["status"], "partial")
        self.assertEqual(aligned["resolved_relocations"], 1)
        self.assertEqual(aligned["unresolved_relocations"], 1)
        self.assertEqual(aligned["uncertain_relocation_bytes"], 4)
        self.assertEqual(aligned["mismatched_relocation_bytes"], 0)
        self.assertEqual(aligned["normalized_exact_instructions"], 2)

    def test_exact_ordinary_bytes_with_unknown_relocation_report_range(self):
        reference = b"\xe8\x1b\x00\x00\x00\xc3"
        candidate = b"\xe8\x00\x00\x00\x00\xc3"
        aligned = raw.compare(
            candidate, [relocation(1, symbol="unknown_target")],
            reference, 0x1000)["aligned_byte_match"]
        self.assertEqual(aligned["matching_bytes"], 2)
        self.assertEqual(aligned["compared_bytes"], 6)
        self.assertAlmostEqual(aligned["byte_accuracy"], 1 / 3)
        self.assertEqual(aligned["byte_accuracy_upper_bound"], 1.0)

    def test_reference_only_site_does_not_veto_scoring(self):
        # The raw-XBE sweep sees an absolute operand the candidate has no
        # relocation for; that is a sweep artifact, so the function still scores.
        reference = b"\xa1\x20\x10\x00\x00\xc3"
        candidate = b"\xa1\x20\x10\x00\x00\xc3"
        result = raw.compare(candidate, [], reference, 0x1000)
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["byte_counts"]["relocation_operand"], 0)
        self.assertEqual(result["byte_accuracy"], 1.0)

    def test_dir32_unambiguous_absolute_mov_is_supported(self):
        reference = b"\xa1\x20\x10\x00\x00\xc3"
        candidate = b"\xa1\xaa\xbb\xcc\xdd\xc3"
        result = raw.compare(candidate, [relocation(1, raw.IMAGE_REL_I386_DIR32)],
                             reference, 0x1000, {0x1020: {"logical_target": 0x1020}})
        self.assertEqual(result["verdict"], "structural exact")

    def test_summary_reports_function_and_byte_weighted_metrics(self):
        records = [{
            "schema_version": 2, "lane": "raw_xbe_structural", "verdict": "structural exact",
            "address": "0x00001000", "function": "exact",
            "reference": {"length": 8}, "matching_non_relocation_bytes": 4,
            "byte_counts": {"non_relocation": 4, "relocation_operand": 4},
            "aligned_byte_match": {
                "status": "scored", "matching_bytes": 6, "compared_bytes": 8,
                "uncertain_relocation_bytes": 2, "accuracy_is_provisional": False,
            },
        }, {
            "schema_version": 2, "lane": "raw_xbe_structural", "verdict": "structural differ",
            "address": "0x00001008", "function": "differ",
            "reference": {"length": 4}, "matching_non_relocation_bytes": 2,
            "byte_counts": {"non_relocation": 4, "relocation_operand": 0},
            "aligned_byte_match": {
                "status": "scored", "matching_bytes": 2, "compared_bytes": 4,
                "uncertain_relocation_bytes": 0, "accuracy_is_provisional": True,
            },
        }]
        old_extent = raw.xref.function_extent
        old_xbe = raw.xref.XBE
        old_root = raw.ROOT
        raw.xref.function_extent = lambda address: (address + 4, "table", "table")
        try:
            with tempfile.TemporaryDirectory() as temp:
                raw.ROOT = Path(temp)
                raw.xref.XBE = Path(temp) / "cachebeta.xbe"
                raw.xref.XBE.write_bytes(b"xbe")
                summary = raw._write_summary(records, ["populate", "--workers", "2"], 2, 0)
        finally:
            raw.xref.function_extent = old_extent
            raw.xref.XBE = old_xbe
            raw.ROOT = old_root
        self.assertEqual(summary["schema_version"], 2)
        self.assertEqual(summary["lane"], "raw_xbe_structural")
        self.assertEqual(summary["function_totals"]["structural_exact"], 1)
        self.assertEqual(summary["byte_totals"]["matching_bytes"], 6)
        self.assertEqual(summary["byte_totals"]["masked_bytes"], 4)
        self.assertEqual(summary["byte_totals"]["byte_accuracy"], 0.75)
        self.assertEqual(summary["byte_totals"]["masked_accuracy"], 10 / 12)
        self.assertEqual(summary["aligned_byte_totals"]["scored_functions"], 2)
        self.assertEqual(summary["aligned_byte_totals"]["matching_bytes"], 8)
        self.assertEqual(summary["aligned_byte_totals"]["compared_bytes"], 12)
        self.assertEqual(summary["aligned_byte_totals"]["byte_accuracy_lower"], 8 / 12)
        self.assertEqual(summary["aligned_byte_totals"]["byte_accuracy_upper"], 10 / 12)
        self.assertEqual(summary["aligned_byte_totals"]["provisional_functions"], 1)

    def test_per_function_report_writes_schema2_json_and_sidecar(self):
        record = raw._record_error(
            {"function": "broken", "address": 0x1234}, "candidate extraction failed")
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "broken.json"
            raw._write_record(record, output)
            self.assertTrue(output.is_file())
            self.assertTrue(output.with_suffix(".md").is_file())
            payload = __import__("json").loads(output.read_text(encoding="utf-8"))
            sidecar = output.with_suffix(".md").read_text(encoding="utf-8")
        self.assertEqual(payload["schema_version"], 2)
        self.assertEqual(payload["verdict"], "not comparable")
        self.assertEqual(payload["verification"]["behavior"]["status"], "untested")
        self.assertIn("Compiler and settings", sidecar)
        self.assertIn("Behavior tests in this audit", sidecar)

    def test_error_report_has_provenance_and_all_verification_axes(self):
        record = raw._record_error(
            {"function": "supplied", "address": 0x1234}, "bad object",
            compiler="supplied object; compiler unavailable")
        self.assertEqual(record["schema_version"], 2)
        self.assertEqual(record["tool"]["compiler"],
                         "supplied object; compiler unavailable")
        self.assertTrue(record["generated_at"])
        self.assertEqual(set(record["verification"]), {
            "boundaries", "compiler", "behavior", "instruction_operands", "literal_bytes"})
        self.assertEqual(record["verification"]["boundaries"]["status"], "uncertain")
        self.assertEqual(record["verification"]["instruction_operands"]["status"], "unverified")
        self.assertEqual(record["verification"]["literal_bytes"]["detail"],
                         "literal comparison was not performed")

    def test_single_failure_replaces_stale_success_for_header_and_compile(self):
        old_regen = raw.vc71.regen_decl_header
        old_compile = raw.vc71.compile_vc71
        try:
            for failure in ("header", "compile"):
                with tempfile.TemporaryDirectory() as temp:
                    root = Path(temp)
                    source = root / "candidate.c"
                    source.write_text("/* candidate */", encoding="utf-8")
                    output = root / "result.json"
                    output.write_text(__import__("json").dumps({
                        "schema_version": 2, "verdict": "structural exact",
                    }), encoding="utf-8")
                    if failure == "header":
                        raw.vc71.regen_decl_header = lambda quiet=True: False
                        raw.vc71.compile_vc71 = lambda source, output, opt="/O2": True
                    else:
                        raw.vc71.regen_decl_header = lambda quiet=True: True
                        raw.vc71.compile_vc71 = lambda source, output, opt="/O2": False
                    args = argparse.Namespace(
                        candidate=None, output=output, function="candidate",
                        address=0x1234, source=source)
                    status = raw._run_single(args)
                    payload = __import__("json").loads(
                        output.read_text(encoding="utf-8"))
                    self.assertEqual(status, 2)
                    self.assertEqual(payload["schema_version"], 2)
                    self.assertEqual(payload["verdict"], "not comparable")
                    self.assertIn("failed", payload["reason"])
                    self.assertTrue(output.with_suffix(".md").is_file())
        finally:
            raw.vc71.regen_decl_header = old_regen
            raw.vc71.compile_vc71 = old_compile

    def test_show_rejects_schema1_summary(self):
        old_root = raw.ROOT
        try:
            with tempfile.TemporaryDirectory() as temp:
                raw.ROOT = Path(temp)
                output = raw.ROOT / "artifacts" / "raw_xbe_structural" / "summary.json"
                output.parent.mkdir(parents=True)
                output.write_text(__import__("json").dumps({
                    "schema_version": 1, "lane": "raw_xbe_structural",
                    "totals": {},
                }), encoding="utf-8")
                captured = io.StringIO()
                with contextlib.redirect_stdout(captured):
                    status = raw.main(["show"])
        finally:
            raw.ROOT = old_root
        self.assertEqual(status, 2)
        self.assertIn("stale", captured.getvalue())

    def test_coff_relocation_parse_preserves_type_and_symbol(self):
        code = b"\xe8\x00\x00\x00\x00\xc3"
        text_offset = 60
        reloc_offset = text_offset + len(code)
        symbol_offset = reloc_offset + 10
        header = struct.pack("<HHIIIHH", 0x14C, 1, 0, symbol_offset, 3, 0, 0)
        section = struct.pack("<8sIIIIIIHHI", b".text\0\0\0", 0, 0, len(code), text_offset,
                              reloc_offset, 0, 1, 0, 0x60000020)
        reloc = struct.pack("<IIH", 1, 2, raw.IMAGE_REL_I386_REL32)
        target = struct.pack("<8sIhHBB", b"target\0\0", 0, 0, 0, 2, 0)
        function = struct.pack("<8sIhHBB", b"function", 0, 1, raw.IMAGE_SYM_DTYPE_FUNCTION, 2, 1)
        aux = struct.pack("<IIIIH", 0, len(code), 0, 0, 0)
        image = header + section + code + reloc + function + aux + target + struct.pack("<I", 13) + b"function\0"
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "candidate.obj"
            path.write_bytes(image)
            parsed, relocs, _ = raw._parse_coff(path, "function")
        self.assertEqual(parsed, code)
        self.assertEqual(relocs[0]["type_name"], "REL32")
        self.assertEqual(relocs[0]["symbol"]["name"], "target")

    def test_populate_selection_covers_canonical_ported_functions(self):
        import json
        kb = json.loads((raw.ROOT / "kb.json").read_text(encoding="utf-8"))
        expected = {int(entry["addr"], 0)
                    for obj in kb.get("objects", [])
                    for entry in obj.get("functions", [])
                    if entry.get("ported") is True and entry.get("addr")}
        items = raw._eligible_functions()
        self.assertEqual({item["address"] for item in items}, expected)
        missing = [item for item in items if item.get("source") is None]
        self.assertLessEqual(len(missing), 1)
        if missing:
            self.assertEqual(missing[0]["function"],
                             "_rasterizer_environment_diffuse_textures_end")
            self.assertIn("source file is missing", missing[0]["selection_reason"])

    def test_summary_reports_planned_tus_and_missing_source_count(self):
        old_root = raw.ROOT
        try:
            with tempfile.TemporaryDirectory() as temp:
                raw.ROOT = Path(temp)
                eligible = [
                    {"function": "backed", "address": 0x1000,
                     "source": Path(temp) / "backed.c"},
                    {"function": "missing", "address": 0x1004,
                     "source": None},
                ]
                records = [raw._record_error(eligible[1], "ported function has no source path")]
                summary = raw._write_summary(records, ["populate"], 1, 0, eligible)
        finally:
            raw.ROOT = old_root
        self.assertEqual(summary["planned_tu_count"], 1)
        self.assertEqual(summary["planned_function_count"], 2)
        self.assertEqual(summary["missing_source_count"], 1)
        self.assertEqual(summary["audited_function_count"], 1)


def literal_relocation(offset, content, symbol="??_C@_03literal"):
    item = relocation(offset, raw.IMAGE_REL_I386_DIR32, symbol)
    item["literal"] = {"section": ".rdata", "content": content}
    return item


def _coff_with_literal(section_flags):
    """One-function COFF whose DIR32 relocation targets a literal in section 2."""
    code = b"\xa1\x00\x00\x00\x00\xc3"
    literal = b"hi\0"
    text_offset = 20 + 2 * 40
    reloc_offset = text_offset + len(code)
    rdata_offset = reloc_offset + 10
    symbol_offset = rdata_offset + len(literal)
    header = struct.pack("<HHIIIHH", 0x14C, 2, 0, symbol_offset, 5, 0, 0)
    text = struct.pack("<8sIIIIIIHHI", b".text\0\0\0", 0, 0, len(code), text_offset,
                       reloc_offset, 0, 1, 0, 0x60000020)
    rdata = struct.pack("<8sIIIIIIHHI", b".rdata\0\0", 0, 0, len(literal), rdata_offset,
                        0, 0, 0, 0, section_flags)
    reloc = struct.pack("<IIH", 1, 4, raw.IMAGE_REL_I386_DIR32)
    function = struct.pack("<8sIhHBB", b"function", 0, 1, raw.IMAGE_SYM_DTYPE_FUNCTION, 2, 1)
    function_aux = struct.pack("<IIIIH", 0, len(code), 0, 0, 0)
    section_symbol = struct.pack("<8sIhHBB", b".rdata\0\0", 0, 2, 0, 3, 1)
    section_aux = struct.pack("<IIIIH", len(literal), 0, 0, 0, 0)
    literal_symbol = struct.pack("<8sIhHBB", b"??_C@lit", 0, 2, 0, 2, 0)
    return (header + text + rdata + code + reloc + literal + function + function_aux +
            section_symbol + section_aux + literal_symbol + struct.pack("<I", 4))


class RelocationIdentityResolverTest(unittest.TestCase):
    """Data globals and read-only literals resolve instead of staying unknown."""

    REFERENCE = b"\xa1\x20\x10\x00\x00\xc3"
    CANDIDATE = b"\xa1\x00\x00\x00\x00\xc3"

    def _compare(self, relocations, reference=None):
        return raw.compare(self.CANDIDATE, relocations, reference or self.REFERENCE, 0x1000)

    def test_kb_data_global_resolves_by_address(self):
        with mock.patch.object(raw, "_kb_data_addresses", return_value={"actor_data": 0x1020}):
            result = self._compare([relocation(1, raw.IMAGE_REL_I386_DIR32, "_actor_data")])
        self.assertEqual(result["verdict"], "structural exact")
        self.assertEqual(result["identity_evidence"]["records"][0]["method"], "kb_data_address")

    def test_kb_data_global_at_other_address_is_mismatch(self):
        with mock.patch.object(raw, "_kb_data_addresses", return_value={"actor_data": 0x1024}):
            result = self._compare([relocation(1, raw.IMAGE_REL_I386_DIR32, "_actor_data")])
        self.assertEqual(result["verdict"], "structural differ")
        self.assertEqual(result["identity_evidence"]["status"], "mismatch")

    def test_equal_read_only_literal_resolves(self):
        with mock.patch.object(raw, "_xbe_rdata_bytes",
                               side_effect=lambda va, n: b"hi\0" if va == 0x1020 else None):
            result = self._compare([literal_relocation(1, b"hi\0")])
        self.assertEqual(result["verdict"], "structural exact")
        record = result["identity_evidence"]["records"][0]
        self.assertEqual(record["method"], "read_only_literal_content")
        self.assertEqual(record["symbol_address"], "0x00001020")

    def test_literal_addend_maps_back_to_literal_start(self):
        candidate = b"\xa1\x01\x00\x00\x00\xc3"
        with mock.patch.object(raw, "_xbe_rdata_bytes",
                               side_effect=lambda va, n: b"hi\0" if va == 0x101f else None):
            result = raw.compare(candidate, [literal_relocation(1, b"hi\0")],
                                 self.REFERENCE, 0x1000)
        self.assertEqual(result["verdict"], "structural exact")

    def test_different_literal_content_is_mismatch(self):
        with mock.patch.object(raw, "_xbe_rdata_bytes", return_value=b"ho\0"):
            result = self._compare([literal_relocation(1, b"hi\0")])
        self.assertEqual(result["verdict"], "structural differ")
        self.assertEqual(result["identity_evidence"]["status"], "mismatch")
        self.assertEqual(result["aligned_byte_match"]["mismatched_relocation_bytes"], 4)

    def test_literal_outside_rdata_stays_unresolved(self):
        with mock.patch.object(raw, "_xbe_rdata_bytes", return_value=None):
            result = self._compare([literal_relocation(1, b"hi\0")])
        self.assertEqual(result["verdict"], "not comparable")
        self.assertEqual(result["identity_evidence"]["status"], "unresolved")

    def test_injected_identity_still_wins_over_literal_content(self):
        with mock.patch.object(raw, "_xbe_rdata_bytes", return_value=b"hi\0"):
            result = raw.compare(self.CANDIDATE, [literal_relocation(1, b"hi\0")],
                                 self.REFERENCE, 0x1000, {0x1020: {"logical_target": 0x9999}})
        self.assertNotEqual(result["verdict"], "structural exact")

    def test_coff_parse_attaches_read_only_comdat_literal(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "candidate.obj"
            path.write_bytes(_coff_with_literal(0x40301040))
            _code, relocs, _ = raw._parse_coff(path, "function")
        self.assertEqual(relocs[0]["literal"], {"section": ".rdata", "content": b"hi\0"})

    def test_coff_parse_ignores_writable_data(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "candidate.obj"
            path.write_bytes(_coff_with_literal(0xC0301040))
            _code, relocs, _ = raw._parse_coff(path, "function")
        self.assertNotIn("literal", relocs[0])

    def test_data_decl_name_matches_knowledge_rule(self):
        self.assertEqual(raw._data_decl_name("char player_ui_globals[0x230];"), "player_ui_globals")
        self.assertEqual(raw._data_decl_name("data_t *actor_data;"), "actor_data")

    def test_exact_c_name_beats_lenient_underscore_fold(self):
        # `_data_packet_decode` (0x11b2a0) and `data_packet_decode` (0x11b750)
        # both fold to one lenient name; the exact C name must disambiguate.
        bounds = {0x11b2a0: {"name": "_data_packet_decode"},
                  0x11b750: {"name": "data_packet_decode"}}
        with mock.patch.object(raw.xref, "_bounds", return_value=bounds):
            self.assertEqual(raw._raw_addresses_for_name("__data_packet_decode"), [0x11b2a0])
            self.assertEqual(raw._raw_addresses_for_name("_data_packet_decode"), [0x11b750])

    def test_lenient_fallback_still_resolves(self):
        bounds = {0x1dd620: {"name": "__allmul"}}
        with mock.patch.object(raw.xref, "_bounds", return_value=bounds):
            self.assertEqual(raw._raw_addresses_for_name("__allmul"), [0x1dd620])

    @staticmethod
    def _local(offset, local_offset):
        return {"offset": offset, "type": raw.IMAGE_REL_I386_DIR32,
                "local_offset": local_offset, "symbol": {"name": "$L1"}}

    def test_inline_switch_table_is_cut_with_pad(self):
        # jmp dword ptr [eax*4 + $L_table]; ret; nop; then two table entries.
        code = (b"\xff\x24\x85" + struct.pack("<I", 0) + b"\xc3\x90" +
                struct.pack("<I", 0) + struct.pack("<I", 0))
        relocs = [self._local(3, 9),        # table base -> offset 9
                  self._local(9, 7),        # entry -> case label (the ret)
                  self._local(13, 7)]
        cut, kept, table = raw._strip_inline_switch_tables(code, relocs)
        self.assertEqual(cut, code[:8])
        self.assertEqual([r["offset"] for r in kept], [3])
        self.assertEqual(table, {"offset": 9, "table_bytes": 8, "pad_bytes": 1})

    def test_case_label_reference_is_not_a_table_start(self):
        # Only table entries point at the case label; with no code reference in
        # front of a trailing table, nothing may be cut.
        code = b"\xc3" * 4 + struct.pack("<I", 0)
        relocs = [self._local(4, 1)]
        self.assertIsNone(raw._strip_inline_switch_tables(code, relocs)[2])

    def test_code_relocation_after_label_blocks_cut(self):
        code = (b"\xff\x24\x85" + struct.pack("<I", 0) +
                b"\xe8" + struct.pack("<i", 0) + b"\xc3")
        relocs = [self._local(3, 7),
                  {"offset": 8, "type": raw.IMAGE_REL_I386_REL32, "symbol": {"name": "_f"}}]
        self.assertIsNone(raw._strip_inline_switch_tables(code, relocs)[2])

    def test_pad_instruction_forms(self):
        for mnemonic, op_str in (("nop", ""), ("int3", ""), ("mov", "edi, edi"),
                                 ("lea", "ecx, [ecx]"), ("lea", "esp, [esp + 0]")):
            self.assertTrue(raw._is_pad_instruction({"mnemonic": mnemonic, "op_str": op_str}))
        self.assertFalse(raw._is_pad_instruction({"mnemonic": "lea", "op_str": "ecx, [eax]"}))
        self.assertFalse(raw._is_pad_instruction({"mnemonic": "jmp", "op_str": "0x10"}))

    def test_table_data_reference_cut_at_table_base(self):
        # jmp dword ptr [eax*4 + 0x1008]; ret; then a table whose first entry
        # decodes as `ret 0x12a3` -- the cut must be the table base, not it.
        code = (b"\xff\x24\x85" + struct.pack("<I", 0x1008) + b"\xc3" +
                b"\xc2\xa3\x12\x00" + struct.pack("<I", 0x1007))
        with mock.patch.object(raw.xref, "function_bytes", return_value=(code, None)), \
                mock.patch.object(raw.xref, "function_extent",
                                  return_value=(0x1000 + len(code), "table_data", "table")):
            self.assertEqual(raw.reference_code(0x1000), (code[:8], None))

    def test_crt_helper_named_fun_in_bounds(self):
        bounds = {0x1d90e0: {"name": "FUN_001d90e0"}}
        with mock.patch.object(raw.xref, "_bounds", return_value=bounds):
            self.assertEqual(raw._raw_addresses_for_name("__chkstk"), [0x1d90e0])


if __name__ == "__main__":
    unittest.main()


class ReferenceProvenanceTest(unittest.TestCase):
    """The reference hash must identify the pristine XBE file, not the span.

    Recording the function's own span hash there looks plausible and is what the
    lane originally did, but the report validates it against the XBE file hash,
    so every record silently failed validation and the dashboard column stayed
    empty.  Both audit lanes must agree on what this field means.
    """

    def test_reference_sha256_is_the_xbe_file_hash(self):
        import raw_byte_audit as strict
        import xbe_reference as xref
        if not pathlib.Path(xref.XBE).is_file():
            self.skipTest("pristine XBE is not available")
        expected = strict.sha256_file(xref.XBE)
        self.assertEqual(raw._hash_path(xref.XBE), expected)

    def test_span_hash_is_not_used_as_reference_provenance(self):
        import xbe_reference as xref
        if not pathlib.Path(xref.XBE).is_file():
            self.skipTest("pristine XBE is not available")
        span = raw._sha256(b"\x90\xc3")
        self.assertNotEqual(raw._hash_path(xref.XBE), span)


class RegisterArgumentFlagTest(unittest.TestCase):
    """Register-argument functions are flagged, and still scored.

    They ARE scored: the patched binary genuinely does not reproduce the
    original's bytes for them, so excluding them would report our accuracy as
    higher than it is.  The flag records why the number is low.
    """

    def setUp(self):
        raw._REG_ARG_CACHE.clear()

    def tearDown(self):
        raw._REG_ARG_CACHE.clear()

    def test_reg_arg_addresses_come_from_kb_decls(self):
        addresses = raw._register_argument_addresses()
        self.assertIsInstance(addresses, set)
        # The repository has a substantial @<reg> population; an empty set would
        # silently disable the exclusion.
        self.assertGreater(len(addresses), 0)

    def test_every_reg_arg_address_declares_a_register(self):
        import json
        addresses = raw._register_argument_addresses()
        kb = json.loads((raw.ROOT / "kb.json").read_text(encoding="utf-8"))
        decls = {}
        for obj in kb.get("objects", []):
            for fn in obj.get("functions", []):
                if fn.get("addr"):
                    decls[int(fn["addr"], 0)] = fn.get("decl") or ""
        for address in list(addresses)[:50]:
            if address in decls:
                self.assertIn("@<", decls[address])


def _insn(code, offset=0):
    return raw._decode_instructions(bytes.fromhex(code))[0] | {"offset": offset}


class DifferenceClassificationTest(unittest.TestCase):
    def classify(self, candidate, reference):
        return raw._classify_aligned_difference(_insn(candidate), _insn(reference))

    def test_register_choice(self):
        # mov eax, [ebp+8]  vs  mov ecx, [ebp+8]
        self.assertEqual(self.classify("8b4508", "8b4d08"), "register")

    def test_operand_order(self):
        # cmp eax, ecx  vs  cmp ecx, eax
        self.assertEqual(self.classify("39c8", "39c1"), "operand_order")

    def test_stack_offset(self):
        # mov eax, [ebp-4]  vs  mov eax, [ebp-8]
        self.assertEqual(self.classify("8b45fc", "8b45f8"), "stack_offset")

    def test_immediate(self):
        # push 0x10  vs  push 0x20
        self.assertEqual(self.classify("6a10", "6a20"), "immediate")

    def test_branch_target(self):
        # jmp +2  vs  jmp +4
        self.assertEqual(self.classify("eb00", "eb02"), "branch_target")

    def test_branch_with_same_printed_target_is_still_a_branch(self):
        # Both print "jne 0x30" once each is decoded at its own offset.
        candidate = _insn("752e", 0) | {"op_str": "0x30"}
        reference = _insn("75e8", 0) | {"op_str": "0x30"}
        self.assertEqual(raw._classify_aligned_difference(candidate, reference), "branch_target")

    def test_unmatched_stack_param_load(self):
        self.assertEqual(raw._classify_unmatched(_insn("8b4508")), "stack_param_load")
        self.assertEqual(raw._classify_unmatched(_insn("8b45fc")), "instruction")
        self.assertEqual(raw._classify_unmatched(_insn("56")), "register_save")

    def test_aligned_compare_records_classes(self):
        # candidate: mov eax,[ebp+8]; ret   reference: mov ecx,[ebp+8]; ret
        result = raw.aligned_byte_compare(bytes.fromhex("8b4508c3"), [],
                                          bytes.fromhex("8b4d08c3"), 0x1000)
        self.assertEqual(result["difference_classes"], {"register": 1})
        self.assertEqual(result["differences"][0]["candidate"], "mov eax, dword ptr [ebp + 8]")
        self.assertFalse(result["differences_truncated"])
        self.assertEqual(result["raw_operand_matching_instructions"], 1)
        self.assertEqual(result["raw_operand_compared_instructions"], 2)
        self.assertEqual(result["raw_operand_accuracy"], 0.5)

    def test_raw_operand_score_detects_masked_values(self):
        # push 0x10 vs push 0x20: same masked operand shape, different raw value.
        result = raw.aligned_byte_compare(bytes.fromhex("6a10c3"), [],
                                          bytes.fromhex("6a20c3"), 0x1000)
        self.assertEqual(result["raw_operand_accuracy"], 0.5)
        self.assertEqual(result["difference_classes"], {"immediate": 1})

    def test_raw_operand_score_excludes_relocated_instructions(self):
        # No comparison is meaningful until a candidate COFF relocation can be
        # resolved against the XBE target, so this call's unresolved relocation
        # leaves only RET in the raw-operand denominator.
        reloc = {"offset": 1, "type": 0x14, "symbol_name": "unknown",
                 "symbol_value": 0, "section_number": 0, "literal": None}
        result = raw.aligned_byte_compare(bytes.fromhex("e800000000c3"), [reloc],
                                          bytes.fromhex("e800000000c3"), 0x1000)
        self.assertEqual(result["raw_operand_compared_instructions"], 1)
        self.assertEqual(result["raw_operand_accuracy"], 1.0)


class RegisterArgumentResidualTest(unittest.TestCase):
    def test_only_param_loads_is_exact_except_abi(self):
        residual = raw._register_argument_residual({
            "status": "scored", "difference_classes": {"candidate_only:stack_param_load": 2}})
        self.assertEqual(residual["status"], "exact_except_register_abi")

    def test_other_differences_are_reported(self):
        residual = raw._register_argument_residual({
            "status": "scored", "difference_classes": {"candidate_only:stack_param_load": 1,
                                                       "register": 3}})
        self.assertEqual(residual["status"], "other_differences")
        self.assertEqual(residual["other_classes"], {"register": 3})

    def test_mismatched_relocation_blocks_exact(self):
        residual = raw._register_argument_residual({
            "status": "scored", "difference_classes": {}, "mismatched_relocations": 1})
        self.assertEqual(residual["status"], "other_differences")


class PerFunctionOptTest(unittest.TestCase):
    def test_override_selects_its_flags_and_object(self):
        source = Path("/repo/src/halo/bink/bink.c")
        with mock.patch.object(raw.vc71, "_per_function_opt_for",
                               return_value={"BinkOpen": "/O2 /Oy"}):
            self.assertEqual(raw._function_opt(source, "BinkOpen"), "/O2 /Oy")
            self.assertEqual(raw._function_opt(source, "other"), raw.DEFAULT_OPT)
        default = raw._candidate_object(Path("/a"), source, raw.DEFAULT_OPT)
        override = raw._candidate_object(Path("/a"), source, "/O2 /Oy")
        self.assertNotEqual(default, override)
        self.assertTrue(override.name.endswith(".optO2Oy.obj"))

    def test_audit_tu_compiles_each_flag_group_once(self):
        calls = []

        def fake_compile(source, output, opt="/O2"):
            calls.append(opt)
            return False

        items = [{"function": "a", "address": 1, "source": Path("/s.c")},
                 {"function": "b", "address": 2, "source": Path("/s.c")},
                 {"function": "c", "address": 3, "source": Path("/s.c")}]
        with mock.patch.object(raw.vc71, "_per_function_opt_for", return_value={"b": "/O2 /Oy"}), \
                mock.patch.object(raw.vc71, "compile_vc71", side_effect=fake_compile), \
                mock.patch.object(raw, "_hash_path", return_value=None):
            records = raw._audit_tu(Path("/s.c"), items, Path("/tmp"), None)
        self.assertEqual(sorted(calls), ["/O2", "/O2 /Oy"])
        self.assertEqual(len(records), 3)
        self.assertTrue(all(r["reason"].startswith("VC71 compilation failed") for _i, r in records))
