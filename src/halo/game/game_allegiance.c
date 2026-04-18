void game_allegiance_initialize(void)
{
  game_allegiance_globals =
    (char *)game_state_malloc("game allegiance globals", 0, 0xb4);
  csmemset(game_allegiance_globals, 0, 0xb4);
}

void game_allegiance_dispose(void)
{
}

void game_allegiance_initialize_for_new_map(void)
{
  int bit;
  int i;

  assert_halt(game_allegiance_globals);

  *(int16_t *)game_allegiance_globals = 0;
  csmemset(game_allegiance_globals + 0x94, 0, 0x10);
  csmemset(game_allegiance_globals + 0xa4, 0, 0x10);

  bit = 0;
  for (i = 0; i < 10; i++) {
    *(uint32_t *)(game_allegiance_globals + 0xa4 + (bit >> 5) * 4) |=
      1 << (bit & 0x1f);
    bit += 11;
  }
}

void game_allegiance_dispose_from_old_map(void)
{
}

/**
 * Returns whether two teams are friendly (not hostile) to each other.
 *
 * Checks a 10x10 bitfield at game_allegiance_globals+0xa4. Each bit represents
 * a team pair (team_a * 10 + team_b). A SET bit means the teams are NOT
 * friendly; a CLEAR bit means they ARE friendly.
 *
 * Out-of-range team indices (negative or >= 10) return true (friendly).
 */
bool game_allegiance_get_team_is_friendly(int16_t team_a, int16_t team_b)
{
  int bit_index;

  if (team_a < 0 || team_a >= 10 || team_b < 0 || team_b >= 10)
    return true;

  bit_index = team_a * 10 + team_b;
  return (*(uint32_t *)(game_allegiance_globals + 0xa4 + (bit_index >> 5) * 4) &
          (1 << (bit_index & 0x1f))) == 0;
}

void game_allegiance_update(void)
{
  int16_t i;
  int16_t *entry;

  i = 0;
  entry = (int16_t *)(game_allegiance_globals + 2);
  if (*(int16_t *)game_allegiance_globals > 0) {
    do {
      if (entry[8] > 0) {
        entry[8] = entry[8] - 1;
        if (entry[8] == 0) {
          if (entry[7] < 1) {
            display_assert("allegiance->current_incidents > 0",
                           "c:\\halo\\SOURCE\\game\\game_allegiance.c", 0x79,
                           1);
            system_exit(-1);
          }
          entry[7] = entry[7] - 1;
          if (entry[7] == 0) {
            /* allegiance_set(entry, friendship=0, force=0)
             * uses EAX=entry, BL=friendship, stack=force */
            __asm__ volatile("pushl $0\n\t"
                             "xorl %%ebx, %%ebx\n\t"
                             "call *%0\n\t"
                             "addl $4, %%esp"
                             :
                             : "r"((void *)0xa7c90), "a"((int)entry)
                             : "ebx", "ecx", "edx", "memory");
          } else {
            entry[8] = entry[3];
          }
        }
      }
      i++;
      entry += 9;
    } while (i < *(int16_t *)game_allegiance_globals);
  }
}
