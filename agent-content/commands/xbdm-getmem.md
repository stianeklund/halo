---
description: Read memory through XBDM
agent: fast
subtask: true
---

Use the `halo-xbdm` skill for XBDM memory-read handling and binary response
rules.

Read Xbox memory through XBDM and save or summarize the bytes.

Argument: `$ARGUMENTS`

Steps:
1. Parse `$ARGUMENTS` for an address and length. Accept either raw RDCP-style args such as `addr=0x80010000 length=0x40` or natural flag-style input if it is unambiguous.
2. If the user requested an output path, pass it to `--output`. Otherwise default to a temporary file only if needed for inspection.
3. Run `python3 tools/xbox/xbdm_rdcp.py --json --binary-length <length> "getmem addr=<addr> length=<length>"` with any requested `--host`, `--port`, or `--output` flags.
4. If bytes were saved to a file, report the path and length.
5. If the user asked for inspection, summarize the bytes in hex without altering the saved file.
6. If XBDM returns a failure, include the exact code and message.

Notes:
- `getmem` returns `203` binary data, so the byte length must match the requested `length`.
