void players_initialize(void)
{
  player_data = game_state_data_new("players", 16, sizeof(player_data_t));
  team_data = game_state_data_new("teams", 16, sizeof(team_data_t));
  players_globals = (players_globals_t *)game_state_malloc(
    "players globals", NULL, sizeof(players_globals_t));
  csmemset(&players_globals->unk_0[4], -1, 0x10u);
  *(_DWORD *)players_globals->unk_0 = -1;
  *(_WORD *)&players_globals->unk_0[36] = 0;
  player_control_globals = (player_control_globals_t *)game_state_malloc(
    "player control globals", 0, sizeof(player_control_globals_t));
}

void players_initialize_for_new_map(void)
{
  player_control_initialize_for_new_map();
  csmemset(players_globals, 0, sizeof(players_globals_t));
  csmemset(&players_globals->unk_0[4], 0xFF, 0x10);
  csmemset(&players_globals->unk_0[0x14], 0xFF, 0x10);
  *(_DWORD *)players_globals->unk_0 = -1;
  players_globals->unk_0[0x29] = 0;
  *(_WORD *)&players_globals->unk_0[0x26] = 0;
  players_globals->unk_0[0x28] = 0;
  *(_WORD *)&players_globals->unk_0[0x2A] = 0xFFFF;
  *(_WORD *)&players_globals->unk_0[0x2C] = 0;
  data_delete_all(player_data);
  data_delete_all(team_data);
  csmemset(&local_player_network_indices, 0xFF, 0x40);
}

void players_dispose_from_old_map(void)
{
  data_make_invalid(player_data);
  data_make_invalid(team_data);
}

void players_dispose(void)
{
  if (player_data)
    player_data = 0;
  if (team_data)
    team_data = 0;
  if (players_globals)
    players_globals = 0;
}

void *machine_get_player_list(int16_t machine_index)
{
  return (char *)&local_player_network_indices +
         (unsigned short)machine_index * 0x10;
}

/* Register a player handle into the machine-local slot table
 * (local_player_network_indices) for a given local_player_index.
 *
 * local_player_index is passed in EAX (register argument).
 * Scans the 4 int-sized slots at
 *   local_player_network_indices[local_player_index & 0xffff][0..3]
 * and writes player_handle into the first slot that is -1 (unused).
 * Asserts if no free slot is found ("failed to create a player"). */
void player_register_machine(unsigned __int16 local_player_index,
                             int player_handle)
{
  int i;
  int *slots;

  slots = (int *)((char *)&local_player_network_indices +
                  (unsigned int)local_player_index * 0x10);
  for (i = 0; i < 4; i++) {
    if (slots[i] == -1) {
      slots[i] = player_handle;
      return;
    }
  }
  display_assert("failed to create a player",
                 "c:\\halo\\SOURCE\\game\\players.c", 0xef, 1);
  system_exit(-1);
}

bool local_player_exists(int16_t local_player_index)
{
  data_iter_t iter;
  char *player;

  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    if (*(int16_t *)(player + 2) == local_player_index)
      return true;
  }
  return false;
}

/* Find the first unused local player index (0..3).
 *
 * First pass: prefer a slot whose gamepad is plugged in (input_has_gamepad)
 * AND which has no existing player (local_player_exists returns false).
 * Second pass (fallback): just find any slot with no existing player.
 * Returns NONE (-1) if all 4 slots are occupied. */
int find_unused_local_player_index(void)
{
  int result;
  int i;

  result = -1;
  for (i = 0; i < 4; i++) {
    if (!input_has_gamepad(i) || local_player_exists(i))
      continue;
    result = i;
    if (i != -1)
      return i;
  }
  /* fallback: any slot without a player */
  for (i = 0; i < 4; i++) {
    if (!local_player_exists(i))
      return i;
  }
  return result;
}

void player_delete(int player_index)
{
  datum_delete(player_data, player_index);
}

int16_t players_get_respawn_failure(void)
{
  return *(int16_t *)((char *)players_globals + 0x2c);
}

int local_player_get_player_index(int16_t local_player_index)
{
  assert_halt(local_player_index >= NONE &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  if (local_player_index == NONE)
    return NONE;
  return *(int *)&players_globals->unk_0[4 + local_player_index * 4];
}

int local_player_set_player_index(unsigned __int16 local_player_index,
                                  int player_index)
{
  int old_player;
  char *player;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  old_player = *(int *)&players_globals->unk_0[4 + local_player_index * 4];
  if (old_player != -1) {
    player = (char *)datum_get(player_data, old_player);
    *(int16_t *)(player + 2) = -1;
  }
  *(int *)&players_globals->unk_0[4 + local_player_index * 4] = player_index;
  if (player_index != -1) {
    player = (char *)datum_get(player_data, player_index);
    *(int16_t *)(player + 2) = local_player_index;
  }
  return old_player;
}

__int16 local_player_count(void)
{
  return *(__int16 *)&players_globals->unk_0[0x24];
}

__int16 local_player_get_next(__int16 local_player_index)
{
  __int16 result;
  __int16 i;

  result = -1;
  for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++) {
    if (*(int *)&players_globals->unk_0[4 + i * 4] != -1 &&
        local_player_index < i) {
      if (i < result || result == -1)
        result = i;
    }
  }
  return result;
}

int player_index_from_unit_index(int unit_index)
{
  data_iter_t iter;
  char *player;
  int result;

  result = NONE;
  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    if (*(int *)(player + 0x34) == unit_index)
      result = iter.datum_handle;
  }
  return result;
}

void player_died(int player_handle)
{
  char *player;
  data_iter_t iter;

  player = (char *)datum_get(player_data, player_handle);
  *(int *)(player + 0x38) = *(int *)(player + 0x34);
  *(int *)(player + 0x34) = NONE;
  if (*(int16_t *)(player + 2) != -1)
    player_control_new_unit(*(int16_t *)(player + 2), NONE);

  *((char *)players_globals + 0x28) = 1;
  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    if (*(int *)(player + 0x34) != NONE)
      *((char *)players_globals + 0x28) = 0;
  }
}

bool players_are_all_dead(void)
{
  return *((char *)players_globals + 0x28);
}

void *players_get_combined_pvs_local(void)
{
  return (char *)players_globals + 0x70;
}

void *players_get_combined_pvs(void)
{
  return (char *)players_globals + 0x30;
}

void player_input_enable(bool enable)
{
  *((char *)players_globals + 0x29) = !enable;
}

bool player_input_enabled(void)
{
  return *((char *)players_globals + 0x29) == 0;
}

/* Check whether any active player's unit is currently airborne.
 *
 * Iterates every player datum. For each player with a valid unit handle:
 *   1. If the root object has flag 0x200000 set (+0x4), return true
 * immediately.
 *   2. If the unit is NOT in a vehicle (unit+0xCC == NONE):
 *      - If unit+0x64 (animation state) == 0: call the biped airborne check
 *        (0x1a0db0); return true if it reports airborne.
 *      - If unit+0x64 == 1: fall through to the altitude check.
 *   3. If the unit IS in a vehicle (unit+0xCC != NONE):
 *      - Look up the vehicle object via object_try_and_get_type (type 2).
 *      - Look up the vehicle tag ('vehi') and check if bit 0x40 is set at
 *        tag+0x17C. If so, fall through to the altitude check.
 *   4. Altitude check: if byte at object+0x428 > 2, return true.
 *
 * Returns false if no player meets any airborne criterion. */
bool any_player_is_in_the_air(void)
{
  data_iter_t iter;
  char *player;
  char *unit_obj;
  int unit_handle;
  char *root_obj;
  int root_handle;
  char *vehicle_obj;
  char *vehi_tag;

  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    unit_handle = *(int *)(player + 0x34);
    if (unit_handle == NONE)
      continue;

    unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
    root_handle = ((int (*)(int))0x13d7f0)(unit_handle);
    root_obj = (char *)object_get_and_verify_type(root_handle, NONE);

    if ((*(unsigned int *)(root_obj + 0x4) & 0x200000) != 0)
      return true;

    if (*(int *)(unit_obj + 0xCC) != NONE) {
      /* Unit is in a vehicle -- ESI becomes the vehicle object */
      vehicle_obj = (char *)object_try_and_get_and_verify_type(
        *(int *)(unit_obj + 0xCC), 2);
      if (vehicle_obj == NULL)
        continue;
      vehi_tag = (char *)tag_get(0x76656869, *(int *)vehicle_obj);
      if ((*(unsigned char *)(vehi_tag + 0x17C) & 0x40) == 0)
        continue;
      /* altitude check uses vehicle object (ESI was reassigned) */
      if (*(unsigned char *)(vehicle_obj + 0x428) > 2)
        return true;
    } else {
      /* Unit is on foot */
      if (*(short *)(unit_obj + 0x64) == 0) {
        if (((bool (*)(int))0x1a0db0)(unit_handle))
          return true;
        continue;
      } else if (*(short *)(unit_obj + 0x64) != 1) {
        continue;
      }
      /* animation state 1: altitude check uses unit object */
      if (*(unsigned char *)(unit_obj + 0x428) > 2)
        return true;
    }
  }
  return false;
}

bool any_player_is_dead(void)
{
  data_iter_t iter;
  char *player;

  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    if (*(int *)(player + 0x34) == -1)
      return true;
  }
  return false;
}

/* Look up an 8-byte record by `index` in the scenario tag_block at offset 0x39C
 * and test it against `object_handle` via FUN_0018ef00.
 *
 * index (AX)     -- element index into the tag_block (8-byte records); the
 *                   sentinel -1 short-circuits to 0 (false).
 * object_handle  -- forwarded unchanged as FUN_0018ef00's second argument.
 *
 * The record's first 16-bit field (record[0], zero-extended) is passed as
 * FUN_0018ef00's first argument. Returns a normalized bool: 1 when index is
 * valid and FUN_0018ef00 returns nonzero, otherwise 0. */
char FUN_000ba850(int16_t index /* @<ax> */, int object_handle)
{
  void *scenario;
  unsigned short *element;

  if (index != -1) {
    scenario = global_scenario_get();
    element = (unsigned short *)tag_block_get_element((char *)scenario + 0x39c,
                                                      (int)index, 8);
    if (FUN_0018ef00((int)*element, object_handle) != 0)
      return 1;
  }
  return 0;
}

/*
 * Tears down a player's currently-controlled unit: records the unit handle in
 * the per-slot globals array (base +0x14, stride 4), marks the player dead,
 * then deactivates and garbage-collects the unit and its held weapon.
 * arg1 (player_index) is passed in EAX; param_2 is a cdecl stack arg whose
 * meaning is uncertain (stored to player+0x38 when != NONE).
 */
void FUN_000ba890(int player_index, int param_2)
{
  char *player;
  int slot;
  int object_handle;
  char *object; /* first object_get_and_verify_type result */
  char *object2; /* second (identical) fetch, used for +0x2a2 read */
  int weapon_handle;

  player = (char *)datum_get(player_data, player_index);
  if (*(int *)(player + 0x34) != NONE) {
    if (game_engine_can_score())
      FUN_000b56f0(*(int *)(player + 0x34), -1, -1, -1);

    slot = *(int16_t *)(player + 2);
    *(int *)((char *)players_globals + slot * 4 + 0x14) =
      *(int *)(player + 0x34);
    player_died(player_index);
    object_handle = *(int *)((char *)players_globals + slot * 4 + 0x14);
    object = (char *)object_get_and_verify_type(object_handle, 3);
    object2 = (char *)object_get_and_verify_type(object_handle, 3);
    weapon_handle =
      unit_get_weapon(object_handle, *(int16_t *)(object2 + 0x2a2));
    *(int *)(object + 0x1c8) = NONE;
    object_deactivate(object_handle);
    object_set_garbage(object_handle, 0);
    if (weapon_handle != NONE)
      object_set_garbage(weapon_handle, 0);
    if (param_2 != NONE)
      *(int *)(player + 0x38) = param_2;
    *((char *)players_globals + 0x28) = 0;
  }
}

/* Spawn an object from a small placement record and attach it to a parent.
 *
 * record         (EDI) -- pointer to a record whose tag_index lives at +0xC.
 *                         Two 16-bit values at +0x10 and +0x12 are copied into
 *                         the freshly created object (see below).
 * parent_handle        -- object handle passed through to
 *                         object_placement_data_new as the placement parent.
 *
 * If record->tag_index (+0xC) is NONE (-1), returns NONE without spawning.
 * Otherwise builds an object_placement (0x88 bytes) for that tag, creates the
 * object, and -- when creation succeeds -- verifies it against type_mask 4 and
 * copies record+0x12 -> object+0x25E and record+0x10 -> object+0x260 (note the
 * crossed source offsets; matches the original store order). Returns the new
 * object handle, or NONE on early-out / failed creation. Structurally faithful
 * lift of FUN_000bac10; EAX return is materialized as -1 at entry. */
int FUN_000bac10(void *record, int parent_handle)
{
  int object_index;
  void *object;
  char placement[0x88];

  object_index = -1;
  if (*(int *)((char *)record + 0xc) != -1) {
    object_placement_data_new(placement, *(int *)((char *)record + 0xc),
                              parent_handle);
    object_index = object_new(placement);
    if (object_index != -1) {
      object = object_get_and_verify_type(object_index, 4);
      *(uint16_t *)((char *)object + 0x25e) =
        *(uint16_t *)((char *)record + 0x12);
      *(uint16_t *)((char *)object + 0x260) =
        *(uint16_t *)((char *)record + 0x10);
    }
  }
  return object_index;
}

/* Update a combined PVS (potentially-visible-set) bit vector from the current
 * player set (or, in editor mode, from the debug observer camera).
 *
 * combined_pvs       (EDI) -- 0x40-byte bit vector buffer, one bit per cluster
 *                             in the current structure_bsp. Zeroed at entry
 *                             then OR-combined with each contributor's PVS.
 * local_player_only        -- if true, only players with a valid
 *                             local_player_index (player+0x2 != -1) contribute.
 *
 * Caller passes combined_pvs in EDI; see callers at 0xbbacc (player_teleport)
 * and 0xbd753/0xbd763 (players_update_before_game) which take addresses inside
 * players_globals (offsets 0x30 and 0x70 -- combined_pvs and
 * combined_pvs_local respectively).
 *
 * Editor branch (game_in_editor() true):
 *   - Look up the leaf index under the debug camera via the bsp3d, mask off
 *     the sign bit, fetch the leaf record from scenario+0xE0 (size 0x10),
 *     read its cluster index at +0x8, and OR that single cluster's
 *     visibility row into combined_pvs.
 *
 * Game branch:
 *   - For each player datum:
 *       - if local_player_only and player has no local_player_index, skip
 *       - if player has a unit, walk to root object and copy its
 *         object.cluster_index (offset 0x4C) into player+0x3C
 *       - if player+0x3C is valid, OR that cluster's visibility row into
 *         combined_pvs.
 *   - Then OR in the cluster returned by 0x13DCC0 (the "currently focused
 *     parent object" cluster -- see objects.c FUN_0013dcc0) when valid. */
void players_update_pvs(void *combined_pvs /* @<edi> */, bool local_player_only)
{
  void *structure_bsp;
  data_iter_t iter;
  char *player;
  int16_t saved_cluster;
  int unit_handle;
  int root_handle;
  char *root_object;
  int16_t root_cluster;
  int16_t player_cluster;
  unsigned char *cluster_data;
  unsigned int cluster_count;

  structure_bsp = ((void *(*)(void))0x18e3c0)(); /* scenario_get */
  csmemset(combined_pvs, 0, 0x40);

  if (game_in_editor()) {
    /* Editor: use the leaf under the observer camera. */
    int leaf_handle;
    int leaf_index;
    void *scenario;
    void *block;
    char *leaf;
    int16_t leaf_cluster;

    leaf_handle =
      ((int (*)(void *))0x18e720)(observer_get_camera(0)); /* bsp3d query */
    if (leaf_handle == -1)
      return;

    leaf_index =
      ((int (*)(void *))0x18e720)(observer_get_camera(0)) & 0x7fffffff;
    scenario = ((void *(*)(void))0x18e3c0)();
    block = (char *)scenario + 0xe0;
    leaf = (char *)tag_block_get_element(block, leaf_index, 0x10);
    leaf_cluster = *(int16_t *)(leaf + 8);
    if (leaf_cluster == -1)
      return;

    cluster_data = (unsigned char *)((void *(*)(void *, int16_t))0x193550)(
      structure_bsp, leaf_cluster);
    cluster_count = (unsigned int)*(int *)((char *)structure_bsp + 0x134);
    ((void (*)(int16_t, void *, void *, void *))0x108f00)(
      (int16_t)cluster_count, combined_pvs, cluster_data, combined_pvs);
    return;
  }

  /* Game: combine PVS from each player + the parent-object cluster. */
  saved_cluster =
    (int16_t)((unsigned short (*)(void))0x13dcc0)(); /* parent obj cluster */

  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    if (local_player_only && *(int16_t *)(player + 2) == -1)
      continue;

    unit_handle = *(int *)(player + 0x34);
    if (unit_handle != -1) {
      root_handle = ((int (*)(int))0x13d7f0)(unit_handle); /* object root */
      root_object = (char *)object_get_and_verify_type(root_handle, -1);
      root_cluster = *(int16_t *)(root_object + 0x4c);
      if (root_cluster != -1)
        *(int16_t *)(player + 0x3c) = root_cluster;
    }

    player_cluster = *(int16_t *)(player + 0x3c);
    if (player_cluster != -1) {
      cluster_data = (unsigned char *)((void *(*)(void *, int16_t))0x193550)(
        structure_bsp, player_cluster);
      cluster_count = (unsigned int)*(int *)((char *)structure_bsp + 0x134);
      ((void (*)(int16_t, void *, void *, void *))0x108f00)(
        (int16_t)cluster_count, combined_pvs, cluster_data, combined_pvs);
    }
  }

  if (saved_cluster != -1) {
    cluster_data = (unsigned char *)((void *(*)(void *, int16_t))0x193550)(
      structure_bsp, saved_cluster);
    cluster_count = (unsigned int)*(int *)((char *)structure_bsp + 0x134);
    ((void (*)(int16_t, void *, void *, void *))0x108f00)(
      (int16_t)cluster_count, combined_pvs, cluster_data, combined_pvs);
  }
}

/* Count how many of the 4 local player slots have a valid (non-NONE) player
 * index assigned in players_globals.
 * Reads players_globals+0x4 through +0x10 (4 dwords). */
int players_compute_local_player_count(void)
{
  int count;
  int *slot;
  int i;

  count = 0;
  slot = (int *)((char *)players_globals + 0x4);
  for (i = 4; i != 0; i--) {
    if (*slot != -1)
      count++;
    slot++;
  }
  return count;
}

/* Check whether the player's unit should interact with a nearby unit
 * (e.g. swap weapons on approach).
 *
 * player_unit_handle  -- the player's unit datum handle
 * nearby_unit_handle  -- the unit near the player to examine
 *
 * Returns true if the player should pick up / interact with the nearby unit.
 * The decision involves:
 *   1. Looking up the nearby unit's weapon tag (weap at +0x308 flags)
 *   2. Checking unit weapon counts (0x1aad90, 0x1aae00)
 *   3. Checking game engine running state
 *   4. Checking unit_can_pick_up_weapon (0xaba00) as fallback */
bool player_examine_nearby_unit(int player_unit_handle, int nearby_unit_handle)
{
  int *nearby_obj;
  char *weap_tag;
  int weapon_count;
  bool can_swap;

  nearby_obj = (int *)object_try_and_get_and_verify_type(nearby_unit_handle, 4);
  weap_tag = (char *)tag_get(0x77656170, *nearby_obj);
  weapon_count = unit_count_weapons(player_unit_handle);
  can_swap = unit_weapon_is_new(player_unit_handle, nearby_unit_handle);
  if ((can_swap && (*(unsigned char *)(weap_tag + 0x308) & 0x10) != 0) ||
      weapon_count == 0) {
    return true;
  }
  if (!game_engine_running()) {
    if (unit_weapon_is_new(player_unit_handle, nearby_unit_handle) &&
        weapon_count < 2) {
      return true;
    }
  }
  if (game_engine_can_pick_up_weapon(player_unit_handle, nearby_unit_handle)) {
    return true;
  }
  return false;
}

/* Clear the action-result fields on a player datum.
 *
 * player_handle is passed in EAX (register argument).
 * Writes 0 to player+0x28 (action result type, word) and
 * NONE (-1) to player+0x24 (action result object, dword). */
void player_reset_action_result(int player_handle /* @<eax> */)
{
  char *player;

  player = (char *)datum_get(player_data, player_handle);
  *(unsigned short *)(player + 0x28) = 0;
  *(int *)(player + 0x24) = -1;
}

/* Attempt to enter a vehicle or interact with a seat object based on the
 * player's current action result.
 *
 * player_handle is passed in EAX (register argument).
 *
 * Action result type (player+0x28):
 *   6 = enter vehicle seat: call unit_set_in_vehicle, then unit_enter_seat.
 *       If both succeed, notify the HUD and clear aim assist. Returns true.
 *   7 = interact with seat object: call unit_enter_seat only.
 *       If it succeeds, notify the HUD. Returns false.
 *   other: returns false immediately.
 *
 * The action result object (player+0x24) is the vehicle or seat object
 * the player is interacting with. */
bool player_try_to_enter_vehicle(int player_handle /* @<eax> */)
{
  char *player;
  int *vehicle_obj;

  player = (char *)datum_get(player_data, player_handle);
  object_get_and_verify_type(*(int *)(player + 0x34), 3);

  if (*(short *)(player + 0x28) == 6) {
    /* Enter vehicle seat */
    if (!unit_set_in_vehicle(*(int *)(player + 0x34), 1))
      return true;
    if (unit_enter_seat(*(int *)(player + 0x34), *(int *)(player + 0x24), 1)) {
      vehicle_obj =
        (int *)object_get_and_verify_type(*(int *)(player + 0x24), 4);
      hud_player_set_vehicle(*(unsigned short *)(player + 0x2), *vehicle_obj);
      player_clear_aim_assist(*(int *)(player + 0x34));
    }
    return true;
  } else if (*(short *)(player + 0x28) == 7) {
    /* Interact with seat object */
    if (unit_enter_seat(*(int *)(player + 0x34), *(int *)(player + 0x24), 1)) {
      vehicle_obj =
        (int *)object_get_and_verify_type(*(int *)(player + 0x24), 4);
      hud_player_set_vehicle(*(unsigned short *)(player + 0x2), *vehicle_obj);
    }
  }
  return false;
}

/* Apply the overshield powerup effect to the player.
 * Builds a player-effect descriptor struct with the overshield parameters
 * and submits it via player_effect_apply. ESI = player_handle. */
void player_apply_overshield_effect(int player_handle)
{
  char *player;
  struct {
    int16_t type;
    int16_t unk_02;
    int32_t pad[3];
    float field_10;
    int16_t field_14;
    int16_t pad_16;
    int32_t pad_18[2];
    float field_20;
    int32_t field_24;
    float field_28;
    float field_2c;
    float field_30;
    float field_34;
  } effect;

  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  if (*(int16_t *)(player + 2) == -1)
    return;

  csmemset(&effect, 0, sizeof(effect));
  effect.type = *(int16_t *)0x2f1480;
  effect.unk_02 = 2;
  effect.field_10 = *(float *)0x2f1490;
  effect.field_14 = *(int16_t *)0x46b6ac;
  effect.field_20 = *(float *)0x2f1484;
  effect.field_24 = 0;
  effect.field_28 = *(float *)0x46b6b0;
  effect.field_2c = *(float *)0x2f1488;
  effect.field_30 = *(float *)0x46b6b4;
  effect.field_34 = *(float *)0x2f148c;
  player_effect_apply(player_handle, &effect, 1.0f);
}

/* Notify the game that active camo was activated (triggers a location-based
 * player effect notification). ESI = player_handle. */
void player_apply_camo_notification(int player_handle)
{
  char *player;
  struct {
    int16_t type;
    int16_t unk_02;
    int32_t pad[3];
    float field_10;
    int16_t field_14;
    int16_t pad_16;
    int32_t pad_18[2];
    float field_20;
    int32_t field_24;
    float field_28;
    float field_2c;
    float field_30;
    float field_34;
  } effect;

  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  if (*(int16_t *)(player + 2) == -1)
    return;

  csmemset(&effect, 0, sizeof(effect));
  effect.type = *(int16_t *)0x2f1494;
  effect.unk_02 = 2;
  effect.field_10 = *(float *)0x2f14a4;
  effect.field_14 = *(int16_t *)0x46b6b8;
  effect.field_20 = *(float *)0x2f1498;
  effect.field_24 = 0;
  effect.field_28 = *(float *)0x46b6bc;
  effect.field_2c = *(float *)0x2f149c;
  effect.field_30 = *(float *)0x2f14a0;
  effect.field_34 = *(float *)0x46b6c0;
  player_effect_apply(player_handle, &effect, 1.0f);
}

/* Apply the health powerup effect to the player.
 * Unlike overshield/camo, this uses entirely inline constants
 * rather than loading from global addresses. ESI = player_handle. */
void player_apply_health_effect(int player_handle)
{
  char *player;
  struct {
    int16_t type;
    int16_t unk_02;
    int32_t pad[3];
    float field_10;
    int16_t field_14;
    int16_t pad_16;
    int32_t pad_18[2];
    float field_20;
    int32_t field_24;
    float field_28;
    float field_2c;
    float field_30;
    float field_34;
  } effect;

  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  if (*(int16_t *)(player + 2) == -1)
    return;

  csmemset(&effect, 0, sizeof(effect));
  effect.type = 6;
  effect.unk_02 = 2;
  effect.field_10 = 2.0f;
  effect.field_14 = 1;
  effect.field_20 = 0.5f;
  effect.field_24 = 0;
  effect.field_28 = 1.0f;
  effect.field_2c = 0.917647f;
  effect.field_30 = 0.917647f;
  effect.field_34 = 0.917647f;
  player_effect_apply(player_handle, &effect, 1.0f);
}

/* Mark the player's unit with the camo-active flag.
 *
 * player_handle (@eax) -- player datum handle.
 * powerup_index         -- powerup slot; only index 0 is acted on, mirroring
 *                          the powerup_idx==0 branch of
 * player_set_respawn_timer.
 *
 * Looks up the player datum, fetches its unit object handle (player+0x34) and
 * verifies it is a unit (object type mask 3, biped/vehicle family).  When
 * powerup_index is 0, sets bit 0x10 in the unit flags at +0x1b4 (the
 * camo-active flag, per player_set_respawn_timer) and clears the powerup-type
 * field at unit+0x3d2.  object_get_and_verify_type is called unconditionally,
 * before the branch, matching the original. */
void player_set_unit_camo_flag(int player_handle /* @<eax> */,
                               int16_t powerup_index)
{
  char *player;
  char *unit_obj;

  player = (char *)datum_get(player_data, player_handle);
  unit_obj = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  if (powerup_index == 0) {
    *(unsigned int *)(unit_obj + 0x1b4) |= 0x10;
    *(int16_t *)(unit_obj + 0x3d2) = 0;
  }
}

/* Set a unit object flag bit (0x20) at unit+0x1b4 for a player's unit.
 *
 * Sibling of player_set_unit_camo_flag (0xbb180); another powerup branch of
 * player_set_respawn_timer.  Looks up the player datum, fetches its unit
 * object handle (player+0x34) and verifies it is a unit (object type mask 3,
 * biped/vehicle family).  When param2 is 0, ORs bit 0x20 into the unit flags
 * at +0x1b4.  object_get_and_verify_type is called unconditionally, before the
 * branch, matching the original. */
void FUN_000bb1c0(int player_index /* @<eax> */, int16_t param2)
{
  char *player;
  char *unit_obj;

  player = (char *)datum_get(player_data, player_index);
  unit_obj = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  if (param2 == 0) {
    *(unsigned int *)(unit_obj + 0x1b4) |= 0x20;
  }
}

/* Sibling of FUN_000bb1c0 (0xbb1c0); another powerup branch of
 * player_set_respawn_timer.  Looks up the player datum, fetches its unit
 * object handle (player+0x34) and verifies it is a unit (object type mask 3,
 * biped/vehicle family).  object_get_and_verify_type is called
 * unconditionally, before the branch, matching the original.  When param2 is
 * 0, clears bit 0x10 of the unit flags dword at +0x1b4. */
void FUN_000bb1f0(int player_index /* @<eax> */, int16_t param2)
{
  char *player;
  char *unit_obj;

  player = (char *)datum_get(player_data, player_index);
  unit_obj = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  if (param2 == 0) {
    *(unsigned int *)(unit_obj + 0x1b4) &= 0xffffffef;
  }
}

/* Allocate and initialise a new player datum.
 *
 * local_player_index  (a1) -- which local player slot to assign; NONE (-1) is
 *                             allowed (player is not locally controlled).
 * player_handle_hint  (a2) -- if -1, allocate the next free datum;
 *                             otherwise re-use this specific datum handle.
 * local_player_index2 (a3) -- same value as a1; written into the player
 *                             record at offset +0x2.
 * player_name         (a4) -- pointer to a wide-char name (max 0xb chars),
 *                             or NULL to use the empty default name.
 *
 * Returns the new player datum handle, or -1 on failure. */
int player_new(unsigned __int16 a1, int a2, unsigned __int16 a3, char *a4)
{
  int player_handle;
  char *player;
  char *player2;
  wchar_t *name_src;

  /* Allocate the player datum. */
  if (a2 == -1) {
    player_handle = data_new_at_index(player_data);
  } else {
    player_handle = data_new_datum(player_data, a2);
  }

  /* Validate the local_player_index argument. */
  if (((a3 < 0) || (3 < a3)) && (a3 != (unsigned __int16)-1)) {
    display_assert(
      "((local_player_index>=0) && (local_player_index<MAXIMUM_NUMBER_OF_"
      "LOCAL_PLAYERS)) || (local_player_index==NONE)",
      "c:\\halo\\SOURCE\\game\\players.c", 0x134, 1);
    system_exit(-1);
  }

  if (player_handle != -1) {
    /* Initialise the new player record. */
    player = (char *)datum_get(player_data, player_handle);

    /* Copy player name (up to 0xb wide chars); use empty default if no name
     * supplied. */
    name_src = (a4 != NULL) ? (wchar_t *)a4 : (wchar_t *)0x26cdf0;
    ustrncpy((wchar_t *)(player + 4), name_src, 0xb);

    *(unsigned __int16 *)(player + 0x1a) = 0;
    *(short *)(player + 0x2) = (short)a3;
    *(int *)(player + 0x34) = -1;
    *(int *)(player + 0x38) = -1;
    *(int *)(player + 0x1c) = -1;
    *(unsigned short *)(player + 0x3c) = 0xffff;
    *(int *)(player + 0x40) = -1;
    *(int *)(player + 0x6c) = 0x3f800000; /* 1.0f */
    *(int *)(player + 0x20) = 1;

    /* Second datum_get for the same handle (compiler re-fetched the
     * pointer after the intervening writes). */
    player2 = (char *)datum_get(player_data, player_handle);
    *(unsigned short *)(player2 + 0x28) = 0;
    *(int *)(player2 + 0x24) = -1;

    *(int *)(player + 0xcc) = -1;
    *(char *)(player + 0xd1) = 0;

    /* Copy full player name into the +0x48 slot if a name was given. */
    if (a4 != NULL) {
      csmemcpy(player + 0x48, a4, 0x20);
    }
  }

  /* Register the player handle in the machine-local slot table. */
  player_register_machine(a1, player_handle);
  return player_handle;
}

/* Grant a unit its starting equipment from a scenario starting-equipment
 * definition block (scenario+0x348, element size 0x68).
 *
 * unit_handle      -- datum handle of the unit to equip (verified as a type-3
 *                     unit object; must be alive: unit+0x1c8 != -1).
 * equipment_index  -- index into the scenario starting_equipment tag block.
 * reset_flag       -- when nonzero, first strip the unit's weapons and zero
 *                     the powerup/grenade accumulators before applying, and
 *                     mark the first attached weapon as the initial weapon.
 *
 * Each of the two weapon slots (equip_def+0x34 / +0x48 tag refs) that is set
 * spawns a weapon object via FUN_000bac10 (record ptr in EDI: equip_def+0x28
 * for slot 1, equip_def+0x3c for slot 2) parented to the unit, then attaches
 * it via unit_enter_seat. On attach failure the weapon is deleted and an
 * error is logged. Finally the definition's two float powerups (+0x24 -> unit
 * +0x94, +0x20 -> unit+0x90) and two grenade-type counts (+0x50,+0x51 ->
 * unit+0x2ce,+0x2cf) are accumulated into the unit. */
void player_add_equipment(int unit_handle, int16_t equipment_index,
                          char reset_flag)
{
  char *unit;
  char *equip_def;
  int weapon;
  char *dst;
  char *src;
  int count;

  if ((unit_handle != -1) && (equipment_index != -1) &&
      (unit = (char *)object_try_and_get_and_verify_type(unit_handle, 3),
       *(int *)(unit + 0x1c8) != -1)) {
    equip_def = (char *)tag_block_get_element(
      (char *)global_scenario_get() + 0x348, (int)equipment_index, 0x68);

    if (reset_flag != '\0') {
      unit_clear_weapons(unit_handle);
      *(int *)(unit + 0x94) = 0;
      *(int *)(unit + 0x90) = 0;
      *(int16_t *)(unit + 0x2ce) = 0;
    }

    if ((*(int *)(equip_def + 0x34) != -1) &&
        (weapon = FUN_000bac10(equip_def + 0x28, unit_handle), weapon != -1) &&
        !unit_enter_seat(unit_handle, weapon,
                         (int16_t)(uint16_t)(reset_flag != '\0'))) {
      error(2, "Could not attach starting weapon to player");
      object_delete(weapon);
    }

    if ((*(int *)(equip_def + 0x48) != -1) &&
        (weapon = FUN_000bac10(equip_def + 0x3c, unit_handle), weapon != -1) &&
        !unit_enter_seat(unit_handle, weapon, 0)) {
      error(2, "Could not attach starting weapon to player");
      object_delete(weapon);
    }

    *(float *)(unit + 0x94) =
      *(float *)(equip_def + 0x24) + *(float *)(unit + 0x94);
    *(float *)(unit + 0x90) =
      *(float *)(equip_def + 0x20) + *(float *)(unit + 0x90);

    dst = unit + 0x2ce;
    src = equip_def + 0x50;
    count = 2;
    do {
      *dst = (char)(*dst + *src);
      src++;
      dst++;
    } while (--count != 0);
  }
}

/* Build the aiming/facing update for a player's unit when riding in a
 * vehicle.
 *
 * If the player's unit is seated in a vehicle and the seat does NOT have
 * the 0x10 flag set (steering seat), transform the player's aiming
 * vector from world-space into the vehicle's local coordinate frame.
 *
 * The transformation uses the vehicle's forward vector (object+0x30)
 * to build a rotation matrix, then multiplies aiming_out by that matrix.
 *
 * datum_handle   -- player datum handle
 * aiming_out     -- [in/out] 3-float aiming direction (yaw/pitch converted)
 * desired_facing -- 2-float desired facing angles (yaw, pitch) */
void player_build_action_update(int datum_handle, float *aiming_out,
                                float *desired_facing)
{
  char *player;
  char *unit;
  char *vehicle;
  char *vehi_tag;
  unsigned char *seat_data;
  float forward[3];
  float matrix[13]; /* 3x3 matrix + scale, 52 bytes at [EBP-0x34] */

  player = (char *)datum_get(player_data, datum_handle);
  angles_to_vector(aiming_out, desired_facing);

  if (*(int *)(player + 0x34) == -1)
    return;

  unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  if (*(int *)(unit + 0xCC) == -1)
    return;

  vehicle =
    (char *)object_try_and_get_and_verify_type(*(int *)(unit + 0xCC), 2);
  if (vehicle == NULL)
    return;

  vehi_tag = (char *)tag_get(0x76656869, *(int *)vehicle);
  seat_data = (unsigned char *)tag_block_get_element(
    vehi_tag + 0x2E4, (int)*(short *)(unit + 0x2A0), 0x11C);
  if ((*seat_data & 0x10) != 0)
    return;

  /* Build a rotation matrix from the vehicle's up vector (forward in
   * object space at +0x30). Cross product with global -Y to get the
   * right vector; if degenerate, fall back to -Z. */
  cross_product3d((float *)(vehicle + 0x30), *(float **)0x31fc4c, forward);
  if (normalize3d(forward) == 0.0f) {
    cross_product3d((float *)(vehicle + 0x30), *(float **)0x31fc50, forward);
    normalize3d(forward);
  }
  matrix_from_forward_and_up(matrix, forward, (float *)(vehicle + 0x30));
  matrix_transform_vector(matrix, aiming_out, aiming_out); /* dup-args-ok */
}

/* 0xbbbe0 — Choose the best-scoring starting location for a player.
 *
 * Scores every starting location as pow(random[0,1], 0.5) * rating and
 * returns the index of the highest-scoring one.  The random weighting
 * jitters the pick so respawns are not perfectly deterministic.
 *
 * The location count comes from scenario+0x354, unless the campaign
 * encounter selector (DAT 0x5ac9f4) is active, in which case it is
 * overridden by the selected encounter block element's +0xa4 field
 * (stride 0xb0, block at scenario+0x42c) when that value is positive.
 *
 * Returns the best index (sign-extended 16-bit, MOVSX in the original),
 * or -1 when there are no locations (count < 1) or none scores above 0.
 *
 * Confirmed: cdecl, one stack arg (player_index in EDI at [EBP+8]);
 *   score = pow(random, 0.5) * rating (FLD double[0x25fea8]=0.5, __CIpow);
 *   strict > update on best (FCOM/FNSTSW/TEST AH,0x41/JNZ).
 * Uncertain: param semantics (player/team index fed to the rating fn);
 *   0xbaae0 (player_get_starting_location) returns a starting-location
 *   pointer used here as an opaque handle passed to the rating fn. */
int find_best_starting_location_index(int player_index)
{
  char *scenario;
  char *elem;
  int16_t count;
  int best_index;
  float best_score;
  float rating;
  double score;
  int loc;
  int i;

  scenario = (char *)global_scenario_get();
  count = *(int16_t *)(scenario + 0x354);
  if (*(int *)0x5ac9f4 != NONE) {
    elem = (char *)tag_block_get_element(scenario + 0x42c,
                                         *(int *)0x5ac9f4 & 0xffff, 0xb0);
    if (*(int *)(elem + 0xa4) > 0) {
      count = (int16_t) * (int *)(elem + 0xa4);
    }
  }

  best_index = -1;
  best_score = 0.0f;
  if (count >= 1) {
    i = 0;
    do {
      loc = (int)player_get_starting_location(i);
      rating = game_engine_get_starting_location_rating(player_index, loc);
      score =
        pow(random_real_range(get_global_random_seed_address(), 0.0f, 1.0f),
            *(double *)0x25fea8) *
        rating;
      if (best_score < score) {
        best_score = (float)score;
        best_index = i;
      }
      i++;
    } while (i < count);
  }

  return (int16_t)best_index;
}

/* Spawn (or respawn) a player.
 *
 * Two paths:
 *   A. Campaign/singleplayer path (game engine NOT running): if the player
 *      already has a "saved unit" parked in players_globals+0x14+idx*4, try
 *      to reuse it.  Otherwise fabricate a new unit from the current spawn
 *      point.
 *   B. Multiplayer / game-engine path: always allocate a fresh unit via
 *      object_placement_data_new + object_new_from_placement_data.
 *
 * Structurally faithful lift of the original FUN_bbcb0.  Helper addresses
 * (0xbbbe0, 0xbaae0, 0xbaba0, 0xba5f0, 0x10cc70, 0x13fc20, 0x13fb30,
 * 0x13ffc0, 0x140cc0, 0x143c80, 0x1adeb0, 0x1adf10, 0xbb410, 0xa99a0,
 * 0x8aa30) are not yet in kb.json; invoked by address to keep the lift
 * narrowly scoped.
 *
 * Uncertain: exact semantics of players_globals+0x14 (cached-unit table
 * per local player), scenario+0x348 (starting-equipment count / flags),
 * globals+0x170 tag block (default unit biped tag), globals+0x164
 * (MP-specific unit tag), and DAT_5ac9f4 (campaign encounter selector).
 * Field names for these are deliberately kept as raw offsets. */
void player_spawn(int player_handle)
{
  char *player; /* [EBP-0x4] player datum ptr (EDI)          */
  int saved_unit; /* ESI: handle of a cached unit to reuse   */
  int16_t local_player_index;
  char *unit_data;
  int prev_weapon;
  int16_t spawn_slot;
  char *globals_ptr; /* [EBP-0x8] game_globals_get() result  */
  char *default_unit_block;
  void *position; /* vec3 from FUN_baae0                    */
  int tag_handle; /* biped tag handle fed to placement data */
  char placement[0x88]; /* [EBP-0xa8] object_placement_data  */
  float orient_tmp[3]; /* [EBP-0x20] local_24: out-param for FUN_a99a0 */
  float orient[3]; /* [EBP-0x14] local_18: copied, passed to FUN_baba0 */
  int new_unit;
  char *unit_obj;
  char *player2; /* re-fetched player ptr after object_new  */
  int scen_starting_count;
  void *mp_unit_block;

  player = (char *)datum_get(player_data, player_handle);
  saved_unit = NONE;
  /* Record the original player pointer for the common tail. */

  /* --- Path A/B selector: campaign code first tries to reuse a cached
   *     unit stored at players_globals+0x14+lpi*4. ---- */
  if (!game_engine_running()) {
    local_player_index = *(int16_t *)(player + 2);
    if (local_player_index != NONE) {
      saved_unit =
        *(int *)&players_globals->unk_0[0x14 + local_player_index * 4];
      *(int *)&players_globals->unk_0[0x14 + local_player_index * 4] = NONE;
      if (saved_unit != NONE) {
        unit_data = (char *)object_get_and_verify_type(saved_unit, 3);
        if ((unit_data[0xb6] & 4) != 0) {
          /* Cached unit was deleted/marked-deleted: drop it and fall
           * through to the fresh-spawn path. */
          object_delete(saved_unit);
          saved_unit = NONE;
        }
      }
    }
  }

  if (!game_engine_running() && saved_unit != NONE) {
    /* --- Reuse cached unit path. --- */
    unit_data = (char *)object_get_and_verify_type(saved_unit, 3);
    prev_weapon = unit_get_weapon(saved_unit, *(int16_t *)(unit_data + 0x2a2));
    if (*(int16_t *)(player + 2) == NONE) {
      display_assert("player->local_player_index!=NONE",
                     "c:\\halo\\SOURCE\\game\\players.c", 0x736, 1);
      system_exit(-1);
    }
    ((void (*)(int))0x13fb30)(saved_unit);
    object_set_garbage(saved_unit, 1);
    ((void (*)(uint16_t, int))0xba5f0)((uint16_t) * (int16_t *)(player + 2),
                                       saved_unit);
    if (prev_weapon != NONE) {
      object_set_garbage(prev_weapon, 1);
    }
  } else {
    /* --- Fresh-spawn path. --- */
    globals_ptr = (char *)global_scenario_get();
    if (*(int *)0x5ac9f4 != NONE) {
      /* Touch the campaign-encounter selector entry (side effect unused
       * here; the original preserves the call). */
      tag_block_get_element(globals_ptr + 0x42c, *(int *)0x5ac9f4 & 0xffff,
                            0xb0);
    }
    spawn_slot = (int16_t)((int (*)(int))0xbbbe0)(player_handle);
    if (spawn_slot == NONE) {
      goto common_tail;
    }
    globals_ptr = (char *)game_globals_get();
    default_unit_block = (char *)tag_block_get_element(
      (char *)game_globals_get() + 0x170, 0, 0xf4);
    if (*(int *)(default_unit_block + 0xc) == NONE) {
      goto common_tail;
    }
    position = ((void *(*)(int16_t))0xbaae0)(spawn_slot);
    if (game_engine_running()) {
      mp_unit_block = tag_block_get_element(globals_ptr + 0x164, 0, 0xa0);
      tag_handle = *(int *)((char *)mp_unit_block + 0x1c);
    } else {
      tag_handle = *(int *)(default_unit_block + 0xc);
    }
    ((void (*)(char *, int, int))0x13fc20)(placement, tag_handle, -1);
    /* Copy position vec3 from FUN_baae0 into placement+0x18..+0x20. */
    *(int *)(placement + 0x18) = *(int *)((char *)position + 0x00);
    *(int *)(placement + 0x1c) = *(int *)((char *)position + 0x04);
    *(int *)(placement + 0x20) = *(int *)((char *)position + 0x08);
    /* placement+0x34 = forward vec3 from yaw (position+0xc). */
    ((void (*)(float *, float))0x10cc70)((float *)(placement + 0x34),
                                         *(float *)((char *)position + 0xc));
    /* placement+0x40 = up vec3 copied from global at *(void**)0x31fc44. */
    *(int *)(placement + 0x40) = *(int *)(*(int *)0x31fc44 + 0);
    *(int *)(placement + 0x44) = *(int *)(*(int *)0x31fc44 + 4);
    *(int *)(placement + 0x48) = *(int *)(*(int *)0x31fc44 + 8);
    /* Compute starting team/color vec3.  The original fetches into
     * local_24, then copies the three dwords into local_18 before calling
     * FUN_baba0 — preserve both buffers. */
    {
      float *ret =
        ((float *(*)(float *, int))0xa99a0)(orient_tmp, player_handle);
      orient[0] = ret[0];
      orient[1] = ret[1];
      orient[2] = ret[2];
    }
    ((void (*)(char *, float *))0xbaba0)(placement, orient);
    new_unit = ((int (*)(char *))0x143c80)(placement);
    if (new_unit == NONE) {
      goto common_tail;
    }
    unit_obj = (char *)object_try_and_get_and_verify_type(new_unit, 3);
    if (unit_obj == NULL) {
      goto common_tail;
    }
    player2 = (char *)datum_get(player_data, player_handle);
    *(int *)(unit_obj + 0x70) = player_handle;
    *(int16_t *)(unit_obj + 0x68) = *(int16_t *)(player2 + 0x20);
    *(int *)(unit_obj + 0x1c8) = player_handle;
    *(int *)(player2 + 0x34) = new_unit;
    ((void (*)(int, char))0x1adf10)(new_unit, 1);
    if (*(int16_t *)(player2 + 2) != NONE) {
      player_control_new_unit((uint16_t) * (int16_t *)(player2 + 2), new_unit);
    }
    if (!game_engine_running()) {
      scen_starting_count = *(int *)((char *)global_scenario_get() + 0x348);
      if (scen_starting_count > 1 && *(int16_t *)(player2 + 0xaa) > 0) {
        player_add_equipment(*(int *)(player2 + 0x34), 1, 1);
      } else if (scen_starting_count != 0) {
        player_add_equipment(*(int *)(player2 + 0x34), 0, 1);
      }
    }
    /* Restore EDI (original player ptr) for the common tail. */
  }

common_tail:
  csmemset(player + 0x68, 0, 4);
  player2 = (char *)datum_get(player_data, player_handle);
  *(int16_t *)(player2 + 0x28) = 0;
  *(int *)(player2 + 0x24) = NONE;
  if (*(int16_t *)(player + 2) != NONE) {
    ((void (*)(int16_t))0x8aa30)(*(int16_t *)(player + 2));
  }
}

/* Attempt to spawn the player into a vehicle or interact with a world
 * object, based on the player's action result type (player+0x28).
 *
 * player_handle is passed in EAX (register argument).
 *
 * Action result types handled:
 *   5  = pickup equipment: clear seat equipment, then try unit_pickup_equipment
 *   8,9 = find nearby seat: try unit_find_nearby_seat + unit_board_vehicle
 *   10 = device group interaction: set device group position
 *   11 = vehicle approach: store approach info on unit, compute approach
 *        direction (front/behind/above/below)
 *   6,7 = default: return false
 *
 * Returns true on success, false otherwise. */
bool player_try_to_spawn_in_vehicle(int player_handle /* @<eax> */)
{
  char *player;
  char *unit;
  char *item_obj;
  int *vehicle_obj;
  int nearby_unit;
  char *nearby_unit_data;
  char *world_matrix_a;
  char *world_matrix_b;
  float delta[3];
  float dot;
  char action_type;
  char out_a[52];
  char out_b[52];

  player = (char *)datum_get(player_data, player_handle);
  object_get_and_verify_type(*(int *)(player + 0x34), 3);

  switch (*(short *)(player + 0x28)) {
  case 5:
    /* Equipment pickup */
    unit_clear_seat_equipment(*(int *)(player + 0x34));
    if (unit_pickup_equipment(*(int *)(player + 0x34), *(int *)(player + 0x24),
                              0)) {
      vehicle_obj =
        (int *)object_get_and_verify_type(*(int *)(player + 0x24), 8);
      hud_player_set_vehicle_seat(*(unsigned short *)(player + 0x2),
                                  *vehicle_obj);
      return true;
    }
    break;

  case 8:
  case 9: {
    /* Find nearby seat and board vehicle */
    nearby_unit = -1;
    if (unit_find_nearby_seat(*(int *)(player + 0x34), *(int *)(player + 0x24),
                              *(short *)(player + 0x2a), &nearby_unit)) {
      unit_board_vehicle(*(int *)(player + 0x34), *(int *)(player + 0x24),
                         *(short *)(player + 0x2a));
      return false;
    }
    if (nearby_unit == -1)
      return false;
    nearby_unit_data = (char *)object_get_and_verify_type(nearby_unit, 3);
    if (*(int *)(nearby_unit_data + 0x1a4) == -1)
      return false;
    ai_handle_unit_approach(*(int *)(nearby_unit_data + 0x1a4),
                            *(int *)(player + 0x34), 1);
    return false;
  }

  case 10:
    /* Device group interaction */
    device_group_set_real(*(int *)(player + 0x24), *(int *)(player + 0x34));
    return true;

  case 11: {
    /* Vehicle approach: compute approach direction */
    unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
    item_obj = (char *)object_get_and_verify_type(*(int *)(player + 0x24), 2);
    *(int *)(unit + 0x2dc) = *(int *)(player + 0x24);
    *(int *)(unit + 0x2e0) = game_time_get();

    {
      float fwd_z = *(float *)(item_obj + 0x2c);
      float abs_fwd_z = fwd_z < 0.0f ? -fwd_z : fwd_z;

      if (abs_fwd_z <= *(double *)0x26ee88) {
        /* Nearly horizontal: compute direction from dot product */
        world_matrix_a =
          (char *)object_get_world_matrix(*(int *)(player + 0x24), out_b);
        world_matrix_b =
          (char *)object_get_world_matrix(*(int *)(player + 0x34), out_a);
        delta[0] =
          *(float *)(world_matrix_a + 0x28) - *(float *)(world_matrix_b + 0x28);
        delta[1] =
          *(float *)(world_matrix_a + 0x2c) - *(float *)(world_matrix_b + 0x2c);
        delta[2] =
          *(float *)(world_matrix_a + 0x30) - *(float *)(world_matrix_b + 0x30);
        cross_product3d(*(float **)0x31fc44, delta, delta); /* dup-args-ok */
        dot = delta[2] * *(float *)(item_obj + 0x2c) +
              delta[1] * *(float *)(item_obj + 0x28) +
              delta[0] * *(float *)(item_obj + 0x24);
        action_type = (dot > 0.0f ? 1 : 0) + 1;
      } else if (*(float *)(item_obj + 0x2c) >= *(float *)0x2533c0) {
        action_type = 4;
      } else {
        action_type = 3;
      }
    }

    *(unsigned char *)(item_obj + 0x424) |= 0x10;
    *(char *)(item_obj + 0x429) = action_type;
    *(char *)(item_obj + 0x42a) = 0;
    break;
  }

  default:
    return false;
  }
  return true;
}

/* unit_control_t layout as used by unit_set_control (from units.c strings):
 *   +0x00  animation_state (byte)
 *   +0x01  aiming_speed (byte)
 *   +0x02  control_flags (uint16)  — flags field
 *   +0x04  weapon_index (int16)
 *   +0x06  grenade_index (int16)
 *   +0x08  zoom_level (int16)
 *   +0x0a  pad
 *   +0x0c  throttle (vec3)
 *   +0x18  primary_trigger (float)
 *   +0x1c  facing_vector (vec3)
 *   +0x28  aiming_vector (vec3)
 *   +0x34  looking_vector (vec3)
 * Total: at least 0x40 bytes. */
typedef struct {
  char animation_state; /* +0x00 */
  char aiming_speed; /* +0x01 */
  int16_t control_flags; /* +0x02 */
  int16_t weapon_index; /* +0x04 */
  int16_t grenade_index; /* +0x06 */
  int16_t zoom_level; /* +0x08 */
  char pad_a[2]; /* +0x0a */
  float throttle_x; /* +0x0c */
  float throttle_y; /* +0x10 */
  float throttle_z; /* +0x14 */
  float primary_trigger; /* +0x18 */
  float facing_x; /* +0x1c */
  float facing_y; /* +0x20 */
  float facing_z; /* +0x24 */
  float aiming_x; /* +0x28 */
  float aiming_y; /* +0x2c */
  float aiming_z; /* +0x30 */
  float looking_x; /* +0x34 */
  float looking_y; /* +0x38 */
  float looking_z; /* +0x3c */
} unit_control_t;

/* player_action_t layout as filled by player_control_get_current_actions:
 *   +0x00  buttons (uint32 flags, bit 6 = binoculars, bit 14 = zoom, bit 7 =
 * alt_attack) +0x04  desired_facing_yaw (float) +0x08  desired_facing_pitch
 * (float) +0x0c  throttle_x (float) +0x10  throttle_y (float) +0x14
 * primary_trigger (float) +0x18  desired_weapon_index (int16) +0x1a
 * desired_grenade_index (int16) +0x1c  desired_zoom_level (int16) +0x1e  pad
 * Total: 0x20 bytes per action entry. */
/* player_action_t now lives in src/types.h, where cs()/co() asserts lock the
 * layout above. */

/* Apply a powerup timer to a player. Despite the kb.json name "respawn_timer",
 * the binary assert and source path show this sets the powerup countdown at
 * player+0x68 (indexed by powerup_type: 0=active_camo, 1=full_spectrum).
 *
 * If the slot is currently empty (timer == 0) and powerup_type == 0 (active
 * camo), also marks the unit at player+0x34 with flag 0x10 in field+0x1b4 and
 * records the type in field+0x3d2.
 *
 * The timer is only ever raised, never lowered: stored = max(current, ticks).
 */
void player_set_respawn_timer(int player_handle, int16_t respawn_type,
                              int16_t respawn_ticks)
{
  char *player;
  char *unit_obj;
  int powerup_idx;

  player = (char *)datum_get(player_data, player_handle);

  /* powerup_type (respawn_type in kb.json) must be 0 or 1 */
  assert_halt(respawn_type >= 0 && respawn_type < 2);

  powerup_idx = (int)respawn_type;

  if (*(int16_t *)(player + 0x68 + powerup_idx * 2) == 0) {
    /* Slot was empty — fetch the unit and mark it. */
    char *player2 = (char *)datum_get(player_data, player_handle);
    unit_obj = (char *)object_get_and_verify_type(*(int *)(player2 + 0x34), 3);
    if (powerup_idx == 0) {
      /* Active camo: set camo-active flag on the unit object. */
      *(unsigned int *)(unit_obj + 0x1b4) |= 0x10;
      *(int16_t *)(unit_obj + 0x3d2) = respawn_type;
    }
  }

  /* Raise the timer: store max(current, ticks). */
  {
    int16_t cur = *(int16_t *)(player + 0x68 + powerup_idx * 2);
    if (cur < respawn_ticks)
      cur = respawn_ticks;
    *(int16_t *)(player + 0x68 + powerup_idx * 2) = cur;
  }
}

/* Decrement the player's short weapon/vehicle timers (at player+0x68,
 * 2 x int16_t).  When a timer reaches zero the corresponding flag bit
 * is cleared on the unit object (bit 0x10 at unit+0x1b4).
 * EBX = datum_handle (register arg). */
void player_update_weapon_timers(int datum_handle)
{
  char *player;
  char *unit;
  int16_t *timer;
  int i;
  int16_t val;

  player = (char *)datum_get(player_data, datum_handle);
  timer = (int16_t *)(player + 0x68);
  for (i = 0; i < 2; i++) {
    val = timer[i];
    if (val > 0) {
      val--;
      timer[i] = val;
      if (val == 0) {
        player = (char *)datum_get(player_data, datum_handle);
        unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
        if (i == 0)
          *(unsigned int *)(unit + 0x1b4) &= ~0x10u;
      }
    }
  }
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((noinline))
#else
__declspec(noinline)
#endif
static bool
players_respawn_coop_teleport(int player_handle, int anchor_unit_handle,
                              void *anchor_position)
{
  return ((bool (*)(int, int, void *))0xbbb80)(
    player_handle, anchor_unit_handle, anchor_position);
}

/* Attempt to respawn all dead players in co-op by teleporting them to a
 * living player's unit position.
 *
 * Returns true if at least one dead player was successfully respawned.
 *
 * Flow:
 *  1. Clear respawn_failure (players_globals+0x2c) to 0.
 *  2. If the respawn-pending flag (players_globals+0x2e) is clear:
 *     a. If dangerous_projectiles_near_player() or any_unit_is_dangerous():
 *        set failure=1, return false.
 *     b. If FUN_425b0() (AI enemies visible/near): set failure=2, return
 *        false.
 *  3. Iterate all players via data_iterator_new/data_iterator_next on
 *     player_data. For each player with a valid unit handle (+0x34 != -1):
 *     - Walk the object parent chain via FUN_13d7f0 to find the root object.
 *     - If root == player->unit (no parent):
 *         call object_try_and_get_type(unit, 1) to get unit data.
 *         live := (unit_data[0x424] & 1) != 0  (alive/shield flag).
 *     - Else (player is seated in a vehicle):
 *         call object_try_and_get_type(root, 2) to get vehicle data.
 *         live := vehicle_data[0x428] != 0  (SETA: passengers > 0).
 *     - If live: set failure=3.
 *     - Else: record unit as iVar7 (a live anchor for respawn).
 *  4. If iVar7 != -1 (at least one living player found):
 *     Reinitialise iterator; for each dead player (unit handle == -1):
 *       - Call FUN_bbcb0(player_handle) to trigger the respawn sequence.
 *       - If player still has no unit: mark respawn failed (bVar2=0).
 *       - Else: look up a spawn position (object_get_and_verify_type(iVar7,
 *         0xffffffff) + 0x50) and call FUN_bbb80(player_handle, iVar7,
 *         spawn_pos) to place them.
 *  5. Update respawn-pending flag and, on success, clear respawn_failure.
 *
 * Uncertain: exact semantics of FUN_bbb80 (player_teleport_to_unit?),
 * FUN_bbcb0 (player_spawn?), FUN_425b0 (ai_enemies_near_player?),
 * and the field meanings at object+0x424, object+0x428. */
bool players_respawn_coop(void)
{
  bool bVar1;
  bool bVar2;
  char uVar3;
  data_iter_t iter;
  char *player;
  int iVar7; /* handle of a living player's unit used as respawn anchor */
  void *live_obj;

  /* Step 1: clear respawn failure code. */
  *(int16_t *)((char *)players_globals + 0x2c) = 0;
  bVar2 = 0;

  /* Step 2a: if not already in a wait state, check for hazards. */
  if (*((char *)players_globals + 0x2e) == 0) {
    if (dangerous_projectiles_near_player() || any_unit_is_dangerous()) {
      *(int16_t *)((char *)players_globals + 0x2c) = 1;
      return (bool)bVar2;
    }
  }

  /* Step 2b: check if enemies are nearby (AI visibility). */
  if (*((char *)players_globals + 0x2e) == 0) {
    if (((bool (*)(void))0x425b0)()) {
      *(int16_t *)((char *)players_globals + 0x2c) = 2;
      return (bool)bVar2;
    }
  }

  /* Step 3: scan all players; find a live anchor unit. */
  iVar7 = NONE;
  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    if (*(int *)(player + 0x34) != NONE) {
      /* Walk parent chain to root object. */
      int root = ((int (*)(int))0x13d7f0)(*(int *)(player + 0x34));
      if (root == *(int *)(player + 0x34)) {
        /* No parent: check unit data at +0x424. */
        char *udata = (char *)object_try_and_get_and_verify_type(
          *(int *)(player + 0x34), 1);
        if (udata != NULL) {
          bVar1 = (udata[0x424] & 1) != 0;
          goto check_live;
        }
      } else {
        /* Has parent (seated): check vehicle data at +0x428. */
        char *vdata = (char *)object_try_and_get_and_verify_type(root, 2);
        if (vdata != NULL) {
          bVar1 = (unsigned char)vdata[0x428] > 0;
          goto check_live;
        }
      }
      iVar7 = *(int *)(player + 0x34);
      continue;
    check_live:
      if (bVar1) {
        *(int16_t *)((char *)players_globals + 0x2c) = 3;
      } else {
        iVar7 = *(int *)(player + 0x34);
      }
    }
  }

  /* Step 4: respawn dead players near the live anchor. */
  if (iVar7 != NONE) {
    bVar2 = 1;
    data_iterator_new(&iter, player_data);
    while ((player = (char *)data_iterator_next(&iter)) != NULL) {
      if (*(int *)(player + 0x34) == NONE) {
        /* Try to spawn the dead player. */
        player_spawn(iter.datum_handle);
        if (*(int *)(player + 0x34) == NONE) {
          /* Still dead: respawn failed. */
          bVar2 = 0;
        } else {
          /* Teleport to anchor unit's position (+0x50). */
          live_obj = object_get_and_verify_type(iVar7, 0xffffffff);
          bVar2 = players_respawn_coop_teleport(iter.datum_handle, iVar7,
                                                (char *)live_obj + 0x50);
        }
      }
    }
  }

  /* Step 5: update the respawn-pending flag and clear failure on success. */
  if (*((char *)players_globals + 0x2e) == 0 || bVar2 != 0) {
    uVar3 = 0;
  } else {
    uVar3 = 1;
  }
  *((char *)players_globals + 0x2e) = uVar3;
  if (bVar2 != 0) {
    *(int16_t *)((char *)players_globals + 0x2c) = 0;
  }
  return bVar2;
}

/* Update one player's unit before game logic runs on the client (0xbc920).
 *
 * @<ebx> = player_index (register arg); object_handle and position are cdecl
 * stack args (position is asserted non-NULL).  Looks up the player's unit and
 * decides whether it must be re-seated / repositioned this tick:
 *   - If the scenario cluster filter (players_globals+0x2a) is active and the
 *     unit is NOT in that cluster (FUN_0018ef00 == 0), force the update.
 *   - Else, if the unit is at a valid location (FUN_0018e720 != -1), bail.
 *   - If the unit holds a seat handle (+0xcc) that differs from the passed
 *     object's seat, exit the seat; if it still holds one, clear the
 *     pending-flag (players_globals+0x2e) and return.
 *   - Otherwise defer to FUN_000bb670 and record its bool result inverted
 *     into players_globals+0x2e.
 */
void players_update_before_game_client(int player_index /* @<ebx> */,
                                       int object_handle, void *position)
{
  char *player;
  int unit_handle;
  char *unit;
  char *other_obj;
  bool skip;
  int16_t cluster;
  int loc;
  void *scenario;
  int16_t *element;
  char in_cluster;
  char moved;

  player = (char *)datum_get(player_data, player_index);
  unit_handle = *(int *)(player + 0x34);
  unit = (char *)object_try_and_get_and_verify_type(unit_handle, 1);
  if (player_index == -1) {
    display_assert("player_index!=NONE", "c:\\halo\\SOURCE\\game\\players.c",
                   0x4c7, 1);
    system_exit(-1);
  }
  if (position == NULL) {
    display_assert("position", "c:\\halo\\SOURCE\\game\\players.c", 0x4c8, 1);
    system_exit(-1);
  }
  if (unit == NULL)
    return;

  cluster = *(int16_t *)((char *)players_globals + 0x2a);
  if (cluster != -1) {
    scenario = global_scenario_get();
    element = (int16_t *)tag_block_get_element((char *)scenario + 0x39c,
                                               (int)cluster, 8);
    in_cluster = FUN_0018ef00((int)*element, unit_handle);
    skip = (in_cluster == 0);
  } else {
    skip = false;
  }

  loc = FUN_0018e720((int)(unit + 0x50));
  if (loc != -1 && !skip)
    return;

  if (*(int *)(unit + 0xcc) != -1) {
    other_obj = (char *)object_get_and_verify_type(object_handle, 1);
    if (*(int *)(unit + 0xcc) != *(int *)(other_obj + 0xcc))
      unit_exit_seat_end(unit_handle);
    if (*(int *)(unit + 0xcc) != -1) {
      *((char *)players_globals + 0x2e) = 0;
      return;
    }
  }

  moved = FUN_000bb670(player_index, object_handle, position);
  *((char *)players_globals + 0x2e) = (moved == 0);
}

/* Priority-filtered pending action-result update (matches 0xbbfe0). */
static void player_set_spawn_action_result(int player_handle,
                                           int16_t action_result_type,
                                           int object_handle,
                                           int16_t seat_index)
{
  char *player;

  player = (char *)datum_get(player_data, player_handle);
  if (action_result_type != 11) {
    int16_t current_type = *(int16_t *)(player + 0x28);
    if (action_result_type == current_type) {
      char *unit_obj;
      char *cur_obj;
      char *new_obj;
      float cur_dx;
      float cur_dy;
      float cur_dz;
      float new_dx;
      float new_dy;
      float new_dz;
      float cur_dist;
      float new_dist;

      unit_obj =
        (char *)object_get_and_verify_type(*(int *)(player + 0x34), -1);
      cur_obj = (char *)object_get_and_verify_type(*(int *)(player + 0x24), -1);
      new_obj = (char *)object_get_and_verify_type(object_handle, -1);

      cur_dx = *(float *)(cur_obj + 0xc) - *(float *)(unit_obj + 0xc);
      cur_dy = *(float *)(cur_obj + 0x10) - *(float *)(unit_obj + 0x10);
      cur_dz = *(float *)(cur_obj + 0x14) - *(float *)(unit_obj + 0x14);

      new_dx = *(float *)(new_obj + 0xc) - *(float *)(unit_obj + 0xc);
      new_dy = *(float *)(new_obj + 0x10) - *(float *)(unit_obj + 0x10);
      new_dz = *(float *)(new_obj + 0x14) - *(float *)(unit_obj + 0x14);

      cur_dist =
        xbox_sqrtf(cur_dx * cur_dx + cur_dy * cur_dy + cur_dz * cur_dz);
      new_dist =
        xbox_sqrtf(new_dx * new_dx + new_dy * new_dy + new_dz * new_dz);
      if (cur_dist <= new_dist)
        return;
    } else if (action_result_type <= current_type) {
      return;
    }
  }

  *(int16_t *)(player + 0x28) = action_result_type;
  *(int *)(player + 0x24) = object_handle;
  *(int16_t *)(player + 0x2a) = seat_index;
}

void player_update_nearby_biped(int datum_handle, int object_handle)
{
  char *player;
  char *nearby_biped;
  char *unit;
  void *game_globals;
  char *difficulty_entry;
  float angle_delta;
  int16_t seat_index;
  int16_t seat_state;

  player = (char *)datum_get(player_data, datum_handle);
  nearby_biped = (char *)object_get_and_verify_type(object_handle, 2);
  if ((*(unsigned char *)(nearby_biped + 0xb6) & 4) != 0)
    return;

  game_globals = game_globals_get();
  difficulty_entry =
    (char *)tag_block_get_element((char *)game_globals + 0x110, 0, 0x80);
  angle_delta = *(float *)0x2568bc - *(float *)(difficulty_entry + 0x70);

  nearby_biped = (char *)object_get_and_verify_type(object_handle, 2);
  if (*(float *)(nearby_biped + 0x38) <= xbox_cosf(angle_delta)) {
    if ((*(unsigned char *)(nearby_biped + 0x424) & 0x10) == 0 &&
        *(int *)(nearby_biped + 0x2d4) == -1) {
      player_set_spawn_action_result(datum_handle, 11, object_handle, -1);
    }
  } else {
    if (unit_current_weapon_is_busy(*(int *)(player + 0x34)))
      return;

    unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
    if (*(float *)(unit + 0x20) * *(float *)(unit + 0x20) +
          *(float *)(unit + 0x1c) * *(float *)(unit + 0x1c) +
          *(float *)(unit + 0x18) * *(float *)(unit + 0x18) >=
        *(float *)0x25bb10)
      return;

    nearby_biped = (char *)object_get_and_verify_type(object_handle, 2);
    if (FUN_00012170((float *)(nearby_biped + 0x3c)) >= *(float *)0x25bb10)
      return;

    seat_index = -1;
    seat_state = unit_find_best_enter_seat(*(int *)(player + 0x34),
                                           object_handle, &seat_index);

    if (seat_state == 2) {
      if (seat_index == -1) {
        display_assert("seat_index != NONE",
                       "c:\\halo\\SOURCE\\game\\players.c", 0x838, 1);
        system_exit(-1);
      }
      player_set_spawn_action_result(datum_handle, 8, object_handle,
                                     seat_index);
      return;
    }
    if (seat_state == 1) {
      if (seat_index == -1) {
        display_assert("seat_index != NONE",
                       "c:\\halo\\SOURCE\\game\\players.c", 0x83d, 1);
        system_exit(-1);
      }
      player_set_spawn_action_result(datum_handle, 9, object_handle,
                                     seat_index);
      return;
    }
  }
}

void player_update_nearby_weapon(int datum_handle, int object_handle)
{
  char *player;
  char *unit;
  char *weapon;
  float local_position[3];

  player = (char *)datum_get(player_data, datum_handle);
  unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  weapon = (char *)object_get_and_verify_type(object_handle, 0x380);

  unit_set_seat_state(*(int *)(player + 0x34), local_position);
  if (!fast_vector_intersects_sphere(local_position, (float *)(unit + 0x1ec),
                                     (float *)(weapon + 0x50),
                                     *(float *)(weapon + 0x5c)))
    return;
  if (!FUN_000971a0(object_handle, local_position, (float *)(unit + 0x1ec)))
    return;
  if (!device_can_change_position(object_handle))
    return;

  player_set_spawn_action_result(datum_handle, 10, object_handle, -1);
}

/* Handle the result of a player interacting with an equipment (powerup) object.
 *
 * Reads the equipment's tag definition to determine the powerup type
 * (offset 0x308 in the 'eqip' tag) and the duration (offset 0x30c,
 * multiplied by 30 ticks/second).  Dispatches by powerup type:
 *   1 = double speed  — adds ticks to players_globals+0x26, enables flag
 *   2 = overshield    — checks unit body vitality, triggers shield effect
 *   5 = health        — checks unit shield vitality, triggers health effect
 *   3 = active camo   — powerup index 0, calls player_try_to_apply_powerup
 *   4 = full-spectrum — powerup index 1, calls player_try_to_apply_powerup
 * On success, notifies the scoring system, plays the equipment pickup
 * sound, and deactivates the equipment object. */
void player_set_action_result_for_equipment(int player_handle,
                                            int equipment_handle)
{
  char *player;
  char *eqip_obj;
  char *tag;
  int16_t powerup_type;
  int16_t ticks;
  int powerup_index;

  player = (char *)datum_get(player_data, player_handle);
  eqip_obj = (char *)object_get_and_verify_type(equipment_handle, 8);
  tag = (char *)tag_get(0x65716970, *(int *)eqip_obj);

  /* Duration in ticks: tag float * 30.0f, truncated to int16_t. */
  ticks = (int16_t)(*(float *)(tag + 0x30c) * 30.0f);
  if (ticks <= 0)
    return;

  powerup_type = *(int16_t *)(tag + 0x308);

  if (powerup_type == 1) {
    /* Double speed: accumulate ticks and set flag. */
    *(int16_t *)((char *)players_globals + 0x26) += ticks;
    game_set_players_are_double_speed(true);
  } else if (powerup_type == 2) {
    /* Overshield: check if unit can receive it. */
    if (!object_double_charge_shield(*(int *)(player + 0x34)))
      return;
    player_apply_overshield_effect(player_handle);
  } else if (powerup_type == 5) {
    /* Health: check if unit can receive it. */
    if (!object_restore_body(*(int *)(player + 0x34)))
      return;
    player_apply_health_effect(player_handle);
  } else {
    /* Active camo (3) or full-spectrum vision (4). */
    if (powerup_type == 3) {
      powerup_index = 0;
    } else if (powerup_type == 4) {
      powerup_index = 1;
    } else {
      display_assert(0, "c:\\halo\\SOURCE\\game\\players.c", 0xac7, 1);
      system_exit(-1);
    }
    /* Try to apply the powerup. */
    if (!((bool (*)(int, int, int16_t))0xbc320)(player_handle, powerup_index,
                                                ticks))
      return;
    /* Active camo (index 0) triggers a location notification. */
    if ((int16_t)powerup_index == 0) {
      player_apply_camo_notification(player_handle);
    }
  }

  /* Common exit: notify scoring, play pickup sound, deactivate equipment. */
  eqip_obj = (char *)object_get_and_verify_type(equipment_handle, 8);
  {
    int16_t local_player_idx =
      *(int16_t *)(player + 2); /* player+0x2: local_player_index */
    ((void (*)(int, int))0xd0c60)((unsigned short)local_player_idx,
                                  *(int *)eqip_obj);
  }
  if (*(int16_t *)(player + 2) != -1) {
    item_activate_equipment_effect(equipment_handle);
  }
  object_delete(equipment_handle);
}

/* Update all player actions before game logic runs for this tick.
 *
 * For each player:
 *   1. Validate the action data received from player_control.
 *   2. If the player has no unit, try to spawn them (in a vehicle or
 *      normally), or defer to the game engine's respawn logic.
 *   3. If the player has a live unit and input is not inhibited, build
 *      a unit_control_t from the action and apply it to the unit via
 *      unit_set_control.  If input IS inhibited but the unit has no
 *      vehicle seat, derive a neutral control from the unit's current
 *      facing/aiming/looking vectors and apply that instead.
 * After iterating all players, update both local and full PVS, then
 * recount local players into players_globals+0x24. */
void players_update_before_game(void)
{
  player_action_t action_buf[16]; /* [EBP-0x2a8]: 16*0x20 = 0x200 bytes */
  data_iter_t iter; /* [EBP-0x14]                          */
  int datum_handle; /* [EBP-0xc] = iter.datum_handle       */
  char *player; /* [EBP-0x4] = current player datum ptr */
  unit_control_t ctl; /* [EBP-0x68]: control for enabled-input path */
  unit_control_t ctl2; /* [EBP-0xac]: control for disabled-input path */
  int action_index;
  player_action_t *action;
  char *unit_data;
  int unit_handle;
  unit_control_t *ctl_ptr;
  char *def_zero; /* ptr to default zero vector (*(char**)0x31fc38) */

  /* Profile enter. */
  if (*(char *)0x449ef1 != 0 && *(char *)0x2f0898 != 0)
    profile_enter_private((void *)0x2f0890);

  /* Collect current player actions from the controller subsystem.
   * action_buf receives up to 16 entries (one per network player slot),
   * each 0x20 bytes. Returns false if the action queue is not ready. */
  if (!player_control_get_current_actions(action_buf)) {
    display_assert(NULL, "c:\\halo\\SOURCE\\game\\players.c", 0x30a, 1);
    system_exit(-1);
  }

  /* Iterate all player datums. */
  data_iterator_new(&iter, player_data);
  player = (char *)data_iterator_next(&iter);
  while (player != NULL) {
    datum_handle = (int)iter.datum_handle;

    /* action_index = low 16 bits of datum handle (slot index in action_buf).
     * Each entry is 0x20 bytes wide. */
    action_index = (int)(int16_t)(datum_handle & 0xffff);

    if (action_index < 0 || action_index >= 16) {
      display_assert(
        "action_index>=0 && action_index<NETWORK_GAME_MAXIMUM_PLAYER_COUNT",
        "c:\\halo\\SOURCE\\game\\players.c", 0x255, 1);
      system_exit(-1);
    }
    action = &action_buf[action_index];

    /* --- Validate action fields: NaN/inf checks on floats --- */

    /* desired_facing.pitch */
    if (((*(uint32_t *)&action->desired_facing_pitch) & 0x7f800000u) ==
        0x7f800000u) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
               "action->desired_facing.pitch",
               *(uint32_t *)&action->desired_facing_pitch,
               (double)action->desired_facing_pitch,
               "c:\\halo\\SOURCE\\game\\players.c", 0x25a, 1);
      display_assert(NULL, "c:\\halo\\SOURCE\\game\\players.c", 0x25a, 1);
      system_exit(-1);
    }

    /* desired_facing.yaw */
    if (((*(uint32_t *)&action->desired_facing_yaw) & 0x7f800000u) ==
        0x7f800000u) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
               "action->desired_facing.yaw",
               *(uint32_t *)&action->desired_facing_yaw,
               (double)action->desired_facing_yaw,
               "c:\\halo\\SOURCE\\game\\players.c", 0x25b, 1);
      display_assert(NULL, "c:\\halo\\SOURCE\\game\\players.c", 0x25b, 1);
      system_exit(-1);
    }

    /* throttle (2D vector) */
    if (((*(uint32_t *)&action->throttle_x) & 0x7f800000u) == 0x7f800000u ||
        ((*(uint32_t *)&action->throttle_y) & 0x7f800000u) == 0x7f800000u) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f)",
               "&action->throttle", (double)action->throttle_x,
               (double)action->throttle_y, "c:\\halo\\SOURCE\\game\\players.c",
               0x25c, 1);
      display_assert(NULL, "c:\\halo\\SOURCE\\game\\players.c", 0x25c, 1);
      system_exit(-1);
    }

    /* primary_trigger */
    if (((*(uint32_t *)&action->primary_trigger) & 0x7f800000u) ==
        0x7f800000u) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
               "action->primary_trigger", *(uint32_t *)&action->primary_trigger,
               (double)action->primary_trigger,
               "c:\\halo\\SOURCE\\game\\players.c", 0x25d, 1);
      display_assert(NULL, "c:\\halo\\SOURCE\\game\\players.c", 0x25d, 1);
      system_exit(-1);
    }

    /* desired_weapon_index: NONE or [0..MAXIMUM_WEAPONS_PER_UNIT=4] */
    if (action->desired_weapon_index != -1 &&
        (action->desired_weapon_index < 0 ||
         action->desired_weapon_index > 4)) {
      display_assert(
        "(NONE == action->desired_weapon_index) || ((action->desired_weapon_"
        "index >= 0) && (action->desired_weapon_index <= "
        "MAXIMUM_WEAPONS_PER_UNIT))",
        "c:\\halo\\SOURCE\\game\\players.c", 0x25f, 1);
      system_exit(-1);
    }

    /* desired_grenade_index: NONE or [0..NUMBER_OF_UNIT_GRENADE_TYPES=2] */
    if (action->desired_grenade_index != -1 &&
        (action->desired_grenade_index < 0 ||
         action->desired_grenade_index > 2)) {
      display_assert(
        "(NONE == action->desired_grenade_index) || ((action->desired_grenade"
        "_index >= 0) && (action->desired_grenade_index <= "
        "NUMBER_OF_UNIT_GRENADE_TYPES))",
        "c:\\halo\\SOURCE\\game\\players.c", 0x260, 1);
      system_exit(-1);
    }

    /* desired_zoom_level: NONE or >= 0 */
    if (action->desired_zoom_level != -1 && action->desired_zoom_level < 0) {
      display_assert(
        "(NONE == action->desired_zoom_level) || ((action->desired_zoom_level"
        " >= 0))",
        "c:\\halo\\SOURCE\\game\\players.c", 0x261, 1);
      system_exit(-1);
    }

    /* --- Spawn logic: player currently has no unit --- */
    if (*(int *)(player + 0x34) == -1 && !game_in_editor()) {
      if (game_engine_running()) {
        /* Multiplayer / game-engine managed respawn.
         * FUN_a8c80: check if player is allowed to respawn (timer, etc).
         * FUN_a8df0: clear the respawn window state.
         * FUN_ad3e0: post-spawn game-engine notification. */
        if (((bool (*)(int))0xa8c80)(datum_handle)) {
          ((void (*)(int))0xa8df0)(datum_handle);
          player_spawn(datum_handle);
          if (*(int *)(player + 0x34) == -1) {
            /* Spawn failed — mark respawn deferred at player+0x2c. */
            *(int *)(player + 0x2c) = 1;
          } else {
            ((void (*)(int))0xad3e0)(datum_handle);
          }
        }
      } else {
        /* Single-player / co-op spawn path.
         * FUN_e43e0: check if some cutscene/mode blocks spawning. */
        if (!((bool (*)(void))0xe43e0)()) {
          if (*(int16_t *)(player + 0xaa) == 0) {
            /* Normal: spawn the player. */
            player_spawn(datum_handle);
          } else if (*((char *)players_globals + 0x28) == 0) {
            /* All-dead flag is clear: call the deferred-respawn helper with
             * players_globals+0x2e byte (respawn context). */
            ((void (*)(int))0x100390)(
              (int)(unsigned char)*((char *)players_globals + 0x2e));
          }
        }
      }
    }

    /* --- Control logic: player has a unit --- */
    unit_handle = *(int *)(player + 0x34);
    if (unit_handle == -1 || !unit_is_alive(unit_handle))
      goto next_player;

    /* Resolve unit data pointer (type 3 = biped/unit). */
    unit_data = (char *)object_get_and_verify_type(unit_handle, 3);

    if (*((char *)players_globals + 0x29) == 0) {
      /* Input is ENABLED. */

      /* Binoculars request (action bit 6): if the unit has no active
       * weapon-seat tag (unit+0xcc == -1) and the game is not in a
       * "no-binoculars" state, set binoculars-pending flag (bit 10). */
      if ((action->buttons & 0x40u) != 0 && *(int *)(unit_data + 0xcc) == -1 &&
          !player_try_to_spawn_in_vehicle(datum_handle)) {
        action->buttons |= 0x400u;
      }

      /* Zoom tracking: player+0x3e caches the zoom-change result.
       * Clear it unless the zoom-hold flag (bit 14) is set and the
       * unit has no active weapon-seat tag. */
      if ((action->buttons & 0x4000u) == 0 ||
          *(int *)(unit_data + 0xcc) != -1) {
        *(char *)(player + 0x3e) = 0;
      } else if (*(char *)(player + 0x3e) == 0) {
        *(char *)(player + 0x3e) =
          (char)player_try_to_enter_vehicle(datum_handle);
      }

      /* Alt-attack / throw-weapon (sign bit of action->buttons byte 0):
       * if set and unit has a weapon in the special slot (unit+0x2c8 != -1),
       * invoke the vehicle-action result handler and clear the seat tag. */
      if ((*(char *)&action->buttons & 0x80) != 0 &&
          *(int *)(unit_data + 0x2c8) != -1) {
        player_set_action_result_for_equipment(datum_handle,
                                               *(int *)(unit_data + 0x2c8));
        unit_clear_seat_tag(*(int *)(player + 0x34));
      }

      /* Determine active weapon handle and handle zoom-change request.
       *
       * Re-fetches unit data (compiler re-fetched the pointer after the
       * intervening writes above), reads the currently-selected weapon slot
       * index from unit+0x2a2 (sign-extended), then calls unit_get_weapon to
       * get the weapon's datum handle.  If valid and the weapon can zoom,
       * propagate scope change (bits 0x1800) to the unit, then sync the
       * active weapon index into the action's desired_weapon_index. */
      {
        char *udata2;
        int16_t active_wi;
        int wep_handle;

        udata2 = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
        active_wi = *(int16_t *)(udata2 + 0x2a2);
        wep_handle = unit_get_weapon(*(int *)(player + 0x34), active_wi);

        if (wep_handle != -1 && ((bool (*)(int))0xfb090)(wep_handle)) {
          /* Weapon can zoom. If scope-change bits set, call FUN_1ae600. */
          if (action->buttons & 0x1800u) {
            unit_set_in_vehicle(*(int *)(player + 0x34), 1);
          }
          /* Sync active weapon index into the action record. */
          action->desired_weapon_index = *(int16_t *)(unit_data + 0x2a2);
        }
      }

      /* Build the unit_control_t from the player action.
       * player_build_action_update writes three floats via
       * internal helper 0x10cc40 at offsets +0/+4/+8 relative to
       * arg2, so arg2 must point at the aiming vector slot
       * (ctl+0x28 = &ctl.aiming_x), NOT at the ctl header.
       * Original 0xbd563 does LEA ECX,[EBP-0x40] = &ctl.aiming_x.
       * Arg1 is the full 32-bit datum_handle (salt|index), forwarded
       * to data_get helper 0x119320. Arg3 is &action->desired_facing_yaw
       * (yaw/pitch float pair). */
      csmemset(&ctl, 0, sizeof(ctl));
      ctl.control_flags = (int16_t)action->buttons;
      player_build_action_update(datum_handle, &ctl.aiming_x,
                                 &action->desired_facing_yaw);

      /* Original 0xbd57a-0xbd58f copies aiming into both facing
       * (ctl+0x1c) and looking (ctl+0x34). player_build_action_update
       * only writes aiming; the caller is responsible for mirroring
       * it into the other two vectors so unit_set_control's unit-vector
       * validation doesn't see zero-length facing/looking. */
      ctl.facing_x = ctl.aiming_x;
      ctl.facing_y = ctl.aiming_y;
      ctl.facing_z = ctl.aiming_z;
      ctl.looking_x = ctl.aiming_x;
      ctl.looking_y = ctl.aiming_y;
      ctl.looking_z = ctl.aiming_z;

      /* Copy action scalars into control (player_build_action_update fills
       * facing/aiming/looking vectors but leaves these untouched). */
      ctl.throttle_x = action->throttle_x;
      ctl.weapon_index = action->desired_weapon_index;
      ctl.throttle_y = action->throttle_y;
      ctl.grenade_index = action->desired_grenade_index;
      ctl.primary_trigger = action->primary_trigger;
      ctl.zoom_level = action->desired_zoom_level;
      ctl.animation_state = 3;
      ctl.aiming_speed = 0;

      /* Validate assembled control data (mirrors unit_set_control
       * internal checks). */
      if (ctl.weapon_index != -1 &&
          (ctl.weapon_index < 0 || ctl.weapon_index > 4)) {
        display_assert(
          "(NONE == control_data.weapon_index) || ((control_data.weapon_"
          "index >= 0) && (control_data.weapon_index <= "
          "MAXIMUM_WEAPONS_PER_UNIT))",
          "c:\\halo\\SOURCE\\game\\players.c", 0x2e2, 1);
        system_exit(-1);
      }
      if (ctl.grenade_index != -1 &&
          (ctl.grenade_index < 0 || ctl.grenade_index > 2)) {
        display_assert(
          "(NONE == control_data.grenade_index) || ((control_data.grenade_"
          "index >= 0) && (control_data.grenade_index <= "
          "NUMBER_OF_UNIT_GRENADE_TYPES))",
          "c:\\halo\\SOURCE\\game\\players.c", 0x2e3, 1);
        system_exit(-1);
      }
      if (ctl.zoom_level != -1 && ctl.zoom_level < 0) {
        display_assert(
          "(NONE == control_data.zoom_level) || ((control_data.zoom_level"
          " >= 0))",
          "c:\\halo\\SOURCE\\game\\players.c", 0x2e4, 1);
        system_exit(-1);
      }
      ctl_ptr = &ctl;

    } else {
      /* Input is DISABLED (players_globals+0x29 != 0). */

      /* If the unit is currently seated in a vehicle (1a4/1a8 != -1),
       * skip the control update entirely. */
      if (*(int *)(unit_data + 0x1a8) != -1 ||
          *(int *)(unit_data + 0x1a4) != -1)
        goto next_player;

      /* Build a neutral control using the unit's own stored orientation
       * vectors, so unit_set_control's normalization asserts pass.
       * Original 0xbd6b3-0xbd722 writes:
       *   ctl2.throttle ← *(char**)0x31fc38 (zero vector)
       *   ctl2.facing   ← unit[0x1d4..0x1dc] (unit's current facing)
       *   ctl2.aiming   ← unit[0x1e0..0x1e8] (unit's current aiming)
       *   ctl2.looking  ← unit[0x204..0x20c] (unit's current looking) */
      csmemset(&ctl2, 0, sizeof(ctl2));
      ctl2.weapon_index = -1;
      ctl2.grenade_index = -1;
      ctl2.zoom_level = -1;
      ctl2.animation_state = 3;
      ctl2.aiming_speed = 0;
      ctl2.control_flags = 0;

      /* throttle = zero vector (*(char**)0x31fc38 → vec3 at [+0..+8]) */
      def_zero = *(char **)0x31fc38;
      ctl2.throttle_x = *(float *)(def_zero + 0);
      ctl2.throttle_y = *(float *)(def_zero + 4);
      ctl2.throttle_z = *(float *)(def_zero + 8);

      /* facing = unit's current facing vector (unit+0x1d4..0x1dc) */
      ctl2.facing_x = *(float *)(unit_data + 0x1d4);
      ctl2.facing_y = *(float *)(unit_data + 0x1d8);
      ctl2.facing_z = *(float *)(unit_data + 0x1dc);

      /* aiming = unit's current aiming vector (unit+0x1e0..0x1e8) */
      ctl2.aiming_x = *(float *)(unit_data + 0x1e0);
      ctl2.aiming_y = *(float *)(unit_data + 0x1e4);
      ctl2.aiming_z = *(float *)(unit_data + 0x1e8);

      /* looking = unit's current looking vector (unit+0x204..0x20c) */
      ctl2.looking_x = *(float *)(unit_data + 0x204);
      ctl2.looking_y = *(float *)(unit_data + 0x208);
      ctl2.looking_z = *(float *)(unit_data + 0x20c);

      ctl_ptr = &ctl2;
    }

    /* Apply the computed control to the unit. */
    unit_set_control(unit_handle, ctl_ptr);

  next_player:
    player = (char *)data_iterator_next(&iter);
  }

  /* Update potential visibility sets:
   * local players first (pass 1), then all players (pass 0).
   * players_update_pvs takes combined_pvs via EDI; original 0xbd100
   * reloads players_globals and offsets by 0x70 then 0x30 before
   * each call. */
  players_update_pvs(players_get_combined_pvs_local(), 1);
  players_update_pvs(players_get_combined_pvs(), 0);

  /* Recount local players: walk the 4 player-handle slots at
   * players_globals+0x4..0x10 and count non-NONE entries.
   * Result is stored at players_globals+0x24 (local_player_count field). */
  {
    int16_t count;
    int *slot;
    int i;

    count = 0;
    slot = (int *)((char *)players_globals + 4);
    for (i = 0; i < 4; i++, slot++) {
      if (*slot != -1)
        count++;
    }
    *(int16_t *)((char *)players_globals + 0x24) = count;
  }

  /* Profile exit. */
  if (*(char *)0x449ef1 != 0 && *(char *)0x2f0898 != 0)
    profile_exit_private((void *)0x2f0890);
}

void player_update_nearby_vehicle(int datum_handle, int object_handle)
{
  char *player;
  char *unit;
  char *nearby;
  int16_t local_player_index;
  int16_t i;
  int16_t seat_index;
  int nearby_weapon_count;
  int current_weapon_handle;
  int *equipment_obj;
  int *nearby_weapon_obj;
  int *current_weapon_obj;
  char *equipment_tag;
  char *nearby_weapon_tag;
  char *current_weapon_tag;
  bool in_vehicle_scope_state;
  bool current_is_special;
  int seat_occupant;

  player = (char *)datum_get(player_data, datum_handle);
  unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  nearby = (char *)object_get_and_verify_type(object_handle, 0x1c);

  if (*(int *)(nearby + 0xcc) != -1 ||
      *(int *)(nearby + 0x1b0) == *(int *)(player + 0x34))
    return;

  local_player_index = *(int16_t *)(player + 2);

  for (i = 0; i < 4; i++) {
    seat_occupant = *(int *)(unit + 0x2a8 + (int)i * 4);
    if (seat_occupant != -1 && weapon_handle_potential_inventory_item(
                                 seat_occupant, object_handle,
                                 (uint16_t)local_player_index, &seat_index)) {
      if (seat_index > 0) {
        equipment_obj = (int *)object_get_and_verify_type(seat_occupant, 4);
        hud_player_enter_vehicle((uint16_t)local_player_index, *equipment_obj,
                                 seat_index);
      }
      break;
    }
  }

  equipment_obj = (int *)object_try_and_get_and_verify_type(object_handle, 8);
  if (equipment_obj != NULL) {
    equipment_tag = (char *)tag_get(0x65716970, *equipment_obj);
    if (*(int16_t *)(equipment_tag + 0x308) == 6) {
      if (unit_try_add_grenade(*(int *)(player + 0x34), object_handle)) {
        hud_player_set_equipment((uint16_t)local_player_index, *equipment_obj);
      }
    } else if (*(int16_t *)(equipment_tag + 0x308) != 0) {
      seat_occupant = unit_get_equipment(*(int *)(player + 0x34));
      if (seat_occupant == -1) {
        player_set_action_result_for_equipment(datum_handle, object_handle);
      } else {
        object_get_and_verify_type(seat_occupant, 8);
        current_weapon_tag = (char *)tag_get(0x65716970, *equipment_obj);
        if (*(int16_t *)(equipment_tag + 0x308) !=
            *(int16_t *)(current_weapon_tag + 0x308)) {
          player_set_spawn_action_result(datum_handle, 5, object_handle, -1);
        }
      }
    }
  }

  nearby_weapon_obj =
    (int *)object_try_and_get_and_verify_type(object_handle, 4);
  if (nearby_weapon_obj == NULL ||
      !unit_can_enter_seat(*(int *)(player + 0x34), object_handle))
    return;

  nearby_weapon_tag = (char *)tag_get(0x77656170, *nearby_weapon_obj);
  in_vehicle_scope_state = (*(unsigned int *)(unit + 0x1b8) & 0x1800) != 0;
  unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  current_weapon_handle =
    unit_get_weapon(*(int *)(player + 0x34), *(int16_t *)(unit + 0x2a2));
  nearby_weapon_count = unit_count_weapons(*(int *)(player + 0x34));

  current_is_special = false;
  if (nearby_weapon_count > 1 && current_weapon_handle != -1 &&
      (*(unsigned char *)(nearby_weapon_tag + 0x308) & 0x10) == 0) {
    current_weapon_obj =
      (int *)object_get_and_verify_type(current_weapon_handle, 4);
    current_weapon_tag = (char *)tag_get(0x77656170, *current_weapon_obj);
    if ((*(unsigned char *)(current_weapon_tag + 0x308) & 0x10) != 0) {
      current_is_special = true;
    }
  }

  if (in_vehicle_scope_state &&
      (*(unsigned char *)(nearby_weapon_tag + 0x308) & 8) != 0)
    return;

  if (player_examine_nearby_unit(*(int *)(player + 0x34), object_handle)) {
    if (unit_enter_seat(*(int *)(player + 0x34), object_handle, 1)) {
      nearby_weapon_obj = (int *)object_get_and_verify_type(object_handle, 4);
      hud_player_set_vehicle((uint16_t)local_player_index, *nearby_weapon_obj);
      player_clear_aim_assist(*(int *)(player + 0x34));
      return;
    }
  } else {
    if (!current_is_special &&
        unit_should_swap_weapon(*(int *)(player + 0x34), object_handle)) {
      current_weapon_obj =
        (int *)object_try_and_get_and_verify_type(current_weapon_handle, 4);
      if (nearby_weapon_count == 1 && current_weapon_obj != NULL &&
          *current_weapon_obj != *nearby_weapon_obj) {
        player_set_spawn_action_result(datum_handle, 7, object_handle, -1);
        return;
      }
      player_set_spawn_action_result(datum_handle, 6, object_handle, -1);
    }
  }
}

/* Check nearby objects via spatial query and dispatch spawn-state events.
 * For each object found within the unit's bounding sphere, switch on the
 * object type to call the appropriate handler.
 * EBX = datum_handle (register arg). */
void player_update_spawn_state(int datum_handle)
{
  char *player;
  char *unit;
  uint16_t count;
  int handles[16];
  int i;
  char *obj;
  int16_t obj_type;

  player = (char *)datum_get(player_data, datum_handle);
  if (*(int *)(player + 0x34) == -1)
    return;
  unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  if (*(int *)(unit + 0xcc) != -1)
    return;

  count = (uint16_t)object_find_in_radius(
    0, 0x11f, (float *)(unit + 0x48), (float *)(unit + 0x50),
    *(float *)(unit + 0x5c), handles, 0x10);
  if ((int16_t)count <= 0)
    return;

  for (i = 0; i < (int16_t)count; i++) {
    obj = (char *)object_get_and_verify_type(handles[i], -1);
    obj_type = *(int16_t *)(obj + 0x64);
    switch (obj_type) {
    case 1:
      player_update_nearby_biped(datum_handle, handles[i]);
      break;
    case 2:
    case 3:
      player_update_nearby_vehicle(datum_handle, handles[i]);
      break;
    case 8:
      player_update_nearby_weapon(datum_handle, handles[i]);
      break;
    }
  }
}

/* Post-game-tick player update.
 *
 * Called once per tick after game logic has run.  Responsibilities:
 *   1. Tick down the double-speed-movement countdown stored at
 *      players_globals+0x26; when it reaches zero, clear the flag.
 *   2. For each player datum:
 *        a. If the telefrag-pending flag (player+0xd0) is clear, decay the
 *           effect timer (player+0xc8) toward zero.
 *        b. If the flag IS set and timer < 0x5a, trigger a player effect
 *           fade via FUN_a2ed0(datum_handle, (float)timer * CONST_26f2e0).
 *           If timer >= 0x5a and the unit exists and is not already flagged
 *           for deletion (bit 0x20 at unit+0xb6), print the "telefragged"
 *           HUD message, stop the effect (FUN_a2930), and mark the unit for
 *           deletion (FUN_1a7f80 sets bit 0x20 at unit+0xb6).
 *        c. Clear the telefrag-pending flag.
 *        d. Advance the player's short weapon/vehicle timers via FUN_bc4b0
 *
 * (EBX = datum_handle register arg). Binary comparison shows this
 * helper
 * only decrements small player/unit timers and is not where
 *           the
 * actual camera blend math lives.
 *        e. If the unit exists and its
 * object-type flags don't include 0x200000, scan scenario trigger volumes (tag
 * block at scenario+0x39c) for BSP-switch triggers that contain the player's
 * unit, and fire the BSP switch if found. f. Reset the player's pending-action
 * fields and call the per-player post-update helper FUN_bdb00 (EBX =
 * datum_handle).
 *   3. Advance the BSP-transition nibble counter packed into
 *      players_globals+0x2f (high nibble = counter, low nibble = bsp index).
 *   4. Handle the "all players dead" restart flag (players_globals+0x28):
 *      if clear, reset DAT_0046b6a8; if set and game engine is not running,
 *      trigger the SP-restart sequence (FUN_100380) and set the flag byte. */
void players_update_after_game(void)
{
  data_iter_t iter; /* [EBP-0x14] */
  int datum_handle; /* [EBP-0xc]  */
  int timer_val; /* [EBP-0x4]  */
  char *player;
  int16_t bsp_counter;
  int16_t i;
  void *block;
  void *entry;
  int16_t entry_bsp;
  char triggers_player;
  char cur_bsp_nibble;
  int unit_obj;
  int unit_handle;
  int scenario_bsp_count;
  unsigned char packed;

  /* Profile enter. */
  if (*(char *)0x449ef1 != 0 && *(char *)0x2f0e90 != 0)
    profile_enter_private((void *)0x2f0e88);

  /* Tick down the double-speed movement countdown. */
  if (*(int16_t *)((char *)players_globals + 0x26) > 0) {
    *(int16_t *)((char *)players_globals + 0x26) -= 1;
    if (*(int16_t *)((char *)players_globals + 0x26) == 0)
      game_set_players_are_double_speed(0);
  }

  /* Iterate all player datums. */
  data_iterator_new(&iter, player_data);
  player = (char *)data_iterator_next(&iter);
  while (player != NULL) {
    datum_handle = (int)iter.datum_handle;
    timer_val = *(int *)(player + 0xc8);

    if (*(char *)(player + 0xd0) == 0) {
      /* Telefrag-pending flag is clear: decay the effect timer. */
      if (timer_val > 0)
        *(int *)(player + 0xc8) = timer_val - 1;
    } else if (timer_val < 0x5a) {
      /* Flag set, early in window: trigger player effect fade.
       * Disasm: FILD [timer_val]; FMUL float ptr [0x26f2e0]; push as float;
       *         PUSH datum_handle; CALL FUN_a2ed0 */
      ((void (*)(int, float))0xa2ed0)(datum_handle,
                                      (float)timer_val * (*(float *)0x26f2e0));
    } else {
      /* Flag set, timer >= 0x5a: telefrag kill path. */
      unit_handle = *(int *)(player + 0x34);
      if (unit_handle != -1) {
        /* Check unit's delete-pending bit (bit 5 at unit+0xb6). */
        unit_obj = (int)object_get_and_verify_type(unit_handle, 3);
        if ((*(unsigned char *)(unit_obj + 0xb6) & 0x20) == 0) {
          /* Print "telefragged" to player's HUD (wchar_t literal). */
          if (*(int16_t *)(player + 2) != -1)
            hud_print_message(*(int16_t *)(player + 2),
                              L"You were telefragged");
          /* Stop player effect. */
          ((void (*)(int))0xa2930)(datum_handle);
          /* Mark unit for deletion: sets bit 0x20 at unit+0xb6. */
          ((void (*)(int))0x1a7f80)(*(int *)(player + 0x34));
        }
      }
    }

    /* Clear the telefrag-pending flag. */
    *(char *)(player + 0xd0) = 0;

    /* Advance the player's short weapon/vehicle timers.
     * Original CALL to
     * FUN_bc4b0 with EBX = datum_handle (register arg). */
    if (*(int *)(player + 0x34) != -1) {
      player_update_weapon_timers(datum_handle);
    }

    /* BSP-switch trigger volume scan. */
    if (*(int *)(player + 0x34) != -1) {
      /* Walk up to the root object to read its type flags.
       * FUN_13d7f0: follows parent chain, returns root object handle. */
      int root_handle = ((int (*)(int))0x13d7f0)(*(int *)(player + 0x34));
      unit_obj = (int)object_get_and_verify_type(root_handle, -1);
      /* Skip if object has type flag 0x200000 set (object+0x4). */
      if ((*(unsigned int *)(unit_obj + 4) & 0x200000) == 0) {
        /* scenario+0x39c = tag block for structure BSP trigger volumes.
         * Each element is 8 bytes: [0]=int16 handle, [2]=int16 bsp_index,
         * [4]=int16 destination_bsp. */
        scenario_t *scen = global_scenario_get();
        block = (void *)((char *)scen + 0x39c);
        scenario_bsp_count = *(int *)block;
        bsp_counter = 0;
        i = 0;
        while ((int)i < scenario_bsp_count) {
          entry = tag_block_get_element(block, (int)i, 8);
          /* [+2] = bsp index this trigger belongs to; DAT_326a0c = current
           * bsp index. */
          entry_bsp = *(int16_t *)((char *)entry + 2);
          if (entry_bsp == *(int16_t *)0x326a0c) {
            /* FUN_18ef00(trigger_handle, player_unit_handle):
             * returns non-zero if unit is inside the trigger volume. */
            triggers_player = (char)((char (*)(int16_t, int))0x18ef00)(
              *(int16_t *)entry, *(int *)(player + 0x34));
            if (triggers_player) {
              /* Extract the current BSP nibble from players_globals+0x2f.
               * Low nibble = current bsp index (sign-extended to byte). */
              cur_bsp_nibble =
                (char)(*(char *)((char *)players_globals + 0x2f) << 4) >> 4;
              if (cur_bsp_nibble != (char)0xff &&
                  (int16_t)cur_bsp_nibble != *(int16_t *)(player + 2)) {
                error(2, "!!!WARNING!!! teleported player triggering a "
                         "bsp switch!!!");
              }
              /* Pack local_player_index into low nibble. */
              *(unsigned char *)((char *)players_globals + 0x2f) &= 0xf;
              *(unsigned char *)((char *)players_globals + 0x2f) ^=
                (*(unsigned char *)(player + 2) ^
                 *(unsigned char *)((char *)players_globals + 0x2f)) &
                0xf;
              /* Record the trigger index and fire the BSP switch.
               * FUN_100500(int16 bsp_index): entry[4] = destination bsp. */
              *(int16_t *)((char *)players_globals + 0x2a) = bsp_counter;
              ((void (*)(int16_t))0x100500)(*(int16_t *)((char *)entry + 4));
            }
          }
          bsp_counter++;
          i++;
        }
      }
    }

    /* Reset pending-action state and run the per-player post helper.
     * datum_get returns the live datum pointer (may differ from 'player'
     * if the block was reallocated during iteration). */
    {
      char *pdatum = (char *)datum_get(player_data, datum_handle);
      *(int16_t *)(pdatum + 0x28) = 0;
      *(int *)(pdatum + 0x24) = -1;
    }
    player_update_spawn_state(datum_handle);

    player = (char *)data_iterator_next(&iter);
  }

  /* Advance the BSP-transition nibble counter at players_globals+0x2f.
   * High nibble is the per-tick counter (incremented by 0x10), low nibble
   * is the BSP destination index.  When the high nibble exceeds 0xc0
   * (i.e., more than 12 ticks elapsed), clamp it to 0xf0 and then clear
   * the low nibble to 0 (invalidate the pending switch). */
  packed = *(unsigned char *)((char *)players_globals + 0x2f);
  if ((packed & 0xf) != 0xf) {
    unsigned char hi = (unsigned char)(packed & 0xf0) + 0x10;
    unsigned char lo = packed & 0xf;
    packed = hi ^ lo;
    *(unsigned char *)((char *)players_globals + 0x2f) = packed;
    packed = *(unsigned char *)((char *)players_globals + 0x2f);
    if ((packed & 0xf0) > 0xc0) {
      packed |= 0xf;
      *(unsigned char *)((char *)players_globals + 0x2f) = packed;
      *(unsigned char *)((char *)players_globals + 0x2f) &= 0xf;
    }
  }

  /* Handle the "all players dead" restart flag at players_globals+0x28.
   * DAT_46b6a8 tracks whether the SP restart has already been kicked off
   * this death sequence. */
  if (*(char *)((char *)players_globals + 0x28) == 0) {
    /* No restart pending: clear the "already triggered" latch. */
    if (*(char *)0x46b6a8 != 0)
      *(char *)0x46b6a8 = 0;
  } else {
    /* Restart pending and engine is not running: kick off restart once. */
    if (!game_engine_running() && *(char *)0x46b6a8 == 0) {
      ((void (*)(void))0x100380)();
      *(char *)0x46b6a8 = 1;
    }
  }

  /* Profile exit. */
  if (*(char *)0x449ef1 != 0 && *(char *)0x2f0e90 != 0)
    profile_exit_private((void *)0x2f0e88);
}

/* FUN_000bdef0 @ 0x000bdef0
 *
 * HaloScript builtin dispatcher, same shape as the breakable-surfaces /
 * recorded-animation builtins below. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); on a
 * non-NULL evaluation record it reads the record's first byte (a single-byte
 * load, XOR EDX,EDX; MOV DL,[EAX]) and passes it to FUN_000c95c0, which
 * returns (byte == 0) in AL. That byte result is stored into a pre-zeroed
 * dword result slot (MOV dword[EBP-4],0 before the call; MOV byte[EBP-4],AL
 * inside the branch) and forwarded zero-extended to hs_return(thread_datum,
 * result).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX for one local):
 *   function_index  int16_t  [EBP+0x08]
 *   thread_datum    int      [EBP+0x0c]  -> reused for hs_return arg1 (ESI)
 *   init            char     [EBP+0x10]
 *
 * FUN_000c95c0 was modeled void(void) by Ghidra (so the decompile showed a
 * no-arg call and read extraout_AL); the disassembly (000bdf14: XOR EDX,EDX;
 * MOV DL,[EAX]; PUSH EDX; CALL 0xc95c0) shows it takes the record's first
 * byte and returns AL = (byte == 0). Its kb decl is corrected to
 * `unsigned char FUN_000c95c0(unsigned char)`. The single ADD ESP,0xc after
 * the hs_return CALL folds FUN_000c95c0's 1 arg and hs_return's 2 args
 * (adjacent-call cleanup). */
void FUN_000bdef0(int16_t function_index, int thread_datum, char init)
{
  volatile unsigned int result_slot;
  unsigned char *record;
  unsigned int result;

  result_slot = 0;
  record = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (record != NULL) {
    result_slot = (unsigned char)FUN_000c95c0(record[0]);
    result = (unsigned int)result_slot;
    hs_return(thread_datum, result);
  }
}

/* 0xbdf40 — HS script function handler: evaluate a macro function and, on a
 * non-null result record, forward the record's first dword to FUN_000c95d0,
 * then commit a 0 result to the calling HS thread. Unlike the 0xc135x float
 * trampolines, no value is read back from the callee — hs_return always
 * commits 0. Same evaluator ABI (function_index, thread_datum, init) as the
 * other hs_evaluate_* handlers.
 *
 * ABI (verified against disassembly 0xbdf40): cdecl, plain RET. thread_datum
 * (arg 2, cached in ESI) flows to both the evaluate call (arg 2) and the
 * hs_return call (arg 1). Call site does MOV EDX,[EAX]; PUSH EDX; CALL
 * 0xc95d0 — passing *result (the record's first dword). The combined
 * ADD ESP,0xc after the two trailing calls confirms 0xc95d0 takes exactly
 * one stack arg (Ghidra's void(void) decl dropped it).
 *
 * NOTE: kb groups 0xbdf40 under players.obj, but it is a HaloScript
 * macro-function handler byte-identical in shape to the hs.obj handlers below
 * and calls hs_macro_function_evaluate/hs_return. Placed in hs.c per lift
 * directive (players.c does not compile under VC71 — clang-only __attribute__
 * / raw fnptr casts — so it would be permanently unmeasurable there).
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0xc95d0 = FUN_000c95d0(int) -> void (record first-dword consumer)
 *   0xcbf80 = hs_return(int thread_handle, int value) */
void FUN_000bdf40(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000c95d0(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bdf80 @ 0x000bdf80
 *
 * HaloScript builtin implementation. Calls FUN_000c95f0() (a no-arg helper
 * that returns a value in EAX) and completes the calling script thread with
 * hs_return(thread_handle, <result>).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  (unused -- never loaded)
 *   thread_handle   int      [EBP+0x0c]  -> hs_return arg1
 *
 * FUN_000c95f0() takes no args; its EAX return is pushed directly as
 * hs_return's value (CALL c95f0; PUSH EAX). The second stack param is then
 * loaded (MOV EAX,[EBP+0xc]) and pushed as hs_return's thread_handle
 * (PUSH EAX; CALL hs_return; ADD ESP,8 cleans the two cdecl args). Ghidra
 * modeled both this function and FUN_000c95f0 as void(void); the EAX return
 * consumed here and the [EBP+0xc] read of the second cdecl param are
 * unmodeled there. */
void FUN_000bdf80(int16_t function_index, int thread_handle)
{
  hs_return(thread_handle, FUN_000c95f0());
}

/* 0xbdfa0 — HS script function handler: evaluate a macro function and, on a
 * non-null result record, forward two 16-bit fields to FUN_000ca430, then
 * commit a 0 result to the calling HS thread. Same evaluator ABI
 * (function_index, thread_datum, init) as the other hs_evaluate_* handlers.
 *
 * ABI (verified against disassembly 0xbdfa0): cdecl, plain RET. thread_datum
 * (arg 2, cached in ESI) flows to both the evaluate call (arg 2) and the
 * hs_return call (arg 1). The call site reads MOVSX EAX,word[result+0]
 * (signed int16 -> int) and MOVZX EDX,word[result+4] (unsigned int16 -> int),
 * then PUSH EDX; PUSH EAX; CALL 0xca430 — two cdecl int args (Ghidra's
 * void(void) decl dropped both). The combined ADD ESP,0x10 after the two
 * trailing calls (ca430's 2 + hs_return's 2) confirms the arg counts. Note
 * result is int*, so the +0x4 read is at (char *)result + 4, a narrow int16.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0xca430 = FUN_000ca430(int, int) -> void (two-field consumer)
 *   0xcbf80 = hs_return(int thread_handle, int value) */
void FUN_000bdfa0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000ca430(*(short *)result, *(unsigned short *)((char *)result + 4));
    hs_return(thread_datum, 0);
  }
}

/* 0xbdfe0 — HS script function handler: evaluate a macro function and, on a
 * non-null result record, forward a cluster index + object handle to
 * FUN_0018ef00, then commit that call's boolean result to the calling HS
 * thread. Same evaluator ABI (function_index, thread_datum, init) as the other
 * hs_evaluate_* handlers.
 *
 * ABI (verified against disassembly 0xbdfe0): cdecl, plain RET. thread_datum
 * (arg 2, cached in ESI) flows to both the evaluate call (arg 2) and the
 * hs_return call (arg 1). On a non-null result the call site reads
 * MOVSX EAX,word[result+0] (SIGNED int16 -> int cluster index) and
 * MOV EDX,dword[result+4] (full int object handle), then PUSH EDX; PUSH EAX;
 * CALL 0x18ef00 -> char in AL. AL is MOVZX-widened into local_8 and becomes
 * hs_return's value arg. The combined ADD ESP,0x10 after the two trailing calls
 * (18ef00's 2 + hs_return's 2) confirms the arg counts. Note result[+0] is a
 * SIGNED int16 (MOVSX), so (int)*result on a short* must stay signed;
 * result[+4] is a full int (dword), unlike the narrow int16 +4 read in
 * FUN_000bdfa0.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0x18ef00 = FUN_0018ef00(int cluster_index, int object_handle) -> char
 *   0xcbf80 = hs_return(int thread_handle, int value) */
void FUN_000bdfe0(int16_t function_index, int thread_datum, char init)
{
  short *result;
  unsigned char eval_result;

  result =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    eval_result =
      (unsigned char)FUN_0018ef00((int)*result, *(int *)(result + 2));
    hs_return(thread_datum, (int)eval_result);
  }
}

/* 0xbe030 — HS script function handler: evaluate a macro function and commit a
 * byte predicate result to the calling HS thread. Evaluates the macro
 * arguments via hs_macro_function_evaluate; on a non-null result record, reads
 * a signed int16 at +0x0 (MOVSX word ptr) and an int at +0x4, passes both to
 * FUN_000ca0f0 (returns a byte in AL), zero-extends that byte and returns it to
 * the thread via hs_return. The dword result slot is pre-zeroed and only the
 * low byte is written (zero-init-then-narrow-store idiom) — modeled with a
 * union so the widened value is the zero-extended byte.
 *
 * ABI (verified against disassembly 0xbe030): cdecl, plain RET. thread_datum
 * (arg 2, cached in ESI) flows to both the evaluate call (arg 2) and the
 * hs_return call (arg 1). Result record: int16 @ +0x0 (signed load), int @
 * +0x4. Callees: 0xcc560 = hs_macro_function_evaluate(int16 fn_index, int
 * thread_datum, char init) -> short* (result record, NULL on failure) 0xca0f0 =
 * FUN_000ca0f0(int16_t word0, int dword4) -> unsigned char 0xcbf80 =
 * hs_return(int thread_handle, int value) */
void FUN_000be030(int16_t function_index, int thread_datum, char init)
{
  short *result;
  union {
    int i;
    unsigned char b;
  } value;

  result =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value.i = 0;
    value.b = FUN_000ca0f0(*result, *(int *)(result + 2));
    hs_return(thread_datum, value.i);
  }
}

/* 0xbe080 — HaloScript macro-function call wrapper. Evaluates a macro
 * function expression on a thread; if the evaluation yields a result node,
 * runs it through FUN_000ca050 (a value/cast evaluator returning a byte in
 * AL) and returns that byte on the calling thread via hs_return.
 *
 * players.obj groups this function, but it calls hs_runtime.obj's static
 * hs_macro_function_evaluate / hs_return, so it is co-located here (in the
 * original binary those callees have external linkage; the lift marks them
 * static, so a cross-TU call from players.c would not link). maintain.py
 * relocates this to players.c — that move must be reverted.
 *
 * Plain cdecl (caller cleans, RET no immediate). Three stack params:
 *   param1 @ EBP+0x8  = function_index (int16_t)
 *   param2 @ EBP+0xc  = thread_datum
 *   param3 @ EBP+0x10 = init (char)
 *
 * Callees (all in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *             -> result node ptr (Ghidra's `int` is really a struct*; NULL
 *                when there is nothing to return)
 *   0xca050 = FUN_000ca050(int16 result[+0], int result[+0x4]) -> byte in AL
 *   0xcbf80 = hs_return(thread_datum, value)
 *
 * Result node layout (EAX from call 1, only read when nonzero):
 *   +0x0 (int16_t) : MOVSX'd and passed as FUN_000ca050 arg1
 *   +0x4 (int32_t) : passed as FUN_000ca050 arg2
 * The returned byte is written into a pre-zeroed dword local (only AL stored),
 * so it is zero-extended (uint8 -> int) before being handed to hs_return.
 */
void FUN_000be080(int16_t function_index, int thread_datum, char init)
{
  int *result;
  int value;

  value = 0;
  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    *(unsigned char *)&value = FUN_000ca050(*(int16_t *)result, result[1]);
    hs_return(thread_datum, value);
  }
}

/* 0xbe1d0 — HaloScript macro-function evaluate-then-finalize wrapper.
 * Evaluates a macro-function expression on a thread; when the evaluation
 * yields a result node (non-NULL record ptr in EAX), it runs a fixed
 * side-effecting step FUN_000ca140() (no args) and then commits a literal
 * 0 back to the calling thread via hs_return(thread_datum, 0). Unlike the
 * value-returning neighbors this does not read any field of the record and
 * always returns 0 — the record is used only as an "evaluation complete"
 * predicate.
 *
 * players.obj groups this, but like its siblings it calls hs_runtime.obj's
 * static hs_macro_function_evaluate / hs_return, so it is co-located here.
 *
 * Plain cdecl (caller cleans, RET no immediate). Three stack params:
 *   param1 @ EBP+0x8  = function_index (int16_t)
 *   param2 @ EBP+0xc  = thread_datum
 *   param3 @ EBP+0x10 = init (char)
 *
 * Callees (all in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *             -> result node ptr in EAX (NULL while evaluation pending)
 *   0xca140 = FUN_000ca140() (void, no args)
 *   0xcbf80 = hs_return(thread_datum, 0)
 */
void FUN_000be1d0(int16_t function_index, int thread_datum, char init)
{
  void *record;

  record =
    (void *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != 0) {
    FUN_000ca140();
    hs_return(thread_datum, 0);
  }
}

/* 0xbe210 — HS built-in evaluator, sibling of FUN_000be1d0. Evaluates a
 * single macro-function via hs_macro_function_evaluate; while that returns
 * NULL the evaluation is still pending and nothing is committed this call.
 * Once it yields a non-NULL result datum, FUN_000c9bb0() runs (side-effect
 * cleanup, void/void) and the thread is committed with hs_return(thread, 0).
 * Standard evaluator ABI (function_index, thread_datum, init), plain cdecl.
 *
 * Callees (all in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *             -> result node ptr in EAX (NULL while evaluation pending)
 *   0xc9bb0 = FUN_000c9bb0() (void, no args)
 *   0xcbf80 = hs_return(thread_datum, 0)
 */
void FUN_000be210(int16_t function_index, int thread_datum, char init)
{
  void *record;

  record =
    (void *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != 0) {
    FUN_000c9bb0();
    hs_return(thread_datum, 0);
  }
}

/* 0xbe270 — HS built-in evaluator, sibling of FUN_000be1d0 / FUN_000be210.
 * Evaluates a single macro-function via hs_macro_function_evaluate; while
 * that returns NULL the evaluation is still pending and nothing is committed
 * this call. Once it yields a non-NULL result datum, its first dword and its
 * zero-extended 16-bit field at +0x4 are handed to FUN_000ca3f0, then the
 * thread is committed with hs_return(thread_datum, 0). Standard evaluator ABI
 * (function_index, thread_datum, init), plain cdecl (caller cleans).
 *
 * Disasm evidence (0xbe28c..0xbe29e): after TEST EAX,EAX / JZ, the non-NULL
 * path does `XOR EDX,EDX; MOV DX,[EAX+0x4]` (u16 zero-extend) and
 * `MOV EAX,[EAX]` (dword), then PUSH EDX; PUSH EAX; CALL 0xca3f0 — i.e.
 * FUN_000ca3f0(record[0], (u16)record->field_0x4). The single trailing
 * ADD ESP,0x10 folds the cleanup of BOTH this 2-arg call and the following
 * 2-arg hs_return(thread_datum, 0). (The prefetch decomp modeled ca3f0 as
 * void/void and dropped both args — corrected here from the binary.)
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *             -> result node ptr in EAX (NULL while evaluation pending)
 *   0xca3f0 = FUN_000ca3f0(int, int) — 2-arg cdecl (reads [EBP+8],[EBP+c])
 *   0xcbf80 = hs_return(thread_datum, 0)
 */
void FUN_000be270(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != 0) {
    FUN_000ca3f0(record[0], *(unsigned short *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* 0xbe2b0 — HS built-in evaluator, sibling of FUN_000be270 above. Evaluates a
 * single macro-function via hs_macro_function_evaluate; while that returns
 * NULL the evaluation is still pending and nothing is committed this call.
 * Once it yields a non-NULL result datum, its first dword and its zero-extended
 * 16-bit field at +0x4 are handed to FUN_000ca410, then the thread is committed
 * with hs_return(thread_datum, 0). Standard evaluator ABI (function_index,
 * thread_datum, init), plain cdecl (caller cleans).
 *
 * Disasm evidence: after TEST EAX,EAX / JZ, the non-NULL path does
 * `XOR EDX,EDX; MOV DX,[EAX+0x4]` (u16 zero-extend) and `MOV EAX,[EAX]`
 * (dword), then PUSH EDX; PUSH EAX; CALL 0xca410 — i.e.
 * FUN_000ca410(record[0], (u16)record->field_0x4). The single trailing
 * ADD ESP,0x10 folds the cleanup of BOTH this 2-arg call and the following
 * 2-arg hs_return(thread_datum, 0). (The prefetch decomp modeled ca410 as
 * void/void and dropped both args — corrected here from the binary.)
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *             -> result node ptr in EAX (NULL while evaluation pending)
 *   0xca410 = FUN_000ca410(int, int) — 2-arg cdecl (reads [EBP+8],[EBP+c])
 *   0xcbf80 = hs_return(thread_datum, 0)
 */
void FUN_000be2b0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != 0) {
    FUN_000ca410(record[0], *(unsigned short *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* 0xbe2f0 — HS built-in evaluator, sibling of FUN_000be270 / FUN_000be2b0
 * above. Evaluates a single macro-function via hs_macro_function_evaluate;
 * while that returns NULL the evaluation is still pending and nothing is
 * committed this call. Once it yields a non-NULL result datum, its first dword
 * and the float at +0x4 are handed to FUN_000c9c10, then the thread is
 * committed with hs_return(thread_datum, 0). Standard evaluator ABI
 * (function_index, thread_datum, init), plain cdecl (caller cleans).
 *
 * Disasm evidence: after TEST EAX,EAX / JZ, the non-NULL path does
 * `FLD  float ptr [EAX+0x4]` (float payload at result+4) and
 * `MOV  EDX,[EAX]` (dword at result+0), then the float is pushed via the MSVC
 * PUSH-then-FSTP idiom (`PUSH ECX; FSTP float ptr [ESP]` = second/higher slot)
 * and `PUSH EDX` supplies the first arg — i.e.
 * FUN_000c9c10(record[0], *(float*)(record+4)). The single trailing
 * ADD ESP,0x10 folds the cleanup of BOTH this 2-arg call and the following
 * 2-arg hs_return(thread_datum, 0). (The prefetch decomp modeled c9c10 as
 * void/void and dropped both args — corrected here from the binary; the float
 * arg would otherwise be silently lost to the push-then-fstp trap.)
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *             -> result node ptr in EAX (NULL while evaluation pending)
 *   0xc9c10 = FUN_000c9c10(int, float) — 2-arg cdecl (dword@+0, float@+4)
 *   0xcbf80 = hs_return(thread_datum, 0)
 */
void FUN_000be2f0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != 0) {
    FUN_000c9c10(record[0], *(float *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* 0xbe330 — HS script function handler: evaluate a macro function and, on a
 * non-null result record, forward the record's first three dwords (at +0x0,
 * +0x4, +0x8) to FUN_000c9c80, then commit a 0 result to the calling HS
 * thread. No value is read back from the callee — hs_return always commits 0.
 * Same evaluator ABI (function_index, thread_datum, init) as the other
 * hs_evaluate_* handlers.
 *
 * ABI (verified against disassembly 0xbe330): cdecl, plain RET. thread_datum
 * (arg 2, cached in ESI) flows to both the evaluate call (arg 2) and the
 * hs_return call (arg 1). On a non-null result the call site does
 * MOV EDX,[result+0x8]; MOV ECX,[result+0x4]; MOV EDX,[result+0x0], then
 * PUSH EDX(+8); PUSH ECX(+4); PUSH EDX(+0); CALL 0xc9c80 — three cdecl int
 * args. Ghidra's void(void) decl for 0xc9c80 dropped all three, misled by the
 * combined ADD ESP,0x14 after the two trailing calls (0xc9c80's 3 args = 0xc
 * plus hs_return's 2 args = 0x8). kb.json decl for 0xc9c80 corrected to
 * void(int,int,int) accordingly.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0xc9c80 = FUN_000c9c80(int, int, int) -> void (record 3-dword consumer)
 *   0xcbf80 = hs_return(int thread_handle, int value) */
void FUN_000be330(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000c9c80(result[0], result[1], result[2]);
    hs_return(thread_datum, 0);
  }
}

/* 0xbe370 — HS script function handler: evaluate a macro function and, on a
 * non-null result record, forward the record's first dword (+0x0) and a
 * narrow unsigned int16 (+0x4) to FUN_000c9bd0, then commit that callee's
 * return value to the calling HS thread. Unlike the handlers that always
 * commit 0, this one reads FUN_000c9bd0's EAX result and passes it to
 * hs_return. Same evaluator ABI (function_index, thread_datum, init) as the
 * other hs_evaluate_* handlers.
 *
 * ABI (verified against disassembly 0xbe370): cdecl, plain RET. thread_datum
 * (arg 2, cached in ESI) flows to both the evaluate call (arg 2) and the
 * hs_return call (arg 1). On a non-null result the call site does
 * MOVZX EDX,word[result+0x4] (UNSIGNED int16 -> int) and MOV EAX,dword[result]
 * (full int), then PUSH EDX; PUSH EAX; CALL 0xc9bd0 -> int in EAX. That EAX is
 * the value arg to hs_return. The combined ADD ESP,0x10 after the two trailing
 * calls (0xc9bd0's 2 + hs_return's 2) confirms the arg counts. Ghidra's
 * void(void) decl for 0xc9bd0 dropped both args; kb.json decl corrected to
 * int(int,int) accordingly. result is int*, so the +0x4 read is at
 * (char *)result + 4, a narrow unsigned int16 (MOVZX).
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0xc9bd0 = FUN_000c9bd0(int value, int type) -> int (coerced value)
 *   0xcbf80 = hs_return(int thread_handle, int value) */
void FUN_000be370(int16_t function_index, int thread_datum, char init)
{
  int *result;
  int value;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value = FUN_000c9bd0(result[0], *(unsigned short *)((char *)result + 4));
    hs_return(thread_datum, value);
  }
}

/* 0xbe3b0 — HS built-in evaluator. Evaluates a single macro-function
 * argument via hs_macro_function_evaluate; while that returns NULL the
 * evaluation is still pending and nothing is committed this call. Once it
 * yields a value datum, its first dword is converted through FUN_000ce420
 * (returns a 16-bit value in AX, zero-extended by the original into the
 * result slot) and committed with hs_return. Standard evaluator ABI
 * (function_index, thread_datum, init). */
void FUN_000be3b0(int16_t function_index, int thread_datum, char init)
{
  int *result;
  unsigned int value;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value = (uint16_t)FUN_000ce420(*result);
    hs_return(thread_datum, (int)value);
  }
}

/* player_rumble_initialize @ 0x000be400
 *
 * HaloScript function-evaluator wrapper. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_handle, init); on a
 * non-NULL evaluation record it forwards the two record fields to FUN_000c9de0
 * and completes the thread with hs_return(thread_handle, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI):
 *   function_index  int16_t  [EBP+0x08]
 *   thread_handle   int      [EBP+0x0c]  (held in ESI)
 *   init            char     [EBP+0x10]
 *
 * hs_macro_function_evaluate returns an evaluation-record pointer in EAX.
 * When non-NULL the original loads EAX+0x00 as a full dword and EAX+0x04 as a
 * MOVZX (zero-extended) 16-bit field, then pushes them right-to-left
 * (PUSH EDX=+0x04; PUSH EAX_val=+0x00). FUN_000c9de0's 2-arg cdecl signature is
 * recovered from this call site (its kb decl was previously void(void)). */
void player_rumble_initialize(int16_t function_index, int thread_handle,
                              char init)
{
  int record;

  record = hs_macro_function_evaluate(function_index, thread_handle, init);
  if (record != 0) {
    FUN_000c9de0(*(int *)record, *(uint16_t *)(record + 4));
    hs_return(thread_handle, 0);
  }
}

/* FUN_000be440 @ 0x000be440
 *
 * HaloScript function-evaluator wrapper, sibling of player_rumble_initialize
 * above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); while that
 * returns NULL the evaluation is still pending and nothing is committed. Once
 * it yields a non-NULL evaluation record, the record's first three dwords
 * (offsets +0x00, +0x04, +0x08) are forwarded to FUN_000c9e50, then the thread
 * is committed with hs_return(thread_datum, 0). Standard evaluator ABI
 * (function_index, thread_datum, init), plain cdecl (caller cleans).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  (held in ESI) -> arg2; reused for
 *                                          hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX. On the non-NULL
 * branch (TEST EAX,EAX / JZ) the original loads three dwords and pushes them
 * right-to-left (MOV EDX,[EAX+8]; MOV ECX,[EAX+4]; MOV EDX,[EAX];
 * PUSH [EAX+8]; PUSH [EAX+4]; PUSH [EAX]) -> FUN_000c9e50(result[0],
 * result[1], result[2]). The single trailing ADD ESP,0x14 folds the cleanup of
 * BOTH this 3-arg call (0xc) and the following 2-arg hs_return(thread_datum, 0)
 * (0x8). Ghidra modeled hs_macro_function_evaluate's return as a plain int and
 * FUN_000c9e50 as void(void), dropping all three args; both are corrected here
 * from the binary (return is a >=12-byte record pointer; FUN_000c9e50 is
 * 3-arg cdecl). */
void FUN_000be440(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000c9e50(result[0], result[1], result[2]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000be480 @ 0x000be480
 *
 * HaloScript function-evaluator wrapper, sibling of FUN_000be440 above.
 * Evaluates the script function via hs_macro_function_evaluate(function_index,
 * thread_datum, init); while that returns NULL the evaluation is still pending
 * and nothing is committed. Once it yields a non-NULL evaluation record, two
 * fields of the record are forwarded to FUN_000c9ec0 and the thread is then
 * committed with hs_return(thread_datum, 0). Standard evaluator ABI
 * (function_index, thread_datum, init), plain cdecl (caller cleans).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  (held in ESI) -> arg2; reused for
 *                                          hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX. On the non-NULL
 * branch (TEST EAX,EAX / JZ) the original reads a zero-extended 16-bit field
 * and the leading dword and pushes them right-to-left:
 *   XOR EDX,EDX; MOV DX,[EAX+4]; MOV EAX,[EAX]; PUSH EDX; PUSH EAX
 *   -> FUN_000c9ec0(record[0], (uint16_t)record[+0x4]).
 * The single trailing ADD ESP,0x10 folds the cleanup of BOTH this 2-arg call
 * (0x8) and the following 2-arg hs_return(thread_datum, 0) (0x8). Ghidra
 * modeled hs_macro_function_evaluate's return as a plain int and FUN_000c9ec0
 * as void(void), dropping both args; both are corrected here from the binary
 * (return is a record pointer; FUN_000c9ec0 is 2-arg cdecl with a dword first
 * arg and a zero-extended 16-bit second arg). The +0x4 field is a 16-bit word
 * (MOVW / zero-extend), so it is read as uint16_t, not a full dword. */
void FUN_000be480(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000c9ec0(result[0], *(uint16_t *)((char *)result + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000be4c0 @ 0x000be4c0
 *
 * HaloScript macro-function evaluator wrapper (side-effect-only variant),
 * sibling of the FUN_000be440/FUN_000be480 evaluators above. Evaluates the
 * script function via hs_macro_function_evaluate(function_index, thread_datum,
 * init); while that returns NULL the evaluation is still pending and nothing is
 * committed. Unlike its siblings the returned record is NOT dereferenced -- on
 * a non-NULL result the wrapper only invokes the parameterless side-effect
 * routine FUN_000c9f30() and then commits the calling thread with
 * hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX; only its
 * nonzero-ness is tested (TEST EAX,EAX / JZ). hs_return's two args are pushed
 * right-to-left (PUSH 0 = value; PUSH thread_datum = thread_handle). Ghidra
 * modeled this function as void(void) and dropped all three stack args; the
 * 3-arg cdecl signature is recovered from the hs_macro_function_evaluate call
 * site (its kb decl was previously the stub void FUN_000be4c0(void)). */
void FUN_000be4c0(int16_t function_index, int thread_datum, char init)
{
  int record;

  record = hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != 0) {
    FUN_000c9f30();
    hs_return(thread_datum, 0);
  }
}

/* 0xbe500 — HS script function handler: evaluate a macro function and, on a
 * non-null result record, forward the record's first two dwords (+0x0, +0x4)
 * and a FLOAT field (+0x8) to FUN_000c9770, then commit that callee's byte
 * return to the calling HS thread. Same evaluator ABI (function_index,
 * thread_datum, init) as the other hs_evaluate_* handlers.
 *
 * ABI (verified against delinked disassembly 0xbe500): cdecl, plain RET.
 * thread_datum (arg 2, cached in ESI) flows to both the evaluate call (arg 2)
 * and the hs_return call (arg 1). On a non-null result the call site loads the
 * three fields and passes them to FUN_000c9770:
 *   FLDS [result+0x8]; PUSH <dummy>; FSTP [ESP]   (float arg3, push-then-fstp)
 *   PUSH [result+0x4] (int arg2); PUSH [result+0x0] (int arg1); CALL 0xc9770
 * then MOV [EBP-4],AL; PUSH ECX(=zero-extended AL); PUSH ESI(=thread_datum);
 * CALL hs_return. The combined ADD ESP,0x14 after the two trailing calls =
 * FUN_000c9770's 3 args (0xc) + hs_return's 2 args (0x8). Ghidra's void(void)
 * decl for 0xc9770 dropped all three args and its AL return, misled by that
 * combined cleanup; kb.json decl for 0xc9770 corrected to
 * unsigned char(int,int,float). The +0x8 field is a FLOAT read via FLDS and
 * passed as a float argument (hazard #2 push-then-fstp), NOT the pushed dummy.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0xc9770 = FUN_000c9770(int, int, float) -> unsigned char (record consumer)
 *   0xcbf80 = hs_return(int thread_handle, int value) */
void FUN_000be500(int16_t function_index, int thread_datum, char init)
{
  int *result;
  unsigned char value;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value = FUN_000c9770(result[0], result[1], *(float *)((char *)result + 8));
    hs_return(thread_datum, value);
  }
}

/* 0xbe5a0 — HS script function handler: evaluate a macro function and, on a
 * non-null result record, forward the record's first dword (+0x0, int) to
 * FUN_000c9d80, then return void to the calling HS thread via
 * hs_return(thread_datum, 0). Same evaluator ABI (function_index, thread_datum,
 * init) as the other hs_evaluate_* handlers.
 *
 * ABI (verified against delinked disassembly 0xbe5a0): cdecl, plain RET.
 * thread_datum (arg 2, cached in ESI) flows to both the evaluate call (arg 2)
 * and the hs_return call (arg 1). On a non-null result (EAX) the call site
 * dereferences the record and passes its first dword to the single-arg callee:
 *   MOV EDX,[EAX] (result[0]); PUSH EDX; CALL 0xc9d80
 * then PUSH 0; PUSH ESI(=thread_datum); CALL hs_return. The combined
 * ADD ESP,0xc after the two trailing calls = FUN_000c9d80's 1 arg (0x4) +
 * hs_return's 2 args (0x8). Ghidra's void(void) decl for 0xc9d80 dropped its
 * single stack arg, misled by that combined cleanup; kb.json decl for 0xc9d80
 * corrected to void(int).
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0xc9d80 = FUN_000c9d80(int) -> void (record consumer)
 *   0xcbf80 = hs_return(int thread_handle, int value) */
void FUN_000be5a0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000c9d80(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000be620 @ 0x000be620
 *
 * HaloScript function-evaluator wrapper (real-valued variant). Evaluates the
 * script function via hs_macro_function_evaluate(function_index, thread_handle,
 * init); on a non-NULL evaluation record it dereferences the record's first
 * dword and passes it to FUN_000ca010, which returns a float in ST0. The float
 * is stored to a 4-byte stack cell and reloaded as a raw int32, then forwarded
 * to hs_return.
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX):
 *   function_index  int16_t  [EBP+0x08]
 *   thread_handle   int      [EBP+0x0c]  (held in ESI)
 *   init            char     [EBP+0x10]
 *
 * The FSTP [EBP-4] / MOV EAX,[EBP-4] / PUSH EAX pattern is a raw 4-byte
 * reinterpret of the float bits into the hs value cell -- NOT an (int) cast,
 * which would truncate. FUN_000ca010's 1-arg cdecl float-returning signature is
 * recovered from this call site (MOV EDX,[EAX]; PUSH EDX; CALL; FSTP [EBP-4]);
 * its kb decl was previously void(void). */
void FUN_000be620(int16_t function_index, int thread_handle, char init)
{
  int record;
  union {
    float f;
    int i;
  } cell;

  record = hs_macro_function_evaluate(function_index, thread_handle, init);
  if (record != 0) {
    cell.f = FUN_000ca010(*(int *)record);
    hs_return(thread_handle, cell.i);
  }
}

/* FUN_000be6a0 @ 0x000be6a0
 *
 * HaloScript function-evaluator wrapper (short-valued variant). Evaluates the
 * script function via hs_macro_function_evaluate(function_index, thread_datum,
 * init); on a non-NULL evaluation record it reads the record's first uint16
 * field, passes it (zero-extended) to numeric_countdown_timer_get, masks the
 * result to 16 bits, and forwards it to hs_return.
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX):
 *   function_index  int16_t  [EBP+0x08]
 *   thread_datum    int      [EBP+0x0c]
 *   init            char     [EBP+0x10]
 *
 * thread_datum is reused as the first arg to hs_return. The record's first
 * field is dereferenced as uint16 (*(ushort *)record) then zero-extended to
 * uint before the countdown-timer lookup; the getter's return is masked
 * &0xffff before hs_return. kb decl was previously void(void). */
void FUN_000be6a0(int16_t function_index, int thread_datum, char init)
{
  unsigned short *record;
  int value;

  record = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (record != 0) {
    value = numeric_countdown_timer_get((unsigned int)*record);
    value = value & 0xffff;
    hs_return(thread_datum, value);
  }
}

/* FUN_000be6f0 @ 0x000be6f0
 *
 * HaloScript builtin implementation. Unlike the surrounding function-evaluator
 * wrappers this does not call hs_macro_function_evaluate: it stops the numeric
 * countdown timer directly, then completes the calling script thread with
 * hs_return(thread_handle, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  (unused -- never loaded)
 *   thread_handle   int      [EBP+0x0c]  -> hs_return arg1
 *
 * numeric_countdown_timer_stop() takes no args. The second stack param is
 * loaded (MOV EAX,[EBP+0xc]) and pushed as hs_return's thread_handle; the
 * constant 0 is pushed as hs_return's value (PUSH 0; PUSH EAX; CALL; ADD
 * ESP,8). Ghidra modeled this void(void); the [EBP+0xc] read of the second
 * cdecl param is unmodeled there (kb decl was previously void(void)). */
void FUN_000be6f0(int16_t function_index, int thread_handle)
{
  numeric_countdown_timer_stop();
  hs_return(thread_handle, 0);
}

/* FUN_000be710 @ 0x000be710
 *
 * HaloScript builtin implementation (restart variant of FUN_000be6f0). Like its
 * neighbor it does not call hs_macro_function_evaluate: it restarts the numeric
 * countdown timer directly, then completes the calling script thread with
 * hs_return(thread_handle, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  (unused -- never loaded)
 *   thread_handle   int      [EBP+0x0c]  -> hs_return arg1
 *
 * numeric_countdown_timer_restart() takes no args. The second stack param is
 * loaded (MOV EAX,[EBP+0xc]) and pushed as hs_return's thread_handle; the
 * constant 0 is pushed as hs_return's value (PUSH 0; PUSH EAX; CALL; ADD
 * ESP,8). Ghidra modeled this void(void) and read only in_stack_00000008 (the
 * second cdecl param); kb decl was previously void(void). */
void FUN_000be710(int16_t function_index, int thread_handle)
{
  numeric_countdown_timer_restart();
  hs_return(thread_handle, 0);
}

/* FUN_000be730 @ 0x000be730
 *
 * HaloScript builtin implementation (breakable-surfaces toggle). Evaluates the
 * script function via hs_macro_function_evaluate(function_index, thread_datum,
 * init); on a non-NULL evaluation record it reads the record's first byte (the
 * boolean "active" flag) and forwards it to breakable_surfaces_enable(char),
 * then completes the calling script thread with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]
 *   thread_datum    int      [EBP+0x0c]  -> hs_return arg1
 *   init            char     [EBP+0x10]
 *
 * hs_macro_function_evaluate returns an evaluation-record pointer in EAX. When
 * non-NULL the original dereferences the record's first byte (a single-byte
 * load, NOT a wider read) and passes it to breakable_surfaces_enable; then
 * pushes 0 and thread_datum for hs_return (PUSH 0; PUSH thread_datum; CALL; ADD
 * ESP,8). Ghidra modeled this void(void); the three cdecl stack params
 * (in_stack_00000004/08/0c) are unmodeled there (kb decl was previously
 * void(void)). */
void FUN_000be730(int16_t function_index, int thread_datum, char init)
{
  char *record;

  record =
    (char *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    breakable_surfaces_enable(*record);
    hs_return(thread_datum, 0);
  }
}

/* player_rumble_set_effect @ 0x000be770
 *
 * Misnomer: this is NOT controller rumble. It is a HaloScript builtin
 * dispatcher (recorded-animation playback) with the same shape as the
 * breakable-surfaces builtin above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); on a
 * non-NULL evaluation record it plays a recorded animation and completes the
 * calling script thread with hs_return(thread_datum, <result>).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX for one local):
 *   function_index  int16_t  [EBP+0x08]
 *   thread_datum    int      [EBP+0x0c]  -> reused for hs_return arg1
 *   init            char     [EBP+0x10]
 *
 * hs_macro_function_evaluate returns an evaluation-record pointer in EAX
 * (piVar2). The local [EBP-4] result slot is pre-zeroed (MOV dword[EBP-4],0)
 * before the call. When the record is non-NULL the original reads:
 *   record[0]  int    (actor handle, offset 0x00, full dword load)
 *   record[1]  int16  (anim index,  offset 0x04, zero-extended word load:
 *                       XOR EDX,EDX; MOV DX,[EAX+4])
 * and calls recorded_animation_play(record[0], (short)record[1]) (PUSH EDX;
 * PUSH EAX -> arg1=record[0], arg2=word@0x04). The char return in AL is stored
 * into the pre-zeroed dword slot, so the full value forwarded is the
 * zero-extended byte, and hs_return(thread_datum, (uint)result) completes the
 * thread (PUSH result; PUSH thread_datum; CALL; combined ADD ESP cleanup). */
void player_rumble_set_effect(int16_t function_index, int thread_datum,
                              char init)
{
  volatile unsigned short result_slot;
  int *record;
  unsigned int result;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    /* The original pre-zeroes the result dword and then stores only the byte
     * return (AL) into it. Routing the zero-extended char return through a
     * volatile stack slot reproduces that store-then-reload codegen shape. */
    result_slot = (unsigned char)recorded_animation_play(
      record[0], (short)((unsigned short *)record)[2]);
    result = (unsigned int)result_slot;
    hs_return(thread_datum, result);
  }
}

/* 0xbe7c0 — HS script function handler (recorded-animation play/delete
 * dispatcher), structurally identical to FUN_000be810. Evaluates the macro
 * arguments via hs_macro_function_evaluate(function_index, thread_datum,
 * init); on a non-NULL evaluation record it reads two record fields, calls the
 * byte-returning worker recorded_animation_play_and_delete, and completes the
 * calling HS thread with hs_return(thread_datum, <byte>).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX local; PUSH ESI):
 *   function_index  int16_t  [EBP+0x08]
 *   thread_datum    int      [EBP+0x0c]  -> held in ESI, reused for hs_return
 *   init            char     [EBP+0x10]
 *
 * The disassembly (NOT the supplied Ghidra pseudocode, which wrongly modeled
 * this void(void), called the worker as void(void) and read its return from a
 * bare extraout_AL — the classic void-EAX/dropped-arg trap) shows: the
 * [EBP-4] result slot is pre-zeroed before the evaluate call.
 * hs_macro_function_evaluate returns the record pointer in EAX; when non-NULL:
 *   record[0]  int    (offset 0x00, MOV EAX,[EAX])
 *   record.w4  int16  (offset 0x04, zero-extended: XOR EDX,EDX; MOV DX,[EAX+4])
 * and calls recorded_animation_play_and_delete(record[0], (short)record.w4)
 * (PUSH EDX; PUSH EAX -> arg1=record[0], arg2=word@0x04). The AL byte return
 * is stored into the pre-zeroed dword slot (MOV [EBP-4],AL), reloaded
 * (MOV ECX,[EBP-4]) and the zero-extended value forwarded to
 * hs_return(thread_datum, result) (PUSH value; PUSH thread_datum; CALL;
 * ADD ESP,0x10 — the two worker args and the two hs_return args cleaned
 * together). Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record*
 *   0x95660 = recorded_animation_play_and_delete(int, short) -> char (AL)
 *   0xcbf80 = hs_return(int thread_handle, int value) */
void FUN_000be7c0(int16_t function_index, int thread_datum, char init)
{
  volatile unsigned short result_slot;
  int *record;
  unsigned int result;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    /* The original pre-zeroes the result dword and then stores only the byte
     * return (AL) into it. Routing the zero-extended char return through a
     * volatile stack slot reproduces that store-then-reload codegen shape. */
    result_slot = (unsigned char)recorded_animation_play_and_delete(
      record[0], (short)((unsigned short *)record)[2]);
    result = (unsigned int)result_slot;
    hs_return(thread_datum, result);
  }
}

/* FUN_000be810 @ 0x000be810
 *
 * HaloScript builtin dispatcher, structurally identical to the recorded-
 * animation builtin (player_rumble_set_effect) above. Evaluates the script
 * function via hs_macro_function_evaluate(function_index, thread_datum, init);
 * on a non-NULL evaluation record it reads two record fields, calls a byte-
 * returning worker, and completes the calling script thread with
 * hs_return(thread_datum, <byte>).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX for one local; PUSH ESI):
 *   function_index  int16_t  [EBP+0x08]
 *   thread_datum    int      [EBP+0x0c]  -> held in ESI, reused for hs_return
 *   init            char     [EBP+0x10]
 *
 * The disassembly (NOT the supplied Ghidra pseudocode, which wrongly modeled
 * this void(void) and dropped both worker arguments and the record derefs)
 * shows: the local [EBP-4] result slot is pre-zeroed (MOVL [EBP-4],0) before
 * the evaluate call. hs_macro_function_evaluate returns the record pointer in
 * EAX. When non-NULL the original reads:
 *   record[0]  int    (offset 0x00, full dword load: MOV EAX,[EAX])
 *   record.w4  int16  (offset 0x04, zero-extended word: XOR EDX,EDX; MOV
 * DX,[EAX+4]) and calls FUN_00095680(record[0], (short)record.w4) (PUSH EDX;
 * PUSH EAX -> arg1=record[0], arg2=word@0x04). The char return in AL is stored
 * into the pre-zeroed dword slot (MOV [EBP-4],AL), reloaded (MOV ECX,[EBP-4]),
 * and the zero-extended value forwarded to hs_return(thread_datum, result)
 * (PUSH value; PUSH thread_datum; CALL; ADD ESP,0x10). */
void FUN_000be810(int16_t function_index, int thread_datum, char init)
{
  volatile unsigned short result_slot;
  int *record;
  unsigned int result;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    /* The original pre-zeroes the result dword and then stores only the byte
     * return (AL) into it. Routing the zero-extended char return through a
     * volatile stack slot reproduces that store-then-reload codegen shape. */
    result_slot = (unsigned char)FUN_00095680(
      record[0], (short)((unsigned short *)record)[2]);
    result = (unsigned int)result_slot;
    hs_return(thread_datum, result);
  }
}

/* FUN_000be8f0 @ 0x000be8f0
 *
 * HaloScript macro-function trampoline (object ranged-attack-inhibited setter),
 * structurally simpler than the byte-returning dispatchers above. Evaluates the
 * script function via hs_macro_function_evaluate(function_index, thread_datum,
 * init); this returns a pointer to a 2-int evaluation record. On a non-NULL
 * record it reads two fields and applies them, then completes the calling
 * script thread with hs_return(thread_datum, 0).
 *
 * cdecl frame:
 *   function_index  int16_t  [EBP+0x08]  -> arg1 of hs_macro_function_evaluate
 *   thread_datum    int      [EBP+0x0c]  -> arg2; reused for hs_return
 *   init            char     [EBP+0x10]  -> arg3
 *
 * Record layout used (from the Ghidra pseudocode, which correctly modeled the
 * stack params here):
 *   record[0]  int   (offset 0x00)  object handle
 *   record[1]  int   (offset 0x04)  inhibit flag, truncated to char
 * -> object_set_ranged_attack_inhibited(record[0], (char)record[1]).
 * The script thread is then resolved with hs_return(thread_datum, 0). */
void FUN_000be8f0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    object_set_ranged_attack_inhibited(record[0], (char)record[1]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000be970 @ 0x000be970
 *
 * HaloScript builtin implementation. Dumps the object subsystem's memory
 * state via objects_dump_memory(), then completes the calling script thread
 * with hs_return(thread_handle, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  (unused -- never loaded)
 *   thread_handle   int      [EBP+0x0c]  -> hs_return arg1
 *
 * objects_dump_memory() takes no args. The second stack param is loaded
 * (MOV EAX,[EBP+0xc]) and pushed as hs_return's thread_handle; the constant 0
 * is pushed as hs_return's value (PUSH 0; PUSH EAX; CALL; ADD ESP,8). Ghidra
 * modeled this void(void) and read only in_stack_00000008 (the second cdecl
 * param); kb decl was previously void(void). */
void FUN_000be970(int16_t function_index, int thread_handle)
{
  objects_dump_memory();
  hs_return(thread_handle, 0);
}

/* FUN_000bea10 @ 0x000bea10
 *
 * HaloScript macro-function trampoline (object scripting-attach). Evaluates the
 * script function via hs_macro_function_evaluate(function_index, thread_datum,
 * init), which returns a pointer to a 4-int evaluation record. On a non-NULL
 * record it forwards the first four dwords to objects_scripting_attach, then
 * completes the calling script thread with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI):
 *   function_index  int16_t  [EBP+0x08]  -> arg1 of hs_macro_function_evaluate
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI, reused for
 * hs_return init            char     [EBP+0x10]  -> arg3
 *
 * Record layout (all full dwords, from delinked disassembly):
 *   record[0]  int  (offset 0x00)  MOV (EAX),EAX
 *   record[1]  int  (offset 0x04)  MOV 0x4(EAX),EDX
 *   record[2]  int  (offset 0x08)  MOV 0x8(EAX),ECX
 *   record[3]  int  (offset 0x0c)  MOV 0xc(EAX),EDX
 * -> objects_scripting_attach(record[0], record[1], record[2], record[3]).
 * ADD ESP,0x18 combines the 16-byte attach cleanup and 8-byte hs_return
 * cleanup. Ghidra modeled this void(void) and read the three cdecl params as
 * in_stack_*. */
void FUN_000bea10(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    objects_scripting_attach(record[0], record[1], record[2], record[3]);
    hs_return(thread_datum, 0);
  }
}

/* 0xbea90 — HS script command handler: force a full garbage-collection pass,
 * then return void to the calling HS thread. This is the `garbage_collect`
 * scripting command; unlike the hs_evaluate_* handlers it takes no macro
 * arguments, so it ignores function_index (arg 1) and init (arg 3) and reads
 * only thread_datum (arg 2) to route the return.
 *
 * ABI (verified against delinked disassembly 0xbea90): cdecl, plain RET.
 * Prologue PUSH EBP;MOV EBP,ESP, then CALL garbage_collect_now (no args),
 * MOV EAX,[EBP+0xc] (thread_datum, the 2nd cdecl slot), PUSH 0; PUSH EAX;
 * CALL hs_return; ADD ESP,0x8 (hs_return's 2 args); POP EBP; RET. Side-effect
 * order preserved: GC runs before the return.
 *
 * Callees (both cdecl, in kb.json):
 *   0x13db50 = garbage_collect_now(void)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bea90(int16_t function_index, int thread_datum, char init)
{
  garbage_collect_now();
  hs_return(thread_datum, 0);
}

/* FUN_000beab0 @ 0x000beab0
 *
 * HaloScript macro-function trampoline (object body-vitality query). A direct
 * sibling of FUN_000bea10 above, differing only in the single-argument middle
 * callee. Evaluates the script function via hs_macro_function_evaluate(
 * function_index, thread_datum, init), which returns a pointer to an evaluation
 * record. On a non-NULL record it forwards the first dword (*record, MOV
 * EDX,[EAX]) to object_get_maximum_body_vitality, then completes the calling
 * script thread with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI, reused for
 *                                           hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * The lone PUSH EDX for object_get_maximum_body_vitality is not cleaned
 * immediately; its 4-byte cleanup is folded into the ADD ESP,0xc after
 * hs_return (0xc = 8 for hs_return's two cdecl args + 4 for the single-arg
 * call). Ghidra modeled this void(void) with the three cdecl params read as
 * in_stack_*. */
void FUN_000beab0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    object_get_maximum_body_vitality(*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000beaf0 @ 0x000beaf0
 *
 * HaloScript macro-function trampoline (object damage-eligibility query). A
 * direct sibling of FUN_000beab0 above, differing only in the single-argument
 * middle callee. Evaluates the script function via hs_macro_function_evaluate(
 * function_index, thread_datum, init), which returns a pointer to an evaluation
 * record. On a non-NULL record it forwards the first dword (*record, MOV
 * EDX,[EAX]) to object_can_take_damage, then completes the calling script
 * thread with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI, reused for
 *                                           hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * The lone PUSH EDX for object_can_take_damage is not cleaned immediately; its
 * 4-byte cleanup is folded into the ADD ESP,0xc after hs_return (0xc = 8 for
 * hs_return's two cdecl args + 4 for the single-arg call). Ghidra modeled this
 * void(void) with the three cdecl params read as in_stack_*. */
void FUN_000beaf0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    object_can_take_damage(*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000beb30 @ 0x000beb30
 *
 * HaloScript macro-function trampoline (object "beautify" command). A direct
 * sibling of FUN_000beab0/FUN_000beaf0 above, differing in the middle callee
 * taking two arguments. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init), which returns
 * a pointer to an evaluation record. On a non-NULL record it forwards the first
 * dword (*record) and the low byte of the second dword ((char)record[1]) to
 * object_beautify, then completes the calling script thread with
 * hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2, reused for hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * Ghidra modeled this void(void) with the three cdecl params read as
 * in_stack_*; the correct prototype is the 3-arg cdecl below. */
void FUN_000beb30(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    object_beautify(record[0], (char)record[1]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000beb70 @ 0x000beb70
 *
 * HaloScript macro-function trampoline (object-list side-effect variant). A
 * direct sibling of the FUN_000bebb0 family above. Evaluates the script
 * function via hs_macro_function_evaluate(function_index, thread_datum, init),
 * which returns a pointer to an evaluation record. On a non-NULL record it
 * forwards the first dword (*record, MOV EDX,[EAX]) to FUN_000c9d40, then
 * completes the calling script thread with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2, reused for hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * BUGFIX (was a players.obj lift regression, e14f0280): the original does
 *   MOV EDX,[EAX]; PUSH EDX; CALL FUN_000c9d40   (0xbeb8c-0xbeb8f)
 * i.e. it passes *record (the object-list handle) to FUN_000c9d40, which
 * iterates that object list (object_list_iterator_first/next at
 * 0xce450/0xce320). Ghidra models FUN_000c9d40 as void(void), so the original
 * lift called it with no argument; FUN_000c9d40 then read a stale stack value
 * as the handle and asserted "object list header index #N is unused or changed"
 * (data.c). The decl for FUN_000c9d40 is corrected to take the object-list
 * handle. */
void FUN_000beb70(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_000c9d40(*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bebb0 @ 0x000bebb0
 *
 * HaloScript macro-function trampoline (object-definition predict variant). A
 * direct sibling of the FUN_000bea10/FUN_000beab0 family above. Evaluates the
 * script function via hs_macro_function_evaluate(function_index, thread_datum,
 * init), which returns a pointer to an evaluation record. On a non-NULL record
 * it forwards the first dword (*record, MOV EAX,[EAX]) to
 * object_definition_predict, then completes the calling script thread with
 * hs_return(thread_datum, 0).
 *
 * cdecl frame:
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2, reused for hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * Ghidra modeled this void(void) with the three cdecl params read as
 * in_stack_*; the correct prototype is the 3-arg cdecl below. kb decl corrected
 * from void(void) so callers pass all three arguments. */
void FUN_000bebb0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    object_definition_predict(*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bebf0 @ 0x000bebf0
 *
 * HaloScript macro-function trampoline, a direct sibling of FUN_000bebb0
 * above. Evaluates the script function via hs_macro_function_evaluate(
 * function_index, thread_datum, init), which returns a pointer to an
 * evaluation record. On a non-NULL record it forwards the first dword
 * (*record, MOV EAX,[EAX]) to FUN_0013dbe0, then completes the calling
 * script thread with hs_return(thread_datum, 0).
 *
 * cdecl frame:
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2, reused for hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * Ghidra modeled this void(void) with the three cdecl params read as
 * in_stack_*; the correct prototype is the 3-arg cdecl below. kb decl
 * corrected from void(void) so callers pass all three arguments. */
void FUN_000bebf0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_0013dbe0(*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bec30 @ 0x000bec30
 *
 * HaloScript macro-function evaluator wrapper, a direct sibling of
 * FUN_000bebf0 above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); while that
 * returns NULL the evaluation is still pending and nothing is committed. Once a
 * non-NULL evaluation record is returned, its first field is loaded as a 16-bit
 * value (*(short *)record) and forwarded to FUN_0013dc10, then the thread is
 * committed with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; reused for hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX. On non-NULL the
 * original loads its first field as a 16-bit value (word load) and passes it to
 * FUN_0013dc10 (which takes a short camera_point_index), then commits the
 * thread with hs_return(thread_datum, 0). Ghidra modeled this void(void) with
 * the three cdecl params read as in_stack_*; the correct prototype is the 3-arg
 * cdecl below. kb decl corrected from void(void) so callers pass all three
 * arguments. */
void FUN_000bec30(int16_t function_index, int thread_datum, char init)
{
  short *record;

  record =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_0013dc10(*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bec70 @ 0x000bec70
 *
 * HaloScript builtin implementation, a direct sibling of the numeric-countdown
 * wrappers above (FUN_000be6f0 / FUN_000be710). It does not call
 * hs_macro_function_evaluate: it invokes the void/void helper FUN_0013dcb0
 * directly, then completes the calling script thread with
 * hs_return(thread_handle, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  (unused -- never loaded)
 *   thread_handle   int      [EBP+0x0c]  -> hs_return arg1
 *
 * FUN_0013dcb0() takes no args and is called first. The second stack param is
 * then loaded (MOV EAX,[EBP+0xc]) and pushed as hs_return's thread_handle; the
 * constant 0 is pushed as hs_return's value (PUSH 0; PUSH EAX; CALL hs_return;
 * ADD ESP,8 cleans the two cdecl args). Ghidra modeled this void(void) and read
 * the second cdecl param as in_stack_00000008 (mislabeled -- it is [EBP+0xc]);
 * kb decl was previously void(void). */
void FUN_000bec70(int16_t function_index, int thread_handle)
{
  FUN_0013dcb0();
  hs_return(thread_handle, 0);
}

/* FUN_000bec90 @ 0x000bec90
 *
 * HaloScript macro-function evaluator wrapper, a direct sibling of
 * FUN_000be3b0 / FUN_000bed20 above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); while that
 * returns NULL the evaluation is still pending and nothing is committed. Once a
 * non-NULL evaluation record is returned, its first dword (*record) is passed
 * to object_pvs_activate, then the calling thread is completed with
 * hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; reused for hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX. On non-NULL the
 * original loads its first dword (MOV [EAX] = *(int *)record) and passes it to
 * object_pvs_activate, then pushes 0 and thread_datum for hs_return. Ghidra
 * modeled this void(void); the three cdecl params were unmodeled (in_stack_*).
 * kb decl for this function was previously void(void). */
void FUN_000bec90(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    object_pvs_activate(*result);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000becd0 @ 0x000becd0
 *
 * HaloScript macro-function evaluator wrapper (byte-valued variant), a direct
 * sibling of FUN_000bec90 / FUN_000bed20 above. Evaluates the script function
 * via hs_macro_function_evaluate(function_index, thread_datum, init); while
 * that returns NULL the evaluation is still pending and nothing is committed.
 * Once a non-NULL evaluation record is returned, its first byte is passed
 * through lights_enable (0x139300, a cdecl byte->byte helper) and the
 * zero-extended result is committed to the calling thread with hs_return.
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX for one local):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI, reused for
 *                                           hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX. On non-NULL the
 * original loads a single byte from it (XOR EDX,EDX; MOV DL,[EAX] = a
 * zero-extended byte load, NOT a full dword) and passes it to lights_enable
 * (PUSH EDX; CALL). That callee is plain cdecl returning a byte in AL: the
 * caller stores only AL (MOV byte[EBP-4],AL) and forwards the zero-extended
 * value. The lone PUSH EDX for lights_enable is not cleaned immediately; its
 * 4-byte cleanup is folded into the ADD ESP,0xc after hs_return (0xc = 8 for
 * hs_return's two cdecl args + 4 for lights_enable's arg), confirming
 * lights_enable is cdecl with one stack arg. Ghidra modeled this void(void);
 * the three cdecl params were unmodeled (in_stack_*) and lights_enable's
 * argument/return were mis-declared void(void) (kb decl for both was
 * previously void(void)). lights_enable's true name is uncertain; it behaves
 * as a boolean toggle/setter returning a state byte. */
void FUN_000becd0(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;
  unsigned int value;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != NULL) {
    value = lights_enable(*result);
    hs_return(thread_datum, (int)value);
  }
}

/* FUN_000bed20 @ 0x000bed20
 *
 * HaloScript macro-function evaluator wrapper (16-bit-valued variant), a direct
 * sibling of FUN_000be3b0 above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); while that
 * returns NULL the evaluation is still pending and nothing is committed. Once a
 * non-NULL evaluation record is returned, its first dword is converted through
 * FUN_00145740 (a cdecl helper returning a 16-bit value in AX) and the
 * zero-extended result is committed to the calling thread with hs_return.
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX for one local):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI, reused for
 *                                           hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX. On non-NULL the
 * original loads its first dword (MOV EDX,[EAX] = *(int *)record, a full-dword
 * load) and passes it to FUN_00145740 (PUSH EDX; CALL). That callee is plain
 * cdecl (RET 0 at 0x1457a7, POP ESI/POP EBP/RET) returning 16 bits: the caller
 * stores only AX (MOV word[EBP-4],AX) and forwards the zero-extended value. The
 * lone PUSH EDX for FUN_00145740 is not cleaned immediately; its 4-byte cleanup
 * is folded into the ADD ESP,0xc after hs_return (0xc = 8 for hs_return's two
 * cdecl args + 4 for FUN_00145740's arg), confirming FUN_00145740 is cdecl.
 * Ghidra modeled this void(void); the three cdecl params were unmodeled
 * (in_stack_*) and FUN_00145740's argument/return were mis-declared void(void)
 * (kb decl for both was previously void(void)). */
void FUN_000bed20(int16_t function_index, int thread_datum, char init)
{
  int *result;
  unsigned int value;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value = (uint16_t)FUN_00145740(*result);
    hs_return(thread_datum, (int)value);
  }
}

/* FUN_000bed70 @ 0x000bed70
 *
 * HaloScript macro-function evaluator wrapper (animation-set variant), a direct
 * sibling of FUN_000bed20 above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); while that
 * returns NULL the evaluation is still pending and nothing is committed. Once a
 * non-NULL evaluation record is returned, its first three dwords are forwarded
 * to FUN_001457b0 (a cdecl helper that sets an object's animation state), then
 * the calling thread is completed with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI for thread_datum):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI, reused for
 *                                           hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX. On non-NULL the
 * original pushes the record's first three dwords in reverse
 * (PUSH [EAX+8]; PUSH [EAX+4]; PUSH [EAX]) and CALLs FUN_001457b0 with three
 * cdecl args = (record[0], record[1], record[2]); record[2] is an animation
 * name pointer (char *). The combined ADD ESP,0x14 after the two trailing calls
 * folds FUN_001457b0's 3-dword cleanup with hs_return's 2-dword cleanup
 * (3 + 2 = 5 dwords = 0x14), confirming both are cdecl. Ghidra modeled this
 * void(void); the three cdecl params were unmodeled (in_stack_*) and
 * FUN_001457b0's arguments were mis-declared void(void) (kb decl was previously
 * void(void) for both this function and FUN_001457b0). */
void FUN_000bed70(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_001457b0(result[0], result[1], (char *)result[2]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bedb0 @ 0x000bedb0
 *
 * HaloScript macro-function evaluator wrapper (animation-state variant), a
 * direct sibling of FUN_000bed70 above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); while that
 * returns NULL the evaluation is still pending and nothing is committed. Once a
 * non-NULL evaluation record is returned, its fields are forwarded to the
 * animation-state helper FUN_001457d0, then the calling thread is completed
 * with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI for thread_datum):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI, reused for
 *                                           hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX. On non-NULL the
 * original forwards four cdecl args to FUN_001457d0 in reverse push order:
 *   PUSH movzx(WORD [EAX+0xc])   -> arg4 = zero-extended 16-bit field @ +0xc
 *   PUSH [EAX+8]                 -> arg3 = record[2] (char *, animation name)
 *   PUSH [EAX+4]                 -> arg2 = record[1]
 *   PUSH [EAX]                   -> arg1 = record[0]
 * This is FUN_001457b0's 3-arg animation-state signature plus a trailing 16-bit
 * argument; the arg4 load is `XOR EDX,EDX; MOV DX, WORD PTR [EAX+0xc]` (an
 * unsigned-short widening, hence the [LOADW] shape). The combined ADD ESP,0x18
 * after the two trailing calls folds FUN_001457d0's 4-dword cleanup (0x10) with
 * hs_return's 2-dword cleanup (0x08), confirming both are cdecl. Ghidra modeled
 * this void(void): the three cdecl params were unmodeled (in_stack_*) and
 * FUN_001457d0's arguments were hidden because its kb decl was void(void). */
void FUN_000bedb0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_001457d0(record[0], record[1], (char *)record[2],
                 *(unsigned short *)(record + 3));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bee00 @ 0x000bee00
 *
 * HaloScript macro-function evaluator wrapper (byte-dispatch variant), a direct
 * sibling of FUN_000bedb0 above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); while that
 * returns NULL the evaluation is still pending and nothing is committed. On a
 * non-NULL evaluation record the zero-extended first byte of the record is
 * forwarded to the side-effect routine at 0x184b60, then the calling thread is
 * completed with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI for thread_datum):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI, reused for
 *                                           hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3
 *
 * hs_macro_function_evaluate returns the record pointer in EAX (TEST EAX,EAX /
 * JZ). On non-NULL the original zero-extends the record's first byte
 * (XOR EDX,EDX; MOV DL,BYTE PTR [EAX]) and pushes it as the single cdecl arg to
 * the routine at 0x184b60, then pushes (0, thread_datum) for hs_return. One
 * combined ADD ESP,0x0c folds 0x184b60's 1-dword cleanup with hs_return's
 * 2-dword cleanup, confirming 0x184b60 is cdecl caller-cleaned with exactly one
 * argument here. Ghidra modeled this void(void): the three cdecl params were
 * unmodeled (in_stack_*) and 0x184b60's argument was hidden because its kb decl
 * was void render_effects(void). The 0x184b60=render_effects attribution is
 * unverified; only its 1-arg cdecl shape is proven at this call site. */
void FUN_000bee00(int16_t function_index, int thread_datum, char init)
{
  unsigned char *record;

  record = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (record != NULL) {
    render_effects(*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bee40 @ 0x000bee40
 *
 * HaloScript macro-function evaluator wrapper (unit blink-enable variant), a
 * direct sibling of FUN_000bee00 above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); while that
 * returns NULL the evaluation is still pending and nothing is committed. On a
 * non-NULL evaluation record the record's first dword (a unit handle) and the
 * zero-extended byte at +0x4 (the boolean flag) are forwarded to
 * unit_scripting_can_blink, then the calling thread is completed with
 * hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI for thread_datum; no _chkstk,
 * no locals):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *                                           (loaded to ECX)
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI across the whole
 *                                           body, reused for hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3 (loaded to EAX)
 *
 * hs_macro_function_evaluate returns the record pointer in EAX (TEST EAX,EAX /
 * JZ skips the body). On non-NULL the original reads the record's +0x4 field as
 * a ZERO-EXTENDED BYTE (XOR EDX,EDX; MOV DL,BYTE PTR [EAX+0x4]) — not a dword —
 * and reloads the handle with MOV EAX,DWORD PTR [EAX], then PUSH EDX; PUSH EAX.
 * The hs_return arg1 comes from the preserved ESI (the ORIGINAL thread_datum),
 * not from the returned record pointer. A single combined ADD ESP,0x10 at
 * 0xbee72 folds unit_scripting_can_blink's 2-dword cleanup with hs_return's
 * 2-dword cleanup; the context-pack ARG_COUNT warning on 0xcbf80 ("cleanup=4")
 * is that merge, and the PUSH count proves hs_return still takes exactly 2.
 * No FPU ops. Ghidra modeled this void(void): the three cdecl params were
 * unmodeled (in_stack_*), so all three come off the stack — no @<reg>.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a9c00 = unit_scripting_can_blink(int unit_handle, char can_blink)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bee40(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    unit_scripting_can_blink(record[0], *(unsigned char *)(record + 1));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bee80 @ 0x000bee80
 *
 * HaloScript macro-function evaluator wrapper (unit "open" variant), a direct
 * sibling of FUN_000bee40 above and structurally identical to FUN_000beb70.
 * Evaluates the script function via hs_macro_function_evaluate(function_index,
 * thread_datum, init); while that returns NULL the evaluation is still pending
 * and nothing is committed. On a non-NULL evaluation record the record's first
 * dword (a unit handle) is forwarded to unit_open, then the calling script
 * thread is completed with hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI for thread_datum; no _chkstk,
 * no locals, no FPU):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *                                           (loaded to ECX)
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI across the whole
 *                                           body, reused for hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3 (loaded to EAX)
 *
 * The evaluate call pushes EAX([+0x10]), ESI([+0x0c]), ECX([+0x08]) and cleans
 * with ADD ESP,0xC, so its first argument is [EBP+0x08]. The returned EAX is
 * tested (TEST EAX,EAX / JZ epilogue) and then dereferenced at offset 0
 * (MOV EDX,[EAX]; PUSH EDX) as the single unit_open argument — i.e. the kb decl
 * `int hs_macro_function_evaluate(...)` really returns a record POINTER; cast
 * at the call site rather than widening the callee decl. hs_return's arg1 comes
 * from the preserved ESI (the ORIGINAL thread_datum), not from the record.
 * A single combined ADD ESP,0xC at 0xbeeac folds unit_open's 1-dword cleanup
 * with hs_return's 2-dword cleanup; the context-pack ARG_COUNT warning on
 * 0xcbf80 ("cleanup=3 vs decl=2") is that merge, and the PUSH count proves
 * hs_return still takes exactly 2 args. Ghidra modeled this void(void), so the
 * three cdecl params showed up as in_stack_* — they are stack args, not @<reg>.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1ae160 = unit_open(int unit_handle)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bee80(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    unit_open(*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000beec0 @ 0x000beec0
 *
 * HaloScript macro-function evaluator wrapper (unit "close" variant), the
 * direct sibling of FUN_000bee80 above: byte-identical in shape, differing
 * only in which record-first-dword consumer it calls (unit_close 0x1ae180
 * instead of unit_open 0x1ae160). Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); while that
 * returns NULL the evaluation is still pending and nothing is committed. On a
 * non-NULL evaluation record the record's first dword (a unit handle) is
 * forwarded to unit_close, then the calling script thread is completed with
 * hs_return(thread_datum, 0).
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI for thread_datum; no _chkstk,
 * no locals, no FPU, plain RET so the caller cleans):
 *   function_index  int16_t  [EBP+0x08]  -> hs_macro_function_evaluate arg1
 *                                           (loaded to ECX)
 *   thread_datum    int      [EBP+0x0c]  -> arg2; held in ESI across the whole
 *                                           body, reused for hs_return arg1
 *   init            char     [EBP+0x10]  -> arg3 (loaded to EAX)
 *
 * The evaluate call pushes EAX([+0x10]), ESI([+0x0c]), ECX([+0x08]) and cleans
 * with ADD ESP,0xC, so its first argument is [EBP+0x08]. The returned EAX is
 * tested (TEST EAX,EAX / JZ epilogue) and then dereferenced at offset 0
 * (MOV EDX,[EAX]; PUSH EDX) as the single unit_close argument — i.e. the kb
 * decl `int hs_macro_function_evaluate(...)` really returns a record POINTER;
 * cast at the call site rather than widening the callee decl. hs_return's arg1
 * comes from the preserved ESI (the ORIGINAL thread_datum), not from the
 * record. A single combined ADD ESP,0xC at 0xbeeec folds unit_close's 1-dword
 * cleanup with hs_return's 2-dword cleanup; the ARG_COUNT enrichment warning
 * on 0xcbf80 ("cleanup=3 vs decl=2") is that merge, and the PUSH count proves
 * hs_return still takes exactly 2 args (do NOT "fix" either decl). Ghidra
 * modeled this void(void), so the three cdecl params showed up as in_stack_*
 * — they are stack args, not @<reg>.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1ae180 = unit_close(int unit_handle)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000beec0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    unit_close(*record);
    hs_return(thread_datum, 0);
  }
}

/* 0xbef40 — HS script function handler: kill a unit. Structural twin of
 * FUN_000beec0 above; only the middle callee differs (unit_kill @0x1a7fa0
 * instead of unit_close @0x1ae180). 13 instructions, standard EBP frame, ESI
 * is the one callee-saved register and holds thread_datum live across the
 * evaluate call — which is why the SAME value feeds both
 * hs_macro_function_evaluate and hs_return (do not source hs_return's arg1
 * from the record).
 *
 * Binary evidence (0xbef40-0xbef71):
 *   EAX=[EBP+0x10] (init), ECX=[EBP+0x08] (function_index), ESI=[EBP+0x0c]
 *   (thread_datum). CALL 1 @0xbef50 pushes EAX, ESI, ECX (cdecl reverse
 *   order) then ADD ESP,0xC -> C order (function_index, thread_datum, init).
 *   TEST EAX,EAX / JZ 0xbef6f skips both remaining calls on NULL.
 *   CALL 2 @0xbef5f: MOV EDX,[EAX]; PUSH EDX -- the argument is the
 *   DEREFERENCE of the returned record at offset 0, not the pointer.
 *   CALL 3 @0xbef67: PUSH 0; PUSH ESI -> hs_return(thread_datum, 0).
 *   ONE combined ADD ESP,0xC at 0xbef6c folds unit_kill's 1 dword with
 *   hs_return's 2 dwords; the ARG_COUNT enrichment warning on 0xcbf80
 *   ("cleanup=3 vs decl=2") is that merge -- hs_return really takes 2 args,
 *   do NOT "fix" its decl. POP ESI; POP EBP; RET (no immediate -> cdecl).
 *   Ghidra modeled this void(void), so the three cdecl params appeared as
 *   in_stack_* pseudo-locals; they are stack args, not @<reg>.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a7fa0 = unit_kill(int unit_handle)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bef40(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    unit_kill(*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bef80 @ 0x000bef80
 *
 * HaloScript builtin dispatcher, same family as FUN_000bdef0 / FUN_000bef40
 * above. Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); on a
 * non-NULL evaluation record it loads the record's FIRST DWORD (a full 32-bit
 * load: MOV EDX,[EAX]; PUSH EDX -- a unit/object handle, exactly as
 * FUN_000bef40 does for unit_kill) and passes it to FUN_001AC0E0, whose
 * 16-bit result (AX) is forwarded to hs_return.
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX for one dword local;
 * PUSH ESI). 29 instructions, 0xbef80-0xbefc1. Plain RET (caller cleans),
 * no _chkstk, no FPU, no SEH.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, held live across the
 *                                          evaluate call and reused as
 *                                          hs_return arg1 (do NOT source it
 *                                          from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   CALL 0xcc560 pushes EAX([EBP+0x10]), ESI([EBP+0xc]), ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init);
 *   ADD ESP,0xc. TEST EAX,EAX / JZ skips both remaining calls on NULL.
 *   MOV dword [EBP-0x4],0 is emitted BEFORE the evaluate call (the result
 *   slot is pre-zeroed); inside the taken branch MOV word [EBP-0x4],AX
 *   writes only the low 16 bits and MOV EAX,dword [EBP-0x4] reads all 32 --
 *   i.e. a zero-extended 16-bit result, which is why the slot is modelled as
 *   a volatile dword rather than a plain int.
 *   One combined ADD ESP,0xc after the hs_return CALL folds FUN_001AC0E0's
 *   1 dword arg with hs_return's 2 (adjacent-call cleanup); the ARG_COUNT
 *   warning on 0xcbf80 ("cleanup=3 vs decl=2") is that merge -- hs_return
 *   really takes 2 args, do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4) and the FUN_001AC0E0
 *   argument was dropped entirely; both are lift artifacts, not @<reg>.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1ac0e0 = FUN_001AC0E0(int handle) -> int16_t in AX (decl corrected
 *              from Ghidra's void(void); semantics of the returned 16-bit
 *              value are UNKNOWN)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bef80(int16_t function_index, int thread_datum, char init)
{
  volatile unsigned int result_slot;
  int *record;
  unsigned int result;

  result_slot = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    result_slot = (unsigned short)FUN_001AC0E0(record[0]);
    result = (unsigned int)result_slot;
    hs_return(thread_datum, result);
  }
}

/* 0xbefd0 — HS script function handler: stop a unit's custom animation.
 *
 * Byte-shape twin of FUN_000bdf40 (differs only in the middle callee). cdecl
 * frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET (no RET
 * immediate — caller cleans).
 *
 * Params from the EBP offsets (Ghidra modeled this void(void) and dropped all
 * three; the in_stack_* names in its output are the tell — they are stack
 * params, NOT register args):
 *   function_index  int16_t  [EBP+0x08] -> ECX -> evaluate arg 1
 *   thread_datum    int      [EBP+0x0c] -> ESI (cached: used twice)
 *   init            char     [EBP+0x10] -> EAX -> evaluate arg 3
 *
 * CALL 0xcc560 pushes EAX(init), ESI(thread_datum), ECX(function_index) and
 * cleans with ADD ESP,0xc — 3 stack args, so the C order is
 * (function_index, thread_datum, init). TEST EAX,EAX; JZ end is the NULL
 * guard on the returned result record.
 *
 * The middle call does MOV EDX,dword ptr [EAX]; PUSH EDX — a FULL 32-bit load
 * of the record's first dword (unlike the byte load in the 0xbdef0 sibling),
 * passed as unit_stop_custom_animation(unit_handle).
 *
 * The single trailing ADD ESP,0xc at 0xbeffc is MERGED cleanup for the 1 arg
 * of unit_stop_custom_animation plus the 2 args of hs_return (1+2 = 3 dwords).
 * The call-site audit's "hs_return cleanup=3 vs decl=2" is a false positive
 * from that adjacent-call merging; the disasm shows exactly 2 pushes for
 * hs_return (PUSH 0x0; PUSH ESI). Same pattern documented on the twin.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560   = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *               char init) -> int* (result record, NULL on failure)
 *   0x1af0d0  = unit_stop_custom_animation(int unit_handle)
 *   0xcbf80   = hs_return(int thread_handle, int value)
 *
 * Placed in hs.c rather than players.c (where kb groups 0xbefd0) per the same
 * lift directive as the twin: players.c does not compile under VC71 (clang-only
 * __attribute__ / raw fnptr casts), so it would be permanently unmeasurable
 * there. */
void FUN_000befd0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    unit_stop_custom_animation(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf010 @ 0x000bf010
 *
 * HaloScript builtin dispatcher, same family as FUN_000bef40 / FUN_000bef80
 * above, but forwarding FOUR evaluated arguments instead of one. Evaluates the
 * script function via hs_macro_function_evaluate(function_index, thread_datum,
 * init); on a non-NULL evaluation record it reads four fields out of the
 * caller-owned argument block and passes them to FUN_001AC180, whose 8-bit
 * result (AL) is forwarded to hs_return.
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ECX for one dword local;
 * PUSH ESI). No _chkstk, no FPU, no SEH.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, held live across the
 *                                          evaluate call and reused as
 *                                          hs_return arg1 (do NOT source it
 *                                          from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   CALL 0xcc560 pushes EAX([EBP+0x10]), ESI([EBP+0xc]), ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init);
 *   ADD ESP,0xc. TEST EAX,EAX / JZ skips both remaining calls on NULL.
 *   The record is the evaluated-argument block, 0x10 bytes / 4 fields:
 *     +0x00 int   (MOV EAX,[EAX])
 *     +0x04 int   (MOV EDX,[EAX+4] at 0xbf03d)
 *     +0x08 int   used as a pointer (MOV ECX,[EAX+8])
 *     +0x0c BYTE  zero-extended (XOR EDX,EDX; MOV DL,[EAX+0xc]) -- a byte
 *                 load, NOT a dword; do not widen it
 *   CALL 0x1ac180 pushes EDX(+0xc byte), ECX(+0x8), EDX(+0x4), EAX(+0x0) in
 *   cdecl reverse order -> C order (field0, field1, field2, field3). EDX is
 *   RELOADED between the two PUSH EDX (the +0x4 load at 0xbf03d sits between
 *   them), so the two pushes are different values -- classic push-sequence
 *   reload, not a duplicated argument.
 *   MOV dword [EBP-0x4],0 at 0xbf021 pre-zeroes the result slot; inside the
 *   taken branch MOV byte [EBP-0x4],AL writes only the low 8 bits and
 *   MOV ECX,dword [EBP-0x4] reads all 32 -- a zero-extended 8-bit result,
 *   which is why the slot is modelled as a volatile dword rather than a plain
 *   char (same shape as FUN_000bef80's 16-bit slot above).
 *   ONE combined ADD ESP,0x18 at 0xbf057 folds FUN_001AC180's 4 dwords with
 *   hs_return's 2; the ARG_COUNT enrichment warning on 0xcbf80
 *   ("cleanup=6 vs decl=2") is that merge -- hs_return really takes 2 args,
 *   do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args,
 *   not @<reg>.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1ac180 = FUN_001ac180(int actor, int anim_tag, void *entry, int do_flag)
 *              -> char in AL (semantics of the returned flag are UNKNOWN)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf010(int16_t function_index, int thread_datum, char init)
{
  volatile unsigned int result_slot;
  int *record;
  unsigned int result;

  result_slot = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    result_slot =
      (unsigned char)FUN_001ac180(record[0], record[1], (void *)record[2],
                                  (int)*(unsigned char *)(record + 3));
    result = (unsigned int)result_slot;
    hs_return(thread_datum, result);
  }
}

/* FUN_000bf060 @ 0x000bf060
 *
 * HaloScript builtin dispatcher, immediate structural twin of FUN_000bf010
 * above: same cdecl frame, same pre-zeroed result dword, same four evaluated
 * arguments -- only the middle callee differs (FUN_001A7DF0 instead of
 * FUN_001AC180). Evaluates the script function via
 * hs_macro_function_evaluate(function_index, thread_datum, init); on a
 * non-NULL evaluation record it reads four fields out of the caller-owned
 * argument block, passes them to FUN_001A7DF0, and forwards that call's 8-bit
 * result (AL) to hs_return.
 *
 * cdecl frame 0xbf060-0xbf0ae, 30 insns (PUSH EBP; MOV EBP,ESP; PUSH ECX for
 * one dword local at EBP-0x4; PUSH ESI -- the only callee-saved register).
 * No _chkstk, no FPU, no SEH, no local buffers. RET carries no immediate.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, held live across the
 *                                          evaluate call and reused as
 *                                          hs_return arg1 (do NOT source it
 *                                          from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   MOV dword [EBP-0x4],0 at 0xbf071 pre-zeroes the result slot BEFORE the
 *   evaluate call. CALL 0xcc560 @0xbf078 pushes EAX([EBP+0x10]),
 *   ESI([EBP+0xc]), ECX([EBP+0x8]) in cdecl reverse order -> C order
 *   (function_index, thread_datum, init); ADD ESP,0xc. TEST EAX,EAX /
 *   JZ 0xbf0aa skips both remaining calls on a NULL record.
 *   The record is the evaluated-argument block, 0x10 bytes / 4 fields:
 *     +0x00 int   (MOV EAX,[EAX])
 *     +0x04 int   (MOV EDX,[EAX+4])
 *     +0x08 int   (MOV ECX,[EAX+8])
 *     +0x0c BYTE  zero-extended (XOR EDX,EDX; MOV DL,[EAX+0xc]) -- a byte
 *                 load, NOT a dword; do not widen it
 *   CALL 0x1a7df0 @0xbf095 pushes EDX(+0xc byte), ECX(+0x8), EDX(+0x4),
 *   EAX(+0x0) in cdecl reverse order -> C order (field0, field1, field2,
 *   field3). EDX is RELOADED between the two PUSH EDX (the +0x4 load sits
 *   between them), so the two pushes carry different values -- push-sequence
 *   reload, not a duplicated argument.
 *   At 0xbf09a only MOV byte [EBP-0x4],AL writes the low 8 bits; at 0xbf09d
 *   MOV ECX,dword [EBP-0x4] reads all 32 -- a zero-extended 8-bit result,
 *   which is why the slot is modelled as a volatile dword rather than a plain
 *   char (same shape as FUN_000bf010's slot above).
 *   CALL 0xcbf80 @0xbf0a2 pushes ECX(result dword) then ESI ->
 *   hs_return(thread_datum, result).
 *   ONE combined ADD ESP,0x18 at 0xbf0a7 folds FUN_001A7DF0's 4 dwords with
 *   hs_return's 2; the ARG_COUNT enrichment warning on 0xcbf80
 *   ("cleanup=6 stack args vs decl=2") is that merge -- hs_return really
 *   takes 2 args, do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args,
 *   not @<reg>.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a7df0 = FUN_001a7df0(int datum_handle, int, int, int) -> char in AL
 *              (semantics of the returned flag are UNKNOWN)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf060(int16_t function_index, int thread_datum, char init)
{
  volatile unsigned int result_slot;
  int *record;
  unsigned int result;

  result_slot = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    result_slot = (unsigned char)FUN_001a7df0(
      record[0], record[1], record[2], (int)*(unsigned char *)(record + 3));
    result = (unsigned int)result_slot;
    hs_return(thread_datum, result);
  }
}

/* FUN_000bf0b0 @ 0x000bf0b0
 *
 * HaloScript builtin dispatcher, third structural twin of FUN_000bf010 /
 * FUN_000bf060 above: same cdecl frame, same pre-zeroed result dword, same
 * evaluate -> worker -> hs_return skeleton. The only differences are the
 * worker (unit_custom_animation_at_frame) and one extra evaluated argument
 * (five fields instead of four).
 *
 * cdecl frame 0xbf0b0-0xbf10e (PUSH EBP; MOV EBP,ESP; PUSH ECX for the single
 * dword local at EBP-0x4; PUSH ESI -- the only callee-saved register, and it
 * holds thread_datum live across the evaluate call). No _chkstk, no FPU, no
 * SEH, no local buffers. RET carries no immediate.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *                                          (do NOT source it from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   MOV dword [EBP-0x4],0 pre-zeroes the whole result slot BEFORE the evaluate
 *   call; later only MOV byte [EBP-0x4],AL writes the low 8 bits and
 *   MOV EAX,dword [EBP-0x4] reads all 32 -- i.e. a zero-extended 8-bit
 *   result, modelled as a volatile dword (same as both twins above), NOT as a
 *   type-punned char.
 *   CALL 0xcc560 pushes EAX([EBP+0x10]), ESI([EBP+0xc]), ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init);
 *   ADD ESP,0xc = 3 args. TEST EAX,EAX / JZ skips both remaining calls on a
 *   NULL record.
 *   The record is the evaluated-argument block, 0x14 bytes / 5 fields:
 *     +0x00 int   (MOV [EAX])
 *     +0x04 int   (MOV [EAX+4])
 *     +0x08 int   (MOV [EAX+8])
 *     +0x0c BYTE  ZERO-extended (XOR ECX,ECX; MOV CL,[EAX+0xc])
 *     +0x10 WORD  ZERO-extended (XOR EDX,EDX; MOV DX,[EAX+0x10]) -- note this
 *                 is a MOVZX-shaped load, not MOVSX; Ghidra's "(short)" cast
 *                 on this field is misleading. Do not widen either narrow
 *                 field to a dword read (lift-learnings 24, LOADW).
 *   CALL 0x1af100 pushes EDX,ECX,EDX,ECX,EDX in cdecl reverse order -> C
 *   order (field0, field1, field2, field3_byte, field4_word). ECX/EDX are
 *   RELOADED between the repeated pushes, so the repeats carry different
 *   values -- push-sequence reload, not duplicated arguments.
 *   CALL 0xcbf80 pushes the result dword then ESI -> hs_return(thread_datum,
 *   result). ONE combined ADD ESP,0x1c folds unit_custom_animation_at_frame's
 *   5 dwords with hs_return's 2; the ARG_COUNT enrichment warning on 0xcbf80
 *   ("cleanup=7 stack args vs decl=2") is that merge -- hs_return really
 *   takes 2 args, do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap).
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1af100 = unit_custom_animation_at_frame(int unit_handle, int, int, int,
 *              int16_t frame) -> char in AL
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf0b0(int16_t function_index, int thread_datum, char init)
{
  volatile unsigned int result_slot;
  int *record;
  unsigned int result;

  result_slot = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    result_slot = (unsigned char)unit_custom_animation_at_frame(
      record[0], record[1], record[2], (int)*(unsigned char *)(record + 3),
      *(unsigned short *)(record + 4));
    result = (unsigned int)result_slot;
    hs_return(thread_datum, result);
  }
}

/* 0xbf110 — HS script function handler: query a per-object boolean and return
 * it to the calling HS thread.
 *
 * Structural graft of the two neighbours: the full-dword argument load of
 * 0xbefd0 combined with the byte-result-into-a-pre-zeroed-dword-slot return
 * shape of 0xc1500.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (one dword local at EBP-0x4);
 * PUSH ESI. No _chkstk. Epilog POP ESI; MOV ESP,EBP; POP EBP; RET — caller
 * cleans, so kb's `void(void)` is the usual Ghidra in_stack_* artifact, NOT a
 * register-arg function. Params from the EBP offsets:
 *   function_index  int16_t  [EBP+0x08] -> ECX -> evaluate arg 1
 *   thread_datum    int      [EBP+0x0c] -> ESI (kept live across both calls)
 *   init            char     [EBP+0x10] -> EAX -> evaluate arg 3
 *
 * CALL 0xcc560 @0xbf128 pushes EAX(init), ESI(thread_datum),
 * ECX(function_index) and cleans with ADD ESP,0xc, so the C order is
 * (function_index, thread_datum, init). TEST EAX,EAX; JZ 0xbf14c wraps the
 * whole body in the NULL guard on the returned result record.
 *
 * MOV dword ptr [EBP-0x4],0x0 at 0xbf121 sits INSIDE that push sequence, i.e.
 * BEFORE the evaluate call — hence `value.i = 0;` first.
 *
 * CALL 0x1ac150 @0xbf137 is preceded by MOV EDX,dword ptr [EAX]; PUSH EDX — a
 * FULL 32-bit load of the record's first dword (a handle), one stack arg.
 * Ghidra modelled this callee as `void FUN_001ac150(void)` and dropped both the
 * argument and the result (§16/§31 implicit-EAX): the result comes back in AL
 * and is stored NARROW with MOV byte ptr [EBP-0x4],AL at 0xbf13c, then reloaded
 * as a full dword (MOV EAX,[EBP-0x4] at 0xbf13f). Because the slot was
 * pre-zeroed, the dword handed to hs_return is the zero-extended low byte —
 * keep the union byte-store idiom, a plain `value.i = ...` would emit a dword
 * store and lose that shape.
 *
 * The single trailing ADD ESP,0xc at 0xbf149 is MERGED cleanup for the 1 arg of
 * FUN_001ac150 plus the 2 args of hs_return (1+2 = 3 dwords) — the call-site
 * audit's "hs_return cleanup=3 vs decl=2" is the same false positive already
 * documented on the 0xbefd0 twin above. Do NOT "fix" hs_return's decl.
 *
 * No FPU ops, no struct writes, no local buffers; the only struct access is
 * result[0] (offset +0x0, dword).
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560   = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *               char init) -> int* (result record, NULL on failure)
 *   0x1ac150  = FUN_001ac150(int handle) -> byte in AL (return width beyond the
 *               low byte is UNKNOWN — only AL is consumed here)
 *   0xcbf80   = hs_return(int thread_handle, int value)
 *
 * Placed in hs.c rather than players.c (where kb groups 0xbf110) per the same
 * lift directive as the neighbours: players.c does not compile under VC71
 * (clang-only __attribute__ / raw fnptr casts), so it would be permanently
 * unmeasurable there. NOTE: a global `maintain.py` run will try to move this
 * function (and the 0xbefd0 twin) into players.c — that move must be rejected.
 */
void FUN_000bf110(int16_t function_index, int thread_datum, char init)
{
  int *result;
  union {
    int i;
    unsigned char b;
  } value;

  value.i = 0;
  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value.b = (unsigned char)FUN_001ac150(result[0]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bf160 @ 0x000bf160
 *
 * HaloScript builtin dispatcher, the simplest member of the family above:
 * evaluate the script arguments, and on a non-NULL argument record call one
 * sound_manager worker with two of its fields, then return a CONSTANT 0 to the
 * script thread. Unlike FUN_000bf010 / FUN_000bf060 / FUN_000bf0b0 there is no
 * result slot at all -- the worker's return value is discarded (nothing reads
 * EAX after CALL 0x1ac0a0) and hs_return's second argument is an immediate
 * PUSH 0x0.
 *
 * cdecl frame 0xbf160-0xbf197, 24 instructions (PUSH EBP; MOV EBP,ESP;
 * PUSH ESI). NO local dword (no `PUSH ECX` in the prologue, unlike the twins),
 * no _chkstk, no FPU, no SEH, no local buffers. ESI is the only callee-saved
 * register and it holds thread_datum live across the evaluate call. RET carries
 * no immediate.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *                                          (do NOT source it from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence (delinked/functions/000bf160.obj, bytes verified):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]), ESI([EBP+0xc]), ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init);
 *   ADD ESP,0xc = 3 args. TEST EAX,EAX / JZ 0xbf195 skips both remaining calls
 *   on a NULL record.
 *   The record is the evaluated-argument block, 2 fields:
 *     +0x00 int   (MOV EAX,dword [EAX])
 *     +0x04 BYTE  ZERO-extended (XOR EDX,EDX; MOV DL,byte [EAX+0x4]) -- a
 *                 MOVZX-shaped load; do NOT widen it to a dword read and do
 *                 NOT let it sign-extend (lift-learnings 24, LOADW).
 *   Note the load order: the +0x4 byte is fetched BEFORE the +0x0 dword,
 *   because the dword load overwrites EAX (the record pointer itself).
 *   CALL 0x1ac0a0 pushes EDX(+0x4 byte) then EAX(+0x0 dword) in cdecl reverse
 *   order -> C order (field0_dword, field1_byte). Ghidra printed this as a
 *   0-argument call and left the two pushes dangling -- the classic dropped-arg
 *   trap (lift-learnings 31 / 0xfc4b0 weapon_owner_update precedent); the
 *   kb.json decl was `void(void)` and has been corrected to 2 cdecl args.
 *   0xbf185 is the ONLY call site of 0x1ac0a0 in the whole XBE
 *   (check_arg_counts.py --callee 0x1ac0a0: sites=1, push=2).
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0).
 *   ONE combined ADD ESP,0x10 at 0xbf192 folds FUN_001AC0A0's 2 dwords with
 *   hs_return's 2; any ARG_COUNT warning on 0xcbf80 ("cleanup=4 vs decl=2") is
 *   that merge -- hs_return really takes 2 args, do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap).
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *              (kb decl says `int`, but the call site dereferences the result;
 *              cast at the call site, as every twin above does)
 *   0x1ac0a0 = FUN_001AC0A0(int, int) -- sound_manager.obj, UNPORTED, semantics
 *              UNKNOWN; return value discarded here
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf160(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_001ac0a0(record[0], (int)*(unsigned char *)(record + 1));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf1a0 @ 0x000bf1a0
 *
 * HaloScript builtin dispatcher, the immediate twin of FUN_000bf160 above:
 * same 3-parameter cdecl shape, same evaluate/NULL-check/hs_return skeleton,
 * same "worker return value discarded, script gets a CONSTANT 0" tail. The two
 * differences from the twin are the worker (0x1ac070 instead of 0x1ac0a0) and
 * the width of the record's second field (a WORD here, a BYTE there).
 *
 * cdecl frame, PUSH EBP; MOV EBP,ESP; PUSH ESI. No local dword (no `PUSH ECX`
 * in the prologue), no _chkstk, no FPU, no SEH, no local buffers. ESI is the
 * only callee-saved register and holds thread_datum live across the evaluate
 * call. RET carries no immediate (caller cleans).
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *                                          (do NOT source it from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   CALL 0xcc560 pushes EAX([EBP+0x10]), ESI([EBP+0xc]), ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init);
 *   ADD ESP,0xc = 3 args. Straight pass-through. TEST EAX,EAX / JZ skips both
 *   remaining calls on a NULL record, so the return is a POINTER (it is
 *   dereferenced below) even though kb.json declares 0xcc560 as returning
 *   `int` -- cast at the call site, as all 25+ twins in this file do.
 *   The record is the evaluated-argument block, 2 fields:
 *     +0x00 int   (MOV EAX,dword [EAX])
 *     +0x04 WORD  ZERO-extended (XOR EDX,EDX; MOV DX,word [EAX+0x4]) -- a
 *                 MOVZX-shaped load; do NOT widen it to a dword read and do
 *                 NOT let it sign-extend via a signed `short` (lift-learnings
 *                 24, LOADW). This is the ONLY divergence from FUN_000bf160,
 *                 whose same-slot field is a byte.
 *   Note the load order: the +0x4 word is fetched BEFORE the +0x0 dword,
 *   because the dword load overwrites EAX (the record pointer itself). MSVC's
 *   right-to-left cdecl argument evaluation reproduces that order from the
 *   call expression below.
 *   CALL 0x1ac070 pushes EDX(+0x4 word) then EAX(+0x0 dword) in cdecl reverse
 *   order -> C order (field0_dword, field1_word). Ghidra printed this as a
 *   0-argument call and left both pushes dangling -- the classic dropped-arg
 *   trap (lift-learnings 31 / 0xfc4b0 weapon_owner_update precedent); the
 *   kb.json decl was `void(void)` and has been corrected to 2 cdecl args,
 *   exactly as its neighbour 0x1ac0a0 was for the twin above.
 *   0xbf1c6 is the ONLY call site of 0x1ac070 in the whole XBE
 *   (check_arg_counts.py --callee 0x1ac070: sites=1, push=2).
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0).
 *   ONE combined ADD ESP,0x10 folds FUN_001AC070's 2 dwords with hs_return's
 *   2; any ARG_COUNT warning on 0xcbf80 ("cleanup=4 vs decl=2") is that merge
 *   -- hs_return really takes 2 args, do NOT "fix" its decl. It is also why
 *   check_arg_counts reports cleanup=none for 0x1ac070 at this site.
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap).
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1ac070 = FUN_001AC070(int, int) -- sound_manager.obj neighbour of
 *              0x1ac0a0, UNPORTED, semantics UNKNOWN; return value discarded
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf1a0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_001ac070(record[0], (int)*(unsigned short *)(record + 1));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf1e0 @ 0x000bf1e0
 *
 * HaloScript builtin dispatcher, third in the 0xbf110/0xbf160/0xbf1a0 family
 * above: identical 3-parameter cdecl shape, identical
 * evaluate / NULL-check / worker / hs_return skeleton, identical "worker return
 * value discarded, script gets a CONSTANT 0" tail. The two differences from
 * FUN_000bf1a0 are the worker (0x1ac030 instead of 0x1ac070) and the width of
 * the record's second field: a BYTE here, a WORD in the twin.
 *
 * cdecl frame, PUSH EBP; MOV EBP,ESP; PUSH ESI. No local dword, no _chkstk,
 * no FPU, no SEH, no local buffers. ESI is the only callee-saved register and
 * holds thread_datum live across the evaluate call. RET carries no immediate
 * (caller cleans).
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *                                          (NOT sourced from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   CALL 0xcc560 @0xbf1f0 pushes EAX([EBP+0x10]), ESI([EBP+0xc]),
 *   ECX([EBP+0x8]) in cdecl reverse order -> C order
 *   (function_index, thread_datum, init); ADD ESP,0xc = 3 args. Straight
 *   pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ 0xbf215 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced, even though
 *   kb.json declares it as returning int (same as the twins; the cast is local
 *   and the kb decl is left alone).
 *
 *   Record deref: XOR EDX,EDX; MOV DL,byte ptr [EAX+0x4] -> zero-extended
 *   BYTE at record+4 (NOT a word -- this is where this function differs from
 *   FUN_000bf1a0); MOV EAX,dword ptr [EAX] -> DWORD at record+0.
 *
 *   CALL 0x1ac030 @0xbf205 pushes EDX (the zero-extended byte) then EAX (the
 *   dword) = cdecl reverse -> C order (record[0], byte at record+4).
 *
 *   CALL 0xcbf80 @0xbf20d pushes 0x0 then ESI -> hs_return(thread_datum, 0).
 *   The script return value is the CONSTANT 0; the 0x1ac030 result is
 *   discarded (0x1ac030 is void). ADD ESP,0x10 @0xbf212 cleans the combined
 *   4 pushes of both calls -- the ARG_COUNT hazard on hs_return
 *   (cleanup=4 vs decl=2) is a false positive from that merged cleanup.
 *
 * Callees (all cdecl, in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1ac030 = FUN_001ac030(int, int) -- sound_manager.obj neighbour of
 *              0x1ac070/0x1ac0a0, UNPORTED, semantics UNKNOWN; return value
 *              discarded. kb.json decl corrected from void(void) to (int,int)
 *              per the two pushes in the disassembly.
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf1e0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_001ac030(record[0], (int)*(unsigned char *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf220 @ 0x000bf220
 *
 * HaloScript builtin dispatcher, next in the 0xbf110/0xbf160/0xbf1a0/0xbf1e0
 * family above: identical 3-parameter cdecl shape, identical
 * evaluate / NULL-check / worker / hs_return skeleton, identical "worker
 * returns nothing, script gets a CONSTANT 0" tail. The differences from the
 * twins are the worker (unit_scripting_enter_vehicle at 0x1b32d0, which takes
 * THREE args instead of two) and the record layout: all three fields here are
 * full 32-bit dwords, with no MOVZX/MOVSX anywhere in the frame -- so, unlike
 * FUN_000bf1a0 (word) and FUN_000bf1e0 (byte), there is no narrowing load to
 * reproduce (lift-learnings 24 / LOADW).
 *
 * cdecl frame, PUSH EBP; MOV EBP,ESP; PUSH ESI. No local dword, no _chkstk,
 * no FPU, no SEH, no local buffers. ESI is the only callee-saved register and
 * holds thread_datum live across the evaluate call. RET carries no immediate
 * (caller cleans). 24 instructions, 0xbf220-0xbf259.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *                                          (NOT sourced from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   CALL 0xcc560 @0xbf230 pushes EAX([EBP+0x10]), ESI([EBP+0xc]),
 *   ECX([EBP+0x8]) in cdecl reverse order -> C order
 *   (function_index, thread_datum, init); ADD ESP,0xc = 3 args. Straight
 *   pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ 0xbf257 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced, even though
 *   kb.json declares it as returning int (same as the twins; the cast is local
 *   and the kb decl is left alone).
 *
 *   Record deref: MOV EDX,dword ptr [EAX+0x8]; MOV ECX,dword ptr [EAX+0x4];
 *   MOV EDX,dword ptr [EAX] -- three DWORD loads, +0x8 first because the +0x0
 *   load overwrites EAX (the record pointer itself). MSVC's right-to-left
 *   cdecl argument evaluation reproduces that order from the natural call
 *   expression. Note EDX is reused across the +0x8 and +0x0 loads.
 *
 *   CALL 0x1b32d0 @0xbf247 pushes EDX(record+0x8), ECX(record+0x4),
 *   EDX(record+0x0) = cdecl reverse -> C order
 *   (record[0], record[1], (char *)record[2]) =
 *   (unit_handle, vehicle_handle, seat_name). The third field is a dword in
 *   the record and a char* in the callee's kb decl, so it is cast at the call
 *   site; the kb decl is left alone.
 *
 *   CALL 0xcbf80 @0xbf24f pushes 0x0 then ESI -> hs_return(thread_datum, 0).
 *   The script return value is the CONSTANT 0; unit_scripting_enter_vehicle
 *   is void. ONE merged ADD ESP,0x14 @0xbf254 cleans both calls (3 + 2 = 5
 *   dwords) -- the ARG_COUNT hazard on hs_return (cleanup=5 vs decl=2) is a
 *   false positive from that merged cleanup, exactly as on the twins.
 *
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap). kb.json decl corrected from
 *   void(void) to (int16_t, int, char).
 *
 * Callees (all cdecl, in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1b32d0 = unit_scripting_enter_vehicle(int unit_handle,
 *              int vehicle_handle, char *seat_name) -- void, already carries
 *              a correct 3-arg decl in kb.json
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf220(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    unit_scripting_enter_vehicle(record[0], record[1], (char *)record[2]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf260 @ 0x000bf260
 *
 * HaloScript builtin dispatcher, same family as FUN_000bf160/0xbf1a0/0xbf1e0/
 * 0xbf220 above: identical 3-parameter cdecl shape, identical
 * evaluate / NULL-check / worker / hs_return skeleton. The difference from the
 * neighbours is that this one's worker RETURNS A BYTE PREDICATE that is handed
 * back to the script, instead of the constant 0 the others return.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (one dword local at [EBP-4]);
 * PUSH ESI. No _chkstk, no FPU, no SEH, no local buffers. Epilog POP ESI;
 * MOV ESP,EBP; POP EBP; RET (no immediate -- caller cleans, cdecl).
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI (callee-saved, held live
 *                                          across CALL 0xcc560 and reused as
 *                                          hs_return arg1 -- do NOT source it
 *                                          from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   0xbf271 MOV dword ptr [EBP-4],0 -- the dword result slot is pre-zeroed
 *   BEFORE the evaluate call.
 *   CALL 0xbf278 -> 0xcc560 pushes EAX([EBP+0x10]), ESI([EBP+0xc]),
 *   ECX([EBP+0x8]) in cdecl reverse order -> C order (function_index,
 *   thread_datum, init); ADD ESP,0xc = 3 args, straight pass-through.
 *   TEST EAX,EAX / JZ 0xbf2a4 skips both remaining calls on a NULL record, so
 *   the return is a POINTER (dereferenced below) even though kb.json declares
 *   0xcc560 as returning `int` -- cast at the call site, as all twins do.
 *   The record is the evaluated-argument block, 3 dwords:
 *     +0x00 int    unit handle   (MOV EDX,[EAX])
 *     +0x04 char * seat name     (MOV ECX,[EAX+4])
 *     +0x08 int    object list   (MOV EDX,[EAX+8])
 *   CALL 0xbf28f -> 0x1a9c90 pushes EDX(+0x8), ECX(+0x4), EDX(+0x0) in cdecl
 *   reverse order -> C order (record[0], record[1], record[2]). Note the loads
 *   run high-offset-first because the +0x0 load overwrites EAX (the record
 *   pointer itself); MSVC's right-to-left cdecl argument evaluation reproduces
 *   that order from the call expression below.
 *   Result handling: MOV byte ptr [EBP-4],AL (BYTE store only, into the dword
 *   zeroed at 0xbf271), then MOV EAX,dword ptr [EBP-4] (full DWORD read) -- an
 *   explicit zero-extension through a stack slot, the same
 *   zero-init-then-narrow-store idiom as FUN_000be030 above; modeled with a
 *   union so the widened value is the zero-extended byte. A plain (int) cast
 *   of the signed `char` return would SIGN-extend and diverge.
 *   CALL 0xbf29c -> 0xcbf80 pushes EAX (the widened result) then ESI ->
 *   hs_return(thread_datum, value). The following ADD ESP,0x14 (20 bytes) is a
 *   SHARED cleanup for 0x1a9c90's 3 pushes plus hs_return's 2; any ARG_COUNT
 *   warning on 0xcbf80 ("cleanup=5 vs decl=2") is that merge -- hs_return
 *   really takes 2 args, do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 *   from `void(void)` to the 3-arg cdecl form.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a9c90 = FUN_001a9c90(int unit_handle, const char *seat_name,
 *              int object_list) -> char predicate in AL
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf260(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    int i;
    unsigned char b;
  } value;

  value.i = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.b = FUN_001a9c90(record[0], (const char *)record[1], record[2]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bf2b0 @ 0x000bf2b0
 *
 * HaloScript builtin dispatcher, the immediate twin of FUN_000bf260 above:
 * identical 3-parameter cdecl shape, identical evaluate / NULL-check / worker /
 * hs_return skeleton, identical byte-predicate-into-pre-zeroed-dword result
 * marshalling. The ONLY difference from the twin is the worker called
 * (0x1a9da0 unit_scripting_vehicle_test_seat instead of 0x1a9c90).
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (one dword local at [EBP-4]);
 * PUSH ESI. No _chkstk, no FPU, no SEH, no local buffers. RET carries no
 * immediate (caller cleans, cdecl).
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI (callee-saved, held live
 *                                          across all three calls and reused
 *                                          as hs_return arg1 -- do NOT source
 *                                          it from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   0xbf2c1 MOV dword ptr [EBP-4],0 -- the dword result slot is pre-zeroed
 *   BEFORE the evaluate call, between the argument pushes and the CALL.
 *   CALL 0xbf2c8 -> 0xcc560 pushes EAX([EBP+0x10]), ESI([EBP+0xc]),
 *   ECX([EBP+0x8]) in cdecl reverse order -> C order (function_index,
 *   thread_datum, init); ADD ESP,0xc at 0xbf2cd = 3 args, straight
 *   pass-through. TEST EAX,EAX / JZ 0xbf2f4 skips both remaining calls on a
 *   NULL record, so the return is a POINTER (dereferenced below) even though
 *   kb.json declares 0xcc560 as returning `int` -- cast at the call site, as
 *   every twin in this family does.
 *   The record is the evaluated-argument block, 3 dwords. All three loads are
 *   FULL DWORD MOVs -- no movzx/movsx, so unlike FUN_000bf1a0 (word field) and
 *   FUN_000bf1e0 (byte field) there is no narrowing load here:
 *     +0x00 int    vehicle index (MOV EDX,[EAX])
 *     +0x04 char * seat name     (MOV ECX,[EAX+4])
 *     +0x08 int    unit index    (MOV EDX,[EAX+8])
 *   CALL 0xbf2df -> 0x1a9da0 pushes EDX(+0x8), ECX(+0x4), EDX(+0x0) in cdecl
 *   reverse order -> C order (record[0], record[1], record[2]). The loads run
 *   high-offset-first because the +0x0 load overwrites EAX (the record pointer
 *   itself); MSVC's right-to-left cdecl argument evaluation reproduces that
 *   order from the call expression below.
 *   Result handling: MOV byte ptr [EBP-4],AL (BYTE store only, into the dword
 *   zeroed at 0xbf2c1), then MOV EAX,dword ptr [EBP-4] (full DWORD read) -- an
 *   explicit zero-extension through a stack slot. Modeled with a union so the
 *   widened value is the zero-extended byte; a plain (int) cast of the signed
 *   `char` return would SIGN-extend and diverge for results >= 0x80.
 *   CALL 0xbf2ec -> 0xcbf80 pushes EAX (the widened result) then ESI ->
 *   hs_return(thread_datum, value). The following ADD ESP,0x14 (20 bytes) at
 *   0xbf2f1 is a SHARED cleanup for 0x1a9da0's 3 pushes plus hs_return's 2;
 *   any ARG_COUNT warning on 0xcbf80 ("cleanup=5 vs decl=2") is that merge --
 *   hs_return really takes 2 args, do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 *   from `void(void)` to the 3-arg cdecl form.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a9da0 = unit_scripting_vehicle_test_seat(int vehicle_index,
 *              const char *seat_name, int unit_index) -> char predicate in AL
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf2b0(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    int i;
    unsigned char b;
  } value;

  value.i = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.b = unit_scripting_vehicle_test_seat(
      record[0], (const char *)record[1], record[2]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bf300 @ 0x000bf300
 *
 * HaloScript builtin dispatcher, same family as FUN_000bf110/0xbf160/0xbf1a0/
 * 0xbf1e0/0xbf2b0 above: identical 3-parameter cdecl shape, identical
 * evaluate / NULL-check / worker / hs_return skeleton, and the "worker return
 * discarded, script gets a CONSTANT 0" tail (like 0xbf160/0xbf1a0/0xbf1e0,
 * unlike the result-slot variant 0xbf2b0). The record here is TWO dwords and
 * the worker is unit_scripting_set_emotion_animation.
 *
 * cdecl frame, 0xbf300-0xbf335, 24 instructions: PUSH EBP; MOV EBP,ESP;
 * PUSH ESI. No local dword (no PUSH ECX in the prologue), no _chkstk, no FPU,
 * no SEH, no local buffers. RET carries no immediate (caller cleans, cdecl).
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI (the only callee-saved
 *                                          register; held live across the
 *                                          evaluate call and reused as
 *                                          hs_return arg1 -- do NOT source it
 *                                          from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   CALL 0xcc560 @0xbf310 pushes EAX([EBP+0x10]), ESI([EBP+0xc]),
 *   ECX([EBP+0x8]) in cdecl reverse order -> C order (function_index,
 *   thread_datum, init); ADD ESP,0xc = 3 args, straight pass-through.
 *   TEST EAX,EAX / JZ 0xbf333 skips BOTH remaining calls on a NULL record, so
 *   the 0xcc560 return is a POINTER that is dereferenced even though kb.json
 *   declares it as returning `int` -- cast at the call site, as every twin in
 *   this family does; the kb decl is left alone.
 *
 *   Record deref, TWO fields, BOTH FULL DWORD MOVs -- there is no MOVZX/MOVSX
 *   anywhere in the function, so unlike FUN_000bf1a0 (word field) and
 *   FUN_000bf1e0 (byte field) neither field is narrowed:
 *     +0x00 int    unit index      (MOV EAX,dword ptr [EAX]     @0xbf31f)
 *     +0x04 char * animation name  (MOV EDX,dword ptr [EAX+0x4] @0xbf31c)
 *   The +0x4 load runs BEFORE the +0x0 load because the +0x0 load overwrites
 *   EAX (the record pointer itself); MSVC's right-to-left cdecl argument
 *   evaluation reproduces that order from the call expression below.
 *
 *   CALL 0x1a9b30 @0xbf323 pushes EDX(+0x4) then EAX(+0x0) = cdecl reverse
 *   -> C order (record[0], record[1]). Nothing reads EAX afterwards, so the
 *   return value is discarded (0x1a9b30 is void).
 *
 *   CALL 0xcbf80 @0xbf32b pushes the immediate 0x0 then ESI ->
 *   hs_return(thread_datum, 0); the script return value is the CONSTANT 0,
 *   there is no result slot. ONE combined ADD ESP,0x10 @0xbf330 folds
 *   unit_scripting_set_emotion_animation's 2 dwords with hs_return's 2 -- any
 *   ARG_COUNT warning on 0xcbf80 ("cleanup=4 vs decl=2") is that merged
 *   cleanup, hs_return really takes 2 args, do NOT "fix" its decl.
 *
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 *   from `void(void)` to the 3-arg cdecl form.
 *
 * Callees (all cdecl, in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a9b30 = unit_scripting_set_emotion_animation(int unit_index,
 *              const char *animation_name) -- void, result discarded
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf300(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    unit_scripting_set_emotion_animation(record[0], (const char *)record[1]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf340 @ 0x000bf340
 *
 * HaloScript builtin dispatcher, same family as FUN_000bf110/0xbf160/0xbf1a0/
 * 0xbf1e0/0xbf2b0/0xbf300 above: identical 3-parameter cdecl shape, identical
 * evaluate / NULL-check / worker / hs_return skeleton, and the "worker return
 * discarded, script gets a CONSTANT 0" tail. The record here is a SINGLE dword
 * and the worker is the unported FUN_001b5500.
 *
 * cdecl frame, 0xbf340-0xbf372, 18 instructions: PUSH EBP; MOV EBP,ESP;
 * PUSH ESI. No local dword (no PUSH ECX in the prologue), no _chkstk, no FPU,
 * no SEH, no local buffers. RET carries no immediate (caller cleans, cdecl).
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI (the only callee-saved
 *                                          register; held live across the
 *                                          evaluate call and reused as
 *                                          hs_return arg1 -- do NOT source it
 *                                          from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   CALL 0xcc560 @0xbf350 pushes EAX([EBP+0x10]), ESI([EBP+0xc]),
 *   ECX([EBP+0x8]) in cdecl reverse order -> C order (function_index,
 *   thread_datum, init); ADD ESP,0xc @0xbf355 = 3 args, straight pass-through.
 *   TEST EAX,EAX / JZ 0xbf36f skips BOTH remaining calls on a NULL record, so
 *   the 0xcc560 return is a POINTER that is dereferenced even though kb.json
 *   declares it as returning `int` -- cast at the call site, as every twin in
 *   this family does; the kb decl is left alone.
 *
 *   Record deref, ONE field, a FULL DWORD MOV -- there is no MOVZX/MOVSX
 *   anywhere in the function, so unlike FUN_000bf1a0 (word field) and
 *   FUN_000bf1e0 (byte field) the field is not narrowed:
 *     +0x00 int   (MOV EDX,dword ptr [EAX] @0xbf35c)
 *
 *   CALL 0x1b5500 @0xbf35f is preceded by exactly one PUSH (EDX = record+0)
 *   and has NO ADD ESP of its own -- its 4 bytes of cleanup are folded into
 *   the single ADD ESP,0xc @0xbf36c that also covers hs_return's 2 dwords.
 *   kb.json declared 0x1b5500 as `void(void)`; that is the void-decl trap
 *   (lift-learnings 31) -- calling it argument-less from C would silently drop
 *   the record field (same class as the a10 FUN_000beb70 dropped-arg crash),
 *   so the decl was corrected to `void FUN_001b5500(int)`. Nothing reads EAX
 *   after the call, so its return value is discarded (it is void).
 *
 *   CALL 0xcbf80 @0xbf367 pushes the immediate 0x0 then ESI ->
 *   hs_return(thread_datum, 0); the script return value is the CONSTANT 0,
 *   there is no result slot. Any ARG_COUNT warning on 0xcbf80
 *   ("cleanup=3 vs decl=2") is the merged cleanup described above -- hs_return
 *   really takes 2 args, do NOT "fix" its decl.
 *
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg>. kb.json's decl was corrected from `void(void)` to the 3-arg cdecl
 *   form.
 *
 * Callees (all cdecl, in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1b5500 = FUN_001b5500(int) -- UNPORTED, void, result discarded
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf340(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_001b5500(record[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf4c0 @ 0x000bf4c0
 *
 * HaloScript builtin dispatcher, same family as FUN_000bf260/0xbf2b0/0xbf300/
 * 0xbf340 above: identical 3-parameter cdecl shape, identical evaluate /
 * NULL-check / worker / hs_return skeleton. The worker here is the already
 * ported vehicle_scripting_load_magic (0x1b3400) and its 16-bit AX result is
 * handed back to the script.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (ONE local dword, the result
 * slot at [EBP-4]); PUSH ESI. No _chkstk, no FPU, no SEH, no local buffers.
 * RET carries no immediate (caller cleans, cdecl).
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI (the only callee-saved
 *                                          register; held live across the
 *                                          evaluate call and reused as
 *                                          hs_return arg1 -- do NOT source it
 *                                          from the record)
 *   init            char     [EBP+0x10]  -> EAX
 *
 * Binary evidence:
 *   MOV dword ptr [EBP-4],0 runs BEFORE the evaluate call (the result slot is
 *   pre-zeroed even on the NULL-record path).
 *   CALL 0xcc560 pushes EAX([EBP+0x10]), ESI([EBP+0xc]), ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init);
 *   ADD ESP,0xc = 3 args, straight pass-through. TEST EAX,EAX / JZ skips both
 *   remaining calls on a NULL record, so the 0xcc560 return is a POINTER that
 *   is dereferenced below even though kb.json declares it as returning `int`
 *   -- cast at the call site, as every twin in this family does.
 *   The record is the evaluated-argument block, 3 dwords, all read with plain
 *   full-width MOVs (there is NO MOVZX/MOVSX anywhere on the record, unlike
 *   FUN_000bf1a0's word field or FUN_000bf1e0's byte field):
 *     +0x00 int  (MOV EDX,dword ptr [EAX])
 *     +0x04 int  (MOV ECX,dword ptr [EAX+0x4])
 *     +0x08 int  (MOV EDX,dword ptr [EAX+0x8])
 *   CALL 0x1b3400 pushes EDX(+0x8), ECX(+0x4), EDX(+0x0) in cdecl reverse
 *   order -> C order (record[0], record[1], record[2]). The loads run
 *   high-offset-first because the +0x0 load overwrites EAX (the record pointer
 *   itself); MSVC's right-to-left cdecl argument evaluation reproduces that
 *   order from the call expression below.
 *   Result handling: MOV word ptr [EBP-4],AX (WORD store only, into the dword
 *   zeroed before the evaluate call), then MOV EAX,dword ptr [EBP-4] (full
 *   DWORD read) -- an explicit zero-extension through a stack slot, the
 *   zero-init-then-narrow-store idiom of FUN_000be030/0xbf260 at word width;
 *   modeled with a union so the widened value is provably the zero-extended
 *   16 bits (the upper 16 bits of the slot are 0).
 *   CALL 0xcbf80 pushes EAX (the widened result) then ESI ->
 *   hs_return(thread_datum, value). The following ADD ESP,0x14 (20 bytes) is a
 *   SHARED cleanup for 0x1b3400's 3 pushes plus hs_return's 2; any ARG_COUNT
 *   warning on 0xcbf80 ("cleanup=5 vs decl=2") is that merge -- hs_return
 *   really takes 2 args, do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params showed up as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 *   from `void(void)` to the 3-arg cdecl form.
 *
 * Callees (all cdecl, in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1b3400 = vehicle_scripting_load_magic(int vehicle_handle,
 *              int seat_substring, int group_handle) -> uint16_t in AX
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf4c0(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    int i;
    unsigned short w;
  } value;

  value.i = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.w = vehicle_scripting_load_magic(record[0], record[1], record[2]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bf510 @ 0xbf510 -- HS macro-function wrapper (2-argument variant).
 * Same idiom as FUN_000bf4c0 directly above, one record field narrower.
 *
 * Binary evidence (cachebeta.xbe, 0xbf510..0xbf555):
 *   Frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (one dword local at EBP-4);
 *   PUSH ESI. No _chkstk. Params are stack args at EBP+8 (int16
 *   function_index), EBP+0xC (int thread_datum, held in ESI across the whole
 *   body) and EBP+0x10 (char init).
 *   MOV dword ptr [EBP-4],0 is emitted BEFORE the first CALL -- the local is
 *   fully zeroed up front, which is what makes the later WORD store a
 *   zero-extension rather than a partial update. Preserve that order.
 *   CALL 0xcc560 pushes EAX(init), ESI(thread_datum), ECX(function_index) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init);
 *   ADD ESP,0xC. TEST EAX,EAX; JZ end -- EAX is an evaluation-record POINTER,
 *   not a value, proven by the following dereferences:
 *     +0x00 int (MOV EAX,dword ptr [EAX])
 *     +0x04 int (MOV EDX,dword ptr [EAX+0x4])
 *   CALL 0x1b5400 pushes EDX(+0x4) then EAX(+0x0) in cdecl reverse order ->
 *   C order (record[0], record[1]). The +0x4 load runs first because the
 *   +0x0 load overwrites EAX (the record pointer itself); MSVC's right-to-left
 *   cdecl argument evaluation reproduces that order from the call expression.
 *   Result handling: MOV word ptr [EBP-4],AX (WORD store only, into the dword
 *   zeroed above), then MOV ECX,dword ptr [EBP-4] (full DWORD read) -- an
 *   explicit zero-extension through the stack slot; modeled with a union so
 *   the widened value is provably the zero-extended low 16 bits.
 *   CALL 0xcbf80 pushes ECX (widened result) then ESI ->
 *   hs_return(thread_datum, value). The following ADD ESP,0x10 (16 bytes) is a
 *   SHARED cleanup for 0x1b5400's 2 pushes plus hs_return's 2; any ARG_COUNT
 *   warning on 0xcbf80 ("cleanup=4 vs decl=2") is that merge -- hs_return
 *   really takes 2 args, do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params appeared as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 *   from `void(void)` to the 3-arg cdecl form, and 0x1b5400's decl from
 *   `void(void)` to the 2-arg / uint16_t-return form the call site proves
 *   (check_arg_counts: single site, push=2; check_stdcall_ret: plain RET, so
 *   cdecl).
 *
 * Callees (all cdecl, in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1b5400 = FUN_001b5400(int, int) -> uint16_t in AX (unported; lives in
 *              the vehicle-scripting address neighbourhood alongside
 *              0x1b3400/vehicle_* but its own semantics are unproven, so the
 *              mechanical FUN_ name is kept)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf510(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    int i;
    unsigned short w;
  } value;

  value.i = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.w = FUN_001b5400(record[0], record[1]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bf560 @ 0xbf560 -- HS macro-function wrapper, single-string-argument
 * variant. Structurally the same skeleton as FUN_000bf160 above: evaluate the
 * script arguments, and on a non-NULL argument record pass ONE record field to
 * a worker, then return a CONSTANT 0 to the script thread. There is no result
 * slot -- the worker returns void and hs_return's second argument is an
 * immediate PUSH 0x0.
 *
 * Binary evidence (cachebeta.xbe, 0xbf560..0xbf591, 22 instructions):
 *   Frame: PUSH EBP; MOV EBP,ESP; PUSH ESI. NO local dword (no `PUSH ECX` in
 *   the prologue, unlike FUN_000bf4c0/FUN_000bf510), no _chkstk, no FPU, no
 *   SEH, no local buffers. ESI is the only callee-saved register and it holds
 *   thread_datum live across the evaluate call. RET carries no immediate
 *   (cdecl, caller-cleaned).
 *     function_index  int16_t  [EBP+0x08]  -> ECX
 *     thread_datum    int      [EBP+0x0c]  -> ESI, reused directly as
 *                                            hs_return arg1 (it is NOT
 *                                            re-sourced from the record)
 *     init            char     [EBP+0x10]  -> EAX
 *   CALL 0xcc560 pushes EAX(init), ESI(thread_datum), ECX(function_index) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init);
 *   ADD ESP,0xC = 3 args. TEST EAX,EAX; JZ 0xbf58f skips BOTH remaining calls
 *   on a NULL record, so EAX is an evaluation-record POINTER, not a value.
 *   Record deref is a SINGLE field: MOV EDX,dword ptr [EAX] -> record+0, a
 *   full 32-bit dword. There is no movzx/movsx anywhere, so unlike
 *   FUN_000bf1a0 (word field) and FUN_000bf1e0 (byte field) there is no
 *   narrowing load here, and unlike the FUN_000bf160 twin there is no second
 *   (+0x4 byte) field either.
 *   PUSH EDX; CALL 0x1ae730 -> scripting_set_magic_base_seat(record[0]) with
 *   ONE argument; the callee's kb decl proves the parameter is a `const char *`
 *   (string pointer), so the dword is cast at the call site.
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0). The second
 *   argument is an IMMEDIATE 0, not a result slot -- there is no local result
 *   dword in this frame at all.
 *   ONE combined ADD ESP,0xc at 0xbf58c folds scripting_set_magic_base_seat's
 *   1 dword with hs_return's 2; any ARG_COUNT warning on 0xcbf80
 *   ("cleanup=12 vs decl=2") is that merge -- hs_return really takes 2 args,
 *   do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params appeared as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 *   from `void(void)` to the 3-arg cdecl form as part of this lift.
 *
 * Callees (all cdecl, in kb.json, no @<reg> args anywhere; the delinked
 * reference carries exactly one DISP32 reloc for each):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *              (kb decl says `int`, but the call site dereferences the result;
 *              cast at the call site, as every twin above does)
 *   0x1ae730 = scripting_set_magic_base_seat(const char *)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf560(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    scripting_set_magic_base_seat((const char *)record[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf5a0 @ 0xbf5a0 -- HS macro-function wrapper, two-argument
 * (unit + seat-name) variant. Same skeleton as FUN_000bf560 above: evaluate
 * the script arguments, and on a non-NULL argument record hand TWO record
 * fields to a worker, then return a CONSTANT 0 to the script thread. The
 * worker returns void, so hs_return's second argument is an immediate PUSH
 * 0x0 -- there is no result slot in this frame.
 *
 * Binary evidence (cachebeta.xbe, 0xbf5a0..0xbf5d5, 26 instructions):
 *   Frame: PUSH EBP; MOV EBP,ESP; PUSH ESI. No local dword (no `PUSH ECX` in
 *   the prologue), no _chkstk, no FPU, no SEH, no local buffers. ESI is the
 *   only callee-saved register and it holds thread_datum live across the
 *   evaluate call. Epilogue POP ESI; POP EBP; RET with NO immediate (cdecl,
 *   caller-cleaned).
 *     function_index  int16_t  [EBP+0x08]  -> ECX  (callee uses it as int16_t)
 *     thread_datum    int      [EBP+0x0c]  -> ESI, reused directly as
 *                                            hs_return arg1 (it is NOT
 *                                            re-sourced from the record)
 *     init            char     [EBP+0x10]  -> EAX
 *   CALL 0xcc560 @0xbf5b0 pushes EAX(init), ESI(thread_datum), ECX
 *   (function_index) in cdecl reverse order -> C order (function_index,
 *   thread_datum, init); ADD ESP,0xC = 3 stack args. TEST EAX,EAX; JZ 0xbf5d3
 *   skips BOTH remaining calls on a NULL record, so EAX is an
 *   evaluation-record POINTER, not a value.
 *   Record deref is TWO FULL DWORD MOVs -- no movzx/movsx anywhere, so unlike
 *   FUN_000bf1a0 (word field) and FUN_000bf1e0 (byte field) there is no
 *   narrowing load here:
 *     MOV EDX,dword ptr [EAX+0x4]   -> record+4 (loaded FIRST)
 *     MOV EAX,dword ptr [EAX]       -> record+0
 *   PUSH EDX; PUSH EAX; CALL 0x1ae750 -- the +4 field is pushed FIRST, so in
 *   cdecl order it is the SECOND C argument: unit_scripting_set_seat(record[0],
 *   record[1]). The callee's kb decl proves arg1 is an `int unit_handle` and
 *   arg2 a `const char *seat_name`, so record+4 is a string pointer, cast at
 *   the call site.
 *   CALL 0xcbf80 @0xbf5cb pushes 0x0 then ESI -> hs_return(thread_datum, 0).
 *   The second argument is an IMMEDIATE 0, not a result slot.
 *   ONE combined ADD ESP,0x10 at 0xbf5d0 folds unit_scripting_set_seat's 2
 *   dwords with hs_return's 2; any ARG_COUNT warning on 0xcbf80
 *   ("cleanup=4 stack args vs decl=2") is that merge -- hs_return really takes
 *   2 args, do NOT "fix" its decl.
 *   Ghidra modelled this void(void), so the three cdecl params appeared as
 *   in_stack_00000004/8/c pseudo-locals (off by 4); they are stack args, not
 *   @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 *   from `void(void)` to the 3-arg cdecl form as part of this lift.
 *
 * Callees (all cdecl, in kb.json, no @<reg> args anywhere; the delinked
 * reference carries exactly one DISP32 reloc for each, in this order):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *              (kb decl says `int`, but the call site dereferences the result;
 *              cast at the call site, as every twin above does)
 *   0x1ae750 = unit_scripting_set_seat(int unit_handle, const char *seat_name)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf5a0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    unit_scripting_set_seat(record[0], (const char *)record[1]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf5e0 @ 0xbf5e0 -- HS script-function wrapper, zero-argument variant
 *   (10 instructions, bare EBP frame, no SUB ESP, no locals, no FPU).
 *
 * Signature (Confirmed by disassembly + family shape): the hs script function
 * dispatch table calls every entry as
 *   void (*)(int16_t function_index, int thread_datum, char init)
 * Only thread_datum ([EBP+0xc]) is read here:
 *   MOV EAX,[EBP+0xc]   -- plain full-width MOV, no MOVZX/MOVSX
 *   PUSH 0x0 / PUSH EAX -- cdecl reverse order -> hs_return(thread_datum, 0)
 *   ADD ESP,0x8         -- exactly 2 stack args, no merged cleanup here
 * function_index and init are never touched: this is a zero-argument script
 * function, so there is NO hs_macro_function_evaluate call, no argument
 * record, and consequently no NULL check (unlike the 1-/2-argument twins
 * above).  The 3-arg cdecl decl is still required so the table dispatch ABI
 * matches its twins.
 *
 * Ghidra modelled this void(void), so the cdecl params surfaced as
 * in_stack_00000008 (off by 4 => [EBP+0xc]); they are STACK args, not @<reg>
 * (lift-learnings 31 / void-decl trap).  kb.json's decl was corrected from
 * `void(void)` to the 3-arg cdecl form as part of this lift.
 *
 * Callees (both cdecl, in kb.json, no @<reg> args):
 *   0x1b2260 = scripting_magic_melee_attack(void)  -- no args, no ADD ESP
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf5e0(int16_t function_index, int thread_datum, char init)
{
  (void)function_index;
  (void)init;

  scripting_magic_melee_attack();
  hs_return(thread_datum, 0);
}

/* FUN_000bf600 @ 0xbf600 -- HS script-function wrapper, one-argument variant
 *   that RETURNS the callee's value (25 instructions; PUSH EBP / MOV EBP,ESP /
 *   PUSH ESI frame, no _chkstk, no SUB ESP, no locals, no FPU, no memory
 *   writes; POP ESI / POP EBP / RET with no immediate => plain cdecl).
 *
 * Signature (Confirmed by disassembly + family shape): the hs script function
 * dispatch table calls every entry as
 *   void (*)(int16_t function_index, int thread_datum, char init)
 *   [EBP+0x8]  -> ECX, function_index (int16_t)
 *   [EBP+0xc]  -> ESI, thread_datum; ESI is kept live across BOTH calls
 *   [EBP+0x10] -> EAX, init (char)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 * from `void(void)` to the 3-arg cdecl form as part of this lift.
 *
 * Call sites (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 @0xbf610 -- PUSH EAX (init) / PUSH ESI (thread_datum) /
 *   PUSH ECX (function_index); cdecl reverse order => the C order is
 *   (function_index, thread_datum, init). ADD ESP,0xc = exactly 3 stack args.
 *   TEST EAX,EAX / JZ exit is the NULL guard on the argument record.
 *
 *   CALL 0x1a9e40 @0xbf61f -- MOV EDX,[EAX] then PUSH EDX, i.e. the FULL DWORD
 *   at record+0. There is no MOVSX/MOVZX anywhere in the function, so unlike
 *   FUN_000bf1a0 (word field) and FUN_000bf1e0 (byte field) this argument is a
 *   plain int32 unit handle. One stack arg, cleaned by the merged ADD ESP
 * below.
 *
 *   CALL 0xcbf80 @0xbf626 -- PUSH EAX / PUSH ESI. The pushed EAX comes straight
 *   out of unit_scripting_unit_riders' return register with no zero/sign-extend
 *   and no temp spill, so it is the plain int result => the second argument of
 *   hs_return is that value, NOT an immediate 0 as in the void-valued twins
 *   above. Do not discard it (dropped-arg trap).
 *   ONE combined ADD ESP,0xc at 0xbf62b folds unit_scripting_unit_riders'
 * single dword with hs_return's two (4 + 8 = 12); any ARG_COUNT warning on
 * 0xcbf80
 *   ("cleanup=3 stack args vs decl=2") is that merge -- hs_return really takes
 *   2 args, do NOT "fix" its decl. Same pattern as FUN_000bf1a0.
 *
 * Nesting the riders call inside hs_return's argument list reproduces the
 * original order: MSVC evaluates/pushes right-to-left, so the inner CALL runs
 * first, its EAX is pushed, then ESI (thread_datum) is pushed.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere; the delinked
 * reference carries exactly one DISP32 reloc for FUN_001a9e40):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *              (kb decl says `int`, but the call site dereferences the result;
 *              cast at the call site, as every twin above does)
 *   0x1a9e40 = unit_scripting_unit_riders(int unit_handle) -> int
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf600(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    hs_return(thread_datum, unit_scripting_unit_riders(record[0]));
  }
}

/* FUN_000bf640 @ 0xbf640 -- HS script-function wrapper, one-argument variant
 *   that RETURNS the worker's value (23 instructions, 0xbf640-0xbf670;
 *   PUSH EBP / MOV EBP,ESP / PUSH ESI frame, no _chkstk, no SUB ESP, no
 *   locals, no FPU, no SEH, no memory writes; RET with no immediate =>
 *   plain cdecl, caller cleans).
 *
 * Structurally identical to FUN_000bf600 directly above; the only difference
 * is the worker called on the record's first dword (0x1a9ec0 here instead of
 * 0x1a9e40).
 *
 * Signature (Confirmed by disassembly + family shape): the hs script function
 * dispatch table calls every entry as
 *   void (*)(int16_t function_index, int thread_datum, char init)
 *   [EBP+0x8]  -> ECX, function_index (int16_t)
 *   [EBP+0xc]  -> ESI, thread_datum; ESI is the callee-saved register kept
 *                 live across the evaluate call and reused for hs_return
 *   [EBP+0x10] -> EAX, init (char)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 * from `void(void)` to the 3-arg cdecl form as part of this lift.
 *
 * Call sites (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 @0xbf650 -- PUSH EAX (init) / PUSH ESI (thread_datum) /
 *   PUSH ECX (function_index); cdecl reverse order => the C order is
 *   (function_index, thread_datum, init), a straight pass-through with no
 *   reordering. ADD ESP,0xc = exactly 3 stack args.
 *   TEST EAX,EAX / JZ 0xbf66e skips BOTH remaining calls, i.e. it is the NULL
 *   guard on the argument record; the result is a pointer even though kb.json
 *   declares hs_macro_function_evaluate as returning `int`, so it is cast
 *   locally here (do NOT change the kb decl) exactly as every twin above does.
 *
 *   CALL 0x1a9ec0 @0xbf65f -- MOV EDX,dword ptr [EAX] then PUSH EDX, i.e. the
 *   FULL DWORD at record+0. There is no MOVSX/MOVZX anywhere in the function,
 *   so unlike FUN_000bf1a0 (word field) and FUN_000bf1e0 (byte field) this
 *   argument is a plain int32 handle, and only that single field is read.
 *   One stack arg, cleaned by the merged ADD ESP below.
 *
 *   CALL 0xcbf80 @0xbf666 -- PUSH EAX / PUSH ESI. The pushed EAX is
 *   FUN_001a9ec0's return register, with no zero/sign-extend and no temp
 *   spill, so the second argument of hs_return is that value, NOT an
 *   immediate 0 as in the void-valued twins (0xbf1a0 / 0xbf1e0). Do not
 *   discard it (dropped-arg trap). thread_datum comes from ESI ([EBP+0xc]),
 *   not from the record.
 *   ONE combined ADD ESP,0xc at 0xbf66b folds FUN_001a9ec0's single dword with
 *   hs_return's two (4 + 8 = 12); the ARG_COUNT warning on 0xcbf80
 *   ("cleanup=3 stack args vs decl=2") is that merge -- hs_return really takes
 *   2 args, do NOT "fix" its decl. Same pattern as FUN_000bf600.
 *
 * Nesting the worker call inside hs_return's argument list reproduces the
 * original order: MSVC evaluates/pushes right-to-left, so the inner CALL runs
 * first, its EAX is pushed, then ESI (thread_datum) is pushed.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a9ec0 = FUN_001a9ec0(int unit_handle) -> int  (unnamed in kb.json;
 *              the parameter name is kb's, the semantics are Uncertain)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf640(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    hs_return(thread_datum, FUN_001a9ec0(record[0]));
  }
}

/* FUN_000bf680 @ 0xbf680 -- HS script-function wrapper, one-argument variant
 *   that RETURNS the worker's value (23 instructions, 0xbf680-0xbf6b0;
 *   PUSH EBP / MOV EBP,ESP / PUSH ESI frame, no _chkstk, no SUB ESP, no
 *   locals, no FPU, no SEH, no memory writes; RET with no immediate =>
 *   plain cdecl, caller cleans).
 *
 * Structurally identical to FUN_000bf640 directly above; the only difference
 * is the worker called on the record's first dword (0x1a9ef0 here instead of
 * 0x1a9ec0). Do NOT copy the `hs_return(thread_datum, 0)` tail of the
 * void-valued twins (0xbf1a0 / 0xbf1e0) -- this variant forwards the worker's
 * return value.
 *
 * Signature (Confirmed by disassembly + family shape): the hs script function
 * dispatch table calls every entry as
 *   void (*)(int16_t function_index, int thread_datum, char init)
 *   [EBP+0x8]  -> ECX, function_index (int16_t)
 *   [EBP+0xc]  -> ESI, thread_datum; ESI is the callee-saved register kept
 *                 live across the evaluate call and reused for hs_return
 *   [EBP+0x10] -> EAX, init (char)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> (lift-learnings 31 / void-decl trap). kb.json's decl was corrected
 * from `void(void)` to the 3-arg cdecl form as part of this lift.
 *
 * Call sites (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 @0xbf690 -- PUSH EAX (init) / PUSH ESI (thread_datum) /
 *   PUSH ECX (function_index); cdecl reverse order => the C order is
 *   (function_index, thread_datum, init), a straight pass-through with no
 *   reordering. ADD ESP,0xc = exactly 3 stack args.
 *   TEST EAX,EAX / JZ 0xbf6ae skips BOTH remaining calls, i.e. it is the NULL
 *   guard on the argument record; the result is a pointer even though kb.json
 *   declares hs_macro_function_evaluate as returning `int`, so it is cast
 *   locally here (do NOT change the kb decl) exactly as every twin above does.
 *
 *   CALL 0x1a9ef0 @0xbf69e -- MOV EDX,dword ptr [EAX] then PUSH EDX, i.e. the
 *   FULL DWORD at record+0. There is no MOVSX/MOVZX anywhere in the function,
 *   so unlike FUN_000bf1a0 (word field) and FUN_000bf1e0 (byte field) this
 *   argument is a plain int32 handle, and only that single field is read (no
 *   buffer-alias risk -- a single deref of one offset).
 *   One stack arg, cleaned by the merged ADD ESP below.
 *
 *   CALL 0xcbf80 @0xbf6a6 -- PUSH EAX / PUSH ESI. The pushed EAX is
 *   FUN_001a9ef0's return register, with no zero/sign-extend and no temp
 *   spill, so the second argument of hs_return is that value, NOT an
 *   immediate 0 as in the void-valued twins. Do not discard it (dropped-arg
 *   trap). thread_datum comes from ESI ([EBP+0xc]), not from the record.
 *   ONE combined ADD ESP,0xc at 0xbf6ab folds FUN_001a9ef0's single dword with
 *   hs_return's two (4 + 8 = 12); the ARG_COUNT warning on 0xcbf80
 *   ("cleanup=3 stack args vs decl=2") is that merge -- hs_return really takes
 *   2 args, do NOT "fix" its decl. Same pattern as FUN_000bf640/0xbf600.
 *
 * Nesting the worker call inside hs_return's argument list reproduces the
 * original order: MSVC evaluates/pushes right-to-left, so the inner CALL runs
 * first, its EAX is pushed, then ESI (thread_datum) is pushed.
 *
 * Callees (all cdecl, all in kb.json, all ported, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a9ef0 = FUN_001a9ef0(int unit_handle) -> int  (unnamed in kb.json;
 *              the parameter name is kb's, the semantics are Uncertain)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf680(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    hs_return(thread_datum, FUN_001a9ef0(record[0]));
  }
}

/* FUN_000bf6c0 @ 0xbf6c0 -- HS script-function wrapper, one-argument variant
 *   whose worker returns a FLOAT (26 instructions, 0xbf6c0-0xbf6f8; PUSH EBP /
 *   MOV EBP,ESP / PUSH ECX / PUSH ESI frame, no _chkstk, no SUB ESP, no SEH;
 *   POP ESI / MOV ESP,EBP / POP EBP / RET with no immediate => plain cdecl,
 *   caller cleans).  The prologue `PUSH ECX` is not an argument -- it reserves
 *   the single 4-byte local at [EBP-0x4] used to spill ST(0).
 *
 * Same evaluate / NULL-check / worker / hs_return skeleton as the
 * 0xbf110 / 0xbf160 / 0xbf1a0 / 0xbf1e0 / 0xbf600 / 0xbf640 / 0xbf680 family
 * above.  Two differences from those twins:
 *   (a) the worker (0x1a7cc0) returns a float in ST(0), not an int in EAX;
 *   (b) hs_return's second argument is that float's RAW BIT PATTERN, not a
 *       converted integer and not an immediate 0.
 *
 * Signature (Confirmed by disassembly + family shape): the hs script function
 * dispatch table calls every entry as
 *   void (*)(int16_t function_index, int thread_datum, char init)
 *   [EBP+0x8]  -> ECX, function_index (int16_t)
 *   [EBP+0xc]  -> ESI, thread_datum; ESI is the callee-saved register kept
 *                 live across the evaluate call and reused for hs_return
 *   [EBP+0x10] -> EAX, init (char, loaded as a dword)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> (lift-learnings 31 / void-decl trap).  kb.json's decl was corrected
 * from `void(void)` to the 3-arg cdecl form as part of this lift.
 *
 * Call sites (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 @0xbf6d0 -- PUSH EAX (init) / PUSH ESI (thread_datum) /
 *   PUSH ECX (function_index); cdecl reverse order => the C order is
 *   (function_index, thread_datum, init), a straight pass-through.
 *   ADD ESP,0xc = exactly 3 stack args.  TEST EAX,EAX / JZ 0xbf6f4 skips BOTH
 *   remaining calls, i.e. it is the NULL guard on the argument record; the
 *   result is a pointer even though kb.json declares
 *   hs_macro_function_evaluate as returning `int`, so it is cast locally here
 *   (do NOT change the kb decl), exactly as every twin above does.
 *
 *   CALL 0x1a7cc0 @0xbf6de -- MOV EDX,dword ptr [EAX] then PUSH EDX, i.e. the
 *   FULL DWORD at record+0.  There is no MOVSX/MOVZX anywhere in the function,
 *   so unlike FUN_000bf1a0 (word field) and FUN_000bf1e0 (byte field) this
 *   argument is a plain int32 handle, and only that ONE field of the record is
 *   read (single deref of one offset -- no buffer-alias risk).
 *
 *   CALL 0xcbf80 @0xbf6ec -- FLOAT-SMUGGLING (lift-learnings 6).  The
 *   disassembly does FSTP dword ptr [EBP-0x4] / MOV EAX,[EBP-0x4] / PUSH EAX:
 *   the float is stored to the local slot and re-loaded as a DWORD, so what
 *   reaches hs_return is the IEEE-754 BIT PATTERN, not `(int)f`.  Ghidra's
 *   `(int)fVar2` is wrong -- a numeric cast would truncate (0.75 -> 0).  Hence
 *   the `*(int *)&value` punning below; it also forces the same spill+reload
 *   the original emits.  Push order EAX then ESI => hs_return(thread_datum,
 *   bits).  ONE combined ADD ESP,0xc at 0xbf6f1 folds hs_return's two args (8)
 *   with the prologue's PUSH ECX local slot (4); the ARG_COUNT warning on
 *   0xcbf80 ("cleanup=3 stack args vs decl=2") is that merge -- hs_return
 *   really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, all ported, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a7cc0 = FUN_001a7cc0(int datum_handle) -> float  (unnamed in kb.json,
 *              implemented in src/halo/units/units.c; the parameter name is
 *              kb's, the semantics are Uncertain)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf6c0(int16_t function_index, int thread_datum, char init)
{
  int *record;
  float value;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value = FUN_001a7cc0(record[0]);
    hs_return(thread_datum, *(int *)&value);
  }
}

/* FUN_000bf700 @ 0xbf700 -- HS script-function wrapper, one-argument variant
 *   whose worker returns a FLOAT.  Structurally identical to FUN_000bf6c0
 *   directly above; the ONLY difference is the worker address (0x1a7d00 here
 *   vs 0x1a7cc0 there).  Frame: PUSH EBP / MOV EBP,ESP / PUSH ECX / PUSH ESI,
 *   no _chkstk, no SUB ESP, no SEH; plain RET with no immediate => cdecl,
 *   caller cleans.  The prologue `PUSH ECX` is not an argument -- it reserves
 *   the single 4-byte local at [EBP-0x4] used to spill ST(0).
 *
 * Signature (Confirmed by disassembly + family shape): the hs script function
 * dispatch table calls every entry as
 *   void (*)(int16_t function_index, int thread_datum, char init)
 *   [EBP+0x8]  -> ECX, function_index (int16_t)
 *   [EBP+0xc]  -> ESI, thread_datum; ESI is the callee-saved register kept
 *                 live across the evaluate call and reused for hs_return
 *   [EBP+0x10] -> EAX, init (char, loaded as a dword)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> (no unaff_/in_EAX/in_ECX appears -- lift-learnings 31 void-decl trap).
 * kb.json's decl was corrected from `void(void)` to the 3-arg cdecl form as
 * part of this lift.
 *
 * Call sites (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 @0xbf710 -- PUSH EAX (init) / PUSH ESI (thread_datum) /
 *   PUSH ECX (function_index); cdecl reverse order => the C order is
 *   (function_index, thread_datum, init), a straight pass-through.
 *   ADD ESP,0xc = exactly 3 stack args.  TEST EAX,EAX / JZ skips BOTH
 *   remaining calls, i.e. it is the NULL guard on the argument record; the
 *   result is a pointer even though kb.json declares
 *   hs_macro_function_evaluate as returning `int`, so it is cast locally here
 *   (do NOT change the kb decl), exactly as every twin above does.
 *
 *   CALL 0x1a7d00 @0xbf720 -- MOV EDX,dword ptr [EAX] then PUSH EDX, i.e. the
 *   FULL DWORD at record+0.  There is no MOVSX/MOVZX anywhere in the function,
 *   so unlike FUN_000bf1a0 (word field) and FUN_000bf1e0 (byte field) this
 *   argument is a plain int32 handle, and only that ONE field of the record is
 *   read (single deref of one offset -- no buffer-alias risk).
 *
 *   CALL 0xcbf80 @0xbf72d -- FLOAT-SMUGGLING (lift-learnings 6).  The
 *   disassembly does FSTP dword ptr [EBP-0x4] / MOV EAX,[EBP-0x4] / PUSH EAX:
 *   the float is stored to the local slot and re-loaded as a DWORD, so what
 *   reaches hs_return is the IEEE-754 BIT PATTERN, not `(int)f`.  Ghidra's
 *   `(int)fVar2` is wrong -- a numeric cast would truncate (0.75 -> 0), and no
 *   FISTP / _ftol2 appears anywhere.  Hence the `*(int *)&value` punning below;
 *   it also forces the same spill+reload the original emits.  Push order EAX
 *   then ESI => hs_return(thread_datum, bits).  ONE combined ADD ESP,0xc at
 *   0xbf732 folds hs_return's two args (8) with the single arg still on the
 *   stack from the 0x1a7d00 call (4, never individually cleaned); the
 *   ARG_COUNT warning on 0xcbf80 ("cleanup=3 stack args vs decl=2") is that
 *   merge -- hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a7d00 = FUN_001a7d00(int datum_handle) -> float  (unnamed in kb.json;
 *              the parameter name is kb's, the semantics are Uncertain)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf700(int16_t function_index, int thread_datum, char init)
{
  int *record;
  float value;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value = FUN_001a7d00(record[0]);
    hs_return(thread_datum, *(int *)&value);
  }
}

/* FUN_000bf740 @ 0x000bf740 -- HS script-function wrapper, one-argument
 *   variant that returns the worker's value truncated to 16 bits
 *   (0xbf740-0xbf781, 66 bytes, 3 CALLs, zero FPU).
 *
 * Structurally the same three-call family shape as FUN_000bf700 directly
 * above, with two differences: the worker at 0x1a7d40 returns an INTEGER in
 * EAX (not a float in ST(0)), and only its low 16 bits reach hs_return.
 *
 * Frame: PUSH EBP / MOV EBP,ESP / PUSH ECX / PUSH ESI; no _chkstk, no
 *   SUB ESP, no SEH; exit is POP ESI / MOV ESP,EBP / POP EBP / RET with no
 *   immediate => cdecl, caller cleans.  The prologue `PUSH ECX` is not an
 *   argument -- it reserves the single 4-byte local at [EBP-0x4].
 *
 * Signature (Confirmed by disassembly + family shape):
 *   [EBP+0x8]  -> ECX, function_index (int16_t)
 *   [EBP+0xc]  -> ESI, thread_datum; ESI is the callee-saved register kept
 *                 live across the evaluate call and reused for hs_return
 *   [EBP+0x10] -> EAX, init (char, loaded as a dword)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> -- no unaff_/in_EAX/in_ECX appears (lift-learnings 31 void-decl
 * trap).  kb.json's decl was corrected from `void(void)` to the 3-arg cdecl
 * form as part of this lift.
 *
 * Call sites (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 @0xbf758 -- PUSH EAX (init) / PUSH ESI (thread_datum) /
 *   PUSH ECX (function_index); cdecl reverse order => the C order is
 *   (function_index, thread_datum, init), a straight pass-through.
 *   ADD ESP,0xc = exactly 3 stack args.  TEST EAX,EAX / JZ skips BOTH
 *   remaining calls, i.e. it is the NULL guard on the evaluation record; the
 *   result is a pointer even though kb.json declares
 *   hs_macro_function_evaluate as returning `int`, so it is cast locally here
 *   (do NOT change the kb decl), exactly as every twin above does.
 *
 *   CALL 0x1a7d40 @0xbf767 -- MOV EDX,dword ptr [EAX] then PUSH EDX, i.e. the
 *   FULL DWORD at record+0.  There is no MOVSX/MOVZX anywhere in the function,
 *   so unlike FUN_000bf1a0 (word field) and FUN_000bf1e0 (byte field) this
 *   argument is a plain int32 handle, and only that ONE field of the record is
 *   read (single deref of one offset -- no buffer-alias risk).
 *
 *   The 16-bit truncation: `MOV dword ptr [EBP-0x4],0x0` at 0xbf751 zeroes the
 *   local BEFORE the evaluate call, then after the worker returns
 *   `MOV word ptr [EBP-0x4],AX` / `MOV EAX,dword ptr [EBP-0x4]` stores only the
 *   low WORD of the integer return into that pre-zeroed dword and reloads the
 *   whole dword.  Net effect is a zero-extension of the low 16 bits.  That
 *   narrow-store-into-pre-zeroed-dword shape is a UNION in the original source,
 *   not a mask: writing it as the flat `value = value & 0xffff;` of
 *   FUN_000be6a0 collapses all three memory accesses into a single
 *   `AND EAX,0xffff` and scores 88.5% (24/28 insns), whereas the union below
 *   reproduces the zero / word-store / dword-reload triple exactly and scores
 *   100.0% (28/28).  Both forms are bit-identical at runtime; only the union
 *   matches codegen.  Same union idiom as FUN_000be620 above.
 *
 *   This is an INTEGER path -- kb declares 0x1a7d40 as returning `int`, the
 *   value arrives in AX/EAX, and there is no FSTP anywhere, so it is NOT the
 *   float bit-smuggling case that FUN_000bf700 directly above has
 *   (lift-learnings 6); the union here is a width pun, not a type pun.
 *
 *   CALL 0xcbf80 @0xbf775 -- PUSH EAX (masked value) / PUSH ESI
 *   (thread_datum) => hs_return(thread_datum, value).  ONE combined
 *   ADD ESP,0xc at 0xbf77a folds hs_return's two args (8) with the single arg
 *   still on the stack from the 0x1a7d40 call (4, never individually cleaned);
 *   the ARG_COUNT warning on 0xcbf80 ("cleanup=3 stack args vs decl=2") is
 *   that merge -- hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a7d40 = FUN_001a7d40(int datum_handle) -> int  (unnamed in kb.json;
 *              the parameter name is kb's, the semantics are Uncertain)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf740(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    unsigned short w;
    int i;
  } value;

  value.i = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.w = (unsigned short)FUN_001a7d40(record[0]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bf790 @ 0x000bf790 -- HS script-function wrapper, two-argument
 *   predicate variant that returns the worker's byte result to the script
 *   engine.
 *
 * Structurally the same three-call family shape as the twins above; the
 * distinguishing details are (a) TWO record fields are read, both as FULL
 * DWORDs, and (b) the worker returns a byte in AL that is zero-extended
 * through a pre-zeroed dword local.
 *
 * Frame: PUSH EBP / MOV EBP,ESP / PUSH ECX / PUSH ESI; no _chkstk, no
 *   SUB ESP, no SEH, no FPU; exit RET has no immediate => cdecl, caller
 *   cleans.  The prologue `PUSH ECX` is not an argument -- it reserves the
 *   single 4-byte local at [EBP-0x4].
 *
 * Signature (Confirmed by disassembly + family shape):
 *   [EBP+0x8]  -> ECX, function_index (int16_t)
 *   [EBP+0xc]  -> ESI, thread_datum; ESI is the callee-saved register kept
 *                 live across the evaluate call and reused for hs_return
 *   [EBP+0x10] -> EAX, init (char, loaded as a dword)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> -- no unaff_/in_EAX/in_ECX appears (lift-learnings 31 void-decl
 * trap).  kb.json's decl was corrected from `void(void)` to the 3-arg cdecl
 * form as part of this lift.
 *
 * Call sites (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 -- PUSH EAX (init) / PUSH ESI (thread_datum) / PUSH ECX
 *   (function_index), then ADD ESP,0xc: cdecl reverse order => the C order is
 *   (function_index, thread_datum, init), a straight pass-through of exactly
 *   3 stack args.  TEST EAX,EAX / JZ 0xbf7d0 skips BOTH remaining calls, i.e.
 *   it is the NULL guard on the evaluation record.  The result is a POINTER
 *   even though kb.json declares hs_macro_function_evaluate as returning
 *   `int`, so it is cast locally here (do NOT change the kb decl), exactly as
 *   every twin above does.
 *
 *   CALL 0x1a7e70 -- MOV EDX,dword ptr [EAX+0x4] is issued BEFORE
 *   MOV EAX,dword ptr [EAX] overwrites the record pointer, then PUSH EDX /
 *   PUSH EAX => the C order is (record[0], record[1]).  MSVC's right-to-left
 *   cdecl argument evaluation reproduces that +0x4-then-+0x0 load order from
 *   the source form below.  BOTH fields are full dwords -- there is no
 *   MOVZX/MOVSX anywhere in the function, so unlike FUN_000bf1a0 (word field)
 *   and FUN_000bf1e0 (byte field) neither argument is narrowed
 *   (lift-learnings 24 LOADW).  Only offsets +0x0 and +0x4 of the record are
 *   touched, both via a single deref each -- no buffer-alias risk.
 *
 *   The byte return: `MOV dword ptr [EBP-0x4],0x0` zeroes the local before the
 *   evaluate call, then after the worker returns `MOV byte ptr [EBP-0x4],AL` /
 *   MOV ECX,dword ptr [EBP-0x4] / PUSH ECX stores only AL into that pre-zeroed
 *   dword and reloads the whole dword.  Net effect is a zero-extension of the
 *   byte (uint8 -> int), NOT the sign-extension a plain
 *   `int value = FUN_001a7e70(...)` would produce from the `char` return, so
 *   the union width-pun below is required for both correctness and codegen
 *   (same idiom as FUN_000bf260 / FUN_000bf2b0 above).
 *
 *   CALL 0xcbf80 -- PUSH ECX (the zero-extended value) / PUSH ESI
 *   (thread_datum) => hs_return(thread_datum, value).  hs_return's first
 *   argument is the PARAMETER thread_datum held in ESI, not any record field.
 *   ONE combined ADD ESP,0x10 folds hs_return's two args (8) with the two args
 *   still on the stack from the 0x1a7e70 call (8, never individually cleaned);
 *   the ARG_COUNT warning on 0xcbf80 ("cleanup=4 stack args vs decl=2") and
 *   the "cleanup=none" report for 0x1a7e70 at this site are both that merge --
 *   hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a7e70 = FUN_001a7e70(int unit_handle, int definition_index) -> char
 *              (unnamed in kb.json; parameter names are kb's, the semantics
 *              are Uncertain)
 *   0xcbf80  = hs_return(int thread_handle, int value) */
void FUN_000bf790(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    int i;
    unsigned char b;
  } value;

  value.i = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.b = FUN_001a7e70(record[0], record[1]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bf7e0 @ 0x000bf7e0 -- HS script-function wrapper, two-argument
 *   predicate variant that returns the worker's byte result to the script
 *   engine.  Instruction-for-instruction the same shape as FUN_000bf790
 *   directly above; the ONLY difference is the worker called (0x1a7ea0
 *   instead of 0x1a7e70).
 *
 * Frame: PUSH EBP / MOV EBP,ESP / PUSH ECX / PUSH ESI; 31 instructions, no
 *   _chkstk, no SUB ESP, no SEH, no FPU; exit RET has no immediate => cdecl,
 *   caller cleans.  The prologue `PUSH ECX` is not an argument -- it reserves
 *   the single 4-byte local at [EBP-0x4].
 *
 * Signature (Confirmed by disassembly + family shape):
 *   [EBP+0x8]  -> ECX, function_index (int16_t per the callee decl)
 *   [EBP+0xc]  -> ESI, thread_datum; ESI is the callee-saved register kept
 *                 live across the evaluate call and reused for hs_return, and
 *                 it is never rewritten in the body (no register-aliasing
 *                 risk, lift-learnings 1)
 *   [EBP+0x10] -> EAX, init (char, loaded as a dword)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> -- no unaff_/in_EAX/in_ECX appears (lift-learnings 31 void-decl
 * trap).  kb.json's decl was corrected from `void(void)` to the 3-arg cdecl
 * form as part of this lift; 3 original call sites push into it.
 *
 * Call sites (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 @0xbf7f8 -- PUSH EAX (init) / PUSH ESI (thread_datum) /
 *   PUSH ECX (function_index), then ADD ESP,0xc: cdecl reverse order => the C
 *   order is (function_index, thread_datum, init), a straight pass-through of
 *   exactly 3 stack args.  `MOV dword ptr [EBP-0x4],0x0` executes between the
 *   pushes and the CALL, so the local is pre-zeroed before the call.
 *   TEST EAX,EAX / JZ 0xbf820 skips BOTH remaining calls, i.e. it is the NULL
 *   guard on the evaluation record -- the record is only dereferenced inside
 *   the guard.  The result is a POINTER even though kb.json declares
 *   hs_macro_function_evaluate as returning `int`, so it is cast locally here
 *   (do NOT change the kb decl), exactly as every twin above does.
 *
 *   CALL 0x1a7ea0 @0xbf80b -- MOV EDX,dword ptr [EAX+0x4] is issued BEFORE
 *   MOV EAX,dword ptr [EAX] overwrites the record pointer, then PUSH EDX /
 *   PUSH EAX => the C order is (record[0], record[1]).  MSVC's right-to-left
 *   cdecl argument evaluation reproduces that +0x4-then-+0x0 load order from
 *   the source form below.  BOTH fields are full dwords -- no MOVZX/MOVSX
 *   appears anywhere in the function, so unlike FUN_000bf1a0 (word field) and
 *   FUN_000bf1e0 (byte field) neither argument is narrowed; in particular
 *   record[0] must NOT be read as *(int16_t *)record the way FUN_000be080
 *   does (lift-learnings 24 LOADW).  Only offsets +0x0 and +0x4 of the record
 *   are touched, one deref each -- no buffer-alias risk.
 *
 *   The byte return: after the worker returns, `MOV byte ptr [EBP-0x4],AL` /
 *   MOV ECX,dword ptr [EBP-0x4] / PUSH ECX stores only AL into the pre-zeroed
 *   dword and reloads the whole dword.  Net effect is a zero-extension of the
 *   byte (uint8 -> int), NOT the sign-extension a plain
 *   `int value = FUN_001a7ea0(...)` would produce from the `char` return, so
 *   the union width-pun below is required for both correctness and codegen,
 *   and the `value.i = 0` pre-zero is load-bearing.
 *
 *   CALL 0xcbf80 @0xbf818 -- PUSH ECX (the zero-extended value) / PUSH ESI
 *   (thread_datum) => hs_return(thread_datum, value).  hs_return's first
 *   argument is the PARAMETER thread_datum held in ESI, not any record field.
 *   ONE combined ADD ESP,0x10 at 0xbf81d folds hs_return's two args (8) with
 *   the two args still on the stack from the 0x1a7ea0 call (8, never
 *   individually cleaned); the ARG_COUNT warning on 0xcbf80 ("cleanup=4 stack
 *   args vs decl=2") is that merge -- hs_return really takes 2 args, do NOT
 *   "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, ported, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a7ea0 = FUN_001a7ea0(int unit_handle, int weapon_def_tag) -> char
 *              (unnamed in kb.json; parameter names are kb's, the semantics
 *              are Uncertain)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bf7e0(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    int i;
    unsigned char b;
  } value;

  value.i = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.b = FUN_001a7ea0(record[0], record[1]);
    hs_return(thread_datum, value.i);
  }
}

/* 0xbf830 — HS script function handler: mark a unit as scripted so it does not
 * drop its items.
 *
 * Byte-shape twin of FUN_000befd0 above; the ONLY difference is the middle
 * callee (0x1a9c40 instead of 0x1af0d0). cdecl frame: PUSH EBP; MOV EBP,ESP;
 * PUSH ESI; ... POP ESI; POP EBP; RET (no RET immediate — caller cleans). No
 * locals, no _chkstk, no FPU ops anywhere in the body.
 *
 * Params from the EBP offsets (Ghidra modelled this `void(void)` and dropped
 * all three; the in_stack_* names in its output are the tell — they are STACK
 * params, NOT register args):
 *   function_index  int16_t  [EBP+0x08] -> ECX -> evaluate arg 1
 *   thread_datum    int      [EBP+0x0c] -> ESI (cached: used twice)
 *   init            char     [EBP+0x10] -> EAX -> evaluate arg 3
 *
 * CALL 0xcc560 @0xbf840 pushes EAX(init), ESI(thread_datum),
 * ECX(function_index) and cleans with ADD ESP,0xc — 3 stack args, so the C
 * order is (function_index, thread_datum, init). TEST EAX,EAX; JZ 0xbf85f is
 * the NULL guard on the returned result record.
 *
 * CALL 0x1a9c40 @0xbf84f is preceded by MOV EDX,dword ptr [EAX]; PUSH EDX — a
 * FULL 32-bit load of the record's first dword (result[0]), NOT the byte load
 * (XOR EDX,EDX; MOV DL,[EAX]) used by the 0xbdef0 sibling. Getting that width
 * wrong is the one real trap here. The callee is void, so nothing is read back.
 *
 * CALL 0xcbf80 @0xbf857 pushes 0x0 then ESI => hs_return(thread_datum, 0). The
 * committed value is a literal 0.
 *
 * The single trailing ADD ESP,0xc at 0xbf85c is MERGED cleanup for the 1 arg of
 * unit_scripting_doesnt_drop_items plus the 2 args of hs_return (1+2 = 3
 * dwords). The call-site audit's "hs_return cleanup=3 vs decl=2" is that same
 * false positive already documented on the 0xbefd0 twin. Do NOT "fix"
 * hs_return's decl.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560   = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *               char init) -> int* (result record, NULL on failure)
 *   0x1a9c40  = unit_scripting_doesnt_drop_items(int object_list)
 *   0xcbf80   = hs_return(int thread_handle, int value)
 *
 * Placed in hs.c rather than players.c (where kb groups 0xbf830) per the same
 * lift directive as the twins: players.c does not compile under VC71
 * (clang-only
 * __attribute__ / raw fnptr casts), so it would be permanently unmeasurable
 * there. */
void FUN_000bf830(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    unit_scripting_doesnt_drop_items(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf870 @ 0x000bf870
 *
 * HaloScript builtin dispatcher, structurally the SAME function as
 * FUN_000bf1e0 above: identical 3-parameter cdecl shape, identical
 * evaluate / NULL-check / worker / hs_return skeleton, identical
 * "worker takes (dword @ record+0, zero-extended BYTE @ record+4) and the
 * script gets a CONSTANT 0" tail. The only difference from FUN_000bf1e0 is
 * the worker called: 0x1a7d80 (units.obj) instead of 0x1ac030.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI. No local dword, no _chkstk,
 * no SUB ESP, no FPU, no SEH, no local buffers. ESI is the only callee-saved
 * register and holds thread_datum live across the evaluate call (never
 * rewritten in the body, so no register-aliasing risk -- lift-learnings 1).
 * The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *                                          (NOT sourced from the record)
 *   init            char     [EBP+0x10]  -> EAX (loaded as a dword)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> -- no unaff_/in_EAX/in_ECX appears (lift-learnings 31 void-decl
 * trap). kb.json's stale `void FUN_000bf870(void);` decl was corrected to the
 * 3-arg cdecl form as part of this lift; leaving a (void) decl over a
 * stack-arg callee is the ESP-drift class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init);
 *   ADD ESP,0xc = 3 args. Straight pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ <end> skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin above; the cast
 *   is local and the kb decl is left alone).
 *
 *   Record deref: XOR EDX,EDX; MOV DL,byte ptr [EAX+0x4] -> zero-extended
 *   BYTE at record+4; MOV EAX,dword ptr [EAX] -> DWORD at record+0. Ghidra
 *   rendered the byte as `piVar1[1]`, i.e. an int at +0x4 -- that width is
 *   WRONG (lift-learnings 24 LOADW); it is a single unsigned byte.
 *   Only offsets +0x0 and +0x4 are touched, one deref each -- no
 *   buffer-alias risk.
 *
 *   CALL 0x1a7d80 pushes EDX (the zero-extended byte) then EAX (the dword) =
 *   cdecl reverse -> C order (record[0], byte at record+4).
 *
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0). The
 *   script return value is the CONSTANT 0, not the evaluated record and not
 *   the worker's result (0x1a7d80 is void); the entire observable effect of
 *   this handler is the 0x1a7d80 side effect. ONE combined ADD ESP,0x10
 *   cleans the 4 pushes of both 2-arg calls -- the ARG_COUNT hazard reported
 *   on hs_return ("cleanup=4 stack args vs decl=2") is a FALSE POSITIVE from
 *   that merged cleanup; hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, all ported, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a7d80 = FUN_001a7d80(int datum_handle, char flag) -> void
 *              (units.obj, lifted in src/halo/units/units.c; parameter names
 *              are kb.json's, the semantics are Uncertain)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bf870(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_001a7d80(record[0], (char)*(unsigned char *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf8b0 @ 0x000bf8b0
 *
 * HaloScript builtin dispatcher, byte-shape twin of FUN_000bf870 directly
 * above (and of FUN_000bf160 / FUN_000bf1e0): identical 3-parameter cdecl
 * shape, identical evaluate / NULL-check / worker / hs_return skeleton,
 * identical "worker takes (dword @ record+0, zero-extended BYTE @ record+4)
 * and the script gets a CONSTANT 0" tail. The only difference from
 * FUN_000bf870 is the worker called: 0x1a9b80 unit_scripting_suspended
 * (units.obj) instead of 0x1a7d80.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI. 24 instructions total
 * (0xbf8b0-0xbf8e7). No local dword (no `PUSH ECX` slot, unlike 0xbf0b0),
 * no _chkstk, no SUB ESP, no FPU, no SEH, no local buffers. ESI is the only
 * callee-saved register and holds thread_datum live across the evaluate call
 * (never rewritten in the body, so no register-aliasing risk --
 * lift-learnings 1). The exit RET carries no immediate => cdecl, caller
 * cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *                                          (NOT sourced from the record)
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_00000004/8/c pseudo-locals (off by 4); they are STACK args, not
 * @<reg> -- no unaff_/in_EAX/in_ECX appears (lift-learnings 31 void-decl
 * trap). kb.json's stale `void FUN_000bf8b0(void);` decl was corrected to
 * the 3-arg cdecl form as part of this lift; leaving a (void) decl over a
 * stack-arg callee is the ESP-drift class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL in the disassembly):
 *   CALL 0xcc560 @0xbf8c0 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) /
 *   ECX([EBP+0x8]) in cdecl reverse order -> C order (function_index,
 *   thread_datum, init); ADD ESP,0xc = 3 args. Straight pass-through of all
 *   three params, no reordering.
 *
 *   TEST EAX,EAX / JZ 0xbf8e5 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin above; the cast
 *   is local and the kb decl is left alone).
 *
 *   Record deref (2 fields, 8 bytes used): MOV EAX,dword ptr [EAX] -> DWORD
 *   at record+0; XOR EDX,EDX; MOV DL,byte ptr [EAX+0x4] -> zero-extended
 *   BYTE at record+4. Ghidra rendered the byte as `piVar1[1]`, i.e. an int at
 *   +0x4 -- that width is WRONG (lift-learnings 24 LOADW); it is a single
 *   unsigned byte. Only offsets +0x0 and +0x4 are touched, one deref each --
 *   no buffer-alias risk.
 *
 *   CALL 0x1a9b80 @0xbf8d5 pushes EDX (the zero-extended byte) then EAX (the
 *   dword) = cdecl reverse -> C order (record[0], byte at record+4). EDX and
 *   EAX are distinct reloads, NOT a duplicated argument.
 *
 *   CALL 0xcbf80 @0xbf8dd pushes 0x0 then ESI -> hs_return(thread_datum, 0).
 *   The script return value is the CONSTANT 0, not the evaluated record and
 *   not the worker's result (unit_scripting_suspended is void); the entire
 *   observable effect of this handler is the 0x1a9b80 side effect. ONE
 *   combined ADD ESP,0x10 at 0xbf8e2 cleans the 4 pushes of both 2-arg calls
 *   -- the ARG_COUNT hazard reported on hs_return ("cleanup=4 stack args vs
 *   decl=2") is a FALSE POSITIVE from that merged cleanup; hs_return really
 *   takes 2 args, do NOT "fix" its decl. Epilogue: POP ESI; POP EBP; RET.
 *
 * Reloc audit of delinked/functions/000bf8b0.obj: exactly 3 DISP32 targets,
 * one each -- FUN_000cc560, FUN_001a9b80, FUN_000cbf80 -- matching the three
 * calls below with no extra global or string reference.
 *
 * Callees (all cdecl, all in kb.json, all ported, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1a9b80 = unit_scripting_suspended(int unit_index, char suspended)
 *              (units.obj; parameter names are kb.json's)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bf8b0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    unit_scripting_suspended(record[0],
                             (char)*(unsigned char *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf8f0 @ 0x000bf8f0
 *
 * HaloScript builtin dispatcher for the solo-player integrated night-vision
 * query. Same 3-parameter cdecl shape as the twins above (FUN_000bf870 /
 * FUN_000bf8b0 / FUN_000bf740), but with two structural differences that are
 * confirmed in the disassembly and must NOT be "normalised" to match them:
 *   1. There is NO hs_macro_function_evaluate call and NO NULL check. The
 *      worker is called unconditionally and its result is always committed.
 *   2. The worker takes no arguments, so none of the three parameters are
 *      forwarded; only thread_datum is read at all.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX => one 4-byte local dword at
 * [EBP-0x4]. 15 instructions total (0xbf8f0-0xbf916). No _chkstk, no SUB ESP,
 * no FPU, no SEH, no local buffers, and no callee-saved register is touched
 * (so no register-aliasing risk -- lift-learnings 1). The exit RET carries no
 * immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  UNUSED by the body
 *   thread_datum    int      [EBP+0x0c]  -> ECX, hs_return arg1
 *   init            char     [EBP+0x10]  UNUSED by the body
 * Ghidra modelled this void(void), so the params surfaced as
 * in_stack_00000008 pseudo-locals (off by 4); they are STACK args, not
 * @<reg> -- no unaff_/in_EAX/in_ECX appears (lift-learnings 31 void-decl
 * trap). kb.json's stale `void FUN_000bf8f0(void);` decl was corrected to the
 * 3-arg cdecl form as part of this lift; leaving a (void) decl over a
 * stack-arg callee is the ESP-drift class of bug from 0x158df0. The two unused
 * parameters are deliberately KEPT in the declaration: the caller is the
 * shared HS dispatcher table, which pushes all three regardless.
 *
 * Binary evidence (traced backward from each CALL in the disassembly):
 *   MOV dword ptr [EBP-0x4],0x0 at entry pre-zeroes the whole local dword
 *   BEFORE the call.
 *
 *   CALL 0x1b2610 @0xbf8fa takes no arguments (no PUSH before it, no ADD ESP
 *   after it => 0-arg cdecl) and returns a single byte in AL.
 *
 *   MOV ECX,[EBP+0xc] then MOV byte ptr [EBP-0x4],AL: only AL is stored into
 *   the already-zeroed dword, so the byte result is zero-extended (uint8 ->
 *   int) rather than sign-extended and rather than MOVZX'd. Ghidra's
 *   `local_8 = (uint)bVar1` hides this; the union below reproduces the
 *   pre-zero + byte-only store shape.
 *
 *   MOV EAX,[EBP-0x4]; PUSH EAX; PUSH ECX; CALL 0xcbf80 -> cdecl reverse push
 *   order => C order hs_return(thread_datum, value). ADD ESP,0x8 here is
 *   standalone (not merged with another call's cleanup, unlike the twins), so
 *   it matches hs_return's 2 declared args exactly and produces no ARG_COUNT
 *   hazard. Epilogue: MOV ESP,EBP; POP EBP; RET.
 *
 * Callees (both cdecl, both in kb.json, both ported, no @<reg> args):
 *   0x1b2610 = unit_solo_player_integrated_night_vision_is_active(void)
 *              -> char (byte in AL; do NOT widen the return at the call)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bf8f0(int16_t function_index, int thread_datum, char init)
{
  union {
    unsigned char b;
    int i;
  } value;

  value.i = 0;
  value.b = (unsigned char)unit_solo_player_integrated_night_vision_is_active();
  hs_return(thread_datum, value.i);
}

/* FUN_000bf920 @ 0x000bf920
 *
 * HaloScript builtin dispatcher, byte-shape twin of FUN_000bf8b0 above:
 * identical 3-parameter cdecl shape and identical evaluate / NULL-check /
 * worker / hs_return skeleton, including the same "worker takes (dword @
 * record+0, zero-extended BYTE @ record+4) and the script gets a CONSTANT 0"
 * tail. The only difference is the worker called: 0x1ae210
 * units_set_desired_flashlight_state (units.obj) instead of 0x1a9b80.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xbf920-0xbf957. No locals, no _chkstk, no SUB ESP, no FPU, no
 * SEH. ESI is the only callee-saved register and holds thread_datum live across
 * the evaluate call. The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bf920(void);` decl was corrected to the 3-arg cdecl form as part
 * of this lift; a (void) decl over a stack-arg callee is the ESP-drift class of
 * bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ skips BOTH remaining calls when the result is NULL, so
 *   the 0xcc560 return is a POINTER that is dereferenced even though kb.json
 *   declares it as returning int (same as every twin above; the cast is local
 *   and the kb decl is left alone).
 *
 *   Record deref (2 fields): MOV EAX,dword ptr [EAX] -> DWORD at record+0;
 *   XOR EDX,EDX; MOV DL,byte ptr [EAX+0x4] -> zero-extended BYTE at record+4.
 *   Ghidra renders the +4 read as `piVar1[1]`, i.e. an int -- that width is
 *   WRONG (lift-learnings 24 LOADW); taking it would pass a garbage flashlight
 *   state. The load is unsigned (XOR/MOV DL, not MOVSX). Only +0x0 and +0x4 are
 *   touched, one deref each -- no buffer-alias risk.
 *
 *   CALL 0x1ae210 pushes EDX (the zero-extended byte) then EAX (the dword) =
 *   cdecl reverse -> C order (record[0], byte at record+4). Distinct reloads,
 *   NOT a duplicated argument.
 *
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0). The script
 *   return value is the CONSTANT 0, not the record and not the worker's result
 *   (units_set_desired_flashlight_state is void); the entire observable effect
 *   is the 0x1ae210 side effect. ONE combined ADD ESP,0x10 cleans the 4 pushes
 *   of both 2-arg calls -- the ARG_COUNT hazard on hs_return ("cleanup=4 vs
 *   decl=2") is that same FALSE POSITIVE documented on the twins; hs_return
 *   really takes 2 args, do NOT "fix" its decl.
 *
 * Reloc audit of delinked/functions/000bf920.obj: exactly 3 DISP32 targets, one
 * each -- FUN_000cc560, FUN_001ae210, FUN_000cbf80 -- matching the three calls
 * below with no extra global or string reference.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1ae210 = units_set_desired_flashlight_state(int object_list, char
 * desired) 0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bf920(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    units_set_desired_flashlight_state(
      record[0], (char)*(unsigned char *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf960 @ 0x000bf960
 *
 * HaloScript builtin dispatcher, byte-shape twin of FUN_000bf920 above:
 * identical 3-parameter cdecl shape and identical evaluate / NULL-check /
 * worker / hs_return skeleton, including the same "worker takes (dword @
 * record+0, zero-extended BYTE @ record+4) and the script gets a CONSTANT 0"
 * tail. The only difference is the worker called: 0x1aa550
 * unit_set_desired_flashlight_state (the single-unit variant) instead of
 * 0x1ae210 units_set_desired_flashlight_state (the object-LIST variant).
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xbf960-0xbf997. No locals, no _chkstk, no SUB ESP, no FPU, no
 * SEH. ESI is the only callee-saved register and holds thread_datum live across
 * the evaluate call. The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bf960(void);` decl was corrected to the 3-arg cdecl form as part
 * of this lift; a (void) decl over a stack-arg callee is the ESP-drift class of
 * bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ 0xbf995 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin above; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref (2 fields): MOV EAX,dword ptr [EAX] -> DWORD at record+0;
 *   XOR EDX,EDX; MOV DL,byte ptr [EAX+0x4] -> zero-extended BYTE at record+4.
 *   Ghidra renders the +4 read as `piVar1[1]`, i.e. an int -- that width is
 *   WRONG (lift-learnings 24 LOADW); taking it would pass a garbage flashlight
 *   state. The load is unsigned (XOR/MOV DL, not MOVSX). Only +0x0 and +0x4 are
 *   touched, one deref each -- no buffer-alias risk.
 *
 *   CALL 0x1aa550 pushes EDX (the zero-extended byte) then EAX (the dword) =
 *   cdecl reverse -> C order (record[0], byte at record+4). Distinct reloads,
 *   NOT a duplicated argument.
 *
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0). The script
 *   return value is the CONSTANT 0, not the record and not the worker's result
 *   (unit_set_desired_flashlight_state is void); the entire observable effect
 *   is the 0x1aa550 side effect. ONE combined ADD ESP,0x10 cleans the 4 pushes
 *   of both 2-arg calls -- the ARG_COUNT hazard on hs_return ("cleanup=4 vs
 *   decl=2") is that same FALSE POSITIVE documented on the twins; hs_return
 *   really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1aa550 = unit_set_desired_flashlight_state(int unit_handle, char desired)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bf960(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    unit_set_desired_flashlight_state(
      record[0], (char)*(unsigned char *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bf9a0 @ 0x000bf9a0
 *
 * HaloScript builtin dispatcher; the READ-side twin of FUN_000bf920 /
 * FUN_000bf960 above. Same 3-parameter cdecl shape and the same evaluate /
 * NULL-check / worker / hs_return skeleton, but this one QUERIES the unit's
 * current flashlight state (0x1aa590) and hands the byte back to the script
 * through hs_return -- unlike the two setters, whose hs_return argument is a
 * CONSTANT 0.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (one 4-byte local at EBP-0x4);
 * PUSH ESI; ... POP ESI; MOV ESP,EBP; POP EBP; RET. Body spans
 * 0xbf9a0-0xbf9e0. No _chkstk, no SUB ESP, no FPU, no SEH. ESI is the only
 * callee-saved register and holds thread_datum live across the evaluate call.
 * The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bf9a0(void);` decl was corrected to the 3-arg cdecl form as part
 * of this lift; a (void) decl over a stack-arg callee is the ESP-drift class of
 * bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ 0xbf9dc skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin above; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref (ONE field only, unlike the +0x0/+0x4 pair of the setters):
 *   MOV EDX,dword ptr [EAX] -> DWORD at record+0, the unit handle. One deref,
 *   single field, no buffer-alias risk.
 *
 *   CALL 0x1aa590 pushes EDX (record[0]) = 1 arg, returning a BYTE in AL. Its
 *   4-byte ESP cleanup is folded into the later combined ADD ESP,0xc.
 *
 *   Result widening: MOV dword ptr [EBP-0x4],0x0 (emitted between the evaluate
 *   pushes and that CALL -- MSVC scheduling of the zero-init of the result
 *   slot) then MOV byte ptr [EBP-0x4],AL then MOV EAX,dword ptr [EBP-0x4].
 *   That is a pre-zeroed dword with only its low byte overwritten and read back
 *   as an int -- NOT a MOVZX and NOT a sign-extending (int)(char) cast
 *   (lift-learnings 24 LOADW). The union below reproduces exactly that store
 *   pair; it is the same idiom that scored 100% on FUN_000bf8f0.
 *
 *   CALL 0xcbf80 pushes EAX (the widened byte) then ESI = cdecl reverse ->
 *   hs_return(thread_datum, value). ONE combined ADD ESP,0xc cleans these 2
 *   pushes plus the 1 leftover push of the 0x1aa590 call -- the ARG_COUNT
 *   hazard on hs_return ("cleanup=0xc vs decl=2") is the same FALSE POSITIVE
 *   documented on the twins; hs_return really takes 2 args, do NOT "fix" its
 *   decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x1aa590 = unit_get_current_flashlight_state(int unit_handle) -> char
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bf9a0(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    unsigned char b;
    int i;
  } value;

  value.i = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.b = (unsigned char)unit_get_current_flashlight_state(record[0]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bfa30 @ 0x000bfa30
 *
 * HaloScript builtin dispatcher; byte-shape twin of FUN_000bf920 /
 * FUN_000bf960 / FUN_000bf8b0 above -- identical 3-parameter cdecl shape and
 * identical evaluate / NULL-check / worker / hs_return skeleton. The delta is
 * the worker (0x97260) and, more importantly, the WIDTH of the second record
 * field: this one is a FLOAT (FLD dword), not the zero-extended BYTE the
 * flashlight twins pass.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xbfa30-0xbfa68. No locals, no SUB ESP, no _chkstk, no SEH. ESI is
 * the only callee-saved register and holds thread_datum live across the
 * evaluate call. The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bfa30(void);` decl was corrected to the 3-arg cdecl form as part
 * of this lift, as was the callee 0x97260's equally stale `void
 * FUN_00097260(void);`; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ 0xbfa66 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin above; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref, exactly two fields, one read each (no buffer-alias risk):
 *     FLD dword ptr [EAX+0x4]  -> FLOAT at record+4
 *     MOV EDX,dword ptr [EAX]  -> DWORD at record+0
 *   The FLD is a 4-byte x87 float load, NOT an integer load: Ghidra dropped it
 *   entirely and the flashlight twins carry a BYTE at this same offset, so the
 *   width is taken from THIS function's disassembly (lift-learnings 24 LOADW).
 *
 *   CALL 0x97260 is the push-then-fstp float hazard (decompiler trap 2): the
 *   PUSH ECX before it is a DUMMY slot immediately overwritten by
 *   FSTP dword ptr [ESP], so the first (highest) pushed argument is the FLOAT
 *   from record+4, not ECX. PUSH EDX (record[0]) follows. cdecl reverse -> C
 *   order FUN_00097260(record[0], *(float *)(record+4)). Two DISTINCT args --
 *   passing ECX, or an (int) cast of the float, would silently corrupt the
 *   argument (lift-learnings 6 float-smuggling).
 *
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0). The script
 *   return value is the CONSTANT 0, not the record and not the worker's result;
 *   the entire observable effect is the 0x97260 side effect. ONE combined
 *   ADD ESP,0x10 cleans the 4 pushes of both 2-arg calls -- the ARG_COUNT
 *   hazard on hs_return ("cleanup=4 vs decl=2") is that same FALSE POSITIVE
 *   documented on the twins; hs_return really takes 2 args, do NOT "fix" its
 *   decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x97260  = FUN_00097260(int, float)  -- unnamed worker, semantics unknown
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfa30(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_00097260(record[0], *(float *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bfa70 @ 0x000bfa70
 *
 * HaloScript builtin dispatcher; the same evaluate / NULL-check / accessor /
 * hs_return skeleton as the FUN_000bf9xx / FUN_000bfa30 twins above and as
 * FUN_000c1350 / FUN_000c1390 in hs.c. The delta here is the accessor: it
 * reads a device object handle out of the result record and returns the
 * device's power as a FLOAT.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (one 4-byte local at EBP-4);
 * saves ESI only. 26 instructions total; no SUB ESP, no _chkstk, no SEH, no
 * struct writes, no buffers (buffer_alias: 0 hits). ESI caches thread_datum
 * across the whole body and is reused as hs_return's first argument. The exit
 * RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bfa70(void);` decl was corrected to the 3-arg cdecl form as part
 * of this lift.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ end skips BOTH remaining calls when the result is NULL,
 *   so the 0xcc560 return is a POINTER that is dereferenced even though kb.json
 *   declares it as returning int (same as every twin above; the cast is local
 *   and the kb decl is left alone).
 *
 *   MOV EDX,dword ptr [EAX] -> the FIRST DWORD of the result record, a device
 *   object handle. PUSH EDX; CALL 0x964a0. Ghidra dropped this argument
 *   entirely (it printed a bare `FUN_000964a0()`), which would have lost the
 *   `*record` load -- the disassembly, not the decompile, is authoritative
 *   here. device_get_power's stack slot is NOT cleaned at its own call site:
 *   the single ADD ESP,0xc after the NEXT call covers 3 dwords (1 for
 *   device_get_power + 2 for hs_return). So device_get_power = 1 cdecl stack
 *   arg, float return in ST0. kb.json's stale `void device_get_power(void);`
 *   was corrected to `float device_get_power(int device_object);` as part of
 *   this lift -- a (void) decl over a stack-arg callee is the ESP-drift class
 *   of bug from 0x158df0, and a void return over an ST0 float is the XCALL
 *   ST0-vs-EAX hazard.
 *
 *   FSTP dword ptr [EBP-4] ; MOV EAX,[EBP-4] ; PUSH EAX -> store the float,
 *   reload the SAME dword, push the raw bits. This is a TYPE-PUN, not a numeric
 *   (int) conversion; a `(int)f` cast would truncate and commit the wrong bits
 *   into the script return slot (lift-learnings 6 float-smuggling). Modelled
 *   with a float/int union, matching FUN_000c1350 / FUN_000c1390 in hs.c.
 *   Ghidra mis-modelled this as `(int)(float)extraout_ST0`.
 *
 *   CALL 0xcbf80 pushes EAX(the punned bits) then ESI -> cdecl reverse ->
 *   hs_return(thread_datum, value.dw). thread_datum flows to BOTH calls. The
 *   arg-count enrichment flagged "cleanup=3 vs decl=2" on hs_return; that is
 *   the FALSE POSITIVE explained by the folded device_get_power cleanup above.
 *   hs_return really takes 2 args -- do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x964a0  = device_get_power(int device_object) -> float in ST0
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfa70(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    float f;
    int dw;
  } value;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.f = device_get_power(record[0]);
    hs_return(thread_datum, value.dw);
  }
}

/* FUN_000bfab0 @ 0x000bfab0
 *
 * HaloScript builtin dispatcher; the QUERY-side hybrid of the two twins that
 * bracket it. It shares the FLOAT second record field of FUN_000bfa30 (FLD
 * dword ptr [EAX+0x4]) with the pre-zeroed-dword / low-byte-store result
 * widening of FUN_000bf8f0 / FUN_000bf9a0 -- i.e. the worker's boolean AL
 * result is what goes back to the script, not the constant 0 the setters use.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (one 4-byte local at EBP-0x4);
 * PUSH ESI; ... POP ESI; MOV ESP,EBP; POP EBP; RET. Body spans
 * 0xbfab0-0xbfaf7. No SUB ESP, no _chkstk, no SEH, no struct writes, no
 * buffers. ESI is the only callee-saved register and holds thread_datum live
 * across the evaluate call. The exit RET carries no immediate => cdecl, caller
 * cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bfab0(void);` decl was corrected to the 3-arg cdecl form as part
 * of this lift, as was the callee 0x97220's equally stale `void
 * FUN_00097220(void);`; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ skips BOTH remaining calls when the result is NULL, so
 *   the 0xcc560 return is a POINTER that is dereferenced even though kb.json
 *   declares it as returning int (same as every twin above; the cast is local
 *   and the kb decl is left alone).
 *
 *   Record deref, exactly two fields, one read each (no buffer-alias risk):
 *     FLD dword ptr [EAX+0x4]  -> FLOAT at record+4
 *     MOV EDX,dword ptr [EAX]  -> DWORD at record+0
 *   The FLD is a 4-byte x87 float load, NOT an integer load. The flashlight
 *   twins (FUN_000bf920 / FUN_000bf960) carry a zero-extended BYTE at this
 *   same +0x4 offset, so the width is taken from THIS function's disassembly
 *   and NOT copied from them (lift-learnings 24 LOADW).
 *
 *   CALL 0x97220 is the push-then-fstp float hazard (decompiler trap 2): the
 *   PUSH ECX before it is a DUMMY slot immediately overwritten by FSTP dword
 *   ptr [ESP], so the first (highest) pushed argument is the FLOAT from
 *   record+4, not ECX. PUSH EDX (record[0]) follows. cdecl reverse -> C order
 *   FUN_00097220(record[0], *(float *)(record+4)). Ghidra printed a bare
 *   zero-argument `FUN_00097220()` -- the disassembly, not the decompile, is
 *   authoritative. Passing ECX, or an (int) cast of the float, would silently
 *   corrupt the argument (lift-learnings 6 float-smuggling).
 *
 *   Result widening: MOV dword ptr [EBP-0x4],0x0 is emitted BEFORE the
 *   evaluate call (MSVC scheduling of the zero-init of the result slot), then
 *   after the worker returns MOV byte ptr [EBP-0x4],AL then MOV EAX,dword ptr
 *   [EBP-0x4]. That is a pre-zeroed dword with only its low byte overwritten
 *   and read back as an int -- NOT a MOVZX and NOT a sign-extending
 *   (int)(char) cast (lift-learnings 24 LOADW). The worker's return is
 *   therefore a BYTE in AL, zero-extended. The union below reproduces exactly
 *   that store pair; it is the same idiom that scored 100% on FUN_000bf8f0.
 *
 *   CALL 0xcbf80 pushes EAX (the widened byte) then ESI = cdecl reverse ->
 *   hs_return(thread_datum, value.i). ONE combined ADD ESP,0x10 cleans the 4
 *   pushes of both 2-arg calls -- the ARG_COUNT hazard on hs_return
 *   ("cleanup=4 vs decl=2") is that same FALSE POSITIVE documented on the
 *   twins; hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x97220  = FUN_00097220(int, float) -> char in AL  (unnamed worker; the
 *              immediate neighbour of FUN_00097260 used by FUN_000bfa30, so
 *              the two are almost certainly a setter/query pair, but nothing
 *              in this function names either -- left as FUN_)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfab0(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    unsigned char b;
    int i;
  } value;

  value.i = 0;
  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.b =
      (unsigned char)FUN_00097220(record[0], *(float *)((char *)record + 4));
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bfb00 @ 0x000bfb00
 *
 * HaloScript builtin dispatcher, float-return twin of the byte/int dispatchers
 * above (FUN_000bf920 / FUN_000bfab0): identical 3-parameter cdecl shape and
 * identical evaluate / NULL-check / worker / hs_return skeleton. The only
 * difference is that the worker returns a FLOAT and its raw 32-bit pattern is
 * handed straight to hs_return.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (one 4-byte local); PUSH ESI;
 * ... POP ESI; MOV ESP,EBP; POP EBP; RET. 25 instructions total. No _chkstk, no
 * SUB ESP, no buffers, no SEH. ESI is the only callee-saved register and holds
 * thread_datum live across the evaluate call. The exit RET carries no immediate
 * => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> (lift-learnings 31
 * void-decl trap). kb.json's stale `void FUN_000bfb00(void);` decl was
 * corrected to the 3-arg cdecl form as part of this lift.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through.
 *
 *   TEST EAX,EAX / JZ skips BOTH remaining calls when the result is NULL, so
 *   the 0xcc560 return is a POINTER that is dereferenced even though kb.json
 *   declares it as returning int (same as every twin above).
 *
 *   Record deref (1 field): MOV EDX,dword ptr [EAX] -> full 32-bit DWORD at
 *   record+0. NOT a MOVSX word load -- do not copy the `*(int16_t *)record`
 *   idiom of the int16 twins here. Only +0x0 is touched, one deref, so there is
 *   no buffer-alias risk.
 *
 *   CALL 0x96470 device_get_position takes exactly ONE stack arg (PUSH EDX =
 *   record[0]) and returns a FLOAT: the very next instruction is
 *   FSTP dword ptr [EBP-0x4]. kb.json declared it `void device_get_position
 *   (void);`, which is WRONG on both counts -- against that decl the call site
 *   would push nothing and read EAX garbage (feedback_xcall_type_audit: a
 *   float-returning callee declared void reads EAX instead of ST0). The decl
 *   was corrected to `float device_get_position(int device_object);`, matching
 *   its immediate neighbour `float device_get_power(int device_object);` at
 *   0x964a0. This lift is its only source caller.
 *
 *   The result is then re-read from the SAME 4-byte slot as an integer
 *   (MOV EAX,[EBP-0x4]; PUSH EAX) and pushed to hs_return. There is no _ftol2
 *   and no FISTP anywhere in the body, so this is a BIT REINTERPRETATION, not a
 *   float->int conversion; Ghidra's `(int)(float)extraout_ST0` is a decompiler
 *   artifact and writing `(int)value` would emit a truncation the original does
 *   not perform. The union below reproduces the FSTP/MOV pair exactly.
 *
 *   CALL 0xcbf80 pushes the raw float bits then ESI -> hs_return(thread_datum,
 *   bits). ONE combined ADD ESP,0xc cleans device_get_position's 1 arg plus
 *   hs_return's 2 args (feedback_cdecl_push_order: do NOT infer a 3-arg
 *   hs_return from that single cleanup).
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x96470 = device_get_position(int device_object) -> float
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfb00(int16_t function_index, int thread_datum, char init)
{
  int *record;
  union {
    float f;
    int i;
  } value;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    value.f = device_get_position(record[0]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bfb40 @ 0x000bfb40
 *
 * HaloScript builtin dispatcher; a SETTER twin of FUN_000bf920 /
 * FUN_000bf960 with a FLOAT payload instead of a byte one. Identical
 * 3-parameter cdecl shape and the same evaluate / NULL-check / worker /
 * hs_return skeleton. The worker's result is discarded and the script gets a
 * CONSTANT 0 back (PUSH 0x0), which is what distinguishes this from the
 * query-side twin FUN_000bfab0 that shares the same (int, float) worker ABI.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xbfb40-0xbfb78. NO local slot at all (no PUSH ECX / SUB ESP),
 * no _chkstk, no FPU arithmetic, no SEH, no struct writes, no buffers. ESI is
 * the only callee-saved register and holds thread_datum live across the
 * evaluate call. The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bfb40(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift, as was the callee 0x97040's equally stale `void
 * FUN_00097040(void);`; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *   TEST EAX,EAX / JZ 0xbfb76 skips BOTH remaining calls, so the 0xcc560
 *   return is a record POINTER that is dereferenced even though kb.json
 *   declares it as returning int (same as every twin; the cast is local and
 *   the kb decl is left alone).
 *   The record is read at exactly two offsets, one field each -- no buffer
 *   aliasing is possible: FLD dword ptr [EAX+0x4] (a FLOAT, which Ghidra
 *   hides entirely) and MOV EDX,dword ptr [EAX] (a dword handle).
 *   CALL 0x97040 is the push-then-fstp float-argument idiom (lift-learnings
 *   hazard 2): PUSH ECX is a DUMMY slot reservation immediately overwritten
 *   by FSTP dword ptr [ESP], then PUSH EDX. Entry stack is therefore
 *   [ESP+0]=EDX (dword) and [ESP+4]=the float => C order
 *   FUN_00097040(record[0], *(float *)(record+4)). Ghidra printed a bare
 *   zero-argument `FUN_00097040()` -- the disassembly, not the decompile, is
 *   authoritative here; lifting it 0-arg would drift ESP and pass nothing.
 *   CALL 0xcbf80 pushes 0x0 then ESI => hs_return(thread_datum, 0). The
 *   returned value is a literal constant; the observable effect of this
 *   builtin is the 0x97040 side effect.
 *   ONE combined ADD ESP,0x10 cleans the 4 pushes of both 2-arg calls -- the
 *   ARG_COUNT hazard on hs_return ("cleanup=4 vs decl=2") is the same FALSE
 *   POSITIVE documented on the twins at 0xbf8b0 / 0xbf920; hs_return really
 *   takes 2 args, do NOT "fix" its decl.
 *
 * Callees (raw CALL targets):
 *   0xcc560  = hs_macro_function_evaluate  (returns the record pointer)
 *   0x97040  = FUN_00097040(int, float) -> void  (unnamed worker; the setter
 *              counterpart of 0x97220, whose result FUN_000bfab0 returns)
 *   0xcbf80  = hs_return
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfb40(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_00097040(record[0], *(float *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bfb80 @ 0x000bfb80
 *
 * HaloScript builtin dispatcher; the device-GROUP counterpart of the
 * device-OBJECT reader FUN_000bfb00. Same 3-parameter cdecl shape and the
 * same evaluate / NULL-check / worker / hs_return skeleton as every twin in
 * this run; what distinguishes it is that the record's first field is an
 * UNSIGNED 16-bit device-group index (MOVZX idiom) rather than a dword
 * object handle, and that the worker returns a float in ST0 whose raw 32-bit
 * pattern is handed to hs_return verbatim.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (ONE 4-byte local -- the FSTP
 * scratch slot at [EBP-4], nothing else); PUSH ESI; ... POP ESI; leave-ish
 * epilogue; plain RET (no immediate) => cdecl, caller cleans. No _chkstk, no
 * SEH, no FPU arithmetic, no loops, no buffers, no struct writes.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, held live across BOTH calls
 *                                           and reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bfb80(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift, as was the callee 0x966b0's equally stale
 * `void device_group_get_value(void);` -- see below.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *   TEST EAX,EAX / JZ skips BOTH remaining calls, so the 0xcc560 return is a
 *   record POINTER that is dereferenced even though kb.json declares it as
 *   returning int (same as every twin; the cast is local, kb decl untouched).
 *   Exactly ONE record field is read: XOR EDX,EDX; MOV DX, word ptr [EAX].
 *   That is the MOVZX idiom -- an UNSIGNED 16-bit load at record+0x0, unlike
 *   the signed-short siblings FUN_000bdfe0 / FUN_000be030. Lifted as
 *   `unsigned short *` so the widening is zero-extension, not sign-extension.
 *   CALL 0x966b0 takes the zero-extended index as its ONE dword stack arg
 *   (PUSH EDX) and returns a float in ST0 -- FSTP dword ptr [EBP-4] lands
 *   immediately after the call. kb.json declared it `void (void)`, which
 *   would have dropped the argument (ESP drift, the 0x158df0 class of bug)
 *   and read EAX garbage as the result; the decl was corrected to
 *   `float device_group_get_value(int device_group_index);` (it is
 *   ported:false, so only the thunk decl changes).
 *   FSTP dword [EBP-4]; MOV EAX,[EBP-4]; PUSH EAX is the float-smuggling
 *   idiom (lift-learnings 6): the float's 32-bit BIT PATTERN is passed
 *   verbatim into hs_return's untyped dword value slot. Modelled with a
 *   union -- a numeric (int) cast would truncate and silently return 0.
 *   CALL 0xcbf80 pushes EAX(value bits) then ESI => hs_return(thread_datum,
 *   value bits).
 *   ONE combined ADD ESP,0xc cleans the 1 push of 0x966b0 (whose cleanup
 *   MSVC folded forward) plus hs_return's 2 -- the ARG_COUNT hazard on
 *   hs_return ("cleanup=3 vs decl=2") is the same FALSE POSITIVE documented
 *   on the twins at 0xbf8b0 / 0xbf920 / 0xbfb40; hs_return really takes 2
 *   args, do NOT "fix" its decl.
 *
 * Callees (raw CALL targets, all cdecl, no @<reg> args anywhere):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x966b0 = device_group_get_value(int device_group_index) -> float
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfb80(int16_t function_index, int thread_datum, char init)
{
  unsigned short *record;
  union {
    float f;
    int i;
  } value;

  record = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (record != NULL) {
    value.f = device_group_get_value((int)record[0]);
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bfbc0 @ 0x000bfbc0
 *
 * HaloScript builtin dispatcher; a near-exact twin of FUN_000bfab0 above --
 * same 3-parameter cdecl shape, same evaluate / NULL-check / worker /
 * hs_return skeleton, and the same "pre-zeroed dword whose low BYTE is
 * overwritten by the worker's AL" result-widening tail.  The two differences
 * from FUN_000bfab0 are the record's first field (an UNSIGNED 16-bit index
 * loaded with the MOVZX idiom rather than a dword object handle) and the
 * worker called (0x96f20 rather than 0x97220).
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ECX (ONE 4-byte local, the result
 * slot at [EBP-4]); PUSH ESI; ... POP ESI; plain RET (no immediate) => cdecl,
 * caller cleans.  Body spans 0xbfbc0-0xbfc0a (75 bytes).  No _chkstk, no SEH,
 * no loops, no buffers, no struct writes, no FPU arithmetic.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, held live across BOTH calls
 *                                           and reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap).  kb.json's stale
 * `void FUN_000bfbc0(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift, as was the callee 0x96f20's equally stale
 * `void FUN_00096f20(void);` -- see below.
 *
 * Binary evidence (traced backward from each CALL):
 *   MOV dword ptr [EBP-0x4],0x0 is emitted BEFORE the evaluate call (MSVC
 *   scheduling of the result slot's zero-init).
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args.  Straight pass-through, no reordering.
 *   TEST EAX,EAX / JZ skips BOTH remaining calls, so the 0xcc560 return is a
 *   record POINTER that is dereferenced even though kb.json declares it as
 *   returning int (same as every twin; the cast is local, kb decl untouched).
 *   Ghidra typed that return `int iVar1` and dropped the dereferences
 *   entirely; the disassembly reads TWO fields from it:
 *     record+0x00  FLD is preceded by XOR EDX,EDX; MOV DX,word ptr [EAX] --
 *                  the MOVZX idiom, an UNSIGNED 16-bit index (zero-extended,
 *                  NOT sign-extended: lift-learnings 24 LOADW).
 *     record+0x04  FLD dword ptr [EAX+4] -- a float.
 *   CALL 0x96f20 is the push-then-fstp float hazard (lift-decompiler-traps
 *   2): PUSH ECX is a DUMMY slot immediately overwritten by FSTP dword ptr
 *   [ESP] at 0xbfbec-0xbfbed, then PUSH EDX.  Ghidra rendered this as a
 *   ZERO-argument call; the real cdecl order is
 *   FUN_00096f20((int)record[0], *(float *)(record + 4)).  kb.json declared
 *   it `void (void)`, which would have dropped BOTH arguments (ESP drift,
 *   the 0x158df0 class of bug) and discarded the AL return (lift-learnings
 *   16 void-EAX); the decl was corrected to
 *   `char FUN_00096f20(int arg0, float arg1);` (ported:false, so only the
 *   thunk decl changes).  Left as FUN_ -- nothing here names it.
 *   Result widening: MOV byte ptr [EBP-0x4],AL then MOV EAX,dword ptr
 *   [EBP-0x4]; PUSH EAX.  A pre-zeroed dword with only its low byte
 *   overwritten and read back as an int -- NOT a MOVZX and NOT a
 *   sign-extending (int)(char) cast.  The union below reproduces exactly that
 *   store pair; it is the same idiom that scored 100% on FUN_000bf8f0 and
 *   FUN_000bfab0.
 *   CALL 0xcbf80 pushes EAX (the widened byte) then ESI = cdecl reverse ->
 *   hs_return(thread_datum, value.i).  ONE combined ADD ESP,0x10 at 0xbfc03
 *   cleans the 2 pushes of 0x96f20 plus hs_return's 2 -- the ARG_COUNT hazard
 *   on hs_return ("cleanup=4 vs decl=2") is the same FALSE POSITIVE
 *   documented on the twins at 0xbf8b0 / 0xbf920 / 0xbfab0 / 0xbfb40;
 *   hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (raw CALL targets, all cdecl, no @<reg> args anywhere):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x96f20 = FUN_00096f20(int index, float value) -> char in AL
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfbc0(int16_t function_index, int thread_datum, char init)
{
  unsigned short *record;
  union {
    unsigned char b;
    int i;
  } value;

  value.i = 0;
  record = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (record != NULL) {
    value.b = (unsigned char)FUN_00096f20((int)record[0],
                                          *(float *)((char *)record + 4));
    hs_return(thread_datum, value.i);
  }
}

/* FUN_000bfc10 @ 0x000bfc10
 *
 * HaloScript builtin dispatcher; the device-GROUP SETTER twin of the
 * device-group reader FUN_000bfb80 above and of the (dword, float) setter
 * FUN_000bfb40. Identical 3-parameter cdecl shape and the same evaluate /
 * NULL-check / worker / hs_return skeleton as every twin in this run. What
 * distinguishes it: the record's first field is an UNSIGNED 16-bit
 * device-group index (the XOR/MOV-DX zero-extend idiom) and the second is a
 * FLOAT, so the worker takes (group_index, value); the script return is the
 * CONSTANT 0, so the entire observable effect is the 0x96510 side effect.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xbfc10-0xbfc4b (28 instructions). NO local slot at all (no
 * PUSH ECX / SUB ESP), no _chkstk, no FPU arithmetic, no SEH, no struct
 * writes, no buffers. ESI is the only callee-saved register and holds
 * thread_datum live across the evaluate call. The exit RET carries no
 * immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bfc10(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift, as was the callee 0x96510's equally stale `void
 * device_group_set_actual_value(void);`; a (void) decl over a stack-arg
 * callee is the ESP-drift class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *   TEST EAX,EAX / JZ 0xbfc49 skips BOTH remaining calls, so the 0xcc560
 *   return is a record POINTER that is dereferenced even though kb.json
 *   declares it as returning int (same as every twin; the cast is local and
 *   the kb decl is left alone).
 *   The record is read at exactly two offsets, one field each -- no buffer
 *   aliasing is possible: FLD dword ptr [EAX+0x4] (a FLOAT, which Ghidra
 *   hides entirely) and XOR EDX,EDX / MOV DX,word ptr [EAX]. That second
 *   read is a zero-extended UNSIGNED 16-bit load, NOT an int (lift-learnings
 *   24 LOADW) -- hence the `unsigned short *` record type, matching the
 *   device-group reader twin FUN_000bfb80.
 *   CALL 0x96510 is the push-then-fstp float-argument idiom (lift-learnings
 *   hazard 2): PUSH ECX is a DUMMY slot reservation immediately overwritten
 *   by FSTP dword ptr [ESP], then PUSH EDX. Entry stack is therefore
 *   [ESP+0]=EDX (the zero-extended group index) and [ESP+4]=the float => C
 *   order device_group_set_actual_value(record[0], *(float *)(record+4)).
 *   Ghidra printed a bare zero-argument call -- the disassembly, not the
 *   decompile, is authoritative here; lifting it 0-arg would drift ESP and
 *   pass nothing.
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0). ONE
 *   combined ADD ESP,0x10 cleans the 4 pushes of both 2-arg calls -- the
 *   ARG_COUNT hazard on hs_return ("cleanup=4 vs decl=2") is that same FALSE
 *   POSITIVE documented on the twins; hs_return really takes 2 args, do NOT
 *   "fix" its decl.
 *
 * Reloc audit of delinked/functions/000bfc10.obj: exactly three DISP32
 * targets -- FUN_000cc560, device_group_set_actual_value, hs_return -- with
 * no global and no string reference.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x96510 = device_group_set_actual_value(int device_group_index,
 *             float value); the first parameter is typed `int` to mirror its
 *             already-declared reader sibling device_group_get_value(int) --
 *             the binary only proves a 32-bit push of a zero-extended word.
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Codegen-shape note (why the float read is `volatile`): under this repo's
 * VC71 flags (/O2 /Ot) MSVC folds a plain `*(float *)p` argument into a
 * 4-byte integer copy (`mov 0x4(%eax),%edx; push %edx`), which the original
 * does NOT do -- it uses the x87 push-then-fstp idiom. Five source spellings
 * were probe-compiled (pointer cast, `((float *)r)[1]`, struct member, local
 * float temp, int* base) and ALL folded to the integer copy; only marking the
 * read `volatile` -- or dropping to /Os, which then breaks the prologue --
 * restores `flds 0x4(%eax) ... push %ecx; fstps (%esp)`. The qualifier is a
 * pure codegen lever: the field is read exactly once either way, so the
 * observable behaviour is unchanged. The same fold silently costs the twin
 * FUN_000bfb40 above the same two instructions (not touched here).
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfc10(int16_t function_index, int thread_datum, char init)
{
  unsigned short *record;

  record = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (record != NULL) {
    device_group_set_actual_value((int)record[0],
                                  *(volatile float *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bfc50 @ 0x000bfc50
 *
 * HaloScript builtin dispatcher; byte-shape twin of FUN_000bf920 above:
 * identical 3-parameter cdecl shape and identical evaluate / NULL-check /
 * worker / hs_return skeleton, including the same "worker takes (dword @
 * record+0, zero-extended BYTE @ record+4) and the script gets a CONSTANT 0"
 * tail. The only difference is the worker called: 0x965f0
 * device_one_sided_set instead of 0x1ae210.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xbfc50-0xbfc87. No locals, no _chkstk, no SUB ESP, no FPU, no
 * SEH. ESI is the only callee-saved register and holds thread_datum live across
 * the evaluate call. The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bfc50(void);` decl was corrected to the 3-arg cdecl form as part
 * of this lift; a (void) decl over a stack-arg callee is the ESP-drift class of
 * bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering.
 *
 *   TEST EAX,EAX / JZ 0xbfc85 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin above; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref (2 fields): MOV EAX,dword ptr [EAX] -> DWORD at record+0;
 *   XOR EDX,EDX; MOV DL,byte ptr [EAX+0x4] -> zero-extended BYTE at record+4.
 *   Ghidra renders the +4 read as an int -- that width is WRONG (lift-learnings
 *   24 LOADW); taking it would pass a garbage one-sided flag. The load is
 *   unsigned (XOR/MOV DL, not MOVSX). Only +0x0 and +0x4 are touched, one deref
 *   each -- no buffer-alias risk.
 *
 *   CALL 0x965f0 pushes EDX (the zero-extended byte) then EAX (the dword) =
 *   cdecl reverse -> C order (record[0], byte at record+4). Distinct reloads,
 *   NOT a duplicated argument. Ghidra shows this call with ZERO arguments --
 *   that is wrong; the disassembly plainly pushes EDX then EAX.
 *
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0). The script
 *   return value is the CONSTANT 0, not the record and not the worker's result;
 *   the entire observable effect is the 0x965f0 side effect. ONE combined ADD
 *   ESP,0x10 cleans the 4 pushes of both 2-arg calls -- the ARG_COUNT hazard on
 *   hs_return ("cleanup=4 vs decl=2") is that same FALSE POSITIVE documented on
 *   the twins; hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Reloc audit of delinked/functions/000bfc50.obj: the first three DISP32
 * targets (offsets 0x11/0x26/0x2e) are FUN_000cc560, FUN_000965f0,
 * FUN_000cbf80 -- matching the three calls below with no extra global or string
 * reference. (The exported range also covers the following function 0xbfc90,
 * whose relocs are the trailing three.)
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x965f0 = device_one_sided_set(int object_list, char one_sided); its stale
 *             `void device_one_sided_set(void);` kb decl was corrected here --
 *             calling through a (void) decl would have left 8 bytes of drift.
 *             Arg names are mechanical (naming-confidence: behaviour-only).
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfc50(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    device_one_sided_set(record[0],
                         (char)*(unsigned char *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bfc90 @ 0x000bfc90
 *
 * HaloScript builtin dispatcher, byte-shape twin of FUN_000bfc50 directly
 * above and of FUN_000bf8b0 / FUN_000bf8f0 / FUN_000bf920 earlier in this TU:
 * identical 3-parameter cdecl shape and identical evaluate / NULL-check /
 * worker / hs_return skeleton, including the same "worker takes (dword @
 * record+0, zero-extended BYTE @ record+4) and the script gets a CONSTANT 0"
 * tail. The only difference from FUN_000bfc50 is the worker called: 0x96630
 * device_operates_automatically_set instead of 0x965f0 device_one_sided_set.
 *
 * Ghidra modelled the function as void(void), so all three cdecl STACK
 * parameters surfaced as in_stack_* pseudo-locals and the 0x96630 call lost
 * BOTH of its arguments (kb.json also declared that callee void(void)). The
 * `void FUN_000bfc90(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL of the 24-instruction body
 * 0xbfc90-0xbfcc7; cdecl prologue PUSH EBP / MOV EBP,ESP / PUSH ESI, no SUB
 * ESP, no locals, no FPU, no SEH, RET with no immediate so the caller cleans):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = 3 args. Straight pass-through, no reordering. ESI (the thread
 *   datum) is callee-saved across the call and reused for hs_return.
 *
 *   TEST EAX,EAX / JZ 0xbfcc5 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref (2 fields): MOV EAX,dword ptr [EAX] -> DWORD at record+0;
 *   XOR EDX,EDX; MOV DL,byte ptr [EAX+0x4] -> zero-extended BYTE at record+4.
 *   Ghidra renders the +4 read as an int -- that width is WRONG (lift-learnings
 *   24 LOADW); taking it would pass a garbage flag. The load is unsigned
 *   (XOR/MOV DL, not MOVSX). Only +0x0 and +0x4 are touched, one deref each --
 *   no buffer-alias risk.
 *
 *   CALL 0x96630 pushes EDX (the zero-extended byte) then EAX (the dword) =
 *   cdecl reverse -> C order (record[0], byte at record+4). Distinct reloads,
 *   NOT a duplicated argument. Ghidra shows this call with ZERO arguments --
 *   that is wrong; the disassembly plainly pushes EDX then EAX.
 *
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0). The script
 *   return value is the CONSTANT 0, not the record and not the worker's result;
 *   the entire observable effect is the 0x96630 side effect. ONE combined ADD
 *   ESP,0x10 cleans the 4 pushes of both 2-arg calls -- the ARG_COUNT hazard on
 *   hs_return ("cleanup=4 vs decl=2") is that same FALSE POSITIVE documented on
 *   the twins; hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x96630 = device_operates_automatically_set(int object_list,
 *             char operates_automatically); its stale
 *             `void device_operates_automatically_set(void);` kb decl was
 *             corrected here -- calling through a (void) decl would have left
 *             8 bytes of drift. Callee is unported, so the change is decl-only.
 *             Arg names are mechanical (naming-confidence: behaviour-only).
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfc90(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    device_operates_automatically_set(
      record[0], (char)*(unsigned char *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bfcd0 @ 0x000bfcd0
 *
 * HaloScript builtin dispatcher, the immediate neighbour of FUN_000bfc90
 * above and the same skeleton as every twin earlier in this TU: evaluate the
 * argument record, NULL-check it, invoke one device worker with two fields of
 * that record, then hand the script a CONSTANT 0.
 *
 * The ONE structural difference from FUN_000bfc90 / FUN_000bfc50 is the WIDTH
 * of the first record field: those twins load a full DWORD (MOV EAX,[EAX]),
 * this one loads a SIGNED WORD (MOVSX EAX, word ptr [EAX]). That is
 * lift-learnings 24 (LOADW) territory -- taking Ghidra's `int` rendering would
 * emit a plain dword load and silently pass 2 bytes of neighbouring record
 * data in the high half. The second field is unchanged: XOR EDX,EDX / MOV DL,
 * byte ptr [EAX+4] = ZERO-extended byte, not signed.
 *
 * Ghidra modelled the function as void(void), so all three cdecl STACK
 * parameters surfaced as in_stack_* pseudo-locals and the 0x96670 call lost
 * BOTH of its arguments (kb.json also declared that callee void(void)). The
 * `void FUN_000bfcd0(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL of the body 0xbfcd0-0xbfd09;
 * cdecl prologue PUSH EBP / MOV EBP,ESP / PUSH ESI, no SUB ESP, no _chkstk, no
 * locals, no FPU, no buffers, no SEH; RET with no immediate so the caller
 * cleans):
 *   CALL 0xcc560 pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) / ECX([EBP+0x8]) in
 *   cdecl reverse order -> C order (function_index, thread_datum, init); ADD
 *   ESP,0xc = exactly 3 args, matching the kb decl. Straight pass-through, no
 *   reordering. ESI (the thread datum) is callee-saved across all three calls
 *   and is reloaded from nothing -- it is reused directly for hs_return, so
 *   there is no register-aliasing ambiguity about which C variable it holds.
 *
 *   TEST EAX,EAX / JZ 0xbfd06 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref (2 fields, one load each -- no buffer-alias risk):
 *     XOR EDX,EDX ; MOV DL, byte ptr [EAX+0x4]  -> unsigned BYTE at record+4
 *     MOVSX EAX, word ptr [EAX]                 -> signed  WORD at record+0
 *   The byte at +4 MUST be read BEFORE the word at +0, because the MOVSX
 *   overwrites the record base in EAX. Writing the two loads as the two
 *   arguments of one cdecl call reproduces that order for free: MSVC evaluates
 *   and pushes cdecl arguments right-to-left, so the +4 byte (last argument) is
 *   materialised first, exactly as in the original.
 *
 *   CALL 0x96670 pushes EDX (the zero-extended byte) then EAX (the sign-
 *   extended word) = cdecl reverse -> C order (word at record+0, byte at
 *   record+4). Distinct reloads, NOT a duplicated argument. Ghidra shows this
 *   call with ZERO arguments -- that is wrong; the disassembly plainly pushes
 *   EDX then EAX. dump_caller_regsetup 0x96670 confirms it, and this is that
 *   callee's ONLY call site in the image.
 *
 *   CALL 0xcbf80 pushes 0x0 then ESI -> hs_return(thread_datum, 0). The script
 *   return value is the CONSTANT 0, not the record and not the worker's result;
 *   the entire observable effect is the 0x96670 side effect. ONE combined ADD
 *   ESP,0x10 cleans the 4 pushes of both 2-arg calls -- the ARG_COUNT hazard on
 *   hs_return ("cleanup=4 vs decl=2") is the same FALSE POSITIVE documented on
 *   the twins; hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x96670 = device_group_change_only_once_more_set(int device_group_index,
 *             char change_only_once_more); its stale
 *             `void device_group_change_only_once_more_set(void);` kb decl was
 *             corrected here -- calling through a (void) decl would have left
 *             8 bytes of drift (the a10 object-list dropped-arg class). The
 *             first parameter is declared `int` because only the CALLER's load
 *             width is proven (a signed 16-bit field); the push is a full
 *             sign-extended dword and the callee's own parameter width is not
 *             observable from here. The sign extension is expressed in the
 *             source by the int16_t deref, not by the decl. Callee is unported,
 *             so the change is decl-only. Arg names are mechanical, taken from
 *             the existing symbol name (naming-confidence: behaviour-only).
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfcd0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    device_group_change_only_once_more_set(
      *(int16_t *)record, (char)*(unsigned char *)((char *)record + 4));
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bfd10 @ 0x000bfd10
 *
 * HaloScript builtin dispatcher for a ZERO-ARGUMENT script function, the
 * immediate neighbour of FUN_000bfcd0 above. It is the DEGENERATE member of
 * the handler family that fills the tail of this TU: because the script
 * builtin takes no arguments there is no argument record to evaluate, so the
 * hs_macro_function_evaluate call and its NULL check -- present in every twin
 * from FUN_000bf870 through FUN_000bfcd0 -- are simply absent. What remains is
 * the worker call followed by the constant script return.
 *
 * Ghidra modelled the function as void(void), so the cdecl STACK parameters
 * surfaced as an `in_stack_00000008` pseudo-local (lift-learnings 31 void-decl
 * trap). These are STACK args, not @<reg>: no unaff_/in_EAX/in_ECX appears
 * anywhere in the decompile, and the body has no register-defining prologue.
 * The stale `void FUN_000bfd10(void);` kb.json decl was corrected to the 3-arg
 * cdecl form as part of this lift; a (void) decl over a stack-arg callee is
 * the ESP-drift class of bug from 0x158df0.
 *
 * Binary evidence (the ENTIRE body, 10 instructions, 0xbfd10-0xbfd27; cdecl
 * prologue PUSH EBP / MOV EBP,ESP with no SUB ESP, no _chkstk, no callee-saved
 * pushes, no locals, no FPU, no SEH, no buffers, no struct deref, no loops and
 * no branches -- straight-line, so there is no register-aliasing ambiguity and
 * no buffer-alias risk anywhere in this function):
 *
 *   CALL 0x1459d0 is emitted with NO preceding pushes and NO stack cleanup
 *   afterwards -> breakable_surfaces_reset(), 0 args, matching its kb decl.
 *   It comes FIRST, before the script return, and that ordering is the whole
 *   observable effect of the builtin. (Ghidra's label for this callee reads
 *   `breakable_surfaces_initialize_for_new_map`; the kb.json name
 *   breakable_surfaces_reset is used instead, per that entry. The callee is
 *   ported=false and still thunks to the original -- it is declared in kb and
 *   callable by name, so no stub is added here.)
 *
 *   MOV EAX,[EBP+0xc] / PUSH 0x0 / PUSH EAX / CALL 0xcbf80 -> cdecl reverse
 *   push order gives C order hs_return(thread_datum, 0). The script return
 *   value is the LITERAL 0, not any computed result. [EBP+0xc] is the SECOND
 *   cdecl stack parameter, i.e. thread_datum, matching the slot every twin in
 *   this family uses for it.
 *
 *   ADD ESP,0x8 cleans exactly those 2 pushes. Unlike the twins -- where ONE
 *   combined ADD ESP,0x10 covers two 2-arg calls and produces the well-known
 *   ARG_COUNT false positive on hs_return -- this function has a single
 *   multi-arg call, so the cleanup is a clean 2-arg confirmation of
 *   hs_return's arity. RET carries no immediate => cdecl, caller cleans.
 *
 * [EBP+0x08] (function_index) and [EBP+0x10] (init) are NEVER read by the
 * body. They are retained in the signature because this is the fixed cdecl
 * shape of the HS builtin handler family -- the dispatch table pushes three
 * arguments at every call site, and declaring fewer would reintroduce the same
 * ESP-drift hazard the corrected decl exists to prevent. Their being unused is
 * a property of THIS builtin (it takes no script arguments), not evidence of a
 * narrower signature.
 *
 * Callees (both cdecl, both in kb.json, no @<reg> args anywhere):
 *   0x1459d0 = breakable_surfaces_reset(void)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfd10(int16_t function_index, int thread_datum, char init)
{
  breakable_surfaces_reset();
  hs_return(thread_datum, 0);
}

/* FUN_000bfd50 @ 0x000bfd50
 *
 * HaloScript builtin dispatcher for a ZERO-ARGUMENT script function, the exact
 * byte-shape twin of FUN_000bfd10 above with a different worker: the builtin
 * takes no script arguments, so the hs_macro_function_evaluate call and its
 * NULL check -- present in every twin from FUN_000bf870 through FUN_000bfcd0 --
 * are simply absent. What remains is the worker call followed by the constant
 * script return.
 *
 * Ghidra modelled the function as void(void), so the cdecl STACK parameters
 * surfaced as an `in_stack_00000008` pseudo-local (lift-learnings 31 void-decl
 * trap). These are STACK args, not @<reg>: no unaff_/in_EAX/in_ECX appears
 * anywhere in the decompile, and the body has no register-defining prologue.
 * The stale `void FUN_000bfd50(void);` kb.json decl was corrected to the 3-arg
 * cdecl form as part of this lift; a (void) decl over a stack-arg callee is
 * the ESP-drift class of bug from 0x158df0.
 *
 * Binary evidence (the ENTIRE body, 8 instructions, 0xbfd50-0xbfd67; cdecl
 * prologue PUSH EBP / MOV EBP,ESP with no SUB ESP, no _chkstk, no callee-saved
 * pushes, no locals, no FPU, no SEH, no buffers, no struct deref, no loops and
 * no branches -- straight-line, so there is no register-aliasing ambiguity and
 * no buffer-alias risk anywhere in this function):
 *
 *   CALL 0xa6a80 is emitted with NO preceding pushes and NO stack cleanup
 *   afterwards (the next instruction is a MOV, not an ADD ESP) -> a 0-argument
 *   call, matching FUN_000a6a80's kb decl `void FUN_000a6a80(void)`. That is
 *   the cheat-all-weapons worker (already ported in game.c, carrying the
 *   contiguous-placement-buffer fix for the 0xf3c90060 tag-index assert); it
 *   is called BY NAME here, never re-lifted or inlined. It comes FIRST, before
 *   the script return, and that ordering is the whole observable effect of the
 *   builtin.
 *
 *   MOV EAX,[EBP+0xc] / PUSH 0x0 / PUSH EAX / CALL 0xcbf80 -> cdecl reverse
 *   push order gives C order hs_return(thread_datum, 0). The script return
 *   value is the LITERAL 0, not the worker's result (the worker is void and
 *   its EAX is never read -- lift-silent-bugs check 4 does not apply, nothing
 *   here consumes an implicit EAX). [EBP+0xc] is the SECOND cdecl stack
 *   parameter, i.e. thread_datum, the slot every twin in this family uses for
 *   it. [EBP+0x08] is never loaded.
 *
 *   ADD ESP,0x8 cleans exactly those 2 pushes. Unlike the twins -- where ONE
 *   combined ADD ESP,0x10 covers two 2-arg calls and produces the well-known
 *   ARG_COUNT false positive on hs_return -- this function has a single
 *   multi-arg call, so the cleanup is a clean 2-arg confirmation of
 *   hs_return's arity. Do NOT "fix" hs_return's decl. RET carries no
 *   immediate => cdecl, caller cleans the incoming args; do not declare it
 *   __stdcall.
 *
 * [EBP+0x08] (function_index) and [EBP+0x10] (init) are NEVER read by the
 * body. They are retained in the signature because this is the fixed cdecl
 * shape of the HS builtin handler family -- the dispatch table pushes three
 * arguments at every call site, and declaring fewer would reintroduce the same
 * ESP-drift hazard the corrected decl exists to prevent. Their being unused is
 * a property of THIS builtin (it takes no script arguments), not evidence of a
 * narrower signature.
 *
 * Callees (both cdecl, both in kb.json, both ported, no @<reg> args anywhere):
 *   0xa6a80 = FUN_000a6a80(void)   -- cheat-all-weapons worker
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfd50(int16_t function_index, int thread_datum, char init)
{
  FUN_000a6a80();
  hs_return(thread_datum, 0);
}

/* FUN_000bfd90 @ 0x000bfd90
 *
 * HaloScript builtin dispatcher for a ZERO-ARGUMENT script function; the exact
 * byte-shape twin of FUN_000bfd50 directly above, differing only in the worker
 * it calls. The builtin takes no script arguments, so the
 * hs_macro_function_evaluate call and its NULL check -- present in the twins
 * from FUN_000bf870 through FUN_000bfcd0 -- are simply absent. What remains is
 * the worker call followed by the constant script return. Do NOT "normalise"
 * this handler to the evaluate/NULL-check shape: there is no evaluate call and
 * no branch anywhere in the body.
 *
 * Ghidra modelled the function as void(void), so the cdecl STACK parameters
 * surfaced as an `in_stack_00000008` pseudo-local (lift-learnings 31 void-decl
 * trap). These are STACK args, not @<reg>: no unaff_/in_EAX/in_ECX appears in
 * the decompile, and the body has no register-defining prologue. The stale
 * `void FUN_000bfd90(void);` kb.json decl was corrected to the 3-arg cdecl form
 * as part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (the ENTIRE body, 10 instructions, 0xbfd90-0xbfda7; cdecl
 * prologue PUSH EBP / MOV EBP,ESP with no SUB ESP, no _chkstk, no callee-saved
 * pushes, no locals, no FPU, no SEH, no buffers, no struct deref, no loops and
 * no branches -- straight-line, so there is no register-aliasing ambiguity, no
 * push-then-fstp float, no struct-field rotation and no buffer-alias risk):
 *
 *   CALL 0xa6830 is emitted with NO preceding pushes and NO stack cleanup
 *   afterwards (the next instruction is a MOV, not an ADD ESP) -> a 0-argument
 *   call, matching cheat_teleport_to_camera's kb decl `void
 *   cheat_teleport_to_camera(void)`. It is called BY NAME, never re-lifted or
 *   inlined. It comes FIRST, before the script return, and that ordering is the
 *   whole observable effect of the builtin.
 *
 *   MOV EAX,[EBP+0xc] / PUSH 0x0 / PUSH EAX / CALL 0xcbf80 -> cdecl reverse
 *   push order (first PUSH is the LAST C argument) gives C order
 *   hs_return(thread_datum, 0). The script return value is the LITERAL 0 from
 *   PUSH 0x0, not the worker's result: cheat_teleport_to_camera is void and its
 *   EAX is never read, so lift-silent-bugs check 4 (void-EAX implicit return,
 *   lift-learnings 16) does not apply -- nothing here consumes an implicit EAX.
 *   [EBP+0xc] is the SECOND cdecl stack parameter, i.e. thread_datum, the slot
 *   every twin in this family uses for it. [EBP+0x08] is never loaded.
 *
 *   ADD ESP,0x8 cleans exactly those 2 pushes -- a clean 2-arg confirmation of
 *   hs_return's arity (this function has a single multi-arg call, so it does
 *   not produce the combined-ADD-ESP,0x10 ARG_COUNT false positive the
 *   evaluate/NULL-check twins do). Do NOT "fix" hs_return's decl. RET carries
 *   no immediate => cdecl, caller cleans the incoming args; do not declare it
 *   __stdcall.
 *
 * [EBP+0x08] (function_index) and [EBP+0x10] (init) are NEVER read by the body.
 * They are retained in the signature because this is the fixed cdecl shape of
 * the HS builtin handler family -- the shared dispatch table pushes three
 * arguments at every call site, and declaring fewer would reintroduce the same
 * ESP-drift hazard the corrected decl exists to prevent. Their being unused is
 * a property of THIS builtin (it takes no script arguments), not evidence of a
 * narrower signature.
 *
 * Callees (both cdecl, both in kb.json, both ported, no @<reg> args anywhere):
 *   0xa6830 = cheat_teleport_to_camera(void)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfd90(int16_t function_index, int thread_datum, char init)
{
  cheat_teleport_to_camera();
  hs_return(thread_datum, 0);
}

/* FUN_000bfdb0 @ 0x000bfdb0
 *
 * HaloScript builtin dispatcher for a ZERO-ARGUMENT script function; the exact
 * byte-shape twin of FUN_000bfd90 directly above, differing only in the worker
 * it calls. The builtin takes no script arguments, so the
 * hs_macro_function_evaluate call and its NULL check -- present in the twins
 * from FUN_000bf870 through FUN_000bfcd0 -- are simply absent. What remains is
 * the worker call followed by the constant script return. Do NOT "normalise"
 * this handler to the evaluate/NULL-check shape: there is no evaluate call and
 * no branch anywhere in the body.
 *
 * Ghidra modelled the function as void(void), so the cdecl STACK parameters
 * surfaced as an `in_stack_00000008` pseudo-local (lift-learnings 31 void-decl
 * trap). These are STACK args, not @<reg>: no unaff_/in_EAX/in_ECX appears in
 * the decompile, and the body has no register-defining prologue. The stale
 * `void FUN_000bfdb0(void);` kb.json decl was corrected to the 3-arg cdecl form
 * as part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (the ENTIRE body, 10 instructions, 0xbfdb0-0xbfdc7; cdecl
 * prologue PUSH EBP / MOV EBP,ESP with no SUB ESP, no _chkstk, no callee-saved
 * pushes, no locals, no FPU, no SEH, no buffers, no struct deref, no loops and
 * no branches -- straight-line, so there is no register-aliasing ambiguity, no
 * push-then-fstp float, no struct-field rotation and no buffer-alias risk):
 *
 *   CALL 0xa68e0 is emitted with NO preceding pushes and NO stack cleanup
 *   afterwards (the next instruction is a MOV, not an ADD ESP) -> a 0-argument
 *   call, matching cheat_all_powerups' kb decl `void cheat_all_powerups(void)`.
 *   It is called BY NAME (the worker already lives in cheats.c), never
 *   re-lifted or inlined. It comes FIRST, before the script return, and that
 *   ordering is the whole observable effect of the builtin.
 *
 *   MOV EAX,[EBP+0xc] / PUSH 0x0 / PUSH EAX / CALL 0xcbf80 -> cdecl reverse
 *   push order (first PUSH is the LAST C argument) gives C order
 *   hs_return(thread_datum, 0). The script return value is the LITERAL 0 from
 *   PUSH 0x0, not the worker's result: cheat_all_powerups is void and its EAX
 *   is never read, so lift-silent-bugs check 4 (void-EAX implicit return,
 *   lift-learnings 16) does not apply -- nothing here consumes an implicit EAX.
 *   [EBP+0xc] is the SECOND cdecl stack parameter, i.e. thread_datum, the slot
 *   every twin in this family uses for it. [EBP+0x08] is never loaded.
 *
 *   ADD ESP,0x8 cleans exactly those 2 pushes -- a clean 2-arg confirmation of
 *   hs_return's arity (this function has a single multi-arg call, so it does
 *   not produce the combined-ADD-ESP,0x10 ARG_COUNT false positive the
 *   evaluate/NULL-check twins do). Do NOT "fix" hs_return's decl. RET carries
 *   no immediate => cdecl, caller cleans the incoming args; do not declare it
 *   __stdcall.
 *
 * [EBP+0x08] (function_index) and [EBP+0x10] (init) are NEVER read by the body.
 * They are retained in the signature because this is the fixed cdecl shape of
 * the HS builtin handler family -- the shared dispatch table pushes three
 * arguments at every call site, and declaring fewer would reintroduce the same
 * ESP-drift hazard the corrected decl exists to prevent. Their being unused is
 * a property of THIS builtin (it takes no script arguments), not evidence of a
 * narrower signature.
 *
 * Callees (both cdecl, both in kb.json, both ported, no @<reg> args anywhere):
 *   0xa68e0 = cheat_all_powerups(void)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfdb0(int16_t function_index, int thread_datum, char init)
{
  cheat_all_powerups();
  hs_return(thread_datum, 0);
}

/* FUN_000bfdd0 @ 0x000bfdd0
 *
 * HaloScript builtin dispatcher for a ONE-ARGUMENT script function; the
 * evaluate/NULL-check twin of FUN_000bf870 / FUN_000bf8b0 / FUN_000bf8f0 /
 * FUN_000bf920 and the rest of the family above. It evaluates the script's
 * argument list, and only if the evaluation produced a record does it invoke
 * the worker and return to the script. Unlike the zero-argument twins
 * immediately above (FUN_000bfd90 / FUN_000bfdb0) the evaluate call and its
 * NULL check ARE present here, so do not "normalise" this one to their shape.
 *
 * Ghidra modelled the function as void(void), so the cdecl STACK parameters
 * surfaced as `in_stack_00000004` / `in_stack_00000008` / `in_stack_0000000c`
 * pseudo-locals (lift-learnings 31 void-decl trap). These are STACK args, not
 * @<reg>: no unaff_/in_EAX/in_ECX appears in the decompile, and the body has no
 * register-defining prologue. The stale `void FUN_000bfdd0(void);` kb.json decl
 * was corrected to the 3-arg cdecl form as part of this lift; a (void) decl
 * over a stack-arg callee is the ESP-drift class of bug from 0x158df0.
 *
 * Binary evidence (the ENTIRE body, 0xbfdd0-0xbfe04; prologue PUSH EBP /
 * MOV EBP,ESP / PUSH ESI, epilogue POP ESI / POP EBP / RET with no immediate
 * => cdecl, caller cleans. No SUB ESP, no _chkstk, no locals, no FPU, no SEH,
 * no buffers, no loops, no jump table -- one forward branch only, so there is
 * no push-then-fstp float, no struct-field rotation and no buffer-alias risk):
 *
 *   Parameter slots: [EBP+0x08] function_index -> ECX, [EBP+0x0c]
 *   thread_datum -> ESI, [EBP+0x10] init -> EAX. ESI is callee-saved and stays
 *   live across the evaluate call, which is exactly why the original preserves
 *   it: it is re-used as hs_return's first argument at the tail. Every register
 *   feeding a PUSH here was loaded from its [EBP+N] slot within the same basic
 *   block, so lift-decompiler-traps hazard 1 (register aliasing over distance)
 *   does not apply.
 *
 *   PUSH EAX / PUSH ESI / PUSH ECX / CALL 0xcc560 -> cdecl reverse push order
 *   (first PUSH is the LAST C argument) gives C order
 *   hs_macro_function_evaluate(function_index, thread_datum, init) -- a
 *   straight pass-through of all three incoming parameters in slot order.
 *   ADD ESP,0xc cleans exactly those 3 pushes, matching the kb decl's arity.
 *
 *   TEST EAX,EAX / JZ 0xbfe02 skips BOTH remaining calls, and EAX is then
 *   dereferenced, so 0xcc560's declared `int` return is really a POINTER to the
 *   evaluated-argument record. It is cast to the record pointer LOCALLY; the
 *   kb.json decl is deliberately left as int, exactly as on every twin in this
 *   cluster, so the shared declaration stays consistent across the family.
 *
 *   XOR EDX,EDX / MOV DX,word ptr [EAX] -> the record's ONLY field is a
 *   ZERO-EXTENDED 16-bit value at offset +0. The width is load-bearing
 *   (lift-learnings 24 LOADW): this is NOT the dword `record[0]` form used by
 *   the FUN_000bf920 twin, and the XOR+MOV DX pairing (rather than MOVSX)
 *   proves it is UNSIGNED. Hence `uint16_t *record` and a plain deref.
 *
 *   VC71 match note: this two-instruction zero-extend is the ENTIRE residual
 *   diff -- 93.6% (23 ours / 24 reference, LCS 22). Our C compiles the widening
 *   to the single `movzx edx,word ptr [eax]`; the original spells it XOR + a
 *   16-bit partial-register MOV. Verified NOT source-recoverable: hoisting the
 *   load into an explicit `uint16_t local_player_index` temp (the shape that
 *   normally forces MSVC's DX partial-register allocation) re-measured at the
 *   identical 93.6% -- VC71 folds the temp straight back into movzx. Both
 *   sequences are semantically identical zero-extends, and equivalence agrees
 *   (100/100 seeds, 0 divergences). Do not chase this instruction; it is a
 *   codegen-selection difference, not a lift defect.
 *
 *   PUSH EDX / CALL 0xa6760 -> a single argument, the zero-extended record
 *   field, i.e. cheat_active_camouflage_local_player(local_player_index).
 *
 *   MOV EAX/PUSH 0x0 / PUSH ESI / CALL 0xcbf80 -> cdecl reverse order gives
 *   hs_return(thread_datum, 0). The script return value is the LITERAL 0 from
 *   PUSH 0x0, not the worker's result: cheat_active_camouflage_local_player is
 *   void and its EAX is never read, so lift-silent-bugs check 4 (void-EAX
 *   implicit return, lift-learnings 16) does not apply here.
 *
 *   The single ADD ESP,0xc at 0xbfdff cleans BOTH tail calls together (1 arg
 *   for 0xa6760 plus 2 args for hs_return). check_lift_hazards therefore
 *   reports an ARG_COUNT finding "cleanup=3 stack args, decl=2" against
 *   hs_return: that is this combined-cleanup artifact, NOT a 3-arg callee. Do
 *   NOT "fix" hs_return's decl -- its arity is independently confirmed by the
 *   clean ADD ESP,0x8 in the single-call twin FUN_000bfdb0 directly above.
 *
 * [EBP+0x08] function_index and [EBP+0x10] init are read only to be forwarded
 * to the evaluate call; nothing else in the body consumes them.
 *
 * Callees (all three cdecl, all in kb.json, all ported, no @<reg> args anywhere
 * in the chain):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record pointer
 *   0xa6760 = cheat_active_camouflage_local_player(int local_player_index)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfdd0(int16_t function_index, int thread_datum, char init)
{
  uint16_t *record;

  record =
    (uint16_t *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    cheat_active_camouflage_local_player((int)*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bfe10 @ 0x000bfe10
 *
 * HaloScript builtin dispatcher for a ZERO-ARGUMENT script function: it
 * unconditionally invokes the worker (reload the cheat table from file) and
 * then commits the calling script thread with a literal 0 result. This is a
 * byte-shape twin of FUN_000bfdb0 / FUN_000bfd90 above, differing ONLY in the
 * worker address; unlike FUN_000bfdd0 there is no argument-list evaluate call
 * and no NULL check, so do not "normalise" this one to that shape.
 *
 * Ghidra modelled the function as void(void), so the cdecl STACK parameter
 * surfaced as an `in_stack_00000008` pseudo-local (lift-learnings 31 void-decl
 * trap). That name is relative to the POST-prologue frame and is NOT arg1: the
 * disassembly reads [EBP+0x0c], i.e. the SECOND cdecl stack slot. These are
 * STACK args, not @<reg>: no unaff_/in_EAX/in_ECX appears in the decompile and
 * the body has no register-defining prologue. The stale
 * `void FUN_000bfe10(void);` kb.json decl was corrected to the 3-arg cdecl form
 * as part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (the ENTIRE body, 0xbfe10-0xbfe27; prologue PUSH EBP /
 * MOV EBP,ESP with no SUB ESP, no _chkstk, no PUSH of callee-saved registers;
 * epilogue POP EBP / RET with no immediate => cdecl, caller cleans. No locals,
 * no FPU, no SEH, no buffers, no loops, no branches at all and no jump table,
 * so there is no push-then-fstp float, no struct-field rotation, no
 * buffer-alias risk and no register-aliasing-over-distance risk):
 *
 *   CALL 0x000a66d0 -> cheats_load_from_file(). Zero arguments, no ESP cleanup
 *   after the call, confirming the void(void) arity in kb.json. It is called
 *   FIRST and UNCONDITIONALLY -- side-effect order is worker-then-return.
 *
 *   MOV EAX,[EBP+0x0c] -> arg2 = thread_datum, loaded in the same basic block
 *   as the PUSH that consumes it. [EBP+0x08] function_index and [EBP+0x10]
 *   init are NEVER read anywhere in the body; they are declared purely to keep
 *   the shared HS evaluator ABI (and therefore the caller's push sequence)
 *   intact across the family.
 *
 *   PUSH 0x0 / PUSH EAX / CALL 0x000cbf80 -> cdecl reverse push order (first
 *   PUSH is the LAST C argument) gives hs_return(thread_datum, 0). The script
 *   return value is the LITERAL 0 from PUSH 0x0, not the worker's result:
 *   cheats_load_from_file is void and its EAX is never read, so
 *   lift-silent-bugs check 4 (void-EAX implicit return, lift-learnings 16) does
 *   not apply here.
 *
 *   ADD ESP,0x8 at 0xbfe23 cleans exactly hs_return's two pushes -- a clean,
 *   uncombined cleanup that independently re-confirms hs_return's arity of 2
 *   (unlike the combined ADD ESP,0xc in FUN_000bfdd0, which produces a
 *   spurious ARG_COUNT hazard finding).
 *
 * Callees (both cdecl, both in kb.json, both ported, no @<reg> args anywhere):
 *   0xa66d0 = cheats_load_from_file(void)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfe10(int16_t function_index, int thread_datum, char init)
{
  cheats_load_from_file();
  hs_return(thread_datum, 0);
}

/* FUN_000bfe30 @ 0x000bfe30
 *
 * HaloScript builtin dispatcher for a ONE-ARGUMENT script function: it
 * evaluates the script's argument list and, only if the evaluation produced a
 * record, forwards the record's single byte field to the worker
 * (ai_globals_ai_active) and commits the calling script thread with a literal 0
 * result. This is the evaluate/NULL-check shape of the family -- the twin of
 * FUN_000bfdd0 above, differing only in the worker address and in the WIDTH of
 * the record field it reads. Do NOT "normalise" it to the zero-argument shape
 * of FUN_000bfe10 / FUN_000bfdb0 directly above: the evaluate call and its
 * forward branch are both present here.
 *
 * Ghidra modelled the function as void(void), so the cdecl STACK parameters
 * surfaced as `in_stack_00000004` / `in_stack_00000008` / `in_stack_0000000c`
 * pseudo-locals (lift-learnings 31 void-decl trap). Those names are relative to
 * the POST-prologue frame and are NOT register args: no unaff_/in_EAX/in_ECX
 * appears in the decompile, and the body has no register-defining prologue, so
 * this is not a skip_reg_args case. The stale `void FUN_000bfe30(void);`
 * kb.json decl was corrected to the 3-arg cdecl form as part of this lift; a
 * (void) decl over a stack-arg callee is the ESP-drift class of bug from
 * 0x158df0.
 *
 * Binary evidence (the ENTIRE body, 0xbfe30-0xbfe63; prologue PUSH EBP /
 * MOV EBP,ESP / PUSH ESI, epilogue POP ESI / POP EBP / RET with no immediate
 * => cdecl, caller cleans. No SUB ESP, no _chkstk, no locals, no FPU, no SEH,
 * no buffers, no loops and no jump table -- one forward branch only, so there
 * is no push-then-fstp float, no struct-field rotation and no buffer-alias
 * risk):
 *
 *   Parameter slots, all three loaded in the prologue's own basic block:
 *   EAX <- [EBP+0x10] init, ECX <- [EBP+0x08] function_index, ESI <-
 *   [EBP+0x0c] thread_datum. ESI is the ONLY callee-saved register used, which
 *   is exactly why the original preserves it: thread_datum must stay live
 *   across the evaluate call to feed hs_return at the tail. Every register
 *   feeding a PUSH was written from its [EBP+N] slot in that same block, so
 *   lift-decompiler-traps hazard 1 (register aliasing over distance) does not
 *   apply.
 *
 *   PUSH EAX / PUSH ESI / PUSH ECX / CALL 0xcc560 -> cdecl reverse push order
 *   (the first PUSH is the LAST C argument) gives C order
 *   hs_macro_function_evaluate(function_index, thread_datum, init) -- a
 *   straight pass-through of all three incoming parameters in slot order.
 *   ADD ESP,0xc cleans exactly those 3 pushes, matching the kb decl's arity.
 *
 *   TEST EAX,EAX / JZ (to the epilogue) skips BOTH remaining calls, and EAX is
 *   then dereferenced, so 0xcc560's declared `int` return is really a POINTER
 *   to the evaluated-argument record. It is cast to the record pointer LOCALLY;
 *   the kb.json decl is deliberately left as int, exactly as on every twin in
 *   this cluster, so the shared declaration stays consistent across the family.
 *
 *   XOR EDX,EDX / MOV DL,byte ptr [EAX] -> the record's ONLY field is a
 *   ZERO-EXTENDED 8-bit value at offset +0. The width is load-bearing
 *   (lift-learnings 24 LOADW): this is neither the dword `record[0]` form of
 *   the FUN_000bf920 twin nor the 16-bit MOV DX form of FUN_000bfdd0, and the
 *   XOR+MOV DL pairing (rather than MOVSX) proves it is UNSIGNED. Hence
 *   `uint8_t *record` and a plain deref; reading it as an int would be a
 *   LOADW-class bug.
 *
 *   PUSH EDX / CALL 0x3f770 -> a single argument, that zero-extended byte, i.e.
 *   ai_globals_ai_active(<flag>). The kb decl's parameter type is `char`, so
 *   the deref is cast at the call site rather than widening the shared decl.
 *
 *   PUSH 0x0 / PUSH ESI / CALL 0xcbf80 -> cdecl reverse order gives
 *   hs_return(thread_datum, 0). The script return value is the LITERAL 0 from
 *   PUSH 0x0, not the worker's result: ai_globals_ai_active is void and its EAX
 *   is never read, so lift-silent-bugs check 4 (void-EAX implicit return,
 *   lift-learnings 16) does not apply here.
 *
 *   The single ADD ESP,0xc at 0xbfe5e cleans BOTH tail calls together (1 arg
 *   for 0x3f770 plus 2 args for hs_return). check_lift_hazards therefore
 *   reports an ARG_COUNT finding "hs_return cleanup=3 stack args, decl=2":
 *   that is this MERGED-cleanup artifact (MSVC folds consecutive cdecl
 *   cleanups), NOT a 3-arg callee. Do NOT "fix" hs_return's decl -- its arity
 *   of 2 is independently confirmed by the clean, uncombined ADD ESP,0x8 in the
 *   single-call twins FUN_000bfe10 / FUN_000bfdb0 directly above.
 *
 * [EBP+0x08] function_index and [EBP+0x10] init are read only to be forwarded
 * to the evaluate call; nothing else in the body consumes them.
 *
 * Callees (all three cdecl, all in kb.json, all ported, no @<reg> args anywhere
 * in the chain):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record pointer
 *   0x3f770 = ai_globals_ai_active(char)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfe30(int16_t function_index, int thread_datum, char init)
{
  uint8_t *record;

  record =
    (uint8_t *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    ai_globals_ai_active((char)*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bfe70 @ 0x000bfe70
 *
 * HaloScript builtin dispatcher; byte-shape twin of FUN_000bfe30 directly
 * above -- same three-call skeleton, differing ONLY in the middle worker
 * (0x3f7b0 ai_globals_dialogue_triggers_enabled instead of 0x3f770
 * ai_globals_ai_active).
 *
 * ABI: PUSH EBP / MOV EBP,ESP / PUSH ESI, no _chkstk and no locals, RET with
 * no immediate -> plain cdecl over three incoming stack slots. Ghidra reported
 * them as `in_stack_00000004/8/c` because the stale kb.json decl said
 * `void FUN_000bfe70(void);`; the decl is corrected here to the 3-arg cdecl
 * form used by every twin in this cluster.
 *
 *   [EBP+0x08] -> ECX  int16_t function_index
 *   [EBP+0x0c] -> ESI  int     thread_datum  (held in ESI across the body)
 *   [EBP+0x10] -> EAX  char    init
 *
 * Call-site verification (every register feeding a PUSH is loaded from its
 * [EBP+N] slot in the same prologue block, so lift-decompiler-traps hazard 1,
 * register aliasing over distance, does not apply):
 *
 *   PUSH EAX / PUSH ESI / PUSH ECX / CALL 0xcc560 -- cdecl reverse push order
 *   (first PUSH is the LAST C argument) gives
 *   hs_macro_function_evaluate(function_index, thread_datum, init), a straight
 *   pass-through in slot order. ADD ESP,0xc cleans exactly those 3 pushes.
 *
 *   TEST EAX,EAX / JZ (to the epilogue) skips BOTH remaining calls, and EAX is
 *   then dereferenced, so 0xcc560's declared `int` return is really a POINTER
 *   to the evaluated-argument record. Cast to the record pointer LOCALLY; the
 *   kb.json decl stays `int` so the shared declaration matches every twin.
 *
 *   XOR EDX,EDX / MOV DL,byte ptr [EAX] -- the record's only consumed field is
 *   a ZERO-EXTENDED 8-bit value at offset +0. The width and signedness are
 *   load-bearing (lift-learnings 24 LOADW): this is NOT the dword `record[0]`
 *   form of FUN_000bf920 (offset +4) nor a 16-bit MOV DX, and the XOR+MOV DL
 *   pairing rather than MOVSX proves UNSIGNED. Hence `uint8_t *record` and a
 *   plain deref; reading it as an int would be a LOADW-class bug.
 *
 *   PUSH EDX / CALL 0x3f7b0 -- one argument, that zero-extended byte, i.e.
 *   ai_globals_dialogue_triggers_enabled(<flag>). Its kb decl parameter type is
 *   `char`, so the deref is cast at the call site rather than widening the
 *   shared decl.
 *
 *   PUSH 0x0 / PUSH ESI / CALL 0xcbf80 -- cdecl reverse order gives
 *   hs_return(thread_datum, 0). The script return value is the LITERAL 0 from
 *   PUSH 0x0, not the worker's result: ai_globals_dialogue_triggers_enabled is
 *   void and its EAX is never read, so lift-silent-bugs check 4 (void-EAX
 *   implicit return, lift-learnings 16) does not apply.
 *
 *   The single ADD ESP,0xc at 0xbfea1 cleans BOTH tail calls together (1 arg
 *   for 0x3f7b0 plus 2 args for hs_return). check_lift_hazards therefore
 *   reports an ARG_COUNT finding "hs_return cleanup=3 stack args, decl=2":
 *   that is this MERGED-cleanup artifact (MSVC folds consecutive cdecl
 *   cleanups), NOT a 3-arg callee. Do NOT "fix" hs_return's decl -- its arity
 *   of 2 is independently confirmed by the clean, uncombined ADD ESP,0x8 in the
 *   single-call twins FUN_000bfe10 / FUN_000bfdb0 above.
 *
 * [EBP+0x08] function_index and [EBP+0x10] init are read only to be forwarded
 * to the evaluate call; nothing else in the body consumes them. No FPU ops, no
 * struct stores, no buffers passed (buffer_alias: 0 hits), no jump table, no
 * SEH, no @<reg> callees anywhere in the chain.
 *
 * Callees (all three cdecl, all in kb.json, all ported):
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> record pointer
 *   0x3f7b0 = ai_globals_dialogue_triggers_enabled(char)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfe70(int16_t function_index, int thread_datum, char init)
{
  uint8_t *record;

  record =
    (uint8_t *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    ai_globals_dialogue_triggers_enabled((char)*record);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bff70 @ 0x000bff70
 *
 * HaloScript builtin dispatcher, byte-shape twin of FUN_000bf920 /
 * FUN_000bf960 above: identical 3-parameter cdecl shape and the same evaluate /
 * NULL-check / two-argument worker / hs_return skeleton. The only structural
 * difference from those two twins is the WIDTH of the second record field --
 * this one reads two FULL DWORDs, not a dword plus a zero-extended byte.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xbff70-0xbffa5. No locals, no _chkstk, no SUB ESP, no FPU, no
 * SEH. ESI is the only callee-saved register and holds thread_datum live across
 * the evaluate call. The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bff70(void);` decl was corrected to the 3-arg cdecl form as part
 * of this lift; a (void) decl over a stack-arg callee is the ESP-drift class of
 * bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 (0xbff80) pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) /
 *   ECX([EBP+0x8]) in cdecl reverse order -> C order (function_index,
 *   thread_datum, init); ADD ESP,0xc = 3 args. Straight pass-through.
 *
 *   TEST EAX,EAX / JZ 0xbffa3 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin above; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref (2 fields, TWO FULL DWORDS): MOV EDX,dword ptr [EAX+0x4] ->
 *   32-bit read at record+4; MOV EAX,dword ptr [EAX] -> 32-bit read at
 *   record+0. There is no XOR/MOV DL and no MOVSX anywhere, so unlike the
 *   0xbf920/0xbf960 twins there is NO narrow-load (lift-learnings 24 LOADW)
 *   caveat here -- `record[1]` is the correct width. Only +0x0 and +0x4 are
 *   touched, one deref each -- no buffer-alias risk.
 *
 *   CALL 0x54860 (0xbff93) pushes EDX (record+4) then EAX (record+0) = cdecl
 *   reverse -> C order (record[0], record[1]). Two distinct reloads, NOT a
 *   duplicated argument.
 *
 *   CALL 0xcbf80 (0xbff9b) pushes 0x0 then ESI -> hs_return(thread_datum, 0).
 *   The script return value is the CONSTANT 0, not the record and not the
 *   worker's result (FUN_00054860 is void); the entire observable effect is the
 *   0x54860 side effect. ONE combined ADD ESP,0x10 at 0xbffa0 cleans the 4
 *   pushes of both 2-arg calls -- the ARG_COUNT hazard on hs_return
 *   ("cleanup=4 vs decl=2") is that same FALSE POSITIVE documented on the
 *   twins; hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Reloc audit of delinked/functions/000bff70.obj: exactly 3 DISP32 targets in
 * this function's range -- FUN_000cc560, FUN_00054860, FUN_000cbf80 --
 * matching the three calls below with no extra global or string reference.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x54860  = FUN_00054860(int unit_handle, unsigned int ai_ref)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bff70(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_00054860(record[0], (unsigned int)record[1]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000bfff0 @ 0x000bfff0
 *
 * HaloScript builtin dispatcher, structural twin of FUN_000bff70 /
 * FUN_000bf920 above: identical 3-parameter cdecl shape and the same
 * evaluate / NULL-check / two-argument worker / hs_return skeleton. The only
 * difference from FUN_000bff70 is the worker it dispatches to -- here the
 * encounters-side ai_attach_free helper FUN_00057770 rather than
 * FUN_00054860.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xbfff0-0xc0025. No locals, no _chkstk, no SUB ESP, no FPU, no
 * SEH. ESI is the only callee-saved register and holds thread_datum live
 * across the evaluate call, which is precisely why it is saved. The exit RET
 * carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX (loaded as a full dword, but
 *                                          declared char to match every twin
 *                                          in this family and the callee decl)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000bfff0(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   CALL 0xcc560 (0xc0000) pushes EAX([EBP+0x10]) / ESI([EBP+0xc]) /
 *   ECX([EBP+0x8]) in cdecl reverse order -> C order (function_index,
 *   thread_datum, init); ADD ESP,0xc = 3 args. Straight pass-through.
 *
 *   TEST EAX,EAX / JZ 0xc0023 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin above; the cast
 *   is local and the kb decl is left alone).
 *
 *   Record deref (2 fields, TWO FULL DWORDS): MOV EDX,dword ptr [EAX+0x4] ->
 *   32-bit read at record+4; MOV EAX,dword ptr [EAX] -> 32-bit read at
 *   record+0. There is no MOVSX/MOVZX and no XOR/MOV DL anywhere, so unlike
 *   the 0xbf920/0xbf960 twins there is NO narrow-load (lift-learnings 24
 *   LOADW) caveat here -- both `record[0]` and `record[1]` are full width.
 *   Only +0x0 and +0x4 are touched, one deref each -- no buffer-alias risk.
 *
 *   CALL 0x57770 (0xc0013) pushes EDX (record+4) then EAX (record+0) = cdecl
 *   reverse -> C order (record[0], record[1]). Two distinct reloads, NOT a
 *   duplicated argument. FUN_00057770 is ai_attach_free(unit_handle,
 *   actv_tag_index): param 1 is `unsigned int` per its kb decl, matching the
 *   untruncated dword load, so record[0] is cast rather than narrowed.
 *
 *   CALL 0xcbf80 (0xc001b) pushes 0x0 then ESI -> hs_return(thread_datum, 0).
 *   The script return value is the literal CONSTANT 0, not a record field and
 *   not the worker's result (FUN_00057770 is void); the entire observable
 *   effect is the 0x57770 side effect. ONE combined ADD ESP,0x10 at 0xc0020
 *   cleans the 4 pushes of both 2-arg calls -- the ARG_COUNT hazard on
 *   hs_return ("cleanup=4 vs decl=2") is that same FALSE POSITIVE documented
 *   on the twins; hs_return really takes 2 args, do NOT "fix" its decl.
 *
 * Reloc audit of delinked/functions/000bfff0.obj: exactly 3 DISP32 targets in
 * this function's range -- FUN_000cc560 (+0x11), FUN_00057770 (+0x24),
 * FUN_000cbf80 (+0x2c) -- matching the three calls below with no extra global
 * or string reference.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x57770  = FUN_00057770(unsigned int unit_handle, int actv_tag_index)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000bfff0(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_00057770((unsigned int)record[0], record[1]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000c0030 @ 0x000c0030
 *
 * HaloScript builtin dispatcher, structural twin of FUN_000bfff0 /
 * FUN_000bff70 / FUN_000bf920 above: identical 3-parameter cdecl shape and
 * the same evaluate / NULL-check / worker / hs_return skeleton. It differs
 * from the twins only in the worker it dispatches to (FUN_00054ac0) and in
 * the worker's arity -- ONE argument here, not two.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xc0030-0xc0061 (50 bytes). No locals, no SUB ESP, no _chkstk,
 * no FPU, no SEH, no stack buffers, no struct stores. ESI is the only
 * callee-saved register and holds thread_datum live across the evaluate
 * call, which is why it is saved. The exit RET carries no immediate =>
 * cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX (loaded as a full dword but
 *                                          declared char to match the callee
 *                                          decl and every twin in this family)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000c0030(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   MOV EAX,[EBP+0x10] / MOV ECX,[EBP+0x8] / MOV ESI,[EBP+0xc] then
 *   PUSH EAX / PUSH ESI / PUSH ECX; CALL 0xcc560; ADD ESP,0xc. cdecl reverse
 *   push order -> C order (function_index, thread_datum, init) = a straight
 *   pass-through of all three params. ESI is assigned exactly once from
 *   [EBP+0xc] and is never reloaded, so the Ghidra register-aliasing trap
 *   (decompiler-traps 1) cannot apply to the later PUSH ESI.
 *
 *   TEST EAX,EAX / JZ 0xc005f skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin above; the cast
 *   is local and the kb decl is left alone).
 *
 *   Record deref (ONE field, ONE FULL DWORD): MOV EDX,dword ptr [EAX] -> a
 *   32-bit read at record+0. There is no MOVSX/MOVZX here, unlike the
 *   0xbf920/0xbf960 twins that narrow record+0 to int16 -- so record[0] is
 *   full width and must NOT be narrowed (lift-learnings 24 LOADW, in
 *   reverse). Only +0x0 is touched, one deref, no buffer-alias risk.
 *
 *   CALL 0x54ac0 (0xc0054) is preceded by a single PUSH EDX = ONE argument,
 *   record[0], matching FUN_00054ac0(int unit_handle).
 *
 *   CALL 0xcbf80 (0xc005a) is preceded by PUSH 0x0 then PUSH ESI = cdecl
 *   reverse -> hs_return(thread_datum, 0). The script return value is the
 *   literal CONSTANT 0, not a record field and not the worker's result
 *   (FUN_00054ac0 is void); the entire observable effect is the 0x54ac0 side
 *   effect. ONE combined ADD ESP,0xc at 0xc005f cleans the 1 push of the
 *   0x54ac0 call plus the 2 pushes of the hs_return call -- the ARG_COUNT
 *   hazard on hs_return ("cleanup=3 vs decl=2") is the same FALSE POSITIVE
 *   documented on the twins; hs_return really takes 2 args, do NOT "fix" its
 *   decl.
 *
 * Reloc audit of delinked/functions/000c0030.obj: exactly 3 DISP32 targets in
 * this function's range -- FUN_000cc560 (+0x11), FUN_00054ac0 (+0x20),
 * FUN_000cbf80 (+0x28) -- matching the three calls below with no extra global
 * or string reference.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x54ac0  = FUN_00054ac0(int unit_handle)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000c0030(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_00054ac0(record[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000c0070 @ 0x000c0070
 *
 * HaloScript builtin dispatcher, direct structural twin of FUN_000c0030
 * immediately above: identical 3-parameter cdecl shape, identical
 * evaluate / NULL-check / worker / hs_return skeleton, and the same
 * one-argument worker arity. The ONLY difference between the two functions
 * is the worker dispatched to -- 0x54b20 here vs 0x54ac0 there.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * 24 instructions, no locals, no SUB ESP, no _chkstk, no FPU, no SEH, no
 * stack buffers, no struct stores. ESI is the only callee-saved register and
 * holds thread_datum live across the evaluate call, which is why it is saved.
 * The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX  (pushed as a full dword; the
 *                                          int16 narrowing lives inside the
 *                                          callee, matching every twin)
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000c0070(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   MOV EAX,[EBP+0x10] / MOV ECX,[EBP+0x8] / MOV ESI,[EBP+0xc] then
 *   PUSH EAX / PUSH ESI / PUSH ECX; CALL 0xcc560 (0xc0080); ADD ESP,0xc.
 *   cdecl reverse push order -> C order (function_index, thread_datum, init)
 *   = a straight pass-through of all three params. ESI is written exactly
 *   once at 0xc007a and never reloaded, so the Ghidra register-aliasing trap
 *   (decompiler-traps 1) cannot apply to the later PUSH ESI.
 *
 *   TEST EAX,EAX / JZ skips BOTH remaining calls when the result is NULL, so
 *   the 0xcc560 return is a POINTER that is dereferenced even though kb.json
 *   declares it as returning int (same as every twin; the cast is local and
 *   the kb decl is left alone).
 *
 *   Record deref (ONE field, ONE FULL DWORD): MOV EDX,dword ptr [EAX] -- a
 *   32-bit read at record+0, no MOVSX/MOVZX, so record[0] must NOT be
 *   narrowed (lift-learnings 24 LOADW, in reverse). Only +0x0 is touched,
 *   one deref, no buffer-alias risk.
 *
 *   CALL 0x54b20 (0xc008f) is preceded by a single PUSH EDX = ONE argument,
 *   record[0], matching FUN_00054b20(int parent_handle).
 *
 *   CALL 0xcbf80 (0xc0097) is preceded by PUSH 0x0 then PUSH ESI = cdecl
 *   reverse -> hs_return(thread_datum, 0). The script return value is the
 *   literal CONSTANT 0, not a record field and not the worker's result
 *   (FUN_00054b20 is void). ONE combined ADD ESP,0xc at 0xc009c cleans the
 *   1 push of the 0x54b20 call plus the 2 pushes of the hs_return call --
 *   the ARG_COUNT hazard on hs_return ("cleanup=3 vs decl=2") is the same
 *   FALSE POSITIVE documented on the twins; hs_return really takes 2 args,
 *   do NOT "fix" its decl.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x54b20  = FUN_00054b20(int parent_handle)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000c0070(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_00054b20(record[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000c00b0 @ 0x000c00b0
 *
 * HaloScript builtin dispatcher, direct structural twin of FUN_000c0070
 * immediately above: identical 3-parameter cdecl shape, identical
 * evaluate / NULL-check / worker / hs_return skeleton, and the same
 * one-argument worker arity. The ONLY difference between the two functions
 * is the worker dispatched to -- 0x54bb0 here vs 0x54b20 there.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * No locals, no SUB ESP, no _chkstk, no FPU, no SEH, no stack buffers, no
 * struct stores. ESI is the only callee-saved register and holds
 * thread_datum live across the evaluate call, which is why it is saved.
 * The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX  (pushed as a full dword; the
 *                                          int16 narrowing lives inside the
 *                                          callee, matching every twin)
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000c00b0(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   0xc00b3 MOV EAX,[EBP+0x10] / 0xc00b6 MOV ECX,[EBP+0x8] /
 *   0xc00ba MOV ESI,[EBP+0xc] then PUSH EAX / PUSH ESI / PUSH ECX;
 *   CALL 0xcc560 (0xc00c0); ADD ESP,0xc. cdecl reverse push order -> C order
 *   (function_index, thread_datum, init) = a straight pass-through.
 *   0xc00cc MOV EDX,dword ptr [EAX] is a FULL 32-bit load from record+0 --
 *   NOT the MOVSX word seen at +0 in the FUN_000bdfa0-family handlers -- so
 *   the handle is kept 32-bit wide and passed unnarrowed to 0x54bb0, whose
 *   kb decl is `void FUN_00054bb0(unsigned int ai_ref)`.
 *   0xc00ce PUSH EDX; CALL 0x54bb0. 0xc00d4 PUSH 0 / PUSH ESI;
 *   CALL 0xcbf80 (hs_return). The single 0xc00dc ADD ESP,0xc is the SHARED
 *   cleanup for all three pushes across those two calls (1 + 2 = 3 dwords);
 *   call_site_audit reads it as a 3-arg hs_return, which is a false positive
 *   -- both callee decls are correct and were left unchanged.
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000c00b0(int16_t function_index, int thread_datum, char init)
{
  unsigned int *record;

  record = (unsigned int *)hs_macro_function_evaluate(function_index,
                                                      thread_datum, init);
  if (record != NULL) {
    FUN_00054bb0(record[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000c00f0 @ 0x000c00f0
 *
 * HaloScript builtin dispatcher, direct structural twin of FUN_000c0030 /
 * FUN_000c0070 / FUN_000c00b0 above: identical 3-parameter cdecl shape,
 * identical evaluate / NULL-check / worker / hs_return skeleton, and the same
 * one-argument worker arity. The ONLY difference from FUN_000c00b0 is the
 * worker dispatched to -- 0x54ca0 here vs 0x54bb0 there.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * 50 bytes (0xc00f0-0xc0121), no locals, no SUB ESP, no _chkstk, no FPU, no
 * SEH, no stack buffers, no struct stores. ESI is the only callee-saved
 * register and holds thread_datum live across the evaluate call, which is why
 * it is saved. The exit RET carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX  (pushed as a full dword; the
 *                                          int16 narrowing lives inside the
 *                                          callee, matching every twin)
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX (loaded as a full dword;
 *                                          declared char to match the callee
 *                                          decl and every twin)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000c00f0(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   PUSH EAX(init) / PUSH ESI(thread_datum) / PUSH ECX(function_index);
 *   CALL 0xcc560 (0xc0100); ADD ESP,0xc. cdecl reverse push order -> C order
 *   (function_index, thread_datum, init) = a straight pass-through of all
 *   three params. ESI is written exactly once and never reloaded, so the
 *   Ghidra register-aliasing trap (decompiler-traps 1) cannot apply to the
 *   later PUSH ESI.
 *
 *   TEST EAX,EAX / JZ 0xc011f skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref (ONE field, ONE FULL DWORD): MOV EDX,dword ptr [EAX] at
 *   0xc010c -- a 32-bit read at record+0, no MOVSX/MOVZX, so record[0] must
 *   NOT be narrowed (lift-learnings 24 LOADW, in reverse). Only +0x0 is
 *   touched, one deref, no buffer-alias risk.
 *
 *   CALL 0x54ca0 (0xc010f) is preceded by a single PUSH EDX = ONE argument,
 *   record[0], matching FUN_00054ca0(unsigned int ai_ref) -- note the
 *   unsigned param here (Ghidra also typed the record as uint*), unlike the
 *   int worker in the 0xc0030 twin.
 *
 *   CALL 0xcbf80 (0xc0117) is preceded by PUSH 0x0 then PUSH ESI = cdecl
 *   reverse -> hs_return(thread_datum, 0). The script return value is the
 *   literal CONSTANT 0, not a record field and not the worker's result (the
 *   worker is void); the entire observable effect is the worker's side effect.
 *
 *   ONE combined ADD ESP,0xc at 0xc011c cleans the 1 push of the 0x54ca0 call
 *   plus the 2 pushes of the hs_return call -- so call_site_audit's ARG_COUNT
 *   finding on hs_return ("cleanup=3 stack args, decl=2") is a FALSE POSITIVE;
 *   hs_return really takes 2 args, and its kb decl was left unchanged (same
 *   dismissal as on the committed twins 0xc0030 / 0xbfff0 / 0xbff70).
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000c00f0(int16_t function_index, int thread_datum, char init)
{
  unsigned int *record;

  record = (unsigned int *)hs_macro_function_evaluate(function_index,
                                                      thread_datum, init);
  if (record != NULL) {
    FUN_00054ca0(record[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000c0130 @ 0x000c0130
 *
 * HaloScript builtin dispatcher, direct structural twin of FUN_000c00f0
 * immediately above (and of FUN_000c0070 / FUN_000c0030 / FUN_000bfff0 /
 * FUN_000bff70 before it): identical 3-parameter cdecl shape, identical
 * evaluate / NULL-check / worker / hs_return skeleton, and the same
 * one-argument worker arity. The ONLY difference from the 0xc00f0 twin is
 * the worker dispatched to -- 0x54d00 here vs 0x54ca0 there.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xc0130-0xc0161 (50 bytes). No locals, no SUB ESP, no _chkstk,
 * no FPU, no SEH, no stack buffers, no struct stores, no globals. ESI is the
 * only callee-saved register and holds thread_datum live across the evaluate
 * call, which is why it is saved. The exit RET carries no immediate =>
 * cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX  (pushed as a full dword; the
 *                                          int16 narrowing lives inside the
 *                                          callee, matching every twin)
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000c0130(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   MOV EAX,[EBP+0x10] (0xc0133) / MOV ECX,[EBP+0x8] (0xc0136) /
 *   MOV ESI,[EBP+0xc] (0xc013a) then PUSH EAX / PUSH ESI / PUSH ECX;
 *   CALL 0xcc560 (0xc0140); ADD ESP,0xc. cdecl reverse push order -> C order
 *   (function_index, thread_datum, init) = a straight pass-through of all
 *   three params. ESI is written exactly once at 0xc013a and is never
 *   reloaded, so the Ghidra register-aliasing trap (decompiler-traps 1)
 *   cannot apply to the later PUSH ESI.
 *
 *   TEST EAX,EAX / JZ 0xc015f skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref (ONE field, ONE FULL DWORD): MOV EDX,dword ptr [EAX] at
 *   0xc014c -- a 32-bit read at record+0, no MOVSX/MOVZX, so record[0] must
 *   NOT be narrowed (lift-learnings 24 LOADW, in reverse). Only +0x0 is
 *   touched, one deref, no buffer-alias risk.
 *
 *   CALL 0x54d00 (0xc014f) is preceded by a single PUSH EDX = ONE argument,
 *   record[0], matching FUN_00054d00(unsigned int ai_ref) -- the unsigned
 *   param is why the record pointer is typed unsigned int * here, as in the
 *   0xc00f0 twin rather than the int * of the 0xc0030 twin.
 *
 *   CALL 0xcbf80 (0xc0157) is preceded by PUSH 0x0 then PUSH ESI = cdecl
 *   reverse -> hs_return(thread_datum, 0). The script return value is the
 *   literal CONSTANT 0, not a record field and not the worker's result (the
 *   worker is void); the entire observable effect is the worker's side effect.
 *
 *   ONE combined ADD ESP,0xc at 0xc015c cleans the 1 push of the 0x54d00 call
 *   plus the 2 pushes of the hs_return call -- so call_site_audit's ARG_COUNT
 *   finding on hs_return ("cleanup=3 stack args, decl=2") is a FALSE POSITIVE;
 *   hs_return really takes 2 args, and its kb decl was left unchanged (same
 *   dismissal as on the committed twins 0xc00f0 / 0xc0070 / 0xc0030).
 *
 * Reloc audit of delinked/functions/000c0130.obj: exactly 3 DISP32 targets in
 * this function's range (offsets 0x11 / 0x20 / 0x28) -- FUN_000cc560,
 * FUN_00054d00, FUN_000cbf80 -- matching the three calls below with no extra
 * global or string reference. (The refs at 0x51/0x60/0x68 belong to the next
 * function in the exported range, 0xc0170.)
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x54d00  = FUN_00054d00(unsigned int ai_ref)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000c0130(int16_t function_index, int thread_datum, char init)
{
  unsigned int *record;

  record = (unsigned int *)hs_macro_function_evaluate(function_index,
                                                      thread_datum, init);
  if (record != NULL) {
    FUN_00054d00(record[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000c0230 @ 0x000c0230
 *
 * HaloScript builtin dispatcher, direct structural twin of FUN_000c0130 /
 * FUN_000c00f0 / FUN_000c0070 / FUN_000c0030 / FUN_000bfff0 / FUN_000bff70
 * above: identical 3-parameter cdecl shape, identical evaluate / NULL-check /
 * worker / hs_return skeleton, and the same one-argument worker arity. The
 * ONLY difference from the 0xc0130 twin is the worker dispatched to --
 * 0x54e80 here vs 0x54d00 there.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xc0230-0xc0261 (50 bytes). No locals, no SUB ESP, no _chkstk,
 * no FPU, no SEH, no stack buffers, no struct stores, no globals. ESI is the
 * only callee-saved register and holds thread_datum live across the evaluate
 * call, which is why it is saved. The exit RET carries no immediate =>
 * cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX  (pushed as a full dword; the
 *                                          int16 narrowing lives inside the
 *                                          callee, matching every twin)
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000c0230(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0.
 *
 * Binary evidence (traced backward from each CALL):
 *   MOV EAX,[EBP+0x10] / MOV ECX,[EBP+0x8] / MOV ESI,[EBP+0xc] (0xc023a)
 *   then PUSH EAX / PUSH ESI / PUSH ECX; CALL 0xcc560 (0xc0240);
 *   ADD ESP,0xc. cdecl reverse push order -> C order (function_index,
 *   thread_datum, init) = a straight pass-through of all three params. ESI is
 *   written exactly once at 0xc023a and is never reloaded, so the Ghidra
 *   register-aliasing trap (decompiler-traps 1) cannot apply to the later
 *   PUSH ESI.
 *
 *   TEST EAX,EAX / JZ 0xc025f skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref (ONE field, ONE FULL DWORD): MOV EDX,dword ptr [EAX] at
 *   0xc024c -- a 32-bit read at record+0, no MOVSX/MOVZX, so record[0] must
 *   NOT be narrowed (lift-learnings 24 LOADW, in reverse). Only +0x0 is
 *   touched, one deref, no buffer-alias risk.
 *
 *   CALL 0x54e80 (0xc024f) is preceded by a single PUSH EDX = ONE argument,
 *   record[0], matching FUN_00054e80(unsigned int ai_ref) -- the unsigned
 *   param is why the record pointer is typed unsigned int * here, as in the
 *   0xc0130 twin rather than the int * of the 0xc0030 twin.
 *
 *   CALL 0xcbf80 (0xc0257) is preceded by PUSH 0x0 then PUSH ESI = cdecl
 *   reverse -> hs_return(thread_datum, 0). The script return value is the
 *   literal CONSTANT 0, not a record field and not the worker's result (the
 *   worker is void); the entire observable effect is the worker's side effect.
 *
 *   ONE combined ADD ESP,0xc at 0xc025c cleans the 1 push of the 0x54e80 call
 *   plus the 2 pushes of the hs_return call -- so call_site_audit's ARG_COUNT
 *   finding on hs_return ("cleanup=3 stack args, decl=2") is a FALSE POSITIVE;
 *   hs_return really takes 2 args, and its kb decl was left unchanged (same
 *   dismissal as on the committed twins 0xc0130 / 0xc00f0 / 0xc0070 /
 *   0xc0030).
 *
 * Reloc audit of delinked/functions/0000c0230.obj: exactly 3 DISP32 targets
 * (offsets 0x11 / 0x20 / 0x28) -- FUN_000cc560, FUN_00054e80, FUN_000cbf80 --
 * matching the three calls below with no extra global or string reference.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x54e80  = FUN_00054e80(unsigned int ai_ref)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. */
void FUN_000c0230(int16_t function_index, int thread_datum, char init)
{
  unsigned int *record;

  record = (unsigned int *)hs_macro_function_evaluate(function_index,
                                                      thread_datum, init);
  if (record != NULL) {
    FUN_00054e80(record[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000c02b0 @ 0x000c02b0
 *
 * HaloScript builtin dispatcher for ai_set_deaf, direct structural twin of
 * FUN_000c0230 / FUN_000c0130 / FUN_000c00f0 / FUN_000c0070 / FUN_000c0030 /
 * FUN_000bfff0 / FUN_000bff70 above: identical 3-parameter cdecl shape and
 * the same evaluate / NULL-check / worker / hs_return skeleton. It differs
 * from those twins in exactly ONE respect -- the worker takes TWO arguments
 * here, not one, so the evaluated argument record is read at two offsets of
 * MIXED width instead of a single dword.
 *
 * cdecl frame: PUSH EBP; MOV EBP,ESP; PUSH ESI; ... POP ESI; POP EBP; RET.
 * Body spans 0xc02b0-0xc02e7 (27 instructions, 56 bytes). No locals, no
 * SUB ESP, no _chkstk, no FPU, no SEH, no stack buffers, no struct stores,
 * no globals. ESI is the only callee-saved register and holds thread_datum
 * live across the evaluate call, which is why it is saved. The exit RET
 * carries no immediate => cdecl, caller cleans.
 *   function_index  int16_t  [EBP+0x08]  -> ECX  (pushed as a full dword; the
 *                                          int16 narrowing lives inside the
 *                                          callee, matching every twin)
 *   thread_datum    int      [EBP+0x0c]  -> ESI, reused as hs_return arg1
 *   init            char     [EBP+0x10]  -> EAX  (loaded as a full dword but
 *                                          declared char to match the callee
 *                                          decl and every twin in this family)
 * Ghidra modelled this void(void), so the three cdecl params surfaced as
 * in_stack_* pseudo-locals; they are STACK args, not @<reg> -- no unaff_/
 * in_EAX/in_ECX appears (lift-learnings 31 void-decl trap). kb.json's stale
 * `void FUN_000c02b0(void);` decl was corrected to the 3-arg cdecl form as
 * part of this lift; a (void) decl over a stack-arg callee is the ESP-drift
 * class of bug from 0x158df0. The RET carries no immediate, so the corrected
 * decl stays cdecl.
 *
 * Binary evidence (traced backward from each CALL, verified against the
 * disassembly of delinked/functions/0000c02b0.obj, not the decompiler):
 *   MOV EAX,[EBP+0x10] / MOV ECX,[EBP+0x8] / MOV ESI,[EBP+0xc] then
 *   PUSH EAX / PUSH ESI / PUSH ECX; CALL 0xcc560 (+0x10); ADD ESP,0xc.
 *   cdecl reverse push order -> C order (function_index, thread_datum, init)
 *   = a straight pass-through of all three params. ESI is written exactly
 *   once from [EBP+0xc] and is never reloaded, so the Ghidra
 *   register-aliasing trap (decompiler-traps 1) cannot apply to the later
 *   PUSH ESI.
 *
 *   TEST EAX,EAX / JZ 0xc02e5 skips BOTH remaining calls when the result is
 *   NULL, so the 0xcc560 return is a POINTER that is dereferenced even though
 *   kb.json declares it as returning int (same as every twin; the cast is
 *   local and the kb decl is left alone).
 *
 *   Record deref -- TWO fields of MIXED width, and this is the only place a
 *   width mistake could hide (lift-learnings 24 LOADW):
 *     XOR EDX,EDX ; MOV DL,byte ptr [EAX+0x4]  -> ZERO-EXTENDED BYTE at +0x4
 *     MOV EAX,dword ptr [EAX]                  -> FULL DWORD at +0x0
 *   The zero-extension (XOR/MOV DL, not MOVSX) is why +0x4 is read through an
 *   unsigned char lvalue below; Ghidra's `(char)puVar1[1]` is a dword-indexed
 *   spelling of the same offset and would invite a full-width load. Only
 *   +0x0 and +0x4 are touched, one deref each, so there is no buffer-alias
 *   risk (decompiler-traps 5) in this 8-byte window. The two loads are
 *   emitted in reverse of push order, which is just cdecl right-to-left
 *   evaluation, not struct-field rotation (decompiler-traps 3) -- both
 *   offsets are unambiguous in the raw disassembly.
 *
 *   CALL 0x55010 (+0x25) is preceded by PUSH EDX then PUSH EAX = cdecl
 *   reverse -> FUN_00055010(record+0x0, record+0x4), matching
 *   FUN_00055010(unsigned int combined_index, char flag) = ai_set_deaf. This
 *   two-argument worker is the ONLY divergence from the 0xc0230 twin.
 *
 *   CALL 0xcbf80 (+0x2d) is preceded by PUSH 0x0 then PUSH ESI = cdecl
 *   reverse -> hs_return(thread_datum, 0). The script return value is the
 *   literal CONSTANT 0, not a record field and not the worker's result
 *   (FUN_00055010 is void); the entire observable effect is the ai_set_deaf
 *   side effect.
 *
 *   ONE combined ADD ESP,0x10 at +0x32 cleans the 2 pushes of the 0x55010
 *   call plus the 2 pushes of the hs_return call -- so the ARG_COUNT finding
 *   on hs_return ("cleanup=4 stack args, decl=2") is the same FALSE POSITIVE
 *   documented on all four committed twins; hs_return really takes 2 args and
 *   its kb decl was left unchanged.
 *
 * Callees (all cdecl, all in kb.json, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x55010  = FUN_00055010(unsigned int combined_index, char flag)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. NOTE: do NOT run
 * maintain.py on this file with an ABSOLUTE path -- it then treats the file
 * as a foreign TU, "moves" all 111 functions out to the same relative path,
 * and leaves players.c empty (observed 2026-07-26). */
void FUN_000c02b0(int16_t function_index, int thread_datum, char init)
{
  unsigned char *record;

  record = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (record != NULL) {
    FUN_00055010(*(unsigned int *)record, (char)record[4]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000c0570 @ 0x000c0570
 *
 * HaloScript builtin dispatcher, direct structural twin of FUN_000c0030 /
 * FUN_000c0070 / FUN_000bfff0 / FUN_000bff70 / FUN_000bfe70 above: identical
 * 3-parameter cdecl shape, identical evaluate / NULL-check / worker /
 * hs_return skeleton, and the same one-argument worker arity. The only
 * difference from FUN_000c0030 is the worker address (0x55870 here vs
 * 0x54ac0 there).
 *
 * Frame (0xc0570-0xc05a1, 50 bytes): PUSH EBP; MOV EBP,ESP; PUSH ESI; ...
 * POP ESI; POP EBP; RET with NO immediate => cdecl, caller cleans the stack.
 * ESI is the only callee-saved register pushed; it holds thread_datum live
 * across the evaluate call, which is exactly why it is saved.
 *
 * Ghidra models this as `void __cdecl FUN_000c0570(void)` with the three
 * arguments surfacing as `in_stack_00000004/8/c` pseudo-locals. Per
 * lift-learnings 31 that is the void-decl trap, NOT a register-argument
 * signal: the disassembly loads all three from the frame --
 *   MOV ECX,[EBP+0x08] -> function_index (int16_t)
 *   MOV ESI,[EBP+0x0c] -> thread_datum   (int)
 *   MOV EAX,[EBP+0x10] -> init           (char)
 * so these are ordinary STACK args. The stale kb.json decl
 * `void FUN_000c0570(void);` was corrected to the 3-arg cdecl form; leaving a
 * (void) decl over a callee that consumes three stack dwords is precisely the
 * 0x158df0 ESP-drift boot-crash class.
 *
 * Call-site verification (all pushes traced backward in the disassembly):
 *   CALL 0xc0580 -> 0xcc560: PUSH EAX; PUSH ESI; PUSH ECX; ADD ESP,0xc.
 *     cdecl reverse push order => C args (function_index, thread_datum, init),
 *     a straight pass-through of all three parameters. ESI is assigned exactly
 *     once from [EBP+0xc] and never reloaded, so the Ghidra register-aliasing
 *     trap cannot apply to the later PUSH ESI.
 *   CALL 0xc058f -> 0x55870: PUSH EDX only => 1 argument, record[0].
 *   CALL 0xc0597 -> 0xcbf80: PUSH 0x0; PUSH ESI => hs_return(thread_datum, 0).
 *
 * NULL guard: TEST EAX,EAX; JZ 0xc059f skips BOTH tail calls, so the 0xcc560
 * return value is dereferenced as a POINTER even though kb.json types it as
 * `int`. The cast is kept local here; the kb declaration is deliberately left
 * untouched (it is shared with every other dispatcher in this family).
 *
 * Record deref: MOV EDX,dword ptr [EAX] -- ONE field at record+0, a FULL
 * 32-bit load with no MOVSX/MOVZX. Per lift-learnings 24 in reverse, this must
 * NOT be narrowed to int16 (unlike the 0xbf920/0xbf960 twins, which do use
 * MOVSX and therefore read a signed short).
 *
 * Apparent arg-count hazard on hs_return is a FALSE POSITIVE: the single
 * `ADD ESP,0xc` at 0xc059c is a MERGED cleanup for BOTH tail calls -- 1 dword
 * for FUN_00055870 (PUSH EDX) plus 2 dwords for hs_return (PUSH 0; PUSH ESI)
 * = 3 dwords. MSVC combined the two cdecl cleanups; hs_return's 2-arg decl and
 * FUN_00055870's 1-arg decl are both correct.
 *
 * Callees (all cdecl, all in kb.json, ported, no @<reg> args anywhere):
 *   0xcc560  = hs_macro_function_evaluate(int16_t, int, char) -> record ptr
 *   0x55870  = FUN_00055870(unsigned int combined_index)
 *   0xcbf80  = hs_return(int thread_handle, int value)
 *
 * No FPU, no _chkstk/SUB ESP, no locals beyond the record pointer, no stack
 * buffers, no struct stores, no SEH, no CONCAT, no intrinsics.
 *
 * Placement: kept here beside its twins deliberately -- the hs helpers are
 * static in this TU; revert any maintain.py relocation. NOTE: do NOT run
 * maintain.py on this file with an ABSOLUTE path -- it then treats the file
 * as a foreign TU, "moves" all functions out to the same relative path, and
 * leaves players.c empty (observed 2026-07-26). */
void FUN_000c0570(int16_t function_index, int thread_datum, char init)
{
  int *record;

  record =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (record != NULL) {
    FUN_00055870(record[0]);
    hs_return(thread_datum, 0);
  }
}
