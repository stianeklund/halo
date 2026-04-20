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
    ((void (*)(void))0x1bc760)();
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

/* Returns true if the named map has already been precached.
 * 0x1bd1b0 reads EDI as the canonical map name (set from 0x19b0d0). */
bool cache_files_precache_map_loaded(char *map_name)
{
  int _edi = (int)((char *(*)(char *))0x19b0d0)(map_name);
  int16_t result;
  asm volatile("movl $0x1bd1b0, %%eax\n\t"
               "call *%%eax"
               : "+D"(_edi), "=a"(result)
               :
               : "ecx", "edx", "memory", "cc");
  return result != -1;
}

#if 0 /* bisect: disable map_begin and map_end */
/* Begin precaching a map from DVD to the cache partition. Returns true
 * if the copy was already done or was successfully started. */
bool cache_files_precache_map_begin(char *map_name, bool show_error)
{
  char path[256];
  char *canonical;
  int16_t cache_idx;

  canonical = ((char *(*)(char *))0x19b0d0)(map_name);
  ((char *(*)(char *))0x19b0d0)(map_name);
  cache_idx = ((int16_t (*)(void))0x1bd1b0)();

  if (cache_idx == -1) {
    if (!((char (*)(void))0x1bcb80)()) {
      error(2, "couldn't find map '%s' on the DVD", canonical);
      if (show_error)
        ((void (*)(void))0xe8d20)();
      return 0;
    }

    {
      int copy_handle;
      int16_t slot;
      int block;

      copy_handle = ((int (*)(bool))0x1ba250)(show_error);
      *(int *)(path + 0x148) = ((int (*)(int))0x1bea30)(copy_handle);
      slot = ((int16_t (*)(int))0x1bd210)(*(int *)(path + 0x148));

      block = ((int (*)(void))0x1bc720)();
      csmemset((void *)(block + 0xc), 0, 0x800);

      *(uint8_t *)0x4e9220 = 1;
      *(int16_t *)0x4e9222 = slot;
      csstrncpy((char *)0x4e9224, canonical, 0x1f);
      *(uint8_t *)0x4e9243 = 0;

      ((int (*)(char *, const char *, ...))0x1d90f0)(
        path, "d:\\maps\\%s.map", canonical);
      error(2, "starting precaching of map '%s'", canonical);

      {
        int file = ((int (*)(char *))0x1bc7e0)(path);
        int mapped = ((int (*)(int))0x1bc7a0)(file);
        ((void (*)(int, int, int))0x1ba2f0)(
          *(int *)(path + 0x148), copy_handle, mapped);
      }
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
  ((void (*)(void))0x1bcfb0)();
  ((void (*)(int16_t))0x1bd020)(*(int16_t *)0x4e9222);

  *(uint8_t *)0x4e9220 = 0;
  *(int16_t *)0x4e9222 = -1;
}
#endif

/* Load cached game state if the cached map metadata matches the currently
 * loaded scenario, map type, checksum, and difficulty. */
void cache_files_precache(void)
{
  char header[0x14c];
  int scratch;

  if (!((char (*)(void *, void *, int, void *, int))0x1c0910)(
        header, &scratch, sizeof(header), (void *)0x345000, 0)) {
    return;
  }

  if (((int (*)(const char *, const char *))0x8dcb0)(header + 0x104,
                                                     "01.10.12.2276") != 0) {
    return;
  }

  {
    const char *scenario_name =
      ((const char *(*)(int))0x1ba1f0)(*(int *)0x326a08);
    if (((int (*)(const char *, const char *))0x8dcb0)(header + 0x4,
                                                       scenario_name) != 0) {
      return;
    }
  }

  if (*(int *)header != *(int *)0x4ea9a0)
    return;
  if (*(int16_t *)(header + 0x124) != *(int16_t *)0x31fa94)
    return;
  if (*(int *)(header + 0x128) != ((int (*)(void))0x1b9920)())
    return;
  if (*(int16_t *)(header + 0x126) != ((int16_t(*)(void))0x100080)())
    return;

  (*(void (**)())0x32eaa4)();
  ((void (*)(void *, void *))0x1c0c20)(*(void **)0x4ea994, (void *)0x345000);
  ((void (*)(int16_t))0xa7440)(((int16_t(*)(void))0x100080)());
  ((void (*)(void))0x1bf790)();
  (*(void (**)())0x32eaa0)();
  ((void (*)(void))0x101c90)();
  *(uint8_t *)0x4ea9a5 = ((char (*)(void))0x1c0370)() != 0;
  ((void (*)(void))0x101ca0)();
}
