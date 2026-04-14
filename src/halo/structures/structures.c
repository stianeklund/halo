/* Structure BSP rendering subsystem init/dispose. */

void structures_initialize(void)
{
  ((void (*)(void))0x193970)();
  ((void (*)(void))0x196290)();
}

void structures_initialize_for_new_map(void)
{
  ((void (*)(void))0x193bb0)();
  ((void (*)(void))0x1962d0)();
}

void structures_dispose_from_old_map(void)
{
  ((void (*)(void))0x1963a0)();
}

void structures_dispose(void)
{
  ((void (*)(void))0x1963b0)();
}
