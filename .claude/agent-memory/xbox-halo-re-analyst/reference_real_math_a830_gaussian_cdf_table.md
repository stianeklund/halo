---
name: real-math-a830-gaussian-cdf-table
description: FUN_0010a830 (real_math.obj) gaussian/noise CDF-table builder — disasm-confirmed constants, x87 spill-width model (float accumulator faithful for both buffer store + feedback), and cos×random pairing; VC71 64.8% is structural (loop-dir + loop-2 unroll + @ebx prologue), gate via equivalence.
metadata:
  type: reference
---

# FUN_0010a830 — gaussian/noise CDF lookup-table builder (real_math.obj)

Lifted 2026-06-30 into `src/halo/math/real_math.c` (ported=false; equivalence is the gate).
ABI: `void FUN_0010a830(float *buf@<ebx>)` (immutable @<ebx>; already in kb.json + kb_reg_baseline.json keyed by addr). Body 0x10a830-0x10a923, next fn 0x10a930.

## What it does
Two passes over a 1024-float buffer:
- Loop 1 (count-down EDI 0x400→0): `buf[i] = sum` FIRST, then accumulate a new
  non-negative term → monotone non-decreasing (CDF shape).
  `sum = (r4+1.0)*0.25 + (cos(i*Fcc)+1)*r3 + (cos(i*Fc8)+1)*r2 + (cos(i*Fc4)+1)*r1 + sum`
  where r1..r4 = `random_math_real((unsigned int*)get_global_random_seed_address())`
  drawn in that order (r1,r2,r3 spilled to 32-bit floats; r4 kept inline in ST0).
- Loop 2: `recip = 1.0/sum` computed ONCE (FDIVR [1.0] on the still-live total ST0),
  then `buf[i] *= recip`. Original UNROLLS loop 2 by 8.

## Constants (disasm-confirmed via read_memory)
- 0x2533c0 = 0.0f  (init accumulator)
- 0x2533c8 = 1.0f  (cos bias added to each cos term; also normalization target C)
- 0x25337c = 0.25f (weight on the 4th/inline random term)
- 0x28c8cc = 0.04479224f  (freq paired with random #3)  ← largest
- 0x28c8c8 = 0.03129321f  (freq paired with random #2)
- 0x28c8c4 = 0.02515729f  (freq paired with random #1)  ← smallest

## Hazard resolutions (all verified against raw disasm, not decompile)
1. SPILL WIDTH: accumulator is a single `float sum`. `FST [EBP-0xc]` (no pop) keeps
   ST0 live → `FSTP [ESI]` writes the same 32-bit-rounded value next iter that
   `FADD [EBP-0xc]` reloads. buf[i] == [EBP-0xc] bit-for-bit. Plain `float sum` is
   faithful for BOTH the buffer store and the feedback — do NOT use double/union.
   r1,r2,r3 = float (32-bit spill via FSTP float). r4 kept inline (no spill) — call
   random inline in the expression.
2. COS×RANDOM PAIRING: pair by the FMUL operand, not computation order.
   c8c4(small)·r1, c8c8(mid)·r2, c8cc(large)·r3.
3. STORE ORDER: `buf[i]=sum` (FSTP [ESI]) happens before the 4 random draws.
4. ADD ORDER: `+ sum` is the LAST addend (FADD [EBP-0xc] right before FST);
   non-associative 1024-deep cumulative sum — keep + sum last.

## VC71 = 64.8% (105 cand / 77 ref) — STRUCTURAL, no FPU-WARN
Gap is entirely: (a) my `for(i=0;i<0x400;i++)` → ascending cmp/jl vs original
count-down DEC/JNZ; (b) original unrolls loop 2 ×8, mine doesn't; (c) @<ebx>
reg-arg prologue (VC71 emits stack-param prologue). Arithmetic/FPU operand order
MATCHES (no [FPU-WARN], no FSUB direction flag). Do not chase the number — equivalence
(float-tolerance) is the gate.

## Callees (cross-object, random_math.obj, both ported=true — call by name)
- random_math_real (0x10b240): `float(unsigned int *seed)`, seed PUSHed on stack (no @<reg>).
- get_global_random_seed_address (0x10b0d0): `int*(void)`.
