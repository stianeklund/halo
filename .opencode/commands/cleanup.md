---
description: Run the evidence-preserving cleanup ladder on already-lifted code
---

Use `cleanup` plus its support skills: `cleanup-baseline`, `cleanup-gap-audit`, `re-comment-capture`, `local-var-cleanup`, `naming-confidence`, `const-enum-recovery`, `struct-recovery`, `struct-assert`, `offset-to-struct`, `cleanup-regression-triage`, and `cleanup-report`.

Target: $ARGUMENTS

Follow the cleanup ladder in order. Default to codegen-preserving work only; do not enter expression simplification or control-flow cleanup unless the user explicitly requests risky cleanup. Stop if the working tree is not suitable, the target is not already lifted/committed, or any verification gate regresses.
