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
| 65–84% | `goal90: FAIL` — check structural cap; if not capped, enter the opus effort ladder (see §Escalation-flow) |
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
| `medium` | Delinked ref only | Opus-high, then the effort ladder |
| `weak` | No delinked ref, or reg-args, or known structural-ceiling class | Opus-high, then the effort ladder (never Haiku) |

**Pilot status:** Haiku lane not yet adopted.  Run 10 strong-oracle targets with
`model="haiku"` in Phase 1, record VC71 pass rate + escalation rate, and commit
results to `artifacts/auto_lift/haiku_pilot_report.md`.  Adopt only if first-pass
≥90% rate is within ~15pp of the Opus first-pass baseline.

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

Applied identically in both `/auto-lift` and `/goal-lift`.  Canonical
implementation: the model/effort policy block in
`.claude/workflows/goal-lift.js`.

Escalation is an **opus effort ladder**, not a model swap.  Fable is not a rung
and is never routed to by default (user policy 2026-08-10: fable only when
explicitly requested).  The improve-pass drain runs on opus; pass
`--improveModel fable` to opt in for the hardest parked targets
(routing_stats measured fable-high at 80% promote, +14.7pp mean gain vs
opus-high 52%, 2026-08-07).

### Ladder

```
attempt 1   lift                             opus-high
gate        VC71 >= bar                      → commit lane
[65,85), not capped, score-context classification matches a recipe-atlas rule id
            → apply the mechanical lever     opus-low   (no escalation slot charged)
[65,85), not capped, no rule match
            → optimizer rung 1               opus-medium
            → rung 2                         opus-xhigh
            → rung 3                         opus-max
ladder exhausted, still sub-bar
            → park with cap hypothesis + warm-start patch
```

Each next rung runs only when all of these hold:
- the previous rung gained < 1pp,
- the target is still below the pass bar,
- the target is not structurally capped,
- remaining budget ≥ `ESCALATION_BUDGET_FLOOR` (120k),
- fewer than `MAX_ESCALATIONS` (3) targets have entered the ladder this run.

Rungs come from `IMPROVE_EFFORTS`; override with `--improveEfforts`,
`--escalationBudgetFloor`, `--maxEscalations`.

### Park (ladder exhausted)

`tools/lift/park.py park` records the attempt as `{model, effort, score}` with a
cap hypothesis and the warm-start patch (the diff, under `artifacts/parked/`),
then reverts the tree.  Never checkout-discard sub-bar work.

### Structure-wrong signals — one fresh-model re-lift

- VC71 match < 65% (control flow / structure wrong)
- ABI audit fails (calling convention reasoning)
- FPU-WARN present (operand order requires careful disasm reading)
- Build fails on the second attempt (not a simple typo)

These mean the structure is wrong, not that the score needs tuning — a higher
effort rung does not help.  Route one re-lift through the improve-pass drain.
A *different* model gives perspective diversity — **Fable** is the strongest
option but is opt-in only (`--improveModel fable`, never routed to by default).
`park.py next --exclude-model <model>` picks the closest-to-bar parked function
that model has not already tried, so the ledger drains model-by-model instead of
retrying the same model.  If that attempt also fails → revert+log with every
attempt recorded; skip the target.

**Do NOT escalate — just revert+log — when:**
- Target has SEH prolog/epilog (not liftable with current tooling)
- Target has >3 register args (disqualified)
- Build fails on an unrelated file (repo state issue, not lift quality)

**[85,98]% with a delinked reference → permuter, not a bigger model.**

Escalation triggers are **realized signals only**: a measured sub-1pp gain after
a rung, or a hard gate failure.  Do not add predictive futility heuristics —
three were tested and measured dead.

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
    {"model": "opus",  "effort": "high",   "failure_stage": "<stage>", "error_summary": "<msg>"},
    {"model": "opus",  "effort": "medium", "failure_stage": "<stage>", "error_summary": "<msg>"},
    {"model": "opus",  "effort": "xhigh",  "failure_stage": "<stage>", "error_summary": "<msg>"}
  ],
  "pipeline_output": "<full pipeline stderr/stdout from last attempt>"
}
```

---

## /mass-lift status — RETIRED (2026-07-07)

`.claude/workflows/mass-lift.js` was **removed** in the 2026-07-07 architecture
review.  It was unwired (no command/skill backed it), committed without the
`xbox-halo-lift-reviewer` fail-closed gate, carried no central model/effort
policy, and its "Opus escalation" was a same-model *same-effort* retry with no
ladder, budget floor, or per-run cap.  `goal-lift.js` is the
single governed mass-decompilation orchestrator; use `/goal-lift` instead.  If a
mass path is ever needed again, extract goal-lift's `M` policy + reviewer gate
into a shared module rather than reviving the old workflow.

---

## Auto-lift helper subcommands

`/auto-lift select`, `/auto-lift score`, and `/auto-lift cache-context` are the
supported helper subcommands. Old batch-artifact review/promote flows have been
retired; use the failure records under `artifacts/auto_lift/failures/` and the
standard `/lift` verification loop for follow-up work.
