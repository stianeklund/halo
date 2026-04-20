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
