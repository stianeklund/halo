# Tooling Audit - 2026-07-07

Scope: `tools/`, command/skill wrappers, hooks, CI workflows, and docs that tell
agents how to use the tooling. This is an evaluation note only; it does not
change behavior.

Working-tree note: the tree was already dirty when this audit started
(`.opencode/commands/permuter-campaign.md`, `.agents/mcp_config.json`,
`ci/snapshot_unicorn_verify.json`, `kb_functions.txt`, `tmp/`, and an untracked
file named `-`). None of those were modified by this pass.

## Inventory Snapshot

Current non-archive `tools/` surface from a text-safe inventory:

- 171 Python files.
- 28 shell files.
- Largest clusters: `equivalence` (63 files), `xbox` (31), `audit` (24),
  `verify` (20), `analysis` (15), `report` (12), `retrieval` (12), hooks (13).

This is not automatically bad. The project has several legitimately different
lanes: lifting, byte verification, behavioral equivalence, real-Xbox deployment,
xemu control, CI dashboards, retrieval, and agent hooks. The problem is that
legacy lanes and one-off probes live next to the core tools with similar names,
so agents cannot reliably tell "primary path" from "historical fallback".

## Primary Tooling Shape

Keep these as first-class, documented entry points:

- Build/patch: `tools/build/build.py`, `tools/build/patch.py`,
  `tools/build/build_hash.py`, `tools/build/gen-build-info.py`.
- Lift orchestration: `tools/lift_pipeline.py`, `tools/llm_auto_lift.py`
  for `select`, `score`, and `cache-context`; plus `tools/lift/*`.
- Static safety gates: `tools/audit/check_lift_hazards.py`,
  `audit_reg_abi.py`, `check_callee_reg_args.py`, `extract_reg_args.py`,
  `generate_lift_commit.py`, `check_ported_deactivations.py`.
- Byte verification: `tools/verify/vc71_verify.py`, `compare_obj.py`,
  `vc71_regression.py`, `test_inventory.py`.
- Behavioral equivalence: `tools/equivalence/unicorn_diff.py`,
  `batch_verify.py`, `batch_equivalence.py`, `regression_test.py`,
  `stubs.py`, `coff_loader.py`, `seeds.py`, `concolic.py`.
- Runtime/A-B: `tools/equivalence/ab_check.py`, `aa_check.py`,
  `behavior_diff.py`, `trajectory_diff.py`, `hmrc.py`,
  `tools/xbox/capture_scenario.py`, `capture_trajectory.py`.
- Xbox/xemu operations: `tools/xbox/deploy_xbox.py`,
  `build_deploy_run.sh`, `clear_cache.py`, `xbdm_rdcp.py`,
  `xbdm_screenshot.py`, `verify_toggles_live.py`, `xemu_qmp.py`,
  `xbox_pad.py`, `xemu_key.py`.
- Reports/CI: `tools/report/generate_decomp_report.py`,
  `generate_ci_status.py`, `history.py`, `runtime_oracle_summary.py`,
  `update_dashboard.sh`, `progress_server.py`.
- Retrieval/agent memory: `tools/retrieval/*`, `tools/memory/*`, and the
  hook scripts wired from `.claude/settings.json`.

## High-Confidence Cleanup

### 1. Retire `/verify-all` and `verify_option3` as a phase (done)

This was the cleanest removal target, but it had to be coordinated because it
was still user-facing:

- `tools/archive/verify_option3.py` self-documents as legacy and superseded by
  `tools/lift_pipeline.py`.
- `tools/archive/verify_all.py` and `tools/archive/build_iso.py` were archived
  with it.
- `.claude/commands/verify.md`, `.opencode/commands/verify.md`, and the
  opencode skill now expose `/verify report` instead of `/verify option3`.
- `.claude/commands/verify-all.md`, `.opencode/commands/verify-all.md`, and
  `.agents/skills/opencode-command-verify-all/SKILL.md` were removed.
- `tools/analysis/sync_agent_content.py` no longer regenerates `verify-all.md`.
- `docs/testing_plan.md`, `docs/agent_workflow.md`,
  `docs/verification_policy.md`, and `docs/plans/workflow-improvement-plan.md`
  now point at `test_inventory.py`, `run_golden_tests.py`, or `lift_pipeline.py`.

Completed action:

- Archived `tools/verify/verify_option3.py`, `tools/verify/verify_all.py`, and
  `tools/build/build_iso.py` together.
- Removed `/verify-all` command wrappers and the generated skill.
- Replaced `/verify option3` with `/verify report`, wrapping
  `tools/verify/test_inventory.py`.
- Keep `test_inventory.py`.
- Smoke-test one normal `/lift` pipeline after the docs/command rewrite.

### 2. Fold `run_golden_xbox.py` into `run_golden_tests.py`

`tools/verify/run_golden_xbox.py` now states the real issue correctly: the
`RUN|BEGIN` / `RUN|END` markers still exist, but the script has no repo invoker
and duplicates the real-hardware path that belongs in `run_golden_tests.py`.

Recommendation:

- Add `run_golden_tests.py --host/--xbox-host` targeting so the same XBDM
  deploy/debug flow works against local xemu or real hardware by address.
- Do one real-Xbox parity run before archiving `run_golden_xbox.py`.
- Update `/verify golden` docs and skills if flags change.

### 3. Retire `llm_auto_lift.py review/promote` (done 2026-07-07)

`tools/llm_auto_lift.py` had legacy `review` and `promote` subcommands for old
batch artifacts, along with `AUTO_ACCEPT`, `NEEDS_REVIEW`, and `REJECT` state
machinery. Those CLI paths and constants have been removed.

Implemented:

- `select`, `score`, and `cache-context` remain the only helper subcommands.
- Updated `.claude/commands/auto-lift.md`, `.opencode/commands/auto-lift.md`,
  `.agents/skills/opencode-command-auto-lift/SKILL.md`, and
  `docs/lift-policy.md`.
- Kept the target-selection and context-cache path intact.

## Consolidation, Not Removal

### 4. One QMP client stack

There are multiple QMP clients and capture paths:

- `tools/xemu_mcp/qmp.py` has the strongest transport contract:
  connect-on-demand, id-matched responses, retry/teardown on transport error,
  and idle disconnect to release xemu's single QMP slot.
- `tools/equivalence/qmp_capture.py` has the strongest capture contract:
  raw QMP, virtual `memsave`, `stop` -> capture -> `cont`, and datum-magic
  verification.
- `tools/xbox/xemu_qmp.py`, `tools/xbox/verify_toggles_live.py`,
  `tools/equivalence/state_snapshot.py`, `game_state_snapshot.py`,
  `capture_snapshot_from_diff.py`, `memsave_snapshot.py`, and
  `dump_xemu_memory.py` each carry some local QMP or memory-capture logic.

Recommendation:

- Extract a sync wrapper around `tools/xemu_mcp/qmp.py`, then move
  `xemu_qmp.py`, `qmp_capture.py`, and `verify_toggles_live.py` onto it first.
- Preserve `qmp_capture.py`'s virtual `memsave` and magic-check behavior.
- Leave `dump_xemu_memory.py` as legacy/XBDM fallback until all docs stop
  steering users toward its broken physical `pmemsave` path.

### 5. Fix capture-doc drift

There is contradictory guidance:

- Current repo doctrine says virtual `memsave` is the proven path and physical
  `pmemsave` is broken on this setup.
- `qmp_capture.py` and `memsave_snapshot.py` say this correctly.
- `docs/testing_plan.md`, `docs/equivalence-testing.md`,
  `.agents/skills/halo-verify-debug/SKILL.md`, and
  `.agents/skills/opencode-command-verify/SKILL.md` still recommend QMP
  `pmemsave` in places.
- `state_snapshot.py`, `game_state_snapshot.py`, and
  `capture_snapshot_from_diff.py` still describe or implement `pmemsave`.

Recommendation:

- Update docs and skills to prefer `memsave_snapshot.py` / `qmp_capture.py`
  virtual `memsave`.
- Mark physical `pmemsave` code paths as legacy or XBDM-only fallback where
  applicable.
- Add a refusal/guard in snapshot writers before emitting unverified dumps.

### 6. Dashboard regeneration duplication (done)

`tools/hooks/post-commit.sh` used to duplicate the command list in
`tools/report/update_dashboard.sh` and had a CodeGraph sync block after
`exit 0`. It now delegates dashboard generation to `update_dashboard.sh` and
runs CodeGraph sync before exiting.

Recommendation:

- Make `post-commit.sh` call `tools/report/update_dashboard.sh`.
- Move the CodeGraph sync before `exit 0`, or remove it if post-checkout /
  post-merge already covers the needed freshness.
- Verify with one scratch commit.

### 7. Live tool in `tools/archive` (done)

`CMakeLists.txt` used to call `tools/archive/gen-struct-checks.py` to generate
`typechecks.c`. This has been moved to `tools/build/gen-struct-checks.py`, so
`tools/archive` can return to meaning "not live."

Recommendation:

- Move `gen-struct-checks.py` back to a live directory, e.g.
  `tools/build/gen-struct-checks.py` or `tools/analysis/gen-struct-checks.py`,
  and update CMake.
- Keep `tools/archive/` for truly historical scripts only.

## Keep, But Label Better

These looked suspicious in a naive reference scan but should not be archived:

- `tools/analysis/classify_cap.py` and `classify_liftability.py`: used by
  `.claude/workflows/goal-lift.js`.
- `tools/audit/token_discipline_hook.py`, `structural_ceiling_hook.py`,
  `tools/memory/skill_router_hook.py`, `prior_fix_hook.py`,
  `action_skill_router.py`: wired from `.claude/settings.json`.
- Hook scripts under `tools/hooks/`: wired through `tools/hooks/install.sh`,
  not always by direct file reference.
- `tools/report/matching.py`: no longer powers the dashboard Score button, but
  self-documents as retained for standalone analysis.
- `tools/xbox/xemu-mon.py`: still used by `tools/xbox/boot_hash.sh`.
- `tools/patches/patch_respawn_time.py`: owner-decision keep.
- `tools/mizuchi/*`: owner-decision keep until the workflow is explicitly
  retired or replaced.

## Triage Queue

These need owner confirmation or a narrow reference check before action. They
are plausible archive candidates, not proven dead:

- `tools/audit/check_gen_hash.py`: usage text appears stale
  (`tools/check_gen_hash.py`), and no live reference was found in the scoped
  scan.
- `tools/audit/validate_kb.py`: useful-looking validator, but not wired into
  the current build/hook/pipeline surface.
- `tools/verify/cache_stats.py`: useful for VC71 cache inspection; keep if
  operators use it manually, otherwise fold into `vc71_verify.py --cache-stats`.
- `tools/verify/run_dual_oracle_tests.py`: likely valuable but low-reference;
  either document as `/verify dual-oracle` backend or archive after confirming
  no current harness users.
- `tools/equivalence/make_deflate_snapshot.py`: low-reference specialist.
- `tools/equivalence/run_trajectory_sweeps.py`: low-reference specialist;
  compare against `aa_check.py`, `ab_check.py`, and `capture_trajectory.py`.
- `tools/xbox/deploy_cache.py`: low-reference but may be a manual operator
  tool; document or archive.
- `tools/xbox/grab_debug_txt.sh`: likely superseded by `xbdm_debug_txt.py`;
  confirm before archiving.
- `tools/xbox/run-xbe.sh`: only obvious reference is archived `magicboot.sh`;
  confirm whether anyone still calls it manually.
- `tools/xbox/xbox_keyboard_pad.py`: keep per previous owner decision, but
  docs should keep pointing automation users at `xbox_pad.py`.

Tests with low textual references are not cleanup candidates by themselves:
`tools/audit/tests/test_check_lift_hazards_new.py`,
`tools/equivalence/test_ab_check.py`, `test_game_state_diff.py`,
`test_stub_arg_trace.py`, `test_value_corpus.py`,
`tools/xbox/test_verify_toggles_live.py`, and similar should stay unless the
covered feature is retired.

## Suggested Phase Order

1. Documentation safety first: fix `pmemsave` vs virtual `memsave` guidance and
   mark broken/legacy capture paths clearly.
2. Move live `gen-struct-checks.py` out of `tools/archive`.
3. Retire `/verify-all` + `verify_option3` + `build_iso` and rewrite command /
   skill docs in one commit.
4. Fold `run_golden_xbox.py` into host-parameterized `run_golden_tests.py`,
   gated by a real-Xbox parity run.
5. Remove `llm_auto_lift.py review/promote` legacy mode.
6. Unify QMP clients behind one sync/async transport library.
7. Clean low-reference manual tools in small owner-confirmed batches.

## Verification For Cleanup PRs

- Always run a scoped reference search before archiving a tool; include module
  imports as well as literal path references.
- After command/skill changes: run
  `rtk python3 tools/analysis/sync_agent_content.py --check` and
  `rtk python3 tools/audit/audit_agent_content.py --strict`.
- After `/verify` changes: run `/verify` help manually on both Claude/OpenCode
  docs or inspect the generated command files.
- After QMP/capture changes: run `tools/equivalence/regression_test.py --quick`
  and one known `ab_check.py --aa-first` fixture when xemu is available.
- After hook/dashboard changes: make one scratch commit and confirm dashboard
  regeneration plus CodeGraph freshness behavior.
