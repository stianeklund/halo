---
name: opencode-command-recover-source
description: "Antigravity/Gemini wrapper for OpenCode /recover-source. Use when the user asks for /recover-source, the recover-source command, or says: Run the manifest-driven faithful source recovery workflow"
---

# OpenCode Command: /recover-source

This skill ports the existing OpenCode command `.opencode/commands/recover-source.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/recover-source` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Invoke the `source-recovery` skill and follow it as the sole end-to-end
workflow for `$ARGUMENTS`. Use `tools/recovery/source_recovery.py` to create,
capture, check, report, and update the manifest. Do not perform speculative
automatic rewriting. Preserve Shape, keep `@<reg>` immutable, park any item
that fails evidence or a gate with a reason, and continue with independent
items. Do not commit unless the user explicitly requests a commit.
