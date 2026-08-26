/* FUN_001bb430 — begin an async read of the next chunk into a decompression
 * read buffer.
 *
 * self (@<eax>) is the per-file decompression read-state block from
 * c:\halo\SOURCE\cache\cache_files_decompress_windows.c (TU confirmed via
 * the __FILE__ strings on the asserts below). No struct has been recovered
 * for it yet, so it is addressed by raw offset; only these fields are
 * touched by this function and only two have assert-proven names:
 *   +0xa8c  read_file_size        (assert "self->current_read_offset<=
 *                                  self->read_file_size")
 *   +0xa94  field_a94             (bytes remaining to read; the read size
 *                                  for this call is this value capped to
 *                                  0x20000)
 *   +0xaa8  current_read_offset   (assert-proven name)
 *   +0xab8  field_ab8 (int16)     (read-sequence counter: copied out to
 *                                  *request, then incremented)
 *
 * request points directly at the caller's read_sequence_index field
 * (dereferenced at offset 0 — the assert text is
 * "request->read_sequence_index==NONE", but nothing else about that
 * struct is touched here, so it is typed as a bare short pointer rather
 * than an invented struct).
 *
 * read_buffer_index selects one of NUMBER_OF_READ_BUFFERS (8) read
 * buffers; validated to be in [0,8).
 *
 * Call-site evidence for the two callees below (both currently
 * under-declared in kb.json as their generic decompiled void(void)/
 * void FUN_001bb190(void) forms — corrected here):
 *   - FUN_001baa50: the disassembly reloads [ebp+8] (request) into EAX
 *     immediately before this call, and that reload is otherwise dead
 *     (request is independently reloaded again afterward for the NONE
 *     assert) — the only explanation is that FUN_001baa50 takes request
 *     via @<eax> and returns the acquired read buffer pointer in EAX.
 *   - FUN_001bb190: disassembly shows 4 pushes (read_buffer_index,
 *     current_read_offset, capped size, buffer) immediately before the
 *     call, cleaned up by the caller's "ADD ESP,0x10" right after — proof
 *     of a 4-argument cdecl call, not the void(void) the decompiler
 *     rendered from the stale kb.json signature.
 *
 * Source: c:\halo\SOURCE\cache\cache_files_decompress_windows.c, asserts
 * at lines 0x5ec, 0x5ed, 0x601.
 */
void FUN_001bb430(char *self, short *request, short read_buffer_index)
{
  void *buffer;
  unsigned int size;
  int new_offset;

  buffer = FUN_001baa50(request);

  size = *(unsigned int *)(self + 0xa94);
  if ((int)size > 0x1ffff) {
    size = 0x20000;
  }

  if (read_buffer_index < 0 || read_buffer_index > 7) {
    display_assert(
      "read_buffer_index>=0 && read_buffer_index<NUMBER_OF_READ_BUFFERS",
      "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c", 0x5ec, 1);
    system_exit(-1);
  }

  if (*request != -1) {
    display_assert("request->read_sequence_index==NONE",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x5ed, 1);
    system_exit(-1);
  }

  *request = *(short *)(self + 0xab8);

  physical_memory_protect(buffer, size, 4);
  FUN_001bb190(buffer, size, *(int *)(self + 0xaa8), read_buffer_index);

  *(short *)(self + 0xab8) = *(short *)(self + 0xab8) + 1;

  new_offset = *(int *)(self + 0xaa8) + (int)size;
  *(unsigned int *)(self + 0xa94) = *(unsigned int *)(self + 0xa94) - size;
  *(int *)(self + 0xaa8) = new_offset;

  if (*(int *)(self + 0xa8c) < new_offset) {
    display_assert("self->current_read_offset<=self->read_file_size",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x601, 1);
    system_exit(-1);
  }
}

/* acquire_read_request — given a pointer into the per-file read-buffer
 * array, recover its index, verify the buffer's overlapped-I/O has
 * completed, clear the completed flag, reset the slot to NONE (0xffff),
 * and hand off to FUN_001bb430 to start the next async read.
 *
 * self is the same per-file decompression read-state block as
 * FUN_001bb430 above. request is a pointer to one of the
 * NUMBER_OF_READ_BUFFERS (8) 2-byte slots in the read-buffer array at
 * self+0xa78 ("_read_buffer_base" in the assert text below);
 * read_buffer_index is recovered as request's element index in that
 * array: ((request - self - 0xa78) >> 1), matching the disassembly's
 * SUB ESI,EBX / SUB ESI,0xa78 / SAR ESI,1 on 2-byte elements.
 *
 *   +0x998  overlapped_completed_flags (uint32 bit vector, 8 bits used,
 *           one per read buffer; assert text:
 *           "BIT_VECTOR_TEST_FLAG(self->overlapped_completed_flags,
 *            _read_buffer_base+read_buffer_index)")
 *   +0xa78  read-buffer array base (2-byte stride, matches request's type)
 *
 * Source: c:\halo\SOURCE\cache\cache_files_decompress_windows.c, asserts
 * at lines 0x652, 0x653.
 */
void acquire_read_request(char *self, short *request)
{
  int read_buffer_index;
  unsigned int mask;
  unsigned int *flags;

  read_buffer_index = ((int)request - (int)self - 0xa78) >> 1;

  if (read_buffer_index < 0 || read_buffer_index >= 8) {
    display_assert(
      "read_buffer_index>=0 && read_buffer_index<NUMBER_OF_READ_BUFFERS",
      "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c", 0x652, 1);
    system_exit(-1);
  }

  mask = 1u << (read_buffer_index & 0x1f);
  flags = (unsigned int *)(self + 0x998) + (read_buffer_index >> 5);

  if ((*flags & mask) == 0) {
    display_assert("BIT_VECTOR_TEST_FLAG(self->overlapped_completed_flags, "
                   "_read_buffer_base+read_buffer_index)",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x653, 1);
    system_exit(-1);
  }

  *flags = *flags & ~mask;
  *request = -1;

  FUN_001bb430(self, request, (short)read_buffer_index);
}

/* cache_copy_initialize_read_data — prime the very first synchronous chunk
 * of a map's decompression read data into the per-file read-state block's
 * primary buffer (self+0x104, 0x800 bytes), then hand off to the async
 * read machinery for steady-state reads.
 *
 * self (@<eax>) is the same per-file decompression read-state block as
 * FUN_001bb430/acquire_read_request above (TU confirmed via the __FILE__
 * assert xref below). No struct has been recovered for it; only the
 * fields touched here are named, reusing the assert-proven/established
 * names from FUN_001bb430's comment where they overlap:
 *   +0x104  primary read buffer, 0x800 bytes (zeroed, then filled)
 *   +0xa94  field_a94        (bytes remaining to read; decremented by
 *                             0x800 here, the same field FUN_001bb430
 *                             caps reads against)
 *   +0xa98  async_write_bytes_left (assert-proven name, from
 *           "global_self->async_write_bytes_left==0"; zeroed on entry,
 *           then asserted zero via a fresh read of the *global* pointer
 *           at 0x32ea98 rather than the local self copy -- reproduced
 *           as-is, matching FUN_001bc280's documented "reloads it after
 *           every call" idiom for that same globals-block pointer)
 *   +0xaa0  field_aa0 (zeroed)
 *   +0xaa4  field_aa4 (set to 0x800)
 *   +0xaa8  current_read_offset (assert-proven name, from FUN_001bb430;
 *           set to 0x800 -- the offset after this first synchronous
 *           0x800-byte read)
 *   +0xaac  field_aac (zeroed)
 *   +0xab0  field_ab0 (zeroed)
 *   +0xac2  field_ac2, int16 (zeroed)
 *
 * Call-site evidence for the two under-declared callees below (both
 * corrected here from their stale kb.json void(void) forms, the same way
 * FUN_001bb430's comment already corrected FUN_001bb190 from a different
 * call site):
 *   - FUN_001bb2d0: disassembly shows 4 pushes (1, 0, 0x800, buffer)
 *     immediately before the call; the caller's single "ADD ESP,0x1c"
 *     after the call cleans up 7 dwords total -- csmemset's 3-arg
 *     cleanup plus this call's 4-arg cleanup, batched together (the same
 *     compiler pattern repeats later in this function: FUN_001bb190's
 *     4 args + cache_file_header_verify's 3 args are cleaned by one
 *     "ADD ESP,0x1c" too). Proof of a 4-argument cdecl call, not the
 *     void(void) the decompiler rendered from the stale signature. The
 *     first three argument slots share buffer/size/offset values with
 *     the proven FUN_001bb190 call below; the fourth slot's role is not
 *     confirmed by this call site alone (1 here vs 8 for FUN_001bb190),
 *     so it is left as a plain untyped int rather than named.
 *   - FUN_001bb190: already corrected to a 4-arg cdecl prototype by
 *     FUN_001bb430's comment above (buffer, size, current_read_offset,
 *     read_buffer_index). Called here with a literal 8
 *     (NUMBER_OF_READ_BUFFERS) rather than a real 0-7 buffer index --
 *     reproduced verbatim, not reinterpreted.
 *   - cache_file_header_verify: matches its existing kb.json prototype;
 *     the path argument is the literal string "blah" resolved from
 *     .rdata at 0x2b89cc (a placeholder/debug string, reproduced as-is).
 *
 * MOV EDI,ESI immediately precedes both the FUN_001bb2d0 and FUN_001bb190
 * calls in the disassembly; it is not consumed by either callee (per the
 * call-site audit and FUN_001bb430's comment, neither takes a register
 * self-arg), so it has no C equivalent here.
 *
 * FUN_001ba930 and FUN_001ba8b0 (both below FUN_001bb190/
 * cache_file_header_verify) are now corrected to self@<esi> prototypes
 * (see their own comments in tags.c): self is still live in ESI at both
 * call sites from this function's own @<eax> entry, with no reload in
 * between, so both are called here with the explicit self argument.
 *
 * Source: c:\halo\SOURCE\cache\cache_files_decompress_windows.c, assert
 * at line 0x3eb.
 */
void cache_copy_initialize_read_data(char *self)
{
  void *buffer;

  buffer = self + 0x104;

  *(unsigned int *)(self + 0xa98) = 0;
  csmemset(buffer, 0, 0x800);

  FUN_001bb2d0(buffer, 0x800, 0, 1);
  FUN_001ba930(self);

  if (*(unsigned int *)(*(unsigned char **)0x32ea98 + 0xa98) != 0) {
    display_assert("global_self->async_write_bytes_left==0",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x3eb, 1);
    system_exit(-1);
  }

  FUN_001bb190(buffer, 0x800, 0, 8);
  FUN_001ba8b0(self);
  cache_file_header_verify(buffer, "blah", 1);

  *(unsigned int *)(self + 0xa94) = *(unsigned int *)(self + 0xa94) - 0x800;
  *(unsigned int *)(self + 0xaa8) = 0x800;
  *(unsigned int *)(self + 0xaa4) = 0x800;
  *(unsigned int *)(self + 0xaa0) = 0;
  *(unsigned int *)(self + 0xaac) = 0;
  *(short *)(self + 0xac2) = 0;
  *(unsigned int *)(self + 0xab0) = 0;
}

/* FUN_001bb8a0 — kick off the initial async read for all
 * NUMBER_OF_READ_BUFFERS (8) read buffers at cache-copy-thread startup.
 * For each slot i in [0,8): assert its "in use" bit at self+0x994 is not
 * already set, assert i is in range, call FUN_001bb430 to start the async
 * read for that slot's request pointer (self+0xa78 + i*2), then mark the
 * bit set.
 *
 * self is the same per-file decompression read-state block as
 * FUN_001bb430/acquire_read_request/cache_copy_initialize_read_data above,
 * passed here as an ordinary stack argument (disassembly reads [EBP+8]
 * directly with no register-spill store, unlike the @<eax> callers of
 * cache_copy_initialize_read_data/FUN_001bb430). Only one new field:
 *   +0x994  overlapped_in_use_flags (uint32 bit vector, 8 bits used, one
 *           per read buffer; assert text:
 *           "!BIT_VECTOR_TEST_FLAG(self->overlapped_in_use_flags,
 *            overlapped_index)")
 * Reuses the assert-proven field from acquire_read_request above:
 *   +0xa78  read-buffer array base (2-byte stride, matches request's type)
 *
 * The second assert ("read_buffer_index>=0 &&
 * read_buffer_index<NUMBER_OF_READ_BUFFERS") is evaluated every iteration
 * against the loop's own bounded counter and can never actually fire —
 * reproduced as-is rather than simplified away, matching the
 * disassembly's unconditional per-iteration check (TEST DI,DI / CMP DI,0x8
 * ahead of the call, independent of the bottom-of-loop bound check).
 *
 * The loop carries two counters that are numerically identical every
 * iteration (overlapped_index, an int used for the bit-vector math, and
 * read_buffer_index, a short used as FUN_001bb430's index argument and as
 * the loop bound) — disassembly keeps them as separate EBP-4/EDI locations
 * updated in lockstep (INC EDX / INC EDI), so both are kept as distinct
 * C variables rather than merged into one.
 *
 * Source: c:\halo\SOURCE\cache\cache_files_decompress_windows.c, asserts
 * at lines 0x416, 0x619. Sole caller: simple_cache_copy_thread (xref
 * 0x1bbfb7, unconditional call).
 */
void FUN_001bb8a0(char *self)
{
  int overlapped_index;
  short read_buffer_index;
  short *request;
  unsigned int mask;
  unsigned int *flags;

  overlapped_index = 0;
  read_buffer_index = 0;
  request = (short *)(self + 0xa78);

  do {
    mask = 1u << (overlapped_index & 0x1f);
    flags = (unsigned int *)(self + 0x994) + (overlapped_index >> 5);

    if ((*flags & mask) != 0) {
      display_assert(
        "!BIT_VECTOR_TEST_FLAG(self->overlapped_in_use_flags, "
        "overlapped_index)",
        "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c", 0x416, 1);
      system_exit(-1);
    }

    if (read_buffer_index < 0 || read_buffer_index >= 8) {
      display_assert(
        "read_buffer_index>=0 && read_buffer_index<NUMBER_OF_READ_BUFFERS",
        "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c", 0x619, 1);
      system_exit(-1);
    }

    FUN_001bb430(self, request, read_buffer_index);

    *flags = *flags | mask;
    read_buffer_index = read_buffer_index + 1;
    overlapped_index = overlapped_index + 1;
    request = request + 1;
  } while (read_buffer_index < 8);
}

/* LARGE_INTEGER as the original source spelled it — the assert text at
 * 0x1bc2a4 is literally "freq.u.HighPart==0", so the source used the
 * .u.LowPart/.u.HighPart member form. */
typedef union {
  struct {
    unsigned long LowPart;
    long HighPart;
  } u;
  __int64 QuadPart;
} CACHE_DECOMPRESS_LARGE_INTEGER;

/* FUN_001bc280 — initialize the cache decompression system.
 *
 * Latches the low dword of the performance-counter frequency into the
 * global at 0x32ea9c, creates the four decompression events, installs the
 * two allocator callbacks, and starts the copy thread.
 *
 * The globals block is reached through the POINTER global at 0x32ea98
 * (unlike the direct 0x4e92xx globals used by the rest of this file); the
 * original reloads it after every call, which a plain re-read of the
 * global reproduces.
 *
 * Touched offsets in that block:
 *   +0x928  cache_copy_compressed_alloc  (0x1ba660)
 *   +0x92c  FUN_001ba6c0                 (0x1ba6c0)
 *   +0x94c  auto-reset event, initially non-signaled
 *   +0x950  manual-reset event, initially non-signaled
 *   +0x954  manual-reset event, initially SIGNALED
 *   +0x958  manual-reset event, initially non-signaled
 *   +0x95c  copy thread handle (simple_cache_copy_thread, 0x1bbea0)
 *
 * Source: c:\halo\SOURCE\cache\cache_files_decompress_windows.c line 0x1e7.
 * Unlike FUN_001bda90 below, this function does NOT null-check any of the
 * returned handles — do not add checks. */
void FUN_001bc280(void)
{
  CACHE_DECOMPRESS_LARGE_INTEGER freq;

  QueryPerformanceFrequency(&freq);
  if (freq.u.HighPart != 0) {
    display_assert("freq.u.HighPart==0",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x1e7, 1);
    system_exit(-1);
  }
  *(unsigned long *)0x32ea9c = freq.u.LowPart;

  *(void **)(*(unsigned char **)0x32ea98 + 0x954) =
    CreateEventA(NULL, 1, 1, NULL);
  *(void **)(*(unsigned char **)0x32ea98 + 0x94c) =
    CreateEventA(NULL, 0, 0, NULL);
  *(void **)(*(unsigned char **)0x32ea98 + 0x950) =
    CreateEventA(NULL, 1, 0, NULL);
  *(void **)(*(unsigned char **)0x32ea98 + 0x958) =
    CreateEventA(NULL, 1, 0, NULL);

  *(void (**)(void))(*(unsigned char **)0x32ea98 + 0x928) =
    cache_copy_compressed_alloc;
  *(void (**)(void))(*(unsigned char **)0x32ea98 + 0x92c) = FUN_001ba6c0;

  *(void **)(*(unsigned char **)0x32ea98 + 0x95c) =
    CreateThread(NULL, 0x4000, simple_cache_copy_thread, NULL, 0, NULL);
}

/* cache_files_dispose (0x1bc360) — release the cache file globals request
 * array allocated by FUN_001bdb10 (init counterpart, elsewhere in this TU).
 * Asserts open_map_file_index==NONE (the map file must already be closed
 * via cache_file_close before dispose runs), then frees the request array.
 * Does NOT null DAT_004e9250 after the free — matches the original, which
 * has no store back to the global after the CALL.
 * Source: c:\halo\SOURCE\cache\cache_files_windows.c line 0xc9/0xcb.
 */
void cache_files_dispose(void)
{
  if (*(int16_t *)0x4e9244 != -1) {
    display_assert("cache_file_globals.open_map_file_index==NONE",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0xc9, 1);
    system_exit(-1);
  }
  debug_free(*(void **)0x4e9250,
             "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0xcb);
}

/* FUN_001bc5c0 — find the first free (inactive) cache IO request slot.
 * Scans the 512-entry request array at DAT_004e9250 (each entry 0x20
 * bytes) for a slot whose active byte at +0x1d is zero, validating each
 * index against [0, MAXIMUM_SIMULTANEOUS_CACHE_REQUESTS) as it goes (same
 * assert/system_exit(-1) idiom as cache_files_io_request_enable and
 * cache_file_block_until_not_busy in this TU). If no free slot is found
 * after a full pass, the scan just restarts from index 0 and loops
 * indefinitely (no wait/yield) until one becomes free — matches the
 * disassembly exactly, including the dead `looped` flag (set once, never
 * read; 0x1bc60e-0x1bc614 both paths jump back to the outer-loop restart
 * regardless of its value).
 * Called by cache_file_read (0x1bc9e0) to allocate a request slot.
 */
short FUN_001bc5c0(void)
{
  bool looped;
  short request_index;

  looped = false;
  do {
    request_index = 0;
    do {
      if (request_index < 0 || request_index > 0x1ff) {
        display_assert("request_index>=0 && "
                       "request_index<MAXIMUM_SIMULTANEOUS_CACHE_REQUESTS",
                       "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x260,
                       1);
        system_exit(-1);
      }
      if (*(char *)(*(int *)0x4e9250 + (int)request_index * 0x20 + 0x1d) == 0) {
        return request_index;
      }
      request_index = request_index + 1;
    } while (request_index < 0x200);
    if (!looped) {
      looped = true;
    }
  } while (1);
}

/* FUN_001bc620 — block until every cache IO request slot is idle by
 * spin-waiting, in slot order, on each of the 512 request slots' active
 * byte (+0x1d) until it clears (no sleep between checks, unlike
 * cache_file_block_until_not_busy which rescans the whole array with a
 * SleepEx(0,1) between passes). Asserts open_map_file_index != NONE up
 * front (0x285) and validates request_index range each iteration (0x260),
 * same assert/system_exit(-1) idiom as the other request-slot scanners in
 * this TU. Called by cache_file_close (0x1bc8f0-region) before clearing
 * DAT_004e9244.
 */
void FUN_001bc620(void)
{
  short request_index;
  int offset;
  char *req;

  if (*(int16_t *)0x4e9244 == -1) {
    display_assert("cache_file_globals.open_map_file_index!=NONE",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x285, 1);
    system_exit(-1);
  }
  request_index = 0;
  offset = 0;
  do {
    if (request_index < 0 || request_index > 0x1ff) {
      display_assert("request_index>=0 && "
                     "request_index<MAXIMUM_SIMULTANEOUS_CACHE_REQUESTS",
                     "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x260,
                     1);
      system_exit(-1);
    }
    req = (char *)(*(int *)0x4e9250 + offset + 0x1d);
    while (*req != 0) {
    }
    request_index = request_index + 1;
    offset = offset + 0x20;
  } while (request_index < 0x200);
}

/* Cache file precaching system for Xbox. Manages background copying of
 * map files from DVD to the hard drive cache partition. */

/* Set the precache thread priority — forwards param to thread handler. */
void cache_files_precache_set_priority(bool high)
{
  ((void (*)(bool))0x1ba290)(high);
}

/* Returns true if a map copy operation is currently in progress. */
bool cache_files_precache_in_progress(void)
{
  return *(uint8_t *)0x4e9220;
}

/* Returns true if the named map is currently being copied. */
bool cache_files_precache_is_copying_map(char *map_name)
{
  if (*(int16_t *)0x4e9222 != -1) {
    char *canonical = ((char *(*)(char *))0x19b0d0)(map_name);
    if (((int (*)(void *, char *))0x8dcb0)((void *)0x4e9224, canonical) == 0)
      return 1;
  }
  return 0;
}

/* Signal the end of the map precache queue. Asserts that a copy is
 * currently in progress. */
void cache_files_precache_map_queue_end(void)
{
  if (!*(uint8_t *)0x4e9220) {
    display_assert("cache_file_globals.copy_in_progress",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x3cb, 1);
    system_exit(-1);
  }
  ((void (*)(void))0x1ba5d0)();
}

/* Cache file slot accessor helpers. All take map_file_index in @<si>.
 * DAT_004e61d8 is an array of 6 cache file entries, each 0x80c bytes.
 * Source: c:\halo\SOURCE\cache\cache_files_windows.c line 0x485/0x49d. */

void *FUN_001bc720(short map_file_index)
{
  if ((int)map_file_index < 0 || (int)map_file_index >= 6) {
    display_assert(
      "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
      "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x485, 1);
    system_exit(-1);
  }
  return (void *)((char *)0x4e61d8 + (int)map_file_index * 0x80c);
}

void FUN_001bc760(short map_file_index)
{
  if ((int)map_file_index < 0 || (int)map_file_index >= 6) {
    display_assert(
      "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
      "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x485, 1);
    system_exit(-1);
  }
  *(unsigned int *)((char *)0x4e61d8 + (int)map_file_index * 0x80c) =
    0xffffffff;
}

unsigned int FUN_001bc7a0(short map_file_index)
{
  if ((int)map_file_index < 0 || (int)map_file_index >= 6) {
    display_assert(
      "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
      "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x485, 1);
    system_exit(-1);
  }
  return *(unsigned int *)((char *)0x4e61d8 + (int)map_file_index * 0x80c);
}

int FUN_001bc7e0(short map_file_index)
{
  int result;

  if ((int)map_file_index < 0 || (int)map_file_index >= 6) {
    display_assert(
      "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
      "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x49d, 1);
    system_exit(-1);
  }
  if (map_file_index <= 1)
    return 0x11600000;
  result = (int)(map_file_index > 2) - 1;
  result &= (int)0xff400000;
  result += 0x2f00000;
  return result;
}

/* Build cache map filename "z:\\cache%03d.map" into buffer.
 * buffer in @<ecx>, index in @<eax> (caller sign-extends short to int). */
void FUN_001bc830(char *buffer, int index)
{
  crt_sprintf(buffer, "z:\\cache%03d.map", index);
}

/* Signal the cache I/O event at DAT_004e9248. */
void FUN_001bc850(void)
{
  SetEvent(*(void **)0x4e9248);
}

/* FUN_001bc860 — IO completion callback for cache read/write.
 * Called by Windows when an overlapped IO request completes.
 * Validates error_code==ERROR_SUCCESS and bytes_transferred==request->size,
 * then signals the event at request->overlapped.hEvent (offset +0x10)
 * and clears the in-progress flags at +0x1d and +0x1e.
 * __stdcall: callee cleans 3 stack args (RET 0xc).
 */
void __stdcall FUN_001bc860(int error_code, int bytes_transferred,
                            void *finished_request)
{
  char *req = (char *)finished_request;
  char *event_ptr;

  if (error_code != 0) {
    display_assert("error_code==ERROR_SUCCESS",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x538, 1);
    system_exit(-1);
  }
  if (bytes_transferred != *(int *)(req + 0x14)) {
    display_assert("bytes_transferred==finished_request->size",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x539, 1);
    system_exit(-1);
  }
  event_ptr = *(char **)(req + 0x10);
  if (!event_ptr) {
    display_assert("finished_request->overlapped.hEvent",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x53a, 1);
    system_exit(-1);
  }
  *event_ptr = 1;
  *(char *)(req + 0x1d) = 0;
  *(char *)(req + 0x1e) = 0;
}

/* FUN_001bc8f0 — IO completion callback for async operations without
 * size tracking.  Validates error_code==ERROR_SUCCESS and hEvent non-null,
 * then sets *hEvent = 1 to signal the waiter.
 * __stdcall: callee cleans 3 stack args (RET 0xc).
 */
void __stdcall FUN_001bc8f0(int error_code, unsigned int param_2,
                            void *overlapped)
{
  if (error_code != 0) {
    display_assert("error_code==ERROR_SUCCESS",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x54a, 1);
    system_exit(-1);
  }
  if (*(int *)((char *)overlapped + 0x10) == 0) {
    display_assert("overlapped->hEvent",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x54b, 1);
    system_exit(-1);
    **(char **)((char *)overlapped + 0x10) = 1;
    return;
  }
  **(char **)((char *)overlapped + 0x10) = 1;
}

/* cache_file_close — close the currently-open map file if any is open.
 * DAT_004e9244 = cache_file_globals.open_map_file_index (int16_t).
 * If not NONE (-1), calls FUN_001bc620 to close the file, then resets to -1.
 * Frameless in the original (no EBP frame).
 */
void cache_file_close(void)
{
  if (*(int16_t *)0x4e9244 != -1) {
    FUN_001bc620();
    *(int16_t *)0x4e9244 = -1;
  }
}

/* cache_file_read — submit an async IO request to the cache file system.
 * Allocates a free request slot via FUN_001bc5c0, validates inputs, fills the
 * slot (offset +0..+0x1e), clears the completion flag, and fires the IO event.
 * Size is rounded up to the next multiple of 0x200 if not aligned.
 * Returns the request slot index.
 */
short cache_file_read(int param_1, int offset, unsigned int size, int buffer,
                      char *completion_flag, char async_flag)
{
  short request_index;
  char *req;

  request_index = FUN_001bc5c0();
  if (request_index < 0 || request_index > 0x1ff) {
    display_assert(
      "request_index>=0 && request_index<MAXIMUM_SIMULTANEOUS_CACHE_REQUESTS",
      "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x260, 1);
    system_exit(-1);
  }
  req = (char *)(*(int *)0x4e9250 + (int)request_index * 0x20);
  if (*(int16_t *)0x4e9244 == -1) {
    display_assert("cache_file_globals.open_map_file_index!=NONE",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x107, 1);
    system_exit(-1);
  }
  if (!buffer) {
    display_assert("buffer", "c:\\halo\\SOURCE\\cache\\cache_files_windows.c",
                   0x10a, 1);
    system_exit(-1);
  }
  if (!completion_flag) {
    display_assert("completion_flag_reference",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x10b, 1);
    system_exit(-1);
  }
  if (offset < 0) {
    display_assert("offset>=0",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x10e, 1);
    system_exit(-1);
  }
  if (size & 0x1ff)
    size = (size | 0x1ff) + 1;
  *completion_flag = 0;
  csmemset(req, 0, 0x14);
  *(char **)(req + 0x10) = completion_flag;
  *(unsigned int *)(req + 0x14) = size;
  *(int *)(req + 0xc) = 0;
  *(int *)(req + 0x8) = offset;
  *(int *)(req + 0x18) = buffer;
  *(char *)(req + 0x1d) = 1;
  *(char *)(req + 0x1c) = async_flag;
  *(char *)(req + 0x1e) = 0;
  SetEvent(*(void **)0x4e9248);
  return request_index;
}

/* Enable an async cache I/O request. Validates request_index is within
 * [0, 512) and sets the enable byte (offset 0x1c) in the request's
 * 0x20-byte entry in the global cache request array at 0x4e9250. */
void cache_files_io_request_enable(int16_t request_index)
{
  if (request_index < 0 || request_index >= 0x200) {
    display_assert(
      "request_index>=0 && request_index<MAXIMUM_SIMULTANEOUS_CACHE_REQUESTS",
      "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x260, 1);
    system_exit(-1);
  }
  *(uint8_t *)(*(int *)0x4e9250 + (int)request_index * 0x20 + 0x1c) = 1;
}

/* Validate a map file by name (0x1bcb80). Builds the path
 * "d:\\maps\\<map_name>.map" into a 256-byte stack buffer, opens it read-
 * only, reads the first 0x800 bytes into the caller-supplied header buffer
 * (passed in EDI), and asks cache_file_header_verify whether the header is
 * legitimate (signature, version, etc). Returns true only if the file
 * exists, the read returned exactly 0x800 bytes, and the header passes
 * verification. Always closes the handle if it was opened.
 *
 * Register args: EAX = map name (for the printf substitution),
 * EDI = 0x800-byte caller buffer that receives the header.
 */
bool FUN_001bcb80(const char *map_name /* @<eax> */,
                  void *header_buf /* @<edi> */)
{
  char path[256];
  int handle;
  uint32_t bytes_read;
  bool ok;

  ok = false;
  crt_sprintf(path, "d:\\maps\\%s.map", map_name);
  handle = CreateFileA(path, 0x80000000, 0, 0, 3, 0, 0);
  if (handle != -1) {
    if (ReadFile(handle, header_buf, 0x800, &bytes_read, 0) != 0 &&
        bytes_read == 0x800 && cache_file_header_verify(header_buf, path, 1)) {
      ok = true;
    }
    CloseHandle(handle);
  }
  return ok;
}

/* cache_file_block_until_not_busy — spin-wait until all 512 cache IO request
 * slots are idle. Loops: sleeps 1ms (SleepEx(0,1)), then scans all slots
 * checking the active byte at +0x1d. If any slot is still active, repeat.
 * DAT_004e9250 = base of the 512-entry request array (each 0x20 bytes).
 */
void cache_file_block_until_not_busy(void)
{
  int active;
  short i;
  int offset;

  do {
    SleepEx(0, 1);
    active = 0;
    i = 0;
    offset = 0;
    do {
      if (i < 0 || i > 0x1ff) {
        display_assert("request_index>=0 && "
                       "request_index<MAXIMUM_SIMULTANEOUS_CACHE_REQUESTS",
                       "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x260,
                       1);
        system_exit(-1);
      }
      if (*(char *)(*(int *)0x4e9250 + offset + 0x1d) != '\0')
        active = 1;
      i = i + 1;
      offset = offset + 0x20;
    } while (i < 0x200);
  } while (active);
}

/* tags_header_register_vertex_and_index_buffers — register D3D vertex and index
 * buffers from a block. block+0x10: vertex buffer count; block+0x14: vertex
 * buffer array base (stride 0xc). block+0x18: index buffer count; block+0x1c:
 * index buffer array base (stride 0xc). Writes 1 to the first dword of each
 * vertex buffer entry and calls D3DResource_Register; writes 0x10001 to each
 * index buffer entry.
 */
void tags_header_register_vertex_and_index_buffers(void *block)
{
  char *b = (char *)block;
  short s;
  int i;

  s = 0;
  if (*(int *)(b + 0x10) > 0) {
    i = 0;
    do {
      unsigned int *entry = (unsigned int *)(*(int *)(b + 0x14) + i * 0xc);
      *entry = 1;
      D3DResource_Register(entry, 0);
      s = s + 1;
      i = (int)s;
    } while (i < *(int *)(b + 0x10));
  }
  s = 0;
  if (*(int *)(b + 0x18) > 0) {
    i = 0;
    do {
      *(unsigned int *)(*(int *)(b + 0x1c) + i * 0xc) = 0x10001;
      s = s + 1;
      i = (int)s;
    } while (i < *(int *)(b + 0x18));
  }
}

/* tags_header_deregister_vertex_and_index_buffers — wait for D3D vertex and
 * index buffers to become idle. Calls D3DResource_BlockUntilNotBusy then
 * asserts !IsBusy for each buffer. Same block layout as
 * tags_header_register_vertex_and_index_buffers.
 */
void tags_header_deregister_vertex_and_index_buffers(void *block)
{
  char *b = (char *)block;
  short s;
  int i;

  s = 0;
  if (*(int *)(b + 0x10) > 0) {
    i = 0;
    do {
      void *entry = (void *)(*(int *)(b + 0x14) + i * 0xc);
      D3DResource_BlockUntilNotBusy(entry);
      if (D3DResource_IsBusy(entry) != 0) {
        display_assert("!IDirect3DVertexBuffer8_IsBusy(vertex_buffer)",
                       "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x205,
                       1);
        system_exit(-1);
      }
      s = s + 1;
      i = (int)s;
    } while (i < *(int *)(b + 0x10));
  }
  s = 0;
  if (*(int *)(b + 0x18) > 0) {
    i = 0;
    do {
      void *entry = (void *)(*(int *)(b + 0x1c) + i * 0xc);
      D3DResource_BlockUntilNotBusy(entry);
      if (D3DResource_IsBusy(entry) != 0) {
        display_assert("!IDirect3DIndexBuffer8_IsBusy(index_buffer)",
                       "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x212,
                       1);
        system_exit(-1);
      }
      s = s + 1;
      i = (int)s;
    } while (i < *(int *)(b + 0x18));
  }
}

/* structure_bsp_header_register_vertex_buffers — register D3D vertex and index
 * buffers from a geometry block. block+4/8: vertex count/array; block+0xc/0x10:
 * index count/array (stride 0xc). Writes 1 to first dword of each buffer entry
 * and calls D3DResource_Register. Same as
 * tags_header_register_vertex_and_index_buffers but uses offsets
 * +4/+8/+0xc/+0x10 instead of +0x10/+0x14/+0x18/+0x1c.
 */
void structure_bsp_header_register_vertex_buffers(void *block)
{
  char *b = (char *)block;
  short s;
  int i;

  s = 0;
  if (*(int *)(b + 4) > 0) {
    i = 0;
    do {
      unsigned int *entry = (unsigned int *)(*(int *)(b + 8) + i * 0xc);
      *entry = 1;
      D3DResource_Register(entry, 0);
      s = s + 1;
      i = (int)s;
    } while (i < *(int *)(b + 4));
  }
  s = 0;
  if (*(int *)(b + 0xc) > 0) {
    i = 0;
    do {
      unsigned int *entry = (unsigned int *)(*(int *)(b + 0x10) + i * 0xc);
      *entry = 1;
      D3DResource_Register(entry, 0);
      s = s + 1;
      i = (int)s;
    } while (i < *(int *)(b + 0xc));
  }
}

/* structure_bsp_header_deregister_vertex_buffers — wait for all vertex and
 * index buffers in a geometry block. Sets DAT_00325652=0x11 (render state),
 * blocks until each D3D resource is idle, then clears DAT_00325652=0. Same
 * struct layout as structure_bsp_header_register_vertex_buffers.
 */
void structure_bsp_header_deregister_vertex_buffers(void *block)
{
  char *b = (char *)block;
  short s;
  int i;

  *(char *)0x325652 = 0x11;
  s = 0;
  if (*(int *)(b + 4) > 0) {
    i = 0;
    do {
      D3DResource_BlockUntilNotBusy((void *)(*(int *)(b + 8) + i * 0xc));
      s = s + 1;
      i = (int)s;
    } while (i < *(int *)(b + 4));
  }
  s = 0;
  if (*(int *)(b + 0xc) > 0) {
    i = 0;
    do {
      D3DResource_BlockUntilNotBusy((void *)(*(int *)(b + 0x10) + i * 0xc));
      s = s + 1;
      i = (int)s;
    } while (i < *(int *)(b + 0xc));
  }
  *(char *)0x325652 = 0;
}

/* FUN_001bcea0 — delete cache map files z:\cacheNNN.map starting at
 * map_file_index+1 up to but not including 20 (@<ax> = map_file_index).
 * Calls SetLastError(0) at the end to clear any DeleteFile error.
 */
void FUN_001bcea0(short map_file_index)
{
  char local_buf[256];
  int i;
  unsigned int count;
  short start;

  start = map_file_index + 1;
  if ((unsigned short)start < 0x14) {
    i = (int)start;
    count = (unsigned int)(unsigned short)(0x14 - start);
    do {
      csprintf(local_buf, "z:\\cache%03d.map", i);
      DeleteFileA(local_buf);
      i = i + 1;
      count = count - 1;
    } while (count != 0);
  }
  SetLastError(0);
}

/* Query the status of the current precache operation. Returns a status
 * code and optionally writes the progress fraction to *progress. */
__int16 cache_files_precache_map_status(float *progress)
{
  int16_t status;

  if (!*(uint8_t *)0x4e9220) {
    display_assert("cache_file_globals.copy_in_progress",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x3a5, 1);
    system_exit(-1);
  }

  status = ((int16_t(*)(float *))0x1badc0)(progress);

  switch ((int)status) {
  case 0:
  case 1:
    return 2;
  case 2:
    FUN_001bc760(*(int16_t *)0x4e9222);
    return 2;
  case 3:
    return 0;
  case 4:
    return 1;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x3c2,
                   1);
    system_exit(-1);
    return 0;
  }
}

/* FUN_001bcfb0 — open/map the cache file for the given slot (@<ax> =
 * map_file_index). Initializes a local OBJECT_ATTRIBUTES-like struct, fills it
 * with the file path pointer at entry+4, and calls SetFileTime to create a
 * file mapping. Cache file entry at DAT_004e61d8 + index*0x80c; file handle at
 * offset +0.
 */
void FUN_001bcfb0(short map_file_index)
{
  char local_buf[16];
  char *entry;

  if ((int)map_file_index < 0 || (int)map_file_index >= 6) {
    display_assert(
      "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
      "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x485, 1);
    system_exit(-1);
  }
  entry = (char *)0x4e61d8 + (int)map_file_index * 0x80c;
  GetLocalTime(local_buf);
  SystemTimeToFileTime(local_buf, entry + 4);
  SetFileTime(*(int *)entry, entry + 4, 0, 0);
}

/* FUN_001bd1b0 — find the cache slot index whose stored map name matches
 * map_name (passed in EDI by the caller). Compares against the name field
 * at DAT_004e6204 + index*0x80c (DAT_004e61d8 + 0x2c, the name field within
 * each cache file entry) for all 6 slots via crt_stricmp. Returns the
 * matching slot index, or -1 if none match. */
int16_t FUN_001bd1b0(const char *map_name)
{
  int16_t map_file_index;

  map_file_index = 0;
  do {
    if (map_file_index < 0 || map_file_index >= 6) {
      display_assert(
        "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
        "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x485, 1);
      system_exit(-1);
    }
    if (crt_stricmp(map_name, (char *)0x4e6204 + (int)map_file_index * 0x80c) ==
        0) {
      return map_file_index;
    }
    map_file_index = map_file_index + 1;
  } while (map_file_index < 6);
  return -1;
}

/* FUN_001bd3a0 — cache file I/O service thread proc. Waits on the cache
 * I/O event at DAT_004e9248 (WaitForSingleObjectEx, INFINITE, alertable)
 * until it is signaled by something other than an APC completion
 * (WAIT_IO_COMPLETION/0xc0 loops back to the wait). Then scans the
 * 512-entry request array at DAT_004e9250 (0x20 bytes/entry) for the
 * pending-and-not-running slot (+0x1d!=0 && +0x1e==0) with the lowest
 * priority byte (+0x1c), breaking ties by the lowest file offset (+0x8) —
 * same unsigned-byte/unsigned-dword strict-less-than selection as the
 * disassembly's CMP/JBE pair. If none is found, loops back to the wait.
 * Otherwise validates the open map-file index, asserts the winning slot
 * isn't already running, marks it running (+0x1e=1), looks up that map
 * file's handle (first dword of its DAT_004e61d8 slot, same value
 * FUN_001bc7a0 returns), and dispatches the request via
 * FUN_001bc3b0(handle, buffer(+0x18), size(+0x14), offset(+0x8),
 * completion_flag(+0x10)) before immediately rescanning without waiting
 * again (JMP 0x1bd3c0 in the disassembly). Never returns; this is the
 * cache IO worker thread's entry proc. Frameless in the original.
 */
void FUN_001bd3a0(void)
{
  unsigned int wait_result;
  char *best;
  char *cur;
  short request_index;
  int offset;
  int16_t map_file_index;
  unsigned int handle;

  do {
    do {
      wait_result = WaitForSingleObjectEx(*(void **)0x4e9248, 0xffffffff, 1);
    } while (wait_result == 0xc0);

    for (;;) {
      best = 0;
      offset = 0;
      request_index = 0;
      do {
        if (request_index < 0 || request_index > 0x1ff) {
          display_assert("request_index>=0 && "
                         "request_index<MAXIMUM_SIMULTANEOUS_CACHE_REQUESTS",
                         "c:\\halo\\SOURCE\\cache\\cache_files_windows.c",
                         0x260, 1);
          system_exit(-1);
        }
        cur = (char *)(*(int *)0x4e9250 + offset);
        if (cur[0x1d] != 0 && cur[0x1e] == 0 &&
            (best == 0 ||
             ((unsigned char)cur[0x1c] < (unsigned char)best[0x1c] &&
              *(unsigned int *)(cur + 8) < *(unsigned int *)(best + 8)))) {
          best = cur;
        }
        request_index = request_index + 1;
        offset = offset + 0x20;
      } while (request_index < 0x200);

      if (best == 0)
        break;

      map_file_index = *(int16_t *)0x4e9244;
      if (map_file_index < 0 || map_file_index > 5) {
        display_assert(
          "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
          "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x485, 1);
        system_exit(-1);
      }
      if (best[0x1e] != 0) {
        display_assert("!best_request->running",
                       "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x4fc,
                       1);
        system_exit(-1);
      }
      handle =
        *(unsigned int *)((char *)0x4e61d8 + (int)map_file_index * 0x80c);
      best[0x1e] = 1;
      FUN_001bc3b0(handle, *(int *)(best + 0x18),
                   *(unsigned int *)(best + 0x14), *(int *)(best + 8),
                   *(char **)(best + 0x10));
    }
  } while (1);
}

/* cache_file_open (0x1bd4d0) — open the cache slot for stripped_name and
 * copy its cached 0x800-byte header out to the caller's buffer. Looks up
 * the slot via FUN_001bd1b0 unconditionally (matches disassembly: the call
 * happens before the parameter-null checks below, not after). Asserts
 * scenario_name/header non-NULL, asserts no map is already open
 * (cache_file_globals.open_map_file_index==NONE), and asserts the lookup
 * found a slot (map_file_index!=NONE). Clears the 0x4000-byte request
 * array, stores the new open_map_file_index, then (after validating the
 * slot index is in range) copies the header from the cache slot entry
 * (DAT_004e61d8 + index*0x80c + 0xc, the header field within each cache
 * file entry) into *header. Always returns true — every failure path
 * halts via system_exit(-1).
 * Source: c:\halo\SOURCE\cache\cache_files_windows.c line 0xd6-0xda/0x485.
 */
bool cache_file_open(const char *stripped_name, void *header)
{
  int16_t map_file_index;

  map_file_index = FUN_001bd1b0(stripped_name);

  if (stripped_name == NULL) {
    display_assert("scenario_name",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0xd6, 1);
    system_exit(-1);
  }
  if (header == NULL) {
    display_assert("header", "c:\\halo\\SOURCE\\cache\\cache_files_windows.c",
                   0xd7, 1);
    system_exit(-1);
  }
  if (*(int16_t *)0x4e9244 != -1) {
    display_assert("cache_file_globals.open_map_file_index==NONE",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0xd9, 1);
    system_exit(-1);
  }
  if (map_file_index == -1) {
    display_assert("map_file_index!=NONE",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0xda, 1);
    system_exit(-1);
  }

  csmemset(*(void **)0x4e9250, 0, 0x4000);
  *(int16_t *)0x4e9244 = map_file_index;

  if (map_file_index < 0 || map_file_index >= 6) {
    display_assert(
      "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
      "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x485, 1);
    system_exit(-1);
  }

  csmemcpy(header, (char *)0x4e61e4 + (int)map_file_index * 0x80c, 0x800);
  return true;
}

/* FUN_001bd5f0 — open or create the 6 cache slot files on Z:.
 * For each slot: opens with OPEN_ALWAYS, checks if existing file has the
 * right size. If the file is new or wrong size, reads the old header (0x800
 * bytes), then resizes via SetFilePointer+SetEndOfFile. After opening,
 * validates the cached header: build string must match "01.10.12.2276" and
 * the CRC must match the DVD source map. Clears slot metadata on mismatch.
 */
void FUN_001bd5f0(void)
{
  char path[256];
  char dvd_header[0x800];
  char read_buf[0x800];
  int expected_size;
  int handle;
  short i;
  bool ok;
  bool nuke_extra;
  char *entry_ptr;
  uint32_t bytes_read;
  uint32_t last_error;

  FUN_001bcea0(6);
  nuke_extra = 0;
  entry_ptr = (char *)0x4e6204;

  for (i = 0; i < 6; i++) {
    ok = 0;

    if (i < 0 || i >= 6) {
      display_assert(
        "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
        "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x485, 1);
      system_exit(-1);
    }

    crt_sprintf(path, "z:\\cache%03d.map", (int)i);

    if (i < 0 || i >= 6) {
      display_assert(
        "map_file_index>=0 && map_file_index<NUMBER_OF_CACHED_MAP_FILES",
        "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x49d, 1);
      system_exit(-1);
    }
    if (i <= 1) {
      expected_size = 0x11600000;
    } else {
      int temp = (int)(i > 2) - 1;
      temp &= (int)0xff400000;
      expected_size = temp + 0x2f00000;
    }

    handle = CreateFileA(path, 0xc0000000, 0, 0, 4, 0x60000000, 0);
    if (handle == -1) {
      {
        char err_buf[256];
        csprintf(err_buf, "couldn't open or create new cache file (#%d)",
                 xapi_GetLastError());
        display_assert(
          err_buf, "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x305, 1);
        system_exit(-1);
      }
      goto post_process;
    }

    last_error = xapi_GetLastError();
    if (last_error == 0xb7 &&
        GetFileSize(handle, 0) == (unsigned int)expected_size) {
      ok = 1;
      goto post_process;
    }

    if (!nuke_extra) {
      FUN_001bcea0(i);
      nuke_extra = 1;
    }

    ReadFile(handle, read_buf, 0x800, &bytes_read, 0);

    if (SetFilePointer(handle, expected_size, 0, 0) != (unsigned int)-1) {
      if (SetEndOfFile(handle)) {
        ok = 1;
      }
    }

    if (!ok) {
      {
        char err_buf[256];
        csprintf(err_buf, "setup for new cache file failed (#%d)",
                 xapi_GetLastError());
        display_assert(
          err_buf, "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x2f9, 1);
        system_exit(-1);
      }
      CloseHandle(handle);
      handle = -1;
    }

  post_process:
    *(int *)(entry_ptr - 0x2c) = handle;

    if (!ok)
      goto clear_entry;

    cache_file_read_header_into_slot(i);

    if (csstrcmp(entry_ptr + 0x20, "01.10.12.2276") != 0) {
      ok = 0;
    }

    if (!FUN_001bcb80(entry_ptr, dvd_header))
      goto clear_entry;

    if (*(int *)(entry_ptr + 0x44) != *(int *)(dvd_header + 0x64))
      goto clear_entry;

    if (ok)
      goto next_slot;

  clear_entry:
    csmemset(entry_ptr - 0x20, 0, 0x800);

  next_slot:
    entry_ptr += 0x80c;
  }
}

/* Returns true if the named map has already been precached.
 * 0x1bd1b0 reads EDI as the canonical map name (set from 0x19b0d0). */
bool cache_files_precache_map_loaded(char *map_name)
{
  int _edi = (int)((char *(*)(char *))0x19b0d0)(map_name);
  int16_t result;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    mov edi, _edi
    mov eax, 0x1bd1b0
    call eax
    mov result, ax
  }
#else
  asm volatile("movl $0x1bd1b0, %%eax\n\t"
               "call *%%eax"
               : "+D"(_edi), "=a"(result)
               :
               : "ecx", "edx", "memory", "cc");
#endif
  return result != -1;
}

/* Begin precaching a map from DVD to the cache partition. Returns true
 * if the copy was already done or was successfully started. */
bool cache_files_precache_map_begin(char *map_name, bool show_error)
{
  char path[256];
  char header_buf[0x800];
  char *canonical;
  int16_t cache_idx;

  canonical = ((char *(*)(char *))0x19b0d0)(map_name);
  ((char *(*)(char *))0x19b0d0)(map_name);
  cache_idx = ((int16_t(*)(void))0x1bd1b0)();

  if (cache_idx == -1) {
    if (!FUN_001bcb80(canonical, header_buf)) {
      error(2, "couldn't find map '%s' on the DVD", canonical);
      if (show_error)
        ((void (*)(void))0xe8d20)();
      return 0;
    }

    {
      int copy_handle;
      int buffer;
      int16_t slot;
      int block;
      int file;
      int mapped;

      copy_handle = ((int (*)(bool))0x1ba250)(show_error);
      buffer = (int)xbox_texture_cache_steal_memory(copy_handle);
      slot =
        FUN_001bd210(*(int16_t *)(header_buf + 0x60), *(int *)(header_buf + 8));

      block = (int)FUN_001bc720(slot);
      csmemset((void *)(block + 0xc), 0, 0x800);

      *(uint8_t *)0x4e9220 = 1;
      *(int16_t *)0x4e9222 = slot;
      csstrncpy((char *)0x4e9224, canonical, 0x1f);
      *(uint8_t *)0x4e9243 = 0;

      ((int (*)(char *, const char *, ...))0x1d90f0)(path, "d:\\maps\\%s.map",
                                                     canonical);
      error(2, "starting precaching of map '%s'", canonical);

      file = FUN_001bc7e0(slot);
      mapped = (int)FUN_001bc7a0(slot);
      FUN_001ba2f0(buffer, copy_handle, mapped, file, path);
    }
  }

  return 1;
}

/* End the current map precache operation. Cleans up resources and
 * resets the precache state. */
void cache_files_precache_map_end(void)
{
  if (!*(uint8_t *)0x4e9220) {
    display_assert("cache_file_globals.copy_in_progress",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x3d4, 1);
    system_exit(-1);
  }

  ((void (*)(void))0x1baf50)();
  ((void (*)(void))0x1beb10)();
  FUN_001bcfb0(*(int16_t *)0x4e9222);
  ((void (*)(int16_t))0x1bd020)(*(int16_t *)0x4e9222);

  *(uint8_t *)0x4e9220 = 0;
  *(int16_t *)0x4e9222 = -1;
}

/* FUN_001bda90 — initialize the cache IO system: create the sleep event and
 * the IO dispatcher thread (FUN_001bd3a0) with 0x4000 bytes of stack.
 * DAT_004e9248 = sleep_event handle, DAT_004e924c = thread handle.
 */
void FUN_001bda90(void)
{
  *(void **)0x4e9248 = CreateEventA(NULL, 0, 0, NULL);
  if (!*(void **)0x4e9248) {
    display_assert("cache_file_globals.sleep_event",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x4b2, 1);
    system_exit(-1);
  }
  *(void **)0x4e924c = CreateThread(NULL, 0x4000, FUN_001bd3a0, NULL, 0, NULL);
  if (!*(void **)0x4e924c) {
    display_assert("cache_file_globals.thread",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0x4b6, 1);
    system_exit(-1);
  }
}

/* FUN_001bdb10 — allocate and initialize cache file globals.
 * Sets open_map_file_index=NONE, allocates request array (0x4000 bytes),
 * creates IO event+thread, and initializes the IO state.
 */
void FUN_001bdb10(void)
{
  *(int16_t *)0x4e9244 = -1;
  *(void **)0x4e9250 = debug_malloc(
    0x4000, 0, "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0xb7);
  if (!*(void **)0x4e9250) {
    display_assert("cache_file_globals.requests",
                   "c:\\halo\\SOURCE\\cache\\cache_files_windows.c", 0xb8, 1);
    system_exit(-1);
  }
  FUN_001bda90();
  FUN_001bd5f0();
  FUN_001bc280();
}

/* Load cached game state if the cached map metadata matches the currently
 * loaded scenario, map type, checksum, and difficulty. */
void cache_files_precache(void)
{
  char header[0x14c];

  if (!game_state_read_header_from_persistent_storage(
        header, (uint32_t *)(header + 0x148), sizeof(header), 0x345000, NULL)) {
    return;
  }

  if (csstrcmp(header + 0x104, "01.10.12.2276") != 0) {
    return;
  }

  {
    const char *scenario_name = tag_get_name(*(int *)0x326a08);
    if (csstrcmp(header + 0x4, scenario_name) != 0) {
      return;
    }
  }

  if (*(int *)header != *(int *)0x4ea9a0)
    return;
  if (*(int16_t *)(header + 0x124) != *(int16_t *)0x31fa94)
    return;
  if (*(int *)(header + 0x128) != FUN_001b9920())
    return;
  if (*(int16_t *)(header + 0x126) != main_get_difficulty())
    return;

  ((void (*)(void))game_state_callback_32eaa4)();
  FUN_001c0c20(*(void **)0x4ea994, 0x345000);
  game_difficulty_level_set(main_get_difficulty());
  game_state_call_after_load_procs();
  ((void (*)(void))game_state_callback_32eaa0)();
  main_lost_map();
  *(uint8_t *)0x4ea9a5 = game_state_write_to_file() != 0;
  main_start_time();
}
