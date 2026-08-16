# Research reuse across auto-session / goal-lift

Status: EVAL BRIEF (2026-08-16). Not an implementation ticket. Written so a
second agent can audit the claims against the tree and push back.

Question asked: does the Claude auto-session / goal-lift workflow accumulate
per-worktree research, and should we reuse it? Upside is a free second lift.
Downside is poisoned inference steering later work.

Primary files:

- `.claude/workflows/auto-session.js`
- `.claude/workflows/goal-lift.js` (`researchPrompt`, `researchMore`, `parkToolPrompt`)
- `tools/llm_auto_lift.py` (`CONTEXT_CACHE`, `FAILURES_DIR`, `cache-context --force`)
- `tools/lift/park.py` (`ledger_root()`, `context` field, `confirm-cap`)

Related incident that triggered the question: `FUN_000c2a80` cold-lifted 9–10
times at a byte-identical 83.6% because the parked ledger was worktree-local
and `classify_cap.py` had no float-arg rule. That selector bug is separately
fixed (`adfe9e278`, `22748b5b0`). This brief is about the *research* layer,
not the skip-guard.

---

## Claim 1 — auto-session does not store research

`auto-session.js` is an outer land-loop. It does not write a research store.
It forwards `--addrs` / `--objects` / `--liftRegArgs` into a nested
`goal-lift` run and then calls `auto_reintegrate.py`. All knowledge lives in
whatever goal-lift writes.

Verify: grep `auto-session.js` for `context_cache`, `park.py`, `research`.
The only research-adjacent comment is about `researchMore()` returning
nothing (line ~186).

## Claim 2 — the research brief dies with the process

`goal-lift.js` `researchMore()` (around line 1544) fills in-memory `briefs[]`
/ `okBriefs[]` and feeds them to `liftPrompt` in the same run. Nothing
serializes that schema (`BRIEF_SCHEMA`) to disk.

On the next auto-session / goal-lift invocation the array is empty and
`researchPrompt` runs again.

## Claim 3 — cache-context is written, then ignored

`researchPrompt` step 3a (around line 416) always runs:

```
timeout 240 rtk python3 tools/llm_auto_lift.py cache-context --target ${t.addr} --force
```

`--force` bypasses the skip-if-cached path in `llm_auto_lift.py` (~2515–2520).
The pack lands at `artifacts/auto_lift/context_cache/<name>.json` (decomp,
disasm, callees, callers, struct_offsets). That is Ghidra output, not agent
diagnosis.

`LiftabilityScorer._gather_ghidra_context` *would* reuse a non-empty pack.
`researchPrompt` never takes that path because it passes `--force` and then
re-decompiles via MCP anyway.

So 9 sessions on `FUN_000c2a80` paid 9 Ghidra+Opus research passes. The pack
on disk was overwritten, not used as a skip.

## Claim 4 — the one shared store that could hold a brief is unused

`park.py` record schema has `"context": {...}|null`, capped at 16KB
(`MAX_CONTEXT_BYTES`), documented as “latest --context research brief”.
`park.py next` returns it as `context` for the improve pass.

Census 2026-08-16 against the shared ledger (`park.ledger_root()` =
`/mnt/g/dev/halo/artifacts/parked` from this worktree):

- 198 records
- **0 with a `context` field**
- 124 with at least one `cap_hypothesis`
- 476 attempts, 129 with `notes`

`goal-lift.js` `parkToolPrompt` (~894–899) calls `park.py park` with
`--score --model --effort --reason --cap-hypothesis --notes --revert-tree`.
It never passes `--context`.

The improve pass (`nextPrompt` → `last_notes` / `tried_summary`) then runs
`researchPrompt` from cold anyway (~1100). Notes are flavor, not a skip.

## Claim 5 — several caches are still worktree-local

`park.py` writes through `ledger_root()` (parent of `git-common-dir`). That
is shared. These are not:

| Store | Path (relative to checkout) | This worktree | Main checkout |
|---|---|---|---|
| context_cache | `artifacts/auto_lift/context_cache/` | 186 | 200 |
| score_context | `artifacts/score_context/` | 4714 | 5288 |
| failures (prior_fail) | `artifacts/auto_lift/failures/` | **0** | **64** |

`llm_auto_lift.py` `FAILURES_DIR` and `CONTEXT_CACHE` are `ROOT / "artifacts" / ...`
where `ROOT` is this file's repo, i.e. the current worktree. Same bug class as
the old parked-dir split that made `skip_parked_repeat` see `parked_attempts=0`.

Consequence: `skip_prior_fail` is half-dead in a linked worktree. A function
that failed 48 times on main is “fresh” here.

`artifacts/` is gitignored. Copies do not travel with the branch.

## Claim 6 — other knowledge stores exist, none are a research ledger

- `tools/retrieval/index.duckdb` — similar *already-ported* functions, rebuilt
  from this checkout's `tools/retrieval/`. Live hook into decompile. Not a
  per-target research brief. Neighbors go stale as more functions land.
- `.claude/agent-memory/` — human/agent notes in this checkout. Not written by
  auto-session. Already demonstrated poison: `reference_float_arg_lowering_cap_confirmed.md`
  claimed several handlers “LANDED” that were still `ported: null` / absent
  from `src/` (the file's own “stored-score-without-source” trap, fired ≥6×).
- `kb_meta.json` — git-tracked inferred claims. Fine as archaeology, bad as a
  skip-gate (parameter names like `yaw`/`object_handle` are labeled inferred
  in the same file).
- `artifacts/score_context/<name>.json` — mechanical VC71 `diff.ops`. This is
  the good reuse: `classify_cap.py` R4/R5 prove a cap from the current
  attempt's own ops. Worktree-local, so a sibling worktree may not see it.

---

## Poison cases (already observed, not hypothetical)

1. **Wrong family.** `docs/lift-learnings.md` §46 (pre-fix) and several
   agent-memory notes filed `shader_environment_texture_animation_evaluate`
   as the float-arg-lowering cap. Its score-context has 10 non-equal ops,
   none of them the R4 GPR-vs-x87 substitution. Real cap is float-`!=`
   assert `fucompp` (now R5). 10 attempts, all `cap_hypothesis` empty, so
   even R3 could not save it. A stored brief saying “float-arg, stop” would
   have been the wrong skip.
2. **LANDED without source.** Score-context + memory prose claimed 0xc2f90 /
   0xc2fe0 / 0xc35b0 / 0xc3760 had landed. `grep` in `src/` was 0 hits; kb
   still `void(void)` / `ported: null`. Replaying that sentence skips a
   real lift or trusts a phantom consumer widen.
3. **FLD emission order ≠ argument order.** Two-float handlers FLD `+0x8`
   before `+0x4` because MSVC fills the deeper slot first. Transcribing
   emission order as C arg order swaps components with no assert, no crash,
   no VC71 signal (both spellings emit the same two GPR moves). A cached
   “arg order = FLD order” note is a silent correctness bug.
4. **Consumer still `void(void)`.** Ghidra drops push-then-fstp args. A
   brief that skipped the widen leaves the next land failing at compile or,
   worse, calling with the wrong register/stack convention via XCALL.

Rule of thumb: mechanical artifacts (XBE bytes, VC71 ops, Ghidra disasm)
age well. Agent prose about what those mean does not.

---

## What reuse is worth

Safe to reuse as a **skip or input**, binary-backed:

- `classify_cap` R4 / R5 / `capped_confirmed` → do not research, do not lift
- `score_context` `diff.ops` → feed classify_cap
- parked `best_score` + `best_patch` → improve warm-start (already done)
- Ghidra `context_cache` pack **if** content-hash matches current XBE/function
  bounds → skip `--force` refetch
- `already_in_source` from a src/ scan (goal-lift step 2b already does this
  live; do not persist a “yes” from last month)

Unsafe to replay as truth:

- `disasm_notes`, full `BRIEF_SCHEMA`, retrieval `neighbors`
- agent-memory “LANDED” / family membership
- `kb_meta.json` inferred names
- `source_path` guesses (`players.c` vs `hs/hs.c`)
- last session's `pre_screen=ok` (kb decls and source move)

---

## Suggested design (for the evaluating agent to accept / reject)

Do **not** persist full research briefs. Persist a small shared typed record.

Location: same `ledger_root()` as park (not `ROOT/artifacts/...`). Either a
new `artifacts/parked/research/<name>.json` or a *replacement* for the unused
`context` blob — not both.

Schema (illustrative, keep it this small):

```
cap_rule            r4_float_arg | r5_fucompp | reg_prologue | none
predicted_score     83.6 | null
consumer_widens     [{addr, from_decl, to_decl}]
fld_slot_order      ["+0x0 int", "+0x4 float", "+0x8 float"]  # dest slots
already_in_source   "src/halo/hs/hs.c" | null
score_context_hash  ...
updated             iso
```

Trust order, fail closed:

1. classify_cap / `capped_confirmed` — skip
2. score_context ops — mechanical
3. distilled fields above
4. `cap_hypothesis` / notes — hint only, never a skip-gate
5. agent-memory / kb_meta — do not auto-ingest

`researchMore` skip when any of: confirmed cap, R4/R5 on existing
score-context, `already_in_source` live grep.

Drop `--force` unless the pack is missing or the Ghidra/bounds hash changed.

Pass a trimmed distilled JSON as `--context` on park, **or** delete the
field. A half-wired store is how “we already researched this” becomes a lie.

Resolve `failures/`, `context_cache`, and `score_context` through
`ledger_root()` the same way parked-dir was fixed. Otherwise reuse only
works in whichever checkout happened to write the file.

Do **not** build: per-worktree research wikis, auto-written agent-memory from
goal-lift, replaying last session's full brief as the next lift prompt,
long-lived retrieval neighbor snapshots.

### Implementation slices if accepted

1. Mechanical bleed-stop (no new schema): drop unconditional `--force`;
   `researchMore` short-circuit on `capped_confirmed` / R4 / R5; point
   `FAILURES_DIR` / `CONTEXT_CACHE` / `SCORE_CONTEXT_DIR` at `ledger_root()`.
   Separate small commits.
2. Distilled record + park write + `researchMore` read. Same PR as wiring or
   deleting `park --context`.
3. Do not start (2) until (1) is in, so we can measure whether the remaining
   re-research is still expensive.

---

## How to verify the claims in this brief

```
# park.context unused
rtk python3 -c "
from pathlib import Path
import json, sys
sys.path.insert(0,'tools/lift')
from park import ledger_root
root = ledger_root()
recs = list(root.glob('*.json'))
ctx = sum(1 for p in recs if json.loads(p.read_text()).get('context'))
print('ledger', root, 'records', len(recs), 'with_context', ctx)
"

# researchPrompt always --force
rg -n 'cache-context --force' .claude/workflows/goal-lift.js

# parkToolPrompt has no --context
rg -n 'park.py park|--context' .claude/workflows/goal-lift.js

# worktree split
ls artifacts/auto_lift/failures 2>/dev/null | wc -l
ls /mnt/g/dev/halo/artifacts/auto_lift/failures 2>/dev/null | wc -l

# R4 vs shader (not the same cap)
rtk python3 tools/analysis/classify_cap.py \
  --name shader_environment_texture_animation_evaluate --addr 0x190a90 --score 86.2 \
  --score-context artifacts/score_context/shader_environment_texture_animation_evaluate.json
```

Expected: `with_context 0`; `--force` present; `--context` absent from the
park command; failure counts disagree across checkouts; classify_cap on
shader returns `fucompp_assert_cap` (R5), not `float_arg_lowering_cap`.
