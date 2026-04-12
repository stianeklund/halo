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
