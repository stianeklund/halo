---
name: real-math-gzdestroy-esi-disasm-audit-bar
description: what cleared FUN_00112cd0 (zlib gz_destroy) on a STATIC 1:1 disasm audit when VC71 is blind (@<esi>-defined prologue + unregistered fresh delinked ref) — a multi-branch cdecl teardown body, not a leaf
metadata:
  type: project
---

FUN_00112cd0 @ 0x112cd0 (real_math.obj) = zlib gzio `gz_destroy`, `int(int gz @<esi>)`, AUTO_ACCEPTed on a static 1:1 disassembly audit. VC71 uninformative for the same reason as [[real-math-gzflush-eax-disasm-audit-bar]] / [[real-math-putlong-eax-ebx-disasm-audit-bar]]: the function RECEIVES its arg in a register (ESI here), so VC71 cannot emit the reg-receiving prologue ([[eax-defining-fn-vc71-ceiling]]), and the delinked ref is fresh/unregistered. The body itself is cdecl-internal (callees take stack args), so the disasm is fully auditable.

**Why disasm audit is sufficient here (no runtime needed):** zero FPU, zero local buffers, no `@<reg>` *callees* (all callees are cdecl `PUSH; CALL; ADD ESP,N`), no duplicate args, no pointer-as-float, no CONCAT, no unclassified control flow. Pure pointer/int teardown. The only ABI subtlety is the function's OWN `@<esi>` receipt, which has ported precedent (0x3b7e0/0x3ec80) and is the documented VC71 ceiling, not a bug.

**The method that cleared it (reusable for the rest of the gzio cluster):**
1. `read_memory` the full byte range (0x112cd0–0x112dab, 0xdb bytes here) and decode with capstone CS_MODE_32 — do NOT trust Ghidra's decompiler polarity on the short-circuit chain.
2. **Resolve every E8 rel32 CALL target arithmetically** (`tgt = call_addr + 5 + disp`) and compare to the kb addr the C calls by name. All 9 calls (debug_free×5, deflateEnd, inflateEnd, crt_fclose, _errno) matched. This catches a wrong-callee lift that name-matching alone would miss.
3. **Verify guard polarity line-by-line**: `test;je skip` => `if(x!=0){...}`; `test;jge skip` => `if(x<0)`. The fclose chain is a 3-link `je`-skip short-circuit: `[esi+40] && fclose(p)!=0 && *_errno()!=0x1d => or edi,-1`. C `&&` chain is faithful.
4. **Decode the push immediates as the gzio.c line numbers** and confirm against C: 0x143=323, 0x158=344, 0x159=345, 0x15a=346, 0x15b=347 — the original /GF-deduped string @0x28d3c8 = "c:\halo\SOURCE\memory\zlib\gzio.c" (inspect_memory_content confirmed, used 5×).
5. **err(EDI) init + flow**: `xor edi,edi` = `err=0`; `mov edi,eax` only on taken w/r branch (shared label after both), so no path leaves err uninitialized; final `mov eax,edi` = `return err`. The NULL guard `test esi,esi;jne` returns -2 (`b8 fe ff ff ff`).
6. Gates: reg-baseline 0 drift, hazard `--changed-only` clean, build green, callees deflateEnd/inflateEnd `ported=true` (declared as 1-stack-arg cdecl, matching the `PUSH ESI;ADD ESP,4` at the call site).

This is the FIRST gzio-cluster member with a **multi-branch body** (vs the leaf gz_flush/putLong) cleared purely static — the bar held because no callee carries an implicit reg arg, so there is no dark dataflow the disasm can't see.
