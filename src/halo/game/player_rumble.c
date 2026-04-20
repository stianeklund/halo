void rumble_initialize(void)
{
  rumble_globals = (char *)game_state_malloc("rumble", 0, 0x82c);
}

void rumble_dispose(void)
{
}

void rumble_initialize_for_new_map(void)
{
  csmemset(rumble_globals, 0, 0x82c);
}

void rumble_clear_for_local_player(int16_t local_player_index)
{
  csmemset(rumble_globals + local_player_index * 0x208, 0, 0x208);
}

/* rumble_clear_all_players (0xb9d60)
 *
 * Clears the entire rumble globals block (all 4 player slots) and then
 * calls input_set_rumble(i, 0, 0) for each gamepad that is present.
 * Unlike rumble_dispose_from_old_map, only checks input_has_gamepad — no
 * double pass. */
void rumble_clear_all_players(void)
{
  int i;

  csmemset(rumble_globals, 0, 0x82c);
  for (i = 0; i < 4; i++) {
    if (input_has_gamepad(i))
      input_set_rumble(i, 0, 0);
  }
}

void rumble_dispose_from_old_map(void)
{
  int i;

  for (i = 0; (int16_t)i < 4; i++)
    input_set_rumble(i, 0, 0);

  csmemset(rumble_globals, 0, 0x82c);

  for (i = 0; i < 4; i++) {
    if (input_has_gamepad(i))
      input_set_rumble(i, 0, 0);
  }
}

uint32_t rumble_calculate(char *slot)
{
  float motors[2];
  float *effect_ptr;
  float *timer_ptr;
  float *effect_start;
  float timer_val;
  float blend;
  float scaled;
  uint16_t left, right;
  int i, j;

  motors[0] = *(float *)(slot + 0x200);
  motors[1] = *(float *)(slot + 0x204);
  effect_ptr = (float *)(slot + 4);
  timer_ptr = (float *)(slot + 0x1e0);

  for (i = 0; i < 8; i++) {
    timer_val = *timer_ptr;
    effect_start = effect_ptr;
    for (j = 0; j < 2; j++) {
      if (timer_val < *effect_ptr) {
        blend = *(float *)0x2533c8 - timer_val / *effect_ptr;
        if (blend < *(float *)0x2533c0)
          blend = 0.0f;
        else if (blend > *(float *)0x2533c8)
          blend = 1.0f;
        motors[j] += ((float (*)(uint16_t, float))0x10a710)(
                       *(uint16_t *)((char *)effect_ptr + 4), blend) *
                     effect_ptr[-1];
      }
      effect_ptr += 5;
    }
    effect_ptr = effect_start + 15;
    timer_ptr++;
  }

  if (*(float *)(rumble_globals + 0x828) != *(float *)0x2533c0) {
    motors[0] +=
      *(float *)(rumble_globals + 0x820) * *(float *)(rumble_globals + 0x828);
    motors[1] +=
      *(float *)(rumble_globals + 0x824) * *(float *)(rumble_globals + 0x828);
  }

  scaled = motors[0] * *(float *)0x2647cc;
  if (scaled < *(float *)0x2533c0)
    scaled = 0.0f;
  else if (scaled > *(float *)0x2647cc)
    scaled = 65535.0f;
  left = (uint16_t)(int)scaled;

  scaled = motors[1] * *(float *)0x2647cc;
  if (scaled < *(float *)0x2533c0)
    scaled = 0.0f;
  else if (scaled > *(float *)0x2647cc)
    scaled = 65535.0f;
  right = (uint16_t)(int)scaled;

  return ((uint32_t)right << 16) | (uint32_t)left;
}

void rumble_update(void)
{
  int i;
  uint32_t rumble;
  int player_handle;
  char *player;
  int16_t local_player_index;
  int controller;
  char *slot;

  for (i = 0; (int16_t)i < 4; i++) {
    slot = rumble_globals + i * 0x208;
    rumble = rumble_calculate(slot);
    *(float *)(slot + 0x1e0) += *(float *)0x2546a4;
    *(float *)(slot + 0x1e4) += *(float *)0x2546a4;
    *(float *)(slot + 0x1e8) += *(float *)0x2546a4;
    *(float *)(slot + 0x1ec) += *(float *)0x2546a4;
    *(float *)(slot + 0x1f0) += *(float *)0x2546a4;
    *(float *)(slot + 0x1f4) += *(float *)0x2546a4;
    *(float *)(slot + 0x1f8) += *(float *)0x2546a4;
    *(float *)(slot + 0x1fc) += *(float *)0x2546a4;
    player_handle = local_player_get_player_index(i);
    if (player_handle == NONE) {
      input_set_rumble(i, 0, 0);
    } else {
      player = (char *)datum_get(player_data, player_handle);
      local_player_index = *(int16_t *)(player + 2);
      if (local_player_index != NONE) {
        controller = ((int (*)(int16_t))0xe0810)(local_player_index);
        if (((bool (*)(int))0xe0b00)(controller)) {
          input_set_rumble(local_player_index, 0, 0);
        } else {
          input_set_rumble(local_player_index, (uint16_t)rumble,
                           (uint16_t)(rumble >> 16));
        }
      }
    }
  }
}
