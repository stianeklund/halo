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

/* fatal_assert — display_assert immediately followed by noreturn system_exit.
 *
 * The original compiler deferred display_assert's cdecl arg cleanup past the
 * noreturn system_exit call and dropped it, so reference assert blocks end
 * "calll display_assert; pushl $-1; calll system_exit" with no addl esp.
 * VC71 instead emits "addl $0x10,%esp" between the two calls (a spurious
 * scoring insn).  Calling through a __stdcall cast reproduces the reference's
 * no-cleanup shape; the path is dead (system_exit never returns) so the
 * unbalanced stack is unreachable and the pushl $-1 still lands on [esp+4]
 * exactly as in the original.
 */
typedef void(__stdcall *fatal_assert_stdcall_fn)(const char *reason,
                                                 const char *filepath, int line,
                                                 int halt);

#define fatal_assert(reason, line)                   \
  ((fatal_assert_stdcall_fn)(void *)display_assert)( \
    reason,                                          \
    "c:\\halo\\SOURCE\\"                             \
    "memory\\stack_memory_pool.c",                   \
    line, 1)


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

/* memory_block_get_user_size (0x11eac0) — block size minus 0x20 header bytes.
 *
 * Register convention: block_hdr in ESI (kb.json @<esi>).
 * Binary: assert block (line 0x237), then an inlined copy of FUN_0011ea90
 * (its own block assert at line 0x22f and the low-31-bit size read), then
 * SUB EAX,0x20. Both assert blocks share one ADD ESP,0x28 cleanup.
 */
unsigned int memory_block_get_user_size(void *block_hdr)
{
  unsigned int *blk = (unsigned int *)block_hdr;

  if (blk == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x237, 1);
    system_exit(-1);
  }
  if (blk == 0) {
    display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x22f, 1);
    system_exit(-1);
  }

  return (blk[0] & 0x7fffffff) - 0x20;
}

/* memory_block_get_pool_index (0x11eb10) — return a block's pool slot index.
 *
 * Register convention: block_hdr in ESI (kb.json @<esi>).
 * Asserts block != NULL (stack_memory_pool.c:0x24a), then returns the dword
 * at +0x04 (the slot index written by FUN_0011ea50).
 */
int memory_block_get_pool_index(void *block_hdr)
{
  int *blk = (int *)block_hdr;

  if (blk == 0) {
    fatal_assert("block", 0x24a);
    system_exit(-1);
  }

  return blk[1];
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

/* FUN_0011ebc0 — find first free slot index in pool slot table.
 *
 * Register convention: pool in EAX (kb.json @<eax>).
 * Returns slot index, or -1 if table is full.
 */
int FUN_0011ebc0(void *pool)
{
  char *pool_p = (char *)pool;
  int slot_count;
  int slot_index;
  int *slot_entry;

  if (pool == 0) {
    display_assert("pool", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                   0x310, 1);
    system_exit(-1);
  }

  slot_count = *(int *)(pool_p + 0xc);
  if (slot_count <= 0) {
    return -1;
  }

  slot_index = 0;
  slot_entry = (int *)(pool_p + 0x34);
  do {
    if (*slot_entry == 0) {
      return slot_index;
    }
    slot_index++;
    slot_entry++;
  } while (slot_index < slot_count);

  return -1;
}

/* FUN_0011ec10 — refresh pool->next_block_index from current table state.
 *
 * Register convention: pool in ESI (kb.json @<esi>).
 *
 * If next_block_index is set, scans forward for the next empty slot and stores
 * it; stores -1 when no empty slot remains.
 */
void FUN_0011ec10(void *pool)
{
  char *pool_p = (char *)pool;
  int slot_index;
  int *slot_entry;

  if (pool == 0) {
    fatal_assert("pool", 0x321);
    system_exit(-1);
  }

  if (*(int *)(pool_p + 0x10) != -1) {
    slot_index = *(int *)(pool_p + 0x10) + 1;
    *(int *)(pool_p + 0x10) = -1;
    if (slot_index < *(int *)(pool_p + 0xc)) {
      slot_entry = (int *)(pool_p + 0x34 + slot_index * 4);
      do {
        if (*slot_entry == 0) {
          *(int *)(pool_p + 0x10) = slot_index;
          return;
        }
        slot_index++;
        slot_entry++;
      } while (slot_index < *(volatile int *)(pool_p + 0xc));
    }
  }
}

/* FUN_0011ec70 — find a free gap large enough for alloc_size.
 *
 * Register convention (kb.json):
 *   - pool in EAX (@<eax>)
 *   - alloc_size in EBX (@<ebx>)
 *   - free_space_in_pool_previous on stack
 *
 * Returns start address of a suitable free span, or NULL.
 * If the free span is between two blocks, writes the previous block header to
 * *free_space_in_pool_previous.
 */
void *FUN_0011ec70(void *pool, int alloc_size,
                   void **free_space_in_pool_previous)
{
  char *pool_p = (char *)pool;
  unsigned int *block;
  unsigned int *next;
  void *ret;

  ret = 0;
  block = *(unsigned int **)(pool_p + 0x2c);
  if (block != 0 && (unsigned int)((char *)block - *(char **)(pool_p + 4)) >=
                      (unsigned int)alloc_size) {
    return *(void **)(pool_p + 4);
  }

  if (block == 0) {
    return ret;
  }

  next = *(unsigned int **)((char *)block + 0xc);
  if (next == 0) {
    return ret;
  }

  while (1) {
    if (block == 0) {
      fatal_assert("block", 0x22f);
      system_exit(-1);
    }

    if ((unsigned int)((char *)next -
                       ((char *)block + (block[0] & 0x7fffffff))) >=
        (unsigned int)alloc_size) {
      break;
    }

    block = next;
    next = *(unsigned int **)((char *)next + 0xc);
    if (next == 0) {
      return ret;
    }
  }

  ret = (char *)block + (block[0] & 0x7fffffff);
  *free_space_in_pool_previous = block;
  return ret;
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
bool memory_block_valid(void *block_hdr)
{
  unsigned int *p = (unsigned int *)block_hdr;
  unsigned int usable_size;

  if (p != 0) {
    usable_size = p[0] & 0x7fffffff;

    if (!(usable_size - 0x20 > 0)) {
      fatal_assert("!\"pointer has invalid size\"", 0x1e4);
      system_exit(-1);
      return false;
    } else {
      if (p[6] != 0x66727964) {
        fatal_assert("!\"this memory has been corrupted\"", 0x1e9);
        system_exit(-1);
        return false;
      }

      if (*(unsigned int *)((char *)p + usable_size - 4) != 0x63686b6e) {
        fatal_assert("!\"wrote beyond the valid address space for this block\"",
                     0x1ee);
        system_exit(-1);
        return false;
      }

      return true;
    }
  }

  return false;
}

/* memory_block_get_user_address (0x11ee50) — return a block's payload pointer.
 *
 * Asserts the header is intact, then returns block_hdr + 0x1c (just past the
 * "fryd" sentinel at +0x18), which is the first byte of user data.
 *
 * Register convention: block_hdr in ESI (kb.json @<esi>); it is forwarded to
 * memory_block_valid in ECX.
 */
void *memory_block_get_user_address(void *block_hdr)
{
  if (!(memory_block_valid(block_hdr) & 0xff)) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f, 1);
    system_exit(-1);
  }

  return (char *)block_hdr + 0x1c;
}

/* FUN_0011ee80 — compact unlocked blocks toward pool base.
 *
 * Register convention: pool in EAX (kb.json @<eax>).
 *
 * Walks blocks in address order and moves unlocked blocks down to remove gaps.
 */
void FUN_0011ee80(void *pool)
{
  char *pool_p = (char *)pool;
  unsigned int *block;
  char *previous_block;
  unsigned int previous_size;

  if (pool == 0 || *(unsigned int *)(pool_p + 4) == 0 || (pool_p + 0x34) == 0) {
    display_assert("pool && pool->base_address && pool->blocks",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x3ad, 1);
    system_exit(-1);
  }

  block = *(unsigned int **)(pool_p + 0x2c);
  if (block == 0 || *(unsigned char *)(pool_p + 0x28) != 0) {
    return;
  }

  previous_block = *(char **)(pool_p + 4);
  previous_size = 0;

  do {
    if (!(memory_block_valid(block) & 0xff)) {
      display_assert("memory_block_valid(block)",
                     "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x215, 1);
      system_exit(-1);
    }

    if ((int)block[0] >= 0) {
      int gap = (int)((char *)block - previous_size - previous_block);

      if (gap > 0) {
        unsigned int size = block[0] & 0x7fffffff;
        unsigned int *moved = (unsigned int *)(previous_block + previous_size);

        memcpy(moved, block, size);
        block = moved;

        if (block[2] != 0) {
          *(unsigned int **)(block[2] + 0xc) = block;
        }
      }
    }

    previous_size = block[0] & 0x7fffffff;
    previous_block = (char *)block;
    block = *(unsigned int **)((char *)block + 0xc);
  } while (block != 0);
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
bool stack_memory_pool_valid_block(void *block_hdr, void *pool)
{
  unsigned int *blk = (unsigned int *)block_hdr;
  char *pool_p = (char *)pool;
  unsigned int *base;
  unsigned int *end;
  unsigned int slot_index;
  unsigned int *slot_entry;

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

  if (!memory_block_valid(block_hdr)) {
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
  if ((int)blk[0] < 0) {
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

/* stack_memory_pool_unlock_block — clear a block's locked (in-use) flag.
 *
 * Verifies the block belongs to the pool and that it is currently locked
 * (high bit of size_flags at +0x00 set), re-validates the block header, then
 * clears the high bit.
 *
 * Register convention: block_hdr in ESI, pool in ECX (kb.json @<esi>/@<ecx>).
 */
void stack_memory_pool_unlock_block(void *block_hdr, void *pool)
{
  unsigned int *blk = (unsigned int *)block_hdr;
  int valid;

  valid = stack_memory_pool_valid_block(block_hdr, pool) & 0xff;
  if (valid) {
    valid = memory_block_valid(block_hdr) & 0xff;
    if (!valid) {
      display_assert("memory_block_valid(block)",
                     "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x215, 1);
      system_exit(-1);
    }

    /* memory_block_is_locked(reference) — high bit of size_flags. */
    if ((blk[0] >> 31) & 1) {
      goto do_unlock;
    }
  }

  display_assert("stack_memory_pool_valid_block(pool, reference) && "
                 "memory_block_is_locked(reference)",
                 "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x2f0, 1);
  system_exit(-1);

do_unlock:
  /* Validate block integrity before clearing the flag. */
  valid = memory_block_valid(block_hdr) & 0xff;
  if (!valid) {
    display_assert("memory_block_valid(block)",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x20c, 1);
    system_exit(-1);
  }

  blk[0] = blk[0] & 0x7fffffff;
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
    fatal_assert("pool && pool->base_address", 0x342);
    system_exit(-1);
  }

  if (alloc_size == 0 || (unsigned int)alloc_size > 0x7fffffff ||
      (unsigned int)alloc_size >= *(unsigned int *)(pool_p + 8)) {
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
        fatal_assert("pool->last_block", 0x370);
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
      fatal_assert("(pool->last_block == NULL) && "
                   "(pool->next_block_index == 0)",
                   0x37d);
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
    fatal_assert("free_space_in_pool_previous", 0x394);
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

/* dispose_handle (0x11f460) — free a handle's block back to its pool.
 *
 * Stack args: [ebp+8] pool (passed in ECX to valid_block, EDI to unlink),
 * [ebp+0xc] block header (EAX to valid_block, ESI to unlink).  No callers
 * found in the binary.  The block's size (low 31 bits of header word) is read
 * before the unlink, then subtracted from pool+0x14; pool+0x1c is decremented.
 *
 * Asserts: "not a valid handle, or handle is locked" (0xe8), "block" (0x22f).
 */
void dispose_handle(void *pool, void *block_hdr)
{
  char *pool_p = (char *)pool;
  unsigned int usable_size;

  if (!stack_memory_pool_valid_block(block_hdr, pool)) {
    fatal_assert("not a valid handle, or handle is locked", 0xe8);
    system_exit(-1);
  }

  if (block_hdr == 0) {
    fatal_assert("block", 0x22f);
    system_exit(-1);
  }

  usable_size = *(unsigned int *)block_hdr & 0x7fffffff;

  stack_memory_pool_unlink_block(block_hdr, pool);

  *(unsigned int *)(pool_p + 0x14) -= usable_size;
  *(int *)(pool_p + 0x1c) -= 1;
}

/* lock_handle (0x11f4e0) — lock the pool block referenced by *h.
 *
 * Stack args: [ebp+8] pool (ECX to valid_block and mark_used), [ebp+0xc] h.
 * *h holds the block header on entry (EAX to valid_block, ESI to mark_used)
 * and receives mark_used's EAX return on exit.
 *
 * The original stores stack_memory_pool_mark_used's EAX into *h.  mark_used
 * (0x11f070) ends with LEA EAX,[ESI+0x1c] at 0x11f12f, i.e. it returns the
 * block's user address, but its kb.json prototype is void, so that value is
 * written out here from the same block pointer.
 *
 * Asserts: h (0xf7), "not a valid handle, or handle is already locked" (0xfa).
 */
void lock_handle(void *pool, void **h)
{
  char *block;

  if (h == 0) {
    fatal_assert("h", 0xf7);
    system_exit(-1);
  }

  block = (char *)*h;

  if (!stack_memory_pool_valid_block(block, pool)) {
    fatal_assert("not a valid handle, or handle is already locked", 0xfa);
    system_exit(-1);
  }

  stack_memory_pool_mark_used(block, pool);
  *h = block + 0x1c;
}

/* unlock_handle (0x11f550) — unlock the pool block that owns a user pointer.
 *
 * Scans the pool's slot table (+0x34, pool->slot_count entries at +0xc) for the
 * block whose payload address (block_hdr + 0x1c) equals the handle, then clears
 * that block's locked flag via stack_memory_pool_unlock_block.
 *
 * memory_block_get_user_address is inlined here in the original (ADD EDI,0x1c
 * at 0x11f5c4), so the offset arithmetic is written out rather than called.
 *
 * Pool struct offsets used here:
 *   +0x0c: slot_count (uint32)
 *   +0x34: slot table base (slot_count entries, 4 bytes each)
 *
 * Asserts: h (0x107), memory_block_valid(block) (0x23f),
 *          "invalid handle, or handle was not locked" (0x111).
 */
void unlock_handle(void *pool, void *h)
{
  char *pool_p = (char *)pool;
  char *block;
  char *candidate;
  unsigned int *slot;
  unsigned int index;
  int valid;

  block = 0;
  index = 0;

  if (h == 0) {
    display_assert("h", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x107,
                   1);
    system_exit(-1);
  }

  if (*(unsigned int *)(pool_p + 0xc) != 0) {
    slot = (unsigned int *)(pool_p + 0x34);
    do {
      candidate = (char *)*slot;
      if (candidate != 0) {
        valid = memory_block_valid(candidate) & 0xff;
        if (!valid) {
          display_assert("memory_block_valid(block)",
                         "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f,
                         1);
          system_exit(-1);
        }

        /* memory_block_get_user_address(candidate) == h */
        if (candidate + 0x1c == (char *)h) {
          block = *(char **)(pool_p + 0x34 + index * 4);
          break;
        }
      }

      index++;
      slot++;
    } while (index < *(unsigned int *)(pool_p + 0xc));
  }

  if (block == 0) {
    display_assert("invalid handle, or handle was not locked",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x111, 1);
    system_exit(-1);
  }

  stack_memory_pool_unlock_block(block, pool);
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
  unsigned int size_flags;
  unsigned int usable_size;
  int valid;

  if (block == 0) {
    fatal_assert("block", 0x197);
    system_exit(-1);
  }

  block = (char *)block + (-0x1c);

  valid = stack_memory_pool_valid_block(block, pool) & 0xff;

  if (!valid) {
    fatal_assert("invalid pointer", 0x19d);
    system_exit(-1);
  }

  if (block == 0) {
    fatal_assert("block", 0x22f);
    system_exit(-1);
  }

  size_flags = *(unsigned int *)block;
  usable_size = size_flags & 0x7fffffff;

  stack_memory_pool_unlink_block(block, pool);

  *(unsigned int *)(pool_p + 0x14) -= usable_size;
  *(int *)(pool_p + 0x1c) -= 1;
}

/* Forwards incoming EDX/ECX/EAX unchanged as the stack args of 0x11f1e0;
 * the only stack param becomes its EAX size. No callers in the binary. */
void *stack_memory_pool_new_block(void *pool, const char *file,
                                  unsigned int line, int alloc_size)
{
  return stack_memory_pool_alloc_internal(alloc_size, pool, file, line);
}

/* stack_memory_pool_alloc_or_resize — allocate new or grow existing block.
 *
 * Register convention (kb.json):
 *   - new_size in EAX (@<eax>)
 *   - pool in ECX (@<ecx>)
 *   - block_hdr/file/line on stack
 *
 * Returns block header pointer (not user pointer), or NULL on failure.
 */
void *stack_memory_pool_alloc_or_resize(int new_size, void *pool,
                                        void *block_hdr, const char *file,
                                        unsigned int line)
{
  void *new_hdr;

  if (new_size == 0) {
    return 0;
  }

  if (block_hdr == 0) {
    return stack_memory_pool_alloc_internal(new_size, pool, file, line);
  }

  if (!stack_memory_pool_valid_block(block_hdr, pool)) {
    fatal_assert("stack_memory_pool_valid_block(pool, reference)", 0x2b4);
    system_exit(-1);
  }

  if ((unsigned int)new_size <= memory_block_get_user_size(block_hdr)) {
    return block_hdr;
  }

  new_hdr = stack_memory_pool_alloc_internal(new_size, pool, file, line);
  if (new_hdr != 0) {
    csmemcpy((char *)new_hdr + 0x1c, (char *)block_hdr + 0x1c,
             memory_block_get_user_size(block_hdr));
    stack_memory_pool_unlink_block(block_hdr, pool);
  }

  return new_hdr;
}

/* pool_new_handle (0x11f810) — allocate a block and update pool accounting.
 *
 * Calls internal allocator 0x11f1e0 with EAX=size and stack args
 * [pool, file, line]. On success updates the pool statistics and returns the
 * block header pointer itself (unlike stack_memory_pool_allocate, which
 * returns block_hdr + 0x1c). Returns NULL on allocation failure.
 *
 * Pool struct offsets touched:
 *   +0x14: bytes_used
 *   +0x18: peak_bytes
 *   +0x1c: alloc_count
 *   +0x20: peak_alloc_count
 *   +0x24: largest_alloc
 */
void *pool_new_handle(void *pool, int size, const char *file, unsigned int line)
{
  char *pool_p = (char *)pool;
  char *block_hdr;
  unsigned int usable_size;
  unsigned int alloc_count;
  unsigned int bytes_used;

  block_hdr = (char *)stack_memory_pool_alloc_internal(size, pool, file, line);

  if (block_hdr != 0) {
    usable_size = *(unsigned int *)block_hdr & 0x7fffffff;

    alloc_count = *(unsigned int *)(pool_p + 0x1c) + 1;
    bytes_used = *(unsigned int *)(pool_p + 0x14) + usable_size;
    *(unsigned int *)(pool_p + 0x14) = bytes_used;
    *(unsigned int *)(pool_p + 0x1c) = alloc_count;

    if ((int)bytes_used > *(int *)(pool_p + 0x18)) {
      *(unsigned int *)(pool_p + 0x18) = bytes_used;
    }

    if (alloc_count > *(unsigned int *)(pool_p + 0x20)) {
      *(unsigned int *)(pool_p + 0x20) = alloc_count;
    }

    if ((*(unsigned int *)block_hdr & 0x7fffffff) >
        *(unsigned int *)(pool_p + 0x24)) {
      *(unsigned int *)(pool_p + 0x24) =
        *(unsigned int *)block_hdr & 0x7fffffff;
    }

    return block_hdr;
  }

  return 0;
}

/* pool_new_handle_clear (0x11f880) — allocate a block, zero its user area,
 * and update pool accounting.
 *
 * Binary evidence (0x11f880..0x11f92a):
 *   CALL 0x11f1e0 with EAX=size, stack [pool, file, line]
 *   CALL 0x11ecf0 memory_block_valid(ECX=block_hdr); assert line 0x23f
 *   CALL 0x8db80  csmemset(block_hdr + 0x1c, 0, size)
 * Returns the block header pointer itself, or NULL on allocation failure.
 * Pool offsets touched: +0x14, +0x18 (signed compare), +0x1c, +0x20, +0x24.
 */
void *pool_new_handle_clear(void *pool, int size, const char *file,
                            unsigned int line)
{
  char *pool_p = (char *)pool;
  char *block_hdr;

  block_hdr = (char *)stack_memory_pool_alloc_internal(size, pool, file, line);

  if (block_hdr != 0) {
    if (!(memory_block_valid(block_hdr) & 0xff)) {
      display_assert("memory_block_valid(block)",
                     "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f, 1);
      system_exit(-1);
    }

    csmemset(block_hdr + 0x1c, 0, size);

    *(unsigned int *)(pool_p + 0x14) += *(unsigned int *)block_hdr & 0x7fffffff;
    *(unsigned int *)(pool_p + 0x1c) += 1;

    if (*(int *)(pool_p + 0x14) > *(int *)(pool_p + 0x18)) {
      *(unsigned int *)(pool_p + 0x18) = *(unsigned int *)(pool_p + 0x14);
    }

    if (*(unsigned int *)(pool_p + 0x1c) > *(unsigned int *)(pool_p + 0x20)) {
      *(unsigned int *)(pool_p + 0x20) = *(unsigned int *)(pool_p + 0x1c);
    }

    if ((*(unsigned int *)block_hdr & 0x7fffffff) >
        *(unsigned int *)(pool_p + 0x24)) {
      *(unsigned int *)(pool_p + 0x24) =
        *(unsigned int *)block_hdr & 0x7fffffff;
    }

    return block_hdr;
  }

  return 0;
}

/* pool_resize_handle (0x11f930) — resize the block referenced by *h and
 * update pool accounting.
 *
 * Binary evidence (0x11f930..0x11fa35):
 *   assert h (line 0xc1)
 *   CALL 0x11ef50 stack_memory_pool_valid_block(EAX=*h, ECX=pool); the AL
 *     result is kept in [EBP-1] and returned on the success paths
 *   inlined FUN_0011ea90 on the old block (assert block, line 0x22f)
 *   CALL 0x11f750 with EAX=new_size, ECX=pool, stack [block, file, line]
 *   CALL 0x11ea90 (ESI=new block) only when largest_alloc grows
 * Returns false (XOR AL,AL) when the resize fails.
 * Pool offsets touched: +0x14, +0x18 (signed compare), +0x24 (unsigned).
 */
bool pool_resize_handle(void *pool, void **h, int new_size, const char *file,
                        unsigned int line)
{
  char *pool_p = (char *)pool;
  unsigned int *block_hdr;
  unsigned int *new_hdr;
  unsigned int old_size;
  bool valid;

  if (h == 0) {
    display_assert("h", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0xc1,
                   1);
    system_exit(-1);
  }

  block_hdr = (unsigned int *)*h;
  valid = stack_memory_pool_valid_block(block_hdr, pool);
  if (!valid) {
    display_assert("not a valid handle, or trying to resize a locked handle",
                   "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0xc5, 1);
    system_exit(-1);
  } else {
    if (block_hdr == 0) {
      display_assert("block", "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c",
                     0x22f, 1);
      system_exit(-1);
    }
    old_size = *block_hdr & 0x7fffffff;

    new_hdr = (unsigned int *)stack_memory_pool_alloc_or_resize(
      new_size, pool, block_hdr, file, line);
    if (new_hdr != 0) {
      *h = new_hdr;
      *(int *)(pool_p + 0x14) += (int)((*new_hdr & 0x7fffffff) - old_size);
      if (*(int *)(pool_p + 0x14) > *(int *)(pool_p + 0x18)) {
        *(int *)(pool_p + 0x18) = *(int *)(pool_p + 0x14);
      }

      if ((*new_hdr & 0x7fffffff) > *(unsigned int *)(pool_p + 0x24)) {
        *(unsigned int *)(pool_p + 0x24) = FUN_0011ea90(new_hdr);
      }

      return valid;
    }

    return 0;
  }

  return valid;
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
  alloc_count = *(unsigned int *)(pool_p + 0x1c) + 1;
  *(unsigned int *)(pool_p + 0x1c) = alloc_count;
  *(unsigned int *)(pool_p + 0x14) = bytes_used;

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

/* pool_new_pointer_clear (0x11faf0) — allocate a block, zero its user area,
 * mark it in-use, update pool accounting, and return the user pointer.
 *
 * Binary evidence (0x11faf0..0x11fbd4):
 *   CALL 0x11f1e0 with EAX=size, stack [pool, file, line]
 *   CALL 0x11ecf0 memory_block_valid(ECX=block_hdr); assert line 0x23f
 *   CALL 0x8db80  csmemset(block_hdr + 0x1c, 0, size)
 *   CALL 0x11f070 stack_memory_pool_mark_used(ESI=block_hdr, ECX=pool)
 *   CALL 0x11ecf0 memory_block_valid(ECX=block_hdr); assert line 0x23f
 * Returns block_hdr + 0x1c, or NULL on allocation failure.
 * Pool offsets touched: +0x14, +0x18 (signed compare), +0x1c, +0x20, +0x24.
 */
void *pool_new_pointer_clear(void *pool, int size, const char *file,
                             unsigned int line)
{
  char *pool_p = (char *)pool;
  char *block_hdr;
  char *pointer;

  pointer = 0;
  block_hdr = (char *)stack_memory_pool_alloc_internal(size, pool, file, line);

  if (block_hdr != 0) {
    if (!(memory_block_valid(block_hdr) & 0xff)) {
      display_assert("memory_block_valid(block)",
                     "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f, 1);
      system_exit(-1);
    }

    pointer = block_hdr + 0x1c;
    csmemset(pointer, 0, size);
    stack_memory_pool_mark_used(block_hdr, pool);

    if (!(memory_block_valid(block_hdr) & 0xff)) {
      display_assert("memory_block_valid(block)",
                     "c:\\halo\\SOURCE\\memory\\stack_memory_pool.c", 0x23f, 1);
      system_exit(-1);
    }

    *(unsigned int *)(pool_p + 0x14) += *(unsigned int *)block_hdr & 0x7fffffff;
    *(unsigned int *)(pool_p + 0x1c) += 1;

    if (*(int *)(pool_p + 0x14) > *(int *)(pool_p + 0x18)) {
      *(unsigned int *)(pool_p + 0x18) = *(unsigned int *)(pool_p + 0x14);
    }

    if (*(unsigned int *)(pool_p + 0x1c) > *(unsigned int *)(pool_p + 0x20)) {
      *(unsigned int *)(pool_p + 0x20) = *(unsigned int *)(pool_p + 0x1c);
    }

    if ((*(unsigned int *)block_hdr & 0x7fffffff) >
        *(unsigned int *)(pool_p + 0x24)) {
      *(unsigned int *)(pool_p + 0x24) =
        *(unsigned int *)block_hdr & 0x7fffffff;
    }
  }

  return pointer;
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
  if ((int)new_size_flags >= 0) {
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


/* FUN_0011fd10 — ratio of one of two texture-page dword counters to the page
 * area.
 *
 * Name left as FUN_: no string in this function proves one.
 *
 * Binary evidence (0011fd10-0011fd40):
 *   MOV AL,[EBP+0xc]; TEST AL,AL      only the low byte of param_2 is tested
 *   JZ -> MOV EDX,[EAX+0x10]          param_2 low byte == 0: dword at +0x10
 *   else  MOV ECX,[EAX+0x14]          param_2 low byte != 0: dword at +0x14
 *   (selected dword stored back into the param_2 stack slot)
 *   MOVSX [EAX+0xa] * MOVSX [EAX+0x8] int16 height * int16 width (IMUL)
 *   FILD selected; FIDIV product      signed int / signed int, result in ST0
 * The +0x08/+0x0a int16 names come from texture_page_verify's assert string;
 * the meanings of the +0x10/+0x14 dwords are unproven (texture_page_new clears
 * both).  The Ghidra decompile ("void (void)") is wrong; the disassembly above
 * is authoritative.
 */
float FUN_0011fd10(void *page, int param_2)
{
  int value;

  if ((char)param_2 != 0) {
    value = *(int *)((char *)page + 0x14);
  } else {
    value = *(int *)((char *)page + 0x10);
  }

  return (float)value /
         (*(int16_t *)((char *)page + 0xa) * *(int16_t *)((char *)page + 8));
}


/* texture_page_verify — validate a texture page header and its data block.
 *
 * Asserts the page pointer is non-NULL, then that both int16 dimensions are
 * positive (width at +0x08, height at +0x0a — names from the assert string
 * "texture_page->width>0 && texture_page->height>0"), then forwards the
 * data_t pointer at +0x18 to data_verify.
 *
 * The page pointer arrives in ESI (kb.json @<esi>): the reference opens with
 * "TEST ESI,ESI" with no prior write to ESI in this function.
 *
 * __FILE__ evidence: c:\halo\SOURCE\memory\texture_page.c, lines 0x104/0x105.
 * kb.json maps 0x11fd50 into stack_memory_pool.obj, so the lift lives in this
 * TU; the texture_page.c path in the assert strings is binary-proven and must
 * be spelled as the original did.
 *
 * The asserts use the same __stdcall-cast no-cleanup shape as fatal_assert
 * above (reference ends "calll display_assert; pushl $-1; calll system_exit"
 * with no stack cleanup); the file path differs, so the macro cannot be reused.
 */
void texture_page_verify(void *texture_page)
{
  char *page = (char *)texture_page;

  if (page == NULL) {
    ((fatal_assert_stdcall_fn)(void *)display_assert)(
      "texture_page", "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x104, 1);
    system_exit(-1);
  }

  if (*(int16_t *)(page + 8) <= 0 || *(int16_t *)(page + 0xa) <= 0) {
    ((fatal_assert_stdcall_fn)(void *)display_assert)(
      "texture_page->width>0 && texture_page->height>0",
      "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x105, 1);
    system_exit(-1);
  }

  data_verify(*(data_t **)(page + 0x18));
}


/* texture_page_new — allocate and initialize a texture page header.
 *
 * Allocates the 0x1c-byte page header via debug_malloc (reference pushes
 * size=0x1c, zero=0, file, line=0x1d), then asserts both int16 dimensions are
 * positive ("page_width>0 && page_height>0", line 0x1f) BEFORE testing the
 * allocation result — the reference tests DI/BX at 0011fdd1 and only reaches
 * "TEST ESI,ESI" at 0011fdfa afterwards.
 *
 * Field map recovered from the store sequence at 0011fe11-0011fe3f:
 *   +0x00 byte   cleared
 *   +0x04 dword  first stack arg ([EBP+8], meaning unproven)
 *   +0x08 int16  page_width   ([EBP+0xc])
 *   +0x0a int16  page_height  ([EBP+0x10])
 *   +0x0c int16  fourth stack arg ([EBP+0x14], meaning unproven)
 *   +0x10 dword  cleared
 *   +0x14 dword  cleared
 *   +0x18 data_t* "texture page textures" data array (0x7fff x 0xc)
 * Field names at +0x08/+0x0a are cross-confirmed by texture_page_verify's
 * assert string "texture_page->width>0 && texture_page->height>0".
 *
 * The store order below is the reference's source order, not MSVC's schedule
 * (the reference interleaves the data_new argument pushes with the stores).
 *
 * On data_new failure the reference pushes EDI, which XOR EDI,EDI set to 0 at
 * 0011fe15 and never rewrote — debug_free is called with a NULL pointer, not
 * with the page (the page is still live in ESI and is not pushed). Reproduced
 * literally; do not "fix" it into debug_free(page).
 *
 * Returns ESI: the page on success, NULL when debug_malloc or data_new failed.
 *
 * __FILE__ evidence: c:\halo\SOURCE\memory\texture_page.c (string 0x2905b0).
 * kb.json maps 0x11fdb0 into stack_memory_pool.obj, so the lift lives in this
 * TU alongside texture_page_verify; the texture_page.c path in the assert and
 * allocator strings is binary-proven and must be spelled as the original did.
 */
void *texture_page_new(uint32_t user_data, int16_t page_width,
                       int16_t page_height, int16_t spacing)
{
  char *texture_page;
  data_t *textures;

  texture_page = (char *)debug_malloc(0x1c, 0,
                                      "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x1d);

  if (page_width <= 0 || page_height <= 0) {
    ((fatal_assert_stdcall_fn)(void *)display_assert)(
      "page_width>0 && page_height>0",
      "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x1f, 1);
    system_exit(-1);
  }

  if (texture_page != NULL) {
    csmemset(texture_page, 0, 0x1c);

    *(int16_t *)(texture_page + 8) = page_width;
    *(int16_t *)(texture_page + 0xa) = page_height;
    *(int16_t *)(texture_page + 0xc) = spacing;
    *(uint32_t *)(texture_page + 4) = user_data;
    *(uint32_t *)(texture_page + 0x10) = 0;
    *(uint32_t *)(texture_page + 0x14) = 0;
    *texture_page = 0;

    *(data_t **)(texture_page + 0x18) = data_new((char *)"texture page textures", 0x7fff, 0xc);

    if (*(data_t **)(texture_page + 0x18) != NULL) {
      data_delete_all(*(data_t **)(texture_page + 0x18));
      texture_page_verify(texture_page);
      return texture_page;
    }

    textures = *(data_t **)(texture_page + 0x18);
    texture_page = NULL;
    debug_free(textures, "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x38);
  }

  return texture_page;
}


/* FUN_0011fe80 — dispose of a texture page header allocated by
 * texture_page_new.
 *
 * Name left as FUN_: kb.json carries no name and no string in this function
 * proves one. Only the file path and line number are binary evidence.
 *
 * Reference shape (0011fe80-0011fea7): the page arrives as the single cdecl
 * stack arg, is loaded into ESI once ("MOV ESI,[EBP+8]") and reused for all
 * three calls:
 *   texture_page_verify(page)   — page passed in ESI (kb.json @<esi>), so the
 *                                 reference emits no push for it
 *   data_dispose(page->+0x18)   — "MOV EAX,[ESI+0x18]; PUSH EAX", the same
 *                                 data_t* field texture_page_new stored and
 *                                 texture_page_verify forwards to data_verify
 *   debug_free(page, file, line) — pushes are 0x45, 0x2905b0, ESI (last arg
 *                                 pushed first), so file/line are the
 *                                 texture_page.c path at 0x2905b0 and line 0x45
 * The single "ADD ESP,0x10" at 0011fea2 is MSVC's coalesced cleanup for both
 * cdecl calls (1 dword + 3 dwords); it is not a 4-argument debug_free, so the
 * artifact's ARG_COUNT hazard note is accounted for by the push count.
 *
 * The data array is disposed BEFORE the header is freed; preserve that order.
 */
void FUN_0011fe80(void *page)
{
  texture_page_verify(page);
  data_dispose(*(data_t **)((char *)page + 0x18));
  debug_free(page, "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x45);
}


/* texture_page_textures_begin (0x11feb0) — verify a texture page, assert it
 * holds no unsorted textures, then set the flag at +0x00.
 *
 * Binary evidence (0x11feb0..0x11fee3):
 *   MOV ESI,[EBP+8]; CALL 0x11fd50   texture_page_verify(page) — page in ESI
 *   CMP byte [ESI],0; JZ              assert "!texture_page->contains_unsorted_
 *                                     textures" (texture_page.c, line 0x4e),
 *                                     display_assert + system_exit(-1), no
 *                                     cleanup between (stdcall-cast shape)
 *   MOV byte [ESI],1                  set the byte at +0x00
 * The assert string names the +0x00 byte contains_unsorted_textures.
 */
void texture_page_textures_begin(void *page)
{
  texture_page_verify(page);
  if (*(char *)page != 0) {
    ((fatal_assert_stdcall_fn)(void *)display_assert)(
      "!texture_page->contains_unsorted_textures",
      "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x4e, 1);
    system_exit(-1);
  }
  *(char *)page = 1;
}


/* FUN_0011fef0 — verify a texture page, then fetch one texture datum from it.
 *
 * Name left as FUN_: kb.json carries no name and this function contains no
 * string or assert that proves one.
 *
 * Reference shape (0011fef0-0011ff0e): two cdecl stack args.
 *   MOV ESI,[EBP+8]            page
 *   CALL 0x11fd50              texture_page_verify(page) — page in ESI
 *                              (kb.json @<esi>), so no push is emitted
 *   MOV EAX,[EBP+0xc]          datum_handle
 *   MOV ECX,[ESI+0x18]         page->+0x18, the same data_t* field
 *                              texture_page_new stores and FUN_0011fe80
 *                              disposes
 *   PUSH EAX; PUSH ECX         cdecl: last arg pushed first, so the call is
 *                              datum_get(data, datum_handle)
 *   CALL 0x119320; ADD ESP,8
 *   RET                        datum_get's EAX return is the return value
 */
void *FUN_0011fef0(void *page, int datum_handle)
{
  texture_page_verify(page);
  return datum_get(*(data_t **)((char *)page + 0x18), datum_handle);
}


/* qsort_texture_indexes — sort predicate over texture indexes in the current
 * texture page (name from the 2276 symbol dump, kb.json tier T1).
 *
 * Reference shape (0011ff10-0011ff66). Two cdecl stack args, both read with
 * MOVSX word — so they are signed int16 indexes, not pointers:
 *   MOV ESI,[0x46e808]; MOV EDI,ESI; CALL 0x11fd50   texture_page_verify(page)
 *   MOV ESI,[0x46e808]; MOV EBX,ESI; CALL 0x11fd50   texture_page_verify(page)
 *   MOVSX EAX,[EBP+8];  MOV ECX,[EBX+0x18]; PUSH EAX; PUSH ECX; CALL datum_get
 *   MOVSX EDX,[EBP+0xc];MOV EAX,[EDI+0x18]; PUSH EDX; PUSH EAX; CALL datum_get
 *   MOVSX ECX,[EAX+0xa]  entry for index_b
 *   MOVSX EDX,[ESI+0xa]  entry for index_a
 *   SUB ECX,EDX; XOR EAX,EAX; TEST ECX,ECX; SETG AL; RET
 *
 * The verify+datum_get pair is FUN_0011fef0 inlined twice (same global page,
 * loaded from 0x46e808 once per inlined copy). MSVC hoisted both verify calls
 * ahead of both datum_get calls; that is the order the binary executes, so it
 * is the order written here. The EDI copy (first global load) feeds the
 * index_b lookup and the EBX copy (second load) feeds index_a — same pointer
 * value either way, but the two loads are reproduced literally.
 *
 * page+0x18 is the "texture page textures" data_t* (see texture_page_new).
 * Entry field +0x0a is the int16 cross-confirmed as the height by the
 * FUN_0011fef0 call sites in model_animations.c / bitmap_utilities.c.
 *
 * Return is a byte predicate (XOR EAX,EAX + SETG AL): true when the index_b
 * entry's +0x0a is greater than the index_a entry's, i.e. a descending sort
 * by that field. The subtraction is performed before the sign test exactly as
 * the reference does it; do not re-spell it as a direct comparison.
 */
bool qsort_texture_indexes(int16_t index_a, int16_t index_b)
{
  char *page_b;
  char *page_a;
  char *entry_a;
  int16_t *entry_a_field_0a;
  char *entry_b;

  page_b = *(char **)0x46e808;
  texture_page_verify(page_b);
  page_a = *(char **)0x46e808;
  texture_page_verify(page_a);

  entry_a = (char *)datum_get(*(data_t **)(page_a + 0x18), index_a);
  entry_a_field_0a = (int16_t *)(entry_a + 0xa);
  entry_b = (char *)datum_get(*(data_t **)(page_b + 0x18), index_b);

  return (*(int16_t *)(entry_b + 0xa) - *entry_a_field_0a) > 0;
}


/* FUN_0011ff70 — pack every live texture of a texture page into channels
 * (0x11ff70..0x120249). Returns false when a texture does not fit.
 *
 * Name left as FUN_: kb.json has no name for it. Assert strings (texture_page.c
 * lines 0x124, 0x105, 0x168) are the only text evidence.
 *
 * Reference shape:
 *   - spacing assert reads int16 +0x0c; ceiling_power2 gets it zero-extended
 *     and its int result is compared against the MOVSX field.
 *   - texture_page_verify(page) via ESI (kb.json @<esi>).
 *   - Walks textures->data in 0xc-byte steps for current_count (+0x2e)
 *     datums. Each datum with a nonzero first word adds its index to a local
 *     int16 array of 0x8000 entries at [EBP-0x10c24].
 *   - Stores the page at 0x46e808 (read by qsort_texture_indexes) and sorts the
 *     array with FUN_00091da0(indexes, count, qsort_texture_indexes).
 *   - spacing (+0x0c) is used only when more than one index exists; mask is
 *     ceiling_power2(spacing)-1 when spacing is nonzero, else 0.
 *   - +0x14 is cleared, then gets width*height added for each texture placed.
 *   - The channel table at [EBP-0xc24] holds 0x200 int16 triples
 *     {x, y, free height}. Channel 0 starts as {0, 0, page height (+0x0a)}.
 *   - Each iteration repeats texture_page_verify's width/height assert (0x105)
 *     and the data_verify call inline (no NULL-page assert), then datum_get.
 *     The texture datum's +0x08/+0x0a (padded by 2*spacing and rounded up by
 *     mask) choose the first channel where it fits. The datum's +0x04/+0x06 get
 *     the channel's x/y plus spacing.
 *   - Line 0x168 asserts after the increment (CMP DX,0x200 on count+1).
 */
bool FUN_0011ff70(void *page)
{
  int16_t texture_indexes[0x8000];
  int16_t channels[0x200 * 3];
  char *texture_page;
  data_t *textures;
  int16_t *datum;
  int16_t datum_index;
  int16_t index_count;
  int16_t spacing;
  int16_t spacing_mask;
  int16_t channel_count;
  int16_t texture_index;

  texture_page = (char *)page;

  if (*(int16_t *)(texture_page + 0xc) != 0 &&
      ceiling_power2(*(int16_t *)(texture_page + 0xc)) !=
        *(int16_t *)(texture_page + 0xc)) {
    ((fatal_assert_stdcall_fn)(void *)display_assert)(
      "texture_page->spacing==0 || "
      "ceiling_power2(texture_page->spacing)==texture_page->spacing",
      "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x124, 1);
    system_exit(-1);
  }

  texture_page_verify(texture_page);

  textures = *(data_t **)(texture_page + 0x18);
  datum = (int16_t *)textures->data;
  index_count = 0;
  for (datum_index = 0; datum_index < textures->current_count;
       datum_index++, datum += 6) {
    if (*datum != 0) {
      texture_indexes[index_count] = datum_index;
      index_count++;
    }
  }

  *(char **)0x46e808 = texture_page;
  FUN_00091da0(texture_indexes, index_count, (void *)qsort_texture_indexes);

  spacing = index_count > 1 ? *(int16_t *)(texture_page + 0xc) : 0;
  spacing_mask = spacing != 0 ? (int16_t)(ceiling_power2(spacing) - 1) : 0;

  *(int32_t *)(texture_page + 0x14) = 0;
  channel_count = 1;
  channels[0] = 0;
  channels[1] = 0;
  channels[2] = *(int16_t *)(texture_page + 0xa);

  for (texture_index = 0; texture_index < index_count; texture_index++) {
    int16_t *texture;
    int16_t *channel;
    int16_t width;
    int16_t height;
    int16_t remaining_height;
    int16_t channel_index;

    datum_index = texture_indexes[texture_index];

    if (*(int16_t *)(texture_page + 8) <= 0 ||
        *(int16_t *)(texture_page + 0xa) <= 0) {
      ((fatal_assert_stdcall_fn)(void *)display_assert)(
        "texture_page->width>0 && texture_page->height>0",
        "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x105, 1);
      system_exit(-1);
    }
    data_verify(*(data_t **)(texture_page + 0x18));
    texture =
      (int16_t *)datum_get(*(data_t **)(texture_page + 0x18), datum_index);

    width = (int16_t)(texture[4] + spacing * 2);
    height = (int16_t)(texture[5] + spacing * 2);
    if ((width & spacing_mask) != 0) {
      width = (int16_t)((width & ~spacing_mask) + spacing);
    }
    if ((height & spacing_mask) != 0) {
      height = (int16_t)((height & ~spacing_mask) + spacing);
    }

    for (channel_index = 0; channel_index < channel_count; channel_index++) {
      channel = &channels[channel_index * 3];
      if (width <= *(int16_t *)(texture_page + 8) - channel[0] &&
          height <= channel[2]) {
        break;
      }
    }
    if (channel_index >= channel_count) {
      return false;
    }

    channel = &channels[channel_index * 3];
    remaining_height = (int16_t)(channel[2] - height);
    texture[2] = (int16_t)(channel[0] + spacing);
    texture[3] = (int16_t)(channel[1] + spacing);
    if (remaining_height != 0) {
      int16_t *new_channel;

      new_channel = &channels[channel_count * 3];
      channel_count++;
      if (channel_count >= 0x200) {
        ((fatal_assert_stdcall_fn)(void *)display_assert)(
          "channel_count<MAXIMUM_TEXTURE_CHANNELS",
          "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x168, 1);
        system_exit(-1);
      }
      new_channel[0] = channel[0];
      new_channel[1] = (int16_t)(channel[1] + height);
      new_channel[2] = remaining_height;
    }
    channel[0] = (int16_t)(channel[0] + width);
    channel[2] = height;
    *(int32_t *)(texture_page + 0x14) += height * width;
  }

  return true;
}
