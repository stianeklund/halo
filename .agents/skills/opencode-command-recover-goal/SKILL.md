---
name: opencode-command-recover-goal
description: "Antigravity/Gemini wrapper for OpenCode /recover-goal. Use when the user asks for /recover-goal, the recover-goal command, or says: Goal-mode source recovery across the frontier with sequential GPT-5.6 Luna category workers"
---

# OpenCode Command: /recover-goal

This skill ports the existing OpenCode command `.opencode/commands/recover-goal.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/recover-goal` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Run the `recover-goal` skill as a real bounded workflow for `$ARGUMENTS`. Do not
just explain the procedure. Execute the phases below and keep a compact progress
record in memory for the final report.

## Driver contract

Parse `N`/`--goal`, `--min-funcs`, `--min-score`, `--object`, `--allow-risky`, and
`--dry-run` as described by `recover-goal`. Defaults are goal `1`, minimum
functions `10`, no minimum score, and no risky categories.

Before any edit, run:

```bash
rtk git status --short
rtk git branch --show-current
```

Fail closed if any dirty, staged, or untracked path is under `src/`, `kb.json`,
`tools/`, `.opencode/`, or `recovery/`. Tolerate only `README.md`, `artifacts/`,
and unrelated untracked paths outside those trees. Never clean or revert someone
else's changes. Print `Recover-goal: finish N object(s) via the source-recovery ladder`
and the branch before proceeding.

## Select and ledger

For each iteration, until `finished == N`:

1. For an explicit first object, generate a frontier JSON and require that object
   to be in `eligible`; otherwise park it with the failed eligibility reason.
2. Otherwise run:

   ```bash
   rtk python3 tools/recovery/recovery_goal.py next --min-funcs M [--min-score X] --json
   ```

   Exit `3` is `queue_exhausted`, not an error. Any other failure gets at most two
   retries before `infra_blocked`/resumable.
3. Unless dry-run, take the row with:

   ```bash
   rtk python3 tools/recovery/recovery_goal.py start <object.obj>
   ```

   Never pass `--force` in the normal loop. If the object has no source files,
   park it with `select: frontier reported no source files`.

## Baseline

For every source file in the selected frontier row, do not edit source or commit:

```bash
rtk python3 tools/build/build.py -q --target halo
rtk python3 tools/recovery/source_recovery.py plan --source <source.c> -o recovery/<stem>.c.json [--allow-risky]
rtk fd '<stem>.c.obj' build/CMakeFiles
rtk python3 tools/recovery/source_recovery.py capture recovery/<stem>.c.json --object <matching-coff.obj>
rtk python3 tools/recovery/source_recovery.py ladder recovery/<stem>.c.json
```

Use a timeout of at least 600 seconds for the build and VC71 commands; never
start a second build while one is still running. If build, plan, COFF discovery,
or capture fails, park the object with a classified reason and continue. A
pre-existing build failure is an object blocker, not a reason to weaken gates.

Aggregate `pending=` counts across all manifests. In dry-run, print categories
that would run and stop after detecting the repeated queue row; do not call
`recovery_goal.py start`, `finish`, or `park`, and do not commit.

## Sequential category loop

Use this exact order, filtering risky entries unless `--allow-risky` was passed:

```text
comments        -> re-comment-capture
local-renames   -> local-var-cleanup
symbol-names    -> naming-confidence
const-enum      -> const-enum-recovery
struct-define   -> struct-recovery + struct-assert
offset-to-field -> offset-to-struct
expr-simplify   -> expr-simplify                 [risky]
control-flow    -> control-flow-cleanup           [risky]
```

For every category with pending items, call the `Task` tool exactly once with
`subagent_type="recovery-category"`, a short description, and a prompt that
includes:

- the object and category ID;
- all manifest and source paths;
- the built COFF path(s);
- the pending item count and IDs/line numbers;
- `AGENTS.md`, `source-recovery`, and the leaf skill as required reading;
- the same-worktree rule;
- the exact required gate and commit procedure;
- a request for the structured result described by the agent profile.

Wait for the task result before launching the next category. Never parallelize
categories for one object. A category result of `applied`, `parked`, or `skipped`
resets the consecutive failure count and allows later categories. A `failed` or
null result increments it; after two consecutive failures, leave prior gated
commits intact, remove only agent-created uncommitted source edits after checking
the status, run:

```bash
rtk python3 tools/recovery/recovery_goal.py park <object.obj> --reason "category_failures: ..."
```

Then continue to the next object unless the systemic park rule fires.

## Finalize

If at least one category applied, run:

```bash
rtk python3 tools/verify/vc71_regression.py update --source <all-source-files>
rtk python3 tools/recovery/source_recovery.py report <each-manifest>
rtk git add -- <manifests> tools/verify/vc71_scores.json
rtk git diff --cached --quiet
rtk git commit -m "recover(<stem>): session report"   # only if staged changes exist
rtk python3 tools/recovery/recovery_goal.py finish <object.obj> \
  --commit <each-category-sha> --commit <session-report-sha-if-any> \
  --category <each-applied-category>
```

The `finish` command is required. Never claim an object finished without its
ledger entry. If no category applied anything, report each manifest and park:

```bash
rtk python3 tools/recovery/recovery_goal.py park <object.obj> \
  --reason "no_applicable_items: every ladder category was skipped or fully parked"
```

If finalization fails after category commits, stop as `finalize_failed`; do not
silently convert it to success.

## Stop and report

Track the last park class. Two consecutive parks with the same class stop as
`systemic_<class>`. Reset the park streak after a productive finished object.
Apply a bounded iteration cap of `N * 4 + 4`.

At the end, run exactly:

```bash
rtk python3 tools/recovery/recovery_goal.py status
```

Report the status table verbatim, then summarize objects finished/parked,
per-object category outcomes, commit SHAs, floors raised, stop reason, park
reason, and `resumable` when infrastructure stopped the run. Do not re-derive or
hide a park reason.
