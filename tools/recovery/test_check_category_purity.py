"""Standard-library tests for the source-recovery category-purity checker."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tools.recovery import check_category_purity as purity


BASE = purity.BASE_C


def status(category, old, new):
    return purity.check_pair(category, old, new).status


class TokenizerTests(unittest.TestCase):
    def test_comments_are_tokens_and_can_be_stripped(self):
        tokens = purity.tokenize("int a; /* c */ // tail\n")
        self.assertEqual([t.kind for t in tokens if t.kind == "comment"].count("comment"), 2)
        self.assertEqual([t.text for t in purity.strip_comments(tokens)],
                         ["int", "a", ";"])

    def test_string_containing_comment_opener_is_one_token(self):
        tokens = purity.strip_comments(purity.tokenize('char *s = "/* not a comment */";'))
        strings = [t for t in tokens if t.kind == "str"]
        self.assertEqual(len(strings), 1)
        self.assertIn("not a comment", strings[0].text)

    def test_pp_numbers_stay_single_tokens(self):
        texts = [t.text for t in purity.tokenize("x = 1.5e-3f + 0x1p3 + 0xFFul;")]
        for expected in ("1.5e-3f", "0x1p3", "0xFFul"):
            self.assertIn(expected, texts)

    def test_multi_char_operators_are_not_split(self):
        texts = [t.text for t in purity.tokenize("a >>= b; c->d; e ... f")]
        for expected in (">>=", "->", "..."):
            self.assertIn(expected, texts)

    def test_line_numbers_track_newlines_inside_block_comments(self):
        tokens = purity.tokenize("int a;\n/* one\ntwo\nthree */\nint b;\n")
        code = purity.strip_comments(tokens)
        self.assertEqual(code[-2].text, "b")
        self.assertEqual(code[-2].line, 5)

    def test_unterminated_comment_does_not_raise(self):
        self.assertTrue(purity.tokenize("int a; /* never closed"))

    def test_escaped_quote_in_char_literal(self):
        tokens = [t for t in purity.tokenize(r"char c = '\''; int after;")]
        self.assertIn("chr", [t.kind for t in tokens])
        self.assertIn("after", [t.text for t in tokens])


class CommentsCategoryTests(unittest.TestCase):
    def test_comment_and_whitespace_only_change_is_pure(self):
        new = BASE.replace("  fVar1 = up[0] * 2.0f;",
                           "\n  /* near-plane factor (0x8c150) */\n"
                           "  fVar1 = up[0] * 2.0f;\n")
        self.assertEqual(status("comments", BASE, new), "pure")

    def test_literal_change_is_violation(self):
        self.assertEqual(status("comments", BASE, BASE.replace("2.0f", "3.0f")),
                         "violation")

    def test_rename_is_violation(self):
        self.assertEqual(status("comments", BASE, BASE.replace("fVar1", "scale")),
                         "violation")

    def test_removed_statement_is_violation(self):
        self.assertEqual(status("comments", BASE, BASE.replace("  int local_8;\n", "")),
                         "violation")


class RenameCategoryTests(unittest.TestCase):
    def test_consistent_rename_with_comment_update_is_pure(self):
        new = BASE.replace("fVar1", "near_plane_scale").replace(
            "/* observer camera */", "/* observer camera: near-plane scale */")
        self.assertEqual(status("local-renames", BASE, new), "pure")

    def test_symbol_names_is_the_same_checker(self):
        new = BASE.replace("update", "observer_update")
        self.assertEqual(status("symbol-names", BASE, new), "pure")
        self.assertIs(purity._CHECKERS["symbol-names"],
                      purity._CHECKERS["local-renames"])

    def test_inconsistent_mapping_is_violation(self):
        new = (BASE.replace("float fVar1;", "float scale;", 1)
                   .replace("fVar1 = up[0]", "scale = up[0]", 1)
                   .replace("up[1] = fVar1;", "up[1] = factor;", 1))
        result = purity.check_pair("local-renames", BASE, new)
        self.assertEqual(result.status, "violation")
        self.assertTrue(any("inconsistent rename" in v.message
                            for v in result.violations))

    def test_non_injective_mapping_is_violation(self):
        new = BASE.replace("fVar1", "value").replace("local_8", "value")
        result = purity.check_pair("local-renames", BASE, new)
        self.assertEqual(result.status, "violation")
        self.assertTrue(any("non-injective" in v.message for v in result.violations))

    def test_keyword_substitution_is_not_a_rename(self):
        result = purity.check_pair("local-renames", BASE,
                                   BASE.replace("int local_8;", "short local_8;"))
        self.assertEqual(result.status, "violation")
        self.assertTrue(any("keyword/type" in v.message for v in result.violations))

    def test_token_count_change_is_violation(self):
        new = BASE.replace("  int local_8;", "  int local_8;\n  up[2] = 0.0f;")
        result = purity.check_pair("local-renames", BASE, new)
        self.assertEqual(result.status, "violation")
        self.assertTrue(any("token count changed" in v.message
                            for v in result.violations))

    def test_literal_change_disguised_as_rename_is_violation(self):
        new = BASE.replace("fVar1", "scale").replace("0x10", "0x14")
        self.assertEqual(status("local-renames", BASE, new), "violation")


class ConstEnumCategoryTests(unittest.TestCase):
    def test_define_plus_literal_substitution_is_pure(self):
        new = (BASE.replace('#include "types.h"',
                            '#include "types.h"\n#define ACTOR_STATE_DEAD 3')
                   .replace("local_8 == 3", "local_8 == ACTOR_STATE_DEAD"))
        self.assertEqual(status("const-enum", BASE, new), "pure")

    def test_enum_block_addition_is_pure(self):
        new = BASE.replace('#include "types.h"',
                           '#include "types.h"\n'
                           'enum { STATE_IDLE = 0, STATE_DEAD = 3 };')
        self.assertEqual(status("const-enum", BASE, new), "pure")

    def test_int_typedef_addition_is_pure(self):
        new = BASE.replace('#include "types.h"',
                           '#include "types.h"\ntypedef int16_t actor_state_t;')
        self.assertEqual(status("const-enum", BASE, new), "pure")

    def test_changed_literal_value_is_violation(self):
        self.assertEqual(status("const-enum", BASE, BASE.replace("== 3", "== 4")),
                         "violation")

    def test_branch_polarity_flip_is_violation(self):
        new = (BASE.replace('#include "types.h"',
                            '#include "types.h"\n#define ACTOR_STATE_DEAD 3')
                   .replace("if (local_8 == 3)", "if (local_8 != ACTOR_STATE_DEAD)"))
        self.assertEqual(status("const-enum", BASE, new), "violation")

    def test_struct_typedef_addition_is_violation(self):
        new = BASE.replace('#include "types.h"',
                           '#include "types.h"\ntypedef struct { int a; } thing_t;')
        self.assertEqual(status("const-enum", BASE, new), "violation")

    def test_identifier_to_identifier_substitution_is_violation(self):
        # A rename hiding in a const-enum commit: not literal -> name.
        self.assertEqual(status("const-enum", BASE, BASE.replace("fVar1", "scale")),
                         "violation")


class StructDefineCategoryTests(unittest.TestCase):
    def test_typedef_and_asserts_appended_is_pure(self):
        new = (BASE + "\ntypedef struct observer_s { int32_t mode; } observer_t;\n"
                      "cs(observer_t, 4);\nco(observer_t, mode, 0x000);\n")
        self.assertEqual(status("struct-define", BASE, new), "pure")

    def test_new_guarded_header_is_pure(self):
        new = ("#ifndef OBSERVER_H\n#define OBSERVER_H\n#include \"types.h\"\n"
               "typedef struct { int32_t mode; } observer_t;\n"
               "cs(observer_t, 4);\n#endif\n")
        self.assertEqual(status("struct-define", "", new), "pure")

    def test_include_addition_is_pure(self):
        new = BASE.replace('#include "types.h"',
                           '#include "types.h"\n#include "camera/observer.h"')
        self.assertEqual(status("struct-define", BASE, new), "pure")

    def test_modified_existing_line_is_violation(self):
        new = (BASE.replace("float fVar1;", "float scale;")
               + "\ntypedef struct { int32_t mode; } observer_t;\n")
        result = purity.check_pair("struct-define", BASE, new)
        self.assertEqual(result.status, "violation")
        self.assertTrue(any("additions-only" in v.message for v in result.violations))

    def test_definition_inside_function_body_is_violation(self):
        new = BASE.replace("  int local_8;",
                           "  int local_8;\n  typedef struct { int a; } inner_t;")
        result = purity.check_pair("struct-define", BASE, new)
        self.assertEqual(result.status, "violation")
        self.assertTrue(any("brace depth" in v.message for v in result.violations))

    def test_valued_define_belongs_to_const_enum(self):
        new = BASE.replace('#include "types.h"',
                           '#include "types.h"\n#define OBSERVER_MODE_MAX 4')
        self.assertEqual(status("struct-define", BASE, new), "violation")

    def test_deleted_file_is_violation(self):
        self.assertEqual(status("struct-define", BASE, ""), "violation")


class OffsetToFieldCategoryTests(unittest.TestCase):
    def test_raw_deref_to_member_access_is_pure(self):
        new = BASE.replace("*(int *)((int)actor + 0x10)", "actor->mode")
        self.assertEqual(status("offset-to-field", BASE, new), "pure")

    def test_added_typed_base_pointer_local_is_pure(self):
        new = (BASE.replace("  int local_8;", "  int local_8;\n  observer_t *obs;")
                   .replace("*(int *)((int)actor + 0x10)", "obs->mode"))
        self.assertEqual(status("offset-to-field", BASE, new), "pure")

    def test_changed_offset_is_violation(self):
        new = BASE.replace("actor + 0x10", "actor + 0x14")
        self.assertEqual(status("offset-to-field", BASE, new), "violation")

    def test_unrelated_expression_rewrite_is_violation(self):
        new = BASE.replace("up[0] * 2.0f", "2.0f * up[0]")
        self.assertEqual(status("offset-to-field", BASE, new), "violation")

    def test_rename_mixed_into_rewrite_is_violation(self):
        new = (BASE.replace("*(int *)((int)actor + 0x10)", "actor->mode")
                   .replace("fVar1", "scale"))
        self.assertEqual(status("offset-to-field", BASE, new), "violation")

    def test_absolute_address_deref_is_not_an_offset_rewrite(self):
        old = "int f(void) { return *(int *)0x2673a4; }\n"
        new = "int f(void) { return globals->mode; }\n"
        self.assertEqual(status("offset-to-field", old, new), "violation")

    def test_added_call_statement_is_violation(self):
        new = (BASE.replace("*(int *)((int)actor + 0x10)", "actor->mode")
                   .replace("  int local_8;", "  int local_8;\n  init(actor);"))
        self.assertEqual(status("offset-to-field", BASE, new), "violation")


class UncheckableCategoryTests(unittest.TestCase):
    def test_risky_categories_report_unchecked(self):
        for category in ("expr-simplify", "control-flow"):
            for new in (BASE, BASE.replace("if (local_8 == 3) {",
                                           "if (local_8 != 3) goto done;"), ""):
                self.assertEqual(status(category, BASE, new), "unchecked",
                                 f"{category} must never claim to judge a diff")

    def test_every_ladder_category_is_either_checked_or_declared_unckeckable(self):
        for category in purity.CATEGORIES:
            self.assertTrue(category in purity._CHECKERS or category in purity.UNCHECKABLE,
                            f"{category} has neither a checker nor a justification")
            self.assertIn(category, purity.RULES)

    def test_unknown_category_raises(self):
        with self.assertRaises(ValueError):
            purity.check_pair("not-a-category", "", "")


class UnitSplitTests(unittest.TestCase):
    """Regressions for two real bugs: a function unit that swallowed the
    following definition, and difflib fragmenting an inserted co() line into
    "existing code modified"."""

    def units(self, text):
        return [" ".join(t.text for t in u.tokens)
                for u in purity._split_top_level(
                    purity.strip_comments(purity.tokenize(text)))]

    def test_function_body_does_not_swallow_the_next_definition(self):
        units = self.units("void f(void) { int a; a = 1; }\n"
                           "typedef struct { int m; } t_t;\n")
        self.assertEqual(len(units), 2)
        self.assertTrue(units[1].startswith("typedef"))

    def test_nested_braces_do_not_end_the_function_early(self):
        units = self.units("void f(int x) { if (x) { x = 2; } }\nint g;\n")
        self.assertEqual(len(units), 2)

    def test_brace_initialiser_stays_one_unit(self):
        units = self.units("int table[] = { 1, 2, 3 };\nint after;\n")
        self.assertEqual(len(units), 2)

    def test_typedef_struct_tail_declarator_stays_one_unit(self):
        units = self.units("typedef struct s { int a; } s_t;\nint after;\n")
        self.assertEqual(len(units), 2)

    def test_directives_are_their_own_units(self):
        units = self.units('#include "a.h"\n#include "b.h"\nint x;\n')
        self.assertEqual(len(units), 3)

    def test_co_assert_inserted_between_existing_ones_is_pure(self):
        old = "co(t, a, 0x000);\nco(t, c, 0x008);\n"
        new = "co(t, a, 0x000);\nco(t, b, 0x004);\nco(t, c, 0x008);\n"
        self.assertEqual(status("struct-define", old, new), "pure")

    def test_field_widened_in_an_existing_struct_is_a_violation(self):
        old = "typedef struct { int16_t a; char pad[0x18]; } t_t;\n"
        new = "typedef struct { int16_t a; char pad[0x10]; int b; } t_t;\n"
        self.assertEqual(status("struct-define", old, new), "violation")


class AlignReplaceTests(unittest.TestCase):
    def _units(self, text):
        return purity._split_top_level(purity.strip_comments(purity.tokenize(text)))

    def test_addition_next_to_a_modification_aligns(self):
        old = self._units("int f(void) { return 3; }\n")
        new = self._units("#define N 3\nint f(void) { return N; }\n")
        aligned = purity._align_replace(old, new)
        self.assertIsNotNone(aligned)
        pairs, additions = aligned
        self.assertEqual(len(pairs), 1)
        self.assertEqual(len(additions), 1)
        self.assertEqual(additions[0].tokens[1].text, "define")

    def test_removal_cannot_be_aligned(self):
        old = self._units("int a; int b;\n")
        new = self._units("int c;\n")
        self.assertIsNone(purity._align_replace(old, new))


class HunkCoalescingTests(unittest.TestCase):
    def test_short_surviving_token_run_is_absorbed_into_one_hunk(self):
        old = purity.strip_comments(purity.tokenize("a = *(int *)((int)p + 0x10);"))
        new = purity.strip_comments(purity.tokenize("a = p->mode;"))
        self.assertEqual(len(purity._hunks(old, new)), 1)

    def test_two_distant_edits_stay_separate_hunks(self):
        old = purity.strip_comments(purity.tokenize(
            "a = 1; b = 2; c = 3; d = 4; e = 5; f = 6;"))
        new = purity.strip_comments(purity.tokenize(
            "a = 9; b = 2; c = 3; d = 4; e = 5; f = 8;"))
        self.assertEqual(len(purity._hunks(old, new)), 2)


class CliTests(unittest.TestCase):
    SCRIPT = purity.ROOT / "tools" / "recovery" / "check_category_purity.py"

    def _run(self, *args):
        return subprocess.run([sys.executable, str(self.SCRIPT), *args],
                              cwd=purity.ROOT, capture_output=True, text=True)

    def test_self_test_exits_zero(self):
        result = self._run("--self-test")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("0 failure(s)", result.stdout)

    def test_explain_lists_every_category(self):
        result = self._run("--explain")
        self.assertEqual(result.returncode, 0, result.stderr)
        for category in purity.CATEGORIES:
            self.assertIn(category, result.stdout)

    def test_risky_category_exits_two(self):
        result = self._run("control-flow", "--revision", "HEAD")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("NOT MECHANICALLY CHECKABLE", result.stdout)

    def test_missing_category_is_a_usage_error(self):
        self.assertEqual(self._run().returncode, 2)

    def test_real_rename_commit_passes_and_ignores_the_manifest(self):
        result = self._run("local-renames", "--revision", "f731679b")
        if "unknown revision" in result.stdout + result.stderr:
            self.skipTest("f731679b not present in this clone")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("ignored 1 non-source file", result.stdout)
        self.assertIn("fVar1->near_plane_scale", result.stdout)

    def test_real_rename_commit_fails_the_wrong_category(self):
        result = self._run("comments", "--revision", "f731679b")
        if "unknown revision" in result.stdout + result.stderr:
            self.skipTest("f731679b not present in this clone")
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("split the commit", result.stdout)

    def test_staged_diff_in_a_scratch_repo(self):
        """The primary invocation (--staged) end to end, in a throwaway repo."""
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)

            def git(*args):
                return subprocess.run(["git", *args], cwd=root,
                                      capture_output=True, text=True)

            git("init", "-q")
            git("config", "user.email", "t@example.com")
            git("config", "user.name", "t")
            target = root / "a.c"
            target.write_text(BASE, encoding="utf-8")
            (root / "notes.json").write_text("{}\n", encoding="utf-8")
            git("add", "-A")
            git("commit", "-qm", "base")

            target.write_text(BASE.replace("fVar1", "scale"), encoding="utf-8")
            (root / "notes.json").write_text('{"status": "applied"}\n',
                                             encoding="utf-8")
            git("add", "-A")

            def run(category):
                return subprocess.run(
                    [sys.executable, str(self.SCRIPT), category, "--staged"],
                    cwd=root, capture_output=True, text=True)

            ok = run("local-renames")
            self.assertEqual(ok.returncode, 0, ok.stdout + ok.stderr)
            self.assertIn("fVar1->scale", ok.stdout)
            self.assertIn("ignored 1 non-source file", ok.stdout)

            bad = run("comments")
            self.assertEqual(bad.returncode, 1, bad.stdout)
            self.assertIn("split the commit", bad.stdout)

    def test_source_filter_narrows_the_diff(self):
        result = self._run("local-renames", "--revision", "f731679b",
                           "--source", "recovery")
        if "unknown revision" in result.stdout + result.stderr:
            self.skipTest("f731679b not present in this clone")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("no source files in the diff", result.stdout)


if __name__ == "__main__":
    unittest.main()
