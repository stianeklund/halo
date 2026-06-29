---
name: core.bin → unicorn state-snapshot feasibility
description: Feasibility study for using the HaloScript core_save/core_load file (d:\core\core.bin) as a --state-snapshot input to tools/equivalence/unicorn_diff.py, as a cleaner alternative to the finicky xemu QMP memsave workflow.
metadata:
  type: reference
  date: 2026-06-24
  status: CONFIRMED — PARTIAL
---

# core.bin → Unicorn State-Snapshot Feasibility

## Verdict: FEASIBLE (PARTIAL)

`core.bin` supplies the **entire game-state heap** in a single deterministic file, avoiding all the xemu QMP capture finickiness. It is a strictly broader heap capture than `infection_swarm.json` and requires no running xemu instance.

However it is **partial**: ~6 pages of XBE .data/.bss globals that live **outside** the heap are not in the file and must still be seeded separately. A converter must merge core.bin (heap) with separately captured global/rdata pages to build a complete snapshot.

---

## core.bin Format and Layout

### Binary evidence (confirmed via Ghidra decompile of 0x1c0570 + 0x1c0680)

`game_state_write_core` (0x1c0570) and `game_state_read_core` (0x1c0680) are thin Win32 I/O wrappers:

```c
// Write (0x1c0570): args = (name, base_address, size)
h = CreateFile("d:\\core\\<name>", GENERIC_WRITE, ..., CREATE_ALWAYS, ...);
WriteFile(h, base_address, size, &written, NULL);
// success gate: written == size  (zero extra bytes allowed)
CloseHandle(h);

// Read (0x1c0680): symmetric — ReadFile(h, base_address, size, &read, NULL)
```

Key facts confirmed by disasm:
1. **Single flat block** — exactly one `NtWriteFile` / `NtReadFile` call, no loop, no chunking, no compression or encoding.
2. **File offset 0 = `base_address` byte 0** — `ByteOffset = 0` in the Nt-call.
3. **No extra header prefix** — the success gate `bytes_written == size` enforces the file is exactly `param_3` bytes. A prepended header would break this equality; it would never pass. The file begins directly at `base_address`.
4. **Size passes through verbatim** — `param_3` reaches `NtWriteFile.Length` with no arithmetic.

Caller (`game_state_save_core`, `src/halo/saved_games/game_state.c:129`) passes:
- `base_address = *(void**)0x4ea994` = `0x80061000` (the game state pool VA)
- `size = (void*)0x345000` = the pool size

### Allocation is deterministic (strongest evidence)

`src/halo/cache/physical_memory_map.c:14-17`:
```c
physical_memory_map_globals.game_state_base_address =
    XPhysicalAlloc(0x345000, GAME_STATE_BASE_ADDRESS - 0x80000000, 0, 4);
assert_halt(
    (unsigned long)physical_memory_map_globals.game_state_base_address ==
    GAME_STATE_BASE_ADDRESS);  // == 0x80061000
```

The `assert_halt` forces the allocation to land at exactly `0x80061000` or the engine halts at boot. This makes hardcoding the base VA safe and the VA → file-offset mapping fully deterministic.

### Pool layout

```
File offset   Virtual address   Size      Description
-----------   ---------------   --------  -----------
0x000000      0x80061000        0x305000  CPU pool (game state heap)
0x305000      0x80366000        0x040000  GPU pool
-----------   ---------------   --------  -----------
0x345000      0x803a6000        (end)     = GAME_STATE_CPU_SIZE + GPU alloc size
```

Source:
- `src/types.h:95`: `GAME_STATE_CPU_SIZE = 0x305000`
- `src/types.h:803`: `GAME_STATE_BASE_ADDRESS = 0x80061000`
- `src/halo/saved_games/game_state.c` (gpu_alloc): `(void*)(0x345000 + (int)base_address - gpu_size)` confirms GPU region follows CPU in the contiguous physical allocation

### Embedded save header (at file offset 0 = pool base)

`game_state_read_core_header` (0x1c0600) is called first by `game_state_load_core` to validate the save before restoring. It reads `0x14c` bytes from the file (= from pool base) and checksums the rest. Layout of the 0x14c-byte header at file offset 0 / VA 0x80061000:

| Pool base offset | Size | Field |
|-----------------|------|-------|
| +0x000 | 4 | checksum |
| +0x004 | 256 | scenario_name |
| +0x104 | 20 | build_version (e.g. "01.10.12.2276") |
| +0x124 | 2 | map_type |
| +0x126 | 2 | tag_checksum |
| +0x128 | 4 | map_checksum |
| +0x148 | 4 | CRC_placeholder |
| = 0x14c | | total header size |

The header is NOT a file-level prefix added by the I/O layer — it lives at pool base because the engine wrote it there before calling `game_state_write_core`.

---

## Base VA + Size Summary

| Property | Value |
|----------|-------|
| File path on Xbox | `d:\core\core.bin` |
| File size | 0x345000 bytes = 3,407,872 bytes |
| Heap start VA | 0x80061000 |
| Heap end VA | 0x803a6000 |
| VA → file offset | `file_offset = VA - 0x80061000` |
| Physical base | 0x00061000 (= `GAME_STATE_BASE_ADDRESS - 0x80000000`) |
| Low mirror VA | 0x00061000 (Xbox kernel maps both 0x80xxxxxx and 0x00xxxxxx to same physical) |

---

## Feasibility: What core.bin Covers

### In core.bin (heap region)

All game-state objects live in the heap. Key data structures (offsets from file start):

| Data | VA | File offset |
|------|----|-------------|
| Object data table ptr dereferences to object data | *0x5a8d50 → 0x800b9370 | 0x58370 |
| Actor data | *0x6325a4 → 0x8027e6e0 | 0x21d6e0 |
| `infection_swarm.json` covers | 0x800b0000–0x803c0000 | 0x4f000–0x35f000 |
| **core.bin covers** | 0x80061000–0x803a6000 | full file |

core.bin coverage is **broader** than `infection_swarm.json` (starts 0x4f000 earlier in the heap, covers the first 0x4f000 bytes that the swarm snapshot misses).

Empirical proof the harness accepts kernel-VA heap regions: `infection_swarm.json` already maps 0x800b0000+ at the kernel VA and the FUN_00038e60 differential walked real actor→swarm→component tables through it successfully. A 0x80061000-keyed core.bin region maps identically.

### NOT in core.bin (must be seeded separately)

XBE .data/.bss globals live at fixed VAs outside the heap:

| Symbol | VA | Required for |
|--------|----|-------------|
| `object_data_table_ptr` | 0x5a8d50 | object lookups |
| `actor_data_ptr` | 0x6325a4 | actor lookups |
| `game_state_globals` | 0x4ea990 | state metadata |
| `global_scenario_index` | 0x326a08 | scenario access |
| `map_type` | 0x31fa94 | map-type gating |
| rdata pages | ~0x250000, ~0x28c000 | constant tables |
| Callback table | ~0x32ea00 | |

These pointer-globals are the "index" into the heap — without them a function cannot locate the heap data it needs. They must be merged in from a separate per-map capture (existing `game_state_snapshot.py` captures them via GDB).

core.bin also contains **no code pages** — fine for leaf / `--allow-stubs` targets, but non-leaf targets needing un-stubbed callee code still require a code source.

---

## Concrete Converter Sketch

```python
#!/usr/bin/env python3
"""
corebin_to_snapshot.py — Convert d:\core\core.bin to a unicorn_diff.py --state-snapshot JSON.

Usage:
    python3 corebin_to_snapshot.py core.bin -o snapshot_corebin.json [--globals globals.json]

    --globals: a state_snapshot.json capturing XBE .data/.bss globals (from game_state_snapshot.py
               or a previous infection_swarm.json-style capture). Merged into the output.
"""

import json
import sys
import argparse

HEAP_BASE_VA = 0x80061000
HEAP_LOW_MIRROR = 0x00061000  # same physical page, low alias
HEAP_SIZE = 0x345000

def convert(corebin_path: str, globals_path: str | None) -> dict:
    with open(corebin_path, "rb") as f:
        data = f.read()
    assert len(data) == HEAP_SIZE, f"Expected 0x{HEAP_SIZE:x} bytes, got {len(data):#x}"

    regions = {}

    # Emit at BOTH VA aliases — unicorn_diff.py doesn't know which the target function uses.
    # dump_xemu_memory.py maps both mirrors for the same reason (build_full_snapshot lines 290-293).
    regions[f"0x{HEAP_BASE_VA:08x}"] = data.hex()       # kernel VA (0x80xxxxxx)
    regions[f"0x{HEAP_LOW_MIRROR:08x}"] = data.hex()    # low mirror  (0x00xxxxxx)

    if globals_path:
        with open(globals_path) as f:
            globals_snap = json.load(f)
        # Merge without overwriting heap regions
        for addr, hexdata in globals_snap.get("regions", {}).items():
            if addr not in regions:
                regions[addr] = hexdata

    return {
        "description": f"Converted from core.bin ({corebin_path})",
        "captured_at": "from-file",
        "source": "corebin_to_snapshot",
        "regions": regions,
        "arg_overrides": {}
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("corebin")
    ap.add_argument("-o", "--output", default="snapshot_corebin.json")
    ap.add_argument("--globals", default=None,
                    help="State snapshot JSON with XBE .data/.bss global pages")
    args = ap.parse_args()

    snap = convert(args.corebin, args.globals)

    with open(args.output, "w") as f:
        json.dump(snap, f, indent=2)
    print(f"Wrote {args.output} ({len(snap['regions'])} regions)")

if __name__ == "__main__":
    main()
```

Usage:
```bash
# Obtain core.bin from Xbox via XBDM:
# 1. In-game: exec core_save "default" in init.txt or HaloScript console
# 2. Download: python3 tools/xbox/xbdm_get.py d:\core\core.bin core.bin

# Optional: capture globals with game_state_snapshot.py (once per map, reusable):
# python3 tools/equivalence/game_state_snapshot.py -o globals_a10.json

# Convert:
python3 tools/equivalence/corebin_to_snapshot.py core.bin \
    --globals globals_a10.json \
    -o artifacts/snapshot_corebin_a10.json

# Run equivalence:
rtk python3 tools/equivalence/unicorn_diff.py <target> \
    --allow-stubs --seeds 100 \
    --state-snapshot artifacts/snapshot_corebin_a10.json
```

---

## Caveats and Unverified Items

### Confirmed
- Flat-dump format: binary-confirmed via Ghidra decompile of 0x1c0570 + 0x1c0680 (2026-06-24)
- Base VA determinism: `assert_halt` at `physical_memory_map.c:15` proves 0x80061000 or halt
- Pool size 0x345000: `physical_memory_allocate` passes this literal; `game_state_read_header_from_persistent_storage` checksums exactly this many bytes
- Kernel-VA mapping works in unicorn harness: empirically proven by `infection_swarm.json` use

### Not directly verified (no real core.bin in repo; not running xemu)
- **Byte-level validation:** No real `core.bin` was byte-inspected. Confirmation: read file offset `+0x104`, expect build string like `"01.10.12.2276"`; offset `+0x4`, expect scenario name. Do this after first capture.
- **Global pointer stability across cores:** Pointer values in .data/.bss globals (0x5a8d50 etc.) may differ between core.bin saves of different map loads. If stable (fixed alloc order per map), global pages are a one-time capture reusable across all cores of the same map. Needs confirmation.
- **GPU pool content relevance:** GPU pool [file 0x305000, 0x345000) = VAs [0x80366000, 0x803a6000). Whether any lifted function reads GPU-pool data is unknown. Most AI/gameplay functions use CPU pool only.
- **Low-mirror alias correctness:** Xbox kernel does map 0x00061000 = 0x80061000 (same physical). On Cerbios dev debug BIOS this may or may not be the case for all pages. Emit both aliases conservatively; Unicorn ignores regions that are never accessed.

### Limitations vs full xemu capture
- No code pages (by design — xemu captures include them; use `--allow-stubs` to compensate)
- No tag cache, texture cache, or sound cache pages (separate physical allocations at different VAs)
- Globals must still come from a separate GDB/QMP capture
- core.bin captures a *paused* game state; dynamic fields (timers, animation frames) are frozen at save time, same as any snapshot

---

## Comparison with Existing Workflows

| Method | Heap coverage | Globals | Reliability | Requires live xemu |
|--------|--------------|---------|-------------|-------------------|
| `infection_swarm.json` | 0x800b0000–0x803c0000 (partial) | ✓ (included) | proven | captured once |
| `game_state_snapshot.py` (GDB+QMP) | full heap | ✓ | finicky | yes |
| **core.bin converter** | 0x80061000–0x803a6000 (full) | ✗ (separate) | deterministic file | no |

core.bin is the cleanest path when: (a) a saved state is available, (b) the target operates on game-state heap data, (c) running xemu for a fresh capture is inconvenient.
