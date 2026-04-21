void game_engine_dispose(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[2])
      vtable[2]();
    current_game_engine = 0;
  }
}

/* game_engine_evaluate_game_complexity (0xa8220)
 *
 * Counts active players and (in multiplayer scenario type 5) vehicle seats
 * of type 4, then sets complexity-level flags in the game-engine state
 * word at 0x5aa720.  Complexity bits:
 *   bit 0  "high":     players>8 && vehicles>1, OR players>12
 *   bit 1  "medium":   players>4 || vehicles>3 || bit0 already set
 *   bit 2  "some":     players>4
 *   bit 3  "many":     players>8
 */
void game_engine_evaluate_game_complexity(void)
{
  data_iter_t iter;
  int player_count;
  int object_count;
  int seat_count;
  int i;
  int16_t si;
  void *scenario;
  int *seat_block;
  int seat_elem;

  player_count = 0;
  object_count = 0;
  seat_count = 0;

  /* Count active players */
  data_iterator_new(&iter, player_data);
  while (data_iterator_next(&iter) != NULL)
    player_count++;

  if (*(int32_t *)0x456b40 != 1) {
    /* Multiplayer scenario type 5: count vehicle seats of type 4 */
    if (*(int32_t *)0x456b10 == 5) {
      scenario = global_scenario_get();
      seat_block = (int *)((char *)scenario + 0x378);
      si = 0;
      if (*seat_block > 0) {
        i = 0;
        do {
          seat_elem = (int)tag_block_get_element(seat_block, i, 0x94);
          if (*(int16_t *)(seat_elem + 0x10) == 4)
            seat_count++;
          si++;
          i = (int)si;
        } while (i < *seat_block);
      }
      /* Clamp seat_count to player_count */
      if (seat_count > player_count)
        seat_count = player_count;
    } else {
      /* All other types: count objects of type 2 */
      int obj_iter[4];
      object_iterator_new(obj_iter, 2, 0);
      while (object_iterator_next(obj_iter) != NULL)
        object_count++;
      seat_count = object_count;
    }
  }

  /* Set complexity flags at 0x5aa720 */
  if ((player_count > 8 && seat_count > 1) || player_count > 12)
    *(uint32_t *)0x5aa720 |= 1;

  if (player_count > 4 || seat_count > 3 || (*(uint32_t *)0x5aa720 & 1) != 0)
    *(uint32_t *)0x5aa720 |= 2;

  if (player_count > 4)
    *(uint32_t *)0x5aa720 |= 4;

  if (player_count > 8)
    *(uint32_t *)0x5aa720 |= 8;
}

void game_engine_dispose_from_old_map(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[4])
      vtable[4]();
  }
}

int game_engine_game_starting(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[7])
      vtable[7]();
    game_engine_evaluate_game_complexity();
  }
  return 0;
}

/* Remove dropped weapons older than 900 ticks (30 seconds) and
 * equipment items whose idle timer exceeds 900 ticks.
 * Weapons that are CTF flags or attached to vehicles are preserved. */
void game_engine_remove_dropped_weapons(void)
{
  int game_time;
  int iter_buf[4];
  int handle;
  char *obj;

  game_time = game_time_get();

  object_iterator_new(iter_buf, 0x1c, 0);
  while (object_iterator_next(iter_buf) != NULL) {
    handle = iter_buf[2];
    obj = (char *)object_get_and_verify_type(handle, 0x1c);
    if (*(int *)(obj + 0x1b4) < game_time - 900 &&
        (*(uint8_t *)(obj + 0x1a4) & 1) == 0) {
      if (object_try_and_get_and_verify_type(handle, 4) == NULL ||
          !weapon_is_flag(handle)) {
        object_delete(handle);
      }
    }
  }

  object_iterator_new(iter_buf, 1, 0);
  while (object_iterator_next(iter_buf) != NULL) {
    handle = iter_buf[2];
    obj = (char *)object_get_and_verify_type(handle, -1);
    if (*(int16_t *)(obj + 0x6c) > (int16_t)900 &&
        (*(uint8_t *)(obj + 0xb6) & 4) != 0) {
      object_delete(handle);
    }
  }
}

/* If the flag weapon is loose (no parent vehicle, no carried bit) and has
 * bit 0x20 at offset 0x1dc set, clear that bit and notify the game engine
 * via vtable slot 16. weapon_index passed in ESI (register arg). */
void game_engine_update_flag_state(int weapon_index /* @<esi> */)
{
  char *obj;

  if (weapon_index == -1) {
    display_assert("weapon_index != NONE",
                   "c:\\halo\\SOURCE\\game\\game_engine.c", 0x78c, 1);
    system_exit(-1);
  }
  if (!weapon_is_flag(weapon_index)) {
    display_assert("weapon_is_flag(weapon_index)",
                   "c:\\halo\\SOURCE\\game\\game_engine.c", 0x78d, 1);
    system_exit(-1);
  }

  obj = (char *)object_get_and_verify_type(weapon_index, 4);
  if (*(int *)(obj + 0xcc) == -1 && (*(uint8_t *)(obj + 0x1a4) & 1) == 0 &&
      (*(uint32_t *)(obj + 0x1dc) & 0x20) != 0) {
    *(uint32_t *)(obj + 0x1dc) &= ~0x20u;
    {
      void (*fn)(int) = ((void (**)(int))current_game_engine)[0x40 / 4];
      if (fn)
        fn(weapon_index);
    }
  }
}

/* Iterate all weapon objects and set their scale from the item tag definition.
 * Uses tag field 0x184 as scale (defaults to 0.7f if zero).
 * For flag weapons with vehicle attachments, notifies the game engine. */
void game_engine_spawn_equipment(void)
{
  int iter_buf[4];
  int handle;
  char *obj;
  char *tag_data;
  float scale;

  if (current_game_engine == 0) {
    display_assert("NULL != game_engine",
                   "c:\\halo\\SOURCE\\game\\game_engine.c", 0x7aa, 1);
    system_exit(-1);
  }

  object_iterator_new(iter_buf, 0x1c, 0);
  while (object_iterator_next(iter_buf) != NULL) {
    handle = iter_buf[2];
    obj = (char *)object_get_and_verify_type(handle, 0x1c);

    if ((*(uint8_t *)(obj + 0x1a4) & 1) == 0) {
      tag_data = (char *)tag_get(0x6974656d, *(int *)obj);
      scale = *(float *)(tag_data + 0x184);
      if (scale == 0.0f)
        scale = *(float *)0x2533c8;
      *(float *)(obj + 0x60) = scale;
      if (scale < *(float *)0x253398 || scale > *(float *)0x254644) {
        display_assert(
          "(item->object.scale >= 0.5f) && (item->object.scale <= 3.f)",
          "c:\\halo\\SOURCE\\game\\game_engine.c", 0x7c0, 1);
        system_exit(-1);
      }
    } else {
      *(float *)(obj + 0x60) = 1.0f;
    }

    {
      void (*spawn_fn)(int, void *) =
        ((void (**)(int, void *))current_game_engine)[0x38 / 4];
      if (spawn_fn != NULL) {
        void *vehicle = object_try_and_get_and_verify_type(handle, 4);
        if (vehicle != NULL && weapon_is_flag(handle)) {
          game_engine_update_flag_state(handle);
          spawn_fn(handle, vehicle);
        }
      }
    }
  }
}

/* Trigger a game restart. Sets a 7-second countdown timer, posts a
 * restart event, and closes all UI widgets. Only fires once (guards
 * against repeated calls via the flag at 0x5aa730). */
void game_engine_start_over(void)
{
  if (*(int *)0x5aa730 == 0) {
    *(int *)0x5aa730 = 1;
    *(float *)0x5aa728 = 7.0f;
    game_engine_post_event(1);
    ui_widgets_close_all();
  }
}

/* game_engine_allow_weapon_pick_up (0xa8b30)
 *
 * Dispatches to vtable slot 0x58/4 = slot 22 of current_game_engine.
 * Returns true (default) when there is no engine, no vtable slot, or
 * the engine does not block pickup.  The JMP-to-vtable tail-call pattern
 * is faithfully preserved: the function jumps directly to the vtable
 * entry after popping EBP, so it acts as a transparent thunk.
 */
bool game_engine_allow_weapon_pick_up(int unit_handle, int weapon_handle)
{
  if (current_game_engine) {
    bool (*fn)(int, int) = ((bool (**)(int, int))current_game_engine)[0x58 / 4];
    if (fn)
      return fn(unit_handle, weapon_handle);
  }
  return true;
}

bool game_engine_running(void)
{
  return current_game_engine != 0;
}

bool game_engine_force_single_screen(void)
{
  return current_game_engine && game_engine_variant_index > 1 &&
         game_engine_variant_index < 4;
}

/* game_engine_unit_can_enter_seat (0xa90b0)
 *
 * Checks whether a unit is allowed to enter a vehicle seat under the
 * current game engine.  Steps:
 *   1. Confirm engine is active.
 *   2. Get the seat object's object_data_t via
 * object_try_and_get_and_verify_type (type mask 4 = vehicle).
 *   3. Check weapon_is_flag on the seat object.
 *   4. If the engine's "seat-entry" vtable slot fires and denies entry,
 *      return false.
 *   5. Otherwise allow entry (true).
 *   6. Assert that a contradictory "must-be-readied" flag state can't occur.
 *
 * Vtable offsets observed in disassembly:
 *   +0x40 (slot 16): called with seat_object_handle when bit 0x20 was already
 *                    set (clearing the flag first)
 *   +0x3c (slot 15): called with (seat_object_handle, player_index) — the
 *                    main entry-gate predicate
 */
bool game_engine_unit_can_enter_seat(int unit_handle, int seat_object_handle)
{
  bool result = true;

  if (!current_game_engine)
    return result;

  char *seat_obj =
    (char *)object_try_and_get_and_verify_type(seat_object_handle, 4);
  if (!seat_obj)
    return result;

  if (!weapon_is_flag(seat_object_handle))
    return result;

  /* If bit 0x20 of object flags was already set, clear it and call
   * vtable slot +0x40 (notify-of-seat-exit). */
  if (*(uint32_t *)(seat_obj + 0x1dc) & 0x20) {
    *(uint32_t *)(seat_obj + 0x1dc) &= ~0x20u;
    void (*slot_0x40)(int) = ((void (**)(int))current_game_engine)[0x40 / 4];
    if (slot_0x40)
      slot_0x40(seat_object_handle);
  }

  /* Set bit 0x20 to mark seat as occupied */
  *(uint32_t *)(seat_obj + 0x1dc) |= 0x20;

  /* Check per-engine entry gate at vtable slot +0x3c */
  int (*gate)(int, int) = ((int (**)(int, int))current_game_engine)[0x3c / 4];
  if (gate) {
    int player_idx = player_index_from_unit_index(unit_handle);
    result = (bool)gate(seat_object_handle, player_idx);
    if (!result)
      return false;
  }

  /* Assert: if weapon has the "must-be-readied" bit AND the unit already
   * has a weapon with that flag, something is wrong. */
  if ((*(uint8_t *)(seat_obj + 0x1dc) & 8) &&
      unit_has_weapon_with_flag(unit_handle, 3)) {
    display_assert(
      "!allow_pick_up || "
      "!TEST_FLAG(weapon->weapon.flags, _weapon_must_be_readied_bit) || "
      "!unit_has_weapon_with_flag(unit_index, _weapon_must_be_readied_bit)",
      "c:\\halo\\SOURCE\\game\\game_engine.c", 0xed2, true);
    system_exit(-1);
  }

  return result;
}

/* game_engine_can_pick_up_weapon (0xaba00)
 *
 * In Juggernaut (game type index 1) with an active engine, returns true
 * only if the weapon is a flag (weapon_is_flag).  In all other game types,
 * or when no engine is active, returns false.
 */
bool game_engine_can_pick_up_weapon(int player_unit_handle,
                                    int weapon_unit_handle)
{
  if (current_game_engine && *(int32_t *)0x456b10 == 1) {
    if (weapon_is_flag(weapon_unit_handle))
      return true;
  }
  return false;
}

/* game_engine_initialize (0xaba30)
 *
 * Initialises the game engine for a new multiplayer session.
 * Clears the 0x24-byte engine state block starting at 0x5aa720, resets
 * game_engine_variant_index to 0, then (if a non-null variant with a
 * non-zero engine-type field is supplied) copies the variant into the
 * globals at 0x456af8 and looks up the engine vtable pointer from the
 * table at 0x2eff98.
 *
 * variant layout (game_variant_t, 0x68 bytes):
 *   +0x00..+0x17 : header fields
 *   +0x18        : engine type index (int32, used as table index into
 *                  the vtable-pointer array at 0x2eff98)
 * The copy is 26 dwords (0x1a * 4 = 0x68 bytes) via MOVSD.REP.
 */
void game_engine_initialize(game_variant_t *variant)
{
  csmemset((void *)0x5aa720, 0, 0x24);
  *(int32_t *)0x5aa730 = 0;

  if (variant && *(int32_t *)((char *)variant + 0x18) != 0) {
    /* Copy variant (0x68 bytes) into 0x456af8 */
    qmemcpy((void *)0x456af8, variant, 0x68);

    /* Validate/clamp the variant fields */
    game_engine_variant_cleanup((game_variant_t *)0x456af8);

    /* Look up vtable pointer for this engine type */
    int engine_type = *(int32_t *)((char *)variant + 0x18);
    current_game_engine =
      (void *)(*(int32_t *)((char *)0x2eff98 + engine_type * 4));
  }
}

/* game_engine_update_non_deterministic (0xacdd0)
 *
 * Handles the post-game fade-out/score-screen sequence.  The state
 * machine at 0x5aa730 drives two phases:
 *
 *   phase 2 – countdown timer (0x5aa728 counts down from ~5 seconds);
 *             transitions to phase 3 when it reaches 0.
 *   phase 3 – score/postgame screen; counts up 0x5aa72c; advances through
 *             several UI "done" polls (game_engine_check_input_button
 *             with @edi index); when all return true either begins a
 *             map-cycle or transitions to a network server reset.
 *
 * game_engine_check_input_button (@edi = button index) is called via
 * inline asm so the correct EDI value is passed.
 */
void game_engine_update_non_deterministic(float dt)
{
  if (!current_game_engine)
    return;

  int phase = *(int32_t *)0x5aa730;

  if (phase == 2) {
    /* Phase 2: run input reset, subtract dt from countdown timer */
    rumble_clear_all_players();
    *(float *)0x5aa728 -= dt;
    /* Transition to phase 3 when timer reaches 0 */
    float t = *(float *)0x5aa728;
    if (!(t < 0.0f) && !(t == 0.0f)) {
      /* still > 0, stay in phase 2 */
    } else {
      *(int32_t *)0x5aa730 = 3;
    }
  } else if (phase == 3) {
    /* Phase 3: run input reset, advance progress counter */
    rumble_clear_all_players();
    *(float *)0x5aa72c += dt;
    if (*(float *)0x5aa72c > 1.0f)
      *(float *)0x5aa72c = 1.0f;

    /* Poll four "done" conditions via @edi-indexed
     * game_engine_check_input_button. EDI indices: 0, 0xc, 1, 0xd (matching
     * disassembly order). */
    bool ok0, ok1, ok2, ok3;

    ok0 = game_engine_check_input_button(0);
    ok1 = game_engine_check_input_button(0xc);

    if (ok0 || ok1) {
      /* At least one input is "done": check for network server to reset */
      void *server = network_game_server_get();
      if (server) {
        network_server_manager_pregame_start(server);
        return;
      }
      return;
    }

    ok2 = game_engine_check_input_button(1);
    ok3 = game_engine_check_input_button(0xd);

    if (!ok2 && !ok3)
      return;

    void *server = network_game_server_get();
    if (server) {
      network_server_manager_pregame_start(server);
      return;
    }
    network_game_abort();
  }
}

/* game_engine_get_variant_by_name (0xadd50)
 *
 * Looks up a default game_variant_t by string name and copies it into
 * *out_variant.  If the name is not recognised the output buffer is not
 * modified (it was zeroed by the caller before this function runs).
 *
 * The variant constructors each take a single pointer to a local
 * game_variant_t-sized buffer and fill it in, then return that same pointer.
 * The filled buffer is then copied into the caller-supplied *out_variant.
 *
 * Supported names (order matches disassembly PUSH sequence):
 *   "race", "team_race", "rally", "slayer", "team_slayer",
 *   "elimination", "stalker", "team_oddball", "accumulation",
 *   "oddball", "ctf", "ironctf", "king", "team_king"
 */
game_variant_t *game_engine_get_variant_by_name(game_variant_t *out_variant,
                                                const char *name)
{
  game_variant_t tmp;

  csmemset(&tmp, 0, sizeof(tmp));

  game_variant_t *src = NULL;

  if (csstrcmp(name, "race") == 0)
    src = game_engine_race_default(&tmp);
  else if (csstrcmp(name, "team_race") == 0)
    src = game_engine_team_race_default(&tmp);
  else if (csstrcmp(name, "rally") == 0)
    src = game_engine_rally_default(&tmp);
  else if (csstrcmp(name, "slayer") == 0)
    src = game_engine_slayer_default(&tmp);
  else if (csstrcmp(name, "team_slayer") == 0)
    src = game_engine_team_slayer_default(&tmp);
  else if (csstrcmp(name, "elimination") == 0)
    src = game_engine_elimination_default(&tmp);
  else if (csstrcmp(name, "stalker") == 0)
    src = game_engine_stalker_default(&tmp);
  else if (csstrcmp(name, "team_oddball") == 0)
    src = game_engine_team_oddball_default(&tmp);
  else if (csstrcmp(name, "accumulation") == 0)
    src = game_engine_accumulation_default(&tmp);
  else if (csstrcmp(name, "oddball") == 0)
    src = game_engine_oddball_default(&tmp);
  else if (csstrcmp(name, "ctf") == 0)
    src = game_engine_ctf_default(&tmp);
  else if (csstrcmp(name, "ironctf") == 0)
    src = game_engine_ironctf_default(&tmp);
  else if (csstrcmp(name, "king") == 0)
    src = game_engine_king_default(&tmp);
  else if (csstrcmp(name, "team_king") == 0)
    src = game_engine_team_king_default(&tmp);

  if (src)
    qmemcpy(&tmp, src, sizeof(tmp));

  /* Copy tmp into *out_variant regardless (matches original rep movsd) */
  qmemcpy(out_variant, &tmp, sizeof(tmp));
  return out_variant;
}

/* game_engine_initialize_for_new_map (0xae760)
 *
 * Prepares the custom game engine for a newly loaded map.  Validates
 * netgame-flag placement, resets the score/phase state, zeroes the
 * 0x400-byte HUD/scoreboard buffer at 0x4566f8, then calls the engine's
 * per-map init vtable slot (+0x0c).  If the engine reports failure, logs
 * a warning and falls back to no engine.  Finally loads the game-engine
 * sound assets.
 */
void game_engine_initialize_for_new_map(void)
{
  if (!current_game_engine)
    return;

  game_engine_validate_map_netgame_flags();
  game_engine_score_reset();
  csmemset((void *)0x4566f8, 0, 0x400);
  *(int32_t *)0x5aa724 = 0;
  *(int32_t *)0x5aa744 = 0;

  void (*init_fn)(void) = ((void (**)(void))current_game_engine)[0x0c / 4];
  if (init_fn) {
    bool ok = (bool)((bool (*)(void))init_fn)();
    if (!ok) {
      error(2, "failed to initialize custome game engine for new map, "
               "reverting to default game engine");
      if (current_game_engine) {
        void (*dispose_fn)(void) =
          ((void (**)(void))current_game_engine)[0x08 / 4];
        if (dispose_fn)
          dispose_fn();
        current_game_engine = NULL;
      }
    }
  }

  game_engine_load_sounds();
}

/* game_engine_player_added (0xae7e0)
 *
 * Called when a player joins (or on map load for all existing players).
 * Initialises several player-datum fields to NONE / default values,
 * zeroes the score entry at player+0xc0, then (if an engine is active)
 * assigns the player their team index (0x5aa724 counter), fires
 * per-player score-display refresh (game_engine_hud_update_player
 * @fastcall ECX/EAX), and finally dispatches to vtable slot +0x14
 * (player-added callback).
 *
 * game_engine_hud_update_player is a fastcall-like function:
 *   ECX = player_handle
 *   EAX = NONE (-1)
 *   EBX = 0 from caller side
 * It is called via inline asm to ensure exact register state.
 *
 * 0x456b14 is a bool: non-zero means networked/team game (uses
 * network_game_client_get to determine team assignment instead of counter).
 */
void game_engine_player_added(int player_data_handle)
{
  char *player_datum = (char *)datum_get(player_data, player_data_handle);

  /* Initialise player datum fields */
  *(int32_t *)(player_datum + 0x74) = NONE;
  *(int32_t *)(player_datum + 0x78) = NONE;
  *(float *)(player_datum + 0x6c) = 1.0f;
  *(int32_t *)(player_datum + 0x70) = NONE;
  *(int32_t *)(player_datum + 0x7c) = NONE;
  csmemset(player_datum + 0xc0, 0, 4);

  if (!current_game_engine)
    return;

  player_datum = (char *)datum_get(player_data, player_data_handle);

  if (*(uint8_t *)0x456b14 == 0) {
    /* Single-player / non-networked: assign team from counter */
    *(uint8_t *)(player_datum + 0x66) = *(uint8_t *)0x5aa724;
    *(int32_t *)(player_datum + 0x20) = (int32_t)(*(uint8_t *)0x5aa724);
    *(int32_t *)0x5aa724 = *(int32_t *)0x5aa724 + 1;
  } else {
    /* Networked: check network_game_client_get for team assignment */
    void *client = network_game_client_get();
    if (client == NULL) {
      /* No client info: use counter with wrap-around to bit 0 */
      *(uint8_t *)(player_datum + 0x66) = *(uint8_t *)0x5aa724;
      *(int32_t *)(player_datum + 0x20) = (int32_t)(*(uint8_t *)0x5aa724);
      int v = *(int32_t *)0x5aa724 + 1;
      /* AND with 0x80000001 then sign-extend (MSVC idiom for mod 2) */
      v &= 0x80000001;
      if (v < 0)
        v = (v - 1) | 0xfffffffe, v++;
      *(int32_t *)0x5aa724 = v;
    } else {
      /* Client present: extract bit 0 from player+0x66 */
      int v = (int32_t)(*(int8_t *)(player_datum + 0x66)) & 0x80000001;
      if (v < 0)
        v = (v - 1) | 0xfffffffe, v++;
      *(int32_t *)(player_datum + 0x20) = v;
    }
  }

  /* Call game_engine_hud_update_player (fastcall: ECX=player_handle,
   * EAX=-1, EBX=0). */
  if (player_data_handle == NONE) {
    /* All players: iterate and call for each */
    data_iter_t iter;
    data_iterator_new(&iter, player_data);
    while (data_iterator_next(&iter) != NULL) {
      int ph = iter.datum_handle;
      __asm__ __volatile__(
        "movl %[ph], %%ecx\n\t"
        "movl $-1, %%eax\n\t"
        "xorl %%ebx, %%ebx\n\t"
        "call *%[fn]"
        :
        : [ph] "m"(ph), [fn] "r"(game_engine_hud_update_player)
        : "eax", "ecx", "edx", "ebx", "esi", "edi", "memory", "cc");
    }
  } else {
    __asm__ __volatile__(
      "movl %[ph], %%ecx\n\t"
      "movl $-1, %%eax\n\t"
      "xorl %%ebx, %%ebx\n\t"
      "call *%[fn]"
      :
      : [ph] "m"(player_data_handle), [fn] "r"(game_engine_hud_update_player)
      : "eax", "ecx", "edx", "ebx", "esi", "edi", "memory", "cc");
  }

  /* Dispatch vtable slot +0x14 (player_added callback) */
  void (*cb)(int) = ((void (**)(int))current_game_engine)[0x14 / 4];
  if (cb)
    cb(player_data_handle);
}

/* game_engine_update (0xaf370)
 *
 * The game-engine state block at 0x5aa720..0x5aa730 overlaps the KB's
 * 16-bit game_engine_variant_index declaration at 0x5aa730, so this lift
 * keeps the 32-bit phase fields on raw addresses until the wider layout is
 * established across the whole subsystem.
 */
void game_engine_update(void)
{
  data_iter_t iter;
  char *player;
  int player_handle;
  char *player_datum;
  int unit_handle;
  object_data_t *unit;
  int object_iter[4];
  void (**vtable)(void);

  if (!current_game_engine)
    return;

  game_engine_score_tick();
  game_engine_remove_dropped_weapons();
  game_engine_spawn_equipment();
  game_engine_periodic_equipment_spawn();

  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    player_handle = iter.datum_handle;

    if (current_game_engine && player_handle != NONE &&
        ((*(uint32_t *)0x456b18 & 8) != 0)) {
      player_datum = (char *)datum_get(player_data, player_handle);
      unit_handle = *(int *)(player_datum + 0x34);
      if (unit_handle != NONE) {
        unit = (object_data_t *)object_get_and_verify_type(unit_handle, 3);
        unit->unk_148 = 0.0f;
        unit->unk_140 = 0.0f;
      }
    }

    if (current_game_engine) {
      vtable = (void (**)(void))current_game_engine;
      if (((*(uint32_t *)0x456b18 & 0x10) != 0 ||
           (vtable[32] &&
            ((char (*)(int, int))vtable[32])(player_handle, 1))) &&
          *(int *)((char *)datum_get(player_data, player_handle) + 0x34) !=
            NONE) {
        player_set_respawn_timer(player_handle, 0, 0xf);
      }
    }

    game_engine_score_update_player(player_handle);

    vtable = (void (**)(void))current_game_engine;
    if (vtable[13])
      ((void (*)(int))vtable[13])(player_handle);
  }

  vtable = (void (**)(void))current_game_engine;
  if (vtable[17])
    ((void (*)(void))vtable[17])();

  switch (*(int *)0x5aa730) {
  case 0:
    if (game_engine_game_over()) {
      game_engine_start_over();
      return;
    }
    break;

  case 1:
    if (*(float *)0x5aa728 <= 2.0f && (*(uint32_t *)0x5aa720 & 0x10) == 0) {
      game_sound_set_music_volume((const char *)0x25386f, 0.0f, 0x1e);
      game_sound_set_music_volume("ambient_nature", 0.2f, 0x1e);
      game_sound_set_music_volume("ambient_machinery", 0.2f, 0x1e);
      game_sound_set_music_volume("ambient_computers", 0.2f, 0x1e);
      *(uint32_t *)0x5aa720 |= 0x10;
    }

    *(float *)0x5aa728 -= 1.0f / 30.0f;
    if (*(float *)0x5aa728 <= 0.0f) {
      *(float *)0x5aa72c = 0.0f;
      *(int *)0x5aa730 = 2;
      *(float *)0x5aa728 = 5.0f;

      data_iterator_new(&iter, player_data);
      while ((player = (char *)data_iterator_next(&iter)) != NULL) {
        unit_handle = *(int *)(player + 0x34);
        if (unit_handle != NONE)
          unit_set_actively_controlled_flag(unit_handle);
        if (*(int16_t *)(player + 2) != NONE)
          scenario_switch_structure_bsp(*(int16_t *)(player + 2));
      }

      object_iterator_new(object_iter, 2, 0);
      while (object_iterator_next(object_iter) != NULL)
        object_delete(object_iter[2]);

      void *server = network_game_server_get();
      if (server != NULL) {
        network_server_manager_game_over(server);
        return;
      }
    }
    break;

  case 2:
  case 3:
    break;

  default:
    display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\game\\game_engine.c",
                   0x985, true);
    system_exit(-1);
  }
}
