---
name: real-math-zlib-disasm-audit-bar
description: What cleared deflateInit2_ 0x112590 (real_math.obj) on a STATIC 1:1 disasm audit when VC71 byte-match was blocked by a stale 54-insn delinked stub
metadata:
  type: project
---

deflateInit2_ FUN_00112590 (real_math.obj zlib cluster, queue Wave 4) was ACCEPTED on a pure cachebeta-disasm 1:1 audit, NOT VC71 byte-match.

**Why VC71 was bypassed:** vc71_verify reported 41.8% (185 candidate vs **54** reference insns). The whole-file delinked/real_math.obj carries a stale 54-insn stub for 0x112590; the real function is ~185 insns. A fresh per-function ref (delinked/functions/00112590.obj, range 0x112590-0x1127b0) was exported but vc71_verify didn't pick it up (objdiff.json registration gap — see [[equiv-lane-objdiff-registration]]). The 41.8% is a reference-staleness artifact, not a structural gap.

**The bar that cleared it (all verified against `disassemble_function 0x112590` on cachebeta.xbe):**
- Validation order + constants: version NULL / `*version != **0x31fc70` / stream_size!=0x38 => -6; strm==0 => -2; default zalloc=0x117ad0/zfree=0x117b00; memLevel 1..9, method==8, windowBits 8..15, level 0..9, strategy 0..2 => -2; windowBits<0 => neg+wrap=1; level==-1 => 6.
- **Indirect zalloc cdecl arg order** = the high-risk item. Traced each interleaved PUSH chronologically; last-pushed (deepest in source order) = leftmost arg. All 5 calls = `(opaque, items, size)`: state(opaque,1,0x16c0), window(opaque,w_size,2), prev(opaque,w_size,2), head(opaque,hash_size,2), pending(opaque,lit_bufsize,4). opaque always = strm+0x28 reloaded per call.
- deflate_state int* index ↔ byte offset map all confirmed: [0]strm/+0, [2]pending_buf/+8, [3]pending_buf_size/+0xc, [6]wrap/+0x18, [9]w_size/+0x24, [0xa]w_bits/+0x28, [0xb]w_mask/+0x2c, [0xc]window/+0x30, [0xe]prev/+0x38, [0xf]head/+0x3c, [0x11]hash_size/+0x44, [0x12]hash_bits/+0x48, [0x13]hash_mask/+0x4c, [0x14]hash_shift/+0x50, [0x1f]level/+0x7c, [0x20]strategy/+0x80, [0x5a4]sym_end/+0x1690, [0x5a5]lit_bufsize/+0x1694, [0x5a7]sym_buf/+0x169c. byte+0x1d=method(8).
- hash_shift = unsigned MUL magic 0xaaaaaaab + SHR EDX,1 = `(uint)(memLevel+9)/3`; param_5∈[1,9] so signed/unsigned identical anyway.
- **sym-buffer pointer math** (second high-risk item): sym_buf = pending_buf + 2*(lit_bufsize>>1); C wrote `pending_buf + (lit_bufsize & 0xfffffffe)` — exact identity (2*(x>>1)==x&~1). sym_end = pending_buf + lit_bufsize*3 via LEA[EAX+ECX*2]+ADD ECX.
- deflateReset deferral: success path returns FUN_00112260(strm) (cdecl 1-arg PUSH EDI/CALL/ADD ESP,4), EAX propagated. kb decl fixed void(void)->`int FUN_00112260(int strm)`; only the new caller in src, so safe.
- Alloc-fail (window/prev/head/pending any NULL): msg=*0x320e20, FUN_00111170(strm) deflateEnd, return -4. state==NULL returns -4 directly with NO msg/NO deflateEnd (separate epilog at 0x11278c).

**Method note for blocked-VC71 zlib lifts:** when the delinked whole-obj ref is a stale stub, the live cachebeta disasm IS the oracle. Trace indirect-callback (zalloc-style) arg order by chronological PUSH reconstruction — interleaved pushes are the only real trap; everything else is a mechanical offset map. Hazard scan --changed-only clean, clang green.
