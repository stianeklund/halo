---
name: geometry-convex-polygon3d-verify-sub90-equiv-fullbody-accept
description: convex_polygon3d_verify 0x106dc0 geometry.obj 76.9% VC71 = AUTO_ACCEPT; 2nd geometry.obj sub-90 equiv clear, same pattern as convex_hull2d_test_vector
metadata:
  type: project
---

convex_polygon3d_verify @0x106dc0 (geometry.obj, src/halo/math/geometry.c, bool(int16_t vertex_count, float *vertices), regs=none) AUTO_ACCEPTED at 76.9% VC71 (144 cand / 142 ref). Second geometry.obj sub-90 equiv clear; same acceptance shape as [[project_geometry_convex_hull2d_test_vector_sub90_equiv_fullbody_accept]].

Why cleared (independently reproduced):
- FULL 1:1 static decode of the entire 142-insn delinked ref (delinked/functions/00106dc0.obj, intel-decoded). Reference normal cross product ref0=edge_a1*edge_b2-edge_a2*edge_b1 / ref1 / ref2 all standard order; per-vertex c0/c1/c2 standard order; dot=ref0*c0+ref1*c1+ref2*c2; comparison `fcomp -1e-6; test ah,0x5; jnp` = `dot < -1e-6 => return 0`; inf/NaN exponent-mask 0x7f800000 on cur[0/1/2]; accept `mov al,1`, reject `xor al,al` (bool). Every insn accounted for, matches source.
- Delinked ref correct/non-truncated: 142 insns, dual proper ret tail (0x179 + 0x182), single data reloc DAT_0028c038 = bytes bd3786b5 = -1e-6f exactly.
- FPU-WARN (`fld` param-slot vs recomputed-local, block1 insn4) is a BENIGN load-SOURCE difference, NOT an operand swap. Proof two ways: (1) manual intel-decode of all cross products = correct order, no real fsubr (the 6 "fsubrp" in objdump AT&T are the known `de e9`=fsubp display artifact, see [[reference_objdump_att_x87_fsub_swap]]); (2) BEHAVIORAL — reference normal uses vertices[0..8] (always in-bounds) recomputed EVERY seed, so a normal-negating swap would flip accept<->reject on many in-bounds seeds; 49/50 random seeds pass, so no swap.
- Equivalence reproduced independently: random 50 seeds = 49 pass, coverage 100.0% (387/387) confidence HIGH. Synthetic snapshot (artifacts/snapshots/convex_polygon3d_verify_synthetic.json, vertex_count=4 CCW square, epsilon pinned) = 100/100 pass (arg-pinned single path → this is the source of leaf_cache's 97.7%/weak, NOT a defect).
- Single random divergence (seed 36) is decisively OOB harness artifact: vertex_count=149 reads 447 floats (1788 B) far past unicorn scratch buffer. Oracle reads garbage/inf → bails INSN 360 (al=0, return 0); candidate reads zero-fill (finite) → loops 149 verts INSN 19897 → return 1. Different uninitialized OOB memory, not logic. Same class as test_vector num_verts=256.
- ABI regs=none; hazard --changed-only clean; diff scoped to geometry.c (this fn, 81 insertions) + kb.json.

Reusable: leaf_cache confidence "weak" can be an artifact of an ARG-PINNED snapshot run (single path = no diversity), NOT a coverage defect — check whether the "weak" run was arg-pinned before treating it as blocking. The random-seed run is the coverage/confidence authority.
