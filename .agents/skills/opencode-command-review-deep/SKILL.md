---
name: opencode-command-review-deep
description: "Antigravity/Gemini wrapper for OpenCode /review-deep. Use when the user asks for /review-deep, the review-deep command, or says: Run a deep code review on the deep agent"
---

# OpenCode Command: /review-deep

This skill ports the existing OpenCode command `.opencode/commands/review-deep.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/review-deep` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Perform a code review with a primary focus on bugs, regressions, risky assumptions, missing validation, and testing gaps.

Argument: $ARGUMENTS (optional file paths, symbols, or review scope)

Behavior:
1. If arguments are provided, focus the review on that scope.
2. If no arguments are provided, review the current uncommitted changes in the repository.
3. Findings must come first and be ordered by severity.
4. Include file and line references where possible.
5. Keep summaries brief and only after enumerating findings.
6. If no findings are discovered, say so explicitly and mention residual risks or testing gaps.
