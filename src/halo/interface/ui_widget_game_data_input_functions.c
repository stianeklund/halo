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

  if (*(short *)((char *)widget + 0xe) == 0) {
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

  controller_index = *(short *)((char *)event_data + 2);
  if (controller_index == -1) {
    controller_index = 0;
  }

  saved_game_file_get_useable_untitled_profile_name(untitled_name);
  if (untitled_name[0] == L'\0') {
    error(2, "unable to create a new untitled profile");
    display_error_deferred(0x25, -1, true, false);
    ui_play_audio_feedback_sound(4);
    return false;
  }

  profile_index = FUN_001c1720(controller_index, untitled_name);
  if (profile_index == -1) {
    error(2, "failed to create a new player profile");
    display_error_deferred(0x25, -1, true, false);
    ui_play_audio_feedback_sound(4);
    return false;
  }

  player_ui_begin_editing_profile(profile_index);
  edit_name = player_ui_get_edit_player_profile();
  if (edit_name == NULL) {
    error(2, "failed to retrieve editable player profile!");
    player_ui_end_editing_profile();
    display_error_deferred(0x25, -1, true, false);
    ui_play_audio_feedback_sound(4);
    return false;
  }

  ustrncpy((wchar_t *)edit_name, untitled_name, 0xb);
  ((wchar_t *)edit_name)[0xb] = L'\0';
  validated = virtual_keyboard_set_validation((wchar_t *)edit_name, 0x18, 8);
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

/* FUN_000f2e60 (0xf2e60)
 * "mp game settings text" data-driven text box widget update. Requires the
 * widget to be a text box (type == 1 at +0xe); otherwise asserts + exits
 * (reference PUSH immediates at 0xf2e6e/0xf2e70/0xf2e75/0xf2e7a). Fetches
 * the active network game object (network_game_get_game); if one exists, writes a
 * 2-state code to the widget's +0x40 word — 0xc when the game object's byte
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

/* FUN_000f2f60 (0xf2f60)
 * "mp game settings text" data-driven text box widget update (game-type
 * variant). Requires the widget to be a text box (type == 1 at +0xe);
 * otherwise asserts + exits (reference PUSH immediates at
 * 0xf2f6e/0xf2f70/0xf2f75/0xf2f7a). Fetches the active network game object
 * (network_game_get_game); if one exists, dispatches on the game object's dword field
 * at +0xbc (jump table at 0xf2ff8, values 1-5) to write the widget's +0x40
 * word:
 *   1                                -> 0x16
 *   2, or any value outside 1..5 (the out-of-range default falls into the
 *     same code as case 2 -- reference 0xf2fa1 JA 0xf2fb3)  -> 0x18
 *   3     -> 0x18 if the game object's dword at +0x100 == 2, else 0x17
 *   4     -> 0x17
 *   5     -> 0x19
 * If there is no active network game, reports error(2, "no network game")
 * instead (reference PUSH immediates at 0xf2fe6/0xf2feb). Evidence:
 * reference disassembly at 0xf2f60-0xf2ff8 plus jump table dwords at
 * 0xf2ff8-0xf300c (0xf2faa/0xf2fb3/0xf2fbc/0xf2fd4/0xf2fdd). */
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
 * game via network_game_get_game(); if none is active, reports error 2 "no network
 * game" and leaves the widget's text buffer untouched. Otherwise reallocates
 * the widget's text buffer (+0x3c) to 8 bytes (4 wchar_t) and formats a
 * signed 16-bit game field at game+0x224 into it with "%d", explicitly
 * null-terminating at wchar index 3 (byte offset 6) regardless of how many
 * digits were written. Evidence: disassembly at 0xf3280-0xf3311 (assert
 * string/line are the reference's own PUSH immediates at
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
