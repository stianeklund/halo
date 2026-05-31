---
description: Build then deploy patched files to a real Xbox via xbcp
subtask: false
---

Use the `halo-deploy-xbdm` skill for the standard build-and-deploy workflow.
This is the **preferred verification path** — always deploy to a real Xbox via
XBDM when a console is available.

Build the project, then deploy recently modified files to a real Xbox using
`tools/xbox/deploy_xbox.py` (which wraps the XDK `xbcp.exe` file transfer tool).

Argument: $ARGUMENTS (optional flags passed to deploy_xbox.py, e.g. `-x 192.168.1.42`, `--full`, `--xbe-only`)

Steps:
1. Run the standard build-and-deploy flow from `halo-deploy-xbdm`.
2. If the build succeeds, run `python3 tools/xbox/deploy_xbox.py $ARGUMENTS`.
3. If no `-x` argument was provided, use `-x $XBOX_HOST` from the environment.
   If that is also unset, warn and stop.
4. If both the build and deploy succeed, run `git rev-parse HEAD` and include
   the full commit hash in the report.
5. Report: build status, commit hash, files deployed, and xbcp output.

Notes:
- Default Xbox destination: `xE:\GAMES\halo-patched` (overridable with `--dest`)
- For fastest iteration use `--xbe-only` to deploy just `default.xbe`
- Use `--full` to also deploy maps/ and bink/ directories
- Use `--since <seconds>` to deploy only files modified within a time window
- `--dry-run` shows what would be copied without actually transferring
- The script deploys only files newer than what is on the Xbox by default via
  `xbcp /d`.
