/* Camera observer — tracks camera position/orientation per player. */

void observer_initialize(void)
{
}

/* Initialize observers for all 4 players. Calls 0x8a350 with ESI
 * pointing to each player's observer data (base 0x33571c, stride 0x29c). */
int observer_initialize_for_new_map(void)
{
  int16_t i;
  char *entry = (char *)0x33571c;

  for (i = 0; i < 4; i++) {
    assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
    {
      char *_esi = entry;
      asm volatile("movl $0x8a350, %%eax\n\t"
                   "call *%%eax"
                   : "+S"(_esi)
                   :
                   : "eax", "ecx", "edx", "edi", "memory", "cc");
    }
    entry += 0x29c;
  }
  return 0;
}

void observer_dispose_from_old_map(void)
{
}

/* Return a pointer to the observer camera result for a local player.
 * Base at 0x33571c, stride 0x29c, camera result at offset +0x74.
 * Validates the cluster index against the current BSP. */
void *observer_get_camera(unsigned __int16 local_player_index)
{
  int16_t idx = (int16_t)local_player_index;
  char *entry;

  if (idx == -1)
    return 0;

  assert_halt(idx >= 0 && idx < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  entry = (char *)0x33571c + (int)idx * 0x29c;

  if (*(int16_t *)(entry + 0x84) < -1 ||
      (int)*(int16_t *)(entry + 0x84) >=
        *(int *)((char *)((void *(*)(void))0x18e3c0)() + 0x134)) {
    display_assert(
      "observer->result.location.cluster_index>=NONE && "
      "observer->result.location.cluster_index<"
      "global_structure_bsp_get()->clusters.count",
      "c:\\halo\\SOURCE\\camera\\observer.c", 0x12d, 1);
    system_exit(-1);
  }

  return (void *)(entry + 0x74);
}

/* NOTE: observer_update (0x8cde0) is complex and left to original. */
