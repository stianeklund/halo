/* sound_definitions.c — Sound definition tag accessors.
 * Object: sound_definitions.obj
 * Original source: c:\halo\SOURCE\sound\sound_definitions.c */

/* FUN_001c8d90 (0x1c8d90) — Returns a pointer into a permutation block's data
 * at the given tick_index. Asserts that tick_index is within the block's count
 * (stored at offset 0x54). The data base pointer is at offset 0x60. */
void *FUN_001c8d90(int permutation_block_ptr, int tick_index)
{
  char *block;
  int base;

  block = (char *)permutation_block_ptr;

  assert_halt((int16_t)tick_index >= 0 &&
              (int)(int16_t)tick_index < *(int *)(block + 0x54));

  base = *(int *)(block + 0x60);
  return (void *)(base + (int)(int16_t)tick_index);
}
