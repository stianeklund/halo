#!/usr/bin/env python3

import os
import struct
import sys
import unittest


sys.path.insert(0, os.path.dirname(__file__))

from knowledge import Function
from patch import generate_reverse_thunk


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


if __name__ == '__main__':
    unittest.main()
