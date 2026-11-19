#define RUNTIME_DECAL_YAW_SCALE   0.02473695f
#define RUNTIME_DECAL_PITCH_SCALE 0.012368475f

void structure_decals_update(uint32_t *old_cluster_visibility,
                  uint32_t *new_cluster_visibility, int16_t cluster_count)
{
  structure_bsp_t *scenario = (structure_bsp_t *)scenario_get();
  tag_block *structure_runtime_decals;
  tag_block *clusters;
  int16_t cluster_index;

  if (structure_decals_globals == NULL) {
    display_assert("structure_decals_globals",
                   "c:\\halo\\SOURCE\\structures\\structure_runtime_decals.c",
                   0x62, true);
    system_exit(-1);
  }

  structure_runtime_decals = &scenario->runtime_decals;
  if (structure_runtime_decals->count == 0 || cluster_count < 1) {
    structure_decals_globals->field_00 = 0;
    return;
  }

  clusters = &scenario->clusters;
  for (cluster_index = 0; cluster_index < cluster_count; ++cluster_index) {
    structure_bsp_cluster_t *cluster =
      (structure_bsp_cluster_t *)tag_block_get_element(clusters, cluster_index, sizeof(structure_bsp_cluster_t));
    bool should_render_cluster_decals;
    volatile bool should_delete_cluster_decals;
    if (cluster->first_decal_index == -1 || cluster->decal_count == 0) {
      should_delete_cluster_decals = false;
      should_render_cluster_decals = false;
    } else {
      should_delete_cluster_decals = false;
      should_render_cluster_decals = true;

      if (structure_decals_globals->field_00 == 0 &&
          (old_cluster_visibility[cluster_index >> 5] &
           (1u << (cluster_index & 0x1f))) != 0 &&
          (new_cluster_visibility[cluster_index >> 5] &
           (1u << (cluster_index & 0x1f))) == 0) {
        should_delete_cluster_decals = true;
      }

      if (((old_cluster_visibility[cluster_index >> 5] &
            (1u << (cluster_index & 0x1f))) != 0 &&
           structure_decals_globals->field_00 == 0) ||
          (new_cluster_visibility[cluster_index >> 5] &
           (1u << (cluster_index & 0x1f))) == 0) {
        should_render_cluster_decals = false;
      }
    }

    if (should_delete_cluster_decals) {
      decals_delete_permanent_from_cluster(cluster_index);
      continue;
    }

    if (should_render_cluster_decals) {
      int runtime_decal_offset;
      for (runtime_decal_offset = 0; runtime_decal_offset < cluster->decal_count;
           ++runtime_decal_offset) {
        structure_runtime_decal_t *runtime_decal =
          (structure_runtime_decal_t *)tag_block_get_element(
            structure_runtime_decals, cluster->first_decal_index + runtime_decal_offset,
            sizeof(structure_runtime_decal_t));
        tag_reference *palette_entry = (tag_reference *)tag_block_get_element(
          &((scenario_structure_bsps_t *)global_scenario_get())->decal_palette,
          runtime_decal->palette_index, sizeof(tag_reference));
        int decal_tag_index = palette_entry->tag_index;
        float decal_angles[2];
        float decal_vector[3];

        tag_get(TAG_GROUP_DECAL, decal_tag_index);

        decal_angles[0] =
          (float)(int)runtime_decal->yaw * RUNTIME_DECAL_YAW_SCALE;
        decal_angles[1] =
          (float)(int)runtime_decal->pitch * RUNTIME_DECAL_PITCH_SCALE;
        angles_to_vector((real_vector3d *)decal_vector, (real_euler_angles2d *)decal_angles);

        decal_new(decal_tag_index, runtime_decal, decal_vector, 1.0f, 1, -1,
                     0);
      }
    }
  }

  structure_decals_globals->field_00 = 0;
}
