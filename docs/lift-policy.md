# Lift Policy Reference

Shared reference for `/auto-lift`, `/goal-lift`, and any future orchestration skill.
All numeric thresholds, escalation logic, and Phase-1 briefing templates live here.
Skills reference this document rather than duplicating prose.

---

## Verify policy presets

Pass `--verify-policy <PRESET>` to `tools/lift_pipeline.py`.

| Preset | Description |
|--------|-------------|
| `auto` | Default. Enable verification for risky functions when inputs exist; fail gracefully if no data. |
| `strict` | Fail if risky function cannot be structurally verified (requires delinked reference). |
| `goal90` | Goal-lift preset — see the **goal90 pass/fail bands** table below. |
| `manual` | Only verify when explicit `--verify-input` / `--verify-auto` flags are set. |

### goal90 pass/fail bands

| VC71 / structural match | Action |
|-------------------------|--------|
| ≥99% | `goal90: PASS` — byte-match sufficient, commit |
| 90–98% | `goal90: PASS` — meets policy, commit |
| 85–89% | `goal90: PASS` with "permuter recommended" note — commit after one permute pass |
| 65–84% | `goal90: FAIL` — check structural cap; one focused Opus escalation allowed if not capped |
| <65% | `goal90: FAIL` — assume lift bug, revert unless there is a clear, cheap fix |
| No VC71 data | Treat as infra/build issue, not pass |

**Max 3 total attempts per function** (including re-delink, permutation, Opus escalation).
After 3 failures → revert and skip.

---

## Oracle-strength model routing

The `select` output (`tools/llm_auto_lift.py select`) now includes an `oracle_strength`
column.  Use it to choose the Phase-1 model:

| oracle_strength | Meaning | Suggested Phase-1 model |
|-----------------|---------|------------------------|
| `strong` | Delinked ref + pure-leaf or stubbable (leaf_cache) + no @\<reg\> args | Haiku (cheap, deterministic oracle catches mistakes) |
| `medium` | Delinked ref only | Sonnet → Opus escalation |
| `weak` | No delinked ref, or reg-args, or known structural-ceiling class | Sonnet → Opus (never Haiku) |

**Pilot status:** Haiku lane not yet adopted.  Run 10 strong-oracle targets with
`model="haiku"` in Phase 1, record VC71 pass rate + escalation rate, and commit
results to `artifacts/auto_lift/haiku_pilot_report.md`.  Adopt only if first-pass
≥90% rate is within ~15pp of Sonnet's baseline.

---

## Phase-1 subagent briefing template

Both `/auto-lift` and `/goal-lift` use this template.  Fill in the bracketed fields.

```
Target: [address] ([name], [object_file])

KB entry:
[paste JSON from: rtk jq '[.. | objects | select(.addr? == "0xADDR")] | .[0]' kb.json]

Decompilation:
[paste from Ghidra MCP decompile_function or cached context pack]

Source file: [path to .c file]
Object: [object_file_name]

File-write instructions (write directly to the repo, do not output code blocks):
1. Write C89 implementation to the source file at the correct address-ordered position
   (after the preceding function, before the following function by address).
2. Update kb.json declaration conservatively if the Ghidra signature is clearer.
3. Update tools/kb_reg_baseline.json for any @<reg> annotations added or changed.
4. Run: rtk python3 tools/analysis/maintain.py [source_file]
5. Run: rtk python3 tools/audit/check_lift_hazards.py
   Fix any hazards that are specific to this target (ERROR-level are blockers).
6. Report format:
   RESOLVED_TARGET: [name]
   Confirmed: [list facts taken directly from disasm/decompile]
   Inferred: [list probable interpretations]
   Uncertain: [list unknowns]
   kb.json updates: [list changes made or "none"]
```

---

## Escalation flow

Applied identically in both `/auto-lift` and `/goal-lift`.

**Escalate to Opus when:**
- VC71 match < 65% (control flow / structure wrong)
- ABI audit fails (calling convention reasoning)
- FPU-WARN present (operand order requires careful disasm reading)
- Build fails on the second attempt (not a simple typo)

**Do NOT escalate — just revert+log — when:**
- Target has SEH prolog/epilog (not liftable with current tooling)
- Target has >3 register args (disqualified)
- Build fails on an unrelated file (repo state issue, not lift quality)

**Escalation steps:**
1. Revert the Sonnet attempt:
   ```bash
   rtk git checkout -- src/ kb.json tools/kb_reg_baseline.json
   ```
2. Re-run Phase 1 using `Agent(subagent_type="xbox-halo-re-analyst")` without
   the `model: "sonnet"` override — the agent's default model (Opus) kicks in.
   Include the same Phase-1 briefing prompt as the original attempt.
3. Re-run Phase 2 (`lift_pipeline.py`).
4. If Opus also fails → revert+log with both attempts recorded; skip target.

---

## Failure log format

Write to `artifacts/auto_lift/failures/<target_name>.json`:

```json
{
  "target": "<name>",
  "addr": "<0x...>",
  "object": "<object_name>",
  "timestamp": "<ISO 8601>",
  "attempts": [
    {"model": "sonnet", "failure_stage": "<stage>", "error_summary": "<msg>"},
    {"model": "opus",   "failure_stage": "<stage>", "error_summary": "<msg>"}
  ],
  "pipeline_output": "<full pipeline stderr/stdout from last attempt>"
}
```

---

## /mass-lift status

`/mass-lift` appears in the session skill registry with a description but has
**no implementation** in this repo or `~/.claude/`.  Do not invoke it expecting
the described behavior.  Implement it as a thin orchestration document over the
`goal90` policy preset when needed.

---

## Notes on legacy subcommands

`/auto-lift review` and `/auto-lift promote` are legacy subcommands for old
batch-artifact workflows.  They still exist in the code (`tools/llm_auto_lift.py`)
but are not part of the standard lift loop.  Use `/auto-lift cache-context` and
`/auto-lift select` instead.
