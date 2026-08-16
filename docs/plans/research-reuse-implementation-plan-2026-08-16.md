# Research reuse implementation plan

Date: 2026-08-16

## Summary

Replace per-target Opus research briefs with fingerprinted mechanical evidence
bundles. Share immutable binary-backed artifacts across worktrees, keep
attempt-specific evidence content-addressed, and never use agent prose as an
automated skip or acceptance gate.

Preserve all existing build, ABI, hazard, VC71, review, and reintegration gates.

## Implementation

### 1. Correct state and establish measurements

- Fix `park.py` so `capped_confirmed` is cleared only when a new attempt
  strictly exceeds the previous best score. Capture the old best first.
- Add phase token deltas from `budget.spent()`, cache hit/miss counts, Ghidra
  builds, retrieval cohort, and final outcome to workflow summaries.
- Treat unfingerprinted park notes, contexts, and failure files as hints only.
- Preserve the confirmed-cap pre-screen and unrelated worktree changes.

### 2. Shared fingerprinted evidence store

Use `ledger_root().parent / "research_cache"` and provide:

```text
research_bundle.py prepare --target <addr> --json
research_bundle.py inspect --target <addr> --json
research_bundle.py stats --json
```

The context fingerprint includes the cachebeta XBE MD5, target address and
bounds, function-byte hash, relevant KB entry, callee declarations, schema and
extractor version, and Ghidra-analysis version. The score fingerprint adds the
candidate source, relevant KB declaration, compiler/options and verifier
version, and reference bytes/bounds.

Persist only typed verdicts and content-addressed artifact references. Do not
persist full briefs, inferred argument meanings, neighbor bodies, source-path
guesses, or source-presence claims. Existing caches are legacy misses.

### 3. Simplify goal-lift

- Prepare mechanical bundles before the analyst; the memoryless analyst owns
  unresolved reasoning and implementation in one context.
- Run live source, KB, fingerprint, structural, and cap checks first.
- Remove unconditional `--force`; a valid hit performs no Ghidra call.
- Use bounded lookahead only for bundle prefetch.
- Keep current score pointers worktree-local and publish immutable copies only
  under a complete attempt fingerprint.
- Route from typed park attempts, not failure-file existence. Park attempts
  carry `outcome`, `fingerprint`, and evidence references.

### 4. Tighten agent context

- Use dedicated memoryless automated analyst and reviewer profiles. Retain the
  memory-enabled manual RE profiles.
- Replace four mandatory per-target skill reads with one versioned compact
  auto-lift checklist; detailed skills remain available on demand.
- Remove invalid `lift-arg-hazards` and `lift-frame-hazards` paths.
- Give reviewers the immutable bundle plus fresh diff, ABI, hazard, build, and
  VC71 evidence. Ghidra is allowed only for invalid or incomplete bundles.
- Keep the mechanical acceptance path and fail-closed reviewer thresholds.

### 5. Retrieval experiment

- Recompute current-index neighbor IDs; never persist neighbor bodies.
- Load at most one neighbor body on demand under a fixed character budget.
- Assign retrieval/control cohorts deterministically by address.
- Do not expand retrieval beyond the cohort experiment until each cohort has at
  least 50 targets and retrieval improves accepted lifts per 100,000 tokens
  without increasing reviewer rejections or runtime failures.

## Tests and acceptance

- Unit-test context and attempt fingerprint stability/invalidation.
- Prove valid hits perform zero Ghidra builds and stale entries rebuild.
- Prove score evidence crosses worktrees only on a full fingerprint match.
- Test confirmed-cap equal/lower/strict-improvement transitions.
- Prove legacy prose/context/failure records cannot trigger skips.
- Statically check for no unconditional force, no Opus research call, no broken
  skill path, and no project memory in automated profiles.
- Run repeated two-target dry runs when Ghidra/runtime infrastructure is
  available; the second run must keep selection/verdicts, build zero repeated
  contexts, and reduce median tokens by at least 30%.
- Run park/classifier tests, hazard and ABI audits, Python syntax checks,
  workflow validation, and a clean Halo build.

## Delivery slices

1. Cap-state fix and measurement fields.
2. Fingerprinted shared store and tests.
3. Mechanical bundle preparation and cache cutover.
4. Memoryless agents and compact checklist.
5. Retrieval experiment and reporting.

Completion means cross-session/worktree lifts reuse only fingerprint-valid
mechanical evidence, research prose never controls decisions, and every
existing safety gate remains active.
