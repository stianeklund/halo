#if defined(__clang__)
#ifndef fabs
#define fabs __builtin_fabs
#endif
#ifndef fabsf
#define fabsf __builtin_fabsf
#endif
#elif defined(_MSC_VER)
extern double __cdecl fabs(double);
#pragma intrinsic(fabs)
#ifndef fabsf
#define fabsf(x) ((float)fabs((double)(x)))
#endif
#endif

/* sound_dsound_get_sample_rate (0x1c90e0)
 *
 * Return the sample rate for the given codec index.  Index 0 yields
 * 22050 Hz, index 1 yields 44100 Hz.  Asserts codec_index is in the
 * range [0, NUMBER_OF_SOUND_SAMPLE_RATES=2). */
int sound_dsound_get_sample_rate(short sample_rate)
{
  if (sample_rate < 0 || sample_rate >= 2) {
    display_assert("sample_rate>=0 && sample_rate<NUMBER_OF_SOUND_SAMPLE_RATES",
                   "c:\\halo\\source\\sound\\sound_definitions.h", 0x135, 1);
    system_exit(-1);
  }
  return *(int *)((char *)0x2bcc18 + sample_rate * 4);
}

/* sound_dsound_gain_to_volume (0x1c9130)
 *
 * Convert a linear gain value [0.0, 1.0] to a DirectSound volume in
 * hundredths of dB.  The formula is: 2000 * log10(gain) + ceiling.
 * If gain is exactly 0, returns -10000 (DSBVOLUME_MIN).
 * Result is clamped to [-10000, ceiling]. */
int sound_dsound_gain_to_volume(float gain, int ceiling)
{
  int volume;

  if (!(gain >= 0.0f && gain <= 1.0f)) {
    display_assert("gain>=0.f && gain<=1.f",
                   "c:\\halo\\source\\sound\\sound_dsound.h", 0x23, 1);
    system_exit(-1);
  }

  if (gain == 0.0f) {
    volume = -10000;
  } else {
    volume = (int)(*(double *)0x2c07b8 * log10(gain) + ceiling);
    if (volume < -10000)
      volume = -10000;
    else if (volume > ceiling)
      volume = ceiling;
  }
  return volume;
}

/* sound_dsound_pitch_to_frequency (0x1c91c0)
 *
 * Convert a pitch scalar to a DirectSound frequency value.  Asserts
 * that sample_rate is either 22050 or 44100.  The result is
 * sample_rate * pitch, clamped to [188, 191983]. */
int sound_dsound_pitch_to_frequency(int sample_rate, float pitch)
{
  float frequency;

  if (sample_rate != 22050 && sample_rate != 44100) {
    display_assert("samples_per_second==22050 || samples_per_second==44100",
                   "c:\\halo\\source\\sound\\sound_dsound.h", 0x36, 1);
    system_exit(-1);
  }

  frequency = (float)sample_rate * pitch;

  if (frequency < *(float *)0x2c0800)
    frequency = *(float *)0x2c0800;
  if (frequency > *(float *)0x2c07fc)
    frequency = *(float *)0x2c07fc;
  return (int)frequency;
}

/* dsound_angle_from_angle (0x1c9230)
 *
 * Convert an angle in radians to integer degrees.  The binary is a
 * six-instruction leaf: FLD [EBP+8]; FMUL [0x2b073c]; JMP _ftol2.
 * The constant at 0x2b073c is 0x42652EE1 == 57.29578f == 180/PI.
 * DirectSound cone/orientation angles are specified in whole degrees. */
int dsound_angle_from_angle(float angle)
{
  return (int)(angle * 57.29578f);
}

/* dsound_occlusion_from_occlusion (0x1c9250)
 *
 * Convert a linear occlusion factor [0.0, 1.0] into a DirectSound
 * volume attenuation in hundredths of dB.  Twelve instructions:
 * FLD [0x2533c8]; PUSH 0; FSUB [EBP+8]; PUSH ECX; FSTP [ESP];
 * CALL sound_dsound_gain_to_volume; ADD ESP,8.  The constant at
 * 0x2533c8 is .rdata 0x3F800000 == 1.0f, so the FSUB (ST0 - m32)
 * computes 1.0f - occlusion: full occlusion (1.0) yields gain 0,
 * i.e. silence.  The result of the tail-position call is returned
 * untouched in EAX (implicit return, no MOV after the CALL). */
int dsound_occlusion_from_occlusion(float occlusion)
{
  return sound_dsound_gain_to_volume(1.0f - occlusion, 0);
}

/* dsound_obstruction_from_obstruction (0x1c9270)
 *
 * Convert a linear obstruction factor [0.0, 1.0] into a DirectSound
 * volume attenuation in hundredths of dB.  Byte-for-byte the same
 * shape as dsound_occlusion_from_occlusion above: FLD [0x2533c8];
 * PUSH 0; FSUB [EBP+8]; PUSH ECX; FSTP [ESP];
 * CALL sound_dsound_gain_to_volume; ADD ESP,8.  The constant at
 * 0x2533c8 is .rdata 0x3F800000 == 1.0f, and FSUB m32 computes
 * ST0 - m32, i.e. 1.0f - obstruction: full obstruction (1.0) maps to
 * gain 0 (silence).  The PUSH ECX is only a placeholder slot that
 * FSTP overwrites with the float, not a third argument (ADD ESP,8
 * confirms exactly two stack args).  The call is in tail position and
 * its EAX falls through to RET, so the result is returned implicitly
 * with no MOV after the CALL. */
int dsound_obstruction_from_obstruction(float obstruction)
{
  return sound_dsound_gain_to_volume(1.0f - obstruction, 0);
}

/* sound_dsound_channel_get (0x1c9290)
 *
 * Return a pointer to the actual dsound channel struct at the given
 * index.  Asserts index is in [0, dsound_globals.actual_channel_count)
 * and less than MAXIMUM_SOUND_CHANNELS (256).  Element size is 0x74. */
void *sound_dsound_channel_get(short index)
{
  if (index < 0 || index >= *(short *)0x4fdfc4) {
    display_assert("index>=0 && index<dsound_globals.actual_channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
    system_exit(-1);
  }
  if (index >= 0x100) {
    display_assert("index<MAXIMUM_SOUND_CHANNELS",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a, 1);
    system_exit(-1);
  }
  return (void *)(0x4fdfc8 + (int)index * 0x74);
}

/* sound_dsound_vchannel_get (0x1c92f0)
 *
 * Return a pointer to the virtual channel struct at the given index.
 * Asserts index is in [0, dsound_globals.virtual_channel_count) and
 * less than MAXIMUM_SOUND_CHANNELS (256).  Element size is 4. */
void *sound_dsound_vchannel_get(short index)
{
  if (index < 0 || index >= *(short *)0x4fdbc2) {
    display_assert("index>=0 && index<dsound_globals.virtual_channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x72, 1);
    system_exit(-1);
  }
  if (index >= 0x100) {
    display_assert("index<MAXIMUM_SOUND_CHANNELS",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x73, 1);
    system_exit(-1);
  }
  return (void *)(0x4fdbc4 + (int)index * 4);
}

/* Global DirectSound failure state.  The reason accumulator is a
 * 256-byte character buffer (bound proven by the CMP EDI,0x100 guard
 * in sound_dsound_set_last_error). */
#define SOUND_DSOUND_ERROR_REASON ((char *)0x4eae38)
#define SOUND_DSOUND_LAST_HRESULT (*(int *)0x4fdba8)

/* sound_dsound_set_last_error (0x1c9350)
 *
 * Record a DirectSound failure.  If hresult is non-NULL, the value it
 * points at is copied into the global last-error slot.  The reason
 * text is then appended to the global 256-byte reason accumulator,
 * but only when the combined length still fits (unsigned compare
 * against 0x100), so an overlong chain of reasons is silently dropped
 * rather than overflowing the buffer.
 *
 * Both arguments arrive in registers: hresult in EAX (callers pass
 * LEA EAX,[EBP-N] of a local HRESULT) and reason in ESI (callers pass
 * a string constant such as "couldn't queue sound packet.").
 *
 * The append is performed by the 2-argument call at 0x1d90f0 with the
 * reason as the format string; that is exactly what the binary does,
 * so it is preserved verbatim rather than "fixed" into a 3-argument
 * sprintf (which would change the push count). */
void sound_dsound_set_last_error(int *hresult, const char *reason)
{
  if (hresult != NULL) {
    SOUND_DSOUND_LAST_HRESULT = *hresult;
  }

  if ((unsigned int)(csstrlen(SOUND_DSOUND_ERROR_REASON) + csstrlen(reason)) <
      0x100) {
    crt_sprintf(SOUND_DSOUND_ERROR_REASON + csstrlen(SOUND_DSOUND_ERROR_REASON),
                reason);
  }
}

/* sound_dsound_channel_update_3d (0x1c94d0)
 *
 * Build and apply full 3D buffer parameters for the given channel.
 * Constructs a 36-byte parameter block with volume levels (converted
 * from gain via gain_to_volume) and a rolloff factor of 0.2.
 *
 * If the channel is not active (field_4 == 0), all volumes are set
 * to -10000 (minimum).  Otherwise, gains are derived from the
 * channel's fade and orientation fields.  When field_5 is set, a
 * global volume at 0x32f6d4 is applied to offset 0x0 and 0x4.
 *
 * Calls IDirectSoundStream_SetAllParameters to commit the block. */
void sound_dsound_channel_update_3d(int channel_index)
{
  void *channel;
  char params[0x24];
  float fade_gain;

  channel = sound_dsound_channel_get((short)channel_index);
  csmemset(params, 0, 0x24);
  *(int *)(params + 0x00) = 0;
  *(int *)(params + 0x04) = 0;
  *(int *)(params + 0x10) = 0;
  *(int *)(params + 0x18) = 0;
  *(float *)(params + 0x20) = 0.2f;

  if (*(char *)((char *)channel + 0x4) != 0) {
    fade_gain = 1.0f - *(float *)((char *)channel + 0x60);

    if (*(char *)((char *)channel + 0x5) != 0) {
      *(int *)(params + 0x04) =
        sound_dsound_gain_to_volume(*(float *)0x32f6d4, 0);
      *(int *)(params + 0x00) =
        sound_dsound_gain_to_volume(*(float *)0x32f6d4, 0);
    } else {
      fade_gain = fade_gain * 0.5f;
    }

    {
      int fade_vol = sound_dsound_gain_to_volume(fade_gain, 0);
      *(int *)(params + 0x08) = fade_vol;
      *(int *)(params + 0x0c) = fade_vol;
    }
    *(int *)(params + 0x14) =
      sound_dsound_gain_to_volume(1.0f - *(float *)((char *)channel + 0x48), 0);
    *(int *)(params + 0x1c) =
      sound_dsound_gain_to_volume(1.0f - *(float *)((char *)channel + 0x44), 0);
  } else {
    *(int *)(params + 0x08) = -10000;
    *(int *)(params + 0x0c) = -10000;
    *(int *)(params + 0x14) = 0;
    *(int *)(params + 0x1c) = 0;
  }

  IDirectSoundStream_SetAllParameters(*(void **)((char *)channel + 0x70),
                                      params, 1);
}

/* sound_dsound_channel_stop_check (0x1c9600)
 *
 * Check if a stopping channel's DirectSound stream has finished
 * processing.  Asserts the channel is in the "stopping" state.
 *
 * Calls dsound_stream_is_active (0x20f069) to test whether the
 * stream's internal kernel status bits (0x10000002) are still set.
 * If the stream is no longer active, flushes it via the
 * IDirectSoundStream vtable Flush method (vtable[6], offset 0x18)
 * and clears the channel's stopping flag.
 *
 * Returns true if the channel was released (stream finished and
 * flushed), false if still active. */
bool sound_dsound_channel_stop_check(short channel_index)
{
  void *channel;
  void *stream;
  int active;
  bool released;

  channel = sound_dsound_channel_get(channel_index);

  assert_halt_msg_at("channel->stopping", "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x4b8, *(char *)((char *)channel + 0x6) != 0);

  stream = *(void **)((char *)channel + 0x70);
  active = dsound_stream_is_active(stream);
  released = (active == 0);

  if (released) {
    void *stream2 = *(void **)((char *)channel + 0x70);
    void **vtable = *(void ***)stream2;
    ((int(__stdcall *)(void *))vtable[6])(stream2);
    *(char *)((char *)channel + 0x6) = 0;
  }

  return released;
}

/* sound_dsound_log_error (0x1c98f0)
 *
 * Log a DirectSound error.  Formats the caller's message with
 * vsprintf, maps the HRESULT to a symbolic name, and emits a level-2
 * error via error().  HRESULT is passed in ESI. */
void sound_dsound_log_error(int hresult, const char *message, ...)
{
  char buffer[0x1000];
  const char *error_name;
  char *arglist;

  arglist = (char *)&message + 4;
  vsprintf(buffer, message, arglist);

  switch (hresult) {
  case (int)0x8878001E:
    error_name = "DSERR_CONTROLUNAVAIL";
    break;
  case (int)0x88780032:
    error_name = "DSERR_INVALIDCALL";
    break;
  case (int)0x88780078:
    error_name = "DSERR_NODRIVER";
    break;
  case (int)0x8007000E:
    error_name = "DSERR_OUTOFMEMORY";
    break;
  case (int)0x80004001:
    error_name = "DSERR_UNSUPPORTED";
    break;
  case (int)0x80004005:
    error_name = "DSERR_GENERIC";
    break;
  case (int)0x80040110:
    error_name = "DSERR_NOAGGREGATION";
    break;
  default:
    error_name = "<unknown error>";
    break;
  }

  error(2, "DirectSound:  '%s' (%s#%d)", buffer, error_name);
}

/* sound_dsound_channel_try_resolve (0x1c99a0)
 *
 * Try to find and assign a free actual channel for the given virtual
 * channel.  Looks up the virtual channel, asserts it has no current
 * assignment (channel_index == NONE) and a valid type_index.
 *
 * Scans actual channels of the matching type, starting from the
 * priority table entry for the vchannel's type_index.  A channel is
 * eligible if its type_flags match and it is either unassigned
 * (virtual_channel_index == NONE) or currently stopping and can be
 * released.
 *
 * If no free channel is found, logs a warning with the type_index. */
void sound_dsound_channel_try_resolve(int virtual_channel_index)
{
  short *vchannel;
  short si;
  void *channel;

  vchannel = (short *)sound_dsound_vchannel_get(virtual_channel_index);

  /* assert: vchannel has no channel assigned */
  if (vchannel[0] != (short)-1) {
    display_assert("vchannel->channel_index==NONE",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x588, 1);
    system_exit(-1);
  }

  /* assert: type_index is valid */
  if (vchannel[1] < 0 || vchannel[1] >= 4) {
    display_assert("vchannel->type_index>=0 && "
                   "vchannel->type_index<NUMBER_OF_SOUND_CHANNEL_TYPES",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x589, 1);
    system_exit(-1);
  }

  si = ((short *)0x5053c8)[vchannel[1]];

  if (vchannel[0] == (short)-1) {
    while (si < *(short *)0x4fdfc4) {
      if (si < 0) {
        display_assert("index>=0 && index<dsound_globals.actual_channel_count",
                       "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
        system_exit(-1);
      }
      if (si >= 0x100) {
        display_assert("index<MAXIMUM_SOUND_CHANNELS",
                       "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a, 1);
        system_exit(-1);
      }

      channel = (void *)(0x4fdfc8 + (int)si * 0x74);

      if (*(short *)((char *)channel + 0x38) !=
          ((short *)0x32fcf8)[vchannel[1]])
        break;

      if (*(short *)((char *)channel + 0x2) == (short)-1) {
        if (*(char *)((char *)channel + 0x6) == 0 ||
            sound_dsound_channel_stop_check(si)) {
          vchannel[0] = si;
        }
      }

      si++;
      if (vchannel[0] != (short)-1)
        break;
    }
  }

  /* if we found a channel, store the back-reference */
  if (vchannel[0] != (short)-1) {
    if (vchannel[0] < 0 || vchannel[0] >= *(short *)0x4fdfc4) {
      display_assert("index>=0 && index<dsound_globals.actual_channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
      system_exit(-1);
    }
    if (vchannel[0] >= 0x100) {
      display_assert("index<MAXIMUM_SOUND_CHANNELS",
                     "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a, 1);
      system_exit(-1);
    }
    *(short *)(0x4fdfc8 + (int)vchannel[0] * 0x74 + 0x2) =
      (short)virtual_channel_index;
  } else {
    error(2, "WARNING: ran out of actual sound channels of type %d",
          (int)vchannel[1]);
  }
}

/* sound_dsound_channel_resolve (0x1c9b40)
 *
 * Look up the virtual channel for the given virtual_channel_index and
 * return the underlying hardware (actual) channel index.  If the
 * virtual channel has no channel assigned (channel_index == NONE),
 * attempt to acquire one via sound_dsound_channel_try_resolve.
 *
 * After resolution, two debug assertions verify consistency:
 *   1. The actual channel's type_flags match sound_channel_type_flags
 *      for the virtual channel's type_index.
 *   2. The actual channel's virtual_channel_index back-references the
 *      caller's virtual_channel_index.
 *
 * Returns the actual channel index (short), or -1 (NONE) if no
 * channel could be resolved. */
short sound_dsound_channel_resolve(int virtual_channel_index)
{
  short *vchannel;
  short channel_index;
  void *channel;

  vchannel = (short *)sound_dsound_vchannel_get(virtual_channel_index);
  channel_index = vchannel[0];
  if (channel_index == -1) {
    sound_dsound_channel_try_resolve(virtual_channel_index);
    channel_index = vchannel[0];
    if (channel_index == -1)
      goto done;
  }

  /* assert: channel type_flags matches the expected type for this vchannel */
  channel = sound_dsound_channel_get(channel_index);
  if (*(short *)((char *)channel + 0x38) != ((short *)0x32fcf8)[vchannel[1]]) {
    display_assert("vchannel->channel_index==NONE || "
                   "channel_get(vchannel->channel_index)->type_flags=="
                   "sound_channel_type_flags[vchannel->type_index]",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x5b8, 1);
    system_exit(-1);
  }

  /* assert: channel's back-reference matches our virtual_channel_index */
  channel_index = vchannel[0];
  if (channel_index != -1) {
    channel = sound_dsound_channel_get(channel_index);
    if (*(short *)((char *)channel + 0x2) != (short)virtual_channel_index) {
      display_assert(
        "vchannel->channel_index==NONE || "
        "channel_get(vchannel->channel_index)->virtual_channel_index=="
        "virtual_channel_index",
        "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x5b9, 1);
      system_exit(-1);
    }
  }

done:
  return vchannel[0];
}

/* sound_dsound_channel_release (0x1c9bf0)
 *
 * Release the actual (hardware) channel currently bound to the virtual
 * channel identified by virtual_channel_index.  Does nothing when the
 * virtual channel has no channel bound (channel_index == NONE).
 *
 * When a channel is bound:
 *   1. Assert the channel back-references this virtual channel.
 *   2. If the channel is still playing (short at +0x0 != 0), stop the
 *      attached stream (channel+0x70) through the XDK media-object
 *      helper at 0x20f081, set the released byte at +0x6 and clear the
 *      playing flag.
 *   3. Clear the two dwords at +0x68 / +0x6c unconditionally -- the
 *      original emits `MOV [ESI+0x68],EBX` / `MOV [ESI+0x6c],EBX` with
 *      EBX == 0 *after* the conditional block, not inside it.
 *   4. Reset both back-references to NONE. */
void sound_dsound_channel_release(int virtual_channel_index)
{
  short *vchannel;
  void *channel;

  vchannel = (short *)sound_dsound_vchannel_get(virtual_channel_index);
  if (vchannel[0] != -1) {
    /* assert: channel's back-reference matches our virtual_channel_index */
    channel = sound_dsound_channel_get(vchannel[0]);
    if (*(short *)((char *)channel + 0x2) != (short)virtual_channel_index) {
      display_assert("channel_get(vchannel->channel_index)->"
                     "virtual_channel_index==virtual_channel_index",
                     "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x5dd, 1);
      system_exit(-1);
    }

    channel = sound_dsound_channel_get(vchannel[0]);
    if (*(short *)channel != 0) {
      FUN_0020f081(*(void **)((char *)channel + 0x70));
      *((char *)channel + 0x6) = 1;
      *(short *)channel = 0;
    }
    *(int *)((char *)channel + 0x68) = 0;
    *(int *)((char *)channel + 0x6c) = 0;

    channel = sound_dsound_channel_get(vchannel[0]);
    *(short *)((char *)channel + 0x2) = -1;
    vchannel[0] = -1;
  }
}

/* dsound_virtual_get_state (0x1c9c80)
 *
 * Backend descriptor slot +0x24 (table entry 0x32f6c0); called only through
 * that pointer (sound_manager.c: `(*(int (**)(int))(backend + 0x24))(i)`).
 * cdecl, one stack arg.  Returns 0 (XOR EAX,EAX) when the virtual channel has
 * no bound channel, otherwise the first int16 of the bound channel record
 * (MOV AX,[EAX] - only AX is defined), after asserting (line 0x5f3) that the
 * channel's virtual_channel_index (+0x2) points back at this virtual channel.
 * Both vchannel reads re-load the index from the vchannel (XOR ESI,ESI /
 * MOV SI,[EDI] twice). */
int16_t dsound_virtual_get_state(int virtual_channel_index)
{
  short *vchannel;

  vchannel = (short *)sound_dsound_vchannel_get((short)virtual_channel_index);
  if (vchannel[0] == -1)
    return 0;
  if (*(short *)((char *)sound_dsound_channel_get(vchannel[0]) + 0x2) !=
      (short)virtual_channel_index) {
    display_assert("channel_get(vchannel->channel_index)->virtual_channel_"
                   "index==virtual_channel_index",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x5f3, 1);
    system_exit(-1);
  }
  return *(short *)sound_dsound_channel_get(vchannel[0]);
}

/* dsound_fix_rear_speakers (0x1c9cf0)
 *
 * Xbox rear-speaker workaround: create a 32-byte 16-bit mono 22050 Hz
 * "inanity" DirectSound buffer, fill it with silence and start it
 * looping so the rear speakers stay fed.  Returns true once the buffer
 * has been created and started.
 *
 * Layouts are taken from the disassembly, not from an SDK header:
 *   DSBUFFERDESC (0x18 bytes, EBP-0x2c): +0x00 dwSize = 0x18,
 *   +0x04 dwFlags = 0, +0x08 dwBufferBytes = 0x20, +0x0c lpwfxFormat,
 *   +0x10 = 0x1f00 (mix-bin field), +0x14 left zero by the csmemset.
 *   WAVEFORMATEX (EBP-0x14): +0x00 wFormatTag = 1 (PCM), +0x02
 *   nChannels = 1, +0x04 nSamplesPerSec = 22050, +0x08
 *   nAvgBytesPerSec = 44100, +0x0c nBlockAlign = 2, +0x0e
 *   wBitsPerSample = 16.  The original never writes cbSize.
 *
 * The SetBufferData HRESULT is discarded by the original; only the
 * CreateSoundBuffer and Play results are tested. */
boolean dsound_fix_rear_speakers(void)
{
  unsigned char desc[0x18];
  unsigned char wfx[0x14];
  int result;

  *(uint16_t *)&wfx[0x00] = 1; /* wFormatTag (WAVE_FORMAT_PCM) */
  *(uint16_t *)&wfx[0x0e] = 0x10; /* wBitsPerSample */
  *(uint16_t *)&wfx[0x02] = 1; /* nChannels */
  *(uint16_t *)&wfx[0x0c] = 2; /* nBlockAlign */
  *(uint32_t *)&wfx[0x04] = 0x5622; /* nSamplesPerSec (22050) */
  *(uint32_t *)&wfx[0x08] = 0xac44; /* nAvgBytesPerSec (44100) */

  csmemset(desc, 0, 0x18);

  *(uint32_t *)&desc[0x00] = 0x18;
  *(uint32_t *)&desc[0x04] = 0;
  *(uint32_t *)&desc[0x08] = 0x20;
  *(uint32_t *)&desc[0x0c] = (uint32_t)wfx;
  *(uint32_t *)&desc[0x10] = 0x1f00;

  result = IDirectSound_CreateSoundBuffer(*(void **)0x50545c, desc,
                                          (void **)0x505460, NULL);
  if (result < 0) {
    sound_dsound_log_error(result, "failed to create the inanity channel.");
    return false;
  }

  csmemset((void *)0x505464, 0, 0x20);
  IDirectSoundBuffer_SetBufferData(*(void **)0x505460, (void *)0x505464, 0x20);

  result = IDirectSoundBuffer_Play(*(void **)0x505460, 0, 0, 1);
  if (result < 0) {
    sound_dsound_log_error(result, "failed to start the inanity channel.");
    return false;
  }

  return true;
}

/* dsound_begin_scene (0x1c9de0)
 *
 * Services DirectSound, then flushes any error text accumulated by
 * sound_dsound_set_last_error through sound_dsound_log_error (HRESULT in
 * ESI), and clears both the reason buffer and the saved HRESULT.  Name and
 * shape from PAL-2342 sound_dsound_xbox.c dsound_begin_scene (T2). */
void dsound_begin_scene(void)
{
  DirectSoundDoWork();
  if (csstrlen(SOUND_DSOUND_ERROR_REASON)) {
    sound_dsound_log_error(SOUND_DSOUND_LAST_HRESULT,
                           SOUND_DSOUND_ERROR_REASON);
  }
  SOUND_DSOUND_ERROR_REASON[0] = 0;
  SOUND_DSOUND_LAST_HRESULT = 0;
}

/* FUN_001c9e20 (0x1c9e20)
 *
 * Per-frame DirectSound service routine.
 *
 * Three phases:
 *   1. Commit the deferred 3D settings batched during the frame; log any
 *      failing HRESULT.
 *   2. Ramp dsound_globals.pause_gain (0x505488) toward its target --
 *      0.0 when dsound_globals.paused (0x505484) is set, 1.0 otherwise --
 *      in steps of 0.15.  The ramp arithmetic is performed in double
 *      precision and stored back as float, matching the original
 *      FLD dword / FADD qword / FSTP dword sequence.  While fading OUT
 *      (paused), every active hardware channel's stream volume is
 *      re-scaled by the new pause gain; while fading IN, the regular
 *      per-channel property update path picks the gain up instead.
 *   3. When the debug channel display flag (0x4fc380) is set, build a
 *      tab-separated report of every channel outside the reserved
 *      [0x10, 0x30] index band and hand it to the debug string renderer.
 *
 * The two index asserts inside the display loop carry lines 0x69/0x6a of
 * sound_dsound_xbox.c -- they come from the inlined channel accessor,
 * not from this function's own body. */
void FUN_001c9e20(void)
{
  char buffer[8192];
  short tab_stops[3];
  const char *name1;
  const char *name2;
  void *channel;
  int result;
  int volume;
  double ramp;
  short channel_index;
  short display_index;

  result = IDirectSound_CommitDeferredSettings(*(void **)0x50545c);
  if (result < 0) {
    sound_dsound_log_error(result, "couldn't commit deferred settings.");
  }

  /* Target pause gain: silent while paused, full otherwise.  The
   * comparison is written with the target inline so the compiler keeps
   * pause_gain in ST0 across the flag test and folds the constant into
   * each arm's FCOMPS, matching the original codegen. */
  if (*(float *)0x505488 != (*(char *)0x505484 != 0 ? 0.0f : 1.0f)) {
    if (!(*(float *)0x505488 >= 0.0f && *(float *)0x505488 <= 1.0f)) {
      display_assert(
        "dsound_globals.pause_gain>=0 && dsound_globals.pause_gain<=1.f",
        "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x27a, 1);
      system_exit(-1);
    }

    /* The ramp is computed in double precision and stored back exactly
     * once; the original keeps the running value on the FPU stack across
     * the clamp (FLD limit / FCOMP ST(1) / FSTP ST(0)). */
    ramp = *(float *)0x505488;
    if (*(char *)0x505484 != 0) {
      /* fading out */
      ramp -= 0.15;
      if (ramp < 0.0) {
        ramp = 0.0;
      }
    } else {
      /* fading in */
      ramp += 0.15;
      if (1.0 <= ramp) {
        ramp = 1.0;
      }
    }
    *(float *)0x505488 = (float)ramp;

    if (!(*(float *)0x505488 >= 0.0f && *(float *)0x505488 <= 1.0f)) {
      display_assert(
        "dsound_globals.pause_gain>=0 && dsound_globals.pause_gain<=1.f",
        "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x285, 1);
      system_exit(-1);
    }

    /* while fading out, re-scale every live stream's volume immediately */
    if (*(char *)0x505484 != 0) {
      channel_index = 0;
      if (0 < *(short *)0x4fdfc4) {
        do {
          channel = sound_dsound_channel_get(channel_index);
          if (*(short *)channel != 0) {
            volume = sound_dsound_gain_to_volume(
              *(float *)0x505488 * *(float *)((char *)channel + 0x3c), 0);
            IDirectSoundStream_SetVolume(*(void **)((char *)channel + 0x70),
                                         volume);
          }
          channel_index = (short)(channel_index + 1);
        } while (channel_index < *(short *)0x4fdfc4);
      }
    }
  }

  /* debug channel display */
  if (*(char *)0x4fc380 != 0) {
    tab_stops[0] = 0x118;
    tab_stops[1] = 0;
    tab_stops[2] = 0;
    draw_string_set_tab_stops(tab_stops, 1);

    display_index = 0;
    buffer[0] = '\0';
    if (0 < *(short *)0x4fdfc4) {
      do {
        if (display_index < 0 || display_index >= *(short *)0x4fdfc4) {
          display_assert(
            "index>=0 && index<dsound_globals.actual_channel_count",
            "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
          system_exit(-1);
        }
        if (display_index >= 0x100) {
          display_assert("index<MAXIMUM_SOUND_CHANNELS",
                         "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a,
                         1);
          system_exit(-1);
        }
        channel = (void *)(0x4fdfc8 + (int)display_index * 0x74);

        /* channels 0x10..0x30 are reserved and never listed */
        if (display_index < 0x10 || display_index > 0x30) {
          if (*(short *)channel != 0) {
            name2 = *(const char **)((char *)channel + 0x6c);
            if (name2 == 0) {
              name2 = "";
            }
            name1 = *(const char **)((char *)channel + 0x68);
            if (name1 == 0) {
              name1 = "";
            }
            crt_sprintf(buffer + csstrlen(buffer), "%d %1.2f %1.2f %s(%s)",
                        (int)*(short *)((char *)channel + 0x8),
                        *(float *)((char *)channel + 0x3c),
                        *(float *)((char *)channel + 0x40), name1, name2);
          }
          crt_sprintf(buffer + csstrlen(buffer), "|t");
          if ((display_index & 1) != 0) {
            crt_sprintf(buffer + csstrlen(buffer), "|n");
          }
        }
        display_index = (short)(display_index + 1);
      } while (display_index < *(short *)0x4fdfc4);
    }

    FUN_00189c40(0, buffer);
  }
}

/* FUN_001ca130 (0x1ca130)
 *
 * Walk every hardware sound channel and finish any pending stop:
 * for a channel whose `stopping` flag is set, busy-wait until its
 * IDirectSoundStream reports inactive, then Flush it (vtable slot 6,
 * byte offset 0x18) and clear the flag.  Afterwards, a channel that
 * has the save-and-quit warning flag set emits the level-2 "the devil"
 * error and the flag is cleared.
 *
 * The body of the spin loop is sound_dsound_channel_stop_check
 * (0x1c9670) inlined by the original compiler -- which is why the two
 * bounds asserts of the channel accessor (lines 0x69/0x6a) and the
 * `channel->stopping` assert (line 0x4b8) are re-executed on every
 * spin iteration, and why the stream pointer is re-loaded from
 * channel+0x70 after the loop rather than cached across it.
 *
 * dsound_globals base 0x4fdfc4; channel array at 0x4fdfc8, stride 0x74.
 * Channel fields used here (widths verified against the disassembly):
 *   +0x00 int16 state     (_sound_channel_idle == 0)
 *   +0x06 uint8 stopping
 *   +0x08 int16 save_and_quit_warning
 *   +0x70 void* stream
 * Ghidra prints the same 0x74 stride as *0x74 / *0x3a / *0x1d
 * depending on the base pointer type it picked; they are all one
 * channel. */
void FUN_001ca130(void)
{
  short i;
  char *channel;
  void *stream;
  void **vtable;
  int active;
  bool released;

  i = 0;
  while (i < *(short *)0x4fdfc4) {
    /* inlined sound_dsound_channel_get(i) */
    if (i < 0 || i >= *(short *)0x4fdfc4) {
      display_assert("index>=0 && index<dsound_globals.actual_channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
      system_exit(-1);
    }
    if (i >= 0x100) {
      display_assert("index<MAXIMUM_SOUND_CHANNELS",
                     "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a, 1);
      system_exit(-1);
    }
    channel = (char *)(0x4fdfc8 + (int)i * 0x74);

    /* assert(channel->stopping || channel->state==_sound_channel_idle) */
    if (*(char *)(channel + 0x6) == 0 && *(short *)channel != 0) {
      display_assert("channel->stopping || channel->state==_sound_channel_idle",
                     "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x2c2, 1);
      system_exit(-1);
    }

    if (*(char *)(channel + 0x6) != 0) {
      /* while (!sound_dsound_channel_stop_check(i)) ; -- inlined */
      do {
        if (i < 0 || i >= *(short *)0x4fdfc4) {
          display_assert(
            "index>=0 && index<dsound_globals.actual_channel_count",
            "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
          system_exit(-1);
        }
        if (i >= 0x100) {
          display_assert("index<MAXIMUM_SOUND_CHANNELS",
                         "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a,
                         1);
          system_exit(-1);
        }
        if (*(char *)(channel + 0x6) == 0) {
          display_assert("channel->stopping",
                         "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x4b8,
                         1);
          system_exit(-1);
        }
        active = dsound_stream_is_active(*(void **)(channel + 0x70));
        released = (active == 0);
      } while (!released);

      /* IDirectSoundStream::Flush -- vtable[6], this passed on the
       * stack (PUSH EAX, no ADD ESP), so __stdcall not __thiscall. */
      stream = *(void **)(channel + 0x70);
      vtable = *(void ***)stream;
      ((int(__stdcall *)(void *))vtable[6])(stream);
      *(char *)(channel + 0x6) = 0;
    }

    if (*(short *)(channel + 0x8) != 0) {
      error(2, "DirectSound: you're screwed if you try to save and quit -- the "
               "devil.");
      *(short *)(channel + 0x8) = 0;
    }
    i = (short)(i + 1);
  }
}

/* FUN_001ca2b0 (0x1ca2b0)
 *
 * Push the 3D listener state -- position, orientation, velocity and the
 * I3DL2 reverb environment -- down to DirectSound, skipping any of the
 * four updates whose inputs have not moved since the last send.
 *
 * `params` is the listener parameter block:
 *   +0x00 position x,y,z    +0x0c front    x,y,z
 *   +0x18 top      x,y,z    +0x24 velocity x,y,z
 *   +0x30 pointer to the 0x48-byte engine-side I3DL2 environment block
 *
 * Halo is Z-up and DirectSound is Y-up, so every vector goes out with
 * its Y and Z components swapped: (x, z, y).  This is verified from the
 * push/FSTP order at each call site, not from the decompiler's argument
 * list -- MSVC passes these floats as `PUSH <dummy>; FSTP [ESP]`.
 *
 * The values last actually sent are cached at 0x5053d0 (position),
 * 0x5053dc (front then top), 0x5053f4 (velocity) and 0x505404 (the whole
 * 0x48-byte environment block).  A delta smaller than the epsilon --
 * 0.05 for position and orientation, 0.01 for velocity -- is dropped,
 * but a clear settings-valid flag at 0x4fdbc0 forces every update to be
 * re-sent regardless.
 *
 * The DSI3DL2LISTENER handed to SetI3DL2Listener is built on the stack
 * from the environment block: the four gain fields go through
 * sound_dsound_gain_to_volume with per-field ceilings (0 for room and
 * roomHF, 1000 for reflections, 2000 for reverb), diffusion and density
 * are scaled by 100, and the remaining fields are copied verbatim.
 * Environment offsets +0x00 and +0x04 are cached but never sent. */
void FUN_001ca2b0(const float *params)
{
  /* DSI3DL2LISTENER (XDK, 0x30 bytes) -- the whole stack frame. */
  struct i3dl2_listener {
    int lRoom;
    int lRoomHF;
    float flRoomRolloffFactor;
    float flDecayTime;
    float flDecayHFRatio;
    int lReflections;
    float flReflectionsDelay;
    int lReverb;
    float flReverbDelay;
    float flDiffusion;
    float flDensity;
    float flHFReference;
  };
  /* Engine-side environment block; copied verbatim into the last-sent
   * cache by a single 0x12-dword move. */
  struct i3dl2_environment {
    int dwords[18];
  };

  struct i3dl2_listener listener;
  const char *environment;
  int result;

  if (!(fabs(params[0] - *(float *)0x5053d0) < 0.05f &&
        fabs(params[1] - *(float *)0x5053d4) < 0.05f &&
        fabs(params[2] - *(float *)0x5053d8) < 0.05f &&
        *(char *)0x4fdbc0 != 0)) {
    result = IDirectSound_SetPosition(*(void **)0x50545c, params[0], params[2],
                                      params[1], 1);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set listener position.");
    }
    *(float *)0x5053d0 = params[0];
    *(float *)0x5053d4 = params[1];
    *(float *)0x5053d8 = params[2];
  }

  if (!(fabs(params[3] - *(float *)0x5053dc) < 0.05f &&
        fabs(params[4] - *(float *)0x5053e0) < 0.05f &&
        fabs(params[5] - *(float *)0x5053e4) < 0.05f &&
        fabs(params[6] - *(float *)0x5053e8) < 0.05f &&
        fabs(params[7] - *(float *)0x5053ec) < 0.05f &&
        fabs(params[8] - *(float *)0x5053f0) < 0.05f &&
        *(char *)0x4fdbc0 != 0)) {
    result = IDirectSound_SetOrientation(*(void **)0x50545c, params[3],
                                         params[5], params[4], params[6],
                                         params[8], params[7], 1);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set listener orientation.");
    }
    *(float *)0x5053dc = params[3];
    *(float *)0x5053e0 = params[4];
    *(float *)0x5053e4 = params[5];
    *(float *)0x5053e8 = params[6];
    *(float *)0x5053ec = params[7];
    *(float *)0x5053f0 = params[8];
  }

  if (!(fabs(params[9] - *(float *)0x5053f4) < 0.01f &&
        fabs(params[10] - *(float *)0x5053f8) < 0.01f &&
        fabs(params[11] - *(float *)0x5053fc) < 0.01f &&
        *(char *)0x4fdbc0 != 0)) {
    result = IDirectSound_SetVelocity(*(void **)0x50545c, params[9], params[11],
                                      params[10], 1);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set listener velocity.");
    }
    *(float *)0x5053f4 = params[9];
    *(float *)0x5053f8 = params[10];
    *(float *)0x5053fc = params[11];
  }

  if (csmemcmp(*(const void **)((const char *)params + 0x30),
               (const void *)0x505404, 0x48) != 0 ||
      *(char *)0x4fdbc0 == 0) {
    environment = *(const char **)((const char *)params + 0x30);
    *(struct i3dl2_environment *)0x505404 =
      *(const struct i3dl2_environment *)environment;

    listener.lRoom =
      sound_dsound_gain_to_volume(*(const float *)(environment + 0x8), 0);
    listener.lRoomHF =
      sound_dsound_gain_to_volume(*(const float *)(environment + 0xc), 0);
    listener.flRoomRolloffFactor = *(const float *)(environment + 0x10);
    listener.flDecayTime = *(const float *)(environment + 0x14);
    listener.flDecayHFRatio = *(const float *)(environment + 0x18);
    listener.lReflections =
      sound_dsound_gain_to_volume(*(const float *)(environment + 0x1c), 1000);
    listener.flReflectionsDelay = *(const float *)(environment + 0x20);
    listener.lReverb =
      sound_dsound_gain_to_volume(*(const float *)(environment + 0x24), 2000);
    listener.flReverbDelay = *(const float *)(environment + 0x28);
    listener.flDiffusion = *(const float *)(environment + 0x2c) * 100.0f;
    listener.flDensity = 100.0f;
    listener.flDensity =
      *(const float *)(environment + 0x30) * listener.flDensity;
    listener.flHFReference = *(const float *)(environment + 0x34);
    IDirectSound_SetI3DL2Listener(*(void **)0x50545c, &listener, 1);
  }
}

/* sound_dsound_update_channel_properties (0x1ca5e0)
 *
 * Apply volume, pitch, and 3D spatial properties to a hardware dsound
 * channel.  Called from sound_dsound_set_channel_properties after the
 * virtual-to-actual channel has been resolved.
 *
 * Volume is always updated (subject to an epsilon check).  Pitch and
 * 3D properties (min/max distance, cone angles, cone outside volume,
 * and the full 3D parameter set) are only updated when update_only==0.
 *
 * Each property is compared against the value cached in the channel
 * struct; if the delta exceeds a per-property epsilon AND a global
 * "initialized" flag (0x4fdbc0) is set, the DirectSound call is
 * skipped.  When the flag is clear, all properties are pushed
 * unconditionally.
 *
 * properties layout (float[8]):
 *   [0] min_distance
 *   [1] max_distance
 *   [2] pitch
 *   [3] gain
 *   [4] inner_cone_angle (radians)
 *   [5] outer_cone_angle (radians)
 *   [6] cone_outside_gain
 *   [7] field_1c (3D-related scalar)
 *
 * Cone angles are converted from radians to degrees (multiplied by
 * 180/pi) and truncated to int before passing to SetConeAngles.
 * Volume values (gain, cone_outside_gain) are converted to hundredths
 * of dB via sound_dsound_gain_to_volume. */
void sound_dsound_update_channel_properties(float *properties,
                                            short channel_index,
                                            int update_only)
{
  void *channel;
  float gain;
  int volume;
  int result;

  channel = sound_dsound_channel_get(channel_index);
  gain = *(float *)0x505488 * properties[3];

  /* assert: properties->gain in [0, 1] */
  if (properties[3] < 0.0f || properties[3] > 1.0f) {
    display_assert("properties->gain>=0.f && properties->gain<=1.f",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x3d4, 1);
    system_exit(-1);
  }
  /* assert: dsound_globals.pause_gain in [0, 1] */
  if (*(float *)0x505488 < 0.0f || *(float *)0x505488 > 1.0f) {
    display_assert(
      "dsound_globals.pause_gain>=0 && dsound_globals.pause_gain<=1.f",
      "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x3d5, 1);
    system_exit(-1);
  }
  /* assert: channel->stream is valid */
  if (*(void **)((char *)channel + 0x70) == 0) {
    display_assert("channel->stream",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x3d6, 1);
    system_exit(-1);
  }

  /* -- volume -- */
  if (fabsf(gain - *(float *)((char *)channel + 0x3c)) >=
        (float)*(double *)0x2549d8 ||
      *(char *)0x4fdbc0 == 0) {
    volume = sound_dsound_gain_to_volume(gain, 0);
    result =
      IDirectSoundStream_SetVolume(*(void **)((char *)channel + 0x70), volume);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel volume.");
    }
    *(float *)((char *)channel + 0x3c) = gain;
  }

  if (update_only != 0)
    goto done;

  /* -- pitch -- */
  if (fabsf(properties[2] - *(float *)((char *)channel + 0x40)) >=
        (float)*(double *)0x2549d8 ||
      *(char *)0x4fdbc0 == 0) {
    int sample_rate;
    int frequency;
    sample_rate = sound_dsound_get_sample_rate(
      (*(unsigned char *)((char *)channel + 0x38) >> 2) & 1);
    frequency = sound_dsound_pitch_to_frequency(sample_rate, properties[2]);
    result = IDirectSoundStream_SetFrequency(*(void **)((char *)channel + 0x70),
                                             frequency);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel pitch.");
    }
    *(float *)((char *)channel + 0x40) = properties[2];
  }

  /* -- 3D properties (only if channel has 3D flag) -- */
  if ((*(unsigned char *)((char *)channel + 0x38) & 1) == 0)
    goto done;

  /* max distance */
  if (fabsf(properties[1] - *(float *)((char *)channel + 0x50)) >=
        (float)*(double *)0x25f0c8 ||
      *(char *)0x4fdbc0 == 0) {
    result = IDirectSoundStream_SetMaxDistance(
      *(void **)((char *)channel + 0x70), properties[1], 1);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel max distance.");
    }
    *(float *)((char *)channel + 0x50) = properties[1];
  }

  /* min distance */
  if (fabsf(properties[0] - *(float *)((char *)channel + 0x4c)) >=
        (float)*(double *)0x25f0c8 ||
      *(char *)0x4fdbc0 == 0) {
    result = IDirectSoundStream_SetMinDistance(
      *(void **)((char *)channel + 0x70), properties[0], 1);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel min distance.");
    }
    *(float *)((char *)channel + 0x4c) = properties[0];
  }

  /* cone angles (radians -> degrees -> int) */
  if (fabsf(properties[4] - *(float *)((char *)channel + 0x58)) >=
        (float)*(double *)0x2c0eb0 ||
      fabsf(properties[5] - *(float *)((char *)channel + 0x5c)) >=
        (float)*(double *)0x2c0eb0 ||
      *(char *)0x4fdbc0 == 0) {
    int inner_deg = (int)(properties[4] * *(float *)0x2b073c);
    int outer_deg = (int)(properties[5] * *(float *)0x2b073c);
    result = IDirectSoundStream_SetConeAngles(
      *(void **)((char *)channel + 0x70), inner_deg, outer_deg, 0);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel cone angles.");
    }
    *(float *)((char *)channel + 0x58) = properties[4];
    *(float *)((char *)channel + 0x5c) = properties[5];
  }

  /* cone outside volume */
  if (fabsf(properties[6] - *(float *)((char *)channel + 0x54)) >=
        (float)*(double *)0x2549d8 ||
      *(char *)0x4fdbc0 == 0) {
    volume = sound_dsound_gain_to_volume(properties[6], 0);
    result = IDirectSoundStream_SetConeOutsideVolume(
      *(void **)((char *)channel + 0x70), volume, 1);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel cone volume.");
    }
    *(float *)((char *)channel + 0x54) = properties[6];
  }

  /* 3D parameter update (field_1c) */
  if (fabsf(properties[7] - *(float *)((char *)channel + 0x60)) >=
        (float)*(double *)0x2549d8 ||
      *(char *)0x4fdbc0 == 0) {
    *(float *)((char *)channel + 0x60) = properties[7];
    sound_dsound_channel_update_3d(channel_index);
  }

done:
  return;
}

/* FUN_001ca900 (0x1ca900)
 *
 * Keep an actual channel's stream fed: while the int16 at channel+0x08 is
 * below 4 and the dword at channel+0x68 is non-zero, query the stream status
 * through IDirectSoundStream vtable slot 3 (byte offset 0x0c; this and
 * &status pushed, no ADD ESP -> __stdcall) and queue another packet with
 * dsound_channel_queue_packet while bit 0 of the status is set.  A failing
 * HRESULT (JL) goes to sound_dsound_set_last_error with
 * "couldn't get channel status." and ends the loop; a clear status bit or a
 * false queue result ends it silently.
 * channel_index arrives in EAX (MOV ESI,EAX at 0x1ca908 before any write). */
void FUN_001ca900(short channel_index)
{
  char *channel;
  void *stream;
  int hresult;
  unsigned int status;

  channel = (char *)sound_dsound_channel_get(channel_index);
  while (*(short *)(channel + 0x8) < 4 && *(int *)(channel + 0x68) != 0) {
    stream = *(void **)(channel + 0x70);
    hresult = ((int(__stdcall *)(void *, unsigned int *))(
      *(void ***)stream)[3])(stream, &status);
    if (hresult < 0) {
      sound_dsound_set_last_error(&hresult, "couldn't get channel status.");
      return;
    }
    if (!(status & 0x1))
      return;
    if (!dsound_channel_queue_packet(channel_index))
      return;
  }
}

/* FUN_001ca970 (0x1ca970)
 *
 * Stream packet-completion callback (RET 0xc -> __stdcall, three stack
 * args).  Arg 1 is an actual-channel index (tested as SI), arg 2 is handed
 * unchanged to FUN_001be140, arg 3 is the packet status HRESULT.
 * An out-of-range index only appends "trying to queue sound to invalid
 * channel." to the reason accumulator (inlined append, no hresult store).
 * A status other than 0 / 0x80004004 records a reason through
 * sound_dsound_set_last_error with a NULL hresult (XOR EAX,EAX) and
 * returns: 0x80004005 "failure", 0x8000000a "pending", else "undefined".
 * Otherwise the permutation reference is released, the int16 count at
 * channel+0x08 is decremented and, unless the byte at 0x505484 is set,
 * a zero count clears the int16 at channel+0x00 while a non-zero count
 * refills the stream via FUN_001ca900 unless the status was 0x80004004. */
void __stdcall FUN_001ca970(short channel_index, int permutation_ptr,
                            int status)
{
  char *channel;

  if (channel_index < 0 || channel_index >= *(short *)0x4fdfc4) {
    if ((unsigned int)(csstrlen(SOUND_DSOUND_ERROR_REASON) +
                       csstrlen("trying to queue sound to invalid channel.")) <
        0x100) {
      crt_sprintf(SOUND_DSOUND_ERROR_REASON +
                    csstrlen(SOUND_DSOUND_ERROR_REASON),
                  "trying to queue sound to invalid channel.");
    }
    return;
  }

  channel = (char *)sound_dsound_channel_get(channel_index);
  if (status != 0 && status != (int)0x80004004) {
    if (status == (int)0x80004005) {
      sound_dsound_set_last_error(NULL, "status is failure.");
      return;
    }
    if (status == (int)0x8000000a) {
      sound_dsound_set_last_error(NULL, "status is pending.");
      return;
    }
    sound_dsound_set_last_error(NULL, "status is undefined.");
    return;
  }

  FUN_001be140(permutation_ptr);
  *(short *)(channel + 0x8) -= 1;
  if (*(char *)0x505484 == 0) {
    if (*(short *)(channel + 0x8) == 0) {
      *(short *)channel = 0;
      return;
    }
    if (status != (int)0x80004004) {
      FUN_001ca900(channel_index);
    }
  }
}

/* sound_dsound_set_channel_properties (0x1caa80)
 *
 * Vtable+0x34 entry point for the DirectSound driver.  Resolves the
 * dsound channel index via sound_dsound_channel_resolve, then forwards
 * to the real property-update function. */
void sound_dsound_set_channel_properties(int channel_index, float *properties,
                                         int update_only)
{
  short channel;

  channel = sound_dsound_channel_resolve(channel_index);
  if (channel != -1) {
    sound_dsound_update_channel_properties(properties, channel, update_only);
  }
}

void FUN_001caab0(char paused)
{
  short channel_index;
  void *channel;
  void *stream;
  void **vtable;
  short flags;
  float a;
  float b;
  int scale;
  int cursor;
  int active;
  bool finished;

  if (paused == *(char *)0x505484) {
    display_assert("paused!=dsound_globals.paused",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x2d8, 1);
    system_exit(-1);
  }

  channel_index = 0;
  if (paused != 0) {
    /* pause: drain any channel that is still stopping */
    if (0 < *(short *)0x4fdfc4) {
      do {
        if (channel_index < 0 || channel_index >= *(short *)0x4fdfc4) {
          display_assert(
            "index>=0 && index<dsound_globals.actual_channel_count",
            "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
          system_exit(-1);
        }
        if (channel_index >= 0x100) {
          display_assert("index<MAXIMUM_SOUND_CHANNELS",
                         "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a,
                         1);
          system_exit(-1);
        }
        channel = (void *)(0x4fdfc8 + (int)channel_index * 0x74);

        if (*(char *)((char *)channel + 6) != 0) {
          do {
            if (channel_index < 0 || channel_index >= *(short *)0x4fdfc4) {
              display_assert(
                "index>=0 && index<dsound_globals.actual_channel_count",
                "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
              system_exit(-1);
            }
            if (channel_index >= 0x100) {
              display_assert("index<MAXIMUM_SOUND_CHANNELS",
                             "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c",
                             0x6a, 1);
              system_exit(-1);
            }
            channel = (void *)(0x4fdfc8 + (int)channel_index * 0x74);

            if (*(char *)((char *)channel + 6) == 0) {
              display_assert("channel->stopping",
                             "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c",
                             0x4b8, 1);
              system_exit(-1);
            }
            active =
              dsound_stream_is_active(*(void **)((char *)channel + 0x70));
            finished = (active == 0);
          } while (!finished);

          stream = *(void **)((char *)channel + 0x70);
          vtable = *(void ***)stream;
          ((int(__stdcall *)(void *))vtable[6])(stream);
          *(char *)((char *)channel + 6) = 0;
        }
        channel_index = (short)(channel_index + 1);
      } while (channel_index < *(short *)0x4fdfc4);
    }
  } else {
    /* pass 1: stop every live stream and rewind its cursor */
    if (0 < *(short *)0x4fdfc4) {
      do {
        if (channel_index < 0 || channel_index >= *(short *)0x4fdfc4) {
          display_assert(
            "index>=0 && index<dsound_globals.actual_channel_count",
            "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
          system_exit(-1);
        }
        if (channel_index >= 0x100) {
          display_assert("index<MAXIMUM_SOUND_CHANNELS",
                         "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a,
                         1);
          system_exit(-1);
        }
        channel = (void *)(0x4fdfc8 + (int)channel_index * 0x74);

        if (*(short *)channel != 0) {
          stream = *(void **)((char *)channel + 0x70);
          vtable = *(void ***)stream;
          ((int(__stdcall *)(void *))vtable[6])(stream);

          flags = *(short *)((char *)channel + 0x38);
          a = (flags & 4) ? 2.0f : 1.0f;
          b = (flags & 2) ? 2.0f : 1.0f;
          scale = (flags & 8) ? 0x900 : 0x2000;
          cursor = (int)((float)*(int *)((char *)channel + 0x64) -
                         (float)scale * b * a * 4.0f);
          *(int *)((char *)channel + 0x64) = cursor;
          *(float *)((char *)channel + 0x3c) = 0.0f;
          cursor = (cursor < 0) ? 0 : cursor;
          *(int *)((char *)channel + 0x64) = cursor;
        }
        channel_index = (short)(channel_index + 1);
      } while (channel_index < *(short *)0x4fdfc4);
    }

    /* pass 2 */
    DirectSoundDoWork();

    /* pass 3: flush the save-and-quit flag and re-kick live channels */
    channel_index = 0;
    if (0 < *(short *)0x4fdfc4) {
      do {
        if (channel_index < 0 || channel_index >= *(short *)0x4fdfc4) {
          display_assert(
            "index>=0 && index<dsound_globals.actual_channel_count",
            "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x69, 1);
          system_exit(-1);
        }
        if (channel_index >= 0x100) {
          display_assert("index<MAXIMUM_SOUND_CHANNELS",
                         "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x6a,
                         1);
          system_exit(-1);
        }
        channel = (void *)(0x4fdfc8 + (int)channel_index * 0x74);

        if (*(short *)((char *)channel + 8) != 0) {
          error(2, "DirectSound: you're screwed if you try to save and quit -- "
                   "the devil.");
          *(short *)((char *)channel + 8) = 0;
        }
        if (*(short *)channel != 0) {
          FUN_001ca900(channel_index);
        }
        channel_index = (short)(channel_index + 1);
      } while (channel_index < *(short *)0x4fdfc4);
    }
  }

  *(char *)0x505484 = paused;
}

/* dsound_channel_set_location (0x1cadd0)
 *
 * Push a 3D channel's position / forward / velocity and two cached scalars
 * down to its DirectSound stream, skipping each sub-update whose inputs have
 * not moved past an epsilon since the last send (same pattern as the
 * listener update FUN_001ca2b0 above).
 *
 * Confirmed (disasm 0x1cadd0-0x1cb0b1):
 *   - `location` arrives in EBX (read at 0x1cae77 with EBX never written).
 *     It is a 9-float block: [0..2] position, [3..5] forward (assert text
 *     "valid_real_normal3d(&location->forward)" at line 0x3a5 on
 *     &location[3]), [6..8] velocity.
 *   - channel_index is [EBP+8]; it is handed to sound_dsound_channel_get in
 *     ESI and to sound_dsound_channel_update_3d in EAX.
 *   - Asserts at lines 0x38a / 0x38b: channel+0x38 bit 0 (3D channel) and
 *     channel+0x70 (stream) non-null.
 *   - SetMode(stream, spatialized ? 0 : 2, 1) (SETNE/DEC/AND 2) runs unless
 *     channel+0x4 already equals the argument and the "settings valid" byte
 *     0x4fdbc0 is set; the byte is stored and mode_changed set BEFORE the
 *     HRESULT test.
 *   - Epsilons are qword constants 0x25f0c8 / 0x28b800 / 0x2549d8 holding the
 *     float-rounded values 0.05f / 0.01f / 0.001f; the compare is
 *     FABS / FCOMP / TEST AH,5 / JP -> "fabs(d) < eps" keeps the skip.
 *   - Every vector is pushed (x, z, y): FSTP [ESP+4] gets element 1,
 *     FSTP [ESP] element 2, then element 0 is PUSHed as a dword.
 *   - Call targets: 0x205350 SetPosition, 0x2052ed SetConeOrientation,
 *     0x205379 SetVelocity (kb XDK names); all stdcall (no ADD ESP).
 *   - Last block: skipped only when |param_4 - ch+0x44| < 0.001,
 *     |param_5 - ch+0x48| < 0.001 (FSUBR), ch+0x5 == param_6, no mode change
 *     and 0x4fdbc0 set; otherwise stores ch+0x5, +0x44, +0x48 and calls
 *     sound_dsound_channel_update_3d(channel_index).
 * Uncertain: meanings of param_4/param_5/param_6 (see kb_meta notes). */
void dsound_channel_set_location(short channel_index, char spatialized,
                                 float *location, float param_4, float param_5,
                                 char param_6)
{
  char *channel;
  int result;
  char mode_changed;

  channel = (char *)sound_dsound_channel_get(channel_index);
  mode_changed = 0;
  assert_halt_msg_at("TEST_FLAG(channel->type_flags, _sound_channel_3d_bit)",
                     "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x38a,
                     (*(unsigned char *)(channel + 0x38) & 1) != 0);
  assert_halt_msg_at("channel->stream",
                     "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x38b,
                     *(void **)(channel + 0x70) != 0);

  if (!(*(char *)(channel + 0x4) == spatialized && *(char *)0x4fdbc0 != 0)) {
    result = IDirectSoundStream_SetMode(*(void **)(channel + 0x70),
                                        spatialized ? 0 : 2, 1);
    *(char *)(channel + 0x4) = spatialized;
    mode_changed = 1;
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel spatialization.");
    }
  }

  if (!(fabs(location[0] - *(float *)(channel + 0x0c)) < 0.05f &&
        fabs(location[1] - *(float *)(channel + 0x10)) < 0.05f &&
        fabs(location[2] - *(float *)(channel + 0x14)) < 0.05f &&
        *(char *)0x4fdbc0 != 0)) {
    result = IDirectSoundStream_SetPosition(
      *(void **)(channel + 0x70), location[0], location[2], location[1], 1);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel position.");
    }
    *(float *)(channel + 0x0c) = location[0];
    *(float *)(channel + 0x10) = location[1];
    *(float *)(channel + 0x14) = location[2];
  }

  if (!(fabs(location[3] - *(float *)(channel + 0x18)) < 0.05f &&
        fabs(location[4] - *(float *)(channel + 0x1c)) < 0.05f &&
        fabs(location[5] - *(float *)(channel + 0x20)) < 0.05f &&
        *(char *)0x4fdbc0 != 0)) {
    assert_halt_msg_at("valid_real_normal3d(&location->forward)",
                       "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x3a5,
                       valid_real_normal3d(&location[3]));
    result = IDirectSoundStream_SetConeOrientation(
      *(void **)(channel + 0x70), location[3], location[5], location[4], 1);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel orientation.");
    }
    *(float *)(channel + 0x18) = location[3];
    *(float *)(channel + 0x1c) = location[4];
    *(float *)(channel + 0x20) = location[5];
  }

  if (!(fabs(location[6] - *(float *)(channel + 0x24)) < 0.01f &&
        fabs(location[7] - *(float *)(channel + 0x28)) < 0.01f &&
        fabs(location[8] - *(float *)(channel + 0x2c)) < 0.01f &&
        *(char *)0x4fdbc0 != 0)) {
    result = IDirectSoundStream_SetVelocity(
      *(void **)(channel + 0x70), location[6], location[8], location[7], 1);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't set channel velocity.");
    }
    *(float *)(channel + 0x24) = location[6];
    *(float *)(channel + 0x28) = location[7];
    *(float *)(channel + 0x2c) = location[8];
  }

  if (!(fabs(param_4 - *(float *)(channel + 0x44)) < 0.001f &&
        fabs(param_5 - *(float *)(channel + 0x48)) < 0.001f &&
        *(char *)(channel + 0x5) == param_6 && mode_changed == 0 &&
        *(char *)0x4fdbc0 != 0)) {
    *(char *)(channel + 0x5) = param_6;
    *(float *)(channel + 0x44) = param_4;
    *(float *)(channel + 0x48) = param_5;
    sound_dsound_channel_update_3d(channel_index);
  }
}

/* FUN_001cb0c0 (0x1cb0c0)
 *
 * Attaches a sound to a DirectSound channel and advances the channel's
 * state machine.  The channel index arrives on the stack; the sound
 * pointer arrives in EDI (LTCG register argument -- see kb.json).
 *
 * The channel state word at +0x00 has three legal values:
 *   0 -> the channel is idle.  Claim it (state := 1), record the sound at
 *        +0x68, clear +0x64 and the int16 at +0x08, flush the deferred
 *        DirectSound settings and run the follow-up pass at 0x1ca900
 *        (a tail call: the original hands channel_index over in EAX).
 *   1 -> already claimed; promote to state 2 and fall through.
 *   2 -> record the sound at +0x6c.
 * Anything else asserts.
 *
 * Store offsets are taken from the disassembly (MOV [ESI+N]); the
 * decompiler's short-index arithmetic split the single dword store at
 * +0x64 into two int16 stores.  channel_index is short: callers push it
 * without MOVSX and the tail call reloads it with a plain dword MOV into
 * EAX. */
void FUN_001cb0c0(short channel_index, void *sound)
{
  void *channel;
  int result;

  channel = sound_dsound_channel_get(channel_index);

  if (*(char *)0x505484 != 0) {
    display_assert("!dsound_globals.paused",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x457, 1);
    system_exit(-1);
  }
  if (sound == NULL) {
    display_assert("sound", "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c",
                   0x458, 1);
    system_exit(-1);
  }

  switch (*(short *)channel) {
  case 0:
    *(short *)channel = 1;
    *(void **)((char *)channel + 0x68) = sound;
    *(int *)((char *)channel + 0x64) = 0;
    *(short *)((char *)channel + 8) = 0;
    result = IDirectSound_CommitDeferredSettings(*(void **)0x50545c);
    if (result < 0) {
      sound_dsound_log_error(result, "couldn't commit deferred settings.");
    }
    FUN_001ca900(channel_index);
    break;

  case 1:
    *(short *)channel = 2;
    /* fall through */
  case 2:
    *(void **)((char *)channel + 0x6c) = sound;
    break;

  default:
    display_assert("bad DirectSound channel state.",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x478, 1);
    system_exit(-1);
    break;
  }
}

/* 0x20f069 -- XDK DirectSound stream inline (stdcall, RET 4).
 * Loads the object pointer at stream+0x24 and tests its dword at +0x8
 * against mask 0x10000002; NEG/SBB/NEG materializes 0/1 in EAX.
 * The meaning of the individual status bits is unconfirmed.
 *
 * The status dword is written asynchronously by the DirectSound runtime,
 * and the stop-wait loops in FUN_001ca130/FUN_001caab0 poll it until it
 * clears.  It must be read through a volatile lvalue: with a plain load
 * clang inlines this body into those loops, hoists the read, and emits
 * `jmp $` (save-and-quit freeze, see b0b676c1e).  One volatile load is
 * the same single MOV the original performs. */
__declspec(noinline) bool __stdcall dsound_stream_is_active(void *stream)
{
  return (*(volatile unsigned int *)(*(char **)((char *)stream + 0x24) + 0x8) &
          0x10000002) != 0;
}

/* 0x20f081 -- XDK DirectSound stream inline (stdcall, RET 4).
 * MOV EAX,[ESP+4]; MOV ECX,[EAX+0x24]; MOV EAX,[ECX]; PUSH 0; PUSH 0;
 * CALL [EAX+0x10]: a thiscall through vtable slot 4 (+0x10) of the object
 * at stream+0x24, with ECX = that object and two zero stack args (callee
 * cleans).  C89/VC71 cannot spell an explicit __thiscall pointer, so the
 * call is expressed as __fastcall with an unused EDX slot: ECX = object,
 * identical stack args and callee cleanup.  The method's meaning is
 * unconfirmed. */
typedef void(__fastcall *dsound_stream_object_method4_t)(void *object,
                                                         int unused_edx,
                                                         int arg0, int arg1);

__declspec(noinline) void __stdcall FUN_0020f081(void *stream)
{
  void *object;

  object = *(void **)((char *)stream + 0x24);
  ((dsound_stream_object_method4_t)(*(void ***)object)[4])(object, 0, 0, 0);
}

/* dsound_initialize_channel (0x1cb210)
 *
 * Creates the DirectSound stream for one actual channel.  type_flags
 * arrives in AX (the caller loads it from the per-type table at 0x32fcf8)
 * and is stored as the channel's type flags at +0x38; channel_index is the
 * only stack argument.  Bits used here: 0 = 3D channel, 1 = stereo,
 * 2 = sample-rate index, 3 = compressed (Xbox ADPCM, format tag 0x69).
 *
 *   1. Reset the channel record (sound_dsound_channel_get): +0x02 = NONE,
 *      +0x06 = 0, the dwords at +0x68/+0x6c = 0.
 *   2. Fill the stream format: PCM at the rate in 0x2bcc1c, 16-bit stereo,
 *      or ADPCM 4-bit with 1/2 channels, block align 36 * channels,
 *      64 samples per block.
 *   3. IDirectSound_CreateSoundStream with a 0x18-byte stream description
 *      (flags 0x10 when 3D, 4 packets, the format, callback FUN_001ca970
 *      and channel_index as context) into the stream slot at +0x70.
 *      Failure logs and returns FALSE.
 *   4. 3D channels get an initial location (zero position/velocity, the
 *      global forward vector from *0x31fc3c) via
 *      dsound_channel_set_location.  Other channels get mix bins picked by
 *      the speaker config (bit 0x10000) and the stereo bit, with volumes
 *      from sound_dsound_gain_to_volume; stereo without bit 0x10000 sets
 *      nothing (the original jumps past the single SetMixBins call site,
 *      so the goto keeps that shape).
 *   5. Push properties {1.0, 1.0, 0...} through
 *      sound_dsound_update_channel_properties and return TRUE.
 *
 * The channel record has no struct yet; offsets are the ones touched
 * here and in this TU. */
boolean dsound_initialize_channel(short type_flags, short channel_index)
{
  /* Stream properties block for sound_dsound_update_channel_properties. */
  float properties[8];
  /* XDK DSSTREAMDESC (0x18 bytes). */
  struct {
    unsigned int flags;
    unsigned int max_attached_packets;
    void *format;
    void (*callback)(void);
    int context;
    unsigned int field_14;
  } desc;
  /* XDK XBOXADPCMWAVEFORMAT / WAVEFORMATEX (0x14 bytes). */
  struct {
    unsigned short format_tag;
    unsigned short channels;
    unsigned int samples_per_sec;
    unsigned int avg_bytes_per_sec;
    unsigned short block_align;
    unsigned short bits_per_sample;
    unsigned short cb_size;
    unsigned short samples_per_block;
  } format;
  unsigned int speaker_config;
  char *channel;
  unsigned int mix_bins;
  int hr;
  boolean success;

  channel = (char *)sound_dsound_channel_get(channel_index);
  *(short *)(channel + 0x38) = type_flags;
  *(short *)(channel + 0x2) = -1;
  *(char *)(channel + 0x6) = 0;
  *(int *)(channel + 0x68) = 0;
  *(int *)(channel + 0x6c) = 0;

  if (!(type_flags & 8)) {
    format.samples_per_sec = *(unsigned int *)0x2bcc1c;
    format.format_tag = 1;
    format.bits_per_sample = 16;
    format.channels = 2;
    format.block_align = 4;
    format.avg_bytes_per_sec = format.samples_per_sec * format.block_align;
  } else {
    format.format_tag = 0x69;
    format.channels = (type_flags & 2) ? 2 : 1;
    format.bits_per_sample = 4;
    format.block_align = format.channels * 36;
    format.samples_per_sec =
      sound_dsound_get_sample_rate((short)((type_flags >> 2) & 1));
    format.avg_bytes_per_sec = format.samples_per_sec / 64 * format.block_align;
    format.cb_size = 2;
    format.samples_per_block = 64;
  }

  csmemset(&desc, 0, sizeof(desc));
  desc.flags = 0;
  desc.max_attached_packets = 4;
  desc.format = &format;
  desc.callback = (void (*)(void))FUN_001ca970;
  desc.context = channel_index;
  if (type_flags & 1) {
    desc.flags = 0x10;
  }

  hr = IDirectSound_CreateSoundStream(*(void **)0x50545c, &desc,
                                      (void **)(channel + 0x70), NULL);
  if (hr >= 0) {
    if (type_flags & 1) {
      /* dsound_channel_set_location location block (0x2c bytes). */
      struct {
        float position[3];
        float forward[3];
        float velocity[3];
        char pad_24[8];
      } location;
      const float *forward;

      csmemset(&location, 0, sizeof(location));
      forward = *(const float **)0x31fc3c;
      location.forward[0] = forward[0];
      location.forward[1] = forward[1];
      location.forward[2] = forward[2];
      dsound_channel_set_location(channel_index, 0, (float *)&location, 0.0f,
                                  0.0f, 0);
    } else {
      int volumes[6];

      IDirectSound_GetSpeakerConfig(*(void **)0x50545c, &speaker_config);
      if (speaker_config & 0x10000) {
        if (!(type_flags & 2)) {
          mix_bins = 7;
          volumes[0] = sound_dsound_gain_to_volume(0.5f, 0);
          volumes[1] = sound_dsound_gain_to_volume(0.5f, 0);
          volumes[2] = sound_dsound_gain_to_volume(0.5f, 0);
        } else {
          mix_bins = 0x1833;
          volumes[0] = sound_dsound_gain_to_volume(1.0f, 0);
          volumes[1] = sound_dsound_gain_to_volume(1.0f, 0);
          volumes[2] = sound_dsound_gain_to_volume(0.5f, 0);
          volumes[3] = sound_dsound_gain_to_volume(0.5f, 0);
          volumes[4] = sound_dsound_gain_to_volume(0.5f, 0);
          volumes[5] = sound_dsound_gain_to_volume(0.5f, 0);
        }
      } else {
        if (type_flags & 2) {
          goto mix_bins_done;
        }
        mix_bins = 3;
        volumes[0] = sound_dsound_gain_to_volume(0.5f, 0);
        volumes[1] = sound_dsound_gain_to_volume(0.5f, 0);
      }
      IDirectSoundStream_SetMixBins(*(void **)(channel + 0x70), mix_bins);
      IDirectSoundStream_SetMixBinVolumes_12(*(void **)(channel + 0x70),
                                             mix_bins, volumes);
    mix_bins_done:;
    }

    success = true;
    csmemset(properties, 0, sizeof(properties));
    properties[0] = 1.0f;
    properties[1] = 1.0f;
    sound_dsound_update_channel_properties(properties, channel_index, 0);
  } else {
    sound_dsound_log_error(hr, "couldn't create sound stream.");
    success = false;
  }

  return success;
}

/* dsound_initialize (0x1cb4c0)
 *
 * Binary: [EBP+8] is a pointer asserted non-NULL as "preferences"
 * (sound_dsound_xbox.c line 0xea); AL is the return value.  Creates the
 * DirectSound object into 0x50545c, copies its caps into 0x50544c..0x505458,
 * sets distance factor 3.048f and rolloff 1.0f, downloads the effects image
 * (0x2bccf0, 0x3a5c bytes; failure is logged but not fatal), then
 * initializes virtual channels (per-type counts at preferences[5..8]) and
 * dsound channels (per-type counts at preferences[1..4], flags from the
 * short table at 0x32fcf8).  The 0x34-byte block passed to FUN_001ca2b0
 * and the preferences layout beyond these reads are UNKNOWN. */
boolean dsound_initialize(short *preferences)
{
  uint32_t params[13];
  uint32_t caps[4];
  void *image_desc;
  uint32_t image_loc[2];
  const uint32_t *source;
  short *vchannel;
  short virtual_index;
  short channel_index;
  short type_index;
  short count_index;
  boolean success;
  int result;

  success = false;
  *(char *)0x4fdbc0 = false;
  *(char *)0x505484 = false;
  *(float *)0x505488 = 1.0f;
  *(void **)0x505460 = NULL;
  if (preferences == NULL) {
    display_assert("preferences",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0xea, 1);
    system_exit(-1);
  }

  result = DirectSoundCreate(NULL, (void **)0x50545c, NULL);
  if (result >= 0) {
    result = IDirectSound_GetCaps(*(void **)0x50545c, caps);
    if (result >= 0) {
      *(uint32_t *)0x50544c = caps[0];
      *(uint32_t *)0x505450 = caps[1];
      *(uint32_t *)0x505454 = caps[2];
      *(uint32_t *)0x505458 = caps[3];
      result = IDirectSound_SetDistanceFactor(*(void **)0x50545c, 3.048f, 0);
      if (result >= 0) {
        result = IDirectSound_SetRolloffFactor(*(void **)0x50545c, 1.0f, 0);
        if (result >= 0) {
          csmemset(params, 0, 0x34);
          source = *(const uint32_t **)0x31fc3c;
          params[3] = source[0];
          params[4] = source[1];
          params[5] = source[2];
          source = *(const uint32_t **)0x31fc44;
          params[6] = source[0];
          params[7] = source[1];
          params[8] = source[2];
          params[12] = 0x2c1220;
          image_loc[0] = 0;
          image_loc[1] = 1;
          result = IDirectSound_DownloadEffectsImage(
            *(void **)0x50545c, (const void *)0x2bccf0, 0x3a5c, image_loc,
            &image_desc);
          if (result < 0) {
            sound_dsound_log_error(result, "could not download effects image.");
          }
          IDirectSound_SetMixBinHeadroom(*(void **)0x50545c, 0x7fffffff, 0);
          DirectSoundUseFullHRTF();
          FUN_001ca2b0((const float *)params);

          virtual_index = 0;
          success = true;
          for (type_index = 0; type_index < 4; type_index++) {
            for (count_index = 0; count_index < preferences[5 + type_index];
                 count_index++) {
              (*(short *)0x4fdbc2)++;
              if (success) {
                vchannel = (short *)sound_dsound_vchannel_get(virtual_index);
                if (type_index < 0 || type_index >= 4) {
                  display_assert(
                    "type_index>=0 && type_index<NUMBER_OF_SOUND_CHANNEL_TYPES",
                    "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x1a6, 1);
                  system_exit(-1);
                }
                vchannel[1] = type_index;
                vchannel[0] = -1;
                virtual_index++;
                success = true;
              } else {
                success = false;
              }
            }
          }

          channel_index = 0;
          for (type_index = 0; type_index < 4; type_index++) {
            ((short *)0x5053c8)[type_index] = channel_index;
            for (count_index = 0; count_index < preferences[1 + type_index];
                 count_index++) {
              (*(short *)0x4fdfc4)++;
              success =
                success && dsound_initialize_channel(
                             ((short *)0x32fcf8)[type_index], channel_index++);
            }
          }

          success = success && dsound_fix_rear_speakers();
        } else {
          sound_dsound_log_error(result, "could not adjust rolloff factor");
        }
      } else {
        sound_dsound_log_error(result, "could not adjust distance factor");
      }
    } else {
      sound_dsound_log_error(result, "could not get caps for sound card?");
    }
  } else {
    sound_dsound_log_error(result, "could not create direct sound object");
  }

  if (success) {
    *(char *)0x4fdbc0 = true;
  } else {
    FUN_001c93f0();
  }
  return success;
}
