---
name: actions-0x1d530-sub90-statesnapshot-clear
description: FUN_0001d530 (actions.obj) 86.5% AUTO_ACCEPTED via PASSING state-snapshot equiv — first sub-90 actions lift cleared on runtime lane, not static
metadata:
  type: project
---

FUN_0001d530 @0x1d530 actions.obj, 86.5% VC71, AUTO_ACCEPTED. Distinct from my
prior sub-90 actions verdicts, which were both NEEDS_RUNTIME.

**What cleared it (the sub-90 runtime-lane bar):**
- Body is a complete 1:1 transcription of disasm: cdecl predicate, 1 reg arg
  (actor_handle @<eax> verified as PUSH EAX for datum_get 2nd arg @0x1d53a),
  2 stack args (param_1 char @EBP+8, actor_index int @EBP+0xc), 3 CALLs all
  resolving to correct kb targets — datum_get(0x119320), datum_absolute_index_to_index(0x119270),
  FUN_0003a7f0(0x3a7f0) — cdecl arg order verified, single ADD ESP,0x10 = 4 pushes.
- datum_get return DISCARDED (EAX overwritten @0x1d544) = validation-only, faithful
  (same handle_evasion precedent). Actor ptr = datum_absolute_index_to_index return,
  which the ORIGINAL binary ALSO dereferences as a pointer (MOV CX,[EAX+0x6e]) — so
  the `(char*)` cast on an int-returning decl is faithful to machine code, NOT a bug.
- All offsets verified: 0x6e (word,signed [2,4) gate), 0x6c (word mode set {7,5,8-if-p1==0,6-if-a4==0&&9c>0}),
  0xa4 (byte), 0x9c (word signed), 0x4 (word -> FUN_0003a7f0). Byte-return (AL only).
- NO FPU/LOADW/IMM warning on THIS fn (loadw hits were siblings d030/d6d0/dab0 with
  the base-fold 0x253fb0 false-positive). ABI audit clean, hazard scan clean.
- **Runtime key:** state-snapshot equiv (infection_swarm live capture, 7 regions),
  100/100 seeds 0 div, 0 mem-trace diffs, stub-arg diff 100 calls 0 mismatch, and the
  datum lookup GENUINELY SUCCEEDED (idx=1 salt=58219 matched real AI table @0x8027e6e0).
  A PASS where oracle(MSVC) and candidate(clang) agree on a real resolved-actor path =
  genuine behavioral evidence, not a harness artifact.

**Bar established:** sub-90 AUTO_ACCEPT needs (a) complete static 1:1 body audit AND
(b) a PASSING state-snapshot equiv on real populated capture. Contrast:
- [[project_actions_consider_grenade_88band_needsruntime]] 88.8% = NEEDS_RUNTIME because
  path was permute (NO real runtime run).
- [[project_evade_88band_equiv_globalseed_artifact]] 88% = NEEDS_RUNTIME because equiv
  FAILED 0/100 as an unseeded-global artifact (no positive runtime evidence).
The 86.5% here cleared where 88% evade did not — the discriminator is a PASSING equiv on
a real-memory snapshot, not the objdiff %. Coverage caveat (return-1 path possibly dark in
equiv under pinned arg_overrides) is covered by the conclusive static audit of that trivial
CMP AL,CL/MOV AL,1/JNZ branch.
