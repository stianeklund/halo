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
 * Returns whether two teams have had an allegiance incident (natural change).
 *
 * Checks a 10x10 bitfield at game_allegiance_globals+0x94. Each bit represents
 * a team pair (team_a * 10 + team_b). A SET bit means the teams have had an
 * incident; a CLEAR bit means they have not.
 *
 * Out-of-range team indices (negative or >= 10) return false.
 */
bool FUN_000a7a90(int16_t team_a, int16_t team_b)
{
  int bit_index;
  bool result;

  result = false;
  if (team_a >= 0 && team_a < 10 && team_b >= 0 && team_b < 10) {
    bit_index = team_a * 10 + team_b;
    result =
      (*(uint32_t *)(game_allegiance_globals + 0x94 + (bit_index >> 5) * 4) &
       (1 << (bit_index & 0x1f))) != 0;
  }
  return result;
}

bool FUN_000a7ae0(int16_t team_a, int16_t team_b)
{
  int16_t i;
  int16_t *entry;

  i = 0;
  entry = (int16_t *)game_allegiance_globals + 1;
  if (*(int16_t *)game_allegiance_globals > 0) {
    while ((entry[0] != team_a || entry[1] != team_b) &&
           (entry[1] != team_a || entry[0] != team_b)) {
      i++;
      entry += 9;
      if (*(int16_t *)game_allegiance_globals <= i) {
        return 0;
      }
    }
    return *((char *)entry + 0xb);
  }
  return 0;
}

int16_t FUN_000a7b40(int16_t team_a, int16_t team_b, int16_t *out_threshold)
{
  int16_t i;
  int16_t result;
  int16_t threshold;
  int16_t *entry;

  i = 0;
  entry = (int16_t *)game_allegiance_globals + 1;
  result = 0;
  threshold = -1;
  if (*(int16_t *)game_allegiance_globals > 0) {
    do {
      if ((entry[0] == team_a && entry[1] == team_b) ||
          (entry[1] == team_a && entry[0] == team_b)) {
        result = entry[7];
        threshold = entry[2];
        break;
      }
      i++;
      entry += 9;
    } while (i < *(int16_t *)game_allegiance_globals);
  }
  if (out_threshold != NULL) {
    *out_threshold = threshold;
  }
  return result;
}

void FUN_000a7bc0(int16_t team_a, int16_t team_b)
{
  int16_t i;
  int16_t *entry;

  i = 0;
  entry = (int16_t *)game_allegiance_globals + 1;
  if (*(int16_t *)game_allegiance_globals > 0) {
    while (
      (entry[0] != team_a || entry[1] != team_b || *((char *)entry + 9) == 0) &&
      (entry[1] != team_a || entry[0] != team_b || *((char *)entry + 8) == 0)) {
      i++;
      entry += 9;
      if (*(int16_t *)game_allegiance_globals <= i) {
        return;
      }
    }
    if (entry[7] > 0 && entry[3] != -1) {
      entry[8] = entry[3];
    }
  }
}

void FUN_000a7c30(int16_t team_a, int16_t team_b)
{
  int16_t i;
  int16_t *entry;

  i = 0;
  entry = (int16_t *)game_allegiance_globals + 1;
  if (*(int16_t *)game_allegiance_globals > 0) {
    while ((entry[0] != team_a || entry[1] != team_b) &&
           (entry[1] != team_a || entry[0] != team_b)) {
      i++;
      entry += 9;
      if (*(int16_t *)game_allegiance_globals <= i) {
        return;
      }
    }
    *((char *)entry + 0xb) = 0;
  }
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

/**
 * Creates or reuses an allegiance entry between two teams.
 *
 * Searches for an existing entry matching (team_a, team_b) or (team_b, team_a).
 * If found, reuses it; otherwise creates a new one (up to 8 entries max).
 * Populates the entry and transitions it to hostile (friendship=0) via
 * game_allegiance_set.
 */
void game_allegiance_create(int16_t team_a, char is_player, int16_t team_b,
                            char is_timer, int16_t threshold, int16_t timer,
                            char is_ally)
{
  int16_t i;
  int16_t count;
  int16_t *entry;
  int16_t *globals;

  globals = (int16_t *)game_allegiance_globals;
  count = globals[0];
  entry = globals + 1;

  for (i = 0; i < count; i++) {
    if ((entry[0] == team_a && entry[1] == team_b) ||
        (entry[1] == team_a && entry[0] == team_b)) {
      break;
    }
    entry += 9;
  }

  if (i >= count) {
    if (count < 8) {
      globals[0] = count + 1;
    } else {
      error(2, "game_allegiance_create: too many allegiances (maximum is %d)",
            8);
      globals = (int16_t *)game_allegiance_globals;
    }
  }

  if (i < globals[0]) {
    entry = globals + 1 + i * 9;
    entry[1] = team_b;
    entry[0] = team_a;
    entry[3] = timer;
    *((char *)entry + 8) = is_player;
    *((char *)entry + 9) = is_timer;
    entry[7] = 0;
    entry[8] = 0;
    *((char *)entry + 0xa) = 1;
    game_allegiance_set(entry, 0, 0);
    *((char *)entry + 0xb) = 0;
    entry[2] = threshold;
    *((char *)entry + 0xc) = is_ally;
  }
}

/**
 * Removes the allegiance entry between two teams.
 *
 * Finds the matching entry, sets it to friendly (friendship=1, forced),
 * then removes it by swapping with the last entry and decrementing the count.
 * Returns true if an entry was found and removed.
 */
bool game_allegiance_remove(int16_t team_a, int16_t team_b)
{
  int16_t i;
  int16_t count;
  int16_t *entry;
  int16_t *globals;

  globals = (int16_t *)game_allegiance_globals;
  count = globals[0];
  entry = globals + 1;

  for (i = 0; i < count; i++) {
    if ((entry[0] == team_a && entry[1] == team_b) ||
        (entry[1] == team_a && entry[0] == team_b)) {
      game_allegiance_set(entry, 1, 1);
      globals = (int16_t *)game_allegiance_globals;
      globals[0] = count - 1;
      if (i < globals[0]) {
        int16_t *last = globals + 1 + globals[0] * 9;
        entry[0] = last[0];
        entry[1] = last[1];
        entry[2] = last[2];
        entry[3] = last[3];
        entry[4] = last[4];
        entry[5] = last[5];
        entry[6] = last[6];
        entry[7] = last[7];
        entry[8] = last[8];
      }
      return true;
    }
    entry += 9;
  }
  return false;
}

/**
 * Bumps the incident count on an allegiance entry, optionally flipping it.
 *
 * Searches for an entry matching (team_a, team_b) or (team_b, team_a).
 * Depending on action: 0 adds +1 incident, 1 adds +3, 2 adds -1.
 * If current_incidents reaches the threshold, flips the entry to friendly
 * via game_allegiance_set and returns true.
 */
bool game_allegiance_bump(int16_t team_a, int16_t team_b, int16_t action,
                          bool *out_changed)
{
  int16_t i;
  int16_t count;
  int16_t delta;
  int16_t *entry;
  int16_t *globals;

  globals = (int16_t *)game_allegiance_globals;
  count = globals[0];
  entry = globals + 1;

  for (i = 0; i < count; i++) {
    if ((entry[0] == team_a && entry[1] == team_b &&
         *((char *)entry + 9) != 0) ||
        (entry[1] == team_a && entry[0] == team_b &&
         *((char *)entry + 8) != 0)) {
      delta = 0;
      if (action == 0) {
        delta = 1;
      } else if (action == 1) {
        delta = 3;
      } else if (action == 2) {
        delta = -1;
      }
      entry[7] = entry[7] + delta;
      if (entry[3] != -1) {
        entry[8] = entry[3];
      }
      if (entry[2] == -1) {
        return false;
      }
      if (entry[7] >= entry[2]) {
        game_allegiance_set(entry, 1, 0);
        if (out_changed != NULL) {
          *out_changed = (*((char *)entry + 0xc) == 0);
        }
        return true;
      }
      return false;
    }
    entry += 9;
  }
  return false;
}
