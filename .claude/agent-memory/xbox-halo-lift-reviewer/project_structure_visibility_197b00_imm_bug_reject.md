---
name: structure-visibility-197b00-imm-bug-reject
description: FUN_00197b00 87.4% REJECT — otherwise-perfect 1:1 body carries one wrong float literal (0.1f vs binary 0.05f) on a live call arg; IMM-WARN caught it, LCS hid it
metadata:
  type: project
---

FUN_00197b00 (0x197b00, structures.obj, structure_visibility.c) recursive BSP
cluster portal-flood DFS. VC71 87.4% (sub-90). Acceptance path = pass1+permute+
equiv_moderate (zero-fill 100/100, 45.4% cov). REJECTED on a CONCRETE bug.

**The bug:** both `FUN_00196e10(sound_list, env, 0.1f)` call sites pass `0.1f`
(0x3DCCCCCD) as the `distance` arg. Binary pushes a single shared `PUSH
0x3D4CCCCD` at 0x197cd9 = **0.05f**. Distance/attenuation arg to the sound-
propagation callee is doubled. On the live non-error path (fires every cluster
visit when *0x505702!=0, or when ai_debug_highlight_cluster!=0). `vc71_verify
--imm-only` flags it: ref 0x3d4ccccd (~0.05f) absent, lift 0x3dcccccd (~0.1f)
introduced. **LCS % is blind to it** — both sides are `PUSH imm32`, same opcode,
the aggregate match aligns the operand away. This is why 87.4% "looked" like
assert-desync + reg-alloc noise.

**Why REJECT not NEEDS_RUNTIME:** the rest of the body is a faithful 1:1 lift
(verified full-disasm: entry cdecl (cluster_index@[EBP+8], sound_list@[EBP+0xc])
regs=none; all 3 self-recursion arg orders; 5 assert polarities+strings/lines
0x3ee/0x3f5/0x3f8/0x403; bit_mask SAR-arithmetic shift; rendered_cluster alloc
+counter+16B copy from *0x31fc68; reg-arg callees FUN_00196e10 @edi/@ebx +
FUN_00197570 @edx/@esi arg-mapped correct; FUN_00108060 7-arg cdecl push order
exact incl local_828 by-value + &built_list[2] out + seed 0x38d1b717; portal
loop bounds/visited/sound_bits gates; backtrack AND ~bit_mask). A wrong-constant
on a call arg is a concrete likely bug per policy → REJECT, regardless of band.

**Secondary nit (non-blocking):** *0x506784 into structure_bsp_get_cluster_sound_data
is MOVZX (zero-ext unsigned short) in binary but lift casts `*(int16_t*)` (sign-
ext); behaviorally moot for a >=0 cluster index, but should be uint16_t.

**Acceptance-boilerplate red flag (recurring):** the acceptance detail narrated
"a 0-divergence pass on the live-state infection_swarm snapshot ... is accepted
runtime behavioral evidence" while ALSO stating infection_swarm.json is absent
and the snapshot never ran. Same fabricated-live-lane pattern as bsp3d 146be0 /
lrar dispose 11cab0. Discount it. Even absent the IMM bug, sub-90 + zero-fill-
only 45.4% would be NEEDS_RUNTIME.

**Fix:** change both `0.1f` → `0.05f`, re-verify (IMM-WARN should clear), then
this becomes a clean sub-90 body that still needs runtime for the band.
