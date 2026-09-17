/* multiplayer playlist profile edit dispose (0x0eea10) — asserts event_data is
 * non-null (halts and exits otherwise), then commits pending edits to the
 * multiplayer playlist profile. If nothing changed it reports the no-op, ends
 * the edit session, closes the widget's last child and marks *widget_deleted.
 * A dirty default profile whose name was never edited is instead routed
 * through the rename prompt. Otherwise the profile is saved and the save
 * result is returned. */
bool ui_widget_multiplayer_profile_save_changes(void *widget, void *event_data,
                                                bool *widget_deleted)
{
  void *last_child;
  bool result;

  result = false;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xdb6,
      1);
    system_exit(-1);
  }

  if (player_ui_edit_profile_is_dirty()) {
    if (player_ui_edit_profile_is_default_profile() &&
        !player_ui_edit_profile_name_is_dirty()) {
      if (!player_ui_prompt_user_to_rename_edit_profile()) {
        error(2, "failed to prompt user to rename profile");
      }
      return result;
    }

    result = player_ui_save_profile();
    if (!result) {
      error(2, "failed to save changes to multiplayer playlist profile");
    }
  } else {
    error(2, "no changes to playlist profile detected; not saving to disk");
    player_ui_end_editing_profile();
    last_child = ui_widget_get_last_child(widget);
    ui_widget_close(last_child);
    *widget_deleted = 1;
  }

  return result;
}

/* color picker menu dispose (event handler table index 62, 0x0eebe0) — frees
 * the child widget cached at +0x40 back to the widget pool, if present. */
bool ui_widget_color_picker_menu_dispose(void *widget, void *event_data,
                                         bool *widget_deleted)
{
  void *child;

  child = *(void **)((char *)widget + 0x40);
  if (child != NULL) {
    widget_free(child);
    *(void **)((char *)widget + 0x40) = NULL;
  }
  return true;
}

/* player profile color picker selection handler (event handler table index 63,
 * 0x0eec10) — validates the spinner list widget hanging off widget+0x38 (type
 * 2, definition tag 'DeLa' with 3 list items), bounds-checks the selected color
 * index at list_widget+0x3c against the profile colour count, then writes it
 * into the profile currently being edited at profile+0x18. Reports a deferred
 * error and returns false when no profile is being edited. */
bool player_profile_color_picker_select_color(void *widget, void *event_data,
                                              bool *widget_deleted)
{
  int *list_widget;
  void *profile;
  short *list_tag;

  (void)event_data;
  (void)widget_deleted;

  list_widget = *(int **)((char *)widget + 0x38);
  profile = player_ui_get_edit_player_profile();

  if (list_widget == NULL || *(short *)((char *)list_widget + 0xe) != 2) {
    display_assert(
      "expected the color select screen to contain a spinner list for the "
      "color picker",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe2e,
      1);
    system_exit(-1);
  }

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *list_widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'player color picker list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe35,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'player color picker list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe36,
      1);
    system_exit(-1);
  }

  if (*(short *)((char *)list_widget + 0x3c) < 0 ||
      (int)*(short *)((char *)list_widget + 0x3c) >= (int)FUN_001c0ed0()) {
    display_assert(
      "invalid player profile color index specified",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe3c,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2,
          "failed to set player profile color because no profile is currently "
          "being edited");
    return false;
  }

  *(short *)((char *)profile + 0x18) = *(short *)((char *)list_widget + 0x3c);
  return true;
}

/* player profile list selection handler (event handler table index 64,
 * 0x0eed10) — validates the 'player profile list' spinner widget (3 items)
 * hanging off widget+0x34, resolves the selected item's profile handle, and
 * either begins editing it, plays a deny sound (no profile / handle == -1),
 * or reports a deferred error (handle >= 0). */
bool FUN_000eed10(void *widget, void *event_data, bool *widget_deleted)
{
  short *list_tag;
  int *list_widget;
  short list_index;
  int profile_handle;

  (void)event_data;
  (void)widget_deleted;

  if (*(short *)((char *)widget + 0xe) != 0) {
    display_assert(
      "expected the player profile select screen to be a container widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe53,
      1);
    system_exit(-1);
  }

  *(int *)0x31e494 = -1; /* DAT_0031e494 — unknown purpose, cleared here */

  list_widget = *(int **)((char *)widget + 0x34);
  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)list_widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'player profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe5d,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'player profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe5e,
      1);
    system_exit(-1);
  }

  list_widget = *(int **)((char *)widget + 0x34);
  list_index = *(short *)((char *)list_widget + 0x3c);
  if (list_index < 0 ||
      (int)list_index >= (int)*(unsigned short *)((char *)list_widget + 0x44)) {
    display_assert(
      "invalid player profile specified from 'player profile list' list "
      "widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe67,
      1);
    system_exit(-1);
  }

  profile_handle = (*(int **)((char *)list_widget + 0x40))[list_index];

  if (profile_handle == -1) {
    ui_play_audio_feedback_sound(4);
    return false;
  }

  if (profile_handle < 0) {
    player_ui_begin_editing_profile(profile_handle);
    return true;
  }

  display_error_deferred(0x1f, -1, true, false);
  ui_play_audio_feedback_sound(4);
  return false;
}

/* player profile edit dispose (0x0eeeb0) — asserts event_data is non-null
 * (halts and exits otherwise), then saves any pending player-profile edits:
 * if nothing changed, no save is attempted; if a save was attempted and
 * succeeded, returns immediately. On no-op or save failure it reports the
 * condition via error(), ends the profile edit session, closes the widget's
 * last child, marks *widget_deleted, and returns false. */
bool FUN_000eeeb0(void *widget, void *event_data, bool *widget_deleted)
{
  void *last_child;
  bool profile_dirty;
  bool result;
  const char *message;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xeaf,
      1);
    system_exit(-1);
  }

  result = false;
  message = "no changes to player profile detected; not saving to disk";
  profile_dirty = player_ui_edit_profile_is_dirty();
  if (profile_dirty) {
    result = player_ui_save_profile();
    message = "failed to save changes to player profile";
  }

  if (!result) {
    error(2, message);
    player_ui_end_editing_profile();
    last_child = ui_widget_get_last_child(widget);
    ui_widget_close(last_child);
    *widget_deleted = 1;
  }

  return result;
}

/* remove local player from network game (0x0ef900, table xref 0x31e278) —
 * validates that the event's controller index (event_data+0x2) is in [0,4)
 * and, if so, quits that local player from the current network game. A NULL
 * event_data or an out-of-range controller index halts with an assert and
 * exits. */
bool ui_widget_remove_local_player_from_network_game(void *widget,
                                                     void *event_data,
                                                     bool *widget_deleted)
{
  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL || *(short *)((char *)event_data + 2) < 0 ||
      *(short *)((char *)event_data + 2) >= 4) {
    display_assert(
      "valid controller index required to remove player from network game",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfe9,
      1);
    system_exit(-1);
  }

  network_game_client_local_player_quit(*(short *)((char *)event_data + 2));
  return true;
}

/* multiplayer profile list selection handler (0x0ef970) — validates the
 * widget hierarchy (widget itself must be a container w/ 3+ children; the
 * sub-widget at widget+0x34 must be a 3-item spinner list), resolves the
 * selected item's profile handle, stores it to DAT_0031e494, and either
 * plays a deny sound (handle == -1, returns false) or returns true. Sibling
 * of FUN_000eed10 (player profile list) but with an extra tag_get-based
 * container check instead of a flag check, and no editing-session branch. */
bool FUN_000ef970(void *widget, void *event_data, bool *widget_deleted)
{
  short *container_tag;
  int *list_widget;
  short *list_tag;
  short list_index;
  int profile_handle;

  (void)event_data;
  (void)widget_deleted;

  container_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*container_tag != 0 || *(int *)((char *)container_tag + 0x3e0) < 3) {
    display_assert(
      "expected the multiplayer profile select screen to be a container w/ "
      "3+ children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x100f, 1);
    system_exit(-1);
  }

  list_widget = *(int **)((char *)widget + 0x34);
  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)list_widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1012, 1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1013, 1);
    system_exit(-1);
  }

  list_widget = *(int **)((char *)widget + 0x34);
  list_index = *(short *)((char *)list_widget + 0x3c);
  if (list_index < 0 ||
      (int)list_index >= (int)*(unsigned short *)((char *)list_widget + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'multiplayer profile "
      "list' list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x101c, 1);
    system_exit(-1);
  }

  profile_handle = (*(int **)((char *)list_widget + 0x40))[list_index];
  *(int *)0x31e494 = profile_handle; /* DAT_0031e494 — unknown purpose */

  if (profile_handle == -1) {
    ui_play_audio_feedback_sound(4);
    return false;
  }

  return true;
}

/* create and edit a new player profile (0x0efde0, table xref 0x31e298) —
 * fetches a default "untitled profile" name, creates a new saved-game
 * profile for the event's controller index (event_data+0x2; sentinel -1
 * defaults to controller 0) under that name, begins editing it, copies the
 * default name into the now-editable profile's name buffer (max 11 chars +
 * NUL), and hands the buffer to the virtual keyboard for validation. Any
 * failure along the way reports a deferred error and plays the deny sound;
 * a validation failure does the same after also ending the edit session. */
bool FUN_000efde0(void *widget, void *event_data, bool *widget_deleted)
{
  wchar_t untitled_name[128];
  short controller_index;
  int profile_index;
  void *edit_name;
  bool validated;

  (void)widget;
  (void)widget_deleted;
  validated = false;

  controller_index = *(short *)((char *)event_data + 2);
  if (controller_index == -1) {
    controller_index = 0;
  }

  saved_game_file_get_useable_untitled_profile_name(untitled_name);
  if (untitled_name[0] == L'\0') {
    error(2, "unable to create a new untitled profile");
    goto failure;
  }

  profile_index = FUN_001c1720(controller_index, untitled_name);
  if (profile_index == -1) {
    error(2, "failed to create a new player profile");
    goto failure;
  }

  player_ui_begin_editing_profile(profile_index);
  edit_name = player_ui_get_edit_player_profile();
  if (edit_name == NULL) {
    error(2, "failed to retrieve editable player profile!");
    player_ui_end_editing_profile();
    goto failure;
  }

  ustrncpy((wchar_t *)edit_name, untitled_name, 0xb);
  ((wchar_t *)edit_name)[0xb] = L'\0';
  validated = virtual_keyboard_set_validation((wchar_t *)edit_name, 0x18, 8);
failure:
  if (!validated) {
    display_error_deferred(0x25, -1, true, false);
    ui_play_audio_feedback_sound(4);
  }

  return validated;
}

/* network start-time-change request handler (0x000efed0) — event handler
 * table entry, same 3-arg bool convention as the siblings above/below in
 * this file (widget/widget_deleted unused here; disasm never touches
 * EBP+8 or EBP+0x10). Always returns true (MOV AL,1 before every RET).
 *
 * Looks up the local network client (network_game_client_get), then scans
 * its player table (network_game_client_get_machine_index() + 0x242,
 * 16 entries, stride 0x20; network_player_is_valid() takes the entry base
 * at +0x226) for a valid entry whose machine index (entry+0) matches this
 * client's own machine index (FUN_00124c40) and whose local-player index
 * (entry+1) matches the field at event_data+2. On a match, requests a game
 * start-time change (request_type=1) and errors if it fails. */
bool FUN_000efed0(void *widget, void *event_data, bool *widget_deleted)
{
  void *client;
  char *player_base;
  unsigned short local_machine_index;
  char *entry;
  int i;
  bool time_change_ok;

  client = network_game_client_get();
  if (client != NULL) {
    player_base = (char *)network_game_client_get_machine_index(client);
    local_machine_index = FUN_00124c40(client);
    entry = player_base + 0x242;
    i = 0;
    while (1) {
      if (network_player_is_valid(entry - 0x1c) &&
          (short)*entry == (short)local_machine_index &&
          (short)entry[1] == *(short *)((char *)event_data + 2)) {
        break;
      }
      i = i + 1;
      entry = entry + 0x20;
      if (i > 0xf) {
        return true;
      }
    }
    time_change_ok = network_game_client_request_start_time_change(client, 1);
    if (!time_change_ok) {
      error(2, "network_game_client_request_start_time_change() failed");
    }
  }
  return true;
}

/* request start-time change (0x0eff70, table xref 0x31e2a0) — scans up to 16
 * player-record slots (client's machine-index base +0x242, stride 0x20) for
 * a valid player whose record bytes at +0x1c/+0x1d match this machine's
 * index and the event's controller index (event_data+0x2); on a match asks
 * the network client to request a start-time change, logging an error if the
 * request is refused. Always returns true regardless of outcome. */
bool FUN_000eff70(void *widget, void *event_data, bool *widget_deleted)
{
  void *client;
  void *machine_base;
  unsigned short local_machine_index;
  char *player_rec;
  int i;
  bool change_ok;

  (void)widget;
  (void)widget_deleted;

  client = network_game_client_get();
  if (client != NULL) {
    machine_base = network_game_client_get_machine_index(client);
    local_machine_index = FUN_00124c40(client);
    player_rec = (char *)machine_base + 0x242;
    i = 0;
    while (1) {
      if (network_player_is_valid(player_rec - 0x1c) &&
          (short)*player_rec == local_machine_index &&
          (short)player_rec[1] == *(short *)((char *)event_data + 2)) {
        break;
      }
      i++;
      player_rec += 0x20;
      if (i > 15) {
        return true;
      }
    }
    change_ok = network_game_client_request_start_time_change(client, 0);
    if (!change_ok) {
      error(2, "network_game_client_request_start_time_change() failed");
    }
  }
  return true;
}

/* disable if no xdemos (event handler table index 86, 0x0f0070) — marks the
 * widget disabled (+0x12) and clears its enabled/visible byte (+0x10) when no
 * Xbox demo content is installed. */
bool ui_widget_disable_if_no_xdemos(void *widget, void *event_data,
                                    bool *widget_deleted)
{
  if (!xbox_demos_available()) {
    *(uint8_t *)((char *)widget + 0x12) = 1;
    *(uint8_t *)((char *)widget + 0x10) = 0;
  }
  return true;
}

/* set single-player controller from event (0x0f00b0, table xref 0x31e2bc) —
 * asserts event_data is non-null (halts and exits otherwise), then sets the
 * single-player local player's controller index to the event's controller
 * index (event_data+0x2). Local player index is always 0. */
bool FUN_000f00b0(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL) {
    display_assert(
      "event != NULL",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x11b9, 1);
    system_exit(-1);
  }

  player_ui_set_single_player_local_player_controller(
    0, *(short *)((char *)event_data + 2));

  return true;
}

/* set second local-player controller from event, refusing a controller
 * already claimed by local player 0 (0x0f0100, table xref 0x31e2c0) —
 * asserts event_data is non-null, then compares the event's controller
 * index (event_data+0x2) against player 0's current controller. If they
 * match, shows error 0x12 (modal, no pause) and marks the widget deleted,
 * returning false. Otherwise assigns that controller to local player 1
 * and returns true. */
bool FUN_000f0100(void *widget, void *event_data, bool *widget_deleted)
{
  short controller_index;
  short current_controller;

  (void)widget;

  if (event_data == NULL) {
    display_assert(
      "event != NULL",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x11c7, 1);
    system_exit(-1);
  }

  controller_index = *(short *)((char *)event_data + 2);
  current_controller = player_ui_get_single_player_local_player_controller(0);

  if (controller_index == current_controller) {
    ui_widget_display_error(0x12, -1, 1, 0);
    *widget_deleted = 1;
    return false;
  }

  player_ui_set_single_player_local_player_controller(1, controller_index);
  return true;
}

/* check network availability, error if unavailable (0x0f0170, table xref
 * 0x31e2c4) — asserts event_data is non-null (halts and exits otherwise).
 * If transport_network_available() is false, shows error 5 with the
 * event's controller index (event_data+0x2, zero-extended) as the local
 * player index (modal, pauses game). Returns the network-available flag
 * regardless of which branch ran. */
bool FUN_000f0170(void *widget, void *event_data, bool *widget_deleted)
{
  bool network_available;

  (void)widget;
  (void)widget_deleted;

  network_available = transport_network_available();

  if (event_data == NULL) {
    display_assert(
      "event != NULL",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x11df, 1);
    system_exit(-1);
  }

  if (!network_available) {
    ui_widget_display_error(5, *(uint16_t *)((char *)event_data + 2), 1, 1);
  }

  return network_available;
}

/* start server if none advertised (0x0f01d0) — asserts widget+0xe is a
 * column-list widget type (3), then, if widget+0x44 (no visible advertised
 * servers) is zero, fetches the network client and, if present and its
 * connection state is 0 ("searching"), forwards this handler's own params to
 * FUN_000E9D40 and returns its result directly (the original tail-propagates
 * FUN_000E9D40's EAX into AL without touching it). If widget+0x44 is
 * non-zero, logs that a new server isn't being started because other servers
 * are already available. Falls through to false on: missing client, non-zero
 * client state, or the log branch. */
bool ui_widget_start_server_if_none_advertised(void *widget, void *event_data,
                                               bool *widget_deleted)
{
  void *client;
  int16_t state;
  int16_t elapsed_pct; /* discarded out-param; MSVC reuses the dead
                        * 'widget' incoming-param stack slot (EBP+0xa) for
                        * this scratch write since widget is already
                        * cached in ESI by this point */

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected a column list for server list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x11f1, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)widget + 0x44) == 0) {
    client = network_game_client_get();
    if (client != NULL) {
      state = network_game_client_get_state(client, &elapsed_pct);
      if (state == 0) {
        return FUN_000E9D40(widget, event_data, widget_deleted);
      }
    }
  } else {
    error(2, "not attempting to start a new server; there are other servers "
             "available");
  }

  return false;
}

/* unjoin network-game player (0x0f0250) — scans the local client's 16
 * player records at machine-index base +0x226 (stride 0x20) for records on
 * the local machine matching event_data+2. Requests removal of that matching
 * record, clears its local-player autojoin flag, and, when exactly one local
 * record was found, tears down or pauses the server before copying autojoin
 * flags to the next multiplayer game. */
bool ui_widget_network_game_unjoin_player(void *widget, void *event_data,
                                          bool *widget_deleted)
{
  void *client;
  char *record;
  char *matched_record;
  short local_machine_index;
  int local_record_count;
  int i;
  void *server;

  (void)widget;
  (void)widget_deleted;

  client = network_game_client_get();
  if (client == NULL) {
    return true;
  }

  matched_record = NULL;
  local_machine_index = network_game_client_get_local_machine_index();
  record = (char *)network_game_client_get_machine_index(client) + 0x226;
  local_record_count = 0;
  i = 16;
  do {
    if (network_player_is_valid(record) &&
        *(signed char *)(record + 0x1c) == local_machine_index) {
      local_record_count = local_record_count + 1;
      if (*(signed char *)(record + 0x1d) ==
          *(short *)((char *)event_data + 2)) {
        if (matched_record != NULL) {
          display_assert(
            "duplicate player registered in game",
            "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
            0x1226, 1);
          system_exit(-1);
        }
        matched_record = record;
      }
    }
    record = record + 0x20;
    i = i - 1;
  } while (i != 0);

  if (local_record_count <= 0) {
    return true;
  }

  if (matched_record != NULL) {
    if (!network_game_client_request_remove_player(client, matched_record)) {
      error(2, "failed to request player removal");
    }
    player_ui_clear_multiplayer_autojoin_for_local_player(
      *(signed char *)(matched_record + 0x1d));
  }

  if (local_record_count == 1) {
    server = network_game_server_get();
    if (server == NULL || !network_game_accept_remote_connections()) {
      dispose_global_network_game_server();
      dispose_global_network_game_client();
    } else {
      server = network_game_server_get();
      if (server != NULL) {
        network_game_server_pause_countdown(server, 1);
        player_ui_autojoin_players_to_next_multiplayer_game();
        return true;
      }
    }
    player_ui_autojoin_players_to_next_multiplayer_game();
    return true;
  }

  return false;
}

/* FUN_000f03d0 (0xf03d0, table xref 0x31e2d0) — closes the widget's last
 * child when neither an in-progress player profile edit nor an in-progress
 * playlist profile edit is active ("no saved game file being edited"
 * cancel path). */
void FUN_000f03d0(void *widget)
{
  void *child;

  if (player_ui_get_edit_player_profile() == NULL &&
      player_ui_get_edit_playlist_profile() == NULL) {
    child = ui_widget_get_last_child(widget);
    error(2, "closing widget '%s' because no saved game file is being edited",
          *(const char **)((char *)child + 4));
    *(uint32_t *)((char *)child + 0x1c) = 1;
    *(uint8_t *)((char *)child + 0x10) = 0;
  }
}

/* new campaign chosen (0x0f0430) — asserts + exits if event_data is NULL.
 * Fetches an unused campaign save-profile name into a scratch wide buffer,
 * copies the first 11 chars (+ null terminator) into the global campaign
 * name-entry buffer (DAT_0046ccd0, 12 x wchar_t), stashes event_data+0x2 (a
 * caller-supplied 16-bit value) into DAT_0031e4fc, then opens the virtual
 * keyboard to let the player edit the name. Logs an error (does not fail)
 * if the keyboard couldn't be invoked; always returns true. */
bool ui_widget_new_campaign_chosen(void *widget, void *event_data,
                                   bool *widget_deleted)
{
  wchar_t campaign_name[128];
  bool keyboard_ok;

  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1285, 1);
    system_exit(-1);
  }

  saved_game_file_get_useable_untitled_profile_name(campaign_name);
  ustrncpy((wchar_t *)0x46ccd0, campaign_name, 0xb);
  *(uint16_t *)0x46cce6 = 0; /* DAT_0046cce6 — null terminator at index 11 */
  *(uint16_t *)0x31e4fc =
    *(uint16_t *)((char *)event_data + 2); /* DAT_0031e4fc */

  keyboard_ok = virtual_keyboard_set_validation((wchar_t *)0x46ccd0, 0x18, 8);
  if (!keyboard_ok) {
    error(2, "failed to invoke the virtual keyboard for a new campaign profile "
             "name");
  }

  return true;
}

/* virtual-keyboard completion handler for the new campaign name
 * (0xf04c0). The virtual-keyboard done flag gates campaign profile creation;
 * its pending controller index and editable name are held in the global UI
 * state initialized by ui_widget_new_campaign_chosen. */
void FUN_000f04c0(void)
{
  wchar_t player_profile[24];
  int profile_index;
  const char *message;
  bool profile_created;

  if (*(short *)0x31e4fc != -1) {
    if (FUN_000f5650()) {
      if (*(wchar_t *)0x46ccd0 != L'\0') {
        player_ui_set_single_player_local_player_controller(0,
                                                            *(short *)0x31e4fc);
        profile_index = FUN_001c1720(*(short *)0x31e4fc, (wchar_t *)0x46ccd0);
        if (profile_index == -1) {
          saved_game_file_get_useable_untitled_profile_name(player_profile);
          ustrncpy((wchar_t *)0x46ccd0, player_profile, 0xb);
          *(wchar_t *)0x46cce6 = L'\0';
          profile_index = FUN_001c1720(*(short *)0x31e4fc, (wchar_t *)0x46ccd0);
          if (profile_index == -1) {
            message = "failed to create new player profile";
            error(2, message);
            main_goto_main_menu();
            display_error_deferred(0x25, -1, true, false);
            ui_play_audio_feedback_sound(4);
            *(short *)0x31e4fc = -1;
            return;
          }
        }
        profile_created = player_profile_new(profile_index, player_profile);
        if (profile_created) {
          player_ui_set_active_player_profile(0, profile_index, player_profile);
          main_set_map_name(*(const char **)0x31e498);
          main_defer_map_map_change();
          *(short *)0x31e4fc = -1;
          return;
        }
        message = "failed to retrieve newly created player profile";
        error(2, message);
        main_goto_main_menu();
        display_error_deferred(0x25, -1, true, false);
        ui_play_audio_feedback_sound(4);
      } else {
        error(2, "can't create a new profile with an empty name");
        ui_play_audio_feedback_sound(4);
      }
    }
    *(short *)0x31e4fc = -1;
  }
}

/* pop history stack once (event handler table index 98, 0x0f0620) — pops one
 * entry from the widget history stack of the widget's local player (+0x8). */
bool ui_widget_pop_history_stack_once(void *widget, void *event_data,
                                      bool *widget_deleted)
{
  ui_widgets_pop_stack(*(uint16_t *)((char *)widget + 0x8));
  return true;
}

/* difficulty menu item select (0xf0640, ui_widget_game_data_function_table
 * xref 0x31e2e4) — asserts the widget is a column list (widget+0xe == 3).
 * If the "difficulty forced for this map" flag (DAT_0046ce3b) is set and
 * the current map (main_get_map_name()) case-insensitively matches the
 * forced-map name string (DAT_0046cd38), preselects the forced difficulty
 * child index (DAT_0046ce38, a stored int16); otherwise preselects index 1
 * (default difficulty). Stores the resolved child widget pointer at
 * widget+0x38 and the selected index at widget+0x3c — the same "selected
 * list item" field pair FUN_000f46e0 uses at +0x3c for its spinner list. */
bool ui_widget_game_data_select_difficulty_item(void *widget)
{
  const char *map_name;
  void *child;
  short forced_index;

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for difficulty menu widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x12fc, 1);
    system_exit(-1);
  }

  if (*(unsigned char *)0x46ce3b == 1) {
    map_name = main_get_map_name();
    if (crt_stricmp((const char *)0x46cd38, map_name) == 0) {
      forced_index = *(short *)0x46ce38; /* DAT_0046ce38 */
      child = widget_instance_get_nth_child(widget, forced_index);
      if (child == NULL) {
        display_assert(
          "failed to find 'difficulty' menu item",
          "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
          0x1301, 1);
        system_exit(-1);
      }
      *(void **)((char *)widget + 0x38) = child;
      *(short *)((char *)widget + 0x3c) = *(short *)0x46ce38;
      return true;
    }
  }

  child = widget_instance_get_nth_child(widget, 1);
  if (child == NULL) {
    display_assert(
      "failed to find 'difficulty' menu item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1307, 1);
    system_exit(-1);
  }
  *(void **)((char *)widget + 0x38) = child;
  *(short *)((char *)widget + 0x3c) = 1;
  return true;
}

/* new campaign if no custom player profiles exist (0xf0740) — calls the
 * saved-game profile enumerator at 0x1c0d50 with the same argument shape as
 * FUN_000e5590 (index -1, two out-param locals with local_4 pre-set to 1,
 * trailing flag 0 here). The enumerator's result is read back as a signed
 * 16-bit value: if it is positive the handler does nothing and returns true;
 * otherwise it forwards the event to ui_widget_new_campaign_chosen (whose
 * return value is discarded — the constant false is materialised in the
 * callee-saved BL before the call) and returns false. */
bool new_campaign_if_no_custom_player_profiles_exist(void *widget,
                                                     void *event_data,
                                                     bool *widget_deleted)
{
  int local_4;
  int local_8;

  local_4 = 1;
  FUN_001c0d50(-1, &local_4, &local_8, 0);
  if ((short)local_4 > 0) {
    return true;
  }

  ui_widget_new_campaign_chosen(widget, event_data, widget_deleted);
  return false;
}

void ui_widget_game_data_function_invoke(
  void *widget, unsigned __int16 game_data_input_reference_function)
{
  assert_halt(widget);

  if (game_data_input_reference_function > 40u) {
    error(2, "invalid game_data_input_reference_function");
  } else {
    ui_widget_game_data_function_table[game_data_input_reference_function](
      widget);
  }
}

/* FUN_000f0aa0 (0xf0aa0)
 * Updates the extended-description text/pic widgets for a "settings select"
 * list widget's currently highlighted item. Resolves the widget's owner's
 * definition tag via widget+0x48 (tag_get('DeLa', tag_index)) and asserts
 * it is a settings-select widget definition (tag+0x3e0 == 2). Walks the
 * sibling chain at widget+0x34 (via +0x2c "next sibling"), counting the
 * index of the previously-selected child (widget+0x38), then asserts the
 * shape of the extended-description container hanging off
 * (*(widget+0x48))+0x34 — a container widget (+0xe==0) whose first child
 * (+0x2c) is a text-box widget (+0xe==1) — and writes the resolved index
 * into both the container (+0x50) and its text child (+0x40).
 *
 * MSVC reuses the dead incoming widget parameter's stack slot (EBP+8) to
 * cache the text-box child pointer once it is no longer needed as widget;
 * represented below with a separate local (text_widget). */
void FUN_000f0aa0(void *widget)
{
  void *widget_def;
  void *description_container;
  void *text_widget;
  int child;
  short index;

  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, **(int **)((char *)widget + 0x48));

  if ((*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x12c, 1);
    system_exit(-1);
  }

  if (*(int *)((char *)widget_def + 0x3e0) != 2) {
    display_assert(
      "this doesn't look like the settings select widget to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x12d, 1);
    system_exit(-1);
  }

  description_container =
    *(void **)((char *)*(int *)((char *)widget + 0x48) + 0x34);
  text_widget = *(void **)((char *)description_container + 0x2c);

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  if (*(short *)((char *)description_container + 0xe) != 0) {
    display_assert(
      "expected a container widget for the settings select list extended "
      "description pic",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x138, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)text_widget + 0xe) == 1) {
    *(short *)((char *)description_container + 0x50) = index;
    *(short *)((char *)text_widget + 0x40) = index;
    return;
  }

  display_assert(
    "expected a text box widget for the settings select list extended "
    "description text",
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x139,
    1);
  system_exit(-1);
}

/* FUN_000f0bb0 (0xf0bb0)
 * Same purpose as FUN_000f0aa0 above (updates an extended-description
 * widget for a "settings select" list widget's currently highlighted
 * item), but for a widget whose extended-description owner (widget+0x48)
 * holds the index directly on its container ((*(widget+0x48))+0x34) and
 * that container's first child (+0x2c), rather than distinguishing a
 * container/text-box pair by type. Counts the index of widget's
 * currently-selected sibling (widget+0x34 chain via +0x2c, compared
 * against widget+0x38), resolves the owner's definition tag via
 * tag_get('DeLa', *(widget+0x48)) and asserts it is a 2-child widget
 * definition (tag+0x3e0 == 2), then writes the resolved index into both
 * the container (+0x40) and its first child (+0x50). */
void FUN_000f0bb0(void *widget)
{
  int child;
  short index;
  void *widget_def;
  int container;

  if ((widget == 0) || (*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x14d, 1);
    system_exit(-1);
  }

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, *(int *)*(int *)((char *)widget + 0x48));

  if (*(int *)((char *)widget_def + 0x3e0) != 2) {
    display_assert(
      "expected a container widget w/ 2 children for the playlist settings "
      "list extended description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x15b, 1);
    system_exit(-1);
  }

  container = *(int *)((char *)*(int *)((char *)widget + 0x48) + 0x34);
  *(short *)((char *)container + 0x40) = index;
  *(short *)((char *)*(int *)((char *)container + 0x2c) + 0x50) = index;
}

/* FUN_000f0c60 (0xf0c60)
 * Same shape as FUN_000f0bb0 above (updates an extended-description widget
 * for a "settings select" list widget's currently highlighted item, owner
 * holding the index directly on its container), reusing the identical
 * pooled string literals for both display_assert messages — only the
 * embedded __LINE__ values (0x173, 0x181 vs 0x14d, 0x15b) differ. */
void FUN_000f0c60(void *widget)
{
  int child;
  short index;
  void *widget_def;
  int container;

  if ((widget == 0) || (*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x173, 1);
    system_exit(-1);
  }

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, *(int *)*(int *)((char *)widget + 0x48));

  if (*(int *)((char *)widget_def + 0x3e0) != 2) {
    display_assert(
      "expected a container widget w/ 2 children for the playlist settings "
      "list extended description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x181, 1);
    system_exit(-1);
  }

  container = *(int *)((char *)*(int *)((char *)widget + 0x48) + 0x34);
  *(short *)((char *)container + 0x40) = index;
  *(short *)((char *)*(int *)((char *)container + 0x2c) + 0x50) = index;
}

/* playlist_settings_menu_update_extended_description (0xf0d10)
 * Same purpose as FUN_000f0bb0/FUN_000f0c60 above (updates an
 * extended-description widget for a "settings select" list widget's
 * currently-highlighted item: counts the index of widget's currently
 * selected sibling via the +0x34/+0x2c chain compared against widget+0x38),
 * but does not resolve a 'DeLa' tag definition — instead it validates
 * widget+0x48's container (+0x34) and that container's first child (+0x2c)
 * directly, and writes the resolved index to container+0x50 and
 * (container's first child)+0x40, the offsets swapped relative to
 * FUN_000f0bb0/FUN_000f0c60's +0x40/+0x50 writes. The final two stores each
 * re-derive the container from widget+0x48 independently (matching two
 * separate reloads in the disassembly, 0xf0d75 and 0xf0d7f), rather than
 * reusing the value computed for the guard check. */
void playlist_settings_menu_update_extended_description(void *widget)
{
  int container;
  int child;
  short index;

  if ((widget == 0) || (*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0) ||
      (container = *(int *)((char *)*(int *)((char *)widget + 0x48) + 0x34),
       container == 0) ||
      (*(int *)((char *)container + 0x2c) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x1a6, 1);
    system_exit(-1);
  }

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  *(short *)((char *)*(int *)((char *)*(int *)((char *)widget + 0x48) + 0x34) +
             0x50) = index;
  *(short *)((char *)*(
               int *)((char *)*(int *)((char *)*(int *)((char *)widget + 0x48) +
                                       0x34) +
                      0x2c) +
             0x40) = index;
}

/* FUN_000f0d90 (0xf0d90)
 * Same shape as FUN_000f0aa0 above (updates the extended-description
 * text/pic widgets for a "settings select" list widget's currently
 * highlighted item), but for the difficulty-select widget: resolves the
 * widget's owner's definition tag via widget+0x48 (tag_get('DeLa',
 * tag_index)) and asserts it is a 2-child widget definition (tag+0x3e0 ==
 * 2). Walks the sibling chain at widget+0x34 (via +0x2c "next sibling"),
 * counting the index of the previously-selected child (widget+0x38), then
 * asserts the shape of the extended-description container hanging off
 * (*(widget+0x48))+0x34 — a container widget (+0xe==0) whose first child
 * (+0x2c) is a text-box widget (+0xe==1) — and writes the resolved index
 * into both the container (+0x50) and its text child (+0x40). */
void FUN_000f0d90(void *widget)
{
  void *widget_def;
  void *description_container;
  void *text_widget;
  int child;
  short index;

  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, **(int **)((char *)widget + 0x48));

  if ((*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x237, 1);
    system_exit(-1);
  }

  if (*(int *)((char *)widget_def + 0x3e0) != 2) {
    display_assert(
      "this doesn't look like the difficulty select widget to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x238, 1);
    system_exit(-1);
  }

  description_container =
    *(void **)((char *)*(int *)((char *)widget + 0x48) + 0x34);
  text_widget = *(void **)((char *)description_container + 0x2c);

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  if (*(short *)((char *)description_container + 0xe) != 0) {
    display_assert(
      "expected a container widget for the difficulty list extended "
      "description pic",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x243, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)text_widget + 0xe) == 1) {
    *(short *)((char *)description_container + 0x50) = index;
    *(short *)((char *)text_widget + 0x40) = index;
    return;
  }

  display_assert(
    "expected a text box widget for the difficulty list extended "
    "description text",
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x244,
    1);
  system_exit(-1);
}

void ui_widget_game_data_build_version(int widget)
{
  wchar_t *v1, *v2; // eax

  if (!ui_widget_game_data_build_version_wide_str[0]) {
    ascii_to_wide(
#if DECOMP_CUSTOM
      build_ui_widget_text,
#else
      "01.10.12.2276",
#endif
      ui_widget_game_data_build_version_wide_str,
      sizeof(ui_widget_game_data_build_version_wide_str));
  }

  if (!*(uint32_t *)(widget + 60)) {
    v1 =
      ui_widget_realloc(0, sizeof(ui_widget_game_data_build_version_wide_str),
                        __FILE__, __LINE__);
    *(uint32_t *)(widget + 60) = (uint32_t)v1;
    if (v1) {
      csmemset(v1, 0, sizeof(ui_widget_game_data_build_version_wide_str));
    }
  }
  v2 = *(wchar_t **)(widget + 60);
  if (v2) {
    ustrncpy(v2, ui_widget_game_data_build_version_wide_str, 0x3Fu);
    *(wchar_t *)(*(uint32_t *)(widget + 60) + 126) = 0;
  }
}

/* netgame_prejoin_players (0xf2390). While the client is in state 2 (joining),
 * builds a 4-entry table of local players that want to play multiplayer,
 * clears the entries for local players already present in the network game
 * player list (16 slots of 0x20 bytes at game+0x226, local-player index byte
 * at +0x243), then sends a join request for each remaining wanted player. */
void netgame_prejoin_players(void)
{
  void *client;
  int game;
  int index;
  char wants_to_play[4];

  client = network_game_client_get();
  if (client == NULL) {
    return;
  }
  if (network_game_client_get_state(client, &index) != 2) {
    return;
  }

  game = network_game_get_game();
  if (game == 0) {
    display_assert(
      "game",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x614, 1);
    system_exit(-1);
  }

  index = 0;
  do {
    wants_to_play[(short)index] =
      (char)player_ui_local_player_wants_to_play_multiplayer((short)index);
    index++;
  } while ((short)index < 4);

  index = 0;
  do {
    if (network_player_is_valid(
          (void *)((char *)(uintptr_t)game + (short)index * 0x20 + 0x226)) &&
        network_game_player_is_local(
          (void *)((char *)(uintptr_t)game + (short)index * 0x20 + 0x226))) {
      wants_to_play[(int)*(char *)((char *)(uintptr_t)game +
                                   (short)index * 0x20 + 0x243)] = 0;
    }
    index++;
  } while ((short)index < 0x10);

  index = 0;
  do {
    if (wants_to_play[(short)index] != 0) {
      if (!network_game_client_add_player(client, (uint16_t)index)) {
        network_game_log("failed to send join request");
      }
    }
    index++;
  } while ((short)index < 4);
}

/* player-profile three-column list update (0x0f2560). Validates the column
 * list and extended-description text widget, then stores the selected list
 * item's spinner setting into the text widget. */
void player_profile_3wide_list_update(void *widget)
{
  void *text_widget;
  void *child;
  void *spinner;
  void *widget_definition;
  int spinner_value;
  short list_value;

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game options list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x8f5, 1);
    system_exit(-1);
  }

  text_widget = *(void **)((char *)widget + 0x48);
  if (text_widget == NULL || *(short *)((char *)text_widget + 0xe) != 1) {
    display_assert(
      "expected a text box for multiplayer game options list extended "
      "description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x8f8, 1);
    system_exit(-1);
  }

  widget_definition = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*(int *)((char *)widget_definition + 0x3e0) < 1) {
    display_assert(
      "expected some list items for multiplayer game settings list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x8fe, 1);
    system_exit(-1);
  }

  if (*(void **)((char *)widget + 0x38) == NULL) {
    *(short *)((char *)text_widget + 0x40) = (short)(uintptr_t)widget;
    return;
  }

  child = *(void **)((char *)widget + 0x34);
  list_value = 0;
  if (child != NULL) {
    do {
      spinner = *(void **)((char *)child + 0x34);
      while (spinner != NULL && *(short *)((char *)spinner + 0xe) != 2) {
        spinner = *(void **)((char *)spinner + 0x2c);
      }
      if (spinner == NULL) {
        display_assert(
          "expected a spinner list somewhere in the multiplayer game options "
          "settings list item makeup",
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x90c, 1);
        system_exit(-1);
      }

      if (child == *(void **)((char *)widget + 0x38)) {
        list_value = (short)(list_value + *(short *)((char *)spinner + 0x3c));
        break;
      }

      spinner_value = *(unsigned short *)((char *)spinner + 0x44);
      list_value = (short)(list_value + spinner_value);
      child = *(void **)((char *)child + 0x2c);
    } while (child != NULL);
  }

  *(short *)((char *)text_widget + 0x40) = list_value;
}

/* FUN_000f2690 (0xf2690)
 * "objective text" data-driven text box widget update. Fetches the current
 * hud objective string (empty if hud_messaging_get_objective() returns NULL
 * or an empty string), requires the widget to be a text box (type == 1 at
 * +0xe), then, when the text is non-empty, reallocates the widget's text
 * buffer (+0x3c) to fit it and copies it in, null-terminated. */
void FUN_000f2690(void *widget)
{
  wchar_t *objective_text;
  wchar_t *new_buf;
  unsigned int len;

  objective_text = (wchar_t *)hud_messaging_get_objective();
  if (objective_text == NULL || *objective_text == 0) {
    len = 0;
  } else {
    len = (unsigned int)ustrlen((unsigned short *)objective_text);
  }

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box for objective text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x924, 1);
    system_exit(-1);
  }

  if (0 < (int)len) {
    new_buf = (wchar_t *)ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), (unsigned short)(len * 2 + 2),
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x928);
    *(wchar_t **)((char *)widget + 0x3c) = new_buf;
    if (new_buf != NULL) {
      ustrncpy(new_buf, objective_text, len);
      *(unsigned short *)((char *)*(wchar_t **)((char *)widget + 0x3c) +
                          len * 2) = 0;
    }
  }
}

/* FUN_000f2720 (0xf2720) — validates a three-column game-options list and
 * its extended-description picture container, then sums each preceding
 * spinner's item count and the selected spinner's selected-item index into
 * the picture widget's +0x50 word. */
void FUN_000f2720(void *widget)
{
  void *picture_widget;
  void *list_item;
  void *spinner;
  void *widget_definition;
  short list_value;

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for game options list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x980, 1);
    system_exit(-1);
  }

  picture_widget = *(void **)((char *)widget + 0x48);
  if (picture_widget == NULL || *(short *)((char *)picture_widget + 0xe) != 0) {
    display_assert(
      "expected a picture (container) for game options list extended "
      "description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x983, 1);
    system_exit(-1);
  }

  widget_definition = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*(int *)((char *)widget_definition + 0x3e0) < 1) {
    display_assert(
      "expected some list items for game settings list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x988, 1);
    system_exit(-1);
  }

  if (*(void **)((char *)widget + 0x38) == NULL) {
    *(short *)((char *)picture_widget + 0x50) = (short)(uintptr_t)widget;
    return;
  }

  list_item = *(void **)((char *)widget + 0x34);
  list_value = 0;
  if (list_item != NULL) {
    while (1) {
      spinner = *(void **)((char *)list_item + 0x34);
      while (spinner != NULL && *(short *)((char *)spinner + 0xe) != 2) {
        spinner = *(void **)((char *)spinner + 0x2c);
      }
      if (spinner == NULL) {
        display_assert(
          "expected a spinner list somewhere in the multiplayer game options "
          "settings list item makeup",
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x996, 1);
        system_exit(-1);
      }

      if (list_item == *(void **)((char *)widget + 0x38)) {
        list_value = (short)(list_value + *(short *)((char *)spinner + 0x3c));
        break;
      }

      list_item = *(void **)((char *)list_item + 0x2c);
      list_value =
        (short)(list_value + *(unsigned short *)((char *)spinner + 0x44));
      if (list_item == NULL) {
        *(short *)((char *)picture_widget + 0x50) = list_value;
        return;
      }
    }
  }

  *(short *)((char *)picture_widget + 0x50) = list_value;
}

/* main_menu_animation_fakery (0xf2850) — requires the widget to be a column
 * list (type == 3 at +0xe) and its cached extended-description child at +0x48
 * to be a non-NULL container/picture widget (type == 0 at +0xe), then copies
 * the column list's selected index (+0x3c, signed 16-bit) into the picture
 * widget's +0x50 slot, clamped at zero. The reference re-loads widget+0x48
 * after the store and writes +0x50 on both clamp branches. Evidence:
 * reference disassembly at 0xf2850-0xf28d5 (assert strings/lines are the
 * reference's own PUSH immediates at 0xf2860/0xf2865/0xf286a and
 * 0xf288e/0xf2893/0xf2898). */
void main_menu_animation_fakery(void *widget)
{
  char *widget_bytes;
  void *picture_widget;
  int zero;
  short value;

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for main menu options list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x9ab, 1);
    system_exit(-1);
  }

  picture_widget = *(void **)((char *)widget + 0x48);
  if (picture_widget == (void *)(zero = 0) ||
      *(short *)((char *)picture_widget + 0xe) != zero) {
    display_assert(
      "expected a picture (container) for main menu options list extended "
      "description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x9ae, 1);
    system_exit(-1);
  }

  picture_widget = *(void **)((char *)widget + 0x48);
  widget_bytes = (char *)widget;
  *(short *)((char *)picture_widget + 0x50) = *(short *)(widget_bytes + 0x3c);

  picture_widget = *(void **)(widget_bytes + 0x48);
  value = *(short *)((char *)picture_widget + 0x50);
  if (value < zero) {
    *(short *)((char *)picture_widget + 0x50) = (short)zero;
  } else {
    *(short *)((char *)(*(void **)(widget_bytes + 0x48)) + 0x50) = value;
  }
}

/* FUN_000f28e0 (0xf28e0)
 * "profile display name" data-driven text box widget update. Requires the
 * widget to be a text box (type == 1 at +0xe) and its bound local player
 * index (+0x8, a signed 16-bit slot) to be in range
 * [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS), fetches that player's active profile
 * (a 0x30-byte opaque record, see player_ui_get_active_player_profile),
 * reallocates the widget's text buffer (+0x3c) to a fixed 0x18 bytes, and
 * copies the first 11 wchar_t of the profile record (its name field) in,
 * null-terminated at wchar index 11 (byte offset 0x16). Evidence: reference
 * disassembly at 0xf28e0-0xf298c (assert strings/lines are the reference's
 * own PUSH immediates at 0xf2911/0xf2917/0xf2940/0xf2946). */
void FUN_000f28e0(void *widget)
{
  unsigned char profile[0x30];
  wchar_t *new_buf;
  short local_player_index;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for profile display name",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x9ec, 1);
    system_exit(-1);
  }

  local_player_index = *(short *)((char *)widget + 8);
  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(
      "profile display name requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x9ee, 1);
    system_exit(-1);
  }

  player_ui_get_active_player_profile(local_player_index, profile);

  new_buf = (wchar_t *)ui_widget_realloc(
    *(int *)((char *)widget + 0x3c), 0x18,
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
    0x9f0);
  *(wchar_t **)((char *)widget + 0x3c) = new_buf;
  if (new_buf != NULL) {
    ustrncpy(new_buf, (wchar_t *)profile, 0xb);
    *(unsigned short *)((char *)new_buf + 0x16) = 0;
  }
}

/* get_active_player_profile_color_index (0xf2b00) — "profile color picture"
 * data-driven widget update. Requires the widget's bound local player index
 * (+0x8, signed 16-bit) to be in [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS), fetches
 * that player's active profile (the same 0x30-byte opaque record as
 * player_ui_get_active_player_profile), reads its color index at +0x18, and
 * stores it into the widget's +0x50 value slot clamped to
 * [0, FUN_001c0ed0() - 1]. A negative profile color index stores 0. The
 * reference calls FUN_001c0ed0 twice on the over-range path (0xf2b64 and
 * 0xf2b71) — both calls are preserved. Evidence: reference disassembly at
 * 0xf2b00-0xf2b8d (assert string/line are the reference's own PUSH immediates
 * at 0xf2b1b/0xf2b20/0xf2b25). */
void get_active_player_profile_color_index(void *widget)
{
  unsigned char profile[0x30];
  short local_player_index;
  short color_index;

  local_player_index = *(short *)((char *)widget + 8);
  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(
      "profile color picture requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xa2d, 1);
    system_exit(-1);
  }

  player_ui_get_active_player_profile(local_player_index, profile);

  color_index = *(short *)(profile + 0x18);
  if (color_index < 0) {
    *(short *)((char *)widget + 0x50) = 0;
    return;
  }
  if ((int)color_index > (int)FUN_001c0ed0() - 1) {
    *(short *)((char *)widget + 0x50) = (short)(FUN_001c0ed0() - 1);
    return;
  }
  *(short *)((char *)widget + 0x50) = color_index;
}

/* FUN_000f2b90 (0xf2b90) — maps the active multiplayer map name to its
 * legacy game-settings text index. */
void FUN_000f2b90(void *widget)
{
  char *map_name;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xa39, 1);
    system_exit(-1);
  }

  map_name = (char *)network_game_get_game();
  if (map_name == NULL) {
    error(2, "no network game");
    return;
  }
  map_name = map_name + 0x24;

  if (crt_strstr(map_name, "beavercreek") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 0;
    return;
  }
  if (crt_strstr(map_name, "sidewinder") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 1;
    return;
  }
  if (crt_strstr(map_name, "damnation") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 2;
    return;
  }
  if (crt_strstr(map_name, "ratrace") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 3;
    return;
  }
  if (crt_strstr(map_name, "prisoner") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 4;
    return;
  }
  if (crt_strstr(map_name, "hangemhigh") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 5;
    return;
  }
  if (crt_strstr(map_name, "chillout") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 6;
    return;
  }
  if (crt_strstr(map_name, "carousel") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 7;
    return;
  }
  if (crt_strstr(map_name, "boardingaction") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 8;
    return;
  }
  if (crt_strstr(map_name, "bloodgulch") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 9;
    return;
  }
  if (crt_strstr(map_name, "wizard") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 10;
    return;
  }
  if (crt_strstr(map_name, "putput") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 0xb;
    return;
  }

  *(unsigned short *)((char *)widget + 0x40) =
    (unsigned short)(0xd - (crt_strstr(map_name, "longest") != NULL));
}

/* FUN_000f2e60 (0xf2e60)
 * "mp game settings text" data-driven text box widget update. Requires the
 * widget to be a text box (type == 1 at +0xe); otherwise asserts + exits
 * (reference PUSH immediates at 0xf2e6e/0xf2e70/0xf2e75/0xf2e7a). Fetches
 * the active network game object (network_game_get_game); if one exists, writes
 * a 2-state code to the widget's +0x40 word — 0xc when the game object's byte
 * at +0xc0 equals 1, else 0xd. If there is no active network game, reports
 * error(2, "no network game") instead (reference PUSH immediates at
 * 0xf2eaf/0xf2eb4). Evidence: reference disassembly at 0xf2e60-0xf2ec1. */
void FUN_000f2e60(void *widget)
{
  int game;
  unsigned char state_byte;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xaaa, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game != 0) {
    state_byte = *(unsigned char *)(game + 0xc0);
    *(unsigned short *)((char *)widget + 0x40) =
      (unsigned short)((state_byte != 1) + 0xc);
  } else {
    error(2, "no network game");
  }
}

/* multiplayer_game_set_text_box_for_score_limit (0xf2ed0)
 * "mp game settings text" numeric text box widget update for the network
 * game's score limit. Requires the widget to be a text box (type == 1 at
 * +0xe); otherwise asserts + exits (reference PUSH immediates at
 * 0xf2edf/0xf2ee1/0xf2ee6/0xf2eeb). Looks up the current network game via
 * network_game_get_game(); if none is active, reports error 2 "no network
 * game" (reference PUSH immediates at 0xf2f4d/0xf2f52) and leaves the text
 * buffer untouched. Otherwise reallocates the widget's text buffer (+0x3c)
 * to 0x10 bytes (8 wchar_t) and formats the game object's dword at +0xe4
 * into it with "%d" (format string at 0x26c118 = L"%d"), then explicitly
 * null-terminates at wchar index 7 (byte offset 0xe) by re-reading the
 * widget's +0x3c pointer. Evidence: reference disassembly at
 * 0xf2ed0-0xf2f5f (ui_widget_realloc call at 0xf2f0a-0xf2f1f;
 * unicode_sprintf call at 0xf2f29-0xf2f40; terminator store at
 * 0xf2f3d/0xf2f44). */
void multiplayer_game_set_text_box_for_score_limit(void *widget)
{
  int game;
  wchar_t *new_buf;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xac3, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game != 0) {
    new_buf = (wchar_t *)ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), 0x10,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xac8);
    *(wchar_t **)((char *)widget + 0x3c) = new_buf;
    if (new_buf != NULL) {
      unicode_sprintf(new_buf, 7, L"%d", *(int *)(game + 0xe4));
      *(unsigned short *)((char *)*(wchar_t **)((char *)widget + 0x3c) + 0xe) =
        0;
    }
  } else {
    error(2, "no network game");
  }
}

/* FUN_000f2f60 (0xf2f60)
 * "mp game settings text" data-driven text box widget update (game-type
 * variant). Requires the widget to be a text box (type == 1 at +0xe);
 * otherwise asserts + exits (reference PUSH immediates at
 * 0xf2f6e/0xf2f70/0xf2f75/0xf2f7a). Fetches the active network game object
 * (network_game_get_game); if one exists, dispatches on the game object's dword
 * field at +0xbc (jump table at 0xf2ff8, values 1-5) to write the widget's
 * +0x40 word: 1                                -> 0x16 2, or any value
 * outside 1..5 (the out-of-range default falls into the same code as case 2 --
 * reference 0xf2fa1 JA 0xf2fb3)  -> 0x18 3     -> 0x18 if the game object's
 * dword at +0x100 == 2, else 0x17 4     -> 0x17 5     -> 0x19 If there is no
 * active network game, reports error(2, "no network game") instead (reference
 * PUSH immediates at 0xf2fe6/0xf2feb). Evidence: reference disassembly at
 * 0xf2f60-0xf2ff8 plus jump table dwords at 0xf2ff8-0xf300c
 * (0xf2faa/0xf2fb3/0xf2fbc/0xf2fd4/0xf2fdd). */
void FUN_000f2f60(void *widget)
{
  int game;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xadc, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game != 0) {
    switch (*(int *)(game + 0xbc)) {
    case 1:
      *(unsigned short *)((char *)widget + 0x40) = 0x16;
      break;
    case 3:
      *(unsigned short *)((char *)widget + 0x40) =
        (*(int *)(game + 0x100) == 2) ? 0x18 : 0x17;
      break;
    case 4:
      *(unsigned short *)((char *)widget + 0x40) = 0x17;
      break;
    case 5:
      *(unsigned short *)((char *)widget + 0x40) = 0x19;
      break;
    case 2:
    default:
      *(unsigned short *)((char *)widget + 0x40) = 0x18;
      break;
    }
  } else {
    error(2, "no network game");
  }
}

/* FUN_000f3280 (0xf3280)
 * "mp game settings text" numeric text box widget update. Requires the
 * widget to be a text box (type == 1 at +0xe). Looks up the current network
 * game via network_game_get_game(); if none is active, reports error 2 "no
 * network game" and leaves the widget's text buffer untouched. Otherwise
 * reallocates the widget's text buffer (+0x3c) to 8 bytes (4 wchar_t) and
 * formats a signed 16-bit game field at game+0x224 into it with "%d",
 * explicitly null-terminating at wchar index 3 (byte offset 6) regardless of
 * how many digits were written. Evidence: disassembly at 0xf3280-0xf3311
 * (assert string/line are the reference's own PUSH immediates at
 * 0xf3291/0xf3296/0xf329b; ui_widget_realloc call at 0xf32bd-0xf32ca; error
 * call at 0xf32fe-0xf3305). */
void FUN_000f3280(void *widget)
{
  int game;
  wchar_t *new_buf;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xb4e, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game != 0) {
    new_buf = (wchar_t *)ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), 8,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xb56);
    *(wchar_t **)((char *)widget + 0x3c) = new_buf;
    if (new_buf != NULL) {
      unicode_sprintf(new_buf, 3, L"%d", *(short *)(game + 0x224));
      *(unsigned short *)((char *)*(wchar_t **)((char *)widget + 0x3c) + 6) = 0;
    }
  } else {
    error(2, "no network game");
  }
}

/* multiplayer_edit_profile_set_ruleset_textbox_string_index (0xf3320)
 * "mp profile edit ruleset text" text box widget update. Requires the widget
 * to be a text box (type == 1 at +0xe); otherwise asserts + exits (assert
 * string/file/line are the reference's own PUSH immediates at
 * 0xf3330/0xf3335/0xf333a). Fetches the playlist profile currently being
 * edited (player_ui_get_edit_playlist_profile at 0xf334e); if none is being
 * edited, reports error 2 "not currently editing a game variant" and leaves
 * the widget untouched. Otherwise maps the profile's ruleset field at +0x18
 * (values 1..5, via the DEC/CMP 4/JA jump table at 0xf3357-0xf3360) onto the
 * widget's +0x40 string index 3..7, with 8 for any other value. */
void multiplayer_edit_profile_set_ruleset_textbox_string_index(void *widget)
{
  int profile;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for mp profile edit ruleset text widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xb69, 1);
    system_exit(-1);
  }

  profile = (int)player_ui_get_edit_playlist_profile();
  if (profile != 0) {
    switch (*(int *)(profile + 0x18)) {
    case 1:
      *(unsigned short *)((char *)widget + 0x40) = 3;
      return;
    case 2:
      *(unsigned short *)((char *)widget + 0x40) = 4;
      return;
    case 3:
      *(unsigned short *)((char *)widget + 0x40) = 5;
      return;
    case 4:
      *(unsigned short *)((char *)widget + 0x40) = 6;
      return;
    case 5:
      *(unsigned short *)((char *)widget + 0x40) = 7;
      return;
    default:
      *(unsigned short *)((char *)widget + 0x40) = 8;
      return;
    }
  } else {
    error(2, "not currently editing a game variant");
  }
}

/* get_editable_player_profile_display_name (0xf3590, ui_widget_game_data_
 * function_table[39]). The name is kb.json's pre-existing placeholder and
 * does not match the observed behavior: the assert strings and the globals
 * this touches (DAT_0046ce3b/0046cd38/0046ce38) are the exact same
 * "difficulty forced for this map" state that
 * ui_widget_game_data_select_difficulty_item above reads, so this is a
 * paired difficulty-warning visibility updater, not a profile-name getter.
 * No source-derived name is available; kept as-is to avoid an unrequested
 * kb.json/table rename.
 *
 * Asserts widget+0x34 is a column list (+0xe == 3, "the difficulty list
 * widget"), then that list's sibling at +0x2c is a text box (+0xe == 1,
 * "expected warning text box"). If the "difficulty forced for this map"
 * flag (DAT_0046ce3b) is set and the current map (main_get_map_name())
 * case-insensitively matches the forced-map name (DAT_0046cd38), writes 1
 * into the warning textbox's +0x10 byte when the list's currently selected
 * index (+0x3c) differs from the forced difficulty index (DAT_0046ce38),
 * else 0. If the flag isn't set or the map doesn't match, unconditionally
 * clears +0x10 to 0. Evidence: reference disassembly at 0xf3590-0xf3630
 * (assert immediates at 0xf35a8/0xf35ad/0xf35b2/0xf35b7 and
 * 0xf35d6/0xf35db/0xf35e0/0xf35e5; crt_stricmp call at 0xf3608). */
void get_editable_player_profile_display_name(void *widget)
{
  void *list_widget;
  void *warning_widget;
  const char *map_name;

  list_widget = *(void **)((char *)widget + 0x34);
  if (list_widget == NULL || *(short *)((char *)list_widget + 0xe) != 3) {
    display_assert(
      "this doesn't look like the difficulty list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xc1f, 1);
    system_exit(-1);
  }

  warning_widget = *(void **)((char *)list_widget + 0x2c);
  if (warning_widget == NULL || *(short *)((char *)warning_widget + 0xe) != 1) {
    display_assert(
      "this doesn't look like the difficulty list widget (expected warning "
      "text box)",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xc23, 1);
    system_exit(-1);
  }

  if (*(unsigned char *)0x46ce3b == 1) {
    map_name = main_get_map_name();
    if (crt_stricmp((const char *)0x46cd38, map_name) == 0) {
      *(unsigned char *)((char *)warning_widget + 0x10) =
        (unsigned char)(*(short *)((char *)list_widget + 0x3c) !=
                        *(short *)0x46ce38);
      return;
    }
  }
  *(unsigned char *)((char *)warning_widget + 0x10) = 0;
}

/* get_editable_playlist_profile_display_name (0xf3640, ui_widget_game_data_
 * function_table). Like the sibling at 0xf3590, kb.json's name is a
 * pre-existing placeholder that does not match the observed behavior: there
 * is no profile lookup or name copy here at all. Kept as-is to avoid an
 * unrequested rename.
 *
 * Asserts the widget is a text box (+0xe == 1); otherwise halts (reference
 * PUSH immediates at 0xf364e/0xf3650/0xf3655/0xf365a: display_assert(
 * "expected a text box widget for system link menu text item", ..., 0xc3c,
 * 1); system_exit(-1)). Then unconditionally writes the widget's +0x24 float
 * field: 1.0f if transport_network_available(), else 0.333f (0x3eaa7efa) --
 * meaning of +0x24 is unconfirmed (no other function in this TU touches it).
 * Evidence: reference disassembly at 0xf3640-0xf368b. */
void get_editable_playlist_profile_display_name(void *widget)
{
  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for system link menu text item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xc3c, 1);
    system_exit(-1);
  }

  if (transport_network_available()) {
    *(float *)((char *)widget + 0x24) = 1.0f;
  } else {
    *(float *)((char *)widget + 0x24) = 0.333f;
  }
}

/* FUN_000f46e0 (0xf46e0)
 * "mp player settings select" quarter-screen profile list widget update.
 *
 * Validates the widget hierarchy (container wrapper + 1-wide DeLa spinner +
 * 3 child widgets), then resolves the currently selected profile id against the
 * global profile record table at 0x5aa3c0 (3 records, stride 0x34).  On a hit
 * it fills the widget's name buffer (widget+0x4c, 0x18 bytes) and the sibling
 * widget's description buffer (sibling+0x3c, 0x200 bytes).  On a miss the
 * item-id array is re-sorted / compacted and the routine restarts from the top
 * (the back edges at 0x48e6 / 0x48fc target 0x46f0, i.e. the whole body is a
 * retry loop).
 *
 * Profile record layout (evidence: the accesses below, stride 0x34):
 *   +0x00 int    profile id
 *   +0x04 wchar_t name[12]
 *   +0x1c short  controller-setup index (< 0 => 0)
 *   +0x1e ushort flags; bit0 = "use default name", bits 8..15 = ustr string
 * index +0x2c byte   button description index +0x2d byte   joystick description
 * index MSVC keeps the record pointer as &record.name (base+4), so the offsets
 * used below are 4 less than the record-relative ones above. */
void FUN_000f46e0(int *widget)
{
  short *list_tag;
  void *wrapper_tag;
  int child; /* [EBP-0x4] */
  int sibling; /* [EBP+0x8] — MSVC reuses the dead incoming param slot */
  int local_id; /* [EBP-0x8] */
  int item_id;
  int *item_ids;
  int *entry;
  int entry_index;
  wchar_t *rec_name;
  wchar_t *name_buf;
  wchar_t *desc_buf;
  wchar_t *src;
  unsigned short flags;
  int tag_index;
  int joystick_tag;
  int button_tag;
  int joystick_str;
  int button_str;
  int clamped;
  int count;
  int used;
  short list_index;

  while (1) {
    if (widget[0xc] == 0) {
      display_assert(
        "expected qtr-screen profile select list to be wrapped in a container "
        "widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x85e, 1);
      system_exit(-1);
    }
    list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *widget);
    if (*list_tag != 2) {
      display_assert(
        "expected a spinner list for 'mp player settings select' widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x864, 1);
      system_exit(-1);
    }
    if (*(int *)((char *)list_tag + 0x3e0) != 0) {
      display_assert(
        "expected 0 children (1-wide spinner) for 'mp player settings select' "
        "widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x865, 1);
      system_exit(-1);
    }
    wrapper_tag = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget[0xc]);
    if (*(int *)((char *)wrapper_tag + 0x3e0) != 3) {
      display_assert(
        "expected qtr-screen profile select wrapper screen to have 3 child "
        "widgets (pic, description, list... in that order)",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x867, 1);
      system_exit(-1);
    }
    child = *(int *)(widget[0xc] + 0x34);
    sibling = *(int *)(child + 0x2c);
    if (widget != *(int **)(sibling + 0x2c)) {
      display_assert(
        "expected qtr-screen profile select wrapper screen to have 3 child "
        "widgets (pic, description, list... in that order)",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x86d, 1);
      system_exit(-1);
    }
    if (*(short *)((char *)widget + 0x3c) < 0 ||
        (int)*(short *)((char *)widget + 0x3c) >=
          (int)*(unsigned short *)((char *)widget + 0x44)) {
      display_assert(
        "qtr-screen profile list has invalid list item index",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x86f, 1);
      system_exit(-1);
    }
    if (*(short *)((char *)widget + 0x3c) < 0 ||
        (int)*(short *)((char *)widget + 0x3c) >=
          (int)*(unsigned short *)((char *)widget + 0x44)) {
      display_assert(
        "invalid list item index",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x872, 1);
      system_exit(-1);
    }

    item_id = ((int *)widget[0x10])[*(short *)((char *)widget + 0x3c)];
    local_id = item_id;
    multiplayer_game_set_text_box_for_game_ruleset(&local_id, 1);

    if (item_id != -1) {
      entry_index = 0;
      entry = (int *)0x5aa3c0;
      do {
        if (*entry == item_id) {
          rec_name = (wchar_t *)(entry_index * 0x34 + 0x5aa3c4);
          if (rec_name != NULL) {
            name_buf =
              (wchar_t *)ui_widget_realloc(widget[0x13], 0x18,
                                           "c:\\halo\\SOURCE\\interface\\ui_"
                                           "widget_game_data_input_functions.c",
                                           0x886);
            widget[0x13] = (int)name_buf;
            if (name_buf == NULL) {
              return;
            }
            flags = *(unsigned short *)((char *)rec_name + 0x1a);
            if ((flags & 1) != 0) {
              tag_index =
                tag_loaded(0x75737472 /* 'ustr' */,
                           "ui\\shell\\strings\\default_player_profile_names");
              src = (wchar_t *)0x26cdf0; /* L"" */
              if (tag_index != -1) {
                src = (wchar_t *)FUN_0019d420(tag_index, flags >> 8);
              }
              ustrncpy((wchar_t *)widget[0x13], src, 0xb);
              *(short *)(widget[0x13] + 0x16) = 0;
            } else {
              ustrncpy((wchar_t *)widget[0x13], rec_name, 0xb);
              *(short *)(widget[0x13] + 0x16) = 0;
            }

            if (*(short *)((char *)rec_name + 0x18) < 0) {
              clamped = 0;
            } else {
              clamped = *(short *)((char *)rec_name + 0x18);
              if (clamped > (int)FUN_001c0ed0() - 1) {
                clamped = (int)FUN_001c0ed0() - 1;
              }
            }
            *(short *)(child + 0x50) = (short)clamped;

            desc_buf =
              (wchar_t *)ui_widget_realloc(*(int *)(sibling + 0x3c), 0x200,
                                           "c:\\halo\\SOURCE\\interface\\ui_"
                                           "widget_game_data_input_functions.c",
                                           0x89c);
            *(wchar_t **)(sibling + 0x3c) = desc_buf;
            if (desc_buf == NULL) {
              return;
            }

            if ((*(unsigned char *)((char *)rec_name + 0x1a) & 1) != 0) {
              joystick_tag =
                tag_loaded(0x75737472 /* 'ustr' */,
                           "ui\\shell\\main_menu\\player_profiles_"
                           "select\\joystick_set_defaults_descriptions");
              button_tag = tag_loaded(0x75737472 /* 'ustr' */,
                                      "ui\\shell\\main_menu\\player_profiles_"
                                      "select\\button_set_long_descriptions");
              if (joystick_tag == -1 || button_tag == -1) {
                **(short **)(sibling + 0x3c) = 0;
                *(short *)(*(int *)(sibling + 0x3c) + 0x1fe) = 0;
                return;
              }
              joystick_str = FUN_0019d420(
                joystick_tag,
                (unsigned short)*(unsigned char *)((char *)rec_name + 0x29));
              button_str = FUN_0019d420(
                button_tag,
                (unsigned short)*(unsigned char *)((char *)rec_name + 0x28));
              unicode_sprintf(*(wchar_t **)(sibling + 0x3c), 0xff, L"%s%hs%s",
                              (wchar_t *)joystick_str, "\r\n",
                              (wchar_t *)button_str);
              *(short *)(*(int *)(sibling + 0x3c) + 0x1fe) = 0;
            } else {
              joystick_tag =
                tag_loaded(0x75737472 /* 'ustr' */,
                           "ui\\shell\\main_menu\\player_profiles_"
                           "select\\joystick_set_short_descriptions");
              button_tag = tag_loaded(0x75737472 /* 'ustr' */,
                                      "ui\\shell\\main_menu\\player_profiles_"
                                      "select\\button_set_short_descriptions");
              if (joystick_tag != -1 && button_tag != -1) {
                joystick_str = FUN_0019d420(
                  joystick_tag,
                  (unsigned short)*(unsigned char *)((char *)rec_name + 0x29));
                button_str = FUN_0019d420(
                  button_tag,
                  (unsigned short)*(unsigned char *)((char *)rec_name + 0x28));
                unicode_sprintf(*(wchar_t **)(sibling + 0x3c), 0xff, L"%s%hs%s",
                                (wchar_t *)joystick_str, "\r\n",
                                (wchar_t *)button_str);
              }
            }
            *(short *)(*(int *)(sibling + 0x3c) + 0x1fe) = 0;
            return;
          }
          break;
        }
        entry = entry + 0xd; /* stride 0x34 bytes */
        entry_index = entry_index + 1;
      } while ((int)entry < 0x5aa45c);
    }

    if (*(unsigned short *)((char *)widget + 0x44) == 0) {
      name_buf = (wchar_t *)ui_widget_realloc(
        widget[0x13], 4,
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x8d2);
      widget[0x13] = (int)name_buf;
      if (name_buf != NULL) {
        *name_buf = 0;
      }
      *(short *)(child + 0x50) = 0;
      desc_buf = (wchar_t *)ui_widget_realloc(
        *(int *)(sibling + 0x3c), 4,
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x8d7);
      *(wchar_t **)(sibling + 0x3c) = desc_buf;
      if (desc_buf != NULL) {
        *desc_buf = 0;
      }
      return;
    }

    item_ids = (int *)widget[0x10];
    count = (int)*(unsigned short *)((char *)widget + 0x44);
    qsort(item_ids, count, 4, FUN_000f3960);
    for (used = 0; used < count; used++) {
      if (item_ids[used] == -1) {
        break;
      }
    }
    list_index = *(short *)((char *)widget + 0x3c);
    *(unsigned short *)((char *)widget + 0x44) = (unsigned short)used;
    if (list_index < 0) {
      *(short *)((char *)widget + 0x3c) = 0;
    } else {
      clamped = (int)(unsigned short)used - 1;
      if ((int)list_index <= clamped) {
        clamped = list_index;
      }
      *(short *)((char *)widget + 0x3c) = (short)clamped;
    }
  }
}


/* --- BATCH 5.1B: ui_widget_game_data_input_functions.obj recovery --- */

/* mp profile init koth rules (0x0ed240) */
bool ui_widget_multiplayer_profile_init_koth_rules(void *widget, void *event_data,
                                                   bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_playlist_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb43,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable game variant"); return false;
  }

  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'moving hill' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb4b,
      1);
    system_exit(-1);
  }

  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) {
      break;
    }
  }

  if (spinner == NULL) {
    display_assert(
      "expected 'moving hill' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb4d,
      1);
    system_exit(-1);
  }

  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x4c] != 0) ? 0 : 1;

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'score to win' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb56,
      1);
    system_exit(-1);
  }

  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) {
      break;
    }
  }

  if (spinner == NULL) {
    display_assert(
      "expected 'score to win' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb58,
      1);
    system_exit(-1);
  }

  switch (*(unsigned int *)(profile + 0x40)) {
  case 2:
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
    break;
  case 5:
    *(unsigned short *)((char *)spinner + 0x3c) = 2;
    break;
  case 10:
    *(unsigned short *)((char *)spinner + 0x3c) = 3;
    break;
  case 15:
    *(unsigned short *)((char *)spinner + 0x3c) = 4;
    break;
  default:
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
    break;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'teams' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb64,
      1);
    system_exit(-1);
  }

  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) {
      break;
    }
  }

  if (spinner == NULL) {
    display_assert(
      "expected 'teams' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb66,
      1);
    system_exit(-1);
  }

  *(unsigned short *)((char *)spinner + 0x3c) = (*(unsigned char *)(profile + 0x1c) == 0) ? 1 : 0;
  return true;
}

/* mp profile init slayer rules (0x0ed470) */
bool playlist_profile_initialize_slayer_rules(void *widget, void *event_data,
                                              bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_playlist_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb7f,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable game variant"); return false;
  }

  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'death bonus' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb87,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'death bonus' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb89,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x4c] == 1) ? 1 : 0;

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'kill in order' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb92,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'kill in order' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb94,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x4e] == 0) ? 1 : 0;

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'kill penalty' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb9d,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'kill penalty' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb9f,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x4d] == 1) ? 1 : 0;

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'kills to win' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xba8,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'kills to win' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbaa,
      1);
    system_exit(-1);
  }
  switch (*(unsigned int *)(profile + 0x40)) {
  case 10: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 15: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 25: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  case 50: *(unsigned short *)((char *)spinner + 0x3c) = 4; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'teams' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbb6,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'teams' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 3000,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (*(unsigned char *)(profile + 0x1c) == 0) ? 1 : 0;
  return true;
}

/* mp profile init oddball rules (0x0ed7c0) */
bool playlist_profile_initialize_oddball_rules(void *widget, void *event_data,
                                               bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;
  int val;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_playlist_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbd1,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable game variant"); return false;
  }

  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'trait with ball' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbd9,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'trait with ball' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbdb,
      1);
    system_exit(-1);
  }
  switch (*(unsigned int *)(profile + 0x54)) {
  case 1: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 2: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 3: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'trait without ball' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbe6,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'trait without ball' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbe8,
      1);
    system_exit(-1);
  }
  switch (*(unsigned int *)(profile + 0x58)) {
  case 1: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 2: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 3: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'speed with ball' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbf3,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'speed with ball' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbf5,
      1);
    system_exit(-1);
  }
  val = *(int *)(profile + 0x50);
  if (val == 0) {
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
  } else if (val == 2) {
    *(unsigned short *)((char *)spinner + 0x3c) = 2;
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'ball type' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbff,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'ball type' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc01,
      1);
    system_exit(-1);
  }
  val = *(int *)(profile + 0x5c);
  if (val == 1) {
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
  } else if (val == 2) {
    *(unsigned short *)((char *)spinner + 0x3c) = 2;
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'random start' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc0b,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'random start' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc0d,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x4c] != 0) ? 0 : 1;

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'ball spawn count' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc16,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'ball spawn count' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc18,
      1);
    system_exit(-1);
  }
  val = *(int *)(profile + 0x60);
  if (val >= 1 && val <= 16) {
    *(unsigned short *)((char *)spinner + 0x3c) = (unsigned short)(val - 1);
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'score to win' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc31,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'score to win' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc33,
      1);
    system_exit(-1);
  }
  switch (*(unsigned int *)(profile + 0x40)) {
  case 2: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 5: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 10: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  case 15: *(unsigned short *)((char *)spinner + 0x3c) = 4; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'teams' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc3f,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'teams' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc41,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (*(unsigned char *)(profile + 0x1c) == 0) ? 1 : 0;
  return true;
}

/* mp profile init racing rules (0x0edcd0) */
bool playlist_profile_initialize_racing_rules(void *widget, void *event_data,
                                              bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;
  int val;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_playlist_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc5a,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable game variant"); return false;
  }

  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'team scoring' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc62,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'team scoring' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc64,
      1);
    system_exit(-1);
  }
  val = *(int *)(profile + 0x50);
  if (val == 1) {
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
  } else if (val == 2) {
    *(unsigned short *)((char *)spinner + 0x3c) = 2;
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'race type' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc6e,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'race type' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc70,
      1);
    system_exit(-1);
  }
  val = *(int *)(profile + 0x4c);
  if (val == 1) {
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
  } else if (val == 2) {
    *(unsigned short *)((char *)spinner + 0x3c) = 2;
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'laps to win' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc7a,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'laps to win' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc7c,
      1);
    system_exit(-1);
  }
  switch (*(unsigned int *)(profile + 0x40)) {
  case 3: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 5: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 10: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  case 15: *(unsigned short *)((char *)spinner + 0x3c) = 4; break;
  case 25: *(unsigned short *)((char *)spinner + 0x3c) = 5; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'teams' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc89,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'teams' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc8b,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (*(unsigned char *)(profile + 0x1c) == 0) ? 1 : 0;
  return true;
}

/* mp profile init player opts (0x0edfb0) */
bool playlist_profile_initialize_player_options(void *widget, void *event_data,
                                                bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;
  int val;
  int health_opt;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_playlist_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xca5,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable game variant"); return false;
  }

  /* 1. number of lives */
  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'number of lives' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcad,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'number of lives' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcaf,
      1);
    system_exit(-1);
  }
  switch (*(unsigned int *)(profile + 0x38)) {
  case 1: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 3: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 5: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  /* 2. maximum health */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'maximum health' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcba,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'maximum health' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcbc,
      1);
    system_exit(-1);
  }
  health_opt = -(int)(*(float *)(profile + 0x3c) * -10.0f);
  switch (health_opt) {
  case 10: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 15: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 20: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  case 30: *(unsigned short *)((char *)spinner + 0x3c) = 4; break;
  case 40: *(unsigned short *)((char *)spinner + 0x3c) = 5; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  /* 3. shields */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'shields' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcc9,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'shields' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xccb,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = ((*(unsigned int *)(profile + 0x20) >> 3) & 1) ? 1 : 0;

  /* 4. respawn time */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'respawn time' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcd5,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'respawn time' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcd7,
      1);
    system_exit(-1);
  }
  val = *(int *)(profile + 0x30);
  if (val == 150) {
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
  } else if (val == 300) {
    *(unsigned short *)((char *)spinner + 0x3c) = 2;
  } else if (val == 450) {
    *(unsigned short *)((char *)spinner + 0x3c) = 3;
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  /* 5. respawn time growth */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'respawn time growth' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xce2,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'respawn time growth' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xce4,
      1);
    system_exit(-1);
  }
  val = *(int *)(profile + 0x2c);
  if (val == 150) {
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
  } else if (val == 300) {
    *(unsigned short *)((char *)spinner + 0x3c) = 2;
  } else if (val == 450) {
    *(unsigned short *)((char *)spinner + 0x3c) = 3;
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  /* 6. odd man out */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'odd man out' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcef,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'odd man out' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcf1,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x28] != 0) ? 0 : 1;

  /* 7. invisible players */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'invisible players' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcfa,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'invisible players' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xcfc,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = ((*(unsigned int *)(profile + 0x20) >> 4) & 1) ? 0 : 1;

  /* 8. suicide penalty */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'suicide penalty' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd07,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  val = *(int *)(profile + 0x34);
  if (val == 150) {
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
  } else if (val == 300) {
    *(unsigned short *)((char *)spinner + 0x3c) = 2;
  } else if (val == 450) {
    *(unsigned short *)((char *)spinner + 0x3c) = 3;
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  return true;
}

/* mp profile init item options (0x0ee500) */
bool playlist_profile_change_item_options(void *widget, void *event_data,
                                          bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;
  unsigned int v4;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_playlist_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd26,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable game variant"); return false;
  }

  /* 1. infinite grenades */
  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'infinite grenades' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd2e,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'infinite grenades' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd30,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = ((*(unsigned int *)(profile + 0x20) >> 2) & 1) ? 0 : 1;

  /* 2. vehicle set */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'vehicle set' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd3a,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'vehicle set' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd3c,
      1);
    system_exit(-1);
  }
  switch (*(unsigned int *)(profile + 0x48)) {
  case 1: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 2: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 3: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  case 4: *(unsigned short *)((char *)spinner + 0x3c) = 4; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  /* 3. weapon set */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'weapon set' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd48,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'weapon set' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd4a,
      1);
    system_exit(-1);
  }
  switch (*(unsigned int *)(profile + 0x44)) {
  case 1: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 2: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 3: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  case 4: *(unsigned short *)((char *)spinner + 0x3c) = 4; break;
  case 5: *(unsigned short *)((char *)spinner + 0x3c) = 5; break;
  case 6: *(unsigned short *)((char *)spinner + 0x3c) = 6; break;
  case 7: *(unsigned short *)((char *)spinner + 0x3c) = 7; break;
  case 8: *(unsigned short *)((char *)spinner + 0x3c) = 8; break;
  case 9: *(unsigned short *)((char *)spinner + 0x3c) = 9; break;
  case 10: *(unsigned short *)((char *)spinner + 0x3c) = 10; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  /* 4. starting equipment */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'starting equipment' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd5d,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'starting equpiment' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd5f,
      1);
    system_exit(-1);
  }
  v4 = (*(unsigned int *)(profile + 0x20) >> 5) & 1;
  if (v4 == 0) {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  } else if (v4 == 1) {
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  return true;
}

/* mp profile init indicator opts (0x0ee810) */
bool playlist_profile_change_indicator_options(void *widget, void *event_data,
                                               bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;
  int val;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_playlist_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd7a,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable game variant"); return false;
  }

  /* 1. radar display */
  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'radar display' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd82,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'radar display' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd84,
      1);
    system_exit(-1);
  }
  val = *(int *)(profile + 0x24);
  if (val == 1) {
    *(unsigned short *)((char *)spinner + 0x3c) = 1;
  } else if (val == 2) {
    *(unsigned short *)((char *)spinner + 0x3c) = 2;
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  /* 2. other players on radar */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'other players on radar' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd8e,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'other players on radar' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd90,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (*(unsigned int *)(profile + 0x20) & 1) ? 0 : 1;

  /* 3. friends on screen */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'friends on screen' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd9a,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'friends on screen' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd9c,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = ((*(unsigned int *)(profile + 0x20) >> 1) & 1) ? 0 : 1;

  return true;
}

/* color picker menu initialize (0x0eead0) */
bool ui_widget_color_picker_menu_initialize(void *widget, void *event_data,
                                            bool *widget_deleted)
{
  short *tag;
  unsigned short color_count;
  char *profile;
  void *alloc_result;
  int i;
  short selected;
  int clamped;

  (void)event_data;
  (void)widget_deleted;

  color_count = FUN_001c0ed0();
  profile = (char *)player_ui_get_edit_player_profile();
  tag = (short *)tag_get(0x44654c61, *(int *)widget);
  if (*tag != 2) {
    display_assert(
      "expected a spinner list widget for 'player color picker list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xdf8,
      1);
    system_exit(-1);
  }
  if (*(int *)(tag + 0x1f0) != 3) {
    display_assert(
      "expected 3 list items for 'player color picker list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xdf9,
      1);
    system_exit(-1);
  }

  alloc_result = ui_widget_realloc(*(int *)((char *)widget + 0x40),
                                   color_count,
                                   "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
                                   0xdfd);
  *(void **)((char *)widget + 0x40) = alloc_result;
  if (alloc_result != NULL) {
    for (i = 0; i < (int)color_count; i++) {
      *(char *)(alloc_result + i) = (char)i;
    }
    *(unsigned short *)((char *)widget + 0x44) = color_count;
  }

  if (profile != NULL) {
    selected = *(short *)(profile + 0x18);
    if (selected < 0) {
      *(short *)(profile + 0x18) = 0;
      *(short *)((char *)widget + 0x3c) = 0;
      return true;
    }
    clamped = (int)color_count - 1;
    if ((int)selected <= clamped) {
      clamped = selected;
    }
    *(short *)(profile + 0x18) = (short)clamped;
    *(short *)((char *)widget + 0x3c) = (short)clamped;
    return true;
  }

  error(2, "failed to find editing player profile"); return false;
}

/* player profile end editing (0x0eee40) */
bool player_profile_end_editing(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  *(int *)0x31e494 = -1;
  player_ui_end_editing_profile();
  return true;
}

/* player profile change name (0x0eee60) */
bool player_profile_change_name(void *widget, void *event_data, bool *widget_deleted)
{
  char *profile;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_player_profile();
  if (profile == NULL) {
    error(2, "failed to retrieve editable player profile"); return false;
  }
  if (!virtual_keyboard_set_validation((wchar_t *)profile, 0x18, 8)) {
    error(2, "failed to invoke virtual keyboard on player profile name"); return false;
  }
  return true;
}

/* plyr prf init cntl settings (0x0eef30) */
bool player_profile_initialize_controller_settings(void *widget, void *event_data,
                                                   bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_player_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for controller settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xed1,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable player profile"); return false;
  }

  /* 1. joystick config */
  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'joystick config' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xed9,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'joystick config' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xedb,
      1);
    system_exit(-1);
  }
  switch (profile[0x29]) {
  case 1: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 2: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 3: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  /* 2. button config */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'button config' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xee6,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'button config' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xee8,
      1);
    system_exit(-1);
  }
  switch (profile[0x28]) {
  case 1: *(unsigned short *)((char *)spinner + 0x3c) = 1; break;
  case 2: *(unsigned short *)((char *)spinner + 0x3c) = 2; break;
  case 3: *(unsigned short *)((char *)spinner + 0x3c) = 3; break;
  case 4: *(unsigned short *)((char *)spinner + 0x3c) = 4; break;
  default: *(unsigned short *)((char *)spinner + 0x3c) = 0; break;
  }

  return true;
}

/* plyr prf init adv cntl set (0x0ef110) */
bool player_profile_initialize_advanced_controller_settings(void *widget, void *event_data,
                                                            bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;
  unsigned char sens;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_player_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for advanced controller settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf04,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable player profile"); return false;
  }

  /* 1. invert joystick */
  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'invert joystick' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf0c,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'invert joystick' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf0e,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x2b] != 0) ? 0 : 1;

  /* 2. look sensitivity */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'look sensitivity' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf17,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'look sensitivity' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf19,
      1);
    system_exit(-1);
  }
  sens = (unsigned char)profile[0x2a];
  if (sens >= 1 && sens <= 10) {
    *(unsigned short *)((char *)spinner + 0x3c) = (unsigned short)(sens - 1);
  } else {
    *(unsigned short *)((char *)spinner + 0x3c) = 0;
  }

  /* 3. controller vibration */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'controller vibration' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf2c,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'controller vibration' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf2e,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x2c] == 1) ? 1 : 0;

  /* 4. flight stick controls */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'flight stick controls' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf37,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'flight stick controls' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf39,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x2d] != 0) ? 0 : 1;

  /* 5. autocenter */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'autocenter' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf42,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'autocenter' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf44,
      1);
    system_exit(-1);
  }
  *(unsigned short *)((char *)spinner + 0x3c) = (profile[0x2e] == 0) ? 1 : 0;

  return true;
}

/* plyr prf save cntl settings (0x0ef3f0) */
bool player_profile_change_controller_settings(void *widget, void *event_data,
                                               bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;
  unsigned short sel;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_player_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for controller settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf5d,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable player profile"); return false;
  }

  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'joystick config' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf65,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'joystick config' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf67,
      1);
    system_exit(-1);
  }
  sel = *(unsigned short *)((char *)spinner + 0x3c);
  switch (sel) {
  case 0: profile[0x29] = 0; break;
  case 1: profile[0x29] = 1; break;
  case 2: profile[0x29] = 2; break;
  case 3: profile[0x29] = 3; break;
  default:
    error(2, "unknown option selected for joystick config");
    break;
  }

  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'button config' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf72,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'button config' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf74,
      1);
    system_exit(-1);
  }
  sel = *(unsigned short *)((char *)spinner + 0x3c);
  if (sel <= 4) {
    profile[0x28] = (char)sel;
    return true;
  }

  error(2, "unknown button config option selected"); return false;
}

/* plyr prf save adv cntl set (0x0ef5c0) */
bool ui_widget_player_profile_save_advanced_controller_settings(void *widget, void *event_data,
                                                                bool *widget_deleted)
{
  char *profile;
  void *child;
  void *spinner;
  short sel;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_player_profile();
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for advanced controller settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf90,
      1);
    system_exit(-1);
  }

  if (profile == NULL) {
    error(2, "failed to retrieve editable player profile"); return false;
  }

  /* 1. invert joystick */
  child = *(void **)((char *)widget + 0x34);
  if (child == NULL) {
    display_assert(
      "expected 'invert joystick' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf98,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'invert joystick' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf9a,
      1);
    system_exit(-1);
  }
  sel = *(short *)((char *)spinner + 0x3c);
  if (sel == 0) {
    profile[0x2b] = 1;
  } else if (sel == 1) {
    profile[0x2b] = 0;
  } else {
    error(2, "unknown option selected for invert joystick");
  }

  /* 2. look sensitivity */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'look sensitivity' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfa3,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'look sensitivity' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfa5,
      1);
    system_exit(-1);
  }
  sel = *(short *)((char *)spinner + 0x3c);
  if (sel >= 0 && sel <= 9) {
    profile[0x2a] = (char)(sel + 1);
  } else {
    error(2, "unknown option selected for look sensitivity");
  }

  /* 3. controller vibration */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'controller vibration' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfb8,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'controller vibration' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfba,
      1);
    system_exit(-1);
  }
  sel = *(short *)((char *)spinner + 0x3c);
  if (sel == 0) {
    profile[0x2c] = 0;
  } else if (sel == 1) {
    profile[0x2c] = 1;
  } else {
    error(2, "unknown option selected for controller vibration");
  }

  /* 4. flight stick controls */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'flight stick controls' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfc3,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'flight stick controls' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfc5,
      1);
    system_exit(-1);
  }
  sel = *(short *)((char *)spinner + 0x3c);
  if (sel == 0) {
    profile[0x2d] = 1;
  } else if (sel == 1) {
    profile[0x2d] = 0;
  } else {
    error(2, "unknown option selected for controller flight_stick_aircraft_controls");
  }

  /* 5. autocenter */
  child = *(void **)((char *)child + 0x2c);
  if (child == NULL) {
    display_assert(
      "expected 'autocenter' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfce,
      1);
    system_exit(-1);
  }
  for (spinner = *(void **)((char *)child + 0x34); spinner != NULL;
       spinner = *(void **)((char *)spinner + 0x2c)) {
    if (*(short *)((char *)spinner + 0xe) == 2) break;
  }
  if (spinner == NULL) {
    display_assert(
      "expected 'autocenter' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfd0,
      1);
    system_exit(-1);
  }
  sel = *(short *)((char *)spinner + 0x3c);
  if (sel == 0) {
    profile[0x2e] = 1;
    return true;
  } else if (sel == 1) {
    profile[0x2e] = 0;
    return true;
  }

  error(2, "unknown option selected for controller autocenter"); return false;
}

/* main menu switch to solo game (0x0ef950) */
bool switch_from_main_menu_to_single_player(void *widget, void *event_data,
                                            bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  set_game_connection(0);
  main_menu_switch_to_single_player();
  player_ui_remember_player1_profile(false);
  return true;
}

/* request del playlist profile (0x0efa80) */
bool delete_playlist_profile_request(void *widget, void *event_data,
                                     bool *widget_deleted)
{
  short *tag;
  void *list_child;
  short list_index;
  int profile_handle;

  (void)event_data;
  (void)widget_deleted;

  tag = (short *)tag_get(0x44654c61, *(int *)widget);
  if (*tag != 0 || *(int *)(tag + 0x1f0) <= 2) {
    display_assert(
      "expected the playlist profile select screen to be a container w/ 3+ children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x103d,
      1);
    system_exit(-1);
  }

  list_child = *(void **)((char *)widget + 0x34);
  tag = (short *)tag_get(0x44654c61, *(int *)list_child);
  if (*tag != 2) {
    display_assert(
      "expected a spinner list widget for 'playlist profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x1040,
      1);
    system_exit(-1);
  }
  if (*(int *)(tag + 0x1f0) != 3) {
    display_assert(
      "expected 3 list items for 'playlist profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x1041,
      1);
    system_exit(-1);
  }

  list_index = *(short *)((char *)list_child + 0x3c);
  if (list_index < 0 || (int)*(unsigned short *)((char *)list_child + 0x44) <= (int)list_index) {
    display_assert(
      "invalid multiplayer profile specified from 'multiplayer profile list' list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x1049,
      1);
    system_exit(-1);
  }

  profile_handle = *(int *)(*(int *)((char *)list_child + 0x40) + list_index * 4);
  *(int *)0x31e494 = profile_handle;
  if (profile_handle != -1) {
    if (profile_handle & 0x40000000) {
      display_error_deferred(0x1a, -1, 1, 0);
      ui_play_audio_feedback_sound(4);
      return false;
    }
    return true;
  }

  ui_play_audio_feedback_sound(4);
  return false;
}

/* final del player profile (0x0efbc0) */
bool delete_player_profile_final(void *widget, void *event_data,
                                 bool *widget_deleted)
{
  int handle;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  handle = *(int *)0x31e494;
  if (handle & 0x40000000) {
    error(2, "sorry, you are not allowed to delete default player profiles"); return false;
  }
  if (!(handle & 0xf)) {
    FUN_001c0d70(handle);
    return true;
  }
  error(2, "#0x%08lX is not a player profile index", handle); return false;
}

/* final del playlist profile (0x0efc10) */
bool delete_playlist_profile_final(void *widget, void *event_data,
                                   bool *widget_deleted)
{
  int handle;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  handle = *(int *)0x31e494;
  if ((handle & 0xf) == 1) {
    FUN_001c1f70(handle);
    return true;
  }
  error(2, "#0x%08lX is not a playlist profile index", handle);
  return false;
}

/* cancel profile delete (0x0efc50) */
bool cancel_profile_delete(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  *(int *)0x31e494 = -1;
  return true;
}

/* create&edit playlist profile (0x0efc60) */
bool create_and_begin_editing_new_gametype_profile(void *widget, void *event_data,
                                                   bool *widget_deleted)
{
  wchar_t name[128];
  char default_variant[104];
  unsigned int copied_settings[26];
  char buffer[256];
  int profile_index;
  char *profile;
  bool vk_result;
  bool persist_result;
  int i;

  (void)widget_deleted;

  if (*(short *)((char *)event_data + 2) < 0 || *(short *)((char *)event_data + 2) >= 4) {
    display_assert(
      "creating a new profile requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x10b0,
      1);
    system_exit(-1);
  }

  saved_game_file_get_useable_untitled_profile_name(name);
  if (name[0] != 0) {
    profile_index = playlist_profile_new(*(unsigned short *)((char *)widget + 8), name);
    if (profile_index != -1) {
      player_ui_begin_editing_profile(profile_index);
      profile = (char *)player_ui_get_edit_playlist_profile();
      if (profile != NULL) {
        game_engine_slayer_default((game_variant_t *)default_variant);
        for (i = 0; i < 26; i++) {
          copied_settings[i] = ((unsigned int *)default_variant)[i];
        }
        csmemcpy(profile, copied_settings, 0x68);
        *(unsigned short *)(profile + 100) = 0;
        ustrncpy((wchar_t *)profile, name, 0xb);
        *(unsigned short *)(profile + 0x16) = 0;
        vk_result = virtual_keyboard_set_validation((wchar_t *)profile, 0x18, 9);
        if (vk_result) {
          persist_result = saved_game_file_get_path_to_enclosing_directory(profile_index, buffer);
          if (persist_result) {
            saved_game_file_remember_last_used_multiplayer_variant_directory(buffer);
          }
          return persist_result;
        }
        return vk_result;
      } else {
        error(2, "failed to retrieve editable game variant profile!");
        player_ui_end_editing_profile();
      }
    } else {
      error(2, "failed to create a new multiplayer game type profile");
    }
  } else {
    error(2, "unable to create a new untitled profile");
  }

  display_error_deferred(0x26, -1, 1, 0);
  ui_play_audio_feedback_sound(4);
  return false;
}

/* net server accept conx (0x0f0010) */
bool network_game_server_accept_connections(void *widget, void *event_data,
                                            bool *widget_deleted)
{
  void *server;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  server = network_game_server_get();
  if (server != NULL) {
    network_game_server_open_game(server);
  }
  return true;
}

/* net server defer start (0x0f0030) */
bool network_game_server_defer_game_start(void *widget, void *event_data,
                                          bool *widget_deleted)
{
  void *server;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  server = network_game_server_get();
  if (server != NULL) {
    network_game_server_pause_countdown(server, 1);
  }
  return true;
}

/* net server allow start (0x0f0050) */
bool network_game_server_allow_game_start(void *widget, void *event_data,
                                          bool *widget_deleted)
{
  void *server;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  server = network_game_server_get();
  if (server != NULL) {
    network_game_server_pause_countdown(server, 0);
  }
  return true;
}

/* run xdemos (0x0f0090) */
bool run_xdemos(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  main_run_demos();
  return true;
}

/* sp reset controller choices (0x0f00a0) */
bool single_player_reset_controller_choices(void *widget, void *event_data,
                                            bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  player_ui_reset_single_player_local_player_controllers();
  return true;
}

/* exit to xbox dashboard (0x0f0420) */
bool exit_to_xbox_dashboard(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  FUN_000e0570();
  return false;
}

/* begin music fade out (0x0f0720) */
bool begin_music_fade_out(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  if (ui_widget_get_attract_mode_flag()) {
    ui_widget_stop_attract_mode();
  }
  return true;
}

/* initialize sp level list solo (0x0f0790) */
bool solo_level_initialize_list_single_player(void *widget, void *event_data,
                                              bool *widget_deleted)
{
  short *tag;
  char profile_name[28];
  char completion_flags[20];
  char unknown_buf[4];
  short counts[2];
  int profile_id;
  int i;
  int clamped;

  if (*(int16_t *)0x31fa94 >= 2) {
    csmemset((void *)0x46cd38, 0, 0x106);
    return ui_widget_initialize_single_player_level_list(widget, event_data, widget_deleted);
  }

  profile_id = player_ui_get_active_player_profile_index(0);
  csmemset((void *)0x46cce8, 0, 0x50);
  if (profile_id != *(int *)0x31e4c0) {
    csmemset((void *)0x46cd38, 0, 0x106);
    *(char *)0x46ce3b = (char)game_state_test_persistent_storage((char *)0x46cd38, (int16_t *)0x46ce38, *(int *)0x46ce3c);
    *(int *)0x31e4c0 = profile_id;
  }

  player_ui_get_active_player_profile(0, profile_name);
  player_profile_save_last_level_played(profile_name, counts, (short *)unknown_buf);

  for (i = 0; i < 10; i++) {
    *(unsigned int *)((char *)0x46cce8 + i * 8) = *(unsigned int *)((char *)0x31e498 + i * 4);
    if (completion_flags[i] != 0 || i == counts[0] + 1 || i == 0) {
      *(char *)((char *)0x46cce8 + i * 8 + 4) = 1;
      *(char *)((char *)0x46cce8 + i * 8 + 5) = (completion_flags[i] >> 1) & 1;
      *(char *)((char *)0x46cce8 + i * 8 + 6) = (completion_flags[i] >> 2) & 1;
      *(char *)((char *)0x46cce8 + i * 8 + 7) = (completion_flags[i] >> 3) & 1;
    }
  }

  tag = (short *)tag_get(0x44654c61, *(int *)widget);
  if (*tag != 2) {
    display_assert(
      "expected a spinner list widget for 'solo level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x25b,
      1);
    system_exit(-1);
  }
  if (*(int *)(tag + 0x1f0) != 3) {
    display_assert(
      "expected 3 list items for 'solo level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x25c,
      1);
    system_exit(-1);
  }

  *(int *)((char *)widget + 0x40) = 0x46cce8;
  *(unsigned short *)((char *)widget + 0x44) = 10;

  clamped = (int)player_ui_get_last_single_player_level_played(0);
  if (clamped < 0) {
    clamped = 0;
  } else if (clamped >= 10) {
    clamped = 9;
  }
  *(short *)((char *)widget + 0x3c) = (short)clamped;

  if (*(char *)0x46ce3b != 1) {
    if (*(char *)0x46ce3c == 1) {
      profile_id = player_ui_get_active_player_profile_index(0);
      if (profile_id != -1) {
        if (*(int *)0x31e4c4 == -1) {
          *(int *)0x31e4c4 = profile_id;
          display_error_deferred(0x27, -1, 1, 0);
          return true;
        }
        *(int *)0x31e4c4 = -1;
      }
    }
  } else {
    *(char *)0x46ce37 = 0;
    for (i = 0; i < 10; i++) {
      if (!crt_stricmp((const char *)0x46cd38, *(const char **)((char *)0x31e498 + i * 4))) {
        *(char *)0x46ce3a = (char)i;
        if ((short)*(short *)0x46ce38 >= 0) {
          if ((short)*(short *)0x46ce38 > 3) {
            *(short *)0x46ce38 = 3;
          }
        } else {
          *(short *)0x46ce38 = 0;
        }
        break;
      }
    }
    if (i == 10) {
      *(char *)0x46ce3b = 0;
    }
  }

  return true;
}

/* widget function null (0x0f0a90) */
void widget_function_null(void *widget)
{
  (void)widget;
}

/* FUN_000f0f30 (0x0f0f30) */
void FUN_000f0f30(void *widget)
{
  (void)widget;
  csmemset((void *)0x46ce40, 0, 0x24);
  /* Server list item update */
}

/* FUN_000f1710 (0x0f1710) */
void FUN_000f1710(void *widget)
{
  (void)widget;
  if (*(int *)(tag_get(0x44654c61, *(int *)widget) + 0x3e0) != 6) {
    display_assert(
      "this doesn't look like the net pregame status screen to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x3fc,
      1);
    system_exit(-1);
  }
}

/* network_pregame_status_screen_update (0x0f1ed0) */
void network_pregame_status_screen_update(void *widget)
{
  (void)widget;
  if (*(int *)(tag_get(0x44654c61, *(int *)widget) + 0x3e0) != 6) {
    display_assert(
      "this doesn't look like the net pregame status screen to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x592,
      1);
    system_exit(-1);
  }
}

/* mutliplayer_settings_select_list_update_displayed_items (0x0f24b0) */
void mutliplayer_settings_select_list_update_displayed_items(void *widget)
{
  void *child;
  void *target;
  short idx;

  if (widget == NULL || *(void **)((char *)widget + 0x38) == NULL ||
      *(void **)((char *)widget + 0x48) == NULL) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x836,
      1);
    system_exit(-1);
  }

  idx = 0;
  for (child = *(void **)((char *)widget + 0x34); child != NULL;
       child = *(void **)((char *)child + 0x2c)) {
    if (child == *(void **)((char *)widget + 0x38)) break;
    idx++;
  }

  target = *(void **)((char *)widget + 0x48);
  if (*(int *)(tag_get(0x44654c61, *(int *)target) + 0x3e0) != 2) {
    display_assert(
      "expected a container widget w/ 2 children for the player profile edit settings list extended description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x844,
      1);
    system_exit(-1);
  }

  target = *(void **)((char *)target + 0x34);
  *(short *)((char *)target + 0x40) = idx;
  *(short *)(*(char **)((char *)target + 0x2c) + 0x50) = idx;
}

/* FUN_000f2990 (0x0f2990) */
void FUN_000f2990(void *widget)
{
  char *profile;
  void *alloc_result;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for profile display name",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x9fe,
      1);
    system_exit(-1);
  }

  if (*(short *)((char *)widget + 8) < 0 || *(short *)((char *)widget + 8) >= 4) {
    display_assert(
      "profile display name requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xa00,
      1);
    system_exit(-1);
  }

  profile = (char *)player_ui_get_edit_player_profile();
  if (profile == NULL) return;

  alloc_result = ui_widget_realloc(*(int *)((char *)widget + 0x3c),
                                   0x18,
                                   "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
                                   0xa04);
  *(void **)((char *)widget + 0x3c) = alloc_result;
  if (alloc_result == NULL) return;

  ustrncpy((wchar_t *)alloc_result, (wchar_t *)(void *)profile, 0xb);
  *(unsigned short *)(alloc_result + 0x16) = 0;
}

/* FUN_000f2a40 (0x0f2a40) */
void FUN_000f2a40(void *widget)
{
  char *profile;
  void *alloc_result;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for profile display name",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xa13,
      1);
    system_exit(-1);
  }

  if (*(short *)((char *)widget + 8) < 0 || *(short *)((char *)widget + 8) >= 4) {
    display_assert(
      "profile display name requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xa15,
      1);
    system_exit(-1);
  }

  profile = (char *)player_ui_get_edit_playlist_profile();
  if (profile != NULL) {
    alloc_result = ui_widget_realloc(*(int *)((char *)widget + 0x3c),
                                     0x18,
                                     "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
                                     0xa19);
    *(void **)((char *)widget + 0x3c) = alloc_result;
    if (alloc_result == NULL) return;

    ustrncpy((wchar_t *)alloc_result, (wchar_t *)(void *)profile, 0xb);
    *(unsigned short *)(alloc_result + 0x16) = 0;
    return;
  }

  error(2, "not currently editing a game variant");
}

/* FUN_000f2d50 (0x0f2d50) */
void FUN_000f2d50(void *widget)
{
  char *netgame;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xa59,
      1);
    system_exit(-1);
  }

  netgame = (char *)network_game_server_get();
  if (netgame == NULL) {
    error(2, "no network game");
    return;
  }

  switch (*(unsigned int *)(netgame + 0xbc)) {
  case 1:
    if (netgame[0xf0] != 1) {
      *(unsigned short *)((char *)widget + 0x40) = (unsigned short)((-(*(int *)(netgame + 0xf4) != 0) & 0x1b) + 3);
    } else {
      *(unsigned short *)((char *)widget + 0x40) = (unsigned short)(0x1d - (*(int *)(netgame + 0xf4) != 0));
    }
    break;
  case 2:
    *(unsigned short *)((char *)widget + 0x40) = 4;
    break;
  case 3:
    if (*(int *)(netgame + 0x100) == 1) {
      *(unsigned short *)((char *)widget + 0x40) = 0x1f;
    } else if (*(int *)(netgame + 0x100) == 2) {
      *(unsigned short *)((char *)widget + 0x40) = 0x20;
    } else {
      *(unsigned short *)((char *)widget + 0x40) = 5;
    }
    break;
  case 4:
    *(unsigned short *)((char *)widget + 0x40) = 6;
    break;
  case 5:
    if (*(int *)(netgame + 0xf0) == 2) {
      *(unsigned short *)((char *)widget + 0x40) = 0x21;
    } else {
      *(unsigned short *)((char *)widget + 0x40) = 7;
    }
    break;
  default:
    *(unsigned short *)((char *)widget + 0x40) = 8;
    break;
  }
}

/* solo_game_objective_text (0x0f3010) */
void solo_game_objective_text(void *widget)
{
  char *netgame;
  const char *map;

  if (*(short *)((char *)widget + 0xe) != 0) {
    display_assert(
      "expected container widget for mp game settings bitmap",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xb09,
      1);
    system_exit(-1);
  }

  netgame = (char *)network_game_server_get();
  if (netgame == NULL) {
    error(2, "no network game");
    return;
  }

  map = netgame + 0x24;
  if (crt_strstr(map, "beavercreek") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 0;
  } else if (crt_strstr(map, "sidewinder") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 1;
  } else if (crt_strstr(map, "damnation") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 2;
  } else if (crt_strstr(map, "ratrace") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 3;
  } else if (crt_strstr(map, "prisoner") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 4;
  } else if (crt_strstr(map, "hangemhigh") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 5;
  } else if (crt_strstr(map, "chillout") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 6;
  } else if (crt_strstr(map, "carousel") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 7;
  } else if (crt_strstr(map, "boardingaction") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 8;
  } else if (crt_strstr(map, "bloodgulch") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 9;
  } else if (crt_strstr(map, "wizard") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 10;
  } else if (crt_strstr(map, "putput") != NULL) {
    *(unsigned short *)((char *)widget + 0x50) = 11;
  } else {
    *(unsigned short *)((char *)widget + 0x50) = (unsigned short)(13 - (crt_strstr(map, "longest") != NULL ? 1 : 0));
  }
}

/* color_picker_get_string (0x0f31d0) */
void color_picker_get_string(void *widget)
{
  char *netgame;

  if (*(short *)((char *)widget + 0xe) != 0) {
    display_assert(
      "expected container widget for mp game settings bitmap",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xb28,
      1);
    system_exit(-1);
  }

  netgame = (char *)network_game_server_get();
  if (netgame == NULL) {
    error(2, "no network game");
    return;
  }

  switch (*(unsigned int *)(netgame + 0xbc)) {
  case 1: *(unsigned short *)((char *)widget + 0x50) = 0; break;
  case 2: *(unsigned short *)((char *)widget + 0x50) = 2; break;
  case 3: *(unsigned short *)((char *)widget + 0x50) = 3; break;
  case 4: *(unsigned short *)((char *)widget + 0x50) = 1; break;
  case 5: *(unsigned short *)((char *)widget + 0x50) = 4; break;
  default: *(unsigned short *)((char *)widget + 0x50) = 5; break;
  }
}

/* system_link_status_check (0x0f33d0) */
void system_link_status_check(void *widget)
{
  (void)widget;
  if (!transport_network_available()) {
    if (!network_game_is_splitscreen_local()) {
      display_error_when_main_menu_loaded(6);
      main_goto_main_menu();
      error(2, "network connection went down!");
    }
  }
}

/* game_options_menu_update_pic_desc (0x0f3400) */
void game_options_menu_update_pic_desc(void *widget)
{
  char *netgame;
  void *server;
    int i;
  char *entry;
  int red_count;
  int blue_count;

  netgame = (char *)network_game_server_get();
  server = network_game_server_get();

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for team game directions",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xbad,
      1);
    system_exit(-1);
  }

  if (server != NULL) {
    if (!network_game_is_splitscreen_local() && netgame != NULL && *(short *)(netgame + 0x112) < 2) {
      *(unsigned short *)((char *)widget + 0x40) = 0x22;
      *(char *)((char *)widget + 0x10) = 1;
      return;
    }
    if (network_game_is_splitscreen_local()) {
      if (netgame == NULL) {
        *(char *)((char *)widget + 0x10) = 0;
        return;
      }
      if (*(short *)(netgame + 0x224) <= 1) {
        *(unsigned short *)((char *)widget + 0x40) = 0x23;
        *(char *)((char *)widget + 0x10) = 1;
        return;
      }
    }
  }

  if (netgame != NULL && netgame[0xc0] == 1) {
    if ((short)FUN_00124D00(network_game_client_get()) <= -1) {
      red_count = 0;
      blue_count = 0;
      entry = netgame + 0x244;
      for (i = 0; i < 16; i++) {
        if (network_player_is_valid(entry - 0x1e)) {
          if (*entry == 0) {
            red_count++;
          } else if (*entry == 1) {
            blue_count++;
          }
        }
        entry += 0x20;
      }
      if (red_count > 0 && blue_count > 0) {
        *(unsigned short *)((char *)widget + 0x40) = 0x1a;
        *(char *)((char *)widget + 0x10) = 1;
        return;
      }
      *(unsigned short *)((char *)widget + 0x40) = 0x1b;
      *(char *)((char *)widget + 0x10) = 1;
      return;
    }
  }

  *(char *)((char *)widget + 0x10) = 0;
}

/* mp_level_select_list_update_displayed_items (0x0f3540) */
void mp_level_select_list_update_displayed_items(void *widget)
{
  char *netgame;

  netgame = (char *)network_game_server_get();
  if (*(short *)((char *)widget + 0xe) != 0) {
    display_assert(
      "expected a container bitmap for mp pregame header widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xbfc,
      1);
    system_exit(-1);
  }

  if (netgame == NULL) return;
  *(unsigned short *)((char *)widget + 0x50) = (netgame[0xc0] != 1) ? 1 : 0;
}

/* FUN_000f3690 (0x0f3690) */
void FUN_000f3690(int *out_indices, void *widget)
{
  short list_index;
  int count;

  count = (int)*(unsigned short *)((char *)widget + 0x44);
  list_index = *(short *)((char *)widget + 0x3c);

  if (*(void **)((char *)widget + 0x38) != *(void **)((char *)widget + 0x34)) {
    if (*(void **)((char *)widget + 0x38) != *(void **)(*(char **)((char *)widget + 0x34) + 0x2c)) {
      out_indices[2] = list_index;
      out_indices[1] = list_index - 1;
      if (out_indices[1] < 0) {
        out_indices[1] = count - 1;
      }
      out_indices[0] = out_indices[1] - 1;
      if (out_indices[0] < 0) {
        out_indices[0] = count - 1;
      }
      goto label_clamp;
    }
    out_indices[1] = list_index;
    out_indices[0] = list_index - 1;
    if (out_indices[0] < 0) {
      out_indices[0] = count - 1;
    }
  } else {
    out_indices[0] = list_index;
    out_indices[1] = list_index + 1;
    if (out_indices[1] == count) {
      out_indices[1] = 0;
    }
  }

  out_indices[2] = out_indices[1] + 1;
  if (out_indices[2] == count) {
    out_indices[2] = 0;
  }

label_clamp:
  if (count <= out_indices[0]) out_indices[0] = -1;
  if (count <= out_indices[1]) out_indices[1] = -1;
  if (count <= out_indices[2]) out_indices[2] = -1;
}

/* multiplayer_game_set_text_box_for_game_ruleset (0x0f3740) */
void multiplayer_game_set_text_box_for_game_ruleset(int *ids, int count)
{
  int i;
  int j;
  char cached[4];
  int *entry;
  int id;

  csmemset(cached, 0, sizeof(cached));
  entry = (int *)0x5aa3c0;
  for (i = 0; i < 3; i++) {
    if (*entry != -1) {
      for (j = 0; j < count; j++) {
        if (*entry == ids[j]) {
          cached[i] = 1;
          break;
        }
      }
    }
    entry += 13;
  }

  for (i = 0; i < count; i++) {
    id = ids[i];
    if (id != -1) {
      entry = (int *)0x5aa3c0;
      for (j = 0; j < 3; j++) {
        if (id == *entry) break;
        entry += 13;
      }
      if (j == 3) {
        for (j = 0; j < 3; j++) {
          if (cached[j] != 1) break;
        }
        if (j >= 3) {
          display_assert(
            "not enough cache profiles",
            "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xca2,
            1);
          system_exit(-1);
        }
        if (player_profile_new(id, (void *)(0x5aa3c4 + j * 0x34))) {
          *(int *)(0x5aa3c0 + j * 0x34) = id;
          cached[j] = 1;
        } else {
          error(2, "failed to cache player profile");
        }
      }
    }
  }
}

/* multiplayer_game_set_text_box_for_teams_noteams (0x0f3850) */
void multiplayer_game_set_text_box_for_teams_noteams(int *ids, int count)
{
  int i;
  int j;
  char cached[4];
  int *entry;
  int id;

  csmemset(cached, 0, sizeof(cached));
  entry = (int *)0x5aa260;
  for (i = 0; i < 3; i++) {
    if (*entry != -1) {
      for (j = 0; j < count; j++) {
        if (*entry == ids[j]) {
          cached[i] = 1;
          break;
        }
      }
    }
    entry += 27;
  }

  for (i = 0; i < count; i++) {
    id = ids[i];
    if (id != -1) {
      entry = (int *)0x5aa260;
      for (j = 0; j < 3; j++) {
        if (id == *entry) break;
        entry += 27;
      }
      if (j == 3) {
        for (j = 0; j < 3; j++) {
          if (cached[j] != 1) break;
        }
        if (j >= 3) {
          display_assert(
            "not enough cache profiles",
            "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0xcd5,
            1);
          system_exit(-1);
        }
        if (playlist_profile_get(id, (void *)(0x5aa264 + j * 0x6c))) {
          *(int *)(0x5aa260 + j * 0x6c) = id;
          cached[j] = 1;
        } else {
          error(2, "failed to cache playlist profile");
        }
      }
    }
  }
}

/* FUN_000f3960 (0x0f3960) */
int __cdecl FUN_000f3960(const void *a, const void *b)
{
  const int *ia = (const int *)a;
  const int *ib = (const int *)b;

  if (*ia != -1) {
    if (*ib != -1) return 0;
    return -1;
  }
  if (*ib == -1) return 0;
  return 1;
}

/* filter_invalid_list_indices (0x0f3990) */
void filter_invalid_list_indices(void)
{
}

/* solo_level_select_list_update_displayed_items (0x0f39c0) */
void solo_level_select_list_update_displayed_items(void *widget)
{
  short *tag;
  int out_indices[3];
  void *item;
  void *name_tb;
  void *pic_cnt;
  void *diff_cnt;
  void *child;
  int i;
  int index;

  tag = (short *)tag_get(0x44654c61, *(int *)widget);
  if (*tag != 2) {
    display_assert(
      "expected a spinner list for 'solo level select' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x618,
      1);
    system_exit(-1);
  }
  if (*(int *)(tag + 0x1f0) != 3) {
    display_assert(
      "expected 3 children (list items) for 'solo level select' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x619,
      1);
    system_exit(-1);
  }

  FUN_000f3690(out_indices, widget);

  for (i = 0; i < 3; i++) {
    index = out_indices[i];
    item = widget_instance_get_nth_child(widget, i);
    name_tb = *(void **)((char *)item + 0x34);
    pic_cnt = *(void **)((char *)name_tb + 0x2c);
    diff_cnt = *(void **)((char *)pic_cnt + 0x2c);

    if (index != -1) {
      *(unsigned short *)((char *)name_tb + 0x40) = (unsigned short)index;
      *(unsigned short *)((char *)pic_cnt + 0x50) = (unsigned short)index;
      *(unsigned short *)((char *)diff_cnt + 0x40) = (unsigned short)index;
      if (*(char *)0x46ce3a == 1 && index == *(char *)0x46ce3a) {
        *(unsigned short *)((char *)diff_cnt + 0x40) = 11;
      }
      child = *(void **)((char *)diff_cnt + 0x34);
      if (child != NULL) {
        *(char *)((char *)child + 0x10) = *(char *)(0x46cced + index * 8);
        child = *(void **)((char *)child + 0x2c);
        if (child != NULL) {
          *(char *)((char *)child + 0x10) = *(char *)(0x46ccee + index * 8);
          child = *(void **)((char *)child + 0x2c);
          if (child != NULL) {
            *(char *)((char *)child + 0x10) = *(char *)(0x46ccef + index * 8);
          }
        }
      }
    } else {
      *(unsigned short *)((char *)name_tb + 0x40) = 10;
      *(unsigned short *)((char *)pic_cnt + 0x50) = 10;
      *(unsigned short *)((char *)diff_cnt + 0x40) = 10;
      child = *(void **)((char *)diff_cnt + 0x34);
      if (child != NULL) {
        *(char *)((char *)child + 0x10) = 0;
        child = *(void **)((char *)child + 0x2c);
        if (child != NULL) {
          *(char *)((char *)child + 0x10) = 0;
          child = *(void **)((char *)child + 0x2c);
          if (child != NULL) {
            *(char *)((char *)child + 0x10) = 0;
          }
        }
      }
    }
  }
}

/* FUN_000f3c80 (0x0f3c80) */
void FUN_000f3c80(void *widget)
{
  short *tag;
  int string_tag;
  int out_indices[3];
  int ids[3];
  int i;
  int id;
  void *item;
  void *name_tb;
  void *pic_cnt;
  void *desc_tb;
  char *cached_profile;
  void *alloc_result;
  wchar_t *src_str;
  int desc_tag;

  string_tag = tag_loaded(0x75737472, "ui\\shell\\strings\\game_variant_descriptions");
  tag = (short *)tag_get(0x44654c61, *(int *)widget);
  if (*tag != 2) {
    display_assert(
      "expected a spinner list for 'multiplayer settings select' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x67d,
      1);
    system_exit(-1);
  }
  if (*(int *)(tag + 0x1f0) != 3) {
    display_assert(
      "expected 3 children (list items) for 'multiplayer settings select' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x67e,
      1);
    system_exit(-1);
  }

  FUN_000f3690(out_indices, widget);
  for (i = 0; i < 3; i++) {
    if (out_indices[i] != -1) {
      ids[i] = *(int *)(*(int *)((char *)widget + 0x40) + out_indices[i] * 4);
    } else {
      ids[i] = -1;
    }
  }

  multiplayer_game_set_text_box_for_teams_noteams(ids, 3);

  for (i = 0; i < 3; i++) {
    if (out_indices[i] == -1) return;

    item = widget_instance_get_nth_child(widget, i);
    name_tb = *(void **)((char *)item + 0x34);
    pic_cnt = *(void **)((char *)name_tb + 0x2c);
    desc_tb = *(void **)((char *)pic_cnt + 0x2c);

    id = ids[i];
    cached_profile = NULL;
    if (id != -1) {
      int j;
      int *entry = (int *)0x5aa260;
      for (j = 0; j < 3; j++) {
        if (*entry == id) {
          cached_profile = (char *)(0x5aa264 + j * 0x6c);
          break;
        }
        entry += 27;
      }
    }

    if (cached_profile != NULL) {
      alloc_result = ui_widget_realloc(*(int *)((char *)name_tb + 0x3c),
                                       0x100,
                                       "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
                                       0x6ba);
      *(void **)((char *)name_tb + 0x3c) = alloc_result;
      if (alloc_result != NULL) {
        ustrncpy((wchar_t *)alloc_result, (wchar_t *)(void *)cached_profile, 0x7f);
        *(unsigned short *)(alloc_result + 0xfe) = 0;
      }

      *(unsigned short *)((char *)pic_cnt + 0x50) = 5;
      alloc_result = ui_widget_realloc(*(int *)((char *)desc_tb + 0x3c),
                                       0x200,
                                       "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
                                       0x6c1);
      *(void **)((char *)desc_tb + 0x3c) = alloc_result;
      if (alloc_result != NULL) {
        *(unsigned short *)alloc_result = 0;
      }

      if (*(unsigned short *)(cached_profile + 100) & 1) {
        switch (*(unsigned int *)(cached_profile + 0x18)) {
        case 1: *(unsigned short *)((char *)pic_cnt + 0x50) = 0; break;
        case 2: *(unsigned short *)((char *)pic_cnt + 0x50) = 2; break;
        case 3: *(unsigned short *)((char *)pic_cnt + 0x50) = 3; break;
        case 4: *(unsigned short *)((char *)pic_cnt + 0x50) = 1; break;
        case 5: *(unsigned short *)((char *)pic_cnt + 0x50) = 4; break;
        }
        if (string_tag != -1 && *(int *)((char *)desc_tb + 0x3c) != 0) {
          src_str = (wchar_t *)(wchar_t *)FUN_0019d420(string_tag, (*(unsigned short *)(cached_profile + 100) >> 8) + 10);
          ustrncpy(*(wchar_t **)((char *)desc_tb + 0x3c), src_str, 0xff);
          *(unsigned short *)(*(int *)((char *)desc_tb + 0x3c) + 0x1fe) = 0;
        }
      } else {
        switch (*(unsigned int *)(cached_profile + 0x18)) {
        case 1:
          *(unsigned short *)((char *)pic_cnt + 0x50) = 0;
          if (string_tag != -1 && *(int *)((char *)desc_tb + 0x3c) != 0) {
            src_str = (wchar_t *)(wchar_t *)FUN_0019d420(string_tag, (cached_profile[0x1c] != 1) ? 0 : 1);
            ustrncpy(*(wchar_t **)((char *)desc_tb + 0x3c), src_str, 0xff);
            *(unsigned short *)(*(int *)((char *)desc_tb + 0x3c) + 0x1fe) = 0;
          }
          break;
        case 2:
          *(unsigned short *)((char *)pic_cnt + 0x50) = 2;
          if (string_tag != -1 && *(int *)((char *)desc_tb + 0x3c) != 0) {
            src_str = (wchar_t *)(wchar_t *)FUN_0019d420(string_tag, (cached_profile[0x1c] != 1) ? 2 : 3);
            ustrncpy(*(wchar_t **)((char *)desc_tb + 0x3c), src_str, 0xff);
            *(unsigned short *)(*(int *)((char *)desc_tb + 0x3c) + 0x1fe) = 0;
          }
          break;
        case 3:
          *(unsigned short *)((char *)pic_cnt + 0x50) = 3;
          if (string_tag != -1 && *(int *)((char *)desc_tb + 0x3c) != 0) {
            src_str = (wchar_t *)(wchar_t *)FUN_0019d420(string_tag, (cached_profile[0x1c] != 1) ? 4 : 5);
            ustrncpy(*(wchar_t **)((char *)desc_tb + 0x3c), src_str, 0xff);
            *(unsigned short *)(*(int *)((char *)desc_tb + 0x3c) + 0x1fe) = 0;
          }
          break;
        case 4:
          *(unsigned short *)((char *)pic_cnt + 0x50) = 1;
          if (string_tag != -1 && *(int *)((char *)desc_tb + 0x3c) != 0) {
            src_str = (wchar_t *)(wchar_t *)FUN_0019d420(string_tag, (cached_profile[0x1c] != 1) ? 6 : 7);
            ustrncpy(*(wchar_t **)((char *)desc_tb + 0x3c), src_str, 0xff);
            *(unsigned short *)(*(int *)((char *)desc_tb + 0x3c) + 0x1fe) = 0;
          }
          break;
        case 5:
          *(unsigned short *)((char *)pic_cnt + 0x50) = 4;
          if (string_tag != -1 && *(int *)((char *)desc_tb + 0x3c) != 0) {
            src_str = (wchar_t *)(wchar_t *)FUN_0019d420(string_tag, (cached_profile[0x1c] != 1) ? 8 : 9);
            ustrncpy(*(wchar_t **)((char *)desc_tb + 0x3c), src_str, 0xff);
            *(unsigned short *)(*(int *)((char *)desc_tb + 0x3c) + 0x1fe) = 0;
          }
          break;
        }
      }
    } else {
      alloc_result = ui_widget_realloc(*(int *)((char *)name_tb + 0x3c),
                                       0x100,
                                       "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
                                       0x751);
      *(void **)((char *)name_tb + 0x3c) = alloc_result;
      if (alloc_result != NULL) {
        *(unsigned short *)alloc_result = 0;
      }
      *(unsigned short *)((char *)pic_cnt + 0x50) = 5;
      alloc_result = ui_widget_realloc(*(int *)((char *)desc_tb + 0x3c),
                                       0x200,
                                       "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
                                       0x756);
      *(void **)((char *)desc_tb + 0x3c) = alloc_result;
      if (alloc_result != NULL) {
        desc_tag = tag_loaded(0x75737472, "ui\\shell\\main_menu\\player_profiles_select\\profile_description_labels");
        *(unsigned short *)alloc_result = 0;
        if (desc_tag != -1) {
          src_str = (wchar_t *)(wchar_t *)FUN_0019d420(desc_tag, 5);
          ustrncpy((wchar_t *)alloc_result, src_str, 0xff);
          *(unsigned short *)((char *)alloc_result + 0x1fe) = 0;
        }
      }
    }
  }
}
