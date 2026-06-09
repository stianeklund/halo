---
name: x87-st0-chaining-ceiling
description: x87 functions that consume a callee's ST0 return directly (no store/reload) or keep temps on the FPU stack across a branch hit a permanent VC71 ceiling unreachable from clean C
metadata:
  type: feedback
---

When the original keeps a value live on the x87 register stack across a C-level
boundary, clang/VC71 cannot reproduce it from clean C source — a permanent
structural ceiling (~73-82%), not a source bug. Two confirmed sub-patterns
(bipeds.obj escalation, 2026-06-06):

1. **Consuming a callee's ST0 return directly.** `FUN_001a2160`: original does
   `CALL normalize3d` (returns angle in ST0) → `FLD ST0; FCOS; FSTP cos; FSIN;
   FSTP sin` — zero angle store/reload. C must write `angle = normalize3d(...)`
   which forces FSTP-to-slot + FLD-back. An `x87_fsincos` helper taking `&cos,
   &sin` is WORSE (72.4% < 73.4% two-call) because the pointer outputs spill.
   Inlining the asm to write named locals hits 74.3% but violates the
   "No Inline ASM in C" rule and only buys +0.9% on a ~74% ceiling — not worth it.
   Keep the plain `x87_fcos(angle); x87_fsin(angle)` two-call form (73.4%).

   **HARD SAFETY RULE (user correction, 2026-06-06):** the hand-rolled sincos
   asm is not merely a rule violation — it is a runtime-crashing bug. The block
   `__asm__("fld %%st(0); fcos; fstps %0; fsin; fstps %1" : "=m"(c),"=m"(s) : "t"(angle))`
   uses an INPUT-ONLY `"t"(angle)` (st0) operand with no matching `"=t"`/`"+t"`
   output. clang assumes such an operand is LEFT ON the x87 stack after the asm,
   but the body net-pops one slot (fld +1, two fstps −2 = −1 vs clang's 0 model)
   → x87 stack-depth drift → NaN/Inf once the 8-deep stack fills. VC71 scores it
   74.3% because it compares opcodes, not FP-stack balance — the score HIDES the
   crash. The ONLY safe x87 inline-asm shape in this repo is the balanced in-place
   form `"=t"(out) : "0"(in)` (1 in, 1 out, net 0) — exactly what every helper in
   src/x87_math.h uses. NEVER hand-roll new x87 asm; always route cos/sin/fmod/tan
   through the vetted x87_math.h helpers. This runs every frame on biped rotation.

2. **Keeping branch results on the FPU stack across a merge.** `FUN_001a0e00`:
   both branches push offset/range/base onto ST0/ST1/ST2 and the merge label
   does `FDIVRP`/`FCOM` on them. C forces every temp through a memory local.

3. **Fused fall-distance compare tail.** `FUN_001a1e70` (cdecl, TRUE per-fn
   86.9% / 96-95 insns via `delinked/functions/FUN_001a1e70.obj` — NOT
   NOP-inflated, the 86.9% is genuine). 2026-06-08: the residual diff mass is
   DOMINATED by the FPU fall-distance tail 0x1f63-0x1f8f, NOT the keystone
   FUN_001a1a10 reg-setup as hypothesized. Original keeps clearance/threshold
   temps on ST0..ST3 across `FADD ST0,ST0; FLD ST2; FMUL ST3; FADDP; FLD ST1;
   FMUL ST2; FCOMPP` (the `c^2 <= clearance^2 + 2*fall_term` test) and branches
   on `JP`/`JNE` (parity-ordered FCOMPP) where clang emits `je`. The keystone
   call setup (`MOV EAX,[0x31fc50]`→`movl 0x0,%eax` reloc-stripped, push reorder,
   `movl %edi,%ecx`) is only ~3-4 of the unmatched insns. Both regions are
   structural ceilings; keystone reg-arg cap is the SMALLER contributor here.
   §10/§16 audit PASS: keystone call args in correct order (scale=6.0f@push,
   out_point=&probe_hit, out_vec=0, direction@eax=*(float**)0x31fc50,
   unit@edi=unit_handle); fn is void and original RET leaves nothing meaningful
   in EAX. Caller FUN_001a6350 — verify its ported status before activation.

**Why:** The x87 register stack is invisible to the C type system; there is no
portable way to say "leave 3 values on the FPU stack across a branch" or "fuse a
function's ST0 return into the next FPU op."

**How to apply:** When a target's residual diff is dominated by `FLD/FSTP`
store-reload pairs around `FCOS/FSIN/FDIV` or `FCOM %st(N)` vs `fcoms mem`,
it's this ceiling — document it and stop, don't churn. BUT first try the cheap
control-flow win (see [[fpu-branch-primitive-restructure]]): matching the
original's `<` comparison primitives + early-exit shape moved FUN_001a0e00 from
77.0% to 82.3% with zero FPU-stack changes.
