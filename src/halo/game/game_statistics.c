void game_statistics_start(void)
{
  *(uint8_t *)0x457060 = 1;
}

/* Update game statistics for all items in the data table.
 * Stores the current game time (in seconds) at +0xb8, marks entry active at
 * +0x8e, and increments the match counter at +0x90 for entries matching
 * param_1. */
void game_statistics_stop(short param_1)
{
  data_iter_t local_14;
  char *item;
  int time;

  data_iterator_new(&local_14, *(void **)0x5aa6d4);
  item = (char *)data_iterator_next(&local_14);
  if (item != NULL) {
    do {
      time = game_time_get();
      *(int *)(item + 0xb8) = time / 0x1e;
      *(short *)(item + 0x8e) = 1;
      if (*(int *)(item + 0x20) == (int)param_1) {
        *(short *)(item + 0x90) += 1;
      }
      item = (char *)data_iterator_next(&local_14);
    } while (item != NULL);
  }
  *(char *)0x457060 = 0;
}

void game_statistics_record_damage(int handle, float vitality, int param_3, int param_4,
                  int param_5)
{
}

/* Record death/kill statistics for a unit that just died.
 * Resolves the unit to a player entry, increments death counters, walks the
 * object's attacker table (4 slots at +0x3e0) to find the best damage
 * contributor, updates kill/assist credits on the attacker entries, then
 * calls game_engine_player_killed to finalize the event. */
void FUN_000b56f0(int handle, int param_2, int param_3, int param_4)
{
  unsigned int player_index;
  int my_team;
  char *player_entry;
  char is_enemy_kill;
  int kill_time;
  int attack_window;
  char *unit_obj;
  unsigned int *att;
  short best_idx;
  float best_score;
  short alt_best;
  short i;
  char *killer_entry;
  unsigned int uVar2;
  short j;
  unsigned int *att2;
  char *assist_entry;
  char is_team_kill;

  if (*(char *)0x457060 == 0) {
    return;
  }
  player_index = (unsigned int)player_index_from_unit_index(handle);
  if (player_index == 0xffffffffU) {
    return;
  }
  player_entry = (char *)datum_get(*(void **)0x5aa6d4, (int)player_index);
  my_team = *(int *)(player_entry + 0x20);
  player_entry = (char *)datum_get(*(void **)0x5aa6d4, (int)player_index);
  if ((unsigned int)param_2 == player_index) {
    *(short *)(player_entry + 0xac) += 1;
  }
  *(short *)(player_entry + 0xaa) += 1;
  *(short *)(player_entry + 0x92) = 0;
  *(short *)(player_entry + 0x96) = (short)0xffff;
  *(short *)(player_entry + 0x94) = 0;

  is_enemy_kill = 0;
  if ((unsigned int)param_2 == 0xffffffffU ||
      !game_allegiance_get_team_is_friendly(my_team, param_4)) {
    is_enemy_kill = 1;
  }

  kill_time = game_time_get() - 0x78;
  attack_window = game_time_get() - 0xb4;
  unit_obj = (char *)object_get_and_verify_type(handle, 3);
  att = (unsigned int *)(unit_obj + 0x3e0);
  best_idx = -1;
  best_score = -3.4028235e+38f;
  alt_best = -1;
  i = 0;
  while (i < 4) {
    if (att[3] == (unsigned int)param_2) {
      alt_best = i;
      goto b56f0_time_check;
    }
    if (alt_best == i)
      goto b56f0_time_check;
    if (!is_enemy_kill)
      goto b56f0_loop_next;
    if (!game_allegiance_get_team_is_friendly(my_team, att[3]))
      goto b56f0_loop_next;
  b56f0_time_check:
    if (att[0] <= (unsigned int)attack_window)
      goto b56f0_loop_next;
    if (!(best_score < *(float *)(att + 1)))
      goto b56f0_loop_next;
    best_score = *(float *)(att + 1);
    best_idx = i;
  b56f0_loop_next:
    i++;
    att += 4;
  }

  is_team_kill = 0;
  if (best_idx == -1) {
    best_idx = alt_best;
    if (best_idx == -1) {
      best_score = 0.0f;
      goto b56f0_check_kill;
    }
  }
  param_2 = *(int *)((int)(short)best_idx * 0x10 + (int)unit_obj + 0x3ec);
  best_score = *(float *)((int)(short)best_idx * 0x10 + (int)unit_obj + 0x3e4) *
               *(float *)0x253524;
b56f0_check_kill:
  if ((unsigned int)param_2 == 0xffffffffU)
    goto b56f0_assist_loop;
  killer_entry = (char *)datum_get(*(void **)0x5aa6d4, param_2);
  if (game_allegiance_get_team_is_friendly(my_team,
                                           *(short *)(killer_entry + 0x20))) {
    *(short *)(killer_entry + 0x98) += 1;
    *(short *)(killer_entry + 0x92) += 1;
    if ((int)*(short *)(killer_entry + 0x96) >= kill_time) {
      *(short *)(killer_entry + 0x94) += 1;
      *(short *)(killer_entry + 0x96) = (short)game_time_get();
    } else {
      *(short *)(killer_entry + 0x94) = 1;
      *(short *)(killer_entry + 0x96) = (short)game_time_get();
    }
  } else {
    *(short *)(killer_entry + 0xa8) += 1;
    is_team_kill = 1;
  }
b56f0_assist_loop:
  if (*(float *)0x2533c0 < best_score) {
    j = 0;
    att2 = (unsigned int *)(unit_obj + 0x3ec);
    while (j < 4) {
      if (j != alt_best) {
        if (!(best_score < *(float *)(att2 - 2)))
          goto b56f0_assist_next;
      }
      uVar2 = *att2;
      if (uVar2 == 0xffffffffU)
        goto b56f0_assist_next;
      if (uVar2 == (unsigned int)param_2)
        goto b56f0_assist_next;
      assist_entry = (char *)datum_get(*(void **)0x5aa6d4, (int)uVar2);
      if (!game_allegiance_get_team_is_friendly(
            my_team, *(short *)(assist_entry + 0x20)))
        goto b56f0_assist_next;
      *(short *)(assist_entry + 0xa0) += 1;
    b56f0_assist_next:
      j++;
      att2 += 4;
    }
  }
  game_engine_player_killed(param_2, param_3, (int)player_index, (int)is_team_kill);
}
