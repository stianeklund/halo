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
