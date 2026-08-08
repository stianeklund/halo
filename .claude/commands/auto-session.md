---
description: Fully-automated lift session — run goal-lift in batches and land each batch into main (auto FF, park-on-conflict)
model: opus
subtask: false
---

This command is a thin dispatcher for the `auto-session` Workflow
(`.claude/workflows/auto-session.js`). All batch/lift/verify/commit/land logic
lives in that script, not here. It composes two existing blocks:

- the **`goal-lift`** workflow (selects targets, lifts, verifies, review-gates,
  commits to the current branch), run once per batch, and
- **`tools/integrate/auto_reintegrate.py`** — the mechanical, fail-closed
  branch→main gate that fast-forwards `main` only when the merge is
  conflict-free and every gate (kb.json object partition, reg-baseline drift,
  clean build, no-drop) is green, and otherwise **parks** and surfaces to you.

**Run this only from an isolated session branch, never on `main`.** The
workflow FF-advances `main` (in the main worktree) to this branch's tip after
each batch; a workflow cannot create its own worktree, so isolation is this
session's existing branch.

Argument: $ARGUMENTS

Parse from $ARGUMENTS (all optional):
- `--batches N` — maximum land cycles before stopping (default: 2).
- `--batch-goal N` — functions per `goal-lift` run (default: 12).
- `--no-land` — lift and commit normally, but never attempt to land. Use when
  `main` is known to be in use by another workstream; commits accumulate on the
  branch and go in as one fast-forward later.

  **Sizing.** Each batch carries ~7 min of serial fixed overhead (select 3.2 +
  retrieval-warm 1.6 + delink-prefetch 1.1 + liftability 0.6 + progress-log 0.5
  + preflight 0.2, measured 2026-08-04). So batch *count* is what costs, not
  batch size: 6x4 spends ~42 min on overhead to produce the same 24 functions
  that 2x12 produces for ~14 min. Prefer few large batches.

  The old advice here was "keep small — smaller branches land conflict-free more
  often", which is why the documented defaults were 6x4 while the script's were
  2x12. That rationale no longer holds: a dirty main worktree is classified
  `inconclusive` rather than `parked`, so the run keeps lifting and retries the
  land next batch instead of throwing the remaining batches away. Small batches
  now buy nothing and cost ~7 min each.
- `--objects obj1,obj2,...` — restrict goal-lift selection to these `.obj`
  files (comma-separated). Split into an array for `args.objects`.
- `--criteria "free text"` — freeform selection instruction passed to
  goal-lift (agent-interpreted, not mechanically enforced).
- `--dry-run` — run goal-lift in dry-run (no commits) and never land; reports
  what each batch would do.

## Steps

1. Print a one-line banner: `Auto-session: {batches} batches x {batch-goal} functions, land-to-main auto-FF`.
2. Call the Workflow tool — do not reimplement any of its steps inline:
   ```
   Workflow({ name: "auto-session", args: { batches: N, batchGoal: M, dryRun: <bool>, noLand: <bool>, objects: [<obj>, ...], criteria: "<text>" } })
   ```
   Omit `objects`/`criteria` when not passed (don't send `null`/empty).
3. The workflow runs in the background. Report the returned task info and
   mention `/workflows` for live progress.
4. When it completes, relay its final summary verbatim (batches landed,
   functions committed, stop reason). **If `park_reason` is set**, send a
   `PushNotification` with the park reason and any conflicts, because a human
   decision is required before more work can land — the branch has committed
   work sitting above `main` that the gate refused to auto-merge (a real
   kb.json/source conflict, a failed gate, or a dirty/locked main worktree).

## Usage

```bash
/auto-session                                  # up to 2 batches of 12, auto-land each
/auto-session --batches 1 --batch-goal 2 --dry-run   # trial: lift 2, gate, land nothing
/auto-session --batches 1 --batch-goal 2       # one real batch, then FF main
/auto-session --batches 3 --batch-goal 12 --objects real_math.obj,rasterizer.obj
/auto-session --batches 2 --batch-goal 12 --no-land  # main is busy; land later
```

## Landing policy (why it may stop early)

- **Auto-FF:** when the branch is conflict-free against `main` and all gates
  pass, `main` fast-forwards to the branch tip. No merge commit, trivially
  reversible.
- **Park-on-conflict:** any real merge conflict (kb.json object conflict is
  never hand-resolved — it once silently dropped 92 functions), failed gate,
  or dirty/locked main worktree → the batch does not land, the workflow
  **stops** (so the branch doesn't accumulate un-landable commits), and you are
  notified. Resolve with the `reintegrate-to-main` skill, then re-run.
