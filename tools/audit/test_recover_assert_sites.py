#!/usr/bin/env python3
"""Unit tests for recover_assert_sites.py (no XBE access)."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import recover_assert_sites as r  # noqa: E402

FILE = "c:\\halo\\SOURCE\\game\\game_time.c"


def sites_of(text):
    blanked = r.cat._blank_comments_and_strings(text)
    start = blanked.index("{")
    return r.source_sites(text, blanked, start, len(text))


def apply(text, binary, positional=False, undecoded=0):
    edits, unmatched = r.plan_edits(sites_of(text), binary, positional, undecoded)
    for edit in sorted(edits, key=lambda e: e.start, reverse=True):
        text = text[:edit.start] + edit.text + text[edit.end:]
    return text, edits, unmatched


class RecoverAssertSitesTest(unittest.TestCase):
    def test_stringize_collapses_whitespace_like_the_preprocessor(self):
        self.assertEqual(r.stringize("  a  &&\n   b "), "a && b")

    def test_c_string_literal_escapes_backslashes(self):
        self.assertEqual(r.c_string_literal(FILE), '"c:\\\\halo\\\\SOURCE\\\\game\\\\game_time.c"')

    def test_exact_text_becomes_assert_halt_at(self):
        text, edits, _ = apply("void f(void)\n{\n  assert_halt(g && g->x);\n}\n",
                               [r.BinarySite("g && g->x", FILE, 0x83, 0)])
        self.assertIn('assert_halt_at("c:\\\\halo\\\\SOURCE\\\\game\\\\game_time.c", 0x83, g && g->x);', text)
        self.assertEqual(edits[0].kind, "exact")

    def test_spacing_difference_keeps_the_original_literal(self):
        text, edits, _ = apply("void f(void)\n{\n  assert_halt(a >= 0);\n}\n",
                               [r.BinarySite("a>=0", FILE, 0x10, 0)])
        self.assertIn('assert_halt_msg_at("a>=0", ', text)
        self.assertTrue(text.rstrip().endswith("a >= 0);\n}") or "0x10, a >= 0)" in text)
        self.assertEqual(edits[0].kind, "spacing")

    def test_assert_halt_msg_keeps_its_message(self):
        text, _, _ = apply('void f(void)\n{\n  assert_halt_msg(p != 0, "p");\n}\n',
                           [r.BinarySite("p", FILE, 0x20, 0)])
        self.assertIn('assert_halt_msg_at("p", "c:\\\\halo', text)
        self.assertIn("0x20, p != 0)", text)

    def test_unknown_text_is_left_alone(self):
        source = "void f(void)\n{\n  assert_halt(ours);\n}\n"
        text, edits, unmatched = apply(source, [r.BinarySite("theirs", FILE, 0x20, 0)])
        self.assertEqual(text, source)
        self.assertEqual(edits, [])
        self.assertEqual(len(unmatched), 1)

    def test_repeated_text_pairs_in_line_order(self):
        text, _, _ = apply("void f(void)\n{\n  assert_halt(x);\n  assert_halt(x);\n}\n",
                           [r.BinarySite("x", FILE, 0x30, 0), r.BinarySite("x", FILE, 0x2f, 0)])
        self.assertLess(text.index("0x2f"), text.index("0x30"))

    def test_multi_line_call_keeps_its_line_count(self):
        source = "void f(void)\n{\n  assert_halt(\n      a &&\n      b);\n  assert_halt(c);\n}\n"
        text, _, _ = apply(source, [r.BinarySite("a && b", FILE, 0x40, 0)])
        self.assertEqual(text.count("\n"), source.count("\n"))
        self.assertEqual(text.splitlines()[5], "  assert_halt(c);")

    def test_crlf_file_stays_crlf(self):
        source = "void f(void)\r\n{\r\n  assert_halt(\r\n      a);\r\n}\r\n"
        text, _, _ = apply(source, [r.BinarySite("a", FILE, 0x41, 0)])
        self.assertEqual(text.count("\r\n"), source.count("\r\n"))
        self.assertNotIn("\n", text.replace("\r\n", ""))

    def test_rewritten_forms_are_not_matched_again(self):
        self.assertEqual(sites_of('void f(void)\n{\n  assert_halt_at("x", 1, a);\n'
                                  '  assert_halt_msg_at("m", "x", 1, a);\n}\n'), [])

    def test_positional_pairs_leftovers_in_line_order(self):
        source = "void f(void)\n{\n  assert_halt(p1);\n  assert_halt(x);\n  assert_halt(p2);\n}\n"
        binary = [r.BinarySite("b", FILE, 0x52, 2), r.BinarySite("x", FILE, 0x51, 1),
                  r.BinarySite("a", FILE, 0x50, 0)]
        text, edits, unmatched = apply(source, binary, positional=True)
        self.assertEqual(unmatched, [])
        self.assertEqual(sorted(e.kind for e in edits), ["exact", "positional", "positional"])
        self.assertIn('assert_halt_msg_at("a", "c:\\\\halo\\\\SOURCE\\\\game\\\\game_time.c", 0x50, p1);', text)
        self.assertIn('assert_halt_msg_at("b", "c:\\\\halo\\\\SOURCE\\\\game\\\\game_time.c", 0x52, p2);', text)

    def test_positional_is_off_by_default(self):
        source = "void f(void)\n{\n  assert_halt(ours);\n}\n"
        text, edits, _ = apply(source, [r.BinarySite("theirs", FILE, 0x20, 0)])
        self.assertEqual((text, edits), (source, []))

    def test_positional_skips_when_text_matches_disagree_with_order(self):
        source = "void f(void)\n{\n  assert_halt(y);\n  assert_halt(ours);\n  assert_halt(x);\n}\n"
        binary = [r.BinarySite("x", FILE, 0x10, 0), r.BinarySite("theirs", FILE, 0x11, 1),
                  r.BinarySite("y", FILE, 0x12, 2)]
        _, edits, unmatched = apply(source, binary, positional=True)
        self.assertNotIn("positional", [e.kind for e in edits])
        self.assertEqual(len(unmatched), 1)

    def test_positional_skips_when_a_call_did_not_decode(self):
        source = "void f(void)\n{\n  assert_halt(ours);\n}\n"
        text, edits, _ = apply(source, [r.BinarySite("theirs", FILE, 0x20, 0)],
                               positional=True, undecoded=1)
        self.assertEqual((text, edits), (source, []))

    def test_positional_skips_on_count_mismatch(self):
        source = "void f(void)\n{\n  assert_halt(a1);\n  assert_halt(a2);\n}\n"
        _, edits, _ = apply(source, [r.BinarySite("theirs", FILE, 0x20, 0)], positional=True)
        self.assertEqual(edits, [])


if __name__ == "__main__":
    unittest.main()
