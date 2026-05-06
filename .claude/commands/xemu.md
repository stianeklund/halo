---
description: Control xemu and hot-swap the patched ISO
agent: fast
subtask: true
---

Use the `halo-build-xemu` skill for repo-specific xemu control rules.

Build/load workflows and xemu control should go through this command. Hot-swap
the patched ISO into xemu using `tools/xbox/xemu_qmp.py`, or launch xemu if not
already running. Use MCP only as a fallback.

Argument: $ARGUMENTS (`load [iso]`, `build-load`, optional flags like `-m` for
monitor, `-T` for trace, `-q` for QMP, or a path to a specific ISO)

Steps:
1. If mode is `build-load`, follow `/build` first, then load the resulting ISO.
2. Determine the ISO path: if a positional arg is given use that, otherwise
   `halo-patched.iso` in the repo root.
3. Follow the xemu control guidance from `halo-build-xemu`.
4. Report the result.

Notes:
- `tools/xbox/xemu_qmp.py` handles discovery and path normalization automatically.
- MCP remains available as a fallback for unsupported operations.
