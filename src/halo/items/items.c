/*
 * text_search_and_replace_function_table[1]  (0x000f52f0, __cdecl)
 *
 * Resolver that renders a single-digit wide replacement string from a widget
 * field.  Reads a signed 16-bit selector at (widget + 8), then writes the
 * digit + NUL terminator into a static wchar_t[2] result buffer at 0x0046cee8
 * and returns a pointer to it (EAX = 0x0046cee8 in every arm of the original).
 *
 *   selector -1 / 0 -> L"1"
 *   selector  1     -> L"2"
 *   selector  2     -> L"3"
 *   selector  3     -> L"4"
 *   otherwise       -> L"?"
 *
 * The original biases the selector by +1 and dispatches through a 5-entry jump
 * table (indices 0..4 for selector -1..3); the switch below reproduces that
 * dense mapping.  The 0x0046cee8 result buffer lies inside the delinker's
 * over-sized game_data build-version array, but is a distinct static owned by
 * this TU, so it is declared as its own symbol (word_46CEE8).
 *
 * ABI: sole cdecl stack arg 'widget' (no register args); leaf, no callees.
 */
wchar_t *FUN_000f52f0(void *widget)
{
  switch (*(short *)((char *)widget + 8)) {
  case -1:
  case 0:
    word_46CEE8[0] = L'1';
    word_46CEE8[1] = L'\0';
    return word_46CEE8;
  case 1:
    word_46CEE8[0] = L'2';
    word_46CEE8[1] = L'\0';
    return word_46CEE8;
  case 2:
    word_46CEE8[0] = L'3';
    word_46CEE8[1] = L'\0';
    return word_46CEE8;
  case 3:
    word_46CEE8[0] = L'4';
    word_46CEE8[1] = L'\0';
    return word_46CEE8;
  default:
    word_46CEE8[0] = L'?';
    word_46CEE8[1] = L'\0';
    return word_46CEE8;
  }
}

/* Virtual on-screen keyboard initialization (items.obj).
 * TU: c:\halo\SOURCE\interface\virtual_keyboard.c (__FILE__ assert @0x28a854).
 *
 * virtual_keyboard_globals lives at 0x46cef0:
 *   +0x00  byte[4]           four separate byte flags (zeroed individually)
 *   +0x04  void *keyboard    'vcky' tag_get result (asserted non-NULL, l.363)
 *   +0x08  int16[3] = 0      state words (0x46cef8/cefa/cefc)
 *   +0x0e  int16 = -1        sentinel (0x46cefe)
 *   +0x10  int16 = -1        sentinel (0x46cf00)
 *   +0x12  int16 = 0         (0x46cf02)
 *   +0x18  int[3] = 0        dwords 0x46cf08/cf0c/cf10
 *   +0x24  int caret_bitmap  'bitm' tag index (0x46cf14; -1 = load failed)
 *
 * Looks up the 'vcky' virtual keyboard tag for "ui\english"; on success
 * caches the tag definition pointer and resets the keyboard state fields,
 * else logs a priority-2 error.  Independently resolves the caret bitmap
 * 'bitm' tag "ui\shell\bitmaps\white" (also error-logged on failure).
 *
 * Returns whether the keyboard definition pointer is non-NULL (SETNE AL
 * from a reload of 0x46cef4; the sole caller at 0xe8809 does TEST AL,AL
 * and logs an error when it is false).
 */
bool virtual_keyboard_initialize(void)
{
  int keyboard_tag;

  *(unsigned char *)0x46cef0 = 0;
  *(unsigned char *)0x46cef1 = 0;
  *(unsigned char *)0x46cef2 = 0;
  *(unsigned char *)0x46cef3 = 0;

  keyboard_tag = tag_loaded(0x76636b79, "ui\\english"); /* 'vcky' */
  if (keyboard_tag != -1) {
    *(void **)0x46cef4 = tag_get(0x76636b79, keyboard_tag); /* 'vcky' */
    if (*(void **)0x46cef4 == (void *)0) {
      display_assert("virtual_keyboard_globals.keyboard",
                     "c:\\halo\\SOURCE\\interface\\virtual_keyboard.c", 363,
                     true);
      system_exit(-1);
    }
    *(short *)0x46cef8 = 0;
    *(short *)0x46cefa = 0;
    *(short *)0x46cefc = 0;
    *(short *)0x46cefe = -1;
    *(short *)0x46cf00 = -1;
    *(short *)0x46cf02 = 0;
    *(int *)0x46cf08 = 0;
    *(int *)0x46cf0c = 0;
    *(int *)0x46cf10 = 0;
  } else {
    error(2, "failed to load virtual keyboard for '%s' language", "<unknown>");
  }

  *(int *)0x46cf14 =
    tag_loaded(0x6269746d, "ui\\shell\\bitmaps\\white"); /* 'bitm' */
  if (*(int *)0x46cf14 == -1) {
    error(2, "failed to load virtual keyboard caret bitmap '%s'",
          "ui\\shell\\bitmaps\\white");
  }

  return *(void **)0x46cef4 != (void *)0;
}

/* Virtual keyboard cursor move handler: advance the keymap row cursor
 * downward (0xf5750, virtual_keyboard.obj TU).
 *
 * Increments the row cursor at 0x46cef8 modulo 5 (5 keymap rows), skipping
 * rows whose key character (keymap byte at 0x28a790[col + row*0xb]) equals
 * the character under the pre-move cursor, so duplicate/merged keys are
 * stepped over in one press. Stores the new row, plays the UI cursor-move
 * sound (selector 1), and returns 1 (move accepted -> caller latches
 * last_move_dir/last_move_time). Sibling of FUN_000f5660/56b0/5700.
 *
 * Disasm notes: row is held in AX for the whole loop (16-bit inc/cmp/xor)
 * and stored to the global once after the loop; the pre-move key byte is
 * cached in DL before the loop; col (0x46cefa) is MOVSX-loaded once. */
char FUN_000f5750(void)
{
  short row;
  short col;
  char original_key;

  row = *(short *)0x46cef8;
  col = *(short *)0x46cefa;
  original_key = ((char *)0x28a790)[(int)col + row * 0xb];
  do {
    row = (short)(row + 1);
    if (row == 5) {
      row = 0;
    }
  } while (((char *)0x28a790)[(int)col + row * 0xb] == original_key);
  *(short *)0x46cef8 = row;
  ui_play_audio_feedback_sound(1);
  return 1;
}

/* Virtual keyboard backspace / delete-char handler (0xf5f30).
 * Deletes the wide-char (UTF-16) immediately before the cursor from the
 * edit buffer. If the cursor (0x46cf0c) is past the buffer base (0x46cf08),
 * shift the tail back by one 2-byte cell via csmemmove, NUL-terminate the
 * final wchar cell, then back the cursor up by one wchar. The cursor
 * feedback sound (selector 1) always plays, whether or not a char was
 * removed.
 * Globals: 0x46cf08 = buffer base ptr, 0x46cf0c = cursor/end ptr,
 * 0x46cefc = buffer capacity in bytes (unsigned 16-bit; loaded via MOVZX).
 * The terminator index uses (capacity >> 1) - 1 to match the original's
 * SHR + scaled-index store [base + (cap>>1)*2 - 2]. */
void FUN_000f5f30(void)
{
  char *cursor;
  int remaining;

  cursor = *(char **)0x46cf0c;
  if (*(char **)0x46cf08 < cursor) {
    remaining = ((int)*(unsigned short *)0x46cefc - (int)cursor) +
                (int)*(char **)0x46cf08;
    if (remaining >= 0) {
      csmemmove(cursor - 2, cursor, (unsigned int)remaining);
      ((unsigned short *)*(
        char **)0x46cf08)[((unsigned int)*(unsigned short *)0x46cefc >> 1) -
                          1] = 0;
      *(char **)0x46cf0c -= 2;
    }
  }
  ui_play_audio_feedback_sound(1);
}

/* Virtual on-screen keyboard input pump (virtual_keyboard.obj).
 * TU: c:\halo\SOURCE\interface\virtual_keyboard.c (__FILE__ assert
 * @0x28a790..).
 *
 * FUN_000f63f0 (0xf63f0): drains the per-frame input event queue for the
 * on-screen keyboard and turns controller events into a single move/action
 * code, then dispatches the matching per-direction handler.
 *
 * virtual_keyboard_globals cluster (see items.c virtual_keyboard_initialize):
 *   0x46cef8  short  cursor row       (keymap row)
 *   0x46cefa  short  cursor col       (keymap col)
 *   0x46cefc  short  buffer_size      (edit buffer capacity, bytes)
 *   0x46cefe  short  last_move_dir    (latched direction for auto-repeat)
 *   0x46cf00  short  current_char     (keymap-resolved character)
 *   0x46cf07  byte   buffer_dirty     (0 = pristine, 1 = user has typed)
 *   0x46cf08  char*  buffer_base      (edit buffer start)
 *   0x46cf0c  char*  cursor           (edit caret within the buffer)
 *   0x46cf10  int    last_move_time   (ms timestamp of last accepted move)
 *   0x46cf58  int    repeat_timer     (ms timestamp gating auto-repeat)
 * Keymap dispatch table at 0x28a790 is a signed byte array indexed
 * [row * 0xb + col] (11 columns) yielding the character stored at 0x46cf00.
 *
 * Move handlers FUN_000f5660/56b0/5700/5750/5fb0/57a0 return a char in AL
 * (0/1); the return latches last_move_dir + last_move_time when it is 1.
 */

/* Controller input event record; event_manager_get_next_event writes 8 bytes
 * (two dwords) here: a 16-bit event type at +0 and a 32-bit payload at +4.
 * The payload is reinterpreted per event type (analog word pair / button
 * id + pressed-flag bytes). */
struct virtual_keyboard_event {
  short type; /* +0 : 1 = analog stick, 3 = button */
  short reserved; /* +2 */
  int data; /* +4 : analog X (low word) / Y (high word), or
                    button id (low byte) + pressed flag (byte +1) */
};

void virtual_keyboard_process_input(void)
{
  struct virtual_keyboard_event event;
  int now_ms;
  int action; /* selected move/action code, -1 = none */
  int timer_tmp; /* value stored back into repeat_timer via the shared tail */
  char moved; /* nonzero once a handler accepted the move */
  char pressed; /* button pressed-flag byte */

  now_ms = system_milliseconds();
  action = -1;
  moved = 0;

  if (!event_manager_get_next_event(&event, -1))
    return;

  do {
    if (event.type == 1) {
      /* analog stick: cardinal only at full deflection */
      if ((short)(event.data >> 16) == 0x7fff) {
        action = 2;
      } else if ((short)(event.data >> 16) == -0x8000) {
        action = 3;
      } else if ((short)event.data == -0x8000) {
        action = 0;
      } else {
        timer_tmp = *(int *)0x46cf58;
        if ((short)event.data == 0x7fff)
          goto set_dir_right;
      }
    } else if (event.type == 3) {
      /* button: high byte = pressed flag, low byte = button id */
      pressed = (char)(event.data >> 8);
      switch (event.data & 0xff) {
      case 0: /* A: select current character */
        if (pressed == 1)
          action = 4;
        break;
      case 1: /* B / start: back */
      case 0xd:
        if (pressed == 1)
          action = 5;
        break;
      case 2: /* X: accept / clear */
        if (pressed == 1) {
          if (*(unsigned char *)0x46cf07 == 1) {
            if (*(short *)0x46cefc == 0)
              display_assert("virtual_keyboard_globals.buffer_size>0",
                             "c:\\halo\\SOURCE\\interface\\virtual_keyboard.c",
                             0x27a, true);
            csmemset(*(char **)0x46cf08, 0, (unsigned int)*(short *)0x46cefc);
            *(char **)0x46cf0c = *(char **)0x46cf08;
            *(unsigned char *)0x46cf07 = 0;
            ui_play_audio_feedback_sound(1);
            moved = 1;
          } else {
            FUN_000f5f30();
            moved = 1;
          }
        }
        break;
      case 6: /* cursor left */
        if (pressed == 1) {
          if (*(char **)0x46cf08 < *(char **)0x46cf0c)
            *(char **)0x46cf0c -= 1;
        cursor_moved:
          *(unsigned char *)0x46cf07 = 0;
          ui_play_audio_feedback_sound(1);
          moved = 1;
        }
        break;
      case 7: /* cursor right */
        if (pressed == 1) {
          if (**(char **)0x46cf0c != 0)
            *(char **)0x46cf0c += 1;
          goto cursor_moved;
        }
        break;
      case 8: /* dpad up (auto-repeat gated) */
        if (*(short *)0x46cefe != 2 ||
            0xf9 < (unsigned int)(now_ms - *(int *)0x46cf58) || pressed == 1) {
          action = 2;
          *(int *)0x46cf58 = now_ms;
        }
        break;
      case 9: /* dpad down */
        if (*(short *)0x46cefe != 3 ||
            0xf9 < (unsigned int)(now_ms - *(int *)0x46cf58) || pressed == 1) {
          action = 3;
          *(int *)0x46cf58 = now_ms;
        }
        break;
      case 10: /* dpad left */
        if (*(short *)0x46cefe != 0 ||
            0xf9 < (unsigned int)(now_ms - *(int *)0x46cf58) || pressed == 1) {
          action = 0;
          *(int *)0x46cf58 = now_ms;
        }
        break;
      case 0xb: /* dpad right */
        timer_tmp = now_ms;
        if (*(short *)0x46cefe != 1 ||
            0xf9 < (unsigned int)(now_ms - *(int *)0x46cf58) || pressed == 1) {
        set_dir_right:
          *(int *)0x46cf58 = timer_tmp;
          action = 1;
        }
        break;
      case 0xc: /* home / reset to origin */
        if (pressed == 1) {
          *(short *)0x46cef8 = 0;
          *(short *)0x46cefa = 0;
          action = 4;
        }
        break;
      }
    }
  } while (event_manager_get_next_event(&event, -1));

  if (action != -1) {
    *(short *)0x46cf00 = (short)(char)((
      char *)0x28a790)[(int)*(short *)0x46cefa + *(short *)0x46cef8 * 0xb];
    switch (action) {
    case 0:
      moved = FUN_000f5660();
      break;
    case 1:
      moved = FUN_000f56b0();
      break;
    case 2:
      moved = FUN_000f5700();
      break;
    case 3:
      moved = FUN_000f5750();
      break;
    case 4:
      moved = FUN_000f5fb0();
      break;
    case 5:
      moved = FUN_000f57a0();
      break;
    }
    if (moved == 1) {
      *(short *)0x46cefe = (short)action;
      *(int *)0x46cf10 = now_ms;
    }
  }
}

#include "x87_math.h"

/* Activate the pickup sound effect for an equipment item.
 * Looks up the equipment tag definition ('eqip') and plays the
 * pickup sound (tag field at +0x31c) at full volume (scale=1.0). */
void item_activate_equipment_effect(int equipment_handle)
{
  int *equip_obj;
  char *tag_def;
  int sound_tag;

  equip_obj = (int *)object_get_and_verify_type(equipment_handle, 8);
  tag_def = (char *)tag_get(0x65716970, *equip_obj);
  sound_tag = *(int *)(tag_def + 0x31c);
  if (sound_tag != NONE) {
    sound_impulse_start(sound_tag, 1.0f);
  }
}

/* Play the pickup sound for an equipment tag (0xf67f0).
 * Reads the pickup sound tag index at equipment_tag+0x31c and plays it. */
void FUN_000f67f0(int equipment_tag_index)
{
  int tag_data = (int)tag_get(0x65716970, equipment_tag_index);
  if (*(int *)(tag_data + 0x31c) != -1) {
    sound_impulse_start(*(int *)(tag_data + 0x31c), 1.0f);
  }
}

/* Item garbage-collection countdown tick (0xf6820).
 * Fetches the item object (type mask 0x10), decrements the signed 16-bit
 * despawn timer at item_obj+0x1dc (seeded to a random [300,600] value by
 * item_begin_garbage_collection), and deletes the object once the timer
 * reaches 0. Returns whether the item survived this tick (timer still > 0);
 * the original latches this into BL via SETG and returns it in AL.
 * Despite the kb name "item_new", the binary behavior is a per-tick
 * release/countdown, not allocation. */
char item_new(int object_handle)
{
  char *item_obj;
  char survived;

  item_obj = (char *)object_get_and_verify_type(object_handle, 0x10);
  *(int16_t *)(item_obj + 0x1dc) = *(int16_t *)(item_obj + 0x1dc) - 1;
  survived = *(int16_t *)(item_obj + 0x1dc) > 0;
  if (!survived) {
    object_delete(object_handle);
  }
  return survived;
}

/* Mark an item (type mask 0x10 = garbage item type) for garbage collection.
 * Sets the garbage flag, ORs object flags bits 18 and 19 (0xc0000), and
 * picks a random despawn timer in [300, 600] ticks stored at item_obj+0x1dc.
 * Returns true on success. */
bool item_begin_garbage_collection(int item_handle)
{
  char *item_obj;
  unsigned int *seed;
  unsigned int flags;

  item_obj = (char *)object_get_and_verify_type(item_handle, 0x10);
  object_set_garbage_flag(item_handle, 1);
  flags = *(unsigned int *)(item_obj + 0x4);
  *(unsigned int *)(item_obj + 0x4) = flags | 0xc0000;
  seed = (unsigned int *)get_global_random_seed_address();
  *(int16_t *)(item_obj + 0x1dc) = random_range(seed, 300, 600);
  return 1;
}

/* Read the type byte (offset +3) from an item datum entry (0xf68b0).
 * Returns as short (zero-extended from byte). */
short FUN_000f68b0(int item_handle)
{
  char *datum;
  datum = (char *)datum_get(*(data_t **)0x5a8d50, item_handle);
  return (unsigned char)datum[3];
}

/* Activate an item: set flags 0x6000, record game time, reset timer (0xf6910).
 */
char item_activate(int item_handle)
{
  char *item_obj;
  item_obj = (char *)object_get_and_verify_type(item_handle, 0x1c);
  *(unsigned int *)(item_obj + 4) = *(unsigned int *)(item_obj + 4) | 0x6000;
  *(int *)(item_obj + 0x1b4) = game_time_get();
  *(int *)(item_obj + 0x1b0) = NONE;
  return 1;
}

/* Iterate all item objects (type 0x1c) and return true if any have
 * a positive danger count, indicating a dangerous item is near a player. */
bool dangerous_items_near_player(void)
{
  char iter[16];
  void *item;

  object_iterator_new(iter, 0x1c, 1);
  for (item = object_iterator_next(iter); item != NULL;
       item = object_iterator_next(iter)) {
    if (*(int16_t *)((char *)item + 0x1a8) > 0)
      return true;
  }
  return false;
}

/* Attach or detach an item from a unit.
 * When unit_handle is valid: sets item flags bit 0 (attached), conditionally
 * sets bit 1 (player-controlled) based on unit's player handle at +0x1c8,
 * copies the player handle to item+0x70, removes item from garbage list,
 * clears bits 3 and 5 of item flags, and resets the item's scenario location
 * at +0x48.
 * When unit_handle is NONE: clears bits 0 and 1, detaching the item. */
void item_attach_to_unit(int item_handle, int unit_handle)
{
  char *item_obj;
  char *unit_obj;
  uint32_t flags;

  item_obj = (char *)object_get_and_verify_type(item_handle, 0x1c);
  if (unit_handle != NONE) {
    unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
    flags = *(uint32_t *)(item_obj + 0x1a4);
    flags |= 1;
    *(uint32_t *)(item_obj + 0x1a4) = flags;
    if (*(int *)(unit_obj + 0x1c8) == NONE) {
      flags = (flags & ~2u) | 1;
    } else {
      flags |= 3;
    }
    *(uint32_t *)(item_obj + 0x1a4) = flags;
    *(int *)(item_obj + 0x70) = *(int *)(unit_obj + 0x1c8);
    object_set_garbage_flag(item_handle, 0);
    *(uint32_t *)(item_obj + 0x1a4) = *(uint32_t *)(item_obj + 0x1a4) & ~0x28u;
    *(int *)(item_obj + 0x48) = NONE;
    *(int16_t *)(item_obj + 0x4c) = (int16_t)NONE;
    scenario_location_reset((int *)(item_obj + 0x48));
  } else {
    *(uint32_t *)(item_obj + 0x1a4) = *(uint32_t *)(item_obj + 0x1a4) & ~3u;
  }
}

/* Initialize the danger countdown for an item (0xf6af0).
 * If the item's danger count at +0x1a8 is zero, spawns an effect using
 * the tag reference at +0x2f4 of the 'item' tag definition (detonation
 * or fuse warning), then sets the countdown to a random value in the
 * range [tag+0x2e0, tag+0x2e4] scaled by 30.0 (ticks per second).
 * Called from item_set_position when the item is flagged for detonation
 * and the game engine is not running (campaign/solo mode). */
void item_detonate(int item_handle)
{
  char *item_obj;
  char *item_tag;
  int *seed;
  float rnd;

  item_obj = (char *)object_get_and_verify_type(item_handle, 0x1c);
  item_tag = (char *)tag_get(0x6974656d, *(int *)item_obj);

  if (*(int16_t *)(item_obj + 0x1a8) == 0) {
    FUN_0009ec30(
      *(int *)(item_tag + 0x2f4), item_handle,
      item_handle, /* dup-args-ok: same handle as source and target */
      NONE, 0, 0, 0, 0);
    seed = get_global_random_seed_address();
    rnd = random_real_range(seed, *(float *)(item_tag + 0x2e0),
                            *(float *)(item_tag + 0x2e4));
    *(int16_t *)(item_obj + 0x1a8) = (int16_t)(int)(rnd * 30.0f);
  }
}

/* Update an item's angular velocity state (0xf6b80).
 * Computes the magnitude of the angular velocity vector at +0x3c..+0x44.
 * If nonzero: sets flag bit 2 (has angular velocity) at +0x1a4, normalizes
 * the angular velocity into the direction vector at +0x1c8..+0x1d0 (unless
 * object flag bit 5 at +0x4 is set, indicating externally driven rotation),
 * then stores sin(magnitude) at +0x1d4 and cos(magnitude) at +0x1d8.
 * If zero: clears flag bit 2 and sets sin=0, cos=1 (identity rotation). */
void FUN_000f6b80(int item_handle)
{
  char *item_obj;
  float x, y, z;
  float mag;
  float inv_mag;

  item_obj = (char *)object_get_and_verify_type(item_handle, 0x1c);
  x = *(float *)(item_obj + 0x3c);
  y = *(float *)(item_obj + 0x40);
  z = *(float *)(item_obj + 0x44);
  mag = sqrtf(x * x + y * y + z * z);

  if (mag != 0.0f) {
    *(uint32_t *)(item_obj + 0x1a4) = *(uint32_t *)(item_obj + 0x1a4) | 4;

    if (!(*(uint8_t *)(item_obj + 0x4) & 0x20)) {
      inv_mag = 1.0f / mag;
      *(float *)(item_obj + 0x1c8) = inv_mag * x;
      *(float *)(item_obj + 0x1cc) = inv_mag * y;
      *(float *)(item_obj + 0x1d0) = inv_mag * z;
    }

    *(float *)(item_obj + 0x1d4) = x87_fsin(mag);
    *(float *)(item_obj + 0x1d8) = x87_fcos(mag);
  } else {
    *(uint32_t *)(item_obj + 0x1a4) = *(uint32_t *)(item_obj + 0x1a4) & ~4u;
    *(float *)(item_obj + 0x1d4) = 0.0f;
    *(float *)(item_obj + 0x1d8) = 1.0f;
  }
}

/* valid_real_vector3d_axes3 (0xf6c40)
 *
 * Validate that three vectors (a, b, c) form a valid orthonormal axis triple:
 * each must be a unit normal (valid_real_normal3d), and each adjacent pair
 * must be mutually perpendicular (dot ~= 0). FUN_00021f70 is the two-float
 * approximate-equality test (dot, 0.0). FUN_00013070(c, a) returns the
 * scalar (dot) that must also be ~0. Returns 1 if all checks pass, else 0.
 *
 * FP term order preserved from disassembly (MSVC scheduling emits the sum
 * components in 0,2,1 order); the sum is commutative so this is harmless
 * mathematically but kept for byte-fidelity. */
char valid_real_vector3d_axes3(float *a, float *b, float *c)
{
  float scalar;

  if (valid_real_normal3d(a)) {
    if (valid_real_normal3d(b)) {
      if (valid_real_normal3d(c)) {
        if ((char)FUN_00021f70(a[0] * b[0] + a[2] * b[2] + a[1] * b[1], 0.0f) !=
            '\0') {
          if ((char)FUN_00021f70(b[0] * c[0] + c[2] * b[2] + c[1] * b[1],
                                 0.0f) != '\0') {
            scalar = FUN_00013070(c, a);
            if ((char)FUN_00021f70(scalar, 0.0f) != '\0') {
              return '\x01';
            }
          }
        }
      }
    }
  }
  return '\0';
}

/* valid_real_matrix4x3 (0xf6d00)
 *
 * Validate a 4x3 affine matrix: 1 scale scalar @ +0x00, three orthonormal
 * axis vectors @ +0x04/+0x10/+0x1C, and a translation point @ +0x28.
 * The scale scalar is finite (not inf/NaN) when its IEEE-754 exponent bits
 * (mask 0x7f800000) are NOT all set. Then the axis triple and the point are
 * validated by their respective helpers. Returns 1 only when every check
 * passes; 0 on any failure or non-finite scale.
 *
 * Ghidra mistyped this void(void); it is a bool-returning cdecl fn taking one
 * float* matrix pointer (proven by the render_cameras.c thunk typedef). Nested
 * -if shape preserved: single success return, fall-through failure. */
char valid_real_matrix4x3(float *mat)
{
  if ((*(uint32_t *)mat & 0x7f800000) != 0x7f800000) {
    if (valid_real_vector3d_axes3(mat + 1, mat + 4, mat + 7) != '\0') {
      if (valid_real_point3d(mat + 10)) {
        return '\x01';
      }
    }
  }
  return '\0';
}

/* item_set_position (0xf6d60)
 *
 * Apply a velocity/position delta to an item and update its angular velocity.
 * Manages collision user depth, ground clamping, angular tumble, and garbage
 * flag state. Called each tick to move free-floating items (grenades, weapons,
 * equipment on the ground).
 *
 * If the item is grounded (flag bit 3 at +0x1a4) and the delta magnitude is
 * above a small epsilon, the item is repositioned to just above the ground
 * plane via a "ground point" marker lookup and plane projection. Otherwise
 * position is simply accumulated. Angular velocity gets a tumble impulse from
 * cross(global_up, delta) scaled by a random factor and pi/2, or a random
 * angular jolt from the ground normal when velocity is near zero.
 *
 * Confirmed: 3 cdecl args (item_handle, position, flag).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with type_mask 0x1c.
 * Confirmed: CALL 0x1ba140 (tag_get) with 'item' (0x6974656d).
 * Confirmed: CALL 0x8d9f0 (display_assert) for collision depth checks.
 * Confirmed: CALL 0xa8e30 (game_engine_running) for flag-dependent branch.
 * Confirmed: CALL 0xf6af0 (item_detonate) if flag set and engine not running.
 * Confirmed: CALL 0x140f10 (object_get_markers_by_string_id) for "ground
 * point". Confirmed: CALL 0x18e3f0 (global_collision_bsp_get) to get collision
 * BSP. Confirmed: CALL 0x19b210 (tag_block_get_element) at bsp+0x3c. Confirmed:
 * CALL 0x99640 (bsp3d_get_plane_from_designator) for plane extraction.
 * Confirmed: CALL 0x12f80 (vector3d_scale_add) for ground projection.
 * Confirmed: CALL 0x143be0 (object_translate) for repositioning item.
 * Confirmed: CALL 0x12170 (FUN_00012170) for vector magnitude. Confirmed: CALL
 * 0x10b0d0 (get_global_random_seed_address). Confirmed: CALL 0x10b240
 * (random_math_real) for random scale. Confirmed: CALL 0x13010 (normalize3d)
 * for cross product normalization. Confirmed: CALL 0x10b380
 * (random_seed_get_direction3d) for degenerate case. Confirmed: CALL 0x121e0
 * (FUN_000121e0) for random angle
 * [-pi/4, pi/4]. Confirmed: CALL 0x213c0 (vector3d_add) for angular velocity
 * accumulation. Confirmed: CALL 0xf6b80 (FUN_000f6b80) with item_handle in EAX.
 * Confirmed: CALL 0x13d920 (object_set_garbage_flag) with (handle, 0).
 * Confirmed: global collision depth at 0x4761d8 (int16_t).
 * Confirmed: collision user stack at 0x5a8c80.
 * Confirmed: global up vector pointer at 0x31fc44 → {0, 0, 1}.
 * Confirmed: epsilon constant at 0x253f44 = ~0.0001f.
 * Confirmed: offset constant at 0x2533e8 = 0.05f.
 * Confirmed: zero constant at 0x2533c0 = 0.0f.
 * Confirmed: pi/2 constant at 0x2568bc = ~1.5708f.
 */
void item_set_position(int item_handle, float *position, int flag)
{
  char *item_obj;
  char *item_tag;
  int16_t marker_count;
  char marker_buf[0x6c];
  float plane[4];
  float cross[3];
  float vel_mag;
  float new_pos[3];
  float scaled_dir[3];
  float *up;
  float scale;
  int bsp;
  int *plane_ref;
  float dot;

  item_obj = (char *)object_get_and_verify_type(item_handle, 0x1c);
  item_tag = (char *)tag_get(0x6974656d, *(int *)item_obj);

  /* Early out if item has flag bit 5 set at +0x1a4 */
  if (*(uint8_t *)(item_obj + 0x1a4) & 0x20)
    return;

  /* Collision depth guard */
  if (*(int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\items\\items.c", 0x218, 1);
    system_exit(-1);
  }

  {
    int16_t depth = *(int16_t *)0x4761d8;
    *(int16_t *)(0x5a8c80 + (int)depth * 2) = 0xb;
    *(int16_t *)0x4761d8 = depth + 1;
  }

  /* Only process if parent object handle (obj+0xCC) is NONE */
  if (*(int *)(item_obj + 0xcc) == NONE) {
    /* If flag param is set, game engine not running, and tag flag bit 1 set,
     * call item_detonate (possibly spawns pickup effect or similar) */
    if ((char)flag != 0) {
      if (!game_engine_running()) {
        if (*(uint8_t *)(item_tag + 0x17c) & 2) {
          item_detonate(item_handle);
        }
      }
    }

    /* Check ground flag (bit 3 of item_flags at +0x1a4) */
    if (!(*(uint8_t *)(item_obj + 0x1a4) & 0x8)) {
      /* Not grounded: just clear the "needs update" flag bit 5 at +0x04 */
      *(uint32_t *)(item_obj + 0x04) =
        *(uint32_t *)(item_obj + 0x04) & 0xffffffdf;
    } else if (*(float *)0x253f44 <= position[0] * position[0] +
                                       position[1] * position[1] +
                                       position[2] * position[2]) {
      /* Grounded and velocity magnitude squared >= epsilon:
       * Try to reposition item to ground plane */
      marker_count = object_get_markers_by_string_id(
        item_handle, (void *)0x28aa90, marker_buf, 1);
      if (marker_count != 0) {
        /* Ground point marker found: project position onto ground plane */
        bsp = (int)global_collision_bsp_get();
        plane_ref = (int *)tag_block_get_element(
          (void *)(bsp + 0x3c), (int)*(int16_t *)(item_obj + 0x1aa), 0xc);
        bsp3d_get_plane_from_designator(bsp, *plane_ref, plane);

        /* Compute offset from plane: 0.05 - (dot(normal, ground_pos) -
         * distance) */
        dot = plane[0] * *(float *)(marker_buf + 0x60) +
              plane[1] * *(float *)(marker_buf + 0x64) +
              plane[2] * *(float *)(marker_buf + 0x68);
        scale = *(float *)0x2533e8 - (dot - plane[3]);

        vector3d_scale_add((float *)(marker_buf + 0x60), plane, scale, new_pos);
        object_translate(item_handle, new_pos, 0);
      }
      /* Clear "needs update" bit 5 at +0x04 and ground bit 3 at +0x1a4 */
      *(uint32_t *)(item_obj + 0x04) =
        *(uint32_t *)(item_obj + 0x04) & 0xffffffdf;
      *(uint32_t *)(item_obj + 0x1a4) =
        *(uint32_t *)(item_obj + 0x1a4) & 0xfffffff7;
    }

    /* Accumulate position delta onto object position at +0x18 */
    *(float *)(item_obj + 0x18) += position[0];
    *(float *)(item_obj + 0x1c) += position[1];
    *(float *)(item_obj + 0x20) += position[2];

    /* Determine angular velocity update path */
    if (*(int *)(item_obj + 0x1b0) != NONE ||
        !(*(uint8_t *)(item_obj + 0x1a4) & 0x8) ||
        *(float *)0x253f44 <= FUN_00012170(position)) {
      /* Normal tumble: compute angular velocity from cross product */
      vel_mag = sqrtf(position[0] * position[0] + position[1] * position[1] +
                      position[2] * position[2]);
      if (vel_mag < *(float *)0x253f44) {
        unsigned int *seed = (unsigned int *)get_global_random_seed_address();
        vel_mag = random_math_real(seed);
      }

      /* cross = cross(global_up, position_delta) */
      up = *(float **)0x31fc44;
      cross[2] = up[0] * position[1] - up[1] * position[0];
      cross[1] = position[0] * up[2] - up[0] * position[2];
      cross[0] = up[1] * position[2] - position[1] * up[2];

      if (normalize3d(cross) <= *(float *)0x2533c0) {
        /* Degenerate cross product: use random direction */
        unsigned int *seed = (unsigned int *)get_global_random_seed_address();
        random_seed_get_direction3d(seed, cross);
      }

      {
        unsigned int *seed = (unsigned int *)get_global_random_seed_address();
        float factor = random_math_real(seed) * vel_mag * *(float *)0x2568bc;
        *(float *)(item_obj + 0x3c) += cross[0] * factor;
        *(float *)(item_obj + 0x40) += cross[1] * factor;
        *(float *)(item_obj + 0x44) += cross[2] * factor;
      }
    } else {
      /* Slow/grounded path: apply random angular jolt from ground normal */
      marker_count = object_get_markers_by_string_id(
        item_handle, (void *)0x28aa90, marker_buf, 1);
      if (marker_count == 0) {
        /* No marker: use global up vector as normal */
        up = *(float **)0x31fc44;
        cross[0] = up[0];
        cross[1] = up[1];
        cross[2] = up[2];
      } else {
        /* Use ground point marker normal (at marker+0x54) */
        cross[0] = *(float *)(marker_buf + 0x54);
        cross[1] = *(float *)(marker_buf + 0x58);
        cross[2] = *(float *)(marker_buf + 0x5c);
      }

      /* Random angle in [-pi/4, pi/4] */
      scale = FUN_000121e0(-1.5707963f, 1.5707963f);
      scaled_dir[0] = cross[0] * scale;
      scaled_dir[1] = cross[1] * scale;
      scaled_dir[2] = cross[2] * scale;

      /* angular_velocity += scaled_dir */
      vector3d_add(
        (float *)(item_obj + 0x3c), scaled_dir,
        (float *)(item_obj + 0x3c)); /* dup-args-ok: in-place accumulation */
    }

    /* Update item velocity/angular state and clear garbage flag */
    FUN_000f6b80(item_handle);
    object_set_garbage_flag(item_handle, 0);
  }

  /* Collision depth unguard */
  if (*(int16_t *)0x4761d8 < 2) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\items\\items.c", 0x28b, 1);
    system_exit(-1);
  }
  *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 - 1;
}
/* virtual_keyboard.c — on-screen (IME-style) text entry state machine.
 *
 * TU: c:\halo\SOURCE\interface\virtual_keyboard.c  (per __FILE__ assert string).
 * kb.json currently files 0xf5500 under items.obj; the assert __FILE__ proves
 * the real translation unit is interface/virtual_keyboard.c.
 *
 * The virtual-keyboard state lives in a packed, mixed-width global block based
 * at 0x46cef0 ("virtual_keyboard_globals"). Field widths are preserved exactly
 * (u8 / u16 / u32 / ptr / wchar[32]); do NOT promote the narrow stores to int.
 * Layout used here (offset from 0x46cef0):
 *   +0x00 u8   active flag
 *   +0x01 u8   (cleared)
 *   +0x02 u8   (cleared)
 *   +0x03 u8   (cleared)
 *   +0x04 u32  readiness gate (read-only here)
 *   +0x06 u8   (cleared)
 *   +0x07 u8   set to 1
 *   +0x08 u16  cursor/selection lo (cleared)
 *   +0x0a u16  cursor/selection hi (cleared)
 *   +0x0c u16  buffer_size, clamped <= 0x40 (unsigned)
 *   +0x0e u16  0xffff sentinel
 *   +0x14 u16  caption_index
 *   +0x16 u8   (cleared)
 *   +0x18 ptr  text_buffer
 *   +0x1c ptr  text_buffer end = base + ustrlen(base) (wchar_t* arithmetic)
 *   +0x20 u32  FUN_001d0581() result
 *   +0x28 wchar[32]  ustrncpy of caller text
 *   +0x66 u16  0
 */

/* virtual_keyboard_set_validation — begin a validated virtual-keyboard entry
 * session over the caller's wchar_t buffer. Asserts the inputs (non-null buffer,
 * non-zero even byte size, no session already active) and that caption_index is
 * a valid virtual-keyboard caption string index. If the subsystem is not ready
 * (or a session is somehow active), returns false without changing state.
 * Otherwise flushes pending UI events, initializes the state block, seeds the
 * edit buffer, plays the forward audio cue and returns true.
 *
 * cdecl, bool return in AL (MOV AL,1 success / XOR AL,AL failure). */
bool virtual_keyboard_set_validation(wchar_t *text_buffer, unsigned short buffer_size,
                                     short caption_index)
{
  int len;

  assert_halt_msg(text_buffer && buffer_size && !(buffer_size & 1) && !*(uint8_t *)0x46cef0,
                  "text_buffer && buffer_size && !(buffer_size&1) && !virtual_keyboard_globals.active");
  assert_halt_msg((caption_index > 7) && (caption_index < 0xb),
                  "(caption_index>=FIRST_VIRTUAL_KEYBOARD_CAPTION_STRING_INDEX) && (caption_index<NUMBER_OF_VIRTUAL_KEYBOARD_STRINGS)");

  if (*(uint8_t *)0x46cef0 != 0 || *(uint32_t *)0x46cef4 == 0)
    return false;

  event_manager_flush();

  *(uint16_t *)0x46cef8 = 0;
  *(uint16_t *)0x46cefa = 0;
  *(uint8_t *)0x46cef0 = 1;
  *(wchar_t **)0x46cf08 = text_buffer;
  len = ustrlen(text_buffer);
  *(wchar_t **)0x46cf0c = text_buffer + len;
  *(uint16_t *)0x46cefc = buffer_size;
  if (buffer_size >= 0x40)
    *(uint16_t *)0x46cefc = 0x40;
  *(uint16_t *)0x46cefe = 0xffff;
  *(uint32_t *)0x46cf10 = (uint32_t)FUN_001d0581();
  *(uint16_t *)0x46cf04 = (uint16_t)caption_index;
  *(uint8_t *)0x46cef1 = 0;
  *(uint8_t *)0x46cef2 = 0;
  *(uint8_t *)0x46cef3 = 0;
  *(uint8_t *)0x46cef7 = 1;
  ustrncpy((wchar_t *)0x46cf18, text_buffer, 0x20);
  *(uint16_t *)0x46cf56 = 0;
  *(uint8_t *)0x46cef6 = 0;
  ui_play_audio_feedback_sound(2);
  return true;
}
