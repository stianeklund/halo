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
int16_t network_game_client_get_state(void *server, void *out_param)
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

/* network_game_client_get_machine (0x124c10)
 *
 * Returns a pointer to the machine record selected by the client's 16-bit
 * index at offset 0. The machine array is embedded in the client structure at
 * offset 0x970 with a stride of 0x44 and an unsigned bound of 4 entries.
 * Returns NULL for a null client or an out-of-range index.
 */
void *network_game_client_get_machine(void *client)
{
  unsigned short machine_index;

  if (client != NULL) {
    machine_index = *(unsigned short *)client;
    if (machine_index < 4)
      return (void *)((uint8_t *)client + 0x970 + machine_index * 0x44);
  }
  return NULL;
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

/* 0x124d40 — Thin wrapper that tail-calls network_connection_write with the
 * same five arguments. The prologue sets up a frame (PUSH EBP / MOV EBP,ESP)
 * and immediately tears it down (POP EBP / JMP 0x128e00), so every argument
 * passes through to the callee unchanged. In the one observed call site
 * (network_game_client_end_frame), the caller resolves a server handle to a
 * connection pointer via network_game_client_get_seconds_to_game_start, then
 * calls this wrapper with the resulting connection pointer, a message buffer,
 * its size, a dest_address, and reliable=0. */
bool FUN_00124d40(void *connection, void *message, unsigned short size,
                  int dest_address, int reliable)
{
  return network_connection_write(connection, message, size, dest_address,
                                  reliable);
}

/* network_game_client_address_matches_server (0x124d50)
 *
 * Asserts the client, its connection handle (+0x82c), the address pointer and
 * that address' first dword are all non-null, then queries the connection's
 * own address into a 0x18-byte stack buffer and reports whether its first
 * dword (the IPv4 address) equals the caller-supplied one. Only the first
 * dword of the filled buffer is read back; the remaining 0x14 bytes are
 * written by network_connection_get_address and discarded.
 */
char network_game_client_address_matches_server(void *client,
                                                void *source_address)
{
  int connection_address[6]; /* EBP-0x18, 0x18 bytes */

  if (client == NULL) {
    display_assert("client != NULL",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x2d2, true);
    system_exit(-1);
  }
  if (*(int *)((char *)client + 0x82c) == 0) {
    display_assert("client->connection",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x2d3, true);
    system_exit(-1);
  }
  if (source_address == NULL) {
    display_assert("address != NULL",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x2d4, true);
    system_exit(-1);
  }
  if (*(int *)source_address == 0) {
    display_assert("address->address.ipv4_address",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x2d5, true);
    system_exit(-1);
  }

  network_connection_get_address(*(int *)((char *)client + 0x82c),
                                 connection_address, 0);
  return connection_address[0] == *(int *)source_address;
}

/* network_game_client_game_out_of_sync (0x124e20)
 *
 * One-shot out-of-sync notification. The byte global at 0x46e8b8 gates the
 * whole body: once it is set nothing happens at all. Otherwise the condition
 * is logged, and the first time through (client flag byte at +0xcac still
 * clear) UI error 8 is raised on every local player. The client flag is set
 * on both paths inside the guard. */
void network_game_client_game_out_of_sync(void *client)
{
  int16_t player_index;

  if (*(char *)0x46e8b8 == '\0') {
    network_game_log("local machine is out of sync with the server");
    if (*((char *)client + 0xcac) == '\0') {
      player_index = local_player_get_next(-1);
      while (player_index != -1) {
        ui_widget_display_error(8, player_index, 1, 0);
        player_index = local_player_get_next(player_index);
      }
    }
    *((char *)client + 0xcac) = 1;
  }
}

/* network_game_client_add_player_to_game (0x125510)
 *
 * Asserts both the client and the incoming player message are non-null, then
 * validates the player and hands it to network_game_add_player on the client's
 * embedded game state (+0x85c). Once the player is accepted, and only while the
 * client is in state 3 (in-game, field at +0xca6), the player record is
 * re-fetched from the client's own player array: base +0xa62, stride 0x20,
 * index from the int16 at +0xa80. That record is spawned, its byte at +0x1f is
 * passed to unstrip_player_index (one stack arg, ADD ESP,4 at 0x1255ac) whose
 * EAX return is the player handle used by every following call. When the
 * record's machine index (+0x1c) equals the client's own machine index (the
 * uint16 at +0x0), the local player index (+0x1d) is bound to the handle.
 *
 * Note the log at the bottom is reached both from the state-3 path (where the
 * pointer has been re-pointed at the array slot) and from the non-state-3
 * path (where it is still the message), matching ESI's reuse in the original.
 */
char network_game_client_add_player_to_game(void *client, void *message)
{
  char *player;
  char added;
  int player_handle;

  added = 0;
  player = (char *)message;
  if (client == NULL || message == NULL) {
    display_assert("client && player",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x462, true);
    system_exit(-1);
  }

  if (!network_player_is_valid(player)) {
    return added;
  }

  added = network_game_add_player((char *)client + 0x85c, player);
  if (!added) {
    return added;
  }

  if (*(int16_t *)((char *)client + 0xca6) == 3) {
    player =
      (char *)client + 0xa62 + (*(int16_t *)((char *)client + 0xa80) << 5);
    added = network_game_spawn_player(player);
    if (!added) {
      return added;
    }
    player_handle = unstrip_player_index((signed char)player[0x1f]);
    if ((int)(signed char)player[0x1c] == (int)*(uint16_t *)client) {
      local_player_set_player_index((unsigned short)(signed char)player[0x1d],
                                    player_handle);
    }
    update_client_add_player(player_handle);
    if (network_game_server_get() != NULL) {
      update_server_add_player(player_handle);
    }
  }

  network_game_log(
    "added new player to the game (machine #%d / controller #%d)",
    (int)(signed char)player[0x1c], (int)(signed char)player[0x1d]);
  return added;
}

/* network_client_switch_to_postgame (0x125610)
 *
 * Asserts client is non-null, then switches the game engine to the postgame
 * state, sets the client state field (offset 0xca6) to 4 (postgame), and
 * logs "switching to postgame". */
void network_client_switch_to_postgame(void *client)
{
  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x48c, 1);
    system_exit(-1);
  }
  game_engine_switch_to_postgame();
  *(int16_t *)((char *)client + 0xca6) = 4;
  network_game_log("switching to postgame");
}

/* 0x125710 — Asserts client is non-null and returns the connection handle
 * (int) stored at offset 0x82c in the client structure. The returned handle
 * is used by the caller (network_game_client_end_frame) as the first argument
 * to FUN_00124d40 (which forwards it to network_connection_write to send a
 * network message). */
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

/* 0x125750 — Asserts client is non-null, then calls
 * network_connection_get_address with the connection handle at offset 0x82c,
 * the output buffer, and flag 0. */
void network_game_client_switch_to_postgame(void *server, void *out)
{
  assert_halt(server);
  network_connection_get_address(*(int *)((char *)server + 0x82c), out, 0);
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
bool network_client_get_oos(void *server)
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
    if (cache_files_give_time_to_precache(map_name)) {
      csmemset(buf, 0, sizeof(buf));
      csstrncpy(buf, map_name, 0x100);
      encoded = (unsigned short *)encode_network_game_message(0x13, buf, 0x100);
      if (encoded != NULL) {
        size = *encoded >> 4;
        if (!network_connection_write((void *)*(int *)((char *)server + 0x82c),
                                      encoded, size, 0, 1)) {
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
  if (!network_game_is_splitscreen_local()) {
    connected = transport_network_available();
    if (!connected) {
      error(2, "network connection went down!");
      display_error_when_main_menu_loaded(6);
    }
  }
  if (connected != true)
    return connected;

  if (network_connection_connected(*(int *)((char *)server + 0x82c))) {
    if ((*(unsigned char *)((char *)server + 0xcaa) & 2) == 0) {
      csmemset(join_payload, 0, 0x50);
      network_game_generate_local_machine_name(join_payload);
      csmemcpy(&join_payload[0x40], (char *)server + 0x84a, 0x10);
      encoded =
        (unsigned short *)encode_network_game_message(0xc, join_payload, 0x50);
      if (encoded == NULL) {
        network_game_log(
          "failed to create a message_client_join_game_request message");
      } else if (network_connection_write(
                   (void *)*(int *)((char *)server + 0x82c), encoded,
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
        transport_server_terminate((int *)((char *)server + 0x830));
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
  if (network_game_is_splitscreen_local())
    goto check_result;
  result = transport_network_available();
  if (result)
    goto main_body;
  error(2, "network connection went down!");
  display_error_when_main_menu_loaded(6);

check_result:
  if (!result)
    goto tail_check;

main_body:
  if (!network_connection_active(*(int *)((char *)server + 0x82c)))
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
  if (!network_connection_active(*(int *)((char *)server + 0x82c))) {
    display_error_when_main_menu_loaded(4);
    return false;
  }
  return result;
}

/* FUN_00126db0 (0x126db0) — network_game_client_idle_ingame
 *
 * Called from the client idle dispatch (FUN_00127070) when state == 3 (ingame).
 * Verifies the server connection is alive, checks if the connection has gone
 * silent (bit 5 of connection+0x30 via network_connection_going_stale),
 * displays per-player error widgets if newly silent, records the silent flag at
 * server+0xcad, then runs the connection idle tick (15-second timeout) and
 * processes incoming messages. Returns false if the connection drops or any
 * critical step fails.
 */
bool FUN_00126db0(void *server)
{
  int connection;
  bool result;
  bool is_silent;
  __int16 player_idx;

  result = true;
  connection = *(int *)((char *)server + 0x82c);
  if (!network_connection_active(connection))
    goto abort;
  if (!network_connection_connected(connection))
    goto abort;

  if (!network_game_is_splitscreen_local()) {
    is_silent = network_connection_going_stale(connection);
    if (!transport_network_available()) {
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
    if (!network_connection_active(connection) ||
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
  if (network_game_is_splitscreen_local())
    goto check_result;
  result = transport_network_available();
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
  if (!network_connection_active(*(int *)((char *)server + 0x82c))) {
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
