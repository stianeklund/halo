#!/usr/bin/env python3
"""Unit tests for the §45 pointer-slot-passed-by-address lint.

unittest, not pytest: pytest is installed neither on the dev box nor in CI,
and the CI step runs `unittest discover`, which collects TestCase classes
only.  Run with:

    python3 -m unittest discover -s tools/audit/tests -p 'test_*.py' -v
"""
import sys, os
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import check_lift_hazards as clh

_FAKE = '/fake/src/halo/units/units.c'

_DEFAULT_CORPUS = {
    ('FUN_0010a1c0', 1, 0x0c): {
        'deref': [('src/halo/physics/collision_bsp.c', 1395)],
    },
}
_DEFAULT_FLOAT_PARAMS = {'FUN_0010a1c0': {1}}


def _run(src, corpus=None, float_params=None):
    """Run the check with a stubbed corpus so the test is hermetic."""
    saved_corpus = clh._PTR_SLOT_CORPUS
    saved_params = clh._FLOAT_PTR_PARAMS
    clh._PTR_SLOT_CORPUS = _DEFAULT_CORPUS if corpus is None else corpus
    clh._FLOAT_PTR_PARAMS = (_DEFAULT_FLOAT_PARAMS if float_params is None
                             else float_params)
    try:
        return clh.check_pointer_slot_arg_form(_FAKE, src, src.split('\n'))
    finally:
        clh._PTR_SLOT_CORPUS = saved_corpus
        clh._FLOAT_PTR_PARAMS = saved_params


class PointerSlotArgFormTest(unittest.TestCase):
    def test_address_of_pointer_slot_is_flagged(self):
        """The melee-lunge bug: &slot passed where the slot holds a pointer."""
        src = ('FUN_0010a1c0(matrix, (float *)(collision_result + 0x0c), '
               'normal_out);\n')
        errors = _run(src)
        self.assertTrue(errors,
                        'should flag the address form of a dereferenced slot')
        self.assertIn('arg 2', errors[0])
        self.assertIn('+0xc', errors[0])

    def test_dereferenced_slot_is_silent(self):
        """The fixed form loads the pointer out of the slot."""
        src = ('FUN_0010a1c0(matrix, *(float **)(collision_result + 0x0c), '
               'normal_out);\n')
        self.assertEqual(_run(src), [])

    def test_non_float_param_is_silent(self):
        """Block/handle pointers legitimately take both forms."""
        src = 'tag_block_get_element((char *)(tag_data + 0x54), index);\n'
        corpus = {('tag_block_get_element', 0, 0x54): {
            'deref': [('src/halo/bitmaps/bitmap_utilities.c', 226)]}}
        self.assertEqual(_run(src, corpus=corpus, float_params={}), [])

    def test_hazard_ok_suppresses(self):
        src = ('FUN_0010a1c0(matrix, (float *)(collision_result + 0x0c), '
               'normal_out); /* hazard-ok: slot-form */\n')
        self.assertEqual(_run(src), [])

    def test_corpus_indexes_both_forms(self):
        """The real corpus builder must see deref and addr forms in src/."""
        sites = list(clh._ptr_slot_sites(_FAKE,
            'f(a, *(float **)(r + 0x0c));\n'
            'g(b, (float *)(r + 0x10));\n'))
        self.assertIn(('f', 1, 'deref', 0x0c, 1), sites)
        self.assertIn(('g', 1, 'addr', 0x10, 2), sites)


class MeleeLungeCallSiteTest(unittest.TestCase):
    """Regression pin for the FUN_001abd90 call site itself."""

    def test_melee_lunge_keeps_the_pointer_load(self):
        """units.c must load the plane pointer out of collision_result+0x0c.

        Passing the address fed the pointer's own bits to FUN_0010a1c0 as
        in_plane[0]; a 0x80xxxxxx pointer is a negative denormal, so the
        resulting normal printed as (-0.000000, 0.000000, 0.000000) and
        tripped assert_valid_real_normal3d at effects.c:0x461, one frame below
        object_cause_damage, on every melee hit.
        """
        repo_root = Path(__file__).resolve().parents[3]
        src = (repo_root / 'src/halo/units/units.c').read_text()
        start = src.index('void FUN_001abd90(')
        body = src[start:start + 6000]
        self.assertIn('*(float **)(collision_result + 0x0c)', body)
        stripped = body.replace('*(float **)(collision_result + 0x0c)', '')
        self.assertNotIn('(float *)(collision_result + 0x0c)', stripped)


if __name__ == '__main__':
    unittest.main()
