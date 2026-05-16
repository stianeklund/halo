# Texture Cache Corruption Investigation

## Symptom

Severe texture corruption during gameplay: horizontal banding with shifted
color channels (green/orange noise). Geometry renders at correct positions with
correct transforms — only the texture data sampled onto surfaces is garbled.
HUD elements (ammo counter, health bar, reticle, zoom indicator) render
correctly because they use a separate 2D overlay path.

**Key observation (split-screen, 2026-05-12):** Bottom viewport renders
perfectly while the top viewport is fully corrupted. This rules out global GPU
state corruption and points to per-view or per-cache-reference corruption —
something that runs between the two viewport render passes is stomping texture
cache state.

### Screenshots

- `xemu-2026-05-14-13-28-30.png` — Single-player zoomed scope, full corruption
- `xemu-2026-05-12-21-31-55.png` — Split-screen, top viewport corrupt / bottom clean

## Hypothesis

Corruption originates in the texture cache init or lookup path, not the
geometry pipeline. Candidate causes:

1. **ABI mismatch at ported/unported boundary.** A ported function passes data
   (base address, size, format code, stride) to an unported callee (or vice
   versa) with a subtle struct offset, calling convention, or register argument
   error — causing D3D texture descriptors to reference wrong memory or use
   wrong format/stride values.

2. **Physical memory layout divergence.** `physical_memory_map.obj` is fully
   ported (3/3). If our `physical_memory_allocate()` produces different base
   addresses or alignment than the original, every downstream cache consumer
   inherits the error.

3. **LRU-V cache state corruption.** `lruv_cache.obj` is partially ported
   (10/22). A bug in a ported LRU function (e.g. wrong page index, off-by-one
   in block list) could hand out stale or overlapping cache blocks.

4. **Per-viewport texture cache reset bug.** The split-screen evidence suggests
   a function that re-initializes or adjusts texture cache state per viewport
   is either missing, misordered, or corrupting a pointer. The first viewport
   renders fine; the second inherits trashed state.

## Ported Coverage of Cache-Adjacent Objects

| Object                     | Ported | Total | Coverage |
|----------------------------|--------|-------|----------|
| `physical_memory_map.obj`  | 3      | 3     | 100%     |
| `bitmaps.obj`              | 16     | 31    | 52%      |
| `lruv_cache.obj`           | 10     | 22    | 45%      |
| `cache_files_windows.obj`  | 8      | 38    | 21%      |
| `bitmap_utilities.obj`     | 3      | 12    | 25%      |
| `xbox_texture_cache.obj`   | 2      | 21    | 10%      |
| `predicted_resources.obj`  | 2      | 8     | 25%      |
| `cache_files.obj`          | 0      | 8     | 0%       |
| `xbox_sound_cache.obj`     | 0      | 9     | 0%       |

The highest-risk boundary is `physical_memory_map.obj` (100% ported) feeding
into `xbox_texture_cache.obj` (10% ported) and `lruv_cache.obj` (45% ported).

## Investigation Approach: Differential Cache Init Trace

Per contributor advice: "compare cache init paths through both executables."

### What This Means

Run the same cache initialization sequence through the **original unpatched
XBE** and the **patched XBE**, recording key state at each step, then diff
to find the divergence point.

### Phase 1 — Map the Init Chain

Trace the full initialization sequence in Ghidra on the original XBE:

```
physical_memory_allocate()
  → XPhysicalAlloc() for tag cache, texture cache, sound cache, game state
  → Records base addresses and sizes in globals

lruv_cache_initialize()
  → Sets up block list, page map, eviction callbacks
  → Receives base address + size from physical_memory_allocate() output

xbox_texture_cache_*() setup
  → steal_memory() — allocates pages from LRU-V cache with guard regions
  → setup_d3d_texture() — builds D3D texture descriptors from hardware format
  → Reads format tables at 0x2b9618 (linear) and 0x2b9660 (swizzled)

Per-frame texture request path
  → xbox_texture_cache_request() / get_hardware_format()
  → Resolves tag reference → cache block → D3D descriptor
```

Document the memory addresses, sizes, format codes, and struct field values
at each stage.

### Phase 2 — Diff Ported Implementations

For each ported function in the chain, compare against the original disassembly:

- **`physical_memory_allocate()`** — Verify base addresses, sizes, alignment
  flags passed to `XPhysicalAlloc()`. A wrong alignment or size propagates to
  every downstream consumer.
- **`lruv_cache_initialize()`** — Verify block count calculation, page size,
  linked list setup. Off-by-one here causes overlapping cache blocks.
- **Ported `bitmaps.obj` functions** — Verify struct field offsets used to pass
  bitmap metadata to the texture cache. Ghidra struct-field-rotation hazard
  (CLAUDE.md hazard #3) applies here.
- **Ported `lruv_cache.obj` functions** — Verify page allocation/deallocation
  logic, especially boundary conditions.

### Phase 3 — Investigate the Split-Screen Path

The per-viewport corruption is the strongest lead. Investigate:

1. What function runs between the two viewport render passes?
2. Does it reset or adjust texture cache pointers?
3. Is there a render target switch that changes the texture cache base?
4. Look at `render_cameras.obj` (3/21 ported) — does a ported camera setup
   function touch texture cache globals?

Candidate areas:
- Render camera setup per viewport
- Rasterizer target switching
- Texture cache flush/invalidate between viewports

### Phase 4 — Runtime Verification

Once a candidate divergence is found:

1. **Bisect with `ported: false`** — Disable suspect ported functions one at a
   time. If corruption disappears, that function (or its interaction with
   neighbors) is the cause.
2. **XBDM memory inspection** — On real hardware, dump texture cache memory
   regions at the corruption point and compare against expected bitmap data.
3. **xemu breakpoint** — Set a breakpoint on `xbox_texture_cache_setup_d3d_texture`
   and inspect the D3D descriptor fields (format, size, pitch, base address)
   for the corrupt viewport vs. the clean one.

## Key Global Addresses

| Address    | Purpose                                      |
|------------|----------------------------------------------|
| 0x4e9220   | Cache precache progress                      |
| 0x4e9250   | Precache map name                            |
| 0x4ea980   | Texture cache instance                       |
| 0x4ea98c   | Stolen memory flag                           |
| 0x2b9618   | D3D linear format table                      |
| 0x2b9660   | D3D swizzled format table                    |

## Files to Read

- `src/halo/cache/physical_memory_map.c` — fully ported, start here
- `src/halo/cache/xbox_texture_cache.c` — partially ported, check boundary
- `src/halo/cache/lruv_cache.c` — partially ported, check page logic
- `src/halo/bitmaps/bitmaps.c` — partially ported, check format/offset fields
- `src/halo/cache/predicted_resources.c` — precache triggers
