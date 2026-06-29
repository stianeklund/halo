---
name: pertick-promotion-gate-355f0-33440
description: 2026-06-28 EXACT per-tick type-5-contact partner(parent) type0->3 promotion gate in actor_perception.c — dispatcher 0x355f0 state machine + perception writer 0x33440; perception prop+0x30 = max of 3 channels; all writers UNPORTED
metadata:
  type: reference
---

CONFIRMED (cachebeta.xbe, Ghidra decompile, actor_perception.c strings authoritative).
Answers "what gates the per-tick type-5-contact -> partner type-3 'acknowledged' promotion".

## Terminology (from binary asserts)
- A type-4/type-5 prop is an ORPHAN ("contact"). Its prop+0xc = PARENT prop handle.
  Asserts: "parent_prop->orphan_prop_index == iterator.index" (0x192, 0x2ef);
  "prop->parent_prop_index != NONE" (0x2ea). So user's "linked PARTNER" = the PARENT prop.
- The prop that gets promoted to type 3 is the PARENT, via its OWN state machine case 0->1->3.
  The type-5 orphan is just the contact; its case 4/5 block (prop+0x3a countdown / prop+0x3c
  increment, threshold 0x2c/0x2d) is the orphan's own maturation — NOT the promotion gate.

## Call path to the type-3 write
ai_update -> FUN_0003f5f0 (per-tick sweep, PORTED 88.4%) -> ... -> dispatcher FUN_000355f0
(0x355f0, UNPORTED) -> become_acknowledged FUN_00033330 (0x33330, UNPORTED) sets prop+0x24=3 @0x3341d.
become_acknowledged has EXACTLY 2 callers: 0x355f0 (per-tick state machine, THE promotion path)
and 0x3d430 actor_handle_unit_effect (PORTED, but EVENT-driven not per-tick; its 3 callers
0x3e570/0x40360/0x55110 are event handlers — NOT the timer-driven promotion).

## THE GATE (dispatcher 0x355f0, switch(prop+0x24), iVar8=current prop)
Two sequential stages. BROKEN evidence (partner stuck prop+0x24==0) => stuck at STAGE A.
- STAGE A (case 0, line ~315): `if (0 < prop[0x30])` — perception (int16, asserted 0..3) must be
  >=1 to leave state 0; on exit sets new_state local_28=1 and resets accumulator prop+0x2c=0.
- STAGE B (case 1, lines ~399-402): `prop[0x2c] += rate; if (prop[0x2c] < _DAT_002533c8) stay;
  else local_28=3` -> become_acknowledged. rate local_30 = DAT_00255f30[perception + knowledge*4]
  -> one of {0, actr+0x74, actr+0x70, actr+0x6c, 1.0}. Low perception or knowledge_type
  (FUN_0002f380) selecting rate 0 => "290 ticks late" or never.
- At LAB_000361da: `case 3: local_11=FUN_00033330();` then `prop+0x24 = new_state`.
- SUSPECT FIELD = prop+0x30 (perception): gates Stage A AND indexes Stage B rate table.
- Promotion does NOT read confirm-timer prop+0x68 directly. prop+0x68 is a parallel countdown
  (lines 186-188: hits 0 -> prop+0x66 unit_effect = -1). It only matters INDIRECTLY via 0x33440 (below).

## WHO WRITES perception prop+0x30  =>  prop_status_refresh FUN_00033440 (0x33440, UNPORTED)
prop+0x30 = max of THREE sub-perception channels (refresh.txt lines 398-409):
  prop+0x30 = max(prop+0x32 [visual], max(prop+0x34 [sound/unit_effect], prop+0x36 [extra-sense]))
- prop+0x32 (visual) = FUN_000314f0(actor, prop, prop+0x104, knowledge, prop+0x38 LOS, ...) UNPORTED.
  Primary input = prop+0x38 (LOS level), written by 0x33440 from ai_test_line_of_sight FUN_000416e0.
- prop+0x34 (sound/effect): if prop+0x66==1||==2 -> 3; else FUN_00031850(...) UNPORTED.  <-- depends on
  prop+0x66 (unit_effect). Confirm-timer prop+0x68 hitting 0 sets prop+0x66=-1, collapsing THIS
  channel — the mechanism behind "after confirm window lapses, never promotes".
- prop+0x36 (extra): 3 if prop+0x66==0.
- ALL gated by prop+0x133==0 (skip) and prop+0x132 (else-branch zeroes 0x30/0x32/0x34/0x36).
- knowledge_type = FUN_0002f380(actor,prop) UNPORTED (asserted 0..3).

## REFRESH THROTTLE (whether 0x33440 even runs this tick) — dispatcher
0x33440 call gated on local_34 (refresh flag), set 1 when prop+0x26 (refresh counter, ++/tick)
reaches actor+0x4e (refresh PERIOD): line 244 `actor[0x4e] <= refresh_counter` -> refresh + reset
prop+0x26=0. Current-target props (prop+0x63 / prop+99 set from actor+0x270/+0x54/+0x3ac/+0x1d0
match at lines 256-282, propagated orphan->parent at ~281) force refresh. If parent never gets
prop+0x63=1 and counter never hits period, 0x33440 skips it => perception never updates.

## Audit conclusion for ported-side hunt
EVERY function that writes perception (0x33440, 314f0, 31850, 2f380, 416e0, 33330, 355f0) is UNPORTED.
So a static mis-lift cannot directly mis-write prop+0x30. The regression must be an INPUT divergence
feeding these (LOS inputs to 0x416e0; unit_effect prop+0x66/confirm-timer prop+0x68 lifecycle;
actor+0x270/+0x54 current-target handles feeding the refresh gate). Consistent with prior memory
[[reference_a10_prop_deletion_path_faithful]] and [[project_a10_promotion_pathmap_corrected]]
(input EQUAL claim should be re-tested specifically on prop+0x30 / prop+0x38 / prop+0x66 each tick).
