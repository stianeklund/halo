---
name: cleanup
tier: user
description: Deprecated alias — /cleanup <target> now runs the source-recovery workflow. Readability rewrites of already-lifted code are manifest-driven; see the source-recovery skill for the ladder, gates, and session rules.
---

# /cleanup — deprecated alias for `/recover-source`

`/cleanup <file | function | kb.json object> [--allow-risky]` means: **run the
source-recovery workflow on that target.**

There is exactly one orchestrator for readability/cleanup work on already-lifted
code. Read and follow `.claude/skills/source-recovery/SKILL.md`. The cleanup
ladder now lives there as the manifest's mandatory category ordering
(`comments` → `local-renames` → `symbol-names` → `const-enum` → `struct-define`
→ `offset-to-field` → `expr-simplify` → `control-flow`), enforced by
`tools/recovery/source_recovery.py`.

Nothing was dropped in the merge: the risk-ordered ladder table, one-commit-per-
category rule, session preconditions, regression protocol, floor ratchet, and
sequential subagent delegation are all in source-recovery. `--allow-risky` is now
a manifest flag set at `plan` time.

The satellite skills are unchanged and still apply:
`cleanup-report` (baseline, gap audit, and report template), `cleanup-regression-triage`, and every leaf category skill.
category skill.
