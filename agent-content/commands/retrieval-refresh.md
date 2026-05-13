---
description: Refresh local retrieval/vector index (extract + embed + stats)
model: sonnet
subtask: false
---

Argument: `$ARGUMENTS` (optional flags)

Refresh the repo-local vector database used by retrieval neighbor injection in
`llm_auto_lift.py cache-context`.

## What it runs

Default sequence:

```bash
rtk python3 tools/retrieval/build_index.py extract
rtk python3 tools/retrieval/build_index.py embed
rtk python3 tools/retrieval/build_index.py stats
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
- Extract summary (`ported functions written`, pseudocode/C coverage)
- Embed summary (`rows embedded`, model, batch/chunk settings)
- Final stats (`total`, `with_pseudocode`, `with_c`, `with_embeddings`, `models`)

## Notes

- This updates the local index at `tools/retrieval/index.duckdb`.
- It does not call external Claude/Mizuchi runner APIs.
