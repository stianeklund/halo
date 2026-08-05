---
name: opencode-command-reintegrate-to-main
description: "Antigravity/Gemini wrapper for OpenCode /reintegrate-to-main. Use when the user asks for /reintegrate-to-main, the reintegrate-to-main command, or says: Safely reintegrate a lift/session worktree branch into main"
---

# OpenCode Command: /reintegrate-to-main

This skill ports the existing OpenCode command `.opencode/commands/reintegrate-to-main.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/reintegrate-to-main` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Use `reintegrate-to-main`.

Request: $ARGUMENTS

Follow the safe branch integration workflow: inspect branch/worktree state, bring the branch up to date, run the required kb.json partition/build/no-drop gates, and fast-forward main only when evidence supports it. Do not use destructive git commands.
