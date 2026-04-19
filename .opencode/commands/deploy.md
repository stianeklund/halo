---
description: Build then deploy patched files to a real Xbox via xbcp
agent: fast
subtask: true
---

Use the `halo-build-xemu` skill for the standard build expectations before
deployment.

Build the project, then deploy recently modified files to a real Xbox using
`tools/deploy_xbox.py` (which wraps the XDK `xbcp.exe` file transfer tool).

Argument: $ARGUMENTS (optional flags passed to deploy_xbox.py, e.g. `-x 192.168.1.42`, `--full`, `--xbe-only`)

Steps:
1. Run the build portion of the standard flow from `halo-build-xemu`.
2. If the build succeeds, run `python3 tools/deploy_xbox.py $ARGUMENTS`.
3. If no `-x` argument was provided, use `-x $XBOX_HOST` from the environment.
   If that is also unset, warn and stop.
4. Report: build status, files deployed, and xbcp output.

Notes:
- Default Xbox destination: `xE:\GAMES\halo-patched` (overridable with `--dest`)
- For fastest iteration use `--xbe-only` to deploy just `default.xbe`
- Use `--full` to also deploy maps/ and bink/ directories
- Use `--since <seconds>` to deploy only files modified within a time window
- `--dry-run` shows what would be copied without actually transferring
- The script deploys only files newer than what is on the Xbox by default via
  `xbcp /d`.
