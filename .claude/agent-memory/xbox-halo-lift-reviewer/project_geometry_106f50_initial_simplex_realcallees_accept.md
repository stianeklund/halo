---
name: geometry-106f50-initial-simplex-realcallees-accept
description: FUN_00106f50 initial-simplex builder 79.9% VC71 = AUTO_ACCEPT (3rd geometry.obj sub-90 clear); real-callees equiv defeats stub-buffer masquerade
metadata:
  type: project
---

FUN_00106f50 @ 0x106f50 (geometry.c:1710) initial-simplex/quickhull seed builder, 8-param cdecl bool. AUTO_ACCEPTED on equivalence, VC71=79.9% (477/479).

**Why AUTO_ACCEPT despite <90%:** the meaningful work (4 argmax passes + 5 FUN_001037b0 plane calls + full vertex/edge/surface emit block + unused-slot zero loops) is RUNTIME byte-exact under `--real-callees`: 100/100 PASS, 90.6% cov, mem-trace 0 failures, stub-arg diff 0 mismatches, all seeds return 0x1 (accept path). The 4 return-0 reject guards are STATICALLY verified against delinked ref (00106f50.obj): min_x!=-1 (cmp 0xffff/je 0x5c4), far/line + `0.01<=best` (fcomps DAT_0031fb3c, test 0x5/jnp 0x5c4), plane + `0.01<=fabs(best_dist)` (fcompp, test 0x41/je 0x5c4), winding `0<best_dist` (fcomps FLOAT_002533c0=0.0, test 0x41/jne skip-swap 0x3a3). return-0 epilogue 0x5c4=`xor al,al`; return-1 0x5b9=`mov al,1`. Pass-4 call push order = [plane(ebp-0x4c), p0(ecx), p1, points+line*3] = source exactly.

**KEY LESSON — real-callees defeats a stub-buffer masquerade (inverse of the usual zero-fill artifact):** the default `--allow-stubs` cube config FAILS 89/100 with oracle=return1 / lifted=return0. This is NOT a source bug. FUN_001037b0 is a plane-WRITER whose caller then READS plane[4]. Stubbed, it writes nothing; the ORACLE "succeeds" only because MSVC reused a non-zero scratch stack slot as plane[1] → best_dist becomes garbage-nonzero → accept. Our lift's fresh zero-init `float plane[4]` → best_dist=0 → plane_index stays -1 → honest return 0. Correct test MUST use `--real-callees` so the plane is actually computed. Contrast the usual masquerade (zero-fill makes CANDIDATE fail an early-guard); here zero-init makes the candidate MORE correct than the stubbed oracle.

**"weak" confidence is not a coverage gap here:** faithful run flagged weak + "all 100 seeds returned identical value 0x1 — low path diversity." That is because the asym snapshot pins point_count 6-12 + unique-argmax points to drive the accept path every seed. Coverage is 90.6% (high); the uncovered ~9% is the null-pointer asserts + reject epilogue, whose branches are covered by static disasm polarity checks. Do not read "weak" tier as insufficient body coverage when coverage% is high and the accept path is the thing under test.

**fabs guard (correctness-neutral, real):** clang `-fno-builtin` ignored `#pragma intrinsic(fabs)` and emitted a CRT `CALL fabs`; original inlines x87 FABS. `#define fabs __builtin_fabs` in the clang `#else` branch restores inline FABS. Verified: freshly-rebuilt clang obj build/equivalence/geometry.c.obj (mtime newer than source) has NO `_fabs` symbol or reloc; delinked ref also has none. VC71 unaffected (uses the pragma branch).

**VC71 79.9% ceiling = benign x87 reorder:** FPU-WARN localized to the pass-2 sum-of-squares argmax (`fld %st(3) vs %st(2)`, `fmul %st(4) vs %st(3)`, `flds (%ecx) vs 0x4(%ecx)`) — commutative reorder of (dx²+dy²+dz²), order-independent. No FSUB/FSUBR direction diff, no cross-product swap. 2 unmatched insns = argmax-spill schedule. Hazards clean (no WARN in geometry.c). Both fns cdecl, no reg_args, ADD ESP 0x10 per 4-arg call.
