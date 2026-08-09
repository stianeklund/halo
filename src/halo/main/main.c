#include "../../common.h"
#include "common.h"

/* x87 FABS as an instruction rather than a call. `fabsf` is a real function in
 * xdk_rt.c, so VC71 emits CALL _fabsf for it; the double `fabs` is a compiler
 * intrinsic and lowers to the single FABS opcode the original uses. Same
 * guarded form as real_math.c. */
#if defined(__clang__)
#define main_fabs_double_from_float(x) __builtin_fabs((double)(x))
#else
extern double __cdecl fabs(double);
#pragma intrinsic(fabs)
#define main_fabs_double_from_float(x) fabs((double)(x))
#endif

/* Zero a local byte range. main_get_solo_level_from_name (0x1006f0) zeroes its
 * 128-byte name buffer with an inline REP STOSD, which is what the MSVC 7.1
 * memset intrinsic emits, so the VC71 lane uses it. clang lowers a 127-byte
 * memset/__builtin_memset to a _memset libcall that does not exist in this
 * freestanding build, so that lane calls the game's own csmemset instead; both
 * zero exactly the same bytes. Same guarded-macro shape as the fabs intrinsic
 * above and as structure_detail_objects.c. */
#if defined(_MSC_VER) && !defined(__clang__)
extern void *__cdecl memset(void *, int, unsigned int);
#pragma intrinsic(memset)
#define main_zero_bytes(p, n) memset((p), 0, (n))
#else
#define main_zero_bytes(p, n) csmemset((p), 0, (n))
#endif

/* Close all UI widgets and display the "damaged media" fatal error screen.
 *
 * Loads the "error_abort_to_dashboard_you_have_no_choice" widget by name,
 * asserts that it is a text box widget (type 1), sets its string_list_index
 * and the global error_string_index to 0x23, marks the widget as needing
 * a text update, then flushes input and enters the halt loop forever.
 * If the widget fails to load, logs an error and enters the halt loop
 * anyway. This function never returns. */
void display_error_damaged_media(void)
{
  void *widget;

  ui_widgets_close_all();
  widget = ui_widget_load_by_name_or_tag(
    "ui\\shell\\error\\error_abort_to_dashboard_you_have_no_choice", -1, 0, -1,
    -1, -1, -1);
  if (widget != NULL) {
    if (*(int16_t *)((char *)widget + 0xe) != 1) {
      display_assert("expected a text box widget",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x90f, 1);
      system_exit(-1);
    }
    *(int16_t *)((char *)widget + 0x40) = 0x23;
    *(uint8_t *)((char *)widget + 0x15) = 1;
    *(int16_t *)0x31e054 = 0x23;
    input_frame_end();
    main_halt_entry();
  }
  error(2, "failed to load '%s' widget",
        "ui\\shell\\error\\error_abort_to_dashboard_you_have_no_choice");
  input_frame_end();
  main_halt_entry();
}

/* ui_widget_display_deferred_errors — flushes the deferred-for-cinematic error
 * queue (4 records at 0x46cc6c, one per local-player slot, 4 bytes each:
 * int16 error_handle @+0, uint8 is_modal @+2, uint8 pause_game @+3). Must run
 * only outside a cinematic; asserts otherwise ("Noooooooooooooooooo!!!",
 * ui_widget.c line 0x93f, system_exit(-1) flavor). For each valid record
 * (0 <= handle < 0x28) it re-issues ui_widget_display_error(handle, slot,
 * is_modal, pause_game), then clears the slot to -1. Ref 0xe8db0. */
void ui_widget_display_deferred_errors(void)
{
  int16_t error_handle;
  int local_player_index;
  int16_t *record;

  if (cinematic_in_progress()) {
    display_assert("Noooooooooooooooooo!!!",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x93f, true);
    system_exit(-1);
  }

  local_player_index = 0;
  record = (int16_t *)0x46cc6c;
  do {
    error_handle = *record;
    if (error_handle >= 0 && error_handle < 0x28) {
      ui_widget_display_error(error_handle, local_player_index, (char)record[1],
                              *(char *)((int)record + 3));
    }
    *record = -1;
    local_player_index = local_player_index + 1;
    record = record + 2;
  } while ((int16_t)local_player_index < 4);
}

/* ui_widget_display_scenario_help — displays the in-game player-help dialog for
 * the scenario that is currently loaded. Copies the scenario tag name into a
 * 256-byte buffer, lowercases it, and matches it against ten level codes
 * ("a10".."d40") to select the matching player_help_screen widget tag. The
 * screen is loaded for the single-player local controller; string_index is then
 * written into the first child widget of type 1 (text box) at +0x40.
 * Asserts: "string_index>=0" (ui_widget.c 0x967) and "expected text box widget
 * in player help screen" (0x986), both system_exit(-1) flavor. Global
 * 0x326a08 is global_scenario_index (NONE when no scenario is loaded).
 * Ref 0xe8e20. */
void ui_widget_display_scenario_help(int16_t string_index)
{
  const char *screen_name;
  void *screen;
  int widget;
  char scenario_name[256];

  if (string_index < 0) {
    display_assert("string_index>=0",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x967, true);
    system_exit(-1);
  }

  if (*(int *)0x326a08 == NONE) {
    error(2, "can't display scenario help because no scenario is loaded");
  } else {
    csstrncpy(scenario_name, tag_get_name(*(int *)0x326a08), 0xff);
    scenario_name[255] = 0;
    csstr_tolower(scenario_name);

    if (crt_strstr(scenario_name, "a10") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_a10";
    } else if (crt_strstr(scenario_name, "a30") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_a30";
    } else if (crt_strstr(scenario_name, "a50") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_a50";
    } else if (crt_strstr(scenario_name, "b30") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_b30";
    } else if (crt_strstr(scenario_name, "b40") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_b40";
    } else if (crt_strstr(scenario_name, "c10") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_c10";
    } else if (crt_strstr(scenario_name, "c20") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_c20";
    } else if (crt_strstr(scenario_name, "c40") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_c40";
    } else if (crt_strstr(scenario_name, "d20") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_d20";
    } else if (crt_strstr(scenario_name, "d40") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_d40";
    } else {
      error(2, "can't display scenario help; unknown scenario is active '%s'",
            scenario_name);
      return;
    }

    screen = ui_widget_load_by_name_or_tag(
      screen_name, NONE, 0,
      (int)player_ui_get_single_player_local_player_controller(0), NONE, NONE,
      NONE);
    if (screen != NULL) {
      widget = *(int *)((int)screen + 0x34);
      while (1) {
        if (widget == 0) {
          display_assert("expected text box widget in player help screen",
                         "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x986,
                         true);
          system_exit(-1);
        }
        if (*(int16_t *)(widget + 0xe) == 1) {
          break;
        }
        widget = *(int *)(widget + 0x2c);
      }
      *(int16_t *)(widget + 0x40) = string_index;
    } else {
      error(2, "failed to load in-game help dialog");
    }
  }
}

/* Guard wrapper: if param_1 is nonzero, change the selected AI encounter
 * by calling ai_debug_change_selected_encounter with direction 0. */
void FUN_000ffe10(char param_1)
{
  if (param_1 != '\0') {
    ai_debug_change_selected_encounter(0);
  }
}

/* Guard wrapper: if param_1 is nonzero, change the selected AI actor
 * by calling ai_debug_change_selected_actor with direction 1 (next). */
void FUN_000ffe30(char param_1)
{
  if (param_1 != '\0') {
    ai_debug_change_selected_actor(1);
  }
}

/* Guard wrapper: if param_1 is nonzero, change the selected AI actor
 * by calling ai_debug_change_selected_actor with direction 0 (prev). */
void FUN_000ffe50(char param_1)
{
  if (param_1 != '\0') {
    ai_debug_change_selected_actor(0);
  }
}

/* Guard wrapper: if param_1 is nonzero, call FUN_00053890. */
void FUN_000ffe70(char param_1)
{
  if (param_1 != '\0') {
    FUN_00053890();
    return;
  }
}

/* Guard wrapper: if param_1 is nonzero, call FUN_0008f630. */
void FUN_000ffe90(char param_1)
{
  if (param_1 != '\0') {
    FUN_0008f630();
    return;
  }
}

/* Set the game connection state (network connection type).
 * Stores the low 16 bits of param into the global word_46DA0C.
 * 0 = local/singleplayer, 2 = client, other values used for host/dedicated. */
void set_game_connection(short param)
{
  word_46DA0C = param;
}

short game_connection(void)
{
  return word_46DA0C;
}

/* Clear the persistent-storage/cache-precache permission flag (0x46da54).
 *
 * Confirmed (0xfff90, whole body is two instructions):
 *     MOV byte ptr [0x0046da54],0x0
 *     RET
 *  - BYTE store, not a dword: Ghidra's `DAT_0046da54 = 0` hides the width, so
 *    the global is a 1-byte flag (writing it as int would clobber 0x46da55,
 *    which is the start of the runtime map_name[255] buffer).
 *  - Plain RET with no immediate and no prologue: cdecl, zero stack args, no
 *    register inputs.
 *
 * Inferred: main_new_map (0xfff10) reads this same byte and only calls the
 * cache-file precache path (0x1bfee0) when it is set, so a 0-write disables
 * that path -- consistent with the "disallow" in the kb.json name. */
void main_disallow_persistent_storage(void)
{
  main_persistent_storage_allowed = 0;
}

/* Queue a map change: copy `name` into the runtime map_name[] buffer
 * (0x46da55, 0x100 bytes: 0x46da55..0x46db54) and, when a game is actually
 * running, arm the deferred map-change flag.
 *
 * Confirmed (0xfffa0):
 *  - Prologue is `PUSH EBP; MOV EBP,ESP` with no `SUB ESP` (no locals), and
 *    the body reads `MOV EAX,[EBP+0x8]` -- one cdecl stack argument, the
 *    name pointer.  Ghidra's `in_stack_00000004` is exactly that slot.  The
 *    old kb.json decl `void main_set_map_name(void)` omitted it.
 *  - `PUSH 0xff; PUSH EAX; PUSH 0x46da55; CALL csstrncpy; ADD ESP,0xc` --
 *    3 cdecl args, so the destination is the map_name buffer and at most
 *    0xff bytes are copied; [0x46db54] is then force-cleared so the name is
 *    always NUL-terminated (same shape as main_set_multiplayer_map_name).
 *  - Four of the five global writes are BYTE stores that Ghidra renders as
 *    bare assignments: [0x46da43] (main_menu_load_pending) -- shown as the
 *    dword field `DAT_0046da40._3_1_`, [0x46db54], [0x46da54]
 *    (main_persistent_storage_allowed) and [0x46da25]
 *    (main_change_map_name_pending).
 *  - The guard is `CALL game_in_editor; TEST AL,AL; JNZ` then
 *    `CALL game_in_progress; TEST AL,AL; JZ ret` -- a short-circuit OR, not
 *    the inverted early-return Ghidra prints.
 *  - `CMP word ptr [0x46da0c],0x0` is a 16-bit read of the game-connection
 *    global, so it must go through `word_46DA0C`, not an int deref.
 *
 * Inferred: the raw-address cast for the buffer (rather than `&map_name`)
 * matches main_get_map_name / main_set_multiplayer_map_name -- the kb.json
 * global is emitted __declspec(dllimport), whose address-of would lower to
 * an indirect __imp_ load instead of the original's immediate. */
void main_set_map_name(const char *name)
{
  main_menu_load_pending = 0;
  csstrncpy((char *)0x46da55, name, 0xff);
  *(char *)0x46db54 = 0;
  main_persistent_storage_allowed = 1;

  if (game_in_editor() || game_in_progress()) {
    if (word_46DA0C == 0) {
      main_change_map_name_pending = 1;
    }
  }
}

void main_defer_map_map_change(void)
{
  main_change_map_name_pending = 0;
}

/* Store the multiplayer map name into the global name buffer at 0x46db55
 * (0x100 bytes: 0x46db55..0x46dc54).  csstrncpy copies at most 0xff bytes and
 * the final byte of the buffer is then force-cleared, so the name is always
 * NUL-terminated.  Finally the cache is given a chance to precache the map.
 *
 * Binary notes (0x100010):
 *   - [0x46dc54] is written with `MOV byte ptr [0x46dc54],0x0` -- a BYTE store,
 *     not a dword (Ghidra's `DAT_0046dc54 = 0` hides the width).
 *   - The single `ADD ESP,0x10` after the second CALL is a merged cleanup for
 *     BOTH calls (3 dwords + 1 dword); the second callee takes one stack arg.
 *   - The bool result of cache_files_give_time_to_precache is discarded. */
void main_set_multiplayer_map_name(const char *name)
{
  csstrncpy((char *)0x46db55, name, 0xff);
  *(char *)0x46dc54 = 0;
  cache_files_give_time_to_precache((const char *)0x46db55);
}

/* Return a pointer to the global map name buffer (0x100040).
 *
 * Body is two instructions: MOV EAX,0x46da55 / RET. 0x46da55 is the
 * runtime-written map_name[255] buffer (zero-filled in the image, NOT an
 * .rdata string literal), so this must be the buffer's address, not a
 * string constant. Same raw-address idiom as main_get_multiplayer_map_name
 * below: the kb.json global `char map_name[255]` is emitted into decl.h as
 * __declspec(dllimport), whose address-of would lower to an indirect
 * __imp_ load rather than the original's immediate. */
const char *main_get_map_name(void)
{
  return (const char *)0x46da55;
}

/* Return a pointer to the global multiplayer map name buffer (0x100050). */
char *main_get_multiplayer_map_name(void)
{
  return (char *)0x46db55;
}

/* Store the difficulty level at 0x31fa90, rejecting out-of-range values
 * (0x100060).
 *
 * The body loads the parameter as a WORD (MOV AX,[EBP+8]) and both bounds
 * checks are signed (JL / JGE), so the parameter is a signed 16-bit value.
 * Out-of-range inputs fall through to the epilogue and leave the global
 * untouched. Bare RET -> __cdecl. */
void main_set_difficulty(int16_t difficulty)
{
  if (difficulty >= 0 && difficulty < 4) {
    *(int16_t *)0x31fa90 = difficulty;
  }
}

/* Return the game variant index from the static table at 0x31fa90. */
int16_t main_get_difficulty(void)
{
  return *(int16_t *)0x31fa90;
}

static const short _game_connection_local = 0;

int __cdecl sort_desired_local_player_controllers(const void *a1,
                                                  const void *a2)
{
  short v1;
  short v2;

  v1 = *(short *)a1;
  v2 = *(short *)a2;
  if (v1 == -1) {
    if (v2 != -1)
      return 1;
  } else if (v2 == -1) {
    return -1;
  }
  if (v2 < v1)
    return 1;
  return (v2 <= v1) - 1;
}

void create_local_players(void)
{
  int i;
  int j;
  int player;
  int16_t gamepad_index;
  int16_t assigned_controllers[4];
  int16_t desired_controllers[4];
  int16_t default_controllers[4];

  if (main_globals.main_menu_scenario_loaded) {
    local_player_set_player_index(0, player_new(0, -1, 0, 0));
    return;
  }

  csmemset(assigned_controllers, -1, sizeof(assigned_controllers));
  csmemset(desired_controllers, -1, sizeof(desired_controllers));
  default_controllers[0] = 0;
  default_controllers[1] = 1;
  default_controllers[2] = 2;
  default_controllers[3] = 3;

  assert_halt(game_connection() == _game_connection_local);

  for (i = 0; i < player_spawn_count; i++) {
    gamepad_index = player_ui_get_single_player_local_player_controller(i);
    desired_controllers[i] = gamepad_index;
    if (gamepad_index == -1) {
      desired_controllers[i] = default_controllers[i];
    }

    assert_halt((desired_controllers[i] >= 0) &&
                (desired_controllers[i] < MAXIMUM_GAMEPADS));

    gamepad_index = desired_controllers[i];
    if (assigned_controllers[gamepad_index] != -1) {
      for (j = 0; j < MAXIMUM_GAMEPADS; j++) {
        if (assigned_controllers[j] == -1) {
          desired_controllers[i] = j;
          assigned_controllers[(int16_t)j] = j;
          break;
        }
      }
      assert_halt(j < MAXIMUM_GAMEPADS);
    } else {
      assigned_controllers[gamepad_index] = gamepad_index;
    }
  }

  qsort(desired_controllers, 4, 2, sort_desired_local_player_controllers);

  for (i = 0; i < player_spawn_count; i++) {
    gamepad_index = desired_controllers[i];
    assert_halt((gamepad_index >= 0) && (gamepad_index < MAXIMUM_GAMEPADS));
    player = player_new(0, -1, gamepad_index, 0);
    local_player_set_player_index(gamepad_index, player);
  }
}

/*
 * main_reset_map - 0x1002a0
 *
 * Confirmed:
 *  - Whole body is 6 instructions, no frame, no CALLs, no FPU:
 *      XOR AL,AL
 *      MOV word ptr [0x0046da40],0xffff
 *      MOV [0x0046da28],AL
 *      MOV byte ptr [0x0046da24],0x1
 *      MOV [0x0046da3b],AL
 *      RET
 *  - 0x46da40 is a WORD store (MOV word ptr, imm16 = 0xffff); the other
 *    three are byte-width stores, so those globals are single-byte flags.
 *  - The single XOR AL,AL feeds both byte-zero stores (0x46da28 and
 *    0x46da3b), which straddle the 0x46da24 = 1 store; the store order is
 *    kept literal here.
 *  - Plain cdecl void(void): RET carries no immediate, and no register is
 *    read before being written, so there are no implicit @<reg> inputs.
 *
 * Inferred:
 *  - Same shape as main_goto_main_menu (reset word_46DA40 to -1, clear
 *    byte_46DA28, arm a pending flag), so this is one of the "request a
 *    main-loop transition" setters — here the map-reset request.
 *
 * Uncertain:
 *  - No string or assert evidence for what byte_46DA3B tracks, so it keeps
 *    its mechanical kb-registered name.
 */
void main_reset_map(void)
{
  word_46DA40 = -1;
  byte_46DA28 = 0;
  game_reset_pending = true;
  byte_46DA3B = 0;
}

/*
 * main_revert_map - 0x1002c0
 *
 * Confirmed:
 *  - Whole body is 6 instructions, no frame, no CALLs, no FPU, no locals:
 *      XOR AL,AL
 *      MOV word ptr [0x0046da40],0xffff
 *      MOV [0x0046da28],AL
 *      MOV byte ptr [0x0046da26],0x1
 *      MOV [0x0046da3b],AL
 *      RET
 *  - 0x46da40 is a WORD store (MOV word ptr, imm16 = 0xffff); the other
 *    three are byte-width stores, so those globals are single-byte flags.
 *    Ghidra's DAT_0046da40._0_2_ agrees on the 16-bit width.
 *  - The single XOR AL,AL feeds both byte-zero stores (0x46da28 and
 *    0x46da3b), which straddle the 0x46da26 = 1 store; the store order is
 *    kept literal here so the shared zero register can be re-materialized.
 *  - Plain cdecl void(void): RET carries no immediate, and no register is
 *    read before being written, so there are no implicit @<reg> inputs.
 *
 * Inferred:
 *  - Byte-for-byte the same shape as main_reset_map (0x1002a0), differing
 *    only in which pending flag is armed: 0x46da24 there, 0x46da26 here.
 *    0x46da26 carries the kb-registered name game_state_revert_pending, so
 *    the request armed here is the game-state revert.
 *  - byte_46DA28 is the same save-attempt-resolved flag cleared by
 *    main_reset_map, main_skip_cinematic, main_save_map_private and
 *    main_new_map.
 *
 * Uncertain:
 *  - No string or assert evidence for what byte_46DA3B tracks, so it keeps
 *    its mechanical kb-registered name.
 */
void main_revert_map(void)
{
  word_46DA40 = -1;
  byte_46DA28 = 0;
  game_state_revert_pending = true;
  byte_46DA3B = 0;
}

/*
 * main_skip_cinematic - 0x1002e0
 *
 * Confirmed:
 *  - Whole body is 4 instructions, no frame, no CALLs, no FPU, no locals:
 *      MOV word ptr [0x0046da40],0xffff
 *      MOV byte ptr [0x0046da28],0x0
 *      MOV byte ptr [0x0046da27],0x1
 *      RET
 *  - Store widths differ and are preserved literally: 0x46da40 is a WORD
 *    store of 0xffff (short = -1); the other two are BYTE-width immediate
 *    stores, so those globals are single-byte flags.
 *  - Unlike main_reset_map, there is no XOR AL,AL feeding the zero store:
 *    each store carries its own immediate, so no shared zero register.
 *  - Plain cdecl void(void): RET carries no immediate, and no register is
 *    read before being written, so there are no implicit @<reg> inputs.
 *
 * Inferred:
 *  - Same shape as main_reset_map / main_goto_main_menu (reset word_46DA40
 *    to -1, clear byte_46DA28, arm one pending flag), so this is another
 *    "request a main-loop transition" setter. 0x46da27 carries the
 *    kb-registered name should_skip_cinematic, so the request armed here is
 *    the cinematic skip.
 *  - byte_46DA28 is the same save-attempt-resolved flag cleared by
 *    main_reset_map, main_save_map_private and main_new_map.
 *
 * Uncertain:
 *  - No string or assert evidence pins what word_46DA40 = -1 means beyond
 *    "the sentinel written by every transition setter in this group", so it
 *    keeps its mechanical kb-registered name.
 */
void main_skip_cinematic(void)
{
  word_46DA40 = -1;
  byte_46DA28 = 0;
  should_skip_cinematic = true;
}

/*
 * main_save_map_nonsafe - 0x100300
 *
 * Confirmed:
 *  - Whole body is 3 instructions, no frame, no CALLs, no FPU, no locals:
 *      MOV byte ptr [0x0046da28],0x1
 *      MOV byte ptr [0x0046da29],0x0
 *      RET
 *  - Both stores are BYTE-width immediates (MOV byte ptr, imm8), so both
 *    globals are single-byte flags; neither is widened here.
 *  - Each store carries its own immediate; there is no shared zero register
 *    (no XOR AL,AL), so the two stores are independent.
 *  - Store order is 0x46da28 then 0x46da29, preserved literally below.
 *  - Plain cdecl void(void): RET carries no immediate, and no register is
 *    read before being written, so there are no implicit @<reg> inputs.
 *
 * Inferred:
 *  - byte_46DA28 is the save-request flag also touched by main_reset_map,
 *    main_skip_cinematic, main_new_map and main_save_map_private; every
 *    other setter in this group CLEARS it, and this one is the only observed
 *    site that SETS it, so this is the arm side of that request.
 *  - 0x46da29 is the save-in-progress / pending flag that
 *    main_save_map_private (0x100eb0) tests: when it is zero the game is not
 *    in a pending safe-save state and main_save_map_private takes the arm
 *    path. Clearing it here therefore arms the request WITHOUT the pending
 *    bit, matching the kb-registered name main_save_map_nonsafe.
 *
 * Uncertain:
 *  - The sibling at 0x100330 (main_save_map_safe, unported) is expected to
 *    be the same two stores with 0x46da29 = 1, but that is not verified here.
 *  - 0x46da29 has no kb-registered name yet, so it keeps its raw address
 *    form, the same idiom main_save_map_private uses.
 */
void main_save_map_nonsafe(void)
{
  byte_46DA28 = 1;
  *(uint8_t *)0x46da29 = 0;
}

/*
 * main_saving_map - 0x100310
 *
 * Confirmed:
 *  - Whole body is 2 instructions / 6 bytes, no frame, no CALLs, no FPU:
 *      00100310  a0 28 da 46 00   MOV AL,byte ptr [0x0046da28]
 *      00100315  c3               RET
 *    (verified byte-for-byte against the pristine cachebeta.xbe, not just
 *    the Ghidra listing).
 *  - The load is an 8-bit MOV AL, NOT a MOVZX/MOVSX, so the upper 24 bits of
 *    EAX are left untouched. The return value is therefore a single byte in
 *    AL, which is why the C prototype must return bool/char and not int --
 *    an int return would make the compiler widen the load and diverge.
 *  - Ghidra decompiles this as an empty `void` body ONLY because the kb.json
 *    decl declared `void`; the return value is real and lives in AL.
 *  - Plain cdecl: RET carries no immediate, and no register is read before
 *    being written, so there are no implicit @<reg> inputs.
 *
 * Inferred:
 *  - byte_46DA28 is the save-request flag written by the surrounding group:
 *    main_save_map_nonsafe (0x100300) SETS it, while main_reset_map,
 *    main_revert_map, main_skip_cinematic, FUN_00100380 and main_new_map all
 *    CLEAR it. This function is the read side of that same flag, which
 *    matches the kb-registered name main_saving_map -- i.e. "is a map save
 *    currently requested/in progress".
 *
 * Uncertain:
 *  - Ghidra reports zero callers. With a 6-byte body and a previously-`void`
 *    decl this is more likely an under-resolved indirect/table reference than
 *    genuinely dead code, so the function is lifted as-is rather than
 *    simplified away on a dead-code assumption.
 */
bool main_saving_map(void)
{
  return byte_46DA28;
}

/*
 * main_save_cancel - 0x100320
 *
 * Confirmed:
 *  - Whole body is 2 instructions, no frame, no CALLs, no FPU, no locals:
 *      00100320  MOV byte ptr [0x0046da28],0x0
 *      00100327  RET
 *  - The store is a BYTE-width immediate (MOV byte ptr, imm8), so 0x46da28
 *    is a single-byte flag here as everywhere else in this group; it is not
 *    widened to int/word.
 *  - The immediate 0 is carried inline by the store. Unlike main_reset_map
 *    (0x1002a0) there is no shared XOR AL,AL feeding a zero register, so the
 *    single store is the entire function body.
 *  - Plain cdecl void(void): RET carries no immediate, and no register is
 *    read before being written, so there are no implicit @<reg> inputs and
 *    no @<reg> callees.
 *
 * Inferred:
 *  - byte_46DA28 is the save-request / save-attempt-resolved flag that
 *    main_save_map_nonsafe (0x100300) SETS and that main_reset_map,
 *    main_skip_cinematic, main_goto_main_menu, FUN_00100380,
 *    main_save_map_private and main_new_map all CLEAR as part of a larger
 *    transition. This function clears that flag and nothing else, which
 *    matches the kb-registered name main_save_cancel: cancel the pending
 *    save request without arming any other transition.
 *
 * Uncertain:
 *  - Ghidra resolves no direct callers. As with main_saving_map (0x100310),
 *    an 8-byte body with no xrefs is more likely an under-resolved
 *    indirect/table reference than dead code, so it is lifted literally.
 */
void main_save_cancel(void)
{
  byte_46DA28 = 0;
}

/*
 * main_save_map_safe - 0x100330
 *
 * Confirmed:
 *  - Whole body is 13 instructions, no frame, no locals, no CALLs, no FPU,
 *    plain cdecl void(void); RET carries no immediate and no register is read
 *    before being written, so there are no implicit @<reg> inputs:
 *      00100330  MOV AL,[0x0046da28]
 *      00100335  XOR ECX,ECX
 *      00100337  CMP AL,CL
 *      00100339  JZ  0x00100343        ; byte_46DA28 == 0 -> arm
 *      0010033b  CMP byte ptr [0x0046da2a],CL
 *      00100341  JZ  0x00100367        ; 0x46da2a == 0 -> skip (RET)
 *      00100343  MOV AL,0x1
 *      00100345  MOV [0x0046da28],AL
 *      0010034a  MOV [0x0046da29],AL
 *      0010034f  MOV [0x0046da2a],AL
 *      00100354  MOV dword ptr [0x0046da2c],ECX
 *      0010035a  MOV dword ptr [0x0046da30],ECX
 *      00100360  MOV word  ptr [0x0046da38],CX
 *      00100367  RET
 *  - STORE WIDTHS ARE MIXED and are taken from the disassembly, not from the
 *    decompiler (which prints every zero store as a bare `0`):
 *    0x46da28/29/2a are BYTE (MOV reg8), 0x46da2c and 0x46da30 are DWORD
 *    (MOV dword ptr), and 0x46da38 is WORD (MOV word ptr, CX). Writing
 *    0x46da38 as a dword would additionally clobber 0x46da3a.
 *  - One `MOV AL,1` feeds all three byte-1 stores and one `XOR ECX,ECX` feeds
 *    all three zero stores, so the store order 28, 29, 2a, 2c, 30, 38 is
 *    preserved literally below.
 *  - Branch sense: the first JZ jumps to the arm block when byte_46DA28 == 0;
 *    the second JZ jumps to the RET when 0x46da2a == 0. The guard is
 *    therefore an OR, taken as written with no inversion.
 *
 * Inferred:
 *  - This is the SAFE counterpart of main_save_map_nonsafe (0x100300). That
 *    one unconditionally sets byte_46DA28 = 1 and clears the pending byte
 *    0x46da29; this one is guarded, SETS the pending byte, and additionally
 *    resets the retry/cooldown/success counters that main_save_map_private
 *    (0x100eb0, already ported) consumes: 0x46da2c cooldown, 0x46da30
 *    total-ticks, 0x46da38 consecutive-success counter. Arming the request
 *    with a clean counter set is what makes the save "safe".
 *  - The guard re-arms when no save is currently requested (byte_46DA28 == 0)
 *    or when the secondary flag 0x46da2a is already set; a request that is
 *    pending without that flag is left untouched.
 *
 * Uncertain:
 *  - 0x46da29, 0x46da2a, 0x46da2c, 0x46da30 and 0x46da38 have no
 *    kb-registered names, so they keep the raw-address idiom already used by
 *    main_save_map_nonsafe and main_save_map_private in this TU.
 *  - The precise meaning of the secondary flag 0x46da2a is not established
 *    here; main_save_map_private only tests it once the retry counter has
 *    exceeded 0xef ticks.
 */
void main_save_map_safe(void)
{
  if (byte_46DA28 == 0 || *(uint8_t *)0x46da2a != 0) {
    byte_46DA28 = 1;
    *(uint8_t *)0x46da29 = 1;
    *(uint8_t *)0x46da2a = 1;
    *(int32_t *)0x46da2c = 0;
    *(int32_t *)0x46da30 = 0;
    *(int16_t *)0x46da38 = 0;
  }
}

/*
 * main_won_map - 0x100370
 *
 * Confirmed:
 *  - Whole body is 3 instructions, no frame, no locals, no CALLs, no FPU:
 *      00100370  MOV byte ptr [0x0046da28],0x0
 *      00100377  MOV byte ptr [0x0046da3a],0x1
 *      0010037e  RET
 *  - Both stores are BYTE-width immediates (MOV byte ptr, imm8), so both
 *    globals are single-byte flags; neither is widened to int/word. This
 *    matches their kb decls (char byte_46DA28, bool
 *    main_won_map_private_pending).
 *  - Store order is 0x46da28 first, then 0x46da3a; preserved literally.
 *  - The two globals are 0x12 bytes apart, so they are distinct main_globals
 *    flags, not two fields of one word.
 *  - Plain cdecl void(void): the RET carries no immediate, and no register is
 *    read before being written anywhere in the body, so there are no implicit
 *    @<reg> inputs and no @<reg> callee contracts.
 *
 * Inferred:
 *  - Setting main_won_map_private_pending arms main_won_map_private
 *    (0x101040) to run on the next main-loop pass: the main loop tests
 *    `if (main_won_map_private_pending) main_won_map_private();`, and
 *    main_won_map_private clears the flag on entry. So this is the public
 *    "request the won-map transition" entry point, deferring the actual work
 *    by one loop iteration.
 *  - Clearing byte_46DA28 cancels any pending save request first, the same
 *    prologue used by main_save_cancel, FUN_00100380 and main_new_map.
 *
 * Uncertain:
 *  - Ghidra resolves no direct callers. A 15-byte body with no xrefs is more
 *    likely an under-resolved indirect/table reference (script/HS command
 *    table) than dead code, so it is lifted literally.
 */
void main_won_map(void)
{
  byte_46DA28 = 0;
  main_won_map_private_pending = 1;
}

/*
 * FUN_00100380 - 0x100380
 *
 * Confirmed:
 *  - Whole body is 3 instructions, no frame, no CALLs, no FPU:
 *      MOV byte ptr [0x0046da28],0x0
 *      MOV byte ptr [0x0046da3b],0x1
 *      RET
 *  - Both stores are BYTE-width immediates (MOV byte ptr, imm8), so both
 *    globals are single-byte flags, not int/word.
 *  - The two globals are 0x13 bytes apart, so they are distinct main_globals
 *    flags, not adjacent fields of one word.
 *  - Plain cdecl void(void): RET carries no immediate, no register is read
 *    before being written, so there are no implicit @<reg> inputs.
 *
 * Inferred:
 *  - Same shape as main_goto_main_menu (clear byte_46DA28, arm a pending
 *    flag), so this is one of the "request a main-loop transition" setters.
 *    byte_46DA28 is the save-attempt-resolved flag also cleared by
 *    main_save_map_private and main_new_map; byte_46DA3B is one of the
 *    main_globals pending flags that main_new_map clears in the same block.
 *
 * Uncertain:
 *  - No string, assert, or caller evidence in the binary for what transition
 *    byte_46DA3B requests, so the function keeps its mechanical FUN_ name and
 *    the globals keep their kb-registered byte_46DAxx names.
 */
void FUN_00100380(void)
{
  byte_46DA28 = 0;
  byte_46DA3B = 1;
}

/*
 * main_respawn - 0x100390
 *
 * Confirmed:
 *  - Whole body is 9 instructions, EBP frame, no locals, no CALLs, no FPU:
 *      00100390  PUSH EBP
 *      00100391  MOV EBP,ESP
 *      00100393  MOV AL,byte ptr [EBP+0x8]
 *      00100396  TEST AL,AL
 *      00100398  MOV byte ptr [0x0046da3c],0x1
 *      0010039f  JZ 0x001003aa
 *      001003a1  MOV word ptr [0x0046da4e],0x5b
 *      001003aa  POP EBP
 *      001003ab  RET
 *  - Takes ONE stack argument at [EBP+0x8], loaded with a plain 1-byte
 *    `MOV AL,byte ptr` — no MOVSX/MOVZX widening — so the parameter is a
 *    single byte (char), not an int. The prior kb decl
 *    `void main_respawn(void);` was wrong and is corrected by this lift;
 *    Ghidra rendered the argument as the synthetic `in_stack_00000004`
 *    only because the empty decl told it there were no parameters.
 *  - Plain RET with no immediate => cdecl, caller cleans the stack.
 *  - No register is read before being written, so there are no implicit
 *    @<reg> inputs and no @<reg> callee contracts.
 *  - Store WIDTHS differ and are preserved literally:
 *      0x46da3c is `MOV byte ptr`, imm8 1        -> char byte_46DA3C
 *      0x46da4e is `MOV word ptr`, imm16 0x5b    -> short word_46DA4E
 *    Declaring either as int would emit a 32-bit store and mismatch.
 *  - The byte store to 0x46da3c is UNCONDITIONAL: MSVC scheduled it into the
 *    gap between the TEST and the JZ, but it is on both paths, so in source
 *    order it precedes the `if`. Only the word store is guarded.
 *  - Branch sense: JZ skips the word store, so the guard is `arg != 0`.
 *
 * Inferred:
 *  - Same "arm a pending main-loop transition flag" shape as main_won_map
 *    (0x100370) and FUN_00100380 (0x100380): byte_46DA3C is one more of the
 *    contiguous main_globals pending-request bytes (0x46da3a, 0x46da3b,
 *    0x46da3c are consecutive), set here and presumably cleared by the
 *    corresponding main-loop handler on entry.
 *  - The parameter selects a variant of the respawn request rather than
 *    supplying data: it only gates whether word_46DA4E is overwritten.
 *
 * Uncertain:
 *  - 0x5b (91) is written as a bare 16-bit immediate with no accompanying
 *    string, assert, or table reference in the binary, so there is no
 *    evidence for what it enumerates. It is kept as a magic literal rather
 *    than given a speculative name.
 *  - word_46DA4E is 0x12 bytes away from byte_46DA3C, so the two are
 *    distinct main_globals slots, not fields of one record; but nothing in
 *    this function reveals what word_46DA4E means.
 *  - The parameter keeps a mechanical name: the binary shows only that it is
 *    a byte tested against zero, not what it selects.
 */
void main_respawn(char reset_flag)
{
  byte_46DA3C = 1;
  if (reset_flag != 0) {
    word_46DA4E = 0x5b;
  }
}

/*
 * main_save_core - 0x1003b0
 *
 * Confirmed:
 *  - Whole body is 6 instructions, no frame, no locals, no _chkstk, no FPU:
 *      001003b0  PUSH 0x28b198                  ; -> "core.bin"
 *      001003b5  PUSH 0x46dd55                  ; -> core_name
 *      001003ba  MOV byte ptr [0x0046da3d],0x1
 *      001003c1  CALL 0x0008dff0                ; csstrcpy
 *      001003c6  ADD ESP,0x8
 *      001003c9  RET
 *  - Plain RET with no immediate and no [EBP+N] reads => void(void), cdecl.
 *  - `ADD ESP,0x8` after the CALL confirms two stack args and matches the
 *    kb decl `char *csstrcpy(char *destination, const char *source)`.
 *    cdecl push order: the FIRST push (0x28b198, the string) is the LAST
 *    argument (source); the SECOND push (0x46dd55) is the destination.
 *  - The flag store is `MOV byte ptr ... ,1`, an 8-bit store.
 *    game_state_save_core_pending is declared `bool` (unsigned char), so the
 *    store stays one byte; widening it to int/short would emit a dword/word
 *    store and mismatch.
 *  - MSVC scheduled the flag store between the argument pushes and the CALL,
 *    but it is unconditional and independent of the call, so source order is
 *    flag-then-call.
 *  - No register is read before being written => no implicit @<reg> inputs
 *    and no @<reg> callee contract.
 *  - csstrcpy's return value (EAX) is not consumed by anything after the
 *    CALL, so it is discarded here, matching the void return.
 *
 * Inferred:
 *  - Arms a deferred core-dump request: the flag at 0x46da3d is the sibling
 *    of game_state_load_core_pending (0x46da3e) in the same contiguous run of
 *    main_globals pending-request bytes, and main_reset_map's cleanup block
 *    clears 0x46da3d alongside the other pending flags. The actual dump is
 *    therefore performed later by the main-loop handler that consumes the
 *    flag, not here.
 *  - core_name (0x46dd55) is the filename buffer that handler reads.
 *
 * Uncertain:
 *  - The size of the core_name buffer is not observable from this function
 *    (csstrcpy is unbounded), so it stays an incomplete `char[]` as declared
 *    in kb.json rather than being given an invented bound.
 */
void main_save_core(void)
{
  game_state_save_core_pending = 1;
  csstrcpy(core_name, "core.bin");
}

/*
 * main_save_core_name - 0x1003d0
 *
 * Arms a deferred core dump using a caller-supplied file name, warning (but
 * not halting) when the name will not fit in the core_name buffer.
 *
 * Confirmed:
 *  - 23 instructions, no locals, no _chkstk, no FPU, no SEH:
 *      001003d0  PUSH EBP / MOV EBP,ESP / PUSH ESI
 *      001003d4  MOV ESI,dword ptr [EBP+0x8]      ; the single stack argument
 *      001003d7  PUSH ESI / CALL 0x8df60 / ADD ESP,0x4          ; csstrlen
 *      001003e0  CMP EAX,0x40 / JC 0x1003fe
 *      001003e5  PUSH 0x0 / PUSH 0x3a5 / PUSH 0x28b0b4 / PUSH 0x28b1a4
 *      001003f6  CALL 0x8d9f0 / ADD ESP,0x10                    ;
 * display_assert 001003fe  PUSH 0x3f / PUSH ESI / PUSH 0x46dd55 00100406  CALL
 * 0x8de70 / ADD ESP,0xc                     ; csstrncpy 0010040e  MOV byte ptr
 * [0x0046da3d],0x1 00100415  POP ESI / POP EBP / RET
 *  - One stack parameter, cdecl: ESI is written from [EBP+8] before any read
 *    of it, and the terminator is a plain RET with no immediate. This is NOT
 *    the `void(void)` that kb.json previously declared. The sole original
 *    call site confirms the single argument: 0x000c27ac loads EDX from a
 *    returned pointer, 0x000c27ae pushes it, 0x000c27af calls 0x1003d0, and
 *    that argument's cleanup is folded into the ADD ESP,0xc at 0x000c27bc
 *    which also covers the two arguments of the following call.
 *  - ESI is callee-saved (PUSH/POP) and carries the argument across all three
 *    calls, so there is no implicit register input and no @<reg> callee
 *    contract; every callee is pure stack cdecl (ADD ESP,0x4 / 0x10 / 0xc).
 *  - `CMP EAX,0x40; JC` is an unsigned-below test, so the length is compared
 *    as unsigned and the warning fires for length >= 0x40. A signed compare
 *    would have emitted JL.
 *  - Warning flavor of assert: halt is 0 (PUSH 0x0) and no system_exit tail
 *    follows the call, so execution falls through into the truncating copy.
 *    Reason "warning, core file name will be truncated to 63 characters",
 *    file "c:\halo\SOURCE\main\main.c", line 0x3a5.
 *  - The copy bound is 0x3f, i.e. 63 characters plus the NUL csstrncpy adds.
 *  - The flag store is an 8-bit MOV byte ptr [0x46da3d],1, matching the
 *    `bool` declaration of game_state_save_core_pending; widening it would
 *    emit a dword store.
 *
 * Inferred:
 *  - MSVC scheduled the flag store after the csstrncpy call here, whereas in
 *    the sibling main_save_core (0x1003b0) it scheduled the same store before
 *    the copy call. The store is unconditional and independent of the copy in
 *    both, so this is instruction scheduling rather than a semantic
 *    difference; source order here follows the emitted order.
 *  - csstrncpy's return value (EAX) is not consumed, matching the void return.
 *
 * Uncertain:
 *  - core_name stays an incomplete `char[]` as declared in kb.json; the 0x3f
 *    bound proves at least 64 bytes are addressable, not the true size.
 */
void main_save_core_name(const char *name)
{
  if ((unsigned int)csstrlen(name) >= 0x40) {
    display_assert("warning, core file name will be truncated to 63 characters",
                   "c:\\halo\\SOURCE\\main\\main.c", 0x3a5, 0);
  }
  csstrncpy(core_name, name, 0x3f);
  game_state_save_core_pending = 1;
}

/*
 * main_load_core - 0x100420
 *
 * Arms a deferred core-image load from the default file name "core.bin". The
 * exact load counterpart of main_save_core (0x1003b0): same string, same
 * destination buffer, only the pending flag differs.
 *
 * Confirmed:
 *  - Whole body is 6 instructions, no frame (no PUSH EBP), no locals, no
 *    _chkstk, no FPU, no SEH:
 *      00100420  PUSH 0x28b198                  ; -> "core.bin"
 *      00100425  PUSH 0x46dd55                  ; -> core_name
 *      0010042a  MOV byte ptr [0x0046da3e],0x1
 *      00100431  CALL 0x0008dff0                ; csstrcpy
 *      00100436  ADD ESP,0x8
 *      00100439  RET
 *  - Plain RET with no immediate, and no [EBP+N] read anywhere, so this is
 *    void(void), cdecl. Nothing is read before being written, so there are no
 *    implicit @<reg> inputs and no @<reg> callee contract.
 *  - `ADD ESP,0x8` after the CALL confirms two stack args, matching the kb
 *    decl `char *csstrcpy(char *destination, const char *source)`. cdecl push
 *    order: the FIRST push (0x28b198, the string) is the LAST argument
 *    (source); the SECOND push (0x46dd55) is the destination. So this is
 *    csstrcpy(dst=core_name, src="core.bin"), not the reverse.
 *  - The flag store is `MOV byte ptr ...,1`, an 8-bit store.
 *    game_state_load_core_pending is declared `bool` (unsigned char), so the
 *    store stays one byte; widening it to short/int would emit a word/dword
 *    store and mismatch.
 *  - MSVC scheduled the flag store between the two argument pushes and the
 *    CALL. The store is unconditional and independent of the call in both
 *    directions, so source order is flag-then-call, exactly as in the sibling
 *    main_save_core.
 *  - csstrcpy's return value (EAX) is not consumed after the CALL, so it is
 *    discarded, matching the void return.
 *
 * Inferred:
 *  - The flag at 0x46da3e is the load sibling of game_state_save_core_pending
 *    (0x46da3d) in the same contiguous run of main_globals pending-request
 *    bytes; main_reset_map's cleanup clears both. The actual load is performed
 *    later by the main-loop handler that consumes the flag, not here.
 *
 * Uncertain:
 *  - The size of the core_name buffer is not observable from this function
 *    (csstrcpy is unbounded), so it stays an incomplete `char[]` as declared
 *    in kb.json rather than being given an invented bound.
 */
void main_load_core(void)
{
  game_state_load_core_pending = 1;
  csstrcpy(core_name, "core.bin");
}

/*
 * main_load_core_at_startup - 0x100440
 *
 * Arms a core-image load from the default file name "core.bin" to be honoured
 * at the next map reset rather than immediately. The deferred-until-startup
 * counterpart of main_load_core (0x100420): same string, same destination
 * buffer, only the pending flag byte differs.
 *
 * Confirmed:
 *  - Whole body is 6 instructions, no frame (no PUSH EBP), no locals, no
 *    _chkstk, no FPU, no SEH:
 *      00100440  PUSH 0x28b198                  ; -> "core.bin"
 *      00100445  PUSH 0x46dd55                  ; -> core_name
 *      0010044a  MOV byte ptr [0x0046da3f],0x1
 *      00100451  CALL 0x0008dff0                ; csstrcpy
 *      00100456  ADD ESP,0x8
 *      00100459  RET
 *  - Plain RET with no immediate and no [EBP+N] read anywhere, so this is
 *    void(void), cdecl, matching the kb decl. No register is read before
 *    being written, so there are no implicit @<reg> inputs and csstrcpy
 *    carries no @<reg> contract (has_reg_args=false).
 *  - `ADD ESP,0x8` after the CALL confirms two stack args, matching the kb
 *    decl `char *csstrcpy(char *destination, const char *source)`. cdecl push
 *    order: the FIRST push (0x28b198, the string) is the LAST argument
 *    (source); the SECOND push (0x46dd55) is the destination. So this is
 *    csstrcpy(dst=core_name, src="core.bin"), not the reverse.
 *  - The flag store is `MOV byte ptr ...,1`, an 8-bit store, so the flag is
 *    declared `bool` (unsigned char) and the store stays one byte; widening
 *    it to short/int would emit a word/dword store and mismatch.
 *  - The flag byte is 0x46da3f, distinct from game_state_load_core_pending
 *    (0x46da3e) and from game_state_save_core_pending (0x46da3d).
 *  - MSVC scheduled the flag store between the two argument pushes and the
 *    CALL. The store is unconditional and independent of the call, so source
 *    order is flag-then-call, exactly as in the siblings main_save_core and
 *    main_load_core.
 *  - csstrcpy's return value (EAX) is not consumed after the CALL, so it is
 *    discarded, matching the void return.
 *
 * Inferred:
 *  - 0x46da3f is the "load core once, at startup" request: main_reset_map
 *    reads it into a temporary, clears it along with the other pending-request
 *    bytes, then assigns that temporary to game_state_load_core_pending. So
 *    setting it here promotes into the ordinary load-core request at the next
 *    reset and then self-clears, which is why a separate byte exists at all.
 *  - core_name (0x46dd55) is the filename buffer that handler reads.
 *
 * Uncertain:
 *  - The size of the core_name buffer is not observable from this function
 *    (csstrcpy is unbounded), so it stays an incomplete `char[]` as declared
 *    in kb.json rather than being given an invented bound.
 */
void main_load_core_at_startup(void)
{
  game_state_load_core_at_startup_pending = 1;
  csstrcpy(core_name, "core.bin");
}

/*
 * main_load_core_name - 0x100460
 *
 * Arms a deferred core-image load from a caller-supplied file name, warning
 * (but not halting) when the name will not fit in the core_name buffer. The
 * exact load counterpart of main_save_core_name (0x1003d0): same length test,
 * same warning text, same destination buffer and bound; only the pending flag
 * byte and the assert line number differ.
 *
 * Confirmed:
 *  - 24 instructions, no locals, no _chkstk, no FPU, no SEH, no struct access:
 *      00100460  PUSH EBP / MOV EBP,ESP / PUSH ESI
 *      00100464  MOV ESI,dword ptr [EBP+0x8]      ; the single stack argument
 *      00100467  PUSH ESI / CALL 0x8df60 / ADD ESP,0x4          ; csstrlen
 *      00100470  CMP EAX,0x40 / JC 0x10048e
 *      00100475  PUSH 0x0 / PUSH 0x3c9 / PUSH 0x28b0b4 / PUSH 0x28b1a4
 *      00100486  CALL 0x8d9f0 / ADD ESP,0x10                    ;
 * display_assert 0010048e  PUSH 0x3f / PUSH ESI / PUSH 0x46dd55 00100496  CALL
 * 0x8de70 / ADD ESP,0xc                     ; csstrncpy 0010049e  MOV byte ptr
 * [0x0046da3e],0x1 001004a5  POP ESI / POP EBP / RET
 *  - One stack parameter, cdecl: ESI is loaded from [EBP+8] before any read of
 *    it, and the terminator is a plain RET with no immediate. This is NOT the
 *    `void(void)` that kb.json previously declared; the decl is corrected as
 *    part of this lift.
 *  - ESI is callee-saved (PUSH/POP) and carries the argument across both
 *    calls, so there is no implicit register input and no @<reg> callee
 *    contract; every callee is pure stack cdecl and the cleanups (ADD ESP,0x4
 *    / 0x10 / 0xc) match the kb decls' stack-arg counts exactly.
 *  - `CMP EAX,0x40; JC` is an unsigned-below test, so the length is compared
 *    as unsigned and the warning fires for length >= 0x40. A signed compare
 *    would have emitted JL.
 *  - Warning flavor of assert: halt is 0 (PUSH 0x0) and no system_exit tail
 *    follows the call, so execution falls through into the truncating copy.
 *    cdecl push order makes the FIRST push (0x0) the LAST argument (halt) and
 *    the LAST push (0x28b1a4) the FIRST (reason). Reason "warning, core file
 *    name will be truncated to 63 characters", file
 *    "c:\halo\SOURCE\main\main.c", line 0x3c9.
 *  - The copy bound is 0x3f, i.e. 63 characters plus the NUL csstrncpy adds.
 *  - The flag store is an 8-bit MOV byte ptr [0x46da3e],1, matching the `bool`
 *    declaration of game_state_load_core_pending; widening it to short/int
 *    would emit a word/dword store and mismatch.
 *  - csstrncpy's return value (EAX) is not consumed after the CALL, so it is
 *    discarded, matching the void return.
 *
 * Inferred:
 *  - MSVC scheduled the flag store after the csstrncpy call, as it did in the
 *    sibling main_save_core_name (0x1003d0) and unlike main_load_core
 *    (0x100420) where the same store precedes the copy call. The store is
 *    unconditional and independent of the copy, so source order follows the
 *    emitted order here.
 *  - 0x46da3e is the deferred-load request byte that main_reset_map's cleanup
 *    clears; the actual load is performed later by the main-loop handler that
 *    consumes the flag, not here.
 *
 * Uncertain:
 *  - core_name stays an incomplete `char[]` as declared in kb.json; the 0x3f
 *    bound and the "63 characters" warning prove at least 64 bytes are
 *    addressable, not the true size.
 */
void main_load_core_name(const char *name)
{
  if ((unsigned int)csstrlen(name) >= 0x40) {
    display_assert("warning, core file name will be truncated to 63 characters",
                   "c:\\halo\\SOURCE\\main\\main.c", 0x3c9, 0);
  }
  csstrncpy(core_name, name, 0x3f);
  game_state_load_core_pending = 1;
}

/*
 * main_load_core_name_at_startup - 0x1004b0
 *
 * Arms a deferred core-image load from a caller-supplied file name, to be
 * performed at engine startup rather than at the next loop iteration. Byte for
 * byte the same shape as main_load_core_name (0x100460); only the assert line
 * number (0x3d7 vs 0x3c9) and the pending flag byte (0x46da3f vs 0x46da3e)
 * differ.
 *
 * Confirmed:
 *  - 72 bytes, 0x1004b0-0x1004f7, no locals, no _chkstk, no FPU, no SEH, no
 *    struct access:
 *      001004b0  PUSH EBP / MOV EBP,ESP / PUSH ESI
 *      001004b4  MOV ESI,dword ptr [EBP+0x8]      ; the single stack argument
 *      001004b7  PUSH ESI / CALL 0x8df60 / ADD ESP,0x4          ; csstrlen
 *              CMP EAX,0x40 / JC                                ; unsigned
 *              PUSH 0x0 / PUSH 0x3d7 / PUSH 0x28b0b4 / PUSH 0x28b1a4
 *              CALL 0x8d9f0 / ADD ESP,0x10                    ; display_assert
 *              PUSH 0x3f / PUSH ESI / PUSH 0x46dd55
 *      001004e6  CALL 0x8de70 / ADD ESP,0xc                     ; csstrncpy
 *      001004ee  MOV byte ptr [0x0046da3f],0x1
 *              POP ESI / POP EBP / RET
 *  - One stack parameter, cdecl: ESI is loaded from [EBP+8] before any read of
 *    it and the terminator is a plain RET with no immediate. This is NOT the
 *    `void(void)` that kb.json previously declared; the decl is corrected as
 *    part of this lift. Ghidra's synthetic `in_stack_00000004` was the tell.
 *  - ESI is callee-saved (PUSH/POP) and carries the argument across both
 *    calls, so there is no implicit register input and no @<reg> callee
 *    contract; every callee is pure stack cdecl and the cleanups (ADD ESP,0x4
 *    / 0x10 / 0xc) match the kb decls' stack-arg counts exactly.
 *  - `CMP EAX,0x40; JC` is an unsigned-below test, so the length is compared
 *    as unsigned and the warning fires for length >= 0x40. A signed compare
 *    would have emitted JL.
 *  - Warning flavor of assert: halt is 0 (PUSH 0x0) and no system_exit tail
 *    follows the call, so execution falls through into the truncating copy.
 *    cdecl push order makes the FIRST push (0x0) the LAST argument (halt) and
 *    the LAST push (0x28b1a4) the FIRST (reason). Line literal is 0x3d7 (983),
 *    distinct from the 0x3c9 of main_load_core_name.
 *  - csstrncpy argument order: the destination 0x46dd55 (core_name) is the LAST
 *    push, the source is ESI (the argument), bound 0x3f.
 *  - The flag store is an 8-bit MOV byte ptr [0x46da3f],1, so
 *    game_state_load_core_at_startup_pending must stay a 1-byte type; widening
 *    it would emit a word/dword store and mismatch.
 *  - csstrncpy's return value (EAX) is not consumed after the CALL, so it is
 *    discarded, matching the void return.
 *
 * Inferred:
 *  - MSVC emitted the copy before the flag store here (0x1004e6 then
 *    0x1004ee), the reverse of main_load_core_at_startup (0x100440) where the
 *    same flag store precedes the copy. Both stores are unconditional and
 *    independent of the copy, so source order follows the emitted order.
 *  - 0x46da3f is the startup-deferred-load request byte that the main-loop
 *    consumer copies to 0x46da3e and then clears; the load itself happens
 *    there, not here.
 *
 * Uncertain:
 *  - core_name stays an incomplete `char[]` as declared in kb.json; the 0x3f
 *    bound and the "63 characters" warning prove at least 64 bytes are
 *    addressable, not the true size.
 */
void main_load_core_name_at_startup(const char *name)
{
  if ((unsigned int)csstrlen(name) >= 0x40) {
    display_assert("warning, core file name will be truncated to 63 characters",
                   "c:\\halo\\SOURCE\\main\\main.c", 0x3d7, 0);
  }
  csstrncpy(core_name, name, 0x3f);
  game_state_load_core_at_startup_pending = 1;
}

/*
 * main_switch_structure_bsp - 0x100500
 *
 * Requests a switch to a different structure BSP of the currently loaded
 * scenario. Validates the requested index against the scenario's
 * structure-BSP count, rejects a switch to the BSP that is already current,
 * then stashes the pending index in the main globals and reloads the HUD.
 *
 * Confirmed (disassembly; PUSH EBP / MOV EBP,ESP, no locals, no _chkstk,
 * no SEH, no FPU):
 *  - Single stack parameter at [EBP+0x8], read as `MOV CX,word ptr [EBP+8]`
 *    -> 16-bit signed. Widened with MOVSX for the 32-bit count compare and
 *    for the console_warning varargs slot. No register arguments.
 *    check_arg_counts.py: 1 site (0xbdded), push=1, ADD ESP,4, conclusive.
 *  - CALL 0x18e380 global_scenario_get() -> EAX, dereferenced without a NULL
 *    check (unlike main_menu_precache_resources at 0x100640).
 *  - TEST CX,CX / JL -> invalid path (negative index rejected).
 *  - MOV ESI,[EAX+0x5a4]; MOVSX EDX,CX; CMP EDX,ESI; JGE -> invalid path.
 *    ESI is push/pop scoped around the compare only (0x100511/0x10051d), so
 *    it holds nothing that outlives the bounds check.
 *  - CMP CX,word ptr [0x326a0c] -> 16-bit compare against the current
 *    structure-BSP index (same global read as *(short *)0x326a0c in
 *    src/halo/ai/actors.c:3571 and src/halo/ai/encounters.c:3188).
 *    Equal -> PUSH EDX (already sign-extended) / PUSH 0x28b20c /
 *    CALL console_warning / ADD ESP,8.
 *  - Switch path: PUSH 1; MOV word ptr [0x46da40],CX; CALL 0xd0d50 hud_load;
 *    ADD ESP,4. The constant is pushed before the store but it is hud_load's
 *    only argument, so the call is hud_load(true).
 *  - Invalid path (0x10054c): MOVSX EAX,CX; PUSH EAX; PUSH 0x28b1e0;
 *    CALL console_warning; ADD ESP,8. cdecl push order: the format string is
 *    pushed last, so it is the first argument.
 *
 * Inferred:
 *  - Scenario offset 0x5a4 is the structure_bsps tag-block element count: it
 *    is a 32-bit value used solely as the exclusive upper bound of a BSP
 *    index. scenario_t in types.h is still capped at 0xF0 with a FIXME, so
 *    this stays a raw offset deref rather than a struct field.
 *  - word_46DA40 is the *pending* BSP index consumed elsewhere;
 *    main_goto_main_menu (0x100620) writes -1 to the same 16-bit global.
 *
 * Uncertain:
 *  - Whether hud_load's argument is semantically "reload" or some other
 *    boolean; only the literal 1 is observed here (0x1003747 passes 0).
 */
void main_switch_structure_bsp(short bsp_index)
{
  scenario_t *scenario;

  scenario = global_scenario_get();
  if (bsp_index >= 0) {
    /* 0x5a4 = scenario structure_bsps block element count (32-bit). */
    if ((int)bsp_index < *(int *)((char *)scenario + 0x5a4)) {
      if (bsp_index == global_structure_bsp_index) {
        console_warning("tried to switch to current structure-bsp %d",
                        (int)bsp_index);
        return;
      }
      word_46DA40 = bsp_index;
      hud_load(1);
      return;
    }
  }
  console_warning("tried to switch to invalid structure-bsp %d",
                  (int)bsp_index);
}

void main_queue_map_name(char *map_name)
{
  if (map_name != 0) {
    csstrncpy(&byte_46DC55, map_name, 0xff);
    byte_46DA50 = 1;
  } else {
    byte_46DC55 = 0;
    byte_46DA50 = 0;
  }
}

/*
 * main_precache_map_tick - 0x1005d0
 *
 * Advances the map precaching state machine. Called periodically to poll
 * precache progress. If a precache is in progress and its status is 1
 * (complete), ends the precache. Then, if no precache is active, begins
 * precaching the queued map name and clears the queue flag.
 */
void main_precache_map_tick(void)
{
  float status;
  if (cache_files_precache_in_progress()) {
    if (cache_files_precache_map_status(&status) == 1) {
      cache_files_precache_map_end();
    }
  }
  if (!cache_files_precache_in_progress()) {
    cache_files_precache_map_begin(&byte_46DC55, false);
    byte_46DA50 = 0;
  }
}

void main_goto_main_menu(void)
{
  word_46DA40 = -1;
  byte_46DA28 = 0;
  main_menu_load_pending = 1;
}

void main_menu_precache_resources(void)
{
  scenario_t *scenario = global_scenario_get();
  if (scenario) {
    assert_halt(scenario->type == _scenario_type_main_menu);
    predicted_resources_precache(&scenario->unk_236);
  }
}

/*
 * main_menu_unload - 0x100690
 *
 * Tears down the main-menu scenario: stops the attract-mode/menu music,
 * clears the UI's "main menu active" byte (0x46cc88, written by
 * main_menu_active), and clears main_globals.main_menu_scenario_loaded
 * (byte at 0x46da42).
 *
 * Confirmed (disassembly, 6 instructions, no frame, no locals):
 *   CALL 0xe4640              -> ui_widget_stop_attract_mode()
 *   PUSH 0 / CALL 0xe43d0 / ADD ESP,4 -> main_menu_active(false)
 *   MOV byte ptr [0x46da42],0 -> main_globals.main_menu_scenario_loaded = 0
 *
 * Note: the byte store is 8-bit at +2 of the main_globals block; 0x46da40
 * itself is a separate word field, so this must not be widened.
 *
 * This is the unload counterpart to main_menu_load (0x101fe0); the same
 * three-step teardown appears inline in main_change_map_name (0x100c60).
 */
void main_menu_unload(void)
{
  ui_widget_stop_attract_mode();
  main_menu_active(false);
  main_globals.main_menu_scenario_loaded = 0;
}

/*
 * main_reset_player_actions - 0x1006b0
 *
 * Resets the player action queue state by deleting all pending updates,
 * re-initializing the queue, and restarting the server update pipeline.
 * Called when closing a UI widget (ui_widget_close) and at the end of
 * each network client frame (network_game_client_end_frame).
 */
void main_reset_player_actions(void)
{
  update_server_delete();
  update_server_new();
  update_server_start();
}

/*
 * main_change_map_name_in_progress - 0x1006c0
 *
 * Returns true while the
 * map-change deadline tick at 0x46da34 is non-zero.
 */
bool main_change_map_name_in_progress(void)
{
  return *(uint32_t *)0x46da34 != 0;
}

/*
 * main_menu_switch_to_single_player - 0x1006d0
 *
 * Confirmed:
 *  - Whole body is 2 instructions, no frame, no _chkstk, no locals, no
 *    CALLs, no FPU, no struct access:
 *      MOV byte ptr [0x0046da25],0x1
 *      RET
 *  - The store is BYTE width with immediate 1, so 0x46da25 is a single-byte
 *    flag; it carries the kb-registered name main_change_map_name_pending.
 *  - Plain cdecl void(void): RET carries no immediate, and no register is
 *    read before being written, so there are no implicit @<reg> inputs and
 *    no @<reg> callee contracts to honor.
 *
 * Inferred:
 *  - Same family as main_reset_map / main_revert_map / main_skip_cinematic:
 *    a "request a main-loop transition" setter that arms exactly one pending
 *    flag. Unlike those, it does NOT reset word_46DA40 or clear
 *    byte_46DA28 — only the one flag is armed.
 *  - The armed flag is consumed by the main game loop, which calls
 *    main_change_map_name() while it is set (see the pending-flag block in
 *    main_loop_body) and clears it inside main_change_map_name.
 *
 * Uncertain:
 *  - The kb global name (main_change_map_name_pending) does not obviously
 *    match the function name (switch_to_single_player). Both names come from
 *    separate evidence and neither is renamed here; the connection is
 *    presumably that returning to single player is performed as a map-name
 *    change, but there is no string or assert evidence pinning that.
 */
void main_menu_switch_to_single_player(void)
{
  main_change_map_name_pending = true;
}

/*
 * main_set_game_connection_to_film_playback - 0x1006e0
 *
 * Confirmed:
 *  - Whole body is 2 instructions, no frame, no _chkstk, no locals, no
 *    CALLs, no FPU, no struct access:
 *      001006e0  MOV byte ptr [0x0046da45],0x1
 *      001006e7  RET
 *  - The store is BYTE width with immediate 1, so 0x46da45 is a single-byte
 *    flag; it carries the kb-registered name byte_46DA45.
 *  - Plain cdecl void(void): RET carries no immediate, and no register is
 *    read before being written, so there are no implicit @<reg> inputs and
 *    no @<reg> callee contracts to honor.
 *  - The same byte is read (not written) by main_setup_connection (0x100e10),
 *    where a set flag forces the "error opening saved film" path and drops
 *    back to the main menu. That is binary evidence tying this byte to film
 *    playback, matching the kb-registered function name.
 *
 * Inferred:
 *  - Same shape as main_menu_switch_to_single_player (0x1006d0): a one-byte
 *    "arm a mode for the next connection setup" setter. Nothing else in the
 *    body clears or resets neighbouring bytes.
 *
 * Uncertain:
 *  - The neighbouring bytes 0x46da44 (xbox_demos_launch_pending) and
 *    0x46da46 (byte_46DA46) may belong to the same small state block, but
 *    nothing here proves that, so they are left as separate globals.
 */
void main_set_game_connection_to_film_playback(void)
{
  byte_46DA45 = 1;
}

/*
 * main_get_solo_level_from_name - 0x1006f0
 *
 * Maps a campaign map name (e.g. "levels\\b30\\b30") to its 0-based solo
 * level index, or -1 when the name matches none of the ten campaign levels.
 *
 * Confirmed:
 *  - Prologue is PUSH EBP; MOV EBP,ESP; SUB ESP,0x80; PUSH EDI, so there is
 *    exactly one 128-byte local at EBP-0x80 and no _chkstk. EDI is saved only
 *    because of the inlined REP STOS below.
 *  - The buffer is zeroed as MOV byte [EBP-0x80],0 followed by
 *    XOR EAX,EAX; MOV ECX,0x1f; LEA EDI,[EBP-0x7f]; REP STOSD; STOSW; STOSB
 *    (a separate first byte plus 127 zero bytes) - the exact MSVC lowering of
 *    a `char buf[128] = ""` aggregate initializer, so that is the source form.
 *  - Prologue reads MOV EAX,dword ptr [EBP+0x8]: one cdecl stack parameter,
 *    the map name. No register is read before being written, so there are no
 *    implicit @<reg> inputs.
 *  - 0x100717 pushes 0x7f, then EAX (the parameter), then LEA ECX,[EBP-0x80]:
 *    csstrncpy(buf, name, 0x7f). MOV byte [EBP-1],0 then forces buf[127] = 0
 *    AFTER the copy and BEFORE the fold, so the ordering here is the original
 *    ordering. Only 0x7f bytes are ever copied - not sizeof(buf).
 *  - 0x100724 calls csstr_tolower(buf) with a single argument. The first three
 *    calls share one ADD ESP,0x18 at 0x100737 (3 + 1 + 2 dwords of arguments);
 *    that merged cleanup is not evidence of a six-argument call.
 *  - Each test is PUSH <needle>; PUSH buf; CALL 0x1d9690; ADD ESP,8, i.e.
 *    crt_strstr(haystack, needle) with the buffer first.
 *  - Every exit sets EAX before MOV ESP,EBP; POP EBP; RET, so the return type
 *    is a 32-bit integer even though Ghidra reports void (lift-learnings §16).
 *    Matching blocks fall through to XOR EAX,EAX / MOV EAX,imm.
 *  - The last test ends NEG EAX; SBB EAX,EAX; AND EAX,0xa; DEC EAX, which is
 *    the MSVC lowering of `matched ? 9 : -1`; VC71 regenerates the idiom from
 *    the ternary, so it is written as a plain conditional here.
 *  - The needle strings live at 0x284870..0x284a8f in DESCENDING test order:
 *    0x284a8c "a10", 0x284a50 "a30", 0x284a14 "a50", 0x2849d8 "b30",
 *    0x28499c "b40", 0x284960 "c10", 0x284924 "c20", 0x2848e8 "c40",
 *    0x2848ac "d20", 0x284870 "d40". The test order below is therefore
 *    a10..d40 mapping to indices 0..9.
 *  - No FPU operations, no struct access.
 *
 * Inferred:
 *  - The result is a campaign level ordinal: the three call sites in this file
 *    use it to index saved-profile level records and to pick the next level,
 *    and 10 is the campaign level count.
 *
 * Uncertain:
 *  - Each needle is a 4-byte level name immediately followed by a
 *    "ui\\shell\\solo_game\\player_help\\player_help_screen_<lvl>" path, which
 *    looks like an adjacent table of {level name, help-screen path} pairs. This
 *    function only ever reads the name field, so the table's shape (and whether
 *    the original indexed it rather than testing inline) is left unknown; the
 *    straight-line CALL sequence in the binary is transcribed literally.
 */
long main_get_solo_level_from_name(const char *name)
{
  char buf[128];

  /* the `char buf[128] = ""` initializer, written out: buf[0] stored on its
   * own, then 127 zero bytes via the inline REP STOSD at 0x1006fd */
  buf[0] = 0;
  main_zero_bytes(buf + 1, 0x7f);

  csstrncpy(buf, name, 0x7f);
  buf[127] = 0;
  csstr_tolower(buf);

  if (crt_strstr(buf, "a10") != NULL) {
    return 0;
  }
  if (crt_strstr(buf, "a30") != NULL) {
    return 1;
  }
  if (crt_strstr(buf, "a50") != NULL) {
    return 2;
  }
  if (crt_strstr(buf, "b30") != NULL) {
    return 3;
  }
  if (crt_strstr(buf, "b40") != NULL) {
    return 4;
  }
  if (crt_strstr(buf, "c10") != NULL) {
    return 5;
  }
  if (crt_strstr(buf, "c20") != NULL) {
    return 6;
  }
  if (crt_strstr(buf, "c40") != NULL) {
    return 7;
  }
  if (crt_strstr(buf, "d20") != NULL) {
    return 8;
  }
  return crt_strstr(buf, "d40") != NULL ? 9 : -1;
}

/*
 * main_get_current_solo_level - 0x100860
 *
 * Returns the solo (campaign) level index of the currently loaded map, or -1
 * when the map name is not one of the ten campaign levels.
 *
 * Confirmed:
 *  - The whole body is four instructions with no frame at all (no PUSH EBP,
 *    no _chkstk, no locals):
 *      PUSH 0x46da55 / CALL 0x1006f0 / ADD ESP,0x4 / RET
 *    The ADD ESP,0x4 proves cdecl with exactly one stack argument, and the
 *    bare RET proves this function itself takes none.
 *  - Nothing is read before being written, so there is no implicit @<reg>
 *    input contract; the callee has none either.
 *  - Execution falls straight out of the CALL with the callee's EAX still
 *    live and no XOR EAX,EAX after it: this forwards
 *    main_get_solo_level_from_name's `long` result. The kb.json decl
 *    previously said `void`, which is the void-EAX implicit-return hazard
 *    (lift-learnings section 16) -- a `void` body would only appear to work by
 *    accident of the return register, so the decl is corrected to `long`.
 *
 * Inferred:
 *  - 0x46da55 is passed as a direct PUSH imm32, so the original did not route
 *    through the main_get_map_name accessor (that would add a CALL). The raw
 *    cast is used for the same reason documented at main_get_map_name above:
 *    the kb.json global `char map_name[255]` is emitted __declspec(dllimport),
 *    and &map_name would lower to an indirect __imp_ load instead of an
 *    immediate. */
long main_get_current_solo_level(void)
{
  return main_get_solo_level_from_name((const char *)0x46da55);
}

/* The ten campaign level name/path strings, indexed by solo level index.
 * Element [0] is "levels\\a10\\a10", already dereferenced as *(char **)0x31fa9c
 * by main_load_last_solo_map below.
 *
 * Addressed through a raw cast rather than a kb.json global because a kb.json
 * global is emitted __declspec(dllimport), and an indexed load through it
 * would lower to an indirect __imp_ load instead of the original's direct
 * MOV EAX,[EAX*4 + 0x31fa9c] (same reason as main_get_map_name above). */
#define SOLO_LEVEL_NAMES ((const char **)0x31fa9c)
/* Unsigned: the original's upper bound is CMP CX,0xA / JNC, an unsigned
 * above-or-equal, so the count's type makes the comparison unsigned (the
 * Bungie NUMBEROF/sizeof idiom yields size_t). A signed 10 lowers to JGE. */
#define NUMBER_OF_SOLO_LEVELS 10u

/*
 * main_get_solo_level_name - 0x100870
 *
 * Returns the campaign map name/path for a solo level index, or NULL when the
 * index falls outside 0..9. Inverse of main_get_solo_level_from_name above.
 *
 * Confirmed:
 *  - Frame is PUSH EBP; MOV EBP,ESP with no local space and no saved
 *    registers, and RET carries no immediate: one cdecl stack argument,
 *    caller-cleaned, no implicit @<reg> inputs.
 *  - The argument is read as MOV CX,word ptr [EBP+8] -- a 16-bit load with no
 *    widening at all -- so the parameter is 16 bits wide, not int.
 *  - XOR EAX,EAX at 0x100877 runs BEFORE both bounds tests, and both failing
 *    tests jump to the shared POP EBP; RET at 0x10088e. That is a single
 *    result local pre-set to NULL with the success store inside one if, not a
 *    pair of early returns.
 *  - The bounds pair is TEST CX,CX / JL (signed, catches negative) followed by
 *    CMP CX,0xA / JNC (unsigned, catches >= 10): the MSVC lowering of
 *    0 <= index && index < 10 once non-negativity has been established.
 *  - MOVSX EAX,CX sign-extends the index before MOV EAX,[EAX*4 + 0x31fa9c],
 *    so the table is ten 4-byte pointers based at 0x31fa9c.
 *
 * Inferred:
 *  - The element type is a string pointer rather than some other 4-byte datum:
 *    main_get_solo_level_from_name maps exactly ten level-name strings back to
 *    indices 0..9, and 0x31fa9c[0] is consumed as a map path.
 *
 * Note: Ghidra decompiles this as `void FUN_00100870(void) { return; }`,
 * missing both the stack parameter and the implicit EAX return
 * (lift-learnings section 16). The disassembly is the only usable evidence.
 */
const char *main_get_solo_level_name(int16_t level_index)
{
  const char *name = NULL;

  if (level_index >= 0 && level_index < NUMBER_OF_SOLO_LEVELS) {
    name = SOLO_LEVEL_NAMES[level_index];
  }

  return name;
}

/*
 * main_run_demos - 0x100890
 *
 * Confirmed:
 *  - Whole body is 2 instructions, no frame, no _chkstk, no locals, no
 *    CALLs, no FPU, no struct access:
 *      00100890  MOV byte ptr [0x0046da44],0x1
 *      00100897  RET
 *  - The store is BYTE width with immediate 1 (delinked reference shows
 *    `movb $0x1, DAT_0046da44`), so 0x46da44 is a single-byte flag; it
 *    carries the kb-registered name xbox_demos_launch_pending.
 *  - Plain cdecl void(void): RET carries no immediate, and no register is
 *    read before being written, so there are no implicit @<reg> inputs and
 *    no @<reg> callee contracts to honor.
 *  - The same byte is read and cleared by the main loop's pending-flag
 *    dispatch (main_loop, 0x1010c0 region): a set flag clears itself and
 *    then calls xbox_demos_launch. So this function only arms the launch;
 *    it deliberately does not clear the flag or call anything.
 *
 * Inferred:
 *  - Same shape as main_set_game_connection_to_film_playback (0x1006e0) and
 *    main_menu_switch_to_single_player (0x1006d0): a one-byte "arm this mode
 *    for the next main-loop iteration" setter.
 *
 * Uncertain:
 *  - The neighbouring bytes 0x46da45 (byte_46DA45, film playback) and
 *    0x46da46 (byte_46DA46) may belong to the same small state block, but
 *    nothing here proves that, so they are left as separate globals. The
 *    store must stay BYTE width for that reason.
 */
void main_run_demos(void)
{
  xbox_demos_launch_pending = 1;
}

/*
 * compute_split_screen_grid - 0x1008a0
 *
 * Finds the smallest grid (horizontal x vertical) whose cell count is at
 * least num_players: the horizontal count grows while it trails the vertical
 * count, otherwise it resets to 1 and the vertical count grows. The result is
 * therefore always horizontal <= vertical.
 *
 * num_players arrives in EBX; the two counts are returned through the stack
 * pointer arguments at [EBP+8] and [EBP+0xC].
 *
 * Confirmed:
 *  - EBX is read before it is written anywhere in the body (TEST EBX,EBX at
 *    0x1008a3 is its first use) and is never modified, so it is a register
 *    argument, not a local.
 *  - Assert reason "num_players>0", file "c:\\halo\\SOURCE\\main\\main.c",
 *    line 0x51c; the tail is PUSH -0x1 / CALL 0x8e2f0 = system_exit(-1),
 *    i.e. the assert_halt flavor (Ghidra renders the tail as
 *    thunk_FUN_001029a0, which is misleading).
 *  - The loop product is computed as vertical * horizontal
 *    (MOV EAX,EDI then IMUL EAX,ESI), and the loop is bottom-tested.
 *  - Frame is PUSH EBP / MOV EBP,ESP / PUSH ESI / PUSH EDI with no local
 *    space: the horizontal count lives in ESI, the vertical count in EDI.
 *  - RET carries no immediate, so the stack arguments are cdecl.
 *
 * Inferred:
 *  - The name is behavioral only. No PDB entry or string names the function
 *    itself; the assert string only proves the register argument is a player
 *    count.
 *
 * Note: the original binary also contains this logic inlined into
 * compute_window_bounds (0x100910), so that copy is deliberately left in
 * place rather than replaced by a call here.
 */
void compute_split_screen_grid(int num_players, int *out_horizontal_count,
                               int *out_vertical_count)
{
  int h = 1;
  int v = 1;

  assert_halt(num_players > 0);

  if (num_players > 1) {
    do {
      if (h < v) {
        h++;
      } else {
        h = 1;
        v++;
      }
    } while (v * h < num_players);
  }

  *out_horizontal_count = h;
  *out_vertical_count = v;
}

/*
 * compute_window_bounds - 0x100910
 *
 * Computes viewport split bounds for a given player in a multi-player split
 * screen layout. Divides the screen area (from globals at 0x32565c/0x325660
 * for x and 0x32565e/0x325662 for y) into a grid, and assigns a sub-rectangle
 * to the player at player_index. Also computes a "full" bounds (a3) that
 * extends to the screen edges for border players.
 *
 * Calls an inlined grid dimension helper (originally at 0x1008a0) that takes
 * EBX as a register arg: finds the smallest (horiz, vert) grid such that
 * horiz * vert >= num_players.
 *
 * When num_players > horizontal_count * vertical_count, player 0 is given a
 * double-wide column (bVar5 flag set), and all other indices are shifted by 1.
 *
 * The "gap" value is 4 pixels when num_players >= 2, else 0.
 *
 * Confirmed:
 *  - Assert strings match: "player_index<num_players",
 *    "vertical_index>=0 && vertical_index<vertical_count",
 *    "horizontal_index>=0 && horizontal_index<horizontal_count".
 *  - Source path: "c:\\halo\\SOURCE\\main\\main.c" with lines 0x54f, 0x56e,
 * 0x56f.
 *  - All screen bounds are int16_t accessed via MOVSX.
 *  - Gap padding applied to inner edges; outer edges replaced by full viewport.
 */
void compute_window_bounds(int player_index, int num_players,
                           viewport_bounds_t *full_bounds,
                           viewport_bounds_t *split_bounds)
{
  int horizontal_count;
  int vertical_count;
  int total_slots;
  int vertical_index;
  int horizontal_index;
  int cell_height;
  int cell_width;
  int wide_cell_width;
  bool has_extra_wide_slot;
  uint16_t gap;

  /* --- assert: player_index < num_players --- */
  assert_halt(player_index < num_players);

  /* gap between sub-windows when more than 1 player */
  gap = (num_players < 2) ? 0 : 4;
  has_extra_wide_slot = false;

  /* --- inlined grid dimension helper (originally at 0x1008a0) ---
   * Finds the smallest (h, v) such that h * v >= num_players, with h <= v.
   * EBX = num_players (register arg in original). */
  {
    int h = 1, v = 1;
    assert_halt(num_players > 0);
    if (num_players > 1) {
      while (v * h < num_players) {
        if (h < v)
          h++;
        else {
          h = 1;
          v++;
        }
      }
    }
    horizontal_count = h;
    vertical_count = v;
  }

  total_slots = horizontal_count * vertical_count;

  /* When grid has spare slots, player 0 gets a double-wide column.
   * Other players shift by +1 so they skip player 0's extra slot. */
  if (total_slots - num_players != 0 && num_players <= total_slots) {
    if (player_index == 0) {
      has_extra_wide_slot = true;
    } else {
      player_index = player_index + 1;
    }
  }

  vertical_index = player_index / horizontal_count;
  horizontal_index = player_index - vertical_index * horizontal_count;

  assert_halt(vertical_index >= 0 && vertical_index < vertical_count);
  assert_halt(horizontal_index >= 0 && horizontal_index < horizontal_count);

  /* screen area globals (not in kb.json) */
  {
    int16_t scr_y0 = *(int16_t *)0x32565c;
    int16_t scr_y1 = *(int16_t *)0x325660;
    int16_t scr_x0 = *(int16_t *)0x32565e;
    int16_t scr_x1 = *(int16_t *)0x325662;

    cell_height = (int16_t)((scr_y1 - scr_y0) / vertical_count);
    cell_width = (int16_t)((scr_x1 - scr_x0) / horizontal_count);
    wide_cell_width = cell_width * (has_extra_wide_slot ? 2 : 1);

    /* compute per-player split bounds */
    split_bounds->x0 = (int16_t)(horizontal_index * wide_cell_width + scr_x0);
    split_bounds->x1 =
      (int16_t)((horizontal_index + 1) * wide_cell_width + scr_x0);
    split_bounds->y0 = (int16_t)(vertical_index * cell_height + scr_y0);
    split_bounds->y1 = (int16_t)((vertical_index + 1) * cell_height + scr_y0);

    /* copy to full bounds before gap adjustments */
    *full_bounds = *split_bounds;

    /* apply gap padding to inner edges of split bounds */
    split_bounds->x0 = split_bounds->x0 + (int16_t)(horizontal_index * gap);
    split_bounds->x1 =
      split_bounds->x1 - (int16_t)((horizontal_index == 0) * gap);
    split_bounds->y0 = split_bounds->y0 + (int16_t)(vertical_index * gap);
    split_bounds->y1 =
      split_bounds->y1 - (int16_t)((vertical_index == 0) * gap);

    /* replace outer edges of full bounds with full viewport bounds */
    if (horizontal_index == 0) {
      full_bounds->x0 = *(int16_t *)0x325656;
    }
    if ((has_extra_wide_slot ? 1 : 0) + horizontal_index + 1 ==
        horizontal_count) {
      full_bounds->x1 = *(int16_t *)0x32565a;
    }
    if (vertical_index == 0) {
      full_bounds->y0 = *(int16_t *)0x325654;
    }
    if (vertical_index + 1 == vertical_count) {
      full_bounds->y1 = *(int16_t *)0x325658;
    }
  }
}

/*
 * main_new_map - 0x100b40
 *
 * Loads a new map from the given game_options. Flushes input, attempts a
 * game_load, initializes the map if successful, creates local players and
 * starts game time, then fires the initial game pulse.
 *
 * Confirmed:
 *  - Calls input_flush (0xcf500), game_load (0xa76b0),
 * game_initialize_for_new_map (0xa7780), error_occurred (0x8f600),
 * create_local_players (0x1000d0), game_time_start (0xb5f40),
 * game_initial_pulse (0xa73c0).
 *  - On game_load failure: error(0, "game_load() failed.").
 *  - On error_occurred: error(0, "main_new_map() failed.").
 *  - Clears many main_globals flags after game_initial_pulse.
 *  - Copies 0x46da3f to 0x46da3e before clearing 0x46da3f.
 *  - Sets word at 0x46da40 to 0xffff (-1).
 *  - If byte at 0x46da54 is set, calls 0x1bfee0 (cache file precache).
 *  - Calls ui_widgets_disable_pause_game(0x1e) at exit.
 */
void main_new_map(game_options_t *game_options)
{
  input_flush();
  if (game_load(game_options)) {
    game_initialize_for_new_map();
  } else {
    error(0, "game_load() failed.");
  }

  if (error_occurred()) {
    error(0, "main_new_map() failed.");
  } else {
    create_local_players();
    game_time_start();
  }

  game_initial_pulse();

  /* Copy game_state_load_core_pending (0x46da3f) to 0x46da3e, then clear
   * many main_globals flags. Order matches original disassembly. */
  {
    uint8_t saved = *(uint8_t *)0x46da3f;
    uint8_t da54 = *(uint8_t *)0x46da54;

    *(uint8_t *)0x46da24 = 0;
    main_change_map_name_pending = 0;
    *(uint8_t *)0x46da26 = 0;
    *(uint8_t *)0x46da27 = 0;
    byte_46DA28 = 0;
    main_won_map_private_pending = 0;
    byte_46DA3B = 0;
    byte_46DA3C = 0;
    *(uint8_t *)0x46da3d = 0;
    *(uint8_t *)0x46da3f = 0;
    word_46DA40 = -1;
    game_state_load_core_pending = saved;

    if (da54 != 0) {
      cache_files_precache();
    }
  }

  ui_widgets_disable_pause_game(0x1e);
}

/*
 * main_change_map_name - 0x100c10
 *
 * Called from the main game loop when main_change_map_name_pending (0x46da25)
 * is set. Fades out the main menu music and UI over 1000 ms, then starts a
 * new game with the map name queued in the global map_name[255] buffer
 * (0x46da55).
 *
 * Confirmed:
 *  - Pending guard: main_globals.main_menu_scenario_loaded (0x46da42) == 1.
 *    If not pending, the function clears the timer deadline (0x46da34) and
 *    falls through to the timer-expired path.
 *  - Timer deadline stored as raw uint32 milliseconds at 0x46da34 (not in
 *    kb.json; used only by this function). Compared against
 *    unk_time_globals.unk_0 (uint32 ms ticker at 0x46d9e0).
 *  - Timer not-yet-started path (deadline == 0):
 *      - FUN_e46a0 returns DAT_0046cc86 (main-menu music-active flag).
 *      - If music is playing (== 1):
 *          deadline = current_ms + 1000
 *          FUN_e5a40(1000) — begin music fade-out over 1000 ms
 *          FUN_e3e10(1)    — enable UI widget
 *          FUN_e3c90(0.0f) — set rasterizer fade to 0 (transparent)
 *        MSVC interleaves: PUSH 0x3e8 (e5a40 arg), then PUSH 0x1 (e3e10 arg),
 *        then PUSH 0x0 (e3c90 arg), cleaned with a single ADD ESP,0xc.
 *      - Early-return if deadline not yet reached (current_ms < deadline).
 *  - Timer-running path (deadline != 0, deadline not yet reached):
 *      delta = deadline - current_ms (int32; add 4294967296.0f if negative to
 *      handle uint32 wrap).
 *      fade = 1.0f - (delta * 0.001f) [constants at 0x2533c8 and 0x255ef8].
 *      FUN_e3c90(fade) — update rasterizer blend.
 *  - Timer-expired (or not-pending) path:
 *      FUN_e3c90(-1.0f)     — set fade to -1.0f (0xbf800000)
 *      FUN_e4640()          — stop main-menu music
 *      FUN_e43d0(0)         — clear UI widget flag2
 *      main_globals.main_menu_scenario_loaded = 0  [cleared mid push-sequence]
 *      FUN_e3e10(0)         — disable UI widget
 *        MSVC interleaves: PUSH 0xbf800000 (e3c90), PUSH 0x0 (e43d0), PUSH 0x0
 *        (e3e10), cleaned with ADD ESP,0xc.
 *      If game_in_progress() and word_46DA0C == 0:
 *        - build game_options_t on the stack (0x10c bytes at [EBP-0x110]):
 *            game_options_new(&game_options)
 *            csstrncpy(game_options.map_name, map_name, 0xff)
 *            game_options.map_name[255] = 0   (explicit null-term)
 *            game_options.difficulty = global_difficulty_level
 *        - game_dispose_from_old_map()
 *        - game_precache_new_map(game_options.map_name, 1)
 *        - game_unload()
 *        - main_new_map(&game_options)
 *        - loop i=0 .. player_spawn_count-1: FUN_1c1c00(i) (save player
 * profile) Loop counter compared as signed int16 against player_spawn_count.
 *  - Deadline cleared to 0 unconditionally at function exit (0x46da34 = 0).
 *  - Float constants:
 *      0x4f800000 = 4294967296.0f (2^32, uint32 wrap correction)
 *      0x3a83126f = ~0.001f       (1/1000 ms-to-fraction scale, at 0x255ef8)
 *      0x3f800000 = 1.0f          (at 0x2533c8)
 *      0xbf800000 = -1.0f
 *
 * Inferred:
 *  - FUN_e46a0 = "main menu music is playing" — reads DAT_0046cc86.
 *  - FUN_e5a40 = begin UI fade / music fade-out (takes fade duration ms).
 *  - FUN_e3c90 = rasterizer_set_fade (takes float; stores raw bits to
 * DAT_0046cc4c).
 *  - FUN_e3e10 = ui_widget_set_flag (bool enable).
 *  - FUN_e4640 = stop_main_menu_music.
 *  - FUN_e43d0 = ui_widget_set_flag2 (bool).
 *  - FUN_1c1c00 = player_profile_save_level (local_player_index).
 *
 * Uncertain:
 *  - Exact semantics of FUN_e5a40, e3c90, e3e10, e43d0 — names are inferred
 *    from callee bodies and context; not confirmed by source strings.
 *  - Whether the note "// FIXME: Merge adjacent globals" on main_globals_t
 *    means 0x46da42 has dual use here vs. in main_menu_load.
 */
void main_change_map_name(void)
{
  game_options_t game_options;
  int delta;
  int i;

  if (main_globals.main_menu_scenario_loaded) {
    if (*(int *)0x46da34 == 0) {
      /* music not yet fading: check if music is still playing */
      if (ui_widget_get_attract_mode_flag()) {
        /* set deadline and kick off the 1000 ms fade sequence */
        *(uint32_t *)0x46da34 = (uint32_t)unk_time_globals.unk_0 + 1000;
        /* MSVC interleaved pre-push: PUSH 0x3e8, PUSH 0x1, PUSH 0x0 */
        main_screen_shell_begin_fade(1000);
        ui_widget_set_events_suppressed(1);
        ui_widgets_set_fade_value(0.0f);
      }
    } else {
      /* compute remaining time and update the rasterizer blend */
      delta = (int)(*(uint32_t *)0x46da34 - (uint32_t)unk_time_globals.unk_0);
      {
        float flt_delta = (float)delta;
        if (delta < 0) {
          flt_delta = flt_delta + 4294967296.0f; /* uint32 wrap correction */
        }
        /* fade = 1.0f - remaining_ms * (1/1000) */
        ui_widgets_set_fade_value(1.0f - flt_delta * *(float *)0x255ef8);
      }
    }
    /* bail out if deadline has not been reached */
    if ((uint32_t)unk_time_globals.unk_0 < *(uint32_t *)0x46da34) {
      return;
    }
  } else {
    /* not pending: clear deadline and fall through to clean-up path */
    *(int *)0x46da34 = 0;
  }

  /* timer expired (or was never pending): finalize fade and start new map */
  /* MSVC interleaved pre-push: PUSH 0xbf800000, PUSH 0x0, PUSH 0x0 */
  ui_widgets_set_fade_value(-1.0f); /* 0xbf800000 */
  ui_widget_stop_attract_mode();
  main_menu_active(false);
  main_globals.main_menu_scenario_loaded = 0;
  ui_widget_set_events_suppressed(0);

  if (game_in_progress() && word_46DA0C == 0) {
    /* initialize game_options from queued map name and difficulty */
    game_options_new(&game_options);
    csstrncpy(game_options.map_name, map_name, 0xff);
    game_options.map_name[255] = 0; /* explicit null-terminator */
    game_options.difficulty = global_difficulty_level;

    game_dispose_from_old_map();
    game_precache_new_map(game_options.map_name, 1);
    game_unload();
    main_new_map(&game_options);

    /* save level progress for each local player (signed int16 compare) */
    for (i = 0; (int16_t)i < player_spawn_count; i++) {
      FUN_001c1c00(i);
    }
  }

  *(int *)0x46da34 = 0;
}

/*
 * main_skip_private - 0x100de0
 *
 * Confirmed:
 *  - Reads/clears two adjacent globals:
 *      0x46da4a = int16_t skip counter (how many ticks to fast-forward)
 *      0x46da49 = bool main_skip_private_pending (cleared on both paths)
 *  - Guard: counter must be > 0 AND cinematic_in_progress() must be true.
 *    If not, emits error(2, "manual skipping doesn't work outside of
 *    cinemtatic start/stop...") [sic — original typo preserved in binary].
 *  - On success: saves current game speed via game_time_get_speed(), sets
 *    speed to 1.0f, then loops calling game_time_update(1/30.0f) once per
 *    tick while the counter is > 0 (counter decremented inside loop).
 *  - After the loop, decrements the counter once more (goes to -1), restores
 *    the saved speed via game_time_set_speed(saved_speed), then zeroes both
 *    globals.
 *  - Float constant 0x3d088889 = ~0.0333f (1/30 sec, one game tick at 30 Hz).
 *  - Float constant 0x3f800000 = 1.0f.
 *  - FSTP/PUSH round-trip: saved speed is stored as float (4 bytes) on the
 *    stack [EBP-4] and reloaded into EAX before being pushed as a raw dword.
 */
void main_skip_private(void)
{
  float saved_speed;
  int16_t *skip_count = (int16_t *)0x46da4a;

  if (*skip_count > 0 && cinematic_in_progress()) {
    saved_speed = game_time_get_speed();
    game_time_set_speed(1.0f);
    while (*skip_count > 0) {
      (*skip_count)--;
      game_time_update(0.03333333507180214f);
    }
    (*skip_count)--;
    game_time_set_speed(saved_speed);
    *skip_count = 0;
    main_skip_private_pending = 0;
    return;
  }
  error(2, "manual skipping doesn't work outside of cinemtatic start/stop...");
  *skip_count = 0;
  main_skip_private_pending = 0;
}

/*
 * main_save_map_private - 0x100eb0
 *
 * Confirmed:
 *  - Returns immediately if game_time_get_paused() (CALL 0xb5c30; JNZ out).
 *  - 0x46da29 (byte): save-in-progress flag. When zero the game is NOT
 *    actively trying to save; when non-zero a save attempt is underway.
 *  - 0x46da2a (byte): secondary flag checked only when the retry counter
 *    (0x46da30) has exceeded 0xef ticks.
 *  - 0x46da2c (dword): cooldown counter. Decremented each tick; save is only
 *    attempted when it falls to <= 0. Reset to 10 after each attempt.
 *  - 0x46da30 (dword): total-ticks counter. Incremented on every call while
 *    the save-pending flag (0x46da29) is set. Used to detect a hung save.
 *  - 0x46da38 (int16_t): consecutive-success counter for game_safe_to_save().
 *    Cleared to 0 on failure, incremented on success. When the original value
 *    reaches >= 3 (i.e. three or more consecutive successes), the save is
 *    triggered (BL set; hud_autosave + game_state_save_pending armed).
 *  - 0x46da28 (byte): cleared to 0 whenever the save attempt is resolved
 *    (success, abort, or overflow). Already in kb.json as byte_46DA28.
 *  - 0x46da2b = game_state_save_pending: set to 1 to arm the save, then the
 *    main loop (0x100eb0 caller) handles the actual game_state_save call.
 *  - CALL 0xd0db0 = hud_autosave(int16_t): notifies HUD; arg is 1.
 *  - CALL 0xff4d0 = local logging helper (int, const char *, ...): same cast
 *    pattern used in game_state.c and cheats.c.
 *  - debug_game_save (0x46e002): when set, enables the "unsafe save" path
 *    (triggers autosave even when 0x46da29==0) and logs the "gave up" message
 *    on overflow instead of silently aborting.
 *  - Overflow path (0x46da30 >= 0xf0 AND 0x46da2a != 0): clears byte_46DA28
 *    and returns without triggering a save regardless of debug_game_save.
 *    With debug_game_save set, logs "gave up trying to save" first.
 *
 * Inferred:
 *  - 0x46da29 is probably "main_save_map_private_pending" or similar — the
 *    name is not confirmed from strings.
 *  - 0x46da2a is probably a secondary "abort on overflow" sub-flag.
 *  - 0x46da2c is a retry-cooldown tick counter; 10-tick interval inferred.
 *  - 0x46da30 is a total-attempt tick counter; 0xf0 = 240 ticks ceiling.
 *  - 0x46da38 is a run-of-good-frames counter gating the actual save trigger.
 *
 * Uncertain:
 *  - Exact semantic names for 0x46da29, 0x46da2a, 0x46da2c, 0x46da30,
 *    0x46da38 — all accessed as hardcoded addresses since not in kb.json.
 */
void main_save_map_private(void)
{
  int orig_ticks;
  int orig_cooldown;
  int16_t orig_safe_count;
  bool trigger;

  if (game_time_get_paused()) {
    return;
  }

  trigger = false;

  if (*(uint8_t *)0x46da29 == 0) {
    /* Not in a pending-save state: only fire if debug_game_save forces it. */
    if (debug_game_save) {
      ((void (*)(int, const char *, ...))0xff4d0)(
        0, "unsafe save"); /* hazard-ok: fnptr-conv */
    }
    /* Fall through to shared trigger tail. */
  } else {
    /* Increment total-ticks counter and check for overflow. */
    orig_ticks = *(int *)0x46da30;
    *(int *)0x46da30 = orig_ticks + 1;

    if (orig_ticks >= 0xf0 && *(uint8_t *)0x46da2a != 0) {
      /* Hung for too long — abort the save. */
      if (debug_game_save) {
        ((void (*)(int, const char *, ...))0xff4d0)(
          0, /* hazard-ok: fnptr-conv */
          "gave up trying to save");
      }
      byte_46DA28 = 0;
      return;
    }

    /* Decrement cooldown counter; only attempt save when it reaches <= 0. */
    orig_cooldown = *(int *)0x46da2c;
    *(int *)0x46da2c = orig_cooldown - 1;
    if (orig_cooldown > 0) {
      return;
    }

    /* Poll game_safe_to_save(); track consecutive successes. */
    if (game_safe_to_save()) {
      orig_safe_count = *(int16_t *)0x46da38;
      *(int16_t *)0x46da38 = orig_safe_count + 1;
      if (orig_safe_count >= 3) {
        trigger = true;
      }
    } else {
      *(int16_t *)0x46da38 = 0;
    }

    /* Reset cooldown regardless of whether the save fires. */
    *(int *)0x46da2c = 10;

    if (!trigger) {
      return;
    }
  }

  /* Shared trigger tail: arm save and clear pending flag. */
  hud_autosave(1);
  game_state_save_pending = 1;
  byte_46DA28 = 0;
}

/*
 * main_won_map_private - 0x101040
 *
 * Confirmed:
 *  - Sets main_menu_load_pending (0x46da43) = 1.
 *  - Clears main_won_map_private_pending (0x46da3a) = 0.
 *  - Calls 0x1006f0(&map_name) → returns a short level index (0-8) for
 *    recognized map names, or -1 for unrecognized. Source path visible in
 *    the callee: "c:\\halo\\SOURCE\\saved games\\player_profile.c". The
 *    callee strips the path prefix (FUN_8de70 = csstrncpy-like), lowercases
 *    (FUN_8d9a0), then does up to 10 strstr comparisons for the 10 SP
 *    level names. Returns 0-8 for a known level; the 10th path returns 9
 *    but the expression `(-(uint)(pcVar1 != 0) & 10) - 1` maps it to 9.
 *  - Zero-extends the 16-bit result (XOR EDI,EDI; MOV DI,AX), then
 *    increments: level_index = (uint16_t)(result) + 1. If (short)level_index
 *    >= 10, level_index is set to -1 (OR EDI,0xffffffff).
 *  - Loops i = 0 .. player_spawn_count-1, calling 0x1c1cc0(i) each
 *    iteration. Callee source: "player_profile.c" — saves level completion
 *    for each local player's profile.
 *  - Calls 0xe4420(level_index) to trigger the inter-level transition:
 *    level_index == -1 → main menu; -1 < level_index < 10 → load next
 *    level; else → "unknown level" error + FUN_100620 (main_menu fallback).
 *  - Loop counter compared as signed 16-bit (CMP SI, word[0x31fa94]).
 */
void main_won_map_private(void)
{
  uint16_t map_level;
  int level_index;
  int i;

  main_menu_load_pending = 1;
  main_won_map_private_pending = 0;

  /* map the current map name to a 0-based level index; unrecognized = -1 */
  map_level = (uint16_t)main_get_solo_level_from_name(map_name);
  level_index = (int)(map_level + 1);
  if ((int16_t)level_index >= 10) {
    level_index = -1;
  }

  /* record level completion in each local player's saved profile */
  for (i = 0; (int16_t)i < player_spawn_count; i++) {
    FUN_001c1cc0(i);
  }

  /* trigger level transition or return to main menu */
  ui_set_next_level(level_index);
}

/*
 * main_frame_rate_debug - 0x101130
 *
 * Confirmed:
 *  - No arguments; void return. cdecl frame, saves EBX/ESI/EDI.
 *  - Enable flag at 0x46e003 (bool, unnamed): when zero the function is a
 *    no-op (returns immediately). Not in kb.json.
 *  - State initialized/reset via flag at 0x46e391 (bool active). On first
 *    call with active==0 and enable==0, exits early. On first call with
 *    active==1 but enable==0, clears the entire state block and exits.
 *  - Frame-time history ring-buffer: float[8] at 0x46e36c (32 bytes).
 *    Current slot index at 0x46e38e (byte, wraps mod 8). Slow-frame bitmask
 *    at 0x46e38c (uint16_t, one bit per slot). Initialized with csmemset.
 *  - Slow-frame threshold: flt_46DA08 (current frame seconds) compared
 *    against double constant 0.036 at 0x28b430 (= ~1/27.78s ≈ 27.8 fps
 *    threshold) via FCOMP double ptr — decompiler shows float cast but
 *    disasm confirms double operand.
 *  - Mask bit set when frame is SLOW (> threshold), cleared when fast.
 *  - Trigger condition: all 8 slots slow (bitmask == 0xff), has-triggered
 *    flag (0x46e38f) == 0. Writes a save-core file and an init .txt log.
 *  - After trigger, sets 0x46e38f=1. Cleared back to 0 once 60 consecutive
 *    fast-frame passes accumulate (counter at 0x46e390, threshold 0x3c=60).
 *  - File helpers called directly by address; not in kb.json:
 *      0x1ba1f0: returns scenario/map name pointer (field+0x10 of globals)
 *      0x19b0d0: strips path prefix (strrchr basename)
 *      0x1d051d: fills TIME_FIELDS-like struct (KeQuerySystemTime +
 *                RtlTimeToTimeFields)
 *      0x1d90f0: internal sprintf variant (not csprintf)
 *      0x1d9e59: __fsopen wrapper (fopen with share mode 0x40)
 *      0x1d98ad: _fwprintf
 *      0x1d9bd2: _fflush
 *      0x1d9dac: _fclose
 *  - String "d:\%s_init.txt" and file modes "r"/"wt"/"a+t" confirm
 *    log-file append behavior.
 *
 * Inferred:
 *  - 0x46e003 = "debug_frame_rate_enable" or similar console/debug flag.
 *  - The 8-slot bitmask going all-1s (0xff) triggers "we're running slow"
 *    recording; 60 consecutive fast frames clears the trigger latch.
 *  - game_state_save_core writes a binary game-state snapshot whose name
 *    encodes the timestamp.
 *
 * Uncertain:
 *  - Exact semantic name of 0x46e003. Could be "profile_frame_rate" or a
 *    different per-build debug flag.
 *  - TIME_FIELDS field ordering relied on (Year/Month/Day/Hour/Min/Sec/Ms).
 *    The sprintf arg order in disasm: Month, Hour, Year, Minute, Second, Ms.
 */
void main_frame_rate_debug(void)
{
  /* frame-time history and state live at fixed addresses, not in kb.json */
  float *frame_times = (float *)0x46e36c; /* float[8] ring buffer */
  uint16_t *slow_mask = (uint16_t *)0x46e38c; /* bitmask: 1=slow slot */
  uint8_t *slot_idx = (uint8_t *)0x46e38e; /* current ring slot 0-7 */
  uint8_t *triggered = (uint8_t *)0x46e38f; /* has-triggered latch */
  uint8_t *fast_count = (uint8_t *)0x46e390; /* consecutive fast frames */
  uint8_t *active = (uint8_t *)0x46e391; /* state initialized flag */
  bool *enable = (bool *)0x46e003; /* debug enable flag */

  /* slow-frame threshold: ~27.8 fps (double, NOT float — disasm confirms) */
  static const double slow_threshold = 0.036; /* 0x28b430 */

  /* sizeof TIME_FIELDS fields (8x int16_t) */
  int16_t tf[8]; /* [0]=Year [1]=Month [2]=Day [3]=Hour [4]=Min [5]=Sec
                    [6]=Ms [7]=Weekday — layout per RtlTimeToTimeFields */

  char core_name[256]; /* [EBP-0x214..-0x115] */
  char init_path[256]; /* [EBP-0x114..-0x15] */

  uint32_t idx;
  uint32_t bit;
  char *map_name;
  void *fp;

  typedef char *(__cdecl * fn_get_scenario_name_t)(int scenario_idx);
  typedef char *(__cdecl * fn_basename_t)(char *path);
  typedef void(__cdecl * fn_get_time_t)(int16_t * tf_out);
  typedef int(__cdecl * fn_sprintf_t)(char *buf, const char *fmt, ...);
  typedef void *(__cdecl * fn_fopen_t)(const char *path, const char *mode);
  typedef int(__cdecl * fn_fwprintf_t)(void *fp, const wchar_t *fmt, ...);
  typedef int(__cdecl * fn_fflush_t)(void *fp);
  typedef int(__cdecl * fn_fclose_t)(void *fp);

  if (*active != 0) {
    if (*enable != 0)
      goto do_update;
    /* active but no longer enabled — reset state */
    *active = 0;
    csmemset(frame_times, 0, 0x20);
    *slow_mask = 0;
    *slot_idx = 0;
    *triggered = 0;
    *fast_count = 0;
    *active = 0;
  }

  if (*enable == 0)
    return;

do_update:
  /* store current frame time in ring slot */
  frame_times[*slot_idx] = flt_46DA08;

  /* update slow-frame bitmask for this slot */
  bit = (uint32_t)(1 << (*slot_idx & 0x1f));
  if (flt_46DA08 <= (float)slow_threshold) {
    *slow_mask = (uint16_t)(*slow_mask & ~(uint16_t)bit);
  } else {
    *slow_mask = (uint16_t)(*slow_mask | (uint16_t)bit);
  }

  /* advance ring index mod 8 */
  idx = (uint32_t)(int8_t)(*slot_idx + 1) & 0x80000007u;
  if ((int32_t)idx < 0)
    idx = (idx - 1 | 0xfffffff8u) + 1;
  *slot_idx = (uint8_t)idx;

  *active = 1;

  if (*triggered == 0) {
    /* first trigger: all 8 slots must be slow (bitmask 0xff) */
    if (*slow_mask == 0xff) {
      /* get scenario/map name, strip path prefix */
      map_name = ((fn_basename_t)0x19b0d0)(
        ((fn_get_scenario_name_t)0x1ba1f0)(global_scenario_index));

      /* get current time fields */
      ((fn_get_time_t)0x1d051d)(tf);

      /* build core snapshot filename:
       * <map>_slow_<mo>_<hr>_<yr>_<min>_<sec>_<ms>.bin */
      ((fn_sprintf_t)0x1d90f0)(core_name, "%s_slow_%d_%d_%d_%d_%d_%d.bin",
                               map_name, (int)(uint16_t)tf[1], /* Month */
                               (int)(uint16_t)tf[3], /* Hour */
                               (int)(uint16_t)tf[0], /* Year */
                               (int)(uint16_t)tf[4], /* Minute */
                               (int)(uint16_t)tf[5], /* Second */
                               (int)(uint16_t)tf[6]); /* Milliseconds */

      /* save binary game-state core */
      game_state_save_core(core_name);

      /* build init.txt path: d:\<map>_init.txt */
      ((fn_sprintf_t)0x1d90f0)(init_path, "d:\\%s_init.txt", map_name);

      /* open file: try "r" first to detect if it exists */
      fp = ((fn_fopen_t)0x1d9e59)(init_path, "r");
      if (fp == (void *)0) {
        /* new file: create with "wt" and write map_name line */
        fp = ((fn_fopen_t)0x1d9e59)(init_path, "wt");
        ((fn_fwprintf_t)0x1d98ad)(fp, L"map_name %s\n", map_name);
      } else {
        /* existing file: close "r" handle, reopen in append mode */
        ((fn_fclose_t)0x1d9dac)(fp);
        fp = ((fn_fopen_t)0x1d9e59)(init_path, "a+t");
      }

      /* append core snapshot filename */
      ((fn_fwprintf_t)0x1d98ad)(fp, L";core_load_name_at_startup %s\n",
                                core_name);
      ((fn_fflush_t)0x1d9bd2)(fp);
      ((fn_fclose_t)0x1d9dac)(fp);

      *triggered = 1;
    }
  } else if (*slot_idx == 0) {
    /* latch active: check if ring just completed a full pass */
    if (*slow_mask != 0) {
      /* still slow frames in window — reset fast counter */
      *fast_count = 0;
      return;
    }
    /* all frames fast this pass */
    *fast_count = *fast_count + 1;
    if (';' < *fast_count) { /* 0x3b=59 threshold: >59 = 60th pass */
      *fast_count = 0;
      *triggered = 0;
      return;
    }
  }
}

/*
 * main_update_time - 0x1013d0
 *
 * Confirmed:
 *  - Reads system_milliseconds() at entry and exit, and tracks both the raw
 *    millisecond delta (unk_time_globals.unk_0) and the hardware flip count
 *    timeline (unk_time_globals.unk_8 / unk_16 / unk_24 / unk_32).
 *  - Selects the larger of the previous target time (unk_8) and the most
 *    recent presented time (unk_32) before adjusting the next frame target.
 *  - When 0x32568d is clear, uses a 33 ms software frame cap:
 *      - clears 0x46dd9a
 *      - if ms_delta < 33, brackets an optional Sleep(33 - ms_delta) with
 *        0x91b70 / 0x91ba0 markers
 *      - else reports the overshoot to 0x8f8c0(ms_delta - 33)
 *  - When 0x32568d is set, optional pacing debug/control is driven by:
 *      - 0x325690 (requested rate; zero treated as 30)
 *      - 0x46dd96 (current divisor), 0x46dd98 (requested divisor)
 *      - failure counters at 0x46dd9e..0x46dda6 (5 x int16)
 *      - target-history slots at 0x46ddb0..0x46ddd0 (5 x int64)
 *      - debug buffer at 0x46ddfc
 *    The control loop evaluates divisors 5..1 (12/15/20/30/60 fps), updates
 *    the failure history, may keep/restore/fail-down the divisor, then adds
 *    the chosen divisor to the selected target time.
 *  - Frame seconds written to flt_46DA08 come from:
 *      - ms delta * 0.001f when 0x32568d is clear (with uint32 wrap fix), or
 *      - (target - previous_target) * (1/60) when 0x32568d is set.
 *  - If main_globals_movie is non-NULL (overlaps smaller timing globals at
 *    0x46da10 / 0x46da20), the computed frame step is overridden by the float
 *    at 0x46da20.
 *  - Non-movie frame seconds are clamped to [0, 1]. In local games
 *    (word_46DA0C == 0), extra caps apply:
 *      - normal path: max 1/15 sec (0x3d888889)
 *      - debug_game_save path: max 1/30 sec (0x3d088889)
 *  - Exit writes:
 *      unk_time_globals.unk_0  = end_ms
 *      unk_time_globals.unk_8  = chosen_target
 *      flt_46DA08              = frame_seconds
 *      0x8f870(frame_seconds)
 *      unk_time_globals.unk_16 = qword_325678
 *
 * Inferred:
 *  - 0x32568d is the per-frame pacing/throttle enable.
 *  - 0x32568e gates the adaptive divisor debugging/control path.
 *  - 0x325690 is a requested presentation rate value that maps to divisors
 *    1..5 via 60 / requested_rate (with 0 meaning 30 fps -> divisor 2).
 *  - The pooled strings "wt" and "dn" used in the debug trace likely mean
 *    "wait" and "down", but the exact abbreviations are left as raw string
 *    references rather than renamed semantics.
 *
 * Uncertain:
 *  - Exact symbolic names for 0x32568d/0x32568e/0x325690, 0x46dd96/0x46dd98,
 *    0x46dd9e..0x46ddd0, and 0x46dd9a.
 *  - Exact semantics of 0x8f870 and 0x8f8c0 beyond the observed global writes.
 *  - No register-argument (`@<reg>`) ABI edges were found in this function or
 *    its caller path; the reverse-thunk audit for this lift found only cdecl /
 *    stdcall calls.
 */
void main_update_time(void)
{
  int end_ms;
  int ms_delta;
  int buffer_length;
  int16_t requested_rate;
  int16_t desired_divisor;
  int16_t chosen_divisor;
  int16_t elapsed_game_ticks;
  int slot;
  int64_t chosen_target;
  int64_t short_target;
  int64_t present_target;
  int64_t previous_target;
  float frame_seconds;
  char *debug_buffer;
  int16_t *failure_counts;
  int64_t *target_history;

  typedef char *(__cdecl * fn_csstrcpy_t)(char *destination,
                                          const char *source);
  typedef void(__cdecl * fn_store_frame_seconds_t)(float frame_seconds);
  typedef void(__cdecl * fn_store_frame_overshoot_t)(int overshoot_ms);
  typedef void(__cdecl * fn_rdtsc_marker_t)(void);
  typedef void(__stdcall * fn_sleep_t)(int milliseconds);

  end_ms = system_milliseconds();
  previous_target = unk_time_globals.unk_8;
  present_target = unk_time_globals.unk_32;
  chosen_target = present_target;
  if (present_target < previous_target) {
    chosen_target = previous_target;
  }

  if (*(char *)0x32568d == '\0') {
    ms_delta = end_ms - (int)unk_time_globals.unk_0;
    *(char *)0x46dd9a = 0;
    if (ms_delta < 0x21) {
      ((fn_rdtsc_marker_t)0x91b70)();
      if (*(char *)0x31fa96 != '\0') {
        ((fn_sleep_t)0x1d0362)(0x21 - ms_delta);
      }
      ((fn_rdtsc_marker_t)0x91ba0)();
    } else {
      ((fn_store_frame_overshoot_t)0x8f8c0)(ms_delta - 0x21);
    }
  } else {
    debug_buffer = (char *)0x46ddfc;
    failure_counts = (int16_t *)0x46dd9e;
    target_history = (int64_t *)0x46ddb0;

    ((fn_csstrcpy_t)0x8dff0)(debug_buffer, "");
    if (*(char *)0x31fa96 != '\0' && *(int16_t *)0x325690 >= 0) {
      requested_rate = *(int16_t *)0x325690;
      if (requested_rate == 0) {
        requested_rate = 0x1e;
      }

      desired_divisor = (int16_t)(0x3c / requested_rate);
      chosen_divisor = desired_divisor;
      *(int16_t *)0x46dd98 = desired_divisor;

      if (*(char *)0x32568e != '\0') {
        int16_t best_divisor;
        int16_t current_divisor;

        elapsed_game_ticks = game_time_get_elapsed();
        current_divisor = *(int16_t *)0x46dd96;
        short_target = (int64_t)(int16_t)(uint16_t)chosen_target;
        best_divisor = 5;

        snprintf(debug_buffer, 0x200,
                 "last%6I64d init%6I64d achv%6I64d pres%6I64d g%d cur%d... ",
                 unk_time_globals.unk_8, unk_time_globals.unk_16,
                 unk_time_globals.unk_24, unk_time_globals.unk_32,
                 (int)elapsed_game_ticks, (int)current_divisor);

        for (slot = 5; slot > 0; slot--) {
          int index;
          int16_t failure_count;
          int16_t clamped_failure_count;
          int16_t target_age;
          int16_t slot_bucket;
          bool ignore_failure;
          const char *label;
          int64_t target_age_raw;

          index = slot - 1;
          failure_count = failure_counts[index];
          clamped_failure_count = failure_count;
          if (clamped_failure_count > 99) {
            clamped_failure_count = 99;
          }

          target_age_raw = short_target - target_history[index];
          if (target_age_raw > 99) {
            target_age = 99;
          } else {
            target_age = (int16_t)target_age_raw;
          }

          slot_bucket = (int16_t)((slot + 1) / 2);
          ignore_failure = false;
          if ((int16_t)((current_divisor + 1) / 2) > slot_bucket &&
              slot_bucket >= (int16_t)(current_divisor / 2) &&
              elapsed_game_ticks > slot_bucket) {
            ignore_failure = true;
          }

          if (unk_time_globals.unk_24 >= unk_time_globals.unk_16 + slot) {
            if (chosen_target >= target_history[index] + 0xf) {
              label = (const char *)0x28b48c;
              if (failure_counts[index] < 4) {
                label = (const char *)0x28b3fc;
              }

              buffer_length = csstrlen(debug_buffer);
              snprintf(debug_buffer + buffer_length, 0x200 - buffer_length,
                       "(%s%2d/%2d) ", label, (int)clamped_failure_count,
                       (int)target_age);
            } else {
              failure_counts[index] = 0;

              buffer_length = csstrlen(debug_buffer);
              snprintf(debug_buffer + buffer_length, 0x200 - buffer_length,
                       "(ok   %2d) ", (int)target_age);
            }
          } else {
            if (ignore_failure) {
              label = "ignor";
            } else {
              failure_counts[index] = failure_count + 1;
              target_history[index] = chosen_target;
              label = "fail ";
            }

            buffer_length = csstrlen(debug_buffer);
            snprintf(debug_buffer + buffer_length, 0x200 - buffer_length,
                     "(%s%2d) ", label, (int)clamped_failure_count);
          }

          if (desired_divisor <= slot && failure_counts[index] < 4) {
            best_divisor = (int16_t)slot;
          }
        }

        if (best_divisor == 0) {
          requested_rate = 999;
        } else {
          requested_rate = (int16_t)(0x3c / best_divisor);
        }

        if (*(int16_t *)0x46dd96 < best_divisor) {
          buffer_length = csstrlen(debug_buffer);
          snprintf(debug_buffer + buffer_length, 0x200 - buffer_length,
                   " FAILDOWN %d", (int)requested_rate);
        } else if (best_divisor < *(int16_t *)0x46dd96) {
          buffer_length = csstrlen(debug_buffer);
          snprintf(debug_buffer + buffer_length, 0x200 - buffer_length,
                   " RESTORE  %d", (int)requested_rate);
        } else {
          buffer_length = csstrlen(debug_buffer);
          snprintf(debug_buffer + buffer_length, 0x200 - buffer_length,
                   " MAINTAIN %d", (int)requested_rate);
        }

        buffer_length = csstrlen(debug_buffer);
        snprintf(debug_buffer + buffer_length, 0x200 - buffer_length,
                 " des %d targ%6I64d", (int)requested_rate,
                 chosen_target + best_divisor);

        chosen_divisor = best_divisor;
      }

      chosen_target += chosen_divisor;
      *(int16_t *)0x46dd96 = chosen_divisor;
    }
  }

  end_ms = system_milliseconds();
  if (chosen_target < qword_325678) {
    chosen_target = qword_325678;
  }

  if (*(char *)0x32568d == '\0') {
    frame_seconds = (float)(end_ms - (int)unk_time_globals.unk_0);
    if (end_ms - (int)unk_time_globals.unk_0 < 0) {
      frame_seconds = frame_seconds + 4294967296.0f;
    }
    frame_seconds = frame_seconds * 0.001000000047497451f;
  } else {
    frame_seconds =
      (float)(chosen_target - unk_time_globals.unk_8) * 0.01666666753590107f;
  }

  if (main_globals_movie == NULL) {
    if (frame_seconds < 0.0f) {
      frame_seconds = 0.0f;
    } else if (frame_seconds > 1.0f) {
      frame_seconds = 1.0f;
    }

    if (word_46DA0C == 0) {
      if (!debug_game_save) {
        if (frame_seconds > 0.06666667014360428f) {
          frame_seconds = 0.06666667014360428f;
        }
      } else if (frame_seconds > 0.03333333507180214f) {
        frame_seconds = 0.03333333507180214f;
      }
    }
  } else {
    frame_seconds = *(float *)0x46da20;
  }

  unk_time_globals.unk_0 = end_ms;
  unk_time_globals.unk_8 = chosen_target;
  flt_46DA08 = frame_seconds;
  ((fn_store_frame_seconds_t)0x8f870)(frame_seconds);
  unk_time_globals.unk_16 = qword_325678;
}

/*
 * main_rasterizer_throttle - 0x101970
 *
 * Confirmed:
 *  - Reads the hardware vblank/flip counter pair (qword_325678 at
 *    0x325678/0x32567c) and writes a "frame start" snapshot
 *    (qword_325678 + 1) into unk_time_globals.unk_24 (0x46d9f8/0x46d9fc).
 *  - Two enable flags gate framerate control:
 *      0x31fa96 = master rasterizer vblank enable
 *      0x32568d = per-frame enable (cleared to 0 on 1000 ms timeout)
 *  - When enabled, waits in a spin loop for the flip counter to reach
 *    unk_time_globals.unk_8 - 1 (0x46d9e8/0x46d9ec). Bracketed by
 *    RDTSC calls at 0x91b70 (start) and 0x91ba0 (end). During the wait,
 *    if cache_files_precache_in_progress (0x1bc6b0) returns true, the
 *    loop calls 0x1d0362(1) (stdcall sleep/yield) each iteration.
 *  - After the wait (or if throttle was skipped), stores a "frame end"
 *    snapshot (qword_325678 + 1) into unk_time_globals.unk_32
 *    (0x46da00/0x46da04).
 *  - Computes frames elapsed since last vblank target as a signed int64
 *    (frame_end - unk_8), clamped to [0, 0x7fff] → int16_t frames_delta.
 *    Captures synced = (0x46dd96 == 0x46dd98) at frame-end snapshot time.
 *  - Appends a debug timing string to the buffer at 0x46ddfc using
 *    csstrlen (0x8df60, called twice due to MSVC pre-push interleaving)
 *    and snprintf (0x1d9179). Format: "%6I64d(targ%6I64d %s%2d)" with
 *    entry flip count, target (unk_8), label, and elapsed ticks.
 *    Label is "THROTTLE" if we waited, "SYNCED  " if on-time, else
 *    "LAPSED  ".
 *  - Writes 0x46dd9a (frame-pacing active flag): 1 iff 0x46dd96 > 0 &&
 *    frames_delta == 0, else 0.
 *  - Calls 0x8f880(frames_delta, synced, debug_buf) to store per-frame
 *    profile counters.
 */
void main_rasterizer_throttle(void)
{
  /* snapshot of hardware flip counter pair at function entry */
  unsigned int entry_flip_lo;
  int entry_flip_hi;
  /* target: unk_time_globals.unk_8 - 1 (64-bit) */
  unsigned int target_lo;
  int target_hi;
  /* system_milliseconds() at start of wait loop (timeout reference) */
  unsigned int ms_start;
  /* true if we actually entered the vblank wait */
  bool did_throttle;
  /* return of cache_files_precache_in_progress */
  bool precache_in_progress;
  /* frames elapsed since vblank target, clamped to [0, 0x7fff] */
  int16_t frames_delta;
  /* whether 0x46dd96 == 0x46dd98 at frame-end snapshot time */
  bool synced;
  /* elapsed flip ticks (throttle path) or sign-extended frames_delta */
  unsigned int elapsed_lo;
  int elapsed_hi;
  /* frame-end flip counter snapshot (qword_325678 + 1) */
  unsigned int end_flip_lo;
  int end_flip_hi;
  /* raw 64-bit delta: frame_end - unk_8 */
  unsigned int udelta_lo;
  int udelta_hi;
  /* debug label: "THROTTLE", "SYNCED  ", or "LAPSED  " */
  const char *label;
  /* csstrlen return values for debug buffer append (called twice per MSVC
   * pre-push interleaving: first to compute remaining space, second to
   * compute end-of-string pointer) */
  int str_len1;
  int str_len2;

  typedef unsigned int(__cdecl * fn_system_ms_t)(void);
  typedef bool(__cdecl * fn_precache_t)(void);
  typedef void(__stdcall * fn_yield_t)(int);
  typedef void(__cdecl * fn_warn_t)(const char *);
  typedef void(__cdecl * fn_rdtsc_t)(void);
  typedef int(__cdecl * fn_csstrlen_t)(const char *);
  typedef void(__cdecl * fn_profile_store_t)(int16_t, bool, const char *);
  typedef int(__cdecl * fn_snprintf_t)(char *, int, const char *, ...);

  /* snapshot hardware flip counter on entry */
  entry_flip_lo = *(unsigned int *)0x325678;
  entry_flip_hi = *(int *)0x32567c;

  /* frame-start snapshot = qword_325678 + 1 (64-bit) */
  *(unsigned int *)0x46d9f8 = entry_flip_lo + 1;
  *(int *)0x46d9fc = entry_flip_hi + (unsigned int)(0xfffffffe < entry_flip_lo);

  did_throttle = false;

  /* framerate control: master enable (0x31fa96) and per-frame enable
   * (0x32568d, cleared on timeout) must both be non-zero */
  if ((*(char *)0x31fa96 != '\0') && (*(char *)0x32568d != '\0')) {
    /* target = unk_time_globals.unk_8 - 1 (64-bit decrement) */
    target_lo = *(unsigned int *)0x46d9e8 - 1;
    target_hi =
      *(int *)0x46d9ec - (unsigned int)(*(unsigned int *)0x46d9e8 == 0);

    /* enter throttle only if current flip count < target (signed 64-bit) */
    if (!(*(int *)0x32567c > target_hi) &&
        !((*(int *)0x32567c == target_hi) &&
          (*(unsigned int *)0x325678 >= target_lo))) {
      ms_start = ((fn_system_ms_t)0x8e370)();
      precache_in_progress = ((fn_precache_t)0x1bc6b0)();
      did_throttle = true;
      ((fn_rdtsc_t)0x91b70)(); /* RDTSC timestamp: throttle start */

      /* re-check: still behind? spin-wait for vblank */
      if (!(*(int *)0x32567c > target_hi) &&
          !((*(int *)0x32567c == target_hi) &&
            (*(unsigned int *)0x325678 >= target_lo))) {
        while (1) {
          if (precache_in_progress) {
            ((fn_yield_t)0x1d0362)(1); /* yield during precache */
          }
          /* timeout guard: give up after 1000 ms */
          if (((fn_system_ms_t)0x8e370)() > ms_start + 1000U) {
            ((fn_warn_t)0xff550)(
              "stuck waiting for VBLANK callback! disabling rasterizer "
              "framerate control");
            *(char *)0x32568d = '\0'; /* disable per-frame throttle */
            break;
          }
          /* exit if flip counter reached target */
          if ((*(int *)0x32567c > target_hi) ||
              ((*(int *)0x32567c == target_hi) &&
               (*(unsigned int *)0x325678 >= target_lo))) {
            break;
          }
        }
      }
      ((fn_rdtsc_t)0x91ba0)(); /* RDTSC timestamp: throttle end */
    }
  }

  /* frame-end snapshot = qword_325678 + 1 (64-bit) */
  end_flip_lo = *(unsigned int *)0x325678 + 1;
  end_flip_hi =
    *(int *)0x32567c + (unsigned int)(*(unsigned int *)0x325678 > 0xfffffffe);

  /* capture synced flag before clobbering registers */
  synced = (*(int16_t *)0x46dd96 == *(int16_t *)0x46dd98);
  *(unsigned int *)0x46da00 = end_flip_lo;
  *(int *)0x46da04 = end_flip_hi;

  /* frames elapsed = frame_end - unk_time_globals.unk_8 (signed 64-bit),
   * clamped to int16 range [0, 0x7fff] */
  udelta_lo = end_flip_lo - *(unsigned int *)0x46d9e8;
  udelta_hi = end_flip_hi - *(int *)0x46d9ec -
              (unsigned int)(end_flip_lo < *(unsigned int *)0x46d9e8);

  if (udelta_hi > 0 || (udelta_hi == 0 && udelta_lo > 0x7fff)) {
    frames_delta = 0x7fff; /* saturate high */
  } else if (udelta_hi < 0) {
    frames_delta = 0; /* underflow: treat as 0 */
  } else {
    frames_delta = (int16_t)udelta_lo;
  }

  /* build debug label and elapsed value for the timing string */
  if (did_throttle) {
    /* elapsed = current flip count - entry flip count (64-bit sub) */
    elapsed_lo = *(unsigned int *)0x325678 - entry_flip_lo;
    elapsed_hi = *(int *)0x32567c - entry_flip_hi -
                 (unsigned int)(*(unsigned int *)0x325678 < entry_flip_lo);
    label = "THROTTLE";
  } else {
    /* use sign-extended frames_delta as elapsed (matches MOVSX/CDQ) */
    elapsed_lo = (unsigned int)(int)frames_delta;
    elapsed_hi = (int)frames_delta >> 0x1f;
    label = "SYNCED  ";
    if (frames_delta != 0) {
      label = "LAPSED  ";
    }
  }

  /* append timing info to the debug ring buffer at 0x46ddfc.
   * csstrlen is called twice due to MSVC pre-push interleaving: the first
   * call measures the current length to compute remaining space; the
   * second call (with the same arg) computes the end-of-string pointer.
   * Both calls produce the same result since the buffer is not modified
   * between them. */
  str_len1 = ((fn_csstrlen_t)0x8df60)((const char *)0x46ddfc);
  str_len2 = ((fn_csstrlen_t)0x8df60)((const char *)0x46ddfc);
  ((fn_snprintf_t)0x1d9179)((char *)(0x46ddfc + str_len2), 0x200 - str_len1,
                            "%6I64d(targ%6I64d %s%2d)", entry_flip_lo,
                            entry_flip_hi, *(unsigned int *)0x46d9e8,
                            *(int *)0x46d9ec, label, elapsed_lo, elapsed_hi);

  /* frame-pacing active flag: set iff dd96 counter > 0 and no frames
   * elapsed this tick (i.e., we're running ahead of schedule) */
  if (*(int16_t *)0x46dd96 > 0 && frames_delta == 0) {
    *(char *)0x46dd9a = 1;
  } else {
    *(char *)0x46dd9a = 0;
  }

  /* store per-frame profile: delta count, synced flag, debug string */
  ((fn_profile_store_t)0x8f880)(frames_delta, synced, (const char *)0x46ddfc);
}

/* main_movie_start - 0x101bc0
 *
 * Begin movie capture: allocate the 640x480 offscreen bitmap that
 * render_frame_present blits each frame into, clear the movie output
 * directory, reset the frame counter at 0x46da1c, and pin the frame step
 * that main_update_time reads back from 0x46da20 to 1/fps.  Frame rates at
 * or below 1.0e-4 fall back to a fixed 1/30s step.
 *
 * ABI: one cdecl float stack param at [EBP+8] (FLD/FDIV dword ptr [EBP+8]);
 * bare RET, so the caller does the ADD ESP,4.
 *
 * FCOM sense (lift-learnings §38): the original compare is
 *   FLD [EBP+8]; FCOMP [0x253f44]; FNSTSW AX; TEST AH,0x41; JNZ <fallback>
 * TEST AH,0x41 masks C0|C3, so the branch is taken for fps <= 1.0e-4 OR NaN
 * and the fallthrough is the STRICT `>` path.  Do not relax this to `>=`.
 *
 * FDIV operand order: FLD dword [0x2533c8] (=1.0f) then FDIV dword [EBP+8],
 * i.e. 1.0f / fps -- the constant is the numerator.
 */
void main_movie_start(float frames_per_second)
{
  if (main_globals_movie != NULL) {
    display_assert("main_globals.movie==NULL", "c:\\halo\\SOURCE\\main\\main.c",
                   0xa6b, true);
    system_exit(-1);
  }

  main_globals_movie = bitmap_2d_new(0x280, 0x1e0, 0, 10);
  if (main_globals_movie != NULL) {
    directory_create_or_delete_contents("movie");

    /* movie frame counter; cleared before the compare (the original
     * schedules this store between the FCOMP and the FNSTSW) */
    *(int *)0x46da1c = 0;

    if (frames_per_second > 1.0e-4f) {
      *(float *)0x46da20 = 1.0f / frames_per_second;
    } else {
      *(float *)0x46da20 = 0.03333333507180214f;
    }
    game_time_set_speed(1.0f);
  }
}

/* Clear both rasterizer timing flags. */
void main_lost_map(void)
{
  *(char *)0x46da46 = 0;
  *(char *)0x46da47 = 0;
}

/* Set the rasterizer frame-skip flag. */
void main_start_time(void)
{
  *(char *)0x46da47 = 1;
}

/*
 * main_crash - 0x101cb0
 *
 * Deliberate crash-test hook: stores the address of a string literal through a
 * NULL pointer to provoke a page fault. Confirmed from the two-instruction
 * body -- MOV dword ptr [0], offset s_chucky (0x28b5a8); RET. The stored value
 * is the string's ADDRESS, not its bytes, and the DIR32 relocation against the
 * literal is the function's only relocation. The store must stay volatile so
 * the compiler cannot discard it as undefined behaviour.
 *
 * Takes one cdecl argument that the body deliberately ignores. Evidence: the
 * function has exactly one call site in the whole XBE (0xc1def, the HaloScript
 * `crash` builtin at 0xc1dd0), and that site pushes one dword -- the string
 * the script passed -- before the CALL. A cdecl parameter that the callee
 * never reads produces byte-identical codegen to no parameter at all, so the
 * two-instruction body is not evidence against the argument; the caller's PUSH
 * is evidence for it.
 */
void main_crash(const char *reason)
{
  *(const char *volatile *)0 = "chucky was here!  NULL belongs to me!!!!!";
}

/*
 * main_print_version - 0x101cc0
 *
 * Prints the build banner to console channel 0. Confirmed from the whole
 * five-instruction body:
 *   PUSH 0x28b5d4  ; format string
 *   PUSH 0x0       ; channel
 *   CALL 0xff4d0   ; console_printf
 *   ADD ESP,0x8
 *   RET
 *
 * The literal at 0x28b5d4 is exactly
 * "halobeta xbox 01.10.12.2276 Oct 12 2001 16:07:48" -- note it does NOT
 * contain "built at:", unlike the longer variant at 0x28b60c used by
 * main_framerate_render. The two must stay separate string literals so each
 * call site keeps referencing its own .rdata slot.
 *
 * console_printf is variadic; two pushes for a format string with no
 * conversion specifiers is the complete argument list (the ADD ESP,0x8
 * cleanup confirms exactly two stack args).
 */
void main_print_version(void)
{
  console_printf(0, "halobeta xbox 01.10.12.2276 Oct 12 2001 16:07:48");
}

/*
 * main_vertical_blank_interrupt_handler - 0x101cd0
 *
 * Interrupt-context callback invoked by the D3D vblank interrupt. Increments
 * the 64-bit hardware flip counter at 0x325678/0x32567c, then optionally
 * records timing history when a flip-count pointer is available.
 *
 * Confirmed:
 *  - Increments qword at 0x325678 (lo) / 0x32567c (hi) by 1 with carry.
 *  - If flip_count_ptr (0x46ddd8) is NULL: copies the current flip counter
 *    to the "presented" snapshot at 0x325680/0x325684, then tail-calls
 *    input_tick (0xcf7e0).
 *  - If flip_count_ptr is non-NULL and *flip_count_ptr != DAT_325670:
 *      - Stores (uint16_t)(flip_lo - presented_lo) into the ring buffer
 *        at 0x46ddde + word_46DDDC * 2.
 *      - Advances word_46DDDC = (word_46DDDC + 1) % 15.
 *      - Copies current flip counter to presented snapshot.
 *      - Updates DAT_325670 = *flip_count_ptr.
 *  - Always tail-calls input_tick (0xcf7e0) at exit.
 *  - No stack frame (no PUSH EBP / MOV EBP,ESP in original — but we emit
 *    one from C; the function is simple enough that the overhead is fine).
 */
void main_vertical_blank_interrupt_handler(void)
{
  uint32_t flip_lo;
  uint32_t presented_lo;
  int16_t ring_index;

  /* increment 64-bit flip counter with carry */
  flip_lo = *(uint32_t *)0x325678 +
            1; /* hazard-ok: value-arithmetic (counter+1 for carry) */
  *(uint32_t *)0x325678 = flip_lo;
  *(uint32_t *)0x32567c = *(uint32_t *)0x32567c + (uint32_t)(flip_lo == 0);

  if (flip_count_ptr == NULL) {
    /* no flip-count source: just snapshot the counter */
    *(uint32_t *)0x325680 = *(uint32_t *)0x325678;
    *(uint32_t *)0x325684 = *(uint32_t *)0x32567c;
    input_tick();
    return;
  }

  if (*flip_count_ptr != *(int *)0x325670) {
    /* flip count changed: record timing delta in ring buffer */
    presented_lo = *(uint16_t *)0x325680;
    ring_index = word_46DDDC;
    *(int16_t *)(0x46ddde + ring_index * 2) =
      (int16_t)((uint16_t) * (uint32_t *)0x325678 - (uint16_t)presented_lo);

    word_46DDDC = (int16_t)((ring_index + 1) % 15);

    *(uint32_t *)0x325680 = *(uint32_t *)0x325678;
    *(uint32_t *)0x325684 = *(uint32_t *)0x32567c;
    *(int *)0x325670 = *flip_count_ptr;
  }

  input_tick();
}

/*
 * main_save_current_solo_map - 0x101d90
 *
 * Writes the current solo-map name to "z:\\last_solo.txt" so it can be
 * reloaded later by main_load_last_solo_map. Called from 0xa6dc0 (the
 * "queue_map" helper) on a successful solo campaign map change.
 *
 * Confirmed:
 *  - Guard: 0x1006f0 maps the map-name string to a campaign level index
 *    (0..9) or 0xffff when the name is not a known solo level. When the
 *    guard returns 0xffff the function returns without opening the file.
 *    Same helper used by main_won_map_private and main_load_last_solo_map.
 *  - File I/O helpers (addresses reused from main_load_last_solo_map /
 *    main_frame_rate_debug; not in kb.json):
 *      fopen  = 0x1d9e59 with mode "w" (DAT_00265938)
 *      fwrite = 0x1db2b3 (signature fwrite(buf, size, count, fp) — the
 *               Ghidra symbol "FID_conflict:_fread" at 0x1db2b3 is
 *               actually fwrite; its body calls the write-buffer helper
 *               at 0x1db19c which performs MOVSD/MOVSB REP from the
 *               caller buffer into the FILE's buffer).
 *      fclose = 0x1d9dac
 *  - File contents: fwrite(map_name, 1, csstrlen(map_name) + 1, fp) —
 *    includes the terminating NUL so the reader can read the full path
 *    as a NUL-terminated C string.
 *  - On fopen failure: error(2, "Couldn't create a file to write the "
 *    "current solo map to") — no ABORT, no fallback. The solo progress
 *    simply isn't persisted.
 *  - The fopen PUSH ESI just before reserves the fclose fp arg slot,
 *    and ADD ESP,0x14 at the tail cleans fwrite's 4 args + fclose's
 *    1 arg together (MSVC pre-push interleaving).
 *
 * Uncertain:
 *  - csstrlen is the size-1 strlen at 0x8df60 (confirmed in kb.json).
 *    Ghidra's "csstrlen" stub in the decomp was the same helper.
 */
void main_save_current_solo_map(char *map_name)
{
  uint16_t level_index;
  void *fp;

  typedef void *(__cdecl * fn_fopen_t)(const char *path, const char *mode);
  typedef size_t(__cdecl * fn_fwrite_t)(const void *buf, size_t size,
                                        size_t count, void *fp);
  typedef int(__cdecl * fn_fclose_t)(void *fp);

  level_index = (uint16_t)main_get_solo_level_from_name(map_name);
  if (level_index == 0xffff) {
    return;
  }

  fp = ((fn_fopen_t)0x1d9e59)("z:\\last_solo.txt", "w");
  if (fp == NULL) {
    error(2, "Couldn't create a file to write the current solo map to");
    return;
  }

  ((fn_fwrite_t)0x1db2b3)(map_name, 1, csstrlen(map_name) + 1, fp);
  ((fn_fclose_t)0x1d9dac)(fp);
}

/*
 * main_load_last_solo_map - 0x101e00
 *
 * Called from the main loop when main_load_last_solo_map_pending (0x46da48)
 * is set. Reads the last-solo-map name from "z:\\last_solo.txt" and queues
 * that map (or the default "levels\\a10\\a10") for the next change-map pass.
 *
 * Confirmed:
 *  - Pending guard: main_load_last_solo_map_pending must be non-zero.
 *    A second guard at 0x1c5940 returns non-zero while a saved-film / demo
 *    playback is active (it reads DAT_0046cc86-adjacent globals 0x4ead58 /
 *    0x4ead60); when that guard fires, the function bails out without
 *    clearing either pending flag.
 *  - File I/O (addresses match the LIBCMT thunks already used by
 *    main_frame_rate_debug):
 *      fopen  = 0x1d9e59 with mode "r" (DAT_002658a4)
 *      fread  = 0x1db3f7 (size_t fread(buf, 1, 0xff, fp))
 *      fclose = 0x1d9dac
 *  - 256-byte stack buffer (local_104 at [EBP-0x100]). fread is clamped to
 *    0xff via a signed compare (JLE) and buf[n] is explicitly nulled.
 *  - 0x1006f0 maps a map-name string to a level index (0-9) or -1 (0xffff).
 *    Same helper already used by main_won_map_private. A 0xffff return means
 *    the loaded path is not a known level; fall back to the default.
 *  - Default map pointer lives at *(char **)0x31fa9c (points at the string
 *    "levels\\a10\\a10" — not in kb.json, accessed by hardcoded address).
 *  - 0xfffa0 is the shared "queue change-map-name" helper: copies the
 *    argument into map_name[] (0x46da55), clears main_menu_load_pending
 *    (0x46da43), sets byte_46DA54, and — if the game is in progress with
 *    word_46DA0C == 0 — arms main_change_map_name_pending.
 *  - On exit: clears main_change_map_name_pending (0x46da25) and
 *    main_load_last_solo_map_pending (0x46da48). The 0xfffa0 helper had
 *    just armed main_change_map_name_pending; this trailing clear undoes
 *    that, which is intentional — the original binary forgoes the
 *    change-map path when loading the last-solo map directly.
 *
 * Inferred:
 *  - 0x1c5940 is a "saved-film / demo is being played back" predicate. Its
 *    body reads DAT_0046ead58 (byte) and DAT_0046ead60 (dword); returns 1
 *    only when both are set. Exact name not confirmed from strings.
 *
 * Uncertain:
 *  - The paired globals gating 0x1c5940 have no strong semantic label yet.
 */
void main_load_last_solo_map(void)
{
  char buf[256];
  void *fp;
  int n;
  char *map_path;
  uint16_t level_index;

  typedef bool(__cdecl * fn_film_active_t)(void);
  typedef void *(__cdecl * fn_fopen_t)(const char *path, const char *mode);
  typedef size_t(__cdecl * fn_fread_t)(void *buf, size_t size, size_t count,
                                       void *fp);
  typedef int(__cdecl * fn_fclose_t)(void *fp);
  typedef void(__cdecl * fn_queue_map_t)(char *map_path);

  if (!main_load_last_solo_map_pending) {
    return;
  }
  if (((fn_film_active_t)0x1c5940)()) {
    return;
  }

  /* default: *(char **)0x31fa9c → "levels\\a10\\a10" */
  map_path = *(char **)0x31fa9c;

  fp = ((fn_fopen_t)0x1d9e59)("z:\\last_solo.txt", "r");
  if (fp != NULL) {
    n = (int)((fn_fread_t)0x1db3f7)(buf, 1, 0xff, fp);
    ((fn_fclose_t)0x1d9dac)(fp);
    if (n > 0xff) {
      n = 0xff;
    }
    buf[n] = 0;
    level_index = (uint16_t)main_get_solo_level_from_name(buf);
    if (level_index != 0xffff) {
      map_path = buf;
    }
  }

  ((fn_queue_map_t)0xfffa0)(map_path);
  main_change_map_name_pending = 0;
  main_load_last_solo_map_pending = 0;
}

/*
 * main_save_map_no_timeout - 0x101ec0
 *
 * Arms a pending map-save request and clears the secondary/timeout flag at
 * 0x46da2a, instead of setting it the way main_save_map_safe (0x100330) does.
 *
 * Confirmed (disassembly; 14 instructions, no frame, no locals, no CALLs):
 *    00101ec0  MOV  CL,byte ptr [0x0046da28]
 *    00101ec6  XOR  EAX,EAX
 *    00101ec8  CMP  CL,AL
 *    00101eca  JZ   0x00101ed4               ; byte_46DA28 == 0 -> arm block
 *    00101ecc  CMP  byte ptr [0x0046da2a],AL
 *    00101ed2  JZ   0x00101ef2               ; 0x46da2a == 0 -> skip arm block
 *    00101ed4  MOV  CL,0x1
 *    00101ed6  MOV  byte ptr [0x0046da28],CL ; BYTE
 *    00101edc  MOV  byte ptr [0x0046da29],CL ; BYTE
 *    00101ee2  MOV  [0x0046da2c],EAX         ; DWORD
 *    00101ee7  MOV  [0x0046da30],EAX         ; DWORD
 *    00101eec  MOV  [0x0046da38],AX          ; WORD, not DWORD
 *    00101ef2  MOV  [0x0046da2a],AL          ; BYTE, branch target: both paths
 *    00101ef7  RET
 *  - Plain cdecl void(void). RET carries no immediate, and no register is read
 *    before being written (CL and EAX are both defined first), so there are no
 *    implicit @<reg> inputs.
 *  - Store widths come from the operand sizes above: 0x46da28/29/2a are BYTE,
 *    0x46da2c and 0x46da30 are DWORD, and 0x46da38 is a 16-bit WORD. Storing
 *    a dword to 0x46da38 would clobber 0x46da3a, which main_won_map
 *    (0x100370) sets to 1.
 *  - The clear of 0x46da2a is the second JZ's branch target, so it runs on
 *    both paths and belongs after the if, not inside it.
 *  - Guard is an OR taken as written, with no inversion: the first JZ enters
 *    the arm block when byte_46DA28 == 0, the second skips it when
 *    0x46da2a == 0.
 *
 * Inferred:
 *  - Same six globals and the same OR guard as main_save_map_safe (0x100330).
 *    The only difference is that 0x100330 stores 1 to 0x46da2a inside the
 *    guard while this variant stores 0 to it unconditionally afterwards,
 *    matching the kb-registered name main_save_map_no_timeout: it arms the
 *    save request with the secondary/timeout flag cleared.
 *  - 0x46da29 is the arm bit, 0x46da2c a cooldown, 0x46da30 a tick total and
 *    0x46da38 a 16-bit consecutive-success counter, all consumed by
 *    main_save_map_private (0x100eb0).
 *
 * Uncertain:
 *  - The precise meaning of the secondary flag 0x46da2a is not established
 *    here; main_save_map_private only tests it once the retry counter has
 *    exceeded 0xef ticks.
 *  - 0x46da29, 0x46da2a, 0x46da2c, 0x46da30 and 0x46da38 have no
 *    kb-registered names, so they keep the raw-address idiom already used by
 *    main_save_map_nonsafe, main_save_map_safe and main_save_map_private in
 *    this TU.
 */
void main_save_map_no_timeout(void)
{
  if (byte_46DA28 == 0 || *(uint8_t *)0x46da2a != 0) {
    byte_46DA28 = 1;
    *(uint8_t *)0x46da29 = 1;
    *(int32_t *)0x46da2c = 0;
    *(int32_t *)0x46da30 = 0;
    *(int16_t *)0x46da38 = 0;
  }
  *(uint8_t *)0x46da2a = 0;
}

/*
 * main_load_ui_scenario - 0x101f00
 *
 * Loads the main-menu UI scenario "levels\\ui\\ui" and initializes the
 * game-engine / director state for the menu. Called from main_menu_load
 * (0x101fe0) when main_globals.main_menu_scenario_loaded is clear, and from
 * the game-startup path with param_1 = 1 to also precache menu resources.
 *
 * Confirmed:
 *  - Precaches the UI level twice. The first call is with the string
 *    literal "levels\\ui\\ui"; the second call passes the same string
 *    after it has been copied into the local game_options.map_name[].
 *    The original binary emits both calls and we preserve that.
 *  - Asserts !main_globals.main_menu_scenario_loaded (the function may
 *    not be re-entered while the menu scenario is already resident).
 *    Original message / path / line are preserved verbatim.
 *  - game_options_t is built entirely on the stack (0x10c bytes at
 *    [EBP-0x10c]); csstrncpy(map_name, "levels\\ui\\ui", 0xff) with an
 *    explicit map_name[255] = 0 terminator. This matches the layout in
 *    types.h (map_name at offset 0xC).
 *  - Tear-down / setup order is: game_dispose_from_old_map, game_unload,
 *    game_engine_dispose, game_set_game_variant(0),
 *    main_menu_scenario_loaded = 1, main_new_map(&game_options).
 *  - Post main_new_map the function calls three director/UI helpers
 *    (0x86cb0, 0x85180, 0xe43d0) and arms main_load_last_solo_map_pending.
 *    The trailing `if (a1)` branch calls main_menu_precache_resources.
 *
 * Inferred:
 *  - 0x86cb0 lives in camera/director.c (its asserts reference that file)
 *    and appears to reset/enable directors for each local player;
 *    called here with 1.
 *  - 0x85180 appears to be a cinematic/cutscene state initializer;
 *    called here with (0, 0, -1).
 *  - 0xe43d0 is the UI widget-flag-2 setter already used by
 *    main_change_map_name; called here with 1.
 *
 * Uncertain:
 *  - Exact semantics of 0x86cb0 / 0x85180 arguments beyond the observed
 *    constant values. Names are withheld pending stronger evidence.
 */
void main_load_ui_scenario(bool a1)
{
  game_options_t game_options;

  typedef void(__cdecl * fn_director_init_t)(int arg);
  typedef void(__cdecl * fn_cinematic_reset_t)(int16_t a, int16_t b, int c);
  typedef void(__cdecl * fn_set_widget_flag2_t)(bool enable);

  game_precache_new_map("levels\\ui\\ui", 1);

  if (main_globals.main_menu_scenario_loaded) {
    display_assert("!main_globals.main_menu_scenario_loaded",
                   "c:\\halo\\SOURCE\\main\\main.c", 0x444, 1);
    system_exit(-1);
  }

  game_options_new(&game_options);
  csstrncpy(game_options.map_name, "levels\\ui\\ui", 0xff);
  game_options.map_name[255] = 0;

  game_precache_new_map(game_options.map_name, 1);
  game_dispose_from_old_map();
  game_unload();
  game_engine_dispose();
  game_set_game_variant(0);

  main_globals.main_menu_scenario_loaded = 1;
  main_new_map(&game_options);

  ((fn_director_init_t)0x86cb0)(1);
  ((fn_cinematic_reset_t)0x85180)(0, 0, -1);
  ((fn_set_widget_flag2_t)0xe43d0)(1);

  main_load_last_solo_map_pending = 1;

  if (a1) {
    main_menu_precache_resources();
  }
}

void main_menu_load(void)
{
  if (!main_globals.main_menu_scenario_loaded) {
    main_load_ui_scenario(0);
  }
  main_screen_shell_load();
  main_menu_precache_resources();
  update_server_delete();
  update_server_new();
  update_server_start();
  game_time_dispose_from_old_map();
  game_time_initialize_for_new_map();
  game_time_start();
  hs_runtime_dispose_from_old_map();
  hs_runtime_initialize_for_new_map();
  main_menu_load_pending = false;
}

/*
 * main_roll_credits (0x102070)
 *
 * End-of-campaign hook. Emits an informational error-log line, drops back to
 * the main menu, then hands off to the event-manager reset at 0xdc110.
 *
 * Binary shape (6 instructions, no frame):
 *   PUSH 0x28b68c / PUSH 2 / CALL error / ADD ESP,8  -> error(2, msg)
 *   CALL 0x101fe0                                    -> main_menu_load()
 *   JMP  0x000dc110                                  -> tail call
 * The trailing JMP is a tail call, written here as a plain final statement.
 */
void main_roll_credits(void)
{
  error(2, "congratulations, you won the game!");
  main_menu_load();
  FUN_000dc110();
}

void main_pregame_render(void)
{
  vector3_t unk[3];

  collision_log_continue_period(1);
  sound_render();

  unk[2].x = 0;
  unk[2].y = 0;
  unk[2].z = 0;
  pregame_render_info.cam1.unk_0 = unk[2];

  unk[1].x = 0;
  unk[1].y = 0;
  unk[1].z = 1.0;
  pregame_render_info.cam1.unk_12 = unk[1];

  pregame_render_info.unk_0 = -1;
  pregame_render_info.unk_2 = 1;

  unk[0].x = 0;
  unk[0].y = 1.0;
  unk[0].z = 0;
  pregame_render_info.cam1.unk_24 = unk[0];

  pregame_render_info.cam1.unk_36 = 0;
  pregame_render_info.cam1.vertical_field_of_view =
    2 *
    atan2(render_camera_get_adjusted_field_of_view_tangent(1.3962634) * 0.75,
          1.0);
  compute_window_bounds(0, 1, &pregame_render_info.cam1.viewport_bounds,
                        &pregame_render_info.cam1.unk_52);
  pregame_render_info.cam1.z_near = 0.0099999998;
  pregame_render_info.cam1.z_far = 1.0;
  qmemcpy(&pregame_render_info.cam0, &pregame_render_info.cam1,
          sizeof(pregame_render_info.cam0));
  render_frame_pregame(&pregame_render_info, main_globals_movie);
  collision_log_end_period();
}

/*
 * set_window_camera_values - 0x1021c0
 *
 * Populates the camera fields of a window_t struct (starting at offset 0x58)
 * from either an observer camera (a3 != NULL) or default global camera
 * pointers when a3 is NULL.
 *
 * Confirmed:
 *  - window param in EDI (stack arg [EBP+8]), camera param in EBX ([EBP+0xc]).
 *  - Copies three 12-byte vectors (position, forward, up) into window+0x58,
 *    window+0x64, window+0x70 respectively.
 *  - Computes vertical_field_of_view = 2 * atan2(tan(fov_half) * scale, 1.0)
 *    and stores at window+0x80.
 *  - When a3 != NULL and window->player != -1, and neither ff4c0 nor
 *    game_time_get_paused returns true, and object type != 3: applies a
 *    matrix transform from the player's object matrix to the camera vectors.
 *  - Sets window+0x7c (unk byte) to 0.
 *  - Copies globals at 0x325694/0x325698 to window+0x94/0x98.
 *  - If *(byte*)0x5aa255 == 0: copies 0x54 bytes (21 dwords) from the
 *    camera area (window+0x58) back to window+0x04 (the previous-frame
 *    camera snapshot), using REP MOVSD.
 *
 * Inferred:
 *  - 0x31fc1c, 0x31fc3c, 0x31fc44 are global pointers to default camera
 *    position, forward, and up vectors (used when no observer is active).
 *  - 0x25afcc is a float scale factor (0.75) for FOV tangent.
 *  - 0x2573d8 is double 1.0 (used as atan2 denominator).
 *  - 0x186460 is render_camera_get_adjusted_field_of_view_tangent (tan of
 *    half FOV).
 *  - 0xff4c0 is likely "game_in_editor" or similar predicate.
 *  - 0xa3370 is object_get_world_matrix (extracts a 4x3 matrix for a datum).
 *  - 0x10a110 builds a 4x3 matrix from position/forward/up vectors.
 *  - 0x109850 is matrix4x3_multiply.
 *  - 0x109540 decomposes a 4x3 matrix back into position/forward/up.
 *  - 0x5aa255 is a "first frame" or "camera not yet initialized" flag.
 */
void set_window_camera_values(void *window, float *a3)
{
  char *win = (char *)window;
  float *dest_pos = (float *)(win + 0x58);
  float *dest_fwd = (float *)(win + 0x64);
  float *dest_up = (float *)(win + 0x70);

  typedef double(__cdecl * fn_tan_fov_t)(float half_fov);
  typedef bool(__cdecl * fn_in_editor_t)(void);
  typedef int16_t(__cdecl * fn_object_type_t)(uint16_t datum);
  typedef void(__cdecl * fn_get_matrix_t)(uint16_t datum, float *out);
  typedef void(__cdecl * fn_build_matrix_t)(float *out, float *pos, float *fwd,
                                            float *up);
  typedef void(__cdecl * fn_mul_matrix_t)(float *a, float *b, float *out);
  typedef void(__cdecl * fn_decompose_t)(float *mat, float *pos, float *fwd,
                                         float *up);

  if (a3 != NULL) {
    /* copy position (a3+0x00), forward (a3+0x20), up (a3+0x2c) */
    dest_pos[0] = a3[0];
    dest_pos[1] = a3[1];
    dest_pos[2] = a3[2];
    dest_fwd[0] = a3[8]; /* offset 0x20 / 4 = 8 */
    dest_fwd[1] = a3[9];
    dest_fwd[2] = a3[10];
    dest_up[0] = a3[11]; /* offset 0x2c / 4 = 11 */
    dest_up[1] = a3[12];
    dest_up[2] = a3[13];

    /* vertical_field_of_view = 2 * atan2(tan(a3[0xe]) * scale, 1.0) */
    {
      double t = ((fn_tan_fov_t)0x186460)(a3[14]);
      double scaled = t * (double)*(float *)0x25afcc;
      double angle = atan2(scaled, *(double *)0x2573d8);
      *(float *)(win + 0x80) = (float)(angle + angle);
    }

    /* apply object matrix transform if player is valid and not in editor
     * or paused, and object type != 3 */
    if (*(int16_t *)win != -1) {
      if (!((fn_in_editor_t)0xff4c0)()) {
        if (!game_time_get_paused()) {
          uint16_t player_datum = *(uint16_t *)win;
          int16_t obj_type = ((fn_object_type_t)0x86410)(player_datum);
          if (obj_type != 3) {
            float obj_matrix[13]; /* 4x3 matrix = 52 bytes */
            float cam_matrix[13];
            ((fn_get_matrix_t)0xa3370)(player_datum, obj_matrix);
            ((fn_build_matrix_t)0x10a110)(cam_matrix, a3, a3 + 8, a3 + 11);
            ((fn_mul_matrix_t)0x109850)(cam_matrix, obj_matrix, cam_matrix);
            ((fn_decompose_t)0x109540)(cam_matrix, dest_pos, dest_fwd, dest_up);
          }
        }
      }
    }
  } else {
    /* no observer camera: use global default camera pointers */
    {
      float *src = *(float **)0x31fc1c;
      dest_pos[0] = src[0];
      dest_pos[1] = src[1];
      dest_pos[2] = src[2];
    }
    {
      float *src = *(float **)0x31fc3c;
      dest_fwd[0] = src[0];
      dest_fwd[1] = src[1];
      dest_fwd[2] = src[2];
    }
    {
      float *src = *(float **)0x31fc44;
      dest_up[0] = src[0];
      dest_up[1] = src[1];
      dest_up[2] = src[2];
    }

    /* default FOV: tan(1.3962634) * scale, doubled atan2 */
    {
      double t = ((fn_tan_fov_t)0x186460)(1.3962634f);
      double scaled = t * (double)*(float *)0x25afcc;
      double angle = atan2(scaled, *(double *)0x2573d8);
      *(float *)(win + 0x80) = (float)(angle + angle);
    }
  }

  /* clear unk byte at offset 0x7c */
  *(uint8_t *)(win + 0x7c) = 0;

  /* copy timing globals */
  *(uint32_t *)(win + 0x94) = *(uint32_t *)0x325694;
  *(uint32_t *)(win + 0x98) = *(uint32_t *)0x325698;

  /* if 0x5aa255 is clear, snapshot camera to previous-frame area */
  if (*(uint8_t *)0x5aa255 == 0) {
    qmemcpy(win + 0x04, win + 0x58, 0x54);
  }
}

void main_present_frame(void)
{
  const char *err_msg;
  char path[512];
  file_ref_t file_ref;

  render_frame_present(0, main_globals_movie);

  if (global_screenshot_count <= 0 && main_globals_movie) {
    snprintf(path, sizeof(path), "movie\\frame%06d.tga", movie_frame_count++);
    file_reference_create_from_path(&file_ref, path, 0);
    err_msg = tiff_export(&file_ref, main_globals_movie);
    if (err_msg) {
      error(2, err_msg);
    }
  }
}

void main_setup_connection(void)
{
  game_options_t game_options;

  if (byte_46DA45) {
    main_menu_load_pending = 0;
    word_46DA0C = 3;
    error(2, "error opening saved film");
    main_menu_load_pending = 1;
  }

  if (main_menu_load_pending) {
    main_menu_load();
    return;
  }

  word_46DA0C = 0;
  game_options_new(&game_options);
  csstrncpy(game_options.map_name, map_name, sizeof(game_options.map_name) - 1);
  game_options.map_name[sizeof(game_options.map_name) - 1] = 0;
  game_options.difficulty = global_difficulty_level;
  game_precache_new_map(game_options.map_name, 1);
  game_dispose_from_old_map();
  main_new_map(&game_options);
}

void main_initialize_time(void)
{
  /* d3d_find_flipcount compares the stored callback pointer against the
   * original XBE address.  The forward thunk for our ported C function
   * lives at a different address, so we must pass the raw original
   * address here.  The reverse thunk at 0x101cd0 redirects into our
   * ported main_vertical_blank_interrupt_handler. */
#define VBLANK_HANDLER_ADDR (void *)0x101CD0

  unk_time_globals.unk_0 = system_milliseconds();
  unk_time_globals.unk_8 = 0L;
  rasterizer_set_vblank_callback(VBLANK_HANDLER_ADDR);
  word_46DDDC = 0;
  csmemset(word_46DDDE, 0, 0x1Eu);
  flip_count_ptr = d3d_find_flipcount();
#undef VBLANK_HANDLER_ADDR
}

/*
 * screenshot_render - 0x102510
 *
 * Renders and saves multi-resolution screenshots. Takes the window array via
 * EDI (register arg). Clamps the screenshot multiplier (int16 at 0x31fa98)
 * to [1, 3], creates a scaled bitmap via bitmap_2d_new (0x7e0b0), renders
 * each tile of each screenshot frame, saves each as a numbered TIF file,
 * then deletes the bitmap.
 *
 * Confirmed:
 *  - Register arg: EDI = window pointer (void *a1@<edi>).
 *  - Clamp logic: if multiplier < 1, set 1; if > 3, set 3; else keep.
 *  - Bitmap created with scaled screen dimensions * multiplier.
 *  - Nested loop: for each of global_screenshot_count x global_screenshot_count
 *    outer frames, for each multiplier x multiplier inner tiles.
 *  - When global_screenshot_count < 2 AND multiplier < 2: single-shot mode
 *    (render_frame gets NULL tile coords, render_frame_present gets NULL).
 *  - Otherwise: render_frame and render_frame_present get tile coordinate
 *    pointers.
 *  - File format: "%dscreenshot%d%d.tif" with (screenshot_index, row, col).
 *  - After all frames: increments screenshot_index (0x46da0e), calls
 *    bitmap_delete (0x7c8f0).
 *  - Clears global_screenshot_count to 0 at exit.
 */
void screenshot_render(void *a1)
{
  int16_t multiplier;
  void *bitmap;
  int16_t outer_row, outer_col;
  int16_t inner_row, inner_col;
  char path[512];
  file_ref_t file_ref;
  int16_t tile_coords[4]; /* local_8, local_6, local_4, local_2 */
  const char *err_msg;

  typedef void *(__cdecl * fn_bitmap_new_t)(int width, int height, int unk,
                                            int depth);
  typedef void(__cdecl * fn_render_frame_t)(
    void *win, int16_t count, int16_t *a4, int16_t *a5, void *bitmap, float a7);
  typedef void(__cdecl * fn_render_present_t)(int16_t * a1, void *bitmap);
  typedef const char *(__cdecl * fn_tiff_export_t)(file_ref_t * info,
                                                   void *bitmap);

  /* clamp multiplier to [1, 3] */
  multiplier = *(int16_t *)0x31fa98;
  if (multiplier < 1) {
    *(int16_t *)0x31fa98 = 1;
  } else if (multiplier > 3) {
    *(int16_t *)0x31fa98 = 3;
  }

  /* create scaled bitmap */
  {
    int16_t scr_x0 = *(int16_t *)0x325654;
    int16_t scr_x1 = *(int16_t *)0x325658;
    int16_t scr_y0 = *(int16_t *)0x325656;
    int16_t scr_y1 = *(int16_t *)0x32565a;
    int w = *(int16_t *)0x31fa98 * (scr_x1 - scr_x0);
    int h = *(int16_t *)0x31fa98 * (scr_y1 - scr_y0);
    bitmap = ((fn_bitmap_new_t)0x7e0b0)(w, h, 0, 10);
  }

  if (bitmap == NULL || *(int *)((char *)bitmap + 0x2c) == 0) {
    goto done;
  }

  console_printf(1, "");
  console_flush();

  for (outer_row = 0; outer_row < global_screenshot_count; outer_row++) {
    for (outer_col = 0; outer_col < global_screenshot_count; outer_col++) {
      for (inner_row = 0; inner_row < *(int16_t *)0x31fa98; inner_row++) {
        for (inner_col = 0; inner_col < *(int16_t *)0x31fa98; inner_col++) {
          tile_coords[0] = inner_col; /* local_8: x tile */
          tile_coords[1] = inner_row; /* local_6: y tile */
          tile_coords[2] = outer_col; /* local_4: outer x */
          tile_coords[3] = outer_row; /* local_2: outer y */

          if (global_screenshot_count < 2 && *(int16_t *)0x31fa98 < 2) {
            /* single-shot: no tile coordinates */
            ((fn_render_frame_t)0x185680)(a1, 1, NULL, NULL, bitmap, 0.0f);
            ((fn_render_present_t)0x184dc0)(NULL, bitmap);
          } else {
            ((fn_render_frame_t)0x185680)(a1, 1, &tile_coords[2],
                                          &tile_coords[0], bitmap, 0.0f);
            ((fn_render_present_t)0x184dc0)(&tile_coords[0], bitmap);
          }
        }
      }

      /* save TIF file */
      crt_sprintf(path, "%dscreenshot%d%d.tif", (int)*(uint16_t *)0x46da0e,
                  (int)outer_row, (int)outer_col);
      file_reference_create_from_path(&file_ref, path, 0);
      err_msg = ((fn_tiff_export_t)0x7f5e0)(&file_ref, bitmap);
      if (err_msg != NULL) {
        error(2, err_msg);
      }
    }
  }

  *(int16_t *)0x46da0e = *(int16_t *)0x46da0e + 1;
  bitmap_delete(bitmap);

done:
  global_screenshot_count = 0;
}

/*
 * main_framerate_render - 0x102700
 *
 * Draws up to three debug overlays in the lower-right corner of the screen,
 * each gated by its own console/debug byte flag and each requiring the
 * interface-globals font tag at *(int*)(*(int*)0x46bd0c + 0x54) to be valid
 * (!= -1, the tag "none" sentinel).
 *
 * Confirmed:
 *  - No arguments; void return. cdecl, frame `sub esp,0x1c`, saves EBX/ESI/EDI.
 *    EBX is zeroed in the prologue and used as the literal 0 for every
 *    NULL/0 push and as the compare operand for the three byte flags.
 *  - Overlay 1 (flag 0x46e004, framerate): fps = 1.0f / max(frame_seconds,
 *    0.01f), where frame_seconds is flt_46DA08 (the same current-frame
 *    seconds global main_frame_rate_debug samples). Disasm:
 *    FLD [0x46da08]; FCOMP [0x25bb10 = 0.01f]; TEST AH,0x41; JNE — i.e. the
 *    0.01f branch is taken when frame_seconds <= 0.01f, so the selected value
 *    is max(). Then FLD [0x2533c8 = 1.0f]; FDIV ST(1) => 1.0f / max (numerator
 *    is the 1.0f constant; division direction verified in disasm).
 *    The quotient is stored to a float slot, reloaded, and FISTP'd => (int).
 *  - Overlay 1 alternate: when byte 0x46dd9a != 0 the displayed number is
 *    60 / (int)*(int16_t*)0x46dd96 via MOV EAX,0x3c; CDQ; IDIV — a signed
 *    32-bit divide (not the 64-bit divide Ghidra's `(longlong)` suggests).
 *    0x46dd96 is the "current frame divisor" main_set_frame_rate maintains,
 *    so this path prints the locked tick rate instead of the measured rate.
 *  - Overlay 1 color: CMP SI,0x1e; JGE keeps *(void**)0x2ee6d4, otherwise
 *    *(void**)0x2ee6d0 — i.e. (short)value >= 30 ? normal : warning color.
 *  - Overlay 2 (flag 0x46e005): walks a 15-entry int16 ring buffer at
 *    0x46ddde backwards from the head index at *(int16_t*)0x46dddc:
 *    index = (head + 14) % 15, stepping by the same (+14 % 15) until the
 *    head is reached again (do/while, so an already-equal first index skips
 *    the loop entirely). Each entry is printed on its own line, the rect
 *    top/bottom both moving up 0x14 per line. Entry value 2 uses
 *    *(void**)0x2ee6c4, anything else *(void**)0x2ee6d0.
 *  - Overlay 3 (flag 0x46e006): only when cache_files_precache_in_progress()
 *    and cache_files_precache_map_status(&progress) returns 0. Prints
 *    (int)(progress * 100.0f [0x253f00]) as a percentage.
 *  - Screen rect: the 8 bytes at 0x506584/0x506588 are copied as two dwords
 *    into a 4 x int16 rectangle2d {top,left,bottom,right}; overlays 1 and 2
 *    then set top = bottom - 0x32 and left = right - 0x32, overlay 3 sets
 *    left = right - 0x32 and top = bottom - 0x64. The -0x32/-0x14/-0x64
 *    adjustments are 32-bit adds of a dword load with only the low 16 bits
 *    stored back (MSVC's short-arithmetic idiom), confirmed at 0x10279f,
 *    0x102861 and 0x10294e.
 *  - Text buffer: snprintf(buf, 3, "%d", v) followed by an explicit
 *    buf[3] = 0 (the `& 0xffffff` Ghidra shows for overlay 2 is that NUL
 *    store into a 4-byte char buffer, not float bit masking). Format string
 *    "%d" is at 0x25acb8.
 *  - Per-overlay call order differs and is preserved verbatim: overlays 1
 *    and 3 do set_style -> set_color -> set_font -> text_draw, overlay 2 does
 *    set_style -> set_font -> set_color -> text_draw (0x102881, 0x102887,
 *    0x1028a5).
 *  - 0x19b7e0 takes one stack argument at all five original call sites
 *    (verified with dump_caller_regsetup.py); its body is
 *    `tag_get('font', arg); *(int*)0x4d9b0c = arg;` and it returns with a
 *    plain RET, so it is cdecl with a single int tag-index parameter. Its
 *    kb.json decl was `void(void)`, which would have dropped the argument.
 *  - The interface-globals pointer at 0x46bd0c is re-read at the top of each
 *    overlay in the original; not hoisted here either.
 *
 * Inferred:
 *  - 0x46e004/0x46e005/0x46e006 are three separate debug-render toggles
 *    (framerate, frame-time history, precache progress).
 *  - 0x2ee6d0 is a "bad/warning" text color shared by overlays 1 and 2;
 *    0x2ee6d4, 0x2ee6c4 and 0x2ee6f4 are the respective normal colors.
 *
 * Uncertain:
 *  - The meaning of the int16 ring-buffer entries in overlay 2 (only the
 *    special value 2 is distinguished) and the exact semantic names of the
 *    three toggles.
 *  - The original frame is 0x1c bytes with a dead 4-byte slot at EBP-0x14
 *    and several stack slots shared between overlays (the float at EBP-8
 *    serves as fps, as overlay 2's text buffer and as overlay 3's out
 *    parameter). That sharing is MSVC slot coalescing of disjoint lifetimes
 *    and is not reproduced field-for-field here.
 */
void main_framerate_render(void)
{
  int16_t bounds[4]; /* rectangle2d: top, left, bottom, right */
  int percent;
  int frame_rate;
  float scaled;
  float value;
  char string[4];
  int font_tag;
  int displayed;
  int index;
  const void *color;

  font_tag = -1;

  /* Overlay 1: measured (or locked) frame rate. */
  if (*(char *)0x46e004 != 0 &&
      (font_tag = *(int *)(*(int *)0x46bd0c + 0x54)) != -1) {
    *(int *)&bounds[0] = *(int *)0x506584;
    *(int *)&bounds[2] = *(int *)0x506588;

    value = *(float *)0x46da08 > 0.01f ? *(float *)0x46da08 : 0.01f;
    value = 1.0f / value;
    frame_rate = (int)value;

    displayed = frame_rate;
    if (*(char *)0x46dd9a != 0) {
      displayed = 60 / (int)*(int16_t *)0x46dd96;
    }

    snprintf(string, 3, "%d", (int)(int16_t)displayed);
    string[3] = 0;

    bounds[1] = (int16_t)(bounds[3] - 0x32);
    bounds[0] = (int16_t)(bounds[2] - 0x32);

    draw_string_set_style_justify_flags(-1, 0, 0);

    color = *(const void **)0x2ee6d4;
    if ((int16_t)displayed < 30) {
      color = *(const void **)0x2ee6d0;
    }
    draw_string_set_color(color);
    draw_string_set_font_tag(font_tag);
    rasterizer_text_draw(bounds, NULL, NULL, 0, string);
  }

  /* Overlay 2: int16 ring buffer at 0x46ddde, newest line at the bottom. */
  if (*(char *)0x46e005 != 0 &&
      (font_tag = *(int *)(*(int *)0x46bd0c + 0x54)) != -1) {
    *(int *)&bounds[0] = *(int *)0x506584;
    *(int *)&bounds[2] = *(int *)0x506588;
    bounds[0] = (int16_t)(bounds[2] - 0x32);
    bounds[1] = (int16_t)(bounds[3] - 0x32);

    index = (int16_t)(((int)*(int16_t *)0x46dddc + 14) % 15);
    if ((int16_t)index != *(int16_t *)0x46dddc) {
      do {
        bounds[0] = (int16_t)(bounds[0] - 0x14);
        bounds[2] = (int16_t)(bounds[2] - 0x14);

        snprintf(string, 3, "%d",
                 (int)*(int16_t *)(0x46ddde + (int16_t)index * 2));
        string[3] = 0;

        draw_string_set_style_justify_flags(-1, 1, 0);
        draw_string_set_font_tag(font_tag);

        color = *(const void **)0x2ee6c4;
        if (*(int16_t *)(0x46ddde + (int16_t)index * 2) != 2) {
          color = *(const void **)0x2ee6d0;
        }
        draw_string_set_color(color);
        rasterizer_text_draw(bounds, NULL, NULL, 0, string);

        index = (int16_t)(((int16_t)index + 14) % 15);
      } while ((int16_t)index != *(int16_t *)0x46dddc);
    }
  }

  /* Overlay 3: map precache progress percentage. */
  if (*(char *)0x46e006 != 0 && cache_files_precache_in_progress() &&
      (font_tag = *(int *)(*(int *)0x46bd0c + 0x54)) != -1 &&
      cache_files_precache_map_status(&value) == 0) {
    scaled = value * 100.0f;
    *(int *)&bounds[0] = *(int *)0x506584;
    *(int *)&bounds[2] = *(int *)0x506588;
    percent = (int)scaled;

    snprintf(string, 3, "%d", (int)(int16_t)percent);
    string[3] = 0;

    bounds[1] = (int16_t)(bounds[3] - 0x32);
    bounds[0] = (int16_t)(bounds[2] - 0x64);

    draw_string_set_style_justify_flags(-1, 0, 0);
    draw_string_set_color(*(const void **)0x2ee6f4);
    draw_string_set_font_tag(font_tag);
    rasterizer_text_draw(bounds, NULL, NULL, 0, string);
  }
}

/*
 * halt_and_catch_fire (0x1029a0) — fatal-error "bluescreen" renderer.
 * Entered after an unrecoverable engine fault. Guards against re-entrant
 * invocation with an INT3 breakpoint, then silences rumble on every
 * connected gamepad and resolves the interface (or fallback system) font.
 * Loops forever: rebuilds a throwaway camera/frustum from the default
 * camera globals, mirrors it into the shared render camera, begins the
 * rasterizer window, draws the build version string and the last
 * recorded error message, then presents the frame. Never returns.
 */
void halt_and_catch_fire(void)
{
  void *had_interface;
  int16_t gamepad_index;
  char has_gamepad;
  int tag_index;
  window_parameters_t window_params;
  float frame_begin_buf[2];
  int32_t screen_pos[2];
  uint8_t text_color[6];
  float *default_pos;
  float *default_fwd;
  float *default_up;
  float *frustum_extra;
  double t;
  double scaled;
  double angle;
  const void *default_color;
  void *error_msg;

  if (*(char *)0x46e392 != 0) {
    FUN_001d980b(0);
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { int 3 }
#else
    __asm__ volatile("int3");
#endif
    return;
  }

  had_interface = FUN_0018e3b0();
  *(char *)0x46e392 = 1;

  for (gamepad_index = 0; gamepad_index < 4; gamepad_index++) {
    has_gamepad = input_has_gamepad(gamepad_index);
    if (has_gamepad != 0) {
      input_set_rumble(gamepad_index, 0, 0);
    }
  }

  tag_index = -1;
  if (had_interface != NULL) {
    tag_index = interface_get_tag_index(1);
  }
  if (tag_index == -1) {
    tag_index = tag_loaded(0x666f6e74, "old tags\\internal system plain");
  }

  for (;;) {
    _rasterizer_reset_state();
    csmemset(frame_begin_buf, 0, sizeof(frame_begin_buf));
    rasterizer_frame_begin(frame_begin_buf);
    _rasterizer_windows_begin();

    csmemset(&window_params, 0, sizeof(window_params));

    /* Default camera position/forward/up: bare PTR_DAT globals hold the
     * pointer VALUE to a live 3-float vector each (same idiom already
     * established above for 0x31fc1c/0x31fc3c/0x31fc44). */
    default_pos = *(float **)0x31fc1c;
    window_params.camera.unk_0.x = default_pos[0];
    window_params.camera.unk_0.y = default_pos[1];
    window_params.camera.unk_0.z = default_pos[2];

    default_fwd = *(float **)0x31fc3c;
    window_params.camera.unk_12.x = default_fwd[0];
    window_params.camera.unk_12.y = default_fwd[1];
    window_params.camera.unk_12.z = default_fwd[2];

    default_up = *(float **)0x31fc44;
    window_params.camera.unk_24.x = default_up[0];
    window_params.camera.unk_24.y = default_up[1];
    window_params.camera.unk_24.z = default_up[2];

    window_params.camera.unk_36 = 0;

    window_params.camera.viewport_bounds.y0 = 0;
    window_params.camera.viewport_bounds.x0 = 0;
    window_params.camera.viewport_bounds.y1 = 0x1e0;
    window_params.camera.viewport_bounds.x1 = 0x280;

    /* Bit-exact dword copy, not an int->float conversion: the original
     * stores a raw undefined4 into these float fields. */
    window_params.camera.z_near = *(float *)0x325694;
    window_params.camera.z_far = *(float *)0x325698;

    t = render_camera_get_adjusted_field_of_view_tangent(1.3962634f);
    scaled = t * (double)*(float *)0x25afcc;
    angle = atan2(scaled, *(double *)0x2573d8);
    window_params.camera.vertical_field_of_view = (float)(angle + angle);

    render_camera_build_frustum(&window_params.camera, 0, window_params.frustum,
                                1);

    window_params.unk_0[0] = 0;

    frustum_extra = *(float **)0x2ee71c;
    window_params.frustum[100] = frustum_extra[0];
    window_params.frustum[101] = frustum_extra[1];
    window_params.frustum[102] = frustum_extra[2];
    window_params.frustum[104] = 0.0f;
    window_params.frustum[105] = 0.0f;
    *(int16_t *)&window_params.frustum[106] = 0;

    /* Mirror into the shared render camera used elsewhere in the renderer. */
    qmemcpy(&unknown_global_camera, &window_params.camera, sizeof(camera_t));

    rasterizer_window_begin(&window_params);

    if (tag_index != -1) {
      screen_pos[0] = *(int32_t *)0x32565c;
      screen_pos[1] = *(int32_t *)0x325660;

      /* text_color is not a color: rasterizer_text_draw's 3rd parameter is
       * an OUT cursor -- draw_string (0x19c5d0) ends with `*param_3 =
       * CONCAT22(line_y, x_end)`, writing the end-of-text position into
       * bytes 0..3. The original zeroes only bytes 0..3 (two word stores
       * of BX at [EBP-0x8]/[EBP-0x6]); bytes 4..5 stay uninitialized and
       * are only ever read into the discarded high word of a dword load. */
      *(int16_t *)&text_color[0] = 0;
      *(int16_t *)&text_color[2] = 0;

      default_color = *(const void **)0x2ee6c4;
      draw_string_set_font(tag_index, -1, 0, 0, default_color);
      draw_string_set_tab_stops(0, 0);
      draw_string_set_color(default_color);
      rasterizer_text_draw(
        screen_pos, 0, text_color, -4,
        "halobeta xbox 01.10.12.2276 built at: Oct 12 2001 16:07:48");

      /* Original (0x102bd5): MOV EDX,[EBP-0x6]; DEC EDX; MOV [EBP-0x14],DX.
       * [EBP-0x6] bytes 0..1 are the HIGH word of the cursor dword the
       * first draw wrote at [EBP-0x8] -- the final line Y. The error
       * message is drawn starting from that line; only the low word of
       * screen_pos[0] is stored (high word untouched, matching the
       * original word store). */
      *(int16_t *)&screen_pos[0] = (int16_t)(*(int32_t *)(text_color + 2) - 1);

      error_msg = error_get();
      rasterizer_text_draw(screen_pos, 0, text_color, -4,
                           (const char *)error_msg);
    }

    FUN_00184980(1);
    FUN_00184980(0);
    FUN_0017e190();
    FUN_00158f90();
    _rasterizer_windows_end();
    _rasterizer_frame_end();
    /* Original calls the thunk at 0x17c930 (jmps to 0x157e40) with two null
     * args -- rasterizer_dynamic_lit_geometry_draw, NOT render_frame_present
     * (0x184dc0). Both args are used by the callee (edi=[ebp+8], esi=[ebp+c]);
     * (0,0) selects its null/no-geometry path. */
    rasterizer_dynamic_lit_geometry_draw(0, 0);
    input_update();
  }
}

/*
 * main_halt_entry — infinite render loop entered after a fatal halt.
 * Continuously processes input, shell idle, event manager, telnet console,
 * UI widgets, pregame rendering, rasterizer throttle, and frame presentation.
 * This keeps the screen alive (e.g. showing an error overlay) even though the
 * game simulation has stopped.  Never returns.
 */
void __noreturn main_halt_entry(void)
{
  for (;;) {
    input_frame_begin();
    input_update();
    shell_idle();
    event_manager_update();
    telnet_console_process();
    process_ui_widgets();
    main_pregame_render();
    main_rasterizer_throttle();
    main_present_frame();
    input_frame_end();
  }
}

void main_game_render(double a2)
{
  bool force_single_screen;
  int player_index;
  window_t *current_window;
  void *camera;
  int num_players;
  int num_screens;
  __int16 next_player;

  lock_global_random_seed();
  collision_log_continue_period(1);
  sound_render();

  force_single_screen = game_engine_force_single_screen();
  next_player = -1;
  num_screens = CLAMP(local_player_count(), 1, 4);
  num_players = num_screens;

  if (force_single_screen || cinematic_in_progress()) {
    num_screens = 1;
    num_players = 1;
  }

  for (player_index = 0; player_index < num_players; player_index++) {
    current_window = &window[player_index];
    camera = NULL;

    compute_window_bounds(player_index, num_players, &current_window->unk_132,
                          &current_window->unk_140);

    if (!force_single_screen && player_index < num_screens) {
      if (!byte_325714 || next_player == -1) {
        if (word_46DA0C == 3) {
          next_player = 0;
        } else {
          next_player = local_player_get_next(next_player);
        }
      }
      current_window->player = next_player;
      camera = observer_get_camera(next_player);
    } else {
      current_window->player = -1;
    }

    set_window_camera_values(current_window, camera);
    current_window->unk_2 = 0;
  }

  current_window = &window[num_players];
  compute_window_bounds(0, 1, &current_window->unk_132,
                        &current_window->unk_140);
  current_window->player = -1;
  current_window->unk_2 = 1;
  set_window_camera_values(current_window, 0);

  if (global_screenshot_count <= 0) {
    render_frame(window, num_players + 1, 0, 0, main_globals_movie, a2);
  } else {
    screenshot_render(window);
  }
  collision_log_end_period();
  unlock_global_random_seed();
}

#ifdef DECOMP_CUSTOM
static void print_startup_banner(void)
{
  error(2, "DECOMP BUILD %s (%s)", build_rev, build_date);
  error(2, "--------------------------------------------------------------");
}
#endif

static __inline void abort_with_error_message(int16_t message_id)
{
  display_error_when_main_menu_loaded(message_id);
  error(2, "the game host went down");
  network_game_abort();
}

void main_loop(void)
{
  bool v0; // cc
  bool v1; // bl
  float a2; // [esp+4h] [ebp-14h]
  float a2a; // [esp+4h] [ebp-14h]
  float a2b; // [esp+4h] [ebp-14h]
  float a2_4; // [esp+8h] [ebp-10h]
  float a2_4a; // [esp+8h] [ebp-10h]
  char v9[4]; // [esp+10h] [ebp-8h] BYREF
  int x;
#ifdef DECOMP_CUSTOM
  /* die-to-core debug hook enable flag; armed from the d:\die_to_core.xts
   * sentinel at startup below (mirrors the recorder *.xts sentinel checks). */
  int die_to_core_enabled = 0;
#endif

  if (!game_in_editor()) {
    csstrncpy(map_name, "levels\\b30\\b30", 0xFFu);
    byte_46DB54 = 0;
  }
  main_menu_load_pending = game_in_editor() == 0;
  word_46DA40 = -1;
  byte_46DA46 = 1;
  console_initialize();
  debug_keys_initialize();
  game_initialize();
  console_startup();

#ifdef DECOMP_CUSTOM
  print_startup_banner();
  /* Arm the die-to-core debug hook if the sentinel exists (same file-attribute
   * probe the input recorder uses for write/read/loop.xts). */
  die_to_core_enabled = (file_get_full_attributes("d:\\die_to_core.xts") != -1);
#endif

  main_setup_connection();
  main_initialize_time();
  while (1) {
    if (!game_in_editor()) {
      if (word_46DA40 != -1) {
        scenario_switch_structure_bsp(word_46DA40);
        word_46DA40 = -1;
        hud_load(0);
      }
      if (byte_46DA3B) {
        if (!(unsigned __int8)game_time_get_paused()) {
          v0 = word_46DA4C++ <= 90;
          if (!v0) {
            byte_46DA3B = 0;
            word_46DA4C = 0;
#ifdef DECOMP_CUSTOM
            /* die-to-core debug hook (d:\die_to_core.xts): reload the fixture
             * core instead of the campaign checkpoint, so death-loops don't
             * need a reboot. Reuses the existing load-core dispatch at the
             * bottom of this same loop iteration (game_state_load_core_pending,
             * checked below). Falls through to the faithful revert when the
             * sentinel is absent or no core name is loaded. */
            if (die_to_core_enabled && core_name[0]) {
              game_state_load_core_pending = 1;
            } else
#endif
              game_state_revert();
          }
        }
      }
      if (main_won_map_private_pending) {
        main_won_map_private();
      }
      if (byte_46DA3C) {
        if (!(unsigned __int8)game_time_get_paused() &&
            !cinematic_in_progress()) {
          v0 = word_46DA4E++ <= 90;
          if (!v0) {
            if (players_respawn_coop()) {
              byte_46DA3C = 0;
              word_46DA4E = 0;
            }
          }
        }
      }
      if (game_state_save_pending) {
        game_state_save();
        hud_autosave(0);
        game_state_save_pending = 0;
      }
      if (main_change_map_name_pending) {
        main_change_map_name();
      }
      if (game_state_revert_pending) {
        game_state_revert();
        ui_widgets_disable_pause_game(30);
        game_state_revert_pending = 0;
      }
      if (should_skip_cinematic) {
        if (cinematic_can_be_skipped()) {
          game_state_revert();
          ui_widgets_disable_pause_game(30);
          game_state_revert_pending = 0;
        }
        should_skip_cinematic = 0;
      }
      if (game_reset_pending && !(unsigned __int8)game_time_get_paused()) {
        scenario_switch_structure_bsp(0);
        game_dispose_from_old_map();
        input_flush();
        game_initialize_for_new_map();
        create_local_players();
        game_time_start();
        game_initial_pulse();
        ui_widgets_disable_pause_game(30);
        game_reset_pending = 0;
      }
      if (game_state_save_core_pending) {
        game_state_save_core(core_name);
        game_state_save_core_pending = 0;
      }
      if (game_state_load_core_pending) {
        game_state_load_core(core_name);
        game_state_load_core_pending = 0;
#ifdef DECOMP_CUSTOM
        /* core-loop / die-to-core: re-sync recorded input to the freshly
         * (re)loaded core — rewind playback to packet 0 so the stored input
         * re-executes from the same state. Active only when input playback is
         * on (read.xts / core_loop.xts = mode 4, loop.xts = mode 5). The mode
         * and handle globals live in input_xbox.c (0x46b818 / 0x46b814). */
        if (*(int *)0x46b818 == 4 || *(int *)0x46b818 == 5) {
          SetFilePointer(*(int *)0x46b814, 0, (int *)0, 0);
        }
#endif
      }
      if (main_menu_load_pending) {
        main_menu_load();
      }
      if (main_load_last_solo_map_pending) {
        main_load_last_solo_map();
      }
      if (xbox_demos_launch_pending) {
        xbox_demos_launch_pending = 0;
        xbox_demos_launch();
      }
      if (main_skip_private_pending) {
        main_skip_private();
      }
      if (byte_46DA50) {
        if (cache_files_precache_in_progress() &&
            (unsigned __int16)cache_files_precache_map_status((float *)v9) ==
              1) {
          cache_files_precache_map_end();
        }
        if (!cache_files_precache_in_progress()) {
          cache_files_precache_map_begin(&byte_46DC55, 0);
          byte_46DA50 = 0;
        }
      }
    } else {
      if (game_reset_pending && !(unsigned __int8)game_time_get_paused()) {
        scenario_switch_structure_bsp(0);
        game_dispose_from_old_map();
        input_flush();
        game_initialize_for_new_map();
        create_local_players();
        game_time_start();
        game_initial_pulse();
        ui_widgets_disable_pause_game(30);
        game_reset_pending = 0;
      }
    }
    profile_frame_start();
    input_frame_begin();
    input_update();
    input_abstraction_update();
    shell_idle();
    event_manager_update();
    telnet_console_process();
    if (!shell_application_is_paused()) {
      v1 = 1;
      x = word_46DA0C;
      if (x == 1) {
        if (!network_game_client_start_frame()) {
          abort_with_error_message(6);
        }
      } else if (x == 2) {
        if (!network_game_client_start_frame()) {
          abort_with_error_message(1);
        } else if (!network_game_server_start_frame()) {
          abort_with_error_message(1);
        }
      } else if (x == 3) {
        break;
      }
      main_update_time();
      process_ui_widgets();
      bink_playback_update();
      if ((!game_in_editor() &&
           (input_key_is_down(0x55u) || input_key_is_down(0))) ||
          editor_should_exit()) {
        if (main_globals_movie) {
          bitmap_delete(main_globals_movie);
          main_globals_movie = 0;
        }
        if (!game_engine_running()) {
          word_46DA40 = -1;
          byte_46DA28 = 0;
          game_reset_pending = 1;
          byte_46DA3B = 0;
        }
      }
      if (game_in_progress()) {
        terminal_update();
        if (!console_update() || word_46DA0C) {
          debug_keys_update();
          cheats_update();
          a2 = (double)(unsigned __int8)byte_46DA46;
          a2 *= flt_46DA08;
          player_control_update(a2);
          x = word_46DA0C;
          if (x > 0 && x <= 2 && !network_game_client_end_frame()) {
            display_error_when_main_menu_loaded(1);
            network_game_abort();
          }
          a2a = (double)(unsigned __int8)byte_46DA46;
          a2a *= flt_46DA08;
          game_time_update(a2a);
          v1 = main_globals.main_menu_scenario_loaded ||
               (byte_46DA46 &&
                ((unsigned __int8)game_time_get_paused() ||
                 game_time_get_elapsed() > 0 || game_time_get_speed() < 1.0));

          v1 &= !game_engine_running() || game_time_get() >= 3;

          collision_log_continue_period(1);
          a2b = (double)(unsigned __int8)byte_46DA46;
          a2b *= flt_46DA08;
          director_update(a2b);
          a2_4 = (double)(unsigned __int8)byte_46DA46;
          a2_4 *= flt_46DA08;
          observer_update(a2_4);
          collision_log_end_period();
          a2_4a = (double)(unsigned __int8)byte_46DA46;
          a2_4a *= flt_46DA08;
          game_engine_update_non_deterministic(a2_4a);
        }
        if (byte_46DA28) {
          main_save_map_private();
        }
        if (v1 && !debug_no_drawing) {
          profile_render_start();
          main_game_render(flt_46DA08);
          profile_render_end();
        }
      } else {
        profile_render_start();
        main_pregame_render();
        profile_render_end();
      }
      main_rasterizer_throttle();
      if (v1 && !debug_no_drawing) {
        main_present_frame();
      }
    }
    input_frame_end();
    profile_frame_end();
    main_frame_rate_debug();
    if (byte_46DA47) {
      byte_46DA47 = 0;
      unk_time_globals.unk_0 = system_milliseconds();
      unk_time_globals.unk_8 = qword_325678;
      byte_46DA46 = 1;
    }
  }
  error(2, "end of saved film");
  x = word_46DA0C;
  switch (x) {
  case 2:
    dispose_global_network_game_server();
    dispose_global_network_game_client();
    break;
  case 1:
    dispose_global_network_game_server();
    break;
  }
  game_dispose_from_old_map();
  game_dispose();
  debug_keys_dispose();
  console_dispose();
}

/*
 * FUN_001034b0 - 0x1034b0
 * Initializes the three arrays embedded in a geometry-build context: an array
 * of 0x0c-byte elements at +0x00, an array of 0x1c-byte elements at +0x0c, and
 * an array of 0x18-byte elements at +0x18. Counterpart of the dispose helper
 * FUN_001034e0 below, which frees the same three tables at those same offsets
 * and walks the +0x0c table with a matching stride of 0x1c.
 *
 * Ghidra mis-detected the prototype as void(void); the sole parameter is a
 * normal cdecl stack argument ([EBP+8]), held in ESI across all three calls
 * (the third call reuses ESI after ADD ESI,0x18). No register arguments.
 *
 * MSVC batches the stack cleanup for all three 2-argument calls into a single
 * ADD ESP,0x18 after the last CALL (3 * 8 = 0x18); that is not evidence of a
 * 6-argument call, so array_new's 2-parameter declaration is correct.
 */
void FUN_001034b0(int *context)
{
  array_new(context, 0xc);
  array_new((int *)((char *)context + 0xc), 0x1c);
  array_new((int *)((char *)context + 0x18), 0x18);
}

/*
 * FUN_001034e0 - 0x1034e0
 * Dispose helper for an object carrying three sub-allocations plus an
 * element table. Walks the element table (base at word offset +3 / byte
 * 0xC, signed count at word offset +4 / byte 0x10) and, for each index,
 * resolves the element pointer via the indexer FUN_00117ee0(base, index,
 * stride=0x1c) and frees it with FUN_00117cf0. After the loop it frees
 * three tables: the object itself (+0x0), the element table (+0xC), and a
 * third table (+0x18). Element stride is 28 bytes.
 *
 * Ghidra mis-detected the prototype as void(void); the sole parameter is a
 * normal cdecl stack argument (in_stack_00000004). Pointer arithmetic is in
 * int-word (4-byte) units.
 */
void FUN_001034e0(int *param_1)
{
  int *elem;
  int index;

  index = 0;
  if (0 < param_1[4]) {
    do {
      elem = (int *)FUN_00117ee0(param_1 + 3, index, 0x1c);
      FUN_00117cf0(elem);
      index = index + 1;
    } while (index < param_1[4]);
  }
  FUN_00117cf0(param_1);
  FUN_00117cf0(param_1 + 3);
  FUN_00117cf0(param_1 + 6);
  return;
}
/*
 * main/main_recursive_tree_walk.c — recursive tree/graph DFS marking helper
 * XBE source: c:\halo\SOURCE\main\main.c
 *   (grouped into its own TU for the recursive walk helper)
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x103530  FUN_00103530  — depth-first marking walk over a node graph
 */


/*
 * FUN_00103530 — depth-first walk of a node graph.
 *
 * Looks up node = base+0x18[node_index] (stride 0x18, 6 dwords). Each node
 * holds three child-list references at node[0..2] and a visited/mark flag at
 * node[3] (0xffffffff == unvisited). If the node is unvisited and the optional
 * caller callback (may be NULL) approves it, the node is stamped with `mark`
 * and the walk recurses into every child referenced by node[0..2] via the
 * child-list array at base+0xc (stride 0x1c).
 *
 * ABI: cdecl, 5 stack params. Ghidra mis-typed this as void(void); the true
 * 5-param prototype, the callback's 4-arg char-returning signature (call site
 * @0x103566), and the recursion's 5th argument (@0x1035c9) are reconstructed
 * from the disassembly push sequences, not the decompiler.
 *
 * FUN_00117ee0(array_base, index, elem_size) returns &array[index].
 *
 * The inner child counter is a 16-bit short widened via MOVSX per iteration;
 * preserved here as `short i` / `(int)i` for codegen fidelity.
 */
void FUN_00103530(int base, char (*visit)(uint32_t, int, uint32_t *, uint32_t),
                  uint32_t visit_arg, uint32_t mark, int node_index)
{
  uint32_t *node;
  int *child_list;
  int elem;
  short i;
  int slot;

  node = (uint32_t *)FUN_00117ee0((int *)(base + 0x18), node_index, 0x18);
  if ((node[3] == 0xffffffff) &&
      /* Visitor argument order is fixed by the push sequence at 0x103566:
       * PUSH EDX([EBP+0x14] = mark), PUSH ESI(node), PUSH EBX(base), PUSH
       * ECX([EBP+0x10] = visit_arg) — first push is the last argument, so
       * visit_arg is argument 0 and mark is argument 3. FUN_00103a00 reads
       * argument 0 as a float* plane, so getting this order wrong hands it an
       * integer to dereference. */
      ((visit == NULL) || ((*visit)(visit_arg, base, node, mark) != 0))) {
    node[3] = mark;
    slot = 3;
    do {
      if (*node != 0xffffffff) {
        child_list =
          (int *)FUN_00117ee0((int *)(base + 0xc), *node & 0x7fffffff, 0x1c);
        i = 0;
        if (0 < child_list[1]) {
          elem = 0;
          do {
            FUN_00103530(base, visit, visit_arg, mark,
                         *(int *)FUN_00117ee0(child_list, elem, 4));
            i = i + 1;
            elem = (int)i;
          } while (elem < child_list[1]);
        }
      }
      node = node + 1;
      slot = slot + -1;
    } while (slot != 0);
  }
}

/*
 * FUN_00103600 — find-or-append a point in a 3-float point block, returning
 * its index.
 *
 * Linearly scans `block` (element size 0xc = three floats) for an element
 * whose x, y and z each differ from `point` by less than 0.001. On a hit the
 * loop breaks with the index in EDI. If the scan runs to completion the point
 * is new: FUN_00117da0 appends a slot and the three floats are copied in.
 * A full block (FUN_00117da0 returns -1) yields -1.
 *
 * ABI (Ghidra's `void FUN_00103600(void)` is wrong on all three counts):
 *   - `point` arrives in EBX: FLD [EBX] @0010361b with no prior write to EBX,
 *     and both call sites in FUN_00103860 reload EBX from their own [EBP+0xc]
 *     / [EBP+0x10] immediately before the CALL.
 *   - `block` arrives in ESI: MOV EAX,[ESI+4] @00103604 reads the element
 *     count before ESI is ever written.
 *   - The function RETURNS the index in EAX: MOV EAX,EDI @001036b6, and the
 *     caller consumes it (MOV EDI,EAX @0010389e). Ghidra dropped the return
 *     because EDI is a callee-saved register it tracked as unaffected.
 *
 * The epsilon is the same (double)0.001f constant at 0x2549d8 used by
 * valid_real_normal3d; each component is compared with FABS / FCOMP double.
 * Note the element pointer is re-fetched through FUN_00117ee0 before every
 * component test rather than being held in a local, matching the original's
 * three separate CALLs @00103614 / @00103639 / @00103660.
 *
 * Callees: FUN_00117ee0(array, index, elem_size) -> &array[index];
 *          FUN_00117da0(array) -> new index or -1.
 */
int FUN_00103600(float *point, int *block)
{
  float *element;
  float pending;
  int index;

  index = 0;
  if (block[1] > 0) {
    index = 0;
    do {
      /* Component 0 loads point[0] AFTER the call (FLD [EBX] @00103619 sits
       * directly after CALL @00103614), so the natural expression order is
       * correct here. Components 1 and 2 load point[N] BEFORE the call and
       * spill it to a stack temp across it (FLD [EBX+4] @0010362f, FSTP
       * [EBP-4] @00103635, then FSUBR [EBP-4] @00103641) -- hence the
       * explicit `pending` local. Folding those into one expression makes
       * MSVC evaluate the call first and emit FSUBS instead of FSUBR. */
      element = (float *)FUN_00117ee0(block, index, 0xc);
      if (main_fabs_double_from_float(point[0] - element[0]) < 0.001f) {
        pending = point[1];
        element = (float *)FUN_00117ee0(block, index, 0xc);
        if (main_fabs_double_from_float(pending - element[1]) < 0.001f) {
          pending = point[2];
          element = (float *)FUN_00117ee0(block, index, 0xc);
          if (main_fabs_double_from_float(pending - element[2]) < 0.001f) {
            break;
          }
        }
      }
      index = index + 1;
    } while (index < block[1]);
  }

  if (index == block[1]) {
    index = FUN_00117da0(block);
    if (index != -1) {
      element = (float *)FUN_00117ee0(block, index, 0xc);
      element[0] = point[0];
      element[1] = point[1];
      element[2] = point[2];
    }
  }
  return index;
}

/*
 * FUN_001036c0 — find-or-create the edge entry for an unordered vertex pair
 * and append a value to its list.
 *
 * `table+0xc` is an array header (its element count at table+0x10) of
 * 0x1c-byte edge entries. Each entry holds the pair's two endpoint keys at
 * +0xc/+0x10 and, at its own base, a nested array of 4-byte values.
 *
 * Scans for an entry matching {key_a, key_b} in EITHER orientation. On a
 * forward match (entry+0xc==key_a, entry+0x10==key_b) the orientation flag is
 * set; on a reversed match (entry+0xc==key_b, entry+0x10==key_a) it is
 * cleared. With no match, a new entry is allocated, its nested array
 * initialised with element size 4, and the keys stored in argument order
 * (flag set). `value` is then appended to that entry's nested array.
 *
 * Returns the entry index with bit 31 SET when the pair was matched/stored
 * forward and CLEARED when it was matched reversed, so the caller recovers
 * both the entry and the traversal direction from one int. Returns -1 if any
 * allocation fails.
 *
 * ABI: `table` arrives in EAX (@<eax>) — MOV EBX,EAX @0x1036c5 reads it before
 * any write. Three cdecl stack params follow. Ghidra mis-typed this void(void),
 * so the stack params surfaced as in_stack_00000004/8/c pseudo-locals and all
 * three RET paths as bare `return;` despite returning in EAX.
 *
 * ECX is NOT an input. PUSH ECX @0x1036c3 is MSVC's 4-byte local reservation;
 * Ghidra rendered the reserved dword's high byte as `(char)((uint)in_ECX >>
 * 0x18)` and thereby invented an ECX parameter. Sole call site @0x1038fa
 * confirms it: ECX is loaded from the caller's [EBP-4] and PUSHED as stack arg
 * 1, while EAX is pushed as stack arg 2 at 0x1038f5 and then RELOADED from
 * [EBP+8] at 0x1038f6 to carry the register argument (the push-then-reload
 * hazard).
 *
 * `reversed` is deliberately left uninitialised, matching the original: on the
 * count<0 path ([EBP-1] never written) the original reads whatever the reserved
 * PUSH ECX left there. That path is unreachable in practice — the field is an
 * array element count, never negative.
 *
 * Callees: FUN_00117ee0(array, index, elem_size) -> &array[index];
 *          FUN_00117da0(array) -> new index or -1;
 *          array_new(array, elem_size).
 */
int FUN_001036c0(int *table, int value, int key_a, int key_b)
{
  int *edge;
  int *slot;
  int index;
  int value_index;
  char reversed;

  index = 0;
  if (table[4] > 0) {
    do {
      edge = (int *)FUN_00117ee0(table + 3, index, 0x1c);
      if (edge[3] == key_a && edge[4] == key_b) {
        reversed = 1;
        break;
      }
      if (edge[3] == key_b && edge[4] == key_a) {
        reversed = 0;
        break;
      }
      index = index + 1;
    } while (index < table[4]);
  }
  if (index == table[4]) {
    index = FUN_00117da0(table + 3);
    reversed = 1;
    if (index == -1) {
      return -1;
    }
    edge = (int *)FUN_00117ee0(table + 3, index, 0x1c);
    array_new(edge, 4);
    edge[3] = key_a;
    edge[4] = key_b;
  }
  if (index == -1) {
    return -1;
  }
  edge = (int *)FUN_00117ee0(table + 3, index, 0x1c);
  value_index = FUN_00117da0(edge);
  if (value_index == -1) {
    return -1;
  }
  slot = (int *)FUN_00117ee0(edge, value_index, 4);
  *slot = value;
  if (reversed != 0) {
    index = (int)((unsigned int)index | 0x80000000u);
  } else {
    index = (int)((unsigned int)index & 0x7fffffffu);
  }
  return index;
}

/*
 * FUN_001037b0 — build the plane through three points.
 *
 *   n = cross(p1 - p0, p2 - p0)
 *   len = normalize3d(n)                      (CALL 0x13010 at 0x103819)
 *   if (len == 0.0f) { out[3] = 0; return NULL; }
 *   out[3] = dot(n, p0);  return out;
 *
 * Every subtraction is `FLD [p1|p2 + k]; FSUB [p0 + k]` — the second/third
 * point minus the first, never the reverse.
 *
 * The multiply order inside each cross-product term is transcribed literally
 * from the emitted FMULs (d2 term first for components 0 and 2, d1 term first
 * for component 1). Reversing any of them negates the plane normal, which
 * would silently invert every facing test built on it (FUN_00103a00,
 * FUN_00103c00) — see lift-decompiler-traps trap 4.
 *
 * The dot product for out[3] accumulates component 1 and component 2 first
 * (FLD [EDI+4]; FMUL [ESI+4]; FLD [ESI+8]; FMUL [EDI+8]; FADDP) and only then
 * adds component 0, so the sum is written in that order to keep the x87 block
 * shape. Ghidra printed the three terms with component 0 leading; the emitted
 * FLD order says otherwise and the disassembly wins. Note also that the middle
 * term loads p0 before out while the other two load out first.
 *
 * Both difference vectors are materialised up front, in the order d1.x/y/z then
 * d2.x/y/z: the original holds all three d1 components on the x87 stack for the
 * whole cross product (FMUL %ST(3),%ST) and spills only d2.x and d2.y to
 * -0xc(EBP)/-0x8(EBP), which is why the frame is 0xc and why the two leftover
 * d1 values are dropped with a pair of FSTP %ST(0) before the CALL. Folding the
 * subtractions back into the cross-product expressions makes VC71 recompute
 * them instead (65.3% match).
 *
 * The zero test is `FCOMP dword [0x2533c0]` against a .rdata 0.0f (verified by
 * read_memory), with TEST AH,0x44 / JNP taken on EQUAL — so the equal case is
 * the out-of-line NULL arm and the success arm falls through.
 *
 * ABI: cdecl, four stack args, no register arguments; returns the out pointer
 * in EAX (MOV EAX,EDI) or NULL (XOR EAX,EAX).
 */
float *FUN_001037b0(float *out_plane, float *p0, float *p1, float *p2)
{
  float d1x, d1y, d1z;
  float d2x, d2y, d2z;
  float length;

  d1x = p1[0] - p0[0];
  d1y = p1[1] - p0[1];
  d1z = p1[2] - p0[2];
  d2x = p2[0] - p0[0];
  d2y = p2[1] - p0[1];
  d2z = p2[2] - p0[2];

  out_plane[0] = d2z * d1y - d2y * d1z;
  out_plane[1] = d1z * d2x - d2z * d1x;
  out_plane[2] = d2y * d1x - d2x * d1y;

  length = normalize3d(out_plane);
  if (length != 0.0f) {
    out_plane[3] =
      out_plane[1] * p0[1] + p0[2] * out_plane[2] + out_plane[0] * p0[0];
    return out_plane;
  }
  out_plane[3] = 0.0f;
  return (float *)0;
}

/*
 * FUN_00103860 — append one triangle to the surface block as three interned
 * edges, optionally rejecting it as a duplicate of an existing surface.
 *
 * `base` is the collision-BSP-like builder FUN_00103c00 walks:
 *   base+0x00  vertex block  (stride 0x0c)
 *   base+0x0c  edge block    (stride 0x1c)
 *   base+0x18  surface block (stride 0x18)
 *   base+0x1c  surface element count
 *
 * A new 0x18-byte surface element is allocated from the surface block: three
 * edge references at [0]/[4]/[8], the mark field at [0xc] reset to 0xffffffff
 * and the trailing 8 bytes cleared. The three positions a/b/c are interned as
 * vertices with FUN_00103600 and the three edges (a-b, b-c, c-a) with
 * FUN_001036c0. A failed intern (-1) invalidates the returned index but does
 * NOT abort: the element is still built and the remaining interns still run.
 *
 * When `flag` is set the finished element is compared against every surface
 * added before it (0 .. count-2). Both sides are compared with bit 31 masked
 * off, because that bit is the per-surface edge direction flag (same encoding
 * FUN_00103c00 and FUN_00103a00 decode). If all three edges of the new element
 * are found in one existing element the triangle is a duplicate: it is drawn
 * with FUN_00104040 in the debug colour at *(float **)0x2ee6f0, a one-shot
 * message is printed (guarded by the byte at 0x0046e393) and -1 is returned.
 *
 * ABI: cdecl, 5 stack params, returns the new element index in EAX — or -1.
 * Ghidra printed `void __cdecl FUN_00103860(void)` with every parameter
 * surfacing as in_stack_* and dropped the return entirely; kb.json carried the
 * same wrong `void`. Both returns are explicit in the disassembly: 0x1039ec
 * `MOV EAX,[EBP-4]` and 0x1039ce `OR EAX,0xffffffff`.
 *
 * `flag` is read as a byte (0x103930 `MOV AL,byte ptr [EBP+0x18]`), so it is a
 * one-byte type — even though both original call sites push a full dword
 * (0x103b9a `MOV EDX,[EBP+0x14]` / 0x19478f `PUSH 0`).
 *
 * Register arguments, read from the disassembly and NOT the decompile (which
 * dropped them): the three FUN_00103600 calls take ESI, loaded once at
 * 0x103867 with the RAW `base` — not base+0x18, despite that callee's `block`
 * parameter name — and EBX, reloaded from [EBP+0xc] / [EBP+0x10] / [EBP+0x14]
 * at 0x10388d / 0x10389b / 0x1038a8. FUN_001036c0 takes `base` in EAX
 * (0x1038f6 `MOV EAX,[EBP+8]`); its stack args from the push order at
 * 0x1038f4-0x1038fa (first push = last arg) are (result, verts[j],
 * verts[(j+1)%3]). That modulo is a real IDIV at 0x1038e6 on a counter that
 * starts at 1, so it is written as `% 3` here rather than folded away.
 *
 * The duplicate-scan counters are 16-bit (`MOVSX EDX,SI` / `MOVSX EBX,CX`,
 * `CMP CX,0x3`), hence the `short` locals.
 */
int FUN_00103860(int base, float *a, float *b, float *c, char flag)
{
  int verts[3];
  int *element;
  int i;
  int countdown;
  int result;
  int next;
  int *edge_out;
  int *vert_in;
  int *existing;
  short new_i;
  short existing_i;

  result = FUN_00117da0((int *)(base + 0x18));
  if (result != -1) {
    element = (int *)FUN_00117ee0((int *)(base + 0x18), result, 0x18);

    verts[0] = FUN_00103600(a, (int *)base);
    verts[1] = FUN_00103600(b, (int *)base);
    verts[2] = FUN_00103600(c, (int *)base);
    if (verts[0] == -1 || verts[1] == -1 || verts[2] == -1) {
      result = -1;
    }

    next = 1;
    countdown = 3;
    edge_out = element;
    vert_in = verts;
    do {
      *edge_out = FUN_001036c0((int *)base, result, *vert_in, verts[next % 3]);
      if (*edge_out == -1) {
        result = -1;
      }
      vert_in = vert_in + 1;
      edge_out = edge_out + 1;
      next = next + 1;
      countdown = countdown - 1;
    } while (countdown != 0);

    element[3] = -1;
    csmemset(element + 4, 0, 8);

    if (flag != '\0') {
      i = 0;
      if (*(int *)(base + 0x1c) - 1 > 0) {
        do {
          existing = (int *)FUN_00117ee0((int *)(base + 0x18), i, 0x18);
          new_i = 0;
          for (;;) {
            existing_i = 0;
            do {
              if (((unsigned int)existing[existing_i] & 0x7fffffff) ==
                  ((unsigned int)element[new_i] & 0x7fffffff)) {
                break;
              }
              existing_i = existing_i + 1;
            } while (existing_i < 3);
            if (existing_i == 3) {
              break; /* this edge is not in `existing` — not a duplicate */
            }
            new_i = new_i + 1;
            if (new_i > 2) {
              /* all three edges matched — the triangle already exists */
              FUN_00104040(a, b, c, *(float **)0x2ee6f0);
              if (*(char *)0x0046e393 == '\0') {
                _wprintf((const char *)0x0028b780);
                *(char *)0x0046e393 = '\x01';
              }
              return -1;
            }
          }
          i = i + 1;
        } while (i < *(int *)(base + 0x1c) - 1);
      }
    }
  }
  return result;
}

/*
 * FUN_00103a00 — surface visitor for the FUN_00103530 marking walk: accept a
 * surface only if its three vertices are coplanar with the seed plane and its
 * winding faces the same way as that plane.
 *
 * This is the `visit` callback FUN_00103c00 hands to FUN_00103530, so the
 * parameter list is the walker's fixed visitor prototype
 * (visit_arg, base, node, mark) — see the push sequence at 0x10356a: PUSH
 * EDX([EBP+0x14] = mark), PUSH ESI(node), PUSH EBX(base), PUSH ECX([EBP+0x10]
 * = visit_arg); the first push is the last argument. Here `visit_arg` is the
 * seed plane (four floats: normal + distance) and `node` is the surface whose
 * three dwords are edge references. `mark` is delivered but never read by this
 * visitor (the walker itself writes it into node[3]).
 *
 * Each edge reference carries a direction flag in bit 31: the low 31 bits
 * index the edge block at base+0xc (stride 0x1c) and the flag selects which of
 * the two vertex indices at edge+0x0c / edge+0x10 belongs to this surface.
 * That index resolves against the vertex block at base (stride 0xc) to a
 * float[3] position. Same two-level decode as FUN_00103c00.
 *
 * All three positions must satisfy |dot(pt, plane) - plane[3]| < 0.01
 * (the double at 0x28b800 is (double)0.01f, compared via FABS / FCOMP qword
 * exactly like FUN_00103600's epsilon tests). The plane through the three
 * points is then built with FUN_001037b0 and the surface is accepted only when
 * dot(built_normal, plane) > 0 (the float at 0x2533c0 is 0.0f; TEST AH,0x41 /
 * JNE at 0x103b65 makes the test strict and rejects NaN).
 *
 * Note the point order handed to FUN_001037b0: PUSH EDX(p1), PUSH ECX(p2),
 * PUSH EDI(p0), PUSH ECX(&out) at 0x103b35-0x103b3b gives
 * FUN_001037b0(&out, p0, p2, p1) — points 1 and 2 are SWAPPED relative to the
 * natural order, which is what selects the winding sense. Do not "fix" this;
 * it would negate the built normal and invert the facing test. FUN_00103c00
 * builds its plane with the same swap.
 *
 * ABI: cdecl, four stack args, returns the accept flag in AL (MOV AL,1 at
 * 0x103b6c / XOR AL,AL at 0x103b75). Ghidra printed
 * `void __cdecl FUN_00103a00(void)` with the arguments surfacing as
 * in_stack_* and dropped the return entirely.
 *
 * The FPU evaluation order in every dot product is component 1, then 2, then
 * 0 (FLD [pt+4] first), and the epsilon subtraction is FSUB — `dot - plane[3]`
 * — not FSUBR.
 */
char FUN_00103a00(uint32_t plane_arg, int base, uint32_t *tri_refs,
                  uint32_t mark)
{
  float *plane;
  float *p0;
  float *p1;
  float *p2;
  float out_plane[4];

  (void)mark; /* delivered by the walker's visitor prototype; never read here */

  p0 = (float *)FUN_00117ee0(
    (int *)base,
    ((int *)(FUN_00117ee0((int *)(base + 0xc), (int)(tri_refs[0] & 0x7fffffff),
                          0x1c) +
             0xc))[(int)(tri_refs[0] & 0x80000000) != 0],
    0xc);
  p1 = (float *)FUN_00117ee0(
    (int *)base,
    ((int *)(FUN_00117ee0((int *)(base + 0xc), (int)(tri_refs[1] & 0x7fffffff),
                          0x1c) +
             0xc))[(int)(tri_refs[1] & 0x80000000) != 0],
    0xc);
  p2 = (float *)FUN_00117ee0(
    (int *)base,
    ((int *)(FUN_00117ee0((int *)(base + 0xc), (int)(tri_refs[2] & 0x7fffffff),
                          0x1c) +
             0xc))[(int)(tri_refs[2] & 0x80000000) != 0],
    0xc);

  /* The plane pointer is only materialized here: the original reloads
   * [EBP+8] into ESI at 0x103ab7, after the last use of tri_refs. */
  plane = (float *)plane_arg;
  if ((main_fabs_double_from_float(p0[1] * plane[1] + p0[2] * plane[2] +
                                   p0[0] * plane[0] - plane[3]) < 0.01f) &&
      (main_fabs_double_from_float(p1[1] * plane[1] + p1[2] * plane[2] +
                                   p1[0] * plane[0] - plane[3]) < 0.01f) &&
      (main_fabs_double_from_float(p2[1] * plane[1] + p2[2] * plane[2] +
                                   p2[0] * plane[0] - plane[3]) < 0.01f)) {
    if (FUN_001037b0(out_plane, p0, p2, p1) != NULL) {
      if (out_plane[0] * plane[0] + out_plane[2] * plane[2] +
            out_plane[1] * plane[1] >
          0.0f) {
        return 1;
      }
    }
  }
  return 0;
}

/*
 * FUN_00103b80 — resolve a triangle's three vertices and forward them.
 *
 * Looks up element `tri` in table A (obj+0x134, stride 0x34) at `index`.
 * That element holds three vertex indices at word offsets +2/+3/+4
 * (byte 0x8/0xc/0x10). Each index is resolved against vertex table B
 * (obj+0x140, stride 0x50); the +8 offset into each 0x50-byte vertex
 * element is the payload passed downstream (a float* — a position/vertex
 * pointer). The three resolved pointers plus `base` and `flag` are handed
 * to FUN_00103860.
 *
 * ABI: cdecl, 4 stack params. Ghidra mis-typed this as void(void) and
 * aliased EDI=[EBP+0xc]/ESI, losing [EBP+0x8]. The true prototype and the
 * 5-arg call to FUN_00103860 are reconstructed from the disassembly push
 * sequences, not the decompiler. FUN_00103860 itself is a 5-param cdecl
 * (verified from its own disasm/decompile): (base, a, b, c, flag).
 *
 * The three inner FUN_00117ee0 calls are written as arguments to
 * FUN_00103860 so MSVC right-to-left evaluation reproduces the original
 * interleaved push order: flag first, then vertex[tri[4]], vertex[tri[3]],
 * vertex[tri[2]], then base last.
 *
 * FUN_00117ee0(array_base, index, elem_size) returns &array[index].
 */
void FUN_00103b80(int base, int obj, int index, int flag)
{
  int *tri;

  tri = (int *)FUN_00117ee0((int *)(obj + 0x134), index, 0x34);
  FUN_00103860(
    base, (float *)(FUN_00117ee0((int *)(obj + 0x140), tri[2], 0x50) + 8),
    (float *)(FUN_00117ee0((int *)(obj + 0x140), tri[3], 0x50) + 8),
    (float *)(FUN_00117ee0((int *)(obj + 0x140), tri[4], 0x50) + 8), flag);
}

/*
 * FUN_00103c00 — walk a BSP's surface list and flood-mark unassigned surfaces.
 *
 * `bsp` is a collision-BSP-like structure of tag blocks:
 *   bsp+0x00 (bsp[0])  vertex block,  element size 0x0c (3 floats)
 *   bsp+0x0c (bsp[3])  edge block,    element size 0x1c
 *   bsp+0x18 (bsp[6])  surface block, element size 0x18
 *   bsp+0x1c (bsp[7])  surface element count
 *
 * For every surface i, the three edge indices at surface[0]/[1]/[2] are
 * resolved. Each edge index carries a direction flag in bit 31: the low 31
 * bits index the edge block, and the flag selects which of the two vertex
 * indices stored at edge+0x0c / edge+0x10 is this surface's vertex. That
 * vertex index is resolved against the vertex block to a float* position.
 *
 * The three positions build a plane via FUN_001037b0(out, p0, p1, p2). If the
 * surface's field at +0x0c is still 0xffffffff (unassigned), a depth-first
 * marking walk FUN_00103530 is seeded from this surface with the running
 * count as the mark value and FUN_00103a00 as the visitor. The number of
 * seeded walks is returned.
 *
 * ABI: cdecl, one stack param. Ghidra prints void __cdecl f(void) with the
 * param surfacing as in_stack_00000004, and prints no return — but 0x103c0d
 * XOR EAX,EAX (zero-iteration path) and 0x103d17 MOV EAX,[EBP-0x8] before the
 * epilogue prove the count is returned in EAX (lift-learnings §16 void-EAX).
 *
 * The 5-arg FUN_00103530 call is reconstructed from the disassembly push
 * sequence (Ghidra dropped all five args): PUSH [EBP-0x4](i), PUSH
 * [EBP-0x8](count), PUSH LEA[EBP-0x18](plane), PUSH 0x103a00, PUSH EDI(bsp);
 * first arg is the last push.
 *
 * The three vertex lookups are written inline as arguments so MSVC's
 * right-to-left evaluation reproduces the original interleaved sequence:
 * the vertex for edge[1] is produced and pushed first, then edge[2], then
 * edge[0], and all three are held on the stack across the intervening calls.
 * That production order is NOT the argument order.
 *
 * FUN_00117ee0(block, index, element_size) returns &block[index].
 */
int FUN_00103c00(int *bsp)
{
  int *edges;
  uint32_t *surface;
  int count;
  int i;
  float plane[4];

  count = 0;
  i = 0;
  if (0 < bsp[7]) {
    edges = bsp + 3;
    do {
      surface = (uint32_t *)FUN_00117ee0(bsp + 6, i, 0x18);
      FUN_001037b0(
        plane,
        (float *)FUN_00117ee0(
          bsp,
          ((int *)(FUN_00117ee0(edges, (int)(surface[0] & 0x7fffffff), 0x1c) +
                   0xc))[(int)(surface[0] & 0x80000000) != 0],
          0xc),
        (float *)FUN_00117ee0(
          bsp,
          ((int *)(FUN_00117ee0(edges, (int)(surface[2] & 0x7fffffff), 0x1c) +
                   0xc))[(int)(surface[2] & 0x80000000) != 0],
          0xc),
        (float *)FUN_00117ee0(
          bsp,
          ((int *)(FUN_00117ee0(edges, (int)(surface[1] & 0x7fffffff), 0x1c) +
                   0xc))[(int)(surface[1] & 0x80000000) != 0],
          0xc));
      if (surface[3] == 0xffffffff) {
        FUN_00103530((int)bsp, FUN_00103a00, (uint32_t)plane, (uint32_t)count,
                     i);
        count = count + 1;
      }
      i = i + 1;
    } while (i < bsp[7]);
  }
  return count;
}

/* Lazily opens the debug VRML output file ("debug.wrl") on the first call,
 * writes the VRML header, flushes it, and caches the FILE* in the global at
 * 0x46e394. Returns whether the handle is non-NULL (open succeeded). The
 * open is idempotent: once the handle is cached, subsequent calls skip the
 * open/write and just report handle-valid status. */
bool FUN_00103d30(void)
{
  if (*(void **)0x46e394 == NULL) {
    *(void **)0x46e394 = crt_fopen("debug.wrl", "w");
    if (*(void **)0x46e394 != NULL) {
      crt_fprintf(*(void **)0x46e394, "#VRML V1.0 ascii\n\n");
      crt_fflush(*(void **)0x46e394);
    }
  }
  return *(void **)0x46e394 != NULL;
}

/* error_geometry.c — debug VRML ("error geometry") output subsystem.
 *
 * Source TU proven by the __FILE__ assert xref
 * "c:\halo\SOURCE\tool\error_geometry.c". Grouped under main.obj alongside its
 * sibling FUN_00103d30 (debug .wrl lazy-open) at 0x103d30.
 */

/* FUN_00103d80 (0x103d80)  error_geometry.c:0x44
 *
 * Re-run the error-geometry file setup against the already-cached path buffer
 * (module-global @0x31fac8, whose initialiser is the string "debug.wrl").
 * Unlike its sibling FUN_00103de0 this entry point takes no source string: it
 * skips the csstrncmp/csstrncpy path update entirely, asserts that no output
 * stream is currently open (error_geometry_file, FILE* @0x46e394), and hands
 * the cached path to the CRT-region helper FUN_001db4a9.
 *
 * cdecl, no parameters and no stack frame: the disassembly at 0x103d80 is 15
 * instructions with no PUSH EBP / SUB ESP and a bare RET, so no locals may be
 * declared here.  The lone argument to FUN_001db4a9 is the literal 0x31fac8
 * (PUSH 0x31fac8 / CALL 0x001db4a9 / POP ECX); the decompiler drops it.  The
 * assert tail rendered as thunk_FUN_001029a0 is CALL 0x0008e2f0 =
 * system_exit(-1), matching every other error_geometry.c assert.
 */
void FUN_00103d80(void)
{
  if (*(void **)0x46e394 != NULL) {
    display_assert("error_geometry_file==NULL",
                   "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0x44, true);
    system_exit(-1);
  }
  FUN_001db4a9((char *)0x31fac8);
}

/* FUN_00103de0 (0x103de0)  error_geometry.c:0x44
 *
 * Retarget the debug error-geometry output file. If 'source' differs from the
 * currently-cached path (module-global buffer @0x31fac8, compared with
 * csstrncmp over 0x3b bytes), then:
 *   - close and clear any open error_geometry_file (FILE* @0x46e394),
 *   - copy 'source' into the path buffer (csstrncpy, 0x3b),
 *   - clear the byte flag @0x31fb03,
 *   - append the ".wrl" extension (FUN_0008dc30 = strcat-like),
 *   - assert the file handle is now NULL, and
 *   - run the CRT-region helper FUN_001db4a9.
 * If 'source' matches the cached path, the call is a no-op.
 *
 * cdecl, verified from disassembly at 0x103de0: the sole stack arg
 * [EBP+0x8]='source' (Ghidra surfaces it as in_stack_00000004 because the
 * kb.json decl was void(void)). The assert tail's decompiler thunk_FUN_001029a0
 * resolves in this TU to system_exit(-1) (CALL 0x8e2f0), matching every other
 * error_geometry.c assert; verified by check_assert_targets.py.
 */
void FUN_00103de0(char *source)
{
  if (csstrncmp((char *)0x31fac8, source, 0x3b) != 0) {
    if (*(void **)0x46e394 != NULL) {
      crt_fclose(*(void **)0x46e394);
      *(void **)0x46e394 = NULL;
    }
    csstrncpy((char *)0x31fac8, source, 0x3b);
    *(char *)0x31fb03 = 0;
    FUN_0008dc30((char *)0x31fac8, ".wrl");
    if (*(void **)0x46e394 != NULL) {
      display_assert("error_geometry_file==NULL",
                     "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0x44, true);
      system_exit(-1);
    }
    /* Arity fix forced by the corrected kb.json decl for FUN_001db4a9: the
     * call site at 0x103e?? pushes the cached path buffer exactly as
     * 0x103dab does (PUSH 0x31fac8 / CALL / POP ECX).  The previous no-arg
     * form left the callee reading an uninitialised [ESP+4]. */
    FUN_001db4a9((char *)0x31fac8);
  }
}

/* FUN_00103e80 (0x103e80)  error_geometry.c:0x8f-0x91
 *
 * Emits a single debug line segment (p0 -> p1) as a VRML/Open-Inventor
 * "Separator" block to the open error-geometry stream *(void**)0x46e394.
 * Both endpoints are transformed by the world matrix at 0x31fb08 and scaled
 * by *(float*)0x253f00 (=100.0f, world units -> cm).  Material is emitted
 * PER_VERTEX with the same colour on both ends: diffuseColor = color[1..3],
 * transparency = *(float*)0x2533c8 (=1.0f) - color[0], i.e. color[] is packed
 * alpha-first, exactly as in the sibling FUN_00104240.  The geometry itself is
 * a two-point IndexedLineSet ("coordIndex[0,1,-1]").  Gated on the
 * debug-geometry-enabled predicate FUN_00103d30.
 *
 * cdecl, verified from disassembly at 0x103e80: [EBP+0x8]=p0 (EDI),
 * [EBP+0xc]=p1 (ESI... EBX), [EBP+0x10]=color (ESI); EBX/ESI/EDI saved.
 * Frame = SUB ESP,0x18 = 24 bytes = ONE contiguous float[6] spanning
 * EBP-0x18..EBP-0x4.  The two matrix_transform_point 'out' arguments are
 * LEA EBP-0x18 and LEA EBP-0xc, i.e. &pt[0] and &pt[3] of that single buffer
 * -- declaring two separate float[3] locals would let clang reorder them, so
 * the one-buffer form is load-bearing.
 *
 * The transparency operand order is FLD [0x2533c8]; FSUB [ESI], i.e.
 * (1.0f - color[0]) and NOT the reverse; MSVC CSEs it and spills the single
 * result twice (FST/FSTP), which is why the value appears duplicated in the
 * argument list -- the duplication is real, not a lifting artefact.  Likewise
 * diffuseColor is genuinely printed twice (once per vertex).
 *
 * The stream global is re-loaded before every crt_fprintf and before the
 * crt_fflush in the original, so it is never cached in a local here.  Assert
 * tails are system_exit(-1) (CALL 0x8e2f0), not halt_and_catch_fire --
 * Ghidra's thunk_FUN_001029a0 at those sites is the known mis-decode.
 */
void FUN_00103e80(float *p0, float *p1, float *color)
{
  float pt[6];

  if (p0 == 0) {
    display_assert("p0", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0x8f,
                   true);
    system_exit(-1);
  }
  if (p1 == 0) {
    display_assert("p1", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0x90,
                   true);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0x91,
                   true);
    system_exit(-1);
  }
  if (FUN_00103d30()) {
    matrix_transform_point((float *)0x31fb08, p0, &pt[0]);
    matrix_transform_point((float *)0x31fb08, p1, &pt[3]);
    crt_fprintf(*(void **)0x46e394, "Separator\n{\n");
    crt_fprintf(*(void **)0x46e394,
                "\tCoordinate3 { point[%f %f %f, %f %f %f] }\n",
                pt[0] * *(float *)0x253f00, pt[1] * *(float *)0x253f00,
                pt[2] * *(float *)0x253f00, pt[3] * *(float *)0x253f00,
                pt[4] * *(float *)0x253f00, pt[5] * *(float *)0x253f00);
    crt_fprintf(*(void **)0x46e394, "\tMaterialBinding { value PER_VERTEX }\n");
    crt_fprintf(*(void **)0x46e394,
                "\tMaterial { diffuseColor[%f %f %f, %f %f %f] "
                "transparency[%f, %f] }\n",
                color[1], color[2], color[3], color[1], color[2], color[3],
                *(float *)0x2533c8 - color[0], *(float *)0x2533c8 - color[0]);
    crt_fprintf(*(void **)0x46e394,
                "\tIndexedLineSet { coordIndex[0,1,-1] }\n");
    crt_fprintf(*(void **)0x46e394, "}\n");
    crt_fflush(*(void **)0x46e394);
  }
}

/* FUN_00104040 (0x104040)  error_geometry.c:0xb1-0xb4
 *
 * Emits a single debug triangle (p0, p1, p2) as a VRML/Open-Inventor
 * "Separator" block to the open error-geometry stream *(void**)0x46e394.
 * All three vertices are transformed by the world matrix at 0x31fb08 and
 * scaled by *(float*)0x253f00 (=100.0f, world units -> cm).  Material is
 * emitted PER_FACE with a single colour: diffuseColor = color[1..3],
 * transparency = *(float*)0x2533c8 (=1.0f) - color[0], i.e. color[] is packed
 * alpha-first, exactly as in the siblings FUN_00103e80 / FUN_00104240.  The
 * geometry itself is a one-face IndexedFaceSet ("coordIndex[0,1,2,-1]").
 * Gated on the debug-geometry-enabled predicate FUN_00103d30.
 *
 * cdecl, verified from disassembly at 0x104040: all four parameters are STACK
 * args -- [EBP+0x8]=p0, [EBP+0xc]=p1, [EBP+0x10]=p2, [EBP+0x14]=color (ESI);
 * EBX/ESI/EDI saved, no register-argument contract.  Frame = SUB ESP,0x24 =
 * 36 bytes = ONE contiguous float[9] spanning EBP-0x24..EBP-0x4.  The three
 * matrix_transform_point 'out' arguments are LEA EBP-0x24, LEA EBP-0x18 and
 * LEA EBP-0xc, i.e. &pt[0], &pt[3] and &pt[6] of that single buffer --
 * declaring three separate float[3] locals would let clang reorder them, so
 * the one-buffer form is load-bearing.  (Ghidra names each triple after its
 * LAST component: local_28 is the array base = pt[0], local_24/local_20 are
 * pt[1]/pt[2].)
 *
 * Push order per matrix_transform_point call is PUSH &out; PUSH src;
 * PUSH 0x31fb08, so the matrix global is passed by ADDRESS, not dereferenced.
 *
 * The transparency operand order is 001041cb FLD [0x2533c8];
 * 001041d1 FSUB [ESI], i.e. (1.0f - color[0]) and NOT the reverse.  Each
 * transformed component is FLD'd then FMUL'd by the scale, i.e. pt * scale.
 * The stream global is re-loaded before every crt_fprintf and before the
 * crt_fflush in the original, so it is never cached in a local here.  Assert
 * tails are system_exit(-1) (CALL 0x8e2f0), not halt_and_catch_fire --
 * Ghidra's thunk_FUN_001029a0 at those sites is the known mis-decode.
 */
void FUN_00104040(float *p0, float *p1, float *p2, float *color)
{
  float pt[9];

  if (p0 == 0) {
    display_assert("p0", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0xb1,
                   true);
    system_exit(-1);
  }
  if (p1 == 0) {
    display_assert("p1", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0xb2,
                   true);
    system_exit(-1);
  }
  if (p2 == 0) {
    display_assert("p2", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0xb3,
                   true);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0xb4,
                   true);
    system_exit(-1);
  }
  if (FUN_00103d30()) {
    matrix_transform_point((float *)0x31fb08, p0, &pt[0]);
    matrix_transform_point((float *)0x31fb08, p1, &pt[3]);
    matrix_transform_point((float *)0x31fb08, p2, &pt[6]);
    crt_fprintf(*(void **)0x46e394, "Separator\n{\n");
    crt_fprintf(*(void **)0x46e394,
                "\tCoordinate3 { point[%f %f %f, %f %f %f, %f %f %f] }\n",
                pt[0] * *(float *)0x253f00, pt[1] * *(float *)0x253f00,
                pt[2] * *(float *)0x253f00, pt[3] * *(float *)0x253f00,
                pt[4] * *(float *)0x253f00, pt[5] * *(float *)0x253f00,
                pt[6] * *(float *)0x253f00, pt[7] * *(float *)0x253f00,
                pt[8] * *(float *)0x253f00);
    crt_fprintf(*(void **)0x46e394, "\tMaterialBinding { value PER_FACE }\n");
    crt_fprintf(*(void **)0x46e394,
                "\tMaterial { diffuseColor[%f %f %f] transparency[%f] }\n",
                color[1], color[2], color[3], *(float *)0x2533c8 - color[0]);
    crt_fprintf(*(void **)0x46e394,
                "\tIndexedFaceSet { coordIndex[0,1,2,-1] }\n");
    crt_fprintf(*(void **)0x46e394, "}\n");
    crt_fflush(*(void **)0x46e394);
  }
}

/* FUN_00104240 (0x104240)  error_geometry.c:0xd3-0xd5
 *
 * Emits a debug polygon (point cloud) as a VRML/Open-Inventor "Separator"
 * block to the open error-geometry stream *(void**)0x46e394.  Each packed
 * 3-float point is transformed by the world matrix at 0x31fb08 into a local
 * float[3] and scaled by *(float*)0x253f00 (=100.0f, world units -> cm) before
 * printing.  Point separator is ", " except after the last point, which closes
 * the array with "] }\n".  Material: diffuseColor = color[1..3],
 * transparency = *(float*)0x2533c8 (=1.0f) - color[0], i.e. color[] is packed
 * alpha-first.  Gated on the debug-geometry-enabled predicate FUN_00103d30 and
 * on count >= 3.
 *
 * cdecl, verified from disassembly at 0x104240: [EBP+0x8]=point_count (only
 * the low 16 bits are used: BX), [EBP+0xc]=points (EDI, stride 12B),
 * [EBP+0x10]=color (ESI).  Frame = 0xc bytes = the single float[3] transform
 * output at EBP-0xc (LEA EDX,[EBP-0xc] is the 'out' arg at 0x10431a).  The
 * (short) truncation is load-bearing in three places: the <0 assert, the >2
 * gate and the >0 loop gates.  The loop trip count is the ZERO-extended low 16
 * bits (MOVZX EBX,BX) while the last-element test uses the SIGN-extended value
 * (MOVSX EAX,BX; DEC EAX, which MSVC spills over the dead [EBP+0xc] param
 * slot).  The stream global is re-loaded before every crt_fprintf in the
 * original, so it is never cached in a local here.  Assert tails are
 * system_exit(-1) (CALL 0x8e2f0), not halt_and_catch_fire.
 */
void FUN_00104240(int point_count, float *points, float *color)
{
  float p[3];
  const char *sep;
  float *pt;
  short count;
  int i;
  int last;
  unsigned int n;

  count = (short)point_count;
  if (count < 0) {
    display_assert("point_count>=0", "c:\\halo\\SOURCE\\tool\\error_geometry.c",
                   0xd3, true);
    system_exit(-1);
  }
  if (points == 0) {
    display_assert("points", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0xd4,
                   true);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0xd5,
                   true);
    system_exit(-1);
  }
  if (count >= 3) {
    if (FUN_00103d30()) {
      crt_fprintf(*(void **)0x46e394, "Separator\n{\n");
      crt_fprintf(*(void **)0x46e394, "\tCoordinate3 { point[");
      if (count > 0) {
        last = (int)count - 1;
        i = 0;
        pt = points;
        n = (unsigned int)(unsigned short)count;
        do {
          matrix_transform_point((float *)0x31fb08, pt, p);
          sep = ", ";
          if (i >= last)
            sep = "] }\n";
          crt_fprintf(*(void **)0x46e394, "%f %f %f%s",
                      p[0] * *(float *)0x253f00, p[1] * *(float *)0x253f00,
                      p[2] * *(float *)0x253f00, sep);
          i = i + 1;
          pt = pt + 3;
          n = n - 1;
        } while (n != 0);
      }
      crt_fprintf(*(void **)0x46e394, "\tMaterialBinding { value PER_FACE }\n");
      crt_fprintf(*(void **)0x46e394,
                  "\tMaterial { diffuseColor[%f %f %f] transparency[%f] }\n",
                  color[1], color[2], color[3], *(float *)0x2533c8 - color[0]);
      crt_fprintf(*(void **)0x46e394, "\tIndexedFaceSet { coordIndex[");
      if (count > 0) {
        i = 0;
        n = (unsigned int)(unsigned short)count;
        do {
          crt_fprintf(*(void **)0x46e394, "%d,", i);
          i = i + 1;
          n = n - 1;
        } while (n != 0);
      }
      crt_fprintf(*(void **)0x46e394, "-1] }\n");
      crt_fprintf(*(void **)0x46e394, "}\n");
      crt_fflush(*(void **)0x46e394);
    }
  }
}

/* FUN_00104430 (0x104430)  error_geometry.c:0x14c-0x14e
 *
 * Emits a debug *multi-polygon* mesh as a VRML/Open-Inventor "Separator" block
 * to the open error-geometry stream *(void**)0x46e394.  Sibling of
 * FUN_00104240 (single point-cloud) but takes a per-polygon vertex count
 * array, so it writes a real IndexedFaceSet with a coordIndex list.
 *
 *   polygon_count  number of polygons
 *   point_counts   short[polygon_count], vertices in each polygon
 *   points         packed 3-float vertices, concatenated over all polygons
 *   color          4 floats per polygon, packed alpha-first (may be NULL)
 *
 * Three passes over point_counts[]:
 *   1. Coordinate3 point[]  -- every vertex, transformed by the world matrix at
 *      0x31fb08 into a local float[3] and scaled by *(float*)0x253f00 (=100.0f,
 *      world units -> cm).  A single running vertex index walks `points`.
 *   2. Material diffuseColor[] -- per polygon, color[p*4+1..3] repeated once
 * per *triangle* (i = 2 .. point_counts[p]-1) then a newline.  ESI is NOT
 *      advanced inside the inner loop in the original (0x1045e0-0x104610), so
 *      the same triple really is printed repeatedly.  transparency =
 *      *(float*)0x2533c8 (=1.0f) - color[0] (plain FSUB, not FSUBR).
 *   3. IndexedFaceSet coordIndex[] -- fan triangulation: for each polygon,
 *      (base, base+i-1, base+i, -1) for i = 2 .. point_counts[p]-1, then
 *      base += point_counts[p] (MOVSX word at 0x1046d4).  Ghidra dropped all
 *      three %d arguments and the base accumulator from its decompile; they are
 *      recovered from the pushes at 0x1046a0-0x1046b5 (PUSH base+i / DEC /
 *      PUSH base+i-1 / PUSH base, ADD ESP,0x14 = 5 dwords).
 *
 * cdecl, verified from disassembly at 0x104430: [EBP+0x8]=polygon_count (ESI,
 * full 32-bit -- no short truncation, unlike FUN_00104240), [EBP+0xc]=
 * point_counts (EDI, stride 2B), [EBP+0x10]=points, [EBP+0x14]=color (EBX).
 * Frame = 0x14 bytes = float[3] transform output at EBP-0x14..EBP-0xc plus the
 * running vertex index at EBP-0x4 and one loop down-counter at EBP-0x8; the
 * other two down-counters are spilled over the dead [EBP+0x10] and [EBP+0x8]
 * param slots, so they are three distinct locals here.  All inner counters are
 * 16-bit (CMP DI/BX/SI, WORD PTR [reg]).  The stream global is re-loaded before
 * every crt_fprintf in the original, so it is never cached in a local.  Assert
 * tails are system_exit(-1) (CALL 0x8e2f0), not halt_and_catch_fire.
 */
void FUN_00104430(int polygon_count, short *point_counts, float *points,
                  float *color)
{
  float p[3];
  float *pt;
  float *cp;
  short *pc;
  short i;
  int vertex_index;
  int n1;
  int n2;
  int n3;

  if (polygon_count < 0) {
    display_assert("polygon_count>=0",
                   "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0x14c, true);
    system_exit(-1);
  }
  if (point_counts == 0) {
    display_assert("point_counts", "c:\\halo\\SOURCE\\tool\\error_geometry.c",
                   0x14d, true);
    system_exit(-1);
  }
  if (points == 0) {
    display_assert("points", "c:\\halo\\SOURCE\\tool\\error_geometry.c", 0x14e,
                   true);
    system_exit(-1);
  }
  if (polygon_count > 0) {
    if (FUN_00103d30()) {
      crt_fprintf(*(void **)0x46e394, "Separator\n{\n");
      crt_fprintf(*(void **)0x46e394, "\tCoordinate3\n\t{\n\t\tpoint\n\t\t[\n");
      vertex_index = 0;
      if (polygon_count > 0) {
        pc = point_counts;
        n1 = polygon_count;
        do {
          i = 0;
          if (*pc > 0) {
            pt = points + vertex_index * 3;
            do {
              matrix_transform_point((float *)0x31fb08, pt, p);
              crt_fprintf(*(void **)0x46e394, "\t\t\t%f %f %f,\n",
                          p[0] * *(float *)0x253f00, p[1] * *(float *)0x253f00,
                          p[2] * *(float *)0x253f00);
              i = i + 1;
              vertex_index = vertex_index + 1;
              pt = pt + 3;
            } while (i < *pc);
          }
          pc = pc + 1;
          n1 = n1 - 1;
        } while (n1 != 0);
      }
      crt_fprintf(*(void **)0x46e394, "\t\t]\n\t}\n");
      crt_fprintf(*(void **)0x46e394,
                  "\tMaterialBinding\n\t{\n\t\tvalue PER_FACE\n\t}\n");
      if (color != 0) {
        crt_fprintf(*(void **)0x46e394,
                    "\tMaterial\n\t{\n\t\tdiffuseColor\n\t\t[\n");
        if (polygon_count > 0) {
          n2 = polygon_count;
          cp = color + 2;
          pc = point_counts;
          do {
            i = 2;
            if (*pc > 2) {
              do {
                crt_fprintf(*(void **)0x46e394, "\t\t\t%f %f %f, ", cp[-1],
                            cp[0], cp[1]);
                i = i + 1;
              } while (i < *pc);
            }
            crt_fprintf(*(void **)0x46e394, "\n");
            cp = cp + 4;
            pc = pc + 1;
            n2 = n2 - 1;
          } while (n2 != 0);
        }
        crt_fprintf(*(void **)0x46e394, "\t\t]\n\t\ttransparency[%f]\n\t}\n",
                    *(float *)0x2533c8 - color[0]);
      }
      crt_fprintf(*(void **)0x46e394,
                  "\tIndexedFaceSet\n\t{\n\t\tcoordIndex\n\t\t[\n");
      vertex_index = 0;
      if (polygon_count > 0) {
        n3 = polygon_count;
        pc = point_counts;
        do {
          crt_fprintf(*(void **)0x46e394, "\t\t\t");
          i = 2;
          if (*pc > 2) {
            do {
              crt_fprintf(*(void **)0x46e394, "%d,%d,%d,-1, ", vertex_index,
                          vertex_index + i - 1, vertex_index + i);
              i = i + 1;
            } while (i < *pc);
          }
          crt_fprintf(*(void **)0x46e394, "\n");
          vertex_index = vertex_index + *pc;
          pc = pc + 1;
          n3 = n3 - 1;
        } while (n3 != 0);
      }
      crt_fprintf(*(void **)0x46e394, "\t\t]\n\t}\n}\n");
      crt_fflush(*(void **)0x46e394);
    }
  }
}
