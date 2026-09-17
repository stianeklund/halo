/* 0xe05f0 */
void overhead_map_initialize(void)
{
}

/* 0xe0600 */
void overhead_map_initialize_for_new_map(void)
{
}

/* 0xe0610 */
void overhead_map_dispose_from_old_map(void)
{
  *(int *)0x46beb0 = 0;
}

/* 0xe0620. No xrefs found; the first two stack arguments are never read, so
 * their types are unknown. The third argument is a pointer to two floats that
 * are cached verbatim and also converted to a halved, registration-relative
 * short position used to build a rectangle at 0x46bebc. */
/* Field roles inferred from use: index 0/2 take the y-derived coordinate and
 * index 1/3 the x-derived one, matching the engine's {top,left,bottom,right}
 * rectangle ordering. The 8-byte copy at the end is what the reference emits
 * as two dword stores. */
typedef struct short_rectangle2d {
  short top;
  short left;
  short bottom;
  short right;
} short_rectangle2d;

void FUN_000e0620(int unknown0, int unknown1, float *position)
{
  short_rectangle2d bounds;
  short x;
  short y;
  int registration;

  if (*(char *)0x46beb0 == 0)
    return;

  *(float *)0x46bec4 = position[0];
  *(float *)0x46bec8 = position[1];

  x = (short)(int)position[0];
  x = (short)(x >> 1);
  registration = *(int *)0x46bed8;
  x = (short)(x - *(short *)(registration + 0x10));

  y = (short)(int)position[1];
  y = (short)(y >> 1);
  y = (short)(y - *(short *)(registration + 0x12));

  *(short *)0x46bece = y;
  *(short *)0x46becc = x;

  bounds.top = (short)(y + *(short *)0x46beb2);
  bounds.left = (short)(x + *(short *)0x46beb4);
  bounds.bottom = (short)(y + *(short *)0x46beb6);
  bounds.right = (short)(x + *(short *)0x46beb8);

  if (*(char *)0x46beba != 0 && local_time_get() - *(int *)0x46bed0 <= 7)
    return;

  *(short_rectangle2d *)0x46bebc = bounds;
  *(int *)0x46bed0 = local_time_get();
}

void player_ui_dispose(void)
{
}

void player_ui_reset_single_player_local_player_controllers(void)
{
  word_46BFC4[0] = 0;
  word_46BFC4[1] = 1;
  word_46BFC4[2] = 2;
  word_46BFC4[3] = 3;
}

/* 0xe0740. Both stack slots arrive as 16-bit values and are range-checked as
 * signed 16-bit quantities (0..3), each branching to an assert on failure.
 * The assert reason strings and line numbers 0x7b/0x7e are the reference's own
 * PUSH immediates (PUSH 0x7b / PUSH 0x282740 at 0xe0751/0xe074c, line 0x7e
 * immediates at 0xe0756 and 0xe0782); assert_halt_msg re-derives them from
 * __LINE__, so the pushed constants differ from the reference. */
void player_ui_set_single_player_local_player_controller(
  __int16 local_player_index, __int16 controller_index)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  assert_halt_msg(controller_index >= 0 && controller_index < 4,
                  "invalid controller index");
  word_46BFC4[local_player_index] = controller_index;
}

__int16
player_ui_get_single_player_local_player_controller(__int16 local_player_index)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  return word_46BFC4[(__int16)local_player_index];
}

/* 0xe0810 */
int player_ui_get_single_player_local_player_from_controller(short local_player_index)
{
  int i;
  for (i = 0; i < 4; i++) {
    if (word_46BFC4[i] == local_player_index) {
      return i;
    }
  }
  return -1;
}

/* 0xe0840. The index is read as a 16-bit stack slot (MOV SI,word ptr [EBP+8])
 * and sign-extended (MOVSX EAX,SI) before both stores.
 * Store 1: MOV byte ptr [ECX+0x46bf14],1 with ECX = index*0x38 -- byte 0x34 of
 * the 0x38-stride per-local-player record that starts at player_ui_globals
 * (0x46bee0), i.e. player_ui_globals + index*0x38 + 0x34.
 * Store 2: MOV byte ptr [EAX+0x46bfc0],1 -- a separate 4-byte flag array that
 * sits at player_ui_globals + 0xe0, immediately after the four records and
 * immediately before word_46BFC4 (0x46bfc4).
 * The assert reason string and the line number 0x9d are the reference's own
 * PUSH immediates at 0xe085f/0xe0855. */
void player_ui_local_player_joined_multiplayer_game(short local_player_index)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  player_ui_globals[local_player_index * 0x38 + 0x34] = 1;
  player_ui_globals[0xe0 + local_player_index] = 1;
}

/* 0xe0890. Same 16-bit stack-slot index and sign-extend-then-*0x38 addressing
 * as player_ui_local_player_joined_multiplayer_game (0xe0840): the reference
 * reads byte 0x34 of the per-local-player record (MOV AL,[ECX+0x46bf14]),
 * the exact byte 0xe0840 sets to 1. Only AL is written, so the return is the
 * boolean byte itself. The assert reason string and the reference line
 * number 0xa7 are the reference's own PUSH immediates at 0xe08af/0xe08a5. */
bool player_ui_local_player_wants_to_play_multiplayer(short local_player_index)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  return player_ui_globals[local_player_index * 0x38 + 0x34] != 0;
}

/* 0xe08e0. The mirror of player_ui_local_player_joined_multiplayer_game
 * (0xe0840) for the per-record byte only: the index arrives as a 16-bit stack
 * slot and only byte 0x34 of the local player's record is cleared to 0 --
 * unlike 0xe0840, this does NOT clear the parallel 0x46bfc0 array at
 * player_ui_globals (0x46bee0). The 0x46bfc0 flag array that 0xe0840 also sets
 * is left untouched. The assert reason string and line number 0xb0 are the
 * reference's own PUSH immediates at 0xe08ff/0xe08f5. */
void player_ui_clear_multiplayer_autojoin_for_local_player(
  short local_player_index)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  player_ui_globals[local_player_index * 0x38 + 0x34] = 0;
}

/* 0xe0930. No arguments, no calls and no index register: four byte loads and
 * four byte stores. The reference copies each of the four bytes at
 * player_ui_globals + 0xe0 (0x46bfc0 -- the array
 * player_ui_local_player_joined_multiplayer_game (0xe0840) sets to 1) into byte
 * 0x34 of the corresponding per-local-player record (0x46bf14, 0x46bf4c,
 * 0x46bf84, 0x46bfbc) -- copying the joined state back into the field
 * player_ui_local_player_wants_to_play_multiplayer (0xe0890) reads and
 * player_ui_clear_multiplayer_autojoin_for_local_player (0xe08e0) clears.
 *
 * Spelled as four direct index stores to keep the reference's exact
 * load-byte/store-byte sequence without emitting a loop. */
void player_ui_autojoin_players_to_next_multiplayer_game(void)
{
  player_ui_globals[0x34] = player_ui_globals[0xe0];
  player_ui_globals[0x38 + 0x34] = player_ui_globals[0xe0 + 1];
  player_ui_globals[0x70 + 0x34] = player_ui_globals[0xe0 + 2];
  player_ui_globals[0xa8 + 0x34] = player_ui_globals[0xe0 + 3];
}

/* 0xe0960. No arguments and no locals: one byte store followed by three calls.
 * 0x46c034 is the single-byte flag player_ui_initialize
 * (0xe1350) clears. Nothing else in the lifted TU touches that offset, so its
 * meaning is still unproven; it stays a raw global write.
 *
 * Notice: the reference pushes 0 directly for the first dispose call's 0
 * store (PUSH 0x0 at 0xe0960, MOV at 0xe0962) and coalesces both cdecl
 * cleanups into a single ADD ESP,0x8 at 0xe097a -- which is why the call-site
 * audit reports two calls on that stack adjustment. */
void player_ui_clear_multiplayer_variant(void)
{
  *(char *)0x46c034 = 0;
  dispose_global_network_game_server();
  dispose_global_network_game_client();
  network_game_set_accept_remote_connections(0);
}

/* 0xe0980. The index arrives as a 16-bit stack slot (MOV SI,word ptr [EBP+8])
 * and the output buffer pointer as [EBP+0xc]. Both the non-NULL and range
 * asserts match the other accessors in this TU.
 *
 * The copy is a 48-byte block (PUSH 0x30 / PUSH EAX / PUSH EDI / CALL
 * csmemcpy, ADD ESP,0xc): record+0 of the 0x38-stride array, exactly the
 * same 0x30 bytes player_ui_initialize (0xe1350) zeroes. Argument order is
 * csmemcpy(out, profile, 0x30). Assert reason strings are the reference's own
 * immediates at 0xe09a7/0xe09a2/0xe099d. */
void player_ui_get_active_player_profile(short local_player_index, void *out)
{
  assert_halt_msg(out != NULL, "profile");
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  csmemcpy(out, player_ui_globals + local_player_index * 0x38, 0x30);
}

/* 0xe09e0. The index arrives as a 16-bit stack slot and both bounds tests are
 * signed 16-bit (TEST SI,SI / JL and CMP SI,4 / JGE). Unlike the functions
 * above there is no assert on an out-of-range index: both failures jump to
 * the shared `OR EAX,-0x1 / POP ESI / RET` exit, returning -1 directly.
 *
 * The read is MOV EAX,dword ptr [EDX+0x46bf10] with EDX = index * 0x38:
 * byte 0x30 of the per-local-player record. This is the field player_ui_initialize
 * (0xe1350) seeds to -1 (*(int *)(profile + 0x30) = -1), so -1 is both the
 * out-of-range sentinel and the unassigned-profile sentinel. Corroborated by
 * the *(int *)0x46bf10 == -1 test in player_ui_remember_player1_profile (0xe0c30).
 *
 * Function returns int, matching the 32-bit field width and the -1 sentinel. */
int player_ui_get_active_player_profile_index(short local_player_index)
{
  if (local_player_index < 0 || local_player_index >= 4)
    return -1;
  return *(int *)(player_ui_globals + local_player_index * 0x38 + 0x30);
}

/* 0xe0a10. Same 16-bit stack-slot index and sign-extend-then-*0x38 addressing
 * as player_ui_get_active_player_profile (0xe0980), but unlike the getter
 * (0xe09e0) an out-of-range index asserts rather than returning a sentinel:
 * the assert reason string and line number 0xb8 are the reference's own PUSH
 * immediates at 0xe0a2f/0xe0a25. The single load is
 * MOV AX,word ptr [ECX+0x46bef8] -- offset 0x18 inside
 * the 0x30-byte profile player_ui_get_active_player_profile (0xe0980) copies
 * and player_ui_initialize (0xe1350) seeds with -1 (*(int16_t *)(profile +
 * 0x18) = -1).
 *
 * Returns short (the 16-bit field value), widening to the int return slot
 * with MOVSX EAX,AX at 0xe0a34 before returning. */
short player_ui_get_last_single_player_level_played(short local_player_index)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  return *(short *)(player_ui_globals + local_player_index * 0x38 + 0x18);
}

/* 0xe0a60. One stack argument (MOV ESI,dword ptr [EBP+8]), asserted non-NULL
 * (line 0xc1 at 0xe0a76). The reference performs a single 48-byte copy:
 * PUSH 0x30 / PUSH ESI / PUSH 0x46bff0 / CALL csmemcpy / ADD ESP,0xc --
 * copying 0x30 bytes from the argument pointer to player_ui_globals + 0x110
 * (0x46bff0).
 *
 * It then sets byte 0x46c034 (player_ui_globals + 0x154) to 1. 0x46c034 is
 * the same byte player_ui_clear_multiplayer_variant (0xe0960) stores 0 to,
 * so this is its writer. The 0x30-byte payload at 0x46bff0 whose
 * meaning is still unproven, so it stays a raw index like at 0xe0960. */
void player_ui_set_game_variant(game_variant_t *variant)
{
  assert_halt_msg(variant != NULL, "game_variant");
  csmemcpy((void *)0x46bff0, variant, 0x30);
  *(char *)0x46c034 = 1;
}

/* 0xe0ab0. The exact inverse of player_ui_set_game_variant (0xe0a60): one
 * stack pointer asserted non-NULL ("game_variant", line 0xc8 at
 * 0xe0ac7/0xe0ac2/0xe0abd), then MOV AL,[0x46c034] / TEST AL,AL / JZ -- the
 * player_ui_globals + 0x154 flag 0xe0a60 sets and 0xe0960 clears. When set,
 * it copies 0x30 bytes from 0x46bff0 (the buffer 0xe0a60 populated) to the
 * caller's destination pointer (PUSH 0x30 / PUSH 0x46bff0 / PUSH ESI /
 * CALL csmemcpy / ADD ESP,0xc).
 *
 * The flag byte is re-loaded after the call (MOV AL,[0x46c034] at 0xe0af1)
 * rather than cached in a register across the csmemcpy call, and returned
 * as the boolean result: true if a variant was active and copied, false if
 * not. Spelled to reproduce the reload. */
bool player_ui_game_variant_specified(game_variant_t *out_variant)
{
  assert_halt_msg(out_variant != NULL, "game_variant");
  if (*(char *)0x46c034 != 0)
    csmemcpy(out_variant, (void *)0x46bff0, 0x30);
  return *(char *)0x46c034 != 0;
}

/* 0xe0b00. The index arrives as a 16-bit stack slot (MOV SI,word ptr [EBP+8]),
 * sign-extended and range-checked 0..3 (the same assert as the other
 * getters).
 *
 * The load is `MOV AL,byte ptr [ECX + 0x46bf08]` with ECX = index * 0x38:
 * byte 0x28 of the per-local-player record. The assert condition is
 * `player_ui_globals[index * 0x38 + 0x28] != 0`? No: the reference does
 * `CMP byte ptr [ECX+0x46bf08],0x0 / SETZ AL` -- returning TRUE when the byte
 * is zero (i.e. rumble disabled is 0, so 0 is disabled, matching the name
 * player_ui_rumble_disabled). Assert reason string and line number 0xcd are
 * immediates at 0xe0b2a/0xe0b20; kb.json had declared the parameter
 * `int16_t`, which matches the MOV SI stack read. */
bool player_ui_rumble_disabled(short local_player_index)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  return player_ui_globals[local_player_index * 0x38 + 0x28] == 0;
}

/* 0xe0b50. The single stack slot is read as MOV ESI,dword ptr [EBP+8] but every
 * following instruction uses SI: bounds check is TEST SI,SI / JL and CMP
 * SI,4 / JGE, and the offset is MOVSX ECX,SI followed by LEA EDX,[ECX+ECX*2]
 * / LEA EAX,[ECX+EDX*4] / ... (index * 0x38). So the parameter is 16-bit.
 *
 * Offset read is `MOV AL,byte ptr [EAX + 0x46bf0a]` -- byte 0x2a of the
 * record.
 *
 * Autolevel logic:
 *   CMP AL,0x0 / JZ -> returns false
 *   CMP AL,0x1 / JZ -> returns true
 *   otherwise -> asserts ("unknown autolevel setting", line 0xdb at 0xe0bcc)
 *   and returns false.
 *
 * Two distinct assert sites: the bounds assert at 0xe0b60 (line 0xd5) and the
 * unknown-value assert at 0xe0bc0 (line 0xdb). Assert reason strings are the
 * reference's own PUSH immediates at 0xe0b6e/0xe0b64 and 0xe0bcc/0xe0bc2. */
bool player_ui_autolevel_enabled(short local_player_index)
{
  unsigned char autolevel;

  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  autolevel = (unsigned char)player_ui_globals[local_player_index * 0x38 + 0x2a];
  if (autolevel == 0)
    return false;
  if (autolevel == 1)
    return true;
  assert_halt_msg(0, "unknown autolevel setting");
  return false;
}

/* 0xe0bf0. Bounds-check-then-forward: the reference reads the 16-bit index
 * (MOV SI,word ptr [EBP+8]), asserts it 0..3 (line 0xe4 at 0xe0c01), then
 * passes the dword at player_ui_globals + index*0x38 + 0x30 straight to
 * saved_game_file_get_path_to_enclosing_directory(profile_index, out_path)
 * -- the same profile index
 * player_ui_get_active_player_profile_index (0xe09e0) returns and
 * player_ui_remember_player1_profile (0xe0c30) tests against -1. The second
 * argument is the caller's buffer pointer (MOV EAX,dword ptr [EBP+0xc]).
 *
 * The call at 0xe0c1b is CALL 0x001c4da0 (ADD ESP,0x8, cdecl) --
 * saved_game_file_get_path_to_enclosing_directory. Its return value is AL
 * (boolean), and player_ui_get_path_to_local_player_profile_directory
 * returns that same AL directly. */
bool player_ui_get_path_to_local_player_profile_directory(
  short local_player_index, char *out_path)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  return saved_game_file_get_path_to_enclosing_directory(
    *(int *)(player_ui_globals + local_player_index * 0x38 + 0x30), out_path);
}

/* 0xe0c30. Records player 1's profile directory and index into the dedicated
 * player-1 globals, but only when player 1 (record 0) actually has an active
 * profile:
 *
 * MOV EAX,[0x0046bf10] reads player_ui_globals + 0x30 (local player 0's
 * active profile index). If it is -1, the function does nothing and returns
 * false (XOR BL,BL / MOV AL,BL).
 *
 * If active:
 *   1. Calls saved_game_file_get_path_to_enclosing_directory(profile_index,
 *      stack_buffer) -- 0x1c4da0, with a 256-byte stack buffer (0x100 bytes at
 *      [EBP-0x104]).
 *   2. If that succeeds (TEST AL,AL != 0), calls
 *      saved_game_file_remember_player1_last_used_profile_directory(stack_buffer)
 *      -- CALL 0x001c2e80.
 *   3. Stores the profile index into 0x0046c110 (the player-1 cached profile
 *      index global) and sets 0x0046c114 to 1 (the player-1 profile valid
 *      flag).
 *   4. Returns true. */
void player_ui_remember_player1_profile(bool a1)
{
  int eax;

  eax = *(int *)0x46bf10;
  if (*(int *)0x30f02c != eax) {
    if (eax != -1) {
      if (!FUN_001c1280(eax, (char *)0x46c110))
        error(2, (const char *)0x282810);
    } else {
      error(2, (const char *)0x282810);
    }
    *(int *)0x30f02c = *(int *)0x46bf10;
  }
  if (a1 && *(char *)0x46c110 != 0)
    saved_game_file_remember_player1_last_used_profile_directory((void *)0x46c110);
}

/* 0xe0c90. Lazy accessor over the two player-1 profile globals that
 * player_ui_remember_player1_profile (0xe0c30) also writes: the cached
 * index at 0x46c110 and the valid flag at 0x46c114.
 *
 * If the valid flag is 0 (CMP byte ptr [0x0046c114],0x0 / JNZ), the function
 * attempts to populate the cache:
 *   1. Calls player_profile_get_player1_last_used_index() -- CALL 0x001c2ec0.
 *   2. If that returns non-negative (TEST EAX,EAX / JL), it saves the result
 *      into 0x46c110 (MOV [0x0046c110],EAX).
 *   3. Sets the valid flag: MOV byte ptr [0x0046c114],0x1.
 *
 * Returns the cached dword at 0x46c110 in both cases (MOV EAX,[0x0046c110]),
 * which is -1 when no valid profile was found or the index that was loaded.
 * Returns int. */
int player_ui_get_player1_last_used_profile_index(void)
{
  if (*(char *)0x46c110 == 0) {
    if (saved_game_file_retrieve_player1_last_used_profile_directory((char *)0x46c110)) {
      *(int *)0x30f02c = saved_game_file_find_profile_index_for_directory_path((char *)0x46c110, 0);
    }
  }
  return *(int *)0x30f02c;
}

/* 0xe0cd0. No arguments, no locals, no frame: a straight-line teardown of any
 * running network session followed by setup of a fast local server for the
 * pregame screen. */
void player_ui_fast_setup_network_server(void)
{
  ui_widgets_close_all();
  dispose_global_network_game_client();
  dispose_global_network_game_server();
  set_game_connection(0);
  main_set_multiplayer_map_name((const char *)0x25386f);
  *(char *)0x46c034 = 0;
  if (!ui_widget_load_by_name_or_tag((const char *)0x2828e0, -1, 0, -1, -1, -1, -1)) {
    error(2, (const char *)0x282840);
    main_goto_main_menu();
    return;
  }
  game_engine_playlist_initialize();
  network_game_set_accept_remote_connections(1);
  if (FUN_0012a890() && FUN_0012a250()) {
    game_engine_playlist_begin();
    set_game_connection(2);
    return;
  }
  dispose_global_network_game_client();
  dispose_global_network_game_server();
  network_game_set_accept_remote_connections(0);
  error(2, (const char *)0x2828ac);
  main_goto_main_menu();
}

/* 0xe0d80 */
bool player_ui_edit_profile_is_default_profile(void)
{
  int profile_index;
  profile_index = *(int *)0x46c038;
  if (profile_index != -1) {
    if ((saved_game_file_get_type(profile_index) & 0xffff) <= 1) {
      return (bool)((profile_index >> 0x1e) & 1);
    }
    error(2, (const char *)0x282938);
  }
  return false;
}

/* 0xe0dd0. Returns true when the profile name currently being edited differs
 * from the pristine copy taken when editing began.
 *
 * 0x46c038 is the dword player_ui_initialize sets to -1 ("no saved game file
 * is being edited"); it is player_ui_globals + 0x158. The two compared name
 * buffers are player_ui_globals + 0x15c (0x46c03c, the live edit copy) and
 * player_ui_globals + 0x1c4 (0x46c0a4, the pristine copy 0x68 bytes later);
 * the reference compares 0xc unicode characters of each.
 *
 * The 0/1 type test is emitted as SUB EAX,0 / JZ / DEC EAX / JZ -- the MSVC
 * sequential-subtract switch lowering, with both cases sharing one target --
 * so it is written as a switch, not an if-chain. The XOR BL,BL feeding both
 * MOV AL,BL exits is the shared `false` return value. */
bool player_ui_edit_profile_name_is_dirty(void)
{
  bool dirty;
  int saved_game_file_index;
  int saved_game_file_type;

  dirty = false;
  saved_game_file_index = *(int *)0x46c038;
  if (saved_game_file_index != -1) {
    saved_game_file_type = saved_game_file_get_type(saved_game_file_index);
    switch (saved_game_file_type) {
    case 0:
      break;
    case 1:
      break;
    default:
      error(2, "unknown saved game file type being edited");
      return dirty;
    }

    if (ustrncmp((const wchar_t *)0x46c03c, (const wchar_t *)0x46c0a4, 0xc) !=
        0)
      return true;
  } else {
    error(2, "not currently editing a saved game file");
  }

  return dirty;
}

/* 0xe0e40. Opens the virtual keyboard on the profile name of the saved game
 * file currently being edited, and returns whether the keyboard session was
 * actually started.
 *
 * Same guard pair as player_ui_edit_profile_name_is_dirty above: 0x46c038
 * (player_ui_globals + 0x158) is -1 when no saved game file is being edited,
 * and only file types 0 and 1 are accepted -- emitted as the MSVC sequential
 * SUB EAX,0 / JZ / DEC EAX / JZ switch lowering with both cases sharing one
 * target. The XOR BL,BL feeding both MOV AL,BL exits is the shared `false`
 * return; the accepted path instead falls through with the callee's AL, so it
 * returns virtual_keyboard_set_validation's result directly.
 *
 * The edited name buffer is player_ui_globals + 0x15c (0x46c03c), 0x18 bytes
 * = 0xc unicode characters, matching the ustrncmp length in the sibling.
 * Caption index 0xa is pushed last, buffer size 0x18 second, buffer first. */
bool player_ui_prompt_user_to_rename_edit_profile(void)
{
  bool started;
  int saved_game_file_index;
  int saved_game_file_type;

  started = false;
  saved_game_file_index = *(int *)0x46c038;
  if (saved_game_file_index != -1) {
    saved_game_file_type = saved_game_file_get_type(saved_game_file_index);
    switch (saved_game_file_type) {
    case 0:
      break;
    case 1:
      break;
    default:
      error(2, "unknown saved game file type being edited");
      return started;
    }

    return virtual_keyboard_set_validation((wchar_t *)0x46c03c, 0x18, 0xa);
  } else {
    error(2, "not currently editing a saved game file");
  }

  return started;
}

/* 0xe0ea0. Returns the live edit copy of the player profile currently being
 * edited, or NULL when the saved game file being edited is not of that type.
 *
 * The reference loads the edited-file index from 0x46c038 (player_ui_globals +
 * 0x158, the dword player_ui_initialize sets to -1) and passes it straight to
 * saved_game_file_get_type -- unlike the two functions above there is no -1
 * guard and no error() report on this path.
 *
 * The type test is emitted branchless: NEG AX / SBB EAX,EAX / NOT EAX / AND
 * EAX,0x46c03c turns the callee's 16-bit result into a full-width mask that is
 * all-ones only when the type is 0, then ANDs it with the buffer address. So
 * only type 0 yields a pointer; the sibling at 0xe0ec0 covers the playlist
 * profile. The masked constant 0x46c03c is player_ui_globals + 0x15c, the same
 * live edit copy player_ui_edit_profile_name_is_dirty and
 * player_ui_prompt_user_to_rename_edit_profile read.
 *
 * The returned type is unproven: only the address is in evidence here, so the
 * pointer stays void *.
 *
 * VC71 sits at 88.9% (8/9) because MSVC lowers the select as
 * AND EAX,-0x46c03c / ADD EAX,0x46c03c instead of the reference's
 * NOT EAX / AND EAX,0x46c03c -- arithmetically the same mask, one instruction
 * apart. Measured identical for `type == 0 ? buf : NULL`,
 * `type != 0 ? NULL : buf`, `!type ? buf : NULL`, and the if/early-return
 * form; the delta is MSVC's ternary-to-arithmetic lowering choosing AND/ADD
 * over NOT/AND on this constant. */
void *player_ui_get_edit_player_profile(void)
{
  return saved_game_file_get_type(*(int *)0x46c038) == 0 ? (void *)0x46c03c :
                                                           NULL;
}

/* 0xe0ec0. Playlist-profile sibling of player_ui_get_edit_player_profile
 * (0xe0ea0): same single load of 0x46c038 into EAX, passed straight to
 * saved_game_file_get_type -- exactly the same frame.
 *
 * The select mask differs: the reference uses DEC AX / NEG AX / SBB EAX,EAX /
 * NOT EAX / AND EAX,0x46c03c -- which, after the DEC, produces a mask of
 * all-ones only when the type is 1 (0xe0ea0's mask, without the DEC, is
 * all-ones for 0). So only type 1 yields a pointer. The buffer is the shared
 * edit copy 0x46c03c is player_ui_globals + 0x15c, the same live
 * edit copy 0xe0ea0, player_ui_edit_profile_name_is_dirty and
 * player_ui_prompt_user_to_rename_edit_profile read -- both getters hand back
 * the one edit buffer and differ only in which file type they accept.
 *
 * The pointed-to type is unproven: only the address is in evidence, so the
 * return stays void *, matching the sibling.
 *
 * The MSVC select-lowering delta documented at 0xe0ea0 (AND EAX,-0x46c03c /
 * ADD EAX,0x46c03c in place of NOT EAX / AND EAX,0x46c03c) applies here too;
 * it is one instruction of the reference and not recoverable by re-spelling
 * the condition. */
void *player_ui_get_edit_playlist_profile(void)
{
  return saved_game_file_get_type(*(int *)0x46c038) == 1 ? (void *)0x46c03c :
                                                           NULL;
}

/* 0xe0ee0 */
bool player_ui_edit_profile_is_dirty(void)
{
  int edit_profile_index;
  int profile_type;
  int diff;
  bool is_dirty;
  short name_len;
  short header_len;

  is_dirty = false;
  edit_profile_index = *(int *)0x46c038;
  if (edit_profile_index != -1) {
    profile_type = saved_game_file_get_type(edit_profile_index) & 0xffff;
    if (profile_type) {
      if (profile_type != 1) {
        error(2, (const char *)0x28298c);
        return false;
      }
      *(short *)0x46c108 = 0;
      *(short *)0x46c0a0 = 0;
      diff = csmemcmp((void *)0x46c0a4, (void *)0x46c03c, 0x68);
      return diff != 0;
    }
    name_len = *(short *)0x46c0be;
    header_len = *(short *)0x46c056;
    *(short *)0x46c0be = 0;
    *(short *)0x46c056 = 0;
    diff = csmemcmp((void *)0x46c0a4, (void *)0x46c03c, 0x30);
    is_dirty = diff != 0;
    *(short *)0x46c056 = header_len;
    *(short *)0x46c0be = name_len;
  }
  return is_dirty;
}

/* 0xe0fd0. Marks every solo level complete on every difficulty in local
 * player 0's profile, then writes the profile back if that player has one.
 *
 * XOR EAX,EAX / MOV CL,0xf / OR byte ptr [EAX + 0x46befc],CL / INC EAX /
 * CMP EAX,0xa / JL -- ten consecutive BYTES (the OR is byte ptr; the
 * decompiler's int indexing is wrong) starting at 0x46befc, which is
 * player_ui_globals + 0x1c: offset 0x1c inside the 0x30-byte profile record
 * player_ui_initialize below zeroes at player_ui_globals + i*0x38. Ten bytes
 * with the low four bits set in each -- one byte per solo level, four
 * difficulty bits per level. Only record 0 is touched; there is no outer
 * per-local-player loop.
 *
 * The loop is spelled do/while because the reference is a bottom-tested loop
 * that MSVC left rolled: the equivalent `for (i = 0; i < 10; i++)` is fully
 * unrolled at /O2 into 39 instructions (37.7% match), while the do/while form
 * reproduces the reference's XOR/OR/INC/CMP/JL exactly (100.0%, 14/14).
 *
 * The [LOADW-WARN] on this function is a false lead: reference and candidate
 * both do a byte `orb %cl, ...`; only the symbol the displacement is taken
 * from differs (the reference relocates against 0x46befc itself, we index
 * from player_ui_globals + 0x1c).
 *
 * MOV EAX,[0x0046bf10] reads player_ui_globals + 0x30, the profile index
 * player_ui_initialize seeds with -1 and
 * player_ui_get_active_player_profile_index returns. The -1 test skips the
 * write-back when local player 0 has no active profile.
 *
 * PUSH 0x46bee0 / PUSH EAX / CALL 0x1c1bc0 / ADD ESP,0x8 -- cdecl, so the
 * last push is the first argument: (profile index, player_ui_globals), and
 * player_ui_globals is exactly profile record 0. 0x1c1bc0 reads two stack
 * dwords, asserts the one at +8 non-NULL ("profile",
 * c:\halo\SOURCE\saved games\player_profile.c line 0x100) and tests the one
 * at +4 against -1 -- matching both operands passed here. Its kb.json decl
 * was `(void)` despite the ADD ESP,0x8 cleanup; widened to the two arguments
 * the callee's own frame reads. */
void player_ui_activate_all_solo_levels(void)
{
  int i;
  int profile_index;

  i = 0;
  do {
    player_ui_globals[0x1c + i] |= 0xf;
    i++;
  } while (i < 10);

  profile_index = *(int *)(player_ui_globals + 0x30);
  if (profile_index != -1)
    player_profile_save(profile_index, player_ui_globals);
}

/* 0xe1000. Walks the players data table and prints one HUD message to every
 * live local player currently assigned a unit:
 *
 * The incoming wide string arrives in ESI as a register argument:
 * FUN_000e1000 has no stack arguments and reads ESI as the source pointer
 * for the hud_print_message call (MOV ESI,dword ptr [EBP+8] would be stack;
 * instead the body begins `MOV EBX,[0x005aa6d4]` with ESI untouched). It is
 * pushed at 0xe102a (PUSH ESI / PUSH EAX / CALL 0x000e62a0) as the
 * second cdecl argument to hud_print_message at 0xe1031. The sole caller,
 * FUN_000e1770 at 0xe178a, does MOV ESI,0x282b78 immediately before the CALL;
 * 0x282b78 is the literal L"Saving...".
 *
 * The loop runs over all players:
 *   1. players_globals is at 0x5aa6d4; the table size is *(int *)(0x5aa6d4 + 0x2e)
 *      (maximum allocated datums in the table, the standard data_array_t size).
 *   2. For each datum index i (0..size-1):
 *      - Reads datum_get(players_globals, i) -- CALL 0x00119320.
 *      - If datum is non-NULL (TEST EAX,EAX / JZ):
 *        - Reads player+0x2c (int16_t local_player_index). If it is != -1
 *          and player+0x34 (int unit_index) is != -1, calls
 *          hud_print_message(local_player_index, wide_message).
 *
 * hud_print_message is at 0xe62a0 (cdecl, (int local_player_index, const wchar_t *msg)).
 * Loop counter is 16-bit signed, incremented with INC DI, comparing against
 * the datum count in EBX with CMP DI,[EBX+0x2e]. */
void FUN_000e1000(wchar_t *message)
{
  short i;
  int table_size;
  char *player;

  if (player_data == NULL)
    return;

  table_size = *(int *)((char *)player_data + 0x2e);
  for (i = 0; i < (short)table_size; i++) {
    player = (char *)datum_get(player_data, i);
    if (player != NULL && *(short *)(player + 0x2c) != -1 &&
        *(int *)(player + 0x34) != -1) {
      hud_print_message(*(short *)(player + 0x2c), message);
    }
  }
}

/* 0xe1050. Whole body is `MOV AL,byte ptr [0x0046bf0b] / RET` -- an absolute,
 * parameterless byte load from 0x46bf0b, which is player_ui_globals (0x46bee0)
 * + 0x2b: offset 0x2b inside local player 0's 0x38-stride profile record.
 * 0x2b is the look-pitch invert flag byte, one byte past the autolevel byte
 * (0x2c rumble at 0xe0b00, 0x2e autolevel at 0xe0b50).
 *
 * Returns boolean (AL). No arguments. Named after its proven field role. */
bool player0_look_pitch_is_inverted(void)
{
  return player_ui_globals[0x2b] != 0;
}

/* 0xe1060. Same per-local-player record as 0xe1050, two bytes lower:
 * `MOV AL,byte ptr [0x0046bf09]`, which is player_ui_globals (0x46bee0) + 0x29:
 * offset 0x29 inside local player 0's 0x38-stride profile record.
 *
 * Unlike 0xe1050 this one normalizes the setting byte to a boolean:
 *   CMP AL,0x0 / JZ -> 1
 *   CMP AL,0x1 / JZ -> 1
 *   otherwise       -> 0
 *   XOR EAX,EAX / RET   .t: MOV EAX,1 / RET
 * so it yields 1 for the two values 0 and 1 and 0 for anything else. The
 * reference uses two separate equality compares, NOT an unsigned range test
 * (which would be `CMP AL,1 / JBE`), so the source form is `== 0 || == 1` and
 * is kept verbatim. The byte is loaded once into AL and reused across both
 * compares, hence the single local here.
 * What the setting selects is unknown -- no string, assert or PDB evidence
 * reaches offset 0x29 -- so the name stays FUN_000e1060.
 * The sole caller is the HaloScript evaluator FUN_000c3940 (call at 0xc394b),
 * which consumes AL immediately into a zeroed dword result slot. No callees.
 * The return is int-width, not byte-width: both exit arms write the whole
 * register (MOV EAX,0x1 / XOR EAX,EAX), where a byte-typed return would emit
 * MOV AL,0x1 / XOR AL,AL -- which is exactly how the byte-returning sibling
 * 0xe1050 ends. Declaring this one `unsigned char` cost the two return
 * instructions (77.8% VC71); `int` matches. */
int FUN_000e1060(void)
{
  unsigned char setting;

  setting = (unsigned char)player_ui_globals[0x29];
  if (setting == 0 || setting == 1)
    return 1;
  return 0;
}

/* 0xe1080 */
void generate_default_player_profile(void *profile)
{
  char *p;
  p = (char *)profile;
  if (!p) {
    assert_halt(0);
  }
  csmemset(p, 0, 0x30);
  *(short *)(p + 0x18) = -1;
  p[0x28] = 0;
  p[0x29] = 0;
}

/* 0xe10c0 */
void FUN_000e10c0(short local_player_index /* @<edi> */)
{
  static const float stick_sensitivities[10] = {
    10.0f, 12.5f, 15.0f, 17.5f, 20.0f, 22.5f, 25.0f, 27.5f, 30.0f, 32.5f
  };
  static const float pitch_yaw_sensitivities[10] = {
    5.0f, 6.25f, 7.5f, 8.75f, 10.0f, 11.25f, 12.5f, 13.75f, 15.0f, 16.25f
  };
  float controls[22];
  int profile_offset;
  int sens_index;
  short controller_index;
  char *profile_base;
  int button_layout;
  char button_config[16];

  csmemset(controls, 0, sizeof(controls));
  if (local_player_index < 0 || local_player_index >= 4) {
    assert_halt(0);
  }

  profile_offset = local_player_index * 0x38;
  profile_base = (char *)(player_ui_globals + profile_offset);

  sens_index = (int)((unsigned char)profile_base[0x2a]) - 1;
  if (sens_index < 0) sens_index = 0;
  if (sens_index > 9) sens_index = 9;

  controls[20] = stick_sensitivities[sens_index];
  controls[21] = pitch_yaw_sensitivities[sens_index];

  button_layout = (int)((unsigned char)profile_base[0x28]);
  csmemset(button_config, 0, sizeof(button_config));
  switch (button_layout) {
  case 0:
    button_config[0] = 1;
    button_config[1] = 5;
    button_config[2] = 6;
    button_config[3] = 7;
    button_config[4] = 0;
    button_config[5] = 4;
    button_config[6] = 2;
    button_config[7] = 3;
    button_config[8] = 0xc;
    button_config[9] = 0xd;
    button_config[10] = 0xe;
    button_config[11] = 0xf;
    break;
  case 1:
    button_config[0] = 1;
    button_config[1] = 0;
    button_config[2] = 7;
    button_config[3] = 6;
    button_config[4] = 0;
    button_config[5] = 4;
    button_config[6] = 2;
    button_config[7] = 3;
    button_config[8] = 0xc;
    button_config[9] = 0xd;
    button_config[10] = 0xe;
    button_config[11] = 0xf;
    break;
  case 2:
    button_config[0] = 1;
    button_config[1] = 0;
    button_config[2] = 0;
    button_config[3] = 7;
    button_config[4] = 6;
    button_config[5] = 4;
    button_config[6] = 2;
    button_config[7] = 3;
    button_config[8] = 0xc;
    button_config[9] = 0xd;
    button_config[10] = 0xe;
    button_config[11] = 0xf;
    break;
  case 3:
    button_config[0] = 6;
    button_config[1] = 0;
    button_config[2] = 1;
    button_config[3] = 7;
    button_config[4] = 0;
    button_config[5] = 4;
    button_config[6] = 2;
    button_config[7] = 3;
    button_config[8] = 0xc;
    button_config[9] = 0xd;
    button_config[10] = 0xe;
    button_config[11] = 0xf;
    break;
  case 4:
    button_config[0] = 0xf;
    button_config[1] = 5;
    button_config[2] = 6;
    button_config[3] = 7;
    button_config[4] = 0;
    button_config[5] = 4;
    button_config[6] = 2;
    button_config[7] = 3;
    button_config[8] = 0xc;
    button_config[9] = 0xd;
    button_config[10] = 0xe;
    button_config[11] = 1;
    break;
  default:
    break;
  }

  controller_index = word_46BFC4[local_player_index];
  if (controller_index != -1) {
    input_abstraction_update_local_player_preferences(controller_index, (int16_t *)button_config);
  }
}

/* 0xe12d0 */
void clear_profile_edit_data(void)
{
  *(int *)0x46c038 = -1;
}

/* 0xe12e0 */
void reset_local_player_profile(short local_player_index)
{
  int offset;
  char *profile;

  offset = local_player_index * 0x38;
  profile = (char *)(player_ui_globals + offset);
  if (!profile) {
    assert_halt(0);
  }
  csmemset(profile, 0, 0x30);
  *(short *)(profile + 0x18) = -1;
  profile[0x28] = 0;
  profile[0x29] = 0;
  *(int *)(profile + 0x30) = -1;
  word_46BFC4[local_player_index] = -1;
}

void player_ui_initialize(void)
{
  int i;
  char *profile;

  csmemset(player_ui_globals, 0, 0x230);
  for (i = 0; i < 4; i++) {
    profile = player_ui_globals + i * 0x38;
    assert_halt(profile != NULL);
    csmemset(profile, 0, 0x30);
    *(int16_t *)(profile + 0x18) = -1;
    *(char *)(profile + 0x28) = 0;
    *(char *)(profile + 0x29) = 0;
    *(int *)(profile + 0x30) = -1;
    word_46BFC4[i] = -1;
  }
  *(int *)0x46c038 = -1;
  *(char *)0x46c10c = 1;
}

/* 0xe13f0. The multiplayer-join reset half of player_ui_initialize (0xe1350):
 * the same four-entry 0x38-stride record array at player_ui_globals (0x46bee0)
 * is re-cleared, but the two globals 0xe1350 sets outside the loop (0x46c038,
 * 0x46c10c) are left alone, and two extra per-entry fields are cleared -- the
 * join flag at record+0x34 (MOV byte ptr [EBX],0x0, EBX walking 0x46bf14 by
 * 0x38) and the parallel byte array at player_ui_globals+0xe0 (0x46bfc0).
 *
 * 0x46bfc0 is the local-player-joined-multiplayer flag array
 * player_ui_local_player_joined_multiplayer_game (0xe0840) sets to 1, so this
 * cleans up join state without discarding the whole UI globals block.
 *
 * The per-record fields cleared in the loop match player_ui_initialize:
 *   - csmemset(record, 0, 0x30)
 *   - record+0x18 (short) = -1
 *   - record+0x28 (byte)  = 0
 *   - record+0x29 (byte)  = 0
 *   - record+0x30 (int)   = -1
 *   - word_46BFC4[i]      = -1
 *   - record+0x34 (byte)  = 0  (the extra field)
 *   - player_ui_globals[0xe0 + i] = 0 (the extra flag array)
 *
 * 0xe1350's ordering: the -1 word store into word_46BFC4 is scheduled before
 * the csmemset at 0xe1425, and the byte stores are scheduled before the
 * csmemset. Spelled to preserve the per-record write set.
 *
 * Loop runs 4 iterations (0..3), testing the local player index against 4.
 * The assert condition is `profile != NULL`, matching player_ui_initialize's
 * assert_halt; the assert reason string and line number 0xdb are the
 * PUSH immediates at 0xe141b/0xe1416; the tested condition is TEST ESI,ESI on
 * the record pointer. */
void player_ui_clear_multiplayer_joins(void)
{
  int local_player_index;
  char *record;

  for (local_player_index = 0; local_player_index < 4; local_player_index++) {
    record = player_ui_globals + local_player_index * 0x38;
    assert_halt_msg(record != NULL, "profile");

    word_46BFC4[local_player_index] = -1;
    *(short *)(record + 0x18) = -1;
    record[0x28] = 0;
    record[0x29] = 0;
    *(int *)(record + 0x30) = -1;
    record[0x34] = 0;

    csmemset(record, 0, 0x30);
    *(short *)(record + 0x18) = -1;
    record[0x28] = 0;
    record[0x29] = 0;
    *(int *)(record + 0x30) = -1;

    player_ui_globals[0xe0 + local_player_index] = 0;
  }
}

/* 0xe1490. The exact writer half of player_ui_get_active_player_profile
 * (0xe0980): same 0x30-byte profile at the front of the 0x38-stride
 * per-local-player record, and same profile index written to record + 0x30.
 *
 * The assert condition covers three invariants in one assert_halt_msg:
 *   1. local_player_index >= 0
 *   2. local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS (4)
 *   3. profile != NULL
 *
 * Emitted as three separate branches to line 0xe2 at 0xe1498/0xe149f/0xe14a7;
 * the assert reason string is `(local_player_index>=0) &&
 * (local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) && (profile != NULL)`,
 * and the line number 0xe2 and file path are the reference's own PUSH
 * immediates at 0xe14b6/0xe14b1/0xe14ac. Note the reason string only names
 * the macro, not the literal 4.
 *
 * Store-before-copy is the reference order: MOV [EAX+0x46bf10],ECX at 0xe14dd
 * sits between the csmemcpy argument pushes and the CALL at 0xe14e3. The store
 * is spelled through player_ui_globals rather than off a record pointer to
 * keep the reference's 0x46bf10(%eax) absolute-base addressing, the same way
 * player_ui_clear_multiplayer_joins does it.
 *
 * csmemcpy pushes are PUSH 0x30 / PUSH ESI / PUSH EDX, i.e. last push first:
 * csmemcpy(record, profile, 0x30) -- destination is the record, source is the
 * caller's profile, the inverse of the getter.
 *
 * CALL 0x000e10c0 at 0xe14eb takes no stack arguments; EDI still holds the
 * local player index (nothing writes EDI after 0xe1498), so this is
 * FUN_000e10c0's `short local_player_index@<edi>` register argument. The
 * POP EDI that follows is the epilogue restore, not an argument. */
void player_ui_set_active_player_profile(short local_player_index,
                                         int profile_index, void *profile)
{
  assert_halt_msg_at("(local_player_index>=0) && "
                     "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) && "
                     "(profile != NULL)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0xe2,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS &&
                       profile != NULL);
  *(int *)(player_ui_globals + local_player_index * 0x38 + 0x30) =
    profile_index;
  csmemcpy(player_ui_globals + local_player_index * 0x38, profile, 0x30);
  FUN_000e10c0(local_player_index);
}

/* 0xe1500 */
void player_ui_begin_editing_profile(int a1)
{
  int profile_type;
  size_t copy_size;

  *(int *)0x46c038 = -1;
  profile_type = saved_game_file_get_type(a1) & 0xffff;
  if (profile_type) {
    if (profile_type != 1) {
      error(2, (const char *)0x282a28, a1);
      return;
    }
    if (!playlist_profile_get(a1, (void *)0x46c0a4)) {
      error(2, (const char *)0x2829f0, a1);
      return;
    }
    copy_size = 0x68;
  } else {
    if (!player_profile_new(a1, (void *)0x46c0a4)) {
      error(2, (const char *)0x2829b8, a1);
      return;
    }
    copy_size = 0x30;
  }
  csmemcpy((void *)0x46c03c, (void *)0x46c0a4, copy_size);
  *(int *)0x46c038 = a1;
}

/* 0xe15b0 */
bool player_ui_save_profile(void)
{
  int profile_index;
  int profile_type;
  char filename[256];
  int new_index;

  profile_index = *(int *)0x46c038;
  profile_type = saved_game_file_get_type(profile_index) & 0xffff;
  if (profile_type) {
    if (profile_type != 1) {
      error(2, (const char *)0x282b40);
    } else {
      if (!player_ui_edit_profile_is_dirty()) {
        error(2, (const char *)0x282af8);
      }
      if (!(profile_index & 0x40000000)) {
        playlist_profile_save(profile_index, (void *)0x46c03c);
        if (saved_game_file_get_path_to_enclosing_directory(profile_index, filename)) {
          *(int *)0x46c038 = -1;
          saved_game_file_remember_last_used_multiplayer_variant_directory(filename);
          return true;
        }
        *(int *)0x46c038 = -1;
        return true;
      }
      if (ustrncmp((const wchar_t *)0x46c03c, (const wchar_t *)0x46c0a4, 0xc) != 0) {
        *(unsigned char *)0x46c0a0 &= ~1;
        new_index = playlist_profile_new(0, (void *)0x46c03c);
        if (new_index != -1) {
          playlist_profile_save(new_index, (void *)0x46c03c);
          *(int *)0x46c038 = new_index;
          if (saved_game_file_get_path_to_enclosing_directory(new_index, filename)) {
            saved_game_file_remember_last_used_multiplayer_variant_directory(filename);
          }
          *(int *)0x46c038 = -1;
          return true;
        }
        error(2, (const char *)0x282acc);
      } else {
        error(2, (const char *)0x282a80);
      }
    }
  } else {
    if (profile_index & 0x40000000) {
      error(2, (const char *)0x282a48);
    }
    if (!player_ui_edit_profile_is_dirty()) {
      error(2, (const char *)0x282af8);
    }
    player_profile_save(profile_index, (void *)0x46c03c);
    *(int *)0x46c038 = -1;
    return true;
  }
  *(int *)0x46c038 = -1;
  return false;
}

/* player_ui_end_editing_profile (0xe1760)
 *
 * Reference is two instructions:
 *   000e1760: MOV dword ptr [0x0046c038],0xffffffff
 *   000e176a: RET
 *
 * 0x46c038 is the "saved game file currently being edited" index that
 * player_ui_initialize (0xe1350) also clears to -1, and that
 * player_ui_edit_profile_name_is_dirty / player_ui_get_edit_player_profile
 * read back. Storing -1 marks "no profile is being edited". Spelled as the
 * absolute-address store the other functions in this TU use, since no named
 * global covers this offset yet. */
void player_ui_end_editing_profile(void)
{
  *(int *)0x46c038 = -1;
}

/* 0xe1770. Commits local player 0's look-pitch-invert setting: store the new
 * value, flush the profile to disk if one is active, then re-run the settings
 * apply pass.
 *
 * The single argument arrives as a byte, not a dword: MOV AL,byte ptr [EBP+8]
 * / MOV [0x0046bf0b],AL, with no zero/sign extension and no test. 0x46bf0b is
 * player_ui_globals (0x46bee0) + 0x2b -- exactly the byte
 * player0_look_pitch_is_inverted (0xe1050) reads back, so this is that
 * setting's writer.
 *
 * CMP dword ptr [0x0046bf10],-0x1 / JZ tests the global in memory and the
 * body then RELOADS it (MOV ECX,dword ptr [0x0046bf10]) for the call, so the
 * source reads player_ui_globals + 0x30 twice rather than caching it in a
 * local -- unlike player_ui_activate_all_solo_levels (0xe0fd0), whose
 * reference loads it once into EAX. Spelled that way here.
 *
 * MOV ESI,0x282b78 / CALL 0x000e1000: 0x282b78 in .rdata is the UTF-16
 * literal L"Saving..." (verified from the XBE image, not from the decompiler),
 * passed in ESI as FUN_000e1000's register argument.
 *
 * PUSH 0x46bee0 / PUSH ECX / CALL 0x001c1bc0 / ADD ESP,0x8 -- cdecl, last push
 * first, so (profile index, player_ui_globals), the same pair 0xe0fd0 passes.
 *
 * XOR EDI,EDI / CALL 0x000e10c0 is a register argument, not dead code:
 * FUN_000e10c0 never writes EDI (its prologue pushes EBP/EBX/ESI only) and
 * reads it at 0xe10d4 as `CMP DI,BX` and at 0xe1199 as `MOVSX ESI,DI` before
 * indexing player_ui_globals + DI*0x38 + 0x8, and pushes it at 0xe12ae. So it
 * takes a signed 16-bit local-player index in EDI, and this call site passes
 * local player 0. kb.json's `(void)` declaration was wrong; widened to
 * `short local_player_index@<edi>`. The PUSH EDI/POP EDI around the call is
 * the caller preserving the register, not an argument push. */
void FUN_000e1770(char invert)
{
  player_ui_globals[0x2b] = invert;
  if (*(int *)(player_ui_globals + 0x30) != -1) {
    FUN_000e1000((wchar_t *)L"Saving...");
    player_profile_save(*(int *)(player_ui_globals + 0x30),
                        player_ui_globals);
  }
  FUN_000e10c0(0);
}

/* 0xe17b0 */
void D3DDevice_SetRenderState_17(void)
{
}
