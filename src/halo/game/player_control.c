/* Return a pointer to the player control data slot for a local player.
 * Each slot is 0x40 bytes, starting at offset 0x10 in the globals struct.
 *
 * The original never inlines this helper: 0xb6380 exists as a real function and
 * every in-TU caller (0xb6a20, 0xb6a70, ...) emits PUSH/CALL 0xb6380 rather
 * than the slot arithmetic.  MSVC's auto-inliner does expand it into very small
 * callers (player_control_get_zoom_level went 12 -> 26 instructions), so the
 * expansion is suppressed here to reproduce the original codegen.  Guarded to
 * VC71 only; clang neither needs nor recognizes the pragma. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(off)
#endif
void *player_control_get_data(int16_t local_player_index)
{
  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return (char *)player_control_globals + local_player_index * 0x40 + 0x10;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma auto_inline(on)
#endif

/* Allocate the player control globals block out of the game state heap.
 * Binary: PUSH 0x110 / PUSH 0 / PUSH "player control globals" /
 * CALL game_state_malloc / ADD ESP,0xc / MOV [player_control_globals],EAX.
 * The 0x110 literal is exactly sizeof(player_control_globals_t). */
void player_control_initialize(void)
{
  player_control_globals = (player_control_globals_t *)game_state_malloc(
    "player control globals", NULL, sizeof(player_control_globals_t));
}

void player_control_dispose(void)
{
}

/* Toggle bit 0 of the flag dword at player_control_globals+0xc.
 * NOTE: the polarity is INVERTED relative to the name and is preserved as-is
 * from the binary: a zero argument SETS bit 0, a non-zero argument CLEARS it.
 * Disassembly (0xb6430): MOV AL,[EBP+8] / TEST AL,AL /
 * MOV ECX,[player_control_globals] / MOV EDX,[ECX+0xc] / JNZ ->AND 0xfffffffe;
 * fallthrough ->OR 0x1.  Both the pointer load and the dword load are hoisted
 * above the branch by MSVC, so they are written once here.
 * player_control_globals_t has no named field at +0xc (opaque 0x110 bytes),
 * hence the raw offset arithmetic. */
void scripted_player_control_set_camera_control(bool camera_control)
{
  uint32_t *flags;
  uint32_t value;

  flags = (uint32_t *)((char *)player_control_globals + 0xc);
  value = *flags;
  if (!camera_control)
    *flags = value | 1u;
  else
    *flags = value & 0xfffffffeu;
}

/* Set action flags on a local player's control slot.
 * ORs the given flags into the player's action_flags field, and
 * optionally into the persistent_action_flags field as well. */
void player_control_set_action_flags(int16_t local_player_index, uint16_t flags,
                                     bool persistent)
{
  player_control_t *pc;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  pc = (player_control_t *)((char *)player_control_globals +
                            local_player_index * 0x40 + 0x10);
  pc->action_flags |= flags;
  if (persistent)
    pc->persistent_action_flags |= flags;
}

/* Evaluate a piecewise-linear transfer function sampled at `count` evenly
 * spaced points over [0,1], with odd symmetry about 0 (the sign of `input`
 * is stripped up front and re-applied to the result).
 *
 * Binary shape (0xb64c0):
 *   [EBP-0x1] negate flag, [EBP-0x8] count-1, [EBP-0xC] int scratch that
 *   holds (int)count and is later reused for low_index. The clamped
 *   position `t` reuses the incoming `input` stack slot [EBP+0x10], so the
 *   parameter is written back to here as well.
 *   0x64cd FCOMP 0.0f  -> negate = (input < 0.0f)
 *   0x64f6 FCOM  0.0   -> lower clamp (double 0.0 literal @0x2602c0)
 *   0x6510 FSUB  1.0f  -> (float)count - 1.0f  (@0x2533c8)
 *   0x6594 FSUB  f[low]  => (f[high] - f[low])
 *   0x659a FSUBR t       => (t - (float)low_index)
 *   0x659f FADD  f[low]  => + f[low]
 * `count` is read as a WORD (MOVSX / CMP BX,word) so it is int16_t, and the
 * _ftol2 result is consumed 16-bit (TEST AX,AX). The assert runs after the
 * clamping, so it can only fire on count <= 0 or function == NULL. */
float evaluate_piecewise_linear_function(int16_t count, float *function,
                                         float input)
{
  bool negate;
  int32_t count_minus_1;
  int16_t low_index;
  int16_t high_index;
  float result;

  negate = input < 0.0f;
  count_minus_1 = count - 1;

  /* PIN(fabs(input)*(count-1), 0.0, count-1.0f) -- written as the macro
   * expansion so the scaled value stays CSE'd in ST0 and the clamp has a
   * single shared store into the `input` slot (0x64f6..0x6529). */
  input = (float)(fabs(input) * count_minus_1 < 0.0 ?
                    0.0 :
                    (fabs(input) * count_minus_1 > count - 1.0f ?
                       count - 1.0f :
                       fabs(input) * count_minus_1));

  low_index = (int16_t)input;
  if (low_index < 0)
    low_index = 0;
  else if (low_index > count_minus_1)
    low_index = (int16_t)count_minus_1;

  high_index = low_index + 1;
  if (high_index > count_minus_1)
    high_index = (int16_t)count_minus_1;

  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0x14b,
                 function && low_index >= 0 && low_index <= high_index &&
                   high_index < count);

  result = (function[high_index] - function[low_index]) * (input - low_index) +
           function[low_index];
  if (negate)
    result = -result;
  return result;
}

/* Return the aiming unit index for a local player's controlled unit.
 * Binary (0xb65c0): MOVSX ESI,word [EBP+8] bounds-checked against
 * [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS), then
 *   MOV ECX,[player_control_globals] / SHL EAX,6 /
 *   MOV EDX,[EAX+ECX+0x10] / LEA EAX,[EAX+ECX+0x10] / PUSH EDX /
 *   CALL unit_get_aiming_unit_index / ADD ESP,4 / RET.
 * The LEA computes the slot pointer that the MOV already used, i.e. the
 * original called an inlined player_control_get_data; the `pc` local here
 * reproduces that shape.
 * The result is never touched after the call, so the return value is the
 * callee's implicit EAX (lift-learnings SS16 void-EAX) -- Ghidra's
 * `void (void)` is wrong on both the parameter and the return. */
int32_t player_control_get_aiming_unit_index(int16_t local_player_index)
{
  player_control_t *pc;

  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                 local_player_index >= 0 &&
                   local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  pc = (player_control_t *)((char *)player_control_globals +
                            local_player_index * 0x40 + 0x10);
  return unit_get_aiming_unit_index(pc->unit_index);
}

/* Return the target (aim-assist) object index for a local player, or NONE if
 * that object handle no longer resolves to a live object.
 * Binary (0xb6620): MOVSX ESI,word [EBP+8] bounds-checked against
 * [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) (same assert line 0xb1 as the sibling
 * above), then
 *   MOV ECX,[player_control_globals] / SHL EAX,6 /
 *   MOV EDX,[EAX+ECX+0x38] / LEA ESI,[EAX+ECX+0x10] /
 *   PUSH -1 / PUSH EDX / CALL object_try_and_get_and_verify_type / ADD ESP,8 /
 *   TEST EAX,EAX / JZ -> OR EAX,-1 ; else MOV EAX,[ESI+0x28] / RET.
 * The LEA reproduces the inlined player_control_get_data; EDX is the same slot
 * field the fallthrough re-reads, i.e. pc->target_object_index at +0x28
 * (globals base + idx*0x40 + 0x10 + 0x28 = the +0x38 displacement above).
 * The success path returns the datum HANDLE, not the pointer the callee
 * returned -- Ghidra's `void (void)` is wrong on both param and return
 * (lift-learnings SS16 void-EAX); the WORD load makes the param int16_t. */
int32_t player_control_get_target_object_index(int16_t local_player_index)
{
  player_control_t *pc;

  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                 local_player_index >= 0 &&
                   local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  pc = (player_control_t *)((char *)player_control_globals +
                            local_player_index * 0x40 + 0x10);
  if (object_try_and_get_and_verify_type(pc->target_object_index, -1) != NULL)
    return pc->target_object_index;
  return NONE;
}

/* Return the field of view (radians) the local player's view should use.
 * Default is the tag-less fallback 1.2217305 (== 70 degrees, the float at
 * 0x26e270); the original FLDs it before the unit_index test and FSTPs it
 * again on the taken branch, so it is initialised up front here.
 * With a unit: resolve the unit object, fetch its 'unit' tag, and ask the
 * currently held weapon (unit+0x2a2 is the weapon slot index, loaded
 * ZERO-extended) for its zoomed field of view; the weapon call is a tail
 * return of ST0. With no weapon the unit tag's own base field of view
 * (unit_tag+0x1a0) is used instead.
 * tag_get's result is live in EBX across unit_get_weapon -- Ghidra discards
 * it (lift-learnings SS11 discarded-result); it is the source of both
 * +0x1a0 reads. Ghidra also reports `void (void)`: the parameter is the
 * MOVSX word at [EBP+8] and the return is a float in ST0. */
real player_control_get_field_of_view(int16_t local_player_index)
{
  player_control_t *pc;
  char *unit_obj;
  char *unit_tag;
  int weapon_handle;
  real field_of_view;

  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                 local_player_index >= 0 &&
                   local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  field_of_view = 1.2217305f;
  pc = (player_control_t *)((char *)player_control_globals +
                            local_player_index * 0x40 + 0x10);
  if (pc->unit_index != NONE) {
    unit_obj = (char *)object_get_and_verify_type(pc->unit_index, 3);
    unit_tag = (char *)tag_get(0x756e6974 /* 'unit' */, *(int *)unit_obj);
    weapon_handle =
      unit_get_weapon(pc->unit_index, *(uint16_t *)(unit_obj + 0x2a2));
    if (weapon_handle != NONE)
      return weapon_get_field_of_view(weapon_handle,
                                      *(real *)(unit_tag + 0x1a0),
                                      (uint16_t)pc->desired_zoom_level);
    field_of_view = *(real *)(unit_tag + 0x1a0);
  }
  return field_of_view;
}

/* Fill a camera-info block for a local player's controlled unit.
 *
 * camera_info layout (the caller reserves 0x28 bytes -- see
 * player_control_update_desired_angles):
 *   +0x00 int     object handle the camera follows (unit, or its vehicle)
 *   +0x04 int16   seat index within that vehicle (NONE when on foot)
 *   +0x08 void*   camera/seat limit block (vehicle seat +0x84, else unit
 *                 tag +0x1a8)
 *   +0x0c real[3] seat position, written by unit_set_seat_state (0x1a9240)
 *
 * On foot the block comes from the unit's own 'unit' tag. When the unit is
 * riding something (unit+0xcc is the vehicle handle, unit+0x2a0 the seat
 * index) the vehicle's 'vehi' seat block (tag+0x2e4, stride 0x11c) supplies
 * both the handle and the limit block, and the object pointer is re-resolved
 * against the vehicle. object_try_and_get_and_verify_type is used for the
 * vehicle so a stale handle simply leaves the on-foot result in place.
 *
 * c:\halo\SOURCE\game\player_control.c */
void player_control_get_unit_camera_info(int16_t local_player_index,
                                         void *camera_info)
{
  char *info;
  player_control_t *pc;
  char *unit_obj;
  char *vehicle_obj;
  char *seat;
  int handle;

  info = (char *)camera_info;
  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0x402, info);
  *(void **)(info + 8) = NULL;

  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                 local_player_index >= 0 &&
                   local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  pc = (player_control_t *)((char *)player_control_globals +
                            (int)local_player_index * 0x40 + 0x10);
  handle = pc->unit_index;
  *(int *)info = handle;
  *(int16_t *)(info + 4) = NONE;
  if (handle != NONE) {
    unit_obj = (char *)object_get_and_verify_type(handle, 3);
    unit_set_seat_state(*(int *)info, (real *)(info + 0xc));
    if (*(int *)(unit_obj + 0xcc) != NONE) {
      vehicle_obj = (char *)object_try_and_get_and_verify_type(
        *(int *)(unit_obj + 0xcc), 2);
      if (vehicle_obj != NULL) {
        seat = (char *)tag_block_get_element(
          (char *)tag_get(0x76656869 /* 'vehi' */, *(int *)vehicle_obj) + 0x2e4,
          *(int16_t *)(unit_obj + 0x2a0), 0x11c);
        handle = *(int *)(unit_obj + 0xcc);
        *(int *)info = handle;
        *(void **)(info + 8) = seat + 0x84;
        *(int16_t *)(info + 4) = *(int16_t *)(unit_obj + 0x2a0);
        unit_obj = (char *)object_get_and_verify_type(handle, 3);
      }
    }
    if (*(int16_t *)(info + 4) == NONE)
      *(void **)(info + 8) =
        (char *)tag_get(0x756e6974 /* 'unit' */, *(int *)unit_obj) + 0x1a8;
  }
}

/* Return the object handle of the unit a local player is currently
 * controlling.
 * Binary (0xb6870): MOVSX ESI,word [EBP+8] bounds-checked against
 * [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) (same assert line 0xb1 as the
 * siblings above), then
 *   MOV ECX,[player_control_globals] / MOVSX EAX,SI / SHL EAX,6 /
 *   MOV EAX,[EAX+ECX+0x10] / RET.
 * The sign-extend of the short is explicit in the original, so the parameter
 * is signed -- an unsigned load would make the `< 0` half of the bounds check
 * (TEST SI,SI / JL) unreachable.  The dword at slot+0x0 is pc->unit_index.
 * Assert tail is the system_exit(-1) flavor (CALL 0x8d9f0 display_assert then
 * PUSH -1 / CALL 0x8e2f0), not halt_and_catch_fire. */
int32_t player_control_get_unit_index(int16_t local_player_index)
{
  player_control_t *pc;

  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                 local_player_index >= 0 &&
                   local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  pc = (player_control_t *)((char *)player_control_globals +
                            local_player_index * 0x40 + 0x10);
  return pc->unit_index;
}

/* Return the weapon handle a local player wants to be holding for a unit.
 * Binary (0xb68c0): bounds-checks local_player_index exactly like its
 * siblings above (assert line 0xb1, system_exit(-1) tail flavor), then
 *   MOV ECX,[player_control_globals] / MOVSX EAX,SI / SHL EAX,6 /
 *   LEA EAX,[EAX+ECX+0x10]           -> the local player's control slot
 *   CMP [EAX],ESI                    -> slot->unit_index == unit_handle?
 *   XOR EDX,EDX / MOV DX,[EAX+0x20]  -> slot->desired_weapon_index
 *   PUSH EDX / PUSH ESI / CALL unit_get_weapon / CMP EAX,-1 / JNZ ->return
 * and otherwise falls through to
 *   PUSH 3 / PUSH ESI / CALL object_get_and_verify_type
 *   MOVSX EAX,word [EAX+0x2a2] / PUSH EAX / PUSH ESI / CALL unit_get_weapon
 *   ADD ESP,0x10 (MSVC merged both slow-path cleanups; each call still
 *                 pushes exactly two args) / POP ESI / POP EBP / RET.
 * Ghidra's `void (void)` is wrong on BOTH the parameters and the return:
 * the RET does no callee cleanup (cdecl, two stack args) and both exits
 * leave unit_get_weapon's EAX untouched (lift-learnings SS16 void-EAX).
 * The two 16-bit loads deliberately differ in extension and are preserved:
 * slot+0x20 is ZERO-extended (XOR EDX,EDX / MOV DX), unit+0x2a2 is
 * SIGN-extended (MOVSX) -- see the identical unit+0x2a2 reads above. */
int player_control_get_desired_weapon(int16_t local_player_index,
                                      int unit_handle)
{
  player_control_t *pc;
  char *unit_obj;
  int weapon_handle;

  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                 local_player_index >= 0 &&
                   local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  pc = (player_control_t *)((char *)player_control_globals +
                            local_player_index * 0x40 + 0x10);
  if (pc->unit_index == unit_handle) {
    weapon_handle =
      unit_get_weapon(unit_handle, (uint16_t)pc->desired_weapon_index);
    if (weapon_handle != NONE)
      return weapon_handle;
  }
  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
  return unit_get_weapon(unit_handle, *(int16_t *)(unit_obj + 0x2a2));
}

/* Return the aim-assist ("autoaim") level for a local player.
 * Frame is PUSH EBP / MOV EBP,ESP / PUSH ESI; the index is loaded as a
 * 16-bit value (MOV SI,[EBP+8]) and range-checked with signed compares
 * (TEST SI,SI / JL; CMP SI,4 / JL), so the parameter is int16_t, not int.
 * The success path re-reads the globals pointer (MOV ECX,[0x457090] --
 * not hoisted), sign-extends the index (MOVSX EAX,SI), scales it by the
 * 0x40 slot stride (SHL EAX,6) and returns FLD [EAX+ECX+0x3c] in ST(0),
 * i.e. player_control_t+0x2c relative to the 0x10-based slot array.
 * The failure path is display_assert(...) + system_exit(-1) (CALL 0x8e2f0,
 * NOT halt_and_catch_fire -- Ghidra's thunk_FUN_001029a0 is wrong here). */
float player_control_get_autoaim_level(int16_t local_player_index)
{
  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                 local_player_index >= 0 &&
                   local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return ((player_control_t *)((char *)player_control_globals +
                               local_player_index * 0x40 + 0x10))
    ->autoaim_level;
}

/* Get the local player index for the player controlling a unit.
 * Looks up the unit's player handle (unit+0x1c8), then reads the local
 * player index (player+0x2) from the player datum. Returns NONE (0xffff)
 * if the unit has no controlling player. */
int16_t unit_get_local_player_index(int unit_handle)
{
  char *unit_obj;
  int player_handle;
  char *player;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
  player_handle = *(int *)(unit_obj + 0x1c8);
  if (player_handle != NONE) {
    player = (char *)datum_get(player_data, player_handle);
    return *(int16_t *)(player + 0x2);
  }
  return (int16_t)NONE;
}

/* Reset every local player's desired zoom level to NONE (un-zoom all).
 * No frame (PUSH ESI / PUSH EDI only): ESI is the 16-bit loop counter and
 * EDI is a byte-offset accumulator advanced by the 0x40 slot stride.  The
 * loop head re-tests the inlined player_control_get_data assert on every
 * iteration (TEST SI,SI / JL fail; CMP SI,4 / JL body), and the tail test
 * (CMP SI,4 / JGE return) is the loop's own bound -- so the assert is not
 * hoisted out.  The globals pointer is re-read inside the loop
 * (MOV EAX,[0x457090]) and the store is a WORD write of 0xffff to
 * [EDI+EAX+0x34], i.e. player_control_t+0x24 (desired_zoom_level) relative
 * to the 0x10-based slot array.  The failure path is display_assert(...)
 * + system_exit(-1), the assert_halt flavor. */
void players_unzoom_all(void)
{
  int16_t local_player_index;
  player_control_t *pc;

  for (local_player_index = 0;
       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
       local_player_index++) {
    assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                   local_player_index >= 0 &&
                     local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
    pc = (player_control_t *)((char *)player_control_globals +
                              local_player_index * 0x40 + 0x10);
    pc->desired_zoom_level = NONE;
  }
}

/* Clear the aim-assist weapon interaction slot for a unit's controlling player.
 * Looks up the player datum via the unit's player handle (unit+0x1c8), then
 * finds the local player index (player+0x2), retrieves the player control slot,
 * and resets the weapon interaction field (slot+0x24) to NONE. */
void player_clear_aim_assist(int unit_handle)
{
  char *unit_obj;
  int player_handle;
  char *player;
  int16_t local_player_index;
  player_control_t *pc;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
  player_handle = *(int *)(unit_obj + 0x1c8);
  if (player_handle != NONE) {
    player = (char *)datum_get(player_data, player_handle);
    local_player_index = *(int16_t *)(player + 0x2);
    if (local_player_index != NONE) {
      pc = (player_control_t *)player_control_get_data(
        (int16_t)local_player_index);
      pc->desired_zoom_level = NONE;
    }
  }
}

/* Return the current zoom (magnification) level for a local player, or NONE
 * when the index is NONE.
 *
 * The default result is hoisted before the guard (OR EAX,0xffffffff at 0xb6a76)
 * and only the low word is overwritten on the taken path (MOV AX,word
 * [EAX+0x24] at 0xb6a85), so the field is a int16_t load, not a
 * sign/zero-extended dword. The guard compares CX (16-bit) against -1. */
int16_t player_control_get_zoom_level(int16_t local_player_index)
{
  player_control_t *pc;
  int16_t zoom_level;

  zoom_level = NONE;
  if (local_player_index != NONE) {
    pc = (player_control_t *)player_control_get_data(local_player_index);
    zoom_level = pc->desired_zoom_level;
  }
  return zoom_level;
}

/* Zero the first two dwords of player_control_globals (+0x00 and +0x04).
 * Disassembly (0xb6a90): MOV EAX,[player_control_globals] /
 * MOV dword ptr [EAX],0x0 / MOV dword ptr [EAX+0x4],0x0 / RET.
 * The globals pointer is loaded exactly once into EAX and both stores go
 * through it, so the global is read once here rather than twice.
 * Both stores are full 32-bit dword width (MOV dword ptr, imm32).
 * player_control_globals_t is opaque (0x110 bytes) with no named fields at
 * +0x00/+0x04, so raw dword access is used.  The meaning of the two fields
 * is UNKNOWN; the name comes from kb.json. */
void player_control_action_test_reset(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  fields[0] = 0;
  fields[1] = 0;
}

/* Test (and mark as tested/used) the "accept" player action.
 *
 * Disassembly (0xb6ab0), 10 instructions, no calls:
 *   MOV EAX,[player_control_globals]   ; pointer loaded ONCE, reused for all
 *   MOV EDX,[EAX+0x4] / MOV ECX,0x4 / OR EDX,ECX / MOV [EAX+0x4],EDX
 *   OR  [EAX+0x8],ECX                  ; same constant, CSE'd into ECX
 *   MOV EAX,[EAX] / SHR EAX,0x2 / AND EAX,0x1 / RET
 *
 * NOTE: Ghidra decompiles this as `void` — it is NOT.  EAX is live at RET and
 * carries bit 2 of the dword at +0x00 (lift-learnings 16, void-EAX implicit
 * return).  kb.json's `void ...(void)` decl was corrected to a bool return.
 *
 * The constant 4 is FLAG(2) where 2 is the "accept" action's bit index; the
 * final SHR 2 / AND 1 is the matching TEST_FLAG.  The two ORs mark the action
 * in the two accumulator dwords at +0x04 and +0x08 (siblings 0xb6ad0 "back",
 * 0xb6af0 "action", 0xb6b10 "jump" use the same shape with other bit indices).
 * player_control_action_test_reset (0xb6a90) zeroes +0x00/+0x04 each frame.
 *
 * player_control_globals_t is opaque (0x110 bytes) with no named fields at
 * +0x00/+0x04/+0x08, so raw dword access is used; the exact meaning of the
 * three dwords is UNKNOWN beyond the bit-per-action encoding. */
bool player_control_action_test_accept(void)
{
  uint32_t *fields;
  uint32_t flag;

  fields = (uint32_t *)player_control_globals;
  flag = 1u << 2;
  fields[1] |= flag;
  fields[2] |= flag;
  return (bool)((fields[0] >> 2) & 1u);
}

/* Test (and mark as tested/used) the "back" player action.
 *
 * Disassembly (0xb6ad0), 10 instructions, no calls -- identical shape to
 * player_control_action_test_accept (0xb6ab0) with bit index 3 instead of 2:
 *   MOV EAX,[player_control_globals]   ; pointer loaded ONCE, reused for all
 *   MOV EDX,[EAX+0x4] / MOV ECX,0x8 / OR EDX,ECX / MOV [EAX+0x4],EDX
 *   OR  [EAX+0x8],ECX                  ; same constant, CSE'd into ECX
 *   MOV EAX,[EAX] / SHR EAX,0x3 / AND EAX,0x1 / RET
 *
 * NOTE: Ghidra decompiles this as `void` -- it is NOT.  EAX is live at RET and
 * carries bit 3 of the dword at +0x00 (lift-learnings 16, void-EAX implicit
 * return).  kb.json's `void ...(void)` decl was corrected to a bool return.
 *
 * The re-read of +0x00 happens AFTER both stores; the ordering is preserved
 * here.  The globals pointer is hoisted (single load), unlike the re-reading
 * sibling player_control_get_autoaim_level. */
bool player_control_action_test_back(void)
{
  uint32_t *fields;
  uint32_t flag;

  fields = (uint32_t *)player_control_globals;
  flag = 1u << 3;
  fields[1] |= flag;
  fields[2] |= flag;
  return (bool)((fields[0] >> 3) & 1u);
}

/* Test (and mark as tested/used) the "action" player action.
 *
 * Disassembly (0xb6af0), 9 instructions, no calls -- same shape as
 * player_control_action_test_accept (0xb6ab0)/_back (0xb6ad0) with bit index 0:
 *   MOV EAX,[player_control_globals]   ; pointer loaded ONCE, reused for all
 *   MOV EDX,[EAX+0x4] / MOV ECX,0x1 / OR EDX,ECX / MOV [EAX+0x4],EDX
 *   OR  [EAX+0x8],ECX                  ; same constant, CSE'd into ECX
 *   MOV EAX,[EAX] / AND EAX,ECX / RET
 *
 * One instruction shorter than the siblings because the bit index is 0: there
 * is no SHR, and the final mask reuses the CSE'd ECX (AND EAX,ECX) rather than
 * an immediate, so the `flag` local is used on the return path too.
 *
 * NOTE: Ghidra decompiles this as `void` -- it is NOT.  EAX is live at RET and
 * carries bit 0 of the dword at +0x00 (lift-learnings 16, void-EAX implicit
 * return).  kb.json's `void ...(void)` decl was corrected to a bool return.
 *
 * The two ORs mark the action in the accumulator dwords at +0x04 and +0x08;
 * the re-read of +0x00 happens AFTER both stores and that ordering is
 * preserved.  player_control_globals_t is opaque (0x110 bytes) with no named
 * fields at +0x00/+0x04/+0x08, so raw dword access is used. */
bool player_control_action_test_action(void)
{
  uint32_t *fields;
  uint32_t flag;

  fields = (uint32_t *)player_control_globals;
  flag = 1u << 0;
  fields[1] |= flag;
  fields[2] |= flag;
  return (bool)(fields[0] & flag);
}

/* Test the "jump" player action.
 *
 * Disassembly (0xb6b10), 5 instructions, no calls, no frame:
 *   MOV EAX,[player_control_globals]   ; pointer loaded ONCE
 *   MOV EAX,dword ptr [EAX]            ; dword at +0x00
 *   SHR EAX,0x1
 *   AND EAX,0x1
 *   RET
 *
 * NOTE: Ghidra decompiles this as `void` with an empty body -- it is NOT.
 * EAX is live at RET and carries bit 1 of the dword at +0x00
 * (lift-learnings 16, void-EAX implicit return).  kb.json's
 * `void ...(void)` decl was corrected to a bool return, matching the three
 * siblings 0xb6ab0/0xb6ad0/0xb6af0.
 *
 * UNLIKE those siblings (10 instructions each) this is a PURE test: there are
 * NO `OR` stores into the accumulator dwords at +0x04/+0x08, so the action is
 * not marked as consumed here.  Adding those ORs would be an invented side
 * effect.  Bit index is 1 (SHR EAX,0x1), i.e. FLAG(1) is the "jump" action bit.
 *
 * player_control_globals_t is opaque (0x110 bytes) with no named field at
 * +0x00, so raw dword access is used. */
bool player_control_action_test_jump(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (bool)((fields[0] >> 1) & 1u);
}

/* Set a player control slot's desired facing angles from a 3D direction vector.
 * Converts the direction vector to yaw+pitch via vector_to_angles (atan2-based
 * vector_to_angles), validates both angles for NaN/Inf, and normalizes yaw
 * to [0, 2*pi) by adding 2*pi if negative. */
void player_control_set_facing(uint16_t local_player_index, float *direction)
{
  player_control_t *pc;
  float *desired_yaw;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  pc = (player_control_t *)((char *)player_control_globals +
                            (int)(int16_t)local_player_index * 0x40 + 0x10);
  desired_yaw = &pc->desired_angles_yaw;

  /* Convert direction vector to yaw/pitch angles */
  vector_to_angles(desired_yaw, direction);

  /* assert_valid_real on desired_angles.pitch (slot+0x10) */
  if ((*(uint32_t *)&pc->desired_angles_pitch & 0x7f800000u) == 0x7f800000u) {
    char *msg = csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                         "player_control->desired_angles.pitch",
                         *(uint32_t *)&pc->desired_angles_pitch,
                         (double)pc->desired_angles_pitch);
    display_assert(msg, "c:\\halo\\SOURCE\\game\\player_control.c", 0xbb, 1);
    system_exit(NONE);
  }

  /* assert_valid_real on desired_angles.yaw (slot+0xc) */
  if ((*(uint32_t *)desired_yaw & 0x7f800000u) == 0x7f800000u) {
    char *msg = csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                         "player_control->desired_angles.yaw",
                         *(uint32_t *)desired_yaw, (double)*desired_yaw);
    display_assert(msg, "c:\\halo\\SOURCE\\game\\player_control.c", 0xbc, 1);
    system_exit(NONE);
  }

  /* Normalize yaw to [0, 2*pi) */
  if (*desired_yaw < *(float *)0x2533c0)
    *desired_yaw += *(float *)0x255a54;
}

void player_control_new_unit(uint16_t local_player_index, int player_index)
{
  player_control_t *pc;
  float *facing;
  int unit;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  pc = (player_control_t *)((char *)player_control_globals +
                            local_player_index * 0x40 + 0x10);
  csmemset(pc, 0, 0x40);
  pc->unit_index = player_index;
  pc->desired_weapon_index = -1;
  pc->desired_grenade_index = -1;
  pc->desired_zoom_level = -1;
  pc->field_0x26 = 0;
  pc->target_object_index = -1;
  pc->pitch_maximum = 1.4922565f; /* +85.5 degrees in radians */
  pc->pitch_minimum = -1.4922565f; /* -85.5 degrees in radians */
  pc->action_flags = 0;
  pc->persistent_action_flags = 0;
  if (player_index != -1) {
    unit = (int)object_get_and_verify_type(player_index, 3);
    facing = &pc->desired_angles_yaw;
    vector_to_angles(facing, (float *)(unit + 0x1d4));
    if (*facing < *(float *)0x2533c0)
      *facing += *(float *)0x255a54;
    pc->desired_weapon_index = *(int16_t *)(unit + 0x2a4);
    pc->desired_grenade_index = (int16_t) * (char *)(unit + 0x2cd);
    pc->desired_zoom_level = (int16_t) * (char *)(unit + 0x2d1);
  }
}

/* Set the desired weapon index on a unit's controlling player.
 * Resolves the unit's player handle (unit+0x1c8), looks up the local player
 * index (player+0x2), retrieves the player control slot, and writes
 * seat_index into the desired weapon field (slot+0x20). */
void player_control_set_unit_seat(int unit_handle, int seat_index)
{
  char *unit_obj;
  int player_handle;
  char *player;
  int16_t local_player_index;
  player_control_t *pc;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
  player_handle = *(int *)(unit_obj + 0x1c8);
  if (player_handle != NONE) {
    player = (char *)datum_get(player_data, player_handle);
    local_player_index = *(int16_t *)(player + 0x2);
    if (local_player_index != NONE) {
      pc = (player_control_t *)player_control_get_data(local_player_index);
      pc->desired_weapon_index = (int16_t)seat_index;
    }
  }
}

/* Apply this frame's look input to a local player's desired aiming angles.
 * yaw_delta/pitch_delta are the raw turn/look deltas (action+0x0c,
 * action+0x10).
 *
 * Yaw advances first, then is constrained to the arc the occupied seat allows:
 * the arc is centred on a marker direction on the unit (the seat definition at
 * +0x24 names the marker) and spans [seat+0xf0, seat+0xf4] about it. A yaw
 * landing outside the arc snaps to whichever end is nearer; the result is then
 * wrapped into [0, 2*pi].
 *
 * The pitch limits are not constants: pc->pitch_minimum/pitch_maximum ease
 * toward targets read from the camera-info limit block at a bounded +-pi/256
 * per call, and those targets are themselves clamped to +-85.5 degrees. Pitch
 * advances last and is clamped to the just-updated limits.
 *
 * c:\halo\SOURCE\game\player_control.c */
void player_control_update_desired_angles(int16_t local_player_index,
                                          float yaw_delta, float pitch_delta)
{
  player_control_t *pc;
  float *desired_pitch;
  void *globals_tag;
  /* +0x00 unit handle, +0x04 seat index, +0x08 seat-limit block ptr -- the
   * three fields player_control_get_unit_camera_info (0xb6740) stores
   * directly. It ALSO does `lea eax,[esi+0xc]` and forwards that to
   * unit_set_seat_state (0x1a9240), which writes a float[3] seat position at
   * +0x0c..+0x18, so the buffer is larger than the three visible stores. The
   * original reserves it at [ebp-0x38] with the next local (desired_pitch) at
   * [ebp-0x10] -- 0x28 bytes of headroom; matched here. Sizing this 0xc let
   * the callee's write land on marker_angles and hang a10 after the opening
   * cinematic. */
  char camera_info[0x28];
  char marker_buf[0x6c]; /* object_get_markers_by_string_id output */
  float marker_angles[2]; /* yaw, pitch */
  float forward[3];
  float pitch_minimum_target;
  float pitch_maximum_target;
  float pitch_target;
  float delta;
  char *unit_obj;
  char *limits;

  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                 local_player_index >= 0 &&
                   local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  pc = (player_control_t *)((char *)player_control_globals +
                            (int)local_player_index * 0x40 + 0x10);
  globals_tag =
    tag_block_get_element((char *)game_globals_get() + 0x110, 0, 0x80);

  desired_pitch = &pc->desired_angles_pitch;
  pitch_minimum_target = -1.4922565f; /* -85.5 degrees */
  pitch_maximum_target = 1.4922565f; /* +85.5 degrees */

  /* valid_euler_angles2d(&player->desired_angles): pitch within +-85.5
   * degrees, yaw within [0, 2*pi], neither infinite nor NaN. */
  if ((*(uint32_t *)&pc->desired_angles_pitch & 0x7f800000) == 0x7f800000 ||
      pc->desired_angles_pitch > 1.4922565f ||
      pc->desired_angles_pitch < -1.4922565f ||
      (*(uint32_t *)&pc->desired_angles_yaw & 0x7f800000) == 0x7f800000 ||
      pc->desired_angles_yaw > 6.2831855f || pc->desired_angles_yaw < 0.0f) {
    display_assert("valid_euler_angles2d(&player->desired_angles)",
                   "c:\\halo\\SOURCE\\game\\player_control.c", 0x494, 1);
    system_exit(NONE);
  }

  player_control_get_unit_camera_info(local_player_index, camera_info);
  pc->desired_angles_yaw = yaw_delta + pc->desired_angles_yaw;

  /* constrain yaw to the arc this seat permits */
  if (*(int16_t *)(camera_info + 4) != NONE) {
    char *seat;

    /* one nested expression: the original cleans all three calls with a
     * single ADD ESP,0x1c */
    seat = (char *)tag_block_get_element(
      (char *)tag_get(
        0x756e6974 /* 'unit' */,
        *(int *)object_get_and_verify_type(*(int *)camera_info, 3)) +
        0x2e4,
      *(int16_t *)(camera_info + 4), 0x11c);

    if (*(float *)(seat + 0xf0) != 0.0f || *(float *)(seat + 0xf4) != 0.0f) {
      float yaw_low;
      float yaw_high;
      float arc;
      float delta_high;

      object_get_markers_by_string_id(*(int *)camera_info, seat + 0x24,
                                      marker_buf, 1);
      /* the marker's forward vector sits at +0x3c in the marker record */
      vector_to_angles(marker_angles, (float *)(marker_buf + 0x3c));

      yaw_low = marker_angles[0] + *(float *)(seat + 0xf0);
      yaw_high = marker_angles[0] + *(float *)(seat + 0xf4);

      arc = yaw_high - yaw_low;
      if (arc >= 3.1415927f)
        arc -= 6.2831855f;
      if (arc <= -3.1415927f)
        arc += 6.2831855f;

      delta_high = yaw_high - pc->desired_angles_yaw;
      if (delta_high >= 3.1415927f)
        delta_high -= 6.2831855f;
      if (delta_high <= -3.1415927f)
        delta_high += 6.2831855f;

      delta = pc->desired_angles_yaw - yaw_low;
      if (delta >= 3.1415927f)
        delta -= 6.2831855f;
      if (delta <= -3.1415927f)
        delta += 6.2831855f;

      if (arc < 0.0f)
        arc += 6.2831855f;

      /* outside the arc: snap to the nearer end */
      if (!((delta_high >= 0.0f && delta_high < arc) ||
            (delta >= 0.0f && delta < arc))) {
        if (fabs(delta_high) > fabs(delta))
          pc->desired_angles_yaw = yaw_low;
        else
          pc->desired_angles_yaw = yaw_high;
      }
    }
  }

  while (pc->desired_angles_yaw < 0.0f)
    pc->desired_angles_yaw += 6.2831855f;
  while (pc->desired_angles_yaw > 6.2831855f)
    pc->desired_angles_yaw -= 6.2831855f;

  /* ease the pitch limits toward the camera's targets */
  limits = *(char **)(camera_info + 8);
  if (limits != NULL) {
    unit_obj = (char *)object_get_and_verify_type(*(int *)camera_info, 3);
    pitch_target = *(float *)(limits + 0x40);

    if (*(float *)(limits + 0x48) != 0.0f ||
        *(float *)(limits + 0x44) != 0.0f) {
      pitch_minimum_target = *(float *)(limits + 0x44);
      pitch_maximum_target = *(float *)(limits + 0x48);

      /* shift the targets by how far the player has turned off the unit's
       * own facing, so the limits track the body rather than the world */
      if (*(int16_t *)(camera_info + 4) != NONE &&
          *(float *)(unit_obj + 0x38) > 0.0f) {
        float offset;

        marker_angles[0] = pc->desired_angles_yaw;
        marker_angles[1] = 0.0f;
        angles_to_vector(forward, marker_angles);
        offset = 0.2f - FUN_0010c510(forward, (float *)(unit_obj + 0x30));
        pitch_minimum_target -= offset;
        pitch_maximum_target -= offset;
        pitch_target -= offset;
      }

      if (pitch_minimum_target < -1.4922565f)
        pitch_minimum_target = -1.4922565f;
      else if (pitch_minimum_target > 1.4922565f)
        pitch_minimum_target = 1.4922565f;

      if (pitch_maximum_target < -1.4922565f)
        pitch_maximum_target = -1.4922565f;
      else if (pitch_maximum_target > 1.4922565f)
        pitch_maximum_target = 1.4922565f;
    }

    if (pitch_target != 0.0f || pc->field_0x26 != 0) {
      float scaled;
      float magnitude;

      scaled =
        (float)(fabs(*desired_pitch - pitch_target) * 0.6366197466850281);
      if ((*(uint32_t *)desired_pitch & 0x7f800000) == 0x7f800000) {
        display_assert(
          csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                   "player->desired_angles.pitch", *(uint32_t *)desired_pitch,
                   (double)*desired_pitch),
          "c:\\halo\\SOURCE\\game\\player_control.c", 0x4f2, 1);
        system_exit(NONE);
      }

      magnitude =
        sqrtf(*(float *)(unit_obj + 0x18) * *(float *)(unit_obj + 0x18) +
              *(float *)(unit_obj + 0x1c) * *(float *)(unit_obj + 0x1c) +
              *(float *)(unit_obj + 0x20) * *(float *)(unit_obj + 0x20));

      if (pitch_target != 0.0f)
        interpolate_scalar(desired_pitch, pitch_target,
                           magnitude * scaled * 0.08f);
      else
        interpolate_scalar(desired_pitch, pitch_target,
                           magnitude * *(float *)((char *)globals_tag + 0x54) *
                             scaled);

      if ((*(uint32_t *)desired_pitch & 0x7f800000) == 0x7f800000) {
        display_assert(
          csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                   "player->desired_angles.pitch", *(uint32_t *)desired_pitch,
                   (double)*desired_pitch),
          "c:\\halo\\SOURCE\\game\\player_control.c", 0x4fd, 1);
        system_exit(NONE);
      }
    }
  }

  /* the limits themselves move no faster than pi/256 per call */
  delta = pitch_minimum_target - pc->pitch_minimum;
  if (delta < -0.0122718466f)
    delta = -0.0122718466f;
  else if (delta > 0.0122718466f)
    delta = 0.0122718466f;
  pc->pitch_minimum += delta;

  delta = pitch_maximum_target - pc->pitch_maximum;
  if (delta < -0.0122718466f)
    delta = -0.0122718466f;
  else if (delta > 0.0122718466f)
    delta = 0.0122718466f;
  pc->pitch_maximum += delta;

  pc->desired_angles_pitch = pitch_delta + pc->desired_angles_pitch;
  if (pc->desired_angles_pitch < pc->pitch_minimum)
    pc->desired_angles_pitch = pc->pitch_minimum;
  else if (pc->desired_angles_pitch > pc->pitch_maximum)
    pc->desired_angles_pitch = pc->pitch_maximum;
}

void player_control_initialize_for_new_map(void)
{
  int i;
  int iVar;
  int scenario;

  *(int *)player_control_globals = 0;
  *((int *)player_control_globals + 1) = 0;
  *((int *)player_control_globals + 2) = 0;
  *((int *)player_control_globals + 3) = 0;
  for (i = 0; (int16_t)i < 4; i++) {
    scenario = (int)game_globals_get();
    iVar = (int)tag_block_get_element((void *)(scenario + 0x110), 0, 0x80);
    player_control_new_unit(i, -1);
    if (*(float *)((char *)0x4570a8 + i * 4) == *(float *)0x2533c0)
      *(int *)((char *)0x4570a8 + i * 4) = *(int *)(iVar + 0x4c);
    if (*(float *)((char *)0x457098 + i * 4) == *(float *)0x2533c0)
      *(int *)((char *)0x457098 + i * 4) = *(int *)(iVar + 0x50);
  }
}

/* Process input for a local player: read controller/keyboard state, handle
 * weapon switching and grenade throwing, detect autoaim idle, validate
 * facing angles, and submit the resulting action to the game engine.
 * Called once per local player per frame from player_control_update. */
void player_control_get_facing(int16_t local_player_index, float delta_time)
{
  player_control_t *pc; /* player control struct (ESI) */
  void *game_tag_elem;
  player_input_t input; /* one frame of controller input */
  int player_index;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  pc = (player_control_t *)((char *)player_control_globals +
                            (int)local_player_index * 0x40 + 0x10);

  /* get game globals tag element (input sensitivity thresholds etc.) */
  {
    void *globals = game_globals_get();
    game_tag_elem = tag_block_get_element((char *)globals + 0x110, 0, 0x80);
  }

  /* fill action with sentinel 0xfa, then read actual input */
  csmemset(&input, 0xfa, 0x20);
  /* get_local_player_input_blob fills *action (0x20 bytes, passed in EBX). */
  get_local_player_input_blob(&input, local_player_index, delta_time);

  player_index = local_player_get_player_index(local_player_index);

  /* validate desired facing angles if player exists */
  if (player_index != NONE) {
    uint32_t bits;
    bits = *(uint32_t *)&pc->desired_angles_pitch;
    if ((bits & 0x7f800000) == 0x7f800000) {
      display_assert("player->desired_angles.pitch",
                     "c:\\halo\\SOURCE\\game\\player_control.c", 0x2ce, 1);
      system_exit(NONE);
    }
    bits = *(uint32_t *)&pc->desired_angles_yaw;
    if ((bits & 0x7f800000) == 0x7f800000) {
      display_assert("player->desired_angles.yaw",
                     "c:\\halo\\SOURCE\\game\\player_control.c", 0x2cf, 1);
      system_exit(NONE);
    }
  }

  /* if director is controlling the camera, clear input */
  if (director_inhibited_input(local_player_index))
    csmemset(&input, 0, 0x20);

  /* handle weapon/vehicle switching when playing locally */
  if (game_connection() == 0) {
    uint8_t flags = *(uint8_t *)&input.action_flags;

    /* weapon switch (action bits 3-4) */
    if (flags & 0x18) {
      int new_weapon;
      if (flags & 0x10)
        new_weapon = units_debug_get_next_unit(pc->unit_index);
      else
        new_weapon = FUN_001AA170(pc->unit_index);
      if (new_weapon != NONE)
        players_set_local_player_unit(local_player_index, new_weapon);
    }

    /* grenade throw (action bit 5) */
    if (flags & 0x20) {
      if (pc->unit_index == NONE)
        goto final_copy;
      unit_debug_ninja_rope(pc->unit_index);
    }
  }

  /* unit-specific handling */
  if (pc->unit_index != NONE) {
    char *unit_obj;
    int weapon_datum;

    unit_obj = (char *)object_get_and_verify_type(pc->unit_index, 3);

    /* look up unit definition tag and current weapon */
    tag_get(0x756e6974, *(int *)unit_obj);
    weapon_datum =
      unit_get_weapon(pc->unit_index, *(uint16_t *)(unit_obj + 0x2a2));

    /* validate player weapon index */
    if (pc->desired_weapon_index == NONE ||
        unit_get_weapon(pc->unit_index, pc->desired_weapon_index) == NONE) {
      pc->desired_weapon_index = *(int16_t *)(unit_obj + 0x2a4);
    }

    /* weapon interaction (action bit 0) */
    if ((*(uint8_t *)&input.action_flags & 1) ||
        unit_get_weapon(pc->unit_index, pc->desired_weapon_index) == NONE ||
        pc->desired_weapon_index == NONE) {
      int16_t new_wp =
        unit_inventory_next_weapon(pc->unit_index, pc->desired_weapon_index,
                                   *(uint8_t *)&input.action_flags & 1);
      pc->desired_weapon_index = new_wp;
      pc->desired_zoom_level = NONE;
    }

    /* check for forced weapon from AI/script */
    {
      int16_t forced = unit_find_weapon_to_ready(pc->unit_index);
      if (forced != NONE && pc->desired_weapon_index != forced) {
        pc->desired_weapon_index = forced;
        pc->desired_zoom_level = NONE;
      }
    }

    /* validate grenade type */
    if (pc->desired_grenade_index == NONE ||
        unit_get_grenade_count(pc->unit_index, pc->desired_grenade_index) ==
          0) {
      pc->desired_grenade_index = (int16_t) * (int8_t *)(unit_obj + 0x2cd);
    }

    /* grenade switch (action bit 1) */
    if ((*(uint8_t *)&input.action_flags & 2) ||
        unit_get_grenade_count(pc->unit_index, pc->desired_grenade_index) ==
          0 ||
        pc->desired_grenade_index == NONE) {
      pc->desired_grenade_index = unit_inventory_next_grenade(
        pc->unit_index, pc->desired_grenade_index, 1);
    }

    /* melee/throw request (action bit 2) */
    if ((*(uint8_t *)&input.action_flags & 4) &&
        (*(uint8_t *)((char *)player_control_globals + 0xc) & 1) == 0 &&
        !game_time_get_paused() && weapon_datum != NONE &&
        !cinematic_in_progress()) {
      pc->desired_zoom_level =
        weapon_rotate_zoom_level(weapon_datum, pc->desired_zoom_level);
    }

    /* apply turning/look input (unless scripted camera); the yaw and pitch
     * deltas for this frame live at action+0x0c / action+0x10. */
    if (!director_inhibited_facing(local_player_index)) {
      player_control_update_desired_angles(
        local_player_index, input.look_yaw_delta, input.look_pitch_delta);
    }

    /* autoaim idle detection: if the player is looking at an enemy
     * (crosshair showing), actively turning (yaw above threshold),
     * and NOT firing, increment the idle counter. When the counter
     * exceeds a tag-defined threshold, enable autoaim assist. */
    if (*(int *)(unit_obj + 0xcc) == NONE) {
      float abs_facing;
      if (!player_ui_autolevel_enabled(local_player_index))
        goto reset_autoaim;
      /* FABS + FCOMP double: check if facing yaw exceeds threshold */
      abs_facing = pc->field_0x14;
      if (abs_facing < 0.0f)
        abs_facing = -abs_facing;
      if (!(abs_facing > *(double *)0x25fea8))
        goto reset_autoaim;
      /* check trigger and throttle below firing threshold */
      if (input.look_pitch_delta >= *(float *)0x253f44)
        goto reset_autoaim;
      if (pc->field_0x30 >= *(float *)0x253f44)
        goto reset_autoaim;
      /* all conditions met — increment idle counter */
      {
        int count = (int)pc->field_0x27 + 1;
        if (count < 0)
          count = 0;
        else if (count > 0x7f)
          count = 0x7f;
        pc->field_0x27 = (int8_t)count;
        pc->field_0x26 =
          (int16_t)(int8_t)count > *(int16_t *)((char *)game_tag_elem + 0x6e);
      }
      goto final_copy;
    reset_autoaim:
      *(uint8_t *)&pc->field_0x27 = 0;
    }
    pc->field_0x26 = 0;
  }

final_copy:
  /* copy action results to player control struct */
  pc->field_0x04 = input.field_0x18;
  *(int *)&pc->field_0x14 = *(int *)&input.field_0x00;
  pc->field_0x18 = *(int *)&input.field_0x04;
  *(int *)&pc->primary_trigger = *(int *)&input.primary_trigger;

  /* validate primary_trigger */
  {
    uint32_t bits = *(uint32_t *)&pc->primary_trigger;
    if ((bits & 0x7f800000) == 0x7f800000) {
      display_assert("player->primary_trigger",
                     "c:\\halo\\SOURCE\\game\\player_control.c", 0x351, 1);
      system_exit(NONE);
    }
  }

  /* submit action to the game engine */
  player_index = local_player_get_player_index(local_player_index);
  if (player_index != NONE) {
    uint32_t bits;
    /* validate final desired angles and trigger */
    bits = *(uint32_t *)&pc->desired_angles_pitch;
    if ((bits & 0x7f800000) == 0x7f800000) {
      display_assert("player->desired_angles.pitch",
                     "c:\\halo\\SOURCE\\game\\player_control.c", 0x35d, 1);
      system_exit(NONE);
    }
    bits = *(uint32_t *)&pc->desired_angles_yaw;
    if ((bits & 0x7f800000) == 0x7f800000) {
      display_assert("player->desired_angles.yaw",
                     "c:\\halo\\SOURCE\\game\\player_control.c", 0x35e, 1);
      system_exit(NONE);
    }
    bits = *(uint32_t *)&pc->primary_trigger;
    if ((bits & 0x7f800000) == 0x7f800000) {
      display_assert("player->primary_trigger",
                     "c:\\halo\\SOURCE\\game\\player_control.c", 0x35f, 1);
      system_exit(NONE);
    }

    /* build and submit player action struct */
    {
      char action_buf[0x20];
      player_action_t *action = (player_action_t *)action_buf;

      action->buttons = pc->field_0x04;
      *(int *)&action->desired_facing_yaw = *(int *)&pc->desired_angles_yaw;
      *(int *)&action->desired_facing_pitch = *(int *)&pc->desired_angles_pitch;
      action->desired_weapon_index = pc->desired_weapon_index;
      action->desired_grenade_index = pc->desired_grenade_index;
      *(int *)&action->throttle_x = *(int *)&pc->field_0x14;
      action->desired_zoom_level = pc->desired_zoom_level;
      *(int *)&action->primary_trigger = *(int *)&pc->primary_trigger;
      *(int *)&action->throttle_y = *(int *)&pc->field_0x18;

      /* validate action facing angles */
      bits = *(uint32_t *)&action->desired_facing_pitch;
      if ((bits & 0x7f800000) == 0x7f800000) {
        display_assert("action.desired_facing.pitch",
                       "c:\\halo\\SOURCE\\game\\player_control.c", 0x369, 1);
        system_exit(NONE);
      }
      bits = *(uint32_t *)&action->desired_facing_yaw;
      if ((bits & 0x7f800000) == 0x7f800000) {
        display_assert("action.desired_facing.yaw",
                       "c:\\halo\\SOURCE\\game\\player_control.c", 0x36a, 1);
        system_exit(NONE);
      }

      update_client_queue(action_buf);
    }
  }
}

void player_control_update(float delta_time)
{
  int16_t i;

  if (profile_global_enable && *(char *)0x2f02a0)
    profile_enter_private((void *)0x2f0298);
  collision_log_begin_period(2);
  update_client_queue_push();
  for (i = 0; i < 4; i++)
    player_control_get_facing(i, delta_time);
  collision_log_end_period();
  if (profile_global_enable && *(char *)0x2f02a0)
    profile_exit_private((void *)0x2f0298);
}

/* Forward a packed look-delta pair to a local player's desired-angle update.
 * delta points at two floats: delta[0] is the yaw (turn) delta and delta[1]
 * the pitch (look) delta -- established from the push order at the
 * player_control_update_desired_angles call site (first PUSH is the last
 * argument, so [delta+4] becomes pitch_delta and [delta+0] yaw_delta).
 * The deltas are only forwarded, never computed here.
 *
 * c:\halo\SOURCE\game\player_control.c */
void FUN_000b8cf0(int16_t local_player_index, float *delta)
{
  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0x467, delta);
  player_control_update_desired_angles(local_player_index, delta[0], delta[1]);
}
