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

void overhead_map_post_rasterize(int unknown0, int unknown1, float *position)
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
  *(char *)0x46bebb = 0;
}

void player_ui_dispose(void)
{
  csmemset(player_ui_globals, 0, sizeof(player_ui_globals));
}

/* 0xe0720. Fills the 4-entry single-player controller table with -1 (no
 * controller). The reference is a single csmemset(&word_46BFC4, -1, 8). */
void player_ui_reset_single_player_local_player_controllers(void)
{
  csmemset(word_46BFC4, -1, sizeof(word_46BFC4));
}

/* 0xe0740. Both parameters are read as 16-bit stack slots ([EBP+8] into SI,
 * [EBP+0xc] into DI), and the store is `MOV word ptr [ECX*2+0x46bfc4],DI`
 * after a MOVSX of the index -- a short-indexed __int16[4] table.
 * The two asserts sit at original player_ui.c lines 0x77 and 0x79 (the PUSH
 * immediates at 0xe0756 and 0xe0782); assert_halt_msg re-derives them from
 * __LINE__, so the pushed constants differ from the reference. */
void player_ui_set_single_player_local_player_controller(
  __int16 local_player_index, __int16 controller_index)
{
  assert_halt_msg_at("invalid local player index", "c:\\halo\\SOURCE\\interface\\player_ui.c", 0x77,
      local_player_index >= 0 && local_player_index < 4);
  assert_halt_msg_at("invalid controller index", "c:\\halo\\SOURCE\\interface\\player_ui.c", 0x79,
      controller_index >= 0 && controller_index < 4);
  word_46BFC4[local_player_index] = controller_index;
}

__int16
player_ui_get_single_player_local_player_controller(__int16 local_player_index)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  return word_46BFC4[(__int16)local_player_index];
}

/* 0xe0810. Reverse lookup of the single-player controller table: return the
 * first local player index whose controller entry equals controller_index,
 * or -1 (NONE) when no entry matches.
 * Confirmed (0xe0810..0xe0839): MOV CX,word [EBP+8] (16-bit parameter);
 *   EAX counts from 0 (XOR EAX,EAX), MOVSX ESI,AX indexes
 *   CMP word [ESI*2+0x46bfc4],CX; JE -> return; INC EAX; CMP AX,4; JL loop.
 *   The miss path returns MOV AX,DX with EDX = -1 (OR EDX,-1), so only AX
 *   carries the result: the return is a 16-bit short, not int.
 * Confirmed: no assert on the argument.
 * Name/signature (inferred):
 *   short player_ui_get_single_player_local_player_from_controller(
 *   short controller_index). */
short player_ui_get_single_player_local_player_from_controller(
  short controller_index)
{
  short local_player_index;

  for (local_player_index = 0;
       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
       local_player_index++) {
    if (word_46BFC4[local_player_index] == controller_index) {
      return local_player_index;
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
  assert_halt_msg_at("(local_player_index>=0) && "
                     "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0x9d,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  player_ui_globals[local_player_index * 0x38 + 0x34] = 1;
  player_ui_globals[0xe0 + local_player_index] = 1;
}

/* 0xe0890. Same 16-bit stack-slot index and sign-extend-then-*0x38 addressing
 * as player_ui_local_player_joined_multiplayer_game (0xe0840): the reference
 * reads MOV AL,byte ptr [EAX+0x46bf14] with EAX = index*0x38, i.e. byte 0x34 of
 * the 0x38-stride per-local-player record at player_ui_globals (0x46bee0) --
 * the exact byte 0xe0840 sets to 1. Only AL is written, so the return is the
 * raw byte, not a normalized comparison. The assert reason string and line
 * number 0xa7 are the reference's own PUSH immediates at 0xe08af/0xe08a5. */
bool player_ui_local_player_wants_to_play_multiplayer(short local_player_index)
{
  assert_halt_msg_at("(local_player_index>=0) && "
                     "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0xa7,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return player_ui_globals[local_player_index * 0x38 + 0x34];
}

/* 0xe08e0. The mirror of player_ui_local_player_joined_multiplayer_game
 * (0xe0840) for the per-record byte only: the index arrives as a 16-bit stack
 * slot (MOV SI,word ptr [EBP+8]), is sign-extended and scaled (MOVSX EAX,SI /
 * IMUL EAX,EAX,0x38), and the single store is
 * MOV byte ptr [EAX+0x46bf14],0x0 -- byte 0x34 of the 0x38-stride record at
 * player_ui_globals (0x46bee0). The 0x46bfc0 flag array that 0xe0840 also sets
 * is NOT touched here. The assert reason string and line number 0xaf are the
 * reference's own PUSH immediates at 0xe08ff/0xe08f5. */
void player_ui_clear_multiplayer_autojoin_for_local_player(
  short local_player_index)
{
  assert_halt_msg_at("(local_player_index>=0) && "
                     "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0xaf,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  player_ui_globals[local_player_index * 0x38 + 0x34] = 0;
}

/* 0xe0930. No arguments, no calls and no index register: four byte loads and
 * four byte stores (MOV AL/CL/DL, then MOV to the destinations), so the
 * reference is straight-line, not a loop. It copies the four-byte flag array at
 * player_ui_globals + 0xe0 (0x46bfc0 -- the array
 * player_ui_local_player_joined_multiplayer_game (0xe0840) sets to 1) into byte
 * 0x34 of each of the four 0x38-stride per-local-player records
 * (0x46bf14/0x46bf4c/0x46bf84/0x46bfbc), i.e. the byte
 * player_ui_local_player_wants_to_play_multiplayer (0xe0890) reads and
 * player_ui_clear_multiplayer_autojoin_for_local_player (0xe08e0) clears.
 * The reference loads src 0/1/2 before storing dst 0, then loads src 3; the
 * four copies are independent, so the observable order is dst 0,1,2,3. */
void player_ui_autojoin_players_to_next_multiplayer_game(void)
{
  player_ui_globals[0x34] = player_ui_globals[0xe0];
  player_ui_globals[0x38 + 0x34] = player_ui_globals[0xe0 + 1];
  player_ui_globals[0x70 + 0x34] = player_ui_globals[0xe0 + 2];
  player_ui_globals[0xa8 + 0x34] = player_ui_globals[0xe0 + 3];
}

/* 0xe0960. No arguments and no locals: one byte store followed by three calls.
 * The store is MOV byte ptr [0x0046c034],0x0 -- a byte at player_ui_globals
 * (0x46bee0) + 0x154, inside the 0x230-byte block player_ui_initialize
 * (0xe1350) clears. Nothing else in the lifted TU touches that offset, so its
 * meaning is unproven; the surrounding calls make a "multiplayer variant
 * present" flag the obvious reading, but that is inference, not evidence.
 * The reference schedules the PUSH 0 for set_game_connection ahead of the
 * store (PUSH 0x0 at 0xe0960, MOV at 0xe0962) and coalesces both cdecl
 * cleanups into a single ADD ESP,0x8 at 0xe097a -- which is why the call-site
 * audit reports cleanup=2 stack args for game_set_game_variant even though it
 * takes one. Both calls pass a single immediate zero. */
void player_ui_clear_multiplayer_variant(void)
{
  player_ui_globals[0x154] = 0;
  set_game_connection(0);
  game_engine_dispose();
  game_set_game_variant(NULL);
}

/* 0xe0980. The index arrives as a 16-bit stack slot (MOV SI,word ptr [EBP+8])
 * and the destination as a dword (MOV EDI,dword ptr [EBP+0xc]). After the
 * assert the reference sign-extends and scales the index
 * (MOVSX EAX,SI / IMUL EAX,EAX,0x38 / ADD EAX,0x46bee0) and copies 0x30 bytes
 * into the caller's buffer -- the 0x30-byte profile at the front of the
 * 0x38-stride per-local-player record at player_ui_globals (0x46bee0), the
 * same 0x30 bytes player_ui_initialize (0xe1350) zeroes. Argument order is
 * PUSH 0x30 / PUSH EAX / PUSH EDI, i.e. csmemcpy(profile, record, 0x30).
 * The assert reason string and line number 0xee are the reference's own PUSH
 * immediates at 0xe09a7/0xe09a2/0xe099d. */
void player_ui_get_active_player_profile(short local_player_index,
                                         void *profile)
{
  assert_halt_msg_at("(local_player_index>=0) && "
                     "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) && "
                     "(profile != NULL)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0xee,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS &&
                       profile != NULL);
  csmemcpy(profile, player_ui_globals + local_player_index * 0x38, 0x30);
}

/* 0xe09e0. The index arrives as a 16-bit stack slot and both bounds tests are
 * 16-bit (TEST AX,AX / JL and CMP AX,0x4 / JGE) -- an out-of-range index is a
 * plain OR EAX,0xffffffff return, NOT an assert like the neighbouring
 * accessors. In range, the reference sign-extends and scales (MOVSX EAX,AX /
 * IMUL EAX,EAX,0x38) and does one dword load, MOV EAX,dword ptr [EAX+0x46bf10]
 * -- offset 0x30 of the 0x38-stride per-local-player record at
 * player_ui_globals (0x46bee0). That is the dword player_ui_initialize
 * (0xe1350) seeds to -1 (*(int *)(profile + 0x30) = -1), so -1 is both the
 * out-of-range result and the "no profile assigned" sentinel, matching the
 * *(int *)0x46bf10 == -1 test in player_ui_remember_player1_profile (0xe0c30).
 * The load is a full dword, so the return is int, not short. */
int player_ui_get_active_player_profile_index(short local_player_index)
{
  if (local_player_index < 0 ||
      local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
    return -1;
  return *(int *)(player_ui_globals + local_player_index * 0x38 + 0x30);
}

/* 0xe0a10. Same 16-bit stack-slot index and sign-extend-then-*0x38 addressing
 * as the neighbouring accessors (MOV SI,word ptr [EBP+8] / MOVSX EAX,SI /
 * IMUL EAX,EAX,0x38). Unlike player_ui_get_active_player_profile_index
 * (0xe09e0) an out-of-range index asserts rather than returning a sentinel:
 * the reason string and line number 0x109 are the reference's own PUSH
 * immediates at 0xe0a2f/0xe0a25. The single load is
 * MOV AX,word ptr [EAX+0x46bf06] -- a 16-bit read of offset 0x26 of the
 * 0x38-stride per-local-player record at player_ui_globals (0x46bee0), inside
 * the 0x30-byte profile player_ui_get_active_player_profile (0xe0980) copies
 * out. Only AX is written (no MOVSX/MOVZX), so the return is a short, not an
 * int; the field's signedness is not proven by this function alone. */
short player_ui_get_last_single_player_level_played(short local_player_index)
{
  assert_halt_msg_at("(local_player_index>=0) && "
                     "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0x109,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return *(short *)(player_ui_globals + local_player_index * 0x38 + 0x26);
}

/* 0xe0a60. One stack argument (MOV ESI,dword ptr [EBP+8]), asserted non-NULL
 * with reason "variant" at line 0x111, then copied verbatim:
 * PUSH 0x68 / PUSH ESI / PUSH 0x46bfcc / CALL csmemcpy, i.e.
 * csmemcpy(0x46bfcc, variant, 0x68). 0x46bfcc is player_ui_globals
 * (0x46bee0) + 0xec, and 0x68 is exactly sizeof(game_variant_t), so the
 * argument is a game_variant_t the UI stores into its own cached slot.
 * The trailing MOV byte ptr [0x0046c034],0x1 is player_ui_globals + 0x154 --
 * the same byte player_ui_clear_multiplayer_variant (0xe0960) stores 0 to,
 * which is why the "variant present" reading is the natural one; the offset's
 * meaning is still unproven, so it stays a raw index like at 0xe0960. */
void player_ui_set_game_variant(game_variant_t *variant)
{
  assert_halt_msg_at("variant", "c:\\halo\\SOURCE\\interface\\player_ui.c",
                     0x111, variant != NULL);
  csmemcpy(player_ui_globals + 0xec, variant, sizeof(game_variant_t));
  player_ui_globals[0x154] = 1;
}

/* 0xe0ab0. The exact inverse of player_ui_set_game_variant (0xe0a60): one
 * stack argument (MOV ESI,dword ptr [EBP+8]) asserted non-NULL with reason
 * "variant" at line 0x11c (the reference's own PUSH immediates at
 * 0xe0ac7/0xe0ac2/0xe0abd), then MOV AL,[0x46c034] / TEST AL,AL / JZ -- the
 * player_ui_globals + 0x154 flag 0xe0a60 sets and 0xe0960 clears. When set,
 * PUSH 0x68 / PUSH 0x46bfcc / PUSH ESI / CALL csmemcpy copies the cached
 * game_variant_t at player_ui_globals + 0xec OUT into the caller's buffer
 * (first PUSH is the last cdecl arg, so destination is the parameter).
 * The flag byte is re-loaded after the call (MOV AL,[0x46c034] at 0xe0af1)
 * and both paths fall into POP ESI / POP EBP / RET with AL live, so the
 * function returns that byte -- kb.json declared it void(void), which the
 * arg-count baseline had already flagged UNDER-DECLARED from the 1-arg
 * cleanup at its call site, and game_engine.c calls it through a
 * char(*)(void *) cast. */
bool player_ui_game_variant_specified(game_variant_t *variant)
{
  assert_halt_msg_at("variant", "c:\\halo\\SOURCE\\interface\\player_ui.c",
                     0x11c, variant != NULL);
  if (player_ui_globals[0x154])
    csmemcpy(variant, player_ui_globals + 0xec, sizeof(game_variant_t));
  return player_ui_globals[0x154];
}

/* 0xe0b00. The index arrives as a 16-bit stack slot (MOV SI,word ptr [EBP+8]),
 * so the parameter is a short, and the NONE test (CMP SI,-0x1 / JNZ) runs
 * BEFORE the range assert -- a caller may legitimately pass -1 and gets a
 * plain XOR AL,AL (false) without tripping the assert.
 * player_rumble.c feeds it
 * player_ui_get_single_player_local_player_from_controller, which returns -1.
 * The body sign-extends and scales (MOVSX EAX,SI / IMUL EAX,EAX,0x38) and does
 * MOV AL,byte ptr [EAX + 0x46bf0c] -- byte 0x2c of the 0x38-stride
 * per-local-player record at player_ui_globals (0x46bee0). Only AL is written,
 * so the return is the raw byte, not a normalized comparison.
 * The assert reason string and line number 0x131 are the reference's own PUSH
 * immediates at 0xe0b2a/0xe0b20; kb.json had declared the parameter
 * int controller_index, but the reference's own assert text names it
 * local_player_index. */
bool player_ui_rumble_disabled(short local_player_index)
{
  if (local_player_index == NONE)
    return false;
  assert_halt_msg_at("(local_player_index>=0) && "
                     "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0x131,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return player_ui_globals[local_player_index * 0x38 + 0x2c];
}

/* 0xe0b50. The single stack slot is read as MOV ESI,dword ptr [EBP+8] but every
 * use is 16-bit (TEST SI,SI / CMP SI,0x4 / CMP SI,-0x1), so the parameter is a
 * short. The reference's own assert text at line 0x140 names it
 * controller_index, not local_player_index as kb.json had declared it.
 * The controller -> local-player mapping is inlined, not a call: after
 * network_game_in_progress (0x12a000) returns false, the reference searches the
 * 4-entry single-player controller table with ECX preloaded to -1
 * (OR ECX,0xffffffff), EAX as the counter, and CMP word ptr
 * [EDX*0x2 + 0x46bfc4],SI -- i.e. word_46BFC4[i] == controller_index -- taking
 * the first match and otherwise leaving -1. When a network game IS in progress
 * the index passes through unchanged (ESI is never reloaded).
 * The NONE test (CMP SI,-0x1 / XOR AL,AL) runs before the second range assert,
 * so an unmapped controller returns false without tripping it.
 * The tail is MOVSX EAX,SI / IMUL EAX,EAX,0x38 / MOV AL,byte ptr
 * [EAX + 0x46bf0e] -- byte 0x2e of the 0x38-stride per-local-player record at
 * player_ui_globals (0x46bee0). Only AL is written, so the return is the raw
 * byte. Both assert reason strings and the line numbers 0x140/0x154 are the
 * reference's own PUSH immediates at 0xe0b6e/0xe0b64 and 0xe0bcc/0xe0bc2. */
bool player_ui_autolevel_enabled(short controller_index)
{
  short local_player_index;
  short i;

  assert_halt_msg_at("(controller_index>=0) && "
                     "(controller_index<MAXIMUM_GAMEPADS)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0x140,
                     controller_index >= 0 &&
                       controller_index < MAXIMUM_GAMEPADS);

  local_player_index = controller_index;
  if (!network_game_in_progress()) {
    local_player_index = NONE;
    for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++) {
      if (word_46BFC4[i] == controller_index) {
        local_player_index = i;
        break;
      }
    }
  }

  if (local_player_index == NONE)
    return false;

  assert_halt_msg_at("(local_player_index>=0) && "
                     "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0x154,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return player_ui_globals[local_player_index * 0x38 + 0x2e];
}

/* 0xe0bf0. Bounds-check-then-forward: the reference reads the 16-bit index
 * from the stack slot (MOV AX,word ptr [EBP+8]), rejects it with a plain
 * XOR AL,AL return on TEST AX,AX/JL or CMP AX,0x4/JGE -- no assert, unlike the
 * neighbouring accessors. In range it sign-extends and scales
 * (MOVSX EDX,AX / IMUL EDX,EDX,0x38) and loads the dword at offset 0x30 of the
 * 0x38-stride per-local-player record at player_ui_globals (0x46bee0), i.e.
 * MOV EAX,dword ptr [EDX+0x46bf10] -- the same active-profile-index dword
 * player_ui_get_active_player_profile_index (0xe09e0) returns and
 * player_ui_remember_player1_profile (0xe0c30) tests against -1. The second
 * stack argument (MOV ECX,dword ptr [EBP+0xc]) is forwarded untouched. The
 * call is cdecl with ADD ESP,0x8 (PUSH ECX then PUSH EAX, so the profile index
 * is arg 1 and the caller's buffer is arg 2) -- the identical (index, buffer)
 * pair player_ui_remember_player1_profile passes to the same 0x1c1280.
 * Nothing follows the CALL but POP EBP/RET, so the callee's AL is the return
 * value; the out-of-range path zeroes only AL, so the return type is bool. */
bool player_ui_get_path_to_local_player_profile_directory(
  short local_player_index, char *path)
{
  if (local_player_index >= 0 &&
      local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
    return player_profile_get_enclosing_directory_path(
      *(int *)(player_ui_globals + local_player_index * 0x38 + 0x30), path);
  return false;
}

void player_ui_remember_player1_profile(bool save)
{
  if (*(int *)0x30f02c != *(int *)0x46bf10) {
    if (*(int *)0x46bf10 == -1) {
      error(2, "player 1 has no active player profile assigned");
    } else {
      if (!player_profile_get_enclosing_directory_path(*(int *)0x46bf10,
                                                       (char *)0x46c110))
        error(2, "player 1 has no active player profile assigned");
    }
    *(int *)0x30f02c = *(int *)0x46bf10;
  }
  if (save && *(char *)0x46c110)
    ((void (*)(void *))0x1c2c50)((void *)0x46c110);
}

/* 0xe0c90. Lazy accessor over the two player-1 profile globals that
 * player_ui_remember_player1_profile (0xe0c30) also writes: the cached
 * directory-path buffer at 0x46c110 and the cached profile index dword at
 * 0x30f02c. Reference shape:
 *   MOV AL,[0x46c110] / TEST AL,AL / JNZ ret_cached  -- path already cached
 *   PUSH 0x46c110 / CALL 0x1c2d20 / ADD ESP,0x4 / TEST AL,AL / JZ ret_cached
 *   PUSH 0x0 / PUSH 0x46c110 / CALL 0x1c38d0 / ADD ESP,0x8
 *   MOV [0x30f02c],EAX / RET
 * ret_cached: MOV EAX,[0x30f02c] / RET
 * So 0x1c2d20 takes the buffer as its single cdecl arg and returns success in
 * AL (bool), and 0x1c38d0 takes (buffer, 0) -- first PUSH is the last arg --
 * and returns the index in EAX. The second argument of 0x1c38d0 is an
 * unknown constant 0 at this call site; its meaning is not established here.
 * The success path returns the freshly computed EAX without reloading the
 * global, hence the assignment-expression return. */
int player_ui_get_player1_last_used_profile_index(void)
{
  if (*(char *)0x46c110 == '\0') {
    if (saved_game_file_retrieve_player1_last_used_profile_directory(
          (char *)0x46c110))
      return (*(int *)0x30f02c =
                saved_game_file_find_profile_index_for_directory_path(
                  (char *)0x46c110, 0));
  }
  return *(int *)0x30f02c;
}

/* 0xe0cd0. No arguments, no locals, no frame: a straight-line teardown of any
 * existing network game followed by an attempt to bring up a server behind the
 * pregame screen. The two dispose calls at 0xe0cd5/0xe0cda are repeated
 * verbatim on the failure path at 0xe0d45/0xe0d4a.
 * The byte store MOV byte ptr [0x0046c034],0x0 at 0xe0d01 is player_ui_globals
 * (0x46bee0) + 0x154 -- the same byte player_ui_clear_multiplayer_variant
 * (0xe0960) clears and player_ui_set_game_variant (0xe0a60) sets; its meaning
 * stays unproven, so it is written as a raw index here too. The reference
 * schedules that store after the seven pushes for the widget load, and
 * coalesces three cdecl cleanups (1 + 1 + 7 dwords) into the single
 * ADD ESP,0x24 at 0xe0d0d -- which is why the call-site audit reports
 * cleanup=9 stack args for ui_widget_load_by_name_or_tag even though it takes
 * seven. The same coalescing gives ADD ESP,0xc at 0xe0d62 for the 1-dword
 * network_game_set_accept_remote_connections push plus error's two.
 * Widget-load arguments are PUSH -1 x4 / PUSH 0 / PUSH -1 / PUSH name, and the
 * first push is the last cdecl argument, so the name leads and every remaining
 * slot is -1 except is_child, which is 0.
 * Both boolean probes are tested as bytes (TEST AL,AL at 0xe0d28/0xe0d31) and
 * both jump to the same failure block at 0xe0d45, i.e. a short-circuit &&.
 * Neither 0x12a890 nor 0x12a250 is named in kb.json, so they keep their FUN_
 * names; from this site alone all that is proven is that each returns a
 * success byte and that the pair gates the playlist start.
 * Both error paths end in JMP 0x00100620 (0xe0d65/0xe0d79) -- a duplicated
 * tail call to main_goto_main_menu, not a shared join. */
void player_ui_fast_setup_network_server(void)
{
  ui_widgets_close_all();
  dispose_global_network_game_server();
  dispose_global_network_game_client();
  set_game_connection(0);
  main_set_multiplayer_map_name("");
  player_ui_globals[0x154] = 0;
  if (ui_widget_load_by_name_or_tag(
        "ui\\shell\\main_menu\\multiplayer_type_select\\connected\\pregame"
        "\\connected_pregame_screen",
        -1, 0, -1, -1, -1, -1) == NULL) {
    error(2, "failed to load network pregame screen... maybe you ran this "
             "from some place other than the game shell UI?");
    main_goto_main_menu();
    return;
  }

  game_engine_playlist_initialize();
  network_game_set_accept_remote_connections(1);
  if (create_global_network_game_server() && create_global_network_game_client()) {
    game_engine_playlist_begin();
    set_game_connection(2);
    return;
  }

  dispose_global_network_game_server();
  dispose_global_network_game_client();
  network_game_set_accept_remote_connections(0);
  error(2, "failed to initiate a multiplayer game server");
  main_goto_main_menu();
}

/* Bit 30 of a saved-game file index.  build_saved_game_file_index (0x1c3710)
 * sets it (inferred name _saved_game_file_index_read_only_bit), and
 * player_ui_save_profile below reports "saving over a default player profile"
 * when it is set. */
#define SAVED_GAME_FILE_INDEX_READ_ONLY_FLAG 0x40000000

/* 0xe0d80 -- player_ui_edit_profile_is_default_profile (inferred name).
 * Returns bit 30 (read-only / default-profile flag) of the saved-game file
 * index being edited, player_ui_globals + 0x158 (0x46c038).
 *   - index == -1: returns false with no report (XOR BL,BL / MOV AL,BL).
 *   - saved_game_file_get_type's 16-bit result is zero-extended (MOVZX EAX,AX)
 *     and range-checked with signed compares (JL 0 / JG 1), so it is held in
 *     an int.  Types 0 (player profile) and 1 (playlist) are accepted.
 *   - the flag is read by reloading the global: SHR EAX,0x1e / AND AL,1.
 *   - any other type reports "unknown saved game file type being edited"
 *     and returns false. */
bool player_ui_edit_profile_is_default_profile(void)
{
  bool result;
  int type;

  result = false;
  if (*(int *)(player_ui_globals + 0x158) != -1) {
    type = saved_game_file_get_type(*(int *)(player_ui_globals + 0x158));
    if (type >= 0 && type <= 1)
      result = (*(int *)(player_ui_globals + 0x158) &
                SAVED_GAME_FILE_INDEX_READ_ONLY_FLAG) != 0;
    else
      error(2, "unknown saved game file type being edited");
  }
  return result;
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
 * returns virtual_keyboard_launch's result directly.
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

    return virtual_keyboard_launch((wchar_t *)0x46c03c, 0x18, 0xa);
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
 * form, so it is a lowering choice, not a spelling that can be recovered. */
void *player_ui_get_edit_player_profile(void)
{
  return saved_game_file_get_type(*(int *)0x46c038) == 0 ? (void *)0x46c03c :
                                                           NULL;
}

/* 0xe0ec0. Playlist-profile sibling of player_ui_get_edit_player_profile
 * above: same body, one saved-game-file type value apart.
 *
 * MOV EAX,[0x46c038] / PUSH EAX / CALL saved_game_file_get_type / ADD ESP,4
 * loads the edited-file index from player_ui_globals + 0x158 and passes it
 * straight through -- no -1 guard, no error() report, exactly as at 0xe0ea0.
 *
 * The type test is the same branchless mask with one extra instruction in
 * front: DEC AX / NEG AX / SBB EAX,EAX / NOT EAX / AND EAX,0x46c03c. DEC AX
 * biases the callee's 16-bit result by one before NEG sets CF, so the mask is
 * all-ones only when the type is 1 (0xe0ea0's mask, without the DEC, is
 * all-ones only when the type is 0). Only type 1 yields a pointer.
 *
 * The masked constant 0x46c03c is player_ui_globals + 0x15c, the same live
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

/* 0xe0ee0 -- player_ui_edit_profile_is_dirty (inferred name).
 * Compares the live edit copy (player_ui_globals + 0x15c, 0x46c03c) against
 * the pristine copy taken by player_ui_begin_editing_profile
 * (player_ui_globals + 0x1c4, 0x46c0a4) with the 16-bit flags word of both
 * temporarily zeroed, so a flags-only change does not count as dirty.
 *   - index == -1: returns false, no report.
 *   - type 0 (player profile, 0x30 bytes): flags at record +0x1a --
 *     original 0x46c0be saved in SI, current 0x46c056 in DI; both zeroed,
 *     csmemcmp(original, current, 0x30), current restored, then original.
 *   - type 1 (playlist, 0x68 bytes): flags at record +0x64 -- original
 *     0x46c108 and current 0x46c0a0 spilled to [EBP-4]/[EBP-8]; same
 *     zero / compare / restore sequence with size 0x68.
 *   - other: "unknown profile type being edited", returns false.
 * The type switch is the SUB EAX,EBX / JZ / DEC EAX / JZ lowering. */
bool player_ui_edit_profile_is_dirty(void)
{
  bool result;
  unsigned short original_flags;
  unsigned short current_flags;

  result = false;
  if (*(int *)(player_ui_globals + 0x158) != -1) {
    switch (saved_game_file_get_type(*(int *)(player_ui_globals + 0x158))) {
    case 0:
      original_flags = *(unsigned short *)(player_ui_globals + 0x1c4 + 0x1a);
      current_flags = *(unsigned short *)(player_ui_globals + 0x15c + 0x1a);
      *(unsigned short *)(player_ui_globals + 0x1c4 + 0x1a) = 0;
      *(unsigned short *)(player_ui_globals + 0x15c + 0x1a) = 0;
      if (csmemcmp(player_ui_globals + 0x1c4, player_ui_globals + 0x15c,
                   0x30) != 0)
        result = true;
      *(unsigned short *)(player_ui_globals + 0x15c + 0x1a) = current_flags;
      *(unsigned short *)(player_ui_globals + 0x1c4 + 0x1a) = original_flags;
      break;

    case 1:
      original_flags = *(unsigned short *)(player_ui_globals + 0x1c4 + 0x64);
      current_flags = *(unsigned short *)(player_ui_globals + 0x15c + 0x64);
      *(unsigned short *)(player_ui_globals + 0x1c4 + 0x64) = 0;
      *(unsigned short *)(player_ui_globals + 0x15c + 0x64) = 0;
      if (csmemcmp(player_ui_globals + 0x1c4, player_ui_globals + 0x15c,
                   0x68) != 0)
        result = true;
      *(unsigned short *)(player_ui_globals + 0x1c4 + 0x64) = original_flags;
      *(unsigned short *)(player_ui_globals + 0x15c + 0x64) = current_flags;
      break;

    default:
      error(2, "unknown profile type being edited");
      break;
    }
  }
  return result;
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
    player_profile_get_from_path(profile_index, player_ui_globals);
}

/* 0xe1000. Walks the players data table and prints one HUD message to every
 * player that has a valid HUD slot.
 *
 * The message pointer arrives in ESI: the function never writes ESI (no
 * PUSH ESI in the prologue, no POP ESI in the epilogue) yet PUSHes it as the
 * second cdecl argument to hud_print_message at 0xe1031. The sole caller,
 * player0_look_invert_pitch at 0xe178a, does MOV ESI,0x282b78 immediately before the CALL;
 * 0x282b78 in .rdata is the UTF-16 literal L"Saving...". So ESI is a real
 * register argument, not a decompiler artifact, and the kb.json `(void)`
 * declaration was wrong.
 *
 * MOV EAX,[0x005aa6d4] / PUSH EAX / LEA ECX,[EBP-0x10] / PUSH ECX /
 * CALL 0x1197b0 -- cdecl, last push first, so (&iter, *(data_t **)0x5aa6d4).
 * The 0x10-byte frame is exactly one data_iter_t.
 *
 * MOVSX EAX,word ptr [EAX+0x2] / CMP AX,0xffff -- offset 2 of a player datum
 * is the signed 16-bit HUD/player index that hud_print_message takes as its
 * first argument, and -1 means "no slot"; the same guard-then-print pair
 * appears at players.c 0x1c (telefrag message).
 *
 * The reference hoists the first data_iterator_next call above the loop and
 * shares one cleanup (ADD ESP,0xc folds the iterator_new 8 and the first
 * next 4), which is the bottom-tested `while (player != NULL)` shape spelled
 * below, not a for/do-while. */
void hud_message_to_all(wchar_t *message)
{
  data_iter_t iter;
  char *player;

  data_iterator_new(&iter, *(data_t **)0x5aa6d4);
  player = (char *)data_iterator_next(&iter);
  while (player != NULL) {
    if (*(int16_t *)(player + 2) != -1)
      hud_print_message(*(int16_t *)(player + 2), message);
    player = (char *)data_iterator_next(&iter);
  }
}

/* 0xe1050. Whole body is `MOV AL,byte ptr [0x0046bf0b] / RET` -- an absolute,
 * unindexed byte load with no branch, no test and no normalization, so the
 * return is the raw setting byte in AL. 0x46bf0b is player_ui_globals
 * (0x46bee0) + 0x2b, i.e. byte 0x2b of the 0x38-stride per-local-player record
 * for local player 0 -- consistent with the name kb.json carries and with the
 * sibling accessors that reach the same record family through an index
 * (0x2c rumble at 0xe0b00, 0x2e autolevel at 0xe0b50).
 * The sole caller, the HaloScript evaluator FUN_000c3910 at 0xc3923, consumes
 * AL immediately (MOV byte ptr [EBP-4],AL over a zeroed dword slot), which is
 * why the return type is `unsigned char` rather than the `void` kb.json
 * originally declared. No callees. */
unsigned char player0_look_pitch_is_inverted(void)
{
  return (unsigned char)player_ui_globals[0x2b];
}

/* 0xe1060. Same per-local-player record as 0xe1050, two bytes lower:
 * 0x46bf09 = player_ui_globals (0x46bee0) + 0x29, i.e. byte 0x29 of local
 * player 0's 0x38-stride record -- the field player_ui_initialize clears with
 * `*(char *)(profile + 0x29) = 0`.
 * Unlike 0xe1050 this one normalizes the setting byte to a boolean:
 *   MOV AL,byte ptr [0x46bf09] / TEST AL,AL / JZ .t / CMP AL,1 / JZ .t /
 *   XOR EAX,EAX / RET   .t: MOV EAX,1 / RET
 * so it yields 1 for the two values 0 and 1 and 0 for anything else. The
 * reference uses two separate equality compares, NOT an unsigned range test
 * (which would be `CMP AL,1 / JBE`), so the source form is `== 0 || == 1` and
 * is kept verbatim. The byte is loaded once into AL and reused across both
 * compares, hence the single local here.
 * What the setting selects is unknown -- no string, assert or PDB evidence
 * reaches offset 0x29 -- so the name stays player0_joystick_set_is_normal.
 * The sole caller is the HaloScript evaluator FUN_000c3940 (call at 0xc394b),
 * which consumes AL immediately into a zeroed dword result slot. No callees.
 * The return is int-width, not byte-width: both exit arms write the whole
 * register (MOV EAX,0x1 / XOR EAX,EAX), where a byte-typed return would emit
 * MOV AL,0x1 / XOR AL,AL -- which is exactly how the byte-returning sibling
 * 0xe1050 ends. Declaring this one `unsigned char` cost the two return
 * instructions (77.8% VC71); `int` matches. */
int player0_joystick_set_is_normal(void)
{
  unsigned char setting;

  setting = (unsigned char)player_ui_globals[0x29];
  if (setting == 0 || setting == 1)
    return 1;
  return 0;
}

/* Enumerations and records used by set_local_player_controls_from_player_profile.
 * Names and member order are inferred.
 * Every value below is
 * confirmed against the 2276 binary: the five button-preset arms of the
 * jump table at 0xe12bc store exactly these codes, the joystick preset is
 * clamped at 3, and the look-sensitivity tables have 10 entries. */
enum {
  _gamepad_analog_button_a = 0,
  _gamepad_analog_button_b,
  _gamepad_analog_button_x,
  _gamepad_analog_button_y,
  _gamepad_analog_button_black,
  _gamepad_analog_button_white,
  _gamepad_analog_button_left_trigger,
  _gamepad_analog_button_right_trigger,
  _gamepad_binary_button_dpad_up = 8,
  _gamepad_binary_button_dpad_down,
  _gamepad_binary_button_dpad_left,
  _gamepad_binary_button_dpad_right,
  _gamepad_binary_button_start,
  _gamepad_binary_button_back,
  _gamepad_binary_button_left_thumb,
  _gamepad_binary_button_right_thumb
};

enum {
  _button_preset_standard = 0,
  _button_preset_swap_triggers,
  _button_preset_swap_a_and_left_trigger,
  _button_preset_swap_b_and_left_trigger,
  _button_preset_swap_b_and_right_thumb
};

enum {
  _joystick_preset_standard = 0,
  _joystick_preset_south_paw,
  _joystick_preset_legacy,
  _joystick_preset_legacy_south_paw
};

#define NUMBER_OF_LOOK_SENSITIVITY_SETTINGS 10

/* The last 8 bytes (+0x28) of the 0x30-byte player profile record.  Offsets
 * +0/+1/+2/+3/+5 are read here; +4 is the byte player_ui_rumble_disabled
 * reads (record +0x2c), +6 the byte player_ui_autolevel_enabled reads
 * (record +0x2e), and player_profile_set_to_default writes +0..+7. */
typedef struct player_profile_controller_settings {
  unsigned char button_preset;
  unsigned char joystick_preset;
  unsigned char look_sensitivity;
  bool invert_look;
  bool vibration_disabled;
  bool flight_stick_aircraft_controls;
  bool autocenter;
  bool ingame_help_disabled;
} player_profile_controller_settings;
cs(player_profile_controller_settings, 0x8);
co(player_profile_controller_settings, look_sensitivity, 0x2);
co(player_profile_controller_settings, flight_stick_aircraft_controls, 0x5);

/* The 0x18-byte block input_abstraction_update_local_player_preferences
 * (0xce740) copies into its per-controller table at 0x46b820.  Offsets from
 * the stores at 0xe11db (+4), 0xe11ec (+0), [EBP-0x10..-5] (+8..+0x13),
 * [EBP-4] word (+0x14), [EBP-2] (+0x16) and [EBP-1] (+0x17); that callee
 * asserts +0x10/+0x11 (buttons 8/9) are start/back. */
typedef struct game_input_preferences {
  float yaw_rate;
  float pitch_rate;
  unsigned char game_control_to_xbox_buttons[12];
  short joystick_controls;
  bool invert_look;
  bool invert_look_aircraft_control;
} game_input_preferences;
cs(game_input_preferences, 0x18);
co(game_input_preferences, game_control_to_xbox_buttons, 0x8);
co(game_input_preferences, joystick_controls, 0x14);
co(game_input_preferences, invert_look_aircraft_control, 0x17);

/* 0xe10c0 -- set_local_player_controls_from_player_profile (inferred name;
 * the 2276 assert sits at line 0x392).
 *
 * local_player_index arrives in DI: both callers (0xe14eb, 0xe17a7) CALL with
 * no pushes and ADD nothing, and the body reads DI before writing it
 * (CMP DI,BX at 0xe10d4).  EDI is never written, and is pushed as the
 * fallback controller index at 0xe12ae.
 *
 * The controller settings are record +0x28 of the 0x38-stride local-player
 * record (ADD EAX,0x46bf08 after IMUL 0x38).  look_sensitivity is read with
 * MOVZX; sensitivity-1 is clamped into [0,9] twice, once into the short pitch
 * index (DX) and once into the yaw index (CX).  The pitch table is the one
 * built at [EBP-0x40] (40..130) and lands at preferences +4; the yaw table at
 * [EBP-0x68] (80..260) lands at +0.  The joystick preset is clamped at 3 with
 * an unsigned compare (JBE).  Button-preset values outside 0..4 skip every
 * button store, leaving the zero-initialised mapping.  The controller comes
 * from word_46BFC4 (zero-extended word load, compared against 0xffff); on -1
 * the local player index itself is passed. */
void set_local_player_controls_from_player_profile(short local_player_index)
{
  game_input_preferences preferences = { 0 };
  float pitch_rate_table[NUMBER_OF_LOOK_SENSITIVITY_SETTINGS] = {
    40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 110.0f, 120.0f, 130.0f
  };
  float yaw_rate_table[NUMBER_OF_LOOK_SENSITIVITY_SETTINGS] = {
    80.0f, 100.0f, 120.0f, 140.0f, 160.0f, 180.0f, 200.0f, 220.0f, 240.0f,
    260.0f
  };
  player_profile_controller_settings *controls;
  int look_sensitivity;
  int yaw_index;
  short pitch_index;
  short controller_index;

  assert_halt_msg_at("(local_player_index>=0) && "
                     "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                     "c:\\halo\\SOURCE\\interface\\player_ui.c", 0x392,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  controls = (player_profile_controller_settings *)(player_ui_globals +
                                                     local_player_index * 0x38 +
                                                     0x28);
  look_sensitivity = controls->look_sensitivity;
  yaw_index = look_sensitivity - 1;
  if (look_sensitivity - 1 < 0) {
    pitch_index = 0;
  } else if (yaw_index > NUMBER_OF_LOOK_SENSITIVITY_SETTINGS - 1) {
    pitch_index = NUMBER_OF_LOOK_SENSITIVITY_SETTINGS - 1;
  } else {
    pitch_index = (short)yaw_index;
  }
  if (yaw_index < 0) {
    yaw_index = 0;
  } else if (yaw_index > NUMBER_OF_LOOK_SENSITIVITY_SETTINGS - 1) {
    yaw_index = NUMBER_OF_LOOK_SENSITIVITY_SETTINGS - 1;
  }

  preferences.pitch_rate = pitch_rate_table[pitch_index];
  preferences.yaw_rate = yaw_rate_table[(short)yaw_index];
  preferences.joystick_controls =
    controls->joystick_preset > _joystick_preset_legacy_south_paw ?
      _joystick_preset_legacy_south_paw :
      controls->joystick_preset;

  switch (controls->button_preset) {
  case _button_preset_standard:
    preferences.game_control_to_xbox_buttons[0] = _gamepad_analog_button_a;
    preferences.game_control_to_xbox_buttons[1] = _gamepad_analog_button_black;
    preferences.game_control_to_xbox_buttons[2] = _gamepad_analog_button_x;
    preferences.game_control_to_xbox_buttons[3] = _gamepad_analog_button_y;
    preferences.game_control_to_xbox_buttons[4] = _gamepad_analog_button_b;
    preferences.game_control_to_xbox_buttons[5] = _gamepad_analog_button_white;
    preferences.game_control_to_xbox_buttons[6] =
      _gamepad_analog_button_left_trigger;
    preferences.game_control_to_xbox_buttons[7] =
      _gamepad_analog_button_right_trigger;
    preferences.game_control_to_xbox_buttons[8] = _gamepad_binary_button_start;
    preferences.game_control_to_xbox_buttons[9] = _gamepad_binary_button_back;
    preferences.game_control_to_xbox_buttons[10] =
      _gamepad_binary_button_left_thumb;
    preferences.game_control_to_xbox_buttons[11] =
      _gamepad_binary_button_right_thumb;
    break;

  case _button_preset_swap_triggers:
    preferences.game_control_to_xbox_buttons[0] = _gamepad_analog_button_a;
    preferences.game_control_to_xbox_buttons[1] = _gamepad_analog_button_black;
    preferences.game_control_to_xbox_buttons[2] = _gamepad_analog_button_x;
    preferences.game_control_to_xbox_buttons[3] = _gamepad_analog_button_y;
    preferences.game_control_to_xbox_buttons[4] = _gamepad_analog_button_b;
    preferences.game_control_to_xbox_buttons[5] = _gamepad_analog_button_white;
    preferences.game_control_to_xbox_buttons[6] =
      _gamepad_analog_button_right_trigger;
    preferences.game_control_to_xbox_buttons[7] =
      _gamepad_analog_button_left_trigger;
    preferences.game_control_to_xbox_buttons[8] = _gamepad_binary_button_start;
    preferences.game_control_to_xbox_buttons[9] = _gamepad_binary_button_back;
    preferences.game_control_to_xbox_buttons[10] =
      _gamepad_binary_button_left_thumb;
    preferences.game_control_to_xbox_buttons[11] =
      _gamepad_binary_button_right_thumb;
    break;

  case _button_preset_swap_a_and_left_trigger:
    preferences.game_control_to_xbox_buttons[0] =
      _gamepad_analog_button_left_trigger;
    preferences.game_control_to_xbox_buttons[1] = _gamepad_analog_button_black;
    preferences.game_control_to_xbox_buttons[2] = _gamepad_analog_button_x;
    preferences.game_control_to_xbox_buttons[3] = _gamepad_analog_button_y;
    preferences.game_control_to_xbox_buttons[4] = _gamepad_analog_button_b;
    preferences.game_control_to_xbox_buttons[5] = _gamepad_analog_button_white;
    preferences.game_control_to_xbox_buttons[6] = _gamepad_analog_button_a;
    preferences.game_control_to_xbox_buttons[7] =
      _gamepad_analog_button_right_trigger;
    preferences.game_control_to_xbox_buttons[8] = _gamepad_binary_button_start;
    preferences.game_control_to_xbox_buttons[9] = _gamepad_binary_button_back;
    preferences.game_control_to_xbox_buttons[10] =
      _gamepad_binary_button_left_thumb;
    preferences.game_control_to_xbox_buttons[11] =
      _gamepad_binary_button_right_thumb;
    break;

  case _button_preset_swap_b_and_left_trigger:
    preferences.game_control_to_xbox_buttons[0] = _gamepad_analog_button_a;
    preferences.game_control_to_xbox_buttons[1] = _gamepad_analog_button_black;
    preferences.game_control_to_xbox_buttons[2] = _gamepad_analog_button_x;
    preferences.game_control_to_xbox_buttons[3] = _gamepad_analog_button_y;
    preferences.game_control_to_xbox_buttons[4] =
      _gamepad_analog_button_left_trigger;
    preferences.game_control_to_xbox_buttons[5] = _gamepad_analog_button_white;
    preferences.game_control_to_xbox_buttons[6] = _gamepad_analog_button_b;
    preferences.game_control_to_xbox_buttons[7] =
      _gamepad_analog_button_right_trigger;
    preferences.game_control_to_xbox_buttons[8] = _gamepad_binary_button_start;
    preferences.game_control_to_xbox_buttons[9] = _gamepad_binary_button_back;
    preferences.game_control_to_xbox_buttons[10] =
      _gamepad_binary_button_left_thumb;
    preferences.game_control_to_xbox_buttons[11] =
      _gamepad_binary_button_right_thumb;
    break;

  case _button_preset_swap_b_and_right_thumb:
    preferences.game_control_to_xbox_buttons[0] = _gamepad_analog_button_a;
    preferences.game_control_to_xbox_buttons[1] = _gamepad_analog_button_black;
    preferences.game_control_to_xbox_buttons[2] = _gamepad_analog_button_x;
    preferences.game_control_to_xbox_buttons[3] = _gamepad_analog_button_y;
    preferences.game_control_to_xbox_buttons[4] =
      _gamepad_binary_button_right_thumb;
    preferences.game_control_to_xbox_buttons[5] = _gamepad_analog_button_white;
    preferences.game_control_to_xbox_buttons[6] =
      _gamepad_analog_button_left_trigger;
    preferences.game_control_to_xbox_buttons[7] =
      _gamepad_analog_button_right_trigger;
    preferences.game_control_to_xbox_buttons[8] = _gamepad_binary_button_start;
    preferences.game_control_to_xbox_buttons[9] = _gamepad_binary_button_back;
    preferences.game_control_to_xbox_buttons[10] =
      _gamepad_binary_button_left_thumb;
    preferences.game_control_to_xbox_buttons[11] = _gamepad_analog_button_b;
    break;
  }

  preferences.invert_look = controls->invert_look;
  preferences.invert_look_aircraft_control =
    controls->flight_stick_aircraft_controls;
  controller_index = word_46BFC4[local_player_index];
  if (controller_index != -1) {
    input_abstraction_update_local_player_preferences(controller_index,
                                                      &preferences);
  } else {
    input_abstraction_update_local_player_preferences(local_player_index,
                                                      &preferences);
  }
}

void player_ui_initialize(void)
{
  int i;
  char *profile;

  csmemset(player_ui_globals, 0, 0x230);
  for (i = 0; i < 4; i++) {
    profile = player_ui_globals + i * 0x38;
    assert_halt_msg_at("profile", "c:\\halo\\SOURCE\\interface\\player_ui.c", 0x365, profile != NULL);
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
 * Those are exactly the two bytes
 * player_ui_local_player_joined_multiplayer_game (0xe0840) sets to 1, so this
 * clears every local player's joined state. Loop control is the
 * strength-reduced record+0x34 pointer (CMP EBX,0x46bff4 after ADD EBX,0x38 --
 * four iterations, 0x46bf14/0x4c/0x84/0xbc), while the counter lives in the
 * dword stack slot [EBP-4]. The counter is a dword (MOV EAX,[EBP-4] / INC EAX /
 * MOV [EBP-4],EAX) but two of its three uses re-read it sign-extended from 16
 * bits (MOVSX EDI,word ptr [EBP-4] for the record base, MOVSX EAX,word ptr
 * [EBP-4] for the 0x46bfc4 index) while the 0x46bfc0 index is the plain dword
 * -- hence the explicit (short) casts on exactly those two, which are
 * load-bearing for codegen. Store order follows the disassembly (0x18, 0x28,
 * 0x29, then word_46BFC4, then 0x30, 0x34, 0x46bfc0), which differs from
 * 0xe1350's ordering: the -1 word store into word_46BFC4 is scheduled before
 * the -1 dword store into record+0x30, both reusing the ECX register set by OR
 * ECX,0xffffffff. Offsets 0x18/0x28/0x29 sit inside the 0x30 bytes csmemset
 * already zeroed and offset 0x18 is then set to -1, so the redundant byte
 * clears are kept verbatim
 * -- they are separate stores in the reference.
 * The assert reason string "profile" and line 0x365 are the reference's own
 * PUSH immediates at 0xe141b/0xe1416; the tested condition is TEST ESI,ESI on
 * the record base, which can never be NULL here.
 * `joined` is written as an explicit walking pointer rather than
 * profile+0x34 because that is what the reference's EBX is, and because the
 * extra loop-carried value is what pushes the counter out of a register and
 * into the [EBP-4] slot: with profile+0x34 the whole function fits in
 * EBX/ESI/EDI, cl.exe drops the EBP frame entirely and the loop terminates on
 * CMP $4 rather than on the pointer (measured 76.2%, 37 of 47 reference
 * instructions, the deficit being exactly the frame setup/teardown and the
 * five stack-slot accesses). The record+0x30 store is spelled through
 * player_ui_globals rather than profile to keep the reference's
 * 0x46bf10(%edi) absolute-base addressing instead of 0x30(%esi). */
void player_ui_clear_multiplayer_joins(void)
{
  int local_player_index;
  char *profile;
  char *joined;

  joined = player_ui_globals + 0x34;
  for (local_player_index = 0;
       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
       local_player_index++) {
    profile = player_ui_globals + (short)local_player_index * 0x38;
    assert_halt_msg_at("profile", "c:\\halo\\SOURCE\\interface\\player_ui.c",
                       0x365, profile != NULL);
    csmemset(profile, 0, 0x30);
    *(int16_t *)(profile + 0x18) = -1;
    *(char *)(profile + 0x28) = 0;
    *(char *)(profile + 0x29) = 0;
    word_46BFC4[(short)local_player_index] = -1;
    *(int *)(player_ui_globals + (short)local_player_index * 0x38 + 0x30) = -1;
    *joined = 0;
    player_ui_globals[0xe0 + local_player_index] = 0;
    joined += 0x38;
  }
}

/* 0xe1490. The exact writer half of player_ui_get_active_player_profile
 * (0xe0980): same 0x30-byte profile at the front of the 0x38-stride
 * per-local-player record at player_ui_globals (0x46bee0), same assert reason
 * string, copied in the opposite direction.
 *
 * Three stack arguments, and kb.json's `(void)` declaration was wrong -- the
 * reference reads all three slots:
 *   MOV ESI,dword ptr [EBP+0x10]   the source profile pointer
 *   MOV EDI,dword ptr [EBP+0x8]    the local player index
 *   MOV ECX,dword ptr [EBP+0xc]    the active-profile index dword
 * The index is a 16-bit stack slot -- both bounds tests are on DI
 * (TEST DI,DI / JL and CMP DI,0x4 / JGE) and the scale is MOVSX EAX,DI /
 * IMUL EAX,EAX,0x38 -- so it is a signed short, matching every other
 * per-local-player accessor in this TU. [EBP+0xc] is loaded and stored as a
 * full dword into record+0x30, which is exactly the dword
 * player_ui_get_active_player_profile_index (0xe09e0) reads back and that
 * player_ui_initialize seeds to -1, so it is the profile index, an int.
 *
 * The assert reason string and line number 0xe2 are the reference's own PUSH
 * immediates at 0xe14b6/0xe14b1/0xe14ac. Note the reason string only names
 * local_player_index and profile; the index dword is unasserted.
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
 * set_local_player_controls_from_player_profile's `short local_player_index@<edi>` register argument. The
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
  set_local_player_controls_from_player_profile(local_player_index);
}

/* 0xe1500 -- player_ui_begin_editing_profile (inferred name).
 *
 * MOV [0x46c038],-1 (player_ui_globals + 0x158, the edited-file index) is
 * stored before the saved_game_file_get_type call; the result is MOVZX'd and
 * dispatched 0 / 1 / other.
 *   type 0 (player profile): CALL 0x1c18f0 (player_profile_get) into
 *     player_ui_globals + 0x1c4 (0x46c0a4), then csmemcpy 0x30 bytes to player_ui_globals + 0x15c
 *     (0x46c03c).
 *   type 1 (playlist): CALL 0x1c26f0 (kb playlist_profile_get) into the
 *     same buffer, then the same csmemcpy with 0x68 bytes.  The two arms share the tail at
 *     0xe1575 with only the size PUSH differing.
 *   other: error(2, "invalid profile index (#%08lX)", profile_index).
 * A failed get reports through error(2, ...) and returns without storing the
 * index.  On success the index is stored at 0xe1587. */
void player_ui_begin_editing_profile(int profile_index)
{
  *(int *)(player_ui_globals + 0x158) = -1;
  switch (saved_game_file_get_type(profile_index)) {
  case 0:
    if (player_profile_get(profile_index, player_ui_globals + 0x1c4)) {
      csmemcpy(player_ui_globals + 0x15c, player_ui_globals + 0x1c4, 0x30);
    } else {
      error(2, "failed to retrieve player profile #%08lX for editing",
            profile_index);
      return;
    }
    break;

  case 1:
    if (playlist_profile_get(
          profile_index, (game_variant_t *)(player_ui_globals + 0x1c4))) {
      csmemcpy(player_ui_globals + 0x15c, player_ui_globals + 0x1c4, 0x68);
    } else {
      error(2, "failed to retrieve playlist profile #%08lX for editing",
            profile_index);
      return;
    }
    break;

  default:
    error(2, "invalid profile index (#%08lX)", profile_index);
    return;
  }
  *(int *)(player_ui_globals + 0x158) = profile_index;
}

/* 0xe15b0 -- player_ui_save_profile (inferred name).
 * Writes the live edit copy (player_ui_globals + 0x15c, 0x46c03c) of the
 * saved-game file being edited (player_ui_globals + 0x158, 0x46c038) back to
 * disk, then clears the edit index to -1 on every path (an inlined
 * clear_profile_edit_data).  There is no -1 guard before
 * saved_game_file_get_type.
 *   - type 0: warns when the read-only bit is set and when the profile is not
 *     dirty, then CALL 0x1c1bc0 (kb player_profile_get_from_path; a
 *     profile save) with (index, edit copy); returns true.
 *   - type 1: warns when not dirty.  Read-only (default) playlist: the name
 *     must differ from the original (ustrncmp 0xc chars), else "cannot save
 *     over default profiles"; a renamed default clears bit 0 of the flags byte
 *     (AND byte [0x46c0a0],0xfe), creates a new playlist file with
 *     playlist_profile_new(0, name), writes the edit copy through CALL
 *     0x1c27f0 (kb playlist_profile_save), adopts the new index, and remembers its
 *     directory.  A writable playlist is written in place and its directory
 *     remembered.  Directory lookup failure does not change the true result.
 *   - other: "failed to save profile because we are not editing one".
 * Frame: SUB ESP,0x100 is the directory path buffer. */
bool player_ui_save_profile(void)
{
  char directory_path[0x100];
  bool result;
  int new_profile_index;

  result = false;
  switch (saved_game_file_get_type(*(int *)(player_ui_globals + 0x158))) {
  case 0:
    if (*(int *)(player_ui_globals + 0x158) &
        SAVED_GAME_FILE_INDEX_READ_ONLY_FLAG)
      error(2, "### WARNING: saving over a default player profile");
    if (!player_ui_edit_profile_is_dirty())
      error(2, "### WARNING: saving player profile even though it hasn't been "
               "changed");
    player_profile_get_from_path(*(int *)(player_ui_globals + 0x158),
                                 player_ui_globals + 0x15c);
    result = true;
    break;

  case 1:
    if (!player_ui_edit_profile_is_dirty())
      error(2, "### WARNING: saving player profile even though it hasn't been "
               "changed");
    if (*(int *)(player_ui_globals + 0x158) &
        SAVED_GAME_FILE_INDEX_READ_ONLY_FLAG) {
      if (ustrncmp((wchar_t *)(player_ui_globals + 0x15c),
                   (wchar_t *)(player_ui_globals + 0x1c4), 0xc) != 0) {
        *(unsigned short *)(player_ui_globals + 0x15c + 0x64) &= ~1;
        new_profile_index =
          playlist_profile_new(0, (wchar_t *)(player_ui_globals + 0x15c));
        if (new_profile_index != -1) {
          playlist_profile_save(
            new_profile_index, (game_variant_t *)(player_ui_globals + 0x15c));
          *(int *)(player_ui_globals + 0x158) = new_profile_index;
          if (saved_game_file_get_path_to_enclosing_directory(new_profile_index,
                                                             directory_path))
            saved_game_file_remember_last_used_multiplayer_variant_directory(
              directory_path);
          result = true;
        } else {
          error(2, "failed to save renamed profile to disk");
        }
      } else {
        error(2, "cannot save over default profiles; must rename and save-as a "
                 "new profile");
      }
    } else {
      playlist_profile_save(
        *(int *)(player_ui_globals + 0x158),
        (game_variant_t *)(player_ui_globals + 0x15c));
      if (saved_game_file_get_path_to_enclosing_directory(
            *(int *)(player_ui_globals + 0x158), directory_path))
        saved_game_file_remember_last_used_multiplayer_variant_directory(
          directory_path);
      result = true;
    }
    break;

  default:
    error(2, "failed to save profile because we are not editing one");
    break;
  }

  *(int *)(player_ui_globals + 0x158) = -1;
  return result;
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
 * passed in ESI as hud_message_to_all's register argument.
 *
 * PUSH 0x46bee0 / PUSH ECX / CALL 0x001c1bc0 / ADD ESP,0x8 -- cdecl, last push
 * first, so (profile index, player_ui_globals), the same pair 0xe0fd0 passes.
 *
 * XOR EDI,EDI / CALL 0x000e10c0 is a register argument, not dead code:
 * set_local_player_controls_from_player_profile never writes EDI (its prologue pushes EBP/EBX/ESI only) and
 * reads it at 0xe10d4 as `CMP DI,BX` and at 0xe1199 as `MOVSX ESI,DI` before
 * indexing player_ui_globals + DI*0x38 + 0x8, and pushes it at 0xe12ae. So it
 * takes a signed 16-bit local-player index in EDI, and this call site passes
 * local player 0. kb.json's `(void)` declaration was wrong; widened to
 * `short local_player_index@<edi>`. The PUSH EDI/POP EDI around the call is
 * the caller preserving the register, not an argument push. */
void player0_look_invert_pitch(char invert)
{
  player_ui_globals[0x2b] = invert;
  if (*(int *)(player_ui_globals + 0x30) != -1) {
    hud_message_to_all(L"Saving...");
    player_profile_get_from_path(*(int *)(player_ui_globals + 0x30),
                                 player_ui_globals);
  }
  set_local_player_controls_from_player_profile(0);
}
