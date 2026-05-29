#include "x87_math.h"

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

/* game_engine_player_count (0xa83a0)
 *
 * Count the number of player datums in the player table.
 */
int game_engine_player_count(void)
{
  data_iter_t iter;
  int count;

  count = 0;
  data_iterator_new(&iter, player_data);
  while (data_iterator_next(&iter) != NULL)
    count++;
  return count;
}

/* sort_statistic_buffer (0xa8440)
 *
 * qsort-style comparator. Compares field at offset +4 of two entries.
 * Returns -1 if param_2's field < param_1's field (descending sort),
 * 1 if greater, 0 if equal. */
int sort_statistic_buffer(int param_1, int param_2)
{
  int a;
  int b;
  int result;

  a = *(int *)(param_1 + 4);
  b = *(int *)(param_2 + 4);
  result = 0;
  if (b < a)
    return -1;
  if (b > a)
    result = 1;
  return result;
}

/* Initialize three 4-float color vectors (stored as integer hex)
 * used by the post-game scoreboard highlight rendering.
 * param_1 = primary highlight color (pinkish: 1.0, 0.459, 0.729, 1.0)
 * param_2 = secondary highlight color (white: 1.0, 1.0, 1.0, 0.0)
 * param_3 = tertiary highlight color (off-white: 1.0, 0.979, 0.961, 0.961) */
void FUN_000a8580(unsigned int *param_1, unsigned int *param_2,
                  unsigned int *param_3)
{
  param_1[1] = 0x3eeaeaeb;
  param_1[2] = 0x3f3ababb;
  param_1[3] = 0x3f800000;
  *param_1 = 0x3f800000;
  param_2[1] = 0x3f800000;
  param_2[2] = 0x3f800000;
  param_2[3] = 0;
  *param_2 = 0x3f800000;
  param_3[1] = 0x3f7ae148;
  param_3[2] = 0x3f75c28f;
  param_3[3] = 0x3f75c28f;
  *param_3 = 0x3f800000;
}

/* Check whether an object is a live biped whose controlling player
 * differs from the value pointed to by param_2. Returns 1 (true) if
 * the object is alive, is a biped (type 0), does not have the
 * equipment-despawn flag (0xb6 bit 2), and is controlled by a player
 * other than *param_2. Used by game-engine iteration filters. */
bool FUN_000a85d0(int param_1, int *param_2)
{
  int target;
  bool result;
  char *obj;

  target = *param_2;
  result = false;
  obj = (char *)object_get_and_verify_type(param_1, -1);
  if ((*(uint8_t *)(obj + 4) & 1) == 0 &&
      (1 << (*(uint8_t *)(obj + 0x64) & 0x1f) & 1U) != 0 &&
      (*(uint8_t *)(obj + 0xb6) & 4) == 0) {
    if (player_index_from_unit_index(param_1) != target) {
      result = true;
    }
  }
  return result;
}

/* get_postgame_hilite_colors (0xa8630)
 *
 * Dispatch to vtable slot 12 (offset 0x30) of the current game engine.
 * No frame pointer in the original binary. */
void get_postgame_hilite_colors(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[12])
      vtable[12]();
  }
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

/* game_engine_load_stage (0xa8a20)
 *
 * Load a game stage. If param_1 is non-null and matches the current
 * multiplayer map name, skip the map name update. Otherwise update
 * the map name from the buffer at 0x5aa760, set the game variant
 * from 0x5aa7a0, and reset the map if not in a network game. */
void game_engine_load_stage(const char *param_1)
{
  if (param_1 == NULL || csstrcmp((const char *)0x5aa760, param_1) != 0) {
    main_set_multiplayer_map_name((const char *)0x5aa760);
  }
  game_set_game_variant((game_variant_t *)0x5aa7a0);
  if (!network_game_in_progress()) {
    main_reset_map();
  }
}

/* game_engine_playlist_begin (0xa8a70)
 *
 * Begin a playlist: set the multiplayer map name from 0x5aa760,
 * set the game variant from 0x5aa7a0, and reset the map if not
 * in a network game. No frame pointer in the original binary. */
void game_engine_playlist_begin(void)
{
  main_set_multiplayer_map_name((const char *)0x5aa760);
  game_set_game_variant((game_variant_t *)0x5aa7a0);
  if (!network_game_in_progress()) {
    main_reset_map();
  }
}

/* Copy the current game variant and map name into the provided buffers. */
bool game_engine_get_current_stage(void *game_variant_dst, void *map_name_dst)
{
  assert_halt(game_variant_dst && map_name_dst);
  csmemcpy(game_variant_dst, (void *)0x5aa7a0, 0x68);
  csstrncpy((char *)map_name_dst, (char *)0x5aa760, 0x3f);
  *((char *)map_name_dst + 0x3f) = 0;
  return true;
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

/* game_engine_player_damaged_player (0xa8b50)
 *
 * Asserts that dead_player_index is valid, then dispatches to vtable
 * slot 0x5c/4 (slot 23) of current_game_engine with all three params.
 */
void game_engine_player_damaged_player(int param_1, int dead_player_index,
                                       int param_3)
{
  assert_halt(dead_player_index != NONE);
  if (current_game_engine) {
    void (*fn)(int, int, int) =
      ((void (**)(int, int, int))current_game_engine)[0x5c / 4];
    if (fn)
      fn(param_1, dead_player_index, param_3);
  }
}

/* game_engine_is_player_leading (0xa8ba0)
 *
 * Returns true if the given player has the highest kill count among all
 * non-eliminated players.  Ties are broken by datum handle index (lower
 * index wins).  Only checked when the player has no object and
 * elimination mode is active (0x456b20 != 0).
 *
 * Register arg: player_handle in EDI.
 */
bool game_engine_is_player_leading(int player_handle)
{
  char *self;
  char *other;
  data_iter_t iter;
  bool leading;

  self = (char *)datum_get(player_data, player_handle);
  if (*(char *)0x456b20 == 0 || *(int *)(self + 0x34) != NONE)
    return false;

  leading = true;
  data_iterator_new(&iter, player_data);
  while ((other = (char *)data_iterator_next(&iter)) != NULL) {
    if (*(int *)(other + 0x34) != NONE)
      continue;
    if (other == self)
      continue;
    if (*(int *)(other + 0x84) > *(int *)(self + 0x84) ||
        (*(int *)(other + 0x84) == *(int *)(self + 0x84) &&
         (player_handle & 0xffff) > (iter.datum_handle & 0xffff)))
      leading = false;
  }
  return leading;
}

/* game_engine_player_is_out_of_lives (0xa8c40)
 *
 * Returns true if the player has no vehicle (object +0x34 == NONE) and
 * their death count (+0xaa) meets or exceeds the lives threshold at
 * 0x456b30.  Returns false when the threshold is zero (unlimited lives).
 */
bool game_engine_player_is_out_of_lives(int player_handle)
{
  bool result;
  char *player;

  result = false;
  if (*(int *)0x456b30 > 0) {
    player = (char *)datum_get(*(void **)0x5aa6d4, player_handle);
    if (*(int *)(player + 0x34) == NONE &&
        (int)*(short *)(player + 0xaa) >= *(int *)0x456b30)
      result = true;
  }
  return result;
}

/* game_engine_player_get_team_index (0xa8d80)
 *
 * Asserts that current_game_engine is non-NULL.  If the engine has a
 * team-count override at vtable+0x74, returns 1.  Otherwise returns the
 * player's team index (offset +2, a short) modulo 2.
 */
unsigned int game_engine_player_get_team_index(int player_handle)
{
  char *player;
  unsigned int team;

  assert_halt_msg(current_game_engine, "game_engine");
  if (*(int *)((char *)current_game_engine + 0x74) == 0) {
    player = (char *)datum_get(*(void **)0x5aa6d4, player_handle);
    team = (int)*(short *)(player + 2) % 2;
    return team;
  }
  return 1;
}

/* game_engine_prespawn_player_update (0xa8df0)
 *
 * If the engine has a vtable slot at +0x6c, tail-calls it with the
 * player handle.  Otherwise computes team = team_index % 2 and writes
 * it to the player datum at +0x20.
 */
void game_engine_prespawn_player_update(int player_handle)
{
  char *player;
  unsigned int team;

  if (current_game_engine) {
    void (*fn)(int) = ((void (**)(int))current_game_engine)[0x6c / 4];
    if (fn) {
      fn(player_handle);
      return;
    }
    player = (char *)datum_get(*(void **)0x5aa6d4, player_handle);
    team = (int)*(short *)(player + 2) % 2;
    *(unsigned int *)(player + 0x20) = team;
  }
}

bool game_engine_running(void)
{
  return current_game_engine != 0;
}

bool game_engine_can_score(void)
{
  return *(int *)0x456b60 == 0 || *(int *)0x5aa730 == 0;
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
  bool result;
  char *seat_obj;
  int (*gate)(int, int);

  result = true;

  if (!current_game_engine)
    return result;

  seat_obj = (char *)object_try_and_get_and_verify_type(seat_object_handle, 4);
  if (!seat_obj)
    return result;

  if (!weapon_is_flag(seat_object_handle))
    return result;

  /* If bit 0x20 of object flags was already set, clear it and call
   * vtable slot +0x40 (notify-of-seat-exit). */
  if (*(uint32_t *)(seat_obj + 0x1dc) & 0x20) {
    void (*slot_0x40)(int) = ((void (**)(int))current_game_engine)[0x40 / 4];
    *(uint32_t *)(seat_obj + 0x1dc) &= ~0x20u;
    if (slot_0x40)
      slot_0x40(seat_object_handle);
  }

  /* Set bit 0x20 to mark seat as occupied */
  *(uint32_t *)(seat_obj + 0x1dc) |= 0x20;

  /* Check per-engine entry gate at vtable slot +0x3c */
  gate = ((int (**)(int, int))current_game_engine)[0x3c / 4];
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

/* get_flag_definition_index (0xa92b0)
 *
 * Returns the flag definition tag index from the first multiplayer
 * information element (game_globals+0x164, element size 0xa0, offset 0xc).
 */
int get_flag_definition_index(void)
{
  void *block;
  char *element;

  global_scenario_get();
  block = (char *)game_globals_get() + 0x164;
  element = (char *)tag_block_get_element(block, 0, 0xa0);
  return *(int *)(element + 0xc);
}

/* get_ball_definition_index (0xa92e0)
 *
 * Returns the ball definition tag index from the first multiplayer
 * information element (game_globals+0x164, element size 0xa0, offset 0x58).
 */
int get_ball_definition_index(void)
{
  void *block;
  char *element;

  global_scenario_get();
  block = (char *)game_globals_get() + 0x164;
  element = (char *)tag_block_get_element(block, 0, 0xa0);
  return *(int *)(element + 0x58);
}

/* game_engine_switch_to_postgame (0xa9310)
 *
 * If the postgame state (0x5aa730) is 0, attempts to get the network
 * game server. If non-null, transitions to state 1 with a 7-second timer
 * (0x40e00000 = 7.0f). Otherwise jumps directly to state 3. */
void game_engine_switch_to_postgame(void)
{
  int iVar1;

  if (*(int *)0x5aa730 == 0) {
    iVar1 = (int)network_game_server_get();
    if (iVar1 != 0) {
      *(int *)0x5aa730 = 1;
      *(int *)0x5aa728 = 0x40e00000;
      return;
    }
    *(int *)0x5aa730 = 3;
  }
}

/* Returns pointer to the game engine variant data structure at 0x456af8
 * (0xa9350). */
void *game_engine_get_variant(void)
{
  return (void *)0x456af8;
}

/* game_engine_get_goal_in_use (0xa9360)
 *
 * Returns whether the goal at the given index is in use.
 * Reads from global_goals array (stride 0x20, base 0x456704). */
char game_engine_get_goal_in_use(short param_1)
{
  return *(char *)(0x456704 + (int)param_1 * 0x20);
}

/* game_engine_get_goal_position (0xa9380)
 *
 * Copies the position (3 floats) of the goal at index param_2 into param_1.
 * Asserts the goal is in use. */
void game_engine_get_goal_position(int *param_1, short param_2)
{
  int iVar1;
  char *src;

  iVar1 = (int)param_2;
  if (*(char *)(0x456704 + iVar1 * 0x20) == '\0') {
    display_assert("global_goal[index].in_use",
                   "c:\\halo\\SOURCE\\game\\game_engine.c", 0xf5c, true);
    system_exit(-1);
  }
  src = (char *)(0x4566f8 + iVar1 * 0x20);
  *param_1 = *(int *)src;
  param_1[1] = *(int *)(src + 4);
  param_1[2] = *(int *)(src + 8);
}

/* game_engine_clear_goal_position (0xa9460)
 *
 * Clears the goal entry at the given index (0x20 bytes starting at
 * 0x4566f8 + index * 0x20). */
void game_engine_clear_goal_position(short param_1)
{
  csmemset((char *)(0x4566f8 + (int)param_1 * 0x20), 0, 0x20);
}

/* game_engine_has_shield (0xa95f0)
 *
 * Returns true (1) if the game engine is inactive or player_index is NONE.
 * Otherwise returns bit 3 of the engine flags (0x456b18), inverted. */
char game_engine_has_shield(int param_1)
{
  char result;

  result = 1;
  if (*(int *)0x456b60 != 0 && param_1 != -1) {
    result = (~(*(int *)0x456b18 >> 3)) & 1;
  }
  return result;
}

/* list_index_to_weapon_definition_index (0xa9680)
 *
 * Returns the weapon definition tag index for a given list index by reading
 * game_globals+0x14c element at param_1 (size 0x10, offset 0xc).
 * Returns -1 if param_1 is -1. */
int list_index_to_weapon_definition_index(int param_1)
{
  int result;
  int iVar1;

  result = -1;
  if (param_1 != -1) {
    iVar1 = (int)game_globals_get();
    result = *(int *)((char *)tag_block_get_element((void *)(iVar1 + 0x14c),
                                                    param_1, 0x10) +
                      0xc);
  }
  return result;
}

/* game_engine_man_out (0xa9900)
 *
 * Returns true if the player is "out" (eliminated). A player is out if:
 *   - their quit flag (player+0xd1) is set, OR
 *   - lives are limited (0x456b30 > 0), they have no biped (player+0x34 == -1),
 *     AND their death count (player+0xaa) >= the lives limit, OR
 *   - they are leading (game_engine_is_player_leading). */
char game_engine_man_out(int param_1)
{
  char *player;

  player = (char *)datum_get(player_data, param_1);
  if (*(char *)(player + 0xd1) == '\0') {
    if (*(int *)0x456b30 > 0) {
      player = (char *)datum_get(player_data, param_1);
      if (*(int *)(player + 0x34) == -1 &&
          (int)*(short *)(player + 0xaa) >= *(int *)0x456b30) {
        return 1;
      }
    }
    if (game_engine_is_player_leading(param_1) == '\0') {
      return 0;
    }
  }
  return 1;
}

/* game_engine_state_message (0xa9970)
 *
 * Sets the player's state message fields (player+0x74 and player+0x78)
 * from param_2 and param_3. */
void game_engine_state_message(int param_1, int param_2, int param_3)
{
  char *player;

  player = (char *)datum_get(player_data, param_1);
  *(int *)(player + 0x74) = param_2;
  *(int *)(player + 0x78) = param_3;
}

/* game_engine_player_depower_active_camo (0xa9aa0)
 *
 * If the player has a biped that is a unit (type mask 3) and has the
 * active camo bit (byte 0x1b4, bit 4) set, sets the camo power field
 * (offset 0x32c) to 0.5f (0x3f000000). */
void game_engine_player_depower_active_camo(int param_1)
{
  char *player;
  char *unit;

  if (param_1 != -1) {
    player = (char *)datum_get(player_data, param_1);
    if (*(int *)(player + 0x34) != -1) {
      unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
      if ((*(unsigned char *)(unit + 0x1b4) & 0x10) != 0) {
        *(int *)(unit + 0x32c) = 0x3f000000;
      }
    }
  }
}

/* Check scenario netgame flags (scenario+0x378, element size 0x94) for
 * duplicate entries: two flags with the same type (param_1) AND same
 * team (offset 0x12). For each duplicate pair found, calls error()
 * with the supplied format string and the team index. Triangle-loop:
 * outer i, inner j = i+1..count-1. */
void FUN_000aa010(short param_1, const char *param_2)
{
  char *scenario_ptr;
  int *flags_block;
  int i;
  int j;
  short outer_next;
  short inner_idx;
  char *elem_i;
  char *elem_j;

  scenario_ptr = (char *)global_scenario_get();
  flags_block = (int *)(scenario_ptr + 0x378);
  if (0 < *flags_block) {
    i = 0;
    outer_next = 1;
    do {
      elem_i = (char *)tag_block_get_element(flags_block, i, 0x94);
      if (param_1 == *(short *)(elem_i + 0x10)) {
        j = (int)outer_next;
        inner_idx = outer_next;
        if (j < *flags_block) {
          do {
            elem_j = (char *)tag_block_get_element(flags_block, j, 0x94);
            if (param_1 == *(short *)(elem_j + 0x10) &&
                *(short *)(elem_j + 0x12) == *(short *)(elem_i + 0x12)) {
              error(2, param_2, (int)*(short *)(elem_j + 0x12));
            }
            inner_idx = inner_idx + 1;
            j = (int)inner_idx;
          } while (j < *flags_block);
        }
      }
      i = (int)outer_next;
      outer_next = outer_next + 1;
    } while (i < *flags_block);
  }
}

/* game_engine_slayer_default (0xaa190)
 *
 * Initialise a game_variant_t with default Slayer settings.
 * Engine type 2 (slayer), score limit 300, 15 lives, 2 weapon sets,
 * 1.0 speed. */
game_variant_t *game_engine_slayer_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 2;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x34) = 0x12c;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 0xf;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_elimination_default (0xaa2b0)
 *
 * Default Elimination variant. Engine type 2, score limit 300,
 * 1 round, 25 lives, 2 weapon sets, 1.0 speed. */
game_variant_t *game_engine_elimination_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 2;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x34) = 0x12c;
  *(int32_t *)((char *)variant + 0x38) = 1;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 0x19;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_team_slayer_default (0xaa580)
 *
 * Default Team Slayer variant. Engine type 2, team play enabled,
 * score/time limits 300, 50 lives, 2 weapon sets, 1.0 speed. */
game_variant_t *game_engine_team_slayer_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 2;
  *(uint8_t *)((char *)variant + 0x1c) = 1;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x30) = 0x12c;
  *(int32_t *)((char *)variant + 0x34) = 0x12c;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 0x32;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_oddball_default (0xaa610)
 *
 * Default Oddball variant. Engine type 3, score/time 150 each,
 * 2 lives, 1 weapon set, ball indicator on, 1.0 speed. */
game_variant_t *game_engine_oddball_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 3;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x24) = 1;
  *(int32_t *)((char *)variant + 0x30) = 0x96;
  *(int32_t *)((char *)variant + 0x34) = 0x96;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 2;
  *(int32_t *)((char *)variant + 0x48) = 1;
  *(uint8_t *)((char *)variant + 0x4d) = 1;
  *(int32_t *)((char *)variant + 0x60) = 1;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_team_oddball_default (0xaa6a0)
 *
 * Default Team Oddball variant. Engine type 3, team play,
 * bitmask 0x23, score limit 300, time limit 150, 2 lives,
 * 1 weapon set, ball indicator on, 1.0 speed. */
game_variant_t *game_engine_team_oddball_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 3;
  *(uint8_t *)((char *)variant + 0x1c) = 1;
  *(int32_t *)((char *)variant + 0x20) = 0x23;
  *(int32_t *)((char *)variant + 0x24) = 1;
  *(int32_t *)((char *)variant + 0x30) = 0x12c;
  *(int32_t *)((char *)variant + 0x34) = 0x96;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 2;
  *(int32_t *)((char *)variant + 0x48) = 1;
  *(int32_t *)((char *)variant + 0x60) = 1;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_accumulation_default (0xaa7c0)
 *
 * Default Accumulation variant. Engine type 3, bitmask 2,
 * score/time 150 each, 5 lives, modifier 2, HUD flags 0x10,
 * 1 score unit, 1.0 speed. */
game_variant_t *game_engine_accumulation_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 3;
  *(int32_t *)((char *)variant + 0x20) = 2;
  *(int32_t *)((char *)variant + 0x24) = 2;
  *(int32_t *)((char *)variant + 0x30) = 0x96;
  *(int32_t *)((char *)variant + 0x34) = 0x96;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 5;
  *(int32_t *)((char *)variant + 0x48) = 1;
  *(int32_t *)((char *)variant + 0x5c) = 1;
  *(int32_t *)((char *)variant + 0x60) = 0x10;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_stalker_default (0xaa900)
 *
 * Default Stalker variant. Engine type 3, bitmask 2,
 * score/time 150 each, 10 lives, multiple weapon/scoring
 * fields, 1.0 speed. */
game_variant_t *game_engine_stalker_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 3;
  *(int32_t *)((char *)variant + 0x20) = 2;
  *(int32_t *)((char *)variant + 0x30) = 0x96;
  *(int32_t *)((char *)variant + 0x34) = 0x96;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 0xa;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(int32_t *)((char *)variant + 0x50) = 2;
  *(int32_t *)((char *)variant + 0x54) = 1;
  *(int32_t *)((char *)variant + 0x58) = 3;
  *(int32_t *)((char *)variant + 0x5c) = 2;
  *(int32_t *)((char *)variant + 0x60) = 1;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_king_default (0xaa9a0)
 *
 * Default King of the Hill variant. Engine type 4,
 * score/time 150 each, 2 lives, modifier 1, 1.0 speed. */
game_variant_t *game_engine_king_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 4;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x24) = 1;
  *(int32_t *)((char *)variant + 0x30) = 0x96;
  *(int32_t *)((char *)variant + 0x34) = 0x96;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 2;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_team_king_default (0xaab30)
 *
 * Default Team King variant. Engine type 4, team play,
 * score limit 300, time limit 150, 2 lives, modifier 1,
 * hill indicator on, 1.0 speed. */
game_variant_t *game_engine_team_king_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 4;
  *(uint8_t *)((char *)variant + 0x1c) = 1;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x24) = 1;
  *(int32_t *)((char *)variant + 0x30) = 0x12c;
  *(int32_t *)((char *)variant + 0x34) = 0x96;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 2;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(uint8_t *)((char *)variant + 0x4c) = 1;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_ctf_default (0xaabc0)
 *
 * Default Capture The Flag variant. Engine type 1, team play,
 * score limit 300, time limit 150, 3 lives, modifier 1,
 * 2 weapon sets, 1.0 speed. */
game_variant_t *game_engine_ctf_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 1;
  *(uint8_t *)((char *)variant + 0x1c) = 1;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x24) = 1;
  *(int32_t *)((char *)variant + 0x30) = 0x12c;
  *(int32_t *)((char *)variant + 0x34) = 0x96;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 3;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_ironctf_default (0xaad70)
 *
 * Default Iron CTF variant. Engine type 1, team play,
 * score limit 450, time limit 150, 3 lives, modifier 1,
 * 4 weapon sets, flag indicator on, 2.0 speed. */
game_variant_t *game_engine_ironctf_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 1;
  *(uint8_t *)((char *)variant + 0x1c) = 1;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x24) = 1;
  *(int32_t *)((char *)variant + 0x30) = 0x1c2;
  *(int32_t *)((char *)variant + 0x34) = 0x96;
  *(int32_t *)((char *)variant + 0x3c) = 0x40000000;
  *(int32_t *)((char *)variant + 0x40) = 3;
  *(int32_t *)((char *)variant + 0x48) = 4;
  *(uint8_t *)((char *)variant + 0x4e) = 1;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_race_default (0xaae00)
 *
 * Default Race variant. Engine type 5, modifier 1,
 * time limit 300, 3 lives, 2 weapon sets, 1.0 speed. */
game_variant_t *game_engine_race_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 5;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x24) = 1;
  *(int32_t *)((char *)variant + 0x34) = 0x12c;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 3;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_rally_default (0xaae90)
 *
 * Default Rally variant. Engine type 5, modifier 1,
 * time limit 300, 15 lives, 2 weapon sets + mode 2,
 * 1.0 speed. */
game_variant_t *game_engine_rally_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 5;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x24) = 1;
  *(int32_t *)((char *)variant + 0x34) = 0x12c;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 0xf;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(int32_t *)((char *)variant + 0x4c) = 2;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* game_engine_team_race_default (0xaaf20)
 *
 * Default Team Race variant. Engine type 5, team play,
 * modifier 1, time limit 300, 3 lives, 2 weapon sets,
 * 1.0 speed. */
game_variant_t *game_engine_team_race_default(game_variant_t *variant)
{
  csmemset(variant, 0, 0x68);
  *(int32_t *)((char *)variant + 0x18) = 5;
  *(uint8_t *)((char *)variant + 0x1c) = 1;
  *(int32_t *)((char *)variant + 0x20) = 3;
  *(int32_t *)((char *)variant + 0x24) = 1;
  *(int32_t *)((char *)variant + 0x34) = 0x12c;
  *(int32_t *)((char *)variant + 0x3c) = 0x3f800000;
  *(int32_t *)((char *)variant + 0x40) = 3;
  *(int32_t *)((char *)variant + 0x48) = 2;
  *(int16_t *)((char *)variant + 0x64) = 1;
  return variant;
}

/* Check if any of the 4 gamepads has the specified button pressed.
 * button_index passed in EDI (register arg). Returns true on first hit. */
bool game_engine_check_input_button(int button_index /* @<edi> */)
{
  int i;

  for (i = 0; i < 4; i++) {
    void *state = input_get_gamepad_state(i);
    if (state != NULL && *((char *)state + 0x10 + button_index) != 0)
      return true;
  }
  return false;
}

/* Validate and clamp all fields of a game variant struct to legal ranges.
 * Normalizes booleans, clamps integers, and clamps the speed float to
 * [0.5f, 2.0f]. Logs an error if any field was changed. */
void game_engine_variant_cleanup(game_variant_t *variant)
{
  char saved[0x68];
  int *v = (int *)variant;
  int game_type;

  csmemcpy(saved, variant, 0x68);

  *(int16_t *)((char *)v + 0x16) = 0;

  game_type = v[6];
  if (game_type < 1)
    game_type = 1;
  else if (game_type > 5)
    game_type = 5;
  v[6] = game_type;

  *(uint8_t *)((char *)v + 0x1c) = (*(uint8_t *)((char *)v + 0x1c) != 0);
  *(uint8_t *)((char *)v + 0x28) = (*(uint8_t *)((char *)v + 0x28) != 0);

  if (v[0xb] <= 0)
    v[0xb] = 0;
  if (v[0xc] <= 0)
    v[0xc] = 0;
  if (v[0xd] <= 0)
    v[0xd] = 0;
  if (v[0xe] <= 0)
    v[0xe] = 0;

  {
    float scale = *(float *)((char *)v + 0x3c);
    if (scale < *(float *)0x25337c)
      scale = *(float *)0x25337c;
    else if (scale > *(float *)0x2533d8)
      scale = *(float *)0x2533d8;
    *(float *)((char *)v + 0x3c) = scale;
  }

  if (v[0x11] < 0)
    v[0x11] = 0;
  else if (v[0x11] > 10)
    v[0x11] = 10;

  if (v[0x12] < 0)
    v[0x12] = 0;
  else if (v[0x12] > 4)
    v[0x12] = 4;

  if (game_type == 1) {
    *(uint8_t *)((char *)v + 0x4c) = (*(uint8_t *)((char *)v + 0x4c) != 0);
    *(uint8_t *)((char *)v + 0x4d) = (*(uint8_t *)((char *)v + 0x4d) != 0);
    *(uint8_t *)((char *)v + 0x4e) = (*(uint8_t *)((char *)v + 0x4e) != 0);
    *(uint8_t *)((char *)v + 0x1c) = 1;
    *(uint8_t *)((char *)v + 0x4f) = (*(uint8_t *)((char *)v + 0x4f) != 0);
    if (v[0x14] < 0)
      v[0x14] = 0;
  } else if (game_type == 2) {
    *(uint8_t *)((char *)v + 0x4c) = (*(uint8_t *)((char *)v + 0x4c) != 0);
    *(uint8_t *)((char *)v + 0x4d) = (*(uint8_t *)((char *)v + 0x4d) != 0);
    *(uint8_t *)((char *)v + 0x4e) = (*(uint8_t *)((char *)v + 0x4e) != 0);
  }

  if (csmemcmp(saved, variant, 0x68) != 0) {
    char before[0x68];
    csmemcpy(before, saved, 0x68);
    csmemcpy(before, variant, 0x68);
    error(
      2,
      "NETGAME CODE FAILURE: game_engine_variant_cleanup changed the variant");
  }
}

/* Returns true if the game has not entered a restart/game-over state. */
bool game_engine_allow_pause(void)
{
  return *(int *)0x5aa730 == 0;
}

/* game_engine_load_sounds (0xab730)
 *
 * Preloads game-engine related sound tags from game globals.
 * Selection depends on game_engine_variant_index (0x456b40) and game type
 * (0x456b10). The final 10 entries are mapped through game_engine_remap_weapon
 * before loading.
 */
void game_engine_load_sounds(void)
{
  char *globals;
  char *engine_globals;
  char *entry;
  int sound_tags[10];
  int i;
  void (*load_sound)(int) = (void (*)(int))0x13dda0;
  int (*map_sound_tag)(int) = (int (*)(int))0x0a9770;

  globals = (char *)game_globals_get();
  engine_globals = (char *)tag_block_get_element(globals + 0x164, 0, 0xa0);

  if (*(int *)0x456b40 == 2) {
    entry = (char *)tag_block_get_element(engine_globals + 0x20, 0, 0x10);
    load_sound(*(int *)(entry + 0xc));
  } else if (*(int *)0x456b40 == 3) {
    entry = (char *)tag_block_get_element(engine_globals + 0x20, 1, 0x10);
    load_sound(*(int *)(entry + 0xc));
  } else if (*(int *)0x456b40 == 4) {
    entry = (char *)tag_block_get_element(engine_globals + 0x20, 2, 0x10);
    load_sound(*(int *)(entry + 0xc));
  } else {
    entry = (char *)tag_block_get_element(engine_globals + 0x20, 0, 0x10);
    load_sound(*(int *)(entry + 0xc));
    entry = (char *)tag_block_get_element(engine_globals + 0x20, 1, 0x10);
    load_sound(*(int *)(entry + 0xc));
    entry = (char *)tag_block_get_element(engine_globals + 0x20, 2, 0x10);
    load_sound(*(int *)(entry + 0xc));
  }

  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 0xc, 0x10);
  load_sound(*(int *)(entry + 0xc));

  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 0xd, 0x10);
  load_sound(*(int *)(entry + 0xc));

  if (*(int *)0x456b10 == 3) {
    globals = (char *)game_globals_get();
    entry = (char *)tag_block_get_element(globals + 0x14c, 10, 0x10);
    load_sound(*(int *)(entry + 0xc));
  }

  if (*(int *)0x456b10 == 1) {
    globals = (char *)game_globals_get();
    entry = (char *)tag_block_get_element(globals + 0x14c, 0xb, 0x10);
    load_sound(*(int *)(entry + 0xc));
  }

  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 0, 0x10);
  sound_tags[0] = *(int *)(entry + 0xc);
  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 1, 0x10);
  sound_tags[1] = *(int *)(entry + 0xc);
  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 2, 0x10);
  sound_tags[2] = *(int *)(entry + 0xc);
  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 3, 0x10);
  sound_tags[3] = *(int *)(entry + 0xc);
  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 4, 0x10);
  sound_tags[4] = *(int *)(entry + 0xc);
  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 5, 0x10);
  sound_tags[5] = *(int *)(entry + 0xc);
  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 6, 0x10);
  sound_tags[6] = *(int *)(entry + 0xc);
  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 7, 0x10);
  sound_tags[7] = *(int *)(entry + 0xc);
  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 8, 0x10);
  sound_tags[8] = *(int *)(entry + 0xc);
  globals = (char *)game_globals_get();
  entry = (char *)tag_block_get_element(globals + 0x14c, 9, 0x10);
  sound_tags[9] = *(int *)(entry + 0xc);

  i = 0;
  while (i < 10) {
    load_sound(map_sound_tag(sound_tags[i]));
    i++;
  }
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
    {
      int engine_type = *(int32_t *)((char *)variant + 0x18);
      current_game_engine =
        (void *)(*(int32_t *)((char *)0x2eff98 + engine_type * 4));
    }
  }
}

/* game_engine_teams_still_playing (0xaba90)
 *
 * Returns true if at least two different teams are represented among
 * active players (i.e. competition is still alive).  Returns true
 * trivially if there are fewer than 2 players.  Skips players whose
 * quit flag (byte +0xd1) is set, and in lives-limited games also skips
 * eliminated players who are leading (they have no object and their
 * respawn count has reached the lives limit).
 */
bool game_engine_teams_still_playing(void)
{
  data_iter_t iter;
  char *player;
  int first_team;
  bool result;

  if (game_engine_player_count() < 2)
    return true;

  result = false;
  first_team = NONE;
  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    if (*(char *)(player + 0xd1) != 0)
      continue;

    if (*(int *)(player + 0x34) == NONE) {
      if (game_engine_is_player_leading(iter.datum_handle))
        continue;
      if (*(int32_t *)0x456b30 >= 1) {
        char *p = (char *)datum_get(player_data, iter.datum_handle);
        if (*(int *)(p + 0x34) == NONE &&
            (int)*(int16_t *)(p + 0xaa) >= *(int32_t *)0x456b30)
          continue;
      }
    }

    assert_halt(*(int *)(player + 0x20) != NONE);
    if (*(int *)(player + 0x20) != first_team) {
      if (first_team != NONE)
        return true;
      first_team = *(int *)(player + 0x20);
    }
  }
  return result;
}

/* Returns true if the game is over: the engine exists but all remaining
 * players/teams are on the same side (no competition left). */
bool game_engine_game_over(void)
{
  if (current_game_engine) {
    if (!game_engine_teams_still_playing())
      return true;
  }
  return false;
}

/* game_engine_get_score_hud_text (0xac4e0)
 *
 * Format a default HUD message for a scoring/death/status event.
 * param_2 selects the message type.  When the engine vtable has a
 * slot-0x7c callback that returns true, param_2 values 7-12 are
 * remapped to their "personal" equivalents (14-19) and the engine's
 * slot-0x48 callback is called to get a score value for the (%d) suffix.
 *
 * Register args: buffer in EDI, buffer_capacity in ESI.
 */
bool game_engine_get_score_hud_text(int player_handle, int param_2,
                                    int hud_player, wchar_t *buffer,
                                    int buffer_capacity)
{
  char *player_datum;
  char *other;
  int score;
  bool result;

  result = true;
  player_datum = (char *)datum_get(player_data, player_handle);
  score = 0;

  if (current_game_engine) {
    bool (*has_score)(int) = ((bool (**)(int))current_game_engine)[0x7c / 4];
    if (has_score && has_score(1)) {
      int (*get_score)(int, int);
      switch (param_2) {
      case 7:
        param_2 = 0x10;
        break;
      case 8:
        param_2 = 0x13;
        break;
      case 9:
        param_2 = 0xf;
        break;
      case 10:
        param_2 = 0xe;
        break;
      case 11:
        param_2 = 0x12;
        break;
      case 12:
        param_2 = 0x11;
        break;
      default:
        if (param_2 < 0xe || param_2 > 0x13)
          goto main_switch;
        break;
      }
      get_score = ((int (**)(int, int))current_game_engine)[0x48 / 4];
      score = get_score(player_handle, 1);
    }
  }

main_switch:
  switch (param_2) {
  case 0:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c63c,
                    player_datum + 4);
    break;
  case 1:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c62c,
                    player_datum + 4);
    break;
  case 2:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c5ec,
                    player_datum + 4);
    break;
  case 3:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c5b4,
                    player_datum + 4);
    break;
  case 4:
    other = (char *)datum_get(player_data, hud_player);
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c58c,
                    player_datum + 4, other + 4);
    break;
  case 5:
    other = (char *)datum_get(player_data, hud_player);
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c560,
                    player_datum + 4, other + 4);
    break;
  case 6:
    datum_get(player_data, hud_player);
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c524,
                    player_datum + 4);
    break;
  case 7:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c4b0);
    game_engine_post_event(0xe);
    break;
  case 8:
    other = (char *)datum_get(player_data, hud_player);
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c440,
                    other + 4);
    break;
  case 9:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c4cc);
    game_engine_post_event(0xf);
    break;
  case 10:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c4e8);
    game_engine_post_event(0x10);
    break;
  case 11:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c45c);
    game_engine_post_event(0x12);
    break;
  case 12:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c494);
    game_engine_post_event(0x11);
    break;
  case 13:
    other = (char *)datum_get(player_data, hud_player);
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c504,
                    other + 4);
    break;
  case 14:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c41c, score);
    game_engine_post_event(0x10);
    break;
  case 15:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c3f8, score);
    game_engine_post_event(0xf);
    break;
  case 16:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c3d4, score);
    game_engine_post_event(0xe);
    break;
  case 17:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c3ac, score);
    game_engine_post_event(0x11);
    break;
  case 18:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c368, score);
    game_engine_post_event(0x12);
    break;
  case 19:
    other = (char *)datum_get(player_data, hud_player);
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c33c,
                    other + 4, score);
    break;
  case 23:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c30c);
    break;
  case 24:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c2e0);
    break;
  case 25:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c2c4,
                    hud_player);
    break;
  case 26:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c28c);
    break;
  case 27:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c258);
    break;
  case 28:
    other = (char *)datum_get(player_data, hud_player);
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c550,
                    other + 4);
    break;
  case 29:
    unicode_sprintf(buffer, buffer_capacity, (const wchar_t *)0x26c230);
    break;
  default:
    result = false;
    break;
  }
  buffer[buffer_capacity - 1] = 0;
  return result;
}

/* match_game_type (0xacb10)
 *
 * Checks whether a set of game-type entries (an array of int16_t values)
 * permits the current engine type. Used to filter scenario multiplayer
 * equipment, starting locations, and netgame flags by game mode.
 *
 * If no engine is active (current_game_engine == NULL), returns true only
 * when all entries are 0 (none/unset).
 *
 * If an engine is active, returns true if ANY entry matches:
 *   - entry == engine_type  (direct match)
 *   - entry == 0xC          (matches all engine types)
 *   - entry == 0xD          (matches all except engine type 1)
 *   - entry == 0xE          (matches all except engine types 1 and 5)
 */
/* 0xacb10 */
bool match_game_type(int player_index, int count, int16_t *entries)
{
  int i;
  bool result;

  if (current_game_engine == NULL) {
    /* No engine: true only if every entry is 0 */
    result = true;
    for (i = 0; i < count; i++) {
      result = result & (entries[i] == 0);
    }
  } else {
    /* Engine active: true if any entry matches the engine type */
    result = false;
    for (i = 0; i < count; i++) {
      int16_t entry = entries[i];
      result = result | (entry == player_index);
      if (entry == 0xC) {
        result = result | true;
      } else if (entry == 0xD) {
        result = result | (player_index != 1);
      } else if (entry == 0xE) {
        result = result | (player_index != 1 && player_index != 5);
      }
    }
  }
  return result;
}

/* game_engine_periodic_equipment_spawn (0xacbb0)
 *
 * Iterates scenario multiplayer equipment entries (+0x384, element size 0x90),
 * filters by game-mode rules, and periodically spawns equipment placements.
 */
void game_engine_periodic_equipment_spawn(void)
{
  int *equip_block;
  int16_t entry_index;
  unsigned char placement[0x88];
  int spawn_period;
  unsigned char *entry;
  int player_index;

  equip_block = (int *)((char *)global_scenario_get() + 0x384);
  entry_index = 0;

  while ((int)entry_index < *equip_block) {
    entry =
      (unsigned char *)tag_block_get_element(equip_block, entry_index, 0x90);

    player_index = -1;
    if (*(int *)0x456b60 != 0) {
      player_index = *(int *)(*(int *)0x456b60 + 4);
    }

    if (match_game_type(player_index, 4, (int16_t *)(entry + 4))) {
      int16_t period_seconds = *(int16_t *)(entry + 0xe);
      spawn_period = 900;

      if (period_seconds == 0) {
        int collection_tag = *(int *)(entry + 0x5c);
        if (collection_tag != -1) {
          char *collection_data = (char *)tag_get(0x69746d63, collection_tag);
          period_seconds = *(int16_t *)(collection_data + 0xc);
          if (period_seconds != 0) {
            spawn_period = (int)period_seconds * 30;
          }
        }
      } else {
        spawn_period = (int)period_seconds * 30;
      }

      if ((game_time_get() % spawn_period) == 0) {
#ifdef MSVC
        int tag_index = ((int(__cdecl *)(int))0xaca70)(*(int *)(entry + 0x5c));
#else
        int tag_index = ((int(__attribute__((regparm(1))) *)(int))0xaca70)(
          *(int *)(entry + 0x5c));
#endif
        object_placement_data_new(placement, tag_index, -1);
        *(int *)(placement + 0x18) = *(int *)(entry + 0x40);
        *(int *)(placement + 0x1c) = *(int *)(entry + 0x44);
        *(int *)(placement + 0x20) = *(int *)(entry + 0x48);

        {
          int object_handle = object_new(placement);
          if (object_handle != -1) {
            int object_data =
              (int)object_get_and_verify_type(object_handle, 0x1c);
            object_set_garbage_flag(object_handle, 0);
            if ((*entry & 1) != 0) {
              *(unsigned int *)(object_data + 4) |= 0x20;
            }
            *(int *)(object_data + 0x1b4) += spawn_period - 900;
          }
        }
      }
    }

    entry_index++;
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
  int phase;

  if (!current_game_engine)
    return;

  phase = *(int32_t *)0x5aa730;

  if (phase == 2) {
    float t;
    /* Phase 2: run input reset, subtract dt from countdown timer */
    rumble_clear_all_players();
    *(float *)0x5aa728 -= dt;
    /* Transition to phase 3 when timer reaches 0 */
    t = *(float *)0x5aa728;
    if (!(t < 0.0f) && !(t == 0.0f)) {
      /* still > 0, stay in phase 2 */
    } else {
      *(int32_t *)0x5aa730 = 3;
    }
  } else if (phase == 3) {
    bool ok0, ok1, ok2, ok3;
    /* Phase 3: run input reset, advance progress counter */
    rumble_clear_all_players();
    *(float *)0x5aa72c += dt;
    if (*(float *)0x5aa72c > 1.0f)
      *(float *)0x5aa72c = 1.0f;

    /* Poll four "done" conditions via @edi-indexed
     * game_engine_check_input_button. EDI indices: 0, 0xc, 1, 0xd (matching
     * disassembly order). */

    ok0 = game_engine_check_input_button(0);
    ok1 = game_engine_check_input_button(0xc);

    if (ok0 || ok1) {
      void *server = network_game_server_get();
      /* At least one input is "done": check for network server to reset */
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

    {
      void *server = network_game_server_get();
      if (server) {
        network_server_manager_pregame_start(server);
        return;
      }
      network_game_abort();
    }
  }
}

/* game_engine_hud_update_player (0xacef0)
 *
 * Per-player HUD score/message update.  Called when a player is added
 * or when a scoring event occurs to refresh the HUD text for a given
 * player.
 *
 * First tries the current game engine's vtable slot 0x64 callback to
 * produce a wchar_t message.  If that callback is null or returns false,
 * falls back to game_engine_get_score_hud_text (0xac4e0) which formats
 * a default message via a large switch on the event type (hud_player).
 *
 * If either path succeeds, null-terminates the buffer and prints it
 * via hud_print_message.
 *
 * Register args:
 *   ECX = player_handle (datum handle into player_data)
 *   EAX = hud_player (event type / -1 for init)
 *   EBX = param3 (extra context, often 0)
 */
/* 0xacef0 */
void game_engine_hud_update_player(int player_handle, int hud_player,
                                   int param3)
{
  wchar_t buffer[0x400];
  char *datum;
  bool got_text;

  datum = (char *)datum_get(player_data, player_handle);
  if (*(short *)(datum + 2) == -1)
    return;

  /* Try vtable slot 0x64 first: engine-specific HUD message callback */
  got_text = false;
  {
    bool (*vtable_fn)(int, int, int, wchar_t *, int) =
      ((bool (**)(int, int, int, wchar_t *, int))current_game_engine)[0x64 / 4];
    if (vtable_fn != NULL) {
      got_text = vtable_fn(player_handle, param3, hud_player, buffer, 0x400);
    }
  }

  /* Fall back to default score text formatter */
  if (!got_text) {
    got_text = game_engine_get_score_hud_text(player_handle, param3, hud_player,
                                              buffer, 0x400);
    if (!got_text)
      return;
  }

  /* Null-terminate the buffer at the last position and print */
  buffer[0x400 - 1] = 0;
  hud_print_message(*(unsigned short *)(datum + 2), buffer);
}

/* find_netgame_flags (0xad160)
 *
 * Search scenario netgame flags (tag block at scenario+0x378, element
 * size 0x94) for entries matching the given type and team index filters.
 * Optionally filters by radius and height around a world position.
 *
 * type/index of -1 act as wildcards (match any).  If position is NULL,
 * all spatial filtering is skipped.  If radius < 0.0f the distance check
 * is skipped; if height <= 0.0f the height check is skipped.
 *
 * Matching flag indices are written into out_indices (up to max_count).
 * Returns the number of matches found.
 */
/* 0xad160 */
int find_netgame_flags(float *position, float radius, float height,
                       int16_t type, int16_t index, int max_count,
                       int *out_indices)
{
  int result;
  int i;
  int16_t si;
  float *entry;
  int *flag_block;
  float radius_sq;

  radius_sq = radius * radius;
  result = 0;

  flag_block = (int *)((char *)global_scenario_get() + 0x378);

  si = 0;
  if (*flag_block < 1)
    return 0;

  i = 0;
  do {
    entry = (float *)tag_block_get_element(flag_block, i, 0x94);

    if ((type == -1 || type == *(int16_t *)((char *)entry + 0x10)) &&
        (index == -1 || index == *(int16_t *)((char *)entry + 0x12))) {
      /* Spatial filtering (only when position is non-NULL) */
      if (position != NULL) {
        /* Radius check: skip if radius < 0.0f */
        if (!(radius < 0.0f)) {
          float dx = position[0] - entry[0];
          float dy = position[1] - entry[1];
          float dz = position[2] - entry[2];
          if (dx * dx + dy * dy + dz * dz > radius_sq)
            goto next;
        }
        /* Height check: skip if height <= 0.0f */
        if (!(height <= 0.0f)) {
          float abs_dz = entry[2] - position[2];
          if (abs_dz < 0.0f)
            abs_dz = -abs_dz;
          if (abs_dz > height)
            goto next;
        }
      }

      /* Store match if there's room */
      if (result < max_count) {
        out_indices[result] = i;
        result++;
      }
    }
  next:
    si++;
    i = (int)si;
  } while (i < *flag_block);

  return result;
}

/* find_netgame_flag (0xad270)
 *
 * Convenience wrapper around find_netgame_flags that finds at most one matching
 * netgame flag.  Returns the index of the first match, or -1 if none.
 */
/* 0xad270 */
int find_netgame_flag(float *position, float radius, float height, int16_t type,
                      int16_t index)
{
  int result = -1;
  find_netgame_flags(position, radius, height, type, index, 1, &result);
  return result;
}

/* game_engine_get_damage_multiplier (0xad530)
 *
 * Computes a per-player damage modifier for the active game engine.
 * If a game engine is active, it normalises the engine's stored float
 * (at 0x456b34, e.g. damage multiplier) by clamping it to
 * [*(float*)0x25337c, *(float*)0x2533d8] and then inverting it
 * (1.0f / clamped).  If either player index is valid and the engine
 * exposes a vtable predicate at slot 0x80/4, the function further
 * scales the result:
 *   - if predicate(player_a, 2) is true: multiply by *(float*)0x2533ec
 *   - if predicate(player_b, 3) is true: multiply by *(float*)0x253398
 * Returns 1.0f when no engine is active (no game-type modifier).
 */
/* 0xad530 */
float game_engine_get_damage_multiplier(int player_a, int player_b)
{
  float local_8;
  char cVar1;
  int engine;
  int (*fn)(int, int);
  float engine_float;
  float clamped;

  engine = *(int *)0x456b60;
  local_8 = *(float *)0x2533c8;
  if (engine != 0) {
    engine_float = *(float *)0x456b34;
    if (engine_float < *(float *)0x25337c) {
      clamped = *(float *)0x25337c;
    } else if (engine_float > *(float *)0x2533d8) {
      clamped = *(float *)0x2533d8;
    } else {
      clamped = engine_float;
    }
    local_8 = *(float *)0x2533c8 / clamped;
  }
  if (player_a != -1 && player_b != -1 && engine != 0) {
    fn = (int (*)(int, int))(*(int *)(engine + 0x80));
    if (fn != 0) {
      cVar1 = (char)fn(player_a, 2);
      engine = *(int *)0x456b60;
      if (cVar1 != '\0') {
        local_8 = local_8 * *(float *)0x2533ec;
      }
    }
    if (engine != 0) {
      fn = (int (*)(int, int))(*(int *)(engine + 0x80));
      if (fn != 0) {
        cVar1 = (char)fn(player_b, 3);
        if (cVar1 != '\0') {
          return local_8 * *(float *)0x253398;
        }
      }
    }
  }
  return local_8;
}

/* game_engine_player_update_netgame_flag (0xad600)
 *
 * Per-player netgame flag proximity check.  Finds the nearest type-6
 * flag (teleporter sender / hill), looks up the paired type-7 flag
 * (teleporter exit / scoring target), runs a LOS check to the
 * destination, and either scores a point or teleports the player.
 */
void game_engine_player_update_netgame_flag(int player_handle)
{
  unsigned char *player;
  unsigned char *scenario;
  unsigned char *unit;
  unsigned char *goal_entry;
  unsigned char *next_goal_entry;
  int selected_goal_index;
  int next_goal_index;
  float unit_pos[3];
  float search_pos[3];
  float distance_a;
  float distance_b;
  static unsigned char los_scratch[0xac6c];
  unsigned char hit_info[0x2c];

  scenario = (unsigned char *)global_scenario_get();
  player = (unsigned char *)datum_get(*(void **)0x5aa6d4, player_handle);

  if (*(int *)(player + 0x34) == -1) {
    return;
  }

  unit =
    (unsigned char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);

  if (*(int *)(player + 0x70) != -1) {
    float *goal_pos = (float *)tag_block_get_element(
      (char *)scenario + 0x378, *(int *)(player + 0x70), 0x94);
    float dx = *(float *)(unit + 0xc) - goal_pos[0];
    float dy = *(float *)(unit + 0x10) - goal_pos[1];
    float dz = *(float *)(unit + 0x14) - goal_pos[2];
    if (dx * dx + dy * dy + dz * dz > *(float *)0x2533c8) {
      *(int *)(player + 0x70) = -1;
    }
  }

  selected_goal_index = -1;
  /* netgame_flag_find_nearest: search for type-6 flag near unit */
  find_netgame_flags((float *)(unit + 0xc), 0.5f, 0.0f, 6, -1, 1,
                     &selected_goal_index);

  if (selected_goal_index == -1 ||
      selected_goal_index == *(int *)(player + 0x70)) {
    return;
  }

  goal_entry = (unsigned char *)tag_block_get_element(
    (char *)scenario + 0x378, selected_goal_index, 0x94);

  next_goal_index = -1;
  /* netgame_flag_find_nearest: find paired type-7 flag by team index */
  find_netgame_flags(0, 0.0f, 0.0f, 7, *(short *)(goal_entry + 0x12), 1,
                     &next_goal_index);

  if (next_goal_index == -1) {
    console_printf(0, (const char *)0x26c66c,
                   (int)*(short *)(goal_entry + 0x12));
    return;
  }

  next_goal_entry = (unsigned char *)tag_block_get_element(
    (char *)scenario + 0x378, next_goal_index, 0x94);

  unit =
    (unsigned char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  unit_pos[0] = *(float *)(unit + 0x24);
  unit_pos[1] = *(float *)(unit + 0x28);
  unit_pos[2] = *(float *)(unit + 0x2c);

  player = (unsigned char *)datum_get(*(void **)0x5aa6d4, player_handle);
  biped_get_camera_height_and_offset(*(int *)(player + 0x34),
                                     (vector3_t *)&search_pos[0], &distance_b,
                                     &distance_a);

  {
    float candidate_pos[3];
    candidate_pos[0] = *(float *)(next_goal_entry + 0x0);
    candidate_pos[1] = *(float *)(next_goal_entry + 0x4);
    candidate_pos[2] = *(float *)(next_goal_entry + 0x8);

    if (FUN_0014ec30(0x200380, candidate_pos, distance_a * 2.0f + distance_b,
                     distance_b, distance_a, -1, los_scratch) &&
        collision_features_test_los(los_scratch, candidate_pos, hit_info)) {
      int hit_object = *(int *)(hit_info + 0x20);
      if (hit_object != -1) {
        unsigned char *hit_any =
          (unsigned char *)object_get_and_verify_type(hit_object, -1);
        if (((1 << (*(unsigned char *)(hit_any + 0x64) & 0x1f)) & 3) != 0) {
          unsigned char *hit_unit =
            (unsigned char *)object_get_and_verify_type(hit_object, 3);
          if (*(int *)(hit_unit + 0x1c8) != -1) {
            unsigned char *carrier_player = (unsigned char *)datum_get(
              *(void **)0x5aa6d4, *(int *)(hit_unit + 0x1c8));
            *(int *)(carrier_player + 0xc8) =
              *(int *)(carrier_player + 0xc8) + 1;
            *(unsigned char *)(carrier_player + 0xd0) = 1;
          }
        }
      }

      if (*(int *)0x456b64 > 0) {
        *(int *)0x456b64 = *(int *)0x456b64 - 1;
        return;
      }

      *(int *)0x456b64 = 0x78;
      hud_print_message(
        (int)(short)unit_get_local_player_index(*(int *)(player + 0x34)),
        (wchar_t *)0x26c684);
      return;
    }
  }

  if (*(short *)(player + 2) != -1) {
    game_engine_post_event(0x1b);
    if (*(short *)(player + 2) != -1) {
      unsigned char effect_desc[0x64];

      csmemset(effect_desc + 2, 0, 0x36);
      *(short *)(effect_desc + 0x00) = *(short *)0x2efe68;
      *(short *)(effect_desc + 0x02) = 2;
      *(int *)(effect_desc + 0x10) = *(int *)0x2efe80;
      *(short *)(effect_desc + 0x14) = *(short *)0x456b68;
      *(int *)(effect_desc + 0x20) = *(int *)0x2efe6c;
      *(int *)(effect_desc + 0x24) = 0;
      *(int *)(effect_desc + 0x28) = *(int *)0x2efe70;
      *(int *)(effect_desc + 0x2c) = *(int *)0x2efe74;
      *(int *)(effect_desc + 0x30) = *(int *)0x2efe78;
      *(int *)(effect_desc + 0x34) = *(int *)0x2efe7c;

      player_effect_apply(player_handle, effect_desc, 1.0f);
    }
  }

  {
    float angle = (float)atan2(unit_pos[1], unit_pos[0]);
    float adjusted = angle + *(float *)(next_goal_entry + 0x0c) -
                     *(float *)(goal_entry + 0x0c);
    unit_pos[0] = x87_fcos(adjusted);
    unit_pos[1] = x87_fsin(adjusted);
    normalize3d(unit_pos);
  }

  /* object_place_at_position: teleport player to destination flag */
  object_set_position(*(int *)(player + 0x34), (float *)next_goal_entry,
                      unit_pos, 0);

  if (*(short *)(player + 2) != -1) {
    player_control_set_facing((unsigned short)*(short *)(player + 2), unit_pos);
  }

  *(int *)(player + 0x70) =
    find_netgame_flag((float *)(unit + 0x0c), 1.0f, 0.0f, 6, -1);
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
  game_variant_t *src;

  csmemset(&tmp, 0, sizeof(tmp));

  src = NULL;

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

/* game_engine_validate_map_netgame_flags (0xae4d0)
 *
 * Validates required netgame flags/spawns/equipment and emits warnings when
 * map metadata is missing or insufficient for multiplayer modes.
 */
void game_engine_validate_map_netgame_flags(void)
{
  int found_index;
  void (*validate_duplicate_flags)(short, const char *) =
    (void (*)(short, const char *))0xaa010;
  void (*validate_flag_out_of_range)(short, short, const char *) =
    (void (*)(short, short, const char *))0xaa0b0;
  void (*validate_spawn_points)(short, int, short, const char *) =
    (void (*)(short, int, short, const char *))0xae400;
  int (*matches_game_type)(int, int, void *) =
    (int (*)(int, int, void *))0xacb10;
  int game_types[5] = { 1, 2, 3, 4, 5 };
  const char *equipment_msgs[5] = {
    "NETGAME MAP FAILURE: failed to find any equipment for ctf",
    "NETGAME MAP FAILURE: failed to find any equipment for slayer",
    "NETGAME MAP FAILURE: failed to find any equipment for oddball",
    "NETGAME MAP FAILURE: failed to find any equipment for king",
    "NETGAME MAP FAILURE: failed to find any equipment for race",
  };
  int i;

  found_index = -1;
  find_netgame_flags(0, 0.0f, 0.0f, 0, 0, 1, &found_index);
  if (found_index == -1) {
    error(2, "NETGAME MAP FAILURE: missing ctf flag [team %d]", 0);
  }

  found_index = -1;
  find_netgame_flags(0, 0.0f, 0.0f, 0, 1, 1, &found_index);
  if (found_index == -1) {
    error(2, "NETGAME MAP FAILURE: missing ctf flag [team %d]", 1);
  }

  validate_duplicate_flags(0,
                           "NETGAME MAP FAILURE: duplicate ctf flag [team %d]");
  validate_flag_out_of_range(
    0, 1, "NETGAME MAP FAILURE: ctf flag out of range [team %d]");

  found_index = -1;
  find_netgame_flags(0, 0.0f, 0.0f, 8, 0, 1, &found_index);
  if (found_index == -1) {
    error(2, "NETGAME MAP FAILURE: missing hill flag [team %d]", 0);
  }

  found_index = -1;
  find_netgame_flags(0, 0.0f, 0.0f, 8, 1, 1, &found_index);
  if (found_index == -1) {
    error(2, "NETGAME MAP FAILURE: missing hill flag [team %d]", 1);
  }

  found_index = -1;
  find_netgame_flags(0, 0.0f, 0.0f, 2, 0, 1, &found_index);
  if (found_index == -1) {
    error(2, "NETGAME MAP FAILURE: missing oddball flag [team %d]", 0);
  }

  found_index = -1;
  find_netgame_flags(0, 0.0f, 0.0f, 2, 1, 1, &found_index);
  if (found_index == -1) {
    error(2, "NETGAME MAP FAILURE: missing oddball flag [team %d]", 1);
  }

  found_index = -1;
  find_netgame_flags(0, 0.0f, 0.0f, 3, 0, 1, &found_index);
  if (found_index == -1) {
    error(2, "NETGAME MAP FAILURE: missing race flag [team %d]", 0);
  }

  found_index = -1;
  find_netgame_flags(0, 0.0f, 0.0f, 3, 1, 1, &found_index);
  if (found_index == -1) {
    error(2, "NETGAME MAP FAILURE: missing race flag [team %d]", 1);
  }

  validate_duplicate_flags(
    3, "NETGAME MAP FAILURE: duplicate race track flag [team %d]");

  validate_spawn_points(1, 0, 4,
                        "NETGAME MAP FAILURE: failed to find enough spawn "
                        "points for ctf team 0 (%d/%d)");
  validate_spawn_points(1, 0, 4,
                        "NETGAME MAP FAILURE: failed to find enough spawn "
                        "points for ctf team 1 (%d/%d)");
  validate_spawn_points(
    2, 0, 4,
    "NETGAME MAP FAILURE: failed to find enough spawn points for slayer %d/%d");
  validate_spawn_points(3, 0, 4,
                        "NETGAME MAP FAILURE: failed to find enough spawn "
                        "points for oddball %d/%d");
  validate_spawn_points(
    4, 0, 4,
    "NETGAME MAP FAILURE: failed to find enough spawn points for king %d/%d");
  validate_spawn_points(
    5, 0, 4,
    "NETGAME MAP FAILURE: failed to find enough spawn points for race %d/%d");

  for (i = 0; i < 5; i++) {
    int count = 0;
    int game_type = game_types[i];
    int *equipment_block = (int *)((char *)global_scenario_get() + 0x384);
    int j;

    for (j = 0; j < *equipment_block; j++) {
      unsigned char *entry =
        (unsigned char *)tag_block_get_element(equipment_block, j, 0x90);
      if (matches_game_type(game_type, 4, entry + 4)) {
        count++;
      }
    }

    if (count == 0) {
      error(2, equipment_msgs[i]);
    }
  }
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

  {
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
      int v;
      /* No client info: use counter with wrap-around to bit 0 */
      *(uint8_t *)(player_datum + 0x66) = *(uint8_t *)0x5aa724;
      *(int32_t *)(player_datum + 0x20) = (int32_t)(*(uint8_t *)0x5aa724);
      v = *(int32_t *)0x5aa724 + 1;
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
      game_engine_hud_update_player(iter.datum_handle, -1, 0);
    }
  } else {
    game_engine_hud_update_player(player_data_handle, -1, 0);
  }

  /* Dispatch vtable slot +0x14 (player_added callback) */
  {
    void (*cb)(int) = ((void (**)(int))current_game_engine)[0x14 / 4];
    if (cb)
      cb(player_data_handle);
  }
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

    game_engine_player_update_netgame_flag(player_handle);

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

      {
        void *server = network_game_server_get();
        if (server != NULL) {
          network_server_manager_game_over(server);
          return;
        }
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

/* Player datum field offsets for respawn/kill tracking */
#define PLAYER_RESPAWN_TICKS      0x2c  /* active countdown (decremented each tick) */
#define PLAYER_DEATH_PENALTY      0x30  /* accumulated penalty (grows per death) */
#define PLAYER_LAST_DEATH_TIME    0x84
#define PLAYER_KILL_STREAK        0x92
#define PLAYER_MULTI_KILL_COUNT   0x94
#define PLAYER_IS_QUITTING        0xd1

/* Game variant respawn fields (copied from game_variant_t at match start) */
#define VARIANT_PENALTY_INCREMENT (*(int *)0x456b24)
#define VARIANT_BASE_RESPAWN_TIME (*(int *)0x456b28)
#define VARIANT_SUICIDE_BONUS     (*(int *)0x456b2c)

/* Respawn floor: 90 ticks = 3 seconds at 30 ticks/sec */
#define MIN_RESPAWN_TICKS  0x5a
#define MAX_PENALTY_MULTIPLIER  5

/* Kill event types dispatched to game_engine_player_event */
#define KILL_EVENT_ENVIRONMENT   1
#define KILL_EVENT_GUARDIANS     2
#define KILL_EVENT_VEHICLE       3
#define KILL_EVENT_NORMAL        4
#define KILL_EVENT_BETRAYAL      5
#define KILL_EVENT_SUICIDE       6
#define KILL_EVENT_DOUBLE_KILL   7
#define KILL_EVENT_GENERIC       8
#define KILL_EVENT_TRIPLE_KILL   9
#define KILL_EVENT_KILLTACULAR  10
#define KILL_EVENT_KILLING_SPREE 11
#define KILL_EVENT_RUNNING_RIOT  12

/* HUD update event types */
#define HUD_EVENT_QUIT_NOTIFY   0x1c
#define HUD_EVENT_BETRAYAL      0xd

/* game_engine_player_killed (0xaf660)
 *
 * Called when a player dies in multiplayer. Records time of death, notifies
 * the active game engine vtable, computes the respawn countdown with
 * penalty accumulation, then broadcasts the appropriate kill/medal event.
 */
void game_engine_player_killed(int killer_handle, int kill_object_handle,
                               int dead_handle, int betrayal)
{
  char *dead_player;
  char *killer_player;
  void (*vtable_fn)(int, int, int, int);
  char same_player;
  char both_valid;
  char is_pvp;
  int penalty;
  int respawn_ticks;
  int kill_event_type;
  data_iter_t iter;
  void *obj_data;
  short multi_kill;
  short kill_streak;

  dead_player = (char *)datum_get(player_data, dead_handle);

  if (dead_handle == NONE) {
    display_assert("dead_player_index != NONE",
                   "c:\\halo\\SOURCE\\game\\game_engine.c", 0x9b3, 1);
    system_exit(-1);
  }

  if (!current_game_engine)
    return;

  *(int *)(dead_player + PLAYER_LAST_DEATH_TIME) = game_time_get();

  vtable_fn = ((void (**)(int, int, int, int))current_game_engine)[0x60 / 4];
  if (vtable_fn)
    vtable_fn(killer_handle, kill_object_handle, dead_handle, betrayal);

  same_player = (killer_handle == dead_handle);
  if (killer_handle != NONE && dead_handle != NONE)
    both_valid = 1;
  else
    both_valid = 0;
  is_pvp = (!(char)betrayal && both_valid && !same_player);

  /* Base respawn = accumulated penalty + variant base time */
  *(int *)(dead_player + PLAYER_RESPAWN_TICKS) =
    *(int *)(dead_player + PLAYER_DEATH_PENALTY) + VARIANT_BASE_RESPAWN_TIME;

  if (VARIANT_PENALTY_INCREMENT > 0) {
    /* Increase death penalty, capped at 5x the increment */
    penalty = *(int *)(dead_player + PLAYER_DEATH_PENALTY) + VARIANT_PENALTY_INCREMENT;
    *(int *)(dead_player + PLAYER_DEATH_PENALTY) = penalty;
    if (penalty > VARIANT_PENALTY_INCREMENT * MAX_PENALTY_MULTIPLIER)
      penalty = VARIANT_PENALTY_INCREMENT * MAX_PENALTY_MULTIPLIER;
    *(int *)(dead_player + PLAYER_DEATH_PENALTY) = penalty;

    if (is_pvp) {
      /* Reward killer: reduce their death penalty */
      if (killer_handle != NONE) {
        killer_player = (char *)datum_get(player_data, killer_handle);
        penalty = *(int *)(killer_player + PLAYER_DEATH_PENALTY) - VARIANT_PENALTY_INCREMENT;
        *(int *)(killer_player + PLAYER_DEATH_PENALTY) = penalty;
        *(int *)(killer_player + PLAYER_DEATH_PENALTY) = penalty <= 0 ? 0 : penalty;
      }
      goto apply_clamp;
    }
  } else {
    if (is_pvp)
      goto apply_clamp;
  }

  /* Non-PvP / betrayal: add bonus time to respawn */
  *(int *)(dead_player + PLAYER_RESPAWN_TICKS) += VARIANT_SUICIDE_BONUS;

apply_clamp:
  respawn_ticks = *(int *)(dead_player + PLAYER_RESPAWN_TICKS);
  if (respawn_ticks <= MIN_RESPAWN_TICKS)
    respawn_ticks = MIN_RESPAWN_TICKS;
  *(int *)(dead_player + PLAYER_RESPAWN_TICKS) = respawn_ticks;

  dead_player = (char *)datum_get(player_data, dead_handle);

  /* Quitting player: notify all players with quit HUD event */
  if (*(char *)(dead_player + PLAYER_IS_QUITTING) != 0) {
    data_iterator_new(&iter, player_data);
    if (data_iterator_next(&iter) != NULL) {
      do {
        game_engine_hud_update_player(iter.datum_handle, dead_handle,
                                      HUD_EVENT_QUIT_NOTIFY);
      } while (data_iterator_next(&iter) != NULL);
    }
    return;
  }

  /* Determine kill event type */
  if (killer_handle == NONE) {
    if (kill_object_handle == NONE) {
      kill_event_type = KILL_EVENT_ENVIRONMENT;
    } else {
      obj_data = object_get_and_verify_type(kill_object_handle, 0xffffffff);
      object_try_and_get_and_verify_type(kill_object_handle, 3);
      switch (*(short *)((char *)obj_data + 0x64)) {
      case 0:
        kill_event_type = KILL_EVENT_GUARDIANS;
        break;
      case 1:
        kill_event_type = KILL_EVENT_VEHICLE;
        break;
      default:
        kill_event_type = KILL_EVENT_ENVIRONMENT;
        break;
      }
    }
  } else if (killer_handle == dead_handle) {
    kill_event_type = KILL_EVENT_SUICIDE;
  } else {
    kill_event_type = KILL_EVENT_NORMAL + ((char)betrayal != 0);
  }

  game_engine_player_event(dead_handle, kill_event_type, killer_handle);

  /* Betrayal: notify killer or broadcast to all players */
  if (kill_event_type == KILL_EVENT_BETRAYAL) {
    if (killer_handle != NONE) {
      game_engine_hud_update_player(killer_handle, dead_handle,
                                    HUD_EVENT_BETRAYAL);
      return;
    }
    data_iterator_new(&iter, player_data);
    if (data_iterator_next(&iter) != NULL) {
      do {
        game_engine_hud_update_player(iter.datum_handle, dead_handle,
                                      HUD_EVENT_BETRAYAL);
      } while (data_iterator_next(&iter) != NULL);
    }
    return;
  }

  if (kill_event_type != KILL_EVENT_NORMAL)
    return;

  /* Medal cascade for normal PvP kills */
  killer_player = (char *)datum_get(player_data, killer_handle);
  multi_kill = *(short *)(killer_player + PLAYER_MULTI_KILL_COUNT);

  if (multi_kill > 3) {
    game_engine_player_event(killer_handle, KILL_EVENT_KILLTACULAR, dead_handle);
    return;
  }
  if (multi_kill == 3) {
    game_engine_player_event(killer_handle, KILL_EVENT_TRIPLE_KILL, dead_handle);
    return;
  }
  if (multi_kill == 2) {
    game_engine_player_event(killer_handle, KILL_EVENT_DOUBLE_KILL, dead_handle);
    return;
  }
  kill_streak = *(short *)(killer_player + PLAYER_KILL_STREAK);
  if (kill_streak == 5) {
    game_engine_player_event(killer_handle, KILL_EVENT_KILLING_SPREE, dead_handle);
    return;
  }
  game_engine_player_event(killer_handle,
                           (kill_streak % 5 != 0) ? KILL_EVENT_GENERIC
                                                  : KILL_EVENT_RUNNING_RIOT,
                           dead_handle);
}

/* FUN_000af9a0 (0xaf9a0) — game_engine post-rasterize hook
 *
 * Called during post-rasterize. For game types 2 (post-game scoreboard)
 * and 3 (post-game delay), renders post-game UI widgets across all four
 * local player slots and clears rumble for each.
 * Game types 0 and 1 are no-ops; any other value asserts unreachable. */
void FUN_000af9a0(void)
{
  int i;
  int16_t bounds[4];

  i = 0;
  if (*(int *)0x456b60 != 0) {
    switch (*(int *)0x5aa730) {
    case 0:
    case 1:
      break;
    case 2:
    case 3:
      game_engine_post_rasterize_post_game();
      bounds[0] = 0;
      bounds[1] = 0;
      bounds[2] = 0x1e0;
      bounds[3] = 0x280;
      do {
        render_ui_widgets_postgame((int16_t)i, bounds);
        rumble_clear_for_local_player((int16_t)i);
        i = i + 1;
      } while (i < 4);
      return;
    default:
      display_assert("!\"unreachable\"",
                     "c:\\halo\\SOURCE\\game\\game_engine.c", 0xdb5, true);
      system_exit(-1);
    }
  }
}

/* FUN_000b04a0 (0xb04a0) — game_engine_ctf.c:0x3f5
 *
 * Assert that the given weapon index refers to a flag weapon.
 * Source file is game_engine_ctf.c but the function is linked into
 * game_engine.obj.
 */
void FUN_000b04a0(int weapon_index)
{
  if (!weapon_is_flag(weapon_index)) {
    display_assert("weapon_is_flag(weapon_index)",
                   "c:\\halo\\SOURCE\\game\\game_engine_ctf.c", 0x3f5, true);
    system_exit(-1);
  }
}

/* FUN_000b04e0 (0xb04e0) — CTF/game-engine score lookup
 *
 * Returns the player's individual score or team score depending on param_2.
 * If param_2 == 0, returns the int16 score at player+0xc4.
 * Otherwise, returns the team score from the 0x456b84 array indexed by
 * the player's team field at player+0x20. */
int FUN_000b04e0(int player_handle, int param_2)
{
  char *player;

  player = (char *)datum_get(player_data, player_handle);
  if (param_2 == 0) {
    return (int)*(int16_t *)(player + 0xc4);
  }
  return ((int *)0x456b84)[*(int *)(player + 0x20)];
}

/* FUN_000b0520 (0xb0520) — check game state == 0 */
bool FUN_000b0520(int param_1)
{
  bool result;
  result = 0;
  if (param_1 == 0) {
    result = 1;
  }
  return result;
}

/* FUN_000b0530 (0xb0530) — CTF/game-engine score format by player
 *
 * Formats the player's score (int16 at player+0xc4) into a wide string
 * buffer using the format string pointer at 0x26c118. */
wchar_t *FUN_000b0530(int player_handle, wchar_t *dst)
{
  char *player;

  player = (char *)datum_get(player_data, player_handle);
  usprintf(dst, (const wchar_t *)0x26c118, (int)*(int16_t *)(player + 0xc4));
  return dst;
}

/* FUN_000b0570 (0xb0570) — CTF/game-engine score header "Score"
 *
 * Formats the static header string L"Score" into the destination buffer. */
wchar_t *FUN_000b0570(wchar_t *dst)
{
  usprintf(dst, L"Score");
  return dst;
}

/* FUN_000b0590 (0xb0590) — CTF/game-engine team score format
 *
 * Formats a team score from the 0x456b84 array, indexed by param_1,
 * into a wide string buffer using the format string at 0x26c118. */
wchar_t *FUN_000b0590(int param_1, wchar_t *dst)
{
  usprintf(dst, (const wchar_t *)0x26c118, ((int *)0x456b84)[param_1]);
  return dst;
}

/* FUN_000b1a60 (0xb1a60) — game-engine score lookup (time-based variant)
 *
 * Returns the player's individual tick-count score or team score depending
 * on param_2. If param_2 == 0, returns the int16 tick count at player+0xc0.
 * Otherwise, returns the team score from the 0x456ba8 array indexed by the
 * player's team field at player+0x20. */
int FUN_000b1a60(int player_handle, int param_2)
{
  char *player;

  player = (char *)datum_get(player_data, player_handle);
  if (param_2 == 0) {
    return (int)*(int16_t *)(player + 0xc0);
  }
  return ((int *)0x456ba8)[*(int *)(player + 0x20)];
}

/* FUN_000b1de0 (0xb1de0) — CTF/game-engine time-based score format
 *
 * Reads the player's tick count at player+0xc0 and formats it as a
 * unicode time string using ticks_to_unicode_time_string. */
wchar_t *FUN_000b1de0(int player_handle, wchar_t *dst)
{
  char *player;

  player = (char *)datum_get(player_data, player_handle);
  ticks_to_unicode_time_string((int)*(int16_t *)(player + 0xc0), 0x100, dst);
  return dst;
}

/* FUN_000b1e20 (0xb1e20) — game-engine score header "Time"
 *
 * Formats the static header string L"Time" into the destination buffer. */
wchar_t *FUN_000b1e20(wchar_t *dst)
{
  usprintf(dst, L"Time");
  return dst;
}

/* FUN_000b1e40 (0xb1e40) — game-engine team time score format
 *
 * Formats a team's time-based score from the 0x456ba8 array, indexed by
 * team_index, into a wide string buffer using ticks_to_unicode_time_string. */
wchar_t *FUN_000b1e40(int team_index, wchar_t *dst)
{
  ticks_to_unicode_time_string(((int *)0x456ba8)[team_index], 0x100, dst);
  return dst;
}

/* Play the score sound for the given event index. Looks up the sound
 * tag from game_globals multiplayer_information sounds block.
 * event_index passed in ESI (register arg). */
void game_engine_play_score_sound(int event_index /* @<esi> */)
{
  void *globals;
  void *mp_info;
  void *entry;
  int tag_index;

  global_scenario_get();
  globals = game_globals_get();
  mp_info = tag_block_get_element((char *)globals + 0x164, 0, 0xa0);
  if (mp_info != NULL && event_index < *(int *)((char *)mp_info + 0x5c)) {
    entry = tag_block_get_element((char *)mp_info + 0x5c, event_index, 0x10);
    if (entry != NULL) {
      tag_index = *(int *)((char *)entry + 0xc);
      if (tag_index != -1)
        sound_impulse_start(tag_index, 1.0f);
    }
  }
}

/* Advance the score event queue. When the current entry's delay expires,
 * shift the queue and play the next sound. */
void game_engine_score_tick(void)
{
  int count = *(int *)0x456dd8;

  if (count == 0)
    return;

  *(int *)0x456de0 = *(int *)0x456de0 - 1;
  if (*(int *)0x456de0 != 0)
    return;

  if (count > 1) {
    int dwords = ((count - 1) & 0x1fffffff) << 1;
    int *dst = (int *)0x456ddc;
    int *src = (int *)0x456de4;
    int i;
    for (i = 0; i < dwords; i++)
      dst[i] = src[i];
  }

  count--;
  *(int *)0x456dd8 = count;
  if (count != 0)
    game_engine_play_score_sound(*(int *)0x456ddc);
}

/* Return the duration in ticks of the score sound at event_index.
 * event_index passed in ESI (register arg). */
int game_engine_get_score_sound_duration(int event_index /* @<esi> */)
{
  void *globals;
  void *mp_info;
  void *entry;
  int tag_index;
  void *tag_data;

  global_scenario_get();
  globals = game_globals_get();
  mp_info = tag_block_get_element((char *)globals + 0x164, 0, 0xa0);
  if (mp_info != NULL && event_index < *(int *)((char *)mp_info + 0x5c)) {
    entry = tag_block_get_element((char *)mp_info + 0x5c, event_index, 0x10);
    if (entry != NULL) {
      tag_index = *(int *)((char *)entry + 0xc);
      if (tag_index != -1) {
        tag_data = tag_get(0x736e6421, tag_index);
        return (*(int *)((char *)tag_data + 0x84) * 30) / 1000;
      }
    }
  }
  return 0;
}

/* Queue a score event. If the event has a sound, enqueue it with a delay
 * equal to the sound duration + 5 ticks. Plays immediately if it's the
 * first/only entry. Events not in the lookup table play directly. */
void game_engine_post_event(int event_type)
{
  int count;

  if (*(uint8_t *)(event_type + 0x2effb8) == 0) {
    game_engine_play_score_sound(event_type);
    return;
  }

  {
    int duration = game_engine_get_score_sound_duration(event_type);
    count = *(int *)0x456dd8;
    if (count < 5) {
      *(int *)(0x456ddc + count * 8) = event_type;
      *(int *)(0x456de0 + count * 8) = duration + 5;
      count++;
      *(int *)0x456dd8 = count;
    }
    if (count == 1)
      game_engine_play_score_sound(event_type);
  }
}

/* Reset the score event queue. Clears the 5-entry buffer at 0x456ddc,
 * sets count to 1, and initializes the first entry with type -1 and
 * 60-tick delay. */
void game_engine_score_reset(void)
{
  csmemset((void *)0x456ddc, 0, 0x28);
  *(int *)0x456dd8 = 1;
  *(int *)0x456ddc = -1;
  *(int *)0x456de0 = 0x3c;
}

/* Dispatch to current game engine vtable slot 8 (0x20).
 * Appends per-frame statistics for the active game type. */
void game_engine_statistics_append(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[0x20 / 4])
      vtable[0x20 / 4]();
  }
}

/* Dispatch to current game engine vtable slot 9 (0x24).
 * Handles an incoming client network message for the game type. */
void game_engine_handle_client_message(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[0x24 / 4])
      vtable[0x24 / 4]();
  }
}

/* Dispatch to current game engine vtable slot 10 (0x28).
 * Handles an incoming server network message for the game type. */
void game_engine_handle_server_message(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[0x28 / 4])
      vtable[0x28 / 4]();
  }
}

/* Compare two statistic entries for qsort by multiple fields.
 * Compares offsets +8, +0xc descending, then +0x10, +0x14 ascending.
 * Returns -1 if param_1 is "better", 1 if param_2, 0 if equal. */
int FUN_000a8470(int param_1, int param_2)
{
  if (*(int *)(param_2 + 8) < *(int *)(param_1 + 8))
    return -1;
  if (*(int *)(param_2 + 8) > *(int *)(param_1 + 8))
    return 1;
  if (*(int *)(param_2 + 0xc) < *(int *)(param_1 + 0xc))
    return -1;
  if (*(int *)(param_2 + 0xc) > *(int *)(param_1 + 0xc))
    return 1;
  if (*(int *)(param_1 + 0x10) < *(int *)(param_2 + 0x10))
    return -1;
  if (*(int *)(param_1 + 0x10) > *(int *)(param_2 + 0x10))
    return 1;
  if (*(int *)(param_2 + 0x14) < *(int *)(param_1 + 0x14))
    return -1;
  if (*(int *)(param_2 + 0x14) > *(int *)(param_1 + 0x14))
    return 1;
  return 0;
}

/* Return whether the game engine allows friendly fire.
 * Returns bit 0 of game_engine_flags if complexity flag bit 2 is clear,
 * otherwise returns the incoming AL (0). */
char FUN_000a9550(void)
{
  if ((*(uint32_t *)0x5aa720 & 4) == 0)
    return (char)(*(uint32_t *)0x456b18 >> 2) & 1;
  return 0;
}

/* Return whether friendly fire is enabled for a specific player.
 * Returns 0 if no engine, player is NONE, or complexity flag bit 2 set. */
char FUN_000a9570(int param_1)
{
  if (current_game_engine && param_1 != -1 &&
      (*(uint32_t *)0x5aa720 & 4) == 0)
    return (char)(*(uint32_t *)0x456b18 >> 2) & 1;
  return 0;
}

/* Return the game engine variant identifier byte.
 * Returns 0 if no engine is active. */
char FUN_000a95a0(void)
{
  if (current_game_engine)
    return *(char *)0x456b14;
  return 0;
}

/* Return whether the game engine has lives enabled and variant is nonzero. */
char FUN_000a95c0(void)
{
  if (current_game_engine &&
      (*(uint8_t *)0x456b18 & 2) != 0 &&
      *(char *)0x456b14 != 0)
    return 1;
  return 0;
}

/* Return a combined spawn/respawn readiness flag.
 * Checks engine active, server/client, game type 2 + flag, and flags byte. */
char FUN_000a9f90(void)
{
  char result;
  int is_client;

  result = 1;
  if (current_game_engine) {
    is_client = *(int *)0x456b1c == 0;
    if (*(int *)0x456b10 == 2 && *(char *)0x456b46 == 0)
      is_client = 0;
    result = (*(uint8_t *)0x456b18 & 1) | (char)is_client;
  }
  return result;
}

/* Dispatch to vtable slot 31 (0x7c).
 * Checks a game-type-specific condition. */
char FUN_000a9fd0(void)
{
  if (current_game_engine) {
    char (*fn)(void) = ((char (**)(void))current_game_engine)[0x7c / 4];
    if (fn)
      return fn();
  }
  return 0;
}

/* Dispatch to vtable slot 32 (0x80).
 * Checks a game-type-specific condition. */
char FUN_000a9ff0(void)
{
  if (current_game_engine) {
    char (*fn)(void) = ((char (**)(void))current_game_engine)[0x80 / 4];
    if (fn)
      return fn();
  }
  return 0;
}

/* Initialize the default map name and game variant for multiplayer. */
void FUN_000aa120(void)
{
  char *map_name;
  char local_6c[104];

  csstrncpy((char *)0x5aa760, "levels\\test\\carousel\\carousel", 0x3f);
  map_name = main_get_multiplayer_map_name();
  if (map_name != NULL && *map_name != 0) {
    csstrncpy((char *)0x5aa760, map_name, 0x3f);
    *(char *)0x5aa79f = 0;
  }
  if (((char (*)(void *))player_ui_game_variant_specified)(local_6c))
    csmemcpy((void *)0x5aa7a0, local_6c, 0x68);
}

/* Set the map name from an external source string. */
void FUN_000ab040(char *param_1)
{
  if (param_1 != NULL && *param_1 != 0)
    csstrncpy((char *)0x5aa760, param_1, 0x3f);
}

/* Copy game variant data from source to the global variant buffer. */
void FUN_000ab070(int param_1)
{
  if (param_1 != 0)
    csmemcpy((void *)0x5aa7a0, (void *)param_1, 0x68);
}

/* Check if a player's invincibility timer has expired.
 * Returns 0 (not invincible) if the timer float is > 0, else 1. */
int16_t FUN_000ab230(int param_1)
{
  int player;

  if (current_game_engine && param_1 != -1) {
    player = (int)datum_get(player_data, param_1);
    if (*(int16_t *)(player + 2) != -1) {
      if (*(float *)(0x5aa734 + *(int16_t *)(player + 2) * 4) > 0.0f)
        return 0;
    }
  }
  return 1;
}

/* Return whether the player's held weapon is a "power weapon" (bit 13 of
 * weapon flags at tag offset +0x308). */
int FUN_000ab290(int param_1)
{
  int player;
  int biped;
  int weapon_handle;
  int weapon_obj;
  int weapon_tag;

  if (param_1 == -1)
    return 0;
  player = (int)datum_get(player_data, param_1);
  if (*(int *)(player + 0x34) == -1)
    return 0;
  biped = (int)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  if (*(int16_t *)(biped + 0x2a2) == -1)
    return 0;
  weapon_handle = *(int *)(biped + 0x2a8 + *(int16_t *)(biped + 0x2a2) * 4);
  if (weapon_handle == -1)
    return 0;
  weapon_obj = (int)object_get_and_verify_type(weapon_handle, 4);
  if (*(int *)weapon_obj == -1)
    return 0;
  weapon_tag = (int)tag_get(0x77656170, *(int *)weapon_obj);
  return (*(uint32_t *)(weapon_tag + 0x308) >> 13) & 1;
}

/* Return 1 if the game engine is inactive (no engine), or bit 0 of flags. */
char FUN_000ab9a0(void)
{
  if (current_game_engine)
    return *(uint8_t *)0x456b18 & 1;
  return 1;
}

/* Return 1 if no engine, or inverted bit 0 of the complexity word. */
char FUN_000ab9c0(void)
{
  if (current_game_engine)
    return ~(*(uint8_t *)0x5aa720) & 1;
  return 1;
}

/* Return 1 if no engine, or inverted bit 1 of the complexity word. */
char FUN_000ab9e0(void)
{
  if (current_game_engine)
    return ~(*(uint8_t *)0x5aa720 >> 1) & 1;
  return 1;
}

/* Initialize a CTF game variant with default settings. */
void FUN_000aa220(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(int *)(buf + 0x14) = 2;
  *(int *)(buf + 0x48) = 2;
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffe3) | 0x23;
  *(char *)(buf + 0x4c) = 1;
  *(char *)(buf + 0x4d) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x20) = 0;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x30) = 0;
  *(int *)(buf + 0x2c) = 0;
  *(int *)(buf + 0x40) = 0x19;
  *(int *)(buf + 0x34) = 0x1c2;
  *(char *)(buf + 0x18) = 0;
  *(int *)(buf + 0x44) = 0;
  *(char *)(buf + 0x4e) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize a Slayer game variant with default settings. */
void FUN_000aa340(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffd2) | 0x12;
  *(int *)(buf + 0x20) = 1;
  *(char *)(buf + 0x4c) = 1;
  *(char *)(buf + 0x4d) = 1;
  *(char *)(buf + 0x4e) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x14) = 2;
  *(int *)(buf + 0x48) = 2;
  *(int *)(buf + 0x2c) = 0x96;
  *(int *)(buf + 0x30) = 0x96;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x40) = 10;
  *(char *)(buf + 0x18) = 0;
  *(int *)(buf + 0x44) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize a King of the Hill game variant. */
void FUN_000aa3d0(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffc3) | 3;
  *(int *)(buf + 0x2c) = 300;
  *(int *)(buf + 0x30) = 300;
  *(int *)(buf + 0x14) = 2;
  *(int *)(buf + 0x48) = 2;
  *(char *)(buf + 0x28) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x20) = 0;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 5;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x40) = 10;
  *(char *)(buf + 0x18) = 0;
  *(int *)(buf + 0x44) = 0;
  *(char *)(buf + 0x4c) = 0;
  *(char *)(buf + 0x4d) = 0;
  *(char *)(buf + 0x4e) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize an Oddball game variant. */
void FUN_000aa460(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffe2) | 0x22;
  *(int *)(buf + 0x20) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x14) = 2;
  *(int *)(buf + 0x48) = 2;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x2c) = 0;
  *(int *)(buf + 0x40) = 0x19;
  *(int *)(buf + 0x30) = 300;
  *(char *)(buf + 0x18) = 0;
  *(int *)(buf + 0x44) = 6;
  *(char *)(buf + 0x4c) = 0;
  *(char *)(buf + 0x4d) = 0;
  *(char *)(buf + 0x4e) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize an Oddball variant (alternate). */
void FUN_000aa4f0(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffe2) | 0x22;
  *(int *)(buf + 0x20) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x14) = 2;
  *(int *)(buf + 0x48) = 2;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x2c) = 0x96;
  *(int *)(buf + 0x40) = 0xf;
  *(int *)(buf + 0x30) = 300;
  *(char *)(buf + 0x18) = 0;
  *(int *)(buf + 0x44) = 4;
  *(char *)(buf + 0x4c) = 0;
  *(char *)(buf + 0x4d) = 0;
  *(char *)(buf + 0x4e) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize a Race game variant. */
void FUN_000aa730(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffc2) | 2;
  *(int *)(buf + 0x20) = 1;
  *(int *)(buf + 0x48) = 1;
  *(int *)(buf + 0x5c) = 1;
  *(char *)(buf + 0x4d) = 1;
  *(int *)(buf + 0x58) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x2c) = 0x96;
  *(int *)(buf + 0x30) = 0x96;
  *(int *)(buf + 0x14) = 3;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x40) = 2;
  *(char *)(buf + 0x18) = 0;
  *(int *)(buf + 0x44) = 0;
  *(char *)(buf + 0x4c) = 0;
  *(int *)(buf + 0x50) = 0;
  *(int *)(buf + 0x54) = 0;
  *(int *)(buf + 0x4f) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize a Race game variant (alternate). */
void FUN_000aa860(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffc3) | 3;
  *(int *)(buf + 0x2c) = 0x96;
  *(int *)(buf + 0x30) = 0x96;
  *(int *)(buf + 0x48) = 2;
  *(int *)(buf + 0x50) = 2;
  *(int *)(buf + 0x58) = 2;
  *(int *)(buf + 0x20) = 1;
  *(int *)(buf + 0x5c) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x14) = 3;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x40) = 10;
  *(char *)(buf + 0x18) = 0;
  *(int *)(buf + 0x44) = 0;
  *(char *)(buf + 0x4d) = 0;
  *(char *)(buf + 0x4c) = 0;
  *(int *)(buf + 0x54) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize a CTF variant (team). */
void FUN_000aaa20(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffe3) | 0x23;
  *(int *)(buf + 0x40) = 2;
  *(int *)(buf + 0x48) = 2;
  *(int *)(buf + 0x20) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x14) = 4;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x2c) = 300;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x30) = 0x1c2;
  *(char *)(buf + 0x18) = 0;
  *(int *)(buf + 0x44) = 0;
  *(char *)(buf + 0x4c) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize a CTF variant (no teams). */
void FUN_000aaab0(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffc3) | 3;
  *(int *)(buf + 0x20) = 1;
  *(char *)(buf + 0x4c) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x40) = 2;
  *(int *)(buf + 0x48) = 2;
  *(int *)(buf + 0x14) = 4;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x2c) = 0;
  *(int *)(buf + 0x30) = 0x96;
  *(char *)(buf + 0x18) = 0;
  *(int *)(buf + 0x44) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize an Assault game variant. */
void FUN_000aac50(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(int *)(buf + 0x14) = 1;
  *(int *)(buf + 0x20) = 1;
  *(char *)(buf + 0x18) = 1;
  *(char *)(buf + 0x4b) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffe3) | 0x23;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x2c) = 300;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x40) = 3;
  *(int *)(buf + 0x30) = 0x1c2;
  *(int *)(buf + 0x44) = 0;
  *(int *)(buf + 0x48) = 2;
  *(char *)(buf + 0x4c) = 0;
  *(char *)(buf + 0x4e) = 0;
  *(char *)(buf + 0x4d) = 0;
  *(int *)(buf + 0x4f) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize an Assault game variant (alternate). */
void FUN_000aace0(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(int *)(buf + 0x14) = 1;
  *(int *)(buf + 0x20) = 1;
  *(char *)(buf + 0x18) = 1;
  *(char *)(buf + 0x4c) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffc3) | 3;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 5;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x2c) = 0;
  *(int *)(buf + 0x40) = 3;
  *(int *)(buf + 0x30) = 0x96;
  *(int *)(buf + 0x44) = 0;
  *(int *)(buf + 0x48) = 2;
  *(char *)(buf + 0x4b) = 0;
  *(char *)(buf + 0x4e) = 0;
  *(char *)(buf + 0x4d) = 0;
  *(int *)(buf + 0x4f) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Initialize a Race game variant (team). */
void FUN_000aafb0(int *param_1)
{
  int i;
  int *src;
  char buf[0x68];

  *(int16_t *)buf = 0;
  csmemset(buf + 2, 0, 0x66);
  *(uint32_t *)(buf + 0x1c) = (*(uint32_t *)(buf + 0x1c) & 0xffffffc3) | 3;
  *(int *)(buf + 0x14) = 5;
  *(int *)(buf + 0x40) = 5;
  *(int *)(buf + 0x20) = 1;
  *(char *)(buf + 0x18) = 1;
  *(int16_t *)(buf + 0x64) = 1;
  *(int *)(buf + 0x48) = 2;
  *(int *)(buf + 0x4c) = 2;
  *(int *)(buf + 0x3c) = 0x3f800000;
  *(int *)(buf + 0x38) = 0;
  *(char *)(buf + 0x28) = 0;
  *(int *)(buf + 0x34) = 0;
  *(int *)(buf + 0x2c) = 0;
  *(int *)(buf + 0x30) = 300;
  *(int *)(buf + 0x44) = 0;
  *(int *)(buf + 0x50) = 0;
  src = (int *)buf;
  for (i = 0x1a; i != 0; i--) {
    *param_1 = *src;
    src++;
    param_1++;
  }
}

/* Set the weapon spawn configuration for a player. */
void FUN_000ab510(int param_1, int param_2)
{
  int weapon;
  int tag_handle;

  if (param_1 != -1) {
    weapon = (int)object_get_and_verify_type(param_1, 4);
    object_set_position(param_1, (float *)param_2, *(float **)0x31fc3c, *(float **)0x31fc44);
    object_reset(param_1);
    *(uint32_t *)(weapon + 0x1dc) = *(uint32_t *)(weapon + 0x1dc) & 0xffffffdf;
    tag_handle = game_time_get();
    *(int *)(weapon + 0x1b4) = tag_handle;
    *(int *)(weapon + 0x1b0) = -1;
  }
}

/* Wrapper: dispatch game_engine_player_event with killer=NONE. */
void FUN_000ad140(int param_1, int param_2)
{
  game_engine_player_event(param_1, param_2, -1);
}

/* Dispatch to vtable slot 33 (0x84) or fall back to FUN_000ae250. */
int game_engine_did_player_win(void)
{
  if (current_game_engine == 0)
    return 0;
  if (((int (**)(void))current_game_engine)[0x84 / 4])
    return ((int (**)(void))current_game_engine)[0x84 / 4]();
  FUN_000ae250();
  return 0;
}

/* Initialize the game engine playlist. */
void game_engine_playlist_initialize(void)
{
  FUN_000aa120();
}

/* FUN_000aceb0 has register args (EAX→ESI, ECX→EDI, EBX) — deferred */

/* Validate a player handle (datum_get). */
void FUN_000aff70(int param_1)
{
  datum_get(player_data, param_1);
}

/* Validate a player handle for post-spawn (datum_get). */
void FUN_000b14e0(int param_1)
{
  datum_get(player_data, param_1);
}

/* Validate a player handle for oddball (datum_get). */
void FUN_000b26b0(int param_1)
{
  datum_get(player_data, param_1);
}

/* Return whether the player is NOT on the hill (byte at 0x456c28[index]). */
int FUN_000b1e70(int param_1)
{
  return *(char *)(0x456c28 + (param_1 & 0xffff)) == 0;
}

/* Initialize 4 team color vectors from the global default color pointer. */
void FUN_000b1aa0(void)
{
  int *src;

  src = *(int **)0x2ee708;
  *(int *)0x5aa6e0 = src[0]; *(int *)0x5aa6e4 = src[1]; *(int *)0x5aa6e8 = src[2];
  *(int *)0x5aa6ec = src[0]; *(int *)0x5aa6f0 = src[1]; *(int *)0x5aa6f4 = src[2];
  *(int *)0x5aa6f8 = src[0]; *(int *)0x5aa6fc = src[1]; *(int *)0x5aa700 = src[2];
  *(int *)0x5aa704 = src[0]; *(int *)0x5aa708 = src[1]; *(int *)0x5aa70c = src[2];
  *(int *)0x5aa710 = 0; *(int *)0x5aa714 = 0; *(int *)0x5aa718 = 0; *(int *)0x5aa71c = 0;
}

/* Assert that a weapon is a flag. */
void FUN_000b2b00(int param_1)
{
  if (!weapon_is_flag(param_1)) {
    display_assert("weapon_is_flag(weapon_index)",
                   "c:\\halo\\SOURCE\\game\\game_engine_oddball.c", 0x3b4, 1);
    system_exit(-1);
  }
}

/* Return the score for a player (team or individual mode). */
int FUN_000b2b40(int param_1, int param_2)
{
  int player;

  player = (int)datum_get(player_data, param_1);
  if (param_2 == 1)
    return *(int *)(0x456e0c + *(int *)(player + 0x20) * 4);
  return *(int *)(0x456e4c + (param_1 & 0xffff) * 4);
}

/* Return 1 if the oddball game type is mode 2, else 0. */
char FUN_000b2bc0(void)
{
  int variant;

  variant = (int)game_engine_get_variant();
  if (*(int *)(variant + 0x5c) - 2 == 0)
    return 1;
  return 0;
}

/* Check if a weapon type index is in the oddball equipment list. */
int FUN_000b2890(int param_1)
{
  int variant;
  int i;

  variant = (int)game_engine_get_variant();
  i = 0;
  if (i < *(int *)(variant + 0x60)) {
    while (*(int *)(0x456ecc + i * 4) != param_1) {
      i++;
      if (*(int *)(variant + 0x60) <= i)
        return 0;
    }
    return 1;
  }
  return 0;
}

/* Check if any oddball equipment slot is empty and unassigned. */
int FUN_000b28c0(void)
{
  int variant;
  int i;

  variant = (int)game_engine_get_variant();
  i = 0;
  if (i < *(int *)(variant + 0x60)) {
    while (*(int *)(0x456e8c + i * 4) != 0 || *(int *)(0x456ecc + i * 4) != -1) {
      i++;
      if (*(int *)(variant + 0x60) <= i)
        return 0;
    }
    return 1;
  }
  return 0;
}

/* Check if a weapon at param_2 belongs to the opposing team of param_1. */
int FUN_000b0170(int param_1, int param_2)
{
  int seat_index;
  int player;
  int weapon;

  seat_index = player_index_from_unit_index(param_1);
  if (seat_index == -1 || param_2 == -1)
    return 1;
  player = (int)datum_get(player_data, seat_index);
  weapon = (int)object_try_and_get_and_verify_type(param_2, 4);
  if (weapon != 0) {
    if (weapon_is_flag(param_2) &&
        (*(uint8_t *)(weapon + 0x1dc) & 0x40) == 0 &&
        (int)*(int16_t *)(weapon + 0x68) == *(int *)(player + 0x20))
      return 0;
  }
  return 1;
}

/* Reset a player's race timer field. */
void FUN_000b3900(int param_1)
{
  int player;

  player = (int)datum_get(player_data, param_1);
  *(int *)(player + 0x88) = 0;
}

/* Return the score column header string ("Score" or "Time"). */
wchar_t *FUN_000b2ca0(wchar_t *param_1)
{
  int variant;

  variant = (int)game_engine_get_variant();
  if (*(int *)(variant + 0x5c) == 2) {
    usprintf(param_1, L"Score");
    return param_1;
  }
  usprintf(param_1, L"Time");
  return param_1;
}

/* Format an individual player's score as a number or time string. */
wchar_t *FUN_000b2c50(int param_1, wchar_t *param_2)
{
  int score;
  int variant;

  score = *(int *)(0x456e4c + (param_1 & 0xffff) * 4);
  variant = (int)game_engine_get_variant();
  if (*(int *)(variant + 0x5c) == 2) {
    usprintf(param_2, *(wchar_t **)0x26c118, score);
    return param_2;
  }
  ticks_to_unicode_time_string(score, 0x100, param_2);
  return param_2;
}

/* Format a team's score as a number or time string. */
wchar_t *FUN_000b2ce0(int param_1, wchar_t *param_2)
{
  int score;
  int variant;

  score = *(int *)(0x456e0c + param_1 * 4);
  variant = (int)game_engine_get_variant();
  if (*(int *)(variant + 0x5c) == 2) {
    usprintf(param_2, *(wchar_t **)0x26c118, score);
    return param_2;
  }
  ticks_to_unicode_time_string(score, 0x100, param_2);
  return param_2;
}

/* Check if a weapon type matches the oddball pickup condition. */
char FUN_000b2c00(int param_1, int param_2)
{
  int variant;
  int i;

  if (param_2 == 0)
    return 0;
  variant = (int)game_engine_get_variant();
  i = 0;
  if (i < *(int *)(variant + 0x60)) {
    do {
      if (*(int *)(0x456ecc + i * 4) == param_1) {
        variant = (int)game_engine_get_variant();
        return param_2 == *(int *)(variant + 0x54);
      }
      i++;
    } while (i < *(int *)(variant + 0x60));
  }
  variant = (int)game_engine_get_variant();
  return param_2 == *(int *)(variant + 0x58);
}

/* Look up the weapon tag index for a given vehicle type from game globals. */
int FUN_000b3770(int param_1)
{
  int game_globals;
  int block;
  int elem0;
  int elem1;
  int elem2;
  int variant;

  global_scenario_get();
  game_globals = (int)game_globals_get();
  block = (int)tag_block_get_element((int *)(game_globals + 0x164), 0, 0xa0);
  block = block + 0x20;
  elem0 = (int)tag_block_get_element((int *)block, 0, 0x10);
  elem1 = (int)tag_block_get_element((int *)block, 1, 0x10);
  elem2 = (int)tag_block_get_element((int *)block, 2, 0x10);
  variant = (int)game_engine_get_variant();
  switch (*(int *)(variant + 0x48)) {
  case 0:
    if (param_1 == 0)
      return *(int *)(elem0 + 0xc);
    if (param_1 == 1)
      return *(int *)(elem2 + 0xc);
    if (param_1 < 6)
      return *(int *)(elem1 + 0xc);
    break;
  case 2:
    if (param_1 < 4)
      return *(int *)(elem0 + 0xc);
    break;
  case 3:
    if (param_1 < 8)
      return *(int *)(elem1 + 0xc);
    break;
  case 4:
    if (param_1 < 4)
      return *(int *)(elem2 + 0xc);
    break;
  }
  return -1;
}

/* Collect all objects of type 2 (weapons) into a buffer and delete them. */
void FUN_000b36f0(void)
{
  int count;
  int handles[32];
  char iter[8];
  int handle;
  int i;

  count = 0;
  object_iterator_new(iter, 2, 0);
  handle = (int)object_iterator_next(iter);
  while (handle != 0) {
    if (count < 0x20) {
      handles[count] = *(int *)(iter + 8);
      count++;
    }
    handle = (int)object_iterator_next(iter);
  }
  i = 0;
  if (0 < count) {
    do {
      object_delete(handles[i]);
      i++;
    } while (i < count);
  }
}

/* Fix duplicate netgame flag sequence indices by reassigning conflicts. */
void FUN_000b3860(void)
{
  int *flag_block;
  int16_t i;
  int elem;
  int16_t seq;
  uint32_t used_mask;
  int j;
  int scenario;

  scenario = (int)global_scenario_get();
  flag_block = (int *)(scenario + 0x378);
  used_mask = 0;
  i = 0;
  if (0 < *flag_block) {
    do {
      elem = (int)tag_block_get_element(flag_block, (int)i, 0x94);
      if (*(int16_t *)(elem + 0x10) == 3 &&
          *(int16_t *)(elem + 0x12) >= 0 &&
          *(int16_t *)(elem + 0x12) < 0x20) {
        seq = *(int16_t *)(elem + 0x12);
        if ((used_mask & (1u << ((uint8_t)seq & 0x1f))) == 0) {
          used_mask |= (1u << ((uint8_t)seq & 0x1f));
        } else {
          for (j = 0; j < 0x20; j++) {
            if ((used_mask & (1u << ((uint8_t)j & 0x1f))) == 0) {
              used_mask |= (1u << ((uint8_t)seq & 0x1f));
              *(int16_t *)(elem + 0x12) = (int16_t)j;
              break;
            }
          }
          if (j >= 0x20)
            *(int16_t *)(elem + 0x12) = (int16_t)j;
        }
      }
      i++;
    } while ((int)i < *flag_block);
  }
}

/* Look up a weapon's index in the game engine weapon list. */
int weapon_definition_index_to_list_index(int param_1)
{
  int game_globals;
  int *block;
  int elem;
  int i;

  game_globals = (int)game_globals_get();
  block = (int *)(game_globals + 0x14c);
  if (*block == 0)
    elem = 0;
  else
    elem = (int)tag_block_get_element(block, 0, 0x10);
  i = -1;
  if (0 < *block) {
    int *p = (int *)(elem + 0xc);
    i = 0;
    while (param_1 != *p) {
      i++;
      p += 4;
      if (*block <= i)
        return -1;
    }
  }
  return i;
}

/* Remap a vehicle tag index based on the current game type variant. */
int game_engine_remap_vehicle(int param_1)
{
  int game_globals;
  int block;
  int elem0;
  int elem1;
  int elem2;
  int variant_type;

  if (current_game_engine == 0)
    return param_1;
  game_globals = (int)game_globals_get();
  block = (int)tag_block_get_element((int *)(game_globals + 0x164), 0, 0xa0);
  block = block + 0x20;
  elem0 = (int)tag_block_get_element((int *)block, 0, 0x10);
  elem1 = (int)tag_block_get_element((int *)block, 1, 0x10);
  elem2 = (int)tag_block_get_element((int *)block, 2, 0x10);
  if (param_1 != *(int *)(elem0 + 0xc) &&
      param_1 != *(int *)(elem1 + 0xc) &&
      param_1 != *(int *)(elem2 + 0xc))
    param_1 = -1;
  variant_type = *(int *)0x456b40;
  switch (variant_type) {
  case 1:
    param_1 = -1;
    break;
  case 2:
    elem0 = (int)tag_block_get_element((int *)block, 0, 0x10);
    if (*(int *)(elem0 + 0xc) != param_1)
      param_1 = -1;
    break;
  case 3:
    elem0 = (int)tag_block_get_element((int *)block, 1, 0x10);
    if (*(int *)(elem0 + 0xc) != param_1)
      param_1 = -1;
    break;
  case 4:
    elem0 = (int)tag_block_get_element((int *)block, 2, 0x10);
    if (*(int *)(elem0 + 0xc) != param_1)
      param_1 = -1;
    break;
  }
  return param_1;
}

/* Check if a player is holding a flag weapon. */
int game_engine_player_has_flag(int param_1)
{
  int player;
  int biped;
  int weapon_handle;
  int i;

  if (param_1 != -1) {
    player = (int)datum_get(player_data, param_1);
    if (*(int *)(player + 0x34) != -1) {
      biped = (int)object_get_and_verify_type(*(int *)(player + 0x34), 3);
      i = 0;
      do {
        weapon_handle = *(int *)(biped + 0x2a8 + i * 4);
        if (weapon_handle != -1) {
          if (weapon_is_flag(weapon_handle))
            return 1;
        }
        i++;
      } while (i < 4);
    }
  }
  return 0;
}

/* Format ticks as "MM:SS" time string into a wide-char buffer. */
void ticks_to_unicode_time_string(int param_1, int param_2, wchar_t *param_3)
{
  int minutes;
  int seconds;
  wchar_t min_buf[64];
  wchar_t sec_buf[64];

  minutes = (param_1 / 30) / 60;
  seconds = (param_1 / 30) % 60;
  if (minutes == 0)
    unicode_sprintf(min_buf, 0x40, *(wchar_t **)0x26c120);
  else
    unicode_sprintf(min_buf, 0x40, *(wchar_t **)0x26c118, minutes);
  if (seconds < 10)
    unicode_sprintf(sec_buf, 0x40, *(wchar_t **)0x26c110, seconds);
  else
    unicode_sprintf(sec_buf, 0x40, *(wchar_t **)0x26c118, seconds);
  unicode_sprintf(param_3, param_2, L"%s:%s", min_buf, sec_buf);
}

/* Update player invisibility based on game engine flags. */
void game_engine_update_player_always_invis(int param_1)
{
  int player;

  if (current_game_engine &&
      ((*(uint8_t *)0x456b18 & 0x10) != 0 ||
       (((char (**)(int, int))current_game_engine)[0x80 / 4] != NULL &&
        ((char (**)(int, int))current_game_engine)[0x80 / 4](param_1, 1)))) {
    player = (int)datum_get(player_data, param_1);
    if (*(int *)(player + 0x34) != -1)
      player_set_respawn_timer(param_1, 0, 0xf);
  }
}

/* game_show_score_team (0xacf90) — deferred, calls game_engine_hud_update_player with register args */

/* game_engine_remap_weapon (0xa9770) — remap weapon tag index based on game type. */
int game_engine_remap_weapon(int param_1)
{
  int index;
  int result;

  index = weapon_definition_index_to_list_index(param_1);
  if (index == 10 || index == 11 || index == -1)
    return param_1;
  if (index == 1)
    index = 7;
  switch (*(int *)0x456b3c) {
  case 1:
    switch (index) {
    case 3: case 4: case 6: case 7:
      return list_index_to_weapon_definition_index(5);
    default:
      return list_index_to_weapon_definition_index(4);
    }
  case 2:
    switch (index) {
    case 3: case 4: case 6: case 7:
      return list_index_to_weapon_definition_index(6);
    }
    break;
  case 3:
    if (index > 2 && index < 6)
      return list_index_to_weapon_definition_index(5);
    return list_index_to_weapon_definition_index(6);
  case 4:
    if (index != 4 && index != 9)
      return list_index_to_weapon_definition_index(9);
    break;
  case 5:
    if (index == 4)
      break;
    if (index == 9)
      return list_index_to_weapon_definition_index(8);
    break;
  case 6:
    return list_index_to_weapon_definition_index(7);
  case 7:
    return list_index_to_weapon_definition_index(8);
  case 8:
    switch (index) {
    case 0: case 3: case 4: case 9:
      return list_index_to_weapon_definition_index(8);
    }
    break;
  case 9:
    if (index == 3)
      break;
    if (index == 5)
      return list_index_to_weapon_definition_index(4);
    if (index != 6)
      break;
    break;
  default:
    break;
  }
  result = list_index_to_weapon_definition_index(index);
  return result;
}

/* game_show_score_you_ally_enemy (0xacff0) — deferred, calls game_engine_hud_update_player with register args */

/* Find a player whose biped is carrying a flag. */
int FUN_000b0100(void)
{
  data_iter_t iter;
  int player;

  data_iterator_new(&iter, player_data);
  player = (int)data_iterator_next(&iter);
  while (1) {
    if (player == 0)
      return -1;
    if (*(int *)(player + 0x34) != -1 &&
        ((char (*)(int))FUN_001ac3b0)(*(int *)(player + 0x34)))
      break;
    player = (int)data_iterator_next(&iter);
  }
  return iter.datum_handle;
}

/* Adjust player movement speed based on race position. */
void FUN_000b3cf0(void)
{
  int max_laps;
  int player;
  int diff;
  int variant;
  char iter[16];

  max_laps = 0;
  data_iterator_new((data_iter_t *)iter, player_data);
  player = (int)data_iterator_next((data_iter_t *)iter);
  while (player != 0) {
    if (max_laps <= *(int16_t *)(player + 0xc2))
      max_laps = (int)*(int16_t *)(player + 0xc2);
    player = (int)data_iterator_next((data_iter_t *)iter);
  }
  data_iterator_new((data_iter_t *)iter, player_data);
  player = (int)data_iterator_next((data_iter_t *)iter);
  while (player != 0) {
    *(int *)(player + 0x6c) = 0x3f800000;
    diff = max_laps - *(int16_t *)(player + 0xc2);
    variant = (int)game_engine_get_variant();
    if (*(int *)(variant + 0x4c) == 2)
      diff = diff / 3;
    if (diff >= 2)
      *(int *)(player + 0x6c) = *(int *)0x253f48;
    else if (diff > 0)
      *(int *)(player + 0x6c) = *(int *)0x253f38;
    player = (int)data_iterator_next((data_iter_t *)iter);
  }
}
