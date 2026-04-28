/* Map a first-person weapon state to an animation graph index (0xdc8c0).
 * Pure lookup table: 24 states (0..23) map to animation indices; any
 * out-of-range state returns -1. */
int16_t FUN_000dc8c0(int16_t state)
{
  switch (state) {
  case 0:
    return 0;
  case 1:
    return 0x15;
  case 2:
    return 0x16;
  case 3:
    return 9;
  case 4:
    return 0xc;
  case 5:
    return 1;
  case 6:
    return 2;
  case 7:
    return 0xe;
  case 8:
    return 0x12;
  case 9:
    return 0x13;
  case 10:
    return 0xd;
  case 11:
    return 5;
  case 12:
    return 6;
  case 13:
    return 7;
  case 14:
    return 8;
  case 15:
    return 0x17;
  case 16:
    return 0x18;
  case 17:
    return 0x19;
  case 18:
    return 0xb;
  case 19:
    return 0xa;
  case 20:
    return 0x10;
  case 21:
    return 0x14;
  case 22:
    return 0x1a;
  case 23:
    return 0x1b;
  default:
    return (int16_t)-1;
  }
}

/* Try to play a third-person weapon sound for an object event (0xdc9d0).
 * When no local player owns the weapon, this function looks up the weapon's
 * animation graph tag, maps the event through two state-translation tables
 * (FUN_000dc800 and FUN_000dc8c0), resolves the animation's sound effect
 * tag reference, and plays it at the global origin with default forward. */
void FUN_000dc9d0(int param_2, int object_handle)
{
  int16_t state;
  int16_t anim_index;
  char *weapon_tag;
  int anim_graph_tag_index;
  char *antr_tag;
  char *block_element;
  int16_t lookup_result;
  char *anim_element;
  char *sound_element;
  int sound_tag_index;

  if (object_handle == -1)
    return;
  if ((int16_t)param_2 == -1)
    return;
  if (!object_try_and_get_and_verify_type(object_handle, 4))
    return;

  {
    int *weapon_obj = (int *)object_get_and_verify_type(object_handle, 4);
    weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
  }

  anim_graph_tag_index = *(int *)(weapon_tag + 0x478);
  if (anim_graph_tag_index == -1)
    return;

  state = FUN_000dc800(param_2);
  if (state == -1)
    return;

  anim_index = FUN_000dc8c0(state);
  if (anim_index == -1)
    return;

  antr_tag = (char *)tag_get(0x616e7472, anim_graph_tag_index);

  if (*(int *)(antr_tag + 0x48) == 0) {
    block_element = NULL;
  } else {
    block_element = (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c);
  }

  if (anim_index < 0 || (int)anim_index >= *(int *)(block_element + 0x10))
    return;

  lookup_result =
    *(int16_t *)(*(int *)(block_element + 0x14) + (int)anim_index * 2);
  if (lookup_result == -1)
    return;

  anim_element =
    (char *)tag_block_get_element(antr_tag + 0x74, (int)lookup_result, 0xb4);
  if (*(int16_t *)(anim_element + 0x3c) == -1)
    return;

  sound_element = (char *)tag_block_get_element(
    antr_tag + 0x54, (int)*(int16_t *)(anim_element + 0x3c), 0x14);
  sound_tag_index = *(int *)(sound_element + 0xc);
  if (sound_tag_index == -1)
    return;

  {
    float *position = *(float **)0x31fc1c;
    float *forward = *(float **)0x31fc3c;
    FUN_001c7e70(object_handle, sound_tag_index, -1, position, forward, 1.0f);
  }
}

/* Toggle the first-person weapon activation state for a local player (0xdcb30).
 * When activating (activate != 0): asserts weapon_index != NONE, then calls
 * FUN_0009e0d0 to start effects. When deactivating: calls FUN_0009c810 to stop
 * effects and FUN_000a1510 to stop sounds. Only acts if the state changes. */
void FUN_000dcb30(int16_t local_player_index, uint8_t activate)
{
  char *fp;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);

  if (activate != *(uint8_t *)fp) {
    if (activate != 0) {
      assert_halt(*(int *)(fp + 8) != -1);
      FUN_0009e0d0((int)local_player_index, *(int *)(fp + 8));
      *(uint8_t *)fp = activate;
      return;
    }
    FUN_0009c810((int)local_player_index);
    FUN_000a1510((int)local_player_index);
    *(uint8_t *)fp = 0;
  }
}

/* Match animation node labels between a model tag and an animation graph tag
 * (0xdcc80). For each node in the model's node block, searches the animation
 * graph's node block for a matching string label via csstrcmp. Stores the
 * matching index in the output array. Returns 1 if all nodes matched, 0 if
 * any node had no match. */
uint8_t FUN_000dcc80(int mode_tag_index, int antr_tag_index, int16_t *output)
{
  char *mode_tag;
  char *antr_tag;
  uint8_t success;
  int16_t outer;
  int outer_int;
  void *antr_nodes; /* pointer to antr tag + 0x68 (node block) */

  mode_tag = (char *)tag_get(0x6d6f6465, mode_tag_index);
  antr_tag = (char *)tag_get(0x616e7472, antr_tag_index);

  success = 1;
  outer = 0;
  outer_int = 0;

  if (*(int *)(mode_tag + 0xb8) < 1)
    return 1;

  antr_nodes = (void *)(antr_tag + 0x68);

  do {
    char *mode_element;
    int16_t inner;
    int inner_int;

    mode_element =
      (char *)tag_block_get_element(mode_tag + 0xb8, outer_int, 0x9c);
    inner = 0;

    if (*(int *)antr_nodes > 0) {
      inner_int = 0;
      do {
        char *antr_element;
        antr_element =
          (char *)tag_block_get_element(antr_nodes, inner_int, 0x40);
        if (csstrcmp(mode_element, antr_element) == 0) {
          if (inner != -1) {
            output[outer_int] = inner;
            goto next_outer;
          }
          break;
        }
        inner = inner + 1;
        inner_int = (int)inner;
      } while (inner_int < *(int *)antr_nodes);
    }

    success = 0;

  next_outer:
    outer = outer + 1;
    outer_int = (int)outer;
    if (outer_int >= *(int *)(mode_tag + 0xb8))
      return success;
  } while (1);
}

/* Find the local player index (0..3) whose unit currently holds the given
 * weapon object. Iterates all local players, resolves each player's
 * controlled unit, and checks if the unit's active weapon slot matches the
 * given object handle. Returns the local player index or -1 if not found. */
int16_t FUN_000dcd60(int object_handle)
{
  int16_t i;

  for (i = 0; i < 4; i++) {
    int player_handle;
    char *player;
    int unit_handle;
    char *unit;
    int16_t weapon_index;

    player_handle = local_player_get_player_index(i);
    if (player_handle == -1)
      continue;

    player = (char *)datum_get(player_data, player_handle);
    unit_handle = *(int *)(player + 0x34);
    if (unit_handle == -1)
      continue;

    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    weapon_index = *(int16_t *)(unit + 0x2a2);
    if (weapon_index == -1)
      continue;

    if (object_handle == *(int *)(unit + 0x2a8 + (int)weapon_index * 4))
      return i;
  }

  return (int16_t)-1;
}

/* Precache the weapon's predicted resources and set the reload timer (0xdce00).
 * If the player has a valid weapon, resolves the weapon tag and calls
 * predicted_resources_precache on the resource block at weapon_tag + 0x4e4.
 * Always sets the timer at fp + 0x12 to 0x1e (30 ticks). */
void FUN_000dce00(int16_t local_player_index)
{
  char *fp;
  int weapon_handle;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  weapon_handle =
    *(int *)((int)local_player_index * 0x1ea0 + 8 + *(int *)0x46bea8);
  fp = (char *)((int)local_player_index * 0x1ea0 + *(int *)0x46bea8);

  if (weapon_handle != -1) {
    int *weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    char *weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
    predicted_resources_precache((int *)(weapon_tag + 0x4e4));
  }

  *(int16_t *)(fp + 0x12) = 0x1e;
}

/* Return a pointer to the node transform for a given node in the local
 * player's first-person weapon animation state (0xdd410).
 * Validates the local_player_index (0..3) and node_index against the
 * animation graph node count. Returns fp_base + 0x108c + node_index * 0x34. */
void *FUN_000dd410(int param_1, int param_2)
{
  int16_t local_player_index = (int16_t)param_1;
  int16_t node_index = (int16_t)param_2;
  char *fp;
  int *weapon_obj;
  char *weapon_tag;
  char *antr_tag;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  weapon_obj = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
  weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(weapon_tag + 0x478));

  assert_halt(node_index >= 0 && (int)node_index < *(int *)(antr_tag + 0x68));

  return (void *)((int)node_index * 0x34 + 0x108c + (int)fp);
}

/* Copy animation node data from the current buffer to the blend buffer and
 * update blend timing (0xdd4d0). Copies node_count * 32 bytes from fp + 0x8c
 * to fp + 0x88c. If blend_ticks >= (fp[0x8a] - fp[0x88]), resets the blend
 * origin to 0 and sets the blend target to blend_ticks. */
void FUN_000dd4d0(int16_t local_player_index, int16_t blend_ticks)
{
  char *fp;
  int *weapon_obj;
  char *weapon_tag;
  char *antr_tag;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  weapon_obj = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
  weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(weapon_tag + 0x478));

  csmemcpy(fp + 0x88c, fp + 0x8c, *(int *)(antr_tag + 0x68) << 5);

  if ((int)blend_ticks >=
      (int)*(int16_t *)(fp + 0x8a) - (int)*(int16_t *)(fp + 0x88)) {
    *(int16_t *)(fp + 0x88) = 0;
    *(int16_t *)(fp + 0x8a) = blend_ticks;
  }
}

/* Set the first-person weapon animation state for a local player (0xddbd0).
 * Applies state-transition filtering: certain incoming states are rejected
 * depending on the current state. For dual-wielding weapons (type 3), maps
 * state 3 to state 0 unless the weapon has a specific flag. Looks up the
 * animation index via FUN_000dc8c0 and the animation graph to validate the
 * transition. If param_3 is nonzero, stops any pending sound. */
void FUN_000ddbd0(int param_1, int param_2, int param_3)
{
  int16_t local_player_index = (int16_t)param_1;
  int16_t state = (int16_t)param_2;
  char *fp;
  int weapon_handle;
  int16_t sVar1;
  int16_t blend_ticks;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  weapon_handle = *(int *)(fp + 8);

  /* If the unit's weapon has the dual-wield flag (byte 0x1dc bit 0),
   * remap certain states. */
  if (weapon_handle != -1) {
    char *weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
    if ((*(uint8_t *)(weapon_obj + 0x1dc) & 1) != 0) {
      if (state == 0x13) {
        state = 2;
      } else if (state == 0x14) {
        state = 0x15;
      }
    }
  }

  /* First switch: filter incoming states based on current state.
   * Binary switch table at 0xdde40/0xdde50 maps:
   *   6,7,8,9 → handler 0 (melee filter)
   *   0xb,0xc → handler 1 (grenade filter)
   *   0x13    → handler 2
   *   0xa,0xd-0x12 → handler 3 (default, no filter) */
  switch (state) {
  case 6:
  case 7:
  case 8:
  case 9: {
    int16_t cur = *(int16_t *)(fp + 0xc);
    if (cur != 0 && cur != 5 && cur != 6 && cur != 4 && cur != 0xf &&
        cur != 0x16 && cur != 0x10 && cur != 0x11 && cur != 0xd && cur != 0xe) {
      return;
    }
    break;
  }
  case 0xb:
  case 0xc:
    if (*(int16_t *)(fp + 0xc) != 0 && *(int16_t *)(fp + 0xc) != 5) {
      return;
    }
    break;
  case 0x13:
    if (*(int16_t *)(fp + 0xc) == 0x13)
      return;
    break;
  default:
    break;
  }

  if (state == -1)
    return;
  if (*(int *)(fp + 8) == -1)
    return;

  {
    int *weapon_obj2 = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
    char *weapon_tag = (char *)tag_get(0x77656170, *weapon_obj2);

    /* For weapon type 3, if state is also 3 and the dual-wield flag is
     * not set, reset state to 0. */
    if (*(int16_t *)(weapon_tag + 0x4e2) == 3 && state == 3 &&
        (*(uint8_t *)((char *)weapon_obj2 + 0x1dc) & 1) == 0) {
      state = 0;
    }

    sVar1 = FUN_000dc8c0(state);

    /* If weapon type is 1 and current state is 0x10, use blend_ticks = 0. */
    if (*(int16_t *)(weapon_tag + 0x4e2) == 1 &&
        *(int16_t *)(fp + 0xc) == 0x10) {
      blend_ticks = 0;
    } else {
      /* Second switch: determine blend tick count. */
      switch (state) {
      case 3:
      case 10:
      case 0x13:
        blend_ticks = 0;
        break;
      case 6:
      case 7:
      case 8:
      case 9:
        blend_ticks = 3;
        break;
      default:
        blend_ticks = 6;
        break;
      }
    }

    if (*(int *)(fp + 4) == -1)
      return;
    if (*(int *)(fp + 8) == -1)
      return;

    {
      int *weapon_obj3 = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
      char *weapon_tag2 = (char *)tag_get(0x77656170, *weapon_obj3);
      char *antr_tag =
        (char *)tag_get(0x616e7472, *(int *)(weapon_tag2 + 0x478));

      if (*(int *)(antr_tag + 0x48) != 0) {
        char *anim_block =
          (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c);
        if (anim_block != NULL && sVar1 >= 0 &&
            (int)sVar1 < *(int *)(anim_block + 0x10)) {
          int16_t anim_index =
            *(int16_t *)(*(int *)(anim_block + 0x14) + (int)sVar1 * 2);
          if (anim_index != -1) {
            if ((char)param_3 != 0 && *(int *)(fp + 0x1e98) != -1 &&
                *(int16_t *)(fp + 0x1e9c) != 1) {
              FUN_001cd450(*(int *)(fp + 0x1e98));
              *(int *)(fp + 0x1e98) = -1;
              *(int16_t *)(fp + 0x1e9c) = -1;
            }
            if (blend_ticks > 0) {
              FUN_000dd4d0(local_player_index, blend_ticks);
            }
            *(int16_t *)(fp + 0xc) = state;
            *(int16_t *)(fp + 0x16) = anim_index;
            *(int16_t *)(fp + 0x18) = 0;
          }
        }
      }
    }
  }
}

/* Initialize or reinitialize a local player's first-person weapon (0xdde80).
 * Clears the current weapon reference, deactivates visual/sound state if
 * previously active, then resolves the unit's current weapon and sets up
 * the animation graph, idle animation index, and initial weapon state. */
void FUN_000dde80(int param_1)
{
  int16_t local_player_index = (int16_t)param_1;
  char *fp;
  uint8_t was_active;
  int weapon_handle;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  was_active = *(uint8_t *)fp;

  /* Clear weapon handle. */
  *(int *)(fp + 8) = -1;

  /* If previously active, deactivate effects and sounds. */
  if (was_active != 0) {
    assert_halt(local_player_index >= 0 &&
                local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    {
      char *fp2 = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
      if (*(uint8_t *)fp2 != 0) {
        FUN_0009c810(param_1);
        FUN_000a1510(param_1);
        *(uint8_t *)fp2 = 0;
      }
    }
  }

  /* If no unit is assigned, skip weapon setup. */
  if (*(int *)(fp + 4) == -1)
    goto done;

  {
    char *unit_obj = (char *)object_get_and_verify_type(*(int *)(fp + 4), 3);
    int16_t weapon_index = *(int16_t *)(unit_obj + 0x2a2);

    weapon_handle = unit_get_weapon(*(int *)(fp + 4), weapon_index);
    if (weapon_handle == -1)
      goto done;

    {
      int *weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
      char *weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);

      if (*(int *)(weapon_tag + 0x468) == -1)
        goto done;
      if (*(int *)(weapon_tag + 0x478) == -1)
        goto done;

      {
        char *antr_tag =
          (char *)tag_get(0x616e7472, *(int *)(weapon_tag + 0x478));

        if (*(int *)(antr_tag + 0x48) == 0)
          goto done;

        {
          char *anim_block =
            (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c);
          if (anim_block == NULL)
            goto done;

          /* Set idle animation index from the animation lookup table. */
          *(int16_t *)(fp + 0x14) = -1;
          if (*(int *)(anim_block + 0x10) > 4) {
            int16_t anim_lookup = *(int16_t *)(*(int *)(anim_block + 0x14) + 8);
            if (anim_lookup != -1) {
              char *anim_entry = (char *)tag_block_get_element(
                antr_tag + 0x74, (int)anim_lookup, 0xb4);
              if (*(int16_t *)(anim_entry + 0x22) >= 9) {
                *(int16_t *)(fp + 0x14) = anim_lookup;
              }
            }
          }

          /* Check game globals for the global fp animation model. */
          {
            char *game_globals = (char *)game_globals_get();
            char *gg_element =
              (char *)tag_block_get_element(game_globals + 0x17c, 0, 0xc0);

            if (*(int *)(gg_element + 0xc) != -1) {
              *(uint8_t *)(fp + 0x1e0e) = FUN_000dcc80(
                *(int *)(gg_element + 0xc), *(int *)(weapon_tag + 0x478),
                (int16_t *)(fp + 0x1e10));
            }
          }

          *(uint8_t *)(fp + 0x1d8c) = FUN_000dcc80(*(int *)(weapon_tag + 0x468),
                                                   *(int *)(weapon_tag + 0x478),
                                                   (int16_t *)(fp + 0x1d8e));

          if (*(uint8_t *)(fp + 0x1d8c) == 0)
            goto done;
          if (*(uint8_t *)(fp + 0x1e0e) == 0)
            goto done;

          /* Set weapon handle and clear animation state. */
          *(int *)(fp + 0x8) = weapon_handle;
          *(int16_t *)(fp + 0xc) = -1;
          *(int16_t *)(fp + 0x16) = -1;
          *(int16_t *)(fp + 0x1a) = -1;
          *(int16_t *)(fp + 0x20) = -1;
          *(int *)(fp + 0x28) = 0;
          *(int *)(fp + 0x2c) = 0;
          *(int16_t *)(fp + 0x10) = 0;
          *(int *)(fp + 0x1e98) = -1;
          *(int16_t *)(fp + 0x1e9c) = -1;

          FUN_000ddbd0(param_1, 0, 1);

          *(int16_t *)(fp + 0x8a) = 0;

          if (was_active != 0) {
            FUN_000dcb30(local_player_index, 1);
          }
        }
      }
    }
  }

done:
  FUN_000dce00(local_player_index);
}

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
