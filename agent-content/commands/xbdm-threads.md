---
description: Query XBDM thread list
agent: fast
subtask: true
---

Use the `halo-xbdm` skill for standard XBDM thread-query handling.

Query XBDM for the current thread list.

Argument: `$ARGUMENTS` (optional host/port hints only)

Steps:
1. Extract any host or port override from `$ARGUMENTS` if present.
2. Run `python3 tools/xbdm_rdcp.py --json threads` with any needed `--host` and `--port` flags.
3. Report the returned thread list clearly.
4. If XBDM returns a failure, include the exact code and message.

Notes:
- `threads` usually returns a `202` multiline response.
