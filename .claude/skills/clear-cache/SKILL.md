---
name: halo-clear-cache
description: Clear Halo CE cache files from Xbox devkit cache partitions
---

# Halo Clear Cache

Use this skill to clear Halo CE related cache files from Xbox devkit cache
partitions via XBDM/RDCP.

## Standard workflow

Run the cache clear tool:

```bash
rtk python3 tools/xbox/clear_cache.py [--dry-run] [-x <host>] [-v]
```

## What it clears

The tool scans and removes Halo files from these partitions:

- **P:** - All titles cache (cache000.map, cache001.map, etc.)
- **T:** - Active title temporary data
- **U:** - Active title save metadata
- **Z:** - Title state files (last_solo.txt, lastprof.txt, etc.)

It recurses into subdirectories on P:, T:, and U: to find nested cache files.
The `z:\saved` directory is always preserved to protect actual save data.

## Safety

- **Always use `--dry-run` first** to preview what would be deleted.
- The tool only removes files matching Halo CE cache patterns:
  - `.map` files (all maps in cache partitions are assumed Halo)
  - `.xbx` files (Xbox save metadata)
  - `.txt` files (last_solo.txt, lastprof.txt, etc.)
  - `savegame.bin`
- Other titles' data and non-Halo files are preserved.

## Output expectations

Report:

- Number of items deleted per partition
- Total space freed
- Any failures with exact error codes
- List of preserved items when verbose

## Important notes

- Requires a reachable Xbox devkit with XBDM running.
- Use `-x <host>` to target a specific Xbox IP.
- The tool is surgical by design; it does **not** wipe entire partitions.
- If no Halo cache files are found, it reports "already clean".
