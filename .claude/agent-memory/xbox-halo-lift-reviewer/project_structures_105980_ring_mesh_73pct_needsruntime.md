---
name: structures-105980-ring-mesh-73pct-needsruntime
description: FUN_00105980 structures.obj ring/cylinder mesh builder; header claimed 91.1% but tool says 73.2% (<90%); full body faithful, 2 equiv failures uncleared = NEEDS_RUNTIME
metadata:
  type: project
---

FUN_00105980 (0x105980, structures.obj, geometry.c:346) — ring/cylinder (torus-like) mesh builder. cdecl void(float*matrix, short*out_vertex_count, short*out_index_run_count, float*out_positions, float*out_texcoords, short*out_indices, short ring_seg, float param_8, int cyl_seg, float param_10). regs=none, ABI clean, all callees cdecl (display_assert 0x8d9f0, rotate_vector3d_by_sincos 0x10b6e0, matrix_transform_point 0x109590, x87_fcos/fsin inlined).

VERDICT: NEEDS_RUNTIME.

**Why:** Task HEADER MISLABELED as "91.1% via redelink" — actual vc71_verify = **73.2% (266/267 insns)** against BOTH whole-file AND freshly re-delinked per-fn ref (delinked/functions/00105980.obj, 267 insns). 91.1% not reproducible. Trust the tool (same lesson as lrar_cache 83.8%). 73.2% = <90% band → policy requires golden/runtime verification + classified mismatches; without clean runtime → NEEDS_RUNTIME.

**Full body decoded 1:1 vs reference disasm — ZERO concrete bugs:**
- Cross product (fVar1,fVar2,0)×axis: A=fVar1*ax1-fVar2*ax0→normal[2]; B=ax0*0-fVar1*ax2→normal[1]; C=fVar2*ax2-ax1*0→normal[0]. Operand order MATCHES source. FPU-WARN detector EMPTY for this fn.
- Magnitude add order (C²+B²)+A² matches source left-to-right; sqrt.
- Normalize guard `fabs(fVar4)>=epsilon@0x2533d0`: fcomp/fnstsw/test ah,0x5/jnp = correct `>=` polarity (jnp skips normalize when <). 1.0f@0x2533c8 fdivr. Consts: 2pi@0x255a54, 0.0f@0x2533c0.
- Index emission: run-start `*out_indices=cyl*2+2` when sVar8==0, then local_8/local_14 pair. Verified.
- Seam copy (sVar8==cyl): out_positions - local_c*3. Last-ring copy: iVar5=(local_c+1)*ring; pos -3*iVar5, tex -2*iVar5. Verified.
- **matrix_transform_point(matrix, out_positions, out_positions)** — duplicate-arg WARN from hazard scan (structures.c:2892) is a FALSE POSITIVE: disasm at 0x256-0x271 pushes esi,esi,ecx = in-place transform IS faithful (original genuinely passes out_positions as both in and out). Not Ghidra register aliasing.
- rotate_vector3d_by_sincos(out_positions, &normal, sin, cos) arg order verified.
- All 3 store paths (early-return zero, final vertex/run counts) verified.

**73.2% LCS driver = benign scheduling/reorder** (insn count near-parity 266/267): FPU stack reuse for A component, axis-global reload placement (source reloads inside inner loop, ref after inner loop — value only consumed in outer cross product = equivalent), store/compute interleaving. NOT logic divergence.

**Blocking gap:** equivalence smoke (artifacts/equivalence/FUN_00105980_smoke.log) = 73 pass / 2 FAIL (scratch buffer differs + mem-trace) / 25 assert-stub INSN-LIMIT errors (benign). 96.1% cov high conf. The 2 failures are on PATHOLOGICAL inputs (param_8=param_10=-1e-07 near-zero scale, all-extreme buffers) where the normalize fabs>=epsilon branch is x87-rounding-sensitive; insn-count diff (oracle 136752 vs lifted 125527) consistent with branch flipping across iterations. STRONGLY suspected float-tolerance artifact (only 2/75 evaluable, only on degenerate inputs; 73 pass at 96.1% cov would catch a systematic bug) but NOT proven — smoke run had NO --float-tolerance.

**Remediation to reach AUTO_ACCEPT:** re-run `unicorn_diff.py 0x105980 --seeds 100 --allow-stubs --mem-trace --float-tolerance 32`; if the 2 scratch failures clear, body is proven → AUTO_ACCEPT. Or golden/dual-oracle. Not REJECT (no concrete bug, no ABI/FPU-order/arg/offset uncertainty).
