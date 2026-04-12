void game_allegiance_initialize(void)
{
  game_allegiance_globals =
    (char *)game_state_malloc("game allegiance globals", 0, 0xb4);
  csmemset(game_allegiance_globals, 0, 0xb4);
}

void game_allegiance_dispose(void)
{
}

void game_allegiance_initialize_for_new_map(void)
{
  int bit;
  int i;

  assert_halt(game_allegiance_globals);

  *(int16_t *)game_allegiance_globals = 0;
  csmemset(game_allegiance_globals + 0x94, 0, 0x10);
  csmemset(game_allegiance_globals + 0xa4, 0, 0x10);

  bit = 0;
  for (i = 0; i < 10; i++) {
    *(uint32_t *)(game_allegiance_globals + 0xa4 + (bit >> 5) * 4) |=
      1 << (bit & 0x1f);
    bit += 11;
  }
}

void game_allegiance_dispose_from_old_map(void)
{
}
