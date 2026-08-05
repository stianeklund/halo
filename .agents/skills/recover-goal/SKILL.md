---
name: recover-goal
description: "Goal-mode source recovery — repeatedly select the recovery frontier, baseline one object, run its source-recovery ladder through sequential category agents, commit each gated category, ratchet floors, and record the ledger until N objects finish or the queue stops."
---

# Recover Goal

`/recover-source` handles one TU. `/recover-goal` is the unattended multi-object
driver around it. It must preserve the Claude workflow's correctness properties:
the ledger is authoritative, category workers are sequential, each category has
one purity-gated commit, and failures are parked rather than hidden.

The OpenCode command is the driver. Invoke the `source-recovery` skill for the
ladder and use the `recovery-category` subagent for exactly one category at a
time. Do not replace the loop with a prose summary or parallel category tasks.

## Arguments

All are optional:

- Bare `N` or `--goal N`: objects to finish, default `1`.
- `--min-funcs N`: skip objects with fewer functions, default `10`.
- `--min-score X`: reject frontier rows below `X`.
- `--object <name>.obj`: explicit first object only; later iterations use the queue.
- `--allow-risky`: include `expr-simplify` and `control-flow`.
- `--dry-run`: plan and capture manifests, but write no ledger entries and make no commits.

## Required phases

1. **Guard:** fail if `src/`, `kb.json`, `tools/`, `.opencode/`, or `recovery/` has dirty/staged/untracked work. Tolerate `README.md`, `artifacts/`, and unrelated untracked files outside those trees. Do not modify the tree to make the guard pass.
2. **Select:** run `recovery_goal.py next --json`, or resolve the explicit object from `recovery_frontier.py --json`; then run `recovery_goal.py start` unless this is a dry run.
3. **Baseline:** build once, plan and capture one manifest per source TU, verify the matching COFF object, and aggregate pending counts by ladder category.
4. **Recover:** for each non-empty category in mandatory order, delegate one `recovery-category` task and wait for it to finish before starting the next category.
5. **Finalize:** ratchet VC71 floors, report each manifest, commit manifests/floor changes if needed, and call `recovery_goal.py finish` with every category and commit SHA. If no category applied, park with `no_applicable_items`.
6. **Report:** run `recovery_goal.py status` and report objects, category outcomes, parked reasons, commit SHAs, floors raised, stop reason, and whether the run is resumable.

## Stop conditions

- Goal reached: `N` objects finished.
- Queue exhausted: `recovery_goal.py next` exits `3`.
- Two consecutive parked objects share the same coarse cause: stop as `systemic_<class>`.
- Two consecutive whole-category failures on one object: park it as `category_failures` and continue.
- Finalization fails after category commits: stop as `finalize_failed`; never claim success.
- Agent/API failure after bounded retries: stop as `infra_blocked`, mark resumable, and do not restart from scratch.
- Dry-run queue repeats because the ledger is intentionally untouched: stop as `dry_run_queue_repeat`.

A park is a finding, not noise. Surface `park_reason` prominently. Never weaken a
gate, bypass purity, use `--no-verify`, or silently discard a manifest.
