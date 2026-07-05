---
description: Goal-mode auto-lift — run until N functions committed at >=90% VC71 or queue exhausted
model: opus
subtask: false
---

This command is a thin dispatcher for the `goal-lift` Workflow
(`.claude/workflows/goal-lift.js`). All select/research/lift/verify/review/commit
logic lives in that script, not in this file. A prose command can only suggest
tool calls to the executing model — it cannot force them, and in practice the
required subagents were being skipped. The workflow's `agent()` calls are real
code, so `xbox-halo-re-analyst` (Phase 1 RE + implementation) and
`xbox-halo-lift-reviewer` (the fail-closed gate before every commit) are
guaranteed to run.

Argument: $ARGUMENTS

Parse from $ARGUMENTS (all optional):
- `--goal N` — commit this many functions before stopping (default: 20)
- `--stop-on-fail N` — stop after N consecutive failures (default: 3)
- `--dry-run` — evaluate candidates through the review gate but never commit
  (the working tree is reverted after each candidate either way, so nothing
  is left dirty for a later candidate to build on top of)
- `--objects obj1,obj2,...` — hard allowlist restricting selection to these
  `.obj` files (comma-separated, e.g. `real_math.obj,rasterizer.obj`). Split
  into an array before passing as `args.objects`. Enforced both in the
  Select prompt and mechanically in code after the agent returns candidates.
- `--criteria "free text"` — freeform instruction appended to the Select
  prompt on top of the built-in rules (e.g. "prefer functions under 40
  instructions"). Agent-interpreted only — not mechanically enforced.

## Steps

1. Print a one-line banner: `Goal: lift {N} functions at >=90% VC71` (`{N}` =
   parsed `--goal`, default 20). `/goal` itself is a separate UI command and
   cannot be invoked programmatically — this banner is just the visible
   marker of what the workflow below is about to do, for whoever is pairing
   this with `/goal` manually.
2. Call the Workflow tool — do not reimplement any of its steps inline:
   ```
   Workflow({ name: "goal-lift", args: { goal: N, stopOnFail: M, dryRun: <bool>, objects: [<obj>, ...], criteria: "<text>" } })
   ```
   Omit `objects`/`criteria` from the args object entirely when not passed on
   the command line (don't pass `null` or empty string/array).
3. The workflow runs in the background. Report the returned task info and
   mention `/workflows` for live progress.
4. When the workflow completes, relay its final summary verbatim (goal
   reached or not, committed/skipped/reverted counts, stop reason) — do not
   re-derive or second-guess it.

## Usage

```bash
/goal-lift                      # run until 20 committed or queue exhausted
/goal-lift --goal 50            # run until 50 committed
/goal-lift --goal 10 --dry-run  # trial run, no commits
/goal-lift --goal 20 --objects real_math.obj,rasterizer.obj   # restrict to these objects only
/goal-lift --goal 15 --criteria "prioritize bipeds.obj and actor_combat.obj; skip anything touching D3D or rasterizer"
/goal-lift --goal 20 --objects units.obj,actors.obj --criteria "prefer smaller functions (under 40 instructions) first"
```
