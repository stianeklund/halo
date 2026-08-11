---
description: Unattended lift campaign — supervise repeated /auto-session runs, resolve known parks, stop on a dry frontier or real conflict
agent: build
---

This command is a **supervisor loop around `/auto-session`**, run by you, the
main session agent. It owns no lift, verify, commit, or merge logic — all of
that lives in `.opencode/commands/auto-session.md` and, below it,
`goal-lift.md` and `tools/integrate/auto_reintegrate.py`. There is no
Workflow-script engine or background task runner in this environment (unlike
the Claude Code version of this command), so you drive the outer loop
yourself, in-process: run one full `/auto-session` invocation to completion,
triage its final report, then decide whether to start the next one. What this
adds over calling `/auto-session` directly once: it re-launches runs, triages
each stop reason, resolves the *known-benign* ones (regen dirt, transient
infra), rotates `--objects` clusters when a frontier stalls, and stops on the
ones a human must own.

**Run only from an isolated session branch, never on `main`** — same
rationale as `/auto-session`: each run fast-forwards `main` (in the main
worktree) to this branch's tip.

Argument: $ARGUMENTS

Parse from $ARGUMENTS (all optional):
- `--max-runs N` — auto-session runs before stopping (default: 6).
- `--batches N` / `--batch-goal N` — passed through to every run unchanged;
  if omitted, `/auto-session`'s own defaults apply.
- `--objects obj1,obj2,...` — restrict selection. Multiple clusters may be
  given as `--objects a.obj,b.obj --objects c.obj` (repeatable); extras
  become the **rotation list** used when a cluster's committable frontier
  stalls.
- `--criteria "free text"` — freeform selection instruction (pass-through).
- `--no-land` — never attempt to land; commits accumulate on the branch.
- `--dry-run` — pass through to every run; nothing commits and nothing
  lands.

There is no `--lift-reg-args` flag here — `.opencode/commands/goal-lift.md`
has no override for its register-arg pre-screen, so `@<reg>` targets are
always skipped by every run this command launches.

## Preflight (once)

1. **Branch guard.** `rtk git branch --show-current`. If `main`, STOP — do
   not start.
2. **Reconcile the parked ledger.** `rtk python3 tools/lift/park.py
   reconcile` (dry run by default). If it reports stale records, re-run with
   `--apply`, then `rtk git add -- artifacts/parked` and commit as a
   standalone chore commit. `artifacts/` is gitignored but the parked
   records are force-tracked, so only already-tracked records stage — that
   is correct. Commit nothing if nothing stages.
3. **Find and triage the main worktree.** `rtk git worktree list` — record
   the path whose entry shows `[main]`. Then `rtk git -C <main-worktree>
   status --short` (tracked only). The land gate absorbs exactly this
   generated set (`_ABSORB_EXACT` / `_ABSORB_PREFIXES` in
   `auto_reintegrate.py`): `README.md` (stats block only),
   `tools/verify/vc71_scores.json`, `tools/objects.csv`,
   `tools/equivalence/leaf_cache.json`, `artifacts/batch_verify/**`.
   **Anything else is authored WIP** — a `.py`, `.c`, `.yml`,
   `kb_meta.json`, or a README diff outside the stats block. Do NOT absorb,
   stash, or commit it. Note it in the final report and run the whole
   campaign with `--no-land`; commits accumulate on the branch and land in
   one fast-forward later.

## Run loop (repeat until a stop condition)

4. **Run `/auto-session` to completion.** Follow the full procedure in
   `.opencode/commands/auto-session.md` exactly as it stands alone —
   branches, batches, and lands as described there — passing through
   `--batches`, `--batch-goal`, `--objects`, `--criteria`, `--no-land`
   (forced on if step 3 found authored WIP), and `--dry-run`. This is one
   "run": it may execute several batches internally before its own loop
   stops. Do not start a second run until this one's final report has
   printed.
5. **Triage that run's final report** (batches attempted/landed, total
   functions committed, stop reason):

   | Run's stop reason | Action |
   |---|---|
   | `batches_exhausted` | Clean run. Continue to the next run. |
   | `queue_exhausted` | `.opencode/commands/goal-lift.md` reports this one way — it does not distinguish a truly dry global frontier from a filtered-empty one. Treat accordingly: if this run had `--objects` scoping, rotate to the next `--objects` cluster and continue; **STOP** if no rotation list remains. If this run had no `--objects` scoping (global selection), the frontier itself is dry — **STOP**. |
   | infra block (Ghidra bridge down, build broken unrelated to the lift, repeated same failure) reported by the run without recovering | There is no run-level checkpoint to resume from here — retry the **whole run** once with identical arguments. If it fails the same way twice, **STOP** and report the failure. |
   | `parked: <reason>` or `inconclusive: <reason>` (from the run's own land-gate calls) | Committed work is sitting on the branch above `main`. Rerun the land gate once yourself (step 6). If it lands, continue to the next run. If it parks/is inconclusive again, **STOP** — report the reason and conflicts, and hand off to `/reintegrate-to-main`. |

   After triage, **append one row to the campaign ledger**
   `artifacts/campaigns/campaigns.jsonl` (create the directory if missing) so
   campaigns are quantifiable and comparable across future selector/procedure
   changes. One JSON object per line:

   ```json
   {"ts": "<ISO-8601 finish time>", "campaign": "<branch>@<campaign start date>",
    "run": <n>, "branch": "<branch>", "objects": [...], "batches": N,
    "batch_goal": N, "functions_committed": N, "batches_landed": N,
    "batches_unlanded": N, "stopped_reason": "...", "wall_s": N}
   ```

   `wall_s` is your own launch→completion timestamps for that run (there is
   no separate process to read the clock from). The ledger lives under
   gitignored `artifacts/` — force-track it once with `rtk git add -f
   artifacts/campaigns/campaigns.jsonl` and include it in the between-run or
   final chore commit (same precedent as the parked records).
   Compare later with e.g.
   `rtk jq -s 'group_by(.campaign) | map({c: .[0].campaign, fns: map(.functions_committed) | add})' artifacts/campaigns/campaigns.jsonl`.

6. **Manual land-gate rerun** (used by the `parked`/`inconclusive` row
   above, at most once per run):
   `rtk python3 tools/integrate/auto_reintegrate.py --branch <branch>
   --json` with an explicit generous timeout; if it still overruns,
   background it and poll for the JSON — never run two gates concurrently.
   `status: landed` → continue. `inconclusive` → main is still busy;
   continue the campaign with `--no-land` from here on. `parked` → STOP as
   above.
7. **Between runs**, re-run `park.py reconcile` — in-run lifts often
   supersede parked records. Commit the `--apply` result as its own chore
   commit.

## Stop conditions

- `--max-runs` reached.
- Frontier dry, or stalled with no rotation cluster left.
- Two **consecutive** zero-commit runs.
- A real park: a merge conflict, a failed gate, or authored WIP found in the
  main worktree.

## Final report

Report, in this order: runs completed / max; total functions committed;
batches landed vs. still unlanded; every park encountered and how it was
resolved (rerun gate / retried the run / rotated / stopped); which branch the
commits sit on and whether they are in `main` yet. Include the campaign's
ledger totals (functions committed, wall time) and, when a prior campaign row
exists in `campaigns.jsonl`, the per-function cost next to the previous
campaign's for comparison.

Close with the explicit note that **promotion to `main` stays human-gated**
when the campaign ran with `--no-land` or ended unlanded — land it
deliberately with `rtk python3 tools/integrate/auto_reintegrate.py --branch
<branch> --json` once `main` is idle (the gate always targets `main`; there
is no `--target` flag), or use `/reintegrate-to-main` if the gate parks.

## Usage

```bash
/campaign                                          # 6 runs, auto-session's own defaults
/campaign --max-runs 3 --batch-goal 8
/campaign --max-runs 4 --objects units.obj,actors.obj --objects effects.obj
/campaign --max-runs 2 --no-land                   # main busy; land once at the end
/campaign --max-runs 1 --dry-run                   # trial: nothing commits, nothing lands
```
