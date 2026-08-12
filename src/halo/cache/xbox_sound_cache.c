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
