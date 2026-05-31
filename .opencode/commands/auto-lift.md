---
description: Target selection, Ghidra context caching, and lift delegation
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
   Agent(subagent_type="xbox-halo-re-analyst", prompt=<brief>)
   ```
   The prompt must include: target address, decompilation output, KB entry JSON,
   source file path, and these file-write instructions:
   - Write the C89 implementation to the source file at the correct address-ordered position
   - Update kb.json declaration if needed (conservatively)
   - Update `tools/kb_reg_baseline.json` for any `@<reg>` annotations
   - Run `rtk python3 tools/analysis/maintain.py <source_file>`
   - Run `rtk python3 tools/audit/check_lift_hazards.py` and fix target-relevant hazards
   - Report: RESOLVED_TARGET, Confirmed/Inferred/Uncertain, kb.json updates made
5. **Phase 2 — build + verify (orchestrator):**
   After the agent returns, ensure a delinked reference exists (export via
   `mcp__ghidra-live__export_delinked_object` if missing), then run:
   ```bash
   rtk python3 tools/lift_pipeline.py --target <name> --no-metadata-update --verify-policy auto
   ```
6. Evaluate pipeline result (see pass/fail criteria below).
7. **On pass**: auto-commit (unless `--dry-run`), reset consecutive failure counter.
8. **On fail**: attempt focused escalation or revert+log (see escalation below),
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

Low-match thresholds (VC71 or objdiff structural match %):
- **>= 65%**: PASS unconditionally — safe for auto-commit
- **50–65%**: PASS if at least one behavior signal (behavior_check or runtime_check)
- **40–50%**: PASS only if BOTH behavior_check AND runtime_check pass
- **< 40%**: hard REJECT — no override
- **FPU-WARN present**: FAIL — operand-order mismatch needs manual review

Pass `--low-match-threshold 65 --low-match-behavior-both-below 50 --low-match-reject-below 40`
to `lift_pipeline.py` to enforce these thresholds.

When no delinked reference exists (no VC71 data), strict policy fails.
The `/lift` skill uses `--verify-policy auto` which accepts when no VC71 data
is available but the build and ABI audit pass.

## Escalation

When a lift fails due to a reasoning-class failure (not a trivial build error),
rerun Phase 1 with the strongest available `xbox-halo-re-analyst` configuration
or hand the artifact to `/verify failure` for triage.

**Escalate when:**
- VC71 match < 65% (control flow / structure wrong)
- ABI audit fails (calling convention reasoning)
- FPU-WARN (operand order requires careful disassembly reading)
- Build fails on the second attempt (not a simple typo)

**Do NOT escalate (just revert+log) when:**
- Target has SEH prolog/epilog (not liftable with current tooling)
- Target has >3 register args (disqualified)
- Build fails on an unrelated file (repo state issue, not lift quality)

**Escalation flow:**
1. Revert the failed attempt: `rtk git checkout -- src/ kb.json`
2. Re-run Phase 1 using `Agent(subagent_type="xbox-halo-re-analyst")` with the
   same prompt as the original attempt.
3. Run Phase 2 again. If the retry also fails, revert+log with both attempts recorded.

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
    {"attempt": "initial", "failure_stage": "<stage>", "error_summary": "<msg>"},
    {"attempt": "escalated", "failure_stage": "<stage>", "error_summary": "<msg>"}
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
6. `review` and `promote` are legacy subcommands for old batch artifacts.
