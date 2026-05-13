void physical_memory_allocate(void)
{
  physical_memory_map_globals.game_state_base_address =
    XPhysicalAlloc(0x345000, GAME_STATE_BASE_ADDRESS - 0x80000000, 0, 4);
  assert_halt(
    (unsigned long)physical_memory_map_globals.game_state_base_address ==
    GAME_STATE_BASE_ADDRESS);

  physical_memory_map_globals.tag_cache_base_address =
    XPhysicalAlloc(0x1600000, TAG_CACHE_BASE_ADDRESS - 0x80000000, 0, 4);
  assert_halt(
    (unsigned long)physical_memory_map_globals.tag_cache_base_address ==
    TAG_CACHE_BASE_ADDRESS);

  physical_memory_map_globals.texture_cache_base_address =
    XPhysicalAlloc(0x1600000, (uint32_t)-1, 0, 0x404);
  assert_halt(physical_memory_map_globals.texture_cache_base_address);

  physical_memory_map_globals.sound_cache_base_address =
    XPhysicalAlloc(0x400000, (uint32_t)-1, 0, 4);
  assert_halt(physical_memory_map_globals.sound_cache_base_address);
}

/* Verify all physical memory map pages are read-write accessible. */
void physical_memory_map_verify(void)
{
  unsigned int addr;
  int page_status;

  addr = (unsigned int)physical_memory_map_globals.tag_cache_base_address;
  while (addr <
         (unsigned int)physical_memory_map_globals.tag_cache_base_address +
           0x1600000) {
    page_status = MmQueryAddressProtect((void *)addr);
    assert_halt(page_status == PAGE_READWRITE);
    addr += 0x1000;
  }

  addr = (unsigned int)physical_memory_map_globals.game_state_base_address;
  while (addr <
         (unsigned int)physical_memory_map_globals.game_state_base_address +
           0x305000) {
    page_status = MmQueryAddressProtect((void *)addr);
    assert_halt(page_status == PAGE_READWRITE);
    addr += 0x1000;
  }
}

/* Free all physical memory map allocations. */
void physical_memory_deallocate(void)
{
  if (physical_memory_map_globals.game_state_base_address) {
    MmFreeContiguousMemory(physical_memory_map_globals.game_state_base_address);
  }
  if (physical_memory_map_globals.tag_cache_base_address) {
    MmFreeContiguousMemory(physical_memory_map_globals.tag_cache_base_address);
  }
  if (physical_memory_map_globals.texture_cache_base_address) {
    MmFreeContiguousMemory(
      physical_memory_map_globals.texture_cache_base_address);
  }
  if (physical_memory_map_globals.sound_cache_base_address) {
    MmFreeContiguousMemory(
      physical_memory_map_globals.sound_cache_base_address);
  }
}