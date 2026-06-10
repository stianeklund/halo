---
description: Target selection, Ghidra context caching, and lift delegation
model: sonnet
subtask: false
---

Use `halo-re-lift` for lift rules and `halo-verify-debug` for validation gates.

Autonomous lift loop: selects the best unported function, lifts it via `/lift`,
auto-commits on success, reverts+logs on failure.

Argument: $ARGUMENTS

Parse the following from $ARGUMENTS (all optional, flags and subcommands):
- `--batch N` — lift up to N functions sequentially (default: 1)
- `--stop-on-fail N` — stop after N consecutive failures (default: 3)
- `--dry-run` — do everything except commit (leave changes for review)
- Subcommands: `select`, `score`, `cache-context`, `review`, `promote` — if one
  of these is the first word, run it directly instead of the lift loop.

## Lift loop

For each target up to `--batch` count (or until `--stop-on-fail` consecutive
failures), repeat:

1. Run `rtk python3 tools/llm_auto_lift.py select --limit 10` to pick best targets.
   Skip any targets that already have a failure record in `artifacts/auto_lift/failures/`.
2. Pick the top `auto-lift` or `cache-context` lane target.
3. If Ghidra MCP is available and no cached context exists, run `cache-context --target <target>`.
4. **Phase 1 — RE analysis + implementation (subagent):**
   Gather lightweight context for the target (KB entry via `rtk jq`, decompilation
   via Ghidra MCP, source file path). Then spawn a subagent:
   ```
   Agent(subagent_type="xbox-halo-re-analyst", model="sonnet", prompt=<brief>)
   ```
   The prompt must follow the **Phase-1 subagent briefing template** in
   `docs/lift-policy.md`, filling in the target address, decompilation output,
   KB entry JSON, and source file path.
5. **Phase 2 — build + verify (orchestrator):**
   After the agent returns, ensure a delinked reference exists (export via
   `mcp__ghidra-live__export_delinked_object` if missing), then run:
   ```bash
   rtk python3 tools/lift_pipeline.py --target <name> --no-metadata-update --verify-policy auto
   ```
6. Evaluate pipeline result (see pass/fail criteria below).
7. **On pass**: auto-commit (unless `--dry-run`), reset consecutive failure counter.
8. **On fail**: attempt Opus escalation or revert+log (see escalation below),
   increment consecutive failure counter.
9. If consecutive failures reach `--stop-on-fail`, stop and report.

After the loop ends, print a summary: N attempted, N committed, N failed, N skipped.

## Pipeline pass/fail criteria

The lift pipeline (`tools/lift_pipeline.py`) runs these stages in order.
Any hard failure stops the pipeline and reports the failing stage.

| Stage | Pass condition | Hard fail? |
|---|---|---|
| **ABI audit** | `audit_reg_abi.py` exit 0 — register args match kb.json | Yes |
| **Build** | `build.py -q --target halo` exit 0 — clean compilation | Yes |
| **VC71 verify** | Compiles with MSVC 7.1, compared against delinked reference | Depends on low-match policy |
| **Low-match policy** (default: strict) | See thresholds below | Yes |

See `docs/lift-policy.md` §Verify-policy-presets for the canonical threshold table.
Use `--verify-policy auto` (default) for this skill.  The `/lift` skill uses
`auto` which accepts when no VC71 data is available but build and ABI audit pass.

## Opus escalation

This skill runs on **Sonnet** by default for cost efficiency.
See `docs/lift-policy.md` §Escalation-flow for the canonical escalation rules and
pass/fail thresholds.  Summary: escalate on VC71 <65%, ABI fail, FPU-WARN, or
second build failure; do not escalate on SEH, >3 reg-args, or unrelated build fail.

## On success — auto-commit

Unless `--dry-run` is set:
```bash
rtk git add -- src/ kb.json
rtk python3 tools/audit/generate_lift_commit.py --batch-name "<target_name>" > /tmp/commit_msg.txt
rtk git commit -F /tmp/commit_msg.txt
```

With `--dry-run`: leave changes staged, report what would be committed, then
revert before the next iteration (`rtk git checkout -- src/ kb.json`).

## On failure — revert + log

```bash
rtk git checkout -- src/ kb.json
```

Write failure record to `artifacts/auto_lift/failures/<target_name>.json`:
```json
{
  "target": "<name>",
  "addr": "<0x...>",
  "object": "<object_name>",
  "timestamp": "<ISO 8601>",
  "attempts": [
    {"model": "sonnet", "failure_stage": "<stage>", "error_summary": "<msg>"},
    {"model": "opus", "failure_stage": "<stage>", "error_summary": "<msg>"}
  ],
  "pipeline_output": "<full pipeline stderr/stdout from last attempt>"
}
```

## Usage

```bash
/auto-lift                              # one-shot: pick 1, lift, commit or revert
/auto-lift --batch 10                   # lift up to 10 functions then stop
/auto-lift --batch 10 --stop-on-fail 3  # stop early if 3 in a row fail
/auto-lift --batch 5 --dry-run          # lift 5 but don't commit — review in morning
/auto-lift select --limit 20            # just show targets, don't lift
/auto-lift cache-context --batch 10     # pre-cache Ghidra context for top 10
```

## Safety rules

1. Code generation is done by `/lift` with full agent context and CLAUDE.md rules.
2. Auto-commit only after the full pipeline passes (unless `--dry-run`).
3. Always revert on failure — never leave broken state in the working tree.
4. Skip targets that already have failure records (don't retry known failures).
5. `--stop-on-fail` prevents runaway failures from burning tokens.
6. `review` and `promote` are legacy subcommands (see `docs/lift-policy.md` §Legacy-subcommands).
