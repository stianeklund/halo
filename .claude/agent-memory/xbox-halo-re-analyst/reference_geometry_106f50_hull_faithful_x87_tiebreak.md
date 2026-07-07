---
name: geometry-106f50-hull-faithful-x87-tiebreak
description: FUN_00106f50 (geometry.obj hull/simplex builder) is behaviorally FAITHFUL; its equivalence "return 0 vs 1" is a compound harness artifact (stubbed plane-builder + x87 argmax tie-break on a symmetric-cube snapshot), not a logic bug
metadata:
  type: project
---

FUN_00106f50 @ 0x106f50 (geometry.c, src/halo/math/geometry.c) — initial-simplex/hull
seed builder. Verified instruction-by-instruction vs disasm: every guard polarity,
offset, sign, operand order, the swap (`0<best_dist`), and all 5 FUN_001037b0
(plane-from-3-points, 0x1037b0) call arg orders MATCH. No cross-product swap,
FSUB-direction, comparison-polarity, or offset bug. VC71 79.9%.

**The reported "systematic return-0 vs oracle return-1" is NOT a real bug — it is a
compound equivalence-harness artifact:**

1. **Default stubs (the repro command) are invalid for this function.** FUN_001037b0
   is the plane builder that WRITES the caller's `plane[4]` output buffer, which the
   plane-pass loop then reads. unicorn_diff stubs it (generic RET, writes nothing).
   The ORACLE "succeeds" only by accident: MSVC reused the pass-3 scratch stack slot
   [EBP-0x48] (perp_x) as `plane[1]`, so the stubbed plane holds non-zero garbage →
   best_dist≠0 → emit → return 1. Our lift's `float plane[4]` is a distinct array =
   zero on unicorn's fresh stack → best_dist=0 → plane_index=-1 → return 0. No source
   change can fix this; you MUST use `--real-callees` (loads the real FUN_001037b0 for
   both sides). Class: "stubbed callee writes the consumed output buffer."

2. **With --real-callees, the residual divergence is an x87 argmax tie-break** on the
   `fun106f50_hull.json` snapshot (a symmetric 10-unit cube). Six points are
   equidistant from the p0-p1 line to 5-6 significant figures; the pass-3 argmax is
   decided by whether perp_y/(dz-proj_z) stay in 80-bit x87 registers (VC71/oracle →
   index that agrees with pure float32) vs clang rounding to float32 (picks a
   different index). This cascades to a different plane_index and swap. Attempting to
   force the 80-bit retention (inline perp_y + disasm sum order `(dz-proj_z)²+perp_y²+
   perp_x²`) did NOT flip clang's choice AND regressed VC71 79.9%→76.2%. Not
   controllable from C — the documented x87 ceiling.

**Proof of faithfulness:** built an ASYMMETRIC random point cloud (well-separated
distances, unique argmax at every stage) → `artifacts/snapshots/fun106f50_asym.json`
→ unicorn_diff --real-callees = **30/30 PASS**. The lift only "diverges" on
pathologically symmetric synthetic input; real collision-hull geometry never ties.

**One real change made (kept in tree, not the "fix"):** clang builds use `-fno-builtin`
which ignores `#pragma intrinsic(fabs)`, so `fabs()` was emitted as a stubbed CRT CALL
instead of the original's inline x87 FABS. Added a clang-only `#define fabs
__builtin_fabs` (guarded `#else` of the MSVC intrinsic pragma). VC71 unchanged (uses
pragma path); game behavior unchanged (fabs linked fine anyway). Binary-evidence-based
fidelity improvement. Note the project's [[reference_msvc_intrinsics]] convention:
`fabsf` is macro-mapped to `xbox_fabsf`→`__builtin_fabsf` under the clang path via
common.h/inlines.h, but double `fabs` is NOT in that remap list.
