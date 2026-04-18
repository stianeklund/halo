---
description: Send a raw RDCP/XBDM command
agent: fast
subtask: true
---

Send a raw RDCP/XBDM command through `tools/xbdm_rdcp.py` and report the response.

Argument: `$ARGUMENTS`

Steps:
1. Treat `$ARGUMENTS` as the full RDCP command string to send.
2. If the user included host or port requirements in natural language, convert them into `--host` and `--port` flags. Otherwise rely on `XBDM_HOST` and `XBDM_PORT`, or the tool defaults.
3. Run `python3 tools/xbdm_rdcp.py --json "<RDCP command built from $ARGUMENTS>"`.
4. If the command is likely to return `203` binary data and the user supplied a length, pass `--binary-length` and `--output` as needed.
5. Return the parsed result, including non-2xx failures.

Notes:
- RDCP commands are ASCII text sent with CRLF terminators by the tool.
- `202` multiline responses are collected automatically.
- `203` responses require a known payload length.
