#!/usr/bin/env python3
"""Tests for slice_looks_truncated -- the guard against emulating a delinked
reference that does not cover the target function.

The motivating case: player_rumble.obj's .text is 0x4a1 bytes and the target
FUN_000b9fd0 sits at section offset 0x4a0, so extract_function returned the
single byte 0x55 ("push ebp").  Emulating that compared the lift against
nothing, yet reported "100% coverage" (1 of 1 byte executed) and 15 failed
seeds -- a truncated reference wearing the costume of a real divergence.

Both directions are pinned: a truncated slice must be rejected, and a complete
slice must not be, or the guard would silently disable working tests.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from coff_loader import FunctionSlice, slice_looks_truncated


def mkslice(code: bytes, reached_end: bool = True) -> FunctionSlice:
    return FunctionSlice(
        name="f", raw_name="_f", code=code, relocs=[],
        reached_section_end=reached_end,
    )


class TestTruncationDetected(unittest.TestCase):
    def test_lone_push_ebp_at_section_end(self):
        """The player_rumble.obj case, verbatim."""
        reason = slice_looks_truncated(mkslice(b"\x55"))
        self.assertIsNotNone(reason)
        self.assertIn("push", reason)

    def test_prologue_without_return(self):
        # push ebp; mov ebp,esp; sub esp,0x10  -- a real prologue, no body/ret.
        reason = slice_looks_truncated(mkslice(b"\x55\x8b\xec\x83\xec\x10"))
        self.assertIsNotNone(reason)

    def test_slice_cut_mid_instruction(self):
        # 0xB8 is "mov eax, imm32" and needs 4 more bytes; only 2 are present.
        reason = slice_looks_truncated(mkslice(b"\x55\xb8\x01\x02"))
        self.assertIsNotNone(reason)
        self.assertIn("stops", reason)

    def test_padding_only_slice(self):
        self.assertIsNotNone(slice_looks_truncated(mkslice(b"\x90\x90\xcc")))


class TestCompleteSliceAccepted(unittest.TestCase):
    def test_plain_ret(self):
        self.assertIsNone(slice_looks_truncated(mkslice(b"\xc3")))

    def test_full_function_body(self):
        # push ebp; mov ebp,esp; xor eax,eax; pop ebp; ret
        code = b"\x55\x8b\xec\x33\xc0\x5d\xc3"
        self.assertIsNone(slice_looks_truncated(mkslice(code)))

    def test_stdcall_ret_imm(self):
        # push ebp; mov ebp,esp; pop ebp; ret 0x8
        self.assertIsNone(slice_looks_truncated(mkslice(b"\x55\x8b\xec\x5d\xc2\x08\x00")))

    def test_tail_call_jmp(self):
        # A thunk: jmp rel32, displacement zeroed by the relocation.
        self.assertIsNone(slice_looks_truncated(mkslice(b"\xe9\x00\x00\x00\x00")))

    def test_tail_call_to_noreturn_callee(self):
        # Assert/halt paths end on a CALL with no following RET.
        self.assertIsNone(slice_looks_truncated(mkslice(b"\x55\x8b\xec\xe8\x00\x00\x00\x00")))

    def test_trailing_alignment_padding_is_stripped(self):
        self.assertIsNone(slice_looks_truncated(mkslice(b"\x33\xc0\xc3\xcc\xcc\xcc")))

    def test_reloc_zeroed_operand_is_not_padding(self):
        """Trailing 0x00 belongs to the instruction; stripping it would turn
        every reloc-terminated thunk into a false truncation report."""
        self.assertIsNone(slice_looks_truncated(mkslice(b"\xe8\x00\x00\x00\x00")))


class TestBoundedScope(unittest.TestCase):
    def test_slice_bounded_by_next_symbol_is_never_flagged(self):
        """A slice that stopped at a following function symbol is complete by
        construction, so the guard must not second-guess its last instruction --
        trailing zero padding decodes as 'add [eax],al' and would fire."""
        self.assertIsNone(
            slice_looks_truncated(mkslice(b"\x55\x8b\xec\x00\x00", reached_end=False)))

    def test_same_bytes_at_section_end_do_flag(self):
        self.assertIsNotNone(
            slice_looks_truncated(mkslice(b"\x55\x8b\xec\x00\x00", reached_end=True)))


if __name__ == "__main__":
    unittest.main(verbosity=2)
