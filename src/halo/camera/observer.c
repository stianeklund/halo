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
    display_assert("observer->result.location.cluster_index>=NONE && "
                   "observer->result.location.cluster_index<"
                   "global_structure_bsp_get()->clusters.count",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x12d, 1);
    system_exit(-1);
  }

  return (void *)(entry + 0x74);
}

/* Per-tick observer update for all local players (0x8cde0).
 * Saves the frame's delta-time into the global at 0x335718, then walks
 * each of MAXIMUM_NUMBER_OF_LOCAL_PLAYERS observers (stride 0x29c from
 * 0x33571c), verifies the header/trailer OBSERVER_SIGNATURE ('!dar' =
 * 0x72616421) and that updated_for_frame is clear, marks it set, and
 * dispatches the three unported observer sub-updates:
 *   - 0x8b060: copies/stages the latest camera block from the director into
 *              observer state, including the transition parameters emitted by
 *              camera 0x89cd0 during vehicle-exit blends (EAX=index)
 *   - 0x8cd40: time-dependent integration; original disassembly shows it
 *              ticking down five transition floats at observer+0x5c..0x6c,
 *              which makes it a strong candidate for the actual smoothing
 *              behavior (skipped when dt matches the previous frame time
 *              stored at 0x2533c0) (EAX=index)
 *   - 0x8c4b0: derives the final observer camera result from that staged and
 *              integrated state (EAX=index)
 * The three callees take the local player index in EAX (register arg),
 * so we dispatch them with inline asm — see cache_files_precache_map_loaded
 * for the established pattern. */
void observer_update(float delta_time)
{
  int16_t i;
  char *observer = (char *)0x33571c;
  int _eax;

  *(float *)0x335718 = delta_time;

  for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++, observer += 0x29c) {
    if (local_player_get_player_index(i) == -1)
      continue;

    if (i < 0 || i >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
      display_assert("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72, 1);
      system_exit(-1);
    }

    if (*(int *)(observer + 0x0) != 0x72616421 ||
        *(int *)(observer + 0x298) != 0x72616421) {
      display_assert("observer->header_signature==OBSERVER_SIGNATURE && "
                     "observer->trailer_signature==OBSERVER_SIGNATURE",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x108, 1);
      system_exit(-1);
    }

    if (*(char *)(observer + 0x70) != 0) {
      display_assert("!observer->updated_for_frame",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x109, 1);
      system_exit(-1);
    }

    *(char *)(observer + 0x70) = 1;

    /* observer input/control sub-update — EAX = local_player_index. */
    _eax = i;
    asm volatile("movl $0x8b060, %%ecx\n\t"
                 "call *%%ecx"
                 : "+a"(_eax)
                 :
                 : "ecx", "edx", "esi", "edi", "memory", "cc");

    /* Time-dependent integration sub-update, skipped when delta_time
     * equals the cached previous delta at 0x2533c0 — EAX = index. */
    if (*(float *)0x335718 != *(float *)0x2533c0) {
      _eax = i;
      asm volatile("movl $0x8cd40, %%ecx\n\t"
                   "call *%%ecx"
                   : "+a"(_eax)
                   :
                   : "ecx", "edx", "esi", "edi", "memory", "cc");
    }

    /* Post-update derivation sub-update — EAX = index. */
    _eax = i;
    asm volatile("movl $0x8c4b0, %%ecx\n\t"
                 "call *%%ecx"
                 : "+a"(_eax)
                 :
                 : "ecx", "edx", "esi", "edi", "memory", "cc");

    if (*(int *)(observer + 0x0) != 0x72616421 ||
        *(int *)(observer + 0x298) != 0x72616421) {
      display_assert("observer->header_signature==OBSERVER_SIGNATURE && "
                     "observer->trailer_signature==OBSERVER_SIGNATURE",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x117, 1);
      system_exit(-1);
    }
  }
}
