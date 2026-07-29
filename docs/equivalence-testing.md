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

**Historical caveat on entries recorded before 2026-07-28.** `run_diff` bound
the target address once near its top and wrote it to the cache ~1100 lines
later, and in between six loops spelled `for addr, ... in` (the `global_reads`
map and the five mem-trace diff lists) rebound that same name at function
scope. When any of them iterated, the measurement was filed under a *memory*
address instead of the function's — so the target's own entry silently never
updated, and the cache collected keys that are not functions at all. 159 such
keys were present (`0x100xxxxx` is `STUB_OBJECT_ARENA`; `0x1`, `0x10`, `0x6c`
and friends are offsets), against a real function span of `0x12000..0x24d009`.
They have been removed, and `test_leaf_cache_key.py` pins the call site with an
AST check — behavioural, because whether the bug bites depends on whether a
given run happens to observe global reads or trace differences, while the shape
is always wrong. Treat pre-fix `coverage_pct` values as a floor: some targets
were never credited with their best run.

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

### Executing from a data page (the all-errors population)

A target where every seed errors has no verdict at all, and coverage work
cannot see it — there is no coverage to improve. The nightly recorded **456
such rows**, and the dominant fault was not what the name suggests:

| fault (first seed) | count |
|---|---|
| `UC_ERR_FETCH_UNMAPPED` | **315** (186 oracle-side, 129 lifted-side) |
| `UC_ERR_READ_UNMAPPED` | 47 |
| timeout | 40 |
| insn-limit | 25 |
| `UC_ERR_INSN_INVALID` | 22 |

Read the side attribution with care: the oracle runs first, so an
`ORACLE-CRASH` masks the candidate entirely. The 186/129 split is mostly
ordering, not evidence that the reference is worse.

The mechanism, from a `BIPED_RING_TRACE=1` trace of `FUN_0003a810`
(`init_cb = *(void(**)(int))(type_def + 0x10); if (init_cb) init_cb(...)`):
a stubbed `datum_get` returns 0, so the chain reads a **small non-zero**
value out of synthetic state, that value passes the `!= NULL` check, and the
indirect call lands in a page auto-mapped for data. Those pages are
zero-filled — and `00 00` decodes as `add [eax], al`, a two-byte instruction
touching neither ESP nor control flow. So execution **slid through the whole
page**, two bytes at a time, and only stopped on walking off the end,
reporting `FETCH_UNMAPPED` at an address unrelated to the actual call. The
ring trace is unmistakable: `0xff27, 0xff29, 0xff2b, …` with ESP constant.

`hook_mem_unmapped` carried a comment claiming it filled with `0xCC` (INT3)
"so that accidental code execution on data pages stops immediately" — while
the code wrote zeros. Both halves were wrong: these are *data* pages, so a
non-zero fill is read back as pointers, counts and sizes, and
`hook_interrupt` swallows INT3 anyway (it exists for MSVC retail assert
halts), which would only turn a two-byte slide into a one-byte one.

The fix guards the **execution** instead, in `hook_code`: if the PC is inside
a page in `_mapped_regions` and is not a stub sentinel, recover exactly as
`hook_fetch_unmapped` does for a still-unmapped target — `EAX=0`, pop the
return address, continue — bounded at 64 recoveries per run. Applied
identically to both sides, so a candidate that computes a *different* call
target surfaces in the differential instead of both sides crashing.

Measured A/B via `HALO_NO_DATA_EXEC_GUARD=1` (a flag, not a source swap):

| Sample | Result |
|---|---|
| 26 `FETCH_UNMAPPED` error targets | **3 error→pass** (0%→60%, 0%→100%, 0%→100%), **2 error→fail** (divergences previously hidden behind the crash), 21 unchanged |
| 26 previously-passing targets | **0 regressions**, 0 coverage changes |

19% of the sampled class gained a verdict — worthwhile, not a silver bullet.
Three tripwires (`FUN_000d7cd0`, `hs_runtime_get_executing_thread_name`,
`game_state_memory_pool_new`) are in `regression_targets.json`; each returns
to all-seeds-error with the guard disabled, so they cannot pass vacuously.

The 21 unchanged targets are a different root cause and remain open — e.g.
`actor_died`, `actor_erase`, `actor_delete_props` still sit at 0%.

### What does NOT lift the remaining tail (measured 2026-07-28)

260 of 822 cached targets still sit below 30% coverage (154 of them
`non_leaf`). Three plausible generic levers were measured against that
population; **none of them is the lever**, so do not spend another cycle
assuming one is.

| Candidate lever | How measured | Result |
|---|---|---|
| Better **callee return values** (`stub_return_overrides` / accessor arena) | 11 non-leaf low-coverage targets, `--rich-stub-returns` on vs off | **0 improved**, coverage identical to 0.1pp |
| Same, with sub-emulation disabled so the return table is actually consulted | 6 targets × {default, `--no-real-callees`, `--no-real-callees --rich-stub-returns`} | **0 improved** |
| **Real captured state** on its own | `FUN_00130270`, `FUN_00130580` with `host_snapshots/network_server_mp.json`, the frame that covers them | 10.0% → 10.0%, 14.3% → 14.3% — **no change** |
| **Argument-consistency** (satisfying the relational entry gate) | same two, plus `size`/`buffer` pinned so `(uint16)size == msg[0] >> 4` | 10.0% → **16.1%**; 14.3% → **11.2%** (down — a different category path) |

Why the return-value lever is dead: `--real-callees` (default since
2026-07-28) already sub-emulates those accessors' real bodies — 18 to 96 real
callees load per target — so the return table is bypassed. And on these
functions execution never reaches an accessor anyway; it dies at the entry
gate.

The gates that actually block are **conjunctions**, and each conjunct needs a
different fix:

1. *Argument-range* — `hs_evaluate_inequality` asserts
   `function_index >= 0xf && function_index <= 0x12` (4 values in 2^16, so
   random seeding never passes).
2. *Argument-to-memory relational* — the network handlers assert
   `(uint16)datagram_size == GET_MESSAGE_SIZE(*message)`, coupling a scalar
   argument to memory reached through another argument.
3. *Chained pool state* — past the gate,
   `datum_get(*(data_t **)0x5aa6c4, …) → thread+0x10 → *+0x4 → …` walks a
   pointer chain that zero-fill and scratch pages cannot satisfy.

These compound: pinning `hs_evaluate_inequality`'s `function_index` into its
valid range moved coverage 9.8% → **0.0%** and 8 passes → 8 errors, because
clearing gate 1 just runs it into gate 3. Satisfying one constraint in
isolation can *lower* coverage and *lose* passing seeds.

`z3_seeds.extract_branch_seeds` is the component nominally responsible for
gate 1, and its own docstring names the limitation: it "creates a seed vector
with that value for **every** parameter … a coarse approximation". It has no
notion of *which* argument a solved constant constrains. `concolic_z3.py` is
separately barred from gates 1 and 2 by design — it requires a model in which
some observed **global** differs, precisely because it assumes the caller
cannot choose incoming argument values (it can: see `arg_overrides`).

**Practical consequence.** The remaining tail is not reachable by a generic
mechanism; it needs per-function authored setup — one snapshot with
`arg_overrides` per function, which is exactly what
`host_snapshots/manifest.json` already does for the actor targets. Budget
that per function, and prefer targets where a real frame already exists.
Whoever revisits this should re-measure rather than trusting the table above,
using `--no-leaf-cache`.

### Worked example: authoring past a relational gate

`network_server_mp_gameupdate.json` is the pattern to copy. Read the gate out
of the C, solve it by hand, and pin the result in the manifest's `args`:

- `FUN_00130270` asserts `(uint16)datagram_size == GET_MESSAGE_SIZE(*message)`,
  i.e. `size == msg[0] >> 4`. So `msg[0] ∈ [size<<4, (size<<4)|15]` — the low
  four bits are free.
- Those free bits pick the path: `msg[0] & 3` must be 0 for valid flags, and
  `(msg[0] >> 2) & 3` is the category. Only **category 3** reaches the packet
  switch, so the low nibble must be `0xc`.
- Inside category 3, `packet_type = *((char *)msg + size - 1)` — the **last**
  byte of the header selects the handler. `0x19`
  (`message_client_game_update`) is the deepest chain.

`size = 16`, `buffer = "0c01" + 00×13 + "19"` follows from those three facts
and lifts coverage 10.0% → **24.4%**, 40/40 pass.

Two things this example establishes:

- **Keep the un-pinned entry too.** Pinning a valid header means the assert
  path is no longer exercised, so the two snapshots cover *different* code.
  `regression_targets.json` carries both, told apart by the optional `label`
  field (display only — `name` is still the symbol unicorn_diff resolves).
- **Do not share one pin across sibling handlers.** The identical pin *lowers*
  `FUN_00130580` (14.3% → 11.2%) because that function dispatches on category
  differently. One snapshot per function, not per object.

`build_host_snapshots.py` parses manifest `args` with
`halorec_to_snapshot._parse_arg_value`, so a value is an int when it parses as
one (`"0x5a90e0"`) and otherwise passes through verbatim — which is what lets
`buffer` carry a hex blob. JSON numbers and `[lo, hi]` scalar ranges also pass
through untouched.

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

### Call-sequence divergences have three shapes (2026-07-29)

`call-seq diverged at index N` was the single largest failure class — 71 of 186
on the 07-29 batch, all filed `suspect-real` — and it could not be adjudicated
at all, because the harness recorded only the index and never the two sequences.
`stubs.py::StubArgDiff` now keeps both and `sequence_relation()` classifies the
shape:

| Shape | Meaning | Bucket |
|-------|---------|--------|
| `truncated` | Shorter sequence is a **prefix** of the longer: one side stopped early (oracle crash, early return, insn limit). Both sides agree on every call they both made. | harness-artifact |
| `shifted` | One extra call in the **middle**, tail lines up. The comparison then walks two lists off by one, so every later position reports divergent. | harness-artifact |
| `divergent` | Genuinely different callee at the same position. | suspect-real |

Measured across all 71: **57 truncated, 4 shifted, 3 divergent** (7 no longer
reproduced). That is 95% artifact, and it moved the corpus from 108 suspect-real
to 40.

Two properties are pinned by test in `test_stub_arg_trace.py` and
`test_triage_classify.py`, because both failure modes are silent:

- A genuinely different callee must **not** be explained away as a shift —
  otherwise the alignment story launders real control-flow bugs.
- A smoke log with **no shape marker** (written before this change) keeps the
  old `suspect-real` verdict. Absent evidence is not evidence of an artifact;
  defaulting old logs to `harness-artifact` would retire real bugs unexamined.

Re-run a target to get the marker: the shape is written to the smoke log by
`unicorn_diff.py`, capped at the first two diverging seeds (the sequences were
identical across seeds in every case observed).

#### The three `divergent` ones were also artifacts: calls attributed to the wrong function (2026-07-29)

Investigating the three `divergent` survivors showed the comparator was counting
calls the target never made. The two sides do not execute the same amount of
code: the oracle CALLs an intra-object sibling and gets a **stub**, while the
candidate either loads that sibling's real body or has **inlined** it. Every call
the sibling then makes was recorded as if the target had made it — so the
sequences differed while the target itself did nothing different.

`StubCallRecord.caller_addr` now records the return address captured at intercept
time (`[ESP]` at the sentinel, i.e. `call_site + 5`), and `StubArgTracer`
carries the target's byte extent for its own side. `compare_stub_arg_traces`
drops records whose caller falls outside that extent: those calls belong to the
sibling's own equivalence target, not this one. The drop count is reported as
`call-seq NESTED-DROPPED oracle=N candidate=M`, never silently swallowed — a
large asymmetry there is exactly the context a reader of the divergence needs.

Result on the three: `FUN_0005a120` went clean on 45 of 46 seeds; the other two
stopped being `divergent` and now self-classify as `truncated` / `shifted`.
**The class contains no genuine control-flow divergences.**

Attribution cannot see through **inlining** — when clang inlines the sibling,
its calls issue from inside the target's own extent (`FUN_000d6cc0` is 1152
candidate bytes against 400 oracle bytes, and its extra `_display_assert` at
`+0x447` is `hud_get_nav_point_data`'s NULL-guard, line `0x60`, firing on
un-seeded harness memory). Those land in the existing `truncated`/`shifted`
buckets, which is the honest place for them.

Three properties pinned by test, each a silent failure mode:

- An extra call made from **inside** the target's extent still fails. Attribution
  must not become a blanket excuse for extra calls.
- `caller_addr == 0` means *unknown*, not *nested* — same principle as the
  missing shape marker above.
- A `CALL` as the function's last instruction leaves a return address equal to
  `end`, so the upper bound is **inclusive**. An exclusive bound would attribute
  the target's own final call to a callee and hide a dropped tail call.

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

### `arg_mismatch` triage, worked through (2026-07-28)

`arg_mismatch` is the ledger's highest-value class — a wrong argument to a
named callee is the §10 signature bug. All 50 entries were classified by the
oracle/candidate value *pair*, which turns out to separate cause cleanly:

| Bucket | n | Cause |
|--------|---|-------|
| oracle in globals arena, candidate `0x0` | 7 | **Harness**: `__imp__` indirection (below) |
| oracle arena slot, candidate real XBE VA | 2 | **Harness**: one side reloc'd, the other hardcodes the absolute |
| both synthetic, different slot | 3 | **Harness**: separate oracle/candidate slot arenas |
| reversed (oracle `0x0`, candidate slot) | 5 | **Harness**: same, other direction |
| differing constant | 24 | mostly **stale** (see below) |
| sign/zero-extension of a 16-bit value | 2 | **REAL** — both fixed here |
| other | 7 | mixed |

Two lessons outweigh the counts:

**1. Most entries no longer reproduced.** `evidence_stale` compares dates at
*day* granularity, so a same-day harness change (here the `--real-callees`
default flip) silently invalidates evidence while the flag still reads
`false`. Re-measure before acting: of a 10-target sample, 7 were already clean.
**Re-measure at the ledger's seed count** — at 4 seeds 7 looked clean, but at
50 two of them still diverged. Passing at low coverage (several were 5–18%) is
not proof of correctness, only that the recorded evidence no longer reproduces.

**2. Roughly 20 entries were ONE comparator bug, not 20 bugs.** Two clusters —
15 × `object_get_and_verify_type` at `arg[1]: oracle=0xffffffff candidate=0x0`
and 5 × `tag_get` at `arg[0]: oracle=0x6f626a65 candidate=0x0` — looked like a
systematically dropped constant, but every call site in `objects.c` passes the
right value. The cause was in `compare_stub_arg_traces`, and the sequence dump
shows it plainly:

```
oracle    = [tag_get('obje',0), tag_get('mode',0)]
candidate = [datum_get(0,0),   tag_get('obje',0), tag_get('mode',0)]
```

`objects.c` *defines* `datum_get`'s neighbours, so an intra-object sibling
resolves internally on the oracle side (which maps its whole `.text`) while the
candidate must route the same call through a sentinel. The candidate's sequence
is therefore the oracle's **shifted by one** — it makes exactly the same two
`tag_get` calls — but the comparator paired the lists positionally and compared
`tag_get` against `datum_get`, reporting a dropped `'obje'` tag-group literal
that does not exist.

Two things were wrong, both now fixed:

- The per-arg loop ran over `zip(oc, cc)` **unbounded**, despite the comment
  above it saying "over matching prefix". Past the divergence the two lists
  no longer describe the same calls. It now truncates at the divergence index.
- The callee-identity scan was skipped whenever the lengths already differed,
  leaving `seq_diverge_idx = min(len)` — 2 in the example above — so even a
  correct truncation would still have arg-compared the misaligned pairs 0 and 1.
  The scan now always runs, giving the true first disagreement (index 0 here).

The seed still **fails** on the sequence divergence itself, which is honest; it
just no longer manufactures argument evidence against innocent code. Real arg
bugs inside the matching prefix are unaffected —
`test_real_arg_bug_before_divergence_still_caught` pins that boundary, and
`test_swapped_args` still passes.

**Known limitation.** Truncating means a genuine wrong-argument bug *after* an
artifact divergence is no longer reported. That evidence was never sound (it
came from comparing misaligned entries), so nothing trustworthy was lost — but
it is a coverage gap, not a clean win. Closing it properly needs sequence
*alignment* (LCS over callee identity, skipping the one-sided calls) instead of
truncation, so the tail can be compared against its true counterpart. Until
then, the sequence divergence is the finding to chase first; fix that and the
arg comparison over the whole sequence comes back for free.

### Resolving oracle globals from the XBE instead of by name (2026-07-29)

Three of the categories in the table above — `oracle arena slot / candidate
real VA`, its reverse, and part of `differing constant` — turned out to share
one cause, and it was not in any lift.

A delinked reference's data labels come from **Ghidra**, and Ghidra's name for
an address need not be kb.json's. 0x5aa8b8 is the decal datum-pool pointer:
Ghidra calls it `g_decals_data`, kb.json declares it `global_decal_data`.
Neither normalizes to the other, so `_build_globals_seeds` resolved nothing,
the oracle's slot for it stayed zero-filled, and the oracle called
`datum_get(0, idx)` while the candidate — which reaches the same global through
an absolute immediate that the emulator's flat memory *does* seed — passed
`0x700700`. All 50 seeds of `FUN_0015b0c0` failed on what read as a dropped
argument.

Name matching was never the right tool. Relocations are function-relative, so
`func_va + reloc.virtual_address` points at exactly the dword the original
linker wrote, and reading it out of the pristine XBE resolves **any** label:

```
+0x46  g_decals_data   -> 0x5aa8b8     (name matching: unresolved)
+0x5c  DAT_0032516c    -> 0x32516c     (name matching: correct)
```

`_xbe_dir32_symbol_addrs()` does this for the target's own relocs and
`_xbe_addrs_at_sites()` for real-callee slots; both feed `sym_addr_hints`,
which outranks every name heuristic — including a bare-name match landing on a
*different* kb global, the hazard `_GLOBAL_NAME_ALIASES` exists to paper over.
A symbol whose sites disagree is dropped rather than guessed.

**The comparator needed the same map.** `&some_global` is a DIR32 reloc in the
oracle, so it pushes a *slot* address, while the candidate pushes the global's
*real* address. Both are correct; numerically they never match. `input_flush`
reported 150 arg mismatches across 50 seeds for exactly this
(`0x500000` vs `0x46ba4c`, `0x500100` vs `0x46bb38`, `0x500200` vs `0x46bba0`).

The excusal is on the **displacement**, not on a range: a pair is soft-matched
only when `o_val - c_val` equals `slot - real_address` for a global this
function actually relocates. That keeps it sound — both sides must have applied
the same offset to the same object, so a wrong index is still reported — and it
covers indices that leave the slot's 256-byte window. They routinely do, in
both directions: `game_engine_clear_goal_position` computes
`0x4566f8 + (short)index * 0x20`, so negative indices land *below*
`GLOBALS_BASE`, and a first cut that range-checked the arena excused only 5 of
41 seeds. `test_wrong_offset_into_right_global_is_still_reported` and
`test_unmapped_slot_is_still_reported` pin the soundness boundary.

Census over all 50 `arg_mismatch` entries at 50 seeds: **47 report zero arg
mismatches**, up from 42 before this change (33 when the triage started). The
equivalence regression gate stays at 69 passed / 0 failed / 0 errors.

Measured effect, at the ledger's 50 seeds:

| target | before | after |
|---|---|---|
| `FUN_0015b0c0` | 6/50 | **50/50** |
| `input_flush` | 0/50 | **50/50** (150 aliases) |
| `game_engine_clear_goal_position` | 8/50 | **49/50** (41 aliases) |
| `FUN_001a7ea0` | 4/50 | **50/50** |
| `FUN_00019110` | 0 passing | arg mismatches → 0 (fails for another reason) |

**Unproven part, stated plainly.** Extending the hints to real-callee slots is
live and correct (7/7 sites resolved on `FUN_0008c030`) but has **not** been
observed to change any result: every real-callee global in the sampled targets
is spelled `DAT_<hex>`, which name matching already resolved correctly. It is
kept because it removes the same latent failure mode from that path, and it is
unit-tested rather than left to chance — not because it fixed anything
measurable.

**Still open** — the 3 entries that are not arg-clean, with causes identified
but not fixed:

- `FUN_0008c030` — oracle passes `0x0` where the candidate passes scratch
  pointers (`0x10000800`, `0x10000c00`); runs under `--real-callees`.
- `FUN_000142a0` — candidate reads `0xcccccccc`, the concolic phase's data-page
  fill, through a data ref the oracle reaches via an un-injected slot. Same
  address-space asymmetry, but on *injected* values rather than passed pointers.
- `debug_keys_initialize` — no result within 600 s.

`FUN_0005ff70` (scratch-pointer layout) and `FUN_0011be10` (errors on every
seed, task #21's class) came back arg-clean in the census and are no longer
`arg_mismatch` findings, though `FUN_0011be10` still errors.

#### The two real bugs (both VC71-blind)

Both were sign/zero-extension of a 16-bit value, in **opposite** directions,
and VC71 scored *identically* before and after each fix (78.4% and 71.8%) —
one instruction in ~90 gets aligned away by the LCS. Only the stub-arg
differential saw them.

- `FUN_0005ae70` (encounters): the squad index is the handle's low word
  **zero**-extended — the original is a plain `AND ESI,0xffff; PUSH ESI` — but
  the lift had an `(int16_t)` cast that sign-extended. 32/50 → 50/50.
- `FUN_0009fd30` (particle_systems): `type_index` is a **signed** 16-bit
  param — the original loads it once with `MOVSWL 0x8(%ebp),%ECX` and feeds
  that to both the `* 0x40` stride (`SHL $0x6`) and the tag-block index — but
  the lift declared it `int`, dropping the sign-extension. Fixed by typing the
  parameter `int16_t` in both the source and the kb.json decl.

Both only diverge for indices ≥ 0x8000, which real data never reaches: these
are latent fidelity bugs, not live ones.

**Detector gap:** `[LOADW-WARN]` (§24) did *not* catch either, though it fired
on other functions in the same file. It models narrowing on *memory field
loads* (`movswl (%ebx)`); here the narrowing applies to a value already in a
register — a parameter or a computed index — which that check structurally
cannot see.

#### `__imp__` indirection (`_seed_dllimport_indirection`)

kb.json globals are declared `HDATA` = `__declspec(dllimport)`, so the clang
candidate reaches one through a **pointer-to-pointer**:

```asm
mov eax,[__imp__event_manager_globals]   ; the slot holds &global
push eax                                  ; ... the global's address
```

The delinked MSVC oracle has no import table and references the same storage
directly as `DAT_0046bd40`. `patch_dir32_relocs` gives each a globals slot —
but for the direct side the slot address *is* the pointer, while the indirect
side's slot must *contain* it. That slot was only seeded when a snapshot or
`_KNOWN_GLOBAL_BYTES` entry existed; with neither it stayed zero and the
candidate passed `NULL`. Both sides then wrote to different pages, so the
memory-trace compare was meaningless rather than merely imprecise.

`_seed_dllimport_indirection` points each `__imp_X` slot at the *other* side's
direct slot for X (falling back to its own), so both agree on the pointer and
share one page. It runs **before** `_build_globals_seeds` so real snapshot data
still wins. When both sides go through `__imp_X` it emits nothing — they
already agree, both dereferencing the same zero.

Self-test: `python3 tools/equivalence/test_dllimport_indirection.py`. Note the
first cut resolved only friendly names and so never matched the oracle's
`DAT_<addr>` spelling — it produced an empty seed map and *zero* behavioural
change; `test_dat_spelling_is_required` pins that.

## Integration Points

| Consumer | What it uses | How |
|----------|-------------|-----|
| `tools/lift_pipeline.py` | `--output-json` | Gates equivalence stage; shows confidence in details |
| `tools/equivalence/batch_verify.py` | CLI invocation | Passes `--mem-trace` by default |
| `tools/llm_auto_lift.py` | `leaf_cache.json` | `+3 eq_high_conf` for high-confidence entries |
| `/verify equivalence` skill | CLI invocation | Delegates to unicorn_diff |

### Who tests the harness (2026-07-29)

The harness verifies lifts; two gates now verify the harness. Both were added
after noticing that the XBE-global fix above shipped with its soundness pins
running nowhere.

| Gate | Fires on | Runs |
|------|----------|------|
| `audit.yml` → *Equivalence harness self-tests* | nightly 03:00 UTC; PR touching `tools/equivalence/**` | `run_all_tests.py` (~5s) |
| `pre-commit-regression-test.sh` | staged `src/*.c`, `src/*.h`, `kb.json`, **`tools/equivalence/*.py`** | `run_all_tests.py`, then `regression_test.py --quick` (~130s) |

Two things this had to get right, both of which a naive version gets wrong:

**`unittest discover` is the wrong runner here.** Only 6 of the 12 suites are
`unittest.TestCase`-based; the other 6 are plain `sys.exit(main())` scripts,
and discovery finds *zero* tests in them. `test_stub_arg_trace.py` — which
holds the pins that a wrong offset into the right global must still be
REPORTED, and that the slot alias is inert without a map — is in the second
group. A discovery step would have reported 49 tests passing while running
none of the ones that matter most. `run_all_tests.py` executes each file as a
subprocess and trusts its exit code, so both shapes count.

**The pre-commit filter used to exclude the harness.** It gated on
`src/*.c`, `src/*.h`, `kb.json` only, so a commit touching nothing but
`tools/equivalence/**` — exactly the change that can break the differential —
skipped the hook and offered no opinion at all.

`run_all_tests.py` also refuses two ways of passing vacuously: a suite with no
`__main__` block is an ERROR (it would exit 0 having run nothing), and a suite
that self-skips for a missing optional dep is a SKIP, surfaced in the CI
summary as a coverage hole rather than folded into the pass count. Without the
venv (`z3` absent) the tally is 10 passed / 2 skipped, which is why `audit.yml`
prefers `VENV_PYTHON` and falls back to `python3`.

Not covered by either gate: `batch_verify.py`'s nightly behaviour itself, and
the fact that `batch_verify_baseline.json` still lists as failing the targets
the XBE fix repaired. That direction is safe — `--fail-on-new` only fails on
divergences *absent* from the baseline, so a fixed target simply stops
appearing — but those entries now over-forgive, and should be refreshed with
`freeze_batch_baseline.py` after the first post-fix nightly (not before, so the
refresh rests on measured results).

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
  run_all_tests.py      — Runs all 12 test_*.py suites (both styles); CI gate
```
