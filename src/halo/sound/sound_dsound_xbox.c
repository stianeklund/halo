/* sound_dsound_get_sample_rate (0x1c90e0)
 *
 * Return the sample rate for the given codec index.  Index 0 yields
 * 22050 Hz, index 1 yields 44100 Hz.  Asserts codec_index is in the
 * range [0, NUMBER_OF_SOUND_SAMPLE_RATES=2). */
int sound_dsound_get_sample_rate(int codec_index)
{
  if (codec_index < 0 || codec_index >= 2) {
    display_assert("sample_rate>=0 && sample_rate<NUMBER_OF_SOUND_SAMPLE_RATES",
                   "c:\\halo\\source\\sound\\sound_definitions.h", 0x135, 1);
    system_exit(-1);
  }
  return *(int *)((char *)0x2bcc18 + codec_index * 4);
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

  if (gain < 0.0f || gain > 1.0f) {
    display_assert("gain>=0.f && gain<=1.f",
                   "c:\\halo\\source\\sound\\sound_dsound.h", 0x23, 1);
    system_exit(-1);
  }

  if (gain == 0.0f)
    return -10000;

  volume = (int)(*(double *)0x2c07b8 * log10(gain) + ceiling);
  if (volume < -10000)
    return -10000;
  if (volume > ceiling)
    volume = ceiling;
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

  assert_halt(*(char *)((char *)channel + 0x6) != 0);

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
  static char buffer[0x1000];
  const char *error_name;
  char *arglist;

  arglist = (char *)&message + 4;
  vsprintf(buffer, message, arglist);

  error_name = "<unknown error>";

  if (hresult > (int)0x8007000E) {
    if (hresult == (int)0x8878001E) {
      error_name = "DSERR_CONTROLUNAVAIL";
    } else if (hresult == (int)0x88780032) {
      error_name = "DSERR_INVALIDCALL";
    } else if (hresult == (int)0x88780078) {
      error_name = "DSERR_NODRIVER";
    }
  } else if (hresult == (int)0x8007000E) {
    error_name = "DSERR_OUTOFMEMORY";
  } else if (hresult == (int)0x80004001) {
    error_name = "DSERR_UNSUPPORTED";
  } else if (hresult == (int)0x80004005) {
    error_name = "DSERR_GENERIC";
  } else if (hresult == (int)0x80040110) {
    error_name = "DSERR_NOAGGREGATION";
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

/* sound_dsound_set_channel_properties (0x1caa80)
 *
 * Vtable+0x34 entry point for the DirectSound driver.  Resolves the
 * dsound channel index via sound_dsound_channel_resolve, then forwards
 * to the real property-update function.
 *
 * The original debug build asserts properties->gain is in [0,1] inside
 * sound_dsound_update_channel_properties (sound_dsound_xbox.c line 980)
 * and the volume conversion at line 35.  Retail strips both assertions;
 * the conversion function's ceiling clamp naturally caps gain > 1.0 to
 * 0 dB (max volume).  We downgrade the assertion to a non-fatal warning
 * and clamp to match retail behavior. */
void sound_dsound_set_channel_properties(int channel_index, float *properties,
                                         int update_only)
{
  short channel;

  if (properties[3] < 0.0f || properties[3] > 1.0f) {
    display_assert("properties->gain>=0.f && properties->gain<=1.f",
                   "c:\\halo\\SOURCE\\sound\\sound_dsound_xbox.c", 0x3d4, 0);
    if (properties[3] < 0.0f)
      properties[3] = 0.0f;
    else
      properties[3] = 1.0f;
  }

  channel = sound_dsound_channel_resolve(channel_index);
  if (channel != -1) {
    sound_dsound_update_channel_properties(properties, channel, update_only);
  }
}
