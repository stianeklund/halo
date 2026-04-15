---
description: Hot-swap the patched ISO into xemu via xemu_qmp
agent: fast
subtask: true
---

Hot-swap the patched ISO into xemu using `tools/xemu_qmp.py` first. Use MCP
only if the script cannot perform the needed action.

Argument: $ARGUMENTS (optional: path to a specific ISO; defaults to `halo-patched.iso`)

Steps:
1. Determine the ISO path: use `$ARGUMENTS` if provided, otherwise `halo-patched.iso` in the repo root
2. If the ISO doesn't exist, check if `halo-patched/default.xbe` exists and suggest running `/build` first
3. Run `python3 tools/xemu_qmp.py --launch-if-missing load-iso <iso_path> --reset`
4. If that fails, use MCP only as a fallback
5. Report the result

Notes:
- `tools/xemu_qmp.py` handles discovery and path normalization automatically
- MCP remains available as a fallback for unsupported operations
