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
