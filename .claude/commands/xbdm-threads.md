Spawn an Agent with `model: "haiku"` to handle all of the following work. Do not execute any steps yourself — pass the full task to the agent.

Query XBDM for the current thread list.

Argument: `$ARGUMENTS` (optional host/port hints only)

Steps:
1. Extract any host or port override from `$ARGUMENTS` if present.
2. Run `python3 tools/xbdm_rdcp.py --json threads` with any needed `--host` and `--port` flags.
3. Report the returned thread list clearly.
4. If XBDM returns a failure, include the exact code and message.

Notes:
- `threads` usually returns a `202` multiline response.
