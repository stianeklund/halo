---
name: ai-los-416e0-ported-producers-faithful
description: 2026-06-23 exhaustive disasm audit of every PORTED callee of ai_test_line_of_sight (0x416e0) vs pristine cachebeta.xbe — all faithful; the actual occlusion deciders are UNPORTED; residual a10 passive-engage bug is an input/state divergence not a static mis-lift
metadata:
  type: project
---

# AI line-of-sight (0x416e0) ported producers — all FAITHFUL vs pristine

Audited 2026-06-23 to find the residual a10 "shoots but won't advance / intermittent LOS flicker"
bug. Conclusion: **NOT a static mis-lift in any ported producer.** Confirms prior memory
[[project_a10_passive_alertness_root_traced]] / a10_passive_alertness_root_traced.

## Authoritative per-tick LOS call graph (bounded 0x416e0..0x41ac4, capstone)
Direct CALL targets of ai_test_line_of_sight: `0x12f80, 0x13010, 0x130d0, 0x4b770, 0x8d9f0,
0x8e2f0, 0xfff80, 0x14df70, 0x18e690, 0x18e800`. (Earlier 0x900-byte window spilled into the
next fn and falsely added 0x41420/0x8dae0 — use the 0x41ac4 ret boundary.)

PORTED of those: 0x14df70, 0x130d0, 0x13010, 0x12f80, 0xfff80, 0x8d9f0, 0x8e2f0.
UNPORTED (original both builds): 0x18e690, 0x18e800 (scenario_ensure_point_within_world), 0x4b770 (ai_debug_lineofsight).

## AI flag set (caller 0x4177a..0x417e1)
Base `ebx=0xc2a3` (bits 0,1,5,7,9,14,15); campaign+global → `ebx=0x23` (bits 0,1,5). Then
`or 0x10`/`or 4` (surface type), `and 0xFFFFFDFF` (clear 0x200). Neither base nor those ops
ever set bit 0x40 (fog) or 0x100000 (zone-track). So inside 0x14df70:
- fog block (gated `flags&0x40`) = **provably skipped** for AI.
- zone-track refinement (gated `flags&0x100000`, the 5×-churned block) = **provably skipped** for AI.
Bit 0x20 set → BSP-fill gate passes; bit 0x80 set only in MP base 0xc2a3 → object loop runs in MP, not campaign.

## Producer audit results (all faithful)
- **0x14df70 raycast**: front/flag-normalize (edi = bl&0x1f), BSP-result fill (+0x14 dist, plane via ptr at local_buf+0x4, leaf lookup, +0x44/+0x48/+0x4c/+0x4e), obj-ref cluster block, object loop scaffold + FUN_0014dce0 call (arg order matches), final-position compute, `return result`(=al=[ebp-1]) — ALL disasm-verified faithful. The two churned/dangerous blocks are flag-excluded for AI.
- **0x130d0 (FUN_000130d0)**: thin wrapper, called 6× in 2nd LOS phase (0x418f7,0x41915,0x41941,0x419ac,0x419cd,0x419ee). Computes `direction=point_b-point_a` (correct order) and forwards to 0x14df70. Faithful.
- **0x12f80 vector3d_scale_add**: `out=base+scale*direction`. Faithful (sample-point offsetting in 2nd phase).
- **0xfff80 game_connection**: `return *(int16_t*)0x46da0c`. Faithful (selects flag base 0xc2a3 vs 0x23).
- **0x13010 normalize3d / 0x8d9f0 assert / 0x8e2f0 exit**: not blocked/clear producers; deterministic — can't cause an *intermittent* flip.

## The actual occlusion deciders are UNPORTED
The two functions that set result=1 ("blocked") inside 0x14df70 are both `ported:null` (original, byte-identical both builds):
- **0x149480 collision_bsp_test_vector** (BSP world raycast)
- **0x14dce0 FUN_0014dce0** (per-object collision test) — only a call site in collision_usage.c, no impl.
0x1417c0 (objects_reconnect_to_structure_bsp) is **NOT** in 0x416e0's call graph (capstone-confirmed) → off the per-tick LOS path.

## Next narrowing step (static lane is exhausted)
Divergence must be in INPUTS/STATE feeding the unported deciders: the origin/target/direction vectors
built upstream in src/halo/ai/actor_looking.c (~4983/6261/6304) and the object-table/BSP state that
0x149480/0x14dce0 read. Do a **runtime input/state diff** at the 0x416e0 call site (capture
origin/direction/target + actor/object state per tick, patched vs pristine) — not another static pass.
One residual unverified link: actor_looking.c's ray-input builder and the actor+0x9e consumer (brief
said prior sessions audited it; honored "do not re-derive" — that is the trusted-but-unre-verified surface).
