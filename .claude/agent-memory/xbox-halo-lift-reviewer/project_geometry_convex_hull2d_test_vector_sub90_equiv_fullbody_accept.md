---
name: geometry-convex-hull2d-test-vector-sub90-equiv-fullbody-accept
description: convex_hull2d_test_vector 0x1063f0 geometry.obj 66.4% VC71 AUTO_ACCEPTED on 100%-coverage HIGH-confidence equivalence; first geometry.obj sub-90 equiv clear; NaN/inf divergence classification
metadata:
  type: project
---

convex_hull2d_test_vector @ 0x1063f0 (geometry.obj, Liang-Barsky ray/segment clip vs convex 2D polygon, returns bool, writes [tmin,tmax] to out params) AUTO_ACCEPTED at 66.4% VC71 on EQUIVALENCE evidence — FIRST geometry.obj sub-90 clear.

**Why AUTO_ACCEPT despite <90%:** the <90% band requires runtime/behavior verification + classified mismatches. Unicorn differential IS that runtime evidence, and here it was full-BODY not guard-only:
- Random-seed equiv (100 seeds, --allow-stubs --float-tolerance 32 --mem-trace): 97 pass / 2 diverge / 1 error, **coverage 275/275 bytes (100.0%) confidence HIGH** — reproduced exactly.
- Both divergences confirmed NaN/inf-only: inputs literally contain `num_verts=256, polygon2d=[-1e7, inf, -inf, NaN...]`; on the divergent seed BOTH sides return AL=1 (accept), only the EAX upper bytes and out_tmin/out_tmax scratch differ, holding NaN/inf bit-patterns (x87 unordered-compare vs clang IEEE NaN propagation, INSN counts 14114 vs 26414). Grep for any finite-input divergence line = EMPTY. Impossible for real finite 2D polygon coords.
- The 1 "error" is a symmetric INSN-LIMIT (oracle=1000001 lifted=1000001 identical → assert→stub runaway loop on pathological input) = harness artifact, not divergence.

**KEY discriminator (same as [[project_actions_0x1d530_sub90_statesnapshot_clear]]):** full-body 100%-coverage passing equiv clears sub-90; contrast [[project_bsp3d_146be0_sub90_guardonly_equiv_needsruntime]] held NEEDS_RUNTIME because its equiv was 16% guard-only (body never ran). The objdiff % is NOT the gate — body coverage of a PASSING equiv is.

**Static verification also fully clean (independently reproduced, not trusted from brief):**
- Delinked ref delinked/functions/001063f0.obj correct: 108 insns, 3 dir32 relocs at +0x7c→DAT_002533d0 (double parallel-eps), +0x8f→FLOAT_002533c0 (float 0.0 denom-sign), +0xd6→DAT_00253f44 (float 1e-4 parallel-outside) map EXACTLY to the source's `*(double*)0x2533d0` / `*(float*)0x2533c0` / `*(float*)0x253f44` reads; multi-return tail intact ending `b0 01 mov $0x1,%al; ret` (no §26 truncation).
- Reproduced 66.4% (109/108 insns) by VC71-compiling geometry.c + compare_obj against per-fn ref (had to `llvm-objcopy --redefine-sym _convex_hull2d_test_vector=FUN_001063f0` because names differ). Ceiling is x87 load/store SCHEDULING + regalloc (candidate recomputes dx/dy inline at edx-base for the `num` term; ref reuses FP-stack slots at ecx-base).
- **FPU-WARN detector run on the matched pair = EMPTY** (no operand-order bug — resolves the objdump AT&T fsubrp-swap ambiguity; the sign is consistent between cand/ref). LOADW empty, IMM empty (constants are memory reads not inline imms). ABI regs=none no hazards. Hazard scan exit 0.
- bool return matches disasm: accept path is 8-bit `mov al,1` (not `mov eax`); return-type fix int16_t→bool this session aligned codegen (+1pp) and killed a false EAX-upper-bits equiv divergence.

**Gotchas noted:** (1) the synthetic snapshot artifacts/snapshots/convex_hull2d_test_vector_synthetic.json pins ALL 6 args so its "50/50" is ONE finite case run 50× (weak/87.3% cov, "all seeds identical" warning) — and that specific ray (origin (2,-1), dir (0,1), square (0,0)-(4,4)) actually REJECTS at edge 2 (tmax=1 < tmin=5), so it exercises edge-classify + min/max + the tmax<tmin reject, NOT the accept path; the accept path + out-param writes are covered by the 100%-coverage random run instead. (2) VC71 for 0x1063f0 is NOT registered in objdiff.json (only bsp3d_FUN_00146e30 was added to working tree); the per-fn ref is on-disk-only, so `vc71_verify src/.../geometry.c` picks the missing whole-file delinked/geometry.obj unit and fails — must compare_obj directly against delinked/functions/001063f0.obj.
