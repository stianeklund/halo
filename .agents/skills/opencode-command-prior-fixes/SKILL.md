---
name: opencode-command-prior-fixes
description: "Antigravity/Gemini wrapper for OpenCode /prior-fixes. Use when the user asks for /prior-fixes, the prior-fixes command, or says: Search prior fixes, agent memory, commits, and retrieval outcomes before debugging"
---

# OpenCode Command: /prior-fixes

This skill ports the existing OpenCode command `.opencode/commands/prior-fixes.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/prior-fixes` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Run a local prior-fix lookup for the symptom or target, then use the returned
matches as hypotheses to confirm or reject.

Argument: $ARGUMENTS (symptom text, target function/address, or subsystem)

Required command:

```bash
rtk python3 tools/memory/prior_fixes.py "$ARGUMENTS"
```

Treat every match as a lead, not proof. Confirm against binary evidence,
disassembly, commit diffs, or runtime oracle before changing code.
