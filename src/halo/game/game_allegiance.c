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

/**
 * Sets the friendship state between two teams in an allegiance entry.
 *
 * Updates two symmetric 10x10 bitfields in game_allegiance_globals:
 *   +0x94 (incidents): if force==0, sets bits (marks incident); if force!=0,
 *         clears bits (removes incident record).
 *   +0xa4 (hostility): if friendship==0, sets bits (hostile); if friendship!=0,
 *         clears bits (friendly). A set bit means NOT friendly, matching
 *         game_allegiance_get_team_is_friendly which returns (bit == 0).
 *
 * Both bitfields are updated symmetrically for (team_a*10+team_b) and
 * (team_b*10+team_a). After updating, the entry's changed flag is set and
 * game_allegiance_apply_change is called to propagate to AI encounters.
 *
 * Skips the update entirely if force==0 and the friendship value hasn't
 * changed.
 */
void game_allegiance_set(int16_t *entry, char friendship, char force)
{
  int16_t team_a;
  int16_t team_b;
  int bit_ab;
  int bit_ba;

  /* early out: if not forced and friendship hasn't changed, do nothing */
  if (!force && *((char *)entry + 0xa) == friendship)
    return;

  /* store new friendship value */
  *((char *)entry + 0xa) = friendship;

  team_a = entry[0];
  team_b = entry[1];

  if (team_a < 10 && team_b < 10) {
    bit_ab = (int)team_a * 10 + (int)team_b;
    bit_ba = (int)team_b * 10 + (int)team_a;

    /* update incidents bitfield at +0x94 */
    if (!force) {
      /* not forced: set incident bits for both team orderings */
      *(uint32_t *)(game_allegiance_globals + 0x94 + (bit_ab >> 5) * 4) |=
        1 << (bit_ab & 0x1f);
      *(uint32_t *)(game_allegiance_globals + 0x94 + (bit_ba >> 5) * 4) |=
        1 << (bit_ba & 0x1f);
    } else {
      /* forced: clear incident bits for both team orderings */
      *(uint32_t *)(game_allegiance_globals + 0x94 + (bit_ab >> 5) * 4) &=
        ~(1 << (bit_ab & 0x1f));
      *(uint32_t *)(game_allegiance_globals + 0x94 + (bit_ba >> 5) * 4) &=
        ~(1 << (bit_ba & 0x1f));
    }

    /* update hostility bitfield at +0xa4 */
    if (!friendship) {
      /* hostile: set hostility bits for both team orderings */
      *(uint32_t *)(game_allegiance_globals + 0xa4 + (bit_ab >> 5) * 4) |=
        1 << (bit_ab & 0x1f);
      *(uint32_t *)(game_allegiance_globals + 0xa4 + (bit_ba >> 5) * 4) |=
        1 << (bit_ba & 0x1f);
    } else {
      /* friendly: clear hostility bits for both team orderings */
      *(uint32_t *)(game_allegiance_globals + 0xa4 + (bit_ab >> 5) * 4) &=
        ~(1 << (bit_ab & 0x1f));
      *(uint32_t *)(game_allegiance_globals + 0xa4 + (bit_ba >> 5) * 4) &=
        ~(1 << (bit_ba & 0x1f));
    }
  }

  /* mark entry as changed */
  *((char *)entry + 0xb) = 1;

  /* propagate allegiance change to AI encounters */
  game_allegiance_apply_change(entry[0], entry[1], friendship, force);
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
            game_allegiance_set(entry, 0, 0);
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
