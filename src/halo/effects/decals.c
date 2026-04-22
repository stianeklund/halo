void FUN_00098b20(float *sprite_bounds, void *definition,
                  int16_t sequence_index, int16_t sprite_index, float extent,
                  float *out_extent)
{
  char *definition_data;
  char *bitmap_tag;
  char *sequence;
  int16_t *sprite;
  char *bitmap;
  float ratio;
  float x_scale;
  float y_scale;

  if (definition == NULL) {
    display_assert("definition", "c:\\halo\\SOURCE\\effects\\decals.c", 0x107,
                   true);
    system_exit(-1);
  }

  if (sprite_bounds == NULL) {
    display_assert("sprite_bounds", "c:\\halo\\SOURCE\\effects\\decals.c",
                   0x108, true);
    system_exit(-1);
  }

  if (out_extent == NULL) {
    display_assert("extent", "c:\\halo\\SOURCE\\effects\\decals.c", 0x109,
                   true);
    system_exit(-1);
  }

  definition_data = (char *)definition;

  bitmap_tag = (char *)tag_get(0x6269746d, *(int *)(definition_data + 0xe4));
  sequence =
    (char *)tag_block_get_element(bitmap_tag + 0x54, (int)sequence_index, 0x40);
  sprite =
    (int16_t *)tag_block_get_element(sequence + 0x34, (int)sprite_index, 0x20);
  bitmap =
    (char *)tag_block_get_element(bitmap_tag + 0x60, (int)sprite[0], 0x30);

  sprite_bounds[0] = *(float *)(sprite + 4);
  sprite_bounds[1] = *(float *)(sprite + 6);
  sprite_bounds[2] = *(float *)(sprite + 8);
  sprite_bounds[3] = *(float *)(sprite + 10);

  ratio = 1.0f;
  if ((definition_data[1] & 1) != 0) {
    ratio = ((float)(int)*(int16_t *)(bitmap + 6) /
             (float)(int)*(int16_t *)(bitmap + 4)) *
            ((*(float *)(sprite + 6) - *(float *)(sprite + 4)) /
             (*(float *)(sprite + 10) - *(float *)(sprite + 8)));
  }

  extent = extent / *(float *)(definition_data + 0xfc);
  x_scale = (float)(int)*(int16_t *)(bitmap + 4) * extent;
  y_scale = (float)(int)*(int16_t *)(bitmap + 6) * extent * ratio;

  out_extent[0] = -*(float *)(sprite + 0xc) * x_scale;
  out_extent[1] = ((*(float *)(sprite + 6) - *(float *)(sprite + 0xc)) -
                   *(float *)(sprite + 4)) *
                  x_scale;
  out_extent[2] = -*(float *)(sprite + 0xe) * y_scale;
  out_extent[3] = ((*(float *)(sprite + 10) - *(float *)(sprite + 0xe)) -
                   *(float *)(sprite + 8)) *
                  y_scale;
}

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

/* decals_update_for_new_map (0x98e70)
 *
 * Scans every live decal entry and clears transient-lifetime flags:
 *   bit 0 (0x1) = "locked"   — always cleared, decrements locked count
 *   bit 1 (0x2) = "permanent" — only cleared when full_reset is true,
 *                               decrements permanent count
 *
 * After the scan, if either count is non-zero an error is logged once
 * (per-static error-once flag at 0x4557de / 0x4557df) and the count is
 * reset to zero.  Finally, the two decal counters (decal_counts_0,
 * decal_counts_1) are zeroed.
 *
 * The bit fields are in the uint16_t at decal_entry+0x2; the counts
 * live in decal_globals+0x2804 (locked) and decal_globals+0x2808 (permanent).
 */
void decals_update_for_new_map(bool full_reset)
{
  data_iter_t iter;
  int16_t *entry;
  char *dg;

  assert_halt(global_decal_data);

  if (*(uint8_t *)((char *)global_decal_data + 0x24) != 0) {
    assert_halt(decal_globals);

    data_iterator_new(&iter, global_decal_data);
    entry = (int16_t *)data_iterator_next(&iter);
    while (entry != NULL) {
      dg = decal_globals;
      if (entry[1] & 1) {
        /* Clear locked flag and decrement locked count. */
        entry[1] = (int16_t)(entry[1] & ~1);
        *(int *)(dg + 0x2804) -= 1;
      }
      if (full_reset && (entry[1] & 2)) {
        /* Clear permanent flag and decrement permanent count. */
        entry[1] = (int16_t)(entry[1] & ~2);
        *(int *)(dg + 0x2808) -= 1;
      }
      entry = (int16_t *)data_iterator_next(&iter);
      dg = decal_globals;
    }

    dg = decal_globals;

    /* Sanity-check locked count. */
    if (*(int *)(dg + 0x2804) != 0) {
      if (*(uint8_t *)0x4557de == 0) {
        error(
          2, "### ERROR decals: locked count is invalid (#%d) -- tell Bernie!!",
          *(int *)(dg + 0x2804));
        *(uint8_t *)0x4557de = 1;
      }
      *(int *)(decal_globals + 0x2804) = 0;
    }

    /* Sanity-check permanent count (only meaningful on full reset). */
    if (full_reset && *(int *)(dg + 0x2808) != 0) {
      if (*(uint8_t *)0x4557df == 0) {
        error(
          2,
          "### ERROR decals: permanent count is invalid (#%d) -- tell Bernie!!",
          *(int *)(dg + 0x2808));
        *(uint8_t *)0x4557df = 1;
        dg = decal_globals;
      }
      *(int *)(dg + 0x2808) = 0;
    }
  }

  decal_counts_0 = 0;
  decal_counts_1 = 0;
}

uint32_t FUN_00099530(float alpha, float *color)
{
  uint32_t a;
  uint32_t r;
  uint32_t g;
  uint32_t b;

  if (alpha < 0.0f || alpha > 1.0f) {
    display_assert("alpha>=0.0f && alpha<=1.0f",
                   "..\\bitmaps\\bitmaps_inlines.h", 0xf3, true);
    system_exit(-1);
  }

  if (!(color[0] >= 0.0f && color[0] <= 1.0f && color[1] >= 0.0f &&
        color[1] <= 1.0f && color[2] >= 0.0f && color[2] <= 1.0f)) {
    error(2, "%s: assert_valid_real_rgb_color(%f, %f, %f)", "color",
          (double)color[0], (double)color[1], (double)color[2],
          "..\\bitmaps\\bitmaps_inlines.h", 0xf4, 1);
    system_exit(-1);
  }

  b = (uint32_t)((int)(color[2] * 255.0f + 0.5f)) & 0xff;
  g = (uint32_t)((int)(color[1] * 255.0f + 0.5f)) & 0xff;
  r = (uint32_t)((int)(color[0] * 255.0f + 0.5f)) & 0xff;
  a = (uint32_t)((int)(alpha * 255.0f + 0.5f));

  return b | (g << 8) | (r << 0x10) | (a << 0x18);
}

int FUN_000998b0(int16_t cluster_index, int16_t layer, int old_index,
                 bool randomize)
{
  int decal_index;
  int decal;
  int previous_index;
  int previous_decal;
  int existing_decal;
  uint32_t random_scaled;
  uint32_t random_value;
  uint32_t *local_seed;
  int unlock_passes;
  data_iter_t iter;
  char *candidate;

  decal_index = data_new_datum(global_decal_data, 0);
  unlock_passes = 0;

  if (cluster_index < 0 || cluster_index >= 0x200) {
    display_assert(
      "cluster_index>=0 && cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0x1df, true);
    system_exit(-1);
  }

  if (layer < 0 || layer >= 5) {
    display_assert("layer>=0 && layer<NUMBER_OF_DECAL_LAYERS",
                   "c:\\halo\\SOURCE\\effects\\decals.c", 0x1e0, true);
    system_exit(-1);
  }

  if (decal_index == -1) {
    error(2, "### ERROR failed to insert decal");
    return -1;
  }

  decal = (int)datum_get(global_decal_data, decal_index);

  if (!randomize) {
    local_seed = random_math_get_local_seed_address();
    random_value = *local_seed * 0x19660d + 0x3c6ef35f;
    *local_seed = random_value;
    random_scaled = (random_value >> 16) * 100;

    if (random_scaled < 0x9fff6) {
      *(int16_t *)(decal + 2) = 1;
      *(int *)(decal_globals + 0x2804) += 1;

      if (*(int *)(decal_globals + 0x2804) > 0x200) {
        data_iterator_new(&iter, global_decal_data);

        while (*(int *)(decal_globals + 0x2804) > 0x100) {
          candidate = (char *)data_iterator_next(&iter);
          if (candidate == NULL) {
            data_iterator_new(&iter, global_decal_data);
            unlock_passes += 1;
            if (unlock_passes >= 100) {
              error(2, "### ERROR decals: failed to unlock decals during "
                       "insert -- tell Bernie!!");
              return -1;
            }
          } else if ((*(uint8_t *)(candidate + 2) & 1) != 0) {
            local_seed = random_math_get_local_seed_address();
            random_value = *local_seed * 0x19660d + 0x3c6ef35f;
            *local_seed = random_value;
            random_scaled = (random_value >> 16) * 100;

            if (random_scaled < 0x28ffd7 || *(int16_t *)(candidate + 4) == -1) {
              *(uint8_t *)(candidate + 2) &= 0xfe;
              *(int *)(decal_globals + 0x2804) -= 1;
            }
          }
        }

        if (*(int *)(decal_globals + 0x2804) < 0 && *(uint8_t *)0x4557dd == 0) {
          error(
            2,
            "### ERROR decals: locked count is invalid (#%d) -- tell Bernie!!",
            *(int *)(decal_globals + 0x2804));
          *(uint8_t *)0x4557dd = 1;
        }
      }
    } else {
      *(int16_t *)(decal + 2) = 0;
    }
  } else {
    *(int16_t *)(decal + 2) = 2;
    *(int *)(decal_globals + 0x2808) += 1;
  }

  if (old_index != -1) {
    existing_decal = (int)datum_get(global_decal_data, old_index);
    if (*(int16_t *)(existing_decal + 4) != cluster_index) {
      display_assert("next->cluster_index==cluster_index",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x22c, true);
      system_exit(-1);
    }

    previous_index = *(int *)(existing_decal + 0x30);
    if (previous_index == -1) {
      *(int *)(decal_globals + ((int)layer * 0x200 + (int)cluster_index) * 4) =
        decal_index;
    } else {
      previous_decal = (int)datum_get(global_decal_data, previous_index);
      *(int *)(previous_decal + 0x34) = decal_index;
    }

    *(int *)(existing_decal + 0x30) = decal_index;
    *(int *)(decal + 0x30) = decal_index;
    *(int16_t *)(decal + 6) = layer;
    *(int *)(decal + 0x34) = old_index;
    *(int16_t *)(decal + 4) = cluster_index;
    return decal_index;
  }

  previous_index =
    *(int *)(decal_globals + ((int)layer * 0x200 + (int)cluster_index) * 4);

  *(int *)(decal + 0x30) = -1;
  *(int *)(decal + 0x34) = previous_index;
  *(int16_t *)(decal + 4) = cluster_index;
  *(int16_t *)(decal + 6) = layer;

  if (previous_index != -1) {
    previous_decal = (int)datum_get(global_decal_data, previous_index);
    *(int *)(previous_decal + 0x30) = decal_index;
  }

  *(int *)(decal_globals + ((int)layer * 0x200 + (int)cluster_index) * 4) =
    decal_index;

  return decal_index;
}

void FUN_00099fd0(int16_t cluster_index)
{
  if (cluster_index < 0 || cluster_index >= 0x200) {
    display_assert(
      "cluster_index>=0 && cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0x377, true);
    system_exit(-1);
  }

  if (*(uint8_t *)((char *)global_decal_data + 0x24) != 0) {
    if (decal_globals == NULL) {
      display_assert("decal_globals", "c:\\halo\\SOURCE\\effects\\decals.c",
                     0x37d, true);
      system_exit(-1);
    }

    for (int16_t layer = 0; layer < 5; ++layer) {
      int decal_index = -1;

      if (cluster_index == -1) {
        if (layer == 0) {
          decal_index = *(int *)(decal_globals + 0x2800);
        }
      } else {
        decal_index =
          *(int *)(decal_globals + (layer * 0x200 + (int)cluster_index) * 4);
      }

      while (decal_index != -1) {
        int current_decal_index = decal_index;
        char *decal = (char *)datum_get(global_decal_data, current_decal_index);
        decal_index = *(int *)(decal + 0x34);

        if (*(int16_t *)(decal + 4) != cluster_index) {
          display_assert("decal->cluster_index==cluster_index",
                         "c:\\halo\\SOURCE\\effects\\decals.c", 0x398, true);
          system_exit(-1);
        }

        if ((*(uint16_t *)(decal + 2) & 2) != 0) {
          *(uint16_t *)(decal + 2) &= (uint16_t)~2;
          *(int *)(decal_globals + 0x2808) -= 1;

          if ((*(uint8_t *)(decal + 2) & 1) != 0) {
            display_assert("!TEST_FLAG(decal->flags, _decal_locked_bit)",
                           "c:\\halo\\SOURCE\\effects\\decals.c", 0x39f, true);
            system_exit(-1);
          }

          FUN_0017cb10(current_decal_index);
        }
      }
    }

    if (*(int *)(decal_globals + 0x2808) < 0) {
      display_assert("decal_globals->permanent_count>=0",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x3a8, true);
      system_exit(-1);
    }
  }
}

void FUN_0009a300(float *bounds, float *projection, float *basis)
{
  int16_t plane_basis;
  uint8_t plane_axis;
  float projected[3];

  if (basis == NULL) {
    display_assert("basis", "c:\\halo\\SOURCE\\effects\\decals.c", 0x410, true);
    system_exit(-1);
  }

  if (projection == NULL) {
    display_assert("projection", "c:\\halo\\SOURCE\\effects\\decals.c", 0x411,
                   true);
    system_exit(-1);
  }

  for (int i = 0; i < 13; ++i) {
    projection[i] = basis[i];
  }

  projection[0xd] = bounds[0];
  projection[0xe] = bounds[1];
  projection[0xf] = bounds[2];
  projection[0x10] = bounds[3];

  projection[0x11] = basis[7];
  projection[0x12] = basis[8];
  projection[0x13] = basis[9];
  projection[0x14] = projection[0x11] * basis[10] +
                     projection[0x12] * basis[11] +
                     projection[0x13] * basis[12];

  if (fabsf(projection[0x13]) >= fabsf(projection[0x12]) &&
      fabsf(projection[0x13]) >= fabsf(projection[0x11])) {
    plane_basis = 2;
  } else {
    plane_basis = fabsf(projection[0x12]) >= fabsf(projection[0x11]) ? 1 : 0;
  }

  *(int16_t *)(projection + 0x15) = plane_basis;
  plane_axis = (uint8_t)FUN_00099270(projection + 0x11, plane_basis);
  *(uint8_t *)((char *)projection + 0x56) = plane_axis;

  projected[0] = bounds[0] * basis[1] + bounds[2] * basis[4] + basis[10];
  projected[1] = bounds[0] * basis[2] + bounds[2] * basis[5] + basis[11];
  projected[2] = bounds[0] * basis[3] + bounds[2] * basis[6] + basis[12];
  FUN_00061df0(projected, plane_basis, plane_axis, projection + 0x16);

  projected[0] = bounds[1] * basis[1] + bounds[2] * basis[4] + basis[10];
  projected[1] = bounds[1] * basis[2] + bounds[2] * basis[5] + basis[11];
  projected[2] = bounds[1] * basis[3] + bounds[2] * basis[6] + basis[12];
  FUN_00061df0(projected, plane_basis, plane_axis, projection + 0x18);

  projected[0] = bounds[1] * basis[1] + bounds[3] * basis[4] + basis[10];
  projected[1] = bounds[1] * basis[2] + bounds[3] * basis[5] + basis[11];
  projected[2] = bounds[1] * basis[3] + bounds[3] * basis[6] + basis[12];
  FUN_00061df0(projected, plane_basis, plane_axis, projection + 0x1a);

  projected[0] = bounds[0] * basis[1] + bounds[3] * basis[4] + basis[10];
  projected[1] = bounds[0] * basis[2] + bounds[3] * basis[5] + basis[11];
  projected[2] = bounds[0] * basis[3] + bounds[3] * basis[6] + basis[12];
  FUN_00061df0(projected, plane_basis, plane_axis, projection + 0x1c);

  projection[0x1e] = projection[0x18] - projection[0x16];
  projection[0x1f] = projection[0x19] - projection[0x17];
  projection[0x20] = projection[0x1c] - projection[0x16];
  projection[0x21] = projection[0x1d] - projection[0x17];
  projection[0x22] = *(float *)0x2533c8 / (projection[0x21] * projection[0x1e] -
                                           projection[0x1f] * projection[0x20]);
}

void FUN_0009c4b0(int decal_tag_index, void *origin, void *direction,
                  float scale, bool randomize, int16_t color_index, int flags)
{
  if (*(uint8_t *)0x2eebd0 != 0) {
    uint32_t *local_random_seed_address = random_math_get_local_seed_address();
    uint32_t local_seed = 0;
    int16_t collision_result[40];

    if (randomize) {
      if (local_random_seed_address == NULL) {
        display_assert("local_random_seed_address",
                       "c:\\halo\\SOURCE\\effects\\decals.c", 0x5fd, true);
        system_exit(-1);
      }

      if (origin == NULL) {
        display_assert("origin", "c:\\halo\\SOURCE\\effects\\decals.c", 0x5fe,
                       true);
        system_exit(-1);
      }

      local_seed = *local_random_seed_address;
      *local_random_seed_address = ((uint32_t *)origin)[2] ^
                                   ((uint32_t *)origin)[1] ^
                                   ((uint32_t *)origin)[0] ^ 0xdeadc0de;
    }

    if (*(int16_t *)0x4761d8 >= 0x20) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x60e, true);
      system_exit(-1);
    }

    {
      int depth = (int)*(int16_t *)0x4761d8;
      *(int16_t *)0x4761d8 += 1;
      *(int16_t *)(0x5a8c80 + depth * 2) = 9;
    }

    if (FUN_0014df70(0x100061, (float *)origin, (float *)direction, -1,
                     collision_result) &&
        collision_result[0] != 0 && collision_result[0] == 2) {
      uint8_t *decal_tag = (uint8_t *)tag_get(0x64656361, decal_tag_index);
      if ((decal_tag[0] & 0x10) == 0) {
        FUN_0009ac90(decal_tag_index, collision_result, direction, scale,
                     randomize, color_index, flags);
      }
    }

    if (*(int16_t *)0x4761d8 <= 1) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x632, true);
      system_exit(-1);
    }

    *(int16_t *)0x4761d8 -= 1;

    if (randomize) {
      if (local_random_seed_address == NULL) {
        display_assert("local_random_seed_address",
                       "c:\\halo\\SOURCE\\effects\\decals.c", 0x636, true);
        system_exit(-1);
      }

      *local_random_seed_address = local_seed;
    }
  }
}

int FUN_0017cae0(uint32_t cache_size)
{
  return FUN_0015b460(cache_size);
}

void *FUN_0017caf0(int cache_index, uint32_t cache_size)
{
  return FUN_0015b890(cache_index, cache_size);
}

void FUN_0017cb10(int decal_index)
{
  FUN_0015b530(decal_index);
}
