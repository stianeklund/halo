/* Check if the server's game is valid (0x12c160).
 * Returns bit 1 of the flags byte at server+6. */
bool FUN_0012c160(void *server)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x220, 1);
    system_exit(-1);
  }
  return (*(uint8_t *)((char *)server + 6) >> 1) & 1;
}

/* Xbox kernel sleep wrapper (stdcall, 2 args) */
typedef void(__stdcall *sleep_fn)(int milliseconds, int alertable);
#define XSleep ((sleep_fn)0x1d01c4)

/* Signal game over to all clients (0x12c370).
 * Sets server state from in-game (1) to post-game (2) and broadcasts
 * a _message_type_server_game_over message. */
void network_server_manager_game_over(void *server)
{
  int data;
  void *msg;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x2ff, 1);
    system_exit(-1);
  }
  if (*(int16_t *)((char *)server + 4) == 1) {
    data = 0;
    *(int16_t *)((char *)server + 4) = 2;
    msg = FUN_0012b700(0x17, &data, 4);
    if (msg) {
      if (FUN_0012f430(server, msg)) {
        network_game_log("server sent message_game_over to all clients");
        return;
      }
      network_game_log(
        "failed to signal all client machines to switch to postgame");
      return;
    }
    network_game_log(
      "failed to create a _message_type_server_game_over message");
  }
}

/* Assert server is non-null and return the connection pointer at offset +8
 * (0x12d570). */
int FUN_0012d570(void *server)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x769, 1);
    system_exit(-1);
  }
  return (int)((char *)server + 8);
}

/* Add a new client connection to the server (0x12d880).
 * Finds the first empty machine slot (short == -1 at +0x448 stride 0x10),
 * validates the remote address, stores the connection, and signals accept. */
bool FUN_0012d880(int server, int new_connection)
{
  char *s = (char *)server;
  bool result = false;
  int i;
  short *slot;
  char addr_buf[24];
  const char *addr_str;

  if (!server || !new_connection) {
    display_assert("server && new_connection",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x7d4, 1);
    system_exit(-1);
  }

  if (!FUN_0012c100((void *)server)) {
    network_game_log(
      "network_game_server_add_new_client() failed because the game is closed");
    return result;
  }

  i = 0;
  slot = (short *)(s + 0x448);
  do {
    if (*slot == -1) {
      csmemset(addr_buf, 0, 24);
      FUN_001283c0(new_connection, addr_buf, 0);
      if (*(int *)addr_buf == 0) {
        network_game_log(
          "network_connection_get_address() failed to get a valid address "
          "in network_game_server_add_new_client()");
        result = false;
      } else {
        if (!FUN_0012a160() && *(int *)addr_buf != 0x7f000001) {
          addr_str = FUN_00081b90(addr_buf);
          network_game_log(
            "remote system tried to join our server but we are not accepting "
            "remote connections: address= '%s'",
            addr_str);
          result = false;
        } else {
          *(int *)(s + i * 0x10 + 0x43c) = new_connection;
          FUN_0012acb0(s + 8, i);
          *(short *)(s + i * 0x10 + 0x448) = (short)i;
          *(short *)(s + i * 0x10 + 0x44a) = 1;
          result = FUN_001285c0(*(int *)s, new_connection);
          if (result == 1) {
            addr_str = FUN_00081b90(addr_buf);
            network_game_log("new remote connection accepted from %s",
                             addr_str);
          }
        }
      }
      break;
    }
    i++;
    slot += 8;
    result = false;
  } while (i < 4);

  if (i == 4) {
    network_game_log("failed to find an available machine slot in "
                     "network_game_server_add_new_client()");
  }

  return result;
}

/* Handle incoming datagrams on the server's public endpoint (0x12d9f0).
 * Loops reading datagrams and dispatching them until none remain. */
bool FUN_0012d9f0(int server)
{
  char *s = (char *)server;
  bool result = true;
  char buffer[0x190];
  char addr[24];
  int size = 0x190;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x810, 1);
    system_exit(-1);
  }

  do {
    if (!FUN_001298f0(*(int *)s, buffer, &size, addr))
      return result;
    result = FUN_00130270((void *)server, buffer, size, addr);
    if (!result) {
      network_game_log("network_game_server_handle_datagram() failed in "
                       "network_game_server_handle_public_endpoint()");
    }
    size = 0x190;
  } while (result);

  return false;
}

/* Postgame state handler (0x12db60).
 * Every 5 seconds sends a heartbeat message (type 0xb) to all clients. */
bool FUN_0012db60(int server)
{
  unsigned int now;
  short data;

  now = system_milliseconds();
  if (now > *(unsigned int *)((char *)server + 0x480) + 5000) {
    data = 0;
    FUN_0012f430((void *)server, FUN_0012b700(0xb, &data, 2));
    *(unsigned int *)((char *)server + 0x480) = now;
  }
  return true;
}

/* Set up the network game variant and server parameters (0x12dc20).
 * Loads the game variant, copies the game name (defaulting to L"<unknown>"),
 * and configures machine count/team settings. */
bool FUN_0012dc20(int server)
{
  char *s = (char *)server;
  char name_buf[0x40];

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x961, 1);
    system_exit(-1);
  }

  network_game_log("setting up a net game");

  if (!FUN_000a8aa0(s + 0xac, s + 0x2c)) {
    error(2, "network game setup failed; probably due to a missing playlist");
    return false;
  }

  csmemcpy(name_buf, (void *)0x281c38, 20);
  csmemset(name_buf + 20, 0, 44);
  FUN_0012aaf0(name_buf);
  ustrncpy((wchar_t *)(s + 8), (wchar_t *)name_buf, 0xf);

  *(short *)(s + 0x26) = 0;
  *(int *)(s + 0x28) = 0;
  *(char *)(s + 0x115) = 2;
  *(char *)(s + 0x116) = 0x10;
  *(char *)(s + 0x117) = (*(char *)(s + 0xc8) != 0) + 1;

  FUN_0012c060((void *)server);

  return true;
}

/* Process all connected client machines (0x12e580).
 * For each of 4 machine slots: checks connection liveness, reads pending
 * messages, handles disconnections and removal. Returns true on success. */
bool FUN_0012e580(int server)
{
  char *s = (char *)server;
  int i;
  short *flags;
  char *machine;
  char buffer[0x800];
  int size;
  int machine_index;
  const char *log_msg;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x827, 1);
    system_exit(-1);
  }

  i = 0;
  flags = (short *)(s + 0x448);

loop_top:
  for (;;) {
    if (i >= 4)
      return true;

    if (*flags == -1)
      goto next_slot;

    machine = (char *)flags - 0xc;

    if (!FUN_00128660(*(int *)machine)) {
      if (FUN_0012e090((void *)server, s + 0x11c + (int)*flags * 0x44)) {
        network_game_log("client machine %x removed from game", (int)*flags);
      } else {
        network_game_log("failed to remove client machine %x from game",
                         (int)*flags);
      }
      FUN_0012de20((void *)server);
      i++;
      flags += 8;
      goto loop_top;
    }

    if (!FUN_00129cf0(*(int *)machine, 0, 0))
      goto remove_path;

    if (!FUN_00128360(*(int *)machine))
      goto remove_path;

    do {
      size = 0x800;
      if (!FUN_001298f0(*(int *)machine, buffer, &size, 0))
        goto next_slot;
    } while (FUN_00130580((void *)server, machine, buffer, size));

    network_game_log("network_game_server_handle_client_message() failed in "
                     "network_game_server_handle_client_machines()");

    if (FUN_0012e090((void *)server, s + 0x11c + (int)*flags * 0x44)) {
      machine_index = (int)*flags;
      log_msg = "client machine removed from game";
    } else {
      if (FUN_0012df50((void *)server, machine))
        goto next_slot;
      machine_index = (int)*flags;
      log_msg = "failed to remove client machine from game";
    }
    goto do_log;

  remove_path:
    if (FUN_0012e090((void *)server, s + 0x11c + (int)*flags * 0x44)) {
      machine_index = (int)*flags;
      log_msg = "client machine removed from game";
    } else {
      machine_index = (int)*flags;
      log_msg = "failed to remove client machine from game";
    }

  do_log:
    network_game_log(log_msg, machine_index);

  next_slot:
    i++;
    flags += 8;
  }
}

/* Pregame tick handler (0x12e750).
 * When loading: enforces a 15-second timeout for client map loads.
 * When not loading: boots dead clients, checks if all players are ready,
 * manages the pregame countdown, and starts the game when ready. */
bool FUN_0012e750(int server)
{
  char *s = (char *)server;
  int now;
  bool result;
  int i;
  int *conn_ptr;
  short *flags_ptr;
  char name_buf[0x20];
  const char *name;
  short countdown;
  int timer_ms;

  now = system_milliseconds();
  result = true;

  if (*(char *)(s + 0x4b9) != 0) {
    if (*(int *)(s + 0x484) == 0)
      return true;

    if ((unsigned int)(system_milliseconds() - *(int *)(s + 0x484)) < 15000)
      return result;

    flags_ptr = (short *)(s + 0x44a);
    for (i = 4; i != 0; i--) {
      if ((*(short *)flags_ptr & 1) && !(*(short *)flags_ptr & 4)) {
        if (FUN_0019f3a0(s + 0x11c + (int)*(short *)(flags_ptr - 1) * 0x44,
                         name_buf, 0x20)) {
          name = name_buf;
        } else {
          name = "<unknown name>";
        }
        network_game_log(
          "forcibly removing client system '%s' due to timeout while "
          "loading for game",
          name);
        if (!FUN_0012df50((void *)server, (void *)(flags_ptr - 7))) {
          display_assert(
            "removed", "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
            0x94e, 1);
          system_exit(-1);
        }
      }
      flags_ptr += 8;
    }
    FUN_0012caa0((void *)server);
    return result;
  }

  conn_ptr = (int *)(s + 0x43c);
  for (i = 0; i < 4; i++) {
    if (*conn_ptr != 0 && !FUN_00128660(*conn_ptr)) {
      network_game_log("booting dead client machine %d", i);
      FUN_0012df50((void *)server, conn_ptr);
    }
    conn_ptr += 4;
  }

  if (*(char *)(s + 0x494) != 1) {
    if (now <= *(int *)(s + 0x480) + 5000)
      return result;
    countdown = 0;
    FUN_0012f430((void *)server, FUN_0012b700(0xa, &countdown, 2));
    *(int *)(s + 0x480) = now;
    return result;
  }

  if (!FUN_0012d150((void *)server) || !FUN_0012d0c0((void *)server) ||
      FUN_0012d040((void *)server) ||
      *(short *)(s + 0x22c) < (short)*(char *)(s + 0x115)) {
    csmemset(s + 0x488, 0, 0x10);
    i = 0;
  } else {
    i = 1;
    timer_ms = FUN_0012bdb0(s + 0x488);
    if (timer_ms == 0) {
      if (FUN_0012dbb0(server) && *(char *)(s + 0x495) == 0) {
        FUN_0012c0b0((void *)server);
        result = FUN_0012c290((void *)server);
        if (result == 1)
          return true;
        network_game_log("network_game_server_start_network_game() failed");
        return result;
      }
    }
    if (now - *(int *)(s + 0x490) < 0x3e9)
      return result;
  }

  *(char *)(s + 0x496) = 0;
  if (i) {
    timer_ms = FUN_0012bdb0(s + 0x488);
    countdown = (short)(timer_ms / 1000);
  } else {
    countdown = -1;
  }

  {
    void *msg = FUN_0012b700(7, &countdown, 2);
    if (!msg)
      return result;
    if (!FUN_0012f430((void *)server, msg)) {
      network_game_log(
        "failed to send a message_server_pregame_countdown to all clients");
      return result;
    }
  }

  *(int *)(s + 0x490) = now;
  return result;
}

/* Dispose the network game server (0x12ea00).
 * Sends graceful exit messages based on current state (pregame or postgame),
 * handles remaining client machines, disconnects, clears the server struct,
 * and resets the in-use flag. */
void network_game_client_dispose(void *server)
{
  char *s;
  int16_t state;
  void *msg;
  const char *log_msg;

  s = (char *)server;
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x120, 1);
    system_exit(-1);
  }
  state = *(int16_t *)(s + 4);
  if (state == 0) {
    msg = FUN_0012b700(9, &server, 4);
    if (!msg) {
      log_msg = "failed to create a "
                "_message_type_server_graceful_game_exit_pregame message";
    } else {
      goto broadcast;
    }
  } else if (state == 2) {
    msg = FUN_0012b700(0x1f, &server, 4);
    if (msg) {
      goto broadcast;
    }
    log_msg = "failed to create a "
              "_message_type_server_graceful_game_exit_postgame message";
  } else {
    goto skip_message;
  }
  goto do_log;

broadcast:
  if (FUN_0012f430(server, msg)) {
    log_msg = "notified all clients that we are going down";
  } else {
    log_msg = "failed to notify all clients that we are going down";
  }
do_log:
  network_game_log(log_msg);

skip_message:
  if (!FUN_0012e580((int)server)) {
    error(2, "network_game_server_handle_client_machines() failed inside "
             "network_game_server_dispose()");
  }
  if (*(int *)s != 0) {
    FUN_00128d30(*(int *)s);
  }
  XSleep(1000, 0);
  FUN_00082b30();
  csmemset(server, 0, 0x4bc);
  if (!*(char *)0x46eed4) {
    display_assert("network_game_server_memory_do_not_use_directly_in_use",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x171, 1);
    system_exit(-1);
  }
  *(char *)0x46eed4 = 0;
  network_game_log("network server disposed");
}

/* Main server tick function (0x12eb20).
 * Verifies network connectivity, validates the game, accepts new client
 * connections, handles the public endpoint, processes client machines,
 * and dispatches based on server state (pregame/ingame/postgame). */
bool network_game_server_start(void *server)
{
  char *s;
  bool result;
  int new_conn;
  int16_t state;
  char addr_buf[24];
  const char *addr_str;

  result = true;
  if (!FUN_00082300()) {
    if (!FUN_0012a170()) {
      display_error_when_main_menu_loaded(6);
      error(2, "network connection went down!");
      return false;
    }
  }

  s = (char *)server;
  if (!FUN_0012c160(server)) {
    network_game_log("the server's game is invalid");
  } else {
    new_conn = 0;
    result = FUN_00129cf0(*(int *)s, 0, &new_conn);
    if (result != 1) {
      network_game_log("network_connection_idle() failed");
      return result;
    }
    if (new_conn != 0) {
      if (FUN_0012d880((int)server, new_conn)) {
        FUN_001283c0(new_conn, addr_buf, 0);
        addr_str = FUN_00081b90(addr_buf);
        network_game_log("new client connected from ip %s (validation pending)",
                         addr_str);
      } else {
        network_game_log("failed to add new client connection to the game");
        FUN_00129130(*(int *)s, new_conn);
      }
    }
    if (!FUN_0012d9f0((int)server)) {
      network_game_log("network_game_server_handle_public_endpoint() failed");
      return false;
    }
    if (!FUN_0012e580((int)server)) {
      network_game_log("network_game_server_handle_client_machines() failed");
      return false;
    }
    state = *(int16_t *)(s + 4);
    if (state == 0) {
      return FUN_0012e750((int)server);
    }
    if (state == 1) {
      return result;
    }
    if (state == 2) {
      return FUN_0012db60((int)server);
    }
    network_game_log("unknown server state");
    return false;
  }
  return result;
}

/* Reset server to pregame state (0x12eca0).
 * If already in postgame, sends pregame reset message, toggles client
 * team assignments, clears per-machine flags, reinitializes game settings,
 * and attempts to start a new game cycle. */
bool network_server_manager_pregame_start(void *server)
{
  char *s;
  bool result;
  int i;
  char *client_ptr;
  char *flags_ptr;
  void *msg;
  char local_buf[0x434];

  s = (char *)server;
  result = false;
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x324, 1);
    system_exit(-1);
  }
  csmemset(s + 0x488, 0, 0x10);
  *(int *)(s + 0x47c) = 0;
  *(int *)(s + 0x484) = 0;
  *(char *)(s + 0x4b9) = 0;
  *(char *)(s + 0x4b8) = 0;
  *(int *)(s + 0x434) = *(int *)(s + 0x434) + 1;

  if (*(int16_t *)(s + 4) != 2) {
    result = FUN_0012dc20((int)server);
    if (*(char *)(s + 0xc8) != 0) {
      client_ptr = s + 0x24c;
      for (i = 0x10; i != 0; i--) {
        if (FUN_0012ac80(client_ptr - 0x1e)) {
          if (*client_ptr == 0) {
            *client_ptr = 1;
          } else if (*client_ptr == 1) {
            *client_ptr = 0;
          }
        }
        client_ptr += 0x20;
      }
    }
    return result;
  }

  /* Postgame → pregame transition */
  {
    int data = 0;
    msg = FUN_0012b700(0x1e, &data, 4);
  }
  if (!msg || !FUN_0012f430(server, msg)) {
    network_game_log(
      "failed to signal all client machines to switch to pregame");
    return false;
  }
  network_game_log("server resetting to pregame");

  /* Toggle client team assignments */
  if (*(char *)(s + 0xc8) != 0) {
    client_ptr = s + 0x24c;
    for (i = 0x10; i != 0; i--) {
      if (FUN_0012ac80(client_ptr - 0x1e)) {
        if (*client_ptr == 0) {
          *client_ptr = 1;
        } else if (*client_ptr == 1) {
          *client_ptr = 0;
        }
      }
      client_ptr += 0x20;
    }
  }

  /* Clear per-machine state flags */
  flags_ptr = s + 0x44a;
  for (i = 4; i != 0; i--) {
    *flags_ptr &= 0xfb;
    *(int *)(flags_ptr - 0xa) = 0;
    *(int *)(flags_ptr - 0x6) = 0;
    flags_ptr += 0x10;
  }

  FUN_0012abc0(s + 8, 0);

  if (FUN_0012dc20((int)server)) {
    csmemcpy(local_buf, s + 8, 0x434);
    msg = FUN_0012b700(6, local_buf, 0x434);
    if (msg && FUN_0012f430(server, msg)) {
      *(int16_t *)(s + 4) = 0;
      return true;
    }
    return result;
  }

  /* Playlist ended — send graceful exit */
  {
    int data = 0;
    msg = FUN_0012b700(9, &data, 4);
    if (msg && FUN_0012f430(server, msg) && FUN_0012e580((int)server)) {
      network_game_log("the playlist has ended - server going down");
      return result;
    }
  }
  network_game_log("the playlist has ended - server going down, but failed to "
                   "alert client machines");
  return result;
}

/* Broadcast a message to all connected client machines (0x12f430).
 * Iterates 4 machine slots, checks each is valid and alive, then copies
 * and sends the message. Returns false if any write fails. */
bool FUN_0012f430(void *server, void *message)
{
  char local_buf[0x600];
  bool result;
  int i;
  unsigned short msg_len;
  int machine;
  int connection;

  result = true;
  if (!server || !message) {
    display_assert(
      "server && message",
      "c:\\halo\\SOURCE\\networking\\network_server_message_handler.c", 0x187,
      1);
    system_exit(-1);
  }

  msg_len = *(unsigned short *)message >> 4;

  for (i = 0; i < 4; i++) {
    machine = FUN_0012d450((int)server, i);
    if (!FUN_0012c500((int)server, machine))
      continue;
    connection = FUN_0012d3b0((void *)machine);
    if (!connection)
      continue;
    if (!FUN_00128660(connection))
      continue;
    if (msg_len > 0x600) {
      display_assert(
        "message_length<=sizeof(message_buffer)",
        "c:\\halo\\SOURCE\\networking\\network_server_message_handler.c", 0x19a,
        1);
      system_exit(-1);
    }
    csmemcpy(local_buf, message, msg_len);
    if (!FUN_00128e00((void *)connection, local_buf, msg_len, 0, 1)) {
      network_game_log("network_game_server_write() failed in "
                       "network_game_server_send_message_to_all_machines()");
      result = false;
    }
  }

  return result;
}
