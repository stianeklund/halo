---
name: opencode-command-clear-cache
description: "Antigravity/Gemini wrapper for OpenCode /clear-cache. Use when the user asks for /clear-cache, the clear-cache command, or says: Clear Halo CE cache files from Xbox devkit cache partitions"
---

# OpenCode Command: /clear-cache

This skill ports the existing OpenCode command `.opencode/commands/clear-cache.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/clear-cache` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Clear Halo CE related cache files from Xbox devkit cache partitions via XBDM/RDCP.

Argument: $ARGUMENTS (optional flags passed to clear_cache.py, e.g. `--dry-run`, `-x 192.168.1.42`, `-v`)

Steps:
1. Load the `halo-clear-cache` skill.
2. Run `rtk python3 tools/xbox/clear_cache.py $ARGUMENTS`.
3. Report: items deleted per partition, total space freed, any failures.

Notes:
- **Always recommend `--dry-run` first** to preview deletions.
- Use `-x <host>` to target a specific Xbox IP.
- The tool is surgical; it does not wipe entire partitions.
- Requires a reachable Xbox devkit with XBDM running.
