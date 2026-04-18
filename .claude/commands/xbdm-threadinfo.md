Spawn an Agent with `model: "haiku"` to handle all of the following work. Do not execute any steps yourself — pass the full task to the agent.

Query XBDM for detailed information about one thread.

Argument: `$ARGUMENTS`

Steps:
1. Treat `$ARGUMENTS` as the threadinfo command arguments, for example `thread=0x1234`.
2. If host or port overrides are present, convert them into `--host` and `--port` flags and keep only the RDCP parameters in the command string.
3. Run `python3 tools/xbdm_rdcp.py --json "threadinfo <RDCP params built from $ARGUMENTS>"`.
4. Report the returned thread information clearly.
5. If XBDM returns a failure, include the exact code and message.

Notes:
- Keep the RDCP parameter formatting intact.
