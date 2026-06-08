---
name: reg-arg-keystone-vc71-ceiling
description: Callers of @<reg> keystones hit a VC71 ceiling because vc71_verify compiles the call as cdecl pushes, not the register-arg thunk
metadata:
  type: feedback
---

Any function that CALLS a register-arg keystone (e.g. `FUN_001a1a10(... direction@<eax>, unit_handle@<edi>)`) has a permanent VC71-match ceiling, because `vc71_verify.py` compiles the standalone `.c` against MSVC **without** applying the kb.json `@<reg>` thunk. The C call `FUN_001a1a10(scale, out_point, out_vec, direction, unit_handle)` compiles to a plain 5-arg cdecl (5 PUSH + `ADD ESP,0x14`), while the original binary sets EAX/EDI in registers and does only 3 PUSH (`ADD ESP,0xc`). Verified on bipeds collision cluster (2026-06):
- 0x1a1b90 (tiny wrapper, ~all body is the keystone call): 78.6% — capped, left ported=false
- 0x1a1e70 (medium, +FPU stack residual): 86.9% — just under 88%, ported=false
- 0x1a1bc0 (large body, artifact amortized): 95.5% — ported=true
- 0x1a1fb0 (large body): 95.1% — ported=true

**Why:** vc71_verify does not run patch.py's register-arg rewriting; the runtime build does. The cdecl-vs-register push mismatch is invisible to C source and unfixable by permuter. Same class as the documented "fastcall @<ecx> permanent preamble mismatch."

**How to apply:** When a caller of an @<reg> keystone scores low on VC71, do NOT treat it as a lift bug or churn the source. The magnitude of the ceiling scales inversely with body size: tiny wrappers (1 keystone call + return) cap ~78%; large bodies clear 88% easily. For sub-88% cases, leave ported=false, confirm the body is hazard-free and the keystone call args trace correctly to the disassembly (direction = the EAX-loaded global, unit_handle = the EDI value), and document the residual as this artifact. Equivalence/runtime is the only lane that can prove behavior for the capped ones; byte-match cannot.

Related: the keystone 0x1a1a10 itself was ported=false in kb.json during this work, which is fine — patch.py thunks unported callees back to the original, so calling it by name has correct runtime ABI regardless. See [[feedback_vc71_structural_ceiling]] and [[feedback_x87_st0_chaining_ceiling]].
