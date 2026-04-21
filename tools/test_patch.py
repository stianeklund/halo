#!/usr/bin/env python3

import json
import os
import struct
import sys
import tempfile
import unittest
from types import SimpleNamespace


sys.path.insert(0, os.path.dirname(__file__))

from knowledge import Function
from patch import (
    find_reg_annotation_mismatches,
    format_register_arg_delta_lines,
    generate_reverse_thunk,
    validate_reg_annotation_baseline_completeness,
)


def rel32(call_site, target):
    return struct.pack('<i', target - (call_site + 5))


class ReverseThunkTests(unittest.TestCase):
    def test_single_reg_arg_uses_minimal_thunk(self):
        sym = Function('void player_reset_action_result(int player_handle@<eax>);')
        impl_addr = 0x401000
        rvthunk_addr = 0x650000

        expected = (
            b'\x50'
            b'\xe8' + rel32(rvthunk_addr + 1, impl_addr) +
            b'\x83\xc4\x04'
            b'\xc3'
        )

        self.assertEqual(generate_reverse_thunk(sym, impl_addr, rvthunk_addr), expected)

    def test_single_reg_plus_stack_arg_keeps_return_address_on_stack(self):
        sym = Function('void players_update_pvs(void *combined_pvs@<edi>, bool local_player_only);')
        impl_addr = 0x4086c0
        rvthunk_addr = 0x65f120

        expected = (
            b'\x8b\x14\x24'
            b'\x8b\x44\x24\x04'
            b'\x89\x04\x24'
            b'\x89\x54\x24\x04'
            b'\x89\xf8'
            b'\x50'
            b'\xe8' + rel32(rvthunk_addr + 17, impl_addr) +
            b'\x83\xc4\x04'
            b'\x8b\x54\x24\x04'
            b'\x8b\x04\x24'
            b'\x89\x44\x24\x04'
            b'\x89\x14\x24'
            b'\xc3'
        )

        self.assertEqual(generate_reverse_thunk(sym, impl_addr, rvthunk_addr), expected)

    def test_two_reg_plus_stack_arg_rotates_return_address_once(self):
        sym = Function(
            'void director_apply_replay_mode_for_player(char reset_flag@<al>, '
            'int16_t local_player_index@<si>, char mode_flags);'
        )
        impl_addr = 0x402000
        rvthunk_addr = 0x651000

        expected = (
            b'\x8b\x14\x24'
            b'\x8b\x44\x24\x04'
            b'\x89\x04\x24'
            b'\x89\x54\x24\x04'
            b'\x0f\xb6\xc0'
            b'\x0f\xb7\xce'
            b'\x51'
            b'\x50'
            b'\xe8' + rel32(rvthunk_addr + 22, impl_addr) +
            b'\x83\xc4\x08'
            b'\x8b\x54\x24\x04'
            b'\x8b\x04\x24'
            b'\x89\x44\x24\x04'
            b'\x89\x14\x24'
            b'\xc3'
        )

        self.assertEqual(generate_reverse_thunk(sym, impl_addr, rvthunk_addr), expected)


class RegAnnotationBaselineTests(unittest.TestCase):
    def write_baseline(self, baseline_decl):
        tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(tmpdir.cleanup)
        path = os.path.join(tmpdir.name, 'kb_reg_baseline.json')
        with open(path, 'w') as f:
            json.dump({'version': 1, 'functions': {'0x1000': baseline_decl}}, f)
        return path

    def test_matching_reg_annotations_pass(self):
        baseline_path = self.write_baseline('void test_func(int value@<eax>, int other);')
        kb = SimpleNamespace(symbols=[Function('void test_func(int value@<eax>, int other);', addr=0x1000)])

        self.assertEqual(find_reg_annotation_mismatches(kb, baseline_path), [])

    def test_lost_reg_annotation_fails(self):
        baseline_path = self.write_baseline('void test_func(int value@<eax>, int other);')
        kb = SimpleNamespace(symbols=[Function('void test_func(int value, int other);', addr=0x1000)])

        mismatches = find_reg_annotation_mismatches(kb, baseline_path)
        self.assertEqual(len(mismatches), 1)
        self.assertEqual(mismatches[0]['expected_reg_args'], [(0, 'eax')])
        self.assertEqual(mismatches[0]['actual_reg_args'], [])

    def test_changed_reg_position_fails(self):
        baseline_path = self.write_baseline('void test_func(int value@<eax>, int other);')
        kb = SimpleNamespace(symbols=[Function('void test_func(int value, int other@<eax>);', addr=0x1000)])

        mismatches = find_reg_annotation_mismatches(kb, baseline_path)
        self.assertEqual(len(mismatches), 1)
        self.assertEqual(mismatches[0]['expected_reg_args'], [(0, 'eax')])
        self.assertEqual(mismatches[0]['actual_reg_args'], [(1, 'eax')])

    def test_missing_symbol_in_kb_fails(self):
        baseline_path = self.write_baseline('void test_func(int value@<eax>, int other);')
        kb = SimpleNamespace(symbols=[])

        mismatches = find_reg_annotation_mismatches(kb, baseline_path)
        self.assertEqual(len(mismatches), 1)
        self.assertIsNone(mismatches[0]['current_decl'])

    def test_delta_lines_show_removed_and_added_register_slots(self):
        self.assertEqual(
            format_register_arg_delta_lines([(0, 'eax')], [(1, 'eax')]),
            ['- arg0@<eax>', '+ arg1@<eax>'],
        )

    def test_delta_lines_show_missing_register_args_as_none(self):
        self.assertEqual(
            format_register_arg_delta_lines([(0, 'eax'), (2, 'edi')], []),
            ['- arg0@<eax>', '- arg2@<edi>', '+ none'],
        )

    def test_missing_baseline_entry_for_current_reg_function_fails(self):
        baseline_path = self.write_baseline('void other_func(int value@<eax>);')
        kb = SimpleNamespace(symbols=[Function('void test_func(int value@<eax>);', addr=0x1000)])

        with self.assertRaisesRegex(ValueError, 'missing 1 current @<reg> function'):
            validate_reg_annotation_baseline_completeness(
                kb,
                {'0x2000': 'void other_func(int value@<eax>);'},
                baseline_path,
            )


if __name__ == '__main__':
    unittest.main()
