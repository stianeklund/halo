---
name: halo-build-xemu
description: Standard project build, ISO creation, and xemu loading workflow
---

# Halo Build And Xemu

Use this skill whenever work needs to build the project, create or refresh the
patched ISO, or control xemu.

## Preferred tools

- Build with `cmake --build build`
- Create ISO with `tools/extract-xiso.exe -c halo-patched halo-patched.iso`
- Control xemu with `python3 tools/xemu_qmp.py`
- Use MCP xemu tools only if `tools/xemu_qmp.py` cannot do the required action

## Standard build and load flow

1. Run `cmake --build build`.
2. If the build fails, stop and report the concrete errors.
3. Create the ISO:
   `tools/extract-xiso.exe -c halo-patched halo-patched.iso`
4. If ISO creation fails with `Permission denied`, eject the mounted ISO first:
   `python3 tools/xemu_qmp.py eject`
   then retry ISO creation.
5. Load and reset xemu:
   `python3 tools/xemu_qmp.py --launch-if-missing load-iso halo-patched.iso --reset`

## xemu control notes

- Default ISO path is `halo-patched.iso` in the repo root.
- If the ISO is missing but `halo-patched/default.xbe` exists, suggest running
  the build flow first.
- For monitor-only control, use `status`, `reset`, `stop`, `cont`, `eject`, or
  `hmp` subcommands through `tools/xemu_qmp.py`.
- If monitor mode is relevant, remind the user that `tools/xemu-mon.py` can run
  commands like `info registers`.

## Report format

Report:

- build status
- ISO path
- xemu load or control result
- any warnings or fallback used
