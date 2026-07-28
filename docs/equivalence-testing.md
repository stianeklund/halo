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

# With selected real game-state memory from xemu/XBDM capture:
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
| `weak` | <30% coverage or all returns identical | Only early-exit tested; use live memory replay or investigate |

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

### Solved injections (`concolic_z3.py`)

The heuristics above read the CMP immediate next to a branch and try
`imm`, `imm±1`. That works when the compared value *is* the global, and
fails as soon as anything sits in between — one arithmetic step, a second
condition, a value derived from two globals.

`concolic_z3.solve_uncovered()` replaces the guess with a solve. It runs
before the heuristics and its results lead the injection list:

1. Symbolize every concrete global address the run recorded a read from.
2. Walk the executed path (the union of visited PCs) from the function
   entry, lifting each instruction with `x86_to_z3` in **permissive**
   mode and accumulating each branch's condition in the direction really
   taken.
3. At the uncovered branch, assert the direction that was never executed.
4. Solve; emit the globals whose solved value differs from the observed
   one, as a single joint injection.

The heuristics still run and their injections are still appended, so this
is strictly additive — a missing z3, a `LiftError`, or an unsatisfiable
path costs nothing relative to the previous behaviour.

Three properties are worth knowing because they shape what you see:

- **Permissive lifting is correct here.** The output is a hypothesis that
  gets re-run and measured, so a dropped instruction costs seed quality,
  never correctness. Proof callers (`z3_equiv`) must keep `strict=True` —
  see Phase 0 and `X86Lifter.__init__`.
- **A register-gated branch reports nothing.** The solver is required to
  find a model in which at least one *relevant* global differs, where
  relevant means it appears in the path condition. Without that
  narrowing, Z3 would flip an unrelated global purely to satisfy
  "something in memory must differ", implying a causal link that is not
  there. `register-gated` in the stats line means the branch is not
  reachable by injection at all.
- **`CALL` is modelled as `EAX := 0`** to match what stubs actually do,
  and callee stack cleanup is not modelled.

Every attempt lands in exactly one bucket and `SolveStats.accounted()`
asserts they sum — an attempt that falls through uncounted reads as
success in the aggregate, which is the failure this harness exists to
catch. When nothing was injectable the line also says *why*:

```
concolic: z3 path solve — 4 attempted, 0 solved, 0 unreachable-by-memory,
          0 register-gated, 4 no-injectable-globals, 0 path-not-reached,
          0 timeout, 0 error [rejected: 5x null-page]
```

`HALO_CONCOLIC_NO_Z3=1` disables the lane, for A/B measurement without a
source swap. Self-test: `test_concolic_z3.py`.

#### Measured effect: none, and the reason is upstream

A/B on 60 targets under 30% coverage (30 seeds, `--allow-stubs`,
`--mem-trace`, `--no-leaf-cache`, `HALO_CONCOLIC_NO_Z3` as the switch):

| | off | on |
|---|---|---|
| mean coverage | 11.9% | 11.9% |
| ≥60% coverage | 3 | 3 |
| better / worse / unchanged | — | 0 / 0 / 59 |

The lane reached only **6 of 60** targets. It is gated behind
`coverage < 60 and passed > 0`, and **24** of the other 54 had *zero
passing seeds* — the candidate errors on every seed, so no amount of
injection-value quality applies.

On the 6 it did reach, all 32 attempts are accounted for: 22
`no-injectable-globals`, 8 `unreachable-by-memory`, 2 `register-gated`,
0 `path-not-reached`. The rejection breakdown names the real blocker —
**every** rejected address was `null-page`. These functions take 0 back
from a stubbed callee and dereference it; the auto-map hook keeps
execution alive and the read lands below 0x10000. Injecting there is
correctly refused: it would be testing behaviour the real game never
has.

So the constraint on this population is **stub-return modelling**, not
input generation. That is Phase 2's unfinished half (`--real-callees`,
`--state-snapshot`), not something Phase 3 can reach. Phase 3 is kept
because it is sound, self-tested, free when idle, and applies wherever
a function reads real globals — but it is not the lever on the ~16%
behavioural-coverage number.

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

## Live Memory Capture And Replay

For functions that depend on complex runtime state (linked lists, hash
tables, game state structs), zero-filled memory won't exercise real code
paths even with concolic feedback.

Live memory captures record selected real memory regions from a running xemu
instance via QMP/HMP virtual `memsave` or XBDM `getmem`. They are JSON
memory-region captures for Unicorn replay, not QEMU VM snapshots. Prefer QMP
virtual `memsave`; use `capture_snapshot_from_diff.py --backend xbdm` when the
running xemu is reachable through XBDM but not QMP. Physical `pmemsave` is
intentionally avoided on this setup because it reads the wrong bytes. Do not use
`savevm`/`loadvm` for oracle testing because it restores old loaded-XBE code
pages and invalidates original-vs-candidate comparisons.

### Capture explicit regions

```python
from tools.equivalence.state_snapshot import capture_from_xemu, save_snapshot

# Capture specific memory regions from running xemu
regions = capture_from_xemu([
    (0x00456600, 0x1000),   # game_allegiance_globals
    (0x00480000, 0x10000),  # game_state page
], output_path="artifacts/snapshots/multiplayer_lobby.json",
   description="4-player FFA, 2 minutes in")
```

### Capture regions from a failed/weak diff

When `unicorn_diff.py --output-json` reports auto-mapped pages or global reads,
use the helper to turn those into xemu capture regions:

```bash
python3 tools/equivalence/unicorn_diff.py FUN_000a7ae0 --allow-stubs \
    --mem-trace --output-json artifacts/equivalence/FUN_000a7ae0.json
python3 tools/equivalence/capture_snapshot_from_diff.py \
    artifacts/equivalence/FUN_000a7ae0.json --dry-run
python3 tools/equivalence/capture_snapshot_from_diff.py \
    artifacts/equivalence/FUN_000a7ae0.json \
    --description "live xemu state for FUN_000a7ae0"

# Fallback when QMP is unavailable but XBDM is reachable:
python3 tools/equivalence/capture_snapshot_from_diff.py \
    artifacts/equivalence/FUN_000a7ae0.json \
    --backend xbdm \
    --description "live XBDM state for FUN_000a7ae0"
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

If coverage remains weak with a real memory snapshot, stop adding random seeds
and move the target to runtime oracle or dual-oracle harness coverage.

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

## What The Harness Deliberately Does Not Compare

The stub-argument differential records every stubbed callee's arguments on both
sides and fails a seed when they differ. Some differences are guaranteed by
construction and are excused — but always *reported*, so an exemption never
silently hides evidence. They appear in the run summary:

```
stub-arg differential: 100 calls, 0 arg mismatch(es), 0 soft-matched stack ptr(s), 50 memset-fill
```

| Exemption | Why it cannot indicate a lift bug |
|-----------|-----------------------------------|
| `display_assert` args 0–2 | `(reason, filepath, lineno, halt)`. Our sources do not reproduce the original's line numbering, and our string literals land in a different section. Arg 3 (`halt`) **is** compared, and an assert that fires on only one side still shows as a call-sequence divergence. |
| `csmemset` arg 1, equal mod 256 | The fill value is converted to `unsigned char`, so `memset(p, -1, n)` and `memset(p, 0xff, n)` write identical bytes. A different low byte still fails. VC71's `[IMM-WARN]` is what catches the wrong literal. |
| Stack pointers on both sides | MSVC and clang lay out frames differently. |
| DIR32 slot pointers on both sides | The two sides get **disjoint** slot ranges (`lft_globals_base = GLOBALS_BASE + len(orc_data_slots) * 256`), so the candidate's arena can run past `GLOBALS_BASE + GLOBALS_SIZE`. `set_globals_arena_top()` tells the comparator how far it actually reaches. |

Each exemption is pinned by `test_stub_arg_trace.py`, together with the case
that must still fail (wrong `halt`, wrong fill low byte, wrong memset size, and
the rules not applying to other callees).

## Reaching Past The Early-Exit Path

Two things kept functions pinned at low coverage:

- **No auto-map for leaves.** The unmapped-access auto-map hook was gated on
  `map_globals`, which is only set for stubbed non-leaf runs. A leaf that reads
  a global — or dereferences an `int`-typed parameter that is really a pointer —
  died on its first access with `UC_ERR_READ_UNMAPPED` before executing
  anything. `auto_map_unmapped=True` installs the hook without also mapping and
  seeding the globals region.
- **Accessors stubbed to 0.** `object_get_and_verify_type`, `datum_get`,
  `tag_get`, `tag_block_get_element` and `global_scenario_get` all return
  `void *`, and a stub returning 0 reads as "not found", so every caller took
  its NULL-check bail-out. They now return distinct pages in a scratch arena
  (`STUB_OBJECT_ARENA`, `stubs.py`) — distinct so that a caller confusing two
  accessors' results still shows as a difference, identical across both sides
  so the differential stays sound, and zero-filled on first touch by the
  auto-map hook so the dereference that follows is safe.

Both changes are symmetric across oracle and candidate. They make functions
execute *more* of their body, so they can surface divergences that were
previously hidden behind an early return — that is the intended outcome, and
those belong in the ledger as new entries rather than being treated as a
regression.

### Measured effect

Controlled before/after, same targets and seeds, `--no-leaf-cache`, HEAD versus
the changes. The default configuration only — `--rich-stub-returns` is off:

| Sample | Metric | Before | After |
|---|---|---|---|
| 100 functions below 30% coverage | mean coverage | 6.8% | **16.2%** |
| | at/above the 60% `high` threshold | 3 | **12** |
| | verdict `error` → `pass` | — | **8** |
| | coverage regressions | — | **0** |
| 100 previously-passing functions | `pass` → `fail`/`error` | — | **0** |
| | coverage regressions | — | **0** |

Re-running a random 20 of the 115 ledger entries classified
`assert_metadata` / `stack_ptr_args` / `benign_arg_width`: **19 now pass**.

One caveat this exercise exposed: only 91 of the 100 sampled
"previously-passing" functions still passed at HEAD, before any change. The
batch summary's verdicts are reused via `--skip-existing`, so a stale `pass`
can persist across source changes. Measure a baseline by re-running, not by
reading the last batch summary — an apparent regression is often drift.

## Divergence Triage And The Ledger

A batch run reports N divergences; on its own that number is not actionable,
because most divergences are produced by the harness rather than by the lift.
`tools/equivalence/triage_failures.py` classifies each one from its smoke log
and persists the result to `tools/equivalence/divergence_ledger.json`.

```bash
# Classify, write/merge the ledger, and print the prioritized bug list
python3 tools/equivalence/triage_failures.py \
    --summary-path artifacts/batch_verify/summary.json \
    --ledger --bug-list
```

Every category maps to one of three **buckets**:

| Bucket | Meaning |
|--------|---------|
| `harness-artifact` | The emulator, not the lift, produced the difference |
| `needs-evidence` | Cannot be adjudicated from the smoke log alone |
| `suspect-real` | Candidate lift bug — investigate |

The two classes that dominate a real batch, and why they are artifacts:

- **`assert_metadata`** — `_display_assert(message, __FILE__, __LINE__)`. Our
  lifted sources do not reproduce the original's line numbering, so `arg[2]`
  differs by construction (`oracle=0xa89 candidate=0x374` is a real observed
  pair), and our string literals land in a different section than the
  original's, so `arg[0]`/`arg[1]` differ too. The exemption is narrow: it
  applies only when *every* differing value is either a small integer (a line
  number) or an image-range pointer (a literal). A garbage or stack value in an
  assert argument is **not** exempted, and any co-occurring divergence — a real
  callee's argument, EAX, ST0, scratch, or a call-sequence change — overrides
  the exemption entirely.
- **`stack_ptr_args`** — MSVC (oracle) and clang (candidate) lay out frames
  differently, so `&local` passed to a stubbed callee has a different numeric
  value on each side however faithful the lift is.

The strongest evidence class is **`arg_mismatch`**: a wrong argument to a
*named* non-assert callee. It reports the callee and argument index, which is
exactly the `docs/lift-learnings.md` §10 caller-side argument swap/drop
signature.

**Priority** is assigned by crossing the behavioural verdict with the
structural one (VC71 match, read from `artifacts/verify_cache/vc71.sqlite`):

| Priority | Rule |
|----------|------|
| `P0` | `suspect-real` **and** VC71 < 85% — diverges behaviourally *and* structurally, so it is a lift bug rather than a structural ceiling (§19) |
| `P1` | `suspect-real` `arg_mismatch` at VC71 ≥ 85% |
| `P2` | any other `suspect-real` |

Ledger entries are merged, not overwritten: `first_seen` is preserved, and a
`status` a human has moved off its auto value (to `confirmed-bug` or `fixed`)
survives re-runs. An entry that stops diverging is kept with status
`no-longer-diverging` rather than silently disappearing. Each entry records
`evidence_log_date` and an `evidence_stale` flag, so a verdict classified from
a smoke log older than the batch summary is visible as such — re-run those
targets before acting on them.

Self-test: `python3 tools/equivalence/test_triage_classify.py`.

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
  state_snapshot.py     — Live memory capture and replay
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
  triage_failures.py    — Divergence classifier + ledger writer
  divergence_ledger.json— Persistent per-divergence category/status/priority
```
