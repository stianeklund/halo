void game_allegiance_initialize(void)
{
  game_allegiance_globals =
    (char *)game_state_malloc("game allegiance globals", 0, 0xb4);
  csmemset(game_allegiance_globals, 0, 0xb4);
}

void game_allegiance_dispose(void)
{
}

void game_allegiance_dispose_from_old_map(void)
{
}
