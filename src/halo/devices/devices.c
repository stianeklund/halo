/* Initialize a newly created device object.
 *
 * Original 0x960c0: resolves the object datum to a device (type mask 0x380),
 * touches its device definition tag, clears the two 16-bit NONE-sentinel
 * fields at +0x1a8 / +0x1b4, and sets flag bit 0x40000 in the object flags
 * word at +0x04. Always returns true.
 */
bool device_new(int object_index)
{
  char *device;
  short none;

  device = (char *)object_get_and_verify_type(object_index, 0x380);

  /* Result deliberately unused: the original discards EAX (OR EAX,-1 on the
   * next instruction). The call exists for its tag-load side effects. */
  tag_get(0x64657669 /* 'devi' */, *(int *)device);

  none = -1;
  *(short *)(device + 0x1b4) = none;
  *(short *)(device + 0x1a8) = none;

  *(int *)(device + 0x04) |= 0x40000;

  return true;
}
