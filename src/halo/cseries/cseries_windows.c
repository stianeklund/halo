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

/* Returns the current time in seconds since the epoch (Unix time).
 * Wraps the CRT time() function with a NULL argument. */
uint32_t system_seconds(void)
{
  return crt_time(NULL);
}

/* Queries Xbox global memory status and returns available and total
 * physical memory in a two-element uint32_t array.
 * output[0] = available physical memory (bytes)
 * output[1] = total physical memory (bytes) */
void FUN_0008e480(uint32_t *output)
{
  uint32_t status[8]; /* MEMORYSTATUS is 0x20 bytes = 8 DWORDs */

  csmemset(status, 0, 0x20);
  status[0] = 0x20; /* dwLength */
  xbox_query_global_memory_status(status);
  csmemset(output, 0, 8);
  output[0] = status[3]; /* dwAvailPhys (offset 0x0C) */
  output[1] = status[2]; /* dwTotalPhys (offset 0x08) */
}
