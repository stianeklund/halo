---
name: halo-xbdm
description: Standard RDCP and XBDM command handling for real Xbox debugging
---

# Halo XBDM

Use this skill whenever work talks to a real Xbox through XBDM or RDCP via
`tools/xbox/xbdm_rdcp.py`.

## General rules

- Prefer `/xbdm <mode>` for user-facing workflows and `rtk python3 tools/xbox/xbdm_rdcp.py --json ...` for direct tool use.
- Convert user-supplied host and port hints into `--host` and `--port` flags.
- Otherwise rely on `XBDM_HOST`, `XBDM_PORT`, or the tool defaults.
- Return parsed results clearly, including non-2xx failures.
- Preserve exact XBDM error codes and messages.

## RDCP response handling

- `202` means multiline text; the tool collects it automatically.
- `203` means binary data; provide `--binary-length` and `--output` when needed.
- When reading memory, the binary length must match the requested length.

## Common workflows

- Raw command passthrough: send the RDCP command string as provided.
- Consolidated command modes: `/xbdm raw`, `/xbdm mem`, `/xbdm context`,
  `/xbdm threads`, `/xbdm modules`, `/xbdm status`, `/xbdm halt`,
  `/xbdm continue`, `/xbdm dir`, `/xbdm fileattrs`, and `/xbdm debug`.
- Memory reads: parse address and length, then use `getmem` with
  `--binary-length`.
- Thread inspection: use `threads`, `threadinfo`, `getcontext`, or
  `getextcontext` and report thread or register state clearly.
- Execution control: use `isstopped`, `halt`, and `continue` for stop-state
  checks and recovery.
- File and module inspection: use `dirlist`, `getfileattributes`, `modules`,
  and `modsections` and present the decoded fields cleanly.

## Output expectations

Report:

- exact command sent
- host or port overrides used, if any
- parsed result or saved output path
- exact failure code and message when the call fails
