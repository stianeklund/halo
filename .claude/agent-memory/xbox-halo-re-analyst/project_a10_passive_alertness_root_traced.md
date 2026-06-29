---
name: a10-passive-alertness-root-traced
description: a10 grunt passive/no-advance root = unported perception/sensory input divergence (prop+0x24 stuck orphan-4/5), NOT a static mis-lift; full ported-chain audit all faithful
metadata:
  type: project
---

# a10 grunt passive-engage / won't-advance: full perception-chain audit (2026-06-23)

**Symptom:** patched (clang) a10 grunts shoot but won't advance; alertness `actor+0x6e` crawls 0->1 vs unpatched 7/3. Discriminator confirmed live.

**Root (traced, evidence-backed):** the near-player grunt's **primary tracked prop** (`actor+0x270`) has perception state `prop+0x24` stuck at **orphan (4/5)** in patched vs **active-threat 3** in unpatched. chain_diff proved patched leaves the player as an unmerged orphan-5 focus + separate state-0 prop, where unpatched merges to ONE active-3. Everything downstream (0x268 threshold index, 0x6e recompute) follows from `prop+0x24`.

**Causal chain (ALL unported = identical code both builds):** prop+0x24 -> 0x300b0 switch -> actor+0x268 -> 0x302b0 recompute `6e = max(0x72, 0x74, table[0x255f18+0x268])`. Promote-to-3 = 0x33330 (`actor_perception_become_acknowledged`), demote-to-5 = 0x32c10 (`actor_perception_abandoned_search`), orphan-test 0x32940, dispatcher 0x355f0, distance/position sense 0x31df0/0x31a90 — every one UNPORTED.

**Ported functions on the chain — ALL AUDITED FAITHFUL vs pristine:**
- FUN_0002f380 (engagement-level reader) — faithful
- FUN_000369c0 (stimulus inject +0x34a, `<`/`==` max) — faithful
- actor_handle_unit_effect 0x3d430 (the unit_effect=3 promote driver; switch 0/1/2/3; gate `prop+0x66<=di`; case-3 -> 0x33330) — faithful. `0x3ca40`=actor_set_dormant (verified, not a wrong-callee).
- actor_action_handle_exit_pursuit 0x1f9a0 (the demote-to-orphan ported caller; states 5/7/8; case8 -> 0x32c10) — faithful. Helper map: tried_to_uncover=0x32b50, tried_to_search=0x32bb0, abandoned_search=0x32c10.
- FUN_00055110 (ai_profile.obj, the unit_effect=3 broadcast; only caller passing 3) — faithful; key callee 0x64b40 (props.obj) UNPORTED.
- state-4 friendly-orphan trio: FUN_00036c50 / actor_derive_target_information 0x3b3c0 / FUN_0003c1c0 -> `actor_perception_create_orphan_from_friend` 0x34970 — all faithful; this is friendly-intel orphan creation, a SEPARATE mechanism, not the player-tracking demote.
- FUN_00024cf0 firing-position scorer — FAITHFUL. Subagent claimed a branch-swap bug at actor_looking.c:5990-6010 — WRONG (JP-direction misread). Real pristine math (constants 0x2533c0=0.0, 0x2533dc=0.866, 0x254df4=-15, 0x254df0=111.96, 0x254cc0=15): split at cos=0.0 (NOT 0.866); cos<0 -> 15+15cos (+assert); cos>=0: cos>0.866 -> (cos-0.866)*111.96+15 (+assert), else flat 15 (no assert). Lift matches exactly incl assert placement. FUN_00014c10 also faithful (query driver, reg-args @ebx/@esi + dead param_3).

**Conclusion:** No static mis-lift in any ported AI/perception/firing function. Same code + divergent output => divergent UPSTREAM INPUT. Since the sensory pipeline (distance 0x31df0, position 0x31a90) is itself unported, the bad input originates even further upstream — spawn/BSP/object-state or per-tick scheduling feeding the perception tick. Corroborated by task's stated ai_profile-OFF-still-passive result (CAVEAT: confirm that toggle was verified live per verify_toggles_live.py before fully leaning on it). Matches prior memory `ai_profile.obj faithful audit` + `biped+0x430 clobber` ("upstream spawn BSP-state").

**Tooling that worked:** existing a10 dumps in artifacts/ai_regression/ (actors_/props_ {unpatched,t0,t1,t2,f1,...}.bin pairs, ELEM actor=0x724 prop=0x138). /tmp/scan_disp.py (brute capstone decode for [reg+disp] writers — NOTE base-pool trap: disp-only match can't tell actor_data 0x6325a4 from prop_data 0x5ab23c from 0x44-descriptor buffers; always trace base to its datum_get). /tmp/probe270.py + chain_diff follows actor+0x270 focus prop correctly (prop_fields.py used nearest-by-distance = WRONG selector).
