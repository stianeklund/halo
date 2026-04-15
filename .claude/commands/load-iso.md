Spawn an Agent with `model: "haiku"` to handle all of the following work. Do not execute any steps yourself — pass the full task to the agent.

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
- `tools/xemu_qmp.py` handles path normalization and xemu discovery automatically
- If the script cannot handle a case, MCP remains an acceptable fallback
