---
name: cleanup
tier: user
description: Readability-rewrite orchestrator for already-lifted code — /cleanup <file|function|object>. Runs the cleanup ladder in risk order (baseline → structs/naming → mechanical rewrites → optional risky rewrites → report), gating every step on VC71 match preservation and committing per category. Default mode is codegen-preserving; risky categories require explicit opt-in.
---

# /cleanup — Readability Rewrite Orchestrator

Coordinates the cleanup skill family over already-implemented (`ported: true`,
committed) code. **Default contract: the compiled bytes do not change.** Anything
beyond that is opt-in.

Usage: `/cleanup <src file | function | kb.json object> [--allow-risky]`

## The cleanup ladder (risk order — always this order)

| # | Step | Skill | Codegen risk | Gate |
|---|---|---|---|---|
| 0 | Baseline | `cleanup-baseline` | — | block exists before any edit |
| 0b | Tooling check | `cleanup-gap-audit` | — | gaps → downgrade plan |
| 1 | Comments / knowledge | `re-comment-capture` | none | match unchanged |
| 2 | Local renames | `local-var-cleanup` | none | match unchanged |
| 3 | Function/global/type names | `naming-confidence` | none | match unchanged |
| 4 | Constants & enums | `const-enum-recovery` | near-zero | match unchanged + no new IMM-WARN |
| 5 | Structs (define) | `struct-recovery` → `struct-assert` | none (defs only) | build passes (cs/co asserts) |
| 6 | Offsets → fields | `offset-to-struct` | low | match ≥ baseline per function |
| 7 | Expressions | `expr-simplify` | medium | match unchanged per unit — **opt-in** |
| 8 | Control flow | `control-flow-cleanup` | high | byte + behavioral oracle — **opt-in** |
| 9 | Report | `cleanup-report` | — | delivered |

Without `--allow-risky` (or explicit user request), stop after step 6. Steps may be
skipped when inapplicable, never reordered — later steps depend on earlier ones
(offset rewrite needs asserts from 5; renames before rewrites keep diffs reviewable).

## Session rules

- **Scope**: one TU (or one kb.json object's TUs) per session. Resist adjacent-file drift.
- **Preconditions**: clean working tree; target is ported AND committed. Never run
  cleanup on an in-flight lift — finish `/lift` verification first.
- **One commit per category** (Separation rule): a reviewer must be able to say
  "commit 3 is renames-only" and skim it. Never combine ladder steps in one commit.
- **Never touched during cleanup**: `@<reg>` annotations, `ported` flags, kb.json
  signatures, build config. A needed signature fix is *lift* work — stop and surface it.
- **Regression protocol**: any gate failure → the category skill reverts its unit; a
  drop discovered later (or runtime misbehavior) → `cleanup-regression-triage` before
  anything else.
- **Floors ratchet up**: after the session, `vc71_regression.py update --source <file>`
  if anything improved, so gains are locked in.

## Delegation

When fanning categories out to subagents: one category per agent, sequential (each
category's commit is the next one's base — parallel category agents on one TU will
collide). Give each agent the baseline block, the target skill name, and the same-
worktree instruction (subagent worktree mismatch has silently discarded work before).

Finish by invoking `cleanup-report` and presenting its output.
