---
name: opencode-command-replay-input
description: "Antigravity/Gemini wrapper for OpenCode /replay-input. Use when the user asks for /replay-input, the replay-input command, or says: Replay an existing deterministic controller-input fixture"
---

# OpenCode Command: /replay-input

This skill ports the existing OpenCode command `.opencode/commands/replay-input.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/replay-input` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Use `replay-input` and `input-replay-testing`.

Request: $ARGUMENTS

Locate the requested fixture under `input-recordings/levels/`, report its key metadata, then run the appropriate `rtk python3 tools/xbox/capture_scenario.py replay ...` command. If the user asks for build comparison, route to `ab-trajectory-testing`.
