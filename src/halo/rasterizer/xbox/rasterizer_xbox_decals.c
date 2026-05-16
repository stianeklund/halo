/*
 * rasterizer_xbox_decals.c
 *
 * Rasterizer decal subsystem: D3D vertex buffer allocation, LRUV vertex
 * cache lifecycle, and per-map init/dispose.
 *
 * Source path (from binary):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_decals.c
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  void *  – global_d3d_device (IDirect3DDevice8 pointer)
 *   0x476ad8  void *  – local_d3d_vertex_buffer (12-byte D3D VB struct)
 *   0x476adc  void *  – local_vertex_cache (lruv_cache handle)
 *   0x476ae0  bool    – locked-decal eviction warning flag (one-shot)
 *   0x476ae1  bool    – permanent-decal eviction warning flag (one-shot)
 *   0x5aa8b8  void *  – global_decal_data (decal data array pointer)
 *   0x32516c  int     – most-recently-queried decal index (debug display)
 */

/* Forward declarations for static callbacks passed to lruv_cache_new */
static void rasterizer_decals_vertex_cache_delete(int decal_index);
static int rasterizer_decals_vertex_cache_query(int decal_index);

/* 0x15b190
 *
 * rasterizer_decals_initialize_for_new_map
 *
 * No-op in this build: the vertex cache is persistent across maps and does
 * not need per-map reinitialization.
 */
void rasterizer_decals_initialize_for_new_map(void)
{
  return;
}

/* 0x15b1a0
 *
 * rasterizer_decals_dispose_from_old_map
 *
 * Asserts the LRUV vertex cache exists, then clears all per-map decal state:
 *   1. Calls decals_update_for_new_map(true) to strip lock/permanent flags
 *      from every live decal datum and reset the global lock/permanent counts.
 *   2. Calls lruv_cache_dispose_all() to evict all cached vertex entries.
 */
void rasterizer_decals_dispose_from_old_map(void)
{
  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x83, 1);
    system_exit(-1);
  }
  decals_update_for_new_map(1);
  lruv_cache_dispose_all(*(void **)0x476adc);
}

/* 0x15b1e0
 *
 * rasterizer_decals_dispose
 *
 * Asserts the LRUV vertex cache exists, resets decal state (non-full reset),
 * and evicts all cached vertex entries.
 */
void FUN_0015b1e0(void)
{
  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x8e, 1);
    system_exit(-1);
  }
  decals_update_for_new_map(0);
  lruv_cache_dispose_all(*(void **)0x476adc);
}

/* 0x15b460
 *
 * Allocate vertex cache space for decal vertices.
 * Asserts that cache_size exceeds sizeof(struct decal_vertex) (0x10 bytes)
 * and is aligned to that size, then forwards to the LRUV cache allocator
 * (FUN_0011de10). Returns the allocated block index, or NONE on failure. */
int FUN_0015b460(uint32_t cache_size)
{
  if (cache_size <= 0x10) {
    display_assert(
      "cache_size>sizeof(struct decal_vertex)",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xcc, 1);
    system_exit(-1);
  }

  if ((cache_size & 0xf) != 0) {
    display_assert(
      "cache_size%sizeof(struct decal_vertex)==0",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xcd, 1);
    system_exit(-1);
  }

  {
    int (*lruv_cache_allocate)(void *, uint32_t) = (void *)0x11de10;
    return lruv_cache_allocate(*(void **)0x476adc, cache_size);
  }
}

/* 0x15b530
 *
 * Removes one decal vertex-cache entry from the LRUV cache.
 * Asserts valid decal index and cache handle before forwarding to the
 * lruv-cache block removal helper.
 */
void FUN_0015b530(int decal_index)
{
  if (decal_index == -1) {
    display_assert(
      "cache_index!=NONE",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x106, 1);
    system_exit(-1);
  }

  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x107, 1);
    system_exit(-1);
  }

  lruv_block_delete(*(void **)0x476adc, decal_index);
}

/* 0x15afa0
 *
 * rasterizer_decals_vertex_cache_delete  (LRUV eviction callback)
 *
 * Called by the LRUV cache when a cached decal vertex block is evicted.
 * Validates decal_index is neither 0 nor NONE (-1), then:
 *   - Warns once if the decal is still locked when evicted.
 *   - Warns once if the decal is still permanent when evicted.
 *   - Calls decal_delete (0x9a160) to free the underlying decal datum.
 */
static void rasterizer_decals_vertex_cache_delete(int decal_index)
{
  char cVar1;
  char *decal;
  char *pcVar3;
  int uVar4;

  /* lruv_has_locked_proc (0x11d4f0): asserts the cache has a lock query cb */
  cVar1 = ((char (*)(void *))0x11d4f0)(*(void **)0x476adc);
  if (cVar1 == '\0') {
    display_assert(
      "lruv_has_locked_proc(local_vertex_cache)",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x1d, 1);
    system_exit(-1);
  }

  if (decal_index == 0) {
    display_assert(
      "decal_index",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x1e, 1);
    system_exit(-1);
    uVar4 = 0x20;
    pcVar3 = "decal_index!=0";
  } else {
    if (decal_index != -1)
      goto LAB_0015b018;
    uVar4 = 0x1f;
    pcVar3 = "decal_index!=NONE";
  }
  display_assert(pcVar3,
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c",
                 uVar4, 1);
  system_exit(-1);

LAB_0015b018:
  decal =
    (char *)datum_absolute_index_to_index(*(void **)0x5aa8b8, decal_index);
  if (decal == 0) {
    error(2,
          "### ERROR decals: stale cache->decal handle (#%d) in rasterizer -- "
          "tell Bernie!!",
          decal_index);
    return;
  }

  if (((*(unsigned char *)(decal + 2) & 1) != 0) &&
      (*(char *)0x476ae0 == '\0')) {
    error(2,
          "### ERROR decals: deleting locked decal (#%d, queried=#%d) in "
          "rasterizer -- tell Bernie!!",
          decal_index, *(int *)0x32516c);
    *(char *)0x476ae0 = '\x01';
  }

  if (((*(unsigned char *)(decal + 2) & 2) != 0) &&
      (*(char *)0x476ae1 == '\0')) {
    error(2,
          "### ERROR decals: deleting permanent decal (#%d, queried=#%d) in "
          "rasterizer -- tell Bernie!!",
          decal_index, *(int *)0x32516c);
    *(char *)0x476ae1 = '\x01';
  }
  /* 0x9a160: decal_delete(int index) — not yet in kb.json */
  ((void (*)(int))0x9a160)(decal_index);
}

/* 0x15b0c0
 *
 * rasterizer_decals_vertex_cache_query  (LRUV locked-probe callback)
 *
 * Called by the LRUV cache to check whether a cached decal may be evicted.
 * Returns 1 (locked / not evictable) if the decal datum has either the
 * locked (bit 0) or permanent (bit 1) flag set; 0 otherwise.
 * Also records decal_index into 0x32516c for debug display.
 */
static int rasterizer_decals_vertex_cache_query(int decal_index)
{
  char *iVar1;
  char *pcVar2;
  int uVar3;

  if (decal_index == 0) {
    display_assert(
      "decal_index",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x47, 1);
    system_exit(-1);
    uVar3 = 0x49;
    pcVar2 = "decal_index!=0";
  } else {
    if (decal_index != -1)
      goto LAB_0015b105;
    uVar3 = 0x48;
    pcVar2 = "decal_index!=NONE";
  }
  display_assert(pcVar2,
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c",
                 uVar3, 1);
  system_exit(-1);

LAB_0015b105:
  iVar1 = datum_get(*(void **)0x5aa8b8, decal_index);
  *(int *)0x32516c = decal_index;
  if ((*(unsigned char *)(iVar1 + 2) & 3) == 0) {
    return 0;
  }
  return 1;
}

/* 0x15b5e0
 *
 * Sets the appropriate rasterizer blend/render state for decals based on
 * the current decal rendering mode (global at 0x476ac8). If mode is 3
 * (shadow), calls FUN_00158ae0 to set up shadow state first. Then selects
 * a texture stage configuration from a local lookup table indexed by mode.
 */
void FUN_0015b5e0(void)
{
  short mode;
  short stage_table[5];

  mode = *(short *)0x476ac8;
  if (mode == 3) {
    FUN_00158ae0(2);
    mode = *(short *)0x476ac8;
  }
  stage_table[0] = 9;
  stage_table[1] = 10;
  stage_table[2] = 6;
  stage_table[3] = 7;
  stage_table[4] = 0x14;
  if (mode >= 0 && mode < 5) {
    FUN_0016fa40((int)stage_table[mode]);
  }
}

/* 0x15b6d0
 *
 * rasterizer_decals_initialize
 *
 * Allocates and registers the D3D decal vertex buffer, then creates the
 * LRUV vertex cache that maps decal indices to vertex-buffer ranges.
 *
 * The vertex buffer struct (12 bytes, from debug_malloc) layout:
 *   +0x0  uint32_t  D3D resource header word (ref-count/type), set to 1
 *   +0x4  void *    GPU memory pointer (game_state_gpu_alloc, 0x28000 bytes)
 *   +0x8  uint32_t  lock flags, set to 0
 *
 * Derived from disassembly:
 *   MOV dword ptr [EAX],     0x1   -> [+0x0]
 *   LEA ESI, [EAX+4];  MOV [ESI], EAX_gpu -> [+0x4]
 *   MOV dword ptr [EAX+0x8], 0x0  -> [+0x8]
 *
 * lruv_cache_new args: (name, capacity=0xa00, max_locked=6,
 *                       entry_size=0x800, delete_cb, query_cb)
 */
void rasterizer_decals_initialize(void)
{
  void *puVar1;
  void *iVar2;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x59, 1);
    system_exit(-1);
  }

  /* Allocate 12-byte D3D vertex buffer descriptor */
  puVar1 = debug_malloc(
    0xc, 0, "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c",
    0x5b);
  *(void **)0x476ad8 = puVar1;

  if (puVar1 == 0) {
    display_assert(
      "local_d3d_vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x5c, 1);
    system_exit(-1);
    /* NOTE: original continues below after assert (EAX re-loaded at 0x15b72c)
     * to set the first field in the struct - preserved as-is */
    puVar1 = *(void **)0x476ad8;
  }

  /* Set D3D resource header word to 1 at +0x0 */
  *(unsigned int *)puVar1 = 1;

  /* Allocate 0x28000 bytes of GPU memory for "decal vertices" */
  iVar2 = game_state_gpu_alloc("decal vertices", 0, 0x28000);

  /* Store GPU pointer at +0x4 in the vertex buffer struct */
  *(void **)((char *)puVar1 + 0x4) = iVar2;

  /* Clear lock flags at +0x8 */
  *(unsigned int *)((char *)*(void **)0x476ad8 + 0x8) = 0;

  if (iVar2 == 0) {
    display_assert(
      "local_d3d_vertex_buffer->Data",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x60, 1);
    system_exit(-1);
  }

  /* Register the vertex buffer with D3D (stdcall, 2 args) */
  D3DResource_Register(*(void **)0x476ad8, 0);

  /* Create the LRUV vertex cache */
  *(void **)0x476adc = lruv_cache_new("decal vertex cache", 0xa00, 6, 0x800,
                                      rasterizer_decals_vertex_cache_delete,
                                      rasterizer_decals_vertex_cache_query);

  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x6a, 1);
    system_exit(-1);
  }
}

/* 0x15b7e0
 *
 * rasterizer_decals_dispose
 *
 * Tears down the decal rasterizer subsystem:
 *   1. Asserts vertex cache, vertex buffer, and D3D device are all non-null.
 *   2. Releases the D3D vertex buffer if non-null, then clears the pointer.
 *   3. Disposes the LRUV vertex cache (frees all entries and cache struct).
 */
void rasterizer_decals_dispose(void)
{
  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x99, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ad8 == 0) {
    display_assert(
      "local_d3d_vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x9a, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x9b, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ad8 != 0) {
    D3DResource_Release(*(void **)0x476ad8);
    *(void **)0x476ad8 = 0;
  }
  lruv_cache_dispose(*(void **)0x476adc);
}

/* 0x15b890
 *
 * Lock a region of the decal vertex buffer for writing.
 * Asserts that cache_index, vertex cache, and D3D device are all valid.
 * Translates the LRUV cache block index into a byte offset via
 * lruv_block_get_address, then locks that region of the D3D vertex buffer. Sets
 * the GPU lock flag (0x325652) to 5 during the lock operation, clears it
 * afterward. Returns a pointer to the locked vertex buffer memory. */
void *FUN_0015b890(int cache_index, uint32_t cache_size)
{
  uint32_t offset;
  void *locked_data;

  locked_data = 0;

  if (cache_index == -1) {
    display_assert(
      "cache_index!=NONE",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xd9, 1);
    system_exit(-1);
  }

  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xda, 1);
    system_exit(-1);
  }

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xdb, 1);
    system_exit(-1);
  }

  {
    uint32_t (*lruv_cache_block_offset)(void *, int) = (void *)0x11da00;
    offset = lruv_cache_block_offset(*(void **)0x476adc, cache_index);
  }

  *(uint16_t *)0x325652 = 5;
  ((void(__stdcall *)(void *, uint32_t, uint32_t, void **, uint32_t))0x1ef100)(
    *(void **)0x476ad8, offset, cache_size, &locked_data, 0x80);
  *(uint16_t *)0x325652 = 0;

  return locked_data;
}
