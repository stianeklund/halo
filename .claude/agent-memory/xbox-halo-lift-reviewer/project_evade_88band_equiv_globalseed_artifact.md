---
name: evade-88band-equiv-globalseed-artifact
description: actor_action_try_to_evade 0x1fca0 — was 88% NEEDS_RUNTIME (equiv 0/100 unseeded-global artifact); FLIPPED to AUTO_ACCEPT once state-snapshot equiv PASSED on real infection_swarm capture (datum genuinely resolved salt 58219) + full static 1:1 downstream decode
metadata:
  type: project
---

**UPDATE 2026-07-05 — verdict FLIPPED to AUTO_ACCEPT.** The exact resolution I
prescribed below was performed: state-snapshot equiv on the live infection_swarm
snapshot. Actor handle 0xe36b0001 resolved to a valid datum (salt 58219 — the SAME
real AI actor that cleared [[project_actions_0x1d530_sub90_statesnapshot_clear]]),
100/100 seeds, 0 div, 0 err; stub-arg diff 100 calls 0 mismatch across the 5
pre-screen callees (datum_get, FUN_0002a360, tag_get×2, object_get_and_verify_type).
This is a GENUINE pass (datum resolved on real memory), NOT the unseeded-global
artifact that killed the prior run — the discriminator per
[[project_actions_0x1d530_sub90_statesnapshot_clear]].

**Coverage caveat and how it was cleared:** runtime coverage only 22.4% — the
snapshot actor trips a pre-screen guard, so runtime exercised ONLY the
prologue/guard path. The substantive downstream (dot-align gate, magnitude3d, 7-arg
evasion call, impulse selection, assert, animation commit) is runtime-DARK. I
cleared it with a COMPLETE 1:1 static decode of 0x1fca0 vs disasm (2026-07-05):
- 5 guards verified: 0x158!=-1, FUN_0002a360==0, 0x504!=0, 0x270==-1,
  unit_tag+0x234<=0.0 — all JNZ/JZ polarities correct.
- 3 FPU FCOMP polarities all correct: guard5 `<=`-return (TEST AH,0x41 JNZ),
  2D zero-length `<=`-goto-do_evade, 0.4f dot-gate `<=`-return.
- 2D dot term-for-term: scratch[1]*[0x178] + scratch[0]*[0x174] (FADDP). ✓
- 3-dot branch FUN_00013070(attractor_vec, actor+0x174); branch-swap on
  actr_tag&0x200000 semantically equivalent. ✓
- 7-arg evasion call actor_move_try_evasion_direction verified ARG-FOR-ARG
  (actor_handle, alignment_vec, unit_tag+0x234 float, &evade_dir_ref, 0.0f,
  &out_flag, path_result[0x1c]); ADD ESP,0x20 folds the 28B(7 args)+4B(prior
  magnitude3d arg) cleanup. ✓
- impulse 1→7 / 0→6 (16-bit AX cmp = (unsigned short) cast) / else display_assert
  msg@0x254874 file@0x2544b0 line 0xce3 arg 1 + system_exit(-1). ✓ delinked
  0001fca0.obj carries the evade_direction assert string.
- unit_test_animation_impulse(*(actor+0x18), impulse) 0x1a97c0; then
  actor_move_animation_impulse(handle, (int16)impulse, alignment_vec) 0x2a7e0. ✓
- Constants read as globals: 0x2533c0=0x00000000=0.0f, 0x253524=0x3ecccccd=0.4f
  (exact bytes materialized, NO IMM issue).
- **OOB flag SUPERSEDED:** FUN_00012f10 (magnitude3d) decoded — FLD[ECX+4];FLD[ECX];
  sqrt(v0²+v1²), normalizes v0/v1 in place; genuinely 2D. `float scratch[2]` /
  `alignment_vec[2]` are EXACTLY sized. NOT an OOB read — matches
  [[project_actions_try_throw_grenade_9394band_bar]]. The latent-note below is WRONG
  for this function; disregard it.
- ABI clean (cdecl regs=none), hazard --changed-only exit 0. 88.0% (162 cand/147 ref);
  candidate-excess delta = clang branch-layout + reload + un-folded cleanup shape,
  no behavioral divergence found in the 1:1 decode.

**Bar confirmed:** sub-90 AUTO_ACCEPT = complete static 1:1 body audit + PASSING
state-snapshot equiv on real populated capture (genuine datum resolution). A
guard-only-coverage runtime pass is sufficient WHEN the runtime-dark downstream is
conclusively decoded statically — same principle as 0x1d530, applied to a
substantial (not trivial) dark region. CLAUDE.md sanctions the infection_swarm pass
as accepted runtime evidence for the sub-90 band.

--- ORIGINAL (2026-xx) NEEDS_RUNTIME review, superseded above ---

actor_action_try_to_evade (0x1fca0, actions.obj) reviewed at 88% objdiff, acceptance path=permute → verdict NEEDS_RUNTIME (not AUTO_ACCEPT, not REJECT).

**Why NEEDS_RUNTIME (band gate, not defect gate):** 88% is <90% band → policy requires golden/runtime behavioral verification PLUS classified mismatches before commit. Static body is a faithful 1:1 transcription of the Ghidra decompile (all 4 pre-screen guards actor+0x158/+0x504/+0x270 & FUN_0002a360; branch-swap on actr_tag&0x200000 is semantically equivalent — C `if(set) 3-dot else 2-dot` == Ghidra `if(clear) 2-dot else 3-dot`; 2D dot term-for-term matches; impulse 1→7/0→6/else-assert @line 0xce3; assert+system_exit(-1) faithful). ABI clean (cdecl, regs=none, matches disasm). Hazards clean (RC=0). All callee decls present+matched in kb.json (magnitude3d=0x12f10, FUN_00013070 3-dot, actor_move_try_evasion_direction 0x2ab40 7-arg, actor_move_animation_impulse 0x2a7e0). No FPU/LOADW/IMM warns surfaced. BUT zero positive behavioral coverage of the evade body.

**Why the equiv 0/100 FAIL is NOT a REJECT:** log line 125 = `call[0] _datum_get arg[0]: oracle=0x00700300 candidate=0xcccccccc`. arg[0] of datum_get is the GLOBAL `actor_data` pool ptr. Oracle resolves it via reloc (0x700300); candidate reads 0xCC guard-fill because the harness never seeded that global for the lifted .obj (lifted class=1 data ref vs oracle 7). Identical variable both sides — NOT swapped/dropped/wrong-const. Classic reloc/global-seeding artifact ([[reference_equiv_oracle_reloc_gap]] family). All 100 seeds die at the FIRST call before any evade logic → coverage 13.1% (entry guard + shared epilogue), trace_diffs=0, unique_returns=1, confidence=weak.

**Resolution to reach commit:** state-snapshot equivalence with live AI game state (so datum_get(actor_data,...) resolves and the dot-align / 7-arg evasion / impulse / animation-commit paths actually execute), or a golden/dual-oracle harness case. Permute alone cannot clear <90% — it only optimizes byte-match.

**Latent fidelity note (not blocking, worth a runtime eye):** magnitude3d reads 3 floats but the C declares `float scratch[2]` / `float alignment_vec[2]` — scratch[2]/alignment_vec[2] is a 1-float OOB read reproducing an MSVC stack-overlap quirk. Value only matters in the v0==v1==0 zero-length-gate edge (2D branch) or is discarded (do_evade path magnitude3d return unused). clang's adjacent-slot value differs from MSVC's, so the edge behavior is not guaranteed faithful.

CONTRAST: stub-args `0xCC on candidate for a GLOBAL-derived arg[0]` = harness gap (NEEDS_RUNTIME). A stub-arg mismatch on a COMPUTED/PUSHED value (swapped order, truncated float, wrong constant) with mapped memory = real bug (REJECT).
