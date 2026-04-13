void scenario_initialize(void)
{
  *(void **)0x5064d0 = game_state_malloc("scenario globals", 0, 0x100);
}

void scenario_dispose_from_old_map(void)
{
  *(char *)0x5057c0 = 0;
}

void scenario_unload(void)
{
  assert_halt(!((bool (*)())0x1c5940)());
  ((void (*)())0x1b9890)();
  *(int *)0x326a08 = NONE;
  *(int16_t *)0x326a0c = NONE;
  *(int16_t *)(*(char **)0x5064d0) = NONE;
  *(int *)0x5064e4 = 0;
  *(int *)0x5064e0 = 0;
  *(int *)0x5064dc = 0;
  *(int *)0x5064d8 = 0;
  *(int *)0x5064d4 = 0;
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
