---
description: Query XBDM module list
agent: fast
subtask: true
---

Use the `halo-xbdm` skill for module-query handling and response reporting.

Query XBDM for the current loaded module list.

Argument: `$ARGUMENTS` (optional host/port hints only)

Steps:
1. Extract any host or port override from `$ARGUMENTS` if present.
2. Run `python3 tools/xbdm_rdcp.py --json modules` with any needed `--host` and `--port` flags.
3. Report the returned module list clearly.
4. If XBDM returns a failure, include the exact code and message.

Notes:
- `modules` usually returns a `202` multiline response.
