#!/usr/bin/env python3
"""Tests for the §54 ported-address-comparison detector.

The bug this guards: a callback slot written by ported code holds the address
of OUR implementation, not the original VA (which only holds the redirect
JMP).  Testing that slot against the bare literal therefore never matches and
the guarded feature dies silently.  It shipped twice on `sound_update_music`'s
lip-sync hook -- 2026-05-03 fixed it, a 2026-08-19 VC71 score pass
reintroduced it and stopped every talking unit's mouth from animating (Captain
Keyes handing over the pistol on a10).  No assert, no crash, no score delta:
only a detector catches this class.

Pure Python; no engine, kb.json or Xbox required.

Run: python3 tools/audit/test_check_ported_addr_compare.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from check_lift_hazards import check_ported_addr_compare

# 0x1c7a10 = FUN_001c7a10, the dialogue sound update callback (ported).
# 0x853c0  = FUN_000853c0, the debug camera update (ported).
PORTED = {0x1C7A10: ('FUN_001c7a10', 'game_sound.obj'),
          0x853C0: ('FUN_000853c0', 'objects.obj')}
# Only FUN_001c7a10 has its symbol address taken anywhere in src/ --
# FUN_000853c0's single store site lives in unported original code.
SYMBOLIC = {0x1C7A10: 'FUN_001c7a10'}


def run(src):
    return check_ported_addr_compare('src/halo/sound/sound_manager.c', src,
                                     src.split('\n'),
                                     ported=PORTED, symbolic=SYMBOLIC)


class TestPortedAddrCompare(unittest.TestCase):

    def test_flags_the_regression_that_shipped(self):
        """The exact line that broke Captain Keyes' mouth animation."""
        errs = run('  if (*(int *)(sound_entry + 0x10) == 0x1c7a10) {\n')
        self.assertEqual(len(errs), 1, errs)
        self.assertIn('FUN_001c7a10', errs[0])
        self.assertIn('0x1c7a10', errs[0])

    def test_accepts_the_symbol_form(self):
        """The fix must be silent."""
        src = ('  if (*(void **)(sound_entry + 0x10) == '
               '(void *)&FUN_001c7a10) {\n')
        self.assertEqual(run(src), [])

    def test_flags_inequality(self):
        self.assertEqual(len(run('  if (cb != 0x1c7a10) return;\n')), 1)

    def test_flags_reversed_operands(self):
        self.assertEqual(len(run('  if (0x1c7a10 == cb) return;\n')), 1)

    def test_flags_through_a_cast(self):
        src = '  if ((void *)cb == (void *)0x1c7a10) return;\n'
        self.assertEqual(len(run(src)), 1)

    def test_ignores_literal_on_both_sides(self):
        """director.c:611 is legitimate: the only store site is unported
        original code, so nothing takes &FUN_000853c0 and the literal on
        both sides is the correct form."""
        src = '  if ((void *)camera_fn != (void *)0x853c0) {\n'
        self.assertEqual(run(src), [])

    def test_ignores_non_function_literals(self):
        self.assertEqual(run('  if (tag_group == 0x736e6421) return;\n'), [])

    def test_ignores_a_plain_store(self):
        """Storing the literal is not the failure mode; the mixed pair is."""
        self.assertEqual(run('  x = 0x1c7a10;\n'), [])

    def test_ignores_a_call_through_the_literal(self):
        """Calling the original VA works -- the redirect JMP is there."""
        self.assertEqual(run('  ((void (*)(void))0x1c7a10)();\n'), [])

    def test_hazard_ok_suppresses(self):
        src = '  if (cb == 0x1c7a10) /* hazard-ok: store side unported */\n'
        self.assertEqual(run(src), [])

    def test_comment_text_is_not_scanned(self):
        src = ('  /* the callback at 0x1c7a10 == the dialogue hook */\n'
               '  do_nothing();\n')
        self.assertEqual(run(src), [])


if __name__ == '__main__':
    unittest.main()
