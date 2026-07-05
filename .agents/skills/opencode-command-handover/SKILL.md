---
name: opencode-command-handover
description: "Antigravity/Gemini wrapper for OpenCode /handover. Use when the user asks for /handover, the handover command, or says: Produce a concise continuation handover for a new agent"
---

# OpenCode Command: /handover

This skill ports the existing OpenCode command `.opencode/commands/handover.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/handover` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Use the `handover` skill.

Produce a concise handover document for the current task or goal. Summarize
what we were working on, important changes, current state, validation status,
risks or uncertainties, and what remains. If the work is complete, say that and
summarize the final state instead of inventing continuation steps.

Argument: $ARGUMENTS (optional focus area)

Do not continue implementation work. Do not perform broad repo exploration.
Only use lightweight checks needed to avoid misleading the next agent.
