# Handover — hud.obj goal-lift

## Objective
Port all remaining `hud.obj` functions (`src/halo/interface/hud.c`) at ≥88% VC71 match OR ≥90% equivalence.

## Current State (after session 2026-06-12b)
- **hud.obj now 33/36 ported.** Working tree clean except gitignored delinked/ objs + pre-existing untracked `.vc71_regcall_*` / agent-memory files.
- Branch `biped-2f40-relift-consolidate`. **6 function commits this session**: d1400 (NEW 88.3%), d16a0 (87.0→88.9%), d0e90 (faithfulness fix 80.6%), **d1890 (NEW 89.4% — cleared bar)**, **d2580 (NEW 74.6% faithful)**, **d1f40 (NEW 78.0% faithful)**.
- Full per-function status + blockers + recipes in `artifacts/auto_lift/goal_progress.md`. Continuity memory: `project_hud_obj_goal_lift`.
- **3 remain**: **d1090** (unported, hardest — variadic sprintf calls w/ Ghidra-lost args + CONCAT22 + float10; callee decls partly resolved in goal_progress.md, incl. a 0x8df60 csstrlen-vs-sprintf CONFLICT to settle from disasm), **d27a0** (unported, largest ~2192B; cursor/icon FSIN/FCOS rotation, kb decl `void*param_6` is really `float angle`), **d1540** (frameless ret-addr helper — stays `ported=false`, NOT portable; exclude from the bar math).
- **KEY LESSON this session**: the prior "register-arg/FPU draws are ~73-82% capped" claim was FALSE — d1890 (register args + FPU) cleared 89.4% with a faithful lift. The real VC71 cap = count of `_ftol` (`(int)float`) + x87-wrapper calls (sin/cos/fmod) vs the original's inline FISTP/FSIN. Always attempt the faithful lift first; only conclude "capped" after measuring.

## Confirmed (this session)
- **Integer branch-inversion lever** (the cheap win): when vc71 --show-diffs shows a single `jne`↔`je` at an integer compare with high insns-matched, the upstream branch has the wrong fall-through arm and desyncs the whole LCS. Read disasm for the not-taken arm, write the C `if` so that arm is first (invert + swap blocks verbatim, zero behavior risk). **Validated twice**: d1400 87.3→88.3, d16a0 87.0→88.9. See `feedback_integer_branch_inversion_realigns_lcs`.
- **vc71_verify --show-diffs convention**: `-` = candidate (cl.exe, `_FUN` underscore-mangled); `+` = reference (delinked, bare `FUN`). The underscore is the tell. Reference always matches live Ghidra disasm.
- d0e90 had an unfaithful `+0.5f` on its `(int)screen_xy[N]` conversions — original is bare `FLD;FISTP` (no fadds). Removed; now faithful.

## Important Changes
- `src/halo/interface/hud.c`: +d1400 (lines ~195-251); d16a0 output-param block arms swapped; d0e90 `(int)` conversions de-+0.5f'd.
- `kb.json`: d1400→`void(void)` ported; director_get_perspective(0x86410)→`int16_t(int16_t)`; FUN_000dabf0(0xdabf0)→`void(int)`.
- `objdiff.json`: registered `halo/hud_d1400_d1531` unit.
- `delinked/hud_d1400_d1531.obj` (new), `delinked/hud_d16a0_d1882.obj` (re-exported fresh) — both gitignored.

## Validation
- VC71 per-function via `vc71_verify.py --no-cache` (rm `artifacts/verify_cache/vc71.sqlite` first after obj edits). d1400 88.3%, d16a0 88.9%, d0e90 80.6%.
- Hazard scan `--changed-only` clean on touched code (one pre-existing line-725 matrix_transform_point WARN in an unrelated function).
- d2320 equivalence RE-CONFIRMED blocked: only divergence is the d1c90 stub-asymmetry (oracle EAX=0 vs candidate EAX=ptr). `--real-callees` made it WORSE (ESP_delta=-644 — needs full-memory snapshot, not arg-overrides).

## Uncertain / Risks — WHY the remaining 8 are blocked
- **VC71 caps (cannot reach 88%)**: FPU draws (d2320, d0e90, d0c80) capped by cl.exe NOT inlining the `x87_math.h` static asm wrappers (the shipped clang build DOES inline them — objdump-confirmed faithful); all round-to-int capped by `_ftol`-vs-`FISTP` (no /QIfist in harness); d04d0 by MSVC switch jump-table layout; register-arg draws (d1890/d2580/d27a0 @<eax>/@<edi>/@<bl>/@<ecx>) by frame preamble.
- **Equivalence caps (cannot reach 90% cleanly)**: harness stubs callees asymmetrically (d2320's `return FUN_000d1c90(...)`: oracle stub→EAX=0, candidate→EAX=arg-ptr); `--mem-trace` skips STACK writes so it can't compare stack out-buffers; synthetic `arg_overrides` snapshots have empty `regions` so `--real-callees` corrupts the stack. d0e90/d04d0 additionally need live game state (biped/camera/object globals e.g. 0x5aa6d4).
- **The unblock requires harness engineering** (a stack-output-buffer compare mode, or symmetric stub returns) OR a verified live full-memory xemu snapshot (finicky on this dev box per CLAUDE.md). Both are fresh-session work, not a lift.
- DO NOT trust the subagent-advisor claim that `FUN_000d1a70` is `void(void*)` — committed d1a70 (`char(int @<eax>, int)`, 92.7%) is correct.
- d1f40/d2580/d27a0/d1890 first-pass agent analyses were unreliable (two agents crashed) — re-analyze from disasm, don't trust drafts.

## Next Steps (fresh session, full context budget)
1. **Harness: add a stack-output-buffer comparison mode** to `unicorn_diff.py` (compare the N-byte buffer a pointer-arg points at, at the RET point, instead of/in addition to EAX). This single change unblocks the equivalence lane for d2320 (compare out_color[4]) and the whole FPU-draw family. See `goal_progress.md` §Synthetic-snapshot for the exact d2320 blocker + 3 fix options.
2. With that lane working, equivalence-verify d2320, d0e90, d04d0 to clear the bar (lifts already verified faithful — 0 real arg bugs).
3. Port the 5 unported FPU draws (d1890 smallest ~447B; d2580, d1090, d1f40, d27a0 larger) — faithful disasm lifts, accept VC71 cap, clear via the equivalence lane. Apply the branch-inversion lever wherever an integer `jne`↔`je` appears.
4. d1540 stays `ported=false` (frameless ret-addr-capture helper; account for it explicitly in the Stop-hook math — it is NOT a portable function).

## Resume Prompt
Continue the hud.obj goal-lift on branch biped-2f40-relift-consolidate. hud.obj 30/36; 3 cleared this session (d1400 88.3, d16a0 88.9 via the integer branch-inversion lever, d0e90 faithfulness fix). The remaining 8 (5 unported FPU draws + d2320/d0e90/d04d0 below-bar + d1540 frameless) are tooling-blocked: VC71 can't measure x87-wrapper/fistp/switch-table caps, and the equivalence harness stubs callees asymmetrically + skips stack out-buffers. FIRST add a stack-output-buffer comparison mode to unicorn_diff.py (unblocks the FPU-draw equivalence lane), THEN equivalence-verify d2320/d0e90/d04d0 and lift the 5 unported via that lane. See `artifacts/auto_lift/goal_progress.md` for per-function disasm notes, the diff-convention, the branch-inversion lever, and the exact d2320 equiv blocker. Use `/lift` discipline (C89, §10 arg-grouping from `ADD ESP,N`, x87_math.h wrappers, faithful binary evidence only). Do NOT change committed d1a70's decl.
