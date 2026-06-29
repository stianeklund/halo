---
name: real-math-fpu-equiv-activation-bar
description: What cleared the real_math.obj FPU-leaf equivalence-gated activation bar (vector_intersects_pill2d 0x10dcb0) — VC71 is a TU-wide structural cap (positive control), so equivalence + STATIC disasm proof of the seed-dark branch carries it; the extraout_EDX register-aliasing trap resolves clean because the cdecl callee preserves EDX
metadata:
  type: project
---

`vector_intersects_pill2d` (0x10dcb0, real_math.obj, 2D segment-vs-capsule)
AUTO_ACCEPTED for activation 2026-06-22. Pure cdecl, char return, calls one
ported cdecl callee FUN_0010cc90. Primary gate = unicorn_diff equivalence
(100/100, HIGH, 68.8% cov, mem-trace + stub-arg clean), NOT VC71 byte-match.

**Why VC71 is not the gate here (TU-wide structural cap, proven by positive control):**
- Independent VC71 re-measure: 31.0% (303/316 insns) on the standalone ref
  `delinked/functions/0010dcb0.obj` (symbol `FUN_0010dcb0`, candidate TU renamed
  it `vector_intersects_pill2d` — use `vc71_verify -f <renamed>` against the
  FUN_-named ref; name-based COMDAT lookup otherwise misses). Brief claimed ~49%
  via objcopy-bridge; the exact % varies by measurement path, both deeply sub-bar.
- POSITIVE CONTROL: the already-SHIPPING ported callee FUN_0010cc90 scores 64.7%
  (65/68) through the identical compare path. Low score is TU-wide, not a
  correctness signal. Mismatch class = x87 FPU scheduling/stack-ordering +
  commutative-add reordering (MSVC71 vs clang) → LCS desync at near-equal counts.

**The decisive register-aliasing trap (lift-decompiler-traps §1) and how it resolved CLEAN:**
- Decompile shows `param_2 = extraout_EDX` between the two FUN_0010cc90 calls, and
  the 2nd call's arg3 is `PUSH EDX` (0x10de09). Source passes original `line_dir`.
  Skim reads "EDX = line_dir ✓" — but EDX was reassigned post-call1 in the model.
- RESOLUTION (primary-source, not inference): FUN_0010cc90 disasm reads ALL its
  args from the stack ([EBP+8/c/10/14]) — pure cdecl, NO reg args — and never
  touches EDX anywhere in its body. EDX (loaded once at entry 0x10dcb6 = line_dir)
  is preserved across call1, so at call2 it still == line_dir. extraout_EDX is just
  Ghidra's conservative post-call model. Source is faithful.

**The coverage→branch mapping that the brief got BACKWARDS (real reviewer trap):**
- Random seeds drive `cross2d >> epsilon` → the NON-PARALLEL branch. The PARALLEL
  branch (cross≈0) is the LARGER one by bytes and only partially seed-covered.
  Concolic Phase 2 did NOT run (68.8% > 60% threshold), so nothing forced cross≈0.
- So equivalence is SILENT on the parallel-branch clamp sub-paths and the
  s_oob&&t_oob double-call path. Do NOT credit those to the 100/100. They are
  carried by STATIC proof: every FPU operand order (delta subtraction, 2D cross
  dir, d0_d1/d0_sq/d1_sq, s/t projection signs+negation, clamp comparison
  directions, b-a diff order) traced line-by-line against the 0x10dcb0 disasm,
  plus guard polarity (return 0 only when both point-seg checks miss) confirmed.
- State evidence as a UNION: non-parallel = equivalence + static; parallel =
  static operand-order + decompile-structure (partial equivalence).

**Policy discharge:** VC71 deeply <90% → policy says NEEDS_RUNTIME UNLESS
golden/runtime behavioral verification + classified mismatches. Name unicorn_diff
explicitly as that behavioral verification (harness proven sensitive — it caught
the upper-EAX garbage that forced the int→char return fix). With mismatch classified
(x87 cap) → AUTO_ACCEPT, not NEEDS_RUNTIME.

**Gate B (cluster isolation):** callee FUN_0010cc90 = geometry; both real callers
(0x10ed70 AABB-edge-vs-pill, 0x10eeb0 triangle-edge-vs-pill — NOT the "7 callers"
the brief claimed) consume return strictly as `if (cVar1 != 0) return 1` (AL-only,
upper EAX dead). Zero edge into GC/lifecycle cluster.

How to apply: for real_math.obj (and similar x87-heavy geometry) FPU-leaf
activations, do NOT reject on low VC71 — confirm the TU-wide cap via a shipping
positive control. Require: (1) equivalence 100/N HIGH >60% non-vacuous; (2) STATIC
disasm proof of every call-site arg (esp. any reg-passed arg across a call — verify
the callee's reg preservation, don't trust extraout_*); (3) STATIC operand-order +
branch-direction trace of any branch the seeds likely missed (check whether concolic
ran; if cov>60% it did NOT, so the rarer branch is dark); (4) Gate B cluster-edge +
caller boolean-consumption check. Relates to [[objects-cdecl-leaf-activation-bar]],
[[compare_obj-diff-polarity]], [[assert-macro-lcs-desync]].
