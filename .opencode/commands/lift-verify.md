---
description: Run lift verification on the deep agent
agent: deep
subtask: true
---

Use the `halo-verify-debug` skill for structural verification rules and report
shape.

Run structural verification through `tools/lift_pipeline.py` using
auto-generated verify payloads.

Argument: $ARGUMENTS (`<target> <new_address> [extra lift_pipeline flags]`)

Behavior:
1. Parse the first token as `<target>` (function name or `0x...` address).
2. Parse the second token as `<new_address>` (lifted function address in
   patched XBE).
3. Treat any remaining tokens as extra flags forwarded to
   `tools/lift_pipeline.py`.
4. Follow the lift verification lane from `halo-verify-debug`.

Notes:
- If extraction writes defaults into `{artifact_dir}` (`orig_decompile.txt`,
  `new_decompile.txt`, optional `orig_callees.txt`, `new_callees.txt`), no
  extra verify flags are needed.
- Otherwise, pass explicit `--orig-decompile-*`, `--new-decompile-*`,
  `--orig-callees-*`, and `--new-callees-*` flags.
