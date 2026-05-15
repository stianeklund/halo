void debug_malloc_check_all(const char *file, int line);

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

void debug_dump_memory_for_file(const char *tag_filter)
{
  debug_allocation_header_t *node;
  void *stream;
  int total;

  debug_malloc_check_all("c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x201);
  node = *(debug_allocation_header_t **)0x2ee758;
  stream = NULL;
  total = 0;
  if (node != NULL) {
    do {
      if (tag_filter == NULL || crt_strstr(node->file, tag_filter) != NULL) {
        if (stream == NULL) {
          stream = crt_fopen("d:\\heap_dump.txt", "a+b");
          if (stream != NULL) {
            crt_fprintf(stream, "% 40s  % 6s % 10s % 10s\r\n", "file", "line",
                        "id", "size");
            goto write_entry;
          }
        } else {
        write_entry:
          crt_fprintf(stream, "% 40s  % 6d % 10d % 10d bytes\r\n", node->file,
                      node->line, node->sequence, node->size);
        }
        total += (int)node->size;
      }
      node = node->next;
    } while (node != NULL);
    if (stream != NULL) {
      crt_fprintf(stream, "\r\nTotal Allocated: %d bytes\r\n\r\n", total);
      crt_fclose(stream);
    }
  }
}

void debug_dump_memory_by_file(void)
{
  uint32_t table[0x200][5];
  debug_allocation_header_t *node;
  void *stream;
  int total_size;
  int alloc_count;
  int32_t idx;
  uint32_t i;
  uint16_t file_count;
  uint16_t j;

  node = *(debug_allocation_header_t **)0x2ee758;
  total_size = 0;
  alloc_count = 0;
  file_count = 0;
  debug_malloc_check_all("c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x229);
  if (node != NULL) {
    do {
      total_size += (int)node->size;
      alloc_count++;
      j = 0;
      while ((int16_t)j < (int16_t)file_count) {
        if (table[j][0] == (uint32_t)(uintptr_t)node->file)
          break;
        j++;
      }
      if (j == file_count) {
        if ((int16_t)file_count > 0x1ff) {
          display_assert("file_count<MAXIMUM_FILES_WITH_POINTERS",
                         "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x249,
                         true);
          system_exit(-1);
        }
        idx = (int16_t)file_count;
        table[idx][0] = (uint32_t)(uintptr_t)node->file;
        table[idx][1] = 1;
        table[idx][2] = node->size;
        table[idx][3] = node->size;
        table[idx][4] = node->size;
        file_count++;
      } else {
        idx = (int16_t)j;
        table[idx][1]++;
        table[idx][4] += node->size;
        if (node->size < table[idx][2])
          table[idx][2] = node->size;
        if (table[idx][3] < node->size)
          table[idx][3] = node->size;
      }
      node = node->next;
    } while (node != NULL);
    if (file_count != 0) {
      stream = crt_fopen("d:\\heap_dump.txt", "a+b");
      if (stream != NULL) {
        qsort(table, (size_t)(int16_t)file_count, sizeof(table[0]),
              FUN_0008e750);
        if ((int16_t)file_count > 0) {
          for (i = 0; i < (uint32_t)(uint16_t)file_count; i++) {
            crt_fprintf(stream,
                        "File: %32s %8d bytes in %4d pointers. (Min: %8d Max: "
                        "%8d Avg: %5.3f)\r\n",
                        (const char *)table[i][0], (int)table[i][4],
                        (int)table[i][1], (int)table[i][2], (int)table[i][3],
                        (double)table[i][4] / (double)table[i][1]);
          }
        }
        if (total_size != *(int *)0x2ee750) {
          display_assert(
            "total_pointer_size==debug_memory_globals.current_heap_size",
            "c:\\halo\\SOURCE\\cseries\\debug_memory.c", 0x264, true);
          system_exit(-1);
        }
        crt_fprintf(stream,
                    "\r\nTotal: %40d bytes in %4d pointers\r\nLargest Heap "
                    "Size: %28d bytes\r\n\r\n",
                    total_size, alloc_count, *(int *)0x2ee754);
        crt_fclose(stream);
      }
    }
  }
}

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
    /* link header into debug allocation list; function reads ESI */
    int _esi = (int)header;
    asm volatile("call *%[fn]"
                 : "+S"(_esi)
                 : [fn] "r"((void *)0x8e950)
                 : "eax", "ecx", "edx", "memory", "cc");
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

  /* validate debug header; reads ESI=header, EBX=file, EDI=line */
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
    /* unlink header from debug allocation list; function reads EAX */
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
  header->begin_guard = 0x3c424144;
  ((void (*)(void *))0x8e3e0)(header);
}
