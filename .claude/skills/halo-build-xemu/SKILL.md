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
- For monitor-only control, use `tools/xbox/xemu_qmp.py` subcommands:
  `status`, `reset`, `stop`, `cont`, `eject`, `hmp`.
- If monitor mode is relevant, remind the user that `tools/xbox/xemu-mon.py`
  can run commands like `info registers`.
- Use MCP xemu tools only if `tools/xbox/xemu_qmp.py` cannot do the required
  action.

## Report format

Report:

- build status
- commit hash from `rtk git rev-parse HEAD` when the build succeeds
- deploy target (xemu / real Xbox)
- any warnings or errors
