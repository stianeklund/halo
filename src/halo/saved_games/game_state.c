#include "xbox.h"

void game_state_dispose(void)
{
  xbox_game_state_dispose_buffer();
  xbox_game_state_close_file();
}

/* game_state functions re-enabled for bisect */
/* Initialize game state for a new map: set flags, clear the save header,
 * populate it with the scenario name, build version, and tag checksums. */
void game_state_initialize_for_new_map(void)
{
  char *header;

  *(uint8_t *)0x4ea9a4 = 1;
  *(uint8_t *)0x4ea9a5 = 0;
  *(int *)0x4ea9a8 = -1;

  header = *(char **)0x4ea9ac;
  csmemset(header, 0, 0x14c);

  /* copy scenario name into header at byte offset 4 */
  {
    char *name = ((char *(*)(int))0x1ba1f0)(*(int *)0x326a08);
    ((void (*)(char *, char *))0x8dff0)(header + 0x4, name);
  }

  /* copy build version string at byte offset 0x104 */
  ((void (*)(char *, const char *))0x8dff0)(header + 0x104, "01.10.12.2276");

  /* store map type (0x124), tag checksum (0x126), and cache checksum (0x128) */
  *(int16_t *)(header + 0x124) = *(int16_t *)0x31fa94;
  *(int16_t *)(header + 0x126) = ((int16_t(*)(void))0xa7460)();
  *(int *)(header + 0x128) = ((int (*)(void))0x1b9920)();
  *(int *)header = *(int *)0x4ea9a0;
}

void game_state_dispose_from_old_map(void)
{
}

/* Save the current game state. Pauses rendering, writes the save file,
 * and records whether the save was successful. */
void game_state_save(void)
{
  char saved;

  (*(void (**)())0x32eaa0)();
  ((void (*)(void))0x101c90)();
  saved = ((char (*)(void))0x1c0370)();
  *(uint8_t *)0x4ea9a5 = (saved != 0);
  ((void (*)(void))0x101ca0)();
}

/* Revert to the last saved game state. If no save exists or the map is
 * flagged for restart, calls the restart function instead. Otherwise,
 * loads the save and calls 13 initialize-for-new-map callbacks. */
void game_state_revert(void)
{
  if (*(uint8_t *)0x4ea9a5 == 0 && *(uint8_t *)0x5054e8 == 0) {
    ((void (*)(void))0x1002a0)();
    return;
  }

  (*(void (**)())0x32eaa4)();
  ((void (*)(void))0x1c0450)();

  {
    void (**callbacks)() = (void (**)())0x32eaa8;
    int i;
    for (i = 0; i < 13; i++)
      callbacks[i]();
  }
}

/* Save the game state to a named core file. Logs success or failure
 * to the console. */
void game_state_save_core(const char *name)
{
  char ok = ((char (*)(const char *, void *, void *))0x1c0570)(
    name, *(void **)0x4ea994, (void *)0x345000);
  if (ok)
    ((void (*)(int, const char *, ...))0xff4d0)(0, "saved '%s'", name);
  else
    ((void (*)(int, const char *, ...))0xff4d0)(0, "error writing '%s'", name);
}

void *game_state_malloc(const char *name, const char *group_name, int size)
{
  void *result;

  assert_halt(!(size & 3));
  assert_halt(!game_state_globals.locked);
  assert_halt(game_state_globals.cpu_allocation_size + size <=
              GAME_STATE_CPU_SIZE);

  result =
    game_state_globals.base_address + game_state_globals.cpu_allocation_size;
  game_state_globals.cpu_allocation_size += size;
  crc_checksum_buffer(&game_state_globals.checksum, &size, 4);
  return result;
}

data_t *game_state_data_new(char *name, __int16 maximum_count, __int16 size)
{
  data_t *data; // esi
  int allocation_size; // [esp-Ch] [ebp-18h]

  allocation_size = data_allocation_size(maximum_count, size);
  data = (data_t *)game_state_malloc(name, "data array", allocation_size);
  data_initialize(data, name, maximum_count, size);
  return data;
}

/* Load a core save file. Validates the header, then restores game state
 * and calls 13 initialize-for-new-map callbacks. */
void game_state_load_core(const char *name)
{
  char header[0x14c];

  if (!((char (*)(const char *, void *, int))0x1c0600)(name, header, 0x14c))
    goto fail;

  if (!((char (*)(void))0x1bfa00)())
    goto fail;

  (*(void (**)())0x32eaa4)();
  ((void (*)(const char *, void *, void *))0x1c0680)(name, *(void **)0x4ea994,
                                                     (void *)0x345000);
  ((void (*)(int, const char *, ...))0xff4d0)(0, "loaded '%s'", name);

  {
    void (**callbacks)() = (void (**)())0x32eaa8;
    int i;
    for (i = 0; i < 13; i++)
      callbacks[i]();
  }
  return;

fail:
  ((void (*)(int, const char *, ...))0xff4d0)(0, "couldn't open '%s'", name);
}

void xbox_game_state_dispose_buffer(void)
{
  assert_halt(*(char *)0x4ea9b0);
  MmFreeContiguousMemory(*(void **)0x4ea9b4);
  *(char *)0x4ea9b0 = 0;
}

void xbox_game_state_close_file(void)
{
  assert_halt(*(char *)0x4ea9bc);
  ((bool (*)(int))0x1cf900)(*(int *)0x4ea9c0);
  *(char *)0x4ea9bc = 0;
}
