---
name: objects-139c20-light-marker
description: FUN_00139c20 light-marker gather lift — eviction-slot single-var, decompiler tail iterator-corruption bug, 9-arg cdecl signature, parallel aliased output arrays
metadata:
  type: project
---

FUN_00139c20 (0x139c20 / object_lights.c) lifted into objects.c (Phase-1, ported=false) 2026-06-21.

Real 9-arg cdecl signature (kb decl + line-82 macro typing were partly wrong):
`void FUN_00139c20(int object_handle, int16_t marker_index, float *position, float bias, int out_index_base, float *out_weights, int out_atten_base, int16_t *count, int16_t max_count)`
- out_index_base and out_atten_base ALIAS the same caller buffer at +2/+0 words (caller FUN_0013a740 passes local_28+2 and local_28). Keep as separate `*(T*)(base + slot*4)` bases; do not normalize.
- out_weights is param_6 (the min-search reads it; store writes brightness*atten).

Two decompiler traps confirmed against disasm:
1. **Eviction slot is a single variable.** In the full-buffer branch the slot register (ECX) is XOR'd to 0, REUSED as the min-search loop counter, and ends == *count after the loop (CMP CX,SI;JL). It is only reassigned to argmin (EDI) on evict (when dimmest < new weight). When new light is dimmer than all, slot stays == count and the final `CMP CX,max_count;JGE` SKIPS the store. Lifting with a fresh 0-init slot var reintroduces a slot-0 clobber. **Why:** advisor-flagged; preserved as `slot = i;` after the search loop.
2. **Tail iterator corruption (decompiler bug).** At 0x139ddc the visited-mark re-fetches the object POINTER into ESI and writes [ESI+0xc], while EBX (the datum INDEX, the loop variable) is preserved for cluster_partition_iter_next. The Ghidra decompiler wrote `iVar5 = datum_get(...); *(iVar5+0xc)=...` overwriting the index — WRONG. Lift uses a separate `light` pointer var and keeps `light_index` intact. **How to apply:** any objects-iteration tail that re-datum_get's to mark a flag must NOT clobber the loop's datum index.

Atten = 1.0f - dist^2/radius^2 (radius=light+0x54); dist via sqrtf (disasm FSQRT on single-precision); brightness via real_rgb_color_brightness(light+0x14). float10 in decompiler is just x87 ST0-chaining (VC71 ceiling, not correctness) — plain float expression is faithful.
