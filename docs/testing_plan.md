# Test Strategy And Verification Plan

## Goal

Make the existing Halo verification stack repeatable, measurable, and safe for
agents to run. This is not a proposal to build a new test architecture first.
The current high-payoff path is to operationalize the verification lanes that
already exist: Unicorn/Z3 equivalence, VC71/delink comparison, runtime oracle
captures, and shared Claude/OpenCode workflow policy.

## Current Direction

The repo already has the right foundations:

- `tools/equivalence/unicorn_diff.py` for per-function behavioral equivalence.
- `tools/equivalence/batch_verify.py` for scalable batch equivalence.
- `tools/verify/vc71_verify.py` and `tools/verify/compare_obj.py` for delinked
  structural comparison.
- `tools/verify/verify_option3.py` for runtime/xemu fallback.
- `src/halo/test_harness.c` for Xbox-context runtime checks.
- `agent-content` as the canonical source for generated Claude and OpenCode
  workflow surfaces.

The weak points are repeatability and reporting:

- workflow docs can drift between `agent-content`, `.claude`, and `.opencode`
- runtime fallback must compile before it can be trusted
- there is no single inventory of which ported functions are Unicorn/Z3-ready
- batch equivalence needs no-regression baseline comparison
- runtime oracle capture should be noninteractive and machine-readable
- the current `TEST_HARNESS` hook is small and should not block higher-value
  verification work

## Verification Matrix

| Lane | Best For | Ground Truth | Primary Tooling |
|---|---|---|---|
| Agent workflow audit | command/skill/agent policy drift | `agent-content` | `sync_agent_content.py`, `audit_agent_content.py` |
| Unicorn/Z3 equivalence | leaf, data-only, stubbable lifted functions | oracle `.obj` vs candidate `.obj` | `unicorn_diff.py`, `batch_verify.py` |
| VC71/delink compare | structural lift quality and FPU warning triage | delinked reference object | `vc71_verify.py`, `compare_obj.py` |
| Runtime oracle | functions needing live engine state | original Xbox behavior | `run_golden_tests.py`, XBDM debug capture |
| Option 3 fallback | xemu/runtime smoke fallback | boot/load/assert behavior | `verify_option3.py` |
| Host deterministic tests | narrow pure helpers with binary-backed expected values | fixed captures or math identities | focused Python/C helpers only |

Do not add Unity. Do not broaden host unit tests to engine-stateful lifted code.
Prefer XBDM/real Xbox runtime verification when available; xemu/Option 3 stays
the fallback lane.

## Phase 1: Align Agent Workflow Policy

Keep `agent-content` canonical and regenerate the derived surfaces.

Commands:

```bash
rtk python3 tools/analysis/sync_agent_content.py --check
rtk python3 tools/audit/audit_agent_content.py --strict
```

Acceptance:

- `.claude` and `.opencode` match generated `agent-content`
- command, skill, and agent parity audits pass
- safety-critical review/no-commit policy is not weakened in derived files

## Phase 2: Repair Runtime Fallback Health

`verify_option3.py` must compile and support a minimal smoke path before it is
used for broader runtime verification.

Commands:

```bash
rtk python3 -m py_compile tools/verify/verify_option3.py
rtk python3 tools/verify/verify_option3.py --target smoke --skip-build --skip-iso
```

Acceptance:

- Python syntax check passes
- skip/dry smoke path writes `artifacts/verify_option3/<run-id>/summary.json`
- later live runs can report build, ISO, objdiff, xemu load/reset, and assert
  tripwire stages

## Phase 3: Add Coverage Inventory

Use `tools/verify/test_inventory.py` to classify the current verification
surface from existing metadata instead of guessing.

The inventory reports:

- total functions and ported functions from `kb.json`
- Unicorn/Z3-verifiable functions
- counts by `leaf`, `data_only`, `stubbable`, `non_leaf`, and unclassified
- missing delinked references
- targets needing runtime oracle coverage
- deterministic blocked reasons and prior batch failure artifacts
- object-level lane summaries

Command:

```bash
rtk python3 tools/verify/test_inventory.py
```

Artifacts:

- `artifacts/test_inventory/summary.json`
- `artifacts/test_inventory/by_object.json`
- `artifacts/test_inventory/blocked_report.json`

Acceptance:

- ported count matches `kb.json`
- class counts match `tools/equivalence/leaf_cache.json`
- blocked reasons are machine-readable

## Phase 4: Make Batch Equivalence A No-Regression Lane

Keep `batch_verify.py` as the standard scalable lane. Extend it rather than
adding a parallel Unicorn runner.

Commands:

```bash
rtk python3 tools/equivalence/batch_verify.py --dry-run
rtk python3 tools/equivalence/batch_verify.py --limit 20 --seeds 10 --csv
rtk python3 tools/equivalence/batch_verify.py --baseline artifacts/batch_verify/summary.json --fail-on-new
```

Reporting must include:

- current pass/fail/error/not_applicable summary
- per-target rows in `summary.json`
- `results.csv` when requested
- object-level summaries
- baseline comparison with new failures versus known failures
- allowlisted known failures or `not_applicable` categories

Acceptance:

- dry run lists candidates from `kb.json` plus `leaf_cache.json`
- limited batch run writes JSON and CSV
- baseline comparison flags new regressions separately from known failures

## Phase 5: Structure Runtime Harness Output

Make the current Xbox harness machine-readable before splitting files or
entrypoints. Stable records are now:

```text
RUN|BEGIN|suite=xbox_harness
ASSERT|PASS|normalize3d mag|got=413BC96E|expected=413BC96E
CASE|BEGIN|clip|clip_square_x
VALUE|clip_square_x.ret|value=0004|changed=00|mask=0000000F
CASE|END|clip|clip_square_x|PASS
RUN|END|passed=31|failed=0|total=31
```

Acceptance:

- runtime output can be parsed without freeform text scraping
- malformed records are rejected by the capture script
- old `debug.txt` transport can remain in place while the format stabilizes

## Phase 6: Replace Manual Golden Capture

`tools/verify/run_golden_tests.py` is now the runtime oracle runner. It creates
temporary metadata overlays instead of asking the operator to edit `kb.json` by
hand.

Command:

```bash
rtk python3 tools/verify/run_golden_tests.py --target FUN_00106510
```

The command:

- writes oracle and candidate overlay JSON files
- builds the oracle variant with `ported=false`
- deploys and captures structured harness output
- builds the candidate variant with `ported=true`
- deploys and captures structured harness output
- diffs oracle versus candidate
- writes `artifacts/runtime_oracle/<run-id>/summary.json`

Acceptance:

- one named harness case can be captured for oracle and candidate
- parser rejects malformed output
- `diff.txt` and `summary.json` clearly show pass/fail and first mismatch

## Phase 7: Temporary Metadata Overlays

Runtime oracle builds use a temporary overlay consumed by `tools/build/patch.py`
through `HALO_KB_OVERLAY` or `--kb-overlay`. The overlay changes build-time
ported behavior without modifying `kb.json`.

Example:

```json
{
  "functions": {
    "FUN_00106510": {
      "ported": false
    }
  }
}
```

Keep test metadata out of `kb.json` unless a later implementation proves a real
need.

## Later Cleanup

These are useful, but they should not block the verification lanes above:

1. Split `TEST_HARNESS` entrypoints after the runtime oracle path is useful.
2. Move harness cases out of the monolithic file when structured capture is
   stable.
3. Add subsystem smoke scenarios after Option 3/XBDM orchestration is healthy.
4. Add narrow host deterministic tests only for pure helpers with binary-backed
   expected values, math identities, or existing golden captures.

## Standard Verification

Run the narrowest meaningful checks first:

```bash
rtk python3 tools/analysis/sync_agent_content.py --check
rtk python3 tools/audit/audit_agent_content.py --strict
rtk python3 -m py_compile tools/verify/verify_option3.py tools/verify/test_inventory.py tools/equivalence/batch_verify.py tools/verify/run_golden_tests.py tools/build/patch.py
rtk python3 tools/verify/verify_option3.py --target smoke --skip-build --skip-iso
rtk python3 tools/verify/test_inventory.py --no-write
rtk python3 tools/equivalence/batch_verify.py --dry-run
```

Full runtime oracle and XBDM checks require a reachable Xbox target.
