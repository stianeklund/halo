---
description: Stop execution through XBDM
agent: fast
subtask: true
---

Stop the current title or target thread through XBDM.

Argument: `$ARGUMENTS` (optional thread-specific parameters plus optional host/port hints)

Steps:
1. Build the RDCP command as `halt` plus any explicit RDCP parameters from `$ARGUMENTS`.
2. Convert any host or port overrides into `--host` and `--port` flags.
3. Run `python3 tools/xbdm_rdcp.py --json "<halt command built from $ARGUMENTS>"`.
4. Report whether execution stopped and include any returned details.
5. If XBDM returns a failure, include the exact code and message.

Notes:
- Use this before context reads if the target must be stopped first.
