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
 *     parent object" cluster -- see objects.c FUN_0013DCC0) when valid. */
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
        char *udata =
          (char *)((void *(*)(int, int))0x13d640)(*(int *)(player + 0x34), 1);
        if (udata != NULL) {
          bVar1 = (udata[0x424] & 1) != 0;
          goto check_live;
        }
      } else {
        /* Has parent (seated): check vehicle data at +0x428. */
        char *vdata = (char *)((void *(*)(int, int))0x13d640)(root, 2);
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
          bVar2 = ((bool (*)(int, int, void *))0xbbb80)(
            iter.datum_handle, iVar7, (char *)live_obj + 0x50);
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
typedef struct {
  uint32_t buttons;
  float desired_facing_yaw;
  float desired_facing_pitch;
  float throttle_x;
  float throttle_y;
  float primary_trigger;
  int16_t desired_weapon_index;
  int16_t desired_grenade_index;
  int16_t desired_zoom_level;
  char pad[2];
} player_action_t;

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
        player_set_action_result_for_vehicle(datum_handle,
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
 *        d. Advance the weapon/vehicle seat timers via FUN_bc4b0 (EBX =
 *           datum_handle register arg).
 *        e. If the unit exists and its object-type flags don't include
 *           0x200000, scan scenario trigger volumes (tag block at
 *           scenario+0x39c) for BSP-switch triggers that contain the
 *           player's unit, and fire the BSP switch if found.
 *        f. Reset the player's pending-action fields and call the
 *           per-player post-update helper FUN_bdb00 (EBX = datum_handle).
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

    /* Advance weapon/vehicle seat timers for this player.
     * Original CALL to FUN_bc4b0 with EBX = datum_handle (register arg). */
    if (*(int *)(player + 0x34) != -1) {
      int _dh = datum_handle;
      __asm__ volatile("movl %0, %%ebx" : : "r"(_dh) : "ebx");
      ((void (*)(void))0xbc4b0)();
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
    /* FUN_bdb00: EBX = datum_handle (register arg). */
    {
      int _dh = datum_handle;
      __asm__ volatile("movl %0, %%ebx" : : "r"(_dh) : "ebx");
      ((void (*)(void))0xbdb00)();
    }

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
