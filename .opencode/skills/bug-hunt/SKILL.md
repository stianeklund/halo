---
name: bug-hunt
description: >-
  Tiered automated bug scanner for Halo CE Xbox lifts. Run after editing source
  files, before deploying to Xbox, before committing, as a pre-commit check, as
  a safety scan, or as an audit. Checks for silent correctness bugs
  (float-as-pointer, void-EAX, address-offset misreads, accumulator misreads,
  discarded builder counts), lift hazards (duplicate args, undersized buffers,
  intrinsic calls, CONCAT survival), register argument mismatches, ABI drift,
  and visual/behavioral regressions. Delegates to check_lift_hazards.py,
  check_callee_reg_args.py, batch_equivalence.py, and extract_reg_args.py.
---

# Bug Hunt — Tiered Automated Bug Scanning

**Auto-triggered** after any `Write`/`Edit` to `src/` or `kb.json`.  
**Manual:** `/bug-hunt [--all]` runs T0+T1; `--full` adds T3.

Every check delegates to an existing script — this skill is the decision tree.

---

## Tier 0 — Edit-time (<5s, after every Write/Edit to src/)

```bash
rtk python3 tools/audit/check_lift_hazards.py --changed-only 2>&1 | head -60
```

If WARN or ERROR lines appear: investigate before continuing. Known false positives
from pre-existing noise in untouched files are suppressed by `--changed-only`.

---

## Tier 1 — Build-time (<30s, after cmake --build)

```bash
rtk python3 tools/audit/check_callee_reg_args.py 2>&1 | tail -20
rtk python3 tools/audit/extract_reg_args.py --check 2>&1
```

`check_callee_reg_args.py`: any MISSING line = blocker.  
`extract_reg_args.py --check`: non-zero exit = `@<reg>` drift, run `--apply` to sync.

---

## Tier 2 — Crash-time (<10s, auto-triggered on crash output)

Delegate to `crash-triage` skill — parses register patterns, matches signal table,
proposes root cause. Then follow `halo-page-fault` for detailed investigation.

---

## Tier 2.5 — Visual/behavioral regression (<5min, on wrong runtime behavior)

When the game runs but produces wrong output (wrong color, missing geometry,
invisible objects, features doing nothing, wrong positions):

```bash
# 1. Verify the deploy is live and correct
rtk python3 tools/xbox/verify_toggles_live.py

# 2. Run silent-bug grep sweep
grep -rn '(float)(int)' src/ --include='*.c'
grep -rnE '\*\([a-zA-Z_]+ \*\)0x[0-9a-f]+ \+ [0-9]' src/ --include='*.c'
```

**Routing:**
- If grep finds hits in recently touched files → load `lift-silent-bugs` skill
- If grep is clean → load `debug` skill Section D (toggle-bisect) to isolate
- If assert in debug.txt → load `debug` skill Section B (assert triage)
- If hang/freeze → load `debug` skill Section C (hang investigation)

---

## Tier 3 — Pre-deploy (~2min, before Xbox deploy)

```bash
# Silent-bug grep sweep (lift-silent-bugs checks 1-5)
grep -rn '(float)(int)' src/ --include='*.c'
grep -rnE '\*\([a-zA-Z_]+ \*\)0x[0-9a-f]+ \+ [0-9]' src/ --include='*.c'

# Equivalence on changed functions
rtk python3 tools/equivalence/batch_equivalence.py --changed-only --seeds 100 --allow-stubs 2>&1 | tail -30

# Full hazard scan
rtk python3 tools/audit/check_lift_hazards.py 2>&1 | tail -40

# Delta arg-count audit (only check recently changed signatures)
rtk python3 tools/audit/check_arg_counts.py --recent-commits 3 2>&1 | tail -20
```

---

## Result interpretation

| Output | Action |
|--------|--------|
| `ERROR` / `HIGH` in any tier | Blocker — fix before continuing |
| `WARN` / `MISSING` | Review item — check if in touched files |
| Clean | Proceed |

When blocked: load the relevant detailed skill (`lift-silent-bugs`, `lift-arg-hazards`,
`lift-decompiler-traps`, `lift-frame-hazards`) for fix guidance.

Never re-run the same check twice in one session. The results are authoritative.
