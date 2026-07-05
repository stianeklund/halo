---
name: opencode-command-xemu
description: "Antigravity/Gemini wrapper for OpenCode /xemu. Use when the user asks for /xemu, the xemu command, or says: Build, deploy, and run on xemu or real Xbox"
---

# OpenCode Command: /xemu

This skill ports the existing OpenCode command `.opencode/commands/xemu.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/xemu` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Use the `halo-build-xemu` skill for repo-specific build/deploy rules.

## Canonical Command

```
./tools/xbox/build_deploy_run.sh -q
```

This single command builds and deploys via XBDM. No ISO step needed.

Argument: $ARGUMENTS (optional flags like `-q` for quiet, `--xbox <host>` for
target override)

Steps:
1. Run `./tools/xbox/build_deploy_run.sh -q` (or with `--xbox <host>` for real
   hardware).
2. If the build fails, stop and report the concrete errors.
3. Report the result.

Notes:
- For real Xbox: `./tools/xbox/build_deploy_run_real_hw.sh -q`
- `tools/xbox/xemu_qmp.py` remains available for monitor-only control (status,
  reset, etc.).
- MCP remains available as a fallback for unsupported operations.
