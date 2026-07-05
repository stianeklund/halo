---
name: opencode-command-maintain
description: "Antigravity/Gemini wrapper for OpenCode /maintain. Use when the user asks for /maintain, the maintain command, or says: Run maintain.py and report the resulting changes"
---

# OpenCode Command: /maintain

This skill ports the existing OpenCode command `.opencode/commands/maintain.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/maintain` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Run the maintain script and report any resulting worktree changes.

Steps:
1. If `$ARGUMENTS` is provided, pass it through to `python3 tools/analysis/maintain.py`.
2. Run `python3 tools/analysis/maintain.py $ARGUMENTS`.
3. Check whether the repository gained new modifications after the run.
4. Report the maintain result and the affected files concisely.
