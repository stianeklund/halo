---
name: actions-obj-stale-delinked-region
description: delinked/actions.obj is STALE past ~0x1dab0 (wrong fn boundaries); export per-function refs for late-region actions.c lifts; 0x1ccc0 decl was void(void) but is char(int,int)
metadata:
  type: reference
---

2026-07-04 lifting actor_action_handle_active_cover_seeking (0x1e700, actions.obj).

**Stale whole-TU delinked reference.** `delinked/actions.obj` has CORRUPTED
function boundaries in the late region (>= ~0x1dab0): `vc71_verify src/halo/ai/actions.c`
shows `FAIL FUN_0001dab0: 16.4% (247/22 insns)` (ref body only 22 insns vs 247 real)
and SKIPS FUN_0001e700 entirely (named candidate can't pair against the stale ref).
Fix = export a fresh per-function ref and verify with `-f`:
`export_delinked_object range 1e700-1e8a0 -> delinked/functions/0001e700.obj`,
then `vc71_verify ... -f actor_action_handle_active_cover_seeking --no-cache`.
Any actions.c lift above ~0x1dab0 needs this per-function export; don't trust the
whole-TU actions.obj score for that region. (See also [[vc71_scores_stale_and_ref_recreation]].)

**Stale callee decl trap.** kb.json 0x1ccc0 was `void actor_action_allow_cover_seeking(void)`
but disasm (`PUSH EBX; PUSH 0; CALL 0x1ccc0; ADD ESP,8; TEST AL,AL`) proves it is
`char actor_action_allow_cover_seeking(int actor_handle, int param_2)` — 2 cdecl args,
char return. Was unported so had zero source callers (safe to fix). Corrected in kb.

**Lift facts (0x1e700).** Real sig `char (int actor_handle, char param2, int param3)`
(KB had `void(int,int,int)`). param2 = byte bool @EBP+0xc, param3 = dword fwd to
FUN_0001d3c0 arg4. Return AL = local byte @EBP+0xb. Per-actor state-trace record
`*(int*)0x331f58 + (handle&0xffff)*0x657c`, fields +0xb8/+0xba/+0xbc/+0xc0 are debug
telemetry only (no control-flow effect). FPU: `FLD actor+0x1bc; FCOMP tag+0x2dc;
TEST AH,0x41; JP` = `(actor+0x1bc <= tag+0x2dc)`. VC71 lift lessons: elapsed = full
32-bit `game_time_get() - *(int*)(actor+0x26c)` truncated only at the +0xbc store
(a two-step `short` form emits a narrow `subw mem,ax` LOADW-WARN); `>= 2` not `1 < x`
for the +0x6e signed check to match `CMP ...,2; JL`. Landed 94.4% VC71.
