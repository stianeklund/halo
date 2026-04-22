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
