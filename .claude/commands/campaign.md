---
description: Unattended lift campaign — supervise repeated /auto-session runs, resolve known parks, stop on a dry frontier or real conflict
model: sonnet
subtask: false
---

This command is a **supervisor loop around `/auto-session`**, run by the main
session agent. It owns no lift, verify, commit, or merge logic — all of that
lives in `.claude/workflows/auto-session.js` and, below it, `goal-lift.js` and
`tools/integrate/auto_reintegrate.py`. What this adds is the outer loop that
`auto-session` deliberately lacks: it re-launches runs, triages each stop
reason, resolves the *known-benign* park classes (regen dirt, gate timeout,
transient infra), and stops on the ones a human must own.

**Run only from an isolated session branch, never on `main`** — same rationale
as `/auto-session`: the workflow FF-advances `main` to this branch's tip.

Argument: $ARGUMENTS

Parse from $ARGUMENTS (all optional):
- `--max-runs N` — auto-session runs before stopping (default: 6).
- `--batches N` / `--batch-goal N` — passed through per run (defaults: 2 / 12).
- `--objects obj1,obj2,...` — restrict selection. Multiple clusters may be given
  as `--objects a.obj,b.obj --objects c.obj` (repeatable); extras become the
  **rotation list** used when a cluster's committable frontier stalls.
- `--criteria "free text"` — freeform selection instruction (pass-through).
- `--no-land` — never attempt to land; commits accumulate on the branch.
- `--lift-reg-args` — let goal-lift lift `@<reg>` targets instead of dropping
  them at its two pre-screens. The remaining frontier is mostly these.
- `--no-widen` — disable the widening ladder; stop on the first zero-commit `queue_exhausted` (old behaviour).
- `--dry-run` — pass through; nothing commits and nothing lands.
- `--model NAME` — override goal-lift's "reasoning stages" (its own M-table
  grouping: select + lift/review, i.e. EXTRACT_MODEL and REASON_MODEL, default
  Opus) for every run in this campaign, e.g. `--model sonnet`. Pass through as
  `model` in every `auto-session` Workflow call (step 5). auto-session.js
  resolves it to BOTH `reasonModel` and `extractModel` (an explicit
  `--reasonModel`/`--extractModel` still wins over the blanket for its own
  role). It deliberately does **not** touch:
  - the commit-gate agent (COMMIT_MODEL), which stays on goal-lift's own
    MECHANICAL_MODEL default (haiku) — it only runs a build and parses the
    result, no reason to pay reasoning-model rates there;
  - the improve-pass agent (IMPROVE_MODEL), which goal-lift itself defaults to
    whatever REASON_MODEL resolves to — so it follows `--model`/`--reasonModel`
    automatically, without being forced independently.
  Use `--reasonModel`/`--extractModel`/`--commitModel`/`--improveModel`
  directly (passed the same way, verbatim) for anything more specific than
  this default split.

## Preflight (once)

1. **Branch guard.** `rtk git branch --show-current`. If `main`, STOP — do not
   start. (auto-session guards this too, but failing here costs nothing.)
2. **Rebase against main before launching any run.** Divergence is the #1
   cause of a run-1 park. `rtk git rev-list --left-right --count main...HEAD`
   (`<main-only>\t<branch-only>`). If `<main-only>` is 0, skip. Otherwise:
   `rtk git -c core.hooksPath=/dev/null -c rerere.enabled=false rebase main`.
   - Clean rebase → continue.
   - Conflict (almost always `kb.json` — git's line-merge conflicts on
     disjoint-object array edits even when both sides touched *different*
     kb.json objects) → `rtk git rebase --abort` immediately. **STOP before
     launching run 1** and hand off to `/reintegrate-to-main` for the
     object-index-aware merge. Do NOT launch a run against an unrebased
     branch and let the land gate discover the conflict after burning a full
     run's tokens — `auto_reintegrate.py`'s land step uses the same plain git
     merge and parks identically after run 1, just later and far more
     expensively (observed 2026-08-12: 7.3M subagent tokens, 891 tool calls,
     ~3h wall for 12 landed functions before the park was even visible).
3. **Reconcile the parked ledger.** `rtk python3 tools/lift/park.py reconcile`
   (dry run by default). If it reports stale records, re-run with `--apply`,
   then `rtk git add -- artifacts/parked` and commit as a standalone chore
   commit. `artifacts/` is gitignored but the parked records are force-tracked,
   so only already-tracked records stage — that is correct. Commit nothing if
   nothing stages.
3b. **Recover hook-blocked parks (rung 0, no LLM spend).** Parked lifts whose
   latest `goal_progress.md` row is a commit-hook block (not a REJECT or
   NEEDS_RUNTIME verdict) are recoverable mechanically:
   `rtk python3 tools/lift/park_recover_run.py --min-score 90` with
   `timeout: 600000` and `run_in_background`; poll its output. It applies only the
   target function's hunks, builds, commits, and promotes the record; any failure
   reverts and moves on. Rerun once per campaign start and again whenever a run
   reports `commit_failed` parks. `needs_manual` / `conflict` rows are left for a
   human. Details: rows land in `artifacts/campaigns/park_recover.jsonl`.
4. **Triage main-worktree dirt.** `rtk git -C <main worktree> status --short`
   (tracked only). The land gate absorbs exactly this generated set
   (`_ABSORB_EXACT` / `_ABSORB_PREFIXES` in `auto_reintegrate.py`):
   `README.md` (stats block only), `tools/verify/vc71_scores.json`,
   `tools/objects.csv`, `tools/equivalence/leaf_cache.json`,
   `artifacts/batch_verify/**`. **Anything else is authored WIP** — a `.py`,
   `.c`, `.yml`, `kb_meta.json`, or a README diff outside the stats block. Do
   NOT absorb, stash, or commit it. Note it in the final report and run the
   whole campaign with `noLand: true`; commits accumulate on the branch and land
   in one FF later.

## Run loop (repeat until a stop condition)

5. **Launch a run.** `Workflow({ name: "auto-session", args: { batches: N,
   batchGoal: M, noLand: <bool>, dryRun: <bool>, liftRegArgs: <bool>,
   objects: [...], criteria: "<text>", model: "<name>", reasonModel: "<name>",
   extractModel: "<name>", commitModel: "<name>", improveModel: "<name>" } })`.
   Omit any key not given on the command line — in particular omit
   `objects`/`criteria`/`model` when not passed, same as before. The four
   granular `*Model` keys are advanced/rare: only include one if the user
   explicitly passed the matching `--reasonModel`/`--extractModel`/
   `--commitModel`/`--improveModel` flag; `--model` alone should map to
   `model` only, not to all four.
6. **Warm the next targets while it runs** (fire-and-forget, ignore failures):
   `rtk python3 tools/lift/llm_auto_lift.py cache-context --batch 8`.
7. **Triage the result** (`{branch, batches_landed, batches_unlanded,
   functions_committed, stopped_reason, park_reason, conflicts, resumable}`):

   | `stopped_reason` | Action |
   |---|---|
   | `batches_exhausted` | Clean run. Continue. |
   | `completed_with_unlanded` / any `batches_unlanded > 0` | Work is committed, landing was merely blocked. Rerun the gate once (step 8), then continue. |
   | `queue_exhausted` | Zero committed. **Do not stop on the first one** — the stock selector only hands goal-lift `lane==auto-lift` targets, and after the code pre-screens (parked, `@<reg>`, CRT/XAPILIB, excluded) a handful survive (observed 2026-10-14: 4 candidates, 1 attempted, 0 committed, twice). Climb the **widening ladder** below, one rung per consecutive zero-commit run. Stop only when the ladder is spent. |
   | `infra_blocked` with `resumable: true` | Resume **once**: `Workflow({ scriptPath: ".claude/workflows/auto-session.js", resumeFromRunId: "<runId>" })`. Succeeded agents replay from cache. If it dies again, do **not** resume twice — salvage with step 8 and continue with a fresh run. |
   | `land_agent_null` | Almost always the gate exceeding the agent's Bash timeout, not a real failure. Rerun the gate (step 8); if it lands, continue. |
   | `parked` | Real signal about this work — merge conflict, failed gate, non-FF. **STOP.** Report `park_reason` + `conflicts` and hand off to `/reintegrate-to-main`. |
   | `guard_failed` / `no_commit` | **STOP** (guard) or count as a zero-commit run (no_commit). |

   After triage, **append one row to the campaign ledger**
   `artifacts/campaigns/campaigns.jsonl` (create the directory if missing) so
   campaigns are quantifiable and comparable across future selector/workflow
   changes. One JSON object per line:

   ```json
   {"ts": "<ISO-8601 finish time>", "campaign": "<branch>@<campaign start date>",
    "run": <n>, "branch": "<branch>", "objects": [...], "batches": N,
    "batch_goal": N, "model": "<name or 'default'>", "functions_committed": N,
    "improve_promoted": N, "batches_landed": N, "batches_unlanded": N,
    "stopped_reason": "...", "tokens_spent": N, "wall_s": N}
   ```
   Record `model` even when `--model` was not given (as `"default"` — Opus) so a
   later `campaigns.jsonl` comparison across runs isn't silently comparing
   different model settings without saying so.

   `tokens_spent` comes from the run's result JSON; `wall_s` is your own
   launch→completion timestamps (workflow scripts cannot read the clock).
   The ledger lives under gitignored `artifacts/` and stays untracked local
   state — do not `git add -f` it or commit it.
   Compare later with e.g.
   `rtk jq -s 'group_by(.campaign) | map({c: .[0].campaign, fns: map(.functions_committed) | add, tokens: map(.tokens_spent) | add})' artifacts/campaigns/campaigns.jsonl`.

8. **Manual land-gate rerun** (used by the rows above, at most once per run):
   `rtk python3 tools/integrate/auto_reintegrate.py --branch <branch> --json`
   with an explicit `timeout: 600000`; if it still overruns, background it and
   poll for the JSON — never run two gates concurrently. `status: landed` →
   continue. `inconclusive` → main is still busy; continue with `noLand: true`.
   `parked` → STOP as above.
9. **Between runs**, re-run `park.py reconcile` — in-run lifts often supersede
   parked records. Commit the `--apply` result as its own chore commit.

## Widening ladder (what to do instead of ending on `queue_exhausted`)

Unless `--no-widen` was passed, a zero-commit `queue_exhausted` (any
`empty_queue*` variant) advances one rung; a run that commits ≥1 function
resets to rung 0. Keep a campaign-level set `attempted_zero` = every address in
the result's `attempted` list from runs that committed 0 — it feeds rung 2 so
the same stuck target (e.g. one that times out `lift_pipeline`) is not
re-offered run after run.

0. **Stock selection** (the run just made).
   (`liftManual: true` is the permanent lane widener: it lets goal-lift take
   `manual-lift` targets without pinning. Rung 1 enables it together with `liftRegArgs`.)
1. **`liftManual: true` + `liftRegArgs: true`** — the remaining auto-lift frontier is mostly
   `@<reg>` targets that the two pre-screens drop. goal-lift's own gates still
   revert wrong decls. Skip this rung if `--lift-reg-args` was already given.
2. **Pin-list from the full selector.** The lane gate is what starves the
   queue (2026-10-14: 46 `manual-lift` + 27 `defer` vs 18 `auto-lift` in the top
   200), and `addrs` pins bypass it. Build the list:
   `rtk python3 tools/lift/llm_auto_lift.py -q select --limit 200 --json`, then keep
   targets with `lane in {auto-lift, manual-lift}`, not `prior_fail`, no
   `parked_status`, not in `attempted_zero`, not in 0x1cf900–0x1de000 (CRT/XAPILIB),
   not `xbox_crt.obj`, and `has_reg_args == false` (unless rung 1 is on). Take
   the top `batchGoal` by `total_score` and pass them as `addrs`. Pinned runs
   cost far more per function (see memory
   `reference_pinned_campaign_cost_multiplier`), so use `batches: 1` and note
   the rung in the ledger row (`"widen_rung": 2`).
3. **Rotate `--objects`** clusters (the existing rotation list), if any remain.
4. **Ladder spent** — empty pin-list at rung 2 and no rotation cluster left →
   run `/byte-campaign --matching-only` once (PAL-guided raw-byte gains on
   ported functions), then **STOP**: the frontier is genuinely dry for this
   branch.

Add `"widen_rung": N` to every ledger row. Never widen past a `parked`,
conflict, or authored-WIP stop; those are not queue problems.

## Stop conditions

- `--max-runs` reached.
- Widening ladder spent (or `--no-widen` and a zero-commit `queue_exhausted`).
- Two **consecutive** zero-commit runs *at the top rung of the ladder* (earlier zero-commit runs climb a rung instead).
- A real park: `stopped_reason: parked`, a merge conflict, or authored WIP found
  in the main worktree.

## Final report

Report, in this order: runs completed / max; total functions committed; batches
landed vs. still unlanded; every park encountered and how it was resolved
(rerun gate / resumed / rotated / stopped); which branch the commits sit on and
whether they are in `main` yet. Include the campaign's ledger totals
(functions committed, tokens spent, wall time) and, when a prior campaign row
exists in `campaigns.jsonl`, the per-function token/time cost next to the
previous campaign's for comparison.

Close with the explicit note that **promotion to `main` stays human-gated** when
the campaign ran with `noLand` or ended unlanded — land it deliberately with
`rtk python3 tools/integrate/auto_reintegrate.py --branch <branch> --json` once
`main` is idle (the gate always targets `main`; there is no `--target` flag), or
use `/reintegrate-to-main` if the gate parks.

## Usage

```bash
/campaign                                          # 6 runs x 2 batches x 12
/campaign --max-runs 3 --batch-goal 8
/campaign --max-runs 4 --objects units.obj,actors.obj --objects effects.obj
/campaign --max-runs 6 --lift-reg-args             # frontier is mostly @<reg> now
/campaign --max-runs 2 --no-land                   # main busy; land once at the end
/campaign --max-runs 1 --dry-run                   # trial: nothing commits, nothing lands
/campaign --max-runs 3 --model sonnet              # override the reasoning model, all runs
```
