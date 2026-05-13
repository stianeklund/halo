---
description: Run Mizuchi prompt/decomp loops with Codex/OpenCode (no Claude SDK runner)
model: sonnet
subtask: false
---

Use this command when you want the Mizuchi workflow but do not want `mizuchi run`
to invoke `claude-runner`.

Argument: `$ARGUMENTS` (optional target: `FUN_...` or `0x...`)

## Goal

Drive the decomp loop with the current agent (Codex/OpenCode), while reusing:
- `artifacts/auto_lift/context_cache/*.json`
- `artifacts/mizuchi/prompts/<FUNC>/`
- `tools/mizuchi/compile_and_view.py`

No external Claude calls should occur in this command unless explicitly requested.

## Flow

1. Resolve target:
   - If `$ARGUMENTS` provided, use it.
   - Otherwise run:
     ```bash
     rtk python3 tools/llm_auto_lift.py select --limit 10
     ```
     Pick the top `auto-lift` or `cache-context` lane target.

2. Ensure cached context exists:
   ```bash
   rtk ls artifacts/auto_lift/context_cache
   ```
   - If missing for target, run:
     ```bash
     rtk python3 tools/llm_auto_lift.py cache-context --target <TARGET>
     ```

3. Generate prompt folder from cache:
   ```bash
   rtk python3 tools/mizuchi/gen_prompts.py --target <TARGET>
   ```
   If prompt exists and refresh is needed, rerun with `--force`.

4. Run Codex/OpenCode decomp loop:
   - Read `artifacts/mizuchi/prompts/<FUNC>/prompt.md`.
   - Produce candidate C code for `<FUNC>`.
   - Validate with:
     ```bash
     rtk python3 tools/mizuchi/compile_and_view.py <FUNC> --c-file <candidate.c>
     ```
   - Iterate until `success: true` (`diff_count == 0`) or progress stalls.

5. If exact match found, hand off to repo verification:
   ```bash
   rtk python3 tools/lift_pipeline.py --target <FUNC> --no-metadata-update --verify-policy auto
   ```

## Output Contract

Report:
- Target
- Best `match_pct` / `diff_count`
- Whether exact match was reached
- Next action (`/lift`, pipeline verification, or manual follow-up)

## Notes

- `tools/mizuchi/run.sh run --config mizuchi.yaml` is intentionally excluded here
  because it uses the configured runner plugin (`claude-runner` in current config).
- This command is provider-agnostic and safe to run on already-cached context.
