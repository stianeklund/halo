---
name: cleanup-regression-triage
tier: agent
triggers: ["cleanup regression", "match dropped", "score dropped", "match regression", "score regression", "vc71 drop", "match fell"]
description: Isolate and explain a VC71-match or test regression caused by cleanup work — localize to the ladder category and unit via per-category commits, classify against the known failure mode of that category, then revert or propose a safer alternative. For runtime symptoms it feeds the debug family with the offending commit pre-identified.
---

# Cleanup Regression Triage

Cleanup regressions are the easy kind — per-category commits mean the search space is
tiny and every category has a characteristic failure mode. Localize → classify →
revert or fix → harden.

## Step 1 — Localize

```bash
rtk python3 tools/verify/vc71_regression.py check --source src/halo/<path>/<file>.c
rtk python3 tools/verify/vc71_verify.py src/halo/<path>/<file>.c --show-diffs
rtk git log --oneline <baseline-rev>..HEAD -- src/halo/<path>/<file>.c
```

Walk the cleanup commits (they are per-category, small): test the file at each rev
(`git stash` + `git checkout <rev> -- <file>` or bisect). VC71 caching makes re-scoring
cheap. Identify the exact commit and, within it, the function(s) that dropped.

Beware known scorer artifacts before blaming a commit: stale delinked chunk refs,
mid-body-ret truncation, and per-fn range issues can masquerade as drops — if the
asymmetry looks structural, re-check the reference (memory: vc71 scoring integrity,
per-fn delink ranges) before reverting real work.

## Step 2 — Classify by category

| Regressing commit category | Expected cause | Check |
|---|---|---|
| comments | impossible — diff wasn't comment-only | inspect diff for stray edits |
| local renames | impossible if rename-only | diff for type/init/decl-position changes |
| constants/enums | bit pattern changed; literal↔const-pool form changed | `--imm-only`; compare immediates at the site |
| offset→struct | width/signedness mismatch; grouped-store base pointer shape lost | `--loadw-only`; §24; lift-score-improve step 2 |
| expr simplify | float reassociation, deleted spill/reload temp, cast width | `--fpu-only`; re-read expr-simplify hard bans |
| control flow | legitimately different codegen | behavioral oracle verdict decides, not the score |

## Step 3 — Decide

- **Revert the unit** (default): restore just the offending function/expression from
  the parent rev; keep the rest of the commit's gains. Park the reverted attempt
  (`artifacts/parked/patches/` — park-never-discard policy) with the classification.
- **Safer alternative**: e.g. keep the raw offset with a `/* keep: codegen-sensitive */`
  comment (`re-comment-capture`), or keep the temp variable but rename it.
- **Accept** only for control-flow commits where the user opted into
  readability-over-match AND the behavioral oracle passed; record old→new floors.

## Step 4 — Runtime/test symptoms (not just score)

Golden/equivalence/ab_check failures after cleanup: the offending commit is found the
same way (step 1, running the failing oracle instead of the scorer). Then hand off to
the `debug` family (toggle-bisect etc.) **with the commit already identified** — don't
re-derive it from crash signals. `prior_fixes.py "<symptom>"` first, per repo rule.

## Step 5 — Harden

Every triaged regression is a `cleanup-gap-audit` data point: if a detector could have
caught it pre-commit, propose it. Recurring pattern → new lift-learnings § (which must
ship its detector).
