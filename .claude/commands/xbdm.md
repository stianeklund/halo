---
description: Run XBDM/RDCP commands and common real-Xbox debug probes
subtask: false
---

Use the `halo-xbdm` skill for standard RDCP parsing, host or port handling,
and response reporting.

Run a raw RDCP/XBDM command or one of the common probe modes through
`tools/xbox/xbdm_rdcp.py` and report the response.

Argument: `$ARGUMENTS` (`raw <rdcp>`, `mem <addr> <len>`, `context [thread=...]`,
`threads`, `modules`, `modsections <args>`, `walkmem [args]`, `status`, `halt`,
`continue`, `dir <args>`, `fileattrs <args>`, `debug [-N]`)

Steps:
1. Parse the first token as a mode. If no known mode is present, treat all arguments as a raw RDCP command.
2. Convert host or port hints into `--host` and `--port` flags. Otherwise rely on `XBDM_HOST`, `XBDM_PORT`, or tool defaults.
3. For `debug`, run `rtk python3 tools/xbox/xbdm_debug_txt.py <args>`.
4. For `mem <addr> <len>`, run `rtk python3 tools/xbox/xbdm_rdcp.py --json --binary-length <len> "getmem addr=<addr> length=<len>"` with `--output` if requested.
5. For convenience modes, map to RDCP commands: `status` -> `isstopped`, `context` -> `getcontext`, `threads` -> `threads`, `modules` -> `modules`, `dir` -> `dirlist`, `fileattrs` -> `getfileattributes`.
6. For `raw`, pass the remaining text unchanged to `rtk python3 tools/xbox/xbdm_rdcp.py --json "<command>"`.
7. Return the parsed result, saved output path, or exact non-2xx failure code and message.

Notes:
- RDCP commands are ASCII text sent with CRLF terminators by the tool.
- `202` multiline responses are collected automatically.
- `203` responses require a known payload length.
