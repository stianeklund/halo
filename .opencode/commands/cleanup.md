---
description: Run the evidence-preserving source-recovery ladder on already-lifted code
---

`source-recovery` is the neutral subset/alias of `/recover-source` for already-lifted
code. Use `/recover-source` for the complete manifest-driven workflow,
including evidence-backed corrective fidelity work.

Use `source-recovery` plus its support skills, in ladder order: `cleanup-report`
(pre-flight), `re-comment-capture`, `name-cleanup` (local renames + const/enum),
`naming-confidence`, `struct-recovery` (+ Phase 2), `header-recovery`,
`offset-to-struct`, and — opt-in only — `expr-simplify` and
`control-flow-cleanup`. `cleanup-regression-triage` isolates a match/test
regression caused by this work.

Target: $ARGUMENTS

Follow the source-recovery ladder in order. Default to codegen-preserving work only; do not enter expression simplification or control-flow source-recovery unless the user explicitly requests risky source-recovery. Stop if the working tree is not suitable, the target is not already lifted/committed, or any verification gate regresses.
