/* Xbox sound cache: LRU-V cache of hardware sound data.
 * Source: c:\halo\SOURCE\cache\xbox_sound_cache.c */

#define XBOX_SOUND_CACHE_FILE "c:\\halo\\SOURCE\\cache\\xbox_sound_cache.c"

/* LRU-V handle for the hardware sound cache. Loaded (MOV EAX,[0x4e9370]), not
 * addressed, so the global holds the pointer rather than the cache itself. */
#define xbox_sound_cache (*(void **)0x4e9370)

/* Datum table the cache handle in sound->field_2c indexes. Also loaded
 * (MOV EAX,[0x4e9368]), so the global holds the pointer. */
#define sound_cache_data (*(data_t **)0x4e9368)

/* Cache datum fields observed by this TU: +0x04 software reference count
 * ("soft"), +0x05 hardware reference count ("hard"), +0x08 the sound
 * permutation the block was filled from. The permutation pointer is handed
 * straight to a %s conversion -- the permutation block begins with its name --
 * and the tag index it belongs to lives at +0x3c. */
#define sound_cache_datum(sound) \
  ((char *)datum_get(sound_cache_data, (sound)->field_2c))
#define sound_cache_permutation(sound) \
  (*(char **)(sound_cache_datum(sound) + 8))

/* Base address of the cache's backing store in hardware sound memory. Named
 * from the assert message ("xbox_sound_cache_globals.base_address"); typed int
 * because its producer (FUN_001bdd70) returns EAX as an int and the only use
 * observed here is the != 0 test. */
#define xbox_sound_cache_base_address (*(int *)0x4e936c)

/* Outstanding hardware sound references. Compared with a 16-bit operand
 * (CMP word ptr [0x5054ea],0), so the global is 2 bytes wide, not 4. Name from
 * the assert message; signedness unproven (only the != 0 test is observed). */
#define hardware_sound_reference_count (*(int16_t *)0x5054ea)

void xbox_sound_cache_idle(void)
{
  lruv_idle(xbox_sound_cache);

  if (hardware_sound_reference_count != 0) {
    display_assert("hardware sound reference count failure.",
                   XBOX_SOUND_CACHE_FILE, 0x94, true);
    system_exit(-1);
  }
}

/* Initialize a freshly created cache record. The sound must not already own a
 * cache allocation; a1 is the dword the creator hands through to +0x34 (its
 * meaning is unproven -- only the store is observed). */
void sound_cache_sound_new(void *a1, sound_cache_sound *sound)
{
  if (sound->cache_base_address != NULL) {
    display_assert("sound->cache_base_address==NULL", XBOX_SOUND_CACHE_FILE,
                   0x9e, true);
    system_exit(-1);
  }

  sound->field_2c = -1;
  sound->cache_base_address = NULL;
  sound->field_34 = a1;
}

/* Give a sound's cache block back to the LRU-V cache and mark the sound as no
 * longer resident. Nothing may still be playing out of the block: both the
 * software and the hardware reference count on the cache datum must be zero.
 * The handle is re-read from the sound on every use (the original keeps no
 * local copy -- the frame has no stack slots, only a saved ESI). */
void sound_cache_sound_delete(sound_cache_sound *sound)
{
  if (sound->field_2c != NONE) {
    if (sound_cache_datum(sound)[4] != 0) {
      display_assert(
        csprintf(error_string_buffer,
                 "tried to delete sound %s(%s) from the cache while it was "
                 "playing (soft).",
                 tag_get_name(*(int *)(sound_cache_permutation(sound) + 0x3c)),
                 sound_cache_permutation(sound)),
        XBOX_SOUND_CACHE_FILE, 0xad, true);
      system_exit(-1);
    }

    if (sound_cache_datum(sound)[5] != 0) {
      display_assert(
        csprintf(error_string_buffer,
                 "tried to delete sound %s(%s) from the cache while it was "
                 "playing (hard).",
                 tag_get_name(*(int *)(sound_cache_permutation(sound) + 0x3c)),
                 sound_cache_permutation(sound)),
        XBOX_SOUND_CACHE_FILE, 0xae, true);
      system_exit(-1);
    }

    lruv_block_delete(xbox_sound_cache, sound->field_2c);
  }

  sound->field_2c = NONE;
  sound->cache_base_address = NULL;
}

/* Bring up the Xbox sound cache: the datum table that tracks per-sound cache
 * records, the LRU-V cache itself (0x400 pages of 2^0xc bytes, up to 0x200
 * blocks, with the TU's own block-delete and block-query callbacks), and the
 * base address of the backing store in hardware sound memory. Every step is
 * fatal on failure. */
void sound_cache_new(void)
{
  sound_cache_data = data_new("xbox sound", 0x200, 0xc);
  if (sound_cache_data == NULL) {
    display_assert("xbox_sound_cache_globals.cache_sounds",
                   XBOX_SOUND_CACHE_FILE, 0x45, true);
    system_exit(-1);
  }

  xbox_sound_cache =
    lruv_new((int)"xbox sound cache", 0x400, 0xc, 0x200,
             (void (*)(int))FUN_001be1b0, (int (*)(int))FUN_001be170);
  if (xbox_sound_cache == NULL) {
    display_assert("xbox_sound_cache_globals.cache", XBOX_SOUND_CACHE_FILE,
                   0x49, true);
    system_exit(-1);
  }

  xbox_sound_cache_base_address = FUN_001bdd70();
  if (xbox_sound_cache_base_address == 0) {
    display_assert("xbox_sound_cache_globals.base_address",
                   XBOX_SOUND_CACHE_FILE, 0x4c, true);
    system_exit(-1);
  }
}

/* Release every cache block that nothing is playing out of. Walks the cache
 * datum table and hands each idle record's sound back to
 * sound_cache_sound_delete; a record whose software (+0x04) or hardware
 * (+0x05) reference count is non-zero is skipped. The deleted argument is the
 * sound the block was filled from (+0x08), not the datum itself. */
void sound_cache_flush(void)
{
  data_iter_t iterator;
  char *cache_datum;

  data_iterator_new(&iterator, sound_cache_data);
  cache_datum = (char *)data_iterator_next(&iterator);
  while (cache_datum != NULL) {
    if (cache_datum[4] == 0 && cache_datum[5] == 0) {
      sound_cache_sound_delete(*(sound_cache_sound **)(cache_datum + 8));
    }
    cache_datum = (char *)data_iterator_next(&iterator);
  }
}
