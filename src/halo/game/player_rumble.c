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
