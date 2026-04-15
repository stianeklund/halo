Spawn an Agent with `model: "haiku"` to handle all of the following work. Do not execute any steps yourself — pass the full task to the agent.

Build the project, create a patched ISO, and hot-swap it into xemu using
`tools/xemu_qmp.py` first. Use MCP only if the script cannot perform the needed
action.

Argument: $ARGUMENTS (unused)

Steps:
1. Run `cmake --build build` and check for errors
2. If the build fails, report the errors and stop
3. If the build succeeds, create the ISO: `tools/extract-xiso.exe -c halo-patched halo-patched.iso`
4. If the ISO creation fails with "Permission denied", the ISO file may be locked by xemu. First try `python3 tools/xemu_qmp.py eject`, then retry the ISO creation. Use MCP only if that fails.
5. Once the ISO is created, load it in xemu with `python3 tools/xemu_qmp.py --launch-if-missing load-iso halo-patched.iso --reset`
6. Report: build status, any warnings, ISO path, xemu load status
