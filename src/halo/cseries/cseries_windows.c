void system_exit(int code)
{
  error(2, "system_exit(%d) — spinning instead of halt_and_catch_fire", code);
  for (;;) {
    /* Spin forever so debug.txt is preserved and XBDM stays reachable.
     * halt_and_catch_fire at 0x1029a0 crashes on null D3D device vtable
     * when called mid-frame, killing the main thread silently. */
  }
  __builtin_unreachable();
}

uint32_t system_milliseconds(void)
{
  return ((uint32_t(*)(void))0x1d0581)();
}
