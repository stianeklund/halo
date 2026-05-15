void FUN_001963c0(uint32_t *old_cluster_visibility,
                  uint32_t *new_cluster_visibility, int16_t cluster_count)
{
  char *scenario = scenario_get();
  uint8_t *runtime_decal_globals = *(uint8_t **)0x4d8ec8;
  char *structure_runtime_decals = scenario + 0x258;
  char *clusters = scenario + 0x134;

  if (runtime_decal_globals == NULL) {
    display_assert("structure_decals_globals",
                   "c:\\halo\\SOURCE\\structures\\structure_runtime_decals.c",
                   0x62, true);
    system_exit(-1);
  }

  if (*(int *)structure_runtime_decals == 0 || cluster_count < 1) {
    *runtime_decal_globals = 0;
    return;
  }

  for (int cluster_index = 0; cluster_index < cluster_count; ++cluster_index) {
    bool should_render_cluster_decals;
    bool should_delete_cluster_decals;
    uint32_t cluster_bit;
    int cluster_word;
    char *cluster = tag_block_get_element(clusters, cluster_index, 0x68);
    int16_t first_runtime_decal = *(int16_t *)(cluster + 0xc);
    uint16_t runtime_decal_count = *(uint16_t *)(cluster + 0xe);

    if (first_runtime_decal == -1 || runtime_decal_count == 0) {
      should_delete_cluster_decals = false;
      should_render_cluster_decals = false;
    } else {
      should_render_cluster_decals = true;
      should_delete_cluster_decals = false;

      cluster_bit = 1u << (cluster_index & 0x1f);
      cluster_word = (cluster_index >> 5);

      if (*runtime_decal_globals == 0 &&
          (old_cluster_visibility[cluster_word] & cluster_bit) != 0 &&
          (new_cluster_visibility[cluster_word] & cluster_bit) == 0) {
        should_delete_cluster_decals = true;
      }

      if (((old_cluster_visibility[cluster_word] & cluster_bit) != 0 &&
           *runtime_decal_globals == 0) ||
          (new_cluster_visibility[cluster_word] & cluster_bit) == 0) {
        should_render_cluster_decals = false;
      }
    }

    if (should_delete_cluster_decals) {
      decals_delete_permanent_from_cluster(cluster_index);
      continue;
    }

    if (should_render_cluster_decals) {
      for (int runtime_decal_offset = 0;
           runtime_decal_offset < runtime_decal_count; ++runtime_decal_offset) {
        char *runtime_decal = tag_block_get_element(
          structure_runtime_decals, first_runtime_decal + runtime_decal_offset,
          0x10);
        char *global_scenario = (char *)global_scenario_get();
        char *structure_bsp = tag_block_get_element(
          global_scenario + 0x3b4, (uint8_t)runtime_decal[0xc], 0x10);
        int decal_tag_index = *(int *)(structure_bsp + 0xc);
        float decal_angles[2];
        float decal_vector[3];

        tag_get(0x64656361, decal_tag_index);

        decal_angles[0] =
          (float)(int)(int8_t)runtime_decal[0xe] * *(const float *)0x2b1958;
        decal_angles[1] =
          (float)(int)(int8_t)runtime_decal[0xf] * *(const float *)0x2b1954;
        angles_to_vector(decal_vector, decal_angles);

        FUN_0009c4b0(decal_tag_index, runtime_decal, decal_vector, 1.0f, 1, -1,
                     0);
      }
    }
  }

  *runtime_decal_globals = 0;
}
