---
description: Fully-automated lift session — run goal-lift in batches and land each batch into main (auto FF, park-on-conflict)
agent: build
model: openai/gpt-5.6-luna
---

Run `/goal-lift` in batches and land each batch into `main` with
`tools/integrate/auto_reintegrate.py` — the mechanical, fail-closed
branch→main gate that fast-forwards `main` only when the merge is
conflict-free and every gate (kb.json object partition, reg-baseline drift,
clean build, no-drop) is green, and otherwise **parks** and surfaces to the
user. There is no Workflow-script engine here, so this command drives the
loop itself, in-process, one batch at a time — do not skip steps or run
batches concurrently.

**Run this only from an isolated session branch, never on `main`.** Each
successful batch fast-forwards `main` (in the main worktree) to this
branch's tip; a command cannot create its own worktree, so isolation is
this session's existing branch. Check `rtk git branch --show-current`
before starting; refuse and report if it is `main`.

Argument: $ARGUMENTS

Parse from $ARGUMENTS (all optional):
- `--batches N` — maximum land cycles before stopping (default: 6).
- `--batch-goal N` — functions per batch (default: 4). Keep small: smaller
  branches land conflict-free far more often and shrink the no-drop surface.
- `--objects obj1,obj2,...` — restrict target selection to these `.obj`
  files (comma-separated), passed through to the batch's target selection.
- `--criteria "free text"` — freeform selection instruction for the batch
  (agent-interpreted, not mechanically enforced).
- `--dry-run` — run each batch in dry-run (no commits) and never land;
  reports what each batch would do.

## Steps

1. Print a one-line banner: `Auto-session: {batches} batches x {batch-goal} functions, land-to-main auto-FF`.
2. Confirm the current branch is not `main` (see above). Record it as `{branch}`.
3. For `batch = 1..batches`:
   a. Run the procedure in `.opencode/commands/goal-lift.md` for this batch,
      with `--goal {batch-goal}` and, if given, `--objects`/`--criteria`/`--dry-run`
      forwarded unchanged. This selects, lifts, verifies, and commits up to
      `{batch-goal}` functions to `{branch}` exactly as `/goal-lift` does
      standalone — the only difference is the batch cap and what happens next.
   b. If the batch committed zero functions (queue exhausted or every
      candidate infra-blocked), stop the loop now — report `stop_reason:
      queue_exhausted` (or the specific infra block) and skip land/reintegrate
      for this batch.
   c. Unless `--dry-run`, land the batch:
      ```bash
      rtk python3 tools/integrate/auto_reintegrate.py --branch {branch} --json
      ```
      With `--dry-run`, run the same command with `--dry-run --json` added and
      never let it advance `main`.
   d. Parse the JSON result's `status` field:
      - `landed` (exit 0) — batch is on `main`. Continue to the next batch.
      - `parked` (exit 3) or `inconclusive` (exit 2) — a real merge conflict
        (kb.json object conflict is never hand-resolved — it once silently
        dropped 92 functions), a failed gate, or a dirty/locked main
        worktree. **Stop the loop immediately** — do not start another
        batch; committed work is sitting on `{branch}` above `main` and
        needs a human decision. Report the `reason` field verbatim and point
        at the `reintegrate-to-main` skill for manual resolution, then
        re-running `/auto-session` to resume.
4. When the loop ends (batches exhausted, queue exhausted, or a park/inconclusive
   stop), print a final report:
   - Batches attempted / landed.
   - Total functions committed across all landed batches.
   - Stop reason (`batches_exhausted` / `queue_exhausted` / infra block /
     `parked: <reason>` / `inconclusive: <reason>`).
   - If parked or inconclusive, state clearly that a human decision is
     required before more work can land, and surface this prominently to the
     user (do not bury it at the end of a long log) — this is not routine
     progress, it means committed work above `main` is stuck.

## Usage

```bash
/auto-session                                  # up to 6 batches of 4, auto-land each
/auto-session --batches 1 --batch-goal 2 --dry-run   # trial: lift 2, gate, land nothing
/auto-session --batches 1 --batch-goal 2       # one real batch, then FF main
/auto-session --batches 10 --batch-goal 4 --objects real_math.obj,rasterizer.obj
```

## Landing policy (why it may stop early)

- **Auto-FF:** when the branch is conflict-free against `main` and all gates
  pass, `main` fast-forwards to the branch tip. No merge commit, trivially
  reversible.
- **Park-on-conflict:** any real merge conflict, failed gate, or
  dirty/locked main worktree → the batch does not land, the loop **stops**
  (so the branch doesn't accumulate un-landable commits), and the user is
  notified. Resolve with the `reintegrate-to-main` skill, then re-run.
