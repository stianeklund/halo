void effects_initialize(void)
{
  effect_data = game_state_data_new("effect", 0x100, 0xfc);
  effect_location_data = game_state_data_new("effect location", 0x200, 0x3c);
  if (!effect_data || !effect_location_data)
    error(0, "couldn't allocate effect globals");
}

void effects_initialize_for_new_map(void)
{
  data_delete_all(effect_data);
  data_delete_all(effect_location_data);
}

void effects_dispose_from_old_map(void)
{
  data_make_invalid(effect_data);
  data_make_invalid(effect_location_data);
}

void effects_dispose(void)
{
  if (effect_data)
    effect_data = 0;
  if (effect_location_data)
    effect_location_data = 0;
}

bool effects_update(float elapsed)
{
  int effect_index;

  if (profile_global_enable && *(bool *)0x2eebf0)
    profile_enter_private((void *)0x2eebe8);

  for (effect_index = data_next_index(effect_data, NONE); effect_index != NONE;
       effect_index = data_next_index(effect_data, effect_index)) {
    effect_update(effect_index, elapsed);
  }

  if (profile_global_enable && *(bool *)0x2eebf0)
    profile_exit_private((void *)0x2eebe8);

  return false;
}
