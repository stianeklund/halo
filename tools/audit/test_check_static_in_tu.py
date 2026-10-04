#!/usr/bin/env python3
"""Tests for the verifier-only static_in_tu compile context.

A kb.json ``"static_in_tu": true`` flag makes the VC7.1 verifier compile the
function ``static __declspec(noinline)`` and drop its decl.h prototype, so
VC7.1 reproduces the original's private register ABI (docs/lift-learnings.md
32a).  These pin the text rewrites in tools/verify/vc71_verify.py and the
blockers in tools/audit/check_static_in_tu.py.  Pure Python; no cl.exe, XBE
or kb.json required.

Run: python3 tools/audit/test_check_static_in_tu.py
"""
import os
import re
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "verify"))

import vc71_verify as vv  # noqa: E402
from check_static_in_tu import binary_refs, source_problems  # noqa: E402

SRC = """\
/* helper(x) in a comment is not a use */
void *helper(const char *name)
{
  return (void *)name;
}

real get(const char *name)
{
  real *val;
  val = (real *)helper(name);
  return val ? *val : 0.0f;
}
"""


class RewriteTests(unittest.TestCase):
    def test_definition_found_prototype_ignored(self):
        self.assertEqual(vv._static_defs_in(SRC, {"helper", "get", "absent"}),
                         {"helper", "get"})
        self.assertEqual(vv._static_defs_in("int helper(int a);\n", {"helper"}), set())

    def test_function_pointer_parameter(self):
        text = "void walk(void (*cb)(int), int n)\n{\n}\n"
        self.assertEqual(vv._static_defs_in(text, {"walk"}), {"walk"})

    def test_definition_rewritten_static_noinline(self):
        out = vv._fastcall_sig_re("helper").sub(vv._static_sub, SRC)
        self.assertIn("static __declspec(noinline) void *helper(const char *name)", out)
        self.assertIn("(real *)helper(name)", out)  # call site untouched

    def test_decl_shadow_drops_only_flagged_prototype(self):
        decl = ("HFUNC void *helper(const char *name);\n"
                "HFUNC real get(const char *name);\n"
                "HFUNC int helper_other(int a);\n")
        pat = vv._DECL_SHADOW_HFUNC_RE.format(name=re.escape("helper"))
        out = re.sub(pat, "// static_in_tu (verifier): helper\n", decl, flags=re.M)
        self.assertNotIn("HFUNC void *helper(", out)
        self.assertIn("HFUNC real get(", out)
        self.assertIn("HFUNC int helper_other(", out)


class SourceBlockerTests(unittest.TestCase):
    def test_clean(self):
        self.assertEqual(source_problems("helper", SRC, []), [])

    def test_address_taken(self):
        text = SRC + "void *slot = (void *)helper;\n"
        self.assertTrue(any("address taken" in p for p in source_problems("helper", text, [])))

    def test_called_before_definition(self):
        text = "void early(void)\n{\n  helper(0);\n}\n" + SRC
        self.assertTrue(any("before its definition" in p
                            for p in source_problems("helper", text, [])))

    def test_named_outside_tu(self):
        probs = source_problems("helper", SRC, [("src/other.c", "  helper(0);\n")])
        self.assertTrue(any("outside its TU" in p for p in probs))
        probs = source_problems("helper", SRC, [("src/other.c", "/* helper(0) */\n")])
        self.assertEqual(probs, [])

    def test_no_definition(self):
        self.assertEqual(source_problems("helper", "int helper(int);\n", []),
                         ["no top-level definition in its source"])


class BinaryRefTests(unittest.TestCase):
    def test_calls_and_dword_refs(self):
        va = 0x1000
        target = 0x1100
        call = b"\xe8" + struct.pack("<i", target - (va + 5))
        jmp = b"\xe9" + struct.pack("<i", target - (va + 10))
        text = call + jmp + b"\x90" * 6
        data = struct.pack("<I", target)
        refs = binary_refs([(va, True, text), (0x2000, False, data)], [target])
        self.assertEqual(refs[target]["calls"], [(va, False), (va + 5, True)])
        self.assertEqual(refs[target]["dwords"], [0x2000])


if __name__ == "__main__":
    unittest.main()
