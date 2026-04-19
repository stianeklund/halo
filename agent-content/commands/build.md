---
description: Build the project, create an ISO, and load it in xemu via xemu_qmp
agent: fast
subtask: true
---

Use the `halo-build-xemu` skill for the standard build, ISO, and xemu workflow.

Build the project, create a patched ISO, and hot-swap it into xemu using
`tools/xemu_qmp.py` first. Use MCP only if the script cannot perform the needed
action.

Argument: $ARGUMENTS (unused)

Steps:
1. Follow the standard build-and-load flow from `halo-build-xemu`.
2. Report: build status, any warnings, ISO path, xemu load status.
