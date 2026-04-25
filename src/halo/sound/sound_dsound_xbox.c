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
