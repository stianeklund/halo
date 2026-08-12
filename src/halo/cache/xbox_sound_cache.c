/* Xbox sound cache: LRU-V cache of hardware sound data.
 * Source: c:\halo\SOURCE\cache\xbox_sound_cache.c */

#define XBOX_SOUND_CACHE_FILE "c:\\halo\\SOURCE\\cache\\xbox_sound_cache.c"

/* LRU-V handle for the hardware sound cache. Loaded (MOV EAX,[0x4e9370]), not
 * addressed, so the global holds the pointer rather than the cache itself. */
#define xbox_sound_cache (*(void **)0x4e9370)

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
