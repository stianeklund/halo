# Repro: effects.c:585 assert — FP-weapon effect claimed by orphaned local slot

**Level:** a10 (Assault on the Control Room)
**Exception (faithful to cachebeta — the original debug build halts here too):**

```
EXCEPTION halt in c:\halo\SOURCE\effects\effects.c,#585: effect->local_player_index==NONE [rev=c74ed4cf]
```

Fires in `effects_start_on_first_person_weapon` (`0x9e0d0`, src/halo/effects/effects.c:1397).

## What this fixture is for

A durable way to re-reach this exception so we can later work on **rendering the
fatal bluescreen the way cachebeta does**. This fixture captures the *repro*, not
a fix. The render-parity work is deferred (see "Known open issue" below).

## Files

| File | Committed? | What |
|------|-----------|------|
| `core.bin` | **NO** (gitignored — copyrighted game-state heap, 3,428,352 bytes) | The a10 checkpoint pulled from xemu at `E:\GAMES\halo-patched\core\core.bin` on 2026-07-04. Restoring it reaches the pre-assert state. |
| `debug.txt` | no (global ignore) | Game log capturing the assert (15:52:58) and the two `corrected player control for restored saved game` events (14:49:43/46). |
| `gamestate.txt` | no | Engine gamestate dump at crash time. |
| `README.md` | yes | This file. |

`core.bin` is host-only. Re-pull with the `getfile` transport in
`tools/xbox/capture_scenario.py` (`remote_size` + `getfile` on
`<title_root>\core\core.bin`). Keep it under this directory.

## Mechanism (verified against the binary + live xemu memory)

Root cause is a **cross-slot double-claim** of a first-person-weapon effect:

1. The single local player (campaign) was in local slot 0; its FP-weapon block
   (`fp[0]`, at `*0x46bea8 + 0*0x1ea0`) was activated → `effects_start(0, weapon)`
   stamps that weapon's effects with `local_player_index = 0`.
2. A **saved-game restore with a controller-port mismatch** (the two
   "corrected player control for restored saved game" events) reassigns the same
   player/biped to **local slot 1** — the local-player index equals the controller
   port (`create_local_players`, src/halo/main/main.c:186).
3. Nothing deactivates the vacated slot 0. `first_person_weapons_update`
   (`0xdeb60`) just `continue`s past player-less slots — **byte-faithful** to the
   original. So `fp[0]` stays active with its effects still stamped `= 0`.
4. Slot 1 later activates FP effects on the **same weapon object** →
   `effects_start(1, weapon)` finds an effect already stamped `local_player_index = 0`
   → assert (`effect->local_player_index != NONE`) → `system_exit(-1)`.

### Live-memory evidence (crash-time state, 2026-07-04)

- `players_globals` (`*0x5aa6cc` = `0x80214e00`): `local_player_count = 1`;
  mapping slot 0 = `-1`, slot 1 = player `0xec700000`.
- player datum idx 0 (`player_data` `*0x5aa6d4`): `player+2` (local-slot back-ref) = **1**,
  `player+0x34` (unit) = `0xe3f50186`.
- FP blocks (`*0x46bea8` = `0x80063f18`, stride `0x1ea0`):
  - `fp[0]`: flag(`+0`) = **1 (active)**, unit(`+4`) = `0xe3f50186`, weapon(`+8`) = `0xe5720165`
  - `fp[1]`: flag = 0, unit = `0xe3f50186`, weapon = `0xe5720165`  ← same biped/weapon, current slot
  - `fp[2]`, `fp[3]`: empty (`-1`)

## Faithfulness

Not a lift regression in the effect/FP path. Verified against the original
delinked binary:
- `effects_start_on_first_person_weapon` (`0x9e0d0`): original has the identical
  `cmp WORD[+0x4c],0xffff` → `display_assert(...,0x249,1)` → `push -1; call system_exit`.
- `effects_stop_on_first_person_weapon` (`0x9c810`): faithful (matches on
  `local_player_index`; location-list GC identical).
- `first_person_weapons_update` (`0xdeb60`): faithful (skips player-less slots).
- toggle `FUN_000dcb30` (`0xdcb30`): faithful; register ABI confirmed
  `@<di>`=local_player_index, `@<bl>`=activate.

The corrupting state originates in the (unported/original) "corrected player
control for restored saved game" path. cachebeta reaches the same assert.

## Repro steps

1. Boot a10 with `core.bin` overlaid:
   `rtk python3 tools/xbox/capture_scenario.py replay --level a10 --scenario effects-585-fp-slot-mismatch --xbe default.xbe`
   (or boot the map and overlay `d:\core\core.bin` manually — start=core path).
2. Ensure the **controller is on a different port than the save was made on**
   (this is what triggers "corrected player control" → the slot-1 reassignment).
   Bind the pad to port 2 (index 1) at restore.
3. In-engine, fire / bring up the first-person weapon so its effects activate on
   the reassigned slot. The assert fires in `effects_start_on_first_person_weapon`.

To confirm the precondition without firing, read the live FP blocks and
`players_globals` (offsets above); a healthy single-player state has the player
in slot 0 with `fp[0]` owning the weapon and `fp[1..3]` empty.

## Copyright-free logic repro (Unicorn)

`tools/equivalence/repro_effects_585.py` reproduces the **assert logic** with no
game data: it runs the real delinked `effects_start_on_first_person_weapon`
(FUN_0009e0d0) in Unicorn against a hand-crafted synthetic effect record (one
effect, `+0x3c`=weapon, `+0x4c`=claiming slot), stubbing the pre-assert callees.

```
rtk python3 tools/equivalence/repro_effects_585.py            # BUG:   reaches effects.c#585 + system_exit(-1)  -> exit 0
rtk python3 tools/equivalence/repro_effects_585.py --control  # NONE:  no assert, clean return                  -> exit 0
rtk python3 tools/equivalence/repro_effects_585.py -v         # add a call trace
```

Deterministic, ~a few hundred bytes of synthetic memory, committable. Requires
`delinked/effects.obj` present (gitignored; re-export via ghidra-live if missing).
This proves the *claim logic* faults; it does NOT exercise the render/halt path
(that needs the real engine + `core.bin`).

## Known open issue (the deferred render work)

On cachebeta this assert renders the white-on-blue fatal screen. On our patched
build it does **not**: the assert logs but `halt_and_catch_fire` (`0x1029a0`,
lifted, deployed, correct) either never gets to loop, or the build has corrupted
the **D3D dirty-resource list** so rendering hard-faults in `FUN_00251d60` at a
wild pointer first. Two sub-problems remain open:
1. Why the assert / `system_exit` does not actually stop the game (execution
   continues into render).
2. What corrupts the D3D dirty-resource chain (`render-context+0x84` /
   `DAT_00632900`) in the patched build.

See memory: `project_halt_and_catch_fire_lifted`,
`project_bluescreen_blocked_by_d3d_resource_corruption`, and this fixture's
sibling memory `project_effects_585_fp_slot_repro`.
