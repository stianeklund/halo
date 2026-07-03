---
description: Refresh local retrieval/vector index (extract + outcomes + embed + stats)
model: sonnet
subtask: false
---

Argument: `$ARGUMENTS` (optional flags)

Refresh the repo-local vector database used by retrieval neighbor injection and
hazard warning lookup during `/lift`.

## What it runs

Default sequence:

```bash
rtk python3 tools/retrieval/build_index.py extract
rtk python3 tools/retrieval/build_index.py outcomes
rtk python3 tools/retrieval/build_index.py embed
rtk python3 tools/retrieval/build_index.py stats
rtk python3 tools/retrieval/usage_report.py
```

## Optional flags

Pass-through flags from `$ARGUMENTS`:

- `--reembed` → force re-embedding all rows
- `--model <name>` → embedding model for `embed`
- `--batch-size <N>` → embed batch size
- `--chunk-size <N>` → rows per commit chunk

Example:

```bash
/retrieval-refresh --reembed --batch-size 2 --chunk-size 50
```

## Output contract

Report:
- Extract summary (`ported functions written`, pseudocode/C/VC71 coverage)
- Outcomes summary (`vc71_score`, `verdict`, `hazard_flags` counts)
- Embed summary (`rows embedded`, model, batch/chunk settings)
- Final stats (`total`, `with_pseudocode`, `with_c`, `with_embeddings`, `with_vc71`, `with_verdict`, `with_hazards`, `models`)
- Usage/adoption report (`retrieval index` rows, `context cache` neighbor-injection stats, `mizuchi` adoption) from `usage_report.py` — measures whether the index is actually being consulted (the only ROI signal for the 95 MB index).

## Notes

- This updates the local index at `tools/retrieval/index.duckdb`.
- It does not call external Claude/Mizuchi runner APIs.
- **Scope: this is the CODE index** (Ghidra pseudocode + lifted C), used by
  `/lift`'s retrieval-neighbor injection. The separate **doc index** (QMD,
  `~/.cache/qmd/index.sqlite`, queried by `/debug-regression` Phase 0 and
  `/lift`'s doc-hazard lookup) covers `docs/`, skills, and commands. QMD's
  guardrail forbids refreshing it automatically — after editing docs, refresh
  it yourself by running these manually:
  ```bash
  qmd update    # re-scan collections for new/changed markdown
  qmd embed     # generate embeddings for the changed chunks
  ```
