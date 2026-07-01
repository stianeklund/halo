---
name: real-math-putlong-eax-ebx-disasm-audit-bar
description: What cleared FUN_001130a0 (zlib putLong) on a STATIC 1:1 cachebeta disasm audit when VC71 is uninformative (@<eax>+@<ebx>-defined prologue ceiling + unregistered fresh delinked ref)
metadata:
  type: project
---

FUN_001130a0 @ 0x1130a0 (real_math.obj) = zlib gzio **putLong**: `void putLong(unsigned int value @<eax>, void *file @<ebx>)` — writes a 32-bit value little-endian as 4 putc calls, LSB first. AUTO_ACCEPTED on STATIC 1:1 disasm.

**Why VC71 is uninformative here (do not gate on it):** the function is BOTH @<eax>- AND @<ebx>-defined. VC71 cannot emit a register-receiving prologue for a function whose own args arrive in EAX/EBX (see [[eax-defining-fn-vc71-ceiling]], distinct from the CALLER ceiling). Combined with an unregistered/fresh delinked ref, VC71 carries zero byte-match signal. This is the same posture as [[real-math-gzflush-eax-disasm-audit-bar]] and [[real-math-zlib-disasm-audit-bar]].

**The bar that cleared it (full-body 1:1 disasm decode, 42 bytes 0x1130a0-0x1130c9):**
- Decoded every byte from `read_memory` (not just the decompiler). Loop body:
  - `8b f0` MOV ESI,EAX (ESI=value from @<eax>); `bf 04...` MOV EDI,4 (i=4)
  - loop: `8b c6` MOV EAX,ESI; `25 ff000000` AND EAX,0xff (value&0xff)
  - `53` PUSH EBX (file/@<ebx>) FIRST => 2nd cdecl slot (stream); `50` PUSH EAX (byte) SECOND => 1st cdecl slot (c) => `_fputc(c, stream)` arg order CORRECT
  - `e8 09860c00` CALL rel32: 0x1130ba+5+0xc8609 = **0x1db6c7** (verified displacement arithmetic, not just trusting the decompiler label)
  - `83 c4 08` ADD ESP,8 (cdecl cleanup, 2 args); `c1 ee 08` SHR ESI,8 (value>>=8); `4f` DEC EDI; `75 e9` JNZ loop
  - EBX (file) untouched across all 4 iterations — confirms it is a stable param, not mutated.
- **CALL-target ABI cross-check:** 0x1db6c7 resolves to cdecl `int(int c, FILE* stream)` — matches the kb decl `int _fputc(int c, void *stream)` and the on-disk push order. _fputc was the sole new caller; decl fixed from void(void) -> int(int,void*).
- **FID_conflict label is non-blocking:** Ghidra FID DB labels 0x1db6c7 as `FID_conflict:_fputc`. Same class as the gz_flush fwrite/_fread FID_conflict — the CALL resolves to the correct addr with the correct cdecl ABI, so the conflicting label does not change argument passing. Do NOT treat an FID_conflict label alone as a blocker when addr+ABI are independently confirmed.
- Reg-baseline `--check`: 541 OK, 0 drift/missing/stale. Hazard `--changed-only`: clean exit 0. Build green, clang clean. @<eax>/@<ebx> ported precedent exists (0x36890, 0x3cb50).

**Reusable rule:** for a tiny @<reg>-defined leaf where VC71 is structurally blind, a full-body byte-level disasm decode that confirms (a) loop trip count, (b) cdecl push order vs the callee's real ABI, (c) CALL displacement arithmetic to the named callee, (d) reg args land where the decl says, and (e) reg params are not unexpectedly mutated — is sufficient for AUTO_ACCEPT. No runtime needed for a pure 4-iteration byte-emit loop with no memory side effects beyond the verified callee.
