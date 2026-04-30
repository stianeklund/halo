---
description: Run the Option 3 verification ladder for a lift target
agent: fast
subtask: true
---

Use the `halo-verify-debug` skill for the Option 3 ladder and reporting rules.

Run the practical Option 3 verification ladder using `tools/verify/verify_option3.py`.

Argument: $ARGUMENTS (`<target> [extra verify_option3 flags]`)

Steps:
1. Parse the first token as `<target>` (function name or `0x...` address).
2. Treat remaining tokens as extra flags passed through to
   `tools/verify/verify_option3.py`.
3. Follow the Option 3 lane from `halo-verify-debug`.

Notes:
- Add `--objdiff-reference <path>` and `--objdiff-candidate <path>` when a delinked reference object exists.
- Add `--load-into-xemu` to hot-load the ISO and reset the VM via `tools/xbox/xemu_qmp.py`.
- Use `--skip-build` / `--skip-iso` for quick re-runs when those artifacts already exist.
