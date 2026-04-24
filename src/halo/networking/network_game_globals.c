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

/* network_game_server_get (0x12a1d0)
 *
 * Returns the global network game server pointer.
 */
void *network_game_server_get(void)
{
  return *(void **)0x46e8bc;
}

/* network_game_client_get (0x12a240)
 *
 * Returns the global network game client pointer.
 */
void *network_game_client_get(void)
{
  return *(void **)0x46e8c0;
}

/* network_game_abort (0x12a780)
 *
 * Signals network-game abort by setting the global abort flag byte.
 */
void network_game_abort(void)
{
  *(unsigned char *)0x46e8c6 = 1;
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
      reason = (int)(intptr_t)FUN_001257a0(*(void **)0x46e8c0);
    } else {
      reason = 0;
    }
    FUN_0012ab80(reason);

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
    FUN_00100620();
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
        FUN_001257e0(*(void **)0x46e8c0)) {
      FUN_00125820(*(void **)0x46e8c0);
      FUN_001257a0(*(void **)0x46e8c0);
      FUN_000b8fa0(local_124);

      if (FUN_00125860(*(void **)0x46e8c0)) {
        flags = FUN_00125820(*(void **)0x46e8c0);
        flags = flags | 0x80000000;
      } else {
        flags = FUN_00125820(*(void **)0x46e8c0);
        flags = flags & 0x7fffffff;
      }

      msg_buf[0] = flags;
      csmemcpy((char *)msg_buf + 8, local_124, 0x80);
      *(uint16_t *)((char *)msg_buf + 6) = (uint16_t)local_player_count();

      uint16_t *msg = (uint16_t *)FUN_0012b700(0x19, msg_buf, 0x88);
      last_send = now;

      if (msg == NULL) {
        network_game_log(
          "failed to create a _message_type_client_game_update message");
        result = false;
      } else {
        FUN_00125750(*(void **)0x46e8c0, local_1c);
        int send_result = FUN_00125710(*(void **)0x46e8c0, msg,
                                        *msg >> 4, local_1c, 0);
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
