void cinematic_initialize(void)
{
  cinematic_globals = (cinematic_globals_t *)game_state_malloc(
    "cinematic globals", 0, sizeof(cinematic_globals_t));
  assert_halt(cinematic_globals);
}

void cinematic_dispose(void)
{
}

void cinematic_initialize_for_new_map(void)
{
  csmemset(cinematic_globals, 0, sizeof(cinematic_globals_t));
  csmemset(cinematic_globals->unk_12, 0xFF, sizeof(cinematic_globals->unk_12));
}

void cinematic_dispose_from_old_map(void)
{
  cinematic_globals->unk_8 = false;
  cinematic_globals->in_progress = false;
}

bool cinematic_can_be_skipped(void)
{
  return cinematic_globals->can_be_skipped;
}

bool cinematic_in_progress(void)
{
  return cinematic_globals->in_progress;
}
