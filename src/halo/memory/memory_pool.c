/* Validate pool structure and all blocks in the linked list (0x11e430).
 * Checks pool/block signatures, linked list consistency, and address bounds. */
void FUN_0011e430(void *pool)
{
  char *p = (char *)pool;
  char *block;
  char *previous_block;

  if (*(int *)p != 0x706f6f6c) {
    display_assert("pool->signature==POOL_SIGNATURE",
                   "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x154, 1);
    system_exit(-1);
  }

  if (*(int *)(p + 0x28) <= 0) {
    display_assert("pool->size>0", "c:\\halo\\SOURCE\\memory\\memory_pool.c",
                   0x155, 1);
    system_exit(-1);
  }

  block = *(char **)(p + 0x30);
  previous_block = NULL;

  while (block != NULL) {
    if (*(char **)(block + 0x10) != previous_block) {
      display_assert("block->previous_block==previous_block",
                     "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x160, 1);
      system_exit(-1);
    }

    if (*(int *)(block + 0x0c) == 0 && *(char **)(p + 0x34) != block) {
      display_assert("block->next_block || pool->last_block==block",
                     "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x161, 1);
      system_exit(-1);
    }

    if (*(int *)block != 0x68656164) {
      display_assert("block->header_signature==BLOCK_HEADER_SIGNATURE",
                     "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x163, 1);
      system_exit(-1);
    }

    if (*(int *)(block + 0x14) != 0x7461696c) {
      display_assert("block->trailer_signature==BLOCK_TRAILER_SIGNATURE",
                     "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x164, 1);
      system_exit(-1);
    }

    if ((unsigned int)block < (unsigned int)*(int *)(p + 0x24)) {
      display_assert("(byte *)block>=(byte *)pool->base_address",
                     "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x166, 1);
      system_exit(-1);
    }

    if ((unsigned int)(*(int *)(block + 0x04) + (int)block) >
        (unsigned int)(*(int *)(p + 0x28) + *(int *)(p + 0x24))) {
      display_assert(
        "(byte *)block+block->size<=(byte *)pool->base_address+pool->size",
        "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x167, 1);
      system_exit(-1);
    }

    previous_block = block;
    block = *(char **)(block + 0x0c);
  }
}

/* Validate a block reference and return the block header pointer (0x11e5a0).
 * Checks that the reference is non-null, verifies pool integrity, confirms
 * the block's stored reference matches, and walks the list to find it. */
void *FUN_0011e5a0(void *pool, void **block_reference)
{
  char *p = (char *)pool;
  char *data;
  char *block;
  char *other_block;

  if (block_reference == NULL || *block_reference == NULL) {
    display_assert("reference && (*reference)",
                   "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x174, 1);
    system_exit(-1);
  }

  FUN_0011e430(pool);

  data = (char *)*block_reference;
  block = data - 0x18;

  if (*(void **)(data - 0x10) != (void *)block_reference) {
    char *msg =
      csprintf((char *)0x5ab100, "expected reference %08x but got %08x",
               *(int *)(data - 0x10), (int)block_reference);
    display_assert(msg, "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x17b, 1);
    system_exit(-1);
  }

  other_block = *(char **)(p + 0x30);
  while (other_block != NULL) {
    if (block == other_block)
      return block;
    other_block = *(char **)(other_block + 0x0c);
  }

  display_assert("other_block", "c:\\halo\\SOURCE\\memory\\memory_pool.c",
                 0x184, 1);
  system_exit(-1);
  return block;
}

/* Allocate a block from a memory pool (0x11e6c0).
 * Block header is 0x18 bytes: 'head', size, reference, next, prev, 'tail'.
 * Blocks are allocated sequentially after the last block (or at base if empty).
 * Returns the user data pointer (past the header) via *block_reference. */
bool memory_pool_block_new(void *pool, void **block_reference, int size)
{
  char *p = (char *)pool;
  unsigned int total_size;
  char *alloc_point;

  total_size = (unsigned int)(size + 0x18);
  if ((total_size & 3) != 0)
    total_size = (total_size | 3) + 1;

  FUN_0011e430(pool);

  if (size < 0) {
    display_assert("size>=0", "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x7c,
                   1);
    system_exit(-1);
  }

  if (*(int *)(p + 0x34) != 0) {
    char *last = *(char **)(p + 0x34);
    alloc_point = last + *(int *)(last + 4);
  } else {
    alloc_point = *(char **)(p + 0x24);
  }

  if ((unsigned int)alloc_point + total_size >
        (unsigned int)(*(int *)(p + 0x28) + *(int *)(p + 0x24)) ||
      alloc_point == NULL) {
    return false;
  }

  *(int *)(alloc_point + 0x00) = 0x68656164;
  *(unsigned int *)(alloc_point + 0x04) = total_size;
  *(void ***)(alloc_point + 0x08) = block_reference;
  *(int *)(alloc_point + 0x0c) = 0;
  *(int *)(alloc_point + 0x10) = *(int *)(p + 0x34);
  *(int *)(alloc_point + 0x14) = 0x7461696c;

  if (*(int *)(p + 0x30) == 0)
    *(char **)(p + 0x30) = alloc_point;

  if (*(int *)(p + 0x34) != 0)
    *(char **)(*(char **)(p + 0x34) + 0x0c) = alloc_point;

  *(char **)(p + 0x34) = alloc_point;

  {
    int free_size = *(int *)(p + 0x2c) - (int)total_size;
    *(int *)(p + 0x2c) = free_size;
    if (free_size < 0) {
      display_assert("pool->free_size>=0",
                     "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0x9c, 1);
      system_exit(-1);
    }
  }

  *block_reference = (void *)(alloc_point + 0x18);
  return true;
}

/* Free a block from a memory pool (0x11e7a0).
 * Resolves the block header via FUN_0011e5a0, adds block size back to
 * free_size, unlinks from the doubly-linked list, then zeroes the memory. */
void memory_pool_block_free(void *pool, void **block_reference)
{
  char *p = (char *)pool;
  char *block = (char *)FUN_0011e5a0(pool, block_reference);
  int free_size;

  free_size = *(int *)(p + 0x2c) + *(int *)(block + 4);
  *(int *)(p + 0x2c) = free_size;

  if (free_size > *(int *)(p + 0x28)) {
    display_assert("pool->free_size<=pool->size",
                   "c:\\halo\\SOURCE\\memory\\memory_pool.c", 0xe6, 1);
    system_exit(-1);
  }

  if (*(int *)(block + 0x10) == 0) {
    *(int *)(p + 0x30) = *(int *)(block + 0x0c);
  } else {
    *(int *)(*(int *)(block + 0x10) + 0x0c) = *(int *)(block + 0x0c);
  }

  if (*(int *)(block + 0x0c) != 0) {
    *(int *)(*(int *)(block + 0x0c) + 0x10) = *(int *)(block + 0x10);
  } else {
    *(int *)(p + 0x34) = *(int *)(block + 0x10);
  }

  csmemset(block, 0, *(int *)(block + 4));
}
