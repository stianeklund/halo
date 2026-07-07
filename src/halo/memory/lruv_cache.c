/* Store a short value through a pointer.
 * 0x11c780 / lruv_cache.obj
 */
void FUN_0011c780(short *param_1, short param_2)
{
  *param_1 = param_2;
}

/* Store 0xffff into a short through a pointer.
 * 0x11c790 / lruv_cache.obj
 */
void FUN_0011c790(short *param_1)
{
  *param_1 = (short)0xffff;
}

/* 0x11c870: Allocate and initialize an lrar_cache.
 * Reserves the 0x48-byte cache header (debug_malloc) plus a block_count*0x10
 * block array, validates the address-range / alignment / boundary / block-count
 * invariants (each a display_assert + system_exit(-1) on failure), rounds the
 * minimum address up to the alignment_bit granularity, installs the caller's
 * lock/unlock callbacks (or the default FUN_0011c780/FUN_0011c790 pair when
 * either is NULL), fills the header fields, stamps the 'lrar' signature at
 * +0x44, and refreshes the function-pointer table before returning the cache.
 * Returns NULL if the block-array allocation fails (frees the header first).
 * Source: c:\halo\SOURCE\memory\lrar_cache.c */
void *lrar_cache_new(const char *name, unsigned int minimum_address,
                     unsigned int maximum_address, short block_count,
                     short alignment_bit, short boundary_bit,
                     void (*lock_proc)(short *, short),
                     void (*unlock_proc)(short *))
{
  char *cache;
  unsigned int alignment_mask;
  void *blocks;

  cache = (char *)debug_malloc(0x48, 0,
                               "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x56);

  if (maximum_address <= minimum_address) {
    display_assert("minimum_address<maximum_address",
                   "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x58, 1);
    system_exit(-1);
  }

  alignment_mask = (1 << ((unsigned char)alignment_bit & 0x1f)) - 1;
  if ((minimum_address & alignment_mask) != 0) {
    minimum_address = (alignment_mask | minimum_address) + 1;
  }

  if (lock_proc == 0 || unlock_proc == 0) {
    lock_proc = FUN_0011c780;
    unlock_proc = FUN_0011c790;
  }

  if (alignment_bit < 0) {
    display_assert("alignment_bit>=0", "c:\\halo\\SOURCE\\memory\\lrar_cache.c",
                   0x66, 1);
    system_exit(-1);
  }
  if (boundary_bit != -1 && boundary_bit < 0) {
    display_assert("boundary_bit==NONE || boundary_bit>=0",
                   "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x67, 1);
    system_exit(-1);
  }
  if (block_count < 1) {
    display_assert("block_count>0", "c:\\halo\\SOURCE\\memory\\lrar_cache.c",
                   0x68, 1);
    system_exit(-1);
  }

  if (cache != 0) {
    blocks = debug_malloc((int)block_count << 4, 0,
                          "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x6c);
    if (blocks == 0) {
      debug_free(cache, "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x8a);
      return 0;
    }
    csmemset(cache, 0, 0x48);
    csmemset(blocks, 0, (int)block_count << 4);
    csstrncpy(cache, name, 0x1f);
    *(unsigned int *)(cache + 0x24) = minimum_address;
    *(unsigned int *)(cache + 0x28) = maximum_address;
    *(short *)(cache + 0x22) = boundary_bit;
    *(unsigned short *)(cache + 0x34) = 0xffff;
    *(unsigned short *)(cache + 0x36) = 0xffff;
    *(short *)(cache + 0x20) = alignment_bit;
    *(void (**)(short *, short))(cache + 0x3c) = lock_proc;
    *(unsigned char *)(cache + 0x1f) = 0;
    *(unsigned int *)(cache + 0x2c) = maximum_address - minimum_address;
    *(void **)(cache + 0x30) = blocks;
    *(short *)(cache + 0x38) = block_count;
    *(void (**)(short *))(cache + 0x40) = unlock_proc;
    *(unsigned int *)(cache + 0x44) = 0x6c726172;
    lruv_update_function_pointers((int)cache);
  }

  return cache;
}

/* 0x11ca20: Dispose of an lrar_cache. Refreshes the cache's function-pointer
 * table (lruv_update_function_pointers, cache passed in EAX), then frees the
 * block sub-buffer stored at cache+0x30 and finally the cache header itself,
 * each via debug_free. Asserts against c:\halo\SOURCE\memory\lrar_cache.c
 * lines 0x98/0x99. NOTE: the kb placeholder name "lruv_has_locked_proc" is a
 * stale misnomer; this routine is a destructor, not a predicate. Disasm:
 * mov esi,[ebp+8]; mov eax,esi; call 0x11c820; mov eax,[esi+0x30];
 * push 0x98; push <file>; push eax; call debug_free; push 0x99; push <file>;
 * push esi; call debug_free. */
void lruv_has_locked_proc(void *cache)
{
  lruv_update_function_pointers((int)cache);
  debug_free(*(void **)((char *)cache + 0x30),
             "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x98);
  debug_free(cache, "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x99);
}

/* 0x11cf00: lrar_cache block getter. Returns the data pointer stored at
 * offset 8 of block_array[block_index]. This belongs to the sibling
 * "lrar_cache" (asserts against c:\halo\SOURCE\memory\lrar_cache.c) that
 * shares this TU with the lruv_cache; its header layout differs from
 * lruv_cache_t (cache+0x30 = block array base ptr, cache+0x38 = int16
 * block_count, block entry stride = 0x10), so raw offset access is used to
 * avoid conflating the two structures. FUN_0011c820 refreshes the function
 * pointers on entry and FUN_0011c7c0 is the matching helper before return.
 */
void *FUN_0011cf00(int cache, short block_index)
{
  int block_array;

  lruv_update_function_pointers(cache);
  if ((block_index < 0) || (*(short *)(cache + 0x38) <= block_index)) {
    display_assert("block_index>=0 && block_index<cache->block_count",
                   "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x16e, true);
    system_exit(-1);
  }
  block_array = *(int *)(cache + 0x30);
  FUN_0011c7c0(cache, block_index * 0x10 + block_array);
  return *(void **)(block_index * 0x10 + block_array + 8);
}

/* 0x11cf60: lrar_cache block free/clear. Sibling of FUN_0011cf00 (same
 * lrar_cache layout, asserts against c:\halo\SOURCE\memory\lrar_cache.c;
 * distinct from lruv_cache_t so raw offset access is used). Refreshes the
 * cache function pointers on entry (called twice, matching the original),
 * bounds-checks block_index against cache->block_count (int16 at cache+0x38,
 * signed), then reads the block's data pointer (first dword of the 0x10-byte
 * entry at cache+0x30). If non-NULL, invokes the cache free callback (cdecl
 * fn ptr at cache+0x40, one arg) with that pointer and nulls the slot.
 */
void FUN_0011cf60(int cache, short block_index)
{
  int *block_ptr;
  int data;

  lruv_update_function_pointers(cache);
  lruv_update_function_pointers(cache);
  if ((block_index < 0) || (*(short *)(cache + 0x38) <= block_index)) {
    display_assert("block_index>=0 && block_index<cache->block_count",
                   "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x16e, true);
    system_exit(-1);
  }
  block_ptr = (int *)(block_index * 0x10 + *(int *)(cache + 0x30));
  FUN_0011c7c0(cache, (int)block_ptr);
  data = *block_ptr;
  if (data != 0) {
    (*(void (**)(int))(cache + 0x40))(data);
    *block_ptr = 0;
  }
}

/* 0x11cfe0: Zero the first 4-byte field of the pointed-to record. Trivial
 * setter that clears a cache head/count word through the stack pointer arg.
 * Source: c:\halo\SOURCE\memory\lrar_cache.c */
void FUN_0011cfe0(void *param_1)
{
  *(int *)param_1 = 0;
}

/* LRU-V (Least Recently Used - Virtual) cache management.
 * Manages a block-based cache with linked list ordering, delete/query
 * callbacks, and page-granularity allocation tracking. Source:
 * c:\halo\SOURCE\memory\lruv_cache.c */

#define LRUV_CACHE_SIGNATURE 0x77656565

/* lruv_cache_t: header for an LRU-V cache allocation.
 * size = 0x44, followed immediately by an inline data_t for block datums. */
typedef struct {
  char name[0x20]; ///< offset=0x00  cache name
  void (*delete_cb)(int); ///< offset=0x20  called on block removal
  int (*query_cb)(int); ///< offset=0x24  query callback
  int page_count; ///< offset=0x28
  int page_size_bits; ///< offset=0x2c
  int field_30; ///< offset=0x30  initialized to 1
  int first_block_index; ///< offset=0x34  datum handle of first block
  int last_block_index; ///< offset=0x38  datum handle of last block
  data_t *blocks; ///< offset=0x3c  pointer to inline data_t at +0x44
  int signature; ///< offset=0x40  LRUV_CACHE_SIGNATURE
} lruv_cache_t;

/* Block datum entry within the cache's data_t.
 * size = 0x1c per datum. Fields beyond offset 0x10 are unknown. */
typedef struct {
  int16_t salt; ///< offset=0x00  datum identifier
  char pad_02[2]; ///< offset=0x02
  int page_count; ///< offset=0x04
  int first_page_index; ///< offset=0x08
  int next_block_index; ///< offset=0x0c  datum handle of next block
  int previous_block_index; ///< offset=0x10  datum handle of previous block
  char unk_14[8]; ///< offset=0x14
} lruv_cache_block_t;

/* 0x11d010: lru_cache block-header integrity validator.
 * lruv_cache.obj (asserts against c:\halo\SOURCE\memory\lru_cache.c line
 * 0x156=342). cdecl(cache, entry). Verified against disassembly.
 *
 * Passes silently when the block header is intact:
 *   - signature word at entry+4, low bit (used/free flag) masked off, equals
 *     0x55626c6a,
 *   - the next-block link at entry+0xc is 0,
 *   - the byte offset (entry - cache+0x34) is >= 0 and cache+0x24 plus that
 *     offset does not exceed cache+0x28 (block lies inside the data region),
 * and
 *   - the page/count field at entry+8 is (unsigned) below cache+0x3c.
 * On any failure it formats the "appears to be corrupt" message into the shared
 * scratch buffer at 0x5ab100 and hits display_assert + system_exit(-1). Note
 * the assert nesting: the file/line/halt args (line 0x156, halt 1) belong to
 * display_assert and are pushed before csprintf's 5 args (Ghidra cdecl arg
 * mis-grouping); the trailing ADD ESP,0x14 confirms the split.
 *
 * The call inside the OK branch is a literal CALL 0x11d010 (self-address) with
 * cdecl args (cache, entry) — verified at 0x11d031. A block that passes the
 * header checks re-satisfies them on re-entry, so this cannot be functional
 * recursion; it is MSVC /OPT:ICF COMDAT folding of a byte-identical sibling
 * validator onto this address. Reproduced as a self-call to match the bytes. */
void FUN_0011d010(int cache, void *entry)
{
  int offset;

  if (((*(unsigned int *)((char *)entry + 4) & 0xfffffffe) == 0x55626c6a) &&
      (*(int *)((char *)entry + 0xc) == 0)) {
    FUN_0011d010(cache, entry);
    offset = (int)entry - *(int *)(cache + 0x34);
    if ((offset >= 0) &&
        (*(int *)(cache + 0x24) + offset <= *(int *)(cache + 0x28)) &&
        (*(unsigned int *)((char *)entry + 8) <
         *(unsigned int *)(cache + 0x3c))) {
      return;
    }
  }
  display_assert(csprintf((char *)0x5ab100,
                          "lru cache %s @%p block @%p appears to be corrupt",
                          cache, cache, entry),
                 "c:\\halo\\SOURCE\\memory\\lru_cache.c", 0x156, 1);
  system_exit(-1);
}

/* 0x11d110: Allocate and initialize an lru_cache. Rounds the per-element size
 * (block_size + 0x10 block header) up to a 4-byte multiple, derives the
 * element count from total_size, and allocates the 0x48-byte cache header. If
 * the caller supplies no backing buffer, allocates the data buffer itself and
 * records ownership in the byte at +0x38 (freed by lru_cache_dispose). NULL
 * callbacks default to the file-local stubs FUN_0011cfd0 / FUN_0011cfe0.
 * Writes the 'curl' signature (0x6c727563) at +0x44 and copies the name into
 * name[0x20] at +0 (NUL-terminated at +0x1f). Runs the pointer-refresh helper
 * (FUN_0011d090, cache in @eax) before returning the cache pointer, or 0 if
 * the data-buffer allocation failed. Both assert-fail paths halt via
 * halt_and_catch_fire. Source: c:\halo\SOURCE\memory\lru_cache.c */
void *FUN_0011d110(const char *name, int total_size, int block_size,
                   void *delete_callback, void *query_callback, void *buffer)
{
  char *cache;
  unsigned int element_size;
  int element_count;
  char owns_buffer;

  if (block_size < 0) {
    display_assert("block_size>=0", "c:\\halo\\SOURCE\\memory\\lru_cache.c",
                   0x5e, 1);
    halt_and_catch_fire();
  }
  if (total_size < block_size) {
    display_assert("total_size>=block_size",
                   "c:\\halo\\SOURCE\\memory\\lru_cache.c", 0x5f, 1);
    halt_and_catch_fire();
  }
  if ((delete_callback == (void *)0) || (query_callback == (void *)0)) {
    delete_callback = (void *)FUN_0011cfd0;
    query_callback = (void *)FUN_0011cfe0;
  }
  element_size = (unsigned int)block_size + 0x10;
  if ((element_size & 3) != 0) {
    element_size = (element_size | 3) + 1;
  }
  element_count = total_size / (int)element_size;
  cache = (char *)debug_malloc(0x48, 0, "c:\\halo\\SOURCE\\memory\\lru_cache.c",
                               0x6e);
  if (cache != (char *)0) {
    owns_buffer = 0;
    if (buffer == (void *)0) {
      buffer = debug_malloc(element_count * element_size, 0,
                            "c:\\halo\\SOURCE\\memory\\lru_cache.c", 0x75);
      owns_buffer = 1;
      if (buffer == (void *)0) {
        debug_free(cache, "c:\\halo\\SOURCE\\memory\\lru_cache.c", 0x8e);
        return (void *)0;
      }
    }
    csmemset(cache, 0, 0x48);
    *(int *)(cache + 0x20) = element_count;
    *(void **)(cache + 0x2c) = delete_callback;
    *(void **)(cache + 0x34) = buffer;
    *(int *)(cache + 0x3c) = 0;
    *(int *)(cache + 0x40) = 0;
    *(unsigned int *)(cache + 0x24) = element_size;
    *(unsigned int *)(cache + 0x28) =
      (unsigned int)element_count * element_size;
    *(unsigned int *)(cache + 0x44) = 0x6c727563;
    *(void **)(cache + 0x30) = query_callback;
    *(unsigned char *)(cache + 0x38) = (unsigned char)owns_buffer;
    csstrncpy(cache, name, 0x1f);
    *(char *)(cache + 0x1f) = 0;
    FUN_0011d090((int)cache);
  }
  return cache;
}

/* 0x11d250: Dispose of an lru_cache. Runs the teardown helper
 * (FUN_0011d090), frees the backing buffer stored at cache+0x34 when the
 * ownership flag byte at cache+0x38 is set, then frees the cache struct
 * itself. debug_free line args 0x9c/0x9d are the MSVC debug-allocator
 * call-site tracking. Source: c:\halo\SOURCE\memory\lru_cache.c */
void lru_cache_dispose(void *cache)
{
  FUN_0011d090((int)cache);
  if (*(char *)((char *)cache + 0x38) != '\0') {
    debug_free(*(void **)((char *)cache + 0x34),
               "c:\\halo\\SOURCE\\memory\\lru_cache.c", 0x9c);
  }
  debug_free(cache, "c:\\halo\\SOURCE\\memory\\lru_cache.c", 0x9d);
}

/* 0x11d2a0: Flush all cached entries. Runs the teardown helper
 * (FUN_0011d090, cache in @eax), then iterates the fixed-stride element
 * array whose base is at cache+0x34, count at cache+0x40, element stride at
 * cache+0x24. For each element it calls FUN_0011d010(cache, entry) and then
 * the per-entry callback (cdecl fn ptr at cache+0x30) with the first dword
 * of the element (*entry). Finally resets the element count at cache+0x40 to
 * zero. Raw offset access matches the sibling cache accessors in this TU
 * (lruv_cache_t offsets 0x24/0x30/0x40 differ from this cache header). */
void FUN_0011d2a0(int cache)
{
  int index;
  int *entry;

  FUN_0011d090(cache);
  entry = *(int **)(cache + 0x34);
  index = 0;
  if (*(int *)(cache + 0x40) < 1) {
    *(int *)(cache + 0x40) = 0;
    return;
  }
  do {
    FUN_0011d010(cache, entry);
    (*(void (**)(int))(cache + 0x30))(*entry);
    index = index + 1;
    entry = (int *)((int)entry + *(int *)(cache + 0x24));
  } while (index < *(int *)(cache + 0x40));
  *(int *)(cache + 0x40) = 0;
}

/* 0x11d3f0: lru_cache entry commit helper.
 * lruv_cache.obj (asserts against c:\halo\SOURCE\memory\lru_cache.c, line
 * 0x156).
 *
 * Refreshes the cache's function pointers (FUN_0011d090, cache passed in EAX),
 * validates the block-header integrity (FUN_0011d010, which checks magic
 * 0x55626c6a and asserts "lru cache ... appears to be corrupt"), then sets
 * bit 0 (valid/in-use flag) in the block-header status word.
 *
 * param_1 (cache) = lru cache ptr. param_2 (block) = pointer just past a block
 * header; header fields live at block-0x10 (block base, passed to the
 * validator) and block-0xc (status word that receives the |1).
 */
void FUN_0011d3f0(int cache, int block)
{
  FUN_0011d090(cache);
  FUN_0011d010(cache, (void *)(block - 0x10));
  *(unsigned int *)(block - 0xc) |= 1;
}

/* 0x11d420: lru_cache entry release helper (mirror of FUN_0011d3f0).
 * lruv_cache.obj.
 *
 * Refreshes the cache's function pointers (FUN_0011d090, cache passed in EAX),
 * validates the block-header integrity (FUN_0011d010, which checks magic
 * 0x55626c6a and asserts "lru cache ... appears to be corrupt"), then clears
 * bit 0 (valid/in-use flag) in the block-header status word.
 *
 * param_1 (cache) = lru cache ptr. param_2 (block) = pointer just past a block
 * header; header fields live at block-0x10 (block base, passed to the
 * validator) and block-0xc (status word that receives the &= ~1). Disasm:
 * mov eax,edi (cache->EAX); call 0x11d090; push (block-0x10); push cache;
 * call 0x11d010; mov eax,[esi+4]; and eax,0xfffffffe; mov [esi+4],eax.
 */
void FUN_0011d420(int cache, int block)
{
  FUN_0011d090(cache);
  FUN_0011d010(cache, (void *)(block - 0x10));
  *(unsigned int *)(block - 0xc) &= 0xfffffffe;
}

/* 0x11d450: lru_cache entry acquire/stamp helper. Sibling of FUN_0011d420.
 * lruv_cache.obj.
 *
 * Refreshes the cache's function pointers (FUN_0011d090, cache passed in EAX),
 * validates the block-header integrity (FUN_0011d010, header at block-0x10),
 * then stamps the block header field at block-8 with the cache's current
 * sequence counter (cache+0x3c) and post-increments that counter.
 *
 * param_1 (cache) = lru cache ptr. param_2 (block) = pointer just past a block
 * header; header base is block-0x10 (passed to the validator), the stamp field
 * is block-8 (= (block-0x10)+8). Disasm: mov eax,esi (cache->EAX);
 * add edi,-0x10 (edi=block-0x10); call 0x11d090; push edi; push esi;
 * call 0x11d010; mov eax,[esi+0x3c]; mov [edi+8],eax; mov eax,[esi+0x3c];
 * inc eax; mov [esi+0x3c],eax. The counter is loaded twice (old value stored,
 * then reloaded and incremented).
 */
void FUN_0011d450(int cache, int block)
{
  FUN_0011d090(cache);
  FUN_0011d010(cache, (void *)(block - 0x10));
  *(int *)(block - 8) = *(int *)(cache + 0x3c);
  *(int *)(cache + 0x3c) = *(int *)(cache + 0x3c) + 1;
}

/* 0x11d480: Compute the total allocation size needed for an lruv_cache
 * with the given maximum block count. Returns sizeof(lruv_cache_t) + data
 * allocation for the block datums. */
int lruv_cache_allocation_size(int maximum_block_count)
{
  return data_allocation_size(maximum_block_count, 0x1c) + 0x44;
}

/* 0x11d4a0: Set the delete and query callbacks on an lruv_cache.
 * param_1 must be non-NULL (asserted). Writes delete_cb to offset 0x20
 * and query_cb to offset 0x24 of the cache header. */
void lruv_cache_set_callbacks(void *cache, void (*delete_cb)(int),
                              int (*query_cb)(int))
{
  lruv_cache_t *c;
  assert_halt(cache);
  c = (lruv_cache_t *)cache;
  c->delete_cb = delete_cb;
  c->query_cb = query_cb;
}

/* 0x11d4f0: Return non-zero if the cache has a query callback set.
 * param_1 must be non-NULL (asserted). Returns bool: query_cb != NULL. */
bool lruv_cache_has_query_cb(void *cache)
{
  lruv_cache_t *c;
  assert_halt(cache);
  c = (lruv_cache_t *)cache;
  return c->query_cb != 0;
}

/* 0x11d550: Verify the integrity of an lruv_cache.
 * Checks the signature, validates the data_t, and optionally walks
 * the linked list verifying forward/backward consistency and page ordering. */
void lruv_cache_verify(void *cache, char do_full_check)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;

  assert_halt(cache);
  assert_halt(c->signature == LRUV_CACHE_SIGNATURE);

  data_verify(c->blocks);

  if (do_full_check) {
    int block_index = c->first_block_index;

    while (block_index != NONE) {
      lruv_cache_block_t *block =
        (lruv_cache_block_t *)datum_get(c->blocks, block_index);

      if (block->previous_block_index == NONE) {
        assert_halt(c->first_block_index == block_index);
      } else {
        lruv_cache_block_t *previous_block = (lruv_cache_block_t *)datum_get(
          c->blocks, block->previous_block_index);
        assert_halt(previous_block->next_block_index == block_index);
        assert_halt(previous_block->first_page_index < block->first_page_index);
        assert_halt(previous_block->first_page_index +
                      previous_block->page_count <=
                    block->first_page_index);
      }

      if (block->next_block_index == NONE) {
        assert_halt(c->last_block_index == block_index);
      } else {
        lruv_cache_block_t *next_block =
          (lruv_cache_block_t *)datum_get(c->blocks, block->next_block_index);
        assert_halt(next_block->previous_block_index == block_index);
        assert_halt(next_block->first_page_index > block->first_page_index);
        assert_halt(block->first_page_index + block->page_count <=
                    next_block->first_page_index);
      }

      block_index = block->next_block_index;
    }
  }
}

/* 0x11d780: Initialize an lruv_cache in pre-allocated memory.
 * Sets up the name, callbacks, page configuration, and the inline data_t
 * for block tracking. */
void lruv_cache_initialize(void *cache, int name, int page_count,
                           int page_size_bits, int maximum_block_count,
                           void (*delete_cb)(int), int (*query_cb)(int))
{
  lruv_cache_t *c = (lruv_cache_t *)cache;
  data_t *blocks = (data_t *)((char *)cache + 0x44);

  assert_halt(name);
  assert_halt(page_count > 0);
  assert_halt(page_size_bits > 0 && page_size_bits < 16);
  assert_halt(maximum_block_count > 0);

  data_initialize(blocks, (char *)name, maximum_block_count, 0x1c);
  data_delete_all(blocks);
  csmemset(cache, 0, 0x44);
  csstrncpy((char *)cache, (const char *)name, 0x1f);

  c->delete_cb = delete_cb;
  c->page_count = page_count;
  c->query_cb = query_cb;
  c->page_size_bits = page_size_bits;
  c->blocks = blocks;
  c->signature = LRUV_CACHE_SIGNATURE;
  c->first_block_index = NONE;
  c->last_block_index = NONE;
  c->field_30 = 1;

  lruv_cache_verify(cache, 1);
}

/* 0x11d890: Dispose of an lruv_cache. Verifies integrity, disposes the
 * underlying data_t, zeros the header, and frees the memory. */
void lruv_cache_dispose(void *cache)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;

  lruv_cache_verify(cache, 1);
  /* blocks is inline at cache+0x44 (not debug_malloc'd), and cache itself is
     game_state_malloc'd — neither has a debug header. Original calls
     data_dispose + debug_free here, which fires non-fatal asserts. */
  data_delete_all(c->blocks);
  csmemset(cache, 0, 0x44);
}

/* 0x11d8d0: Idle tick for an lruv_cache. Runs the fast integrity check
 * (do_full_check=0) and increments the counter at offset 0x30. */
void lruv_idle(void *cache)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;

  lruv_cache_verify(cache, 0);
  c->field_30 = c->field_30 + 1;
}

/* 0x11d8f0: Remove a single block from the cache's linked list and
 * delete the datum. Calls the delete callback if set. Relinks neighbors. */
void lruv_block_delete(void *cache, int block_index)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;
  lruv_cache_block_t *block =
    (lruv_cache_block_t *)datum_get(c->blocks, block_index);

  lruv_cache_verify(cache, 1);

  if (c->delete_cb != NULL) {
    c->delete_cb(block_index);
  }

  /* Unlink from previous neighbor */
  if (block->previous_block_index == NONE) {
    assert_halt(c->first_block_index == block_index);
    c->first_block_index = block->next_block_index;
  } else {
    lruv_cache_block_t *prev =
      (lruv_cache_block_t *)datum_get(c->blocks, block->previous_block_index);
    prev->next_block_index = block->next_block_index;
  }

  /* Unlink from next neighbor */
  if (block->next_block_index == NONE) {
    assert_halt(c->last_block_index == block_index);
    c->last_block_index = block->previous_block_index;
  } else {
    lruv_cache_block_t *next =
      (lruv_cache_block_t *)datum_get(c->blocks, block->next_block_index);
    next->previous_block_index = block->previous_block_index;
  }

  datum_delete(c->blocks, block_index);
  lruv_cache_verify(cache, 1);
}

/* lruv_debug_to_file (0x11d9d0)
 *
 * Touch a cache block — set its last-access stamp to the cache's
 * current frame counter (field_30).
 */
void lruv_debug_to_file(void *cache, int datum_handle)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;
  lruv_cache_block_t *block;

  lruv_cache_verify(cache, 0);
  block = (lruv_cache_block_t *)datum_get(c->blocks, datum_handle);
  *(int *)((char *)block + 0x14) = c->field_30;
}

/* lruv_block_get_address (0x11da00)
 *
 * Return the base page address of a cache block: the block's
 * first_page_index (entry+0x8) shifted left by the cache's
 * page_size_bits (cache+0x2c, masked to the low 5 bits).  Runs the
 * fast integrity check before the datum lookup.
 */
int lruv_block_get_address(void *lruv, int block_index)
{
  lruv_cache_t *c = (lruv_cache_t *)lruv;
  lruv_cache_block_t *block;

  lruv_cache_verify(lruv, 0);
  block = (lruv_cache_block_t *)datum_get(c->blocks, block_index);
  return block->first_page_index << (c->page_size_bits & 0x1f);
}

/* lruv_block_touched (0x11da30)
 *
 * Return true if the given cache block was stamped during the current
 * cache cycle: compares the block's stamp field (block+0x14) against
 * the cache's current generation counter (cache+0x30, field_30).  Runs
 * the fast integrity check before the datum lookup.  Sibling of
 * lruv_block_get_address (0x11da00).
 */
bool lruv_block_touched(void *lruv, int block_index)
{
  lruv_cache_t *c = (lruv_cache_t *)lruv;
  lruv_cache_block_t *block;

  lruv_cache_verify(lruv, 0);
  block = (lruv_cache_block_t *)datum_get(c->blocks, block_index);
  return *(int *)block->unk_14 == c->field_30;
}

/* lruv_cache_get_page_usage (0x11da60)
 *
 * Build a per-page usage/flag map for the cache.  Zeroes the caller's
 * usage buffer (page_count bytes, one byte per page), then walks every
 * cache block via the inline data_t and stamps the page range the block
 * occupies [first_page_index, first_page_index + page_count) with a
 * flag byte:
 *   bit0 (1) always set for a page owned by a block
 *   bit3 (8) set when the optional query callback (cache+0x24) reports
 *            the block's datum handle as in-use (checks non-NULL first)
 *   bit1 (2) set when the block's stamp (block+0x14) equals the cache's
 *            current generation (field_30) - touched this cycle
 *   bit2 (4) set when the block's stamp + 0x1e is older than the current
 *            generation - stale.
 * Runs the full integrity check first.
 * Source: c:\halo\SOURCE\memory\lruv_cache.c
 */
void lruv_cache_get_page_usage(void *cache, unsigned char *usage)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;
  lruv_cache_block_t *block;
  data_iter_t iter;
  unsigned char flags;

  lruv_cache_verify(cache, 1);
  csmemset(usage, 0, c->page_count);
  data_iterator_new(&iter, c->blocks);

  block = (lruv_cache_block_t *)data_iterator_next(&iter);
  while (block != NULL) {
    flags = 1;
    if (c->query_cb != NULL && (char)c->query_cb(iter.datum_handle) != 0) {
      flags = 9;
    }
    if (*(unsigned int *)block->unk_14 == (unsigned int)c->field_30) {
      flags |= 2;
    }
    if (*(unsigned int *)block->unk_14 + 0x1e < (unsigned int)c->field_30) {
      flags |= 4;
    }
    csmemset(usage + block->first_page_index, flags, block->page_count);
    block = (lruv_cache_block_t *)data_iterator_next(&iter);
  }
}

/* lruv_resize (0x11db00)
 *
 * Resize the lruv_cache to new_page_count pages.  Any block whose
 * page range (first_page_index + page_count) exceeds the new limit is
 * evicted via lruv_block_delete before the page_count field is updated.
 */
void lruv_resize(void *cache, int new_page_count)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;
  lruv_cache_block_t *block;
  data_iter_t iter;

  assert_halt(new_page_count > 0);
  lruv_cache_verify(cache, 1);
  data_iterator_new(&iter, c->blocks);

  block = (lruv_cache_block_t *)data_iterator_next(&iter);
  while (block != NULL) {
    if (block->first_page_index + block->page_count > new_page_count) {
      lruv_block_delete(cache, iter.datum_handle);
    }
    block = (lruv_cache_block_t *)data_iterator_next(&iter);
  }

  c->page_count = new_page_count;
}

/* FUN_0011db90 (0x11db90)
 *
 * Dump LRUV cache diagnostic state to a file.  Used when a cache
 * allocation fails to record page layout, block occupancy, and
 * per-block age for post-mortem analysis.
 */
void FUN_0011db90(const char *path, const char *tag_name, int alloc_size,
                  void *cache, void *fn1, void *fn2)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;
  void *stream;
  int page_size, pages_needed, total_pages;
  int page_index, block_handle;
  int page_count, age;
  bool locked;
  const char *block_name;

  lruv_cache_verify(cache, 1);

  stream = crt_fopen(path, (const char *)0x28fdcc);
  if (stream == NULL)
    return;

  crt_fprintf(stream, "%s (v1: only blocks used this frame are locked)\n",
              cache);
  ((void (*)(void *))fn1)(stream);

  page_size = 1 << (c->page_size_bits & 0x1f);
  pages_needed = alloc_size >> (c->page_size_bits & 0x1f);
  if ((alloc_size & (page_size - 1)) != 0)
    pages_needed++;

  crt_fprintf(stream,
              "\n#%d pages, each #%d bytes\n"
              "#%d blocks at frame index #%d\n"
              "failed allocation of \"%s\" was #%d bytes (#%d pages)\n\n",
              c->page_count, page_size,
              (int)*(int16_t *)((char *)c->blocks + 0x30), c->field_30,
              tag_name, alloc_size, pages_needed);

  total_pages = c->page_count;
  block_handle = c->first_block_index;
  page_index = 0;

  while (page_index < total_pages) {
    age = 0;
    locked = false;

    if (block_handle == -1) {
      page_count = total_pages - page_index;
      page_index = total_pages;
      block_name = (const char *)0x25386f;
    } else {
      lruv_cache_block_t *block =
        (lruv_cache_block_t *)datum_get(c->blocks, block_handle);

      if (page_index != block->first_page_index) {
        page_count = block->first_page_index - page_index;
        assert_halt(page_count > 0);
        page_index = block->first_page_index;
        block_name = (const char *)0x25386f;
      } else {
        page_count = block->page_count;
        age = c->field_30 - *(int *)((char *)block + 0x14);

        if (c->query_cb != NULL) {
          locked = c->query_cb(block_handle) != 0;
        } else {
          locked = false;
        }
        if (*(int *)((char *)block + 0x14) + 1 >= (unsigned int)c->field_30)
          locked = true;

        page_index = block->first_page_index + block->page_count;
        block_name = ((const char *(*)(int))fn2)(block_handle);
        block_handle = block->next_block_index;
        if (block_name == NULL)
          block_name = (const char *)0x25386f;
      }
    }

    if (age > 9999)
      age = 9999;

    crt_fprintf(stream, "%s % 5d% 5d %s\n",
                locked ? (const char *)0x28fd20 : (const char *)0x25b06c,
                page_count, age, block_name);

    total_pages = c->page_count;
  }

  crt_fprintf(stream, (const char *)0x260ee4);
  crt_fclose(stream);
}

/* 0x11dd60: Allocate and initialize a new lruv_cache on the debug heap.
 * Computes the datum-array allocation for maximum_block_count records of
 * 0x1c bytes (data_allocation_size), adds the 0x44-byte cache header, and
 * allocates via debug_malloc. If allocation succeeds, initializes the cache
 * with the caller's parameters and callbacks. Returns the cache pointer, or
 * NULL on allocation failure.
 *
 * Disasm note: Ghidra mis-groups the cdecl args here — MSVC pushes the outer
 * debug_malloc args (zero=0, __FILE__, __LINE__=0x52) before evaluating the
 * inner data_allocation_size(param_4, 0x1c) call, so the decompiler wrongly
 * shows those three as extra data_allocation_size arguments.
 * data_allocation_size is 2-arg (count, size); debug_malloc is 4-arg (size,
 * zero, file, line). */
void *lruv_new(int name, int page_count, int page_size_bits,
               int maximum_block_count, void (*delete_cb)(int),
               int (*query_cb)(int))
{
  void *cache;

  cache = debug_malloc(data_allocation_size(maximum_block_count, 0x1c) + 0x44,
                       0, "c:\\halo\\SOURCE\\memory\\lruv_cache.c", 0x52);
  if (cache != NULL) {
    lruv_cache_initialize(cache, name, page_count, page_size_bits,
                          maximum_block_count, delete_cb, query_cb);
  }
  return cache;
}

/* 0x11ddc0: Dispose all blocks from the cache by iterating the data_t
 * and removing each block individually via lruv_block_delete. */
void lruv_cache_dispose_all(void *cache)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;
  data_iter_t iter;

  lruv_cache_verify(cache, 1);
  data_iterator_new(&iter, c->blocks);

  while (data_iterator_next(&iter) != NULL) {
    lruv_block_delete(cache, iter.datum_handle);
  }
}

/* 0x1bfe90: Allocate and initialize a new lruv_cache from game state memory.
 * Returns a pointer to the initialized cache. */
void *lruv_cache_new(const char *name, int capacity, int max_locked,
                     int entry_size, void (*delete_cb)(int),
                     int (*query_cb)(int))
{
  int alloc_size = lruv_cache_allocation_size(entry_size);
  void *cache = game_state_malloc(name, "lruv cache", alloc_size);

  lruv_cache_initialize(cache, (int)name, capacity, max_locked, entry_size,
                        delete_cb, query_cb);

  return cache;
}
