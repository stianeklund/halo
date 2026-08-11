---
name: opencode-command-capture-input
description: "Antigravity/Gemini wrapper for OpenCode /capture-input. Use when the user asks for /capture-input, the capture-input command, or says: Capture a deterministic controller-input fixture for Halo CE Xbox testing"
---

# OpenCode Command: /capture-input

This skill ports the existing OpenCode command `.opencode/commands/capture-input.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/capture-input` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Use `input-fixture` and `input-replay-testing`.

Request: $ARGUMENTS

Run the fixture capture wizard behavior: identify level, scenario name, build, start mode, difficulty, tail padding, title, and purpose; then drive `rtk python3 tools/xbox/capture_scenario.py` with the selected options. Ask concise questions only for missing choices that affect the command.
