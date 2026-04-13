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

void *debug_malloc(uint32_t size, bool zero, const char *file, int line)
{
  debug_allocation_header_t *header;

  if (size >= 0x10000000) {
    display_assert("size>=0 && size<MAXIMUM_POINTER_SIZE",
                   "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0xd6, true);
    system_exit(-1);
  }

  if (*(uint32_t *)0x2ee74c != 0x53414654 ||
      *(uint32_t *)0x2ee768 != 0x53414654) {
    display_assert(
      csprintf(error_string_buffer,
               "Debug memory manager is uninitialized or corrupted. "
               "(%s:%d)",
               file, line),
      "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x91, true);
    system_exit(-1);
  }

  header = ((debug_allocation_header_t * (*)(uint32_t))0x8e3d0)(size + 0x24);
  if (header == NULL) {
    return NULL;
  }

  header->file = file;
  header->begin_guard = 0x2d2d2d3e;
  header->line = line;
  header->sequence = *(uint32_t *)0x2ee764;
  (*(uint32_t *)0x2ee764)++;
  header->size = size;
  *(uint32_t *)((char *)header + size + 0x20) = 0x3c2d2d2d;

  {
    register debug_allocation_header_t *_esi asm("esi") = header;
    asm volatile("" : "+r"(_esi));
    ((void (*)(void))0x8e950)();
  }

  header = (debug_allocation_header_t *)((char *)header + 0x20);
  csmemset(header, zero ? 0 : 0xca, size);
  *(uint32_t *)0x2ee750 += size;
  if (*(uint32_t *)0x2ee754 < *(uint32_t *)0x2ee750) {
    *(uint32_t *)0x2ee754 = *(uint32_t *)0x2ee750;
  }

  return header;
}

void debug_free(void *ptr, const char *file, int line)
{
  debug_allocation_header_t *header;

  header = (debug_allocation_header_t *)((char *)ptr - 0x20);
  if (*(uint32_t *)0x2ee74c != 0x53414654 ||
      *(uint32_t *)0x2ee768 != 0x53414654) {
    display_assert(
      csprintf(error_string_buffer,
               "Debug memory manager is uninitialized or corrupted. "
               "(%s:%d)",
               file, line),
      "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x91, true);
    system_exit(-1);
  }

  ((void (*)(void))0x8e7d0)();
  if (*(uint32_t *)((char *)header + header->size + 0x20) != 0x3c2d2d2d) {
    display_assert(
      csprintf(error_string_buffer,
               "Pointer allocated at %s, %d has overrun the end of its "
               "buffer. (Size: %d) (%s:%d)",
               header->file, header->line, header->size, file, line),
      "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 199, true);
    system_exit(-1);
  }

  *(uint32_t *)0x2ee750 -= header->size;
  {
    register debug_allocation_header_t *_eax asm("eax") = header;
    asm volatile("" : "+r"(_eax));
    ((void (*)(const char *, int))0x8e9f0)(file, line);
  }
  header->begin_guard = 0x3c424144;
  ((void (*)(void *))0x8e3e0)(header);
}
