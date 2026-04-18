Spawn an Agent with `model: "haiku"` to handle all of the following work. Do not execute any steps yourself — pass the full task to the agent.

Build the project, then deploy recently modified files to a real Xbox using
`tools/deploy_xbox.py` (which wraps the XDK `xbcp.exe` file transfer tool).

Argument: $ARGUMENTS (optional flags passed to deploy_xbox.py, e.g. `-x 192.168.1.42`, `--full`, `--xbe-only`)

Steps:
1. Run `cmake --build build` and check for errors
2. If the build fails, report the errors and stop
3. If the build succeeds, run `python3 tools/deploy_xbox.py $ARGUMENTS`
   - If no `-x` argument was provided, use `-x $XBOX_HOST` from the
     environment. If that is also unset, warn and stop.
   - The script automatically deploys only files newer than what is on the
     Xbox (uses `xbcp /d`).
4. Report: build status, files deployed, xbcp output

Notes:
- Default Xbox destination: `xE:\GAMES\halo-patched` (overridable with `--dest`)
- For fastest iteration use `--xbe-only` to deploy just `default.xbe`
- Use `--full` to also deploy maps/ and bink/ directories
- Use `--since <seconds>` to deploy only files modified within a time window
- `--dry-run` shows what would be copied without actually transferring
