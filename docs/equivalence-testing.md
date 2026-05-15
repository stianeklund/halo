# Equivalence Testing: Coverage, Concolic, and Memory Traces

## Overview

`tools/equivalence/unicorn_diff.py` runs MSVC-compiled oracle code and
clang-compiled lifted code in parallel Unicorn x86 emulators, comparing
CPU/FPU state and memory writes after each run.

Three phases of testing:

1. **Phase 1 — Random seeds**: corner-case and random inputs (existing)
2. **Phase 2 — Concolic feedback**: automatic when Phase 1 coverage < 60%;
   injects non-zero values into zero-filled global memory to reach untested
   branches
3. **Memory-trace differential**: compares all non-stack memory writes
   between oracle and candidate, catching side-effect bugs

## Quick Start

```bash
# Leaf function (pure math, no globals):
python3 tools/equivalence/unicorn_diff.py vector3d_scale_add --seeds 100

# Non-leaf function (reads game state):
python3 tools/equivalence/unicorn_diff.py FUN_000a7ae0 --allow-stubs --seeds 50

# With memory-trace comparison:
python3 tools/equivalence/unicorn_diff.py FUN_000a7ae0 --allow-stubs --mem-trace

# With real game state from xemu snapshot:
python3 tools/equivalence/unicorn_diff.py FUN_000a7ae0 --allow-stubs \
    --state-snapshot artifacts/snapshots/multiplayer_lobby.json

# Disable concolic Phase 2 (for speed):
python3 tools/equivalence/unicorn_diff.py FUN_000a7ae0 --allow-stubs --no-concolic
```

## Coverage Tracking

Every run reports instruction-level code coverage:

```
coverage: 47/47 bytes (100.0%) — confidence: high
```

| Confidence | Meaning | Action |
|------------|---------|--------|
| `high` | >60% coverage, diverse return values | Strong evidence of equivalence |
| `moderate` | >60% coverage after concolic, or >30% with diversity | Reasonable evidence; investigate if critical |
| `weak` | <30% coverage or all returns identical | Only early-exit tested; use state snapshot or investigate |

Coverage and confidence are persisted to `tools/equivalence/leaf_cache.json`
and consumed by:
- `tools/lift_pipeline.py` — shown in equivalence stage details
- `tools/llm_auto_lift.py` — `+3 eq_high_conf` scoring bonus
- `tools/equivalence/batch_verify.py` — included in batch reports

## Concolic Feedback (Phase 2)

When Phase 1 coverage is below 60%, Phase 2 activates automatically:

1. **Disassemble** the oracle function, find all conditional branches
2. **Identify uncovered branches** — directions never taken during Phase 1
3. **Analyze memory conditions** — for each untaken branch, find what
   memory reads influence it (CMP [mem], imm patterns)
4. **Generate injections** — values that would force the untaken direction
5. **Re-run** with injected memory, accumulate coverage

Example output:
```
coverage: 32/84 bytes (38.1%) — confidence: weak
WARNING: all 20 seeds returned identical value (0x00000000) — low path diversity

concolic: 1 uncovered branch(es), 3 injection(s)
concolic result: 15 seeds, coverage 38.1% → 94.0%, confidence: moderate
```

The concolic module (`tools/equivalence/concolic.py`) handles:
- `CMP [mem], imm` — inject value satisfying the untaken condition
- `TEST [mem], mask` — inject mask value
- Fallback — for all zero-valued global reads, try small non-zero values

Disable with `--no-concolic` if Phase 2 causes false positives on
functions with deliberately uninitialized state.

## Memory-Trace Differential

`--mem-trace` records all memory writes (excluding stack) during emulation
and compares the write sets between oracle and candidate.

This catches bugs that return-value comparison misses:
- **Wrong struct offset**: write to `field+4` instead of `field+8`
- **Missing writes**: function should call memset but doesn't
- **Write to wrong address**: `table[i+1]` instead of `table[i]`
- **Extra writes**: lifted code writes to an address the oracle doesn't

Enabled by default in `lift_pipeline.py` and `batch_verify.py`.
Use `--verbose` to see per-seed trace diffs.

## State Snapshots

For functions that depend on complex runtime state (linked lists, hash
tables, game state structs), zero-filled memory won't exercise real code
paths even with concolic feedback.

State snapshots capture real memory from a running xemu instance:

### Capture a snapshot

```python
from tools.equivalence.state_snapshot import capture_from_xemu, save_snapshot

# Capture specific memory regions from running xemu
regions = capture_from_xemu([
    (0x00456600, 0x1000),   # game_allegiance_globals
    (0x00480000, 0x10000),  # game_state page
], output_path="artifacts/snapshots/multiplayer_lobby.json",
   description="4-player FFA, 2 minutes in")
```

### Manual snapshot (no xemu needed)

Create a JSON file:
```json
{
  "description": "allegiance table with 3 teams",
  "captured_at": "2026-05-15T12:00:00+00:00",
  "regions": {
    "0x00500100": "0300",
    "0x00500110": "0100020001000300"
  }
}
```

### Use in testing

```bash
python3 tools/equivalence/unicorn_diff.py FUN_000a7ae0 --allow-stubs \
    --state-snapshot artifacts/snapshots/multiplayer_lobby.json
```

The snapshot data is written into Unicorn's memory before execution,
replacing zero-fill. Both oracle and candidate see identical initial state.

## JSON Output Schema

`--output-json` writes structured results consumed by the pipeline:

```json
{
  "target": "FUN_000a7ae0",
  "status": "pass",
  "applicable": true,
  "reason": null,
  "passed": 35,
  "failed": 0,
  "errors": 0,
  "seeds": 35,
  "coverage_pct": 94.0,
  "unique_returns": 1,
  "confidence": "moderate",
  "trace_diffs": 0,
  "concolic_seeds": 15,
  "phase1_coverage_pct": 38.1,
  "log_path": "artifacts/equivalence/FUN_000a7ae0_smoke.log"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `coverage_pct` | float | Final code coverage after all phases |
| `confidence` | string | `high`, `moderate`, or `weak` |
| `trace_diffs` | int | Seeds with memory-trace divergences |
| `concolic_seeds` | int | Additional seeds from Phase 2 (0 if not triggered) |
| `phase1_coverage_pct` | float | Coverage before concolic (only if concolic ran) |
| `unique_returns` | int | Distinct EAX values across all seeds |

## Integration Points

| Consumer | What it uses | How |
|----------|-------------|-----|
| `tools/lift_pipeline.py` | `--output-json` | Gates equivalence stage; shows confidence in details |
| `tools/equivalence/batch_verify.py` | CLI invocation | Passes `--mem-trace` by default |
| `tools/llm_auto_lift.py` | `leaf_cache.json` | `+3 eq_high_conf` for high-confidence entries |
| `/verify equivalence` skill | CLI invocation | Delegates to unicorn_diff |

## File Map

```
tools/equivalence/
  unicorn_diff.py       — Main differential tester (Phase 1 + 2 + trace)
  concolic.py           — Branch analysis and memory injection generation
  state_snapshot.py     — Snapshot capture (xemu) and replay
  seeds.py              — Input seed generator (corner + random + safe-mode)
  state.py              — CPU state capture, comparison, trace diff
  stubs.py              — Non-leaf callee stubbing and relocation patching
  abi.py                — Calling convention and argument marshaling
  coff_loader.py        — COFF .obj parser
  z3_seeds.py           — Z3-based branch-coverage seed generation
  z3_equiv.py           — Z3 formal equivalence prover (small leaf functions)
  x86_to_z3.py          — x86 → Z3 abstract interpreter
  known_globals.json    — XBE global values for emulator seeding
  leaf_cache.json       — Persistent classification + confidence cache
  batch_verify.py       — Batch runner for multiple functions
```
