void scenario_initialize(void)
{
  *(void **)0x5064d0 = game_state_malloc("scenario globals", 0, 0x100);
}

/* Initialize scenario globals for a new map: clear game globals data,
 * copy default gravity/speed from constant table, reset a flag. */
void scenario_initialize_for_new_map(void)
{
  char *globals;

  ((void (*)(void))0x190500)();
  globals = *(char **)0x5064d0;
  csmemset(globals + 4, 0, 0xb0);
  qmemcpy(globals + 0xb8, (void *)0x2c1220, 0x48);
  *(uint8_t *)(globals + 0xb4) = 0;
}

void scenario_dispose_from_old_map(void)
{
  *(char *)0x5057c0 = 0;
}

/* scenario_frame_update is a direct trampoline to the wind update at
 * 0x18ffe0. The original binary has a single JMP instruction here. */
void scenario_frame_update(float delta_time)
{
  ((void (*)(float))0x18ffe0)(delta_time);
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

/* Return the game globals tag data pointer. Asserts if not loaded. */
void *game_globals_get(void)
{
  if (!*(void **)0x5064d4) {
    display_assert("global_game_globals",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xdd, 1);
    system_exit(-1);
  }
  return *(void **)0x5064d4;
}

/* Switch the active structure BSP. Calls dispose callbacks on the old BSP,
 * loads the new one from the scenario tag, and calls initialize callbacks.
 * Returns false if the BSP index is invalid or the BSP fails to load. */
bool scenario_switch_structure_bsp(__int16 bsp_index)
{
  char *scenario_tag;
  char *bsp_ref;
  bool had_old_bsp;
  bool result = 0;

  if (bsp_index == *(int16_t *)0x326a0c)
    return result;
  if (bsp_index < 0)
    return result;

  scenario_tag = *(char **)0x5064e4;
  if ((int)bsp_index >= *(int *)(scenario_tag + 0x5a4))
    return result;

  bsp_ref = (char *)((int (*)(void *, int, int))0x19b210)(scenario_tag + 0x5a4,
                                                          (int)bsp_index, 0x20);

  assert_halt(*(char **)0x5064e4);

  /* stop rendering during BSP switch */
  ((void (*)(void))0x101c90)();
  ((void (*)(int))0x14d070)(0);

  had_old_bsp = 0;
  if (*(int16_t *)0x326a0c != -1) {
    /* call 10 dispose-from-old-map callbacks */
    int *dispose_table = (int *)0x326a44;
    int i;
    for (i = 0; i < 10; i++)
      ((void (*)(void))dispose_table[i])();

    /* unload old BSP tag */
    {
      char *old_bsp = (char *)((int (*)(void *, int, int))0x19b210)(
        *(char **)0x5064e4 + 0x5a4, (int)*(int16_t *)0x326a0c, 0x20);
      ((void (*)(void *))0x1ba0c0)(old_bsp);
    }

    *(int16_t *)(*(char **)0x5064d0) = -1;
    *(int16_t *)0x326a0c = -1;
    had_old_bsp = 1;
  }

  /* load new BSP */
  if (((bool (*)(void *))0x1b9fa0)(bsp_ref)) {
    char *bsp_tag = (char *)tag_get(0x73627370, *(int *)(bsp_ref + 0x1c));
    *(char **)0x5064e0 = bsp_tag;

    *(char **)0x5064dc =
      (char *)((int (*)(void *, int, int))0x19b210)(bsp_tag + 0xb0, 0, 0x60);
    *(char **)0x5064d8 =
      (char *)((int (*)(void *, int, int))0x19b210)(bsp_tag + 0xb0, 0, 0x60);

    *(int16_t *)(*(char **)0x5064d0) = bsp_index;
    *(int16_t *)0x326a0c = bsp_index;

    if (had_old_bsp) {
      /* call 13 initialize-for-new-map callbacks */
      int *init_table = (int *)0x326a10;
      int i;
      for (i = 0; i < 13; i++)
        ((void (*)(void))init_table[i])();
    }
    result = 1;
  } else {
    /* BSP load failed */
    error(0, "%s", *(char **)(bsp_ref + 0x14));
  }

  /* resume rendering */
  ((void (*)(int))0x14d070)(1);
  ((void (*)(void))0x101ca0)();

  return result;
}

/* Load a scenario from the map cache. Opens the cache file, loads the
 * scenario and game globals tags, and switches to BSP 0. */
bool scenario_load(const char *map_name)
{
  int tag_index;
  char *scenario_tag;
  bool result = 0;

  ((void (*)(void *, const char *))0x8e770)((void *)0x326a6c, "cache\\");
  tag_index = ((int (*)(const char *))0x1b9e70)(map_name);
  *(int *)0x326a08 = tag_index;

  if (tag_index == -1) {
    /* map not found — print error with map path line by line */
    char *path = (char *)0x25386f;
    char *nl;
    error(1, "couldn't open map file");
    do {
      nl = ((char *(*)(const char *, int))0x1d95d0)(path, '\n');
      if (nl)
        *nl = 0;
      error(1, "%s", path);
      if (!nl)
        break;
      *nl = '\n';
      path = nl + 1;
    } while (path);
    return result;
  }

  scenario_tag = (char *)tag_get(0x73636e72, tag_index);
  *(char **)0x5064e4 = scenario_tag;

  if (*(int *)(scenario_tag + 0x5a4) < 1) {
    error(1, "scenario has no structure bsps");
    return result;
  }

  /* load game globals tag ("matg") */
  {
    int matg_index =
      ((int (*)(int, const char *))0x1b9930)(0x6d617467, "globals\\globals");
    *(char **)0x5064d4 = (char *)tag_get(0x6d617467, matg_index);
  }

  if (scenario_switch_structure_bsp(0))
    return 1;

  return result;
}
