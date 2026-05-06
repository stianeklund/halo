---
description: Target selection, Ghidra context caching, and lift delegation
model: sonnet
subtask: true
---

Use `halo-re-lift` for lift rules and `halo-verify-debug` for validation gates.

Autonomous lift loop: selects the best unported function, lifts it via `/lift`,
auto-commits on success, reverts+logs on failure. Designed for `/loop /auto-lift`.

Argument: $ARGUMENTS (optional: `select|score|cache-context|review|promote ...`)

## Default flow (no arguments)

1. Run `rtk python3 tools/llm_auto_lift.py select --limit 10` to pick best targets.
2. Pick the top `auto-lift` or `cache-context` lane target.
3. If Ghidra MCP is available and no cached context exists, run `cache-context --target <target>`.
4. Delegate to `/lift <target>` (includes `maintain.py` via lift_pipeline).
5. Evaluate pipeline result (see pass/fail criteria below).
6. **On pass**: auto-commit, report commit hash.
7. **On fail**: attempt Opus escalation or revert+log (see escalation below).

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

## Opus escalation

This skill runs on **Sonnet** by default for cost efficiency. When a lift fails
on Sonnet due to a reasoning-class failure (not a trivial build error), escalate
to Opus:

**Escalate to Opus when:**
- VC71 match < 65% (control flow / structure wrong)
- ABI audit fails (calling convention reasoning)
- FPU-WARN (operand order requires careful disassembly reading)
- Build fails on the second attempt (not a simple typo)

**Do NOT escalate (just revert+log) when:**
- Target has SEH prolog/epilog (not liftable with current tooling)
- Target has >3 register args (disqualified)
- Build fails on an unrelated file (repo state issue, not lift quality)

**Escalation flow:**
1. Revert the Sonnet attempt: `rtk git checkout -- src/ kb.json`
2. Re-run `/lift <target>` using Agent tool with `model: opus`
3. If Opus also fails, revert+log with both attempts recorded.

## On success — auto-commit

```bash
rtk git add -- src/ kb.json
rtk python3 tools/audit/generate_lift_commit.py --batch-name "<target_name>" > /tmp/commit_msg.txt
rtk git commit -F /tmp/commit_msg.txt
```

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
/auto-lift                    # one-shot: pick, lift, commit or revert
/loop /auto-lift              # autonomous: repeat until stopped
/auto-lift select --limit 20  # just show targets, don't lift
/auto-lift cache-context --batch 10  # pre-cache Ghidra context for top 10
```

## Safety rules

1. Code generation is done by `/lift` with full agent context and CLAUDE.md rules.
2. Auto-commit only after the full pipeline passes.
3. Always revert on failure — never leave broken state in the working tree.
4. `review` and `promote` are legacy subcommands for old batch artifacts.

When used with `/loop`, each iteration picks the next best target, lifts it,
and either commits or reverts+logs. Check `artifacts/auto_lift/failures/` to
review targets that failed.
