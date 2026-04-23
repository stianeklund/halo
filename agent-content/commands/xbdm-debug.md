---
description: Copy or tail debug.txt through XBDM
subtask: true
---

Use the `halo-xbdm` skill for RDCP argument handling and response reporting.

Copy `debug.txt` from the Xbox, or print the last N lines.

Argument: `$ARGUMENTS`

Steps:
1. Treat `$ARGUMENTS` as arguments for `tools/xbdm_debug_txt.py`.
2. Use `-10`, `-5`, or `--lines -10` to print the last N lines instead of copying the full file.
3. Run `python3 tools/xbdm_debug_txt.py $ARGUMENTS`.
4. If no line count is supplied, report the saved repo path for the full file copy.
5. If XBDM returns a failure, include the exact code and message.
