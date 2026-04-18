---
description: List files through XBDM
agent: fast
subtask: true
---

List files in a directory through XBDM.

Argument: `$ARGUMENTS`

Steps:
1. Treat `$ARGUMENTS` as the RDCP parameters for `dirlist`, such as `name="D:\\"`.
2. Convert any host or port overrides into `--host` and `--port` flags.
3. Run `python3 tools/xbdm_rdcp.py --json "dirlist <RDCP params built from $ARGUMENTS>"`.
4. Report the returned files and attributes clearly.
5. If XBDM returns a failure, include the exact code and message.
