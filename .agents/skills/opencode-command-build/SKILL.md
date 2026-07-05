---
name: opencode-command-build
description: "Antigravity/Gemini wrapper for OpenCode /build. Use when the user asks for /build, the build command, or says: Build the project and produce a patched XBE"
---

# OpenCode Command: /build

This skill ports the existing OpenCode command `.opencode/commands/build.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/build` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Use the `halo-build-xemu` skill for the standard build workflow.

Build the project with `tools/build/build.py`. The patched XBE lands in
`halo-patched/default.xbe`. Never create ISOs — the user loads the XBE
into xemu themselves.

Argument: $ARGUMENTS (unused)

Steps:
1. Run `rtk python3 tools/build/build.py -q --target halo`.
2. If the build succeeds, run `git rev-parse HEAD` and include the full commit
   hash in the report.
3. Report: build status, commit hash, any warnings.
