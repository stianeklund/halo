---
name: recover-goal
tier: user
description: Goal-mode readability recovery — loop the recovery frontier, run the source-recovery ladder per object, park on failure, until N objects are recovered or the queue is exhausted. The unattended driver around /recover-source.
---

# /recover-goal — Goal-Mode Source Recovery

Thin dispatcher for the `recover-goal` **Workflow**
(`.claude/workflows/recover-goal.js`). All select/baseline/ladder/commit/park/
finish logic lives in that script, not in this file.

`/recover-source` recovers **one** TU. This runs it object after object
unattended: take the next target off the recovery frontier, work the ladder one
category per **sequential** Opus agent (one purity-gated commit each), ratchet the
VC71 floors, record the outcome in `recovery/goal_ledger.json`, repeat. It adds no
gates and weakens none.

It is a workflow rather than prose because a prose command can only *suggest* tool
calls, and two things kept getting skipped: **one agent per category** (each
category's commit is the next one's base, and a combined diff fails
`check_category_purity.py`) and **the ledger** (skipping `start`/`finish`/`park`
makes the loop re-take an object it already parked, forever). In the workflow both
are real code.

Doctrine stays in **`source-recovery`** (`.claude/skills/source-recovery/SKILL.md`)
— the ladder order, the gate table, the measurement traps, the session rules. The
workflow points every category agent at it; do not restate or reinterpret it here.

Argument: $ARGUMENTS

Parse from $ARGUMENTS (all optional):
- `N` (bare positional) or `--goal N` — objects to **finish** before stopping
  (default: 1). Keep it small: each object is several Opus agents.
- `--min-funcs N` — skip trivially small objects (default: 10).
- `--min-score X` — refuse objects scored below X on the recovery frontier.
- `--object <name>.obj` — explicit first target, bypassing selection for that one
  iteration only; later iterations fall back to the frontier queue.
- `--allow-risky` — pass `--allow-risky` to `plan`, opening ladder categories 7–8
  (`expr-simplify`, `control-flow`). Without it the manifest refuses those items;
  that is the safe default.
- `--dry-run` — baseline each object and report the per-category debt and what
  *would* run. Writes no ledger entry and no commit; the only tree effect is the
  `recovery/<stem>.c.json` manifest(s) that `plan`/`capture` produce.

## Steps

1. Print a one-line banner: `Recover-goal: finish {N} object(s) via the source-recovery ladder`.
2. Call the Workflow tool — do not reimplement any of its steps inline:
   ```
   Workflow({ name: "recover-goal", args: { goal: N, minFuncs: M, minScore: X, object: "<name>.obj", allowRisky: <bool>, dryRun: <bool> } })
   ```
   Omit every key that was not passed on the command line (don't send `null`,
   `0`, or an empty string).
3. The workflow runs in the background. Report the returned task info and mention
   `/workflows` for live progress.
4. When it completes, relay its final summary **verbatim** — objects finished vs
   parked, per-object category outcomes, commit shas, floors raised, stop reason,
   and the ledger table. Do not re-derive or second-guess it. If `park_reason` is
   set, surface it prominently: a park is a finding, not noise.

## Usage

```bash
/recover-goal                                   # finish 1 object
/recover-goal 3                                 # finish 3 objects
/recover-goal --goal 3 --dry-run                # show the debt and plan, change nothing
/recover-goal --object hud_weapon.obj           # start on a specific object
/recover-goal 2 --min-funcs 15 --min-score 80   # only substantial, high-scoring objects
/recover-goal 1 --allow-risky                   # opt in to expr-simplify / control-flow
```

## Stop conditions (why it may end early)

- **Goal reached** — N objects finished.
- **Queue exhausted** — `recovery_goal.py next` exits 3. An exhausted queue with
  everything parked is a finding, not success.
- **`systemic_<class>`** — two consecutive objects parked for the same *class* of
  reason (both build failures, both capture failures). Stopping beats filling the
  ledger with parks that hide the one real cause.
- **Object aborted** — 2 consecutive category failures on one object: uncommitted
  edits are discarded, already-gated category commits are kept, the object is
  parked, and the loop moves on.
- **`guard_failed`** — the tree is dirty under `src/`, `kb.json`, `tools/`,
  `.claude/`, or `recovery/`. A recovery commit must not pick up someone else's
  in-flight lift. (`README.md` and `artifacts/` are tolerated as known noise.)
- **`infra_blocked`** — an agent returned null (API outage) after bounded retries.
  The result carries `resumable: true`: resume the run rather than restarting it,
  once the outage has cleared.

## See also

`source-recovery` is the single source of truth per object.
`cleanup-regression-triage` handles a regression noticed *after* a commit landed
(the per-category commits make it a one-commit bisect); `cleanup-report` is the
human-facing write-up. `goal-lift` / `auto-session` are the same goal-mode shape
for *lifting* — mirror their fail-closed posture, not their target selection.
