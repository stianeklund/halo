---
description: Read extended thread context
agent: fast
subtask: true
---

Read extended thread register context through XBDM.

Argument: `$ARGUMENTS`

Steps:
1. Treat `$ARGUMENTS` as the RDCP parameters for `getextcontext`, such as `thread=0x1234`.
2. Convert any host or port overrides into `--host` and `--port` flags.
3. Run `python3 tools/xbdm_rdcp.py --json "getextcontext <RDCP params built from $ARGUMENTS>"`.
4. Report the returned extended register state clearly.
5. If the target is not stopped, say so explicitly.
6. If XBDM returns a failure, include the exact code and message.
