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

/* Read one entry from D3D_g_DeferredTextureState[stage][sub_index] into *result.
 * The array is a flat DWORD array at 0x1fb498 with 32 entries per stage row.
 * Register args: EAX=stage, ECX=sub_index; stack arg: result pointer. */
void FUN_00155110(int stage /* @<eax> */, int sub_index /* @<ecx> */, unsigned int *result)
{
  *result = ((unsigned int *)0x1fb498)[stage * 32 + sub_index];
}
