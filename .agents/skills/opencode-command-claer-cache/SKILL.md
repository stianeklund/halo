---
name: opencode-command-claer-cache
description: "Antigravity/Gemini wrapper for OpenCode /claer-cache. Use when the user asks for /claer-cache, the claer-cache command, or says: Alias for /clear-cache (kept for typo compatibility)"
---

# OpenCode Command: /claer-cache

This skill ports the existing OpenCode command `.opencode/commands/claer-cache.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/claer-cache` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Alias command for `/clear-cache`.

Clear Halo CE related cache files from Xbox devkit cache partitions via XBDM/RDCP,
and clear local xemu shader cache files.

Argument: $ARGUMENTS (optional flags passed to clear_cache.py, e.g. `--dry-run`, `-x 192.168.1.42`, `-v`)

Steps:
1. Load the `halo-clear-cache` skill.
2. Run `rtk python3 tools/xbox/clear_cache.py $ARGUMENTS`.
3. Report: Xbox items deleted per partition, local xemu items deleted, and any failures.
