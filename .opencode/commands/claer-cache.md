---
description: Alias for /clear-cache (kept for typo compatibility)
agent: fast
subtask: true
---

Alias command for `/clear-cache`.

Clear Halo CE related cache files from Xbox devkit cache partitions via XBDM/RDCP,
and clear local xemu shader cache files.

Argument: $ARGUMENTS (optional flags passed to clear_cache.py, e.g. `--dry-run`, `-x 192.168.1.42`, `-v`)

Steps:
1. Load the `halo-clear-cache` skill.
2. Run `rtk python3 tools/xbox/clear_cache.py $ARGUMENTS`.
3. Report: Xbox items deleted per partition, local xemu items deleted, and any failures.
