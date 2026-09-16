/* Refresh every local player's HUD weapon state (0xda980).
 * Source: c:\halo\SOURCE\interface\hud_weapon.c line 0xd4.
 * Stack-guard idiom: 0x200-byte 0x62 fill plus a return-address canary
 * (FUN_000d1540), both asserted after the per-player loop. */
void hud_update_weapon(void)
{
  int guard[128];
  unsigned char weapon_state[32];
  int empty_state[8];
  int return_addr;
  int unit_handle;
  int weapon_handle;
  int update_handle;
  int update_tag_index;
  void *update_state;
  void *player;
  void *unit;
  void *other_unit;
  void *weapon_tag;
  unsigned char *weapon_entry;
  void *hud_weapon_state;
  volatile int *p;
  int n;
  short local_player_index;
  short i;
  short corrupt_index;

  return_addr = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);

  local_player_index = local_player_get_next(-1);
  while (local_player_index != -1) {
    if (local_player_get_player_index(local_player_index) == -1) {
      goto next_player;
    }
    player = datum_get(*(data_t **)0x5aa6d4,
                       local_player_get_player_index(local_player_index));
    unit_handle = *(int *)((char *)player + 0x34);
    if (unit_handle == -1) {
      goto next_player;
    }

    unit = object_get_and_verify_type(unit_handle, 3);
    weapon_handle =
      unit_inventory_get_weapon(unit_handle, *(unsigned short *)((char *)unit + 0x2a2));
    if (weapon_handle != -1) {
      goto have_weapon;
    }

    /* No weapon in hand: try the unit this one is riding (parent at +0xcc)
     * when its seat (+0x2a0) is flagged 0x8 in the unit tag's seat block. */
    unit = object_get_and_verify_type(unit_handle, 3);
    if (*(int *)((char *)unit + 0xcc) != -1 &&
        *(short *)((char *)unit + 0x2a0) != -1) {
      weapon_entry = (unsigned char *)tag_block_get_element(
        (char *)tag_get(0x756e6974, *(int *)object_get_and_verify_type(
                                      *(int *)((char *)unit + 0xcc), 3)) +
          0x2e4,
        (int)*(short *)((char *)unit + 0x2a0), 0x11c);
      if ((*weapon_entry & 8) == 0) {
        goto store_weapon;
      }
      other_unit = object_get_and_verify_type(*(int *)((char *)unit + 0xcc), 3);
      weapon_handle =
        unit_inventory_get_weapon(*(int *)((char *)unit + 0xcc),
                        *(unsigned short *)((char *)other_unit + 0x2a2));
      if (weapon_handle != -1) {
        goto have_weapon;
      }
    }

    if (unit_count_weapons(unit_handle) != 0) {
      goto store_weapon;
    }
    empty_state[0] = 0;
    p = (volatile int *)empty_state + 1;
    for (n = 7; n != 0; n--) {
      *p = 0;
      p++;
    }
    update_handle = -1;
    update_tag_index = *(int *)((char *)*(void **)0x46bd0c + 0x2cc);
    update_state = empty_state;
    goto update_hud;

  have_weapon:
    weapon_tag =
      tag_get(0x77656170, *(int *)object_get_and_verify_type(weapon_handle, 4));
    weapon_build_weapon_interface_state(weapon_handle, (int)weapon_state);
    if (*(int *)((char *)weapon_tag + 0x48c) == -1) {
      goto store_weapon;
    }
    update_handle = weapon_handle;
    update_tag_index = *(int *)((char *)weapon_tag + 0x48c);
    update_state = weapon_state;

  update_hud:
    FUN_000d9960(local_player_index, update_handle, update_tag_index,
                 update_state);

  store_weapon:
    hud_weapon_state = FUN_000d8bc0(local_player_index);
    *(int *)((char *)hud_weapon_state + 0x20) = weapon_handle;

  next_player:
    local_player_index = local_player_get_next(local_player_index);
  }

  corrupt_index = -1;
  for (i = 0x7f; i >= 0; i--) {
    if (guard[(int)i] != 0x62626262) {
      corrupt_index = i;
      break;
    }
  }

  if (FUN_000d1540() != return_addr) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0xd4, 1);
    system_exit(-1);
  }

  if (corrupt_index != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)corrupt_index),
      "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0xd4, 1);
    system_exit(-1);
  }
}

/* Classify a unit relative to a local player, for the motion sensor / event
 * display (0xdaee0).  Takes local_player_index in @<ebx> and the unit object
 * handle in @<esi>; the result byte is returned in AL.
 *
 * Observed result codes (meanings inferred from the branches, names unknown):
 *   5  unit_handle == -1
 *   0  the unit belongs to this same local player
 *   2  the object is not a unit (object_try_and_get_and_verify_type(,3) NULL)
 *   1/2  biped:   game_allegiance_get_team_is_friendly(...) + 1
 *   3/4  vehicle: game_allegiance_get_team_is_friendly(...) + 3
 *   3/4  empty vehicle: 4 when the unit tag's second block element names
 *        "c_dropship", else 3
 * The AL width leaves char vs unsigned char undecidable here; char is used.
 *
 * Shape notes (binary-derived, do not "simplify"):
 *   - the local player's team at player+0x20 is read BEFORE the
 *     unit_handle == -1 early return;
 *   - player_index_from_unit_index is called twice (000daf0b, 000daf1d);
 *   - the occupant branch re-fetches the unit with a fresh
 *     object_get_and_verify_type(occupant_handle, 3) instead of reusing `unit`;
 *   - the biped branch re-derives the local player's team with a second
 *     local_player_get_player_index/datum_get pair rather than reusing the
 *     [EBP-4] copy. */
char FUN_000daee0(int local_player_index, int unit_handle)
{
  int local_team;
  int unit_player_index;
  void *player;
  void *unit;
  void *vehicle;
  int occupant_handle;
  void *occupant;
  void *unit_tag;
  void *seat;

  player =
    datum_get(player_data, local_player_get_player_index(local_player_index));
  local_team = *(int *)((char *)player + 0x20);
  if (unit_handle == -1) {
    return 5;
  }
  if (player_index_from_unit_index(unit_handle) == -1) {
    unit_player_index = -1;
  } else {
    unit_player_index =
      *(int16_t *)((char *)datum_get(
                     player_data, player_index_from_unit_index(unit_handle)) +
                   2);
  }
  if (unit_player_index == local_player_index) {
    return 0;
  }
  if (object_try_and_get_and_verify_type(unit_handle, 3) == 0) {
    return 2;
  }
  unit = object_get_and_verify_type(unit_handle, 3);
  if (object_try_and_get_and_verify_type(unit_handle, 2) != 0) {
    vehicle = object_get_and_verify_type(unit_handle, 2);
    occupant_handle = *(int *)((char *)vehicle + 0x2d8);
    if (occupant_handle != -1) {
      occupant = object_get_and_verify_type(occupant_handle, 3);
      return (char)(game_allegiance_get_team_is_friendly(
                      *(uint16_t *)((char *)occupant + 0x68), local_team) +
                    3);
    }
    {
      occupant_handle = *(int *)((char *)vehicle + 0x2d4);
      if (occupant_handle == -1) {
        unit_tag = tag_get(0x756e6974, *(int *)vehicle);
        if (*(int *)((char *)unit_tag + 0x2e4) > 1) {
          seat = tag_block_get_element((char *)unit_tag + 0x2e4, 0, 0x11c);
          if (csstrncmp((char *)seat + 4, "c_dropship", 10) == 0) {
            return 4;
          }
        }
        return 3;
      }
    }
    occupant = object_get_and_verify_type(occupant_handle, 3);
    return (char)(game_allegiance_get_team_is_friendly(
                    *(uint16_t *)((char *)occupant + 0x68), local_team) +
                  3);
  }
  player =
    datum_get(player_data, local_player_get_player_index(local_player_index));
  local_team = *(int *)((char *)player + 0x20);
  return (char)(game_allegiance_get_team_is_friendly(
                  *(uint16_t *)((char *)unit + 0x68), local_team) +
                1);
}


/* Per-local-player motion sensor state accessor (0xdb0b0).
 * Takes local_player_index in @<si>; the state block allocated by
 * motion_sensor_initialize holds 4 records of 0x568 bytes (0x15a8 total).
 * Source: c:\halo\SOURCE\interface\motion_sensor.c line 0x11f. */
void *FUN_000db0b0(short local_player_index)
{
  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert("local_player_index>=0 && "
                   "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                   "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x11f, 1);
    system_exit(-1);
  }
  return (void *)((char *)*(void **)0x46bd2c + local_player_index * 0x568);
}

/* Allocate the motion sensor (radar) game state block (0xdb0f0). */
void motion_sensor_initialize(void)
{
  *(void **)0x46bd2c =
    game_state_malloc("motion sensor (radar)", "sensor data", 0x15a8);
  if (*(void **)0x46bd2c == 0) {
    display_assert("motion_sensor_globals",
                   "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x12a, 1);
    system_exit(-1);
  }
}

/* (0xdb140) */
void FUN_000db140(void)
{
}

/* Clear the motion sensor state and pre-initialize slot tables (0xdb150). */
void FUN_000db150(void)
{
  unsigned char *p;
  unsigned char *row;
  unsigned char *cell;
  int i;
  int j;
  int k;

  csmemset(*(void **)0x46bd2c, 0, 0x15a8);
  p = (unsigned char *)*(void **)0x46bd2c + 2;
  i = 4;
  do {
    j = 10;
    row = p;
    do {
      k = 0x10;
      cell = row;
      do {
        *cell = 6;
        cell += 4;
        k--;
      } while (k != 0);
      row += 0x84;
      j--;
    } while (j != 0);
    p += 0x568;
    i--;
  } while (i != 0);
}

/* (0xdb1b0) */
void FUN_000db1b0(void)
{
}

/* Install a motion sensor reference point and overlay scale, then rebuild the
 * radar overlay via the thunk at 0x17d050 (0xdb1e0).
 * The assert text names the @<esi> parameter "reference"; the meaning of the
 * remaining three stack parameters is unproven (param_2 is never read here,
 * it exists only because the word at [EBP+0x10] is).
 * Source: c:\halo\SOURCE\interface\motion_sensor.c line 0x349. */
void FUN_000db1e0(int *reference, int param_2, bool param_3, short param_4)
{
  if (reference == NULL) {
    display_assert("reference", "c:\\halo\\SOURCE\\interface\\motion_sensor.c",
                   0x349, 1);
    system_exit(-1);
  }
  *(short *)0x5aa676 = param_4;
  *(float *)0x2f66f4 = 0.75f;
  if (!param_3)
    *(float *)0x2f66f4 = 1.0f;
  *(int *)0x5aa680 = reference[0];
  *(int *)0x5aa684 = reference[1];
  FUN_0017d050();
}

/* Update and draw one local player's motion sensor (radar) for a screen point
 * (0xdbfb0).
 * param_1 is compared as an int16 against NONE (-1) but forwarded as the full
 * dword: MOV ESI,[EBP+8] / CMP SI,-1 / PUSH ESI / MOV ECX,ESI.
 * param_3 is the "pt" the assert names -- FUN_000dbcb0 reads it through
 * @<eax> as two int16s, so it is a point2d; the meaning of param_2 is
 * unproven, it is only forwarded on the stack and FUN_000dbcb0 never reads
 * that slot.
 * Source: c:\halo\SOURCE\interface\motion_sensor.c line 0x1dc. */
void FUN_000dbfb0(int param_1, int param_2, int param_3)
{
  if (param_3 == 0) {
    display_assert("pt", "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x1dc,
                   1);
    system_exit(-1);
  }
  if ((short)param_1 != -1) {
    update_motion_sensor(param_1);
    FUN_000dbcb0((short *)param_3, param_1, param_2);
  }
}

/**
 * Check whether it is time to start the attract-mode tab sequence.
 *
 * Returns true when the main menu has been idle long enough (>0x124f8 ms
 * ~= 75 seconds) with no input events.  As a side-effect, starts or stops
 * title music depending on whether the idle threshold (0x11f1c ms ~= 73 s)
 * has been crossed.
 */
bool event_manager_tab_check(void)
{
  unsigned int now;
  unsigned int last_event;
  bool attract_flag;

  if (cache_files_precache_in_progress()) {
    float progress;
    if (cache_files_precache_map_status(&progress) == 1)
      cache_files_precache_map_end();
  }

  if (ui_widget_is_main_menu_loaded() && !cache_files_precache_in_progress() &&
      !network_game_in_progress() && !bink_playback_active()) {
    now = system_milliseconds();
    last_event = event_manager_get_last_event_time();
    if (*(unsigned int *)0x46bd38 > last_event)
      last_event = *(unsigned int *)0x46bd38;
    attract_flag = ui_widget_get_attract_mode_flag();
    if (now - last_event >= 0x11f1c) {
      if (attract_flag)
        ui_widget_stop_attract_mode();
    } else {
      if (!attract_flag)
        ui_widget_start_title_music();
    }
    if (now - last_event >= 0x124f8)
      return true;
  }
  return false;
}

/**
 * Stop attract mode and all sounds, then play the credits Bink video.
 */
void FUN_000dc110(void)
{
  ui_widget_stop_attract_mode();
  sound_stop_all();
  bink_playback_start("d:\\bink\\credits.bik", 0x2e);
}

/**
 * Record the current time as the "mark" timestamp, used by
 * event_manager_tab_check to measure idle duration.
 */
void event_manager_mark_time(void)
{
  *(unsigned int *)0x46bd38 = system_milliseconds();
}

/**
 * Pick and play a random attract-mode Bink video, ensuring it differs
 * from the previously played one.  Resets the mark-time afterward so
 * the idle clock restarts when the video finishes.
 */
void event_manager_tab_process(void)
{
  const char *attract_files[3];
  int16_t idx;

  attract_files[0] = "d:\\bink\\attract1.bik";
  attract_files[1] = "d:\\bink\\attract2.bik";
  attract_files[2] = "d:\\bink\\attract3.bik";

  do {
    idx = seed_random_range(random_math_get_local_seed_address(), 0, 3);
    if (idx < 0)
      idx = 0;
    else if (idx > 2)
      idx = 2;
  } while (idx == *(int16_t *)0x2f670c);

  *(int16_t *)0x2f670c = idx;
  ui_widget_stop_attract_mode();
  bink_playback_start(attract_files[idx], 0x2e);

  if (!bink_playback_active())
    *(unsigned int *)0x46bd38 = system_milliseconds();
}

void event_manager_initialize(void)
{
  csmemset(event_manager_globals, 0, 0x108);
  *(_DWORD *)(event_manager_globals + 4) = system_milliseconds();
  event_manager_globals[0] = 1;
}

void event_manager_dispose(void)
{
  csmemset(event_manager_globals, 0, 0x108);
}

/**
 * Zero out the 0x100-byte event ring buffer, discarding all queued events.
 */
void event_manager_flush(void)
{
  csmemset((void *)0x46bd48, 0, 0x100);
}

/**
 * Set or clear the event suppression flag.  While suppressed,
 * event_manager_dispatch ignores all incoming events.
 */
void event_manager_suppress(int suppress)
{
  *(char *)0x46bd41 = (char)suppress;
}

/**
 * Retrieve the next queued event for the given local player (or any
 * player if player_index == NONE / -1).  Scans the per-player event
 * ring from newest to oldest, copies the first non-empty slot into
 * event_data, clears that slot, and returns true.  Returns false when
 * no events remain.
 */
bool event_manager_get_next_event(void *event_data, int16_t player_index)
{
  int i;
  int16_t pi;
  int16_t *slot;

  assert_halt(event_data &&
              ((player_index >= 0 && player_index < MAXIMUM_GAMEPADS) ||
               player_index == NONE));

  if (!event_manager_globals[0])
    return false;

  if (player_index == NONE) {
    for (pi = 0; pi < 4; pi++) {
      if (event_manager_get_next_event(event_data, pi))
        return true;
    }
    return false;
  }

  /* scan from slot 7 (newest) down to slot 0 (oldest) */
  slot = (int16_t *)(0x46bd80 + (int)player_index * 0x40);
  for (i = 7; i >= 0; i--) {
    if (*slot != 0) {
      int idx = i + (int)player_index * 8;
      *(int *)event_data = *(int *)(0x46bd48 + idx * 8);
      *((int *)event_data + 1) = *(int *)(0x46bd4c + idx * 8);
      *(int16_t *)(0x46bd48 + idx * 8) = 0;
      return true;
    }
    slot -= 4;
  }
  return false;
}

/**
 * Return the timestamp of the last non-empty event dispatched.
 */
unsigned int event_manager_get_last_event_time(void)
{
  return *(unsigned int *)0x46bd44;
}

void event_manager_dispatch(int16_t *event, int16_t player_index)
{
  bool dispatch;
  int now;
  int x, y;
  int ax, ay;
  int pi;

  if (*(char *)0x46bd41)
    return;

  now = system_milliseconds();

  if (event[0] == 1) {
    x = (int)event[2];
    y = (int)event[3];

    ax = x < 0 ? -x : x;
    if (ax < 0x7332) {
      ay = y < 0 ? -y : y;
      if (ay < 0x7332) {
        dispatch = false;
        goto store_stick1;
      }
    }

    ax = x < 0 ? -x : x;
    if (ax >= 0x7332) {
      pi = (int)player_index * 4;
      ay = *(int *)(0x46be68 + pi);
      if (ay < 0)
        ay = -ay;
      if (ay < 0x7332)
        goto record_stick1;
    }

    ay = y < 0 ? -y : y;
    if (ay >= 0x7332) {
      pi = (int)player_index * 4;
      ax = *(int *)(0x46be78 + pi);
      if (ax < 0)
        ax = -ax;
      if (ax < 0x7332)
        goto record_stick1;
    }

    pi = (int)player_index * 4;
    if ((unsigned int)(now - *(int *)(0x46be48 + pi)) < 0xfa) {
      dispatch = false;
      goto store_stick1;
    }

  record_stick1:
    *(int *)(0x46be48 + pi) = now;
    dispatch = true;

    ax = x < 0 ? -x : x;
    if (ax >= 0x7332) {
      if (x >= 0) {
        event[2] = 0x7fff;
        x = 0x7fff;
      } else {
        event[2] = (int16_t)0x8000;
        x = (int)(int16_t)0x8000;
      }
    }

    ay = y < 0 ? -y : y;
    if (ay >= 0x7332) {
      if (y >= 0) {
        event[3] = 0x7fff;
        y = 0x7fff;
      } else {
        event[3] = (int16_t)0x8000;
        y = (int)(int16_t)0x8000;
      }
    }

  store_stick1:
    *(int *)(0x46be68 + (int)player_index * 4) = x;
    *(int *)(0x46be78 + (int)player_index * 4) = y;
  } else if (event[0] == 2) {
    x = (int)event[2];
    y = (int)event[3];

    ax = x < 0 ? -x : x;
    if (ax < 0x7332) {
      ay = y < 0 ? -y : y;
      if (ay < 0x7332) {
        dispatch = false;
        goto store_stick2;
      }
    }

    ax = x < 0 ? -x : x;
    if (ax >= 0x7332) {
      pi = (int)player_index * 4;
      ay = *(int *)(0x46be88 + pi);
      if (ay < 0)
        ay = -ay;
      if (ay < 0x7332)
        goto record_stick2;
    }

    ay = y < 0 ? -y : y;
    if (ay >= 0x7332) {
      pi = (int)player_index * 4;
      ax = *(int *)(0x46be98 + pi);
      if (ax < 0)
        ax = -ax;
      if (ax < 0x7332)
        goto record_stick2;
    }

    pi = (int)player_index * 4;
    if ((unsigned int)(now - *(int *)(0x46be58 + pi)) < 0xfa) {
      dispatch = false;
      goto store_stick2;
    }

  record_stick2:
    *(int *)(0x46be58 + pi) = now;
    dispatch = true;

    ax = x < 0 ? -x : x;
    if (ax >= 0x7332) {
      if (x >= 0) {
        event[2] = 0x7fff;
        x = 0x7fff;
      } else {
        event[2] = (int16_t)0x8000;
        x = (int)(int16_t)0x8000;
      }
    }

    ay = y < 0 ? -y : y;
    if (ay >= 0x7332) {
      if (y >= 0) {
        event[3] = 0x7fff;
        y = 0x7fff;
      } else {
        event[3] = (int16_t)0x8000;
        y = (int)(int16_t)0x8000;
      }
    }

  store_stick2:
    *(int *)(0x46be88 + (int)player_index * 4) = x;
    *(int *)(0x46be98 + (int)player_index * 4) = y;
  } else {
    goto record_event;
  }

  if (!dispatch)
    return;

record_event:
  event[1] = player_index;
  pi = (int)player_index * 0x40;
  csmemmove((void *)(0x46bd48 + pi), (void *)(0x46bd50 + pi), 0x38);
  *(int *)(0x46bd48 + pi) = *(int *)event;
  *(int *)(0x46bd4c + pi) = *(int *)&event[2];
  if (event[0] != 0)
    *(int *)0x46bd44 = now;
}

void event_manager_update(void)
{
  int16_t event[4];
  int16_t empty_event[4];
  char *state;
  int i;
  int16_t j;
  bool had_event;

  if (!event_manager_globals[0])
    return;

  for (i = 0; (int16_t)i < 4; i++) {
    had_event = false;
    if (!input_has_gamepad(i) ||
        (state = (char *)input_get_gamepad_state(i)) == NULL)
      goto send_empty;

    /* left stick */
    if (*(int16_t *)(state + 0x20) != 0 || *(int16_t *)(state + 0x22) != 0) {
      *(int32_t *)&event[2] = *(int32_t *)(state + 0x20);
      event[0] = 1;
      event_manager_dispatch(event, (int16_t)i);
      had_event = true;
    }

    /* right stick */
    if (*(int16_t *)(state + 0x24) != 0 || *(int16_t *)(state + 0x26) != 0) {
      *(int32_t *)&event[2] = *(int32_t *)(state + 0x24);
      event[0] = 2;
      event_manager_dispatch(event, (int16_t)i);
      had_event = true;
    }

    /* buttons (16 digital buttons) */
    for (j = 0; j < 0x10; j++) {
      if (state[0x10 + j] != 0) {
        event[0] = 3;
        ((uint8_t *)&event[2])[0] = (uint8_t)j;
        ((uint8_t *)&event[2])[1] = (uint8_t)state[0x10 + j];
        event_manager_dispatch(event, (int16_t)i);
        had_event = true;
      }
    }

    if (had_event)
      continue;

  send_empty:
    *(int32_t *)&empty_event[1] = 0;
    empty_event[0] = 0;
    empty_event[3] = 0;
    event_manager_dispatch(empty_event, (int16_t)i);
  }
}

/* Wrapper: forward three args to animation_update_internal with update_kind=0
 * (0xdc730). */
void FUN_000dc730(int param_1, short *param_2, int *param_3)
{
  animation_update_internal(0, param_1, param_2, param_3);
}

/* Map a game-event type to a UI-widget event type. */
int16_t FUN_000dc800(int event)
{
  int result;

  switch ((int16_t)event) {
  case 0:
    result = 6;
    break;
  case 1:
    result = 7;
    break;
  case 2:
    result = 8;
    break;
  case 3:
    result = 9;
    break;
  case 4:
    result = 10;
    break;
  case 5:
    result = 11;
    break;
  case 6:
    result = 12;
    break;
  case 9:
    result = 13;
    break;
  case 10:
    result = 14;
    break;
  case 11:
    result = 18;
    break;
  case 12:
    result = 19;
    break;
  case 14:
    result = 4;
    break;
  case 15:
    result = 1;
    break;
  case 17:
    result = 20;
    break;
  case 16:
    result = 23;
    break;
  default:
    result = -1;
    break;
  }

  return (int16_t)result;
}
