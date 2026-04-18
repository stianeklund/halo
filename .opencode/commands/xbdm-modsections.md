---
description: Query module section layout
agent: fast
subtask: true
---

Query section layout for a loaded module through XBDM.

Argument: `$ARGUMENTS`

Steps:
1. Treat `$ARGUMENTS` as the RDCP parameters for `modsections`, such as `name=default.xbe` or `name=halo.dll`.
2. Convert any host or port overrides into `--host` and `--port` flags.
3. Run `python3 tools/xbdm_rdcp.py --json "modsections <RDCP params built from $ARGUMENTS>"`.
4. Report the returned section names, base addresses, sizes, and flags clearly.
5. If XBDM returns a failure, include the exact code and message.

Notes:
- This is useful for mapping code/data ranges before memory reads.
