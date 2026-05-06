---
name: halo-deploy-xbdm
description: Build and deploy patched files to a real Xbox via XBDM/XBCP
---

# Halo Deploy XBDM

Use this skill when the goal is to build the project and deploy patched files
to a real Xbox. Do not build an ISO unless the user explicitly asks for xemu.

## Preferred tools

- Build with `cmake --build build`
- Deploy with `python3 tools/xbox/deploy_xbox.py`
- Use `tools/xbox/xbdm_rdcp.py` for direct XBDM checks when needed

## Standard build and deploy flow

1. Run `cmake --build build`.
2. If the build fails, stop and report the concrete errors.
3. Deploy to the Xbox:
   `python3 tools/xbox/deploy_xbox.py`
4. If the deploy succeeds, include the build hash and the deployed files in the report.

## Verification priority

- Prefer real Xbox via XBDM over xemu+ISO whenever a console is available.
- Do not switch to ISO/xemu unless the user asks for it or the Xbox is unavailable.

## Report format

Report:

- build status
- commit hash from `git rev-parse HEAD` when the build and deploy succeed
- deployed files
- XBCP/XBDM output
