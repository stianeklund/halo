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
4. Delegate to `/lift <target>` (includes `maintain.py` via lift_pipeline).
5. Evaluate pipeline result (see pass/fail criteria below).
6. Run the autonomous review gate (see Agent review gate below).
7. **On pass + AUTO_ACCEPT**: auto-commit (unless `--dry-run`), reset consecutive failure counter.
8. **On fail / NEEDS_RUNTIME / REJECT**: attempt Opus escalation or revert+log (see escalation below),
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

Mechanical low-match thresholds (VC71 or objdiff structural match %):
- **>= 90%**: pipeline may pass structurally, but auto-commit still requires
  the Agent review gate below.
- **85–90%**: pipeline may pass only with at least one behavior signal
  (`behavior_check` or `runtime_check`), then requires Agent review.
- **80–85%**: pipeline may pass only if BOTH `behavior_check` AND
  `runtime_check` pass, then requires Agent review.
- **< 80%**: hard REJECT for auto-lift. Do not auto-commit.
- **FPU-WARN present**: FAIL — operand-order mismatch needs reviewer evidence
  and should normally be treated as REJECT.

Pass `--low-match-threshold 90 --low-match-behavior-both-below 85 --low-match-reject-below 80`
to `lift_pipeline.py` to enforce the mechanical floor.

When no delinked reference exists (no VC71/objdiff data), strict policy fails.
Do not auto-commit a lift without structural verification data.

## Agent review gate

Auto-lift must be close to end-to-end automated, but the review step is still
required. It is performed by an LLM agent, not a human, and must fail closed.

After the pipeline passes, invoke an Opus `xbox-halo-lift-reviewer` review pass
with the target, source diff, `artifacts/lift_runs/.../summary.json`,
VC71/objdiff output, hazard scan output, ABI audit output, and any cached
Ghidra caller/callee/disassembly context.

The reviewer must emit this exact verdict line:

`AUTOLIFT_REVIEW: AUTO_ACCEPT | NEEDS_RUNTIME | REJECT`

Reviewer acceptance policy:
- **>= 98% structural match**: `AUTO_ACCEPT` may be returned if ABI/build/VC71
  pass, hazard scan is clean, no FPU warnings are present, and no call-arg or
  memory-offset uncertainty remains.
- **95–98% structural match**: `AUTO_ACCEPT` requires explicit mismatch
  classification proving all differences are harmless compiler-shape changes
  (register allocation, instruction scheduling, equivalent stack layout, or
  equivalent constant materialization).
- **90–95% structural match**: `AUTO_ACCEPT` requires mismatch classification,
  full call-site argument audit, memory offset/global side-effect audit, and
  no unresolved `Uncertain` findings. Otherwise return `NEEDS_RUNTIME` or
  `REJECT`.
- **< 90% structural match**: return `NEEDS_RUNTIME` unless golden/runtime
  behavior verification already passed and all mismatches are classified.
- **No structural data, FPU-WARN, ABI uncertainty, suspicious duplicate args,
  pointer-as-float warning, unverified register args, unknown memory offsets,
  or unclassified control-flow differences**: return `REJECT`.

The review report must include:
- Target, address, object, source path, match percentage, and match source.
- Mismatch classes, each marked harmless or blocking.
- Call Argument Audit: every call verified, or listed as blocking.
- Memory Offset / Global Side-effect Audit: every nontrivial access verified,
  or listed as blocking.
- ABI Audit: register args and caller expectations verified.
- Verdict rationale in Confirmed / Inferred / Uncertain terms.

Auto-commit is allowed only when both the pipeline passes and the reviewer
returns `AUTOLIFT_REVIEW: AUTO_ACCEPT`.

## Opus escalation

This skill runs on **Sonnet** by default for cost efficiency. When a lift fails
on Sonnet due to a reasoning-class failure (not a trivial build error), escalate
to Opus:

**Escalate to Opus when:**
- VC71/objdiff match < 95% or reviewer returns `NEEDS_RUNTIME`
- ABI audit fails (calling convention reasoning)
- FPU-WARN (operand order requires careful disassembly reading)
- Build fails on the second attempt (not a simple typo)
- Agent review returns `REJECT` for a reason that can plausibly be fixed by
  better disassembly analysis (call args, memory offsets, field rotation)

**Do NOT escalate (just revert+log) when:**
- Target has SEH prolog/epilog (not liftable with current tooling)
- Target has >3 register args (disqualified)
- Build fails on an unrelated file (repo state issue, not lift quality)

**Escalation flow:**
1. Revert the Sonnet attempt: `rtk git checkout -- src/ kb.json`
2. Re-run `/lift <target>` using Agent tool with `model: opus`
3. If Opus also fails, revert+log with both attempts recorded.

## On success — auto-commit

Unless `--dry-run` is set:
```bash
rtk git add -- src/ kb.json
rtk python3 tools/audit/generate_lift_commit.py --batch-name "<target_name>" > /tmp/commit_msg.txt
rtk git commit -F /tmp/commit_msg.txt
```

Before staging, confirm the latest review report contains:
`AUTOLIFT_REVIEW: AUTO_ACCEPT`.

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
6. `review` and `promote` are legacy subcommands for old batch artifacts.
