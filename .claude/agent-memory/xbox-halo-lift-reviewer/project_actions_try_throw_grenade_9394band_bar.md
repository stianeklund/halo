---
name: actions-try-throw-grenade-93-94-band-bar
description: actor_action_try_to_throw_grenade (0x1fa60) AUTO_ACCEPTED at 93.4% VC71 — third confirming instance of the 90-95% band static clear in actions.obj; resolves magnitude3d(0x12f10) as genuinely 2D (delta[2] is NOT an OOB read)
metadata:
  type: project
---

actor_action_try_to_throw_grenade (0x1fa60, ai/actions.c) AUTO_ACCEPTED at 93.4%
VC71 (90 cand / 92 ref insns; no FPU/LOADW/IMM warn on this fn). cdecl (ABI
regs=none), hazard --changed-only exit 0, delinked ref present (FUN_0001fa60 in
delinked/actions.obj @0x41c0). Third confirming instance of the 90-95% band
cleared STATICALLY, alongside [[project_actions_combat_targeting_9036band_bar]]
(0x1dea0 93.6% and 0x1e700 94.4%). Full 0x1fa60 disasm decoded 1:1 vs source.

**magnitude3d(0x12f10) is a MISNOMER — it is genuinely 2D.** The kb decl names it
`float magnitude3d(float *v)` but the body computes `SQRT(v[1]^2 + v[0]^2)` and
normalizes only `v[0]`/`v[1]` in place (returns FLOAT_002533c0=0.0 below eps
_DAT_002533d0). So the source's `float delta[2]` passed to magnitude3d(delta) is
EXACTLY sized — NO out-of-bounds read, NOT an MSVC stack-overlap hazard. This is a
DIFFERENT function/context from the try_to_evade note in
[[project_evade_88band_equiv_globalseed_artifact]] which flagged "magnitude3d reads
scratch[2] OOB" — do NOT re-flag 0x12f10 as OOB; it reads 2 floats by construction.

**All 6 CALLs cdecl (reg_args=null), arg order/count verified via PUSH order + ADD ESP:**
- datum_get(actor_data@0x6325a4, handle) 0x119320; ADD ESP shared 0x14 (5 dwords)
- object_get_and_verify_type(*(actor+0x18), 3) 0x13d680 (PUSH 3; PUSH handle)
- unit_is_busy(*(actor+0x18)) 0x1a9ad0 (bool, 1 arg)
- actor_action_test_grenade(handle) 0x1d180 (char, 1 arg, ADD ESP,4)
- magnitude3d(delta) 0x12f10 (float*, ADD ESP,4)
- datum_get(*0x5ab270, actor+0x34) 0x119320 tail (ADD ESP,8) + game_time_get() 0xb5aa0 (int)

**Three FPU compare polarities all verified correct (no FPU-WARN):**
- cooldown: FLD object+0x9c; FCOMP [0x2533c0=0.0]; FNSTSW; TEST AH,0x41; JZ→ret
  = enter body iff object+0x9c <= 0.0 (source `<= *(float*)0x2533c0`). ✓
- magnitude: FCOMP mag vs [0x2533c0=0.0]; TEST AH,0x41; JNZ→ret
  = enter iff mag > 0.0 (source `0x2533c0 < magnitude3d(delta)`). ✓
- dot arc: FADDP dot; FCOMP [0x2533dc]; TEST AH,0x5; JNP→ret
  = JNP taken (return) iff dot < const; enter iff dot >= const (source
  `*(float*)0x2533dc <= dot`). ✓ `>=` idiom via C0/parity.

**Constants read as globals, not literals (correct):** 0x2533c0 = 0x00000000 =
0.0f; 0x2533dc = 0x3f5db3d7 = 0.866025388 = cos(30 deg). Source uses
`*(float*)0x2533dc` so it materializes the EXACT bytes 0x3f5db3d7 — this is the
CORRECT way and avoids the [[project_actor_aim_grenade_cos_literal_fixed]] bug
(that lift wrote rounded literal 0.857651889f ≠ 0x3f5db3d7). No IMM-WARN, matches.

**Memory offsets all 1:1:** 0x18,0x9c,0x6a0(pending, cleared on 2 paths),0x6a8/0x6ac
(grenade target),0x12c/0x130(self pos),0x174/0x178(facing),0x45c(commit=1),
0x34(encounter handle, NONE=-1 guarded),0x5c(encounter timestamp). Writes:
actor+0x6a0=0, actor+0x45c=1, encounter+0x5c=game_time_get(). All present.

**Decl refinements correct & necessary:** try_to_throw_grenade void(int,int) →
char(int,char) — `char flag` faithful (disasm MOV AL,[EBP+0xc]; only low byte
read); char return in AL. test_grenade void(void) → char(int) — matches its
1-PUSH/AL-read call site (same fix as [[project_actions_consider_grenade_88band_needsruntime]]).

**2-insn ref>cand delta = benign compiler shape** (epilogue-dedup / reg-alloc via
XOR BL,BL default-return + MOV AL,BL). No census detector fired.

**Caller-independence precedent holds:** this fn is CALLED BY consider_grenade
(0x1fb80, a NEEDS_RUNTIME sibling at 88.8%) and CALLS test_grenade — acceptance
rests on THIS body's fidelity + correct call-site args, both verified. Being in the
90-95% band (not sub-90) is what makes it static-clearable where consider_grenade
was not. Verdict: AUTO_ACCEPT.
