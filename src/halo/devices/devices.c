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

/* Drive a device's animation graph from its current position/power values.
 *
 * Original 0x96310. Resolves the object datum to a device (type mask 0x380),
 * loads its 'devi' definition and the definition's 'antr' (model animation
 * graph) tag, then takes element 0 of the antr block at +0x30 (element size
 * 0x60). That element holds a count at +0x54 and a pointer to an array of
 * int16 animation indices at +0x58; index NONE (-1) means "no animation".
 *
 * Index [0] is driven by the device position (object +0x1b8), optionally
 * inverted when object flag bit 0 at +0x1a4 is set. The devi flags word at
 * +0x17c selects whether the whole frame count or frame_count-1 is used
 * (bit 0) and whether the overlay path is taken (bit 1).
 *
 * Index [1] is driven by the device power (object +0x1ac) and always uses
 * the plain apply path.
 *
 * Animation elements live in the antr block at +0x74 (element size 0xb4) and
 * carry their frame count as an int16 at +0x22.
 */
void device_preprocess_node_orientations(int object_datum, void *node_data)
{
  char *device;
  char *devi;
  char *antr;
  char *elem;
  char *anim;
  unsigned int flags;
  int frame_count;
  float position;
  float frame;

  device = (char *)object_get_and_verify_type(object_datum, 0x380);
  devi = (char *)tag_get(0x64657669 /* 'devi' */, *(int *)device);
  antr = (char *)tag_get(0x616e7472 /* 'antr' */, *(int *)(devi + 0x44));

  if (*(int *)(antr + 0x30) != 0) {
    elem = (char *)tag_block_get_element(antr + 0x30, 0, 0x60);
    if (elem != (char *)0) {
      if (*(int *)(elem + 0x54) > 0 && **(short **)(elem + 0x58) != -1) {
        anim = (char *)tag_block_get_element(antr + 0x74,
                                             **(short **)(elem + 0x58), 0xb4);

        if ((*(unsigned char *)(device + 0x1a4) & 1) != 0) {
          position = 1.0f - *(float *)(device + 0x1b8);
        } else {
          position = *(float *)(device + 0x1b8);
        }

        flags = *(unsigned int *)(devi + 0x17c);
        if ((flags & 1) != 0) {
          frame_count = *(short *)(anim + 0x22);
        } else {
          frame_count = *(short *)(anim + 0x22) - 1;
        }

        frame = (float)frame_count * position;

        if ((flags & 2) != 0) {
          overlay_animation_apply(anim, (int)frame, node_data);
        } else {
          FUN_00122690(anim, frame, node_data);
        }
      }

      if (*(int *)(elem + 0x54) > 1 && (*(short **)(elem + 0x58))[1] != -1) {
        anim = (char *)tag_block_get_element(
          antr + 0x74, (*(short **)(elem + 0x58))[1], 0xb4);
        FUN_00122690(anim,
                     (float)(int)*(short *)(anim + 0x22) *
                       *(float *)(device + 0x1ac),
                     node_data);
      }
    }
  }
}
