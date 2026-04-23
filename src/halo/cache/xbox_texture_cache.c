/* Xbox texture cache: retrieve and block on hardware texture data.
 * Source: c:\halo\SOURCE\cache\xbox_texture_cache.c */
void *xbox_texture_cache_steal_memory(unsigned int size)
{
  int page_count = ((int)(size + (((int)size >> 0x1f) & 0x3fffU)) >> 0xe) + 1;
  int remaining_page_count = 0x4fe - page_count;
  char *base = (char *)FUN_001bdd60() + (remaining_page_count << 0xe);
  int stolen_size = page_count << 0xe;

  if (remaining_page_count < 1) {
    display_assert("remaining_page_count>0",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x13f,
                   true);
    system_exit(-1);
  }

  if (*(int8_t *)0x4ea984 != 0) {
    display_assert("!xbox_texture_cache_globals.stolen_memory",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x140,
                   true);
    system_exit(-1);
  }

  FUN_0011db00(*(void **)0x4ea980, remaining_page_count);
  FUN_001d371d(base + 0x104000, stolen_size, 4);
  FUN_001d371d(base, 0x104000, 2);
  FUN_001d371d(base + 0x104000 + stolen_size, 0x104000, 2);
  *(int8_t *)0x4ea984 = 1;
  return base + 0x104000;
}

void xbox_texture_cache_return_memory(void)
{
  if (*(int8_t *)0x4ea984 == 0) {
    display_assert("xbox_texture_cache_globals.stolen_memory",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x159,
                   true);
    system_exit(-1);
  }

  FUN_0011db00(*(void **)0x4ea980, 0x580);
  FUN_001d371d(FUN_001bdd60(0x1600000, 0x404), 0x1600000, 0x404);
  *(int8_t *)0x4ea984 = 0;
}

bool xbox_texture_cache_request(void *hardware_format @<eax>, bool block)
{
  int cache_block_index = FUN_00183290(hardware_format);

  if (cache_block_index <= *(int32_t *)((char *)hardware_format + 0x1c)) {
    cache_block_index = *(int32_t *)((char *)hardware_format + 0x1c);
  }

  cache_block_index = FUN_0011de10(*(void **)0x4ea980, cache_block_index);
  if (cache_block_index != -1) {
    int cache_page_index = FUN_0011da00(*(void **)0x4ea980, cache_block_index) +
                           *(int32_t *)0x4ea97c;
    int new_texture_index = FUN_00119570(*(void **)0x4ea978, cache_block_index);
    char *cache_entry = datum_get(*(void **)0x4ea978, cache_block_index);

    if (new_texture_index != cache_block_index) {
      display_assert("new_texture_index==cache_block_index",
                     "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x1af,
                     true);
      system_exit(-1);
    }

    *(int32_t *)((char *)hardware_format + 0x24) = cache_block_index;
    *(int32_t *)((char *)hardware_format + 0x2c) = cache_page_index;
    *(void **)(cache_entry + 8) = hardware_format;
    FUN_001bee30();
    *(int16_t *)(cache_entry + 2) =
      FUN_001bc9e0(*(int32_t *)((char *)hardware_format + 0x20),
                   *(int32_t *)((char *)hardware_format + 0x18),
                   *(int32_t *)((char *)hardware_format + 0x1c),
                   cache_page_index, cache_entry + 4, block);
    return true;
  }

  return false;
}

void *xbox_texture_cache_get_hardware_format(void *hardware_format, bool block,
                                             bool load)
{
  void *result = NULL;

  if (!load && block) {
    display_assert("load || !block",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0xd2, true);
    system_exit(-1);
  }

  if (*(int8_t *)((char *)hardware_format + 0xe) < 0) {
    if (*(int32_t *)((char *)hardware_format + 0x24) == -1 && load) {
      xbox_texture_cache_request(hardware_format, block);
    }
    if (*(int32_t *)((char *)hardware_format + 0x24) != -1) {
      void *entry = datum_get(*(void **)0x4ea978,
                              *(int32_t *)((char *)hardware_format + 0x24));
      FUN_0011d9d0(*(void **)0x4ea980,
                   *(int32_t *)((char *)hardware_format + 0x24));
      if (block) {
        if (*(int8_t *)((char *)entry + 4) != 0)
          goto loaded;
        if (*(uint8_t *)0x4ea98a) {
          const char *name =
            tag_get_name(*(int32_t *)((char *)hardware_format + 0x20));
          console_warning((const char *)0x257984, name);
        }
        cache_files_io_request_enable(*(int16_t *)((char *)entry + 2));
      }
      do {
        if (*(int8_t *)((char *)entry + 4) == 0) {
          unsigned int t0 = FUN_001cb8e0();
          unsigned int t1 = system_milliseconds();
          if (t1 - t0 > 0x84u) {
            FUN_001cf2f0();
          }
          SwitchToThread();
        } else {
        loaded:
          if (*(int8_t *)((char *)entry + 5) == 0) {
            *(int8_t *)((char *)entry + 5) = 1;
          }
          result = (char *)entry + 0xc;
          if (result)
            break;
        }
        if (!block)
          return result;
      } while (true);
    }
  } else {
    result = *(void **)((char *)hardware_format + 0x28);
  }

  if (block && !result) {
    unsigned int now = system_milliseconds();
    if (now - *(unsigned int *)0x4ea98c > 10000u) {
      terminal_output(
        *(void **)0x2ee6f4,
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",
        NULL);
      error(2, "YOU GOT STABBED!!!! double-click \"GETSTABBED.BAT\" on your PC "
               "now!!!");
      terminal_output(
        *(void **)0x2ee6f4,
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",
        NULL);
      FUN_0011db90("d:\\stabbed.txt",
                   tag_get_name(*(int32_t *)((char *)hardware_format + 0x20)),
                   *(int32_t *)((char *)hardware_format + 0x1c),
                   *(void **)0x4ea980, (void *)0x18ef30, (void *)0x1beb70);
      *(unsigned int *)0x4ea98c = system_milliseconds();
    }
    result = FUN_00155580(hardware_format);
    if (!result) {
      display_assert("hardware_format",
                     "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x127,
                     true);
      system_exit(-1);
    }
  }

  return result;
}
