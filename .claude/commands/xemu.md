Spawn an Agent with `model: "haiku"` to handle all of the following work. Do not execute any steps yourself — pass the full task to the agent.

Hot-swap the patched ISO into xemu using `tools/xemu_qmp.py`, or launch xemu
if not already running. Use MCP only as a fallback.

Argument: $ARGUMENTS (optional flags like "-m" for monitor, "-T" for trace, "-q" for QMP, or a path to a specific ISO)

Steps:
1. Determine the ISO path: if a positional arg is given use that, otherwise `halo-patched.iso` in the repo root
2. If the ISO doesn't exist, check if `halo-patched/default.xbe` exists and suggest running `/build` first
3. Prefer `python3 tools/xemu_qmp.py --launch-if-missing load-iso <iso_path> --reset`
4. If the user asked for monitor-only control, use `python3 tools/xemu_qmp.py status|reset|stop|cont|eject|hmp ...` as appropriate
5. If the script fails and the action is still needed, use MCP as a fallback
6. If `-m` flag was used, remind the user they can query with `tools/xemu-mon.py "info registers"` etc.
7. Report the result

Notes:
- `tools/xemu_qmp.py` handles discovery and path normalization automatically
- MCP remains available as a fallback for unsupported operations
