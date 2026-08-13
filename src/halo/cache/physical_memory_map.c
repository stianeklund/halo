#include "xbox.h" /* Xbox kernel/XAPI decls (Mm*, XPhysicalAlloc) */

#ifdef HALO_RETAIL64
#define HALO_TAG_CACHE_SIZE 0x1600000
#define HALO_TEXTURE_CACHE_SIZE 0x800000
#define HALO_SOUND_CACHE_SIZE 0x200000
#else
#define HALO_TAG_CACHE_SIZE 0x1600000
#define HALO_TEXTURE_CACHE_SIZE 0x1600000
#define HALO_SOUND_CACHE_SIZE 0x400000
#endif

void physical_memory_allocate(void)
{
  physical_memory_map_globals.game_state_base_address =
    XPhysicalAlloc(0x345000, GAME_STATE_BASE_ADDRESS - 0x80000000, 0, 4);
  assert_halt(
    (unsigned long)physical_memory_map_globals.game_state_base_address ==
    GAME_STATE_BASE_ADDRESS);

  physical_memory_map_globals.tag_cache_base_address = XPhysicalAlloc(
    HALO_TAG_CACHE_SIZE, TAG_CACHE_BASE_ADDRESS - 0x80000000, 0, 4);
  assert_halt(
    (unsigned long)physical_memory_map_globals.tag_cache_base_address ==
    TAG_CACHE_BASE_ADDRESS);

  physical_memory_map_globals.texture_cache_base_address =
    XPhysicalAlloc(HALO_TEXTURE_CACHE_SIZE, (uint32_t)-1, 0, 0x404);
  assert_halt(physical_memory_map_globals.texture_cache_base_address);

  physical_memory_map_globals.sound_cache_base_address =
    XPhysicalAlloc(HALO_SOUND_CACHE_SIZE, (uint32_t)-1, 0, 4);
  assert_halt(physical_memory_map_globals.sound_cache_base_address);
}

/* Verify all physical memory map pages are read-write accessible. */
void physical_memory_map_verify(void)
{
  unsigned int addr;
  int page_status;

  for (addr = (unsigned int)physical_memory_map_globals.tag_cache_base_address;
       addr < (unsigned int)physical_memory_map_globals.tag_cache_base_address + HALO_TAG_CACHE_SIZE;
       addr += 0x1000)
  {
    page_status = MmQueryAddressProtect((void *)addr);
    assert_halt(page_status == PAGE_READWRITE);
  }

  for (addr = (unsigned int)physical_memory_map_globals.game_state_base_address;
       addr < (unsigned int)physical_memory_map_globals.game_state_base_address + 0x305000;
       addr += 0x1000)
  {
    page_status = MmQueryAddressProtect((void *)addr);
    assert_halt(page_status == PAGE_READWRITE);
  }
}

/* Free all physical memory map allocations. */
void physical_memory_deallocate(void)
{
  if (physical_memory_map_globals.game_state_base_address)
    MmFreeContiguousMemory(physical_memory_map_globals.game_state_base_address);
  if (physical_memory_map_globals.tag_cache_base_address)
    MmFreeContiguousMemory(physical_memory_map_globals.tag_cache_base_address);
  if (physical_memory_map_globals.texture_cache_base_address)
    MmFreeContiguousMemory(physical_memory_map_globals.texture_cache_base_address);
  if (physical_memory_map_globals.sound_cache_base_address)
    MmFreeContiguousMemory(physical_memory_map_globals.sound_cache_base_address);
}
