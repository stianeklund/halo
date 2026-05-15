/* Tear down DirectSound: walk all active channels and release their
 * buffer objects, then release the primary buffer and DirectSound
 * objects themselves, finally clearing the dsound_globals state.
 *
 * Each channel struct is 0x74 bytes; field at offset 0x70 holds a
 * pointer to a small COM-like object whose vtable second slot
 * (vtbl+4) is the Release method. Globals layout matches
 * sound_dsound_xbox.c so the assertion strings reference that TU. */
void FUN_001c93f0(void)
{
  int i;
  int *channel_obj;
  short channel_count;
  void *primary_buffer;
  void *dsound_obj;

  i = 0;
  channel_count = *(short *)0x4fdfc4;
  if (channel_count > 0) {
    do {
      if ((short)i < 0 || (short)i >= channel_count) {
        display_assert("index>=0 && index<dsound_globals.actual_channel_count",
                       "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
        system_exit(-1);
      }
      if ((short)i >= 0x100) {
        display_assert("index<MAXIMUM_SOUND_CHANNELS",
                       "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a, 1);
        system_exit(-1);
      }
      channel_obj = *(int **)(0x4fdfc8 + (short)i * 0x74 + 0x70);
      if (channel_obj != 0) {
        /* vtable slot 1 (offset 0x4) — Release. */
        ((void(__stdcall *)(int *))(*(void **)((char *)*channel_obj + 4)))(
          channel_obj);
      }
      i = i + 1;
      channel_count = *(short *)0x4fdfc4;
    } while (i < channel_count);
  }

  primary_buffer = *(void **)0x505460;
  if (primary_buffer != 0) {
    /* IDirectSoundBuffer_Release stdcall thunk. */
    ((void(__stdcall *)(void *))0x203897)(primary_buffer);
  }
  *(short *)0x4fdfc4 = 0;
  *(short *)0x4fdbc2 = 0;

  dsound_obj = *(void **)0x50545c;
  if (dsound_obj != 0) {
    /* IDirectSound_Release stdcall thunk. */
    ((void(__stdcall *)(void *))0x203861)(dsound_obj);
    *(void **)0x50545c = 0;
  }
  *(char *)0x4fdbc0 = 0;
}

/* Return the DirectSound handle if sound is initialized, else 0 (0x1c94b0). */
int bink_get_dsound_handle(void)
{
  return *(char *)0x4fdbc0 ? *(int *)0x50545c : 0;
}
