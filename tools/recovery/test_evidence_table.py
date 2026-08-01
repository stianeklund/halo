"""Standard-library tests for the struct-recovery evidence table artifact."""

import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tools.recovery import evidence_table as et


def table(**overrides):
    """A minimal valid table; overrides replace top-level keys."""
    base = {
        "schema": 1,
        "struct": "sample_t",
        "size": {"value": "0x08", "evidence": "csmemset 0x8 @FUN_00001000"},
        "sources": ["FUN_00001000"],
        "fields": [
            {"offset": "0x00", "width": 2, "signed": True, "name": "count",
             "confidence": "named", "evidence": "assert \"count>=0\""},
            {"offset": "0x04", "width": 4, "kind": "float", "confidence": "typed",
             "evidence": "FLD [esi+4]"},
        ],
    }
    base.update(overrides)
    return json.loads(json.dumps(base))


def fields(*entries):
    return table(fields=[json.loads(json.dumps(entry)) for entry in entries])


class SelfTestTests(unittest.TestCase):
    def test_script_self_test_passes(self):
        script = et.ROOT / "tools" / "recovery" / "evidence_table.py"
        result = subprocess.run([sys.executable, str(script), "--self-test"],
                               cwd=et.ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("FAIL", result.stdout)


class ValidatorTests(unittest.TestCase):
    def errors(self, blob):
        return et.validate(blob)[0]

    def assertRejects(self, blob, needle):
        found = self.errors(blob)
        self.assertTrue(any(needle in error for error in found),
                        "expected %r in %s" % (needle, found))

    def test_minimal_table_validates(self):
        self.assertEqual(self.errors(table()), [])

    def test_top_level_must_be_object(self):
        self.assertRejects([], "expected a JSON object")

    def test_schema_version_is_pinned(self):
        self.assertRejects(table(schema=2), "\"schema\" must be 1")

    def test_struct_name_must_be_identifier(self):
        self.assertRejects(table(struct="packet definition"), "must be a C identifier")

    def test_unknown_top_level_key_rejected(self):
        self.assertRejects(table(fileds=[]), "unknown key(s) fileds")

    def test_sources_required_and_nonempty(self):
        missing = table()
        del missing["sources"]
        self.assertRejects(missing, "missing required key 'sources'")
        self.assertRejects(table(sources=[]), "non-empty list of provenance")

    def test_size_requires_value_and_evidence(self):
        self.assertRejects(table(size={"value": "0x08"}), "exactly \"value\" and \"evidence\"")
        self.assertRejects(table(size={"value": "0x08", "evidence": " "}),
                           "requires an \"evidence\" citation")
        self.assertRejects(table(stride={"value": 0, "evidence": "IMUL 0"}), "must be positive")

    def test_fields_must_be_nonempty(self):
        self.assertRejects(table(fields=[]), "non-empty list")

    def test_offset_must_be_hex_string(self):
        self.assertRejects(fields({"offset": 4, "width": 4, "signed": False,
                                   "confidence": "typed", "evidence": "e"}),
                           "offset must be a hex string")

    def test_width_must_be_1_2_4_8(self):
        self.assertRejects(fields({"offset": "0x00", "width": 3, "signed": False,
                                   "confidence": "typed", "evidence": "e"}),
                           "width must be one of")

    def test_integer_signedness_must_be_explicit(self):
        self.assertRejects(fields({"offset": "0x00", "width": 4,
                                   "confidence": "typed", "evidence": "e"}),
                           "requires an explicit \"signed\"")

    def test_width_eight_integer_points_at_concat_packing(self):
        self.assertRejects(fields({"offset": "0x00", "width": 8, "signed": False,
                                   "confidence": "typed", "evidence": "e"}),
                           "CONCAT packing")
        self.assertEqual(self.errors(fields({"offset": "0x00", "width": 8,
                                             "kind": "float64", "confidence": "typed",
                                             "evidence": "FLD qword"})), [])

    def test_float_and_pointer_widths_are_fixed(self):
        self.assertRejects(fields({"offset": "0x00", "width": 2, "kind": "float",
                                   "confidence": "typed", "evidence": "e"}),
                           "kind 'float' requires width 4")
        self.assertRejects(fields({"offset": "0x00", "width": 2, "kind": "pointer",
                                   "confidence": "typed", "evidence": "e"}),
                           "kind 'pointer' requires width 4")
        self.assertRejects(fields({"offset": "0x00", "width": 4, "signed": False,
                                   "points_to": "void", "confidence": "typed",
                                   "evidence": "e"}),
                           "only applies to kind 'pointer'")

    def test_named_confidence_requires_name_and_evidence(self):
        self.assertRejects(fields({"offset": "0x00", "width": 4, "signed": False,
                                   "confidence": "named", "evidence": "e"}),
                           "requires a \"name\"")
        self.assertRejects(fields({"offset": "0x00", "width": 4, "signed": False,
                                   "confidence": "named", "name": "flags"}),
                           "requires a one-line \"evidence\"")

    def test_typed_confidence_requires_evidence(self):
        self.assertRejects(fields({"offset": "0x00", "width": 4, "signed": False,
                                   "confidence": "typed"}),
                           "requires a one-line \"evidence\"")

    def test_named_gap_rejected(self):
        self.assertRejects(fields({"offset": "0x00", "width": 4, "confidence": "gap",
                                   "name": "reserved"}), "must not carry a name")

    def test_unnamed_gap_needs_neither_name_nor_signedness(self):
        self.assertEqual(self.errors(fields({"offset": "0x00", "width": 4,
                                             "confidence": "gap"})), [])

    def test_bad_confidence_value_rejected(self):
        self.assertRejects(fields({"offset": "0x00", "width": 4, "signed": False,
                                   "confidence": "guessed", "evidence": "e"}),
                           "confidence must be one of")

    def test_offsets_must_be_strictly_increasing(self):
        self.assertRejects(fields({"offset": "0x04", "width": 4, "signed": False,
                                   "confidence": "typed", "evidence": "e"},
                                  {"offset": "0x00", "width": 2, "signed": False,
                                   "confidence": "typed", "evidence": "e"}),
                           "increasing offset order")
        self.assertRejects(fields({"offset": "0x00", "width": 2, "signed": False,
                                   "confidence": "typed", "evidence": "e"},
                                  {"offset": "0x00", "width": 2, "signed": False,
                                   "confidence": "typed", "evidence": "e2"}),
                           "must be strictly")

    def test_fields_may_not_overlap(self):
        self.assertRejects(fields({"offset": "0x00", "width": 4, "signed": False,
                                   "confidence": "typed", "evidence": "e"},
                                  {"offset": "0x02", "width": 2, "signed": False,
                                   "confidence": "typed", "evidence": "e"}),
                           "overlaps")

    def test_array_span_participates_in_overlap(self):
        self.assertRejects(fields({"offset": "0x00", "width": 4, "array_len": 2,
                                   "kind": "float", "confidence": "typed", "evidence": "e"},
                                  {"offset": "0x04", "width": 4, "kind": "float",
                                   "confidence": "typed", "evidence": "e"}),
                           "overlaps")
        self.assertRejects(fields({"offset": "0x00", "width": 4, "array_len": 0,
                                   "kind": "float", "confidence": "typed", "evidence": "e"}),
                           "array_len must be a positive integer")

    def test_last_field_must_fit_declared_size(self):
        self.assertRejects(fields({"offset": "0x00", "width": 4, "array_len": 3,
                                   "kind": "float", "confidence": "typed", "evidence": "e"}),
                           "past the declared size")

    def test_duplicate_member_names_rejected(self):
        self.assertRejects(fields({"offset": "0x00", "width": 2, "signed": True,
                                   "name": "count", "confidence": "named", "evidence": "e"},
                                  {"offset": "0x04", "width": 2, "signed": True,
                                   "name": "count", "confidence": "named", "evidence": "e"}),
                           "duplicate member name")

    def test_union_rules(self):
        union = {"offset": "0x04", "size": 4, "name": "value",
                 "discriminator": "type at 0x00",
                 "arms": [{"name": "as_index", "width": 4, "signed": True, "evidence": "A"},
                          {"name": "as_scale", "width": 4, "kind": "float", "evidence": "B"}]}
        head = [table()["fields"][0]]
        self.assertEqual(self.errors(table(fields=head, unions=[union])), [])
        self.assertRejects(table(fields=head, unions=[dict(union, arms=union["arms"][:1])]),
                           "at least 2 arms")
        self.assertRejects(table(fields=head, unions=[dict(union, discriminator="")]),
                           "\"discriminator\" is required")
        self.assertRejects(table(fields=head, unions=[dict(union, size=8)]),
                           "declared footprint 8 != the C sizeof")
        self.assertRejects(table(fields=head, unions=[dict(union, offset="0x00")]),
                           "duplicates")
        armless = dict(union, arms=[dict(union["arms"][0], evidence=""), union["arms"][1]])
        self.assertRejects(table(fields=head, unions=[armless]),
                           "every arm needs its own \"evidence\"")

    def test_warnings_do_not_block(self):
        unproven = table()
        del unproven["size"]
        errors, warnings = et.validate(unproven)
        self.assertEqual(errors, [])
        self.assertTrue(any("size unproven" in warning for warning in warnings))
        packed = fields({"offset": "0x01", "width": 4, "signed": False,
                         "confidence": "typed", "evidence": "e"})
        errors, warnings = et.validate(packed)
        self.assertEqual(errors, [])
        self.assertTrue(any("pragma pack" in warning for warning in warnings))


class RendererTests(unittest.TestCase):
    def test_render_is_deterministic(self):
        first = et.render(table())
        for _attempt in range(3):
            self.assertEqual(et.render(table()), first)

    def test_render_shape(self):
        text = et.render(table())
        self.assertIn("typedef struct sample_t {", text)
        self.assertIn("} sample_t;", text)
        self.assertIn("cs(sample_t, 0x08);", text)
        self.assertIn("co(sample_t, count, 0x00);", text)
        self.assertIn("uint8_t pad_02[2];", text)          # implicit hole
        self.assertNotIn("co(sample_t, pad_02", text)      # derived pad: no assert
        self.assertTrue(text.endswith("\n"))

    def test_unnamed_field_becomes_field_offset(self):
        text = et.render(fields({"offset": "0x00", "width": 4, "signed": False,
                                 "confidence": "typed", "evidence": "unknown"}))
        self.assertIn("uint32_t field_00;", text)
        self.assertIn("co(sample_t, field_00, 0x00);", text)

    def test_explicit_gap_is_padding_and_asserted(self):
        text = et.render(fields({"offset": "0x00", "width": 4, "confidence": "gap"}))
        self.assertIn("uint8_t pad_00[4];", text)
        self.assertIn("co(sample_t, pad_00, 0x00);", text)

    def test_size_unproven_comment_replaces_cs(self):
        unproven = table()
        del unproven["size"]
        text = et.render(unproven)
        self.assertNotIn("cs(", text)
        self.assertIn("/* size unproven - largest observed access at 0x08 */", text)

    def test_pointer_and_array_declarations(self):
        text = et.render(fields({"offset": "0x00", "width": 4, "kind": "pointer",
                                 "points_to": "const char", "name": "name",
                                 "confidence": "named", "evidence": "assert"},
                                {"offset": "0x04", "width": 4, "kind": "float",
                                 "array_len": 1, "confidence": "typed", "evidence": "FLD"}))
        self.assertIn("const char *name;", text)
        self.assertIn("float field_04[1];", text)

    def test_union_renders_named_c89_member(self):
        union = {"offset": "0x04", "size": 4, "name": "value",
                 "discriminator": "type at 0x00",
                 "arms": [{"name": "as_index", "width": 4, "signed": True, "evidence": "A"},
                          {"name": "as_scale", "width": 4, "kind": "float", "evidence": "B"}]}
        text = et.render(table(fields=[table()["fields"][0]], unions=[union]))
        self.assertIn("union {  /* discriminator: type at 0x00 */", text)
        self.assertIn("int32_t as_index;", text)
        self.assertIn("} value;", text)
        self.assertIn("co(sample_t, value, 0x04);", text)

    def test_offsets_use_uppercase_hex_and_identifiers_lowercase(self):
        text = et.render(fields({"offset": "0x0a", "width": 2, "signed": False,
                                 "confidence": "typed", "evidence": "MOVZX"}))
        self.assertIn("uint16_t field_0a;", text)
        self.assertIn("co(sample_t, field_0a, 0x0A);", text)


class WorkedExampleTests(unittest.TestCase):
    """The committed artifact must round-trip against the real asserted struct."""

    path = et.ROOT / "recovery" / "evidence" / "packet_definition.json"

    def setUp(self):
        self.table = et.load(self.path)

    def test_artifact_validates(self):
        errors, _warnings = et.validate(self.table)
        self.assertEqual(errors, [])

    def test_cli_validate_and_render_succeed(self):
        script = et.ROOT / "tools" / "recovery" / "evidence_table.py"
        for command in ("validate", "render"):
            result = subprocess.run([sys.executable, str(script), command, str(self.path)],
                                    cwd=et.ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_render_matches_types_h_layout(self):
        """Offsets, widths and padding must agree with src/types.h:777-794."""
        rendered = et.render(self.table)
        source = (et.ROOT / "src" / "types.h").read_text(encoding="utf-8")
        block = source.split("} packet_definition;")[0].split("typedef struct")[-1]
        existing = re.findall(r"^\s*(.+?)\s+\**(\w+);\s*///< offset=(0x[0-9A-Fa-f]+)",
                              block, re.MULTILINE)
        self.assertTrue(existing, "could not locate packet_definition in src/types.h")
        for _type, name, offset in existing:
            self.assertIn("co(packet_definition, %s, %s);"
                          % (name, et._fmt_off(int(offset, 16))), rendered)
        self.assertIn("cs(packet_definition, 0x14);", rendered)
        # Widths must agree too: these declarations are verbatim src/types.h.
        for declaration in ("const char *name;", "uint32_t field_04;", "int16_t size;",
                            "int16_t version;", "int16_t *fields;", "uint8_t validated;"):
            self.assertIn(declaration, rendered)
        # types.h leaves the tail implicit; the artifact spells it out.
        self.assertIn("uint8_t pad_11[3];", rendered)

    def test_missing_artifact_is_a_clean_error(self):
        with tempfile.TemporaryDirectory(dir=et.ROOT) as temp:
            with self.assertRaises(et.EvidenceError):
                et.load(Path(temp) / "nope.json")


if __name__ == "__main__":
    unittest.main()
