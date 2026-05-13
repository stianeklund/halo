/* FUN_00129130 (0x129130)
 *
 * Disconnect a client connection from a server connection's client list.
 * Removes the client's endpoint from the server's endpoint set, disposes the
 * client connection, and clears the slot.
 *
 * NOTE: __FILE__ string references network_connection.c, but this function
 * is linked into network_game_globals.obj.
 */
bool FUN_00129130(int server_connection, int client_connection)
{
  int *client_list;
  int i;

  assert_halt(server_connection);
  assert_halt(client_connection);
  assert_halt(*(uint8_t *)(server_connection + 0x30) & 1);
  assert_halt(*(int *)(server_connection + 0x38));

  client_list = (int *)(server_connection + 0x3c);
  assert_halt(client_list);

  i = 0;
  while (client_list[i] == 0 || client_list[i] != client_connection) {
    i++;
    if (i >= 5)
      return false;
  }

  if (*(int *)client_connection != 0) {
    short result =
      FUN_00082850(*(int *)*(int *)(server_connection + 0x3c + i * 4),
                   *(int *)(server_connection + 0x38));
    if (result != 0) {
      error(2,
            "failed to remove a client endpoint from the server's endpoint set "
            "(maybe it was already removed)");
    }
  }

  network_connection_delete(*(int *)(server_connection + 0x3c + i * 4));
  *(int *)(server_connection + 0x3c + i * 4) = 0;
  return true;
}

/* FUN_001298f0 (0x1298f0)
 *
 * Read a message from a network connection. If the connection is a server,
 * reads from the unreliable incoming queue. If the connection is a client,
 * tries the reliable queue first, then falls back to unreliable if the
 * connection is a clientside client.
 *
 * NOTE: __FILE__ string references network_connection.c, but this function
 * is linked into network_game_globals.obj.
 */
bool FUN_001298f0(int connection, void *buffer, int *size, void *addr)
{
  bool result;

  if ((*(uint32_t *)(connection + 0x30) & 1) != 0) {
    return FUN_001286e0(connection, buffer, size, addr);
  }

  if ((*(uint32_t *)(connection + 0x30) & 6) == 0) {
    assert_halt_msg(
      0, "connection->flags&FLAG(_connection_create_clientside_client_bit) || "
         "connection->flags&FLAG(_connection_create_serverside_client_bit)");
  }

  result = FUN_001292f0(connection, buffer, size, addr);
  if (!result && (*(uint8_t *)(connection + 0x30) & 2) != 0) {
    result = FUN_001286e0(connection, buffer, size, addr);
  }
  return result;
}

/* FUN_00129cf0 (0x129cf0)
 *
 * Network connection idle processing. Handles timeout detection, reliable and
 * unreliable endpoint servicing, and datagram reception from the network
 * endpoint into the circular queue.
 *
 * NOTE: __FILE__ string references network_connection.c, but this function
 * is linked into network_game_globals.obj.
 */
bool FUN_00129cf0(int connection, int timeout, int *output)
{
  unsigned int now;
  uint32_t flags;
  bool ok;
  bool is_connected;
  int bytes_read;
  short addr_result;
  unsigned int queue_space;
  uint8_t recv_buf[400];
  uint8_t addr_buf[24];
  void *log_stream;

  now = system_milliseconds();
  ok = true;

  assert_halt(connection);

  flags = *(uint32_t *)(connection + 0x30) & ~0x20u;
  *(uint32_t *)(connection + 0x30) = flags;

  if (timeout == 0) {
    *(unsigned int *)(connection + 0x8) = now;
  } else {
    if (now > (unsigned int)(*(int *)(connection + 0x8) + 5000)) {
      *(uint32_t *)(connection + 0x30) = flags | 0x20;
    }
    if (now > (unsigned int)(*(int *)(connection + 0x8) + timeout)) {
      if (*(uint8_t *)0x46e8ba == 0) {
        error(2, "timeout in network_connection_idle");
        return false;
      }
      error(2, "dont timeout is active so not timing out of a connection");
      *(unsigned int *)(connection + 0x8) = now;
    }
  }

  if ((*(uint32_t *)(connection + 0x30) & 1) != 0) {
    ok = FUN_00129a30(connection, output);
    if (!ok) {
      error(2, "network_connection_idle_server_reliable_endpoint failed");
      return false;
    }
  } else {
    if ((*(uint32_t *)(connection + 0x30) & 6) != 0) {
      ok = FUN_001294d0(connection);
      if (!ok) {
        error(2, "network_connection_idle_client_reliable_endpoint failed");
        return false;
      }
    }
  }

  if (*(int *)(connection + 0x4) == 0)
    return ok;

  queue_space = circular_queue_free_space(*(int *)(connection + 0x14));

  while (ok && queue_space > 0x193) {
    is_connected = FUN_000831a0(*(int *)(connection + 0x4));

    if (!is_connected) {
      bytes_read =
        FUN_00084520(*(int *)(connection + 0x4), recv_buf, 400, addr_buf);

      if (bytes_read > 0) {
        if (*(int *)(connection + 0x18) != 0) {
          int elapsed =
            (int)(system_milliseconds() - *(unsigned int *)(connection + 0x1c));
          double dval = (double)elapsed;
          if (elapsed < 0)
            dval = dval + *(double *)0x265d40;
          dval = dval * *(double *)0x294bf0;
          crt_fprintf(*(void **)(connection + 0x18), "%g\t%ld\t%ld\t%ld\t%ld\n",
                      dval, 0, bytes_read, 0, 0);
          log_stream = *(void **)(connection + 0x18);
          goto flush_log;
        }
        goto process_datagram;
      }
    } else {
      bytes_read = FUN_00082e50(*(int *)(connection + 0x4), recv_buf, 400);

      if (bytes_read > 0) {
        addr_result = FUN_00083a60(*(int *)(connection + 0x4), addr_buf);
        if (addr_result != 0) {
          csmemset(addr_buf, 0, 0x18);
          *(uint16_t *)(addr_buf + 0x10) = 4;
        }

        if (*(int *)(connection + 0x18) != 0) {
          int elapsed =
            (int)(system_milliseconds() - *(unsigned int *)(connection + 0x1c));
          double dval = (double)elapsed;
          if (elapsed < 0)
            dval = dval + *(double *)0x265d40;
          dval = dval * *(double *)0x294bf0;
          crt_fprintf(*(void **)(connection + 0x18), "%g\t%ld\t%ld\t%ld\t%ld\n",
                      dval, 0, bytes_read, 0, 0);
          log_stream = *(void **)(connection + 0x18);
        flush_log:
          crt_fflush(log_stream);
        }
      process_datagram:
        *(int *)(connection + 0x24) = *(int *)(connection + 0x24) + 1;
      }
    }

    if (bytes_read > 400) {
      assert_halt_msg(0, "endpoint read buffer overflowed");
    }

    if (bytes_read < 1)
      return ok;

    if (*(int *)addr_buf == 0) {
      error(2, "datagram received from unknown address");
    } else {
      csmemcpy(recv_buf + bytes_read, addr_buf, 4);
      ok = FUN_00118ec0(*(int *)(connection + 0x14), recv_buf, bytes_read + 4);
      if (!ok) {
        assert_halt_msg(
          0, "circular_queue_queue_data() failed though it should have had "
             "enough room");
      }
    }

    queue_space = circular_queue_free_space(*(int *)(connection + 0x14));
  }

  return ok;
}

/* network_game_in_progress (0x12a000)
 *
 * Returns true when either global network game endpoint pointer is non-null.
 */
bool network_game_in_progress(void)
{
  if (*(int *)0x46e8c0 == 0 && *(int *)0x46e8bc == 0) {
    return false;
  }

  return true;
}

/* network_game_accept_remote_connections (0x12a160)
 *
 * Returns the network game globals byte at 0x46e8c4.
 */
bool network_game_accept_remote_connections(void)
{
  return *(uint8_t *)0x46e8c4;
}

/* FUN_0012a170 (0x12a170)
 *
 * Returns true when a server connection exists (0x46e8bc != NULL) and the
 * global byte at 0x46e8c4 is zero.
 */
bool FUN_0012a170(void)
{
  if (*(void **)0x46e8bc != NULL && *(uint8_t *)0x46e8c4 == 0) {
    return true;
  }
  return false;
}

/* network_game_server_get (0x12a1d0)
 *
 * Returns the global network game server pointer.
 */
void *network_game_server_get(void)
{
  return *(void **)0x46e8bc;
}

/* dispose_global_network_game_client (0x12a1e0)
 *
 * Tear down the global network game client if one exists.
 */
void dispose_global_network_game_client(void)
{
  if (*(void **)0x46e8bc != NULL) {
    network_game_client_dispose(*(void **)0x46e8bc);
    *(void **)0x46e8bc = NULL;
    *(uint8_t *)0x46e8c5 = 0;
  }
}

/* network_game_server_start_frame (0x12a210)
 *
 * Begin a network game server frame.  Returns the result of the
 * server's start function, or true with a warning if no server exists.
 */
bool network_game_server_start_frame(void)
{
  if (*(void **)0x46e8bc != NULL) {
    return network_game_server_start(*(void **)0x46e8bc);
  }
  error(2, "no network game server");
  return true;
}

/* network_game_client_get (0x12a240)
 *
 * Returns the global network game client pointer.
 */
void *network_game_client_get(void)
{
  return *(void **)0x46e8c0;
}

/* dispose_global_network_game_server (0x12a2a0)
 *
 * Tear down the global network game server if one exists.
 */
void dispose_global_network_game_server(void)
{
  if (*(void **)0x46e8c0 != NULL) {
    network_game_server_dispose(*(void **)0x46e8c0);
    *(void **)0x46e8c0 = NULL;
  }
  *(uint8_t *)0x46e8c6 = 0;
}

/* network_game_client_start_frame (0x12a2d0)
 *
 * Begin a network game client frame.  If the abort flag (0x46e8c6) is
 * set, performs an orderly disconnect: disposes both server and client,
 * then returns true.  Otherwise, polls the server for its current state
 * and logs state transitions.
 */
bool network_game_client_start_frame(void)
{
  int16_t state;
  int local_4;

  if (*(uint8_t *)0x46e8c6 == 1) {
    FUN_000fff70(0);

    int reason;
    if (*(void **)0x46e8bc != NULL) {
      reason = FUN_0012d570(*(void **)0x46e8bc);
    } else if (*(void **)0x46e8c0 != NULL) {
      reason = (int)(intptr_t)network_game_client_get_machine_index(
        *(void **)0x46e8c0);
    } else {
      reason = 0;
    }
    network_game_end_and_load_ui(reason);

    if (*(void **)0x46e8c0 != NULL) {
      network_game_server_dispose(*(void **)0x46e8c0);
      *(void **)0x46e8c0 = NULL;
    }
    *(uint8_t *)0x46e8c6 = 0;
    if (*(void **)0x46e8bc != NULL) {
      network_game_client_dispose(*(void **)0x46e8bc);
      *(void **)0x46e8bc = NULL;
      *(uint8_t *)0x46e8c5 = 0;
    }
    main_goto_main_menu();
    return true;
  }

  bool result = FUN_00127070(*(void **)0x46e8c0);
  if (!result)
    return false;

  if (FUN_00124cc0(*(void **)0x46e8c0) != 0)
    return false;

  state = FUN_00124a30(*(void **)0x46e8c0, &local_4);

  switch (state) {
  case 0:
    if (*(int16_t *)0x322cd8 != state)
      network_game_log("searching for a network game ...");
    break;
  case 1:
    if (*(int16_t *)0x322cd8 != state)
      network_game_log("joining a network game ...");
    break;
  case 2:
    if (*(int16_t *)0x322cd8 != state)
      network_game_log("waiting for game to start ...");
    break;
  case 3:
    if (*(int16_t *)0x322cd8 != state)
      network_game_log("client signalled to begin loading for network game");
    break;
  case 4:
    if (*(int16_t *)0x322cd8 != state)
      network_game_log("waiting for game to restart ...");
    break;
  default:
    display_assert("client is in an unknown state",
                   "c:\\halo\\SOURCE\\networking\\network_game_globals.c",
                   0x160, 1);
    system_exit(-1);
    break;
  }
  *(int16_t *)0x322cd8 = state;
  return result;
}

/* network_game_client_end_frame (0x12a500)
 *
 * End a network game client frame.  If no server exists, resets player
 * actions.  Otherwise, in state 3 (in-game), sends a game update packet
 * to the server at most every 16ms.
 */
bool network_game_client_end_frame(void)
{
  int16_t state;
  int now;
  int last_send;
  bool result;
  uint32_t flags;
  uint8_t local_124[128];
  uint32_t msg_buf[34];
  uint8_t local_1c[24];

  result = true;

  if (*(void **)0x46e8c0 == NULL) {
    FUN_000fff70(0);
    main_reset_player_actions();
    return true;
  }

  state = FUN_00124a30(*(void **)0x46e8c0, NULL);
  last_send = *(int *)0x46e8c8;

  if (state == 3) {
    now = system_milliseconds();
    last_send = *(int *)0x46e8c8;

    if ((unsigned int)(now - *(int *)0x46e8c8) > 0xf &&
        network_game_client_get_available_games(*(void **)0x46e8c0)) {
      network_game_client_get_error(*(void **)0x46e8c0);
      network_game_client_get_machine_index(*(void **)0x46e8c0);
      FUN_000b8fa0(local_124);

      if (FUN_00125860(*(void **)0x46e8c0)) {
        flags = network_game_client_get_error(*(void **)0x46e8c0);
        flags = flags | 0x80000000;
      } else {
        flags = network_game_client_get_error(*(void **)0x46e8c0);
        flags = flags & 0x7fffffff;
      }

      msg_buf[0] = flags;
      csmemcpy((char *)msg_buf + 8, local_124, 0x80);
      *(uint16_t *)((char *)msg_buf + 6) = (uint16_t)local_player_count();

      uint16_t *msg =
        (uint16_t *)encode_network_game_message(0x19, msg_buf, 0x88);
      last_send = now;

      if (msg == NULL) {
        network_game_log(
          "failed to create a _message_type_client_game_update message");
        result = false;
      } else {
        network_game_client_switch_to_postgame(*(void **)0x46e8c0, local_1c);
        int send_result = network_game_client_get_seconds_to_game_start(
          *(void **)0x46e8c0, msg, *msg >> 4, local_1c, 0);
        result = FUN_00124d40(send_result);
        if (!result) {
          network_game_log("failed to send a game update to the server");
          *(int *)0x46e8c8 = now;
          return false;
        }
      }
    }
  }

  *(int *)0x46e8c8 = last_send;
  return result;
}

/* network_game_abort (0x12a780)
 *
 * Signals network-game abort by setting the global abort flag byte.
 */
void network_game_abort(void)
{
  *(unsigned char *)0x46e8c6 = 1;
}
