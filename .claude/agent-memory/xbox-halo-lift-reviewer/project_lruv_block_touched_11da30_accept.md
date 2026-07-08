---
name: lruv-block-touched-11da30-accept
description: lruv_block_touched 0x11da30 100% VC71 = AUTO_ACCEPT; char[8] field decay is single-load not double-deref
metadata:
  type: project
---

lruv_block_touched (0x11da30, lruv_cache.obj) 100% VC71 = AUTO_ACCEPT. cdecl bool(void*,int), reg_args=none.

Full 1:1 decode: verify(lruv,0)@0x11d550 + datum_get(blocks@+0x3c,idx)@0x119320 both cdecl arg-verified; body `*(int*)block->unk_14 == c->field_30` compiles to `MOV EAX,[EAX+0x14]; SUB EAX,[ESI+0x30]; NEG/SBB/INC` (standard `==`→bool idiom). Semantics: block+0x14 stamp == cache+0x30 generation → touched this cycle. Sibling of lruv_block_get_address 0x11da00, lruv_block_stamp (writes field_30 to block+0x14).

**Why:** `*(int *)block->unk_14` looks like a double-deref bug (deref field value as pointer) but `unk_14` is `char unk_14[8]` at offset 0x14 (struct line 211) → array decays to `&block->unk_14` = block+0x14, so `*(int*)block->unk_14` = single dword load. 100% VC71 confirms single MOV (a real double-deref would add an instruction and break 100%).

**How to apply:** For lruv_cache.obj sibling one-liners, do NOT flag `*(int*)block->unk_N` as pointer-as-value/double-deref when unk_N is a char[] array member — it's address-of decay = single load. Related [[project_lruv_cache_11d110_stateless_allocator_sub90_accept]]. Note: hazard scan `--changed-only` WARNs csprintf dup-arg at lruv_cache.c:252 (inside lruv_cache_verify) — that is pre-existing code OUTSIDE the added function (640-658), not introduced by this lift, non-blocking for this target.
