---
name: scenario-obj-leaf-abi-misnames
description: scenario.obj 0x18bxxx-0x18fxxx leaf batch — render_sprite.c funcs are register-arg, PDB-import kb names contradict the binary, and the activatable wins are the cdecl BSP/cluster query helpers
metadata:
  type: reference
---

scenario.obj has two distinct sub-clusters in the 0x18b000-0x18fff0 range; the
ABI split is sharp and the kb "NAMED LOOKUPS" decls are often wrong.

## Register-arg sub-cluster (render_sprite.c) — expect @reg ceiling, lift dormant
These read ECX/EDX/EAX/EBX/ESI/EDI BEFORE any [EBP+x] stack arg. VC71 cannot
emit a @reg-DEFINED frameless prologue (cdecl param-shift cascade) so they cap
~75-82%, AND vanilla unicorn_diff can't bridge reg-vs-stack ABI (stub-arg diff
fails on the reg-carried arg or on a global data-ref like the players pool
0x5aa6d4 read from zero-filled candidate memory). Both verification lanes are
false-negatives by construction — verifiable only via true dual-oracle-by-address
(asm thunk) or golden-master. Confirmed register-arg in this batch:
- 0x18b010 @<esi> (frameless; local-player first-person view predicate). Sole
  caller FUN_0018c100 sets `MOV ESI,[EDI]`. Recovered + baseline updated, DORMANT.
- 0x18b130 @<esi> float-return (passes ESI to object_get_and_verify_type(-1)).
- 0x18b930 @<eax>(out) @<edx>(in) @<ecx>(out) + 1 stack ptr (plane/normal builder).
- 0x18d040 @<eax> @<ecx> + 3 stack (fog/lighting scalar).
- 0x18d140 @<esi> + 1 stack (sprite-group bitmap add, render_sprite.c).
- 0x18d360 cdecl(!) but calls many render fns (build_sprites_end).
- 0x18d490 @<ecx> @<eax> @<ebx> + stack (sprite basis builder).
- 0x18cf10 @<ebx> @<esi> @<edi> + stack (sprite origin transform).
- 0x18fb20 @<ecx> @<edx> + 1 stack float (per-component move-toward clamp).
- 0x18ff00 @<eax> @<edx> + 2 stack float (FPU, PTR_0031fc38 fwd-vector based).

## cdecl sub-cluster (scenario.c BSP/cluster queries) — these ACTIVATE
Pure cdecl, single stack arg, clean to land ≥88%:
- 0x18ed90 char(short cluster_index, float* point): cluster shape volume test.
  Type-0 AABB bounds at box+0x48/0x4c/0x50/0x54/0x58/0x5c; type-1 oriented box
  via matrix4x3_from_forward_up_position(local, box+0x48, box+0x30, box+0x3c) +
  real_matrix3x3_transform_point. VC71 90.0% after (a) `switch(type)` to match
  SUB/DEC/JZ dispatch (+20pp from if/else) and (b) writing max-bound checks as
  `!(point < bound)` not `point >= bound` to match the `<`-family FCOMP/JP mask.
- 0x18e9b0 scenario_location_potentially_visible (all-players PVS variant of
  *_local at line 319; assert line 0x1ef vs 0x1e7). 94.6%.
- 0x18e5c0 char(int location): cluster background-sound (lsnd) flag test
  (bsp+0x134 cluster -> +4 sound ref -> bsp+0x1fc elem 0x74 -> +0x2c lsnd index
  -> tag[0]&1). 92.4% after single-exit `result` var matching the BL-held-default.
- 0x18d670 float(short mode, float* v1, float* v2): pure FPU leaf,
  |dot(v1,v2)/|v1||; mode0=1.0, mode2=1.0-that. VC71 81.4% -> 96.3% by
  fabsf-intrinsification: wrap `fabsf(x)` in `#if defined(_MSC_VER) &&
  !defined(__clang__)` -> `(float)fabs((double)x)` (VC71 /Oi inlines x87 FABS),
  `#else` byte-identical fabsf (clang/shipped build UNCHANGED). Generalizes the
  cos/sin edit-#4 to the fabs/sqrt family; killing the fabsf CALL collapsed a
  cascade (removed the call + the callee-saved ESI spill that parked `mode`
  across it) -> +14.9pp from a 1-line wrap. Residual: lone FDIVRP/FXCH vs FDIVP
  x87 stack-arrangement idiom (not a bug). EQUIV 100/100 prior -> activated.
- 0x18bf80 int(int object_handle, float lod): cached render-state slot
  resolver/evictor (obj+0x120 index; pool 0x50652c; age=DAT_506544-datum+0xc,
  neg clamped to 1000.0). VC71 79.8% (x87 ST0-chaining in eviction loop +
  split-epilogue), equiv vacuous -> DORMANT. Calls FUN_0018bc60(int,float,char)
  whose kb decl was `void(void)` — fix the callee decl + rebuild decl.h BEFORE
  vc71_verify or the float arg double-promotes (8-byte push) — a real crash hazard.

## kb name mis-attributions (PDB import) — DO NOT propagate
The "NAMED LOOKUPS" kb names contradict the binary; keep FUN_ + evidence comment:
- 0x18d670 "scenario_reload_structure_bsp_if_necessary" = vector-angle helper.
- 0x18b130 "...tag_index_get" returns a FLOAT in ST0, not an index.
- 0x18b930 "scenario_illumination_at_point" = plane/normal constructor.

Shared globals: 0x5064e0/0x5064e4 = global_structure_bsp / global_scenario;
0x50652c = cached object render states pool; 0x5aa6d4 = players data pool;
0x506548 = local-player index (read 16-bit); 0x2533c0=0.0f, 0x2533c8=1.0f,
0x254cb8=1000.0f.
