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

short game_connection(void)
{
  return word_46DA0C;
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

void main_menu_precache_resources(void)
{
  scenario_t *scenario = global_scenario_get();
  if (scenario) {
    assert_halt(scenario->type == _scenario_type_main_menu);
    predicted_resources_precache(&scenario->unk_236);
  }
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

  typedef bool(__cdecl * fn_music_playing_t)(void);
  typedef void(__cdecl * fn_ui_fade_start_t)(int duration_ms);
  typedef void(__cdecl * fn_set_fade_t)(float fade);
  typedef void(__cdecl * fn_stop_music_t)(void);
  typedef void(__cdecl * fn_set_widget_flag2_t)(bool enable);
  typedef void(__cdecl * fn_save_player_level_t)(int local_player_index);

  if (main_globals.main_menu_scenario_loaded) {
    if (*(int *)0x46da34 == 0) {
      /* music not yet fading: check if music is still playing */
      if (((fn_music_playing_t)0xe46a0)()) {
        /* set deadline and kick off the 1000 ms fade sequence */
        *(uint32_t *)0x46da34 = (uint32_t)unk_time_globals.unk_0 + 1000;
        /* MSVC interleaved pre-push: PUSH 0x3e8, PUSH 0x1, PUSH 0x0 */
        ((fn_ui_fade_start_t)0xe5a40)(1000);
        ui_widget_set_events_suppressed(1);
        ((fn_set_fade_t)0xe3c90)(0.0f);
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
        ((fn_set_fade_t)0xe3c90)(1.0f - flt_delta * *(float *)0x255ef8);
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
  ((fn_set_fade_t)0xe3c90)(-1.0f); /* 0xbf800000 */
  ((fn_stop_music_t)0xe4640)();
  ((fn_set_widget_flag2_t)0xe43d0)(0);
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
      ((fn_save_player_level_t)0x1c1c00)(i);
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
      ((void (*)(int, const char *, ...))0xff4d0)(0, "unsafe save");
    }
    /* Fall through to shared trigger tail. */
  } else {
    /* Increment total-ticks counter and check for overflow. */
    orig_ticks = *(int *)0x46da30;
    *(int *)0x46da30 = orig_ticks + 1;

    if (orig_ticks >= 0xf0 && *(uint8_t *)0x46da2a != 0) {
      /* Hung for too long — abort the save. */
      if (debug_game_save) {
        ((void (*)(int, const char *, ...))0xff4d0)(0,
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

  typedef uint16_t(__cdecl * fn_map_to_level_t)(char *map_name);
  typedef void(__cdecl * fn_save_player_level_t)(int local_player_index);
  typedef void(__cdecl * fn_level_transition_t)(int level_index);

  main_menu_load_pending = 1;
  main_won_map_private_pending = 0;

  /* map the current map name to a 0-based level index; unrecognized = -1 */
  map_level = ((fn_map_to_level_t)0x1006f0)(map_name);
  level_index = (int)(map_level + 1);
  if ((int16_t)level_index >= 10) {
    level_index = -1;
  }

  /* record level completion in each local player's saved profile */
  for (i = 0; (int16_t)i < player_spawn_count; i++) {
    ((fn_save_player_level_t)0x1c1cc0)(i);
  }

  /* trigger level transition or return to main menu */
  ((fn_level_transition_t)0xe4420)(level_index);
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
 *    Ghidra's "FUN_0008df60" stub in the decomp was the same helper.
 */
void main_save_current_solo_map(char *map_name)
{
  uint16_t level_index;
  void *fp;

  typedef uint16_t(__cdecl * fn_map_to_level_t)(char *map_name);
  typedef void *(__cdecl * fn_fopen_t)(const char *path, const char *mode);
  typedef size_t(__cdecl * fn_fwrite_t)(const void *buf, size_t size,
                                        size_t count, void *fp);
  typedef int(__cdecl * fn_fclose_t)(void *fp);

  level_index = ((fn_map_to_level_t)0x1006f0)(map_name);
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
  typedef uint16_t(__cdecl * fn_map_to_level_t)(char *map_name);
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
    level_index = ((fn_map_to_level_t)0x1006f0)(buf);
    if (level_index != 0xffff) {
      map_path = buf;
    }
  }

  ((fn_queue_map_t)0xfffa0)(map_path);
  main_change_map_name_pending = 0;
  main_load_last_solo_map_pending = 0;
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
// FIXME: d3d_find_flipcount checks handler address, so we cannot
//        provide function reference here until we re-implement it.
#define main_vertical_blank_interrupt_handler (void *)0x101CD0

  unk_time_globals.unk_0 = system_milliseconds();
  unk_time_globals.unk_8 = 0L;
  rasterizer_set_vblank_callback(main_vertical_blank_interrupt_handler);
  word_46DDDC = 0;
  csmemset(word_46DDDE, 0, 0x1Eu);
  flip_count_ptr = d3d_find_flipcount();
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
