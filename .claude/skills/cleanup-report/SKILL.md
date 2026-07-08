---
name: cleanup-report
tier: agent
triggers: ["cleanup report", "before/after report", "before after report", "cleanup pr", "cleanup summary"]
description: Produce the standard before/after report closing a cleanup session — baseline, per-category commits, verification evidence, match table (before → after), reverted/kept-raw items, risks, and follow-ups. Feeds the PR body / commit trailer and the next session's starting point.
---

# Cleanup PR Report

The report is the deliverable that makes cleanup reviewable: a reviewer should be able
to verify the "codegen unchanged" claim from the report alone, without re-deriving it.
Everything in it comes from artifacts the session already produced — no new analysis.

## Inputs

- The `cleanup-baseline` block (verbatim — it is the "before").
- The per-category commit list: `rtk git log --oneline <baseline-rev>..HEAD -- src/ kb.json`.
- Final gate outputs: `vc71_regression.py check` (all targets), `vc71_verify.py`
  per-function scores, `check_lift_hazards.py --changed-only`, and any behavioral
  oracle runs (equivalence seeds/coverage/confidence, golden results).
- Revert/park notes from the category skills (`kept-raw`, `kept-as-is`, parked patches).

## Template

```markdown
# Cleanup report — <TU/object> (<baseline-rev> → <head-rev>, <date>)

## Baseline
<the cleanup-baseline block, verbatim>

## Changes (one commit per category)
| commit | category | summary |
|---|---|---|
| <sha> | local renames | 34 locals in objects.c, mechanical vocab |
| <sha> | constants | 6 magic numbers → named (2 flag bits, 1 sentinel) |
| <sha> | struct/offsets | object_header defined (cs/co ×14); 41 raw offsets replaced |

## Verification
- vc71_regression check: PASS, 0 floors moved (attach the command + tail)
- match table: <fn> <before%> → <after%> (must be = or ↑ unless risky opt-in noted)
- hazards --changed-only: clean / pre-existing WARNs unchanged (list)
- behavioral oracles run (control-flow commits only): <tool, seeds, verdict>

## Not done / reverted
- <fn>: offset rewrite reverted — codegen-sensitive grouped stores (parked: <path>)
- <access>: left raw — stack-overlap alias (§2), commented at site

## Risks
- <anything a reviewer should re-check, e.g. a T2 name resting on one call site>

## Follow-ups
- <cleanup-gap-audit proposals, remaining raw offsets, next TU candidates>
```

## Rules

- **Match table is mandatory and honest**: every touched function, before → after.
  "Unchanged" is the success criterion; report it plainly, don't editorialize.
- Reverted/kept-raw items are first-class content — they save the next session from
  re-attempting them.
- Commit messages for cleanup are NOT lift commits: do not use
  `generate_lift_commit.py`; plain scoped messages
  (`cleanup(objects.c): mechanical local renames, match unchanged`) with the report as
  the PR body.
- The prepare-commit-msg hook (fixed 2026-07-08) skips explicit `-m`/`-F` messages
  and only prepopulates interactive editor commits. If the installed
  `.git/hooks/prepare-commit-msg` predates the fix (a stale copy instead of the
  symlink to `tools/hooks/prepare-commit-msg-hook.sh`), it will rewrite plain
  cleanup messages into lift-style ones — verify the symlink, or commit with
  `git -c core.hooksPath=/dev/null commit -F <msg>` after running the pre-commit
  checks manually (`extract_reg_args.py --check`, `check_lift_hazards.py --staged-only`).
- If floors were raised (`vc71_regression.py update`), say so and include the diff of
  `tools/verify/vc71_scores.json` in the same PR.
- File the report where the session artifacts live (PR body; long-form copy under
  `artifacts/cleanup_reports/` if the user wants a paper trail — that dir is
  host-local, don't commit it).
