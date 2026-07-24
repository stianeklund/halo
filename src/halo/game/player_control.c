/* Return a pointer to the player control data slot for a local player.
 * Each slot is 0x40 bytes, starting at offset 0x10 in the globals struct. */
void *player_control_get_data(int16_t local_player_index)
{
  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return (char *)player_control_globals + local_player_index * 0x40 + 0x10;
}

void player_control_dispose(void)
{
}

/* Set action flags on a local player's control slot.
 * ORs the given flags into the player's action_flags field, and
 * optionally into the persistent_action_flags field as well. */
void player_control_set_action_flags(int16_t local_player_index, uint16_t flags,
                                     bool persistent)
{
  uint16_t *slot;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  slot = (uint16_t *)((char *)player_control_globals +
                      local_player_index * 0x40 + 0x10);
  *(uint16_t *)((char *)slot + 8) |= flags;
  if (persistent)
    *(uint16_t *)((char *)slot + 0xa) |= flags;
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
  char *slot;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
  player_handle = *(int *)(unit_obj + 0x1c8);
  if (player_handle != NONE) {
    player = (char *)datum_get(player_data, player_handle);
    local_player_index = *(int16_t *)(player + 0x2);
    if (local_player_index != NONE) {
      slot = (char *)player_control_get_data((int16_t)local_player_index);
      *(int16_t *)(slot + 0x24) = NONE;
    }
  }
}

/* Set a player control slot's desired facing angles from a 3D direction vector.
 * Converts the direction vector to yaw+pitch via vector_to_angles (atan2-based
 * vector_to_angles), validates both angles for NaN/Inf, and normalizes yaw
 * to [0, 2*pi) by adding 2*pi if negative. */
void player_control_set_facing(uint16_t local_player_index, float *direction)
{
  char *player_slot;
  float *desired_yaw;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  player_slot = (char *)player_control_globals +
                (int)(int16_t)local_player_index * 0x40 + 0x10;
  desired_yaw = (float *)(player_slot + 0xc);

  /* Convert direction vector to yaw/pitch angles */
  vector_to_angles(desired_yaw, direction);

  /* assert_valid_real on desired_angles.pitch (slot+0x10) */
  if ((*(uint32_t *)(player_slot + 0x10) & 0x7f800000u) == 0x7f800000u) {
    char *msg = csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                         "player_control->desired_angles.pitch",
                         *(uint32_t *)(player_slot + 0x10),
                         (double)*(float *)(player_slot + 0x10));
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
  int *slot;
  float *facing;
  int unit;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  slot =
    (int *)((char *)player_control_globals + local_player_index * 0x40 + 0x10);
  csmemset(slot, 0, 0x40);
  *slot = player_index;
  *(int16_t *)(slot + 8) = -1;
  *(int16_t *)((char *)slot + 0x22) = -1;
  *(int16_t *)(slot + 9) = -1;
  *(char *)((char *)slot + 0x26) = 0;
  slot[10] = -1;
  *(float *)(slot + 0xf) = 1.49f;
  *(float *)(slot + 0xe) = -1.49f;
  *(int16_t *)(slot + 2) = 0;
  *(int16_t *)((char *)slot + 10) = 0;
  if (player_index != -1) {
    unit = (int)object_get_and_verify_type(player_index, 3);
    facing = (float *)(slot + 3);
    vector_to_angles(facing, (float *)(unit + 0x1d4));
    if (*facing < *(float *)0x2533c0)
      *facing += *(float *)0x255a54;
    *(int16_t *)(slot + 8) = *(int16_t *)(unit + 0x2a4);
    *(int16_t *)((char *)slot + 0x22) = (int16_t) * (char *)(unit + 0x2cd);
    *(int16_t *)(slot + 9) = (int16_t) * (char *)(unit + 0x2d1);
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
  char *slot;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
  player_handle = *(int *)(unit_obj + 0x1c8);
  if (player_handle != NONE) {
    player = (char *)datum_get(player_data, player_handle);
    local_player_index = *(int16_t *)(player + 0x2);
    if (local_player_index != NONE) {
      slot = (char *)player_control_get_data(local_player_index);
      *(int16_t *)(slot + 0x20) = (int16_t)seat_index;
    }
  }
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
  char action[0x20]; /* input action struct, filled by get_input */
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
  csmemset(action, 0xfa, 0x20);
  /* get_local_player_input_blob fills *action (0x20 bytes, passed in EBX). */
  get_local_player_input_blob(action, local_player_index, delta_time);

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
    csmemset(action, 0, 0x20);

  /* handle weapon/vehicle switching when playing locally */
  if (game_connection() == 0) {
    uint8_t flags = *(uint8_t *)(action + 0x1c);

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
    weapon_datum = unit_get_weapon(
      pc->unit_index, *(uint16_t *)(unit_obj + 0x2a2));

    /* validate player weapon index */
    if (pc->desired_weapon_index == NONE ||
        unit_get_weapon(
          pc->unit_index, pc->desired_weapon_index) == NONE) {
      pc->desired_weapon_index = *(int16_t *)(unit_obj + 0x2a4);
    }

    /* weapon interaction (action bit 0) */
    if ((*(uint8_t *)(action + 0x1c) & 1) ||
        unit_get_weapon(
          pc->unit_index, pc->desired_weapon_index) == NONE ||
        pc->desired_weapon_index == NONE) {
      int16_t new_wp = unit_inventory_next_weapon(
        pc->unit_index, pc->desired_weapon_index,
        *(uint8_t *)(action + 0x1c) & 1);
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
        unit_get_grenade_count(
          pc->unit_index, pc->desired_grenade_index) == 0) {
      pc->desired_grenade_index = (int16_t) * (int8_t *)(unit_obj + 0x2cd);
    }

    /* grenade switch (action bit 1) */
    if ((*(uint8_t *)(action + 0x1c) & 2) ||
        unit_get_grenade_count(
          pc->unit_index, pc->desired_grenade_index) == 0 ||
        pc->desired_grenade_index == NONE) {
      pc->desired_grenade_index = unit_inventory_next_grenade(
        pc->unit_index, pc->desired_grenade_index, 1);
    }

    /* melee/throw request (action bit 2) */
    if ((*(uint8_t *)(action + 0x1c) & 4) &&
        (*(uint8_t *)((char *)player_control_globals + 0xc) & 1) == 0 &&
        !game_time_get_paused() && weapon_datum != NONE &&
        !cinematic_in_progress()) {
      pc->desired_zoom_level = weapon_rotate_zoom_level(
        weapon_datum, pc->desired_zoom_level);
    }

    /* apply turning/look input (unless scripted camera). FUN_000b7f90 takes
     * local_player_index in EAX; a0/a1 are the raw turn/look input dwords at
     * action+0x0c / action+0x10. */
    if (!director_inhibited_facing(local_player_index)) {
      FUN_000b7f90(local_player_index, *(int *)(action + 0x0c),
                   *(int *)(action + 0x10));
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
      if (*(float *)(action + 0x10) >= *(float *)0x253f44)
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
  pc->field_0x04 = *(int *)(action + 0x18);
  *(int *)&pc->field_0x14 = *(int *)(action + 0x00);
  pc->field_0x18 = *(int *)(action + 0x04);
  *(int *)&pc->primary_trigger = *(int *)(action + 0x08);

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
      char player_action[0x20];
      *(int *)(player_action + 0x00) = pc->field_0x04;
      *(int *)(player_action + 0x04) = *(int *)&pc->desired_angles_yaw;
      *(int *)(player_action + 0x08) = *(int *)&pc->desired_angles_pitch;
      *(int16_t *)(player_action + 0x18) = pc->desired_weapon_index;
      *(int16_t *)(player_action + 0x1a) = pc->desired_grenade_index;
      *(int *)(player_action + 0x0c) = *(int *)&pc->field_0x14;
      *(int16_t *)(player_action + 0x1c) = pc->desired_zoom_level;
      *(int *)(player_action + 0x14) = *(int *)&pc->primary_trigger;
      *(int *)(player_action + 0x10) = pc->field_0x18;

      /* validate action facing angles */
      bits = *(uint32_t *)(player_action + 0x08);
      if ((bits & 0x7f800000) == 0x7f800000) {
        display_assert("action.desired_facing.pitch",
                       "c:\\halo\\SOURCE\\game\\player_control.c", 0x369, 1);
        system_exit(NONE);
      }
      bits = *(uint32_t *)(player_action + 0x04);
      if ((bits & 0x7f800000) == 0x7f800000) {
        display_assert("action.desired_facing.yaw",
                       "c:\\halo\\SOURCE\\game\\player_control.c", 0x36a, 1);
        system_exit(NONE);
      }

      update_client_queue(player_action);
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
