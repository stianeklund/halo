# VC71 Dashboard Scoring and Match Statistics

## Objective

Make VC71 match statistics complete, current, and safe across dashboard, CLI,
CI, and commit workflows.

## Status

Implemented. The four defects below are fixed and covered by tests; the
remaining items are listed under [Not done](#not-done).

## The data model (unchanged)

Two score files, two jobs:

- `tools/verify/vc71_scores.json` — **the floor.** Committed, raise-only,
  always complete. The commit hooks' regression tripwire. Never lowered except
  by a deliberate `populate --rebaseline`.
- `tools/verify/vc71_current.json` — **the honest snapshot.** Gitignored,
  present truth, only as complete as its last `populate` pass. May go down: a
  real regression has to be visible.

`tools/verify/reference_validity.json` is the third output — the attention
queue, carrying a per-function `state` (`compile_failed`, `no_reference`) for
everything that could not be scored.

## What was wrong, and what fixed it

### 1. Dashboard scores were erased by the next regeneration

`POST /api/score` called `run_vc71_verify` directly and wrote **only** the
floor. `generate_decomp_report.py` reads the **current** snapshot. So a button
score rendered immediately, then vanished when the five-minute background
regeneration rewrote `report.json`.

The handler also carried its own hand-rolled copy of the raise-only floor
policy, making it a second writer of a file with delicate semantics. The copies
had already drifted once — see the `make_score_entry` docstring: provenance
fields were being stripped on every dashboard refresh.

**Fix:** `_run_score` now calls `vc71_regression.cmd_populate` scoped with
`--source <the unit's TU>`. That writes floor + current + attention queue
through the one set of writers. There is no scoring or persistence policy left
in `progress_server.py`; it reads the numbers back via `load_current()` and
mirrors them into the live `report.json` so the open dashboard updates without
waiting for a regeneration.

`populate --source` **merges** (`_merged_current` / `_merged_validity`), so a
one-TU refresh adds to the snapshot instead of truncating it to one TU.

Two behaviours preserved deliberately:

- The floor is still written, still raise-only — same policy as the pre-commit
  gate. `--rebaseline` is never passed from a web endpoint; it REPLACES floors
  and can lower them.
- The old handler's per-function retry survives as
  `populate --per-function-fallback` (see §3).

### 2. A partial current snapshot shadowed the entire floor

The generator picked one file wholesale: prefer current, fall back to floor only
when current was **entirely** empty. Any scoped or sharded populate leaves a
current file holding a fraction of the tree, and every function outside it
rendered a bare `—` despite a good floor score. The all-empty case was already
known (gh-pages 2026-07-09: 3474 functions blank while the floor held 3689 real
scores); the partial case is the same bug one step short of total.

**Fix:** `merge_vc71_scores()` layers current over floor **per function**.
Current wins where it measured; the floor covers everything else. The generator
prints the split (`VC71 scores: N current + M from the committed floor`) so a
mixed dashboard is not mistaken for one coherent measurement. Measured effect on
the live data: 213 → 182 functions that exist only in the floor now render.

### 3. kb-only TUs were invisible to routine scoring

Discovery breadth was bolted to floor semantics:

```python
tus = _discover_scoreable_tus(include_kb_only=rebaseline)   # was
```

Every reference is now derived from the pristine XBE, so a TU without an
`objdiff.json` unit is perfectly scoreable — but the only flag that would
*discover* it was also the flag that REPLACES (and can lower) every floor it
touches. CI runs bare `populate`, so CI was structurally blind to those TUs.

**Fix:** the two are separate knobs. `populate` discovers kb-only TUs **by
default**; `--no-kb-only` restricts to `objdiff.json` units. `--rebaseline` now
only means "REPLACE rather than raise-only".

### 4. Unauthenticated compile endpoint on every interface

`progress_server.py` defaulted to `--host 0.0.0.0`, and `/api/score` has no
authentication while it compiles source and rewrites score files.

**Fix:** default bind is `127.0.0.1`. Exposing it is now an explicit
`--host` — put access control in front of it first.

## Supporting changes

- `save_baseline` / `save_current` / `save_validity` write through
  `_atomic_write_json` (temp file + rename), which already existed but was used
  only by the caches. Several agents run gates concurrently in this checkout; a
  torn write leaves JSON every later reader silently discards.
- New `load_current()`, the counterpart to `load_baseline()`.
- New `_result_key()`: the one place that answers "does this result dict already
  hold a score for this function", across plain name, `FUN_<addr>` aliases, and
  namespace-qualified keys. The report generator, the dashboard, and the
  per-function retry all need the same answer.
- `_measure_source(src, per_function_fallback=False)` retries each ported
  function the whole-TU run left unscored, on its own, and clears its attention-
  queue entry if the retry scores it. Off by default (one extra compile per
  missing function — a whole-tree CI pass cannot afford it); on for the
  dashboard button, which is one interactive unit.
- A scoped populate now prints both numbers (`N measured this pass, M total`).
  Printing only the slice made a one-TU refresh look like it had shrunk the
  snapshot to 31 functions.
- Dead code removed from `progress_server.py`: `_is_synthesizable`,
  `_score_for_result`, `_has_function_ref`, `same_source_refs`. Reference
  selection is `vc71_verify`'s job, resolved per function.

## Tests

Neither suite ran anywhere in CI before, which is how a score you could watch
appear and then vanish kept every gate green. Both are now wired into
`audit.yml` as *Score-persistence self-tests*.

- `tools/verify/test_vc71_regression.py` (63 tests) — added
  `TestCurrentSnapshot` (load/save roundtrip; a scoped pass merges rather than
  truncates), `TestResultKeyJoin` (all four alias forms), `TestPerFunctionFallback`
  (off = drop stays flagged; on = recovered and dequeued; an already-scored
  function is not retried), `TestDiscoveryBreadth` (routine populate sees
  kb-only TUs; `--rebaseline` alone does not widen discovery).
- `tools/report/test_vc71_score_merge.py` (11 tests) — merge semantics,
  including that current may *lower* a score, that a partial current does not
  shadow the floor, and that neither input is mutated.

## Manual verification performed

1. `populate --source src/halo/bitmaps/libtiff/tif_flush.c --per-function-fallback`
   → 31 functions measured; snapshot 5809 → 5840 (merge preserved the rest);
   floor unchanged (`git diff` clean on `vc71_scores.json`).
2. Server bound to `127.0.0.1:8099` (confirmed via `ss -ltn`).
3. `POST /api/score {"unit":"tif_flush"}` → HTTP 200, 31 scores.
4. Full report regeneration afterwards → tif_flush still shows 31/55 functions
   with a `match_percent`. **This is the original bug, confirmed fixed.**

## Not done

- **No lock around concurrent populates.** Atomic writes prevent a torn file,
  but `_merged_current` is read-modify-write: two simultaneous scoped passes can
  still lose one of their slices. Not observed, and single-user local usage
  makes it unlikely — a single lockfile around `cmd_populate` is the fix if it
  ever bites.
- **No authentication on `/api/score`.** Loopback-only is the mitigation, not a
  solution. Anything that exposes this server needs auth first.
- **Coverage status is not rendered.** `reference_validity.json` already
  distinguishes `compile_failed` from `no_reference` per function; the dashboard
  still shows only a blank match for both. Purely a rendering gap.
- **Snapshot scope/timestamp is not published.** The generator prints the
  current-vs-floor split to stderr; it is not in `report.json`, so the HTML
  cannot show how much of what it displays was measured today.
