---
name: lrar-cache-dispose-11cab0-stateful-guardonly-needsruntime
description: lruv_cache.obj FUN_0011cab0 lrar_cache_dispose 87% VC71 = NEEDS_RUNTIME. Static body 1:1 clean but sub-90 core block-free loop is DARK under zero-fill equiv (guard-only 27.1%); STATEFUL sibling of the stateless lruv_cache_new (which was AUTO_ACCEPT). Distinct-struct lrar vs lruv note.
metadata:
  type: project
---

FUN_0011cab0 (0x11cab0, lruv_cache.obj) = `lrar_cache_dispose`, cdecl `void(int cache)`.
87% VC71. Verdict = **NEEDS_RUNTIME** (not REJECT — no concrete bug; not AUTO_ACCEPT — sub-90 core loop dark).

**Full static decode 0x11cab0-0x11cbec = ZERO defects.** Verified 1:1 vs disasm:
- entry: MOV EAX,ESI; CALL 0x11c820 = lruv_update_function_pointers(cache@<eax>) — reg arg correctly set up (kb decl `int cache@<eax>`).
- head=*(short*)(cache+0x34); empty-cache JZ path resets head/tail=-1 (both entry and internal `goto done` set AX=-1 first).
- 3 assert branches, all polarities correct (signed vs unsigned): 0x199 header (magic @+0x44==0x6c726172 'lrar'; [+0x24]>=[+0x28] unsigned JNC; [+0x2c]<1 signed JLE; block_count[+0x38]<=0 signed); 0x16e index (block_index<0 || >=block_count); 0x186 block record (block[1]@off4==0x52626c6b 'klbR'; size=block[3]@off0xc <0 || >=[+0x2c]; block[2]@off8 <[+0x24] unsigned JC; block[2]+size >[+0x28] unsigned JBE-ok).
- core work: block=block_index*0x10+[+0x30]; if block[0]!=0 → PUSH block[0]; CALL [cache+0x40] (free cb, cdecl 1-arg); block[0]=0. ring advance INC + wrap at block_count(XOR EBX) + -1 sentinel guard (CMP BX,-1). tail-match → done. head/tail=-1 reset.
- csprintf 4-arg (buf,fmt,cache,cache)@0x199 / 5-arg (+block)@0x186; display_assert(msg,file,line,1). All ADD ESP cleanups match arg counts. Both format strings have matching %s/%p count.

**Why NEEDS_RUNTIME (the blocking call):** 87% is <90%. Band policy = golden/runtime BODY verification required. The equiv that ran is **zero-fill, 27.1% coverage, GUARD-ONLY**: only the empty-cache head==-1 reset path + header/corrupt-assert branches (concolic) executed. The substantive body — the block-iteration loop that validates each 'klbR' record, calls the free callback via cache+0x40, clears block[0], and walks head→tail through the ring with block_count wraparound — is **UNREACHABLE without a populated 'lrar'-magic ring buffer**, and no such snapshot/fixture exists. This is the bsp3d-88.4% guard-only pattern, NOT a body defect.

**KEY discriminator vs sibling lruv_cache_new 85.1% AUTO_ACCEPT:** that one is genuinely STATELESS → zero-fill+stubs was the COMPLETE lane (whole body ran, HIGH conf). `dispose` is STATEFUL — its core disposal work depends on a live populated cache, and that entire region is dark. Coverage discriminator = "did the substantive body run" — here it did NOT. Same 'stateless→accept / stateful-dark→needs-runtime' split as [[project_lruv_cache_11d110_stateless_allocator_sub90_accept]] vs [[project_bsp3d_146be0_sub90_guardonly_equiv_needsruntime]].

**Discount contradictory boilerplate:** acceptance path claimed "0-divergence pass on live infection_swarm snapshot = accepted evidence" BUT equiv detail says infection_swarm.json ABSENT + no fixture has a 'lrar' cache. The live pass did NOT run. Same self-contradictory copy-paste as the sibling. Do not credit it.

**Non-blocking fidelity nits (LCS-gap only, not defects):**
- Source uses `halt_and_catch_fire()` on assert paths where binary does `system_exit(-1)` (PUSH -1; CALL 0x8e2f0). Same as sibling FUN_0011d110. Unreachable on valid input. See [[feedback_system_exit_vs_hcf]].
- Hazard WARN "duplicate cache args" (lines 145/164/332) is a FALSE POSITIVE — disasm literally PUSHes ESI twice for %s+%p. Faithful.

**lrar vs lruv struct note:** two DISTINCT cache structs share this .obj. lruv_cache (magic 'curl'=0x6c727563 @+0x44, asserts lruv_cache.c) vs lrar_cache (magic 'lrar'=0x6c726172 @+0x44, asserts lrar_cache.c, layout: +0x24/28/2c range, +0x30 block array, +0x34 head/+0x36 tail/+0x38 block_count shorts, +0x40 free_cb). Source correctly targets lrar layout. No cross-struct offset contamination.

**Path forward:** synthetic state-snapshot equiv (lift-synthetic-equivalence) — struct layout fully known from this decode, so build a valid ring: 'lrar'@+0x44, sane +0x24/28/2c range, +0x30→block array of 'klbR' records with block[0] non-NULL, head/tail/block_count consistent → drive unicorn_diff down the free loop + wraparound. OR a dual-oracle golden harness on a populated cache.
