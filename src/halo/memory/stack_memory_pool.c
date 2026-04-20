/* stack_memory_pool.c — stack-based memory pool allocator.
 *
 * Manages a fixed-size memory arena divided into variable-size blocks, tracked
 * in a slot table appended immediately after the pool header struct. Blocks
 * carry a 0x1c-byte header; the high bit of the first dword flags whether the
 * block is "in use" (marked by stack_memory_pool_mark_used internally). The
 * pool tracks bytes_used, peak_bytes, alloc_count, peak_alloc_count, and
 * largest_alloc for diagnostics.
 *
 * All internal helpers (valid_block, unlink_block, alloc_or_resize,
 * memory_block_valid, mark_used) use non-standard register-passing conventions
 * (EAX/ECX/ESI/EDI) and are called via inline asm.
 */

/* Internal register-convention callees — not exported, called via asm.
 *
 * stack_memory_pool_valid_block  (0x11ef50): EAX=block_hdr, ECX=pool -> AL
 * stack_memory_pool_unlink_block (0x11efd0): ESI=block_hdr, EDI=pool
 * stack_memory_pool_alloc_or_resize (0x11f750): EAX=new_size, ECX=pool;
 *   stack [block_hdr, file, line] -> EAX=new_block_hdr
 * memory_block_valid (0x11ecf0): ECX=block_hdr -> AL
 * stack_memory_pool_mark_used (0x11f070): ESI=block_hdr, EDI=pool
 *   (ESI must also equal pool->current_slot via context)
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
  char *block_hdr; /* block header = block - 0x1c */
  unsigned int size_flags;
  unsigned int usable_size;
  int valid;
  int _eax;

  if (block == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x197, 1);
    system_exit(-1);
  }

  block_hdr = (char *)block - 0x1c;

  /* Call stack_memory_pool_valid_block(EAX=block_hdr, ECX=pool) -> AL. */
  _eax = (int)block_hdr;
  __asm__ __volatile__("movl %[pool], %%ecx\n\t"
                       "call *%[fn]"
                       : "+a"(_eax)
                       : [pool] "r"(pool), [fn] "r"((void *)0x0011ef50)
                       : "ecx", "edx", "memory", "cc");
  valid = _eax & 0xff;

  if (!valid) {
    display_assert("invalid pointer",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x19d, 1);
    system_exit(-1);
  }

  /* block assertion — the block_hdr pointer was already adjusted; the original
   * re-checks ESI (which is block_hdr) here and asserts it is non-NULL.
   * Since we asserted block != NULL above and subtracted 0x1c, this can only
   * be NULL if block == 0x1c, which the valid_block check would have caught.
   * Replicate the assert for fidelity. */
  if (block_hdr == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x22f, 1);
    system_exit(-1);
  }

  /* Read size_flags from block_hdr[0] and strip the in-use high bit. */
  size_flags = *(unsigned int *)block_hdr;
  usable_size = size_flags & 0x7fffffff;

  /* Call stack_memory_pool_unlink_block(ESI=block_hdr, EDI=pool).
   * Use "S"/"D" constraints to bind directly to ESI/EDI. */
  {
    void *_esi = block_hdr;
    void *_edi = pool;
    __asm__ __volatile__("call *%[fn]"
                         : "+S"(_esi), "+D"(_edi)
                         : [fn] "r"((void *)0x0011efd0)
                         : "eax", "ecx", "edx", "memory", "cc");
  }

  /* Update pool accounting. */
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
  int _eax;

  /* Call internal allocator: EAX=size, stack=[pool, file, line]. */
  {
    unsigned int args[3];
    args[0] = (unsigned int)pool;
    args[1] = (unsigned int)file;
    args[2] = line;
    _eax = size;
    __asm__ __volatile__("pushl 8(%[a])\n\t" /* push line */
                         "pushl 4(%[a])\n\t" /* push file */
                         "pushl 0(%[a])\n\t" /* push pool */
                         "call *%[fn]\n\t"
                         "addl $0xc, %%esp"
                         : "+a"(_eax)
                         : [a] "r"(args), [fn] "r"((void *)0x0011f1e0)
                         : "ecx", "edx", "memory", "cc");
  }
  block_hdr = (char *)_eax;

  if (block_hdr == 0) {
    return 0;
  }

  /* mark_used helper consumes ESI=block_hdr and ECX=pool. */
  {
    void *_esi = block_hdr;
    int _ecx = (int)pool;
    __asm__ __volatile__("call *%[fn]"
                         : "+S"(_esi), "+c"(_ecx)
                         : [fn] "r"((void *)0x0011f070)
                         : "eax", "edx", "edi", "memory", "cc");
  }

  /* Validate returned block header. */
  _eax = 0;
  __asm__ __volatile__("movl %[bh], %%ecx\n\t"
                       "call *%[fn]"
                       : "+a"(_eax)
                       : [bh] "r"(block_hdr), [fn] "r"((void *)0x0011ecf0)
                       : "ecx", "edx", "memory", "cc");
  valid = _eax & 0xff;

  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f, 1);
    system_exit(-1);
  }

  /* Update pool accounting. */
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
  int _eax;

  /* Compute old block header pointer and save old usable size. */
  if (block == 0) {
    block_hdr = 0;
  } else {
    block_hdr =
      (char *)block - 0x18; /* = block - 0x1c + 4, i.e. ESI=block-0x18 */
  }
  old_size = 0;
  if (block_hdr != 0) {
    block_hdr = block_hdr - 4; /* now block_hdr = block - 0x1c */
    if (block_hdr == 0) {
      display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                     0x22f, 1);
      system_exit(-1);
    }
    old_size = *(unsigned int *)block_hdr & 0x7fffffff;
  }

  /* Call alloc_or_resize(EAX=new_size, ECX=pool; stack=[block_hdr, file,
   * line]). Stack args are pushed right-to-left. Use an args array to avoid
   * register pressure from multiple "r" inputs plus pushl. */
  {
    unsigned int args[3];
    int _ecx;
    args[0] = (unsigned int)block_hdr;
    args[1] = (unsigned int)file;
    args[2] = (unsigned int)line;
    _ecx = (int)pool;
    _eax = (int)(unsigned int)new_size;
    __asm__ __volatile__("pushl 8(%[a])\n\t" /* push line */
                         "pushl 4(%[a])\n\t" /* push file */
                         "pushl 0(%[a])\n\t" /* push block_hdr */
                         "call  *%[fn]\n\t"
                         "addl  $0xc, %%esp"
                         : "+a"(_eax), "+c"(_ecx)
                         : [a] "r"(args), [fn] "r"((void *)0x0011f750)
                         : "edx", "memory", "cc");
  }
  new_hdr = (char *)_eax;

  if (new_hdr == 0) {
    return 0;
  }

  /* Validate new block: memory_block_valid(ECX=new_hdr) -> AL */
  _eax = 0;
  __asm__ __volatile__("movl %[bh], %%ecx\n\t"
                       "call *%[fn]"
                       : "+a"(_eax)
                       : [bh] "r"(new_hdr), [fn] "r"((void *)0x0011ecf0)
                       : "ecx", "edx", "memory", "cc");
  valid = _eax & 0xff;

  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x215, 1);
    system_exit(-1);
  }

  /* If the block's in-use flag (high bit) is clear, mark it used.
   * mark_used (0x11f070) reads ESI=block_hdr and ECX=pool. */
  new_size_flags = *(unsigned int *)new_hdr;
  if (!((new_size_flags >> 0x1f) & 1)) {
    void *_esi2 = new_hdr;
    int _ecx2 = (int)pool;
    __asm__ __volatile__("call *%[fn]"
                         : "+S"(_esi2), "+c"(_ecx2)
                         : [fn] "r"((void *)0x0011f070)
                         : "eax", "edx", "edi", "memory", "cc");
  }

  /* Validate new block again after mark_used. */
  _eax = 0;
  __asm__ __volatile__("movl %[bh], %%ecx\n\t"
                       "call *%[fn]"
                       : "+a"(_eax)
                       : [bh] "r"(new_hdr), [fn] "r"((void *)0x0011ecf0)
                       : "ecx", "edx", "memory", "cc");
  valid = _eax & 0xff;

  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f, 1);
    system_exit(-1);
  }

  /* Update pool stats. */
  new_usable = *(unsigned int *)new_hdr & 0x7fffffff;
  curr_bytes = *(unsigned int *)(pool_p + 0x14);
  curr_bytes += new_usable - old_size;
  *(unsigned int *)(pool_p + 0x14) = curr_bytes;

  /* alloc_count: increment by 1 only when old block was NULL (old_size == 0).
   * Original: SETLE DL (signed <=: old_size==0 means EBX==0, so 0<=0 is true,
   * DL=1). ADD ECX, EDX. */
  alloc_count = *(unsigned int *)(pool_p + 0x1c);
  alloc_count += (old_size == 0) ? 1 : 0;
  *(unsigned int *)(pool_p + 0x1c) = alloc_count;

  /* peak_bytes = max(peak_bytes, bytes_used). */
  if ((int)curr_bytes > *(int *)(pool_p + 0x18)) {
    *(unsigned int *)(pool_p + 0x18) = curr_bytes;
  }

  /* peak_alloc_count = max(peak_alloc_count, alloc_count). */
  if (alloc_count > *(unsigned int *)(pool_p + 0x20)) {
    *(unsigned int *)(pool_p + 0x20) = alloc_count;
  }

  /* largest_alloc = max(largest_alloc, new_usable). */
  if (new_usable > *(unsigned int *)(pool_p + 0x24)) {
    *(unsigned int *)(pool_p + 0x24) = new_usable;
    return (void *)(new_hdr + 0x1c);
  }

  return (void *)(new_hdr + 0x1c);
}
