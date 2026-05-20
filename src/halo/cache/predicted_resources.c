/* Returns the predicted-resources global at 0x4e9254 (0x1bdd40).
 * MOV EAX,[0x004e9254]; RET — 6-byte leaf getter. */
int FUN_001bdd40(void)
{
  return *(int *)0x4e9254;
}

/* Returns the predicted-resources global at 0x4e9258 (0x1bdd50).
 * MOV EAX,[0x004e9258]; RET — 6-byte leaf getter. */
int FUN_001bdd50(void)
{
  return *(int *)0x4e9258;
}

/* Returns the predicted-resources global at 0x4e925c (0x1bdd60).
 * MOV EAX,[0x004e925c]; RET — 6-byte leaf getter. */
void *FUN_001bdd60(void)
{
  return *(void **)0x4e925c;
}

/* Returns the predicted-resources global at 0x4e9260 (0x1bdd70).
 * MOV EAX,[0x004e9260]; RET — 6-byte leaf getter. */
int FUN_001bdd70(void)
{
  return *(int *)0x4e9260;
}

/* Precache sound permutations for a sound tag (0x1bdd80).
 * Iterates all pitch ranges and their permutations, requesting
 * each to be loaded into the sound cache. */
void FUN_001bdd80(int tag_index)
{
  char *sound_data = (char *)tag_get(0x736e6421, tag_index);
  int *pitch_ranges = (int *)(sound_data + 0x98);
  int16_t i;

  for (i = 0; i < *pitch_ranges; i++) {
    char *pitch_range =
      (char *)tag_block_get_element(pitch_ranges, (int)i, 0x48);
    int16_t j;

    for (j = 0; j < *(int16_t *)(pitch_range + 0x2c); j++) {
      void *permutation =
        tag_block_get_element((void *)(pitch_range + 0x3c), (int)j, 0x7c);
      sound_cache_request_sound(permutation, 0, 1, 0);
    }
  }
}

/* Precache predicted resources from a tag_block (0x1bde10).
 * Iterates elements (size 8): type 0 = bitmap, type 1 = sound.
 * For bitmaps, resolves the 'bitm' tag and precaches the hardware texture.
 * For sounds, calls FUN_001bdd80 to precache the sound permutations. */
void predicted_resources_precache(void *tag_block)
{
  int16_t i;
  int count = *(int *)tag_block;

  for (i = 0; i < count; i++) {
    int16_t *element = (int16_t *)tag_block_get_element(tag_block, (int)i, 8);
    int16_t type = *element;

    if (type == 0) {
      int16_t bitmap_index = element[1];
      int tag_datum = *(int *)(element + 2);
      char *tag_data = (char *)tag_get(0x6269746d, tag_datum);
      void *bitmaps_block = (void *)(tag_data + 0x60);
      void *hw_format =
        tag_block_get_element(bitmaps_block, (int)bitmap_index, 0x30);
      xbox_texture_cache_get_hardware_format(hw_format, false, true);
    } else if (type == 1) {
      int tag_datum = *(int *)(element + 2);
      FUN_001bdd80(tag_datum);
    }
  }
}

/* Dispose predicted-resources subsystem state (0x1bde90).
 * Calls data_dispose on the resource data table (0x4e9368),
 * lruv_cache_dispose on the LRU cache (0x4e9370),
 * then clears the active-flag at 0x4e936c. */
void FUN_001bde90(void)
{
  data_dispose(*(data_t **)0x4e9368);
  lruv_cache_dispose(*(void **)0x4e9370);
  *(int *)0x4e936c = 0;
}

/* Delete all entries from the predicted-resources data table (0x1bdec0).
 * Calls data_delete_all on the resource data table at 0x4e9368. */
void FUN_001bdec0(void)
{
  data_delete_all(*(data_t **)0x4e9368);
}
