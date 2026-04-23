/* stack_memory_pool.c — stack-based memory pool allocator.
 *
 * Manages a fixed-size memory arena divided into variable-size blocks, tracked
 * in a slot table appended immediately after the pool header struct. Blocks
 * carry a 0x1c-byte header; the high bit of the first dword flags whether the
 * block is "in use" (marked by stack_memory_pool_mark_used internally). The
 * pool tracks bytes_used, peak_bytes, alloc_count, peak_alloc_count, and
 * largest_alloc for diagnostics.
 *
 * Internal helpers (valid_block, unlink_block, alloc_or_resize,
 * memory_block_valid, mark_used) use non-standard register-passing conventions
 * (EAX/ECX/ESI/EDI) declared in kb.json with @<reg> annotations.
 */


/* stack_memory_pool_initialize — reset a pool back to its initial state.
 *
 * Saves the four pre-configured header fields (tag/name at +0, base_address
 * at +4, pool_size at +8, slot_count at +c), zeroes the entire 0x34-byte
 * header plus the slot table (slot_count * 4 bytes), then restores those
 * fields and writes a self-pointer into table[0] to seed the free-slot list.
 */
void stack_memory_pool_initialize(void *pool)
{
  unsigned int *p = (unsigned int *)pool;
  unsigned int saved0, saved1, saved2, saved3;
  unsigned int *table;
  unsigned int table_ptr;

  if (pool == 0) {
    display_assert("pool", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x1b0, 1);
    system_exit(-1);
  }

  /* Save the pre-configured fields that survive an initialize call. */
  saved0 = p[0]; /* field at +0x00 (e.g. pool tag/name pointer) */
  saved1 = p[1]; /* field at +0x04 (base_address) */
  saved2 = p[2]; /* field at +0x08 (pool_size in bytes) */
  saved3 = p[3]; /* field at +0x0c (slot_count) */

  table = p + 0xd; /* &pool[0x34] — start of slot table */

  /* Zero the slot table first (slot_count * 4 bytes). */
  csmemset(table, 0, saved3 * 4);

  /* Zero the 0x34-byte pool header. */
  csmemset(pool, 0, 0x34);

  /* Restore the pre-configured fields. */
  p[0] = saved0;
  p[1] = saved1;
  p[2] = saved2; /* field at +0x08 */
  p[3] = saved3; /* field at +0x0c */

  /* Seed slot table[0] with &table[0] (self-pointer for free-list init).
   * The original uses csmemcpy(table, &local_table_ptr, 4) where
   * local_table_ptr == table. */
  table_ptr = (unsigned int)table;
  csmemcpy(table, &table_ptr, 4);
}

/* FUN_0011ea50 — initialize a freshly allocated block header.
 *
 * Register convention (kb.json):
 *   - slot_index on stack (cdecl first arg)
 *   - block_hdr in ESI (@<esi>)
 *   - block_size in EDI (@<edi>)
 *
 * Writes header fields:
 *   +0x00 size_flags = block_size
 *   +0x04 slot_index
 *   +0x18 "fryd" sentinel (0x66727964)
 *   +block_size-4 "chkn" sentinel (0x63686b6e)
 */
void FUN_0011ea50(int slot_index, void *block_hdr, int block_size)
{
  int *blk = (int *)block_hdr;

  if (blk == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x21f, 1);
    system_exit(-1);
  }

  blk[0] = block_size;
  blk[1] = slot_index;
  blk[6] = 0x66727964;
  *(int *)((char *)blk + block_size - 4) = 0x63686b6e;
}

/* FUN_0011ea90 — return block usable size from header.
 *
 * Register convention: block_hdr in ESI (kb.json @<esi>).
 * Returns low 31 bits of dword at +0x00 (size_flags).
 */
unsigned int FUN_0011ea90(void *block_hdr)
{
  unsigned int *blk = (unsigned int *)block_hdr;

  if (blk == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x22f, 1);
    system_exit(-1);
  }

  return blk[0] & 0x7fffffff;
}

/* FUN_0011eb40 — compute largest free tail space in pool.
 *
 * Register convention: pool in ESI (kb.json @<esi>).
 *
 * If pool has no blocks, returns pool_size.
 * Otherwise returns bytes from end of last block to pool end.
 */
unsigned int FUN_0011eb40(void *pool)
{
  char *pool_p = (char *)pool;
  unsigned int *last_block;

  if (pool == 0 || *(unsigned int *)(pool_p + 4) == 0) {
    display_assert("pool && pool->base_address",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x2fc, 1);
    system_exit(-1);
  }

  if (*(unsigned int *)(pool_p + 0x2c) == 0) {
    return *(unsigned int *)(pool_p + 8);
  }

  last_block = *(unsigned int **)(pool_p + 0x30);
  if (last_block == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x22f, 1);
    system_exit(-1);
  }

  return *(unsigned int *)(pool_p + 8) - (last_block[0] & 0x7fffffff) +
         *(unsigned int *)(pool_p + 4) - (unsigned int)last_block;
}

/* memory_block_valid — validate a block header's integrity.
 *
 * Checks three conditions on the block header pointed to by block_hdr:
 *   1. The usable size (low 31 bits of dword at +0x00) must be > 0x20
 *      (the block must have payload beyond the 0x20-byte header overhead).
 *   2. The "fryd" sentinel (0x66727964) at block_hdr+0x18 must be intact.
 *   3. The "chkn" sentinel (0x63686b6e) at block_hdr[usable_size - 4]
 *      must be intact (end-of-block canary).
 *
 * Returns 1 if all checks pass, 0 otherwise (after asserting on failure).
 *
 * Register convention: block_hdr passed in ECX (declared @<ecx> in kb.json).
 */
int memory_block_valid(void *block_hdr)
{
  unsigned int *p = (unsigned int *)block_hdr;
  unsigned int usable_size;

  if (p == 0) {
    return 0;
  }

  usable_size = p[0] & 0x7fffffff;

  if (usable_size == 0x20) {
    display_assert("!\"pointer has invalid size\"",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x1e4, 1);
    system_exit(-1);
    return 0;
  }

  if (p[6] != 0x66727964) {
    display_assert("!\"this memory has been corrupted\"",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x1e9, 1);
    system_exit(-1);
    return 0;
  }

  if (*(unsigned int *)((char *)p + usable_size - 4) != 0x63686b6e) {
    display_assert("!\"wrote beyond the valid address space for this block\"",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x1ee, 1);
    system_exit(-1);
    return 0;
  }

  return 1;
}

/* stack_memory_pool_valid_block — verify a block belongs to a pool.
 *
 * Checks that block_hdr falls within the pool's address range
 * (base_address <= block_hdr < base_address + pool_size), then validates
 * the block's integrity via memory_block_valid. Finally, looks up the
 * block's slot index, fetches the corresponding slot-table entry, and
 * compares three header fields (dwords at +0, +8, +c) between the
 * slot-table pointer and block_hdr to ensure they match.
 *
 * Returns 1 if all checks pass, 0 otherwise.
 *
 * Register convention: block_hdr in EAX, pool in ECX (kb.json @<eax>/@<ecx>).
 *
 * Block header layout used:
 *   +0x00: size_flags (low 31 bits = usable_size, high bit = in-use)
 *   +0x04: slot_index
 *   +0x08: field at +8 (prev pointer in linked list)
 *   +0x0c: field at +c (next pointer in linked list)
 *
 * Pool header layout used:
 *   +0x04: base_address
 *   +0x08: pool_size
 *   +0x0c: slot_count
 *   +0x34: start of slot table (slot_count entries, 4 bytes each)
 */
int stack_memory_pool_valid_block(void *block_hdr, void *pool)
{
  unsigned int *blk = (unsigned int *)block_hdr;
  char *pool_p = (char *)pool;
  unsigned int *base;
  unsigned int *end;
  unsigned int slot_index;
  unsigned int *slot_entry;
  int valid;

  if (pool == 0 || *(unsigned int *)(pool_p + 4) == 0) {
    display_assert("pool && pool->base_address",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x3d7, 1);
    system_exit(-1);
  }

  base = *(unsigned int **)(pool_p + 4);
  end = (unsigned int *)((char *)base + *(unsigned int *)(pool_p + 8));

  if (blk < base || blk >= end) {
    return 0;
  }

  valid = memory_block_valid(block_hdr) & 0xff;
  if (!valid) {
    return 0;
  }

  /* Inline of block-slot-index getter (0x11eb10): assert block != NULL,
   * return dword at block_hdr+4. block_hdr is known non-NULL here. */
  slot_index = blk[1];

  if (slot_index >= *(unsigned int *)(pool_p + 0xc)) {
    return 0;
  }

  slot_entry = *(unsigned int **)(pool_p + 0x34 + slot_index * 4);
  if (slot_entry == 0) {
    return 0;
  }

  if (slot_entry[0] != blk[0]) {
    return 0;
  }
  if (slot_entry[2] != blk[2]) {
    return 0;
  }
  if (slot_entry[3] != blk[3]) {
    return 0;
  }

  return 1;
}

/* stack_memory_pool_unlink_block — remove a block from the pool's linked list.
 *
 * Validates the block belongs to this pool, then unlinks it from the
 * doubly-linked block list. Updates pool->first_block (+0x2c) and
 * pool->last_block (+0x30) if the removed block was at either end.
 * Clears the slot-table entry and updates pool->next_block_index (+0x10).
 *
 * Register convention: block_hdr in ESI, pool in EDI (kb.json @<esi>/@<edi>).
 *
 * Block header offsets used:
 *   +0x04: slot_index
 *   +0x08: prev pointer
 *   +0x0c: next pointer
 *
 * Pool header offsets used:
 *   +0x10: next_block_index
 *   +0x2c: first_block
 *   +0x30: last_block
 *   +0x34: slot table base
 */
void stack_memory_pool_unlink_block(void *block_hdr, void *pool)
{
  char *blk = (char *)block_hdr;
  char *pool_p = (char *)pool;
  unsigned int slot_index;
  unsigned int *prev;
  unsigned int *next;
  int valid;

  valid = stack_memory_pool_valid_block(block_hdr, pool) & 0xff;
  if (!valid) {
    display_assert("stack_memory_pool_valid_block(pool, reference)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x2d3, 1);
    system_exit(-1);
  }

  if (blk == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x24a, 1);
    system_exit(-1);
  }

  slot_index = *(unsigned int *)(blk + 0x4);
  prev = *(unsigned int **)(blk + 0x8);
  next = *(unsigned int **)(blk + 0xc);

  /* Patch prev's next pointer. */
  if (prev != 0) {
    *(unsigned int *)((char *)prev + 0xc) = (unsigned int)next;
  }

  /* Patch next's prev pointer. */
  if (next != 0) {
    *(unsigned int *)((char *)next + 0x8) = (unsigned int)prev;
  }

  /* Update first_block if we unlinked it. */
  if ((unsigned int)blk == *(unsigned int *)(pool_p + 0x2c)) {
    *(unsigned int *)(pool_p + 0x2c) = (unsigned int)next;
  }

  /* Update last_block if we unlinked it. */
  if ((unsigned int)blk == *(unsigned int *)(pool_p + 0x30)) {
    *(unsigned int *)(pool_p + 0x30) = (unsigned int)prev;
  }

  /* Clear the slot-table entry. */
  *(unsigned int *)(pool_p + 0x34 + slot_index * 4) = 0;

  /* Update next_block_index: reuse the freed slot if pool still has blocks,
   * otherwise set to 0. Pattern: -(first_block != 0) & slot_index. */
  {
    unsigned int first_block = *(unsigned int *)(pool_p + 0x2c);
    unsigned int neg = -(first_block != 0);
    *(unsigned int *)(pool_p + 0x10) = neg & slot_index;
  }
}

/* stack_memory_pool_mark_used — mark a block as in-use and validate it.
 *
 * Verifies the block belongs to the pool and is not already locked
 * (high bit of size_flags at +0x00 must be clear). Then validates with
 * memory_block_valid, sets the high bit to mark the block in-use, and
 * validates again. Returns a pointer to the user data area (block_hdr + 0x1c).
 *
 * Register convention: block_hdr in ESI, pool in ECX (kb.json @<esi>/@<ecx>).
 *
 * Note: the return value (block_hdr + 0x1c) is placed in EAX but the kb.json
 * prototype declares void return. Callers in the original binary do not
 * use the return value.
 */
void stack_memory_pool_mark_used(void *block_hdr, void *pool)
{
  unsigned int *blk = (unsigned int *)block_hdr;
  int valid;

  valid = stack_memory_pool_valid_block(block_hdr, pool) & 0xff;

  if (!valid) {
    /* Fall through — but also check memory_block_valid and the locked flag
     * before reaching the combined assert. */
    goto combined_assert;
  }

  valid = memory_block_valid(block_hdr) & 0xff;
  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x215, 1);
    system_exit(-1);
  }

  /* Check if block is already locked (high bit set). */
  if ((blk[0] >> 0x1f) & 1) {
    goto combined_assert;
  }

  goto do_mark;

combined_assert:
  display_assert("stack_memory_pool_valid_block(pool, reference) && "
                 "!memory_block_is_locked(reference)",
                 "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x2e5, 1);
  system_exit(-1);

do_mark:
  /* Validate block integrity before marking. */
  valid = memory_block_valid(block_hdr) & 0xff;
  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x203, 1);
    system_exit(-1);
  }

  /* Set the high bit to mark the block as in-use. */
  blk[0] = blk[0] | 0x80000000;

  /* Validate block integrity after marking. */
  valid = memory_block_valid(block_hdr) & 0xff;
  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f, 1);
    system_exit(-1);
  }
}

/* stack_memory_pool_alloc_internal — allocate a block header inside the pool.
 *
 * Register convention follows kb.json:
 *   - alloc_size in EAX (@<eax>)
 *   - pool/file/line on stack (cdecl order)
 *
 * Uses internal helpers in the same object to compact blocks, find free space,
 * choose a slot index, initialize block sentinels, and refresh next index.
 * Returns block header pointer on success, NULL on failure.
 */
void *stack_memory_pool_alloc_internal(int alloc_size, void *pool,
                                       const char *file, unsigned int line)
{
  char *pool_p = (char *)pool;
  char *free_space_in_pool_previous;
  char *block_hdr;
  unsigned int aligned_block_size;
  unsigned int largest_free;
  int slot_index;
  int free_space_found;

  if (pool == 0 || *(int *)(pool_p + 4) == 0) {
    display_assert("pool && pool->base_address",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x342, 1);
    system_exit(-1);
  }

  if (alloc_size == 0 || (unsigned int)alloc_size > 0x7fffffff ||
      *(unsigned int *)(pool_p + 8) <= (unsigned int)alloc_size) {
    display_assert("invalid size",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x3a4, 0);
    return 0;
  }

  free_space_in_pool_previous = 0;
  free_space_found = 0;

  aligned_block_size = (unsigned int)alloc_size + 0x20;
  while ((aligned_block_size & 3) != 0) {
    aligned_block_size++;
  }

  largest_free = FUN_0011eb40(pool);
  if (largest_free < aligned_block_size) {
    FUN_0011ee80(pool);
    largest_free = FUN_0011eb40(pool);
    if (largest_free < aligned_block_size) {
      free_space_found = (int)FUN_0011ec70(
        pool, aligned_block_size, (void **)&free_space_in_pool_previous);
      if (free_space_found == 0) {
        display_assert(
          "allocation from memory pool failed; unable to find sufficient space "
          "in the pool",
          "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x39f, 0);
        return 0;
      }
    }
  }

  if (*(int *)(pool_p + 0x10) == -1) {
    slot_index = FUN_0011ebc0(pool);
    *(int *)(pool_p + 0x10) = slot_index;
    if (slot_index == -1) {
      display_assert("the memory pool has no more unsused master pointers; you "
                     "need to use a "
                     "bigger pool",
                     "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x35f, 0);
    }
  }

  slot_index = *(int *)(pool_p + 0x10);
  if (slot_index == -1) {
    return 0;
  }

  if (free_space_found == 0) {
    if (*(int *)(pool_p + 0x2c) == 0) {
      *(unsigned int *)(pool_p + 0x34 + slot_index * 4) =
        *(unsigned int *)(pool_p + 4);
    } else {
      if (*(int *)(pool_p + 0x30) == 0) {
        display_assert("pool->last_block",
                       "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x370,
                       1);
        system_exit(-1);
      }

      *(int *)(pool_p + 0x34 + *(int *)(pool_p + 0x10) * 4) =
        FUN_0011ea90(*(void **)(pool_p + 0x30)) + *(int *)(pool_p + 0x30);
    }
  } else {
    *(int *)(pool_p + 0x34 + slot_index * 4) = free_space_found;
  }

  block_hdr = *(char **)(pool_p + 0x34 + *(int *)(pool_p + 0x10) * 4);
  FUN_0011ea50(*(int *)(pool_p + 0x10), block_hdr, aligned_block_size);
  *(const char **)(block_hdr + 0x10) = file;
  *(unsigned int *)(block_hdr + 0x14) = line;

  if (*(int *)(pool_p + 0x2c) == 0) {
    if (*(int *)(pool_p + 0x30) != 0 || *(int *)(pool_p + 0x10) != 0) {
      display_assert(
        "(pool->last_block == NULL) && (pool->next_block_index == 0)",
        "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x37d, 1);
      system_exit(-1);
    }

    *(char **)(pool_p + 0x30) = block_hdr;
    *(char **)(pool_p + 0x2c) = block_hdr;
    *(int *)(block_hdr + 8) = 0;
    *(int *)(block_hdr + 0xc) = 0;
    FUN_0011ec10(pool);
    return block_hdr;
  }

  if ((unsigned int)block_hdr < *(unsigned int *)(pool_p + 0x2c)) {
    *(int *)(block_hdr + 8) = 0;
    *(int *)(block_hdr + 0xc) = *(int *)(pool_p + 0x2c);
    *(int *)(*(int *)(pool_p + 0x2c) + 8) = (int)block_hdr;
    *(char **)(pool_p + 0x2c) = block_hdr;
    FUN_0011ec10(pool);
    return block_hdr;
  }

  if ((unsigned int)block_hdr > *(unsigned int *)(pool_p + 0x30)) {
    *(int *)(block_hdr + 0xc) = 0;
    *(int *)(block_hdr + 8) = *(int *)(pool_p + 0x30);
    *(int *)(*(int *)(pool_p + 0x30) + 0xc) = (int)block_hdr;
    *(char **)(pool_p + 0x30) = block_hdr;
    FUN_0011ec10(pool);
    return block_hdr;
  }

  if (free_space_in_pool_previous == 0) {
    display_assert("free_space_in_pool_previous",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x394, 1);
    system_exit(-1);
  }

  *(char **)(block_hdr + 8) = free_space_in_pool_previous;
  *(int *)(block_hdr + 0xc) = *(int *)(free_space_in_pool_previous + 0xc);
  *(char **)(free_space_in_pool_previous + 0xc) = block_hdr;
  if (*(int *)(block_hdr + 0xc) != 0) {
    *(int *)(*(int *)(block_hdr + 0xc) + 8) = (int)block_hdr;
  }

  FUN_0011ec10(pool);
  return block_hdr;
}

/* stack_memory_pool_deallocate — free a block back to the pool.
 *
 * Walks back 0x1c bytes from the user pointer to reach the block header,
 * validates the block belongs to this pool, unlinks it from the doubly-linked
 * list via the internal unlink_block helper, then subtracts the block's usable
 * size from pool->bytes_used and decrements pool->alloc_count.
 *
 * Pool struct offsets used here:
 *   +0x14: bytes_used  (uint32)
 *   +0x1c: alloc_count (int32)
 */
void stack_memory_pool_deallocate(void *pool, void *block)
{
  char *pool_p = (char *)pool;
  char *block_hdr;
  unsigned int size_flags;
  unsigned int usable_size;
  int valid;

  if (block == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x197, 1);
    system_exit(-1);
  }

  block_hdr = (char *)block - 0x1c;

  valid = stack_memory_pool_valid_block(block_hdr, pool) & 0xff;

  if (!valid) {
    display_assert("invalid pointer",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x19d, 1);
    system_exit(-1);
  }

  if (block_hdr == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x22f, 1);
    system_exit(-1);
  }

  size_flags = *(unsigned int *)block_hdr;
  usable_size = size_flags & 0x7fffffff;

  stack_memory_pool_unlink_block(block_hdr, pool);

  *(unsigned int *)(pool_p + 0x14) -= usable_size;
  *(int *)(pool_p + 0x1c) -= 1;
}

/* stack_memory_pool_allocate — allocate a new block from the pool.
 *
 * Calls internal allocator 0x11f1e0 with EAX=size and stack args
 * [pool, file, line]. On success, marks the returned block in-use,
 * validates it, updates pool accounting, and returns user pointer
 * (block_hdr + 0x1c). Returns NULL on allocation failure.
 *
 * Pool struct offsets touched:
 *   +0x14: bytes_used
 *   +0x18: peak_bytes
 *   +0x1c: alloc_count
 *   +0x20: peak_alloc_count
 *   +0x24: largest_alloc
 */
void *stack_memory_pool_allocate(void *pool, int size, const char *file,
                                 unsigned int line)
{
  char *pool_p = (char *)pool;
  char *block_hdr;
  unsigned int size_flags;
  unsigned int usable_size;
  unsigned int alloc_count;
  unsigned int bytes_used;
  int valid;

  block_hdr = (char *)stack_memory_pool_alloc_internal(size, pool, file, line);

  if (block_hdr == 0) {
    return 0;
  }

  stack_memory_pool_mark_used(block_hdr, pool);

  valid = memory_block_valid(block_hdr) & 0xff;

  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f, 1);
    system_exit(-1);
  }

  size_flags = *(unsigned int *)block_hdr;
  usable_size = size_flags & 0x7fffffff;

  bytes_used = *(unsigned int *)(pool_p + 0x14) + usable_size;
  *(unsigned int *)(pool_p + 0x14) = bytes_used;

  alloc_count = *(unsigned int *)(pool_p + 0x1c) + 1;
  *(unsigned int *)(pool_p + 0x1c) = alloc_count;

  if ((int)bytes_used > *(int *)(pool_p + 0x18)) {
    *(unsigned int *)(pool_p + 0x18) = bytes_used;
  }

  if (alloc_count > *(unsigned int *)(pool_p + 0x20)) {
    *(unsigned int *)(pool_p + 0x20) = alloc_count;
  }

  if (usable_size > *(unsigned int *)(pool_p + 0x24)) {
    *(unsigned int *)(pool_p + 0x24) = usable_size;
  }

  return (void *)(block_hdr + 0x1c);
}

/* stack_memory_pool_realloc — resize (or allocate) a block in the pool.
 *
 * If block == NULL: pure allocation (old_size = 0).
 * Otherwise: block_hdr = block - 0x1c, old_size = block_hdr[0] & 0x7fffffff.
 *
 * Delegates to the internal alloc_or_resize helper (0x11f750), which returns
 * the new block header pointer. On success, validates the new block, marks it
 * in-use if it was previously free (high bit clear), validates again, then
 * updates pool statistics:
 *   +0x14: bytes_used
 *   +0x18: peak_bytes
 *   +0x1c: alloc_count (incremented by 1 only when old block was NULL)
 *   +0x20: peak_alloc_count
 *   +0x24: largest_alloc
 *
 * Returns pointer to user data area (block_hdr + 0x1c), or NULL on failure.
 */
void *stack_memory_pool_realloc(void *pool, int block, unsigned short new_size,
                                const char *file, unsigned int line)
{
  char *pool_p = (char *)pool;
  char *block_hdr;
  char *new_hdr;
  unsigned int old_size;
  unsigned int new_size_flags;
  unsigned int new_usable;
  unsigned int alloc_count;
  unsigned int curr_bytes;
  int valid;

  if (block == 0) {
    block_hdr = 0;
  } else {
    block_hdr = (char *)block - 0x18;
  }
  old_size = 0;
  if (block_hdr != 0) {
    block_hdr = block_hdr - 4;
    if (block_hdr == 0) {
      display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                     0x22f, 1);
      system_exit(-1);
    }
    old_size = *(unsigned int *)block_hdr & 0x7fffffff;
  }

  new_hdr = (char *)stack_memory_pool_alloc_or_resize(
    (int)(unsigned int)new_size, pool, block_hdr, file, line);

  if (new_hdr == 0) {
    return 0;
  }

  valid = memory_block_valid(new_hdr) & 0xff;

  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x215, 1);
    system_exit(-1);
  }

  new_size_flags = *(unsigned int *)new_hdr;
  if (!((new_size_flags >> 0x1f) & 1)) {
    stack_memory_pool_mark_used(new_hdr, pool);
  }

  valid = memory_block_valid(new_hdr) & 0xff;

  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f, 1);
    system_exit(-1);
  }

  new_usable = *(unsigned int *)new_hdr & 0x7fffffff;
  curr_bytes = *(unsigned int *)(pool_p + 0x14);
  curr_bytes += new_usable - old_size;
  *(unsigned int *)(pool_p + 0x14) = curr_bytes;

  alloc_count = *(unsigned int *)(pool_p + 0x1c);
  alloc_count += (old_size == 0) ? 1 : 0;
  *(unsigned int *)(pool_p + 0x1c) = alloc_count;

  if ((int)curr_bytes > *(int *)(pool_p + 0x18)) {
    *(unsigned int *)(pool_p + 0x18) = curr_bytes;
  }

  if (alloc_count > *(unsigned int *)(pool_p + 0x20)) {
    *(unsigned int *)(pool_p + 0x20) = alloc_count;
  }

  if (new_usable > *(unsigned int *)(pool_p + 0x24)) {
    *(unsigned int *)(pool_p + 0x24) = new_usable;
    return (void *)(new_hdr + 0x1c);
  }

  return (void *)(new_hdr + 0x1c);
}
