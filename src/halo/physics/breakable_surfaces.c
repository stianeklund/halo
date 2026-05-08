/* 0x1457f0 — Returns a pointer to the health float for a breakable surface,
 * indexed by global_structure_bsp_index and surface_index within the globals
 * buffer starting at offset 0x204. */
float *FUN_001457f0(short surface_index)
{
  assert_halt(breakable_surface_globals);
  assert_halt(global_structure_bsp_index >= 0 &&
              global_structure_bsp_index < 16);
  assert_halt(surface_index >= 0 && surface_index < 256);
  return (
    float *)(breakable_surface_globals + 0x204 +
             ((int)global_structure_bsp_index * 256 + (int)surface_index) * 4);
}

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

/* 0x146a90 — Apply breakable-surface damage: reduces surface health based on
 * the damage effect tag's range/modifier and material vitality. Clears the
 * surface bit and fires the destruction callback if health reaches zero. */
void FUN_00146a90(int surface_id, void *damage_params, int unknown)
{
  float *health_ptr;
  char *material_data;
  char *jpt_tag;
  float damage;
  float new_health;
  char *bsp_data;
  int surface_idx;
  uint32_t *word_ptr;
  int material_type;

  assert_halt(breakable_surface_globals);
  if (*breakable_surface_globals == 0)
    return;
  if ((short)surface_id == -1)
    return;
  if (*(int *)damage_params == -1)
    return;
  if (*(short *)((char *)damage_params + 0x4c) == -1)
    return;

  health_ptr = FUN_001457f0((short)surface_id);
  if (*health_ptr <= 0.0f)
    return;

  material_type = (int)*(short *)((char *)damage_params + 0x4c);
  material_data = (char *)FUN_0018e500((short)material_type);
  if (material_data == NULL)
    return;
  if (*(float *)(material_data + 0x2d4) <= 0.0f)
    return;

  jpt_tag = (char *)tag_get(0x6a707421, *(int *)damage_params);

  damage =
    FUN_000121e0(*(float *)(jpt_tag + 0x1d4), *(float *)(jpt_tag + 0x1d8));
  damage = (damage - *(float *)(jpt_tag + 0x1d0)) *
             *(float *)((char *)damage_params + 0x40) +
           *(float *)(jpt_tag + 0x1d0);
  damage = damage * *(float *)(jpt_tag + 0x200 + material_type * 4) /
           *(float *)(material_data + 0x2d4);
  new_health = *health_ptr - damage;
  *health_ptr = new_health;

  if (new_health <= 0.0f) {
    bsp_data = breakable_surfaces_get_bsp_surface_data();
    surface_idx = (int)(short)surface_id;
    word_ptr = (uint32_t *)(bsp_data + (surface_idx >> 5) * 4);
    *word_ptr = *word_ptr & ~(1 << (surface_idx & 0x1f));
    FUN_00145ad0((unsigned short)surface_id, damage_params, unknown);
  }
}
