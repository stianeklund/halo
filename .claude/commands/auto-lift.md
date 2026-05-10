---
description: Target selection, Ghidra context caching, and lift delegation
model: sonnet
subtask: false
---

Use `halo-re-lift` for lift rules and `halo-verify-debug` for validation gates.

Autonomous lift loop: selects the best unported function, lifts it in an
isolated subagent (fresh context + git worktree), auto-commits on success,
reverts+logs on failure.

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

1. **Select target**: run `rtk python3 tools/llm_auto_lift.py select --limit 10`.
   Skip targets that already have a failure record in `artifacts/auto_lift/failures/`.
   Pick the top `auto-lift` or `cache-context` lane target.

2. **Pre-screen**: run the prescreen on the specific target before any Ghidra calls:
   ```bash
   rtk python3 tools/analysis/structural_prescreen.py --function <NAME>
   ```
   - No output → no delinked ref, proceed normally.
   - `difficulty=reject` → log as skipped (not a failure), pick next target.
     Do NOT increment the consecutive failure counter.
   - `difficulty=hard` or `callee_reg_args` in risk factors → note the flags
     and pass them to the lift subagent as context hints.
   - `difficulty=easy, score=0` → proceed with high confidence.

3. **Cache context** (if `cache-context` lane and no cached context):
   ```bash
   rtk python3 tools/llm_auto_lift.py cache-context --target <target>
   ```

4. **Lift in isolated subagent** — spawn an Agent with `isolation="worktree"`.
   The worktree gives the subagent its own git branch; the parent merges on
   success and discards on failure. Pass a fully self-contained prompt including:
   - Target name, address, object, source path
   - Prescreen output (difficulty, score, risk factors)
   - Instruction to follow the `halo-re-lift` skill workflow
   - Commit instruction: after pipeline passes and reviewer returns AUTO_ACCEPT,
     run the standard commit sequence (see "On success" below)

   Using a subagent per lift keeps the parent context small across --batch runs.
   Each lift's Ghidra decompilation and disassembly stays inside the subagent.

5. **Evaluate subagent result**: the subagent returns one of:
   - `AUTO_ACCEPT` — pipeline passed, reviewer accepted, commit made in worktree
   - `NEEDS_RUNTIME` — pipeline passed but needs behavior verification
   - `REJECT` / build/ABI fail — lift failed

6. **On AUTO_ACCEPT**: merge the worktree commit (unless `--dry-run`).
   Reset consecutive failure counter.

7. **On NEEDS_RUNTIME**: discard worktree. Log to failures with stage
   `needs_runtime`. Increment consecutive failure counter.

8. **On REJECT / hard fail**: attempt Opus escalation (see below), or discard
   worktree and log failure. Increment consecutive failure counter.

9. If consecutive failures reach `--stop-on-fail`, stop and report.

After the loop ends, print a summary: N attempted, N committed, N failed, N skipped.

## Pipeline pass/fail criteria

The lift subagent runs `tools/lift_pipeline.py`. Any hard failure stops the
pipeline and reports the failing stage.

| Stage | Pass condition | Hard fail? |
|---|---|---|
| **ABI audit** | `audit_reg_abi.py` exit 0 | Yes |
| **Build** | `build.py -q --target halo` exit 0 | Yes |
| **VC71 verify** | Compiled with MSVC 7.1, compared against delinked reference | Depends on policy |
| **Low-match policy** | See thresholds below | Yes |

Low-match thresholds (VC71 or objdiff structural match %):
- **>= 90%**: may pass; auto-commit still requires Agent review gate.
- **85–90%**: pass only with at least one behavior signal, then requires review.
- **80–85%**: pass only if BOTH `behavior_check` AND `runtime_check` pass.
- **< 80%**: hard REJECT. Do not auto-commit.
- **FPU-WARN present**: FAIL — needs reviewer evidence, treat as REJECT.

When no delinked reference exists, strict policy fails. Do not auto-commit
without structural verification data.

Pass `--low-match-threshold 90 --low-match-behavior-both-below 85 --low-match-reject-below 80`
to `lift_pipeline.py` to enforce the mechanical floor.

## Agent review gate

After the pipeline passes, the lift subagent invokes an Opus
`xbox-halo-lift-reviewer` pass with: target, source diff,
`artifacts/lift_runs/.../summary.json`, VC71/objdiff output, hazard scan,
ABI audit output, and any Ghidra caller/callee/disassembly context.

The reviewer emits exactly:

`AUTOLIFT_REVIEW: AUTO_ACCEPT | NEEDS_RUNTIME | REJECT`

Acceptance policy:
- **>= 98% match**: `AUTO_ACCEPT` if ABI/build/VC71 pass, hazard scan clean,
  no FPU warnings, no call-arg or memory-offset uncertainty.
- **95–98%**: `AUTO_ACCEPT` requires explicit mismatch classification proving
  all differences are harmless (register allocation, instruction scheduling,
  equivalent stack layout, equivalent constant materialization).
- **90–95%**: `AUTO_ACCEPT` requires mismatch classification, full call-site
  argument audit, memory offset/global side-effect audit, and no unresolved
  `Uncertain` findings. Otherwise `NEEDS_RUNTIME` or `REJECT`.
- **< 90%**: `NEEDS_RUNTIME` unless golden/runtime verification passed and all
  mismatches are classified.
- **FPU-WARN, ABI uncertainty, suspicious duplicate args, pointer-as-float,
  unverified register args, unknown memory offsets, unclassified control-flow
  differences**: `REJECT`.

Review report must include: target + address + object + source path + match%
+ match source; mismatch classes (harmless or blocking); call argument audit;
memory offset / global side-effect audit; ABI audit; verdict rationale in
Confirmed / Inferred / Uncertain terms.

Auto-commit only when pipeline passes AND reviewer returns `AUTO_ACCEPT`.

## Opus escalation

This skill runs on **Sonnet** by default. Escalate the lift subagent to Opus when:
- VC71/objdiff match < 95% or reviewer returns `NEEDS_RUNTIME`
- ABI audit fails (calling convention reasoning required)
- FPU-WARN (operand order needs careful disassembly reading)
- Build fails on the second attempt (not a simple typo)
- Reviewer returns `REJECT` for a reason plausibly fixable by better disassembly
  analysis (call args, memory offsets, field rotation)

Do NOT escalate (revert+log immediately) when:
- Target has SEH prolog/epilog
- Target has >3 register args (disqualified)
- Build fails on an unrelated file (repo state issue)

Escalation flow:
1. Discard the Sonnet worktree.
2. Spawn a new Agent(isolation="worktree", model="opus") with the same prompt
   plus the Sonnet failure summary.
3. If Opus also fails, discard and log with both attempts recorded.

## On success — auto-commit

Run inside the lift subagent after reviewer returns `AUTO_ACCEPT`:
```bash
rtk git add -- src/ kb.json
rtk python3 tools/audit/generate_lift_commit.py --batch-name "<target_name>" > /tmp/commit_msg.txt
rtk git commit -F /tmp/commit_msg.txt
```

With `--dry-run`: leave changes staged, report what would be committed, then
revert before the next iteration (`rtk git checkout -- src/ kb.json`).

## On failure — revert + log

Discard the isolated worktree. Write failure record to
`artifacts/auto_lift/failures/<target_name>.json`:
```json
{
  "target": "<name>",
  "addr": "<0x...>",
  "object": "<object_name>",
  "timestamp": "<ISO 8601>",
  "prescreen": {"difficulty": "<easy|medium|hard|reject>", "score": 0, "risks": []},
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

1. Each lift runs in an isolated Agent with a git worktree — failures can't
   corrupt the working tree or accumulate stale context across iterations.
2. Auto-commit only after pipeline passes + `AUTOLIFT_REVIEW: AUTO_ACCEPT`.
3. `reject`-tier prescreen skips count toward the skip total, not failures.
4. Skip targets with existing failure records (don't retry known failures).
5. `--stop-on-fail` prevents runaway failures from burning tokens.
6. `review` and `promote` are legacy subcommands for old batch artifacts.
