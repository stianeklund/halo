/* Process a weapon event for a local player's first-person weapon (0xde140).
 * Handles reload initiation, weapon put-away, aim-assist clearing, and state
 * transitions. Computes reload count from trigger data and weapon ammo state,
 * then selects the appropriate animation state. */
void FUN_000de140(int param_1, int param_2)
{
  char *fp;
  int saved_event;
  int new_state;

  if ((int16_t)param_1 == -1)
    return;

  assert_halt((int16_t)param_1 >= 0 &&
              (int16_t)param_1 < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)(int16_t)param_1 * 0x1ea0);
  saved_event = (int)(int16_t)param_2;

  switch ((int16_t)param_2) {
  case 0:
    *(float *)(fp + 0x2c) += 0.05f;
    break;
  case 9:
  case 10:
    player_clear_aim_assist(*(int *)(fp + 4));
    break;
  case 0xc:
    FUN_000dde80(param_1);
    break;
  case 0xd:
    *(int *)(fp + 0x8) = -1;
    break;
  }

  if (*(int *)(fp + 0x8) == -1)
    goto default_handler;

  {
    int *weapon;
    char *tag;

    weapon = (int *)object_get_and_verify_type(*(int *)(fp + 0x8), 4);
    if (*weapon == -1)
      goto default_handler;

    tag = (char *)tag_get(0x77656170, *weapon);
    if (*(int16_t *)(tag + 0x4e2) != 1)
      goto default_handler;

    if ((int16_t)param_2 != 9 && (int16_t)param_2 != 10)
      goto default_handler;

    {
      char *trigger;
      int16_t current_state;
      int16_t rounds_loaded;
      int16_t ammo_remaining;
      int reload_count;
      int16_t reload_type;

      trigger = (char *)tag_block_get_element(tag + 0x4f0, 0, 0x70);
      current_state = *(int16_t *)(fp + 0xc);
      rounds_loaded = *(int16_t *)((char *)weapon + 0x260);
      ammo_remaining = *(int16_t *)((char *)weapon + 0x25e);

      reload_count = (int)*(int16_t *)(trigger + 0xa) - (int)rounds_loaded;
      if (reload_count > (int)ammo_remaining)
        reload_count = (int)ammo_remaining;

      if (current_state == 0xf || current_state == 0x16 ||
          current_state == 0x10 || current_state == 0x11 ||
          current_state == 0xd || current_state == 0xe ||
          *(int16_t *)((char *)weapon + 0x258) != 0) {
        if (reload_count == 1)
          *(int16_t *)(fp + 0x1e94) = 1;
        else
          *(int16_t *)(fp + 0x1e94) = -1;
      } else {
        *(int16_t *)(fp + 0x1e92) = (int16_t)reload_count;
        *(uint8_t *)(fp + 0x1e90) = (rounds_loaded == 0);
        *(uint16_t *)(fp + 0x1e94) = (uint16_t)(((reload_count != 1) - 1) & 2);
      }

      reload_type = *(int16_t *)(fp + 0x1e94);
      if (reload_type == -1) {
        new_state = 0xd;
      } else if (reload_type == 0 || reload_type == 2) {
        new_state = 0xf;
      } else {
        goto default_handler;
      }
      goto apply_state;
    }
  }

default_handler:
  new_state = (int)FUN_000dc800(param_2);
  if ((int16_t)new_state == -1)
    goto cleanup;

apply_state:
  FUN_000ddbd0(param_1, new_state, 1);

cleanup:
  if (saved_event == 0xc)
    *(int16_t *)(fp + 0x8a) = 0;
}

/* Notify the first-person weapon system of an object event (0xde3b0).
 * Finds the local player holding the weapon, processes the event, and if
 * no local player owns it, attempts third-person sound playback. */
void FUN_000de3b0(int object_handle, int param_2)
{
  int16_t local_player = FUN_000dcd60(object_handle);
  FUN_000de140(local_player, param_2);
  if (local_player == -1) {
    FUN_000dc9d0(param_2, object_handle);
  }
}

/* Update first-person weapon state for all local players. Detects when
 * a player's controlled unit changes and reinitializes their weapon
 * rendering state. Calls per-player weapon update each frame. */
void first_person_weapons_update(void)
{
  int i;
  int offset;

  offset = 0;
  for (i = 0; (int16_t)i < 4; i++, offset += 0x1ea0) {
    int player_handle;
    char *fp;
    void *player;
    int unit;

    player_handle = local_player_get_player_index(i);
    if (player_handle == NONE)
      continue;

    assert_halt((int16_t)i >= 0 &&
                (int16_t)i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    fp = (char *)*(int *)0x46bea8 + offset;
    player = datum_get(player_data, player_handle);
    unit = *(int *)((char *)player + 0x34);

    if (*(int *)(fp + 4) != unit) {
      assert_halt((int16_t)i >= 0 &&
                  (int16_t)i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
      *(uint8_t *)(fp + 0x50) = 0;
      *(int *)(fp + 4) = unit;
      FUN_000dde80(i);
    }
    if (*(int *)(fp + 8) == NONE)
      FUN_000dde80(i);
    ((void (*)(int))0xde560)(i);
  }
}
