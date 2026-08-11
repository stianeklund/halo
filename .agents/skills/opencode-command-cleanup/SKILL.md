---
name: opencode-command-cleanup
description: "Antigravity/Gemini wrapper for OpenCode /cleanup. Use when the user asks for /cleanup, the cleanup command, or says: Run the evidence-preserving source-recovery ladder on already-lifted code"
---

# OpenCode Command: /cleanup

This skill ports the existing OpenCode command `.opencode/commands/cleanup.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/cleanup` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

`source-recovery` is the neutral subset/alias of `/recover-source` for already-lifted
code. Use `/recover-source` for the complete manifest-driven workflow,
including evidence-backed corrective fidelity work.

Use `source-recovery` plus its support skills, in ladder order: `cleanup-report`
(pre-flight), `re-comment-capture`, `name-cleanup` (local renames + const/enum),
`naming-confidence`, `struct-recovery` (+ Phase 2), `header-recovery`,
`offset-to-struct`, and — opt-in only — `expr-simplify` and
`control-flow-cleanup`. `cleanup-regression-triage` isolates a match/test
regression caused by this work.

Target: $ARGUMENTS

Follow the source-recovery ladder in order. Default to codegen-preserving work only; do not enter expression simplification or control-flow source-recovery unless the user explicitly requests risky source-recovery. Stop if the working tree is not suitable, the target is not already lifted/committed, or any verification gate regresses.
