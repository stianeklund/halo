---
description: Run the Halo lift bug-hunt router: hazards, silent bugs, ABI drift, and pre-deploy checks
agent: deep
---

Load the `bug-hunt` skill and follow its tiered decision tree.

Argument: $ARGUMENTS (`--all`, `--full`, `--changed-only`, or a short symptom)

Routing:
- Default / `--all`: run Tier 0 and Tier 1 from `bug-hunt`.
- `--full`: run Tier 0, Tier 1, and Tier 3.
- Symptom includes crash, page fault, assert, hang, wrong behavior, wrong color, invisible geometry, or regression: load `debug` and `crash-triage` first, then return to `bug-hunt` for scans.
- Any WARN/HIGH/ERROR in touched files: load the detailed skill named by `bug-hunt` (`lift-silent-bugs`, `lift-arg-hazards`, `lift-decompiler-traps`, or `lift-frame-hazards`).

Report commands run, blocking findings, and the next narrow fix.
