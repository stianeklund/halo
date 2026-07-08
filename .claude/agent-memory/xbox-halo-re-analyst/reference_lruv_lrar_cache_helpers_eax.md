---
name: lruv-lrar-cache-helpers-eax
description: lruv_cache.obj guard helpers FUN_0011c820/FUN_0011c7c0 take cache in @eax; TU also hosts a distinct lrar_cache struct
metadata:
  type: reference
---

lruv_cache.obj (src/halo/memory/lruv_cache.c) mixes two cache families that share the TU:

- **lruv_cache_t** (0x44 header): 0x30=field_30(int frame ctr), 0x34/0x38=first/last_block_index, 0x3c=data_t*, stride 0x1c, sig 0x77656565. Asserts vs `lruv_cache.c`.
- **lrar_cache** (distinct): cache+0x30 = raw block-array base ptr, cache+0x38 = int16 block_count, block entry stride = 0x10, data pointer at entry+8. Asserts vs `c:\halo\SOURCE\memory\lrar_cache.c`. Do NOT reuse lruv_cache_t for lrar accessors — use raw offset access. FUN_0011cf00 (block getter) is lrar.

**Guard helpers take cache in @eax (decompiler/prefetch report them as void(void) — WRONG):**
- FUN_0011c820 (lruv_update_function_pointers): `void f(int cache@<eax>)` — entry "refresh function pointers".
- FUN_0011c7c0: `void f(int cache@<eax>)` — matching pre-return helper.

Both accessors do `mov eax, edi` (edi=cache) immediately before each `call`; EAX return is never consumed, so EAX is purely an inbound arg. Calling them arg-less compiles/asserts fine but is a silent reg-arg bug and costs ~3 insns of VC71 (84.7% → 100.0% on FUN_0011cf00 once cache is passed). These helpers are shared by every lruv/lrar accessor in the TU — any future accessor lift must pass cache to them.

**Lifted accessors so far:** FUN_0011cf00 (block getter, entry+8) and FUN_0011cf60 (block free/clear, entry+0 = ptr, calls free_cb at cache+0x40 then nulls slot; note it calls lruv_update_function_pointers TWICE on entry). FUN_0011cf60 is plain cdecl `void f(int cache, short block_index)` — it does NOT take @eax itself; only its helpers do. Placed in lruv_cache.c beside FUN_0011cf00 (NOT a separate lrar_cache.c). VC71 87.5% (per-fn ref delinked/functions/0011cf60.obj; whole-object ref truncates → falsely low); permuter can't beat it; equivalence 100/100 high-conf. Residual gap = call-through-ptr scheduling + doubled entry call.

**Second @eax guard helper — FUN_0011d090** (distinct from 0x11c820/0x11c7c0): `void FUN_0011d090(int cache@<eax>)`, the fn-ptr refresh called on entry by the 0x11d2a0..0x11d4xx accessor cluster. Original passes cache via `mov eax,esi; call`. Every ported caller of it is VC71-capped in the ~77-85% band because the VC71 verify harness declares it stack-arg (push+add esp) not @eax: FUN_0011d2a0=76.9%, FUN_0011d3f0=85.0%, FUN_0011d110=85.1%, FUN_0011d300=83.3% (13/11 insns, the +2 = push/add-esp vs mov-eax). This is a caller-of-@reg-keystone structural cap, NOT a source bug — field reads match exactly; equivalence 100/100 confirms. Don't churn these.

Also: prefetch delinked ref for the 0x11cf00 gap functions (0x11c7c0..0x11cf60 region) was missing from delinked/lruv_cache.obj; per-function re-export (range 11cf00-11cf5f → delinked/functions/0011cf00.obj) was needed for VC71 scoring.
