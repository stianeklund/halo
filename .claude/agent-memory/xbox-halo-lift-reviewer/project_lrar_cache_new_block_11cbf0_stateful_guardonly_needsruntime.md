---
name: lrar-cache-new-block-11cbf0-stateful-guardonly-needsruntime
description: lruv_cache.obj FUN_0011cbf0 lrar_cache_new_block 83.8% VC71 = NEEDS_RUNTIME. Full static body 1:1 clean (zero defects) but sub-90 stateful allocator core (ring-walk/eviction/new-block-write) DARK under zero-fill equiv (37.9% cov, moderate). Third lruv/lrar sibling confirming stateless-accept vs stateful-dark split. Task header mislabeled score as 100%.
metadata:
  type: project
---

FUN_0011cbf0 (0x11cbf0, lruv_cache.obj) = `lrar_cache_new_block` (allocate a block in the lrar ring),
cdecl `short(int cache, int size, void *data)`. **83.8% VC71** (283 cand / 254 ref insns).
Verdict = **NEEDS_RUNTIME** (not REJECT — no concrete bug; not AUTO_ACCEPT — sub-90 stateful core dark).

**SCORE MISLABEL:** task header said "Structural match 100%" — FALSE. `vc71_verify --function FUN_0011cbf0
--no-cache` = 83.8%. The acceptance-path body-text (83.8) was correct; the header was wrong. Always re-run
VC71 when header % and acceptance-path % disagree — trust the tool.

**Full static decode 0x11cbf0-0x11cef7 = ZERO defects.** Verified 1:1 vs disasm:
- entry MOV EAX,EDI;OR ESI,-1;CALL 0x11c820 = lruv_update_function_pointers(cache@<eax>), new_block_index(ESI)=-1.
- align: MOV CL,[EDI+0x20];SHL 1,CL;DEC;TEST size;OR;INC = round size up to primary alignment.
- range guard: size<0 (JL) or size>[+0x2c] (JG) → return -1 (0x11ceef). Matches `size>=0 && size<=[+0x2c]`.
- @reg call FUN_0011ca60 BOTH sites correct: 0x11cc72 MOV AX,[EDI+0x36](tail); 0x11ccbe MOV EAX,EBX(block_index);
  EDI=cache throughout → `FUN_0011ca60(block_index@<ax>, cache@<edi>)` per kb decl. int ret used as int* (ptr).
- search_addr = block[3]+block[2] (MOV [EAX+0xc];ADD [EAX+0x8]). secondary boundary align at [+0x22] (-1=none).
- eviction inner-loop: free cb 1-arg cdecl `PUSH block[0];CALL [EDI+0x40]`; block[0]=0; block[1]=-1; ring INC+wrap.
- 4 assert paths (0x199 header 'lrar'@+0x44, 0x16e index, 0x186 'klbR'@+4 record, 0x111 addr-bounds, 0x11c
  overlap) all polarities correct (signed vs unsigned JC/JBE/JL/JGE). All use `system_exit(-1)` (PUSH -1;CALL 0x8e2f0).
- fit-retry: new_block_addr+size <= [+0x28] (JBE) → break/write, else [+0x36]=-1; loop (0x11cdfb).
- new-block write 0x11ceb5: [ESI+0xc]=size, [ESI+4]=0x52626c6b 'klbR', [ESI+8]=addr, [ESI]=data (ESI=idx*0x10+[+0x30]).
- **insert cb arg order VERIFIED**: 0x11ced0 `PUSH ESI(new_block_index); PUSH EAX(data); CALL [EDI+0x3c]` =
  cdecl `(data, new_block_index)` — data first, index second. Source line 271 matches EXACTLY. Not swapped.
- head/tail link [+0x36]=idx, [+0x34]=next_index(or idx if -1). return AX=SI=new_block_index.

**Why NEEDS_RUNTIME (the blocking call):** 83.8% < 90%. Band policy = golden/runtime BODY verification required.
Equiv that ran is **zero-fill, 37.9% coverage, moderate confidence** (140 seeds pass, 0 diverge, 0 stub-arg
mismatch, +40 concolic). NOT a live-state snapshot. This allocator is **STATEFUL** — its real work (walking a
populated 'lrar' block ring via FUN_0011ca60, evicting OVERLAPPING blocks through the free cb, multi-block
fit-retry with a full cache, the final overlap-validation sweep) is UNREACHABLE without a populated ring buffer.
0 stub-arg mismatches only proves the paths that RAN are fine; the substantive eviction/overlap body is dark.
Same bsp3d-88.4% / dispose-87% guard-only pattern.

**KEY discriminator = stateless-accept vs stateful-dark split (now 3 siblings):**
- lruv_cache_new 0x11d110 85.1% AUTO_ACCEPT — genuinely STATELESS → zero-fill+stubs IS complete lane (74.9% cov HIGH).
- lrar_cache_dispose 0x11cab0 87% NEEDS_RUNTIME — STATEFUL, free-loop dark (27.1% guard-only).
- **lrar_cache_new_block 0x11cbf0 83.8% NEEDS_RUNTIME — STATEFUL, ring-walk/eviction/write dark (37.9%).**
See [[project_lruv_cache_11d110_stateless_allocator_sub90_accept]],
[[project_lrar_cache_dispose_11cab0_stateful_guardonly_needsruntime]], [[project_bsp3d_146be0_sub90_guardonly_equiv_needsruntime]].

**Discount contradictory boilerplate AGAIN:** acceptance path says "State-snapshot equivalence was NOT run
(score below [85,89] band)" then claims "a 0-divergence pass on the live infection_swarm snapshot IS accepted
runtime evidence." Self-contradictory copy-paste — the live pass did NOT happen; infection_swarm has no 'lrar'
cache. Same as both siblings. Do not credit it.

**Non-blocking nits:** hazard WARN duplicate cache args (lines 202/222/426) = csprintf %s+%p both PUSH EDI(cache)
twice — FALSE POSITIVE, faithful. Mismatch class (16.2% gap, cand 283 > ref 254) = display_assert/csprintf
assert-macro LCS desync + x87-free regalloc/scheduling — NOT a body defect.

**Path forward:** synthetic state-snapshot equiv (lift-synthetic-equivalence) — layout fully known: build a
valid 'lrar' ring ('lrar'@+0x44, sane +0x24/28/2c/30 array of 'klbR' records with non-NULL block[0], consistent
head[+0x34]/tail[+0x36]/count[+0x38]) → drive unicorn_diff down the eviction + fit-retry + write paths. OR
dual-oracle golden on a populated cache.
