Spawn an Agent with `model: "haiku"` to handle all of the following work. Do not execute any steps yourself — pass the full task to the agent.

Query whether the target is currently stopped and why.

Argument: `$ARGUMENTS` (optional thread-specific parameters plus optional host/port hints)

Steps:
1. Build the RDCP command as `isstopped` plus any explicit RDCP parameters from `$ARGUMENTS`.
2. Convert any host or port overrides into `--host` and `--port` flags.
3. Run `python3 tools/xbdm_rdcp.py --json "<isstopped command built from $ARGUMENTS>"`.
4. Report the stop state and any reason or thread information returned.
5. If XBDM returns a failure, include the exact code and message.
