void physical_memory_allocate(void)
{
  // FIXME: using __stdcall in kb.json breaks linking hence this cast
  void *(__stdcall * p_XPhysicalAlloc)(size_t, uint32_t, uint32_t, uint32_t) =
    (void *)XPhysicalAlloc;

  physical_memory_map_globals.game_state_base_address =
    p_XPhysicalAlloc(0x345000, GAME_STATE_BASE_ADDRESS - 0x80000000, 0, 4);
  assert_halt(
    (unsigned long)physical_memory_map_globals.game_state_base_address ==
    GAME_STATE_BASE_ADDRESS);

  physical_memory_map_globals.tag_cache_base_address =
    p_XPhysicalAlloc(0x1600000, TAG_CACHE_BASE_ADDRESS - 0x80000000, 0, 4);
  assert_halt(
    (unsigned long)physical_memory_map_globals.tag_cache_base_address ==
    TAG_CACHE_BASE_ADDRESS);

  physical_memory_map_globals.texture_cache_base_address =
    p_XPhysicalAlloc(0x1600000, (uint32_t)-1, 0, 0x404);
  assert_halt(physical_memory_map_globals.texture_cache_base_address);

  physical_memory_map_globals.sound_cache_base_address =
    p_XPhysicalAlloc(0x400000, (uint32_t)-1, 0, 4);
  assert_halt(physical_memory_map_globals.sound_cache_base_address);
}
