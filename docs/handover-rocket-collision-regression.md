# Handover: Rocket Collision Regression (FUN_0014df70)

## Problem
Rockets and grenades sometimes don't explode when hitting inclined surfaces — the projectile disappears without detonation effects. Does NOT occur with the unpatched original game.

## Root Cause
Toggle-bisect confirmed: `FUN_0014df70` (collision raycast, `src/halo/physics/collision_usage.c`). Deactivating it (running original code) eliminates the bug entirely.

## Fixes Applied (committed)
1. **Removed both non-original defensive guards** (~lines 999-1017 and 1181-1187) that zeroed `collision_result+4/+8/+0xc/+0x10` when `+0xc & 0x7FFFFFFF == 0x7FFFFFFF`. These destroyed valid BSP cluster refs needed for detonation — the main rocket bug.
2. **Added missing `|dist| < |dir_dot|` fog condition** with double-precision comparison matching original x87 `FCOMP qword ptr [0x2533d0]`.
3. **Replaced guards with surgical cluster clamp** — only sets `collision_result+0xc = 0` when `scenario_location_from_point` returns -1.
4. **Added cluster_index validity check** in `decals.c` at `FUN_0009c4b0` call site (prevents decal assert).

## RESOLVED 2026-06-22 (zone-track block restored faithfully)
The remaining intermittent failure was the `collision_flags & 0x100000` zone-track tail of `FUN_0014df70`. The prior lift of it had three bugs: refine constant `1/512` (`0x3a000000`) instead of `1/4096` (`0x39800000`); a **swapped `vector3d_scale_add` base/dir** (base=normal, dir=position) that overwrote `col_result+0x18` with the garbage point `normal + position/512` → wrong cluster/position → suppressed detonation/effects; and a dead-break top-tested loop (the PoA a10 freeze). The block was then removed wholesale in `59ac3322`.

Restored from an instruction-anchored disassembly of `cachebeta.xbe` (`0x14e500-0x14e631`): in-place `vector3d_scale_add(+0x18, +0x24 normal, 1/4096, +0x18)`; `step = (1/4096)/|dot(direction,normal)|` with a `1/32` parallel-ray fallback; the original's `do/while` with the **live `frac <= 0` terminator** checked after `scenario_location_from_point` (clamp keeps `frac >= 0` so it always terminates). Only refines `+0xc/+0x14/+0x18`; gated on `0x100000` so the render caller (`0xfff80`) never enters it. BSP hit classification (`col_result[0]=2`, material via `scenario+0xa4` leaf lookup, plane normal + `plane_negate`) verified byte-faithful and unchanged.

Evidence: VC71 `FUN_0014df70` 85.5% (was 77%); deployed impl `0x4af186` has the `1/4096` constant; hazard-clean. Box-confirmed (preliminary): grenade/rocket detonation OK, no new freeze. Commit: `fix(collision): restore faithful 0x100000 zone-track block in FUN_0014df70`.

If a residual intermittent failure ever recurs, the next untested suspect (below) is the object-iteration section.

## Original "Remaining Intermittent Issue" note (pre-2026-06-22, kept for history)
After all fixes above, rockets STILL sometimes fail to detonate on inclines, but it's harder to reproduce. The main detonation bug is fixed; this is a separate subtle issue.

### Bisect Status (sections of FUN_0014df70)
| Section | Tested? | Result |
|---------|---------|--------|
| Fog/atmosphere (lines ~1004-1067) | YES (disabled) | Bug persisted — NOT the cause |
| Zone tracking (lines ~1137-1179) | YES (disabled) | Bug persisted — NOT the cause |
| Object iteration (lines ~1069-1121) | **NOT YET TESTED** | Next to test |
| BSP extraction offsets | Verified by RE analyst | Confirmed correct |
| `collision_bsp_test_vector` args | Verified by RE analyst | All 8 args confirmed correct |

### Key Facts from RE Analysis
- BSP hit extraction offsets at local_buf+0x0..+0x18 are correct
- Plane pointer dereference at local_buf+0x4 is correct (pointer, not inline)
- obj_ref_last formula `local_buf + count*4 + 0x14` is correct
- `use_water = (collision_flags >> 7) & 1` is correct
- The original has a dormant `TEST BL,0x3; JNZ; OR EBX,0x3` pre-check we're missing (but never triggers for known callers)

### Likely Next Steps
1. Disable object iteration section and test
2. If still occurs, the issue may be in FUN_000f8720 (swept-sphere wrapper) or FUN_000f90d0 (collision response) — both ported
3. Consider adding debug instrumentation to log collision_result[0] and collision_result[0x1a] when a rocket collision doesn't produce det_result=4

### Detonation Flow Reference
- `FUN_000f8720` (swept-sphere, 3 casts) → `FUN_0014df70` (raycast)
- `FUN_000f90d0` reads `collision_result[0]` (hit type: 0=fog, 2=BSP, 3=object) and `collision_result[0x1a]` (material index at byte 0x34) to select detonation result (0-4, where 4=detonate)
- If fog overrides BSP hit: `collision_result[0]` becomes 0, material index changes → detonation decision changes
- Projectile code at lines ~2518-2552 of projectiles.c

### Build Notes
- 4 unimplemented units.obj functions deactivated (0x136b40, 0x137540, 0x1a0db0, 0x1a6350) — from incomplete worktree merge
- `FUN_000fad00` return type fixed (void → int16_t)
- Reg baseline updated for worktree-merged @<reg> functions
