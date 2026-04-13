Run the assist-first lift workflow through `tools/lift_pipeline.py`.

Argument: $ARGUMENTS (optional target + optional extra `lift_pipeline.py` flags)

Behavior:
1. If a target is provided, use it as `--target` (function name or hex address from `kb.json`).
2. If no target is provided, allow the pipeline to auto-pick the top frontier candidate.
3. Run:
   - with target: `python3 tools/lift_pipeline.py --target <target> --no-metadata-update <extra_flags>`
   - without target: `python3 tools/lift_pipeline.py --no-metadata-update <extra_flags>`
4. Do not write `kb_meta.json` by default from this command.
5. Report:
   - selected target (name/address/object)
   - stage pass/fail summary
   - artifact summary path under `artifacts/lift_runs/.../summary.json`

Notes:
- Use this command for day-to-day assist runs.
- Use `/lift-verify` for structural verification with `--verify-auto`.
