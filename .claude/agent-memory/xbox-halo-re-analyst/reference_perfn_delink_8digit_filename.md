---
name: perfn-delink-8digit-filename
description: vc71_verify per-function ref lookup needs 8-hex-digit filename (00105980.obj); prefetch often writes 6-digit (105980.obj) → falsely "no reference"
metadata:
  type: reference
---

vc71_verify.py `_per_function_ref()` matches `FUN_([0-9a-f]{8})` and looks for
`delinked/functions/<8hexdigits>.obj` (e.g. `00105980.obj`). The lift-prefetch
stage sometimes writes the per-function ref with a 6-digit name
(`105980.obj`), so vc71_verify reports "No existing delinked reference
contains: FUN_..." and the pipeline's vc71 step FAILs with no score — even
though a valid per-fn ref exists.

**Why:** filename-format mismatch, not a missing/stale ref. The whole `delinked/functions/`
dir uses zero-padded 8-digit names (`00012110.obj`).

**How to apply:** if vc71 says the delinked ref is missing but
`ls delinked/functions/ | grep <6digit>` shows a file, just
`cp delinked/functions/<6digit>.obj delinked/functions/00<6digit>.obj` and
re-run. Do NOT waste turns re-exporting via ghidra-live (step 6b) — the ref is
already complete. Also pass `--no-cache` to vc71_verify after editing the
source, or it compares against a stale cached VC71 object that predates your
function (symptom: "Function ... not found in both objects"). Related:
[[reference_perfn_delink_range_and_stub_returns]].
