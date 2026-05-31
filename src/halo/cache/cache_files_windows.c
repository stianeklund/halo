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
        display_assert(err_buf,
                       "c:\\halo\\SOURCE\\cache\\cache_files_windows.c",
                       0x305, 1);
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
        display_assert(err_buf,
                       "c:\\halo\\SOURCE\\cache\\cache_files_windows.c",
                       0x2f9, 1);
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
  cache_idx = ((int16_t (*)(void))0x1bd1b0)();

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
      slot = FUN_001bd210(
        *(int16_t *)(header_buf + 0x60), *(int *)(header_buf + 8));

      block = (int)FUN_001bc720(slot);
      csmemset((void *)(block + 0xc), 0, 0x800);

      *(uint8_t *)0x4e9220 = 1;
      *(int16_t *)0x4e9222 = slot;
      csstrncpy((char *)0x4e9224, canonical, 0x1f);
      *(uint8_t *)0x4e9243 = 0;

      ((int (*)(char *, const char *, ...))0x1d90f0)(
        path, "d:\\maps\\%s.map", canonical);
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
