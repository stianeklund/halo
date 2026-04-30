---
description: Analyze and reclassify <common> functions in kb.json
---

Run `tools/classify_common.py` to analyze the 150+ functions currently in
`<common>` that lack an object file assignment in `kb.json`.

## Modes

Pick the mode that matches the goal:

| Flag | What it does | Requires Ghidra? |
|------|-------------|-----------------|
| `--summary` | Counts and top targets | No |
| `--high-only` | Only high-confidence reclassifications | No |
| `--by-target` | Grouped by proposed destination object | No |
| `--blocks` | Gap clusters — probable missing objects | No |
| `--delinker-plan` | Ready-to-paste Ghidra RPC commands | No |
| `--delinker-analyze` | Live delinker export + XBE string resolution | **Yes** |
| `--json` | Machine-readable output | No |

## Typical workflows

**Quick overview:**
```
python3 tools/classify_common.py --summary
```

**Full delinker-powered analysis** (requires Ghidra with delinker plugin on port 18080):
```
python3 tools/classify_common.py --delinker-analyze
```

This exports COFF objects via the Ghidra delinker RPC, parses relocation
tables for `__FILE__` assert strings, and resolves full source paths from
the XBE binary. It reports confirmed matches, newly discovered object files,
and mismatched predictions.

## After reclassification

When moving functions from `<common>` to a real object in `kb.json`:
1. Only move functions with delinker-confirmed source file evidence.
2. Verify the function is not already ported in a different source file.
3. The post-edit hook runs `maintain.py` automatically.
4. Run `python3 tools/analysis/frontier.py` to see updated frontier scores.

## Notes

- The `--delinker-analyze` mode calls the Ghidra delinker RPC directly
  (port 18080), not through the MCP SSE transport.
- The script reads full source paths from the XBE at
  `halo-patched/cachebeta.xbe` to resolve truncated Ghidra symbol names.
