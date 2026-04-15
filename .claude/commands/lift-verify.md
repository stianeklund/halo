Run structural verification through `tools/lift_pipeline.py` using
auto-generated verify payloads.

Argument: $ARGUMENTS (`<target> <new_address> [extra lift_pipeline flags]`)

Behavior:
1. Parse the first token as `<target>` (function name or `0x...` address).
2. Parse the second token as `<new_address>` (lifted function address in patched XBE).
3. Treat any remaining tokens as extra flags forwarded to `tools/lift_pipeline.py`.
4. Run:
   `python3 tools/lift_pipeline.py --target <target> --verify-auto --verify-new-address <new_address> --no-metadata-update <extra_flags>`
5. Report:
   - verify payload path
   - `verify_lift` stage result
   - artifact summary path under `artifacts/lift_runs/.../summary.json`

Notes:
- If extraction writes defaults into `{artifact_dir}` (`orig_decompile.txt`,
  `new_decompile.txt`, optional `orig_callees.txt`, `new_callees.txt`), no
  extra verify flags are needed.
- Otherwise, pass explicit `--orig-decompile-*`, `--new-decompile-*`,
  `--orig-callees-*`, and `--new-callees-*` flags.
