---
name: permuter-campaign
description: >
  Permuter campaign, batch permute, low-match VC71, push stuck lifts toward 100%:
  run a parallel permuter campaign across low-match already-ported functions. Discovers
  eligible targets (VC71 match in [85, 98]% with a delinked reference), spawns isolated
  parallel permuter search workers (each with its own output directory), then applies
  improvements serially with a whole-TU regression gate — reverting any candidate that
  drops a neighbor. Produces a before/after match table. Trigger when the user asks to
  run a permuter campaign, batch-permute low-match functions, or push stuck functions
  toward 100%.
---

# Permuter Campaign

A two-phase orchestration for improving already-ported functions that have a VC71
byte-match score in the [85, 98]% band — the permuter's sweet spot.

**Phase 1 (parallel):** Run the decomp-permuter search for each target in an isolated
scratch directory. No source tree is modified during this phase.

**Phase 2 (serial, by TU):** Apply the best candidate from each successful search,
rebuild, then run a two-gate check: (a) target VC71 match improved, and (b) the
equivalence suite confirms no neighboring function's behavior regressed. Neighbors
whose compiled bytes are unchanged are skipped; only byte-changed neighbors run the
harness. Auto-revert on any gate failure.

---

## Pre-flight

Before anything else, run these checks. Stop if any fail.

```bash
# 1. Permuter is cloned
ls third_party/decomp-permuter/permuter.py || echo "MISSING — clone per docs/permuter-adapter.md"

# 2. VC71 CL.Exe is accessible
test -f "/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71/CL.Exe" || echo "MISSING VC71"

# 3. Build is clean
rtk python3 tools/build/build.py -q --target halo 2>&1 | tail -5

# 4. Sync decl.h (stale headers cause pycparser failures; always sync before batch runs)
rtk python3 tools/build/knowledge.py --decl-only -q 2>&1 | tail -3
```

Kill any orphaned permuter workers from prior runs before starting:
```bash
pkill -f "permuter.py" 2>/dev/null; sleep 1
```

---

## Step 1: Target Discovery

Find all ported functions whose VC71 match is in [85, 98]% and that have a
delinked reference. Functions below 85% have structural bugs the permuter
cannot fix. Functions above 98% are not worth the cycles.

### 1a. Build the candidate list

List all TUs that have delinked references:
```bash
rtk python3 tools/verify/vc71_verify.py --list 2>/dev/null | grep "\[OK\]"
```

For each TU, run vc71_verify to get per-function scores. When a TU has many
functions, use `--function` to check specific ones if you already have a
shortlist:
```bash
rtk python3 tools/verify/vc71_verify.py <source_file> --no-cache 2>&1 | grep -E "^  (FUN_|[a-z])"
```

Parse the output for lines like:
```
  FUN_0003ac20 : 91.2% [93 insn LCS of 102] → match 91%
```

### 1b. Filter and rank

Collect into a manifest at `artifacts/permuter_campaign/targets.json`:
```json
[
  {
    "name": "FUN_0003ac20",
    "addr": "0x3ac20",
    "source_file": "src/halo/game_engine/game_engine.c",
    "delinked_ref": "delinked/game_engine.obj",
    "baseline_pct": 91.2,
    "tu_functions": ["FUN_0003ac20", "other_func_in_same_file"]
  }
]
```

**Eligibility gates (skip if any fail):**
1. `baseline_pct` in `[85.0, 98.0]` — the permuter band.
2. Delinked reference exists in `delinked/` (hard requirement for byte-match evidence).
3. Not a known structural ceiling: exclude functions with `@<ecx>`, `@<eax>` register
   args (permanent preamble mismatch ~15pp gap), SEH-wrapped bodies (~55% ceiling),
   and NT/kernel-import callees (can't emulate). Check with:
   ```bash
   rtk jq '[.. | objects | select(.addr? == "0xADDR")] | .[0].decl' kb.json
   ```
4. Source implementation exists (`rtk rg "^[a-zA-Z_].*FUNCNAME\b" src/ --no-heading -l`).
5. No `__asm` blocks — pycparser cannot parse MSVC asm (the permuter silently fails).

**Rank by**: `baseline_pct` descending (closest to 99% first — easiest wins).
**Cap at 20 targets per campaign run** to bound wall-clock time.

### 1c. Record TU baselines (VC71 + obj checksums)

For every TU that appears in the target list, record two things before any
source edits — you need both for the regression gate in Phase 2.

**VC71 baseline (gate a):**
```bash
rtk python3 tools/verify/vc71_verify.py <tu_source_file> --no-cache 2>&1 \
  | grep -E "^\s+(FUN_|[a-z])" > $CAMPAIGN_DIR/baseline_<TU_name>.txt
```

**Object checksum baseline (gate b — byte-change detection):**

After a clean build, capture an md5 fingerprint of every built object for
functions in this TU. The build artifacts live under `build/halo/CMakeFiles/`:
```bash
# Find the built .obj for each function's TU by object name from kb.json
OBJ_NAME=$(rtk jq -r '[.. | objects | select(.addr? == "0xADDR")] | .[0].obj' kb.json)
find build/ -name "${OBJ_NAME%.obj}*.o" 2>/dev/null | head -3

# Checksum each relevant object file
for obj in $(find build/ -name "*.o" -newer src/<tu>.c 2>/dev/null); do
  md5sum "$obj" >> $CAMPAIGN_DIR/obj_baseline_<TU_name>.md5
done
```

Save these files now. In Phase 2, diff these checksums after apply to find
which neighbors need equivalence testing.

If capturing object-level checksums is impractical (complex CMake paths), fall
back to: record the full `vc71_verify` diff output (`--show-diffs`) for all TU
functions; compare after apply; any function whose instruction diff changed from
baseline needs the equivalence harness.

---

## Step 2: Parallel Permuter Search (Phase 1)

Create a timestamped run directory:
```bash
mkdir -p artifacts/permuter_campaign/$(date +%Y%m%d_%H%M%S)
export CAMPAIGN_DIR=artifacts/permuter_campaign/$(date +%Y%m%d_%H%M%S)
```

Spawn one Agent worker per target. Each worker is fully isolated — it reads
source and produces candidates but does NOT modify the source tree.

### Worker isolation rules (read by each spawned agent)

Each worker receives:
- Its target's entry from `targets.json`
- A dedicated output directory: `$CAMPAIGN_DIR/<funcname>/`
- These operating constraints:

```
WORKER RULES:
- Do NOT modify any file under src/, kb.json, or CMakeLists.txt.
- Your scratch space is $CAMPAIGN_DIR/<funcname>/. Write only there.
- Do NOT run git commands.
- Do NOT run build.py.
- Your only job: run the permuter search and report the best candidate.
```

### Worker procedure (for each spawned agent)

**Step A: Apply known VC71 patterns (in-memory only)**

Before running the permuter, review the target function's source and look for
patterns that could close the gap. Consult `references/vc71_patterns.md` for
the full taxonomy. Run:
```bash
rtk python3 tools/verify/vc71_verify.py <source_file> --function <funcname> --show-diffs
```

Identify which patterns (if any) account for large diff blocks. Write a
`$CAMPAIGN_DIR/<funcname>/pattern_notes.txt` describing what you found and
what you would fix. Do NOT apply the fix yet — just document it. The fix will
be applied later in the serial phase under the regression gate.

**Step B: Run the permuter search**

```bash
timeout 150 rtk python3 tools/permuter/run.py \
  --function <funcname> \
  --source <source_file> \
  --time 120 \
  --threads 2 \
  --output-dir $CAMPAIGN_DIR/<funcname>/permuter_work \
  --quiet 2>&1 || echo "[timed-out]"
```

Read the output from `$CAMPAIGN_DIR/<funcname>/permuter_work/lcs_results.txt`.

**Step C: Report results**

Write `$CAMPAIGN_DIR/<funcname>/search_result.json`:
```json
{
  "name": "<funcname>",
  "source_file": "<path>",
  "baseline_pct": 91.2,
  "permuter_best_pct": 94.1,
  "permuter_improved": true,
  "best_candidate_dir": "$CAMPAIGN_DIR/<funcname>/permuter_work/output-<penalty>",
  "pattern_notes": "comparison direction at FPU branch around 0x3ac45",
  "skip_reason": null
}
```

Set `permuter_improved: false` and `skip_reason` if:
- `lcs_results.txt` is absent or empty (no candidates)
- The best LCS from `lcs_results.txt` does not exceed `baseline_pct`
- Pre-compile of `base.c` failed (incompatible function — cannot permute)

### Concurrency cap

Spawn at most **4 workers concurrently** to avoid VC71 temp-dir contention and
CPU thrash. With 2 threads each, that is 8 VC71 compile slots total. If you
have more than 4 targets, queue them in batches of 4.

---

## Step 3: Serial Apply Loop (Phase 2)

After all workers complete, collect their `search_result.json` files. Sort
winners by `permuter_best_pct` descending so the best improvements go first.

For each winner (`permuter_improved: true`), process one at a time:

### 3a. Pattern fix (if any)

If the worker's `pattern_notes.txt` identifies an applicable VC71 pattern,
apply the source fix first (before the permuter candidate). Then run:
```bash
rtk python3 tools/verify/vc71_verify.py <source_file> --function <funcname> --no-cache
```

If the pattern fix alone reaches >=99%, skip the permuter candidate and commit
directly. If it regresses any TU neighbor (see gate below), revert immediately
and record `PATTERN_REGRESSED`.

### 3b. Apply the permuter candidate

The permuter's best candidate is an isolated `source.c` that contains only the
target function body. Merge it back into the live source file.

**Do NOT blindly overwrite the source file** — the permuter's `source.c` is the
function body after AST round-tripping, which may have lost type aliases or
`#include` dependencies. Instead:
1. Read `$CAMPAIGN_DIR/<funcname>/permuter_work/base.c` to see the extracted body.
2. Read `$CAMPAIGN_DIR/<funcname>/permuter_work/output-<penalty>/source.c` to see the permuted body.
3. Identify the structural change (the diff between base.c and source.c).
4. Apply that structural change to the original source file's implementation of the function.
5. Rebuild to confirm it compiles: `rtk python3 tools/build/build.py -q --target halo`

If the build fails, revert: `rtk git checkout -- <source_file>`

### 3c. Two-gate regression check

Gate (a) is a VC71 byte-match improvement. Gate (b) is behavioral equivalence —
**the equivalence suite, not a VC71 percentage proxy** — because a neighbor can
produce the same LCS% against a fixed reference while its behavior has changed.

**Gate (a): VC71 match improved for the target**

Re-run vc71_verify on the entire TU:
```bash
rtk python3 tools/verify/vc71_verify.py <source_file> --no-cache 2>&1
```

The target function's score must be strictly higher than its baseline. If not,
revert and record `NO_IMPROVEMENT`.

**Gate (b): Equivalence suite — target + changed neighbors**

After a successful build, diff the compiled neighbor objects against their
pre-edit state to identify which neighbors actually changed bytes:

```bash
# Capture neighbor obj checksums before the edit (do this in Step 1c alongside
# the VC71 baseline):
objdump -t build/<obj>.obj | md5sum > $CAMPAIGN_DIR/neighbor_obj_baseline/<name>.md5

# After apply:
objdump -t build/<obj>.obj | md5sum > $CAMPAIGN_DIR/neighbor_obj_after/<name>.md5
```

Neighbors whose object bytes are **identical** to baseline are provably
unaffected — skip them. For every neighbor whose bytes changed, run the
equivalence harness:

```bash
# For the target (always):
timeout 90 rtk python3 tools/equivalence/unicorn_diff.py <funcname> \
  --seeds 50 --allow-stubs --mem-trace --z3-equiv \
  --output-json $CAMPAIGN_DIR/<funcname>/equiv_result.json 2>&1 || echo "[timed-out]"

# For each byte-changed neighbor:
timeout 90 rtk python3 tools/equivalence/unicorn_diff.py <neighbor_name> \
  --seeds 30 --allow-stubs --mem-trace \
  --output-json $CAMPAIGN_DIR/<funcname>/equiv_neighbor_<neighbor_name>.json 2>&1 || echo "[timed-out]"
```

Read the JSON output. Key field is `status`: `"pass"` is acceptable;
`"fail"` or `"error"` blocks the commit.

**Commit if and only if all of these hold:**
1. Gate (a): target VC71 score strictly improved.
2. Gate (b) target: equivalence status is `"pass"` (or `"z3_proven"`).
3. Gate (b) neighbors: every byte-changed neighbor's equivalence status is
   `"pass"` or `"z3_proven"`.

**Revert if any condition fails:**
```bash
rtk git checkout -- <source_file>
```

Record which gate failed and why in the results table.

**Equivalence skip conditions** (treat as pass, do not run harness):
- Function has no delinked reference (cannot run oracle). Note: these should
  have been excluded at Step 1, but guard here too.
- Neighbor's leaf_cache.json class is absent or `"uncached"` and equivalence
  would exceed the 90s timeout: note `EQUIV_SKIPPED` but do not block commit.
  The VC71 byte-delta (gate a) plus the byte-identical check is then the
  evidence.

### 3d. Commit

Only when the regression gate passes:
```bash
rtk git add -- <source_file>
rtk python3 tools/audit/generate_lift_commit.py \
  --batch-name "permuter-campaign: <funcname> <baseline>%→<new>%" \
  > /tmp/perm_commit.txt
rtk git commit -F /tmp/perm_commit.txt
```

**No freeform commit messages.** Use `generate_lift_commit.py` only.
The ABI audit gate inside `generate_lift_commit.py` must pass — permuter
candidates do not change function signatures, so this should always pass.

Record the outcome in a running table (see Step 4).

---

## Step 4: Final Report

After all candidates are processed, produce a Markdown table at
`artifacts/permuter_campaign/results_<timestamp>.md`:

```markdown
# Permuter Campaign Results — <timestamp>

| Function | Source File | Before | After | Delta | Equiv (target) | Status |
|----------|-------------|--------|-------|-------|----------------|--------|
| FUN_0003ac20 | game_engine.c | 91.2% | 96.4% | +5.2pp | pass | COMMITTED |
| FUN_0014b220 | collision_features.c | 87.5% | 87.5% | +0.0pp | — | NO_IMPROVEMENT |
| FUN_00138e30 | some_file.c | 93.1% | 94.8% | +1.7pp | fail | REVERTED (equiv fail) |
| FUN_000abc10 | objects.c | 88.0% | 90.5% | +2.5pp | pass | REVERTED (neighbor regressed) |
| ... | | | | | | |

## Summary
- Targets searched: N
- Improved and committed: N
- No permuter improvement found: N
- Pattern fix applied (no permuter needed): N
- Reverted — VC71 gate failed (no real improvement): N
- Reverted — equivalence gate failed (target behavior changed): N
- Reverted — neighbor equivalence failed (regression detected): N
- Skipped (not eligible): N

## Notes on Reverted Candidates
For each revert, brief note: which gate failed, which neighbor was affected,
and the equivalence result JSON status for diagnostic use.
```

Print the summary table to the conversation and save the file.

---

## Known Structural Ceilings (Do Not Permute)

These will never reach high match — skip early:

| Pattern | Ceiling | Reason |
|---------|---------|--------|
| `@<ecx>` / `@<eax>` fastcall functions | ~65–80% | Preamble `MOV ECX, [ESP+4]` not in our lifted code |
| `__SEH_prolog` / `__SEH_epilog` in body | ~55% | Frame shape differs |
| NT kernel import callees | ~45–65% | Can't emulate; no byte-match evidence |
| trivial 1–3 line wrappers | ~40% | Too few instructions for permuter to work on |
| MSVC loop-unroll vs `REP STOSD` | ~65–70% | Permuter can't transform loop to rep |

---

## Failure Recovery

**VC71 CL.Exe crashes / hangs:** Kill the TMPDIR process:
```bash
pkill -f "CL.Exe"; rm -rf build/vc71/permuter_tmp/permuter_*
```
Then re-run the failed worker with `--time 60` (shorter run to confirm it
compiles cleanly before a full run).

**pycparser parse failure:** The permuter printed `0 iterations` or `Traceback
... pycparser`. This means the function uses a C construct pycparser cannot
parse (MSVC-specific syntax, `__asm`, compound literals). Mark as `SKIP_PYCPARSER`
and move on. Do NOT retry — it will fail the same way.

**decl.h out of sync:** Build fails with `call to undeclared function`. Run:
```bash
rtk python3 tools/build/knowledge.py --decl-only -q
```
Then retry the build.

**Orphaned permuter workers consuming CPU:** `pkill -f "permuter.py"` before
starting a new campaign.

---

## Reference Files

- `references/vc71_patterns.md` — taxonomy of VC71 codegen patterns to apply
  before running the permuter. Read this when the worker examines the function's
  diff in Step 2 Phase A.
- `docs/permuter-adapter.md` — permuter setup requirements, known limitations
  (pycparser, C89 only, no `__asm`), and adapter file descriptions.
- `tools/permuter/run.py` — the permuter driver; `--help` for full flag list.
- `tools/verify/vc71_verify.py` — VC71 verify tool; `--list` to see available TUs.
