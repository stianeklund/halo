# Handover

## Objective
Faithfully fix the cyan/turquoise **explosion & plasma hot-core** rendering bug introduced by the `rasterizer_transparent_geometry_group_draw` lift (0x174d10, commit ab13228d). NOT by deactivation — the user explicitly rejected `ported=false` as the final fix.

## Current State
- Root cause **not yet found**, but the A/B test resolved: **hypothesis B confirmed — the bug is intrinsic to env's own clang code.**
- **Only-env test result (user-confirmed):** with all non-env cases skipped, the explosion core is **still cyan** ("blue effect is back"). No non-env case ran, so nothing external corrupts env's state → env's clang code alone produces the cyan.
- All earlier debug instrumentation has been **removed** (color markers, `g_tgd_dbg`/`g_stg_dbg`/`g_anim_dbg` arrays + helpers, `g_type_hits`, env-skip `return`). Only **one** diagnostic line remains (`rasterizer.c:391`).
- Deployed build on the box: **rev `15fdc660`**, `0x174d10` **`ported=true`**, with the "only-env" diagnostic active.
- Bug visual confirmed from saved screenshots: `xbdm/screenshots/transparent_rasterizer/unpatched/shot-0009.png` (warm yellow-white core) vs `patched f2b1bb0a/shot-0009.png` (cyan core). Outer sparks/smoke are warm/correct in BOTH — only the bright core is R↔B swapped (orange 255,140,0 → cyan 0,140,255).

## Confirmed (tool/binary-backed this session)
- **Toggle re-verified on clean tree:** `0x174d10 ported=false` → warm core (user confirmed "looks fine/normal"); `ported=true` → cyan. Bug is in our clang-compiled 0x174d10 body.
- **Only-env isolation (user-confirmed):** diagnostic gate skipping all non-env cases still yields a cyan core. Rules out hypothesis A (non-env state/data corruption). The bug is in env's own clang code.
- **0x174d10 only READS the matrix/fog globals env uses** (`0x5a5c64…0x5a5c84`, `0x5a5bc8…0x5a5bd0`, `0x5a5bd4…0x5a5bdc`, `0x5a5c04/0x5a5c08`, `0x5a5e18`) — it does not write them; they are set by original (non-ported) functions and are therefore identical in the cyan and warm builds. Not the source.
- **The cyan core is drawn by the env case (case 1 = "shader_environment-style multitexture").** env-skip removed it; markers ruled out generic/chicago/glass/meter. It is a **z-sprite** (`env+0x5c==2`, so `has_multi=1`).
- **env is byte/semantically faithful in the shipped clang build**, verified against `delinked/rasterizer.obj` AND the clang object `build/CMakeFiles/halo.dir/src/halo/rasterizer/rasterizer.c.obj`:
  - Combiner immediates + reloc targets all match (`0x5a5ac0←0x18201415`, `0x5a5b48←0x8080000/0x8050000`, `0x5a5b50←0x250c0508`, `0x5a5b98←0x54421`, has_multi `0x1c190000`, blend-switch rows).
  - Calls/args match: `rasterizer_set_texture_bitmap_data(0,grp+0x5c)`, stage 0/1 `SetTextureStageStateSmart` ops, render states `0x7f/0x43/0x3b/0x3c` (0x43=0x10101 correct in clang), `FUN_001580b0(env+0x2a)`, `FUN_00178b40(0x41,…)`, `SetVertexShaderConstant(0x58/-0x51/-0x3f)`.
  - Fog/z-sprite float block is computationally equivalent (both `fchs` negations present, `fdivrp` same direction, opacity ternary via clang `fcmovne/fcmovu`).
- **All env callees run ORIGINAL code** — `rasterizer_set_texture*`, `rasterizer_set_pixel_shader`, `FUN_001906b0/00190e10/00178b40/001580b0` were only *renamed* in kb.json (decl only), not ported. So the bug cannot be in a callee (confirmed: no C impl in `src/`).
- **VC71:** `FUN_00174d10` 84.5% (3962/4034), only benign codegen diffs (push→mov for Smart helpers, jp→jne FPU-compare encodings, inline clamp expansion). `--show-diffs` reviewed; no wrong immediate/arg/missing call.
- Color packers (`FUN_000d1c90`, `FUN_000d1dd0`, `real_a_rgb_color_to_pixel32`/0x99530) all declared correct `uint32_t` return — no XCALL/return-register bug; env doesn't call them anyway.

## Important Changes
- `src/halo/rasterizer/rasterizer.c:391`: **DIAGNOSTIC (temporary)** — `if (*(short *)(sh + 0x24) != 1) goto next_pass; /* MK-ONLY-ENV … */`. Makes only env-typed groups draw, skipping all other cases. **Must be removed** before any commit.
- Instrumentation cleanup (markers/arrays) is done and correct — the file otherwise matches the intended clean lift.
- `kb.json`: scoped `git status` shows it currently **clean** vs HEAD (`0x174d10 ported=true`). The pre-existing unrelated deactivations noted in older memory are not present in the working tree now — re-verify before committing regardless.

## Validation
- `rtk python3 tools/build/build.py -q --target halo` — passes (hazard summary only).
- `rtk python3 tools/verify/vc71_verify.py src/halo/rasterizer/rasterizer.c --show-diffs --no-cache` — PASS 84.5%, diffs benign.
- `tools/xbox/build_deploy_run.sh -q` — deployed rev `15fdc660`, `verify: OK`.
- `check_xcall_types.py` / `check_lift_hazards.py` — no rasterizer.c color-path hazards (only benign duplicate-arg heuristics).
- **NOT run / pending:** the user's visual observation of the "only-env" build (the decisive A/B datum). Equivalence (`unicorn_diff`) on 0x174d10 not run.

## Uncertain / Risks
- **The core paradox (now sharper):** env's clang code alone renders the z-sprite core cyan (A ruled out), yet every semantically-important env instruction I inspected — combiner immediates+reloc targets, all calls+args, texture-stage ops, render states, vertex-shader index, skin_xform, texanim, fog float block — is byte/semantically faithful to the delinked original. The divergent instruction has NOT been located by spot-checking.
- Remaining candidates (all hypothesis B):
  - An env instruction not yet diffed exhaustively (need a COMPLETE instruction-level diff of the whole env case, clang vs original, not spot checks).
  - A clang-specific float→int / load-width / precision behavior in an env computation that changes an uploaded vertex-shader constant (skin_xform1 / texanim1 / fog_consts at vsh 0x58 / -0x51 / -0x3f) enough to alter the z-sprite's sampled color. Fog math verified equivalent; texanim rows 8-15 come from original `FUN_00190e10`; skin_xform is identity/memcpy — so this is not yet explained.
- Fog-tint mechanism (core fogged toward bluish atmosphere) is still plausible but unproven; needs runtime capture of the uploaded vsh constants at the z-sprite draw.
- Do NOT commit with the `MK-ONLY-ENV` diagnostic present (`rasterizer.c:391`).

## Next Steps
1. **Complete instruction-level diff of the entire env case (case 1)**, clang object `build/CMakeFiles/halo.dir/src/halo/rasterizer/rasterizer.c.obj` vs `delinked/rasterizer.obj` (env combiner is clang `0x1b22` / delinked `0x6e24`; env case spans roughly clang `0x18xx–0x1c80`, delinked `0x69xx–0x7090`). Normalize register allocation but flag ANY divergent memory operand, immediate, call target, FPU op form (fsub/fsubr, fdiv/fdivr), or float→int conversion (`_ftol2`/`cvt`). The bug is one such instruction.
2. In parallel, **runtime capture**: on the cyan build, capture the vertex-shader constants env uploads at the z-sprite draw — skin_xform1 (vsh `0x58`), texanim1 (`-0x51`), fog_consts (`-0x3f`) — and compare against the warm (`ported=false`) build. A divergent uploaded constant localizes the bad computation. Alternatively `unicorn_diff --state-snapshot` on 0x174d10 with a z-sprite-triggering snapshot.
3. Consider a targeted sub-bisect *within env* by neutralizing one uploaded constant at a time (e.g. force identity skin_xform1, or zero fog_consts) to see which one drives the cyan — reversible diagnostic, one build each.
4. Once located and fixed faithfully: **remove `src/halo/rasterizer/rasterizer.c:391` diagnostic**, rebuild+deploy, user-verify warm core, then commit via `tools/audit/generate_lift_commit.py` (kb.json stays `ported=true`).

## Resume Prompt
Resuming the cyan explosion/plasma **hot-core** bug in `rasterizer_transparent_geometry_group_draw` (0x174d10). Established: bug is in our clang build of 0x174d10 (`ported=false`=warm, `ported=true`=cyan); the core is the **env case z-sprite** (`env+0x5c==2`); and the **only-env isolation test confirmed hypothesis B — env's own clang code produces the cyan** (all non-env cases skipped, still cyan). env is byte/semantically faithful in every instruction spot-checked (combiner, calls, args, render/texture-stage state, vertex shader, skin_xform, texanim, fog), and all callees run original code, so the divergent instruction has NOT been found. Next: do a COMPLETE clang-vs-delinked instruction diff of the whole env case (clang combiner `0x1b22` / delinked `0x6e24`), and/or capture the vertex-shader constants env uploads at the z-sprite draw (vsh `0x58`/`-0x51`/`-0x3f`) vs the warm build. Deployed rev `15fdc660` still has a temporary diagnostic at `src/halo/rasterizer/rasterizer.c:391` (`MK-ONLY-ENV`) — REMOVE before committing. Faithful fix only, no deactivation. Full context: `docs/handover-rasterizer-tgd-cyan-core.md`; memory `project_rasterizer_tgd_cyan_core_bug.md`.

---

## RESOLVED (2026-07-04, commit 9f282f64)

Root cause: `has_multi` condition inversion in the env case. Original `0x175830`
(`cmp word [esi+0x5c],2` / `mov byte [ebp+0xb],1` / `jne` keeps the 1) means
`has_multi = (env+0x58 != -1) && (env+0x5c != 2)`; the lift had `== 2`.
Z-sprite cores therefore enabled the extra multitexture combiner stage
(0x1c190000/0xc090000) sampling the 3D fade volume as color -> cyan tint.
Found via deterministic a10 replay + gdb first-hit capture at the z-sprite
final-combiner store on both builds: original leaves texanim rows 2-3 zero and
no stage-1 combiner words. Fixed build now matches field-for-field.
MK-ONLY-ENV diagnostic removed; ported=true; fix deployed and verified.
