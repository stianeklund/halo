---
description: Walk Xbox virtual memory map
agent: fast
subtask: true
---

Use the `halo-xbdm` skill for RDCP argument handling and response reporting.

Walk the Xbox virtual memory map through XBDM.

Argument: `$ARGUMENTS`

Steps:
1. Treat `$ARGUMENTS` as the RDCP parameters for `walkmem`, if any.
2. Convert any host or port overrides into `--host` and `--port` flags.
3. Run `python3 tools/xbdm_rdcp.py --json "walkmem <RDCP params built from $ARGUMENTS>"`.
4. Report the returned mapped ranges and attributes clearly.
5. If XBDM returns a failure, include the exact code and message.

Notes:
- This is useful for checking whether an address range is mapped before `getmem`.
