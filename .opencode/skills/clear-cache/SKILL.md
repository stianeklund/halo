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
rtk python3 tools/xbox/clear_cache.py [--dry-run] [--deep] [-x <host>] [-v]
```

## What it clears

The tool scans these partitions:

- **Z:** - Confirmed Halo runtime cache/state paths (`cacheNNN.map`, `last_solo.txt`, `savegame.bin`, etc.)
- **P/T/U:** - Devkit-visible cache/save aliases, scanned conservatively for Halo map-cache names only

By default, `z:\saved` is preserved to protect actual save data. Pass `--deep` to also remove confirmed generated files under `z:\saved`, such as `hdmu.map`, default profile `.sav` files, and default playlist `blam.lst` files.

## Safety

- **Always use `--dry-run` first** to preview what would be deleted.
- The tool only removes path-specific Halo CE cache targets:
  - `z:\cacheNNN.map`, including stale `cache006.map` through `cache019.map`
  - `z:\last_solo.txt`, `z:\lastprof.txt`, `z:\lastmpvr.txt`, `z:\lastmpmp.txt`
  - `z:\savegame.bin`
  - With `--deep`: generated files under `z:\saved`
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
