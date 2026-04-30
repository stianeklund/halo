---
description: Resume execution through XBDM
agent: fast
subtask: true
---

Use the `halo-xbdm` skill for standard XBDM execution-state handling.

Resume execution through XBDM.

Argument: `$ARGUMENTS` (optional thread-specific parameters plus optional host/port hints)

Steps:
1. Build the RDCP command as `continue` plus any explicit RDCP parameters from `$ARGUMENTS`.
2. Convert any host or port overrides into `--host` and `--port` flags.
3. Run `python3 tools/xbox/xbdm_rdcp.py --json "<continue command built from $ARGUMENTS>"`.
4. Report whether execution resumed and include any returned details.
5. If XBDM returns a failure, include the exact code and message.
