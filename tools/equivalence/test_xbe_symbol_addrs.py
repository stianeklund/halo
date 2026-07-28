#!/usr/bin/env python3
"""Guard: oracle DIR32 symbols resolve to their ORIGINAL address by reading the
pristine XBE, not by matching the delinked label against a kb.json name.

The delinked reference's data labels come from Ghidra, and Ghidra's name for an
address need not be kb.json's.  0x5aa8b8 is the decal datum-pool pointer:
Ghidra labels it ``g_decals_data``, kb.json declares it ``global_decal_data``.
Neither normalizes to the other, so name matching resolved nothing, the
oracle's slot for it stayed zero, and ``datum_get`` saw arg[0]=0 on the oracle
side against the seeded 0x700700 the candidate read through its absolute
immediate.  All 50 seeds of FUN_0015b0c0 failed on what looked like a dropped
argument (2026-07-28).

Relocs are function-relative, so ``func_va + r.virtual_address`` is exactly the
dword the original linker wrote -- ground truth, and independent of whatever
the label happens to be called.
"""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import unicorn_diff as u
from stubs import IMAGE_REL_I386_DIR32

IMAGE_REL_I386_REL32 = 0x0014

# FUN_0015b0c0 (rasterizer_decals): the reloc at +0x46 is g_decals_data and the
# original code stores 0x5aa8b8 there; +0x5c is DAT_0032516c -> 0x32516c.
DECAL_FUNC_VA = 0x15B0C0
DECALS_DATA_OFF = 0x46
DECALS_DATA_ADDR = 0x5AA8B8
NAMED_MISS = "g_decals_data"


class FakeReloc:
    def __init__(self, virtual_address, symbol_name,
                 reloc_type=IMAGE_REL_I386_DIR32):
        self.virtual_address = virtual_address
        self.symbol_name = symbol_name
        self.reloc_type = reloc_type


def _xbe_available():
    return bool(u._xbe_sections())


class XbeSymbolAddrTest(unittest.TestCase):
    """Address resolution straight out of the pristine XBE."""

    def setUp(self):
        if not _xbe_available():
            self.skipTest("pristine XBE not available")

    def test_resolves_label_that_name_matching_misses(self):
        """The whole point: g_decals_data matches no kb.json name."""
        self.assertIsNone(
            u._load_symbol_addrs().get(u._normalize_global_symbol(NAMED_MISS)),
            "test premise broken: kb.json now has a g_decals_data global, so "
            "this no longer exercises the label-mismatch path")
        hints = u._xbe_dir32_symbol_addrs(
            DECAL_FUNC_VA, [FakeReloc(DECALS_DATA_OFF, NAMED_MISS)])
        self.assertEqual(hints.get(NAMED_MISS), DECALS_DATA_ADDR)

    def test_hex_spelled_label_agrees_with_its_own_name(self):
        """DAT_0032516c already self-describes; the XBE must agree with it."""
        hints = u._xbe_dir32_symbol_addrs(
            DECAL_FUNC_VA, [FakeReloc(0x5C, "DAT_0032516c")])
        self.assertEqual(hints.get("DAT_0032516c"), 0x32516C)

    def test_disp32_relocs_are_ignored(self):
        """A call site holds a displacement, not an address."""
        hints = u._xbe_dir32_symbol_addrs(
            DECAL_FUNC_VA,
            [FakeReloc(0x1A, "FUN_0008d9f0", IMAGE_REL_I386_REL32)])
        self.assertEqual(hints, {})

    def test_out_of_range_value_is_ignored(self):
        """+0x0 of this function is `push ebp`-ish opcode bytes, not a VA."""
        hints = u._xbe_dir32_symbol_addrs(
            DECAL_FUNC_VA, [FakeReloc(0x0, "bogus")])
        self.assertNotIn("bogus", hints)

    def test_conflicting_addresses_drop_the_symbol(self):
        """Two sites, two answers -> refuse to guess rather than pick one."""
        hints = u._xbe_dir32_symbol_addrs(
            DECAL_FUNC_VA,
            [FakeReloc(DECALS_DATA_OFF, "dup"), FakeReloc(0x5C, "dup")])
        self.assertNotIn("dup", hints)


class CalleeSiteResolutionTest(unittest.TestCase):
    """_xbe_addrs_at_sites also backs real-callee slots, whose relocation
    sites come from StubManager rather than the target's own reloc list.

    Note: in the targets sampled so far every real-callee global is spelled
    DAT_<hex>, which name matching already resolves correctly, so this path is
    sound and live (7/7 resolved on FUN_0008c030) but has not been observed to
    CHANGE a result.  It is tested here rather than left to chance.
    """

    def setUp(self):
        if not _xbe_available():
            self.skipTest("pristine XBE not available")

    def test_dict_form_resolves(self):
        site = DECAL_FUNC_VA + DECALS_DATA_OFF
        self.assertEqual(u._xbe_addrs_at_sites({NAMED_MISS: site}),
                         {NAMED_MISS: DECALS_DATA_ADDR})

    def test_pair_iterable_form_resolves(self):
        site = DECAL_FUNC_VA + DECALS_DATA_OFF
        self.assertEqual(u._xbe_addrs_at_sites(iter([(NAMED_MISS, site)])),
                         {NAMED_MISS: DECALS_DATA_ADDR})

    def test_pair_form_drops_disagreeing_sites(self):
        """A dict would silently keep the last write; the pair list must not."""
        pairs = [("dup", DECAL_FUNC_VA + DECALS_DATA_OFF),
                 ("dup", DECAL_FUNC_VA + 0x5C)]
        self.assertNotIn("dup", u._xbe_addrs_at_sites(pairs))

    def test_agreeing_duplicate_sites_are_kept(self):
        """Same symbol, same address at both sites -> not ambiguous."""
        site = DECAL_FUNC_VA + DECALS_DATA_OFF
        self.assertEqual(
            u._xbe_addrs_at_sites([(NAMED_MISS, site), (NAMED_MISS, site)]),
            {NAMED_MISS: DECALS_DATA_ADDR})


class HintPriorityTest(unittest.TestCase):
    """Hints must outrank every name heuristic in the seeder."""

    def test_hint_beats_a_wrong_bare_name_match(self):
        """_GLOBAL_NAME_ALIASES exists because a bare-name match can land on a
        DIFFERENT kb global (g_object_header_data vs object_header_data).  An
        XBE-derived address is ground truth and must win outright."""
        known = sorted(u._KNOWN_GLOBAL_BYTES)[0]
        seeds = u._build_globals_seeds(
            {"g_object_header_data": 0x500000},
            sym_addr_hints={"g_object_header_data": known})
        self.assertIn(0x500000, seeds)
        self.assertEqual(seeds[0x500000][:4],
                         u._KNOWN_GLOBAL_BYTES[known][:4])

    def test_absent_hint_falls_back_to_name_resolution(self):
        """Strictly additive: no hint must change nothing."""
        smap = {"DAT_%08x" % sorted(u._KNOWN_GLOBAL_BYTES)[0]: 0x500000}
        self.assertEqual(u._build_globals_seeds(smap),
                         u._build_globals_seeds(smap, sym_addr_hints={}))

    def test_dllimport_pairing_uses_hints_for_the_direct_side(self):
        """A hinted oracle label must still pair with the candidate's
        __imp__ slot, so both sides deref to one page."""
        addr = 0x46BD40
        orc = {NAMED_MISS: 0x500000}
        lft = {"__imp__event_manager_globals": 0x500900}
        seeds = u._seed_dllimport_indirection(
            orc, lft, orc_addr_hints={NAMED_MISS: addr})
        self.assertEqual(seeds.get(0x500900), struct.pack("<I", 0x500000))


if __name__ == "__main__":
    unittest.main(verbosity=2)
