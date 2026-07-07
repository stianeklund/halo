/* 0x11c820: Validate an lrar_cache header ('lrar' magic @+0x44, minimum<maximum
 * address, block_size(+0x2c)>=1, block_count(+0x38)>=1). On corruption formats
 * the standard message into the shared scratch buffer (0x5ab100) and hits
 * display_assert + system_exit(-1). cache passed in EAX.
 * Source: c:\halo\SOURCE\memory\lrar_cache.c line 0x199. */
void lruv_update_function_pointers(int cache)
{
  if ((*(int *)(cache + 0x44) != 0x6c726172) ||
      (*(unsigned int *)(cache + 0x28) <= *(unsigned int *)(cache + 0x24)) ||
      (*(int *)(cache + 0x2c) < 1) ||
      (*(short *)(cache + 0x38) < 1)) {
    display_assert(csprintf((char *)0x5ab100,
                            "lrar cache %s @%p appears to be corrupt",
                            cache, cache),
                   "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x199, 1);
    system_exit(-1);
  }
}

/* 0x11c7c0: Validate an lrar_cache block record ('klbR' magic @block+4, size
 * (block+0xc) in [0, cache->block_size@+0x2c), address (block+8) within the
 * cache's [minimum@+0x24, maximum@+0x28) range). On corruption formats
 * "... block @%p appears to be corrupt" and hits display_assert +
 * system_exit(-1). cache in EAX, block in ESI. Source: lrar_cache.c 0x186. */
void FUN_0011c7c0(int cache, int block)
{
  int size = *(int *)(block + 0xc);
  if ((*(int *)(block + 4) != 0x52626c6b) ||
      (size < 0) ||
      (*(int *)(cache + 0x2c) <= size) ||
      (*(unsigned int *)(block + 8) < *(unsigned int *)(cache + 0x24)) ||
      (*(unsigned int *)(cache + 0x28) < *(unsigned int *)(block + 8) + size)) {
    display_assert(csprintf((char *)0x5ab100,
                            "lrar cache %s @%p block @%p appears to be corrupt",
                            cache, cache, block),
                   "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x186, 1);
    system_exit(-1);
  }
}

/* 0x11ca60: Look up and validate an lrar_cache block record by index. Validates
 * the cache header (lruv_update_function_pointers), bounds-checks block_index
 * against block_count (+0x38; line 0x16e), computes the record pointer
 * (block_index*0x10 + block_array@+0x30), validates it (FUN_0011c7c0), and
 * returns it. block_index in AX, cache in EDI. Source: lrar_cache.c. */
int FUN_0011ca60(short block_index, int cache)
{
  int block;

  lruv_update_function_pointers(cache);
  if ((block_index < 0) || (*(short *)(cache + 0x38) <= block_index)) {
    display_assert("block_index>=0 && block_index<cache->block_count",
                   "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x16e, 1);
    system_exit(-1);
  }
  block = (int)block_index * 0x10 + *(int *)(cache + 0x30);
  FUN_0011c7c0(cache, block);
  return block;
}

/* 0x11c5d0: Touch an lrar_cache block (FUN_0011c210 relink bookkeeping) and
 * return its byte offset from the cache's minimum address
 * (block - cache->minimum_address@+0x24). block in EAX, cache in ECX. */
int FUN_0011c5d0(int block, int cache)
{
  FUN_0011c210(cache, block);
  return block - *(int *)(cache + 0x24);
}

/* 0x11c7a0: Free an lrar_cache block's user allocation. If the block's user
 * pointer (block[0]) is non-NULL, invokes the cache free callback (cache+0x40)
 * on it and clears the slot. block in ESI, cache on the stack.
 * (kb name lruv_allocation_size is a punpckhdq PDB misnomer; this is the
 * per-block free helper.) */
void lruv_allocation_size(int block, int cache)
{
  if (*(int *)block != 0) {
    (*(void (**)(int))(cache + 0x40))(*(int *)block);
    *(int *)block = 0;
  }
}

/* 0x11d090: Validate an lru_cache header ('curl' magic @+0x44; head@+0x34,
 * block_size@+0x2c, block_array@+0x30 non-zero; maximum@+0x28 ==
 * page_size(+0x20) * page_count(+0x24); page_count>=0x10; used(+0x40) in
 * [0, page_size]). On corruption formats "lru cache %s @%p appears to be
 * corrupt" and hits display_assert + system_exit(-1). cache in EAX.
 * Source: c:\halo\SOURCE\memory\lru_cache.c line 0x16b. */
void FUN_0011d090(int cache)
{
  int page_size;

  if ((*(int *)(cache + 0x44) == 0x6c727563) &&
      (*(int *)(cache + 0x34) != 0) &&
      (*(int *)(cache + 0x2c) != 0) &&
      (*(int *)(cache + 0x30) != 0)) {
    page_size = *(int *)(cache + 0x20);
    if ((*(int *)(cache + 0x28) ==
         page_size * *(unsigned int *)(cache + 0x24)) &&
        (0xf < *(unsigned int *)(cache + 0x24)) &&
        (-1 < page_size) &&
        (-1 < *(int *)(cache + 0x40)) &&
        (*(int *)(cache + 0x40) <= page_size)) {
      return;
    }
  }
  display_assert(csprintf((char *)0x5ab100,
                          "lru cache %s @%p appears to be corrupt",
                          cache, cache),
                 "c:\\halo\\SOURCE\\memory\\lru_cache.c", 0x16b, 1);
  system_exit(-1);
}

/* 0x11d300: Return an lru_cache's remaining free capacity: page_size(+0x20)
 * minus the used page count (+0x40), after validating the cache header
 * (FUN_0011d090, cache in EAX). cdecl(cache).
 * Source: c:\halo\SOURCE\memory\lru_cache.c */
int FUN_0011d300(int cache)
{
  FUN_0011d090(cache);
  return *(int *)(cache + 0x20) - *(int *)(cache + 0x40);
}

/* 0x11d320: lru_cache new-block allocator ('jblU' variant, magic 0x55626c6a).
 * cdecl(cache, value) -> payload pointer (Ghidra's __thiscall/ECX guess is
 * wrong: prologue does MOV ESI,[EBP+8] (cache) and MOV EAX,[EBP+0xc] (value),
 * the epilog is a plain RET, and PUSH ECX at 0x11d323 only reserves the local
 * LRU-serial slot).
 *
 * Validates the 'curl' header (FUN_0011d090, cache in EAX). If the live count
 * (cache+0x40) is below capacity (cache+0x20), it appends a fresh record at
 * cache+0x34 + count*stride(cache+0x24) and bumps the count. Once full it walks
 * all `count` records, validating each (FUN_0011d010), and picks the unlocked
 * block (block+4 bit0 == 0) with the smallest LRU serial (block+8, unsigned) as
 * the eviction victim; if every block is locked it returns NULL. The victim's
 * user pointer (block[0]) is passed to the evict callback (cache+0x30) before
 * reuse.
 *
 * The selected/new block is then initialised: +0x00 = value, +0x04 = magic
 * (bit0 clear/unlocked), +0x08 = next LRU serial (cache+0x3c, post-increment),
 * +0x0c = 0. The init callback (cache+0x2c) is invoked with (value, payload)
 * and the payload pointer (block+0x10) is returned (NULL on failure).
 * Source: c:\halo\SOURCE\memory\lru_cache.c */
void *FUN_0011d320(int cache, int value)
{
  int count;
  int block;
  int victim;
  unsigned int victim_serial;
  int i;

  victim = 0;
  FUN_0011d090(cache);                     /* validate 'curl' header (cache@eax) */
  count = *(int *)(cache + 0x40);          /* +0x40 live block count */
  if (count == *(int *)(cache + 0x20)) {   /* +0x20 capacity: cache full -> evict LRU */
    block = *(int *)(cache + 0x34);        /* +0x34 first block record */
    victim_serial = 0;                     /* dead init; only read once victim != 0 */
    if (count < 1) {
      return (void *)0;
    }
    i = 0;
    do {
      FUN_0011d010(cache, (void *)block);  /* validate block header */
      if (((*(unsigned char *)(block + 4) & 1) == 0) &&   /* +0x04 bit0 == unlocked */
          ((victim == 0) ||
           (*(unsigned int *)(block + 8) < victim_serial))) {  /* +0x08 older serial */
        victim_serial = *(unsigned int *)(block + 8);
        victim = block;
      }
      i = i + 1;
      block = block + *(int *)(cache + 0x24);   /* +0x24 stride advance */
    } while (i < *(int *)(cache + 0x40));        /* re-read count each iter (faithful) */
    if (victim == 0) {
      return (void *)0;                          /* all blocks locked */
    }
    (*(void (**)(int))(cache + 0x30))(*(int *)victim);  /* +0x30 evict callback(user_ptr) */
    block = victim;
  }
  else {                                   /* room to append */
    block = *(int *)(cache + 0x24) * count + *(int *)(cache + 0x34);
    *(int *)(cache + 0x40) = count + 1;
  }

  if (block == 0) {
    return (void *)0;
  }
  *(int *)block = value;                    /* +0x00 user value */
  *(int *)(block + 4) = 0x55626c6a;         /* +0x04 'jblU' magic, unlocked */
  *(int *)(block + 8) = *(int *)(cache + 0x3c);           /* +0x08 LRU serial */
  *(int *)(cache + 0x3c) = *(int *)(cache + 0x3c) + 1;    /* +0x3c serial clock++ */
  *(int *)(block + 0xc) = 0;                /* +0x0c link */
  (*(void (**)(int, void *))(cache + 0x2c))(*(int *)block,
                                            (void *)(block + 0x10));  /* +0x2c init cb(value,payload) */
  return (void *)(block + 0x10);            /* +0x10 payload */
}

/* 0x11c5f0: 'hlbA' least-recently-used contiguous block allocator. Runs the
 * cache post-touch bookkeeping (FUN_0011c290, cache in @eax), grows the request
 * by the 0x10-byte block header and rounds it up to 4 bytes, then rejects it if
 * it cannot fit under the cache capacity (+0x20). It walks the forward-linked
 * block list (head @cache+0x2c, next @block+0xc), touching each visited block
 * (FUN_0011c210, cache in @ebx + block in @esi) and remembering the first free
 * (bit0-clear) block. When the running placement offset leaves room before the
 * next block -- or the list end is reached -- it checks the region capacity:
 * on a fit it evicts every not-yet-marked block in [free_block, cursor) via the
 * free callback (cache+0x34), writes a fresh block record (data @+0, 'hlbA'
 * magic 0x41626c68 @+4, byte size @+8, next @+0xc), invokes the allocate/init
 * callback (cache+0x30)(data, user_ptr), relinks the record as the new head
 * (cache+0x2c) and returns its user area (block+0x10); on a capacity miss it
 * resets the cursor to the region base (cache+0x24), clears the placement
 * state and retries the whole list once (16-bit wrap counter) before giving up.
 * Returns NULL on an oversized request or after the single retry fails. Block
 * flags share the magic dword: bit0 = in-use, bit1 = already-freed/marked.
 * cdecl(cache, size, data). This function contains no integrity asserts, so no
 * source file/line is fixed by the binary; it lives in the memory-cache TU.
 * Source: c:\halo\SOURCE\memory (lru allocator; exact file/line not pinned). */
void *FUN_0011c5f0(int cache, int size, void *data)
{
  void *result;
  int *prev_block;
  int *cur;
  int *free_block;
  int *evict;
  int *new_block;
  int base_offset;
  short wrap_count;
  int prev_wrapped;

  result = (void *)0x0;
  FUN_0011c290(cache);

  size = size + 0x10;
  if ((size & 3) != 0) {
    size = (size | 3) + 1;
  }
  if ((size < 0) || (*(int *)(cache + 0x20) < size)) {
    return (void *)0x0;
  }

  prev_block = *(int **)(cache + 0x2c);   /* head@+0x2c */
  free_block = (int *)0x0;
  wrap_count = 0;
  if (prev_block == (int *)0x0) {
    cur = (int *)0x0;
  } else {
    cur = (int *)prev_block[3];           /* head->next */
  }

  do {
    if (prev_block == (int *)0x0) {
      base_offset = 0;
    } else {
      FUN_0011c210(cache, (int)prev_block);
      base_offset =
        (prev_block[2] - *(int *)(cache + 0x24)) + (int)prev_block;
    }

    if (cur == (int *)0x0) {
      goto cap_check;
    }

    FUN_0011c210(cache, (int)cur);
    FUN_0011c210(cache, (int)cur);
    if ((base_offset + size) <=
        (int)((int)cur - *(int *)(cache + 0x24))) {
      goto cap_check;   /* candidate fits before this block */
    }

    /* current block overlaps the candidate range */
    if ((*(unsigned char *)((int)cur + 4) & 1) != 0) {
      /* in-use: step past it and restart the free run */
      prev_block = cur;
      cur = (int *)cur[3];
      free_block = (int *)0x0;
    } else {
      /* free: remember the first free block, keep scanning */
      if (free_block == (int *)0x0) {
        free_block = cur;
      }
      cur = (int *)cur[3];
    }
    goto cont;

  cap_check:
    if (*(int *)(cache + 0x20) < (base_offset + size)) {
      /* no room before region end: reset cursor to base and wrap once */
      cur = *(int **)(cache + 0x24);
      prev_block = (int *)0x0;
      free_block = (int *)0x0;
      prev_wrapped = (wrap_count != 0);
      wrap_count = (short)(wrap_count + 1);
      if (prev_wrapped) {
        return result;
      }
    } else {
      /* fits: evict unmarked blocks in [free_block, cur), then write record */
      for (evict = free_block;
           (evict != (int *)0x0) && (evict != cur);
           evict = (int *)evict[3]) {
        if ((*(unsigned char *)((int)evict + 4) & 2) == 0) {
          (*(void (**)(int))(cache + 0x34))(evict[0]);
          *(int *)((int)evict + 4) =
            (*(int *)((int)evict + 4) & 0xfffffffeU) | 2;
        }
      }
      new_block = (int *)(*(int *)(cache + 0x24) + base_offset);
      result = (void *)((int)new_block + 0x10);
      new_block[2] = size;                /* size   @+0x8 */
      new_block[1] = 0x41626c68;          /* 'hlbA' @+0x4 */
      new_block[0] = (int)data;           /* data   @+0x0 */
      new_block[3] = (int)cur;            /* next   @+0xc */
      (*(void (**)(void *, void *))(cache + 0x30))(data, result);
      if (prev_block != (int *)0x0) {
        prev_block[3] = (int)new_block;
      }
      *(int **)(cache + 0x2c) = new_block;
    }

  cont:
    if (result != (void *)0x0) {
      return result;
    }
  } while (1);
}

/* 0x11c530: Mark a cached block dirty / most-recently-used. Requires a
 * non-NULL user pointer (asserted, line 0x12a). Runs the cache post-touch
 * bookkeeping (FUN_0011c290, cache in EAX) then the block-relink step
 * (FUN_0011c210, cache in EBX + block header in ESI, header = pointer-0x10),
 * and finally SETS bit0 of the dword at pointer-0xc (header+4), marking the
 * block in-use. Source: c:\halo\SOURCE\memory\lra_cache.c */
void FUN_0011c530(int cache, int block)
{
  int header = block - 0x10;
  if (block == 0) {
    display_assert("pointer", "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x12a, 1);
    system_exit(-1);
  }
  FUN_0011c290(cache);
  FUN_0011c210(cache, header);
  *(unsigned int *)(header + 4) |= 1;
}

/* 0x11c580: Release a cached block back to its cache. Requires a non-NULL user
 * pointer (asserted, line 0x13a). The block header sits 0x10 bytes before the
 * user pointer. Runs the cache post-touch bookkeeping (FUN_0011c290, cache in
 * EAX) then the block-release step (FUN_0011c210, cache in EBX + header in
 * ESI), and finally CLEARS bit0 of the dword at pointer-0xc (header+4),
 * marking the block not-in-use. Source: c:\halo\SOURCE\memory\lra_cache.c */
void FUN_0011c580(int cache, void *pointer)
{
  char *block = (char *)pointer - 0x10;
  if (pointer == NULL) {
    display_assert("pointer", "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x13a, 1);
    system_exit(-1);
  }
  FUN_0011c290(cache);
  FUN_0011c210(cache, (int)block);
  *(unsigned int *)(block + 4) &= 0xfffffffe;
}

/* 0x11cab0: Dispose of every live block in an lrar_cache. Refreshes the
 * function-pointer table (lruv_update_function_pointers, cache passed in EAX),
 * then walks the ring buffer from head(+0x34) to tail(+0x36), wrapping at
 * block_count(+0x38). For each block it validates the cache header ('lrar'
 * magic @+0x44, base<end, size>=1, count>=1; line 0x199), the block index
 * (line 0x16e) and the block record ('klbR' magic @block+4, size in range,
 * offset inside the data region; line 0x186) -- each failure formats the
 * "appears to be corrupt" message into the shared scratch buffer at 0x5ab100
 * and hits display_assert + system_exit(-1). When a block holds a non-NULL
 * user pointer (block[0]) it is passed to the free callback at cache+0x40 and
 * the slot is cleared. Finally head/tail are reset to -1 (0xffff). The
 * file/line/halt assert args are pushed before csprintf's args (Ghidra cdecl
 * arg mis-grouping). Source: c:\halo\SOURCE\memory\lrar_cache.c */
void lrar_cache_dispose(int cache)
{
  short block_index;
  int *block;
  int block_size;

  lruv_update_function_pointers(cache);
  block_index = *(short *)(cache + 0x34);
  while (block_index != -1) {
    while (1) {
      if ((*(int *)(cache + 0x44) != 0x6c726172) ||
          (*(unsigned int *)(cache + 0x28) <= *(unsigned int *)(cache + 0x24)) ||
          (*(int *)(cache + 0x2c) < 1) ||
          (*(short *)(cache + 0x38) <= 0)) {
        display_assert(csprintf((char *)0x5ab100,
                                "lrar cache %s @%p appears to be corrupt",
                                cache, cache),
                       "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x199, 1);
        system_exit(-1);
      }
      if ((block_index < 0) || (*(short *)(cache + 0x38) <= block_index)) {
        display_assert("block_index>=0 && block_index<cache->block_count",
                       "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x16e, 1);
        system_exit(-1);
      }
      block = (int *)(block_index * 0x10 + *(int *)(cache + 0x30));
      block_size = block[3];
      if ((block[1] != 0x52626c6b) ||
          (block_size < 0) ||
          (*(int *)(cache + 0x2c) <= block_size) ||
          ((unsigned int)block[2] < *(unsigned int *)(cache + 0x24)) ||
          (*(unsigned int *)(cache + 0x28) <
           (unsigned int)(block[2] + block_size))) {
        display_assert(csprintf((char *)0x5ab100,
                                "lrar cache %s @%p block @%p appears to be corrupt",
                                cache, cache, block),
                       "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x186, 1);
        system_exit(-1);
      }
      if (*block != 0) {
        (*(void (**)(int))(cache + 0x40))(*block);
        *block = 0;
      }
      if (block_index == *(short *)(cache + 0x36)) {
        goto done;
      }
      block_index = block_index + 1;
      if (block_index != *(short *)(cache + 0x38)) {
        break;
      }
      block_index = 0;
    }
  }
done:
  *(short *)(cache + 0x34) = (short)0xffff;
  *(short *)(cache + 0x36) = (short)0xffff;
}

/* 0x11cbf0: lrar_cache block allocator. Refreshes the cache's function-pointer
 * table (lruv_update_function_pointers, cache in @eax), rounds the request up
 * to the primary alignment (cache+0x20), then walks the block ring to find a
 * contiguous address range >= size that fits under cache->maximum_address
 * (+0x28), evicting overlapping blocks via the free callback (cache+0x40) as it
 * goes. Applies an optional secondary boundary alignment (cache+0x22, -1=none).
 * On a fit it writes a fresh block record (data ptr +0, 'lRbk' magic +4,
 * address +8, size +0xc), registers it via the insert callback
 * (cache+0x3c(data, index)), and links it as the new last/head block. Returns
 * the new block index (or 0xffff on reject). Integrity asserts reference
 * c:\halo\SOURCE\memory\lrar_cache.c. The @reg callee FUN_0011ca60 fetches a
 * block record (block_index @<ax>, cache @<edi>). */
short FUN_0011cbf0(int cache, int size, void *data)
{
  int new_block_index;
  unsigned int new_block_address;
  unsigned int search_address;
  unsigned int align_mask;
  int boundary;
  unsigned int boundary_mask;
  unsigned int aligned_base;
  short block_index;
  short next_index;
  int *block;
  unsigned int *new_block;
  int end;

  new_block_index = -1;
  lruv_update_function_pointers(cache);

  align_mask = (1 << (*(unsigned char *)(cache + 0x20) & 0x1f)) - 1;
  if (((unsigned int)size & align_mask) != 0) {
    size = (align_mask | (unsigned int)size) + 1;
  }

  if ((size >= 0) && (size <= *(int *)(cache + 0x2c))) {
    block_index = *(short *)(cache + 0x34);
    if (*(short *)(cache + 0x36) == -1) {
      new_block_index = 0;
    } else {
      new_block_index = *(short *)(cache + 0x36) + 1;
    }
    next_index = block_index;
    if (*(short *)(cache + 0x38) <= (short)new_block_index) {
      new_block_index = 0;
    }

    while (1) {
      if (*(short *)(cache + 0x36) == -1) {
        search_address = *(unsigned int *)(cache + 0x24);
      } else {
        block = (int *)FUN_0011ca60(*(short *)(cache + 0x36), cache);
        search_address = block[3] + block[2];
      }
      new_block_address = search_address;

      if (*(short *)(cache + 0x22) != -1) {
        boundary = 1 << (*(short *)(cache + 0x22) & 0x1f);
        boundary_mask = ~((unsigned int)boundary - 1U);
        aligned_base = boundary_mask & search_address;
        block_index = next_index;
        if (aligned_base != ((search_address + size) & boundary_mask)) {
          new_block_address = aligned_base + boundary;
        }
      }

      if (block_index != -1) {
        block = (int *)FUN_0011ca60(block_index, cache);
        while ((block_index == (short)new_block_index) ||
               ((search_address <= (unsigned int)block[2]) &&
                ((unsigned int)block[2] < new_block_address + size))) {
          if (block[0] != 0) {
            (*(void (**)(void *))(cache + 0x40))((void *)block[0]);
            block[0] = 0;
          }
          block[1] = -1;
          next_index = block_index + 1;
          if (*(short *)(cache + 0x38) <= next_index) {
            next_index = 0;
          }
          if ((*(int *)(cache + 0x44) != 0x6c726172) ||
              (*(unsigned int *)(cache + 0x28) <=
               *(unsigned int *)(cache + 0x24)) ||
              (*(int *)(cache + 0x2c) < 1) || (*(short *)(cache + 0x38) < 1)) {
            display_assert(csprintf((char *)0x5ab100,
                                    "lrar cache %s @%p appears to be corrupt",
                                    cache, cache),
                           "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x199, 1);
            system_exit(-1);
          }
          if ((next_index < 0) || (*(short *)(cache + 0x38) <= next_index)) {
            display_assert("block_index>=0 && block_index<cache->block_count",
                           "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x16e, 1);
            system_exit(-1);
          }
          block = (int *)(next_index * 0x10 + *(int *)(cache + 0x30));
          block_index = next_index;
          if ((*(int *)(next_index * 0x10 + 4 + *(int *)(cache + 0x30)) !=
               0x52626c6b) ||
              (block[3] < 0) || (*(int *)(cache + 0x2c) <= block[3]) ||
              ((unsigned int)block[2] < *(unsigned int *)(cache + 0x24)) ||
              (*(unsigned int *)(cache + 0x28) <
               (unsigned int)(block[2] + block[3]))) {
            display_assert(
              csprintf((char *)0x5ab100,
                       "lrar cache %s @%p block @%p appears to be corrupt",
                       cache, cache, block),
              "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x186, 1);
            system_exit(-1);
          }
        }
      }

      if (new_block_address + size <= *(unsigned int *)(cache + 0x28)) {
        break;
      }
      *(short *)(cache + 0x36) = -1;
    }

    new_block =
      (unsigned int *)((short)new_block_index * 0x10 + *(int *)(cache + 0x30));
    if ((new_block_address < *(unsigned int *)(cache + 0x24)) ||
        (*(unsigned int *)(cache + 0x28) < new_block_address + size)) {
      display_assert("adjusted_new_block_address>=cache->minimum_address && "
                     "adjusted_new_block_address+size<=cache->maximum_address",
                     "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x111, 1);
      system_exit(-1);
    }

    block_index = 0;
    if (0 < *(short *)(cache + 0x38)) {
      do {
        end = block_index * 0x10 + *(int *)(cache + 0x30);
        if ((*(int *)(block_index * 0x10 + 4 + *(int *)(cache + 0x30)) ==
             0x52626c6b) &&
            (search_address = *(unsigned int *)(end + 8),
             new_block_address <
               (unsigned int)(*(int *)(end + 0xc) + search_address)) &&
            (search_address < new_block_address + size)) {
          display_assert(
            "adjusted_new_block_address>=test_block->address+test_block->size "
            "|| adjusted_new_block_address+size<=test_block->address",
            "c:\\halo\\SOURCE\\memory\\lrar_cache.c", 0x11c, 1);
          system_exit(-1);
        }
        block_index = block_index + 1;
      } while (block_index < *(short *)(cache + 0x38));
    }

    new_block[3] = size;
    new_block[1] = 0x52626c6b;
    new_block[2] = new_block_address;
    new_block[0] = (unsigned int)data;
    (*(void (**)(void *, int))(cache + 0x3c))(data, new_block_index);
    *(short *)(cache + 0x36) = (short)new_block_index;
    if (next_index == -1) {
      *(short *)(cache + 0x34) = (short)new_block_index;
    } else {
      *(short *)(cache + 0x34) = next_index;
    }
  }

  return (short)new_block_index;
}

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

/* 0x11cfd0: Store a 4-byte value through the pointer arg. Trivial default
 * delete callback: writes the supplied value into the first field of the
 * record. Sibling of FUN_0011cfe0. Source: c:\halo\SOURCE\memory\lrar_cache.c
 */
void FUN_0011cfd0(int *ptr, int value)
{
  *ptr = value;
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

/* FUN_0011de10  (0x11de10)  lruv_cache.obj  -  lru_cache "new block" allocator.
 *
 * int FUN_0011de10(lruv_cache_t *cache, unsigned int size)
 *
 * Allocates a cache block large enough to hold `size` bytes and returns its
 * datum handle, or NONE(-1) if no room could be made / the datum pool is full.
 * param_2 is a BYTE COUNT, not a block index (Ghidra's kb decl was wrong):
 * desired_page_count = ceil(size / (1 << cache->page_size_bits)).
 *
 * Walks the LRU list treating cache page space as alternating gap/block
 * segments, folds each segment into a 256-entry ring of candidate "holes",
 * picks the least-recently-used contiguous run of desired_page_count pages
 * (min usage-stamp, tie-broken by smaller span), evicts every block
 * overlapping the chosen run (plus the tracked oldest-unlocked block when the
 * datum pool is exhausted), allocates a new datum, links it into the LRU list,
 * stamps it with cache->field_30 and returns the new handle.
 *
 * A block is "locked" (not reclaimable) if cache->query_cb(handle) is true OR
 * its usage stamp (block+0x14) equals cache->field_30. NONE = -1. All stamp
 * compares are unsigned; all page-count compares signed. Verified against
 * disassembly 0x11de10-0x11e325. Asserts: c:\halo\SOURCE\memory\lruv_cache.c.
 * Frame is > 0x1000 (int holes[1024]) so MSVC emits _chkstk; clang covers it. */
int FUN_0011de10(void *cache, unsigned int size)
{
  lruv_cache_t *c = (lruv_cache_t *)cache;
  int holes[1024];              /* 256 holes x {block_index,max_stamp,start_page,page_count} */
  data_iter_t iter;
  lruv_cache_block_t *block;
  lruv_cache_block_t *rec;
  lruv_cache_block_t *next_rec;
  lruv_cache_block_t *head_rec;
  lruv_cache_block_t *new_rec;

  int shift;
  int desired_page_count;
  int accumulator;              /* current page position along the walk (EDI) */
  int walk_cursor;              /* current block datum handle, or NONE (EBP-0xc) */
  int pending_hole_block_index; /* block that will own the next opened hole (EBP-0x20) */

  int hole_write;               /* ring write cursor (EBP-0x8) */
  int hole_read;                /* ring finalize/read cursor (EBP-0x4) */
  int read_cursor;              /* per-segment accumulate cursor (SI) */
  int saved_read;
  int next_write;
  int base;                     /* holes[] element base = cursor*4 */

  unsigned int segment_stamp;   /* EBP-0x1c */
  int segment_page_count;       /* EBP-0x10 */
  int locked;
  int skip_accumulate;

  int oldest_unlocked_block;    /* EBP-0x18 */
  unsigned int oldest_unlocked_stamp; /* EBP-0x38 */
  unsigned int block_stamp;

  int have_best;                /* EBP+0xf */
  int best_block_index;         /* EBP-0x30 */
  unsigned int best_max_stamp;  /* EBP-0x2c */
  int best_start_page;          /* EBP-0x28 */
  int best_page_count;          /* EBP-0x24 */

  int new_block_index;
  int region_end;
  int block_end;

  shift = (int)((unsigned char)c->page_size_bits) & 0x1f;
  desired_page_count = (int)size >> shift;
  if ((size & ((1u << shift) - 1u)) != 0) {
    desired_page_count = desired_page_count + 1;
  }
  if (desired_page_count <= 0) {
    display_assert("desired_page_count>0",
                   "c:\\halo\\SOURCE\\memory\\lruv_cache.c", 0xe1, 1);
    system_exit(-1);
  }

  have_best = 0;
  accumulator = 0;
  hole_write = 0;
  hole_read = 0;
  pending_hole_block_index = -1;
  walk_cursor = c->first_block_index;
  oldest_unlocked_block = -1;
  oldest_unlocked_stamp = 0;
  best_block_index = 0;
  best_max_stamp = 0;
  best_start_page = 0;
  best_page_count = 0;
  segment_stamp = 0;
  segment_page_count = 0;

  if (c->page_count < 1) {
    return -1;
  }

  for (;;) {
    skip_accumulate = 0;

    /* open a fresh hole at the current page position, unless advancing the
     * write cursor would collide with the read side. */
    next_write = (hole_write == 0xff) ? 0 : (hole_write + 1);
    if (next_write != hole_read) {
      base = hole_write * 4;
      holes[base + 0] = pending_hole_block_index; /* owning block, or NONE */
      holes[base + 2] = accumulator;              /* start_page */
      holes[base + 1] = 0;                        /* max_stamp  */
      holes[base + 3] = 0;                        /* page_count */
      hole_write = (hole_write == 0xff) ? 0 : (hole_write + 1);
    }

    if (walk_cursor == -1) {
      /* trailing free region after the last block */
      segment_page_count = c->page_count - accumulator;
      segment_stamp = 0;
      accumulator = c->page_count;
    } else {
      block = (lruv_cache_block_t *)datum_get(c->blocks, walk_cursor);
      if (accumulator == block->first_page_index) {
        /* contiguous block segment */
        block_stamp = *(unsigned int *)&block->unk_14[0]; /* usage stamp @ +0x14 */
        segment_stamp = block_stamp;
        segment_page_count = block->page_count;
        locked = 0;
        if (c->query_cb != 0) {
          if (c->query_cb(walk_cursor) != 0) {
            locked = 1;
          }
        }
        if (block_stamp == (unsigned int)c->field_30) {
          locked = 1;
        } else if (!locked) {
          if (oldest_unlocked_block == -1 || block_stamp < oldest_unlocked_stamp) {
            oldest_unlocked_block = walk_cursor;
            oldest_unlocked_stamp = block_stamp;
          }
        }
        pending_hole_block_index = walk_cursor;
        walk_cursor = block->next_block_index;
        accumulator = block->first_page_index + block->page_count; /* block end */
        if (locked) {
          hole_read = hole_write;     /* a locked block breaks contiguity: flush ring */
          skip_accumulate = 1;
        }
      } else {
        /* gap segment before this block; block re-processed next iteration */
        segment_page_count = block->first_page_index - accumulator;
        segment_stamp = 0;
        if (segment_page_count <= 0) {
          display_assert("page_count>0",
                         "c:\\halo\\SOURCE\\memory\\lruv_cache.c", 0x137, 1);
          system_exit(-1);
        }
        accumulator = block->first_page_index;
      }
    }

    /* fold this segment into every open hole; finalize candidates FIFO. */
    if (!skip_accumulate) {
      read_cursor = hole_read;
      while (read_cursor != hole_write) {
        saved_read = read_cursor;
        base = read_cursor * 4;
        if (segment_stamp > (unsigned int)holes[base + 1]) {
          holes[base + 1] = (int)segment_stamp;
        }
        holes[base + 3] = holes[base + 3] + segment_page_count;
        if (holes[base + 3] >= desired_page_count) {
          if (!have_best
              || (unsigned int)holes[base + 1] < best_max_stamp
              || ((unsigned int)holes[base + 1] == best_max_stamp
                  && holes[base + 3] < best_page_count)) {
            best_block_index = holes[base + 0];
            best_max_stamp   = (unsigned int)holes[base + 1];
            best_start_page  = holes[base + 2];
            best_page_count  = holes[base + 3];
            have_best = 1;
          }
          if (hole_read != read_cursor) {
            display_assert("hole_read_index==hole_index",
                           "c:\\halo\\SOURCE\\memory\\lruv_cache.c", 0x15f, 1);
            system_exit(-1);
          }
          hole_read = (hole_read == 0xff) ? 0 : (hole_read + 1);
        }
        read_cursor = (saved_read == 0xff) ? 0 : (saved_read + 1);
      }
    }

    if (accumulator < c->page_count) {
      continue;
    }
    break;
  }

  if (!have_best) {
    return -1;
  }

  /* evict every block overlapping the chosen region. */
  region_end = desired_page_count + best_start_page;
  data_iterator_new(&iter, c->blocks);
  rec = (lruv_cache_block_t *)data_iterator_next(&iter);
  while (rec != 0) {
    block_end = rec->first_page_index + rec->page_count;
    if (rec->first_page_index < region_end && block_end > best_start_page) {
      if (c->query_cb != 0) {
        if (c->query_cb(iter.datum_handle) != 0) {
          display_assert(
              "!cache->locked_block_proc || !cache->locked_block_proc(iterator.index)",
              "c:\\halo\\SOURCE\\memory\\lruv_cache.c", 0x177, 1);
          system_exit(-1);
        }
      }
      lruv_block_delete(c, iter.datum_handle);
    }
    rec = (lruv_cache_block_t *)data_iterator_next(&iter);
  }

  /* datum pool exhausted: also evict the tracked oldest-unlocked block. */
  if (c->blocks->unk_48 == c->blocks->maximum_count && oldest_unlocked_block != -1) {
    if (best_block_index == oldest_unlocked_block) {
      rec = (lruv_cache_block_t *)datum_get(c->blocks, oldest_unlocked_block);
      best_block_index = rec->previous_block_index;
    }
    if (datum_get(c->blocks, oldest_unlocked_block) == 0) {
      display_assert("lruv_cache_block_get(cache, oldest_unlocked_block_index)",
                     "c:\\halo\\SOURCE\\memory\\lruv_cache.c", 0x188, 1);
      system_exit(-1);
    }
    if (c->query_cb != 0) {
      if (c->query_cb(oldest_unlocked_block) != 0) {
        display_assert(
            "!cache->locked_block_proc || !cache->locked_block_proc(oldest_unlocked_block_index)",
            "c:\\halo\\SOURCE\\memory\\lruv_cache.c", 0x189, 1);
        system_exit(-1);
      }
    }
    lruv_block_delete(c, oldest_unlocked_block);
  }

  /* allocate the new block datum and link it into the LRU list. */
  new_block_index = data_new_at_index(c->blocks);
  if (new_block_index == -1) {
    return -1;
  }
  new_rec = (lruv_cache_block_t *)datum_get(c->blocks, new_block_index);

  if (best_block_index == -1) {
    if (c->first_block_index == -1) {
      /* empty list */
      if (c->last_block_index != -1) {
        display_assert("cache->last_block_index==NONE",
                       "c:\\halo\\SOURCE\\memory\\lruv_cache.c", 0x198, 1);
        system_exit(-1);
      }
      new_rec->previous_block_index = -1;
      c->last_block_index = new_block_index;
      new_rec->next_block_index = c->first_block_index;
      c->first_block_index = new_block_index;
    } else {
      /* insert before the current head */
      head_rec = (lruv_cache_block_t *)datum_get(c->blocks, c->first_block_index);
      if (head_rec->previous_block_index != -1) {
        display_assert("next_block->previous_block_index==NONE",
                       "c:\\halo\\SOURCE\\memory\\lruv_cache.c", 0x1a0, 1);
        system_exit(-1);
      }
      new_rec->previous_block_index = -1;
      head_rec->previous_block_index = new_block_index;
      new_rec->next_block_index = c->first_block_index;
      c->first_block_index = new_block_index;
    }
  } else {
    /* insert immediately after best_block_index */
    rec = (lruv_cache_block_t *)datum_get(c->blocks, best_block_index);
    if (rec->next_block_index != -1) {
      next_rec = (lruv_cache_block_t *)datum_get(c->blocks, rec->next_block_index);
      new_rec->previous_block_index = next_rec->previous_block_index;
      next_rec->previous_block_index = new_block_index;
    } else {
      new_rec->previous_block_index = c->last_block_index;
      c->last_block_index = new_block_index;
    }
    rec = (lruv_cache_block_t *)datum_get(c->blocks, best_block_index);
    new_rec->next_block_index = rec->next_block_index;
    rec->next_block_index = new_block_index;
  }

  new_rec->first_page_index = best_start_page;
  new_rec->page_count = desired_page_count;
  *(int *)&new_rec->unk_14[0] = c->field_30;
  lruv_cache_verify(c, 1);
  return new_block_index;
}
