---
description: Inspect file metadata through XBDM
agent: fast
subtask: true
---

Inspect file metadata through XBDM.

Argument: `$ARGUMENTS`

Steps:
1. Treat `$ARGUMENTS` as the RDCP parameters for `getfileattributes`, such as `name="D:\\default.xbe"`.
2. Convert any host or port overrides into `--host` and `--port` flags.
3. Run `python3 tools/xbdm_rdcp.py --json "getfileattributes <RDCP params built from $ARGUMENTS>"`.
4. Report the returned size, timestamps, and flags clearly.
5. If XBDM returns a failure, include the exact code and message.
