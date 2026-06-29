---
name: nested-call-interleave-recovery
description: Recover +12-15pp VC71 on small getters that call object_get_root_parent/object_get_and_verify_type by writing a SINGLE nested cdecl expression instead of two statements
metadata:
  type: feedback
---

Small objects.obj getters that do `root = object_get_root_parent(h); obj = object_get_and_verify_type(root, -1);` cap at ~82% VC71 as two statements, because MSVC pre-pushes the `-1` type_mask BEFORE evaluating the inner getter (right-to-left cdecl arg eval, interleaved push scheduling). Two C statements force the `-1` push LATE → 2-3 mismatched insns on a tiny denominator = big % hit.

**Fix:** collapse to one nested expression:
```c
obj = (object_data_t *)object_get_and_verify_type(
        object_get_root_parent(object_handle), -1);
```
This reproduces `push -1; [push h; call root_parent; add esp 4]; push eax; call verify; add esp 8`.

**Why:** matches the original's exact expression shape, not just equivalent logic. **How to apply:** any time a diff shows `pushl $-0x1` ordered differently (reference pushes it first) around a `verify(getter(...), mask)` pattern. Proven 2026-06-21: object_get_location (0x140130) 82.4%→97.1%; object_pvs_get_cluster_index (0x13dcc0) 78.8%→81.2% (helps but multi-call bodies still cap). Same lever recovered object_header_block_reference_get (0x13dfc0) 85.7%→97.9% by reading reference fields INLINE (no cached int16_t locals) + `<= 0` not `< 1` for the `>0` assert + `(short)header->data_size` to force MOVSX.

Related: [[reg-arg-keystone-vc71-ceiling]], [[eax-defining-fn-vc71-ceiling]].

**Residual small-function ceiling (do NOT churn):** the MSVC callee-saved-register sentinel idiom — original does `OR ESI,-1` / `MOV CL,1` early into a saved reg that survives the loop, then `MOV AX,SI` at the miss-return. Seeding a C `result=-1` local does NOT reliably get VC71 to pick a callee-saved reg (it uses AX → `orw $0xffff,%ax`). Costs 2-3 insns on tiny functions (0x135f20 82.4%, 0x13c9e0 88.9%, 0x13c490 84.0%). Not fixable from clean C; accept at ceiling.
