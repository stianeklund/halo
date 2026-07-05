---
name: opencode-command-permuter-campaign
description: "Antigravity/Gemini wrapper for OpenCode /permuter-campaign. Use when the user asks for /permuter-campaign, the permuter-campaign command, or says: Run or plan a VC71 permuter campaign for 85-98% low-match lifted functions"
---

# OpenCode Command: /permuter-campaign

This skill ports the existing OpenCode command `.opencode/commands/permuter-campaign.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/permuter-campaign` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Load the `permuter-campaign` skill and follow it exactly. If the Skill tool does not expose it, read `.claude/skills/permuter-campaign/SKILL.md` first.

Argument: $ARGUMENTS (optional shortlist, object, source file, or campaign limit)

Use this command when the user says `permuter`, `batch permute`, `low match`, `VC71 85-98%`, or `push stuck functions toward 100%`.

Before running workers, confirm the target is eligible: already ported, VC71 score in `[85, 98]%`, delinked reference exists, no known structural ceiling, source parses for the permuter, and the build is clean.

Report the candidate list, skipped targets with reasons, worker results, gate results, and any committed/reverted improvements.
