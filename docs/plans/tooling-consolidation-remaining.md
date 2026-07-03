# Tooling Consolidation — Remaining Work (audit 2026-07-03)

Source audit: `tmp/tooling_audit_2026-07-03.md`. This doc tracks what was **done**,
what **deviated** from the audit, and what **remains**, so a future session can pick
up without re-deriving the reference maps.

Rule for the whole effort: **archive via `git mv` to `tools/archive/`, never delete;
each phase is its own commit; nothing bundled with lift work.**

---

## Done (committed on main)

| Commit | Phase | Summary |
|--------|-------|---------|
| `12cc1154` | P0 | xemu_qmp launch-path fix; hook installer + prepare-commit-msg + rtk-trust reconcile; runtime-oracle.yml push-trigger removed; vc71_regression double-update de-duped; dump_xemu_memory doc redirect; verify_option3/run_golden_xbox docstrings |
| `82c99814` | P1.6 | archived 13 unreferenced debris/superseded tools |
| `713ddfe2` | P1.6b | archived biped-activation session scaffolding (+ handover-doc note) |
| `4283c3c2` | P1.8 | promoted triage_failures (nightly CI) + usage_report (/retrieval-refresh) |
| `f9e87718` | P2.9 | renamed `xdbm_screenshot.py` → `xbdm_screenshot.py` across 9 files + user-memory |

Hook installer note: `tools/hooks/install.sh` is the authority; it ran and installed
`prepare-commit-msg` + appended the rtk-trust block to codegraph's `post-checkout`
(backup at `.git/hooks/.backup-<ts>/`). `--check` is wired non-blocking into SessionStart.

---

## Deviations from the audit/plan (READ THIS before acting on the audit verbatim)

The audit had several inaccurate "0 refs / dead" claims — verified false during execution:

1. **Do NOT archive these — they have live imports/hooks** (audit said "0 refs"):
   - `tools/equivalence/game_state_replay.py` — imported by `test_game_state_diff.py:18` (`from game_state_replay import diff_snapshots`).
   - `tools/build/build_hash.py` — imported by `tools/xbox/deploy_xbox.py:37` (`from build.build_hash import print_build_hash`), called :932.
   - `tools/analysis/auto_discover.py` — live via `pre-commit-auto-discover` hook + `tools/analysis/progress.py:269`.
   A bare-basename grep misses module-style imports; always also grep `from <module> import`.

2. **Kept (not dead enough to archive), contra audit P1.6b:**
   - `tools/xbox/xbox_keyboard_pad.py` — working, documented human-input tool (`docs/xbox-pad.md`).
   - `tools/analysis/punpckhdq_import.py` + `apply_punpckhdq_renames.py` — documented re-runnable analysis pair (CLAUDE.md/AGENTS.md).
   - `tools/docs/c20_h1_{pages,scripting,tags}.py` — `c20_h1_tags.py` is referenced by `docs/references/h1/pages/index.md:70` (live), so it is NOT 0-ref.

3. **`run_golden_xbox.py` "stale marker" claim was wrong** — the harness DOES emit
   `RUN|END|...` (`src/halo/test_harness.c:217,948`). The real issue is it has *no invoker*.
   Its docstring was corrected to say so (pending P2.11 fold, not "broken markers").

4. **`dump_xemu_memory` reach was inflated** — "13 docs + 7 skills" was really ~4 authoritative
   files + 1 skill (`debug-xemu`); the "7 skills" were `token_discipline` bookkeeping JSONs.

5. **Owner-decision tools — evaluated, all KEEP** (audit marked them archive candidates):
   - `tools/xbox/xemu-mon.py` — **in use**: `tools/xbox/boot_hash.sh:81,84` invokes it for
     `x /Nwx` memory dumps + `info status`. NOT fully superseded by the xemu MCP.
   - `tools/analysis/structural_prescreen.py` (note: `analysis/`, not `lift/`) — **in use**:
     `tools/llm_auto_lift.py:720` `from analysis.structural_prescreen import screen_all`.
   - `tools/report/matching.py` — self-documents as "retained for standalone analysis only;
     Score button no longer uses it." Misleading name but harmless; keep (optional: rename).
   - `tools/patches/patch_respawn_time.py` — keep (owner: "a fun thing to keep").
   - mizuchi loop — still owner-decision; untouched.

---

## Remaining work

### P2.10 — Retire the /verify-all + verify_option3 legacy chain
Superseded by `tools/lift_pipeline.py`. Reference map (verified; excludes worktrees/pycache/token_discipline):
- **Archive** `tools/verify/verify_option3.py`, `tools/verify/verify_all.py`, and
  `tools/build/build_iso.py` (build_iso's only live consumer is verify_option3).
- **Delete** `.claude/commands/verify-all.md`.
- **Edit** `.claude/commands/verify.md`: remove mode 8 `option3` + its command line; add a
  `report` mode wrapping `tools/verify/test_inventory.py` (keep test_inventory — it is
  standalone: reads kb.json / leaf_cache / delinked manifest).
- **Edit** `.claude/skills/halo-verify-debug/SKILL.md` (references verify_option3 + build_iso).
- **Edit** docs referencing the chain: `docs/testing_plan.md` (this doc *is* the /verify-all
  spec — deprecate or rewrite), `docs/agent_workflow.md`, `docs/verification_policy.md`,
  `docs/plans/workflow-improvement-plan.md`.
- **Check** `tools/analysis/sync_agent_content.py` (references "verify-all" — it may regenerate
  agent content; confirm it won't re-introduce the removed refs).
- CAUTION: the `verify_all` substring collides with `batch_verify_allowlist.json` in
  equivalence.yml / run_local_equiv.sh / update_dashboard.sh — those are NOT verify_all.py refs.
- Also retire `llm_auto_lift.py` `review`/`promote` subcommands + AUTO_ACCEPT/NEEDS_REVIEW/REJECT
  machinery **carefully** (core file, 22 commits) — smoke-test `select` + `cache-context` after.

### P2.11 — Fold run_golden_xbox → run_golden_tests
Merge into `run_golden_tests.py --backend {xemu,xbdm}`; archive `run_golden_xbox.py`.
**Needs a real-Xbox parity run** (`--backend xbdm`) before archiving — box-dependent.

### P2.12 — Schedule ab_check frozen-golden CI tripwire
Nightly self-hosted job: `ab_check.py --golden <frozen>.halorec`.
**Needs a frozen golden `.halorec`** captured from cachebeta once — box-dependent (host-only,
gitignored artifact; do not commit game memory).

### P3.13 — One QMP client library
Extract `tools/xemu_mcp/qmp_sync.py` (reuse `qmp.py` sync-wrapped); consumers `xemu_qmp.py`,
`qmp_capture.py`, `verify_toggles_live.py`. Leave `dump_xemu_memory.py` legacy path + `xemu-mon.py`.
Never touch gdbstub `:1234`; QMP `:4444` is single-client. Behavior-lock with `regression_test.py --quick`.

### P3.14 — unicorn_diff ingestion/reporting extraction
Move `--state-snapshot`/`--from-halorec`/`--value-corpus` ingestion into `snapshot_input.py`;
move `--batch-classify`/`--list-funcs` reporting out. **Audit says do this LAST and coordinate
with in-flight work** — `unicorn_diff.py` (2601 lines) + `stubs.py` are daily-churn. Behavior-lock
with `regression_test.py --quick` + one full `batch_verify` diff vs previous `summary.json`.

### P3.15 — Dashboard regen unification + post-commit dead-code
- `tools/hooks/post-commit.sh`: its codegraph sync block sits **after `exit 0`** → dead code.
  Move it before the exit, or confirm codegraph sync is covered by post-checkout/post-merge and
  remove the dead block.
- Have `post-commit.sh` call `tools/report/update_dashboard.sh` instead of duplicating the
  `generate_ci_status.py` + `generate_decomp_report.py` command list.

### P3.16 — Enrichment dedup (+ owner items resolved above)
De-dup `enrichment_hook.render_enrichment_markdown` (self-documented inlined copy at
`tools/retrieval/enrichment_hook.py:23`) against the `llm_auto_lift.py` original.

### Foolproofing (audit §8) — not yet done
- `mcp-servers.sh probe xemu` = raw-QMP fallback probe; SessionStart health-check for xemu.
- `vc71_verify.py` should exit with the exact `batch_delink.py` remediation command when the
  delinked ref is missing.
- `memsave_snapshot.py` / `state_snapshot.py` should refuse to write **unverified** dumps
  (datum-magic check *before* write).
- Print pre-commit gate count in `generate_lift_commit.py` output.

---

## Verification checklist (per remaining phase)
- P2.10: `/verify` help has no option3; one full `/lift` pipeline run green; `test_inventory.py`
  still runs standalone via the new `report` mode.
- P2.11: real-Xbox `--backend xbdm` golden run matches prior behavior before archiving.
- P3.13/P3.14: `regression_test.py --quick` green + (P3.14) one `batch_verify` diff with no new
  failures + `ab_check --aa-first` clean.
- P3.15: one scratch commit still regenerates the dashboard and runs codegraph sync.
