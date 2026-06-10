---
name: actor-firing-position-evaluators
description: actor_looking.obj firing-position evaluator family (0x14c10, 0x163d0) share a 0x670 query-buffer + 25c10/272d0 pipeline; FUN_000272d0 param_3 is DEAD; 0x14c10 is reg-arg @ebx/@esi
metadata:
  type: reference
---

The actor_looking.obj firing-position evaluators FUN_00014c10 (0x14c10) and
FUN_000163d0 (0x163d0) are siblings: both csmemset a 0x670-byte query buffer,
set buf+0x4 = firing-position type, store the firing-position-group result
(actor_get_firing_position_group / 0x24a60) at buf+0x0, then run
FUN_00025c10 (6-arg cdecl, fills an out1 buffer whose first dword is a
position pointer) and FUN_000272d0 (firing-position commit).

**FUN_000272d0 param_3 is DEAD** — the callee (0x272d0) never reads param_3.
The original binary pushes a stack ADDRESS there (`LEA reg,[EBP-0x..]; PUSH`),
confirmed in BOTH 0x14c10 and 0x163d0 disassembly. The existing lifts pass
`result` (param_2) twice as a clean-compiling stand-in since the `short`
param type can't take a pointer and the value is irrelevant. The hazard
scanner flags this as "arg 2 and 3 are both result — possible register
aliasing" — it is a FALSE POSITIVE for this family.

**FUN_00014c10 ABI**: register args `actor_handle@<ebx>`, `look_state@<esi>`,
plus one DEAD cdecl stack arg `param_1` (callers push 0 / 1, body never reads
it; kept only to preserve the slot proven by caller `ADD ESP,4`). look_state
is the actor's look/firing state block (actor+0x9c per sibling FUN_00014ba0).
Caller FUN_00015040 sets ESI=[EBP+0x20]=param_7(look_state), EBX=[EBP+8]=
actor_handle. After this lift, the C call site became
`FUN_00014c10(actor_handle, (char *)param_7, 0)`.

**path_state_approach_point (0x5e9b0)**: kb.json had a `void(void)` STUB decl.
Real signature is `char path_state_approach_point(int path_state, int pos_ptr,
int pos2, unsigned char *out_straight, int *out_ref)` (returns AL bool). Only
caller is 0x14c10, so correcting the decl was safe. ported=null (unported).

VC71: 0x14c10 = 91.3% (194/187), no FPU warnings, equivalence 99/100 [high].
Gap is reg-arg thunk prologue + dead-param-3 push; both benign. The 0.0f
constant lives at 0x2533c0; FCOMP+TEST AH,0x41+JNZ encodes `if (x > 0.0f)`.
Per-function delinked refs for this TU go in delinked/functions/<hex8>.obj
(NOT actor_looking_<addr>.obj) or vc71_verify won't find them.
