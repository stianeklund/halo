---
name: cleanup-report
tier: agent
triggers: ["cleanup report", "before/after report", "before after report", "cleanup pr", "cleanup summary", "cleanup baseline", "record baseline", "before cleanup", "baseline snapshot", "match floor", "score floor", "vc71_regression", "tooling gap", "gap audit", "missing detector", "missing tooling", "no detector", "tooling audit"]
description: Cleanup session lifecycle — record pre-flight baseline, audit tooling gaps, and produce the standard before/after report closing a cleanup session.
---

# Cleanup Session Lifecycle (Baseline, Gap Audit & Report)

This skill covers the complete administrative lifecycle of a cleanup session:
1. **Pre-flight Baseline:** Record codegen and match floors before editing.
2. **Tooling Gap Audit:** Confirm detectors exist for every invariant the cleanup could break.
3. **Session Report:** Produce the final before/after verification deliverable for review.

---

## 1. Pre-Flight Baseline

Cleanup of already-lifted code is only safe if you can prove codegen did not regress. **No cleanup edit is allowed before the baseline is established.**

Machine capture command: `rtk python3 tools/recovery/source_recovery.py capture <manifest> --object <obj>`

### Manual Baseline Checklist:
- **Clean working tree:** Run `rtk git status` and `rtk git diff --stat`.
- **Match floors:** Check `rtk python3 tools/verify/vc71_regression.py check --source src/halo/.../<file>.c` and `rtk python3 tools/verify/vc71_verify.py src/halo/.../<file>.c`.
- **Delinked reference:** Confirm presence with `objdump -t delinked/<obj>.obj`.
- **Hazards & Oracles:** Run `check_lift_hazards.py --changed-only` and note available equivalence or golden harness coverage.

---

## 2. Tooling Gap Audit

Before starting a cleanup category, verify that a mechanical detector exists to catch regressions in the invariants being modified:

| Invariant | Existing Detector |
|---|---|
| Byte-match preserved | `vc71_regression.py check` |
| FPU / LOADW / IMM literals | `vc71_verify.py` (`--fpu-only`, `--loadw-only`, `--imm-only`) |
| Lift hazards (intrinsics, buffers) | `check_lift_hazards.py` |
| `@<reg>` ABI drift | `extract_reg_args.py --check` / `audit_reg_abi.py` |
| Struct layout asserts | `cs()` / `co()` static assertions in `src/types.h` |

If a planned edit has no existing detector to catch a silent failure, flag it as a gap and use conservative manual verification or park the category until a detector is built.

---

## 3. Session PR Report

Produce the standard before/after summary when closing a cleanup session (or run `tools/recovery/source_recovery.py report <manifest>`):

```markdown
# Cleanup report — <TU/object> (<baseline-rev> → <head-rev>, <date>)

## Baseline
<the cleanup-baseline block, verbatim>

## Changes (one commit per category)
| commit | category | summary |
|---|---|---|
| <sha> | local renames | 34 locals in objects.c, mechanical vocab |
| <sha> | constants | 6 magic numbers → named (2 flag bits, 1 sentinel) |
| <sha> | struct/offsets | object_header defined (cs/co ×14); 41 raw offsets replaced |

## Verification
- vc71_regression check: PASS, 0 floors moved
- match table: <fn> <before%> → <after%> (must be = or ↑)
- hazards --changed-only: clean
- behavioral oracles: <tool, seeds, verdict>

## Not done / reverted
- <fn>: offset rewrite reverted (parked: <path>)

## Risks & Follow-ups
- <remaining raw offsets, next TU candidates>
```
