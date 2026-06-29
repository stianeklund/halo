---
name: encounter-blind-0x40-writers-faithful
description: 2026-06-29 DISASM result for the a10 vis-divert hunt. encounter+0x40 = the BLIND flag (pool *0x5ab270 = "encounter", data_new at 0x58eb0). The local_4 divert gate in prop_status_refresh 0x33440 reads blind via datum_get(encounter_pool, actor+0x34)+0x40. BOTH ported writers of encounter+0x40 are byte-FAITHFUL (0x5a120 init-from-def, 0x5ad60 encounter_set_blind), and there is NO per-tick writer — so the "ported fn sets blind=1" hypothesis is REFUTED. Divergence, if real, is the actor->encounter association (actor+0x34) or a mis-inferred divert, not the blind value.
metadata:
  type: reference
---

CONFIRMED from raw capstone disasm of halo-patched/cachebeta.xbe (unported = byte-identical both builds).

## Pool identity: *0x5ab270 = "encounter" pool
data_new at 0x58eb0: push 0x6c (max 108), push 0x80 (element size), push 0x254868="encounter";
call 0x1bfe10; mov [0x5ab270],eax (the ONLY store to the pool ptr, at 0x58ec6).
File c:\halo\SOURCE\ai\encounters.c, pool var "encounter_data". So actor+0x34 = encounter handle.

## encounter+0x40 = the BLIND flag; +0x41 = DEAF
encounter_set_blind 0x5ad60 writes +0x40; encounter_set_deaf 0x5ad90 writes +0x41 (named in kb.json).
The 0x33440 orphan/parent divert gate local_4 ([ebp-4]) reads it:
  [ebp-0x18] = datum_get(encounter_pool, actor+0x34)  (0x33490-0x3349e, only if actor+0x34 != -1 else 0)
  local_4 = ([ebp-0x18] != 0 AND byte[[ebp-0x18]+0x40] != 0) OR (actor+0x6a == 1)
  local_4 != 0 -> orphan branch diverts to ZERO-store 0x33986 -> prop+0x32 vis forced 0.
NOTE the first term is TWO conditions (encounter exists AND blind set), not just "blind".

## ALL encounter+0x40 writers (comprehensive scan: any-size store + and/or bitops, all 85
## encounter-pool-referencing fns, base traced to datum_get(0x5ab270)):
1. 0x5a120 encounter_initialize_from_definition (encounters.obj, ported=true):
   store 0x5a171 `mov [esi+0x40],cl`, cl = (encounter_def+0x20 >> 2) & 1 (bit2). esi=datum_get(encounter_pool).
   C lift encounters.c:2585 matches disasm EXACTLY. encounter_def+0x20 = map tag flags = SAME both builds.
   => init produces identical blind in both builds; CANNOT be the divergence.
2. 0x5ad60 encounter_set_blind(encounter_handle, char value) (encounters.obj, ported=true):
   guard `if (*(char*)(*0x632574 + 1) != 0)` then datum_get + `mov [eax+0x40], value`. C lift
   encounters.c:3341 is FAITHFUL (reproduces the guard, store, datum). 0x632574 written by ai_initialize 0x3f670.
   Only caller (e8-rel32 scan) = 0x55090 (ai_profile.obj, ported) = a SCRIPT/CONSOLE command handler
   (prints debug string, forwards caller value) — NOT a per-tick/spawn path.
3. 0x3dc20 +0x40 stores are ACTOR+0x40, NOT encounter (base-pool trap): esi is the actor record there
   (esi+0x34 is used AS an encounter handle into datum_get at 0x3dfbe). EXCLUDED.

## VERDICT: named-writer hypothesis REFUTED
No per-tick ported writer sets encounter blind. Both real writers faithful; init is map-data-driven
(identical both builds). So encounter+0x40 cannot spontaneously differ between builds for the same encounter.
The coordinator's "by elimination blind=1 in ours" was INFERRED (measured actor+0x6a=3 and prop+0x133=0,
then attributed the remaining divert to blind) — NOT measured, and it collapsed local_4's TWO-condition
first term into one.

## DISCRIMINATING MEASUREMENTS (hand to coordinator; they have replay infra)
M1: in our build, does the state-5 prop reach 0x33967 (visual) or divert to 0x33986? (tests divert directly;
    prop+0x38=0 only means the 0x314f0 entry gate would pass, it does NOT prove the divert.)
M2 (if divert confirmed): read BOTH builds at matched tick: actor+0x34 (encounter handle), whether
    datum_get(encounter_pool, actor+0x34) is null, and encounter+0x40 (blind).
Interpretation:
 - blind identical both builds -> elimination wrong; divert cause is not blind. Re-test M1.
 - actor+0x34 DIFFERS (ours binds an encounter, cachebeta -1/null) -> bug = actor<->encounter ASSOCIATION,
   not blind. Encounter may be legitimately blind from map; defect is binding the grunt to it. Candidate
   ported writer of actor+0x34: FUN_0003c410 (actors.obj, ported=true) store 0x3c49d `mov [esi+0x34],edi`
   (NEEDS esi=actor-record + field=encounter-binding verification before blaming). actor_clear_flee_target
   0x15f30 writes +0x34=-1 but is ported=false. See [[reference_a10_revert_path_setup_audit]] for actor+0x34
   feeders (actor_activate 0x3ec80 chain, 0x3dc20).
 - same encounter, blind genuinely differs -> impossible w/o a writer hit -> re-examine revert-vs-fresh-load
   (init 0x5a120 runs only on fresh load) and 0x55090 script callers.

## Scan caveats (deferred — M1/M2 supersede)
e8-rel32 caller scan only (indirect/table calls to 0x5ad60 not covered; low risk for a script setter).
Wide-store scan limited to encounter-pool-referencing fns; a helper that receives an encounter ptr as a
param without the pool literal would be missed (low risk; would still need datum from a pool-referencing caller).
