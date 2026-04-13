void decals_initialize(void)
{
  global_decal_data = game_state_data_new("decals", 0x800, 0x38);
  assert_halt(global_decal_data);
  global_decal_data->identifier_zero_invalid = 1;
  decal_globals = (char *)game_state_malloc("decal globals", 0, 0x280c);
  assert_halt(decal_globals);
  rasterizer_decals_initialize();
  decal_counts_0 = 0;
  decal_counts_1 = 0;
}

void decals_initialize_for_new_map(void)
{
  assert_halt(global_decal_data);
  assert_halt(decal_globals);
  csmemset(decal_globals, 0xFF, 0x2800);
  *(_DWORD *)(decal_globals + 0x2800) = -1;
  *(_DWORD *)(decal_globals + 0x2804) = 0;
  *(_DWORD *)(decal_globals + 0x2808) = 0;
  data_delete_all(global_decal_data);
  rasterizer_decals_initialize_for_new_map();
  decal_counts_0 = 0;
  decal_counts_1 = 0;
}

void decals_dispose_from_old_map(void)
{
  assert_halt(global_decal_data);
  assert_halt(decal_globals);
  rasterizer_decals_dispose_from_old_map();
  data_make_invalid(global_decal_data);
}

void decals_dispose(void)
{
  global_decal_data = 0;
  rasterizer_decals_dispose();
}
