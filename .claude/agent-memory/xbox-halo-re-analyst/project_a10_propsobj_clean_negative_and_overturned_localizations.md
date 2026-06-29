---
name: a10-propsobj-clean-negative-and-overturned-localizations
description: a10 grunt-passive — props.obj all-10-ported audit CLEAN NEGATIVE (none write +0x270/+0xc/state); plus three prior localizations OVERTURNED by this session's deterministic fresh-boot oracle. Bug = ported producer feeding divergent per-tick INPUT to the faithful (unported) promotion machine.
metadata:
  type: project
---

# a10 grunt-passive: props.obj clean-negative + overturned localizations (2026-06-29)

## props.obj exhaustive ported-audit — CLEAN NEGATIVE (analyst a92cd4572c84d5ba6)
All 10 ported props.obj fns disasm-verified vs pristine:
- 0x64100/40/50/60 = prop_data alloc/noop/dispose/clear — touch none of +0x270/+0xc/chain.
- **0x64400 prop_remove_from_actor** (@eax actor,@edi prop) — chain unlink via actor+0x50/[prop+8]; splice `prev->next=removed->next` correct; reads +0x270 only (assert). FAITHFUL.
- 0x64540 prop_iterator_new / 0x64570 prop_iterator_next — read-only chain walk. FAITHFUL.
- 0x64a80 prop_detach = 0x64400+free. FAITHFUL.
- **0x64ab0 prop_get_active_by_unit_index** (the B2 selector candidate) — accept = `(state>=2 OR state<0) AND (prop+0x18==obj OR (prop+0x14!=0 && prop+0x1c==target && !=-1))`. `state>=2` filter is the BINARY'S OWN behavior (returns only active props); C props.c:288 matches disasm exactly. On AI path its callers (0x32380) use it as a QUERY on scripted-look/projectile/vehicle branches, NOT a +0x270 writer on a6's normal-player path. FAITHFUL.
- 0x64ee0 = libtiff TIFFClose, mis-grouped, irrelevant.

**None of the 10 writes actor+0x270, sets/clears [prop+0xc] parent, or mis-links.** Those exact divergent fields are written ONLY by UNPORTED code: actor+0x270 by actor_situation_update 0x303f0; [prop+0xc] parent + state by 0x33330 / 0x355f0. B2/linkage branch CLOSED. Confidence HIGH.

## Three prior localizations OVERTURNED by the deterministic fresh-boot oracle
The deterministic input-replay diff oracle (fixture a10-checkpoint-5s-action, default.xbe vs cachebeta.xbe, FRESH boots — NOT the drifting --loop, NOT old uncommitted artifacts) overturned:
1. [[a10-earliest-divergent-field]] "3 extra props e2a/e2b crowd the chain → scan/creation cluster" — **REFUTED**: deterministic whole-pool diff shows populations IDENTICAL (182/182 active props, 53/53 distinct tracked-objects, ~20 player-props same aggregate state distribution). No extra props. The "extra props" were drift/run-variance artifacts of the old non-deterministic captures.
2. [[a10-passive-culprit-ai-obj]] "single object = ai.obj, suspect 0x3f5f0" — **REFUTED**: ai.obj-alone fresh-boot revert stays PASSIVE (0/2, 14 dense samples). Reverting ai.obj alone does NOT fix. The old ai.obj bisect used the lost "3b" config / drifting loop.
3. props.obj linkage/selection (B2) — **CLEAN NEGATIVE** (audit above).

## Deterministic a6-specific signature (the real, reproducible divergence)
At matched a6 geometry (same grunt slot6/0xe38b, same door, same input):
- **default**: a6's actor+0x270 → ORPHAN prop idx0x15 state=5 (aged/search-terminal), accum=0, vis=0; with a separate PARENT prop idx0x11 state=0. i.e. player tracked by an unmerged parent(state0)+orphan(state5) PAIR.
- **cachebeta**: a6's player-prop = SINGLE prop, parent=-1, state=3 (active), accum=1.0, vis=3.
So default a6's accumulator (+0x2c) never reaches 1.0 → never settles at active-3; it ages to 5. The accumulation per tick is driven by perception (+0x30) and the rate table indexed by actor+0x6c/0x70/0x74 — all UNPORTED math.

## Remaining hypothesis (B1, input-feeder form)
A ported producer upstream feeds the faithful unported promotion machine (0x355f0 dispatcher / 0x33330 promote / 0x303f0 situation) a divergent per-tick INPUT for a6 specifically, so a6's player-prop accumulator stalls (ages to 5) instead of reaching 1.0 (settles at 3). The corrupt input is NOT in props.obj and NOT ai.obj-alone.

## Allocator-revert confound (why naive toggle-bisect is unreliable here)
Reverting allocator objects (encounters.obj/actors.obj) changes actor allocation (slot/salt) AND scenario timing → a6 may not be the door grunt, may not reach the door in a 35s fresh boot. So "alloff = FIXED" may be confounded (a DIFFERENT grunt engaged). Single-object reverts of NON-allocator objects are measurable; allocator reverts are not, under the deterministic a6-identity oracle.

## Tools (deterministic era)
/tmp/pool_diff.py (whole-pool diff, a6 by slot6/0xe38b + player-track), /tmp/scan_door.py (build-robust door-grunt finder by position+player-track), /tmp/a6_rate.py (per-dwell promote-rate, PLAYER-prop filter unit==0xe3f50186, criterion vis>=3 or oST==3 NOT oST>=3). default.xbe replay loops are deterministic only on FRESH boots; --loop iterations DRIFT.
