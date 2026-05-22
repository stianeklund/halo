#include <stdarg.h>

typedef struct debug_allocation_header {
  uint32_t begin_guard;
  struct debug_allocation_header *next;
  struct debug_allocation_header *previous;
  uint32_t size;
  const char *file;
  int line;
  uint32_t sequence;
  uint32_t checksum;
} debug_allocation_header_t;

/* debug_alloc_verify (0x8e6d0) — check end-of-buffer sentinel for a pointer.
 * Called before free/realloc to verify no overrun occurred.
 * @eax: ptr (void *) — user pointer (header+0x20) */
void FUN_0008e6d0(void *ptr /* @<eax> */, const char *file, int line)
{
  uint32_t size;

  size = *(uint32_t *)((char *)ptr - 0x14); /* header->size */
  if (*(uint32_t *)((char *)ptr + size) != 0x3c2d2d2d) {
    display_assert(
      csprintf((char *)0x5ab100,
               "Pointer allocated at %s, %d has overrun the end of its "
               "buffer. (Size: %d) (%s:%d)",
               *(const char **)((char *)ptr - 0x10), /* header->file */
               *(int *)((char *)ptr - 0xc), /* header->line */
               size, file, line),
      "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0xc7, 1);
    system_exit(-1);
  }
}

/* memory_check (0x8e770) — track min/max physical free memory at a
 * checkpoint. Warns if the spread exceeds 0x4000 bytes, indicating
 * non-deterministic allocation between runs. */
void memory_check(uint32_t *min_max, const char *location)
{
  uint32_t status[8];
  uint32_t avail_phys;
  uint32_t cur_min, cur_max, diff;

  xbox_query_global_memory_status(status);
  avail_phys = status[3];

  cur_min = min_max[0];
  if (avail_phys < cur_min) {
    cur_min = avail_phys;
  }
  min_max[0] = cur_min;

  cur_max = min_max[1];
  if (avail_phys > cur_max) {
    cur_max = avail_phys;
  }
  min_max[1] = cur_max;

  diff = cur_max - cur_min;
  if (diff > 0x4000) {
    error(2,
          "memory check failed at %s, difference between min and max memory "
          "free is %d",
          location, diff);
    error(2, "  avail_phys=%u min=%u max=%u", avail_phys, cur_min, cur_max);
  }
}

/* Get a random value from the local random seed. */
uint16_t FUN_0008e7c0(void)
{
  return random_seed_step(random_math_get_local_seed_address());
}

/* debug_alloc_header_validate (0x8e7d0) — validate a debug allocation header.
 * Called before any operation on a debug-tracked pointer.
 * Checks: heap initialized, pointer in valid range, guard word valid, CRC
 * matches.
 * @esi: header  @ebx: caller file  @edi: caller line */
void FUN_0008e7d0(void *header_v /* @<esi> */, const char *file /* @<ebx> */,
                  int line /* @<edi> */)
{
  debug_allocation_header_t *header = (debug_allocation_header_t *)header_v;
  uint32_t checksum;
  const char *reason;

  if (*(uint32_t *)0x2ee758 == 0) {
    display_assert(
      csprintf((char *)0x5ab100,
               "Attempted an operation with pointer at 0x%x when no pointers "
               "have been allocated. (%s:%d)",
               (char *)header + 0x20, file, line),
      "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0xa0, 1);
    system_exit(-1);
  }

  if ((uint32_t)header < *(uint32_t *)0x2ee75c ||
      (uint32_t)header > *(uint32_t *)0x2ee760) {
    display_assert(
      csprintf((char *)0x5ab100,
               "Attempted an operation with pointer at 0x%x, outside of the "
               "valid pointer range. (%s:%d)",
               (char *)header + 0x20, file, line),
      "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0xa5, 1);
    system_exit(-1);
  }

  if (header->begin_guard == 0x3c424144) {
    reason = (const char *)0x267e10; /* "Pointer has been disposed." */
  } else if (header->begin_guard == 0x2d2d2d3e) {
    /* active allocation — verify CRC over the first 0x1c bytes */
    crc_new(&checksum);
    crc_checksum_buffer(&checksum, header, 0x1c);
    if (checksum == header->checksum) {
      return;
    }
    reason = (const char *)0x267de0; /* "Checksum is incorrect." */
  } else {
    reason = (const char *)0x267df8; /* "Signature is incorrect." */
  }

  display_assert(
    csprintf((char *)0x5ab100,
             "Invalid pointer: header: 0x%x signature: 0x%x line: %d "
             "file: 0x%x size: 0x%x reason: %s (%s:%d)",
             header, header->begin_guard, header->line, header->file,
             header->size, reason, file, line),
    "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0xb7, 1);
  system_exit(-1);
}

/* debug_alloc_fill (0x8e8e0) — fill a newly-allocated region with random bytes.
 * Walks the region 2 bytes at a time using the local random seed; handles odd
 * sizes with a final single-byte write.
 * @ebx: ptr (void *) — start of region to fill */
void FUN_0008e8e0(void *ptr /* @<ebx> */, uint32_t size)
{
  if (ptr == NULL) {
    display_assert("pointer", "c:\\halo\\SOURCE\\cseries\\debug_memory.c",
                   0x10d, 1);
    system_exit(-1);
  }
  {
    /* ESI=p, EDI=end — saved after the null check, restored before odd-byte */
    uint16_t *p = (uint16_t *)ptr;
    uint16_t *end = (uint16_t *)((uint8_t *)ptr + size - 1);
    if (p < end) {
      do {
        *p = random_seed_step(random_math_get_local_seed_address());
        p++;
      } while (p < end);
    }
  }
  if (size & 1) {
    *(uint8_t *)((uint8_t *)ptr + size - 1) =
      (uint8_t)random_seed_step(random_math_get_local_seed_address());
  }
}

/* debug_alloc_list_link (0x8e950) — prepend a header to the global debug list.
 * Updates the min/max pointer bounds, splices the node at the head, recomputes
 * the displaced head's CRC, then sets the new node's CRC.
 * @esi: header (debug_allocation_header_t *) */
void FUN_0008e950(void *header_v /* @<esi> */)
{
  debug_allocation_header_t *header = (debug_allocation_header_t *)header_v;
  debug_allocation_header_t *head;
  uint32_t checksum;

  head = *(debug_allocation_header_t **)0x2ee758;

  if (head == NULL || (uint32_t)header < *(uint32_t *)0x2ee75c) {
    *(uint32_t *)0x2ee75c = (uint32_t)header;
  }
  if (head == NULL || (uint32_t)header > *(uint32_t *)0x2ee760) {
    *(uint32_t *)0x2ee760 = (uint32_t)header;
  }

  header->next = head;

  if (head != NULL) {
    head->previous = header;
    /* recompute displaced head's checksum (prev pointer changed) */
    crc_new(&checksum);
    crc_checksum_buffer(&checksum, head, 0x1c);
    head->checksum = checksum;
  }

  header->previous = NULL;
  *(debug_allocation_header_t **)0x2ee758 = header;

  crc_new(&checksum);
  crc_checksum_buffer(&checksum, header, 0x1c);
  header->checksum = checksum;
}

/* debug_alloc_list_unlink (0x8e9f0) — remove a header from the global debug
 * list. Handles both head and non-head removal; recomputes CRCs of affected
 * neighbours.
 * @eax: header (debug_allocation_header_t *) */
void FUN_0008e9f0(void *header_v /* @<eax> */, const char *file, int line)
{
  debug_allocation_header_t *header = (debug_allocation_header_t *)header_v;
  debug_allocation_header_t *prev;
  debug_allocation_header_t *next;
  uint32_t checksum;

  if (header == *(debug_allocation_header_t **)0x2ee758) {
    /* removing the head node — cache next so MSVC can keep it in a register */
    next = header->next;
    *(debug_allocation_header_t **)0x2ee758 = next;
    if (next != NULL) {
      next->previous = NULL;
      crc_new(&checksum);
      crc_checksum_buffer(&checksum, next, 0x1c);
      next->checksum = checksum;
    }
  } else {
    prev = header->previous;
    if (prev == NULL) {
      display_assert("previous", "c:\\halo\\SOURCE\\cseries\\debug_memory.c",
                     0x1be, 1);
      system_exit(-1);
    }
    next = header->next;
    prev->next = next;
    crc_new(&checksum);
    crc_checksum_buffer(&checksum, prev, 0x1c);
    prev->checksum = checksum;
    if (next != NULL) {
      next->previous = prev;
      crc_new(&checksum);
      crc_checksum_buffer(&checksum, next, 0x1c);
      next->checksum = checksum;
    }
  }
}

/* debug_malloc_check_all (0x8ead0) — walk every debug-tracked allocation,
 * validate each header, and check the end-of-buffer sentinel word. Reports
 * overruns via display_assert and exits. Also guards against an uninitialized
 * heap manager before iterating. */
void debug_malloc_check_all(const char *file, int line)
{
  debug_allocation_header_t *node;

  if (*(uint32_t *)0x2ee74c != 0x53414654 ||
      *(uint32_t *)0x2ee768 != 0x53414654) {
    display_assert(csprintf((char *)0x267c70, file, line),
                   "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x91, 1);
    system_exit(-1);
  }

  for (node = *(debug_allocation_header_t **)0x2ee758; node != NULL;
       node = node->next) {
    FUN_0008e7d0(node, file, line);
    if (*(uint32_t *)((char *)node + 0x20 + node->size) != 0x3c2d2d2d) {
      display_assert(
        csprintf((char *)0x5ab100,
                 "Pointer allocated at %s, %d has overrun the end of its "
                 "buffer. (Size: %d) (%s:%d)",
                 node->file, node->line, node->size, file, line),
        "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0xc7, 1);
      system_exit(-1);
    }
  }
}

/* Reallocate a debug-tracked allocation, preserving the original file/line.
 * Validates the debug memory sentinel and existing header before reallocating.
 * If ptr is NULL, behaves like debug_malloc; if new_size is 0, frees. */
void *debug_realloc(void *ptr, int new_size, const char *file, int line)
{
  typedef void *(*realloc_fn)(void *, int);
  debug_allocation_header_t *header;
  debug_allocation_header_t *new_header;
  void *result = NULL;
  uint32_t old_size = 0;
  int total_alloc_size = new_size + 0x24;

  if (ptr == NULL && new_size == 0) {
    display_assert("pointer || size",
                   "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x151, true);
    system_exit(-1);
  } else if ((unsigned int)new_size >= 0x10000000) {
    display_assert("size>=0 && size<MAXIMUM_POINTER_SIZE",
                   "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x152, true);
    system_exit(-1);
  }

  if (*(uint32_t *)0x2ee74c != 0x53414654 ||
      *(uint32_t *)0x2ee768 != 0x53414654) {
    display_assert(
      csprintf(error_string_buffer,
               "Debug memory manager is uninitialized or corrupted. (%s:%d)",
               file, line),
      "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x91, true);
    system_exit(-1);
  }

  header = NULL;
  if (ptr != NULL) {
    header = (debug_allocation_header_t *)((char *)ptr - 0x20);

    /* validate header */
    FUN_0008e7d0(header, file, line);

    /* verify allocation (end-of-buffer sentinel check) */
    FUN_0008e6d0(ptr, file, line);

    /* unlink header from debug allocation list */
    FUN_0008e9f0(header, file, line);

    line = (int)header->line;
    old_size = header->size;
    file = header->file;
    header->begin_guard = 0x3c424144;
  }

  if (new_size == 0 && ptr != NULL) {
    total_alloc_size = 0;
  }

  new_header = (debug_allocation_header_t *)((realloc_fn)0x8e3f0)(
    header, total_alloc_size);

  if (new_header != NULL) {
    new_header->begin_guard = 0x2d2d2d3e;
    new_header->line = line;
    new_header->file = file;
    new_header->sequence = *(uint32_t *)0x2ee764;
    (*(uint32_t *)0x2ee764)++;
    new_header->size = (uint32_t)new_size;
    *(uint32_t *)((char *)new_header + new_size + 0x20) = 0x3c2d2d2d;

    /* link new header into debug allocation list */
    FUN_0008e950(new_header);

    result = (void *)((char *)new_header + 0x20);

    if ((uint32_t)new_size > old_size) {
      /* fill new bytes with random data */
      uint32_t delta = (uint32_t)new_size - old_size;
      FUN_0008e8e0((char *)result + old_size, delta);
    }

    if (result != NULL)
      goto update_stats;
  }

  if (new_size != 0)
    return result;

update_stats:
  *(uint32_t *)0x2ee750 += ((uint32_t)new_size - old_size);
  if (*(uint32_t *)0x2ee754 < *(uint32_t *)0x2ee750) {
    *(uint32_t *)0x2ee754 = *(uint32_t *)0x2ee750;
  }
  return result;
}

/* FUN_0008f1e0 (0x8f1e0) — dump all debug allocations to heap_dump.txt. */
void FUN_0008f1e0(void)
{
  debug_dump_memory_for_file(NULL);
}

/*
 * debug_string_to_display — write a debug message to d:\debug.txt.
 *
 * On the first call, writes a header block with the build version string,
 * reference function name, and address. Then appends the message to the
 * debug log file. If include_timestamp is non-zero, prepends a timestamp
 * in "MM.DD.YY HH:MM:SS  " format.
 *
 * Confirmed: first-call flag at 0x2ee76c; recursive calls for header.
 * Confirmed: fopen with mode "a+b" at 0x267f84.
 * Confirmed: localtime for timestamp; fprintf with "%02d.%02d.%02d ..." format.
 * Confirmed: fprintf(stream, "%s", message) for the actual output.
 * Confirmed: version string at 0x268150.
 */
void debug_string_to_display(const char *message, int include_timestamp)
{
  void *stream;
  void *tm;
  char buf[1024];
  int timeval;

  if (*(uint8_t *)0x2ee76c != 0) {
    *(uint8_t *)0x2ee76c = 0;
    debug_string_to_display((const char *)0x2681a4, 0);
    debug_string_to_display((const char *)0x268150, 1);
    crt_sprintf(buf, (const char *)0x268118, (const char *)0x268134);
    debug_string_to_display(buf, 1);
    crt_sprintf(buf, (const char *)0x268100, (void *)0x8f230);
    debug_string_to_display(buf, 1);
  }

  if (*(uint8_t *)0x5aa8e1 == 0)
    return;

  stream = crt_fopen("d:\\debug.txt", (const char *)0x267f84);
  if (stream == NULL)
    return;

  if (include_timestamp != 0) {
    crt_time(&timeval);
    tm = crt_localtime(&timeval);
    if (tm == NULL) {
      crt_fprintf(stream, (const char *)0x2680b8);
    } else {
      crt_fprintf(stream, (const char *)0x2680d0,
                  *(int *)((char *)tm + 0x10) + 1, *(int *)((char *)tm + 0xc),
                  *(int *)((char *)tm + 0x14) % 100, *(int *)((char *)tm + 0x8),
                  *(int *)((char *)tm + 0x4), *(int *)((char *)tm + 0x0));
    }
  }

  crt_fprintf(stream, (const char *)0x257984, message);
  crt_fclose(stream);
}

/* error() — the central error/warning handler for all subsystems.
 * priority levels:
 *   0 = fatal (calls system_exit after logging)
 *   1 = warning (sets the "has warning" flag)
 *   2 = error (rate-limited, logged to console and buffer)
 *   3 = silent (logged to debug output only, not console)
 * The error message is vsprintf-formatted into a 1024-byte local buffer,
 * "\r\n" is appended, and the result is accumulated into the global error
 * message buffer at 0x5aa8e8 (max 2048 bytes). When the buffer overflows,
 * old messages are trimmed and replaced with a truncation prefix. */
void error(unsigned __int16 priority, const char *format, ...)
{
  char buf[1024];
  char *found;
  int new_size;

  /* validate priority range */
  if (priority < 0 || priority >= 4) {
    display_assert("priority>=0 && priority<NUMBER_OF_ERROR_MESSAGE_PRIORITIES",
                   "c:\\halo\\SOURCE\\cseries\\errors.c", 0x61, 1);
    system_exit(-1);
  }

  /* rate-limit priority-2 errors when rate limiting is enabled */
  if (*(uint8_t *)0x5aa8e4 != 0 && priority == 2) {
    int now = system_milliseconds();
    if (now > *(int *)0x33618c + 900)
      *(int *)0x336190 = 0;
    *(int *)0x33618c = now;
    if (*(int *)0x336190 == 10) {
      ((void (*)(void *, const char *))0xe3a10)(
        *(void **)0x2ee6c4, "too many errors, only printing to debug.txt");
    }
    *(int *)0x336190 = *(int *)0x336190 + 1;
    if (*(int *)0x336190 >= 10)
      priority = 3;
  }

  /* reentrancy guard */
  if (*(uint8_t *)0x5aa8e3 != 0)
    return;
  *(uint8_t *)0x5aa8e3 = 1;

  /* set "has warning" flag for priority 1 */
  if (priority == 1)
    *(uint8_t *)0x5aa8e0 = 1;

  if (format != 0) {
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, (char *)args);
    va_end(args);

    /* append "\r\n" to formatted message */
    ((void (*)(char *, const char *))0x8dc30)(buf, (const char *)0x261f2c);

    /* output to console (unless silent priority) */
    if (priority != 3) {
      ((void (*)(void *, const char *, char *))0xe3a10)(
        *(void **)0x2ee6c4, (const char *)0x257984, buf);
    }

    /* write to debug output */
    debug_string_to_display(buf, 1);

    /* accumulate into global error buffer */
    new_size = csstrlen(buf);

    if ((int)*(int16_t *)0x5aa8e6 + new_size >= 0x800) {
      /* buffer overflow — truncate old messages */
      int prefix_size = csstrlen("[...too many errors to print...]\r\n");
      int skip = prefix_size + 0x400 + new_size;
      int buf_len;
      int copy_size;

      if (skip < 0)
        skip = 0;
      else if (skip > (int)*(int16_t *)0x5aa8e6 - 1)
        skip = (int)*(int16_t *)0x5aa8e6 - 1;

      found = crt_strchr((const char *)(0x5aa8e8 + skip), '\n');
      if (found == (char *)0)
        buf_len = (int)*(int16_t *)0x5aa8e6;
      else
        buf_len = (int)(found - (char *)0x5aa8e8) + 1;

      copy_size = (int)*(int16_t *)0x5aa8e6 - buf_len;

      if (prefix_size + copy_size + new_size >= 0x800) {
        display_assert("prefix_size + copy_size + new_size < "
                       "ERROR_MESSAGE_BUFFER_MAXIMUM_SIZE",
                       "c:\\halo\\SOURCE\\cseries\\errors.c", 0xbf, 1);
        system_exit(-1);
      }

      /* prepend truncation marker */
      csstrncpy((char *)0x5aa8e8, "[...too many errors to print...]\r\n",
                prefix_size);

      /* copy remaining old messages after the marker */
      if (copy_size > 0) {
        ((void (*)(void *, void *, int))0x8dae0)(
          (void *)(0x5aa8e8 + prefix_size), found, copy_size);
      }

      *(int16_t *)0x5aa8e6 = (int16_t)(copy_size + prefix_size);
      *(char *)(0x5aa8e8 + copy_size + prefix_size) = 0;
    }

    if ((int)*(int16_t *)0x5aa8e6 + new_size < 0x800) {
      ((void (*)(char *, char *))0x8dff0)(
        (char *)(0x5aa8e8 + (int)*(int16_t *)0x5aa8e6), buf);
      *(int16_t *)0x5aa8e6 += (int16_t)new_size;
    }
  }

  /* fatal error: exit */
  if (priority == 0)
    system_exit(-5000);

  *(uint8_t *)0x5aa8e3 = 0;
}

bool error_occurred(void)
{
  bool occurred;

  occurred = *(uint8_t *)0x5aa8e0 != 0;
  *(uint8_t *)0x5aa8e0 = 0;
  *(int16_t *)0x5aa8e6 = 0;
  return occurred;
}

/* FUN_0008f630 (0x8f630) — reset error-tracking ring buffer
 *
 * Stamps the ring buffer header with a magic value, then iterates through
 * any live entry pointers, clearing their handle field to INVALID (0xffffffff).
 * Resets all ring buffer counters and flags to their initial states. */
void FUN_0008f630(void)
{
  int16_t i;
  int32_t *entry;

  *(int32_t *)0x3361a0 = 0x2bb5c755;
  *(int32_t *)0x3361a4 = 0;
  if (*(int16_t *)0x3361b0 > 0) {
    i = 0;
    do {
      entry = (int32_t *)(((int32_t *)0x3361b4)[i]);
      i++;
      entry[1] = (int32_t)0xffffffff;
    } while (i < *(int16_t *)0x3361b0);
  }
  *(uint8_t *)0x449ef1 = 1;
  *(int16_t *)0x3361b0 = 0;
  *(int16_t *)0x3361a8 = 0;
  *(uint8_t *)0x3361aa = 1;
  *(int32_t *)0x3361ac = 0;
  *(int16_t *)0x3365c2 = 0;
  *(int16_t *)0x3365c4 = 0;
  *(int32_t *)0x3365bc = 999;
  *(int32_t *)0x3365b4 = 0;
}

/* FUN_0008f6b0 (0x8f6b0) — advance the profiling frame counter and accumulate
 * per-frame timing stats for all active entries.
 * Returns 1 when a full second (120 frames) has elapsed, 0 otherwise. */
int FUN_0008f6b0(void)
{
  int16_t i;
  int32_t entry;
  int32_t frame;
  uint32_t old_frame_low;
  uint32_t rolling_low;
  uint32_t snap_low;
  int32_t snap_high;
  int32_t snap_int;
  uint32_t *acc_low_ptr;
  uint32_t acc_old;
  int32_t new_frame;

  if (*(uint8_t *)0x449ef1 != 0 && *(int16_t *)0x3361b0 > 0) {
    i = 0;
    do {
      entry = ((int32_t *)0x3361b4)[i];
      if (*(uint8_t *)(entry + 8) != 0) {
        frame = *(int32_t *)0x3361ac;

        /* subtract old per-frame snapshot from rolling totals */
        old_frame_low = *(uint32_t *)(entry + 0x208 + frame * 8);
        rolling_low = *(uint32_t *)(entry + 0x20);
        *(uint32_t *)(entry + 0x20) = rolling_low - old_frame_low;
        *(int32_t *)(entry + 0x24) =
          (*(int32_t *)(entry + 0x24) -
           *(int32_t *)(entry + 0x20c + frame * 8)) -
          (int32_t)(rolling_low < old_frame_low ? 1U : 0U);
        *(int32_t *)(entry + 0x18) -= *(int32_t *)(entry + 0x28 + frame * 4);

        /* store new snapshot into per-frame history slot */
        *(int32_t *)(entry + 0x208 + frame * 8) = *(int32_t *)(entry + 0x5d0);
        *(int32_t *)(entry + 0x20c + frame * 8) = *(int32_t *)(entry + 0x5d4);
        *(int32_t *)(entry + 0x28 + frame * 4) = *(int32_t *)(entry + 0x5cc);

        /* add new snapshot to rolling totals */
        rolling_low = *(uint32_t *)(entry + 0x20);
        snap_low = *(uint32_t *)(entry + 0x5d0);
        snap_high = *(int32_t *)(entry + 0x5d4);
        *(uint32_t *)(entry + 0x20) = rolling_low + snap_low;
        *(int32_t *)(entry + 0x24) +=
          snap_high +
          (int32_t)(*(uint32_t *)(entry + 0x20) < rolling_low ? 1U : 0U);
        snap_int = *(int32_t *)(entry + 0x5cc);
        *(int32_t *)(entry + 0x18) += snap_int;

        /* update uint64 max */
        if (*(int32_t *)(entry + 0x5f4) <= snap_high &&
            (*(int32_t *)(entry + 0x5f4) < snap_high ||
             *(uint32_t *)(entry + 0x5f0) < snap_low)) {
          *(uint32_t *)(entry + 0x5f0) = snap_low;
          *(int32_t *)(entry + 0x5f4) = snap_high;
        }
        /* update int max */
        if (*(int32_t *)(entry + 0x5e8) < snap_int)
          *(int32_t *)(entry + 0x5e8) = snap_int;

        /* accumulate into all-time totals */
        acc_low_ptr = (uint32_t *)(entry + 0x5e0);
        acc_old = *acc_low_ptr;
        *acc_low_ptr += snap_low;
        *(int32_t *)(entry + 0x5d0) = 0;
        *(uint32_t *)(entry + 0x5e4) +=
          (uint32_t)snap_high + (*acc_low_ptr < acc_old ? 1U : 0U);
        *(int32_t *)(entry + 0x5d8) += snap_int;
        *(int32_t *)(entry + 0x5d4) = 0;
        *(int32_t *)(entry + 0x5cc) = 0;
        *(int32_t *)(entry + 0x5c8) += 1;
      }
      i++;
    } while (i < *(int16_t *)0x3361b0);
  }

  new_frame = *(int32_t *)0x3361ac + 1;
  *(uint8_t *)0x3361aa = 0;
  *(int32_t *)0x3361ac = new_frame % 0x78;
  return new_frame / 0x78;
}

/* FUN_0008f810 (0x8f810) — store two 32-bit performance counter values into
 * globals at 0x449ed8 / 0x449edc. */
void FUN_0008f810(int param_1, int param_2)
{
  *(int *)0x449ed8 = param_1;
  *(int *)0x449edc = param_2;
}
