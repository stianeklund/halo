# OpenCode + Ghidra MCP

This repository already contains Claude-oriented Ghidra MCP workflow hints:

- `.claude/settings.local.json` allows the `mcp__ghidra__...` tools.
- `.claude/commands/lift.md` defines a `/lift` workflow around those tools.

OpenCode can use the same MCP server, but the actual server definition is not
checked into this repository. To make OpenCode leverage it, configure an MCP
server named `ghidra` in your OpenCode config and then use the project command
at `.opencode/commands/lift.md`.

## What already works in OpenCode

- OpenCode reads `CLAUDE.md` as a fallback project rules file when `AGENTS.md`
  is absent, so this repo's main workflow guidance is already reused.
- OpenCode reads commands from `.opencode/commands/`, so this repo now ships an
  OpenCode `/lift` command that mirrors the Claude command.

## Required user-local MCP setup

Add a `ghidra` MCP server to either:

- `~/.config/opencode/opencode.json`
- or a project-local `opencode.json`

Example local MCP configuration:

```jsonc
{
  "$schema": "https://opencode.ai/config.json",
  "mcp": {
    "ghidra": {
      "type": "local",
      "command": ["<replace-with-your-ghidra-mcp-launcher>"]
    }
  }
}
```

If your Ghidra MCP server is remote instead:

```jsonc
{
  "$schema": "https://opencode.ai/config.json",
  "mcp": {
    "ghidra": {
      "type": "remote",
      "url": "https://<your-ghidra-mcp-endpoint>"
    }
  }
}
```

Use the server name `ghidra` so the prompt and tool naming stay consistent.

## Usage

1. Start OpenCode in this repo.
2. Make sure the `ghidra` MCP server is enabled and available.
3. Run `/lift <function-name-or-address>`.

Example:

```text
/lift game_state_dispose
```

## Notes

- The repo does not currently document the exact Ghidra MCP launcher command
  Claude Code was using, only the allowed tool names after that MCP was already
  connected.
- If you want this to be zero-config for all contributors, the missing piece is
  to check in a real `opencode.json` MCP definition once the canonical Ghidra
  MCP launch command or endpoint is known and stable.
