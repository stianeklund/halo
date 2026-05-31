---
name: halo-build-xemu
description: Standard project build, deploy, and run workflow
---

# Halo Build, Deploy & Run

Use this skill whenever work needs to build the project and deploy to xemu or
real Xbox.

## Canonical Command

```
./tools/xbox/build_deploy_run.sh -q
```

This single command handles build (`tools/build/build.py`) and XBDM deploy
(`tools/xbox/deploy_xbox.py`) in one step. No ISO creation is needed — the XBE
is hot-patched directly into the running instance.

## Variants

| Target | Command |
|--------|---------|
| xemu (local) | `./tools/xbox/build_deploy_run.sh -q` |
| Real Xbox | `./tools/xbox/build_deploy_run_real_hw.sh -q` |
| Custom host | `./tools/xbox/build_deploy_run.sh --xbox <host> -q` |

The real-hardware wrapper sets `XBOX_HOST` to `10.0.0.29` by default.

## xemu control notes

- Default xemu host is `127.0.0.1` (override via `XBOX_HOST` or `--xbox`).
- Use `mcp__xemu__*` tools directly for monitor control — they are the primary
  interface (the daemon auto-starts via SessionStart hook):
  - `mcp__xemu__xemu_status` — QMP connection and VM status
  - `mcp__xemu__xemu_pause` / `mcp__xemu__xemu_resume` — pause/resume VM
  - `mcp__xemu__xemu_screenshot` — capture screen
  - `mcp__xemu__xemu_send_monitor_command("info registers")` — HMP passthrough
  - `mcp__xemu__xemu_send_monitor_command("x /16xw 0x<addr>")` — examine memory
- `tools/xbox/xemu_qmp.py` is a fallback only when the MCP daemon is unavailable.

## Report format

Report:

- build status
- commit hash from `git rev-parse HEAD` when the build succeeds
- deploy target (xemu / real Xbox)
- any warnings or errors
