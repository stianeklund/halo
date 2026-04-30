/* Find the D3D flipcount by probing internal D3D state (0x1cf880).
 * Validates the D3D context: interrupts enabled, vblank callback matches,
 * and vblank count is sane. Returns pointer to the flipcount variable. */
int *d3d_find_flipcount(void)
{
  if (*(int *)0x1fdebc != 1) {
    error(2,
          "### WARNING: direct3d context unreadable "
          "(interrupts 0x%08X != 0x00000001)...",
          *(unsigned int *)0x1fdebc);
  } else {
    if (*(int *)0x1fdffc != (int)0x101cd0) {
      error(2,
            "### WARNING: direct3d context unreadable "
            "(callback 0x%08X != 0x%08X)...",
            *(int *)0x1fdffc, 0x101cd0);
    } else {
      if (*(unsigned int *)0x1fe634 <= 0x10000)
        return (int *)0x1fe028;
      error(2,
            "### WARNING: direct3d context unreadable "
            "(vblank 0x%08X greater than 0x00010000)...",
            *(unsigned int *)0x1fe634);
    }
  }
  display_assert(
    "### FATAL ERROR LOCATING DIRECT3D FLIPCOUNT, THIS IS HORRIBLY BAD",
    "c:\\halo\\SOURCE\\main\\d3d_intimacy.cpp", 0x35, 1);
  system_exit(-1);
  return NULL;
}
