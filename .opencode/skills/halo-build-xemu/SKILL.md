---
name: halo-build-xemu
description: Standard project build, ISO creation, and xemu loading workflow
---

# Halo Build And Xemu

Use this skill whenever work needs to build the project, create or refresh the
patched ISO, or control xemu.

## Preferred tools

- Build with `python3 tools/build/build.py -q --target halo`
- Create ISO with `python3 tools/build/build_iso.py` (handles xemu eject, retries, and
  excludes IDA database files automatically)
- Control xemu with `python3 tools/xbox/xemu_qmp.py`
- Use MCP xemu tools only if `tools/xbox/xemu_qmp.py` cannot do the required action

## Standard build and load flow

1. Run `python3 tools/build/build.py -q --target halo`.
2. If the build fails, stop and report the concrete errors.
3. Create the ISO:
   `python3 tools/build/build_iso.py`
4. Load and reset xemu:
   `python3 tools/xbox/xemu_qmp.py --launch-if-missing load-iso halo-patched.iso --reset`
5. The build hash is printed automatically by `patch.py` and `build_iso.py`.

## xemu control notes

- Default ISO path is `halo-patched.iso` in the repo root.
- If the ISO is missing but `halo-patched/default.xbe` exists, suggest running
  the build flow first.
- For monitor-only control, use `status`, `reset`, `stop`, `cont`, `eject`, or
  `hmp` subcommands through `tools/xbox/xemu_qmp.py`.
- If monitor mode is relevant, remind the user that `tools/xbox/xemu-mon.py` can run
  commands like `info registers`.

## Report format

Report:

- build status
- build hash from the build scripts
- ISO path
- xemu load or control result
- any warnings or fallback used
