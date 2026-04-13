void breakable_surfaces_initialize(void)
{
  assert_halt(!breakable_surface_globals);
  breakable_surface_globals =
    (char *)game_state_malloc("breakable surface globals", 0, 0x4204);
}

void breakable_surfaces_dispose(void)
{
}

void breakable_surfaces_initialize_for_new_map(void)
{
  int i;
  int j;
  uint32_t *floats;

  assert_halt(breakable_surface_globals);

  *breakable_surface_globals = 1;
  for (i = 0; i < 16; i++) {
    csmemset(breakable_surface_globals + 1 + i * 0x20, 0xFF, 0x20);
    floats = (uint32_t *)(breakable_surface_globals + 0x204 + i * 0x400);
    for (j = 0; j < 0x100; j++)
      floats[j] = 0x3f800000;
  }
}

void breakable_surfaces_dispose_from_old_map(void)
{
}
