#!/usr/bin/env python3
"""Tests for game-state persistent storage CRC round-trip.

Validates that the save/load checksum logic is self-consistent:
the CRC written during game_state_write_to_persistent_storage can be
verified by game_state_read_header_from_persistent_storage.

These tests are pure Python (no engine/Xbox required).  They reimplement
the same CRC-32 algorithm used in src/halo/memory/crc.c (polynomial
0xEDB88320, init 0xFFFFFFFF, no final XOR) so we can exercise the
header-checksum round-trip that broke when the scratch pointer was
misrouted (2026-05-27 fix).
"""

import os
import struct
import sys
import unittest
from pathlib import Path

HEADER_SIZE = 0x14C
CHECKSUM_OFFSET = 0x148
TOTAL_BUFFER_SIZE = 0x345000

# CRC-32 reimplementation matching src/halo/memory/crc.c
_CRC_TABLE = None

def _build_crc_table():
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
        table.append(crc & 0xFFFFFFFF)
    return table

def crc_new():
    return 0xFFFFFFFF

def crc_checksum_buffer(checksum, data):
    global _CRC_TABLE
    if _CRC_TABLE is None:
        _CRC_TABLE = _build_crc_table()
    value = checksum
    for byte in data:
        value = (value >> 8) ^ _CRC_TABLE[(byte ^ value) & 0xFF]
    return value & 0xFFFFFFFF


def write_side_compute_checksum(game_state_buffer):
    """Simulate game_state_write_to_persistent_storage (0x1c0ac0).

    1. Zero the checksum field at header+0x148
    2. CRC the entire buffer (header with zeroed checksum + body)
    3. Store the CRC at header+0x148
    4. Save original header, zero header in buffer, write buffer, then
       overwrite with original header (file ends up with CRC in place)

    Returns the modified buffer (with CRC embedded at offset 0x148).
    """
    buf = bytearray(game_state_buffer)
    struct.pack_into("<I", buf, CHECKSUM_OFFSET, 0)
    crc = crc_new()
    crc = crc_checksum_buffer(crc, buf)
    struct.pack_into("<I", buf, CHECKSUM_OFFSET, crc)
    return bytes(buf), crc


def read_side_verify_checksum(file_data):
    """Simulate game_state_read_header_from_persistent_storage (0x1c0910).

    The CORRECT implementation (after the fix):
    1. Read header (0x14c bytes) from file
    2. saved_checksum = header[0x148]  (via pointer into header buffer)
    3. header[0x148] = 0               (zero checksum field in-place)
    4. CRC over header
    5. CRC over remaining body in 128KB chunks
    6. Compare computed CRC with saved_checksum

    Returns (match: bool, saved_checksum, computed_checksum).
    """
    header = bytearray(file_data[:HEADER_SIZE])
    saved_checksum = struct.unpack_from("<I", header, CHECKSUM_OFFSET)[0]
    struct.pack_into("<I", header, CHECKSUM_OFFSET, 0)

    crc = crc_new()
    crc = crc_checksum_buffer(crc, header)

    body = file_data[HEADER_SIZE:]
    offset = 0
    while offset < len(body):
        chunk = body[offset:offset + 0x20000]
        crc = crc_checksum_buffer(crc, chunk)
        offset += 0x20000

    return (crc == saved_checksum, saved_checksum, crc)


def read_side_verify_BUGGY(file_data):
    """Simulate the BROKEN pre-fix behavior.

    Bug: scratch was a separate local variable, not header+0x148.
    This means:
    - saved_checksum reads uninitialized garbage (we simulate as 0)
    - the checksum field in the header is NOT zeroed
    - CRC is computed over header with CRC still embedded
    """
    header = bytearray(file_data[:HEADER_SIZE])
    saved_checksum = 0  # uninitialized scratch variable
    # scratch = 0  (zeros the wrong location, header CRC field untouched)

    crc = crc_new()
    crc = crc_checksum_buffer(crc, header)  # CRC field NOT zeroed!

    body = file_data[HEADER_SIZE:]
    offset = 0
    while offset < len(body):
        chunk = body[offset:offset + 0x20000]
        crc = crc_checksum_buffer(crc, chunk)
        offset += 0x20000

    return (crc == saved_checksum, saved_checksum, crc)


class TestCRC(unittest.TestCase):
    """Verify the CRC-32 implementation matches known values."""

    def test_empty_buffer(self):
        crc = crc_new()
        crc = crc_checksum_buffer(crc, b"")
        self.assertEqual(crc, 0xFFFFFFFF)

    def test_known_value(self):
        crc = crc_new()
        crc = crc_checksum_buffer(crc, b"123456789")
        # Standard CRC-32 of "123456789" with init=0xFFFFFFFF and no
        # final XOR is 0xCBF43926.  Our implementation omits the final
        # ~crc step that most libraries apply, so the raw accumulator
        # value is the bitwise inverse: 0x340BC6D9.
        self.assertEqual(crc, 0x340BC6D9)

    def test_incremental_equals_oneshot(self):
        data = os.urandom(4096)
        crc_one = crc_checksum_buffer(crc_new(), data)
        crc_inc = crc_new()
        crc_inc = crc_checksum_buffer(crc_inc, data[:1024])
        crc_inc = crc_checksum_buffer(crc_inc, data[1024:2048])
        crc_inc = crc_checksum_buffer(crc_inc, data[2048:])
        self.assertEqual(crc_one, crc_inc)


class TestPersistentStorageRoundTrip(unittest.TestCase):
    """Verify write→read checksum round-trip."""

    def _make_buffer(self, seed=42):
        import random
        rng = random.Random(seed)
        return bytes(rng.getrandbits(8) for _ in range(TOTAL_BUFFER_SIZE))

    def test_roundtrip_zeros(self):
        buf = b"\x00" * TOTAL_BUFFER_SIZE
        file_data, written_crc = write_side_compute_checksum(buf)
        match, saved, computed = read_side_verify_checksum(file_data)
        self.assertTrue(match, f"CRC mismatch: saved=0x{saved:08X} computed=0x{computed:08X}")

    def test_roundtrip_random(self):
        buf = self._make_buffer(seed=1)
        file_data, written_crc = write_side_compute_checksum(buf)
        match, saved, computed = read_side_verify_checksum(file_data)
        self.assertTrue(match, f"CRC mismatch: saved=0x{saved:08X} computed=0x{computed:08X}")
        self.assertEqual(written_crc, saved)

    def test_roundtrip_header_only_differs(self):
        buf = bytearray(TOTAL_BUFFER_SIZE)
        buf[0x04] = 0x41  # scenario name byte
        buf[0x104] = 0x30  # build version byte
        buf[0x124] = 0x01  # map type
        file_data, _ = write_side_compute_checksum(bytes(buf))
        match, saved, computed = read_side_verify_checksum(file_data)
        self.assertTrue(match)

    def test_corruption_detected(self):
        buf = self._make_buffer(seed=2)
        file_data, _ = write_side_compute_checksum(buf)
        corrupted = bytearray(file_data)
        corrupted[0x200] ^= 0xFF  # flip a byte in the body
        match, _, _ = read_side_verify_checksum(bytes(corrupted))
        self.assertFalse(match, "Corrupted data should fail checksum")

    def test_header_corruption_detected(self):
        buf = self._make_buffer(seed=3)
        file_data, _ = write_side_compute_checksum(buf)
        corrupted = bytearray(file_data)
        corrupted[0x10] ^= 0xFF  # flip a byte in the header
        match, _, _ = read_side_verify_checksum(bytes(corrupted))
        self.assertFalse(match)

    def test_buggy_read_always_fails(self):
        """Demonstrate that the pre-fix bug causes checksum to always fail."""
        buf = self._make_buffer(seed=4)
        file_data, _ = write_side_compute_checksum(buf)
        match, _, _ = read_side_verify_BUGGY(file_data)
        self.assertFalse(match, "Buggy read should always fail (this is the regression test)")

    def test_checksum_field_is_last_4_bytes(self):
        buf = b"\x00" * TOTAL_BUFFER_SIZE
        file_data, crc = write_side_compute_checksum(buf)
        stored = struct.unpack_from("<I", file_data, CHECKSUM_OFFSET)[0]
        self.assertEqual(stored, crc)
        self.assertEqual(CHECKSUM_OFFSET + 4, HEADER_SIZE)


class TestScratchPointerRegression(unittest.TestCase):
    """Regression test for the scratch-pointer bug (2026-05-27).

    The second argument to game_state_read_header_from_persistent_storage
    must point INTO the header buffer at offset 0x148, not to a separate
    stack variable.  This test encodes the invariant.
    """

    def test_correct_caller_pattern(self):
        """Verify cache_files_precache-style caller works."""
        buf = self._make_state()
        file_data, _ = write_side_compute_checksum(buf)
        match, _, _ = read_side_verify_checksum(file_data)
        self.assertTrue(match)

    def test_separate_scratch_fails(self):
        """Verify that a separate scratch variable breaks the round-trip."""
        buf = self._make_state()
        file_data, _ = write_side_compute_checksum(buf)
        match, _, _ = read_side_verify_BUGGY(file_data)
        self.assertFalse(match)

    def _make_state(self):
        buf = bytearray(TOTAL_BUFFER_SIZE)
        buf[0x04:0x04+5] = b"test\x00"
        buf[0x104:0x104+14] = b"01.10.12.2276"
        return bytes(buf)


if __name__ == "__main__":
    unittest.main()
