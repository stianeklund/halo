---
name: permuter-output-retention
description: tools/permuter/run.py deletes its work dir (and the NEW BEST candidate source) unless --keep or --output-dir is passed — always pass --output-dir
metadata:
  type: feedback
---

Always pass `--output-dir artifacts/permuter/<func>_run` (or `--keep`) to
`tools/permuter/run.py`.

**Why:** Without it, the run's temp work dir — including every
`output-<penalty>-1/source.c` candidate and the NEW BEST source — is
deleted in the `finally` cleanup after the summary prints. A 17ab0 run
found an 89.6% candidate (baseline 87.6%) and the source was unrecoverable;
the run had to be repeated with `--output-dir` to capture an equivalent
89.7% candidate.

**How to apply:** Add `--output-dir` to every permuter invocation, then
review `output-*/diff.txt` before applying. Also re-check candidates for
UB transforms: a 25c10 candidate hoisted `int *new_var = &ctx_field` into a
conditionally-executed block but used it unconditionally (uninitialized
pointer read) — the safe equivalent scored no better than baseline, so the
"+0.4pp" was purely the UB. Reject such candidates.
