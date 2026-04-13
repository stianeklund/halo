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
