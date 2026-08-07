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

/* Read a device object's current power level.
 *
 * Original 0x964a0. For a real datum handle the object is resolved as a
 * device (type mask 0x380) and the float at +0x1ac is returned; that is the
 * same power field device_preprocess_node_orientations drives animation
 * index [1] from. A NONE handle (-1) yields the shared float constant at
 * 0x2533c0 instead. Both paths leave the result in ST0 (FLD).
 */
float device_get_power(int device_object)
{
  if (device_object != -1) {
    return *(float *)((char *)object_get_and_verify_type(device_object, 0x380) +
                      0x1ac);
  }

  return *(float *)0x2533c0;
}

/* Set or clear a machine's "never appears locked" flag.
 *
 * Original 0x964d0. Unlike its neighbours this resolves the object with type
 * mask 0x80 (object type 7, machine) alone rather than the 0x380 device
 * supertype, and uses the _try_ variant of the accessor, so a handle that is
 * live but not a machine yields NULL and the call becomes a no-op. Both the
 * NONE-sentinel test and the NULL test are early-outs performed in that order.
 *
 * The flags word at +0x1c4 is the machine flags dword; bit 2 (0x4) is
 * "never appears locked". Sibling setters drive bit 0 (operates
 * automatically) and bit 1 (one sided) of the same word.
 */
void device_set_never_appears_locked(int object_index,
                                     char never_appears_locked)
{
  char *machine;

  if (object_index == -1) {
    return;
  }

  machine = (char *)object_try_and_get_and_verify_type(object_index, 0x80);
  if (machine == (char *)0) {
    return;
  }

  if (never_appears_locked != '\0') {
    *(unsigned int *)(machine + 0x1c4) |= 4;
    return;
  }

  *(unsigned int *)(machine + 0x1c4) &= 0xfffffffb;
}

/* Set or clear a machine's "operates automatically" behaviour.
 *
 * Original 0x96630. Resolves the object with type mask 0x80 (machine) using
 * the _try_ accessor, so a live handle that is not a machine yields NULL and
 * the call is a no-op. Unlike device_set_never_appears_locked there is no
 * NONE-sentinel early-out; the handle goes straight to the accessor.
 *
 * The polarity is INVERTED with respect to the parameter: bit 0 of the machine
 * flags dword at +0x1c4 means "does NOT operate automatically", so a zero
 * argument SETS the bit and a non-zero argument CLEARS it. Sibling setters
 * drive bit 1 (one sided) and bit 2 (never appears locked) of the same word.
 *
 * The flags word is loaded once, above the branch, and each arm performs a
 * single store (the original hoists MOV ECX,[EAX+0x1c4] between the byte
 * TEST and the conditional jump).
 */
void device_operates_automatically_set(int object_list,
                                       char operates_automatically)
{
  char *machine;
  unsigned int flags;

  machine = (char *)object_try_and_get_and_verify_type(object_list, 0x80);
  if (machine == (char *)0) {
    return;
  }

  flags = *(unsigned int *)(machine + 0x1c4);

  if (operates_automatically == '\0') {
    *(unsigned int *)(machine + 0x1c4) = flags | 1;
    return;
  }

  *(unsigned int *)(machine + 0x1c4) = flags & 0xfffffffe;
}

/* Set or clear a device group's "change only once more" request.
 *
 * Original 0x96670. The device-group index is a plain array/datum index and
 * the NONE sentinel (-1) is rejected up front, before the datum lookup — this
 * is the only sibling in the file that guards the handle instead of relying on
 * a _try_ accessor.
 *
 * The group record is resolved out of the device-group data array whose
 * data_t pointer lives at 0x5aa8c8, and only the flags byte at +0x02 is
 * touched. Bit 0 carries the request itself; bit 1 is a companion flag that
 * both arms clear unconditionally (its meaning is unproven).
 *
 * Each arm performs two separate read-modify-write byte operations on the same
 * flags byte rather than one combined mask, and the request arm has its own
 * epilogue (the original returns from inside the true branch at 0x96699).
 */
void device_group_change_only_once_more_set(int device_group_index,
                                            char change_only_once_more)
{
  unsigned char *device_group;

  if (device_group_index == -1) {
    return;
  }

  device_group =
    (unsigned char *)datum_get(*(data_t **)0x5aa8c8, device_group_index);

  if (change_only_once_more != '\0') {
    device_group[2] |= 1;
    device_group[2] &= 0xfd;
    return;
  }

  device_group[2] &= 0xfe;
  device_group[2] &= 0xfd;
}

/* Read a device group's current value.
 *
 * Original 0x966b0: sign-extends the low 16 bits of the incoming dword index
 * (MOVSX word ptr [EBP+8]) before handing it to datum_get against the
 * device-group data pool at 0x5aa8c8, then returns the float at +0x04 of the
 * resolved record (FLD dword ptr [EAX+4] -- the value is left in ST0).
 *
 * The datum_get result is dereferenced unconditionally: unlike its sibling
 * device_group_change_only_once_more_set, the original performs no NONE (-1)
 * index test and no NULL check on the returned record. That is preserved
 * deliberately -- adding a defensive guard would change the shape.
 */
float device_group_get_value(int device_group_index)
{
  char *device_group;

  device_group =
    (char *)datum_get(*(data_t **)0x5aa8c8, (int)(short)device_group_index);

  return *(float *)(device_group + 4);
}

/* Route a device-group "set real value" request to the concrete device kind.
 *
 * Original 0x966d0. The handle is resolved as a device (type mask 0x380) and
 * the object's 16-bit type field at +0x64 selects the handler:
 *
 *   type 7 (machine) -> FUN_00095be0, which re-resolves the same handle with
 *                       mask 0x80 and loads its 'mach' definition tag.
 *   type 8 (control) -> FUN_000958f0, which re-resolves with mask 0x100 and
 *                       loads its 'ctrl' definition tag.
 *
 * Any other device type falls through and does nothing.
 *
 * The type is read exactly once (MOVSX EAX, word ptr [EAX+0x64]) and both
 * comparisons run off that single sign-extended register via SUB EAX,7 /
 * DEC EAX. That lowering is why the selector is a switch rather than an
 * if-else cascade, and why the local is an int fed by a 16-bit load: the
 * load stays MOVSX-width, but the compares are done at 32 bits.
 *
 * Both handlers are cdecl and take (device_group_handle, unit_handle): each
 * call site pushes two dwords and cleans 8 bytes. Neither body dereferences
 * the second argument in this build, but the argument is part of the shared
 * signature and must still be pushed.
 *
 * The accessor result is dereferenced without a NULL check, matching the
 * original -- no defensive guard is added.
 */
void device_group_set_real(int device_group_handle, int unit_handle)
{
  int type;

  type =
    *(short *)((char *)object_get_and_verify_type(device_group_handle, 0x380) +
               0x64);

  switch (type) {
  case 7:
    FUN_00095be0(device_group_handle, unit_handle);
    break;
  case 8:
    FUN_000958f0(device_group_handle, unit_handle);
    return;
  }
}

/* Decide whether a device is currently allowed to move towards its desired
 * position.
 *
 * Original 0x96720. The handle is resolved as a device (type mask 0x380) and
 * the answer is seeded false (XOR AL,AL at 0x9673e) before the guard, so a
 * device whose group index at +0x1b4 is NONE (-1) always answers false without
 * touching the device-group pool.
 *
 * Two records are pulled out of the device-group data array at 0x5aa8c8: the
 * device's own group (+0x1b4) and a second group index at +0x1a8 whose value
 * gates the motion. The answer then starts true and each of three independent
 * tests can only clear it -- the original emits three fall-through
 * XOR AL,AL blocks, never an else chain:
 *
 *   1. the group's 16-bit flags at +0x02 with BOTH bit 0 and bit 1 set,
 *   2. object flag bit 1 (0x2) in the byte at +0x1a4,
 *   3. the second group's value at +0x04 not being exactly 1.0f.
 *
 * Test 3 is an INTEGER compare of the raw float bits (CMP dword ptr [EDX+4],
 * 0x3f800000) -- the whole function contains no FPU instruction, so the value
 * is read as a uint32 rather than compared as a float.
 *
 * The +0x1b4 group index is held as a 16-bit local (MOV CX,word / CMP CX,-1 /
 * MOVSX ECX,CX) while the +0x1a8 index is loaded straight to width at its use
 * site (MOVSX EAX,word ptr); that asymmetry is preserved.
 */
bool device_can_change_position(int object_handle)
{
  char *device;
  char *device_group;
  char *power_group;
  short group_index;
  unsigned short group_flags;
  bool can_change;

  device = (char *)object_get_and_verify_type(object_handle, 0x380);

  group_index = *(short *)(device + 0x1b4);
  can_change = false;

  if (group_index != -1) {
    device_group = (char *)datum_get(*(data_t **)0x5aa8c8, group_index);
    power_group =
      (char *)datum_get(*(data_t **)0x5aa8c8, (int)*(short *)(device + 0x1a8));

    group_flags = *(unsigned short *)(device_group + 2);

    can_change = true;
    if ((group_flags & 1) != 0 && (group_flags & 2) != 0) {
      can_change = false;
    }
    if ((*(unsigned char *)(device + 0x1a4) & 2) != 0) {
      can_change = false;
    }
    if (*(unsigned int *)(power_group + 4) != 0x3f800000) {
      can_change = false;
    }
  }

  return can_change;
}

/* Spawn the effect or impulse sound that a device's state machine asked for.
 *
 * Original 0x967a0. Ghidra reports void(void) with in_stack_ parameters because
 * the prologue has no `sub esp` and it loses the argument slots; the
 * disassembly is unambiguous -- EBP+0x8 is loaded into ESI (object handle) and
 * EBP+0xC into EBX (tag index), cleaned by the caller, so this is
 * cdecl void(int, int).
 *
 * A NONE (-1) tag index means "nothing to play" and leaves immediately. That
 * guard is tested before ESI/EDI are pushed, which is why the early exit at
 * 0x9683f pops only EBX/EBP while both work paths fall through the wider
 * EDI/ESI/EBX/EBP epilogue at 0x9683d.
 *
 * The object is resolved as a device (type mask 0x380) unconditionally, before
 * the tag group is even known, and its result is discarded on the 'snd!' path.
 * The call is kept where the original put it because object_get_and_verify_type
 * asserts on a type mismatch -- sinking it into the 'effe' arm would drop that
 * check for sounds.
 *
 * The tag group then selects the spawn routine:
 *   'effe' (0x65666665) -> 0x9ec30, carrying the device's two floats at +0x1b8
 *                          and +0x1ac and passing the object handle twice,
 *   'snd!' (0x736e6421) -> object_impulse_sound_new, positioned at the global
 *                          point pointed to by [0x31fc1c] with the global
 *                          forward vector and gain 1.0f,
 *   anything else       -> the devices.c:761 assert, which does not return.
 *
 * Block layout arbitrates the source shape: the assert is the fall-through at
 * 0x967e7 and sits BEFORE both spawn bodies ('snd!' at 0x967fb, 'effe' at
 * 0x9681e), so 'effe' is tested first and the 'snd!' check is nested inside the
 * negated arm rather than written as an else-if chain.
 */
void FUN_000967a0(int object_handle, int tag_index)
{
  char *device;
  int group_tag;

  if (tag_index != -1) {
    device = (char *)object_get_and_verify_type(object_handle, 0x380);
    group_tag = tag_get_group_tag(tag_index);

    if (group_tag != 0x65666665) { /* 'effe' */
      if (group_tag != 0x736e6421) { /* 'snd!' */
        display_assert(0, "c:\\halo\\SOURCE\\devices\\devices.c", 0x2f9, true);
        system_exit(-1);
      }

      object_impulse_sound_new(object_handle, tag_index, -1,
                               *(float **)0x31fc1c, global_forward_vector_ptr,
                               1.0f);
      return;
    }

    /* The two floats are moved through GPRs (MOV EDX,[EDI+0x1ac];
     * MOV EAX,[EDI+0x1b8]; PUSH EDX; PUSH EAX), never the x87 stack, so the
     * push order -- not the load order -- fixes the arguments: +0x1b8 is
     * param_5 and +0x1ac is param_6. */
    FUN_0009ec30(tag_index, object_handle, object_handle, -1,
                 *(float *)(device + 0x1b8), *(float *)(device + 0x1ac), 0, 0);
  }
}
