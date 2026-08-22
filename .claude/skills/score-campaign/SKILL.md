---
name: score-campaign
tier: user
description: Goal-mode VC71 byte-accuracy campaign — sweep the whole score frontier (not just the permuter's [85,98] band), route each target to the right mechanism (vc71-match-optimizer levers below 85%, permuter-campaign at [85,98]%), gate every change, ledger the results, and stop on goal/dry-frontier/park. The unattended driver `/campaign` is to `/auto-session`.
---

# /score-campaign — Goal-Mode Byte-Accuracy Campaign

`/permuter-campaign` covers one band ([85, 98]% with a delinked reference).
`vc71-match-optimizer` fixes one function at a time. Neither loops the whole
kb.json for a stated number of *improved* functions and neither keeps a
cross-run ledger. This is that outer loop — the score-work analog of
`/campaign` (which supervises `/auto-session` for lift work) and `/recover-goal`
(goal-mode readability). It adds no gates and weakens none: every commit still
goes through `score_improve.py check` (lever band) or the permuter's two-gate
regression check (permuter band).

This skill is **byte-accuracy only**. It never touches naming, comments, or
control-flow shape for readability — that is `source-recovery`'s job, kept
deliberately separate (mixing the two makes a regression unbisectable).

Argument: $ARGUMENTS

## Argument parsing (all optional)

- `N` (bare positional) or `--goal N` — functions **improved** (committed,
  strictly higher score) before stopping (default: 8).
- `--max-runs N` — batches before stopping regardless of goal (default: 6).
- `--band permuter|lever|both` — restrict to one mechanism (default: both).
- `--min-score X` — floor; below this a low score is more likely a real logic
  bug than a codegen lever and is flagged, not attempted (default: 50).
- `--max-score X` — ceiling; scores at or above this are done (default: 98.9).
- `--tu <file.c>` / `--object <name>.obj` — restrict discovery to given
  translation unit(s)/object cluster(s); repeatable.
- `--min-insns N` — skip targets whose reference (`n_r`) is smaller than this;
  too few instructions for a lever to mean anything (default: 6). Does not
  apply to a target already carrying a `score_context` classification — a
  classified tiny wrapper is a known-shape bug (see hs_dispose precedent
  below), not noise.
- `--attempt-ceilings` — also attempt targets classified with a permanent
  ceiling rule (`regarg_structural_ceiling`, `regarg_static_helper_ceiling`,
  SEH wrappers, `@<reg>` fastcall preambles). Off by default — these do not
  move.
- `--dry-run` — discovery, banding, and routing only; no edits, no commits.

## Preflight (once)

1. **Build clean.** `rtk python3 tools/build/build.py -q --target halo`.
2. **Sync decl.h.** `rtk python3 tools/build/knowledge.py --decl-only -q` —
   stale headers break both `vc71_verify` and the permuter's pycparser step.
3. **Branch note.** Unlike `/auto-session`, this flow never merges parallel
   worktrees, so `main` is not blocked outright. Still recommend a session
   branch — many small unattended commits are easier to review and revert as
   a set. If on `main` and the user gave no explicit override, ask once
   before the first commit.
4. **Reconcile the ledger.** Read `artifacts/score_campaigns/attempted.json`
   (create as `{}` if absent). Drop any entry whose function's *current*
   `tools/verify/vc71_scores.json` score already meets `--max-score` (someone
   else already fixed it) or whose `artifacts/score_context/<name>.json` no
   longer exists (context stale after a re-lift).
5. **Staleness caveat.** `vc71_scores.json` can lag uncommitted worktree edits
   by several percentage points (observed drift: up to 22.5pp on 9/255
   functions in one TU). Treat every score from the census/jq queries below as
   a **prioritization hint**, not ground truth — each dispatched worker
   re-measures live with `vc71_verify.py --no-cache` before recording its own
   baseline.

## Step 1 — Discover the frontier

**Aggregate view first** (cheap, gives band histogram + known classification
rule counts):
```bash
rtk python3 tools/verify/vc71_low_score_census.py --output /tmp/score_census.json
jq '.distribution, .low_score.rule_counts' /tmp/score_census.json
```

**Full eligible list** (the census tool only lists sub-70; query the scores
file directly for the requested band):
```bash
jq -r --argjson lo "$MIN_SCORE" --argjson hi "$MAX_SCORE" --argjson mi "$MIN_INSNS" '
  .scores | to_entries[]
  | select(.value.kind == "auto")
  | select(.value.n_r >= $mi)
  | select(.value.score >= $lo and .value.score < $hi)
  | [.key, .value.score, .value.n_r, .value.source] | @tsv
' tools/verify/vc71_scores.json | sort -k2 -n
```
(`kind == "auto"` excludes `thunk`/`table_data`/`no_terminator` rows — those
are 1-instruction stubs or data tables, not real function bodies to lever.)

For each candidate, check for a score-context pack:
```bash
jq '{classification}' "artifacts/score_context/<name>.json" 2>/dev/null
```
- Any `regarg_structural_ceiling` / `regarg_static_helper_ceiling` rule and no
  `--attempt-ceilings` → mark `skip_reason: ceiling`.
- Otherwise keep the classification — it goes into the worker brief so the
  agent jumps straight to the matching `lift-score-improve` recipe instead of
  re-deriving it from a raw diff.

Drop anything already in `attempted.json` with `result` in
`{"ceiling", "reverted", "no_improvement"}` from the **same** or a very recent
run — retry it in a later campaign once neighboring TU state has changed, not
this one.

**Split by band:**
- `permuter_targets`: score in `[85.0, min(98.0, MAX_SCORE))`. Quick-check a
  delinked reference exists before routing here:
  `ls delinked/$(jq -r '...|.obj' kb.json).obj 2>/dev/null` — if absent, move
  the target to `lever_targets` instead (permuter-campaign would skip it
  anyway; save the round-trip).
- `lever_targets`: everything else in `[MIN_SCORE, 85.0)`.

Group both lists by `source` (TU). Rank `permuter_targets` by score descending
(permuter-campaign re-ranks internally too, but a shorter, pre-filtered list
means fewer wasted worker slots). Rank `lever_targets` classified-first (a
known `rule` id is a near-guaranteed win), then by score descending.

Cap this run's batch: **permuter band** — hand the whole filtered shortlist to
`permuter-campaign`, which already caps at 20 internally. **Lever band** — cap
at **3 TUs**, up to **5 targets per TU**, this run.

If `--dry-run`: print the two shortlists with source files and classification,
write nothing, stop here.

## Step 2 — Dispatch: permuter band

If `--band` includes `permuter` and `permuter_targets` is non-empty, invoke
the `permuter-campaign` skill for this run, telling it explicitly which
source files / function names to prioritize (it will still run its own
eligibility gates — that is correct, not redundant, since it also checks
`__asm` blocks and known ceiling patterns this pass doesn't). Let it run its
full four-step procedure (parallel search → serial apply → two-gate check →
commit) unmodified. Do not re-implement any part of it here.

When it finishes, read its results table (`artifacts/permuter_campaign/results_*.md`)
and fold every `COMMITTED` row into this campaign's ledger (Step 4).

## Step 3 — Dispatch: lever band

This band has no existing batch mechanism — `vc71-match-optimizer` is a
single-invocation agent. This campaign supplies the batching and briefing;
the agent still owns the actual gate (`score_improve.py baseline`/`check`)
and the commit, exactly as it does standalone.

For each TU group in `lever_targets` (serially — a shared TU means shared
baseline state and commit ordering matters; do not run two TUs' agents
concurrently if they might touch the same `kb.json` region):

1. Record the TU baseline once, outside the agent, so the campaign has its
   own before/after regardless of what the agent reports:
   ```bash
   rtk python3 tools/verify/score_improve.py baseline \
     --source <tu.c> --output artifacts/score_campaigns/<run>/<tu-stem>-baseline.json
   ```
2. Spawn one `vc71-match-optimizer` Agent for this TU with a self-contained
   brief: the TU path, the list of target function names with current score
   and `n_r`, each target's `score_context` classification (if any), and:
   - Use the `lift-score-improve` recipe atlas; for a classified target, go
     straight to the matching rule's recipe.
   - One evidence-backed lever at a time, gated with
     `tools/verify/score_improve.py check` against the baseline from step 1 —
     keep only a change that improves the target ≥0.01pp with no neighbor
     regression and no new warning.
   - Stay inside the given function list; do not touch other functions in the
     TU.
   - Commit each accepted improvement itself via
     `tools/audit/generate_lift_commit.py` (mktemp message file, never a
     fixed path — shared by every concurrent agent/cron job on this box).
   - Return a per-function `{name, score_before, score_after, commit_sha,
     result}` table plus tokens/time.
3. Read the agent's report. For any target it marked `no_improvement` or
   `reverted`, trust it — do not re-run the same lever search yourself.

## Step 4 — Ledger

Append one row to `artifacts/score_campaigns/score_campaigns.jsonl` per run:
```json
{"ts": "<ISO-8601>", "campaign": "<branch>@<start-date>", "run": N,
 "band": "both|permuter|lever", "targets_attempted": N,
 "functions_improved": N, "total_pp_gained": N, "reverted": N,
 "ceiling_skipped": N, "tokens_spent": N, "wall_s": N}
```
Update `artifacts/score_campaigns/attempted.json` per function:
`{"<name>": {"last_run_ts": "...", "result": "improved|no_improvement|reverted|ceiling", "score_before": N, "score_after": N}}`.

Both files live under gitignored `artifacts/` — force-track once
(`rtk git add -f artifacts/score_campaigns/*.json*`) and commit alongside the
run's other work as a standalone chore commit (same precedent as
`campaigns.jsonl` and the parked-record ledger — a missing path in
`git show --stat` later is data loss, not an empty run).

## Stop conditions

- `--goal` reached (cumulative `functions_improved` across this campaign's
  runs).
- `--max-runs` reached.
- **Frontier exhausted** — Step 1 produces two empty shortlists after
  ledger/ceiling filtering. Report this as a finding: which bands are dry,
  how many were skipped as ceilings vs. flagged sub-`--min-score`.
- **Two consecutive runs with `functions_improved == 0`.**
- **`guard_failed`** — `rtk git status --short` shows tracked dirt outside the
  known-absorbed generated set (`README.md` stats block,
  `tools/verify/vc71_scores.json`, `artifacts/score_context/**`,
  `tools/equivalence/leaf_cache.json`, `artifacts/**`). Anything else is
  authored WIP — stop, do not commit over it.

## Final report

Report, in order: runs completed / max; functions improved vs. goal, by band;
total pp gained; every ceiling/park encountered and why; the ledger totals
(functions improved, tokens spent, wall time) and, if a prior row exists in
`score_campaigns.jsonl`, the per-function token/time cost next to it for
trend comparison. Sub-`--min-score` targets that were flagged rather than
attempted are **not** failures of this campaign — list them separately as
candidates for `xbox-halo-re-analyst` / a re-lift review, since a genuinely
wrong score that low is usually a logic bug, not a codegen lever (per the
verification decision table in CLAUDE.md: "<85% → investigate lift, don't
permute" — this campaign's own floor is more conservative than that, by
design, to keep the unattended loop from silently papering over real bugs).

## Caveats carried from prior campaigns

- **`"ref": "synth"` is the normal reference source** (derived from the
  pristine XBE + the committed bounds table), not a sign of a missing or
  fake reference — do not filter on it.
- **A call-count delta on a tiny reference (< ~12 instructions) is a dropped
  call, not a codegen artifact.** `hs_dispose` sat at 66.7% for this exact
  reason — one of its two calls was missing, not a shape mismatch. Any
  classified `SHAPE-WARN`/frame-mismatch target under `--min-insns` should
  still be attempted, not silently skipped (the `score_context` override in
  Step 1 handles this).
- **A high mnemonic score with a low operand score can mean a bad reference**
  (truncated at the first RET, stale cached `.obj`, whole-object section-0
  compare) rather than a bad lift — the worker should check
  `tools/verify/vc71_current.json`'s `n_c`/`n_r` ratio before spending levers
  on what might be a measurement bug.
- **A stored 100% score is not proof of correctness** — one was later found
  to be the wrong loop variable. If bisecting a runtime regression lands on a
  commit this campaign made, treat "but VC71 was 100%" as no defense.

## See also

`vc71-match-optimizer` and `permuter-campaign` own the actual lever/permute
mechanics — this skill only discovers, batches, briefs, and ledgers.
`lift-score-improve` is the recipe atlas both mechanisms draw from.
`halo-verify-debug` covers the verification ladder this all sits on top of.
`/campaign` is the lift-work analog (supervises `/auto-session` toward a
functions-committed goal); `/recover-goal` is the readability analog — mirror
their fail-closed posture (stop on real signal, never paper over a park), not
their target-selection logic, which is unrelated to score work.
