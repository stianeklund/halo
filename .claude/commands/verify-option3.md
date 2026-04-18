Spawn an Agent with `model: "haiku"` to handle all of the following work. Do not execute any steps yourself — pass the full task to the agent.

Run the practical Option 3 verification ladder using `tools/verify_option3.py`.

Argument: $ARGUMENTS (`<target> [extra verify_option3 flags]`)

Steps:
1. Parse the first token as `<target>` (function name or `0x...` address).
2. Treat remaining tokens as extra flags passed through to `tools/verify_option3.py`.
3. Run:
   `python3 tools/verify_option3.py --target <target> <extra_flags>`
4. Report:
   - stage results (build / build_iso / objdiff / xemu_load_reset / assert_tripwire)
   - PASS/FAIL verdict
   - artifact summary path under `artifacts/verify_option3/.../summary.json`

Notes:
- Add `--objdiff-reference <path>` and `--objdiff-candidate <path>` when a delinked reference object exists.
- Add `--load-into-xemu` to hot-load the ISO and reset the VM via `tools/xemu_qmp.py`.
- Use `--skip-build` / `--skip-iso` for quick re-runs when those artifacts already exist.
