---
description: Run the manifest-driven faithful source recovery workflow
agent: build
model: openai/gpt-5.6-luna
---

Invoke the `source-recovery` skill and follow it as the sole end-to-end
workflow for `$ARGUMENTS`. Use `tools/recovery/source_recovery.py` to create,
capture, check, report, and update the manifest. Do not perform speculative
automatic rewriting. Preserve Shape, keep `@<reg>` immutable, park any item
that fails evidence or a gate with a reason, and continue with independent
items. Do not commit unless the user explicitly requests a commit.
