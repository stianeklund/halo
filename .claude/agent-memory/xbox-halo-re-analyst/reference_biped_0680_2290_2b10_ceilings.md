---
name: biped-0680-2290-2b10-ceilings
description: 0x1a0680 (83.2%, NOP-inflated to 61.5%), 0x1a2290 (84.4%), 0x1a2b10 (84.0%) are all at structural VC71 ceilings; three source experiments proven not to help — do not re-churn
metadata:
  type: reference
---

Measured 2026-06-08 (worktree scalable-coalescing-lobster, HEAD 657a5351).
These three dormant bipeds.c functions are at byte-match ceilings; I tried and
reverted three evidence-backed mechanical experiments that each produced
BYTE-IDENTICAL codegen (zero gain). Net source change for this session: none.

**0x1a0680 `char FUN_001a0680(int unit_handle)` (cdecl) — TRUE 83.2% (82/91).**
- vc71_verify reports 61.5% (82/152) because it picks the whole-TU `bipeds.obj`
  ref whose denominator includes ~61 trailing NOP/label bytes to the next fn
  (biped_place 0x1a07c0). This is the [[whole-tu-nop-inflation]] artifact.
- TRUE per-fn ref: Ghidra Body `001a0680-001a0773` → export range `1a0680-1a0774`:
  `mcp__ghidra-live__export_delinked_object export_path=...\001a0680.obj
   selection_mode=range range=1a0680-1a0774 run_relocation_synthesizer=true`
  then `python3 tools/verify/compare_obj.py build/vc71/bipeds.obj
   delinked/functions/001a0680.obj -f FUN_001a0680` → **83.2% (82/91)**.
- 9 residual insns, ALL integer (no FPU in this fn): EBX↔EDI callee-saved alloc
  for unit_handle/antr_tag; clang register-counter+jmp-to-test loop form vs the
  original's memory-counter (`-0x8(%ebp)`) fall-through; `xorb %al,%al`
  constant-return vs original `movb -0x1(%ebp),%al` reload (optimizer proved
  result==0 on the fall-through). Capped.
- Tried (no gain, reverted): per-iteration re-read `while(i<*(int*)(antr+0x68))`
  — clang hoists invariant; `for(i=0;i<count;i++)` — identical to do-while.

**0x1a2290 `char FUN_001a2290(int unit_handle@<edi>)` — 84.4% (143/139), ref accurate (NOT NOP-inflated; candidate>ref).**
- Residual: `[5] movl 0x8(%ebp),%edi` = the @<edi> keystone cdecl-vs-register
  prologue artifact (EDI is not an arg reg in ANY MSVC convention, so vc71's
  `.vc71_regcall_` transform cannot express it — same immutable cap as
  [[reg-arg-keystone-vc71-ceiling]], here ON the keystone itself); boolean
  `success` tracked in BL/CL register vs `[EBP-1]` stack byte; `[52]` velocity
  load-form (integer-copy+float-reload vs direct flds) scheduling. FPU section
  (the `<` FCOM/`TEST AH,0x5`/JP launch-dot block) already matches.
- Tried (no gain, reverted): combined `if(g1||g2) return success;` with
  `success=0` up front — clang emitted byte-identical code to the two separate
  `return 0;` blocks; the early-exit epilogue did not merge into the shared tail.
- Equivalence already proved product-equivalent (5 exact writes incl velocity);
  activation case is equivalence, not byte-match.

**0x1a2b10 `void FUN_001a2b10(int unit_handle@<edi>)` — 84.0% (52/48), ref accurate.**
- Original is FRAMELESS (`PUSH ESI; PUSH 1; PUSH EDI; CALL` — no EBP frame) and
  takes EDI directly. Our cdecl C compiles a full EBP frame + `movl 0x8(%ebp),%edi`
  → `[1-4]` mismatch (@<edi> + frameless). Plus `[41]/[49]`: the two
  `FUN_001a0f10(unit,2,idx)` calls route idx via `index@<bx>` (correctly declared
  in kb.json), but vc71 does NOT apply the thunk so it pushes idx as a stack arg
  (`pushl $0`) vs the original `XOR EBX,EBX`/`MOV EBX,1` — caller-of-reg-arg-
  keystone cap. The whole sum-of-squares FPU block (`TEST AH,0x41` = `>` family)
  already matches. No mechanical fix exists.

**Decision recorded:** leave all three `ported=false`; do not churn the source.
Byte-match cannot clear the mid-80s here — every residual is a vc71 ABI-modeling
artifact (register args / frameless) or optimizer discretion (reg-alloc, loop
lowering, constant-return). The activation lane for these is equivalence/runtime,
not VC71. The faithful numbers are 83.2 / 84.4 / 84.0.
