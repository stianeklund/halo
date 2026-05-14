---
description: Clear Halo CE cache files from Xbox devkit cache partitions
agent: fast
subtask: true
---

Clear Halo CE related cache files from Xbox devkit cache partitions via XBDM/RDCP.

Argument: $ARGUMENTS (optional flags passed to clear_cache.py, e.g. `--dry-run`, `-x 192.168.1.42`, `-v`)

Steps:
1. Load the `halo-clear-cache` skill.
2. Run `rtk python3 tools/xbox/clear_cache.py $ARGUMENTS`.
3. Report: items deleted per partition, total space freed, any failures.

Notes:
- **Always recommend `--dry-run` first** to preview deletions.
- Use `-x <host>` to target a specific Xbox IP.
- The tool is surgical; it does not wipe entire partitions.
- Requires a reachable Xbox devkit with XBDM running.
