---
description: Run the standard verification chain from docs/testing_plan.md
subtask: false
---

Run the standard verification chain covering audit, inventory, and batch
equivalence. Non-interactive; suitable for CI.

Argument: $ARGUMENTS (`[--limit N] [--batch-csv] [--skip-build] [--skip-iso]`)

The standard chain executes these phases in order:

**Phase 1: Python Syntax & Runtime Fallback Smoke**
```bash
rtk python3 -m py_compile \
  tools/verify/verify_option3.py \
  tools/verify/test_inventory.py \
  tools/equivalence/batch_verify.py \
  tools/verify/run_golden_tests.py \
  tools/build/patch.py
rtk python3 tools/verify/verify_option3.py --target smoke --skip-build --skip-iso
```

**Phase 2: Coverage Inventory**
```bash
rtk python3 tools/verify/test_inventory.py --no-write
```
- Reports: total functions, ported, Unicorn/Z3 verifiable, lane counts
- Artifacts: `artifacts/test_inventory/summary.json`, `by_object.json`, `blocked_report.json`

**Phase 3: Batch Equivalence Dry-Run**
```bash
rtk python3 tools/equivalence/batch_verify.py --dry-run
```
- Lists all Unicorn/Z3 candidates from `kb.json` + `leaf_cache.json`
- Does not execute any emulation
- Targets with weak coverage should be rerun later with xemu `pmemsave` state
  snapshots through `unicorn_diff.py --state-snapshot`.

**Phase 3b: Limited Batch Run (with --limit)**
```bash
rtk python3 tools/equivalence/batch_verify.py \
  --limit N --csv --baseline artifacts/batch_verify/summary.json --fail-on-new
```
- Runs N candidates with CSV output and baseline comparison
- Fails if new regressions vs known failures
- Use `--limit 0` to skip the batch run

**Phase 4: Runtime Oracle Follow-Up**
- Use `rtk python3 tools/verify/run_golden_tests.py --target <target>` for
  stateful targets that cannot be proven through delink/equivalence.
- Prefer dual-oracle harness cases for high-value stateful targets once
  available; they compare original and candidate in one initialized engine run.

Flags:
- `--limit N` — run N candidates in batch_verify (0=skip batch run)
- `--batch-csv` — write results.csv alongside summary.json
- `--skip-build` — pass through to verify_option3 smoke
- `--skip-iso` — pass through to verify_option3 smoke

Also available as a single script:
```bash
rtk python3 tools/verify/verify_all.py [--limit N --batch-csv]
```

Report:
- per-phase pass/fail with exit codes
- summary.json and results.csv paths
- new failures vs known failures (with --limit)
- test inventory counts
