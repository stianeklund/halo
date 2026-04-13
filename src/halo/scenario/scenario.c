void scenario_initialize(void)
{
  *(void **)0x5064d0 = game_state_malloc("scenario globals", 0, 0x100);
}

void scenario_dispose_from_old_map(void)
{
  *(char *)0x5057c0 = 0;
}

scenario_t *global_scenario_get(void)
{
  assert_halt(*(void **)0x5064e4);
  return *(scenario_t **)0x5064e4;
}

void *scenario_get(void)
{
  assert_halt(global_structure_bsp);
  return global_structure_bsp;
}
