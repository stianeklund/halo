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
    idx = random_range(random_math_get_local_seed_address(), 0, 3);
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
  ((void (*)(void *, void *, int))0x8dae0)((void *)(0x46bd48 + pi),
                                           (void *)(0x46bd50 + pi), 0x38);
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

/* Map a game-event type to a UI-widget event type. */
int16_t FUN_000dc800(int event)
{
  switch ((int16_t)event) {
  case 0:
    return 6;
  case 1:
    return 7;
  case 2:
    return 8;
  case 3:
    return 9;
  case 4:
    return 10;
  case 5:
    return 11;
  case 6:
    return 12;
  case 9:
    return 13;
  case 10:
    return 14;
  case 11:
    return 18;
  case 12:
    return 19;
  case 14:
    return 4;
  case 15:
    return 1;
  case 16:
    return 23;
  case 17:
    return 20;
  default:
    return -1;
  }
}
