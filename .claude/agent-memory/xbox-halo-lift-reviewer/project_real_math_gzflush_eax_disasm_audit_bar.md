---
name: real-math-gzflush-eax-disasm-audit-bar
description: What cleared FUN_00112f00 (gz_flush internal, real_math.obj) on a 1:1 cachebeta-disasm audit when VC71 was uninformative (2.5%) due to @<eax>-defined prologue ceiling + unregistered fresh delinked ref
metadata:
  type: project
---

AUTO_ACCEPT of FUN_00112f00 @0x112f00 (real_math.obj zlib gz_flush internal, `unsigned int(int gz @<eax>, int flush)`) on a STATIC 1:1 disasm audit, NOT VC71.

**Why VC71 was uninformative (both must hold to bypass the structural-data-is-blocking rule):**
1. `@<eax>`-DEFINED function — VC71 cannot emit the EAX-receiving prologue (`MOV ESI,EAX`); known permanent ceiling (see [[eax-defining-fn-vc71-ceiling]] in user auto-memory).
2. No valid delinked ref (prior was a 1-insn stub); a fresh delinked/functions/00112f00.obj was exported but NOT objdiff-registered → vc71 reports a meaningless 2.5%. (Same registration-gap class as the equiv-lane objdiff-registration note.)

**What carried the accept (method = full 0x112f00-0x112fb7 disasm vs C, block by block):**
- Guard: `MOV ESI,EAX`(gz), `XOR EBX,EBX`(done=0), gz==0||*(char*)(gz+0x5c)!='w'(0x77)→ret 0xfffffffe, then *(gz+4)=0. ✓
- fwrite call args: cdecl PUSH order at 0x112f32-37 = (buf=*(gz+0x48), 1, n=EDI, file=*(gz+0x40)); result!=n → *(gz+0x38)=-1, ret -1. ✓
- deflate: PUSH flush, PUSH ESI → cdecl(gz, flush); store z_err at gz+0x38. ✓
- n==0 && z_err==-5 special case: gated by `TEST EDI,EDI; JNZ` so EDI(=n)==0 on that path; C `*(gz+0x38)=n` ≡ disasm `MOV [ESI+0x38],EDI` (both 0). ✓
- done recompute + while(z_err==0||==1): exact. ✓
- SBB return idiom 0x112f95-9d (DEC/NEG/SBB/AND) = `-(unsigned)(z_err!=1) & z_err` (maps 1→0). ✓

**The one wart that is NON-blocking (binary is faithful, name is cosmetic):**
0x1db2b3 is **fwrite**, NOT fread. Ghidra symbol "FID_conflict:_fread" is a documented FID hash-collision misnomer (fwrite & fread share signature `size_t(void*,size_t,size_t,FILE*)`). PROOF: 0x1db2b3 body calls write-buffer helper 0x1db19c which does `MOVSD.REP ES:EDI,ESI` copying FROM caller buf INTO the FILE buffer + calls write 0x1df419 / putc 0x1de28c. The repo already documents this at src/halo/main/main.c:1558-1566 (main_save_current_solo_map casts 0x1db2b3 as fwrite). The C lift names it `_fread` (inherited from kb.json:836-837 which maps `_fread`→0x1db2b3) — so the EMITTED call resolves to the correct address with the correct cdecl ABI/arg order; machine output is identical. **Accept; recommend the kb.json `_fread` decl + all real_math.c/main.c call sites be renamed to a faithful name (e.g. crt_fwrite or fwrite_0x1db2b3) for clarity, but this is a rename chore, not a correctness gate.**

Contrast vs [[real-math-zlib-disasm-audit-bar]] (0x112590 deflateInit2_, STATIC 1:1, VC71 blocked by stale 54-insn stub): same family of accept — STATIC disasm-audit accept is allowed ONLY when VC71 is provably uninformative for a structural reason (@<reg> prologue and/or stale/unregistered delinked ref) AND the audit covers guard + every call's arg order + loop/branch logic + return idiom with zero unresolved Uncertain.
