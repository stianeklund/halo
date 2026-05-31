# Plan: bulk real-callee + live-snapshot equivalence for the stubbable backlog

Status: **proposed** (2026-05-30). Owner: equivalence tooling.
Prereqs already landed: `memsave_snapshot.py` (virtual capture), double-width
global seeding, `unicorn_diff.py --real-callees`, the local cron lanes
(`run_local_equiv.sh fast|full`).

## Problem

~200 ported "stubbable" (non-leaf) functions sit at low Unicorn coverage and
`weak`/`moderate` confidence. The dominant limiter is **not** missing data — it
is that `unicorn_diff` stubs every non-math callee to `return 0`
(`stubs.py:720`). Iterator/helper loops therefore never execute:

```c
data_iterator_new(&iter, player_data);
while (data_iterator_next(&iter) != NULL)   // stub returns NULL -> body never runs
    count++;
```

Two levers fix this and are now built:
1. **`--real-callees`** — load each callee's delinked oracle code and run it
   natively in place (recursive, raw-call + intra-obj redirection, FUN_/named
   alias normalization). The loop then iterates for real.
2. **live snapshots** — the iterator needs a real `data_array` to walk;
   `memsave_snapshot.py` captures the guest's virtual memory (byte-exact, no
   physical translation) and `--state-snapshot` replays it deterministically.

Proven on `game_engine_player_count`: 73.4% weak -> **100% high, 20/20 PASS**,
iterator counts the real players on both oracle and lifted sides.

**The remaining work is orchestration at scale**: hundreds of functions, each
needing a snapshot that contains the live globals/heap *it* reads, applied
inside `batch_verify` without blowing up runtime or generating assert noise.

## Key insight: shared snapshots, not per-function

The naive model ("one snapshot per function") does not scale — capture needs a
live match and there are hundreds of targets. But the stubbable functions
**read a small, highly-overlapping set of roots**: `current_game_engine`
(0x456b60), `player_data` (0x5aa6d4), the object/tag data_arrays, scenario
globals. A handful of **broad virtual snapshots — one per representative game
state** — covers the great majority. Resolution becomes "pick the snapshot that
has live data for this function's referenced globals," not "capture a new one."

## Architecture

### 1. Snapshot library (committed fixtures)
`tools/equivalence/state_snapshots/<state>.json`, a few canonical states:
- `mp_bloodgulch.json` — 16-player-capable MP match (players, vehicles, objects).
- `sp_midlevel.json` — single-player mid-level (AI actors, encounters live).
- `menu.json` — shell/UI globals (for menu/HUD functions).

Each is a **broad** virtual snapshot: all non-zero image-global pages plus heap
regions reachable by following the known root pointers (game_engine, player_data,
object table, tag cache) to a bounded depth. Built once locally from a live xemu;
committed (tracked, like `regression_snapshots/`). Size target < ~2 MB each by
capturing reachable regions, not a full 64 MB dump.

New `memsave_snapshot.py capture-broad --state <name>` subcommand:
- memsave all image-global pages (reloc-target union across kb, deduped).
- follow root pointers (config list of `(global_addr -> struct size, follow)`),
  one or two hops, into the heap; memsave each reached region.
- pause VM during capture for consistency; resume after.

### 2. Target -> snapshot resolution (reloc-driven)
For each candidate, reuse the discovery already in `memsave_snapshot.discover_regions`:
parse its DIR32 targets, then **score each library snapshot** by how many of
those targets it covers with *non-zero* (live) data. Pick the best-covering
snapshot; if none covers a meaningful fraction, skip real-callee mode for that
target (fall back to trampolines — no regression). Cache the chosen mapping in
`tools/equivalence/snapshot_assignments.json` so reruns are cheap.

### 3. batch_verify integration
- Add `--real-callees` passthrough (flag -> `run_verify` -> `unicorn_diff` cmd).
- Add `--snapshot-dir tools/equivalence/state_snapshots` and per-target snapshot
  selection (from the assignment cache, computed on first run).
- **Gate it**: only enable `--real-callees`+snapshot for targets that are
  (a) non-leaf/stubbable AND (b) currently `weak`/`moderate` confidence or
  coverage < 60%. Leaves and already-high-confidence targets keep the cheap path.
- Per-target timeout already exists; real-callee runs are heavier, so bump the
  per-target budget for gated targets only.

### 4. Measurement (prove the win, catch regressions)
- `run_verify` records coverage% and confidence to `leaf_cache.json` already.
  Add a **coverage delta** column to the batch summary: confidence/coverage with
  vs. without real-callees, so the sweep quantifies how many functions moved
  weak -> high.
- Feed the `equiv_status`/coverage into the dashboard's Verified column (it
  already reads batch_verify result JSONs) so the bulk win is visible.

## Risks & mitigations
- **Assert paths / non-termination.** A real callee can hit a `display_assert`
  or loop if an invariant the snapshot doesn't satisfy fails. Mitigation: the
  harness already treats crash/INSN-LIMIT as *error → skip seed* (not a false
  divergence); keep `--max-insn` bounded; intrinsics (`display_assert`,
  `halt_and_catch_fire`, memcpy, math) stay Python intercepts. Errors are noise,
  not wrong verdicts.
- **Heap-address volatility.** Heap pointers differ between captures. A snapshot
  is self-consistent (pointer global + pointed-to region captured together), so
  replay is deterministic regardless of where the heap landed. Never mix regions
  from two captures.
- **Snapshot staleness vs. code.** Snapshots encode *data layout*, not code; a
  lift change doesn't invalidate them. Recapture only if struct layouts change
  (rare) — document the capture procedure so it's reproducible.
- **Runtime blow-up.** Gating (only low-confidence stubbables) and the broad-vs-
  full snapshot size cap keep the nightly sweep within budget. Real-callee recursion
  is bounded (`MAX_RECURSION_DEPTH`, `MAX_REAL_CALLEES`).
- **False agreement.** Both sides run the *same* oracle callee code, so a bug in
  a callee can't be caught by running it on both sides — but the TARGET's own
  lifted code is still differentially tested; real callees only restore realistic
  inputs/side-effects. Document this clearly (real-callees raises coverage of the
  target, it does not verify the callees).

## Phased rollout
- **Phase 1 (done / in progress):** per-function fixtures for 2 proven targets
  in the fast cron lane. Validates the mechanism end-to-end.
- **Phase 2:** capture the 2–3 broad library snapshots; build `capture-broad`
  and the reloc-driven assignment resolver; wire `--real-callees`+`--snapshot-dir`
  into `batch_verify` behind the confidence gate; run a scoped sweep over the
  ~200 stubbables.
- **Phase 3:** persist coverage deltas, surface in the dashboard, fold the gated
  real-callee path into the nightly `full` cron lane. Expand library states as
  gaps appear (functions whose globals no snapshot covers).

## Acceptance criteria
- A nightly `full` run re-scores the stubbable backlog with real-callees+snapshots
  for gated targets, with no increase in *false* divergences (errors are allowed).
- A measurable count of functions move `weak`/`moderate` -> `high` confidence,
  reported in the batch summary and dashboard.
- The path is deterministic and headless (replays committed fixtures); capture is
  a documented, occasional manual step.

## Open questions
- Snapshot capture cadence: how often do struct layouts change enough to require
  recapture? (Expectation: rarely.)
- Should `capture-broad` follow object/tag tables transitively (deeper heap) for
  rendering/object functions, or is one hop enough? Decide empirically from
  coverage deltas.
- Where to draw the confidence gate (coverage < 60%? weak-only?) to balance
  sweep runtime against coverage gained.

## Pointers
- `tools/equivalence/memsave_snapshot.py` — virtual capture (extend with `capture-broad`).
- `tools/equivalence/unicorn_diff.py` — `--real-callees`, `_build_globals_seeds`.
- `tools/equivalence/stubs.py` — `prepare_stubs(real_callees=...)`, raw-call/alias handling.
- `tools/equivalence/batch_verify.py` — add passthrough + gating + assignment cache.
- `tools/equivalence/regression_targets.json` + `regression_snapshots/` — the Phase-1 pattern.
- Memory: `reference_equiv_oracle_reloc_gap` (root-cause + tooling notes).
