# Handover — 0xA0001 band-aid removed, rogue self-hosted-runner deploys, 0x1b0630 partial re-lift

## TL;DR
- **Part 1 (0xA0001 biped surface-index crash): root cause was ALREADY fixed by commit `26bb162a`** (ancestor of HEAD). The reader-side band-aid in `bipeds.c` was **vestigial** and has been **removed** (faithful — the original reader does NO validation). Statically proven; NOT cleanly runtime-validated (see "Rogue runner" — the box was being clobbered by uncontrolled deploys).
- **A self-hosted GitHub Actions runner is auto-deploying TEST_HARNESS builds to the shared Xbox on every push to `main`.** This is "something is enabling the harness / deploying builds I'm not controlling." It also leaves a stale `TEST_HARNESS=1 shell_xbox.o` in the shared `build/`. Must be stabilized before any runtime validation is trustworthy.
- **0x1b0630 euler-aim**: disasm-grounded block-fix APPLIED to `units.c` (correct) but the function needs a FULL re-lift; kept **dormant** (`ported=false`).
- Multiple agents active concurrently in `/mnt/g/dev/halo`: 2 other `claude` sessions + `opencode` + the runner. Committing is hazardous (could stage their work).

---

## PART 1 — 0xA0001 (RESOLVED in code, faithful)
Assert `tag_groups.c:3089` "#idx not valid in [0, surface_count)" — `0xA0001` (datum handle) used as a raw BSP surface index.

**Analyst (Ghidra) findings, all disasm-confirmed:**
- `unit+0x430/+0x434/+0x448` are **raw BSP surface indices** (sentinel -1); `0xA0001` is illegitimate.
- The ORIGINAL reader `biped_find_pathfinding_surface_index` (0x1a1bc0) does **zero validation** (only `== -1` check; disasm 0x1a1c51/0x1a1c9f). So our clamp was **not byte-faithful**.
- Both writers `FUN_001a25e0` (0x1a25e0) and `FUN_001a1a10` (0x1a1a10) are `ported=false` (run ORIGINAL); `FUN_001a25e0`'s lift is byte-faithful anyway (raw index only).
- Creation init `biped_new` (0x1a4990, original) sets 0x430/0x434 = -1 correctly.
- **Real root cause = commit `26bb162a`** (ancestor of HEAD): `FUN_001a2f40` (ported) was copying `.flags` (entry+0x28, datum-handle-shaped) into physics+0xa4 instead of `.surface_handle` (entry+0x24); `FUN_001a5300` (original) copies physics+0xa4 → biped+0x430. Fix restored `.surface_handle`; current source `bipeds.c:3593/3599` matches.

**Action taken:** deleted the DEFENSIVE clamp block in `bipeds.c` (was ~lines 1767-1776: `if ((unsigned)unit_obj[0x10c] >= *(int*)(bsp+0x3c)) unit_obj[0x10c]=-1;` + same for `[0x112]` + comment). The reader now goes straight from `unit_obj[0x111] = game_time;` to `if (unit_obj[0x10c] != -1) {` — byte-faithful. Hazard-clean.

**Runtime status:** the specific `0xA0001` / `tag_groups.c:3089` assert did NOT recur in any observed boot. NOT cleanly validated on a10 in-level AI pathfinding because the box kept being clobbered by the rogue runner (below). A cryo-intro HANG was observed on an UNCONTROLLED rogue build (`8157d5740b`@22:43) — NOT the 0xA0001 assert, no biped pathfinding in the cryo intro, so band-aid removal is not implicated.

---

## ROGUE RUNNER — the "uncontrolled deploys / harness" cause
- **Self-hosted GitHub Actions runner** on this machine: `~/actions-runner/run...` (PIDs ~271 runsvc.sh, ~370 Runner.Listener).
- `.github/workflows/runtime-oracle.yml`: `runs-on: self-hosted`, triggers on **push to main** touching `src/**` or `kb.json` (also 4am cron + dispatch). It symlinks the SHARED `/mnt/g/dev/halo/halo-patched` into the workspace and runs `tools/verify/runtime_oracle_batch.py` → `tools/verify/run_golden_tests.py` → `deploy_xbox.py`.
- `run_golden_tests.py:50` configures `build/` with `-DHALO_TEST_HARNESS=ON`, builds (so `shell_xbox.o` gets `TEST_HARNESS=1`), runs golden tests, then `:74` reconfigures `OFF` — but the restore is **configure-only, no rebuild**, so it LEAVES a stale harness-ON `shell_xbox.o` in the shared `build/`.
- Net effect: any push to main by the other sessions → runner builds a harness-ON XBE and deploys it to the live box, AND poisons the shared build dir. With `TEST_HARNESS` on, `shell_xbox.c:79-85` runs `run_tests(); for(;;){}` → never `main_loop()` → a10 never loads. The earlier `Permission denied: halo-patched/default.xbe` was a concurrent-write collision with a runner job.

**Recommended fixes (user's infra):**
1. Pause the runner during manual work: `sudo systemctl stop 'actions.runner.*'` (or kill PIDs 370/271).
2. `run_golden_tests.py`: after restoring `HALO_TEST_HARNESS=OFF`, RE-RUN the build so no stale harness object remains.
3. `runtime-oracle.yml`: build/deploy in an ISOLATED dir, not the shared `build/` + `halo-patched/` + live box; and reconsider the `push:`-to-main trigger (or gate it).

**Build-env workaround used this session:** `rm build/CMakeFiles/halo.dir/src/halo/shell_xbox.c.obj` then rebuild → forces harness-OFF recompile (binary shrank 4,599,808→4,595,712, confirming the harness path dropped; the build `rev` token is NOT content-derived, so it misleadingly stayed constant — trust file size + debug.txt `suite=xbox_harness` presence/absence).

---

## PART 2 — deactivated functions
- **0x29040** (actor_look_update), **0x21710** (ballistic), **0x22ba0** (aim_grenade): flipped `ported:true` in WT — but this just RESTORES HEAD (the prior session's WT had deactivated them; HEAD already had them true). 0x29040 fix is committed (280cede7, 98.3% VC71, 20/20 equiv). Not independently runtime-validated this session.
- **0x25c10** (firing-position evaluator): stays `ported:false` (allowlisted; our lift produces a garbage tag_block ptr → freeze). Full re-lift NOT started (task #5).
- **0x1b0630** (euler aim): **block-fix APPLIED to `units.c` ~12151** but kept DORMANT. Details below. Needs full-function re-lift (task #6).

### 0x1b0630 disasm-grounded findings (analyst) — applied to the cross/dot/acos block only
- `0x1d94f0` **IS acos** (MSVC CRT acos; arg in ST(0)). The old `((float(*)(void))0x1d94f0)()` raw cast was a stub bug. Use `acosf(dot)` (repo idiom: `actor_moving.c` `arccosine`, `bipeds.c:233`, `random_math.c`). Do NOT add 0x1d94f0 to kb.json.
- The dropped acos argument = the **clamped dot product** `vel_end·end_aim`, clamped to [-1,1] (consts 0x255e94=-1.0, 0x2533c8=+1.0), stored at `[EBP+0x18]` (reused param-5 slot).
- The angular-velocity vector = `cross(end_aim, vel_end)`. Our old C wrongly read `rot_angles[N]` — **buffer aliasing**: the 2nd `angles_to_vector(vel_end, …)` overwrites the `[EBP-0x28]/[EBP-0x24]` slots the decompiler named `rot_angles[1..2]`.
- Sequence: cross → `normalize3d(angular_velocity)` (ret discarded) → `acosf(clamped dot)` → clamp angle to `angular_velocity_limit` → `angular_velocity[i] *= angle`.
- Applied fix uses contiguous `float total_angles[2]` (avoids the scalar-to-vector-helper hazard) + `acosf`. Build PASS, hazard-clean, ABI PASS.
- **BUT** VC71 only 73.5%; equivalence inconclusive (stub-arg divergence at call index 0 = `real_matrix4x3_transform_point`, BEFORE the edited block — a clang-vs-MSVC frame-address artifact). The function's untouched clamp/wrapping/planner-call sections (`FUN_001ac680` x2 @0x1b0968/0x1b097e, `FUN_001a8550` x2 @0x1b09ae/0x1b09d3) are NOT line-verified. **Full re-lift + §10 caller audit needed before reactivation.** Delinked ref exists: `delinked/functions/001b0630.obj`.

---

## UNCOMMITTED WORKING-TREE STATE (as of this handover)
- `src/halo/units/bipeds.c` — band-aid clamp REMOVED (Part 1, faithful). KEEP+COMMIT.
- `src/halo/units/units.c` — 0x1b0630 block-fix (PARTIAL; function dormant). HOLD or commit clearly labeled "partial re-lift, dormant".
- `kb.json` — net vs HEAD: 0x25c10 true→false (correct; allowlisted). (0x29040/0x21710/0x22ba0 back to true = HEAD, no net change.)
- `src/halo/tag_files/tag_groups.c` — TEMP DIAG (`TBGE-OOR … __builtin_return_address`) at the 0xc11 assert. REVERT before commit (was kept in the deployed build to log the 0xA0001 caller chain if it recurred; `git checkout` was dcg-BLOCKED — revert via Edit). NOTE: the deployed box build still has this diag compiled in.
- Parallel-session load-bearing fixes (user confirmed "commit everything sound"): `xdk_rt.c`, `xbox_crt.c` (chkstk frame reservation), `vector_math.c` (normalize3d 0x2533d0 read as double), `real_math.c` + `objects.c` (0x2549d8 epsilon as double).
- `tools/audit/deactivation_allowlist.json` — parallel session added 0x25c10/0x1b0630 entries (+7 lines). **0x12f990 (network_server_manager) is `ported:false` + NOT allowlisted → blocks the pre-commit deactivation gate.** Allowlist it (documented pre-existing) or bypass.
- Junk to delete (handover list, untracked): `-`, `=0x4b`, `=5,`, `=8`, `in`, `2026-06-16`, `qmp_test.py`, `src/halo/physics/.vc71_regcall_collision_usage.c`.

## NEXT STEPS
1. **Stabilize the box**: stop the self-hosted runner; confirm the other claude/opencode sessions aren't pushing to main.
2. Deploy ONE known build (harness-OFF: confirm via `debug.txt` has no `suite=xbox_harness` and reaches `main_loop`), then runtime-validate a10 in-level AI biped pathfinding → confirm no `tag_groups.c:3089`.
3. Commit (locally; a push triggers the runner): parallel fixes; Part 1 band-aid removal; kb.json (0x25c10 deactivation) + allowlist 0x12f990. Revert tag_groups diag first. Use `generate_lift_commit.py`.
4. Full re-lift 0x1b0630 (whole function + §10 caller audit), then reactivate. Then 0x25c10.
5. Investigate the a10 cryo-intro HANG (`memory check failed at scenario_load` + double `object_create`) on a CLEAN build to see if it's real or a deploy-collision artifact.
