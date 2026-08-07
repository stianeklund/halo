"""In-memory tests for mechanical struct refinement and offset rewriting.

These cover the decision logic only.  The layout dump, compile and object diff
are exercised for real by the pilot workflow, not mocked here -- a mocked
compiler would test the mock.
"""

import shutil
import tempfile
import unittest
from pathlib import Path

from tools.recovery.structize import (
    _addr_pattern,
    _classify,
    _deref_pattern,
    _describe,
    _self_test,
    apply_rewrites,
    field_covering,
    field_name_for,
    hex_width,
    function_spans,
    plan_splits,
    scalar_of,
    site_outcomes,
)


def _layout():
    """A small packed struct standing in for a recovered pool element."""
    return {
        "struct": "t", "size": 0x20,
        "fields": {
            0x0: {"offset": 0, "name": "salt", "type": "int16_t", "kind": "int",
                  "width": 2, "signed": True, "count": 1, "is_pad": False, "span": 2},
            0x2: {"offset": 2, "name": "pad_02", "type": "char", "kind": "int",
                  "width": 1, "signed": None, "count": 0x0E, "is_pad": True, "span": 0x0E},
            0x10: {"offset": 0x10, "name": "vec", "type": "float", "kind": "float",
                   "width": 4, "signed": None, "count": 3, "is_pad": False, "span": 12},
            0x1C: {"offset": 0x1C, "name": "flags", "type": "uint32_t", "kind": "int",
                   "width": 4, "signed": False, "count": 1, "is_pad": False, "span": 4},
        },
    }


def _site(line, base="p", taking_address=False, layout=None):
    layout = layout or _layout()
    pattern = _addr_pattern(base) if taking_address else _deref_pattern(base)
    match = pattern.search(line)
    assert match, "pattern did not match: %s" % line
    return _classify(layout, 1, line, match, taking_address)


class SelfTestTests(unittest.TestCase):
    def test_builtin_self_test_passes(self):
        self.assertEqual(_self_test(), 0)


class ScalarModelTests(unittest.TestCase):
    def test_signedness_is_tracked(self):
        self.assertEqual(scalar_of("unsigned short"), ("int", 2, False))
        self.assertEqual(scalar_of("int16_t"), ("int", 2, True))

    def test_char_signedness_is_unknown_not_assumed(self):
        self.assertIsNone(scalar_of("char")[2])

    def test_pointers_are_not_scalars(self):
        self.assertIsNone(scalar_of("foo_t *"))
        self.assertIsNone(scalar_of("void *"))

    def test_describe_does_not_double_prefix(self):
        self.assertEqual(_describe({"kind": "int", "width": 2, "signed": True}), "int16")
        self.assertEqual(_describe({"kind": "int", "width": 2, "signed": False}), "uint16")
        self.assertEqual(_describe({"kind": "float", "width": 4, "signed": None}), "float32")

    def test_hex_width_covers_the_struct(self):
        self.assertEqual(hex_width(0x724), 3)
        self.assertEqual(hex_width(0x10), 2)

    def test_field_names_follow_doctrine(self):
        self.assertEqual(field_name_for(0x2), "field_02")
        self.assertEqual(field_name_for(0x18, 3), "field_018")


class ClassificationTests(unittest.TestCase):
    """Every refusal here is a bug the tool declines to introduce."""

    def test_exact_match_is_rewritten(self):
        site = _site("x = *(int16_t *)(p + 0x0);")
        self.assertEqual(site["verdict"], "rewrite")
        self.assertEqual(site["field"], "salt")

    def test_array_element_index_is_computed(self):
        self.assertEqual(_site("x = *(float *)(p + 0x18);")["field"], "vec[2]")

    def test_float_read_of_int_field_is_refused(self):
        self.assertEqual(_site("x = *(float *)(p + 0x1c);")["verdict"], "refuse")

    def test_int_read_of_float_field_is_refused(self):
        self.assertEqual(_site("x = *(int *)(p + 0x10);")["verdict"], "refuse")

    def test_signedness_mismatch_is_refused(self):
        # int32_t vs uint32_t is MOVSX vs MOVZX at narrow widths and a real
        # semantic difference at any width.
        self.assertEqual(_site("x = *(int32_t *)(p + 0x1c);")["verdict"], "refuse")

    def test_interior_offset_of_scalar_is_refused(self):
        self.assertEqual(_site("x = *(char *)(p + 0x1d);")["verdict"], "refuse")

    def test_unaligned_array_offset_is_refused(self):
        self.assertEqual(_site("x = *(float *)(p + 0x12);")["verdict"], "refuse")

    def test_volatile_is_refused(self):
        self.assertEqual(_site("x = *(volatile int16_t *)(p + 0x0);")["verdict"], "refuse")

    def test_whole_struct_copy_is_refused(self):
        self.assertEqual(_site("x = *(vector3_t *)(p + 0x10);")["verdict"], "refuse")

    def test_address_taken_is_refused(self):
        site = _site("f((float *)(p + 0x10));", taking_address=True)
        self.assertEqual(site["verdict"], "refuse")

    def test_offset_beyond_sizeof_flags_a_bad_binding(self):
        site = _site("x = *(int32_t *)(p + 0x40);")
        self.assertEqual(site["verdict"], "refuse")
        self.assertIn("binding is wrong", site["reason"])

    def test_char_star_form_is_recognised(self):
        self.assertEqual(_site("x = *(int16_t *)((char *)p + 0x0);")["verdict"], "rewrite")

    def test_pad_access_requests_a_split(self):
        site = _site("x = *(int16_t *)(p + 0x4);")
        self.assertEqual(site["verdict"], "split")
        self.assertEqual(site["pad"], "pad_02")


class FieldCoveringTests(unittest.TestCase):
    def test_interior_offsets_resolve_to_their_field(self):
        self.assertEqual(field_covering(_layout(), 0x14)["name"], "vec")

    def test_offset_past_the_last_field_is_unresolved(self):
        self.assertIsNone(field_covering(_layout(), 0x30))


class SplitPlanningTests(unittest.TestCase):
    def _census(self, lines):
        sites = []
        for line in lines:
            site = _site(line)
            site.update(pad="pad_02", pad_offset=2)
            sites.append(site)
        return {"struct": "t", "base": "p", "source": "x.c", "sites": sites}

    def test_span_is_preserved_exactly(self):
        plan = plan_splits(self._census(["x = *(int16_t *)(p + 0x4);",
                                         "x = *(int32_t *)(p + 0x8);"]), _layout())
        total = 0
        for line in plan["plans"][0]["replacement"]:
            if "[0x" in line:
                total += int(line.split("[0x")[1].split("]")[0], 16)
            elif "int16_t" in line:
                total += 2
            elif "int32_t" in line:
                total += 4
        self.assertEqual(total, 0x0E, "pad span must be preserved or sizeof changes")

    def test_repeat_accesses_produce_one_field(self):
        plan = plan_splits(self._census(["x = *(int16_t *)(p + 0x4);"] * 5), _layout())
        self.assertEqual(plan["plans"][0]["fields_added"], 1)

    def test_conflicting_widths_are_refused_not_guessed(self):
        plan = plan_splits(self._census(["x = *(int16_t *)(p + 0x4);",
                                         "x = *(int32_t *)(p + 0x4);"]), _layout())
        self.assertEqual(plan["plans"], [])
        self.assertEqual(len(plan["conflicts"]), 1)

    def test_conflict_names_every_observed_type_once(self):
        plan = plan_splits(self._census(["x = *(int16_t *)(p + 0x4);",
                                         "x = *(int32_t *)(p + 0x4);",
                                         "x = *(int32_t *)(p + 0x4);"]), _layout())
        self.assertEqual(len(plan["conflicts"]), 1)
        self.assertIn("int16", plan["conflicts"][0]["reason"])
        self.assertIn("int32", plan["conflicts"][0]["reason"])

    def test_overlapping_fields_are_refused(self):
        plan = plan_splits(self._census(["x = *(int32_t *)(p + 0x4);",
                                         "x = *(int16_t *)(p + 0x6);"]), _layout())
        self.assertTrue(plan["conflicts"])


class FunctionSpanTests(unittest.TestCase):
    SOURCE = (
        "#include <x.h>\n"
        "static int helper(char *a)\n"
        "{\n"
        "  return *(int *)(a + 0x4);\n"
        "}\n"
        "\n"
        "void second(char *a)\n"
        "{\n"
        "  if (a) {\n"
        "    *(int *)(a + 0x8) = 1;\n"
        "  }\n"
        "}\n"
    )

    def test_lines_map_to_their_enclosing_function(self):
        spans = function_spans(self.SOURCE)
        self.assertEqual(spans[4], "helper")
        self.assertEqual(spans[10], "second")

    def test_file_scope_lines_are_unattributed(self):
        self.assertIsNone(function_spans(self.SOURCE).get(1))


class RewriteTests(unittest.TestCase):
    LINE = "  x = *(int16_t *)(p + 0x0);\n"

    def setUp(self):
        self._dir = tempfile.mkdtemp(prefix="structize-test-")
        self.source = Path(self._dir) / "x.c"
        self.source.write_text("void f(char *p)\n{\n%s}\n" % self.LINE, encoding="utf-8")
        self.addCleanup(shutil.rmtree, self._dir, True)

    def _census(self):
        site = _site(self.LINE)
        site["function"] = "f"
        site["line"] = 3
        return {"struct": "t", "base": "p", "source": "x.c", "sites": [site]}

    def test_inline_cast_is_the_default_mode(self):
        result = apply_rewrites(self._census(), source=self.source, apply=False)
        self.assertEqual(result["mode"], "inline-cast")
        self.assertEqual(result["prefix"], "((t *)p)->")
        self.assertEqual(result["sites_rewritten"], 1)

    def test_dry_run_does_not_touch_the_file(self):
        before = self.source.read_text(encoding="utf-8")
        apply_rewrites(self._census(), source=self.source, apply=False)
        self.assertEqual(self.source.read_text(encoding="utf-8"), before)

    def test_apply_writes_the_field_access(self):
        apply_rewrites(self._census(), source=self.source, apply=True)
        self.assertIn("((t *)p)->salt", self.source.read_text(encoding="utf-8"))

    def test_base_declaration_is_never_retyped(self):
        # A partial retype would rescale every un-rewritten `p + 0xNN` site.
        apply_rewrites(self._census(), source=self.source, apply=True)
        self.assertIn("void f(char *p)", self.source.read_text(encoding="utf-8"))

    def test_excluded_functions_are_not_rewritten(self):
        result = apply_rewrites(self._census(), source=self.source, apply=False,
                                exclude=["f"])
        self.assertEqual(result["sites_eligible"], 0)

    def test_only_filter_selects_one_function(self):
        self.assertEqual(apply_rewrites(self._census(), source=self.source, apply=False,
                                        only=["other"])["sites_eligible"], 0)
        self.assertEqual(apply_rewrites(self._census(), source=self.source, apply=False,
                                        only=["f"])["sites_eligible"], 1)


class SiteOutcomeTests(unittest.TestCase):
    """Manifest write-back must be conservative: a half-converted line is not done."""

    def _sites(self, *specs):
        sites = []
        for line, verdict, function in specs:
            site = {"line": line, "verdict": verdict, "function": function,
                    "reason": "because", "offset": 0}
            sites.append(site)
        return {"struct": "t", "base": "p", "source": "x.c", "sites": sites}

    def test_fully_rewritten_line_is_applied(self):
        out = site_outcomes(self._sites((3, "rewrite", "f"), (3, "rewrite", "f")))
        self.assertEqual(out[3]["applied"], 2)
        self.assertEqual(out[3]["parked"], 0)

    def test_line_in_a_parked_function_is_parked(self):
        out = site_outcomes(self._sites((3, "rewrite", "f")), parked_functions=["f"],
                            park_reason="scheduling")
        self.assertEqual(out[3]["parked"], 1)
        self.assertEqual(out[3]["reason"], "scheduling")

    def test_mixed_line_counts_both(self):
        out = site_outcomes(self._sites((3, "rewrite", "f"), (3, "refuse", "f")))
        self.assertEqual((out[3]["applied"], out[3]["parked"]), (1, 1))

    def test_unresolved_offset_is_parked_with_its_reason(self):
        out = site_outcomes(self._sites((7, "split", "f")))
        self.assertEqual(out[7]["parked"], 1)
        self.assertIn("offset unresolved", out[7]["reason"])


if __name__ == "__main__":
    unittest.main()
