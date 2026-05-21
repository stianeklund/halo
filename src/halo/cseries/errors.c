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
    {
      int _esi = (int)node;
      int _ebx = (int)file;
      int _edi = line;
      asm volatile("movl $0x8e7d0, %%eax\n\tcall *%%eax"
                   : "+S"(_esi), "+b"(_ebx), "+D"(_edi)
                   :
                   : "eax", "ecx", "edx", "memory", "cc");
    }
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

    /* validate header: ESI=header, EBX=file, EDI=line */
    {
      int _esi = (int)header;
      int _ebx = (int)file;
      int _edi = line;
      asm volatile("movl $0x8e7d0, %%eax\n\t"
                   "call *%%eax"
                   : "+S"(_esi), "+b"(_ebx), "+D"(_edi)
                   :
                   : "eax", "ecx", "edx", "memory", "cc");
    }

    /* verify allocation: EAX=ptr, stack: file, line */
    {
      int _eax = (int)ptr;
      int _file = (int)file;
      int _line = line;
      asm volatile(
        "pushl %[line]\n\t"
        "pushl %[file]\n\t"
        "call *%[fn]\n\t"
        "addl $8, %%esp"
        : "+a"(_eax)
        : [fn] "r"((void *)0x8e6d0), [file] "r"(_file), [line] "r"(_line)
        : "ecx", "edx", "memory", "cc");
    }

    /* unlink from list: EAX=header, stack: file, line */
    {
      int _eax = (int)header;
      int _file = (int)file;
      int _line = line;
      asm volatile(
        "pushl %[line]\n\t"
        "pushl %[file]\n\t"
        "call *%[fn]\n\t"
        "addl $8, %%esp"
        : "+a"(_eax)
        : [fn] "r"((void *)0x8e9f0), [file] "r"(_file), [line] "r"(_line)
        : "ecx", "edx", "memory", "cc");
    }

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

    /* link into list: ESI=new_header */
    {
      int _esi = (int)new_header;
      asm volatile("call *%[fn]"
                   : "+S"(_esi)
                   : [fn] "r"((void *)0x8e950)
                   : "eax", "ecx", "edx", "memory", "cc");
    }

    result = (void *)((char *)new_header + 0x20);

    if ((uint32_t)new_size > old_size) {
      /* fill new area: EBX=fill_start, stack=delta */
      uint32_t delta = (uint32_t)new_size - old_size;
      int _ebx = (int)((char *)result + old_size);
      asm volatile("pushl %[delta]\n\t"
                   "call *%[fn]\n\t"
                   "addl $4, %%esp"
                   : "+b"(_ebx)
                   : [fn] "r"((void *)0x8e8e0), [delta] "r"(delta)
                   : "eax", "ecx", "edx", "memory", "cc");
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
