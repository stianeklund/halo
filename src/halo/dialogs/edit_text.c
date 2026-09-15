/* Render the device debug overlay string for one device object (0x96d70).
 *
 * Binary evidence (0x96d70..0x96f19, cdecl, one stack arg at [EBP+8] loaded
 * into EDI; kb decl said (void) but MOV EDI,[EBP+0x8] proves the parameter,
 * and xrefs_to reports no callers so no call site is affected):
 *
 * 1. Resolve the object with object_get_and_verify_type(handle, 0x380) into
 *    ESI -- same type mask device_group_set_object_value (0x97040) uses.
 * 2. Gate the whole body on the debug flag byte at 0x5aa8c4 (TEST AL,AL /
 *    JZ 0x96f14). The object lookup happens before the gate.
 * 3. Build the text in a 512-byte stack buffer (SUB ESP,0x20c = 512 + the
 *    12-byte vector3_t), seeded with csstrcpy(buffer, "").
 * 4. Four appends, each addressed as buffer + csstrlen(buffer):
 *      "power %.2f/vel %.2f"      <- +0x1ac (power), +0x1b0 (velocity)
 *      " (group %d desired %.2f)" <- when the int16 at +0x1a8 is not NONE
 *      "|nposition %.2f/vel %.2f" <- +0x1b8 (position), +0x1bc (velocity)
 *      " (group %d desired %.2f)" <- when the int16 at +0x1b4 is not NONE
 *    "|n" is the engine's own line-break escape; string-table spacing
 *    (0x269b34/0x269b50/0x269b6c, 28 bytes apart) confirms the 24-char
 *    literal rather than a "\n" escape.
 *    The float operand order is taken from the FSTP double slots, not the
 *    decompiler: MSVC stores the LAST double first (reusing the still-live
 *    pushed args of the preceding call as its slot at 0x96db2 / 0x96e0c /
 *    0x96e9b), then SUB ESP for the first one.
 * 5. Both group blocks datum_get the device-group record out of the pool at
 *    0x5aa8c8 (the pool devices.c uses) and print its float at +0x04 as
 *    "desired". The index is sign-extended (MOVSX) for datum_get but
 *    zero-extended (MOVZX) for the %d -- both forms are in the disassembly
 *    at 0x96df8/0x96e02 and 0x96e81/0x96e94, so they are kept distinct.
 * 6. Finally take the object's world position and lift it by
 *    up_vector * 0.4f (the .rdata constant at 0x253524 = 0x3ECCCCCD, the
 *    same up-vector/offset pair vehicles.c already documents for 0x31fc44)
 *    before handing position, text and the color word at 0x2ee6c4 to the
 *    debug-string renderer at 0x189cb0.
 *
 * Note on the hazard scan: the ARG_COUNT findings are expected. crt_sprintf
 * is variadic, so its 0x18/0x14 cleanups are 4+4+8+8 and 4+4+4+8; and the
 * 0x18 after FUN_00189cb0 (0x96f11) is its own 16 bytes plus the 8 bytes of
 * object_get_world_position's args that MSVC never popped separately. */
void FUN_00096d70(int object_handle)
{
  char *object;
  char *device_group;
  float *up;
  char buffer[512];
  vector3_t position;

  object = (char *)object_get_and_verify_type(object_handle, 0x380);
  if (*(char *)0x5aa8c4 != '\0') {
    csstrcpy(buffer, "");
    crt_sprintf(buffer + csstrlen(buffer), "power %.2f/vel %.2f",
                (double)*(float *)(object + 0x1ac),
                (double)*(float *)(object + 0x1b0));
    if (*(int16_t *)(object + 0x1a8) != -1) {
      device_group = (char *)datum_get(*(data_t **)0x5aa8c8,
                                       (int)*(int16_t *)(object + 0x1a8));
      crt_sprintf(buffer + csstrlen(buffer), " (group %d desired %.2f)",
                  (int)*(uint16_t *)(object + 0x1a8),
                  (double)*(float *)(device_group + 4));
    }
    crt_sprintf(buffer + csstrlen(buffer), "|nposition %.2f/vel %.2f",
                (double)*(float *)(object + 0x1b8),
                (double)*(float *)(object + 0x1bc));
    if (*(int16_t *)(object + 0x1b4) != -1) {
      device_group = (char *)datum_get(*(data_t **)0x5aa8c8,
                                       (int)*(int16_t *)(object + 0x1b4));
      crt_sprintf(buffer + csstrlen(buffer), " (group %d desired %.2f)",
                  (int)*(uint16_t *)(object + 0x1b4),
                  (double)*(float *)(device_group + 4));
    }
    object_get_world_position(object_handle, &position);
    up = *(float **)0x31fc44;
    position.x = up[0] * 0.4f + position.x;
    position.y = up[1] * 0.4f + position.y;
    position.z = up[2] * 0.4f + position.z;
    FUN_00189cb0('\0', &position, buffer, *(int *)0x2ee6c4);
  }
}

/* Sets a device group's cached value and, if it actually changed, notifies
 * every live device attached to that group (0x96f20).
 *
 * Binary evidence (0x96f20..0x97032, cdecl, args at [EBP+8] int16, [EBP+0xc]
 * float; no other lift's kb decl matches -- this is a standalone entry):
 *
 * 1. Clamp value to [0,1] against the .rdata constants at 0x2533c0 (0.0f) and
 *    0x2533c8 (1.0f) -- same idiom and same addresses as
 *    device_group_set_actual_value below.
 * 2. If device_group_index (the int16 at [EBP+8]) is NONE (-1), return false
 *    (BL stays 0 from the XOR BL,BL at entry) without touching anything.
 * 3. Otherwise datum_get the device-group record (pool at 0x5aa8c8, same
 *    pool as device_group_set_actual_value/device_group_new/
 *    device_group_get_value in devices.c). That record's +0x02 is a flags
 *    word and +0x04 is the cached float value -- confirmed by
 *    device_group_new's seeding of the same two fields.
 * 4. If the new value equals the cached one, this is a no-op (return false).
 * 5. If both flag bits 0x1 and 0x2 are already set, this is also a no-op
 *    (matches the `(flags & 1) != 0 && (flags & 2) != 0` gate devices.c
 *    already uses for this same flags word).
 * 6. Otherwise: OR bit 0x2 into the flags, store the new value, set the
 *    return flag true, then walk every device object (object_iterator_new
 *    type_mask 0x380, same mask control_toggle/device_new use). The iterator
 *    buffer is a 16-byte/int[4] struct identical to the one
 *    device_group_set_actual_value and vehicles.c use; index [2] (byte
 *    offset 0x08) holds the current object's datum handle, confirmed by the
 *    vehicles.c comment on object_iterator_next.
 * 7. For each device object whose own group-index field at +0x1a8 (int16)
 *    equals device_group_index, resolve its 'devi' tag (tag_get(0x64657669,
 *    *(int*)object) -- object+0 is the tag index, same field device_new
 *    reads) and forward one of two definition-relative effect-tag fields to
 *    device_effect_new(object_handle, tag_index): +0x1fc when the (already
 *    clamped) value is > 0.0f, +0x1ec otherwise. device_effect_new itself gates
 *    on tag_index != -1 and spawns the 'effe'/'snd!' effect, so no NONE
 *    check is needed here.
 *
 * Callers (xrefs_to): control_toggle (0x95874), FUN_00095c60 (0x95e87,
 * 0x95edd), FUN_00097220 (0x9724b, below), FUN_00097260 (0x97299, below),
 * device_group_set_desired_value_evaluate (0xbfbf1). */
char FUN_00096f20(int device_group_index, float value)
{
  int16_t index;
  char *device_group;
  unsigned short flags;
  char result;
  int iterator[4];
  char *object;
  char *definition;
  int object_handle;
  int tag_value;

  result = 0;
  index = (int16_t)device_group_index;

  if (value < *(float *)0x2533c0) {
    value = 0.0f;
  } else if (value > *(float *)0x2533c8) {
    value = 1.0f;
  }

  if (index != -1) {
    device_group = (char *)datum_get(*(data_t **)0x5aa8c8, index);
    if (*(float *)(device_group + 4) != value) {
      flags = *(unsigned short *)(device_group + 2);
      if ((flags & 1) == 0 || (flags & 2) == 0) {
        *(unsigned short *)(device_group + 2) = flags | 2;
        *(float *)(device_group + 4) = value;
        result = 1;

        object_iterator_new(iterator, 0x380, 0);
        object = (char *)object_iterator_next(iterator);
        while (object != NULL) {
          definition = (char *)tag_get(0x64657669 /* 'devi' */, *(int *)object);
          if (*(int16_t *)(object + 0x1a8) == index) {
            object_handle = iterator[2];
            if (value > *(float *)0x2533c0) {
              tag_value = *(int *)(definition + 0x1fc);
            } else {
              tag_value = *(int *)(definition + 0x1ec);
            }
            device_effect_new(object_handle, tag_value);
          }
          object = (char *)object_iterator_next(iterator);
        }
      }
    }
  }

  return result;
}

/* Forwards a value to the device group attached to a device-family object
 * (0x97040). Resolves object_handle as a device|control|machine object
 * (type_mask 0x380); if it has a device_group_index (int16_t at +0x1b4)
 * other than -1, calls device_group_set_actual_value with that index and
 * the given value. No-op if object_handle == -1, the object can't be
 * resolved, or there is no attached device group.
 * Callers: FUN_00095c10 (0x95c45), device_set_actual_position_evaluate (0xbfb66). */
void FUN_00097040(int object_handle, float value)
{
  char *object;
  int16_t device_group_index;

  if (object_handle != -1) {
    object = (char *)object_get_and_verify_type(object_handle, 0x380);
    device_group_index = *(int16_t *)(object + 0x1b4);
    if (device_group_index != -1) {
      device_group_set_actual_value(device_group_index, value);
    }
  }
}

/* Seed a device-family object's two device-effect group indices from its
 * definition record (0x97080). Resolves object_handle as a device|control|
 * machine object (type_mask 0x380), touches its 'devi' tag definition
 * (tag_get(0x64657669, *(int *)object) -- result discarded), then:
 *   - int16 at record+0x00 -> object+0x1a8; if it is NONE (-1) a fresh
 *     device effect is created first with initial value
 *     (record_flags & 2) ? 0.0f : 1.0f and flags 4.
 *   - int16 at record+0x02 -> object+0x1b4; if NONE, initial value
 *     (record_flags & 1) ? 1.0f : 0.0f and flags ((record_flags & 4) | 0x10)
 *     >> 2 (i.e. 4 or 5).
 * Both indices are then looked up in the device-group pool at 0x5aa8c8 and
 * the dword at +0x04 of each record is cached into object+0x1ac / +0x1b8.
 * The reference issues two further datum_get calls on the same two indices
 * whose results are discarded (0x97165, 0x97179) -- reproduced verbatim.
 * Finally record_flags bits 0x8 and 0x10 OR 1 and 2 into the object flags
 * word at +0x1a4.
 * Stack-cleanup note: ADD ESP,0x10 at 0x970a8 covers
 * object_get_and_verify_type + tag_get; ADD ESP,0x20 at 0x97181 covers all
 * four datum_get calls -- both ARG_COUNT hazards are grouped cleanups, not
 * extra arguments. */
void FUN_00097080(int object_handle, void *a2)
{
  char *object;
  char *record;
  char *device_group;
  int16_t index;
  uint32_t record_flags;

  object = (char *)object_get_and_verify_type(object_handle, 0x380);
  tag_get(0x64657669 /* 'devi' */, *(int *)object);
  record = (char *)a2;

  index = *(int16_t *)record;
  if (index == -1) {
    index =
      device_group_new((*(uint8_t *)(record + 4) & 2) != 0 ? 0.0f : 1.0f, 4);
  }
  *(int16_t *)(object + 0x1a8) = index;

  index = *(int16_t *)(record + 2);
  if (index == -1) {
    record_flags = *(uint32_t *)(record + 4);
    index = device_group_new((record_flags & 1) != 0 ? 1.0f : 0.0f,
                              (short)(((record_flags & 4) | 0x10) >> 2));
  }
  *(int16_t *)(object + 0x1b4) = index;

  device_group =
    (char *)datum_get(*(data_t **)0x5aa8c8, (int)*(int16_t *)(object + 0x1a8));
  *(int *)(object + 0x1ac) = *(int *)(device_group + 4);
  device_group =
    (char *)datum_get(*(data_t **)0x5aa8c8, (int)*(int16_t *)(object + 0x1b4));
  *(int *)(object + 0x1b8) = *(int *)(device_group + 4);
  datum_get(*(data_t **)0x5aa8c8, (int)*(int16_t *)(object + 0x1a8));
  datum_get(*(data_t **)0x5aa8c8, (int)*(int16_t *)(object + 0x1b4));

  if ((*(uint8_t *)(record + 4) & 8) != 0) {
    *(uint32_t *)(object + 0x1a4) |= 1;
  }
  if ((*(uint8_t *)(record + 4) & 0x10) != 0) {
    *(uint32_t *)(object + 0x1a4) |= 2;
  }
}

/* Check if the object's "front" marker faces away from the aim direction
 * (0x971a0). Returns false if the marker forward dot aim > 0 (facing towards
 * aim), true otherwise (facing away, or if the object/marker can't be
 * resolved). */
bool FUN_000971a0(int object_handle, float *position, float *aim_position)
{
  char *obj = (char *)object_try_and_get_and_verify_type(object_handle, 0x100);
  if (obj && (*(uint8_t *)(obj + 0x1c4) & 1) == 0) {
    char marker_buf[0x6c];
    int16_t count =
      object_get_markers_by_string_id(object_handle, "front", marker_buf, 1);
    if (count == 1) {
      float *fwd = (float *)(marker_buf + 0x3c);
      float dot = fwd[0] * aim_position[0] + fwd[1] * aim_position[1] +
                  fwd[2] * aim_position[2];
      if (dot > 0.0f) {
        return false;
      }
    }
  }
  return true;
}

/* Forwards a value to FUN_00096f20 keyed by the device-family object's
 * device_group_index (0x97220). Resolves object_handle (arg0) as a
 * device|control|machine object (type_mask 0x380, same resolve as
 * FUN_00097040 above); if its device_group_index (int16_t at +0x1b4) is not
 * -1, tail-calls FUN_00096f20(device_group_index, arg1) and returns its
 * result. Returns 0 (AL cleared) if object_handle == -1, the object can't be
 * resolved, or there is no attached device group -- FUN_00096f20 is never
 * reached on those paths.
 * arg1 is an opaque float forwarded byte-for-byte (PUSH of the raw dword at
 * [EBP+0xc], no FLD/FSTP) -- this function never interprets it.
 * Caller: device_set_desired_position_evaluate (0xbfade), which resolves arg0/arg1 from a
 * hs_macro_function_evaluate() record. FUN_00096f20 is unported; declared in
 * kb.json as `char FUN_00096f20(int arg0, float arg1);`. */
char FUN_00097220(int arg0, float arg1)
{
  char *object;
  int16_t device_group_index;

  if (arg0 != -1) {
    object = (char *)object_get_and_verify_type(arg0, 0x380);
    device_group_index = *(int16_t *)(object + 0x1b4);
    if (device_group_index != -1) {
      return FUN_00096f20(device_group_index, arg1);
    }
  }
  return 0;
}

/* Sets a device-family object's "on" flag and cached value, then forwards
 * the value to FUN_00096f20 keyed by a second int16 field (0x97260).
 * Resolves object_handle as a device|control|machine object (type_mask
 * 0x380, same resolve as FUN_00097040/FUN_00097220 above). If resolved:
 * ORs bit 0x4 into the flags word at +0x1a4, stores value (raw dword, no
 * FPU conversion) at +0x1ac, then calls
 * FUN_00096f20(sign-extended int16 at +0x1a8, value).
 * No-op if object_handle == -1 or the object can't be resolved.
 * value is forwarded to FUN_00096f20 byte-for-byte (MOV, no FLD/FSTP).
 * Caller: device_set_power_evaluate (0xbfa56).
 * FUN_00096f20 is unported; declared in kb.json as
 * `char FUN_00096f20(int arg0, float arg1);`. */
void FUN_00097260(int object_handle, float value)
{
  char *object;

  if (object_handle != -1) {
    object = (char *)object_get_and_verify_type(object_handle, 0x380);
    *(uint32_t *)(object + 0x1a4) |= 4;
    *(float *)(object + 0x1ac) = value;
    FUN_00096f20((int)*(int16_t *)(object + 0x1a8), value);
  }
}

/* Clamp cursor and selection to valid range [0, strlen] (0x972b0).
 * If cursor == selection after clamping, cancels the selection.
 * Snaps both to valid character boundaries via unicode_snap_cursor. */
void edit_text_clamp_cursor(void *edit_text)
{
  int *et = (int *)edit_text;
  int16_t len;
  int16_t cursor;
  int16_t clamped_cursor;
  int16_t sel;
  int16_t clamped_sel;

  len = (int16_t)csstrlen((const char *)et[0]);

  cursor = *(int16_t *)((int)et + 6);
  if (cursor < 0) {
    clamped_cursor = 0;
  } else if (cursor > len) {
    clamped_cursor = len;
  } else {
    clamped_cursor = cursor;
  }

  sel = *(int16_t *)((int)et + 8);
  *(int16_t *)((int)et + 6) = clamped_cursor;
  if (sel < -1) {
    clamped_sel = -1;
  } else if (sel > len) {
    clamped_sel = len;
  } else {
    clamped_sel = sel;
  }

  *(int16_t *)((int)et + 8) = clamped_sel;
  if (clamped_cursor == clamped_sel) {
    *(int16_t *)((int)et + 8) = -1;
  }

  unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
  if (*(int16_t *)((int)et + 8) != -1) {
    unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 8));
  }
}

/* Moves the text cursor to the end of the edit text buffer and
 * clears any active selection. Asserts that the edit_text struct
 * is valid (non-null, has buffer, max_length > 0, strlen <= max). */
void edit_text_set_cursor_to_end(void *edit_text)
{
  int *et = (int *)edit_text;
  int16_t len;

  if (et == NULL || et[0] == 0 || *(int16_t *)((int)et + 4) <= 0 ||
      (unsigned int)csstrlen((const char *)et[0]) >
        (unsigned int)(int)*(int16_t *)((int)et + 4)) {
    display_assert("valid_edit_text(edit)",
                   "c:\\halo\\SOURCE\\dialogs\\edit_text.c", 0x9f, 1);
    system_exit(-1);
  }

  edit_text_clamp_cursor(edit_text);

  len = (int16_t)csstrlen((const char *)et[0]);
  *(int16_t *)((int)et + 6) = len;
  *(int16_t *)((int)et + 8) = -1;
}

/* Get the selection range as ordered (min, max) of cursor and anchor (0x973a0).
 * Returns false if no selection is active (selection_start == -1). */
bool edit_text_get_selection_range(void *edit_text, int16_t *out_start,
                                   int16_t *out_end)
{
  int *et = (int *)edit_text;
  int16_t sel;
  int16_t cursor;

  if (et == NULL || et[0] == 0 || *(int16_t *)((int)et + 4) <= 0 ||
      (unsigned int)csstrlen((const char *)et[0]) >
        (unsigned int)(int)*(int16_t *)((int)et + 4)) {
    display_assert("valid_edit_text(edit)",
                   "c:\\halo\\SOURCE\\dialogs\\edit_text.c", 0xae, 1);
    system_exit(-1);
  }

  edit_text_clamp_cursor(edit_text);

  sel = *(int16_t *)((int)et + 8);
  if (sel == -1)
    return false;

  cursor = *(int16_t *)((int)et + 6);
  *out_start = (sel > cursor) ? cursor : sel;

  sel = *(int16_t *)((int)et + 8);
  cursor = *(int16_t *)((int)et + 6);
  *out_end = (sel > cursor) ? sel : cursor;

  return true;
}

/* Validates the edit_text struct and initializes cursor state by
 * placing the cursor at the end of the current text. */
void edit_text_initialize(void *edit_text)
{
  int *et = (int *)edit_text;

  if (et == NULL || et[0] == 0 || *(int16_t *)((int)et + 4) <= 0 ||
      (unsigned int)csstrlen((const char *)et[0]) >
        (unsigned int)(int)*(int16_t *)((int)et + 4)) {
    display_assert("valid_edit_text(edit)",
                   "c:\\halo\\SOURCE\\dialogs\\edit_text.c", 0x19, 1);
    system_exit(-1);
  }

  edit_text_set_cursor_to_end(edit_text);
}

/* Processes a single key event for the edit_text widget. Handles:
 * - Character insertion (with or without active selection)
 * - Left/Right arrow keys for cursor movement
 * - Shift+arrow for extending selection
 * - Backspace/Delete for character or selection deletion
 * When a selection is active, typing replaces it. Backspace/Delete
 * remove the selection range. Arrow keys collapse the selection to
 * the appropriate end. All cursor changes are snapped to unicode
 * character boundaries via unicode_snap_cursor.
 *
 * key_event layout:
 *   offset 0: uint8_t flags (bit 0 = shift held)
 *   offset 1: uint8_t character code
 *   offset 2: int16_t key code (0x1d=backspace, 0x54=delete, 0x4f=left,
 * 0x50=right)
 *
 * edit_text layout:
 *   offset 0: char* text buffer pointer
 *   offset 4: int16_t max_length
 *   offset 6: int16_t cursor_pos
 *   offset 8: int16_t selection (-1 = no selection)
 */
void edit_text_process_key(void *edit_text, void *keystroke)
{
  int *et = (int *)edit_text;
  unsigned char *key = (unsigned char *)keystroke;
  int16_t sel_start, sel_end;
  int text;
  int len;
  int cursor_pos;
  int16_t key_code;

  if (et == NULL || et[0] == 0 || *(int16_t *)((int)et + 4) <= 0 ||
      (unsigned int)csstrlen((const char *)et[0]) >
        (unsigned int)(int)*(int16_t *)((int)et + 4)) {
    display_assert("valid_edit_text(edit)",
                   "c:\\halo\\SOURCE\\dialogs\\edit_text.c", 0x23, 1);
    system_exit(-1);
  }

  edit_text_clamp_cursor(edit_text);

  key_code = *(int16_t *)(key + 2);

  /* --- Backspace / Delete --- */
  if (key_code == 0x1d || key_code == 0x54) {
    /* If there is an active selection, delete the selected range */
    if (edit_text_get_selection_range(edit_text, &sel_end, &sel_start)) {
      text = et[0];
      len = csstrlen((const char *)(text + (int)sel_start));
      csmemmove((void *)(text + (int)sel_end),
                (const void *)(text + (int)sel_start), (unsigned int)(len + 1));
      *(int16_t *)((int)et + 6) = sel_end;
      *(int16_t *)((int)et + 8) = -1;
      unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
      return;
    }

    if (key_code == 0x1d) {
      /* Backspace: delete character before cursor */
      int16_t old_cursor = *(int16_t *)((int)et + 6);
      if (old_cursor > 0) {
        unicode_cursor_backward((const char *)et[0], (int16_t *)((int)et + 6));
        text = et[0];
        len = csstrlen((const char *)(text + (int)old_cursor));
        csmemmove((void *)(text + (int)*(int16_t *)((int)et + 6)),
                  (const void *)(text + (int)old_cursor),
                  (unsigned int)(len + 1));
      }
    } else {
      /* Delete: delete character at cursor */
      int16_t cur = *(int16_t *)((int)et + 6);
      int16_t temp_cursor;

      if ((unsigned int)(int)cur >= (unsigned int)csstrlen((const char *)et[0]))
        goto snap_and_return;

      temp_cursor = cur;
      unicode_cursor_forward((const char *)et[0], &temp_cursor);
      text = et[0];
      len = csstrlen((const char *)(text + (int)temp_cursor));
      csmemmove((void *)(text + (int)*(int16_t *)((int)et + 6)),
                (const void *)(text + (int)temp_cursor),
                (unsigned int)(len + 1));
    }
    goto snap_and_return;
  }

  /* --- Left / Right arrow --- */
  if (key_code == 0x4f || key_code == 0x50) {
    if ((key[0] & 1) == 0) {
      /* No shift: if selection active, collapse to appropriate end */
      if (edit_text_get_selection_range(edit_text, &sel_start, &sel_end)) {
        *(int16_t *)((int)et + 8) = -1;
        if (key_code == 0x4f) {
          /* Left: move cursor to selection start */
          *(int16_t *)((int)et + 6) = sel_start;
          unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
          return;
        }
        /* Right: move cursor to selection end */
        *(int16_t *)((int)et + 6) = sel_end;
        unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
        return;
      }
      if ((key[0] & 1) == 0)
        goto move_cursor;
    }

    /* Shift held (or shift re-check fell through): begin/extend selection */
    if (*(int16_t *)((int)et + 8) == -1) {
      *(int16_t *)((int)et + 8) = *(int16_t *)((int)et + 6);
    }

  move_cursor:
    if (key_code == 0x4f && *(int16_t *)((int)et + 6) > 0) {
      unicode_cursor_backward((const char *)et[0], (int16_t *)((int)et + 6));
    } else if (key_code == 0x50) {
      if ((unsigned int)(int)*(int16_t *)((int)et + 6) <
          (unsigned int)csstrlen((const char *)et[0])) {
        unicode_cursor_forward((const char *)et[0], (int16_t *)((int)et + 6));
      }
    }

    /* If selection collapsed (cursor == selection anchor), clear it */
    if (*(int16_t *)((int)et + 8) == *(int16_t *)((int)et + 6)) {
      *(int16_t *)((int)et + 8) = -1;
      unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
      return;
    }
    goto snap_and_return;
  }

  /* --- Character insertion --- */
  if (key[1] == 0 || key[1] == 0xff)
    goto snap_and_return;

  if (edit_text_get_selection_range(edit_text, &sel_start, &sel_end)) {
    /* Replace selection with typed character */
    text = et[0];
    len = csstrlen((const char *)(text + (int)sel_end));
    csmemmove((void *)(text + (int)sel_start + 1),
              (const void *)(text + (int)sel_end), (unsigned int)(len + 1));
    *(int16_t *)((int)et + 6) = sel_start;
    *(int16_t *)((int)et + 8) = -1;
    *(unsigned char *)((int)sel_start + et[0]) = key[1];
    *(int16_t *)((int)et + 6) = *(int16_t *)((int)et + 6) + 1;
    unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
    return;
  }

  /* No selection: insert at cursor if room */
  if ((unsigned int)csstrlen((const char *)et[0]) >=
      (unsigned int)(int)*(int16_t *)((int)et + 4))
    goto snap_and_return;

  cursor_pos = (int)*(int16_t *)((int)et + 6) + et[0];
  len = csstrlen((const char *)cursor_pos);
  csmemmove((void *)(cursor_pos + 1), (const void *)cursor_pos,
            (unsigned int)(len + 1));
  *(unsigned char *)((int)*(int16_t *)((int)et + 6) + et[0]) = key[1];
  *(int16_t *)((int)et + 6) = *(int16_t *)((int)et + 6) + 1;
  unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
  return;

snap_and_return:
  unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
  return;
}
