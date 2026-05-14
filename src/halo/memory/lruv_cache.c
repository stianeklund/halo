/* 0x11d480: Compute the total allocation size needed for an lruv_cache
 * with the given maximum block count. Returns sizeof(lruv_cache_t) + data
 * allocation for the block datums. */
int lruv_cache_allocation_size(int maximum_block_count)
{
  return data_allocation_size(maximum_block_count, 0x1c) + 0x44;
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

/* FUN_0011db00 (0x11db00)
 *
 * Resize the lruv_cache to new_page_count pages.  Any block whose
 * page range (first_page_index + page_count) exceeds the new limit is
 * evicted via lruv_block_delete before the page_count field is updated.
 */
void FUN_0011db00(void *cache, int new_page_count)
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
