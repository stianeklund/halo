---
name: accel-position-trio-dormant
description: real_math.obj accelerate-to-position FP-motion trio (0x10f770/0x10fa30/0x110650) lifted DORMANT; true ABIs, CRT-callee IDs, and the _CI_TWO_ARG harness swap bug
metadata:
  type: reference
---

# accelerate-to-position FP-motion trio (real_math.obj) — lifted DORMANT 2026-07-01

All three lifted faithfully but committed `ported:false` (NEEDS_RUNTIME, a10-bug-class:
runtime-live motion, wrong activation is worse than dormant). Commits 75c97488 / e86f6451 / 32f754f7.

## True ABIs (kb decls were placeholder `void(void)` — all pure cdecl, NO reg args)
- **0x10f770 angular_accelerate_to_position**: `void(float *facing, float *target_facing,
  float *ang_vel, float max_ang_speed, float ang_accel)`. Reads global fwd-vec ptr
  `*(float**)0x31fc38` on snap paths. Rotation axis = cross(facing, target_facing) — VERIFIED
  no FPU-WARN (a10 cross-product store-order hazard ABSENT here; each FSUBP FSTP'd immediately).
  CRT callee FUN_001d94f0 = _CIacos → lift as `acosf` (macro→xbox_acosf). VC71 79.3%.
- **0x10fa30 accerate_to_position3d** (keep the typo): `char(float *pos, float *vel,
  float *target, int unused, float max_length)`. arg4 [EBP+0x14] is DEAD (never read); [EBP+0x18]
  is a real float so ≥5 slots. delta[] must be contiguous float[3] (addr → normalize3d +
  FUN_0010f9b0). VC71 92.7%, no FPU-WARN.
- **0x110650 FUN_00110650**: `void(float *p_a, float *p_b, float accel, float value,
  float wrap_min, float wrap_max, char wrap_flag)`. Decompiler's __thiscall/param_1 is a MISREAD
  (incoming ECX never read pre-overwrite; prologue PUSH ECX = local temp [EBP-0x4]) — do NOT add
  @<ecx> or every stack arg shifts. Neutral p_a/p_b names, forwarded positionally to
  accelerate_to_position 0x10f5b0. VC71 69.5% (structural: inlined FPREM loop + inlined callee).

## FUN_001daf7e = _CIfmod = C fmod = FPREM (truncated), NOT FPREM1
- Thunk `MOV EDX,0x3312d0; JMP` = MSVC _CI* FPU-arg dispatcher.
- MUST lift as `x87_fmod` (src/x87_math.h, uses FPREM + C2-reduction loop). The `fmod` macro
  (common.h→inlines.h `#define fmod xbox_fmod`) uses **FPREM1** (IEEE round-to-nearest) = WRONG
  semantics for angle-wrap. check_lift_hazards flags literal `fmod(` — heed it, use x87_fmod.
- Arg order fmod(dividend, divisor) proven by 3 anchors: disasm FLD order (ST1=dividend deeper,
  ST0=divisor top), x87_fmod helper asm, and one-arg _CI convention (last-pushed = ST0 = 2nd arg).

## Equivalence is a WEAK/uninterpretable oracle for this class (Gate 2)
- 0x10f770: zero-fill only hits degenerate snap-return (15.5% cov weak; null 0x31fc38 deref
  crashes 67/100). Main rotation path needs bespoke snapshot with real facing/target/ang_vel ptrs.
- 0x10fa30: 0/100 fail = STUB ASYMMETRY not bug. Return-0 trampoline stub of FUN_0010f9b0 forces
  BOTH sides to 0 at `TEST AL,AL;JZ`; observed lifted=1 is logically impossible under symmetric
  stubbing → asymmetry. --real-callees recovers 0→28% pass. infection_swarm.json arg_overrides
  target actor_handle (different fn), don't pin these ptrs.
- 0x110650: lifted INLINES both callees (classed 'leaf', 0 relocs) while oracle CALLs stubs →
  stub-arg differential "call-seq diverged at index 0". 9/9 sequential base seeds PASS (arithmetic
  correct). Uninterpretable, not a divergence.

## HARNESS BUG (filed, not fixed): tools/equivalence/stubs.py _CI_TWO_ARG operand order swapped
- Line ~1180: `_CI_TWO_ARG[name](st0_val, st1_val)` but _CIfmod/_CIatan2/_CIpow use ST1=arg1
  (deeper), ST0=arg2 (per the one-arg _CI convention: last-pushed lands in ST0). Should be
  `(st1_val, st0_val)`. Affects cifmod/ciatan2/cipow. Also fun_001daf7e is NOT registered in
  _CRT_MATH_ADDRS, so fmod-using lifts stub it to return-0. Wiring `"fun_001daf7e":"cifmod"` +
  flipping the arg order is the correct fix but perturbs shared infra — do as a separate change.
