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
