---
name: bsp3d-146be0-sub90-guardonly-equiv-needsruntime
description: FUN_00146be0 bsp3d.obj 88.4% = NEEDS_RUNTIME; full static clear but zero-fill equiv only hit guard path, live snapshot absent
metadata:
  type: project
---

FUN_00146be0 (0x146be0, bsp3d.obj, breakable_surfaces_area_of_effect_damage) 88.4% VC71 = NEEDS_RUNTIME.

FIRST bsp3d.obj review. Body was STATICALLY CLEARED 1:1 vs disasm (no bug found), yet held at NEEDS_RUNTIME on the band policy.

**Why NEEDS_RUNTIME not AUTO_ACCEPT:** <90% band requires golden/runtime behavioral verification of the actual body. The equiv that RAN was zero-fill only, coverage 16.1% (weak). `char *breakable_surface_globals` (HDATA) zero-filled → the enabled-flag guard (`MOV AL,[EDX]; TEST AL,AL; JZ`) fails immediately, so all 100 seeds took the early-exit. The AoE loop, distance test, bitfield clear, and `FUN_00145ad0` spawn had ZERO runtime coverage. Stub-arg differential's "200 calls / 0 mismatches" covers only `scenario_get`+`tag_get` (both BEFORE the guard) — none of the loop-body callees ever ran. The acceptance path narrated a "live-state infection_swarm snapshot = accepted runtime evidence" lane, but `artifacts/snapshots/infection_swarm.json` is ABSENT — that lane never executed. A 100/100 pass on a guard-only path is vacuous for the body.

**Why not REJECT:** full static 1:1 audit found no concrete defect. ABI clean (cdecl regs=none), hazard scan clean (exit 0). All verified:
- Short-circuit order matches: `*breakable_surface_globals!=0` checked FIRST, then guard-float OR (`+0x1d4!=0 || +0x1d8!=0`), matching C && left-to-right.
- 3 FPU polarities correct: two guard `!=0.0f` (TEST AH,0x44 JP/JNP idiom), `4.0f < effect_radius` error gate (TEST AH,0x41 JNZ), and dist test `dx²+dy²+dz² <= (surf[3]+effect_radius)²` — FCOMPP(thr²,dist²) C0/TEST AH,0x1/JNZ decoded stack-by-stack, polarity CORRECT.
- All call-site arg orders verified: tag_get(0x6a707421,*damage_params); error(2,str,double); tag_block_get_element(block, surface_index, 0x30); FUN_00145ad0(i, damage_params, surf[0x10]).
- Dual-index preserve-shape FAITHFUL: raw int EBX for breakable_surface_extant/get + FUN_00145ad0; sign-extended short EDI (MOVSX EDI,BX) for tag_block_get_element index AND bitfield (idx>>5*4 via SAR, idx&0x1f). Source models with `i` (int) vs `surface_index=(short)i`. Diverges only if count>32767 (never in practice) — lift preserves it anyway.
- Offsets: guards +0x1d4/+0x1d8, effect_radius +0x4, surf +0/+4/+8/+0xc/+0x10, damage_params +0x28/+0x2c/+0x30, count/block scenario+0x16c re-read each iter. All match.

**Discriminator (consistent with prior sub-90 rulings):** sub-90 clears on the RUNTIME lane ONLY when a PASSING equiv on a REAL/live snapshot GENUINELY exercised the body (datum lookups succeeded, real branches hit) — cf. [[project_actions_0x1d530_sub90_statesnapshot_clear]], [[project_actions_allow_cover_seeking_sub90_runtime_lane]]. A static-verified-faithful body with NO body-covering runtime = NEEDS_RUNTIME — cf. [[project_actions_consider_grenade_88band_needsruntime]] (this is the same shape). To clear: capture a live snapshot where scenario has breakable surfaces + populated damage_params so the AoE loop/distance/bitfield/spawn run at 0-divergence.
