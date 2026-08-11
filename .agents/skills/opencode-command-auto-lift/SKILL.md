---
name: opencode-command-auto-lift
description: "Antigravity/Gemini wrapper for OpenCode /auto-lift. Use when the user asks for /auto-lift, the auto-lift command, or says: Guarded OpenCode lift loop with automatic target selection"
---

# OpenCode Command: /auto-lift

This skill ports the existing OpenCode command `.opencode/commands/auto-lift.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/auto-lift` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Run the OpenCode-native automation driver. It selects one candidate at a time,
starts a separate OpenCode lift agent, accepts only a new clean commit after
the `goal90` verification gate, then selects the next candidate.

For existing helper subcommands, keep direct selector behavior:

```bash
rtk python3 tools/llm_auto_lift.py select|score|cache-context <remaining arguments>
```

Otherwise run:

```bash
rtk python3 tools/auto_lift_opencode.py $ARGUMENTS
```

Report its compact summary verbatim. Do not run additional lifting work in this
command session.

Usage:

```bash
/auto-lift --dry-run
/auto-lift --goal 1 --objects items.obj
/auto-lift --goal 5 --stop-on-fail 2 --criteria "prefer small leaf functions"
/auto-lift --batch 5
/auto-lift select --limit 20
```

Safety:

- Refuses `main` unless `--allow-main` is explicit.
- Refuses tracked changes in lift-owned paths; untracked `ci/` and artifacts do not block.
- Never resets, restores, or checks out a failed candidate. A dirty post-agent state stops safely.
- Default OpenCode CLI agent is `build`; override with `--agent` or `--model` only when registered locally.
