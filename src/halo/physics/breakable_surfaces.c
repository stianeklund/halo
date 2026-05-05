void breakable_surfaces_initialize(void)
{
  assert_halt(!breakable_surface_globals);
  breakable_surface_globals =
    (char *)game_state_malloc("breakable surface globals", 0, 0x4204);
}

void breakable_surfaces_dispose(void)
{
}

void breakable_surfaces_initialize_for_new_map(void)
{
  int i;
  int j;
  uint32_t *floats;

  assert_halt(breakable_surface_globals);

  *breakable_surface_globals = 1;
  for (i = 0; i < 16; i++) {
    csmemset(breakable_surface_globals + 1 + i * 0x20, 0xFF, 0x20);
    floats = (uint32_t *)(breakable_surface_globals + 0x204 + i * 0x400);
    for (j = 0; j < 0x100; j++)
      floats[j] = 0x3f800000;
  }
}

void breakable_surfaces_dispose_from_old_map(void)
{
}

/* 0x1459e0 — breakable_surfaces_get_bsp_surface_data
 *
 * Returns a pointer to the 32-byte bitfield array for the current structure
 * BSP within the breakable_surface_globals buffer. The layout of the buffer
 * is: byte[0] = flags, then 16 × 32-byte entries (one per BSP) starting at
 * offset 1.
 *
 * Asserts that breakable_surface_globals is initialised and that
 * global_structure_bsp_index is in [0, MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO).
 *
 * Confirmed: MOV EAX,[0x46f08c] TEST/JNZ assert pattern at 0x1459e0.
 * Confirmed: MOV AX,[0x326a0c] — int16_t global_structure_bsp_index.
 * Confirmed: assert "global_structure_bsp_index>=0 &&
 *   global_structure_bsp_index<MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO" at line
 *   0x8b (139), __FILE__ "c:\halo\SOURCE\physics\breakable_surfaces.c".
 * Confirmed: LEA EAX,[EAX + ECX*1 + 0x1] — ptr = globals + bsp_index*32 + 1.
 */
char *breakable_surfaces_get_bsp_surface_data(void)
{
  assert_halt(breakable_surface_globals);
  assert_halt(global_structure_bsp_index >= 0 &&
              global_structure_bsp_index < 16);
  return breakable_surface_globals + 1 + (int)global_structure_bsp_index * 32;
}
