---
name: qmd
description: "qmd, markdown search, notes, wiki, retrieve documents: Bootstrap QMD search instructions from the installed qmd CLI. Use when users ask to find notes, retrieve documents, inspect a wiki, or answer from indexed local markdown."
license: MIT
compatibility: Requires qmd CLI. Run `qmd skill show` for version-matched instructions.
---

# QMD - Query Markdown Documents

This installed skill is intentionally a small bootstrap so it does not go stale
when the qmd package updates.

Load the full, version-matched QMD instructions from the CLI:

```bash
rtk qmd skill show
```

Then follow those instructions. Search first, fetch full sources with `qmd get`
or `qmd multi-get`, and answer from retrieved text rather than snippets.
