---
name: biped-physics-step-0x1a2f40
description: FUN_001a2f40 biped physics-step field map, new_position/new_velocity offset inversion, and back-half non-reconstruction ceiling
metadata:
  type: reference
---

FUN_001a2f40 (0x1a2f40) is the largest function in bipeds.obj (~5.3 KB, 1613
instructions): the biped ground/movement physics step. Single register arg in
ESI (read-before-write); caller FUN_001a5300 does `LEA ESI,[EBP-0xe4]; CALL`
with no pushes and discards EAX → `void FUN_001a2f40(void *physics@<esi>)`.
Huge _chkstk frame (0xafac) for a ~44 KB debug-draw scratch buffer.

## Confirmed field map (physics = float*, index = byte_offset/4)
- +0x04 flags(u16); bit 0x10/0x20/0x01 select movement mode, bit 0x200 sub-mode,
  bit 0x40/0x80/0x100 select debug-draw color.
- +0x08 position[3]; +0x14 control/forward axis input; +0x20 ground normal;
  +0x2c velocity[3]; +0x38 step-height; +0x3c control direction (physics[0xf..0x11]);
  +0x48 friction (used as `1.0 - friction`, 1.0f = *(float*)0x2533c8);
  +0x4c max speed (mode 0x10); +0x50 max speed (mode 0x01); +0x80 ground plane.
- +0xa0 state/result word (mixed u16 and byte access to the SAME field).
- +0xac new_position; +0xb8 new_velocity; +0xc8 step distance.

## Offset inversion (the load-bearing gotcha)
The binary's own assert strings prove +0xac = `&physics->new_position`
(string @0x2b5080) and +0xb8 = `&physics->new_velocity` (string @0x2b5068).
The Ghidra decompiler labels these INVERTED. Trust the assert strings, not the
decompiler field names.

## Two resolved keystone callees (were void(void) in kb.json)
- FUN_00150550: 10-arg cdecl collision/line-of-sight query; returns u16 count
  in AX; arg9 = max count (0x10), arg10 = base of 16-entry 0x2c-byte result
  array; arg4 is a float pushed as physics[0x15]. Callers: FUN_00149ce0,
  FUN_001506d0, FUN_001a2f40 (none from ported source → safe to re-decl).
- FUN_0014c4b0: 4-arg cdecl surface/feature lookup; returns bool in AL; arg4 is
  a 0x2c-byte out_result buffer (same entry layout as the result array).

## Lift status / ceiling (UPDATED 2026-06-07: FULL reconstruction)
FULL faithful lift, ported=false (dormant). VC71 = 75.8% (mnemonic-LCS),
candidate 1589 vs ref 1613 insns. The earlier 19.3% was a DECLINED attempt
(back half never written), NOT a measured cap — do not trust the old
"back half not reconstructable" line. The entire body is now reconstructed:
prologue, 2 input asserts, all 4 movement modes (0x10/0x20/0x01 + default
surface-walk with slope curve), debug-draw colour, FUN_00150550 query, the
nearest-'mach'-surface refinement loop, the planar clamp, loop-A ground-plane
select, loops B/C (nearest object / mach scan), the +0xac/+0xb8/+0xc8
writeback, biped step-down, and 2 NaN asserts.

### How it was reconstructed (key facts for re-lift)
- vc71_verify uses MNEMONIC-ONLY LCS (offsets/operands don't score). So model
  locals as plain C vars/arrays; what scores is the OPERATION SEQUENCE. Get the
  C right and MSVC 7.1 reproduces the codegen.
- `-NAN` sentinel stores to ESI[0x26/0x27/0x29/0x2a] are INTEGER
  `MOV dword,0xffffffff` (none-handle), NOT float NaN. Write as `*(int*)`.
- `local_40`/result_count is the u16 count from FUN_00150550 (in AX), int.
- new_position(+0xac): UNCERTAIN. Current source recomputes `los_dir - los_dir2`
  at the writeback (float store). But the ORIGINAL writeback (0x1a4160) does NOT
  recompute: it INTEGER-copies (`movl`) three dwords from a per-mode carried stack
  vector ([EBP-0x18/-0x14/-0x10]) into +0xac, and integer-copies los_dir
  ([EBP-0x78/-0x74/-0x70]) into new_velocity(+0xb8); the fresh FSUB+FSQRT only
  computes the step distance (+0xc8). That carried [-0x18] vector is written
  per-movement-mode (rotation at 0x3341/0x34af, `[-0x30]+[-0x78]` at 0x3760, the
  `proj` scale-add at 0x3c37) — it is NOT obviously `los_dir - los_dir2`. So the
  REPRESENTATION differs (int-copy of a carried slot vs float recompute) — proven.
  Whether the VALUE diverges is UNRESOLVED (would need equivalence; fn dormant).
  Recovering the writeback's integer-copy run faithfully would require threading the
  carried new_position vector through every mode = path-spanning rewrite, NOT a
  mechanical block. Deferred. (~+0.6pp upside, real latent-bug risk if value diverges.)
- FUN_00150550 arg4 was `float` in kb.json but the original pushes physics[0x15]
  as a RAW dword; corrected the kb decl to `int arg4` (only caller is this
  dormant fn) and pass `*(int*)&physics[0x15]`. Same for arg5.
- mode-0x20 double-stores gx/gy to both new_velocity and the carried planar
  normal via `a = physics[0x2e] = expr` (fsts+fstps); +0xc0 is an int copy.
- `static char buf[44040]` drops the _chkstk prologue; use a PLAIN stack local
  to force `mov eax,0xafac; call _chkstk`.
- Loops walk a pointer at stride 0x2c (add 0x2c), not `&results[count-n]`.
- File uses `sqrtf()` (NOT `SQRT` — that's a Ghidra-ism clang rejects).

### Proven structural cap (why 88% is unreachable)
~367 insns sit in <=4-insn diff blocks = pure x87 SCHEDULING the C front end
can't control: fcomp-vs-fucomp, fxch, doubled `flds` before fcomp, ST0 spills.
Two large blocks proven non-yielding across 4+ attempts: the slope-response
material curve (~24 insns, doubled-load compares) and the mode-0x10 clamp
(normalize3d + fxch interleave) — see [[feedback_x87_st0_chaining_ceiling]] and
[[feedback_fpu_comparison_direction]]. The dead debug-draw branch (0x1a3721,
~26 insns, gated on debug global 0x4e4cf2) has a fall-through control-flow
shape that an if/else can't match. Recovering ALL remaining mechanical >=10
blocks would reach ~82-83%, still short of 88%. Decision rule that ended the
oscillation: per residual block, FPU-stack juggling (fxch/fld %st/fucomp/
doubled-load) = capped/leave; straight-line mov/fld/fchs/fstp/branch =
mechanical/fix.

### Writeback / asserts / block-23 — mechanical-recovery pass (2026-06-08)
- VC71 75.76% (1587) -> 75.94% (1590), still rounds to 75.9%. One value-preserving
  win: the two final NaN asserts now read each vec3 component through a single
  base pointer (`unsigned int *np=&physics[0x2b]; np[0]/np[1]/np[2]`) instead of
  three `*(unsigned int*)&physics[0x2b/2c/2d]` — matches the original's
  `[EDI]/[EDI+4]/[EDI+8]` (EDI=ESI+0xac) + `MOV [EBP-0x40],reg` per-component temp
  store at 0x1a42dc/0x1a436b. +3 matched mnemonics, NO neighbor regressed, no value
  change. (Block scope braces needed for C89 — the pointer is block-local.)
- block-20 (`fmuls 0x8(%edi)` vs `0x3c(%esi)`) and block-23 (`flds 0x7c(%esi);
  fsubs 0x2533c8; ...` the hi-segment slope interp at 0x1a356e, source line ~3129)
  are CAPPED: operands are IDENTICAL (block-23 = `(physics[0x1f]-1.0)*(height-
  physics[0x1d])/(physics[0x1e]-physics[0x1d])+1.0`), only x87 SCHEDULING differs.
  And vc71 LCS is mnemonic-only, so operand-only diffs score 0pp anyway — never a lever.
- The remaining ~23-insn gap is the value-coupled writeback (above) + the proven
  x87/scheduling caps. 75.9% is the faithful mechanical ceiling without a
  path-spanning rewrite. The "~82-83%" estimate in the older note assumed the
  writeback was mechanically recoverable; it is not (value-coupled). Treat ~76-77%
  as the honest cap for this function absent equivalence-backed restructuring.

### Refinement-block scale_add operates on `proj`, NOT new_velocity (2026-06-07)
At 0x1a3c65..0x1a3c8b the in-place `vector3d_scale_add(base,dir,scale,out)` call
aliases base==out==EBP-0x78, and EBP-0x78 is the `proj` stack local (3 floats,
proven by the `face` dot product at 0x1a3c08-0x1a3c25 multiplying EBP-0x78/-0x74/
-0x70 against bestN=EBP-0xA8). It is `proj += -(face+*0x2546a4)*bestN`, NOT a
write to new_velocity(+0xb8). cdecl push trace: 0x1a3c6e PUSH &-0x78 (out),
0x1a3c6f+FSTP[ESP] (scale), 0x1a3c7b PUSH &-0xA8 (dir=bestN), 0x1a3c7f PUSH &-0x78
(base). The original DOES alias base==out — keep it; scale_add is component-wise
(`out[i]=scale*dir[i]+base[i]`) so in-place is safe. The hazard scanner flags
`arg 1 and 4 both "proj"` — that is a FALSE POSITIVE for this pattern (same as the
accepted bipeds.c lines 2495/2692/2694); do NOT add a temp to silence it. An
earlier draft wrote this to `physics+0xb8` (corrupting new_velocity) — now fixed.

The result-array entry is 0x2c bytes: +0x00 point[4], +0x10 normal[3],
+0x1c plane_d, +0x20 object handle, +0x24 surface handle, +0x28 flags/material
(typed as struct biped_collision_result in bipeds.c).

Caller/dispatcher cluster context: [[reference_biped_update_dispatcher]].
