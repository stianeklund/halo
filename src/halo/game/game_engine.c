void game_engine_dispose(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[2])
      vtable[2]();
    current_game_engine = 0;
  }
}

void game_engine_dispose_from_old_map(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[4])
      vtable[4]();
  }
}

bool game_engine_force_single_screen(void)
{
  return current_game_engine && game_engine_variant_index > 1 &&
         game_engine_variant_index < 4;
}
