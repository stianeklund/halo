---
name: cleanup-gap-audit
tier: agent
triggers: ["tooling gap", "gap audit", "missing detector", "missing tooling", "no detector", "tooling audit"]
description: Before a cleanup campaign, audit whether the infrastructure needed to do it SAFELY exists — a detector/gate for every invariant the cleanup could break — and propose small, delegated tooling tasks for the gaps. Extends the repo's "learnings-must-ship-a-detector" rule from bugs to cleanup invariants.
---

# Tooling Gap Audit

A cleanup category is only safe when every invariant it can break has a mechanical
detector. This skill inventories what exists, names what's missing, and turns each gap
into a small, single-purpose tooling task — it does **not** build the tools inline
during a cleanup session.

## Step 1 — Inventory what already exists

Do not propose a tool that duplicates these:

| Invariant | Existing detector |
|---|---|
| Byte-match preserved | `vc71_regression.py check` (floors in `vc71_scores.json`) |
| FPU operand order / field width / literals | `vc71_verify.py` `--fpu-only` / `--loadw-only` / `--imm-only` |
| Lift hazards (intrinsics, buffer alias, CONCAT, float-smuggling, …) | `check_lift_hazards.py` (+ pre-commit + CI) |
| XCALL type mismatch | `check_xcall_types.py` |
| `@<reg>` ABI drift | `extract_reg_args.py --check`, `audit_reg_abi.py` |
| Arg-count vs `ADD ESP,N` | `check_arg_counts.py` |
| Caller register setup | `dump_caller_regsetup.py` |
| Struct layout asserts | `cs()`/`co()` macros in `src/types.h` (compile-time) |
| Behavioral equivalence | `unicorn_diff.py`, golden harness, `ab_check.py` |
| Raw fn-ptr casts | `check_raw_casts.py` |
| Source-file organization | `tools/analysis/maintain.py` |

## Step 2 — Map planned cleanup → invariants → gaps

For each cleanup category planned this session, ask: *if this edit silently broke its
invariant, which existing detector would fire?* If the answer is "none", that's a gap.

Recurring gap examples for cleanup work (check whether they've since been built before
proposing — `rtk fd <keyword> tools/`):

- **Rename-only verifier**: prove a diff is pure identifier renaming (token-level diff
  ignoring identifiers) — makes local-var-cleanup and naming commits reviewable at a glance.
- **Offset-rewrite equivalence**: for `offset-to-struct`, a checker that maps each removed
  `*(T*)(base+0xNN)` to a struct field whose `co()` assert matches `0xNN` and whose type
  width matches `T`.
- **Enum/constant bit-pattern check**: replaced literal compiles to the identical
  immediate (extends the `[IMM-WARN]` census to named constants).
- **Struct coverage report**: which ported TUs still access a given struct by raw offset.

## Step 3 — Propose delegated tasks

For each gap, emit a task spec (small enough for one subagent/session):

```
GAP: <invariant with no detector>
RISK IF SKIPPED: <what silently ships>
PROPOSED TOOL: tools/audit/<name>.py — <one-line contract, exit codes>
WIRES INTO: check_lift_hazards.py | pre-commit | vc71_verify | standalone
SIZE: S (<150 LOC) / M (<400)
FALLBACK UNTIL BUILT: <manual procedure or "block this cleanup category">
```

## Rules

- A gap with no detector and no manual fallback **blocks** that cleanup category —
  downgrade the plan (e.g. skip control-flow cleanup, keep comments/renames only).
- Follow the repo rule: a documented grep/regex counts as a detector only when actually
  implemented as a check.
- Proposals go to the user / task list; building them mid-cleanup mixes concerns.
