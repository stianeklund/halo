---
description: Run the evidence-preserving source-recovery ladder on already-lifted code
---

`source-recovery` is the neutral subset/alias of `/recover-source` for already-lifted
code. Use `/recover-source` for the complete manifest-driven workflow,
including evidence-backed corrective fidelity work.

Use `source-recovery` plus its support skills: `source-recovery-baseline`, `source-recovery-gap-audit`, `re-comment-capture`, `local-var-source-recovery`, `naming-confidence`, `name-cleanup`, `struct-recovery`, `struct-recovery` (Phase 2), `offset-to-struct`, `source-recovery-regression-triage`, and `source-recovery-report`.

Target: $ARGUMENTS

Follow the source-recovery ladder in order. Default to codegen-preserving work only; do not enter expression simplification or control-flow source-recovery unless the user explicitly requests risky source-recovery. Stop if the working tree is not suitable, the target is not already lifted/committed, or any verification gate regresses.
