/* Halo CE Xbox — server-side network message dispatch.
 * c:\halo\SOURCE\networking\network_server_message_handler.c
 *
 * network_game_server_handle_client_message() (0x130580) validates an
 * incoming connected-client message, then dispatches on the trailing packet
 * type to one of the per-message handlers below.  Each handler decodes its
 * packet with decode_network_game_message() (FUN_0012bce0) and applies the
 * requested state change, re-broadcasting pregame game data on success.
 *
 * Callers pass the server pointer in a register (ESI for most handlers, EAX
 * for FUN_00130180); the machine index, message buffer and message size are
 * stack arguments.  See kb.json @<reg> annotations.  All callee declarations
 * come from the generated decl.h. */

/* Handle client add-player request during pregame (0x12fd80).
 * network_game_server_handle_message_client_add_player_request_pregame:
 * requires pregame state, decodes the packet, adds the network player, then
 * re-sends pregame game data. */
char FUN_0012fd80(int server, int machine, void *message_data, int message_size)
{
  char decoded_buf[32];
  short packet_type;
  short packet_version;

  if (network_game_server_get_state(server, (short *)0) != 0) {
    network_game_log("failed to handle a message_client_add_player_request_"
                     "pregame because the server is not in pregame");
    return 1;
  }
  message_size -= 2;
  packet_type = 0xd;
  packet_version = 1;
  if (!FUN_0012bce0((int)decoded_buf, (int)((char *)message_data + 2),
                    (short *)&message_size, &packet_type, &packet_version, 3)) {
    network_game_log("server failed to decode a message_client_add_player_"
                     "request_pregame packet");
    return 1;
  }
  if (!network_game_server_add_player_to_game(server, machine, decoded_buf)) {
    network_game_log("server failed to add a network player in network_game_"
                     "server_handle_message_client_add_player_request_"
                     "pregame()");
    return 1;
  }
  if (!FUN_0012f5d0((void *)server)) {
    network_game_log("server failed to send pregame game data in network_game_"
                     "server_handle_message_client_add_player_request_"
                     "pregame()");
    return 1;
  }
  return 1;
}

/* Handle client remove-player request during pregame (0x12fe40). */
char FUN_0012fe40(int server, int machine, void *message_data, int message_size)
{
  char decoded_buf[32];
  short packet_type;
  short packet_version;

  if (network_game_server_get_state(server, (short *)0) != 0) {
    network_game_log("failed to handle a message_client_remove_player_request_"
                     "pregame because the server is not in pregame");
    return 1;
  }
  message_size -= 2;
  packet_type = 0xe;
  packet_version = 1;
  if (!FUN_0012bce0((int)decoded_buf, (int)((char *)message_data + 2),
                    (short *)&message_size, &packet_type, &packet_version, 3)) {
    network_game_log("server failed to decode a message_client_remove_player_"
                     "request_pregame packet");
    return 1;
  }
  if (!network_game_server_remove_player_from_game(server, machine,
                                                   (int)decoded_buf)) {
    network_game_log("server failed to remove a network player in network_"
                     "game_server_handle_message_client_remove_player_request_"
                     "pregame()");
    return 1;
  }
  if (!FUN_0012f5d0((void *)server)) {
    network_game_log("server failed to send pregame game data in network_game_"
                     "server_handle_message_client_remove_player_request_"
                     "pregame()");
    return 1;
  }
  return 1;
}

/* Handle client machine-settings request during pregame (0x12ff00).
 * Applies the settings, then logs the machine name (converted wide->ascii in
 * place) and its index (decoded_buf[0x40]) before re-sending game data. */
char FUN_0012ff00(int server, int machine, void *message_data, int message_size)
{
  char decoded_buf[65];
  short packet_type;
  short packet_version;
  char *name;

  if (network_game_server_get_state(server, (short *)0) != 0) {
    network_game_log("failed to handle a message_client_settings_request "
                     "because the server is not in pregame");
    return 1;
  }
  message_size -= 2;
  packet_type = 0xf;
  packet_version = 1;
  if (!FUN_0012bce0((int)decoded_buf, (int)((char *)message_data + 2),
                    (short *)&message_size, &packet_type, &packet_version, 3)) {
    network_game_log("server failed to decode a message_client_settings_"
                     "request packet");
    return 1;
  }
  if (!FUN_0012ca00(server, machine, (int)decoded_buf)) {
    network_game_log("network_game_server_adjust_machine_settings() failed in "
                     "network_game_server_handle_message_client_settings_"
                     "request()");
    return 1;
  }
  name = wide_to_ascii((const wchar_t *)decoded_buf, decoded_buf, 0x40);
  network_game_log("server received machine settings for machine #%d/'%s'",
                   (int)decoded_buf[0x40], name);
  if (!FUN_0012f5d0((void *)server)) {
    network_game_log("server failed to send pregame game data in network_game_"
                     "server_handle_message_client_settings_request()");
    return 1;
  }
  return 1;
}

/* Handle client player-settings request during pregame (0x12ffe0). */
char FUN_0012ffe0(int server, int machine, void *message_data, int message_size)
{
  char decoded_buf[32];
  short packet_type;
  short packet_version;
  int game;

  if (network_game_server_get_state(server, (short *)0) != 0) {
    network_game_log("failed to handle a message_client_player_settings_"
                     "request because the server is not in pregame");
    return 1;
  }
  message_size -= 2;
  packet_type = 0x10;
  packet_version = 1;
  if (!FUN_0012bce0((int)decoded_buf, (int)((char *)message_data + 2),
                    (short *)&message_size, &packet_type, &packet_version, 3)) {
    network_game_log("server failed to decode a message_client_player_"
                     "settings_request packet");
    return 1;
  }
  game = network_game_server_get_game((void *)server);
  if (!network_game_update_player(game, decoded_buf)) {
    network_game_log("network_game_update_player() failed in network_game_"
                     "server_handle_message_client_player_settings_request()");
    return 1;
  }
  network_game_log("server received updated player settings");
  if (!FUN_0012f5d0((void *)server)) {
    network_game_log("server failed to send pregame game data in network_game_"
                     "server_handle_message_client_player_settings_request()");
    return 1;
  }
  return 1;
}

/* Handle client graceful game-exit during pregame (0x1300b0).
 * Looks up the client machine and removes it from the game. */
char FUN_001300b0(int server, int machine, void *message_data, int message_size)
{
  char decoded_buf[4];
  short packet_type;
  short packet_version;
  int client_machine;

  if (network_game_server_get_state(server, (short *)0) != 0) {
    network_game_log("failed to handle a message_client_graceful_game_exit_"
                     "pregame message because the server is not in pregame");
    return 1;
  }
  message_size -= 2;
  packet_type = 0x12;
  packet_version = 1;
  if (!FUN_0012bce0((int)decoded_buf, (int)((char *)message_data + 2),
                    (short *)&message_size, &packet_type, &packet_version, 3)) {
    network_game_log("server failed to decode a message_client_graceful_game_"
                     "exit_pregame packet");
    return 1;
  }
  client_machine =
    network_game_server_get_client_machine(server, machine, (int *)0);
  if (!FUN_0012e090((void *)server, (void *)client_machine)) {
    network_game_log("network_game_server_remove_machine_from_game() failed in "
                     "network_game_server_handle_message_client_graceful_game_"
                     "exit_pregame()");
    return 1;
  }
  if (!FUN_0012f5d0((void *)server)) {
    network_game_log("server failed to send pregame game data in network_game_"
                     "server_handle_message_client_graceful_game_exit_"
                     "pregame()");
    return 1;
  }
  return 1;
}

/* Handle client remove-player request during a live game (0x130180).
 * Requires in-game state; removes the player, then broadcasts a
 * remove-player notification (type 0x16) to all machines.
 * server arrives in EAX (see kb.json @<eax>). */
char FUN_00130180(int server, int machine, void *message_data, int message_size)
{
  char decoded_buf[32];
  char notify_buf[36];
  short packet_type;
  short packet_version;
  int game;
  void *message;
  char result;

  if (network_game_server_get_state(server, (short *)0) != 1) {
    network_game_log("failed to handle a message_client_remove_player_request_"
                     "ingame because the server is not in game");
    return 1;
  }
  message_size -= 2;
  packet_type = 0x1a;
  packet_version = 1;
  if (!FUN_0012bce0((int)decoded_buf, (int)((char *)message_data + 2),
                    (short *)&message_size, &packet_type, &packet_version, 5)) {
    network_game_log("server failed to decode a message_client_remove_player_"
                     "request_ingame packet");
    return 1;
  }
  game = network_game_server_get_game((void *)server);
  if (network_game_remove_player(game, (int)decoded_buf)) {
    csmemcpy(notify_buf, decoded_buf, 0x20);
    *(int *)(notify_buf + 0x20) = game_time_get() + 0x21;
    message = encode_network_game_message(0x16, notify_buf, 0x24);
    if (message != (void *)0) {
      result = (char)FUN_0012f430((void *)server, message);
      if (!result) {
        network_game_log("network_game_server_send_message_to_all_machines() "
                         "failed in network_game_server_handle_message_client_"
                         "remove_player_request_ingame()");
      }
      return result;
    }
  }
  return 1;
}

/* Handle an incoming connectionless datagram (0x130270).
 * network_game_server_handle_datagram: validates the datagram header, rejects
 * error/data message categories, then dispatches broadcast game-search (0),
 * ping (1) and game-update (0x19) packets.  These arrive without an
 * established client machine, so the sender address is logged on every error.
 * All four arguments are stack parameters. */
bool FUN_00130270(void *server, void *buffer, int size, void *addr)
{
  unsigned short *msg;
  unsigned char category;
  short packet_type;
  short packet_version;
  char game_update_buf[136];
  char broadcast_buf[4];
  char ping_buf[8];
  int machine;

  msg = (unsigned short *)buffer;
  if (server == (void *)0 || msg == (unsigned short *)0 || addr == (void *)0 ||
      (unsigned short)size <= 2 ||
      (unsigned short)size != (unsigned short)(msg[0] >> 4)) {
    display_assert("server && message && source_address && (datagram_size > "
                   "sizeof(message_header)) && (datagram_size == "
                   "GET_MESSAGE_SIZE(*message))",
                   "c:\\halo\\SOURCE\\networking\\network_server_message_"
                   "handler.c",
                   0x40, 1);
    system_exit(-1);
  }
  category = (unsigned char)(msg[0] >> 2) & 3;
  if ((msg[0] & 3) != 0) {
    network_game_log("server received a datagram with invalid flags; sender= "
                     "'%s'",
                     transport_address_to_string(addr));
    return 1;
  }
  if (category == 1) {
    if ((unsigned short)size < 0x83) {
      network_game_log("server received a malformed/damaged message; sender= "
                       "'%s'",
                       transport_address_to_string(addr));
      return 1;
    }
    network_game_log("server received low-level error message: error= #%d "
                     "(%s); sender= '%s'",
                     ((unsigned char *)(msg + 1))[0x80], (char *)(msg + 1),
                     transport_address_to_string(addr));
    return 1;
  }
  if (category == 2) {
    network_game_log("server received a bad message type (_message_type_data); "
                     "sender= '%s'",
                     transport_address_to_string(addr));
    return 1;
  }
  if (category != 3) {
    network_game_log("server received a datagram with an unknown message type "
                     "(#%d); sender= '%s'",
                     category, transport_address_to_string(addr));
    return 1;
  }
  packet_version = 1;
  packet_type = *((char *)msg + (short)(unsigned short)size - 1);
  size -= 2;
  switch (packet_type) {
  case 0:
    if (!network_game_accept_remote_connections())
      return 1;
    if (!FUN_0012bce0((int)broadcast_buf, (int)((char *)msg + 2),
                      (short *)&size, &packet_type, &packet_version, 0)) {
      network_game_log("failed to decode a message_client_broadcast_game_"
                       "search packet");
      return 1;
    }
    if (!network_game_server_reset_to_pregame((int)server, broadcast_buf,
                                              addr)) {
      network_game_log("server failed to advertise game to prospective client "
                       "at '%s'",
                       transport_address_to_string(addr));
      return 1;
    }
    return 1;
  case 1:
    if (!network_game_accept_remote_connections())
      return 1;
    if (!FUN_0012bce0((int)ping_buf, (int)((char *)msg + 2), (short *)&size,
                      &packet_type, &packet_version, 0)) {
      network_game_log("failed to decode a message_client_ping packet");
      return 1;
    }
    if (!FUN_0012f8d0((int)server, ping_buf, addr)) {
      network_game_log("server failed to handle a client ping");
      return 1;
    }
    return 1;
  case 0x19:
    if (network_game_server_get_state((int)server, (short *)0) != 1) {
      network_game_log("ignoring a message_client_game_update message; we are "
                       "not in game");
      return 1;
    }
    machine = network_game_server_get_client_machine_at_address(
      (int)server, *(int *)addr);
    if (machine == 0) {
      network_game_log("failed to handle a message_client_game_update message; "
                       "this client doesn't seem to be in the game");
      return 1;
    }
    if (!FUN_0012bce0((int)game_update_buf, (int)((char *)msg + 2),
                      (short *)&size, &packet_type, &packet_version, 5)) {
      network_game_log("failed to decode a message_client_game_update packet");
      return 1;
    }
    network_game_server_handle_client_update_packet((int)server, machine,
                                                    game_update_buf);
    return 1;
  default:
    network_game_log("server received datagram with an unexpected packet type; "
                     "sender= '%s'",
                     transport_address_to_string(addr));
    return 1;
  }
}

/* Dispatch an incoming connected-client message (0x130580).
 * network_game_server_handle_client_message: validates header size/flags,
 * rejects error/data message categories, gates un-validated clients to the
 * join request only, then switches on the trailing packet-type byte. */
bool FUN_00130580(void *server, void *machine, void *buffer, int size)
{
  unsigned short *msg;
  unsigned char category;
  char packet_type;

  msg = (unsigned short *)buffer;
  if (server == (void *)0 || machine == (void *)0 || msg == (unsigned short *)0 ||
      (unsigned short)size != (unsigned short)(msg[0] >> 4)) {
    display_assert("server && machine && message && (message_buffer_size == "
                   "GET_MESSAGE_SIZE(*message))",
                   "c:\\halo\\SOURCE\\networking\\network_server_message_"
                   "handler.c",
                   0xcf, 1);
    system_exit(-1);
  }
  category = (unsigned char)(msg[0] >> 2) & 3;
  if ((msg[0] & 3) != 0) {
    network_game_log("server received client message with invalid flags");
    return 1;
  }
  if (category == 1) {
    if ((unsigned short)size < 0x83) {
      network_game_log("server received a malformed/damaged message from a "
                       "client");
      return 1;
    }
    network_game_log("server received low-level error message from a client: "
                     "error= #%d (%s)",
                     ((unsigned char *)(msg + 1))[0x80], (char *)(msg + 1));
    return 1;
  }
  if (category == 2) {
    network_game_log("server received a bad message type from a client "
                     "(_message_type_data)");
    return 1;
  }
  if (category != 3) {
    network_game_log("server received a client message with an unknown message "
                     "type (#%d)",
                     category);
    return 1;
  }
  packet_type = *((char *)msg + (short)(unsigned short)size - 1);
  if (network_game_server_client_machine_is_joined_to_game((int)server,
                                                           (int)machine) == 0 &&
      packet_type != 0xc) {
    network_game_log("an un-validated client sent something other than a join "
                     "request message");
    return 1;
  }
  switch (packet_type) {
  case 0xc:
    if (!FUN_0012f990((int)server, machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_join_game_"
                       "request() failed");
      return 0;
    }
    break;
  case 0xd:
    if (!FUN_0012fd80((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_add_player_"
                       "request_pregame() failed");
      return 0;
    }
    break;
  case 0xe:
    if (!FUN_0012fe40((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_remove_"
                       "player_request_pregame() failed");
      return 0;
    }
    break;
  case 0xf:
    if (!FUN_0012ff00((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_settings_"
                       "request() failed");
      return 0;
    }
    break;
  case 0x10:
    if (!FUN_0012ffe0((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_player_"
                       "settings_request() failed");
      return 0;
    }
    break;
  case 0x11:
    if (!FUN_0012f040((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_game_start_"
                       "request() failed");
      return 0;
    }
    break;
  case 0x12:
  case 0x22:
    if (!FUN_001300b0((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_graceful_"
                       "game_exit_pregame() failed");
      return 0;
    }
    break;
  case 0x13:
    if (!FUN_0012f0d0((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_graceful_"
                       "game_exit_pregame() failed");
      return 0;
    }
    break;
  case 0x18:
    if (!FUN_0012f170((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_loaded() "
                       "failed");
      return 0;
    }
    break;
  case 0x1a:
    if (!FUN_0012f200((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_add_player_"
                       "request_ingame() failed");
      return 0;
    }
    break;
  case 0x1b:
    if (!FUN_00130180((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_remove_"
                       "player_request_ingame() failed");
      return 0;
    }
    break;
  case 0x20:
    if (!FUN_0012f290((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_remove_"
                       "player_request_postgame() failed");
      return 0;
    }
    break;
  case 0x21:
    if (!FUN_0012f330((int)server, (int)machine, buffer, size)) {
      network_game_log("network_game_server_handle_message_client_switch_to_"
                       "pregame() failed");
      return 0;
    }
    break;
  default:
    network_game_log("bad or inappropriate packet type received from a client "
                     "(#%d)",
                     packet_type);
    return 1;
  }
  return 1;
}
