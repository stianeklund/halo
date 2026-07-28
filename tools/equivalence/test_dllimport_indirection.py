#!/usr/bin/env python3
"""Guard: a dllimport (indirect) global reference must resolve to the same
storage as the other side's direct reference.

kb.json globals are ``HDATA`` = ``__declspec(dllimport)``, so the clang
candidate reaches one through a pointer-to-pointer::

    mov eax,[__imp__event_manager_globals]   ; slot holds &global
    push eax

while the delinked MSVC oracle references the same storage directly as
``DAT_0046bd40``.  patch_dir32_relocs gives each a globals slot, but only the
direct side's slot address *is* the pointer -- the indirect side's slot has to
*contain* it.  Left unseeded that slot is zero, so the candidate passed NULL
where the oracle passed its slot: six targets (event_manager_initialize,
event_manager_dispose, cheats_initialize, player_ui_initialize,
player_ui_dispose, players_initialize_for_new_map) reported a csmemset arg[0]
mismatch of exactly ``oracle=0x500000 candidate=0x0``, and their memory traces
compared writes to two different pages.

VC71 cannot catch this class -- it is a harness artifact, not a codegen
difference -- so these are unit tests over the seed map rather than a
byte-match check.
"""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import unicorn_diff as u

# event_manager_globals lives at 0x46bd40; the oracle spells it DAT_0046bd40.
GLOBAL_ADDR = 0x46BD40
ORC_SLOT = 0x500000
LFT_SLOT = 0x500900


class DllimportIndirectionTest(unittest.TestCase):
    def test_symbol_resolution_still_works(self):
        """The fix relies on name->address resolution; fail loudly if that
        table stops carrying the symbol rather than silently seeding nothing."""
        addrs = u._load_symbol_addrs()
        self.assertIn("event_manager_globals", addrs)
        self.assertEqual(addrs["event_manager_globals"], GLOBAL_ADDR)
        self.assertEqual(
            u._normalize_global_symbol("__imp__event_manager_globals"),
            "event_manager_globals")

    def test_imp_slot_seeded_with_direct_side_slot(self):
        """Candidate indirect + oracle direct -> imp slot holds ORACLE's slot."""
        seeds = u._seed_dllimport_indirection(
            {"DAT_0046bd40": ORC_SLOT},
            {"__imp__event_manager_globals": LFT_SLOT})
        self.assertIn(LFT_SLOT, seeds)
        self.assertEqual(struct.unpack("<I", seeds[LFT_SLOT])[0], ORC_SLOT)

    def test_dat_spelling_is_required(self):
        """Regression on the first cut of this fix, which resolved only
        friendly names and so never matched the oracle's DAT_<addr> side --
        producing an empty seed map and no behaviour change at all."""
        seeds = u._seed_dllimport_indirection(
            {"DAT_0046bd40": ORC_SLOT},
            {"__imp__event_manager_globals": LFT_SLOT})
        self.assertTrue(seeds, "DAT_<addr> oracle symbols must resolve")

    def test_reverse_direction(self):
        """Oracle indirect + candidate direct -> imp slot holds CANDIDATE's."""
        seeds = u._seed_dllimport_indirection(
            {"__imp__event_manager_globals": ORC_SLOT},
            {"event_manager_globals": LFT_SLOT})
        self.assertEqual(struct.unpack("<I", seeds[ORC_SLOT])[0], LFT_SLOT)

    def test_both_indirect_emits_nothing(self):
        """Both sides deref the same zero already, so there is nothing to
        reconcile and no slot should be forced."""
        seeds = u._seed_dllimport_indirection(
            {"__imp__event_manager_globals": ORC_SLOT},
            {"__imp__event_manager_globals": LFT_SLOT})
        self.assertEqual(seeds, {})

    def test_unresolvable_symbol_is_skipped(self):
        seeds = u._seed_dllimport_indirection(
            {"DAT_0046bd40": ORC_SLOT},
            {"__imp__no_such_global_anywhere": LFT_SLOT})
        self.assertEqual(seeds, {})

    def test_real_data_overrides_indirection_seed(self):
        """_build_globals_seeds must win: callers apply it after this map, so a
        snapshot-backed address is not clobbered by the synthetic pointer."""
        src = Path(u.__file__).read_text(encoding="utf-8")
        i_ind = src.index("globals_seeds = _seed_dllimport_indirection")
        i_bld = src.index("globals_seeds.update(_build_globals_seeds(")
        self.assertLess(i_ind, i_bld,
                        "indirection seeds must be applied BEFORE "
                        "_build_globals_seeds so real data takes precedence")


if __name__ == "__main__":
    unittest.main(verbosity=2)
