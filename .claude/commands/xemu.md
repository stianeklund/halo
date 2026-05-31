---
description: Build, deploy, and run on xemu or real Xbox
subtask: false
---

Use the `halo-build-xemu` skill for repo-specific build/deploy rules.

## Canonical Command

```
./tools/xbox/build_deploy_run.sh -q
```

This single command builds and deploys via XBDM. No ISO step needed.

Argument: $ARGUMENTS (optional flags like `-q` for quiet, `--xbox <host>` for
target override)

Steps:
1. Run `./tools/xbox/build_deploy_run.sh -q` (or with `--xbox <host>` for real
   hardware).
2. If the build fails, stop and report the concrete errors.
3. Report the result.

Notes:
- For real Xbox: `./tools/xbox/build_deploy_run_real_hw.sh -q`
- Use `mcp__xemu__*` tools for monitor control (status, pause, resume, HMP
  passthrough). The daemon auto-starts via SessionStart hook.
- `tools/xbox/xemu_qmp.py` is a fallback only when the MCP daemon is unavailable.
