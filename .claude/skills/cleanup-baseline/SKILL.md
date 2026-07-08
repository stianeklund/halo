---
name: cleanup-baseline
tier: agent
triggers: ["cleanup baseline", "record baseline", "before cleanup", "baseline snapshot", "match floor", "score floor", "vc71_regression"]
description: Record the verification baseline BEFORE any cleanup/refactor of already-lifted code — VC71 match %, object-diff state, harness/equivalence status, hazard state, working-tree state, and the exact commands used. The recorded block is the reference point for cleanup-regression-triage and cleanup-report. Invoke first in every cleanup session, before touching any file.
---

# Cleanup Baseline

Cleanup of already-lifted code is only safe if you can prove the codegen did not
change. This skill records the "before" state so every later step has a mechanical
comparison point. **No cleanup edit is allowed before the baseline block exists.**

## Existing tooling (use, don't rebuild)

| What | Tool |
|---|---|
| Per-function match floors (committed) | `tools/verify/vc71_regression.py` → `tools/verify/vc71_scores.json` |
| Current per-function match | `tools/verify/vc71_verify.py` (SQLite-cached; `--no-cache` to force) |
| Hazard state | `tools/audit/check_lift_hazards.py --changed-only` |
| Delinked reference presence | `objdump -t delinked/<obj>.obj` |
| Equivalence confidence | `tools/equivalence/leaf_cache.json` (via `rtk jq`) |
| Golden harness cases | `src/halo/test_harness.c` |

## Procedure

1. **Working tree.** `rtk git status` and `rtk git diff --stat -- src/ kb.json CMakeLists.txt`.
   Cleanup starts from a **clean tree** (or a recorded stash). If dirty with unrelated
   work, stop and surface it — never mix cleanup into someone else's diff.

2. **Match floors.** For each target file:
   ```bash
   rtk python3 tools/verify/vc71_regression.py check --source src/halo/<path>/<file>.c
   rtk python3 tools/verify/vc71_verify.py src/halo/<path>/<file>.c
   ```
   Record per-function match % and any pre-existing `[FPU-WARN]`/`[LOADW-WARN]`/`[IMM-WARN]`
   lines verbatim — pre-existing warnings must not be attributed to your cleanup later.
   If a floor is missing for a ported function, run
   `vc71_regression.py update --source <file>` first so the gate has teeth.

3. **Delinked reference.** `objdump -t delinked/<obj>.obj | grep -E "FUN_<addr>|<name>"`
   for each function you plan to touch. **No delinked reference → no byte-match oracle →
   only zero-codegen-risk categories are allowed** (comments, local renames); record this
   restriction in the baseline block.

4. **Hazards.** `rtk python3 tools/audit/check_lift_hazards.py --changed-only` on a clean
   tree should report nothing; also run the full-file scan for the targets and record
   pre-existing WARNs.

5. **Behavioral oracles available.** Record which exist for each target:
   - golden harness case: `rtk rg '<func>' src/halo/test_harness.c`
   - equivalence history: `rtk jq '."<addr>"' tools/equivalence/leaf_cache.json`
   - runtime fixture coverage (a10 smoke, ab_check golden) if the TU is runtime-visible.

6. **Emit the baseline block** (goes in the session notes and later into cleanup-report):

   ```
   ## Baseline — <file(s)> @ <git rev-parse --short HEAD> (<date>)
   tree: clean | stash <ref>
   functions: <name> <addr> match=<%> floor=<%> delinked=<yes/no> golden=<yes/no> equiv=<tier/none>
   pre-existing warns: <verbatim or "none">
   commands: <the exact commands run above>
   ```

## Rules

- Never `vc71_regression.py update --force` (lowering a floor) as part of baseline.
- Never flip `ported` flags or touch `@<reg>` annotations while baselining.
- Baseline is read-only: if any step required an edit, that edit was not baseline.

Next: hand the block to the `cleanup` orchestrator (or directly to the category skill
you were asked for). On any later score drop → `cleanup-regression-triage`.
