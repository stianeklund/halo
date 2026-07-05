---
name: actions-handle-evasion-9698band-bar
description: actor_action_handle_evasion 0x20670 96.8% (249/245) AUTO_ACCEPTED — 95-98 band cleared by full 1:1 disasm audit; 4-insn delta = epilogue-dedup shape; calls two NEEDS_RUNTIME siblings but call sites correct
metadata:
  type: project
---

actor_action_handle_evasion (0x20670, actions.obj) VC71 96.8% (249 ref / 245 cand), path=pass1 → **AUTO_ACCEPT**.

**Why AUTO_ACCEPT (95-98 band = prove all mismatches are harmless compiler shape):** Whole body decoded 1:1 vs the 0x20670 disassembly (0x20670-0x20984, ~249 insns). Every element accounted for:
- Prologue: datum_get(actor_data@0x6325a4, handle) → ESI; tag_get(0x61637476 'actv', actor+0x5c) → actv_tag@[EBP-0x10]; tag_get(0x61637472 'actr', actor+0x58) → actr_tag@[EBP-0x8]; game_time_get() → now@[EBP-0xc]. Push order reverse-verified.
- Both datum_get lookups in block1 (actor+0x3ac) and block5 (actor+0x270) use **prop_data@0x5ab23c** — matches disasm (both load [0x5ab23c]); comment correct.
- 3 FPU compares all correct polarity: `actr_tag+0x318 > *0x2533c0` (FCOMP/TEST AH,0x41/JNZ), `radius > *0x253f38` (FCOM, ST0 kept), `actor+0x354 <= radius` early-return (FCOMP/TEST AH,0x5/JP `<`-idiom), and block6 `random_math_real(...) < actr_tag+0x318` (same JP idiom). See [[feedback_fpu_comparison_direction]].
- 12 call sites ALL resolve to the exact C names, ALL cdecl (reg=none), arity/types match: datum_get, tag_get, game_time_get, allow_cover_seeking(h,1)/(h,0), try_to_seek_cover(h,0,1), FUN_0001d3c0(h,5,actor+0x3ac,0), consider_grenade(h), get_global_random_seed_address→random_math_real(seed), actor_target_unit_index(h), FUN_00046f10(0x18,actor+0x18,tgt,-1,-1,-1,0) [7-arg, interleaved-eval push verified], try_to_evade(h).
- _ftol2 at tail (CALL 0x1d9068) written as `(short)(int)(actr_tag+0x31c * *0x253394)` — NOT called as a fn. Correct per intrinsics table.
- random_math_real float return: block3 discarded via FSTP ST0 (C plain call stmt), block6 used in FCOMP. Both correct.
- `*(int*)(actor+0x354)=0` writes (blocks 4/6/7) vs `*(float*)` read (block2): int-0 store == float-0.0 store (same 0x00000000 dword). Benign.
- Memory offsets all 1:1 (actor +0x5c/0x58/0x3a8/0x158/0x3ac/0x36c/0x374/0x378/0x1ca/0x354/0x504/0x358/0x270/0x6c/0xa0/0x18/0x368/0x3bb; prop +0xa4/0x38/0x122/0x121; actv+0x184; actr+0x310/0x314/0x318/0x31c + flags 0x400000/0x20).

**The 4-insn delta (ref 249 > cand 245) = epilogue-dedup shape:** original keeps a SEPARATE leave/ret for the FUN_0001d3c0 return-1 path (0x20766-0x20773: POP EDI; MOV [EBP-1],1; MOV AL; POP ESI/EBX; leave; RET), while clang merges it into the shared epilogue at 0x2097b. Textbook harmless instruction-scheduling/epilogue-dedup. No FPU/LOADW/IMM warn on target (LOADW warns were on d030/d6d0/dab0, not 20670). Hazard scan clean for actions.c. ABI cdecl matches disasm, no @<reg> callees.

**Decl changes correct:** actor_action_handle_evasion void→`char(int)` +ported=true (returns AL). actor_action_try_to_evade void(void)→`char(int actor_handle)` — this parent CALLs it with 1 stack arg + tests AL, confirming the signature fix.

**KEY: parent cleared statically even though it calls two siblings I flagged NEEDS_RUNTIME** ([[project_actions_consider_grenade_88band_needsruntime]] 0x1fb80, [[project_evade_88band_equiv_globalseed_artifact]] 0x1fca0). A caller's acceptance depends on (a) its OWN body fidelity and (b) its call sites passing correct args — both verified here. Callee internal-compute confidence is orthogonal: if callees run ported they're their own review's gate; if ported=false they run original code (correct). Do NOT block a caller because a callee is weakly verified, as long as the call site is right.

CONTRAST: [[project_actions_combat_targeting_9036band_bar]] (93.6, 90-95 band, small cdecl body) and the sub-90 siblings (NEEDS_RUNTIME via band/coverage gate). This is the 95-98 lane: prove-harmless-shape, no runtime needed when body is 1:1.
