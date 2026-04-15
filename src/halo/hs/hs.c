/* HaloScript (hs) subsystem — scripting engine init/dispose/update. */

/* Dispose: clean up runtime state. */
void hs_dispose(void)
{
  /* calls hs_runtime_dispose_from_old_map */
  ((void (*)(void))0xca800)();
}

/* Per-tick script update with optional profiling. */
void hs_update(void)
{
  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x2f1c18 != 0)
    profile_enter_private((void *)0x2f1c10);

  ((void (*)(void))0xcde00)();

  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x2f1c18 != 0)
    profile_exit_private((void *)0x2f1c10);
}

/* Initialize hs for a new map: set up the script environment from the
 * scenario's script data if present. */
void hs_initialize_for_new_map(void)
{
  char *scenario_tag;

  if (*(int *)0x326a08 == -1)
    scenario_tag = 0;
  else
    scenario_tag = (char *)((void *(*)(void))0x18e380)();

  ((void (*)(void))0xc3b60)();

  if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) != 0)
    ((void (*)(int))0xc4c10)(0);

  ((void (*)(void))0xce1c0)();
  ((void (*)(void))0xcdb30)();
}

/* Dispose from old map: clean up script threads and runtime data. */
void hs_dispose_from_old_map(void)
{
  if (*(void **)0x5aa6c8 != 0) {
    ((void (*)(void))0xc3ca0)();
    if (*(uint8_t *)0x46b6d9 != 0) {
      ((void (*)(void *))0x119550)(*(void **)0x5aa6c8);
      ((void (*)(void *))0x119520)(*(void **)0x5aa6c8);
      *(uint8_t *)0x46b6d9 = 0;
    }
    *(void **)0x5aa6c8 = 0;
  }
  ((void (*)(void))0xca800)();
  ((void (*)(void))0xce1e0)();
}

/* Initialize the scripting engine: validate type names, set up
 * runtime tables, and initialize for the current map. */
void hs_initialize(void)
{
  char *scenario_tag;

  if (*(void **)0x2f1568 == 0) {
    display_assert("you can't add an hs type without defining its name.",
                   "c:\\halo\\SOURCE\\hs\\hs.c", 0xf5, 1);
    system_exit(-1);
  }

  ((void (*)(void))0xce150)();
  ((void (*)(void))0xca700)();

  if (*(int *)0x326a08 == -1)
    scenario_tag = 0;
  else
    scenario_tag = (char *)((void *(*)(void))0x18e380)();

  ((void (*)(void))0xc3b60)();

  if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) != 0)
    ((void (*)(int))0xc4c10)(0);

  ((void (*)(void))0xce1c0)();
  ((void (*)(void))0xcdb30)();
}
