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
