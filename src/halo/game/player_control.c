/* ---------------------------------------------------------------------------
 * Angle constants used throughout this TU.
 *
 * Two forms appear below and the difference is deliberate, not cosmetic.
 * Where the original compares against a slot in its own .rdata constant pool
 * the lift reads that slot by address, so VC71 emits the same FCOMP/FLD
 * operand (see the note on FUN_000b6dd0); where the original materializes an
 * inline immediate instead -- e.g. MOV [EBP-0xC],0x3fbf0243 at 0xb800f -- the
 * lift uses a source literal.  Swapping one form for the other changes the
 * emitted code, so do not "unify" them.
 *
 * Every value below is the exact float32 the binary holds.  The derivations in
 * the comments are there so the magnitudes are checkable; never substitute the
 * computed expression for the literal, because the fold can differ in the last
 * bit and lands as an [IMM-WARN] against the delinked reference.
 * ------------------------------------------------------------------------- */

/* Pool-resident copies, read by address to preserve the original operand. */
#define REAL_ZERO_POOL        (*(float *)0x2533c0) /* 0.0f         */
#define REAL_PI_POOL          (*(float *)0x256980) /* 3.1415927f   */
#define REAL_NEGATIVE_PI_POOL (*(float *)0x26e280) /* -3.1415927f  */
#define REAL_TWO_PI_POOL      (*(float *)0x255a54) /* 6.2831855f   */

/* Inline immediates. */
#define REAL_PI      3.1415927f /* 0x40490fdb */
#define REAL_TWO_PI  6.2831855f /* 0x40c90fdb */
#define REAL_HALF_PI 1.5707964f /* 0x3fc90fdb */

/* Limits on player->desired_angles.pitch: +-85.5 degrees, equivalently
 * (PI/2) * 0.95.  float32 0x3fbf0243 / 0xbfbf0243.  The binary uses both
 * forms of this pair -- as FCOMP operands out of .rdata 0x26e37c / 0x26e378
 * (the inlined valid_euler_angles2d bounds) and as inline immediates (the
 * easing targets stored at 0xb8008 / 0xb800f). */
#define MAXIMUM_DESIRED_PITCH 1.49225652217865f
#define MINIMUM_DESIRED_PITCH (-1.49225652217865f)

/* Per-call ceiling on how fast pitch_minimum/pitch_maximum may move toward
 * their targets: PI/256, exactly float32 0x3c490fdb. */
#define MAXIMUM_PITCH_LIMIT_CHANGE 0.0122718466f

/* float32(2/PI) widened to double -- the original holds the narrowed value,
 * so this is 0.6366197466850281, not the double 2/PI (0.6366197723675814). */
#define REAL_TWO_OVER_PI 0.6366197466850281

/* Field-of-view fallback when the tag supplies none: 70 degrees. */
#define DEFAULT_FIELD_OF_VIEW 1.2217305f

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
  field_of_view = DEFAULT_FIELD_OF_VIEW;
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

/* Test the "primary trigger" player action.
 *
 * Disassembly (0xb6b20), 5 instructions, no calls, no frame:
 *   MOV EAX,[player_control_globals]   ; pointer loaded ONCE
 *   MOV EAX,dword ptr [EAX]            ; dword at +0x00
 *   SHR EAX,0x4
 *   AND EAX,0x1
 *   RET
 *
 * NOTE: Ghidra decompiles this as `void` with an empty body -- it is NOT.
 * EAX is live at RET and carries bit 4 of the dword at +0x00
 * (lift-learnings 16, void-EAX implicit return).  kb.json's
 * `void ...(void)` decl was corrected to a bool return, matching the siblings
 * 0xb6ab0/0xb6ad0/0xb6af0/0xb6b10.
 *
 * Same PURE-test shape as player_control_action_test_jump (0xb6b10): there are
 * NO `OR` stores into the accumulator dwords at +0x04/+0x08, unlike the 10-insn
 * members of the family, so the action is not marked as consumed here.  Adding
 * those ORs would be an invented side effect.  Bit index is 4 (SHR EAX,0x4),
 * i.e. FLAG(4) is the "primary trigger" action bit.
 *
 * player_control_globals_t is opaque (0x110 bytes) with no named field at
 * +0x00, so raw dword access is used. */
bool player_control_action_test_primary_trigger(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (bool)((fields[0] >> 4) & 1u);
}

/* Test the "grenade trigger" player action.
 *
 * Disassembly (0xb6b30), 5 instructions, no calls, no frame:
 *   MOV EAX,[player_control_globals]   ; pointer loaded ONCE
 *   MOV EAX,dword ptr [EAX]            ; dword at +0x00
 *   SHR EAX,0x5
 *   AND EAX,0x1
 *   RET
 *
 * NOTE: Ghidra decompiles this as `void` with an empty body -- it is NOT.
 * EAX is live at RET and carries bit 5 of the dword at +0x00
 * (lift-learnings 16, void-EAX implicit return).  kb.json's
 * `void ...(void)` decl was corrected to a bool return, matching the siblings
 * 0xb6ab0/0xb6ad0/0xb6af0/0xb6b10/0xb6b20.
 *
 * Same PURE-test shape as player_control_action_test_jump (0xb6b10) and
 * _primary_trigger (0xb6b20): there are NO `OR` stores into the accumulator
 * dwords at +0x04/+0x08, unlike the 10-insn members of the family, so the
 * action is not marked as consumed here.  Adding those ORs would be an
 * invented side effect.  Bit index is 5 (SHR EAX,0x5), i.e. FLAG(5) is the
 * "grenade trigger" action bit.
 *
 * The read is at globals+0x00 -- the GLOBAL action dword -- not the per-player
 * player_control_t.action_flags at slot+0x08; the two are distinct.
 *
 * player_control_globals_t is opaque (0x110 bytes) with no named field at
 * +0x00, so raw dword access is used. */
bool player_control_action_test_grenade_trigger(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (bool)((fields[0] >> 5) & 1u);
}

/* Test the "zoom" player action.
 *
 * Disassembly (0xb6b40), 5 instructions, no calls, no frame:
 *   MOV EAX,[player_control_globals]   ; pointer loaded ONCE
 *   MOV EAX,dword ptr [EAX]            ; dword at +0x00
 *   SHR EAX,0x6
 *   AND EAX,0x1
 *   RET
 *
 * NOTE: Ghidra decompiles this as `void` with an empty body -- it is NOT.
 * EAX is live at RET and carries bit 6 of the dword at +0x00
 * (lift-learnings 16, void-EAX implicit return).  kb.json's
 * `void ...(void)` decl was corrected to a bool return, matching the siblings
 * 0xb6ab0/0xb6ad0/0xb6af0/0xb6b10/0xb6b20/0xb6b30.
 *
 * Same PURE-test shape as player_control_action_test_jump (0xb6b10),
 * _primary_trigger (0xb6b20) and _grenade_trigger (0xb6b30): there are NO `OR`
 * stores into the accumulator dwords at +0x04/+0x08, unlike the 10-insn members
 * of the family, so the action is not marked as consumed here.  Adding those
 * ORs would be an invented side effect.  Bit index is 6 (SHR EAX,0x6), i.e.
 * FLAG(6) is the "zoom" action bit.
 *
 * The read is at globals+0x00 -- the GLOBAL action dword -- not the per-player
 * player_control_t.action_flags at slot+0x08; the two are distinct, and +0x00
 * is NOT the 0x10-based per-player slot array (stride 0x40) used elsewhere in
 * this TU, so it must not be indexed by local_player_index.
 *
 * player_control_globals_t is opaque (0x110 bytes) with no named field at
 * +0x00, so raw dword access is used. */
bool player_control_action_test_zoom(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (bool)((fields[0] >> 6) & 1u);
}

/* Test whether ALL FOUR move-relative direction bits (11..14) of the global
 * action dword are set simultaneously.  Original (0xb6b50, 8 instructions):
 *
 *   MOV EAX,[0x00457090]      ; player_control_globals
 *   MOV EAX,dword ptr [EAX]   ; globals+0x00, global action dword
 *   NOT EAX
 *   AND EAX,0x7800
 *   NEG EAX / SBB EAX,EAX / INC EAX   ; == (eax == 0)
 *   RET
 *
 * NOTE: Ghidra decompiles this as `void` with an empty body -- it is NOT.
 * EAX is live at RET (lift-learnings 16, void-EAX implicit return); kb.json's
 * `void ...(void)` decl was corrected to a returning one, like the siblings
 * 0xb6ab0/0xb6ad0/0xb6af0/0xb6b10/0xb6b20/0xb6b30/0xb6b40.
 *
 * The return type is `int`, not this project's `bool` (which is a 1-byte
 * `unsigned char`): the original's SBB/INC operate on the full 32-bit EAX
 * (`sbb eax,eax; inc eax`), whereas a 1-byte return makes VC71 emit
 * `sbb al,al; inc al` -- measured, 75.0% vs 100.0% match.  The value is still
 * 0/1, so it is usable in any boolean context.
 *
 * The mask is an ALL-SET test, not an ANY-SET test: NOT+AND+(==0) means every
 * one of bits 11,12,13,14 must be set.  `(v & 0x7800) != 0` would be the
 * inverted-polarity bug and is invisible to both VC71 and asserts.
 *
 * Same PURE-test shape as the other 8-instruction members of the family: no
 * `OR` stores into the accumulator dwords at +0x04/+0x08, so the action is not
 * marked consumed here.  Adding those ORs would be an invented side effect.
 *
 * The read is at globals+0x00 -- the GLOBAL action dword -- not the per-player
 * slot array at +0x10 (stride 0x40), so it must not be indexed by
 * local_player_index.  player_control_globals_t is opaque (0x110 bytes) with no
 * named field at +0x00, so raw dword access is used. */
int player_control_action_test_move_relative_all_directions(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (~fields[0] & 0x7800u) == 0;
}

/* Test whether ALL FOUR look-relative direction bits (7..10) of the global
 * action dword are set simultaneously.  Original (0xb6b70, 8 instructions):
 *
 *   MOV EAX,[0x00457090]      ; player_control_globals
 *   MOV EAX,dword ptr [EAX]   ; globals+0x00, global action dword
 *   NOT EAX
 *   AND EAX,0x780
 *   NEG EAX / SBB EAX,EAX / INC EAX   ; == (eax == 0)
 *   RET
 *
 * Structurally identical to its immediate neighbour 0xb6b50
 * (_move_relative_all_directions); only the mask differs (0x780 vs 0x7800).
 *
 * NOTE: Ghidra decompiles this as `void` with an empty body -- it is NOT.
 * EAX is live at RET (lift-learnings 16, void-EAX implicit return); kb.json's
 * `void ...(void)` decl was corrected to a returning one, like the siblings
 * 0xb6ab0/0xb6ad0/0xb6af0/0xb6b10/0xb6b20/0xb6b30/0xb6b40/0xb6b50.
 *
 * The return type is `int`, not this project's `bool` (which is a 1-byte
 * `unsigned char`): the original's SBB/INC operate on the full 32-bit EAX
 * (`sbb eax,eax; inc eax`), whereas a 1-byte return makes VC71 emit
 * `sbb al,al; inc al` -- measured on 0xb6b50, 75.0% vs 100.0% match.  The
 * value is still 0/1, so it is usable in any boolean context.
 *
 * The mask is an ALL-SET test, not an ANY-SET test: NOT+AND+(==0) means every
 * one of bits 7,8,9,10 must be set.  `(v & 0x780) != 0` would be the
 * inverted-polarity bug and is invisible to both VC71 and asserts.  Writing
 * the comparison as `== 0` (rather than `!x` or a ternary) is what makes VC71
 * emit the NEG/SBB/INC idiom.
 *
 * Same PURE-test shape as the other 8-instruction members of the family: no
 * `OR` stores into the accumulator dwords at +0x04/+0x08, so the action is not
 * marked consumed here.  Adding those ORs would be an invented side effect.
 *
 * The read is at globals+0x00 -- the GLOBAL action dword -- not the per-player
 * slot array at +0x10 (stride 0x40), so it must not be indexed by
 * local_player_index.  player_control_globals_t is opaque (0x110 bytes) with no
 * named field at +0x00, so raw dword access is used. */
int player_control_action_test_look_relative_all_directions(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (~fields[0] & 0x780u) == 0;
}

/* Test whether the "look relative left" action bit is set (lift-learnings 16,
 * void-EAX implicit return).  kb.json's `void ...(void)` decl was corrected to
 * a bool return, matching the siblings 0xb6ab0/0xb6ad0/0xb6af0/0xb6b10/0xb6b20/
 * 0xb6b30/0xb6b40.
 *
 * Same 5-instruction PURE-test shape as player_control_action_test_zoom
 * (0xb6b40): there are NO `OR` stores into the accumulator dwords at
 * +0x04/+0x08, unlike the 10-insn members of the family, so the action is not
 * marked as consumed here.  Adding those ORs would be an invented side effect.
 * Bit index is 9 (SHR EAX,0x9) -- one of the four bits (7..10) covered by the
 * 0x780 mask in player_control_action_test_look_relative_all_directions.
 *
 * The read is at globals+0x00 -- the GLOBAL action dword -- not the per-player
 * slot array at +0x10 (stride 0x40), so it must not be indexed by
 * local_player_index.  player_control_globals_t is opaque (0x110 bytes) with no
 * named field at +0x00, so raw dword access is used. */
bool player_control_action_test_look_relative_left(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (bool)((fields[0] >> 9) & 1u);
}

/* Test whether the "look relative right" action bit is set (lift-learnings 16,
 * void-EAX implicit return).  kb.json's `void ...(void)` decl was corrected to
 * a bool return, matching the siblings 0xb6ab0/0xb6ad0/0xb6af0/0xb6b10/0xb6b20/
 * 0xb6b30/0xb6b40/0xb6b90.
 *
 * Disassembly (0xb6ba0), 5 instructions, no calls -- the PURE-test shape:
 *   MOV EAX,[player_control_globals] / MOV EAX,[EAX] / SHR EAX,0xa /
 *   AND EAX,0x1 / RET
 * There are NO `OR` stores into the accumulator dwords at +0x04/+0x08, unlike
 * the 10-insn members of the family, so the action is not marked as consumed
 * here.  Adding those ORs would be an invented side effect.
 * Bit index is 10 (SHR EAX,0xa) -- one of the four bits (7..10) covered by the
 * 0x780 mask in player_control_action_test_look_relative_all_directions.
 *
 * The read is at globals+0x00 -- the GLOBAL action dword -- not the per-player
 * slot array at +0x10 (stride 0x40), so it must not be indexed by
 * local_player_index.  player_control_globals_t is opaque (0x110 bytes) with no
 * named field at +0x00, so raw dword access is used. */
bool player_control_action_test_look_relative_right(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (bool)((fields[0] >> 10) & 1u);
}

/* Test whether the "look relative up" action bit is set (lift-learnings 16,
 * void-EAX implicit return).  kb.json's `void ...(void)` decl was corrected to
 * a bool return, matching the siblings 0xb6ab0/0xb6ad0/0xb6af0/0xb6b10/0xb6b20/
 * 0xb6b30/0xb6b40/0xb6b90/0xb6ba0.
 *
 * Disassembly (0xb6bb0), 5 instructions, no calls -- the PURE-test shape:
 *   MOV EAX,[player_control_globals] / MOV EAX,[EAX] / SHR EAX,0x7 /
 *   AND EAX,0x1 / RET
 * There are NO `OR` stores into the accumulator dwords at +0x04/+0x08, unlike
 * the 10-insn members of the family, so the action is not marked as consumed
 * here.  Adding those ORs would be an invented side effect.
 * Bit index is 7 (SHR EAX,0x7) -- one of the four bits (7..10) covered by the
 * 0x780 mask in player_control_action_test_look_relative_all_directions.
 *
 * The read is at globals+0x00 -- the GLOBAL action dword -- not the per-player
 * slot array at +0x10 (stride 0x40), so it must not be indexed by
 * local_player_index.  player_control_globals_t is opaque (0x110 bytes) with no
 * named field at +0x00, so raw dword access is used. */
bool player_control_action_test_look_relative_up(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (bool)((fields[0] >> 7) & 1u);
}

/* Test the "look relative down" action bit of the global action dword.
 * Original (0xb6bc0) is 5 instructions:
 *   MOV EAX,[0x00457090] / MOV EAX,dword ptr [EAX] / SHR EAX,0x8 /
 *   AND EAX,0x1 / RET
 * There are NO `OR` stores into the accumulator dwords at +0x04/+0x08, unlike
 * the 10-insn members of the family, so the action is not marked as consumed
 * here.  Adding those ORs would be an invented side effect.
 * Bit index is 8 (SHR EAX,0x8) -- one of the four bits (7..10) covered by the
 * 0x780 mask in player_control_action_test_look_relative_all_directions.  Do
 * not confuse the single-bit 0x100 test with that 0x780 all-directions mask.
 *
 * The read is at globals+0x00 -- the GLOBAL action dword -- not the per-player
 * slot array at +0x10 (stride 0x40), so it must not be indexed by
 * local_player_index.  player_control_globals_t is opaque (0x110 bytes) with no
 * named field at +0x00, so raw dword access is used.
 *
 * EAX is live at RET; the kb.json `void` decl (and hence Ghidra's empty-body
 * decompile) was wrong -- corrected to bool (lift-learnings 16, void-EAX). */
bool player_control_action_test_look_relative_down(void)
{
  uint32_t *fields;

  fields = (uint32_t *)player_control_globals;
  return (bool)((fields[0] >> 8) & 1u);
}

/* Fold one local player's input blob into player_control_globals' action
 * bitfield, then reconcile the sticky/latched action bits (0xb6bd0).
 *
 * `input_state` arrives in ESI (@<esi>) -- MOV AL,[ESI+0x14] @0xb6bd0 is the
 * function's FIRST instruction, so ESI is read before any write. There is no
 * EBP frame and no stack parameter. Sole caller: get_local_player_input_blob
 * @0xb70b0. Ghidra typed the whole thing void(void) with `float *unaff_ESI`,
 * so every flag word came out as a bogus `(float)` store -- the disassembly
 * shows plain integer AND/OR on [ESI+0x18]/[ESI+0x1c], no FPU involved.
 *
 * input_state layout (offsets from the disassembly, not the decompiler):
 *   +0x00 float  axis A          +0x14 char   accept/press flag
 *   +0x04 float  axis B          +0x15 char   secondary press flag
 *   +0x08 float  trigger        +0x18 uint32 button flags (0x02, 0x40, 0x2000)
 *   +0x0c float  axis C          +0x1c uint32 latch flags (0x01, 0x02, 0x04)
 *   +0x10 float  axis D
 *
 * player_control_globals is opaque (0x110 bytes, no named fields), so raw
 * dword access is used as elsewhere in this TU:
 *   fields[0] = action bitfield being built
 *   fields[1] = "already tested" mask   fields[2] = sticky action state
 *
 * The four axis blocks are tri-state: FCOMP against 0.0f twice, once for
 * `> 0` and once for `< 0`, with zero setting neither bit. Branch polarity
 * per the derivation on FUN_000b6dd0 above -- TEST AH,0x41 keeps C0|C3 so
 * JNZ is taken unless st0 > mem, and TEST AH,0x5 + JP is taken unless
 * st0 < mem. 0.0f is read from its pool address 0x2533c0 rather than written
 * as a literal, for the reason given above.
 *
 * The tail reconciles three sticky bits, gated on the byte at 0x2f0292.
 * Both gate arms follow the same shape -- if the action was already tested
 * (fields[1] bit) just clear the source flag; otherwise, when the sticky bit
 * (fields[2]) is live, refresh it from the source flag and then clear that
 * flag -- but they read DIFFERENT source bits and use different masks:
 *   0x2f0292 == 0:  esi+0x18 bit 0x40, esi+0x1c bit 0x01, fields[2] bit 0x8
 *   0x2f0292 != 0:  esi+0x18 bit 0x02, esi+0x1c bit 0x02, fields[2] bit 0x4
 * Note the asymmetry in the third block: the != 0 arm tests and sets
 * fields[2] bit 0x4 (TEST AL,0x4 @0xb6da5, OR EAX,0x4 @0xb6daf) where the
 * == 0 arm uses bit 0x8 (TEST AL,0x8 @0xb6d58, OR EAX,0x8 @0xb6d62). That is
 * what the binary does and is preserved verbatim; it may well be an original
 * bug, but this is not the place to fix it. */
void FUN_000b6bd0(char *input_state)
{
  uint32_t *fields;

  if (input_state[0x14] != 0 && cinematic_can_be_skipped()) {
    main_skip_cinematic();
  }
  fields = (uint32_t *)player_control_globals;
  if ((*(uint32_t *)(input_state + 0x18) & 0x40) != 0) {
    fields[0] |= 1;
  }
  if ((*(uint32_t *)(input_state + 0x18) & 2) != 0) {
    fields[0] |= 2;
  }
  if (input_state[0x14] != 0) {
    fields[0] |= 4;
  }
  if (input_state[0x15] != 0) {
    fields[0] |= 8;
  }
  if (*(float *)(input_state + 8) > REAL_ZERO_POOL) {
    fields[0] |= 0x10;
  }
  if ((*(uint32_t *)(input_state + 0x18) & 0x2000) != 0) {
    fields[0] |= 0x20;
  }
  if ((*(uint32_t *)(input_state + 0x1c) & 4) != 0) {
    fields[0] |= 0x40;
  }
  if (*(float *)(input_state + 0x10) > REAL_ZERO_POOL) {
    fields[0] |= 0x80;
  } else if (*(float *)(input_state + 0x10) < REAL_ZERO_POOL) {
    fields[0] |= 0x100;
  }
  if (*(float *)(input_state + 0xc) > REAL_ZERO_POOL) {
    fields[0] |= 0x200;
  } else if (*(float *)(input_state + 0xc) < REAL_ZERO_POOL) {
    fields[0] |= 0x400;
  }
  if (*(float *)input_state > REAL_ZERO_POOL) {
    fields[0] |= 0x800;
  } else if (*(float *)input_state < REAL_ZERO_POOL) {
    fields[0] |= 0x1000;
  }
  if (*(float *)(input_state + 4) > REAL_ZERO_POOL) {
    fields[0] |= 0x2000;
  } else if (*(float *)(input_state + 4) < REAL_ZERO_POOL) {
    fields[0] |= 0x4000;
  }

  if ((fields[1] & 1) == 0) {
    if ((fields[2] & 1) != 0) {
      if ((*(uint32_t *)(input_state + 0x18) & 0x40) != 0) {
        fields[2] |= 1;
      } else {
        fields[2] &= 0xfffffffe;
      }
      *(uint32_t *)(input_state + 0x18) &= 0xffffffbf;
    }
  } else {
    *(uint32_t *)(input_state + 0x18) &= 0xffffffbf;
  }

  if (*(char *)0x2f0292 != 0) {
    if ((fields[1] & 4) == 0) {
      if ((fields[2] & 4) == 0) {
        goto latch_1c;
      }
      if ((*(uint32_t *)(input_state + 0x18) & 2) != 0) {
        fields[2] |= 4;
      } else {
        fields[2] &= 0xfffffffb;
      }
    }
    *(uint32_t *)(input_state + 0x18) &= 0xfffffffd;
  latch_1c:
    if ((fields[1] & 8) == 0) {
      if ((fields[2] & 4) == 0) {
        return;
      }
      if ((*(uint32_t *)(input_state + 0x1c) & 2) != 0) {
        fields[2] |= 4;
      } else {
        fields[2] &= 0xfffffffb;
      }
    }
    *(uint32_t *)(input_state + 0x1c) &= 0xfffffffd;
    return;
  }

  if ((fields[1] & 4) == 0) {
    if ((fields[2] & 4) == 0) {
      goto latch_1c_alt;
    }
    if ((*(uint32_t *)(input_state + 0x18) & 0x40) != 0) {
      fields[2] |= 4;
    } else {
      fields[2] &= 0xfffffffb;
    }
  }
  *(uint32_t *)(input_state + 0x18) &= 0xffffffbf;
latch_1c_alt:
  if ((fields[1] & 8) == 0) {
    if ((fields[2] & 8) == 0) {
      return;
    }
    if ((*(uint32_t *)(input_state + 0x1c) & 1) != 0) {
      fields[2] |= 8;
    } else {
      fields[2] &= 0xfffffff7;
    }
  }
  *(uint32_t *)(input_state + 0x1c) &= 0xfffffffe;
}

/* Signed angular difference `param_2 - param_1`, wrapped into (-pi, pi).
 *
 * Confirmed from disassembly (0xb6dd0, 12 instructions, plain EBP frame, no
 * locals, no _chkstk, no CALLs, cdecl with the result in ST(0)):
 *   FLD  [EBP+0xc]        ; param_2
 *   FSUB [EBP+0x8]        ; st0 = param_2 - param_1  (NOT param_1 - param_2)
 *   FCOM [0x00256980]     ; vs pi
 *   FNSTSW AX / TEST AH,0x1 / JNZ skip   ; C0 set => delta < pi => skip
 *   FSUB [0x00255a54]     ; delta -= 2*pi  when delta >= pi
 *   FCOM [0x0026e280]     ; vs -pi
 *   FNSTSW AX / TEST AH,0x41 / JP skip   ; (C0|C3)==0 => delta > -pi => skip
 *   FADD [0x00255a54]     ; delta += 2*pi  when delta <= -pi
 *
 * Branch-polarity derivation for the second test (the boundary differs from
 * the first, so it is spelled out rather than assumed).  TEST AH,0x41 keeps
 * C0 (less) and C3 (equal); JP is taken only on even parity of that result:
 *   delta <  -pi -> C0=1        -> 0x01 -> PF=0 -> fall through -> add
 *   delta == -pi -> C3=1        -> 0x40 -> PF=0 -> fall through -> add
 *   delta >  -pi -> C0=C3=0     -> 0x00 -> PF=1 -> JP taken     -> skip
 * So the low correction is `delta <= -pi` while the high one is
 * `delta >= pi`, giving a half-open result range of (-pi, pi).
 *
 * Each correction is applied at most once -- the original has no loop, so a
 * delta outside (-3*pi, 3*pi) is deliberately left unwrapped.  Both `if`s are
 * evaluated in sequence (the second is not an `else`), matching the fall-
 * through from the first block into the second FCOM.
 *
 * Constants are read as raw-address globals (pi at 0x256980, 2*pi at 0x255a54,
 * -pi at 0x26e280) to share the original's constant pool; substituting source
 * literals would change the emitted immediates. */
float FUN_000b6dd0(float param_1, float param_2)
{
  float delta;

  delta = param_2 - param_1;
  if (delta >= REAL_PI_POOL)
    delta -= REAL_TWO_PI_POOL;
  if (delta <= REAL_NEGATIVE_PI_POOL)
    delta += REAL_TWO_PI_POOL;
  return delta;
}

/* Clamp a 2D vector to a maximum length, in place.
 *
 * Returns true when the vector was longer than `max_length` and therefore
 * rescaled, false when it was left untouched.  The boolean is real: the
 * original sets AL on the clamped path (MOV AL,1 at 0xb6e3c) and clears it on
 * the pass-through (XOR AL,AL at 0xb6e51), so a `void` lift would leave EAX
 * garbage for callers.
 *
 * Squared lengths are compared to avoid a square root on the common
 * (unclamped) path.  The summand order below is the original's FPU load order:
 * 0xb6e16 loads v[1] before 0xb6e19 loads v[0].  The scale is
 * `max_length / sqrt(len_sq)` -- 0xb6e3e is FDIVR against [EBP+0xc], which
 * divides the memory operand by ST0, not the other way round.
 *
 * The comparison is written with `len_sq` on the left on purpose.  0xb6e2f is
 * FLD ST(1) / FCOMPP / TEST AH,0x41 / JNZ, i.e. len_sq is re-pushed above the
 * squared limit and the branch skips the clamp when C0|C3 (len_sq <= max^2).
 * Spelling it `max*max < len_sq` instead collapses to FCOMP ST(1) + JP and
 * loses an instruction. */
bool limit2d(float *v, float max_length)
{
  float len_sq;
  float scale;

  len_sq = v[1] * v[1] + v[0] * v[0];
  if (len_sq > max_length * max_length) {
    scale = max_length / sqrtf(len_sq);
    v[0] = scale * v[0];
    v[1] = scale * v[1];
    return true;
  }
  return false;
}

/* Move `*value` toward `target`, by at most `max_delta` per call.
 *
 * Equivalent to `*value += clamp(target - *value, -max_delta, +max_delta)`.
 *
 * The delta is assigned back over the `target` parameter on purpose: the
 * original has no locals at all (PUSH EBP / MOV EBP,ESP with no SUB ESP) and
 * spills the difference into the incoming parameter slot -- 0xb6e63 is
 * FLD [EBP+0xc]; FSUB [ECX]; FSTP [EBP+0xc].  Introducing a fresh local here
 * would grow the frame and shift every subsequent access.
 *
 * Subtraction direction is confirmed: FLD target then FSUB of the memory
 * operand (no FSUBR), i.e. `target - *value`.
 *
 * Three separate stores rather than a clamped temporary: the original's three
 * paths each FLD their own addend (-max_delta is left live in ST1 from the
 * first comparison at 0xb6e6e, max_delta is reloaded at 0xb6e91, the delta at
 * 0xb6e97) and tail-merge onto a single FADD [ECX]; FSTP [ECX] at 0xb6e9a.
 * The addend is on the left of the `+` because it is ST0 and `*value` is the
 * memory operand of the FADD, not the other way round.
 *
 * Written as a nested `if` with the -max_delta case in the trailing `else`,
 * not as an `if / else if / else` chain.  0xb6e6e is FLD max_delta; FCHS;
 * FLD delta; FCOMP -- -max_delta is pushed FIRST and stays live as ST1 across
 * the comparison, so the taken branch at 0xb6e9a already has its addend in
 * ST0 and the fall-through has to FSTP ST0 to discard it (0xb6e7f).  Testing
 * `target < -max_delta` instead makes VC71 schedule the negation as ST0 and
 * compare against memory (FCOMP [EBP+0xc]), losing that reuse.
 *
 * TEST AH,0x5 + JNP at 0xb6e79 is "not less", i.e. the fall-through condition
 * is `delta >= -max_delta` (an unordered/NaN compare sets C0|C2, giving even
 * parity and therefore falling through as well).  Written as `!(delta <
 * neg_max_delta)` -- NOT `delta >= neg_max_delta`, which compiles to
 * TEST AH,0x1 + JNE and takes the opposite branch on NaN.
 *
 * The delta spill through the parameter slot is done via a volatile view:
 * the original's FSTP/FLD round-trip truncates the x87 80-bit subtraction
 * result to a 32-bit float before it is compared and added.  Without the
 * volatile, clang keeps the delta in ST at extended precision; when
 * `target` and `*value` nearly cancel (e.g. *value=753172.19,
 * target~-0.375), the extra bits shift the result by thousands of ULPs
 * (unicorn seed 23: oracle -0.375 vs unforced lift -0.37970486). */
void interpolate_scalar(float *value, float target, float max_delta)
{
  volatile float *delta = &target;
  float neg_max_delta;

  *delta = target - *value;
  neg_max_delta = -max_delta;
  if (!(*delta < neg_max_delta)) {
    if (*delta > max_delta)
      *value = max_delta + *value;
    else
      *value = *delta + *value;
  } else {
    *value = neg_max_delta + *value;
  }
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
    char *msg = csprintf(error_string_buffer,
                         "%s: assert_valid_real(0x%08X %f)",
                         "player_control->desired_angles.pitch",
                         *(uint32_t *)&pc->desired_angles_pitch,
                         (double)pc->desired_angles_pitch);
    display_assert(msg, "c:\\halo\\SOURCE\\game\\player_control.c", 0xbb, 1);
    system_exit(NONE);
  }

  /* assert_valid_real on desired_angles.yaw (slot+0xc) */
  if ((*(uint32_t *)desired_yaw & 0x7f800000u) == 0x7f800000u) {
    char *msg = csprintf(error_string_buffer,
                         "%s: assert_valid_real(0x%08X %f)",
                         "player_control->desired_angles.yaw",
                         *(uint32_t *)desired_yaw, (double)*desired_yaw);
    display_assert(msg, "c:\\halo\\SOURCE\\game\\player_control.c", 0xbc, 1);
    system_exit(NONE);
  }

  /* Normalize yaw to [0, 2*pi) */
  if (*desired_yaw < REAL_ZERO_POOL)
    *desired_yaw += REAL_TWO_PI_POOL;
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
  pc->pitch_maximum = MAXIMUM_DESIRED_PITCH; /* +85.5 degrees in radians */
  pc->pitch_minimum = MINIMUM_DESIRED_PITCH; /* -85.5 degrees in radians */
  pc->action_flags = 0;
  pc->persistent_action_flags = 0;
  if (player_index != -1) {
    unit = (int)object_get_and_verify_type(player_index, 3);
    facing = &pc->desired_angles_yaw;
    vector_to_angles(facing, (float *)(unit + 0x1d4));
    if (*facing < REAL_ZERO_POOL)
      *facing += REAL_TWO_PI_POOL;
    pc->desired_weapon_index = *(int16_t *)(unit + 0x2a4);
    pc->desired_grenade_index = (int16_t) * (char *)(unit + 0x2cd);
    pc->desired_zoom_level = (int16_t) * (char *)(unit + 0x2d1);
  }
}

/* Return a pointer to a local player's desired facing angles (the 2-float
 * euler_angles2d {yaw, pitch} at player_control_t+0x0c).
 *
 * Binary: 0xb7e30 ends with LEA EAX,[ESI+0xc] / POP ESI / POP EBP / RET, so the
 * function returns &player->desired_angles -- kb.json previously declared it
 * void(void), which was wrong on both the parameter and the return.
 *
 * Two things the original inlines and this lift must inline too, or the codegen
 * diverges:
 *   1. player_control_get_data(): the slot arithmetic appears literally here
 *      (MOV ECX,[player_control_globals]; MOVSX EAX,SI; SHL EAX,6;
 *      LEA ESI,[EAX+ECX+0x10]) with no CALL to 0xb6380, and it carries that
 *      helper's own assert line (0xb1).  The helper is defined above under
 *      #pragma auto_inline(off) -- which is what other callers in this TU
 *      need -- so it is expanded by hand here instead.
 *   2. valid_euler_angles2d(): six inline tests, no CALL.  Pitch is validated
 *      BEFORE yaw.  Each test is a NaN/Inf reject on the raw bits followed by
 *      an upper and a lower bound, BOTH inclusive:
 *        pitch in [-1.49225652217865, 1.49225652217865]   (0x26e378 / 0x26e37c)
 *        yaw   in [0.0, 6.2831855]                        (0x2533c0 / 0x255a54)
 *      Both upper bounds are `<=`, not `<`: at 0xb7e8b/0xb8052 the sequence is
 *      FCOMP <bound> / FNSTSW AX / TEST AH,0x41 / JP <assert>, and JP is taken
 *      only when C0=C3=0 (ST0 > bound) or unordered -- equality (C3=1) falls
 *      through to the pass path.  A strict `<` here asserted the moment the
 *      player looked fully up in MP: player_control_update_desired_angles
 *      clamps desired_angles.pitch to pc->pitch_maximum, which eases to
 *      exactly 1.4922565f (0x3fbf0243, the same bit pattern this bound holds),
 *      so pitch lands ON the bound rather than below it.
 *      The bound values were read out of the XBE (.rdata bit patterns
 *      0xbfbf0243 / 0x3fbf0243 / 0x00000000 / 0x40c90fdb); no name evidence
 *      exists for the pitch bounds, so they stay as literals.
 *
 * c:\halo\SOURCE\game\player_control.c */
real *player_control_get_facing_angles(int16_t local_player_index)
{
  player_control_t *player;

  /* inlined player_control_get_data() -- keeps that helper's assert line */
  assert_halt_at("c:\\halo\\SOURCE\\game\\player_control.c", 0xb1,
                 local_player_index >= 0 &&
                   local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  player = (player_control_t *)((char *)player_control_globals +
                                local_player_index * 0x40 + 0x10);

  if (!((*(uint32_t *)&player->desired_angles_pitch & 0x7f800000) !=
          0x7f800000 &&
        player->desired_angles_pitch <= MAXIMUM_DESIRED_PITCH &&
        player->desired_angles_pitch >= MINIMUM_DESIRED_PITCH &&
        (*(uint32_t *)&player->desired_angles_yaw & 0x7f800000) != 0x7f800000 &&
        player->desired_angles_yaw <= REAL_TWO_PI &&
        player->desired_angles_yaw >= 0.0f)) {
    display_assert("valid_euler_angles2d(&player->desired_angles)",
                   "c:\\halo\\SOURCE\\game\\player_control.c", 0x3c0, 1);
    system_exit(NONE);
  }
  return &player->desired_angles_yaw;
}

/* Build the this-frame action record for a local player and hand back the
 * caller's buffer.
 *
 * The body is a three-call composition; the interesting part is the MSVC
 * push-sharing at 0xb7f19..0xb7f36, which the decompiler renders as two
 * unrelated calls:
 *
 *     PUSH ESI / CALL 0xb7e30 / ADD ESP,4     ; angles = get_facing_angles(idx)
 *     PUSH EAX                                ; -> arg3 of 0xbb560
 *     PUSH EDI                                ; -> arg2 of 0xbb560
 *     PUSH ESI / CALL 0xba3c0 / ADD ESP,4     ; local_player_get_player_index
 *     PUSH EAX                                ; -> arg1 of 0xbb560
 *     CALL 0xbb560 / ADD ESP,0xC
 *
 * The two cleanups are 4 then 0xC (not one 0x10): the angles and out pointers
 * are pushed into 0xbb560's frame before the inner call runs.  That is just
 * cdecl right-to-left evaluation, so the nested call expression below emits
 * the same shape.
 *
 * 0xb7f38 is MOV EAX,EDI -- the function returns its own second parameter, an
 * implicit-EAX return (lift-learnings 16) that the previous kb.json decl
 * (void(void)) dropped along with both parameters.
 *
 * local_player_index is forwarded to both callees straight out of its dword
 * stack slot (MOV ESI,[EBP+8]); there is no MOVSX, so no widening cast here.
 *
 * c:\halo\SOURCE\game\player_control.c */
real *player_control_get_facing_direction(int16_t local_player_index,
                                          real *facing_out)
{
  player_build_action_update(
    local_player_get_player_index(local_player_index), facing_out,
    player_control_get_facing_angles(local_player_index));
  return facing_out;
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
  pitch_minimum_target = MINIMUM_DESIRED_PITCH; /* -85.5 degrees */
  pitch_maximum_target = MAXIMUM_DESIRED_PITCH; /* +85.5 degrees */

  /* valid_euler_angles2d(&player->desired_angles): pitch within +-85.5
   * degrees, yaw within [0, 2*pi], neither infinite nor NaN. Negated
   * conjunction, matching the original's guard encodings (TEST AH,0x41/JP,
   * TEST AH,1/JNE, TEST AH,1/JE at 0xb8000..0xb802d) and the sibling
   * validator in player_control_get_facing_angles -- the positive De Morgan
   * form compiles to NaN-transparent guards (0x41/JE, 0x5/JNP) and was the
   * [FCOM-WARN] on this function. Behavior is identical either way here
   * because the exponent-bits prescreen rejects NaN/Inf first. */
  if (!((*(uint32_t *)&pc->desired_angles_pitch & 0x7f800000) != 0x7f800000 &&
        pc->desired_angles_pitch <= MAXIMUM_DESIRED_PITCH &&
        pc->desired_angles_pitch >= MINIMUM_DESIRED_PITCH &&
        (*(uint32_t *)&pc->desired_angles_yaw & 0x7f800000) != 0x7f800000 &&
        pc->desired_angles_yaw <= REAL_TWO_PI &&
        pc->desired_angles_yaw >= 0.0f)) {
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
      if (arc >= REAL_PI)
        arc -= REAL_TWO_PI;
      if (arc <= (-REAL_PI))
        arc += REAL_TWO_PI;

      delta_high = yaw_high - pc->desired_angles_yaw;
      if (delta_high >= REAL_PI)
        delta_high -= REAL_TWO_PI;
      if (delta_high <= (-REAL_PI))
        delta_high += REAL_TWO_PI;

      delta = pc->desired_angles_yaw - yaw_low;
      if (delta >= REAL_PI)
        delta -= REAL_TWO_PI;
      if (delta <= (-REAL_PI))
        delta += REAL_TWO_PI;

      if (arc < 0.0f)
        arc += REAL_TWO_PI;

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
    pc->desired_angles_yaw += REAL_TWO_PI;
  while (pc->desired_angles_yaw > REAL_TWO_PI)
    pc->desired_angles_yaw -= REAL_TWO_PI;

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
       * own facing, so the limits track the body rather than the world.
       * 0xb8324: FCOMP [0x2549d4] = 0.2f -- the gate is `> 0.2f`, NOT
       * `> 0.0f`.  0xb8358: FSUBR [0x2568bc] = 0x3fc90fdb = pi/2 -- the
       * offset is the ELEVATION of the unit vector off the yaw-plane
       * forward, `pi/2 - angle_between(forward, unit+0x30)`.  A misplaced
       * 0.2f here shifted every pitch target up by pi/2 - 0.2 = 1.3708 rad
       * (b30 hand-off aim-up: pitch settled at +1.2895 vs faithful
       * -0.0804, delta 1.3699). */
      if (*(int16_t *)(camera_info + 4) != NONE &&
          *(float *)(unit_obj + 0x38) > 0.2f) {
        float offset;

        marker_angles[0] = pc->desired_angles_yaw;
        marker_angles[1] = 0.0f;
        angles_to_vector(forward, marker_angles);
        offset =
          REAL_HALF_PI - FUN_0010c510(forward, (float *)(unit_obj + 0x30));
        pitch_minimum_target -= offset;
        pitch_maximum_target -= offset;
        pitch_target -= offset;
      }

      if (pitch_minimum_target < MINIMUM_DESIRED_PITCH)
        pitch_minimum_target = MINIMUM_DESIRED_PITCH;
      else if (pitch_minimum_target > MAXIMUM_DESIRED_PITCH)
        pitch_minimum_target = MAXIMUM_DESIRED_PITCH;

      if (pitch_maximum_target < MINIMUM_DESIRED_PITCH)
        pitch_maximum_target = MINIMUM_DESIRED_PITCH;
      else if (pitch_maximum_target > MAXIMUM_DESIRED_PITCH)
        pitch_maximum_target = MAXIMUM_DESIRED_PITCH;
    }

    if (pitch_target != 0.0f || pc->field_0x26 != 0) {
      float scaled;
      float magnitude;

      scaled =
        (float)(fabs(*desired_pitch - pitch_target) * REAL_TWO_OVER_PI);
      if ((*(uint32_t *)desired_pitch & 0x7f800000) == 0x7f800000) {
        display_assert(
          csprintf(error_string_buffer, "%s: assert_valid_real(0x%08X %f)",
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
          csprintf(error_string_buffer, "%s: assert_valid_real(0x%08X %f)",
                   "player->desired_angles.pitch", *(uint32_t *)desired_pitch,
                   (double)*desired_pitch),
          "c:\\halo\\SOURCE\\game\\player_control.c", 0x4fd, 1);
        system_exit(NONE);
      }
    }
  }

  /* the limits themselves move no faster than pi/256 per call */
  delta = pitch_minimum_target - pc->pitch_minimum;
  if (delta < (-MAXIMUM_PITCH_LIMIT_CHANGE))
    delta = (-MAXIMUM_PITCH_LIMIT_CHANGE);
  else if (delta > MAXIMUM_PITCH_LIMIT_CHANGE)
    delta = MAXIMUM_PITCH_LIMIT_CHANGE;
  pc->pitch_minimum += delta;

  delta = pitch_maximum_target - pc->pitch_maximum;
  if (delta < (-MAXIMUM_PITCH_LIMIT_CHANGE))
    delta = (-MAXIMUM_PITCH_LIMIT_CHANGE);
  else if (delta > MAXIMUM_PITCH_LIMIT_CHANGE)
    delta = MAXIMUM_PITCH_LIMIT_CHANGE;
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
    /* Both stores are integer moves of the float bits (MOV EAX,[EBX+0x4C] /
     * MOV [ESI+0x4570A8],EAX at 0xb8617), not float copies -- keep the
     * `*(int *)&` form or VC71 emits FLD/FSTP instead. */
    if (flt_4570A8[i] == REAL_ZERO_POOL)
      *(int *)&flt_4570A8[i] = *(int *)(iVar + 0x4c);
    if (flt_457098[i] == REAL_ZERO_POOL)
      *(int *)&flt_457098[i] = *(int *)(iVar + 0x50);
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
      /* Single && chain matching the original's guard encodings (TEST
       * AH,0x41/JNE then two TEST AH,5/JP at 0xb8c1c..0xb8c4c, each jumping
       * to the reset path, increment block on the fallthrough). fabs() is
       * the FABS instruction there, not a compare-and-negate; NaN in either
       * float condition takes the reset path. A goto-chain of negated
       * guards is NaN-equivalent but VC71 lays the increment block out of
       * line ([FCOM-WARN]: lift-only TEST AH,5/JNP shape). */
      if (player_ui_autolevel_enabled(local_player_index) &&
          fabs(pc->field_0x14) > *(double *)0x25fea8 &&
          input.look_pitch_delta < *(float *)0x253f44 &&
          pc->field_0x30 < *(float *)0x253f44) {
        /* all conditions met — increment idle counter */
        int count = (int)pc->field_0x27 + 1;
        if (count < 0)
          count = 0;
        else if (count > 0x7f)
          count = 0x7f;
        pc->field_0x27 = (int8_t)count;
        pc->field_0x26 =
          (int16_t)(int8_t)count > *(int16_t *)((char *)game_tag_elem + 0x6e);
        goto final_copy;
      }
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
