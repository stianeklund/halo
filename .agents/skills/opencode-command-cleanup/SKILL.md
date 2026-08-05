---
name: opencode-command-cleanup
description: "Antigravity/Gemini wrapper for OpenCode /cleanup. Use when the user asks for /cleanup, the cleanup command, or says: Run the evidence-preserving cleanup ladder on already-lifted code"
---

# OpenCode Command: /cleanup

This skill ports the existing OpenCode command `.opencode/commands/cleanup.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/cleanup` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

`cleanup` is the neutral subset/alias of `/recover-source` for already-lifted
code. Use `/recover-source` for the complete manifest-driven workflow,
including evidence-backed corrective fidelity work.

Use `cleanup` plus its support skills: `cleanup-baseline`, `cleanup-gap-audit`, `re-comment-capture`, `local-var-cleanup`, `naming-confidence`, `const-enum-recovery`, `struct-recovery`, `struct-assert`, `offset-to-struct`, `cleanup-regression-triage`, and `cleanup-report`.

Target: $ARGUMENTS

Follow the cleanup ladder in order. Default to codegen-preserving work only; do not enter expression simplification or control-flow cleanup unless the user explicitly requests risky cleanup. Stop if the working tree is not suitable, the target is not already lifted/committed, or any verification gate regresses.
