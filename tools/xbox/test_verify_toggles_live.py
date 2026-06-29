#!/usr/bin/env python3
"""Offline unit tests for verify_toggles_live.classify (no xemu needed).

The regression these lock: `--all-off` walks all ~175 ported=false functions and
the old classifier flagged ANY `E9` jmp at entry as an ACTIVE redirect. The
original XBE contains genuine JMP-thunks (e.g. 0x1459d0 = `E9 1B FF FF FF` ->
0x1458f0) that are ORIGINAL code -- misreading them as ACTIVE false-failed the
liveness gate on a perfectly live build, turning every ab_check run INCONCLUSIVE.
A redirect jumps UP into our appended impl (>= IMPL_REGION_FLOOR); a thunk jumps
within the original .text (< floor).

    python3 -m unittest tools.xbox.test_verify_toggles_live
"""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "xbox"))
import verify_toggles_live as v  # noqa: E402


def _jmp(va, target):
    """Build the `E9 rel32` bytes for a jmp at `va` landing on `target`."""
    rel = (target - (va + 5)) & 0xFFFFFFFF
    return bytes([0xE9]) + rel.to_bytes(4, "little")


class ClassifyContract(unittest.TestCase):
    def test_push_ret_trampoline_is_active(self):
        b = bytes([0x68]) + (0x700000).to_bytes(4, "little") + bytes([0xC3, 0x00])
        state, target, _ = v.classify(0x13ab20, b)
        self.assertEqual(state, "ACTIVE")
        self.assertEqual(target, 0x700000)

    def test_jmp_up_into_impl_is_active(self):
        va = 0x13ab20
        target = 0x700000  # >= IMPL_REGION_FLOOR (0x642000)
        state, got, _ = v.classify(va, _jmp(va, target))
        self.assertEqual(state, "ACTIVE")
        self.assertEqual(got, target)

    def test_original_jmp_thunk_is_original(self):
        # The exact field-observed regression: 0x1459d0 `E9 1B FF FF FF` -> 0x1458f0.
        b = bytes([0xE9, 0x1B, 0xFF, 0xFF, 0xFF])
        state, target, detail = v.classify(0x1459d0, b)
        self.assertEqual(state, "ORIGINAL", f"thunk misread as ACTIVE: {detail}")
        self.assertEqual(target, 0x1458F0)

    def test_jmp_just_below_floor_is_original(self):
        va = 0x200000
        target = v.IMPL_REGION_FLOOR - 1
        state, _t, _ = v.classify(va, _jmp(va, target))
        self.assertEqual(state, "ORIGINAL")

    def test_genuine_prologue_is_original(self):
        b = bytes([0x55, 0x8B, 0xEC, 0x83])  # push ebp; mov ebp,esp; ...
        state, target, _ = v.classify(0x100000, b)
        self.assertEqual(state, "ORIGINAL")
        self.assertIsNone(target)


if __name__ == "__main__":
    unittest.main()
