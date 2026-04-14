/* Rasterizer common init/dispose and per-frame update. */

/* Initialize rasterizer data for a new map. Loads the rasterizer data
 * tag block from game globals and initializes rendering subsystems. */
void rasterizer_initialize_for_new_map(void)
{
  char *globals = (char *)game_globals_get();
  assert_halt(globals);

  if (*(int *)(globals + 0x134) == 0) {
    *(int *)0x476204 = 0;
  } else {
    *(int *)0x476204 =
      ((int (*)(void *, int, int))0x19b210)(globals + 0x134, 0, 0x1ac);
  }
  assert_halt(*(int *)0x476204);

  ((void (*)(void))0x181150)();
  ((void (*)(void))0x1836f0)();
  ((void (*)(void))0x17d950)();

  if (*(void **)0x47e4d0 != 0)
    csmemset(*(void **)0x47e4d0, 0, 0x10);

  ((void (*)(int))0x17dec0)(0);
}

/* Dispose rasterizer state from old map. */
void rasterizer_dispose_from_old_map(void)
{
  ((void (*)(void))0x17d980)();
  ((void (*)(void))0x1836f0)();
  *(int *)0x476204 = 0;
}

/* Per-frame rasterizer update — stores the delta time. */
void rasterizer_frame_update(float delta_time)
{
  *(float *)0x5a5e1c = delta_time;
}
