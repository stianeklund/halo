/* 0x1249b0 — network_game_server_dispose.
 * Tears down the network game client connection. If the server pointer is
 * non-null, closes its connection handle and clears the in-use flag. */
void network_game_server_dispose(void *server)
{
  if (server != NULL) {
    if (*(int *)((char *)server + 0x82c) != 0)
      network_connection_delete(*(int *)((char *)server + 0x82c));
    if (*(char *)0x46e8b9 == '\0') {
      display_assert("network_game_client_dont_use_directly_in_use",
                     "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                     0xb2, 1);
      system_exit(-1);
    }
    *(char *)0x46e8b9 = '\0';
  }
  network_game_log("network client disposed");
}

/* 0x124a30 — Returns the connection state (int16_t at offset 0xca6) and
 * optionally writes elapsed-time percentage into out_param. The time
 * calculation divides (current_ms - stored_ms) * 100 by 120000. */
int16_t FUN_00124a30(void *server, void *out_param)
{
  unsigned int diff;

  assert_halt(server);
  if (out_param != NULL) {
    *(short *)out_param = 0;
    if (*(short *)((char *)server + 0xca6) == 1) {
      diff = system_milliseconds() * 100 -
             *(unsigned int *)((char *)server + 0x834) * 100;
      *(short *)out_param = (short)(diff / 120000);
    }
  }
  return *(int16_t *)((char *)server + 0xca6);
}

/* FUN_00124c40 (0x124c40)
 *
 * Asserts client is non-null and returns the client's 16-bit value at +0.
 */
uint16_t FUN_00124c40(void *client)
{
  uint16_t *client_words;

  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x1fd, true);
    system_exit(-1);
  }

  client_words = (uint16_t *)client;
  return client_words[0];
}

/* 0x124cc0 — Asserts client is non-null and returns the int16_t field at
 * offset 0xca8. */
int16_t FUN_00124cc0(void *server)
{
  assert_halt(server);
  return *(int16_t *)((char *)server + 0xca8);
}

/* 0x124d40 — Thin wrapper that tail-calls FUN_00128e00 with the same five
 * arguments. The prologue sets up a frame (PUSH EBP / MOV EBP,ESP) and
 * immediately tears it down (POP EBP / JMP 0x128e00), so every argument
 * passes through to the callee unchanged. In the one observed call site
 * (network_game_client_end_frame), the caller resolves a server handle to a
 * connection pointer via network_game_client_get_seconds_to_game_start, then
 * calls this wrapper with the resulting connection pointer, a message buffer,
 * its size, a dest_address, and reliable=0. */
bool FUN_00124d40(void *connection, void *message, unsigned short size,
                  int dest_address, int reliable)
{
  return FUN_00128e00(connection, message, size, dest_address, reliable);
}

/* 0x125710 — Asserts client is non-null and returns the connection handle
 * (int) stored at offset 0x82c in the client structure. The returned handle
 * is used by the caller (network_game_client_end_frame) as the first argument
 * to FUN_00124d40 (which forwards it to FUN_00128e00 to send a network
 * message). */
int network_game_client_get_seconds_to_game_start(void *client)
{
  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x4b3, true);
    system_exit(-1);
  }
  return *(int *)((char *)client + 0x82c);
}

/* 0x125750 — Asserts client is non-null, then calls FUN_001283c0 with the
 * connection handle at offset 0x82c, the output buffer, and flag 0. */
void network_game_client_switch_to_postgame(void *server, void *out)
{
  assert_halt(server);
  FUN_001283c0(*(int *)((char *)server + 0x82c), out, 0);
}

/* network_game_client_get_machine_index (0x1257a0)
 *
 * Asserts client is non-null and returns client + 0x85c.
 */
void *network_game_client_get_machine_index(void *client)
{
  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x4cd, true);
    system_exit(-1);
  }

  return (void *)((uint8_t *)client + 0x85c);
}

/* 0x1257e0 — Asserts client is non-null and returns whether the int field at
 * offset 0xc98 is non-zero. */
bool network_game_client_get_available_games(void *server)
{
  assert_halt(server);
  return *(int *)((char *)server + 0xc98) != 0;
}

/* 0x125820 — Asserts client is non-null and returns the uint32_t field at
 * offset 0xc98 (the raw value that network_game_client_get_available_games
 * tests for non-zero). */
uint32_t network_game_client_get_error(void *server)
{
  assert_halt(server);
  return *(uint32_t *)((char *)server + 0xc98);
}

/* 0x125860 — Asserts client is non-null and returns the byte field at
 * offset 0xcac. */
bool FUN_00125860(void *server)
{
  assert_halt(server);
  return *(char *)((char *)server + 0xcac);
}

/* FUN_00126000 (0x126000) — network_game_client_send_graceful_exit_pregame
 *
 * Periodically (every 1000ms) encodes and sends a
 * message_client_graceful_game_exit_pregame (type 0x13) containing the
 * multiplayer map name to the server connection. */
void FUN_00126000(void *server)
{
  int now;
  char *map_name;
  char buf[256];
  unsigned short *encoded;
  unsigned short size;

  now = system_milliseconds();
  if (*(int *)((char *)server + 0xca0) + 1000 < now) {
    map_name = main_get_multiplayer_map_name();
    *(int *)((char *)server + 0xca0) = now;
    if (FUN_001b9de0(map_name)) {
      memset(buf, 0, sizeof(buf));
      csstrncpy(buf, map_name, 0x100);
      encoded = (unsigned short *)encode_network_game_message(0x13, buf, 0x100);
      if (encoded != NULL) {
        size = *encoded >> 4;
        if (!FUN_00128e00(*(int *)((char *)server + 0x82c), encoded, size, 0,
                          1)) {
          network_game_log("network_game_client_write() failed while sending a "
                           "message_client_graceful_game_exit_pregame message");
        }
      }
    }
  }
}

/* FUN_001260c0 (0x1260c0) — network_game_client_process_incoming_messages
 *
 * Drains all pending messages from the server connection. Loops calling
 * FUN_001298f0 to receive each message, then FUN_00127ea0 to handle it.
 * Returns true if all messages were processed successfully, false if any
 * handler fails. */
bool FUN_001260c0(void *server)
{
  bool result;
  char local_820[2048];
  char local_20[24];
  int local_8;

  result = true;
  do {
    local_8 = 0x800;
    if (!FUN_001298f0(*(int *)((char *)server + 0x82c), local_820, &local_8,
                      local_20))
      return result;
    result = FUN_00127ea0(server, local_820, local_8, local_20);
    if (!result)
      network_game_log("network_game_client_handle_message() failed in "
                       "network_game_client_process_incoming_messages()");
  } while (result);
  return result;
}

/* FUN_00126b60 (0x126b60) — network_game_client_idle_joining
 *
 * Called from the client idle dispatch (FUN_00127070) when state == 1
 * (joining). Verifies network connectivity, sends a join request once,
 * and checks for 120s timeout on the connect-process. Returns false
 * if connection drops, join request fails, or connection times out. */
bool FUN_00126b60(void *server)
{
  bool connected;
  unsigned char join_payload[0x50];
  unsigned short *encoded;
  int now_ms;
  int connect_handle;

  connected = true;
  if (!FUN_0012a170()) {
    connected = FUN_00082300();
    if (!connected) {
      error(2, "network connection went down!");
      display_error_when_main_menu_loaded(6);
    }
  }
  if (connected != true)
    return connected;

  if (FUN_00128360(*(int *)((char *)server + 0x82c))) {
    if ((*(unsigned char *)((char *)server + 0xcaa) & 2) == 0) {
      csmemset(join_payload, 0, 0x50);
      FUN_0012aaf0(join_payload);
      csmemcpy(&join_payload[0x40], (char *)server + 0x84a, 0x10);
      encoded = (unsigned short *)FUN_0012b700(0xc, join_payload, 0x50);
      if (encoded == NULL) {
        network_game_log(
          "failed to create a message_client_join_game_request message");
      } else if (FUN_00128e00(*(int *)((char *)server + 0x82c), encoded,
                              (unsigned short)(*encoded >> 4), 0, 1)) {
        *(unsigned char *)((char *)server + 0xcaa) =
          *(unsigned char *)((char *)server + 0xcaa) | 2;
      } else {
        network_game_log("network_game_client_write() failed to send a "
                         "message_client_join_game_request message");
      }
    }
    *(int *)((char *)server + 0x830) = 0;
  } else {
    connect_handle = *(int *)((char *)server + 0x830);
    if (connect_handle != 0) {
      now_ms = (int)system_milliseconds();
      if ((unsigned int)(now_ms - *(int *)((char *)server + 0x834)) > 120000) {
        network_game_log(
          "client connection process has timed out; aborting connection "
          "attempt");
        FUN_00084300(*(int *)((char *)server + 0x830));
        *(int *)((char *)server + 0x830) = 0;
        return false;
      }
    }
  }

  connected = FUN_00129cf0(*(int *)((char *)server + 0x82c), 5000, 0);
  if (!connected) {
    network_game_log("network_connection_idle() failed in "
                     "network_game_client_idle_joining()");
    return false;
  }
  connected = FUN_001260c0(server);
  if (!connected) {
    network_game_log(
      "network_game_client_process_incoming_messages() failed in "
      "network_game_client_idle_joining()");
    return false;
  }
  return connected;
}

/* FUN_00126ce0 (0x126ce0) — network_game_client_idle_pregame
 *
 * Called from the client idle dispatch (FUN_00127070) when state == 2
 * (pregame). Checks network connectivity, processes the connection, and handles
 * incoming messages. Returns false if the connection drops or processing fails.
 */
bool FUN_00126ce0(void *server)
{
  bool result;

  result = true;
  if (FUN_0012a170())
    goto check_result;
  result = FUN_00082300();
  if (result)
    goto main_body;
  error(2, "network connection went down!");
  display_error_when_main_menu_loaded(6);

check_result:
  if (!result)
    goto tail_check;

main_body:
  if (!FUN_00128660(*(int *)((char *)server + 0x82c)))
    goto fail;
  if (!network_connection_connected(*(int *)((char *)server + 0x82c)))
    goto fail;
  FUN_00126000(server);
  result = FUN_00129cf0(*(int *)((char *)server + 0x82c), 15000, 0);
  if (!result) {
    network_game_log("network_connection_idle() failed in "
                     "network_game_client_idle_pregame()");
    goto tail_check;
  }
  result = FUN_001260c0(server);
  if (result)
    return result;
  network_game_log("network_game_client_process_incoming_messages() failed in "
                   "network_game_client_idle_pregame()");
  goto tail_check;

fail:
  result = false;

tail_check:
  if (!FUN_00128660(*(int *)((char *)server + 0x82c))) {
    display_error_when_main_menu_loaded(4);
    return false;
  }
  return result;
}

/* FUN_00126db0 (0x126db0) — network_game_client_idle_ingame
 *
 * Called from the client idle dispatch (FUN_00127070) when state == 3 (ingame).
 * Verifies the server connection is alive, checks if the connection has gone
 * silent (bit 5 of connection+0x30 via FUN_001286a0), displays per-player
 * error widgets if newly silent, records the silent flag at server+0xcad, then
 * runs the connection idle tick (15-second timeout) and processes incoming
 * messages. Returns false if the connection drops or any critical step fails.
 */
bool FUN_00126db0(void *server)
{
  int connection;
  bool result;
  bool is_silent;
  __int16 player_idx;

  result = true;
  connection = *(int *)((char *)server + 0x82c);
  if (!FUN_00128660(connection))
    goto abort;
  if (!network_connection_connected(connection))
    goto abort;

  if (!FUN_0012a170()) {
    is_silent = FUN_001286a0(connection);
    if (!FUN_00082300()) {
      error(2, "network connection went down (idle in game)!");
      display_error_when_main_menu_loaded(6);
      network_game_log("network connection went down (idle in game)!");
      result = false;
      goto write_flag;
    }
    if (is_silent && !*(char *)((char *)server + 0xcad)) {
      player_idx = local_player_get_next(-1);
      while (player_idx != (__int16)-1) {
        ui_widget_display_error(9, player_idx, 0, 0);
        player_idx = local_player_get_next(player_idx);
      }
      network_game_log(
        "network client connection has been silent for a dangerously long"
        " amount of time");
    }
  write_flag:
    *(char *)((char *)server + 0xcad) = (char)is_silent;
    if (!result)
      return result;
  }

  connection = *(int *)((char *)server + 0x82c);
  result = FUN_00129cf0(connection, 15000, 0);
  if (!result) {
    connection = *(int *)((char *)server + 0x82c);
    if (!FUN_00128660(connection) ||
        !network_connection_connected(connection)) {
      error(2, "new2 idle in game abort hit");
      display_error_when_main_menu_loaded(4);
      result = false;
    }
    network_game_log(
      "network_connection_idle() failed in network_game_client_idle_ingame()");
    return result;
  }
  result = FUN_001260c0(server);
  if (!result)
    network_game_log("network_game_client_process_incoming_messages() failed in"
                     " network_game_client_idle_ingame()");
  return result;

abort:
  error(2, "new idle in game abort hit");
  display_error_when_main_menu_loaded(4);
  return false;
}

/* network_game_client_idle (0x126f40) — network_game_client_idle_postgame
 *
 * Called from the client idle dispatch (FUN_00127070) when state == 4
 * (postgame). Checks network connectivity, runs the connection idle with a
 * 15-second timeout, and processes incoming messages. Returns false if the
 * connection drops or processing fails. */
bool network_game_client_idle(void *server)
{
  bool result;

  result = true;
  if (FUN_0012a170())
    goto check_result;
  result = FUN_00082300();
  if (result)
    goto main_body;
  error(2, "network connection went down!");
  display_error_when_main_menu_loaded(6);

check_result:
  if (!result)
    goto tail_check;

main_body:
  result = FUN_00129cf0(*(int *)((char *)server + 0x82c), 15000, 0);
  if (!result) {
    network_game_log("network_connection_idle() failed in "
                     "network_game_client_idle_postgame()");
    goto tail_check;
  }
  result = FUN_001260c0(server);
  if (result)
    return result;
  network_game_log("network_game_client_process_incoming_messages() failed in "
                   "network_game_client_idle_postgame()");

tail_check:
  if (!FUN_00128660(*(int *)((char *)server + 0x82c))) {
    display_error_when_main_menu_loaded(4);
    return false;
  }
  return result;
}

/* 0x127070 — Network client idle dispatch: asserts client non-null, switches
 * on the connection state at offset 0xca6, and calls the appropriate
 * state-specific idle handler. Logs and returns false on handler failure. */
bool FUN_00127070(void *server)
{
  bool result;

  result = 0;
  assert_halt(server);
  switch (*(unsigned short *)((char *)server + 0xca6)) {
  case 0:
    result = FUN_001268a0(server);
    if (!result) {
      network_game_log("network_game_client_idle_searching() failed");
      return 0;
    }
    break;
  case 1:
    result = FUN_00126b60(server);
    if (!result) {
      network_game_log("network_game_client_idle_joining() failed");
      return 0;
    }
    break;
  case 2:
    result = FUN_00126ce0(server);
    if (!result) {
      network_game_log("network_game_client_idle_pregame() failed");
      return 0;
    }
    break;
  case 3:
    result = FUN_00126db0(server);
    if (!result) {
      network_game_log("network_game_client_idle_ingame() failed");
      return 0;
    }
    break;
  case 4:
    result = network_game_client_idle(server);
    if (!result) {
      network_game_log("network_game_client_idle_postgame() failed");
      return 0;
    }
    break;
  default:
    assert_halt(!"unknown client state");
  }
  return result;
}
