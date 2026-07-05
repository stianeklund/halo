---
name: opencode-command-debug
description: "Antigravity/Gemini wrapper for OpenCode /debug. Use when the user asks for /debug, the debug command, or says: Universal debugging entry point for crash, hang, assert, visual regression, build failure, deploy failure, or wrong behavior"
---

# OpenCode Command: /debug

This skill ports the existing OpenCode command `.opencode/commands/debug.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/debug` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Use the `debug` skill for symptom routing and inline debugging procedures.

Argument: $ARGUMENTS (describe the symptom: crash output, assert text, visual
regression, build error, or behavior description)

First run prior-fix lookup:

```bash
rtk python3 tools/memory/prior_fixes.py "$ARGUMENTS"
```

Then load the recommended skill(s) from the lookup output and follow the debug
skill's symptom router. Treat matches as hypotheses only; confirm against
binary/disassembly/runtime evidence before fixing code.
