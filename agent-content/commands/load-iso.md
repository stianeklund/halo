---
description: Hot-swap the patched ISO into xemu via xemu_qmp
agent: fast
subtask: true
---

Use the `halo-build-xemu` skill for the repo's preferred xemu load workflow.

Hot-swap the patched ISO into xemu using `tools/xemu_qmp.py` first. Use MCP
only if the script cannot perform the needed action.

Argument: $ARGUMENTS (optional: path to a specific ISO; defaults to `halo-patched.iso`)

Steps:
1. Determine the ISO path: use `$ARGUMENTS` if provided, otherwise
   `halo-patched.iso` in the repo root.
2. Follow the load-and-reset flow from `halo-build-xemu`.
3. Report the result.

Notes:
- `tools/xemu_qmp.py` handles discovery and path normalization automatically.
- MCP remains available as a fallback for unsupported operations.
