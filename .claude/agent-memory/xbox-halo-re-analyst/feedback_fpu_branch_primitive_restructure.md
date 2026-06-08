---
name: fpu-branch-primitive-restructure
description: Match the original's FCOMP comparison primitives and early-exit shape (not algebraically-equivalent nesting) to recover several percent on FPU branch-heavy lifts
metadata:
  type: feedback
---

When an FPU function branches on float comparisons, write the C conditions in
the original's *primitive* form rather than an algebraically-equivalent rewrite.
Moved `FUN_001a0e00` from 77.0% to 82.3% VC71 (79/79 insns) with no FPU-stack
changes (bipeds.obj, 2026-06-06).

**Why:** clang lowers `<`, `<=`, `>=` to different FCOMP/FNSTSW/TEST/Jcc
encodings. The original here used `<` everywhere (`FCOMP; TEST AH,0x5; JP/JNP`)
and inverted jumps to early-exit. Rewriting the draft's `if (fVar1 <= threshold)
{ nested }` as `if (threshold < fVar1) return;` + flattened branches matched the
jump encodings and the two-exit structure.

**How to apply:**
- Read the disasm's `TEST AH,<mask>` + `Jcc`: `TEST AH,0x5` (C0|C2) is the `<`
  family; `TEST AH,0x41` (C3|C0) is the `<=`/`>` family. Write the C operator
  that clang lowers to the *same* mask. (See [[feedback_fpu_comparison_direction]].)
- Mirror early-`return` exits instead of wrapping the body in a positive `if`.
  Original `if (range <= 0) {pop; pop; exit}` → C `if (range <= 0.0f) return;`.
- This is orthogonal to the FPU-stack-retention ceiling
  ([[x87-st0-chaining-ceiling]]) — do the cheap branch-shape win first, then
  assess whether the remaining gap is pure FPU register allocation.
