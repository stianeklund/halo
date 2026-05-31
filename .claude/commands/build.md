---
description: Build the project and produce a patched XBE
subtask: false
---

Use the `halo-build-xemu` skill for the standard build workflow.

Build the project with `tools/build/build.py`. The patched XBE lands in
`halo-patched/default.xbe`. Never create ISOs — the user loads the XBE
into xemu themselves.

Argument: $ARGUMENTS (unused)

Steps:
1. Run `rtk python3 tools/build/build.py -q --target halo`.
2. If the build succeeds, run `git rev-parse HEAD` and include the full commit
   hash in the report.
3. Report: build status, commit hash, any warnings.
