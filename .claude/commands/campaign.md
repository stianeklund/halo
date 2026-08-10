---
description: Unattended lift campaign — supervise repeated /auto-session runs, resolve known parks, stop on a dry frontier or real conflict
model: opus
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
- `--dry-run` — pass through; nothing commits and nothing lands.

## Preflight (once)

1. **Branch guard.** `rtk git branch --show-current`. If `main`, STOP — do not
   start. (auto-session guards this too, but failing here costs nothing.)
2. **Reconcile the parked ledger.** `rtk python3 tools/lift/park.py reconcile`
   (dry run by default). If it reports stale records, re-run with `--apply`,
   then `rtk git add -- artifacts/parked` and commit as a standalone chore
   commit. `artifacts/` is gitignored but the parked records are force-tracked,
   so only already-tracked records stage — that is correct. Commit nothing if
   nothing stages.
3. **Triage main-worktree dirt.** `rtk git -C <main worktree> status --short`
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

4. **Launch a run.** `Workflow({ name: "auto-session", args: { batches: N,
   batchGoal: M, noLand: <bool>, dryRun: <bool>, liftRegArgs: <bool>,
   objects: [...], criteria: "<text>" } })`. Omit `objects`/`criteria` when not
   given. It runs in the background.
5. **Warm the next targets while it runs** (fire-and-forget, ignore failures):
   `rtk python3 tools/llm_auto_lift.py cache-context --batch 8`.
6. **Triage the result** (`{branch, batches_landed, batches_unlanded,
   functions_committed, stopped_reason, park_reason, conflicts, resumable}`):

   | `stopped_reason` | Action |
   |---|---|
   | `batches_exhausted` | Clean run. Continue. |
   | `completed_with_unlanded` / any `batches_unlanded > 0` | Work is committed, landing was merely blocked. Rerun the gate once (step 7), then continue. |
   | `queue_exhausted` | Read the run log line `goal-lift committed 0 (reason: …)`. Bare `empty_queue` → frontier dry, **STOP**. `empty_queue_after_filter` / `_after_prescreen` / `_after_liftability` → committable frontier stalled: rotate to the next `--objects` cluster and continue; **STOP** if no rotation list remains. |
   | `infra_blocked` with `resumable: true` | Resume **once**: `Workflow({ scriptPath: ".claude/workflows/auto-session.js", resumeFromRunId: "<runId>" })`. Succeeded agents replay from cache. If it dies again, do **not** resume twice — salvage with step 7 and continue with a fresh run. |
   | `land_agent_null` | Almost always the gate exceeding the agent's Bash timeout, not a real failure. Rerun the gate (step 7); if it lands, continue. |
   | `parked` | Real signal about this work — merge conflict, failed gate, non-FF. **STOP.** Report `park_reason` + `conflicts` and hand off to `/reintegrate-to-main`. |
   | `guard_failed` / `no_commit` | **STOP** (guard) or count as a zero-commit run (no_commit). |

   After triage, **append one row to the campaign ledger**
   `artifacts/campaigns/campaigns.jsonl` (create the directory if missing) so
   campaigns are quantifiable and comparable across future selector/workflow
   changes. One JSON object per line:

   ```json
   {"ts": "<ISO-8601 finish time>", "campaign": "<branch>@<campaign start date>",
    "run": <n>, "branch": "<branch>", "objects": [...], "batches": N,
    "batch_goal": N, "functions_committed": N, "improve_promoted": N,
    "batches_landed": N, "batches_unlanded": N, "stopped_reason": "...",
    "tokens_spent": N, "wall_s": N}
   ```

   `tokens_spent` comes from the run's result JSON; `wall_s` is your own
   launch→completion timestamps (workflow scripts cannot read the clock).
   The ledger lives under gitignored `artifacts/` — force-track it once with
   `rtk git add -f artifacts/campaigns/campaigns.jsonl` and include it in the
   between-run or final chore commit (same precedent as the parked records).
   Compare later with e.g.
   `rtk jq -s 'group_by(.campaign) | map({c: .[0].campaign, fns: map(.functions_committed) | add, tokens: map(.tokens_spent) | add})' artifacts/campaigns/campaigns.jsonl`.

7. **Manual land-gate rerun** (used by the rows above, at most once per run):
   `rtk python3 tools/integrate/auto_reintegrate.py --branch <branch> --json`
   with an explicit `timeout: 600000`; if it still overruns, background it and
   poll for the JSON — never run two gates concurrently. `status: landed` →
   continue. `inconclusive` → main is still busy; continue with `noLand: true`.
   `parked` → STOP as above.
8. **Between runs**, re-run `park.py reconcile` — in-run lifts often supersede
   parked records. Commit the `--apply` result as its own chore commit.

## Stop conditions

- `--max-runs` reached.
- Frontier dry (`empty_queue`), or stalled with no rotation cluster left.
- Two **consecutive** zero-commit runs.
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
```
