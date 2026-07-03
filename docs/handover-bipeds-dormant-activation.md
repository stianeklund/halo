# Handover — bipeds.obj `ported:false` functions to look at

## Objective
Identify and triage the 12 dormant (`ported:false`) bipeds.obj functions now in
`main` — which are VC71-capped, which are activation candidates, and which to
look at next.

## Current State
- All bipeds work LANDED in `main` (fast-forward `ba585430 → c57c15d0`,
  2026-06-08). 27 functions active; **12 remain `ported:false` (inert)**.
- `ported:false` = C body is dead code; `patch.py` tail-calls the ORIGINAL
  machine code. Improving/scoring them changes nothing at runtime until flipped.
- This session re-lifted ONE of them (`0x1a2f40`, 19.3→75.9%); the other 11 were
  empirically confirmed VC71-capped or equivalence-blocked (NEEDS_RUNTIME).
- Activated nothing: `ported:true` count is 2718 before and after the FF.

### The 12 dormant functions (VC71 scores re-verified this session)
| addr | decl / conv | VC71 | category | why dormant / note |
|------|-------------|------|----------|--------------------|
| 0x1a0680 | `char FUN_001a0680(int unit_handle)` — **cdecl** | **83.2** (61.5 reported) | A | closest cdecl to 88%; 61.5 is whole-TU NOP-inflation, real=83.2 via per-fn ref |
| 0x1a1b90 | `int biped_approximate_surface_index(int, float*)` — **cdecl** | 78.6 | A | below permuter floor — investigate lift (control-flow/FPU) before permuting |
| 0x1a1e70 | `void FUN_001a1e70(int unit_handle)` — **cdecl** | 85.9 | A | permuter ran 188 non-vacuous iters → +0.15%; capped by `@<edi>` keystone `FUN_001a1a10` |
| 0x1a0b30 | `char FUN_001a0b30(int@<edi>)` | 81.4 | B | reg-arg cap; equiv 100% cov but product (object_delete) unwitnessed → NEEDS_RUNTIME |
| 0x1a0e00 | `void FUN_001a0e00(float threshold, int@<eax>)` | 82.3 | B | **strongest equiv** (50/50 HIGH, 3 exact heap writes, 0 div); CX=1 branch unhit → NEEDS_RUNTIME |
| 0x1a1a10 | `int FUN_001a1a10(float, float*, void*, float*@<eax>, int@<edi>)` | 80.0 | B | reg-arg cap (this is the 1e70 keystone) |
| 0x1a2160 | `void FUN_001a2160(int@<eax>)` | 73.4 | B | reg-arg cap |
| 0x1a2290 | `char FUN_001a2290(int@<edi>)` | 84.4 | B | reg-arg cap; obj+0x424 disjoint write proven BENIGN; actor-veto path unexercised → NEEDS_RUNTIME |
| 0x1a25e0 | `void FUN_001a25e0(int@<ecx>)` | 79.3 | B | reg-arg cap |
| 0x1a2a60 | `void FUN_001a2a60(int@<edi>, char *state)` | 86.5 | B | reg-arg cap |
| 0x1a2b10 | `void FUN_001a2b10(int@<edi>)` | 84.0 | B | reg-arg cap |
| 0x1a2f40 | `void FUN_001a2f40(void *physics@<esi>)` | **75.9** | C | re-lifted this session (was 19.3, store bug fixed); cap ~76-77% (value-coupled writeback + x87) |

### Categories (drives strategy)
- **A — cdecl, VC71-improvable** (0680, 1b90, 1e70): no frameless-fastcall penalty;
  can plausibly reach ≥88% byte-match → activate on VC71 evidence.
- **B — register-arg, VC71 structurally capped mid-80s** (0b30, 0e00, 1a10, 2160,
  2290, 25e0, 2a60, 2b10): `@<reg>` callees compile to cdecl frame+pushes; VC71
  scores vs the original frameless prologue → permanent ceiling. **Only activatable
  via equivalence/dual-oracle**, not VC71. Earned, not assumed (permuter positive
  control + analyst byte-identical fixes both confirmed the cap).
- **C — was a real defect, now re-lifted** (2f40): capped ~76%, stays dormant.

## Confirmed
- 12 addresses above are all `ported:false` on `main` (`rtk jq` on main:kb.json).
- `ported:true` set identical before/after FF (2718) — no activations.
- 0x1a2f40 re-lift builds clean (`BUILD_EXIT=0`); bipeds.c byte-identical to the
  vetted re-lift; all merged JSON (`kb.json`, `kb_meta.json`, scores, leaf_cache)
  valid (`jq empty` OK).
- Equivalence re-run this session on 0e00/2290/0b30 (unicorn_diff, combat/tweaked
  snapshot, env `BIPED_REAL_TAGS=1 BIPED_SIBLING_RESOLVE=1 BIPED_HEAP_COMPARE=1`)
  → xbox-halo-lift-reviewer verdict **NEEDS_RUNTIME** for all three.

## Important Changes (this session, now in main)
- `src/halo/units/bipeds.c`: 0x1a2f40 full re-lift + NaN-assert refine (dormant).
- `kb.json`: `FUN_00150550` arg4 `float`→`int` (unported callee of 0x1a2f40 only).
- `docs/handover-bipeds-dormant.md`: original investigation-planning doc (now stale).
- `_inject_1430_dualoracle.py`, `_assemble_biped_snaps.py`, `_assemble_combat_snaps.py`,
  `_biped_capture.py`, `_probe_memtrace.py`, `_probe_sideeffects.py`: dual-oracle /
  equivalence scaffolding for activation. **Archived 2026-07-03 to `tools/archive/`**
  (single-session scaffolding; restore from there if you resume this investigation).
- `.claude/agent-memory/xbox-halo-re-analyst/` + `xbox-halo-lift-reviewer/`:
  field maps, VC71 ceilings, FILD-vs-FLD rule, equivalence snapshot reference,
  NEEDS_RUNTIME activation-bar notes.

## Validation
- Build: `rtk python3 tools/build/build.py -q --target halo` → exit 0 (hazard scan:
  only 16 benign `duplicate_args` + 2 `frame_sizes`, all dangerous categories 0).
- FF to main: `git merge --ff-only` → `ba585430..c57c15d0`, 25 files, no conflicts.
- NOT run this session: any new VC71 re-score beyond the recorded values; permuter
  on 0x1a2a60 (exit 144 / empty — abandoned, class already proven capped via 1e70).
- NOT done: any dual-oracle/golden run that witnesses a Category-B product on its
  gated path (the missing piece for activation).

## Uncertain / Risks
- Category-B VC71 ceilings are proven structural (reg-arg frameless prologue), but
  the **equivalence verdicts are NEEDS_RUNTIME, not PASS** — each candidate has an
  unexercised gated path: 0e00 CX=1 output branch (`word[obj+0x460]=1`), 2290 actor
  veto (`actor_aim_jump`, all seeds had `actor_handle==-1`), 0b30 product
  (`object_delete` via stubbed `FUN_00140cc0`, return always 0).
- Activating any dormant fn flips runtime behavior; several are reg-arg callees of
  active functions — build + smoke-test active callers after any activation.
- 0x1a2f40 at 75.9% is below 88% — keep dormant; its value is the fixed store bug
  + banked RE knowledge, not activation.

## Next Steps (ranked by ROI)
1. **0x1a0e00 via dual-oracle** — best activation candidate. Build the true
   dual-oracle harness (per 0x1a1430 precedent: per-fn asm thunk to place the
   `@<eax>` arg in register), drive `threshold` between the two scaled tag
   thresholds (`obj_tag+0x3dc`/`+0x3e0`) to witness the CX=1 branch, then flip
   `ported:true` if it matches.
2. **0x1a0680 via VC71** — closest cdecl (83.2% real). Re-export an exact-range
   delinked ref, `compare_obj.py`, then a small structural/permuter pass to clear
   88% → activate on byte-match.
3. **0x1a1e70** — in permuter band (85.9) but capped by keystone `FUN_001a1a10`;
   either resolve the keystone or accept the cap (don't re-churn — proven).
4. Reuse the dual-oracle harness from step 1 for the rest of Category B
   (0b30/2290/2160/25e0/2a60/2b10/1a10).

## Resume Prompt
> The 12 dormant (`ported:false`) bipeds.obj functions are in `main` (@c57c15d0)
> as inert dead code. See `docs/handover-bipeds-dormant-activation.md` for the
> per-function table + categories. Category A (cdecl: 0680/1b90/1e70) can reach
> ≥88% VC71 → activate on byte-match. Category B (8 reg-arg fns) is VC71-capped
> mid-80s → activate only on equivalence/dual-oracle. 0x1a2f40 (C) is re-lifted,
> capped ~76%, stays dormant. Start with 0x1a0e00 (strongest equiv, needs the CX=1
> branch driven) by building the dual-oracle harness (asm thunk for the `@<reg>`
> arg, per the 0x1a1430 precedent in `tools/_inject_1430_dualoracle.py`). Relevant
> memory: project_bipeds_dormant_improvement, feedback_true_dual_oracle_by_address,
> feedback_vc71_structural_ceiling, reg_arg_keystone_vc71_ceiling.
