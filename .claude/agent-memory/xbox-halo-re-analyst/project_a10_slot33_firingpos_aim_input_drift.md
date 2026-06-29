---
name: a10-slot33-firingpos-aim-input-drift
description: a10 slot-33 grunt firing_pos/move_mode A/B divergence (found by ab_check.py) = TWO downstream symptoms of ONE upstream root = aim/look vector (actor+0x180/+0x18c) divergence present from the FIRST captured frame; ported biped-aim producers 0x1b0630/0x1b3690 are prime suspects. Gated toggle-bisect EXONERATED firing-pos eval + action-block (Group A, 14 fns) and path/movement layer (Group B, 41 fns).
metadata:
  type: project
---

# a10 slot-33 firing_pos/move_mode divergence — ROOT = aim-vector input drift (2026-06-29)

A/B harness (`tools/equivalence/ab_check.py`, level a10, fixture a10-checkpoint-5s-action,
cachebeta vs default) flagged grunt **slot 33** diverging: faithful holds
`firing_pos`(+0x470)=13611 / `move_mode`(+0x46c)=2 sustained 100+ ticks; patched
resets to 0/1 (`AB_EXIT=3`). Same entity confirmed (slot 33, same early pos).

## What it ISN'T (gated toggle-bisect, all liveness-confirmed)
- **Cross-product store-swap in actor_input_update 0x3dc20** — ALREADY FIXED in this
  tree (actors.c ~7318/7344 store uz/ux and az/ax in FPU-LIFO order; the prior-art
  fix commit c85ad81a is a DIFFERENT branch lineage but the code is present here).
- **Group A EXONERATED** (firing-pos eval/scorer + action-block writers, 14 fns:
  0x163d0 0x14c10 0x25c10 0x272d0 0x27090 0x16590 0x15250 0x19d00 0x15cf0 0x15eb0
  0x19940 0x19c70 0x1a7e0 0x19370). Deactivated all 14, gate-confirmed ORIGINAL in
  running build, divergence PERSISTED.
- **Group B EXONERATED for the teardown** (path/movement, 41 fns in path.obj +
  path_smoothing + path_obstacle_avoidance + actor_moving.obj incl. actor_move_to_point
  0x2d720, actor_move_to_firing_position 0x2d900, actor_path_refresh 0x2cdb0,
  path_input_new 0x5dfc0). Deactivating Group B FIXED the onset lag (grunt starts
  moving on-time) but the teardown PERSISTED.

## The mechanism (two symptoms, one root)
1. **Onset lag (~10 ticks):** patched grunt starts physically moving ~10 ticks late
   after the move command. Lives in the path/movement EXECUTION layer (Group B fixed it).
2. **Teardown:** `FUN_00015cf0` (0x15cf0, actor_looking.c — per-tick guard-state look
   countdown) `wake:` block (line 1875-1893) clears the firing-position hold (+0xa4/0xa6/0xa8,
   sets +0xc0=0, +0xc4=-1). Gated by `if (+0xa0==0 || at_dest(+0x484)==0) return;` — only
   fires after arrival. The +0xa6 countdown phase + the post-wake re-init (by
   FUN_00019370 action_obey) collapses mm 2→1, +0xe0/+0xca/+0x3e8 →0.

## ROOT (decisive free probe — pre-move aim divergence)
At the FIRST captured frame (tick ~6322), BEFORE the grunt moves, positions are
near-identical but the **aim/look vectors differ massively**:
- FAITHFUL:  aimx(actor+0x180)=-0.92  lookx(actor+0x18c)=-0.92  (aimed toward target)
- PATCHED:   aimx=-0.31  lookx=-0.31  (different direction, ~37deg off)
- a6 countdown also starts at different values (78 vs 126).

`actor+0x180` (aiming) <- `biped+0x1ec`; `actor+0x18c` (looking) <- `biped+0x210`
(copied verbatim by 0x3dc20 at actors.c:7273/7291). So the divergence is UPSTREAM in
the **biped aim/look vectors**, accumulated before AI scope — input drift, not a
single static mis-lift in the AI layer. Consistent with multi-session a10 memory
("ALL writers UNPORTED -> input divergence").

## CANDIDATE SET for next step (biped aim producers, units/biped layer)
- **0x1b0630** (ported) and **0x1b3690** (ported) — biped aim producers writing
  biped+0x1ec/+0x210 (per reference_biped_update_dispatcher + project_biped_aim_up_euler_nan
  fix 28125ffc). PRIME SUSPECTS — toggle these (units/biped layer) next.
- 0x1a5300 (unported) — biped BSP/move update, also touches biped aim state.
- These are x87-sensitive (euler/quaternion aim math) — could be precision-through-
  threshold rather than a clean swap bug.

## METHOD LESSONS (critical — burned 2 vacuous A/B cycles)
- `capture_scenario.py replay --xbe default.xbe` does NOT deploy the local XBE — it
  only boots whatever default.xbe is already on the box. Must `deploy_xbox.py --xbe-only`
  FIRST.
- `build.py --target halo` builds only the ELF, NOT the patched XBE. Use
  `build.py --target patched_xbe` to run patch.py (the `patched_xbe` cmake target
  DEPENDS on the halo ELF + patch.py but NOT on kb.json — so a kb.json-only change
  does NOT trigger re-patch; remove halo-patched/default.xbe or build patched_xbe to force).
- HARD GATE every toggle: `verify_toggles_live.py --addr <orig VAs>` must read ORIGINAL
  (genuine 55 8b ec prologue, no push-ret) in the RUNNING build before trusting any A/B.
  ported=false leaves the ORIGINAL VA bytes intact (no redirect) and puts JMP-to-orig at
  the IMPL entry — so push-ret at the original VA = STILL ACTIVE = vacuous test.
- behavior_diff/ab_check window ±4 absorbs the ~10-tick onset lag; read the per-tick
  slot probe, NOT the bare DIVERGENT verdict, to separate onset-lag from teardown.
