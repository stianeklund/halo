/* Tick a millisecond countdown timer. Subtracts elapsed time from
   time_remaining, clamps to zero, and returns the remaining value.
   countdown[0] = time_remaining, countdown[1] = last_tick_time. */
int FUN_0012bdb0(void *countdown)
{
  int now;
  int elapsed;
  int remaining;
  int *timer = (int *)countdown;

  now = system_milliseconds();
  if (now > timer[1]) {
    elapsed = now - timer[1];
    if (elapsed < timer[0]) {
      timer[0] = timer[0] - elapsed;
    } else {
      timer[0] = 0;
    }
  }
  remaining = timer[0];
  timer[1] = now;
  assert_halt(remaining >= 0);
  return remaining;
}

/* Open the server's game (0x12c060).
 * Sets bit 0 of the flags byte at server+6 (marking the game as open),
 * then tells the underlying connection to open, and logs "opening game". */
void network_game_server_open_game(void *server)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x1fc, 1);
    system_exit(-1);
  }
  *(uint8_t *)((char *)server + 6) |= 1;
  network_server_allow_client_connections(*(int *)server, 1);
  network_game_log("opening game");
}

/* Close the server's game (0x12c0b0).
 * Clears bit 0 of the flags byte at server+6 (marking the game as closed),
 * then tells the underlying connection to close, and logs "closing game". */
void network_game_server_close_game(void *server)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x208, 1);
    system_exit(-1);
  }
  *(uint8_t *)((char *)server + 6) &= ~1;
  network_server_allow_client_connections(*(int *)server, 0);
  network_game_log("closing game");
}

/* Check if the server's game is open (0x12c100).
 * Returns bit 0 of the flags byte at server+6, set by
 * network_game_server_open_game and cleared by network_game_server_close_game.
 */
bool network_game_server_game_is_open(void *server)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x214, 1);
    system_exit(-1);
  }
  return *(uint8_t *)((char *)server + 6) & 1;
}

/* Check if the server's game is valid (0x12c160).
 * Returns bit 1 of the flags byte at server+6. */
bool network_game_server_game_is_valid(void *server)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x220, 1);
    system_exit(-1);
  }
  return (*(uint8_t *)((char *)server + 6) >> 1) & 1;
}

/* Handle a client player-removal request while in-game (0x12c1c0).
 * Asserts the server is in state 1 (in-game). Iterates the 16 client
 * entries at server+0x22e (stride 0x20). For each active client whose
 * machine_index byte (+0x1c) matches the machine slot's index (+0xc),
 * copies 0x20 bytes of client data, appends a quit time, and broadcasts
 * a type-0x16 message to all machines. */
void FUN_0012c1c0(int server, int client)
{
  char *s = (char *)server;
  char *ptr;
  char local_buf[0x24];
  int quit_time;
  void *msg;
  int i;

  if (*(short *)(s + 4) != 1) {
    display_assert("_network_game_server_state_ingame == server->state",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x267, 1);
    system_exit(-1);
  }

  ptr = s + 0x22e;
  for (i = 0x10; i != 0; i--) {
    if (network_player_is_valid(ptr)) {
      if ((short)*(signed char *)(ptr + 0x1c) ==
          *(short *)((char *)client + 0xc)) {
        csmemcpy(local_buf, ptr, 0x20);
        quit_time = game_time_get() + 0x21;
        *(int *)(local_buf + 0x20) = quit_time;
        error(2, "sending quit out of game, time = %x", quit_time);
        msg = encode_network_game_message(0x16, local_buf, 0x24);
        if (msg) {
          if (!FUN_0012f430((void *)server, msg)) {
            network_game_log(
              "network_game_server_send_message_to_all_machines() failed in "
              "network_game_server_handle_message_client_remove_player_"
              "request_ingame()");
          }
        }
      }
    }
    ptr += 0x20;
  }
}

/* Signal client machines to begin loading for a network game (0x12c290).
 * Copies server game-variant data at server+8 (0x434 bytes) into a local
 * buffer, builds a type-6 message from it and broadcasts it, then builds
 * a type-8 message with a zero payload and broadcasts that too.  On
 * success sets server+0x4b9 (loading flag) to 1.  Always clears
 * server+0x47c and always returns true regardless of success or failure. */
bool FUN_0012c290(void *server)
{
  int data;
  void *msg;
  char local_buf[0x434];

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x2de, 1);
    system_exit(-1);
  }
  if (*(char *)((char *)server + 0x4b9) == 0) {
    data = 0;
    csmemcpy(local_buf, (char *)server + 8, 0x434);
    msg = encode_network_game_message(6, local_buf, 0x434);
    if (msg && FUN_0012f430(server, msg)) {
      msg = encode_network_game_message(8, &data, 4);
      if (msg && FUN_0012f430(server, msg)) {
        network_game_log(
          "signalling client machines to begin loading for network game");
        *(int *)((char *)server + 0x47c) = 0;
        *(char *)((char *)server + 0x4b9) = 1;
        return true;
      }
    }
    network_game_log(
      "failed to signal client machines to begin loading for network game");
  }
  *(int *)((char *)server + 0x47c) = 0;
  return true;
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
    msg = encode_network_game_message(0x17, &data, 4);
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

/* Check if a machine is marked as valid/active on this server (0x12c500).
 * Asserts both server and machine are non-null, then returns bit 1 of the
 * flags byte at machine+0xe (shifted right by 1, masked to a bool). */
bool FUN_0012c500(int server, int machine)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x3cd, 1);
    system_exit(-1);
  }
  if (!machine) {
    display_assert("machine",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x3ce, 1);
    system_exit(-1);
  }
  return (*(uint8_t *)((char *)machine + 0xe) >> 1) & 1;
}

/* Finalize server loading after all machines have loaded (0x12caa0).
 * Sets the server state to 1, clears the timer at +0x484, then copies a
 * "local game data loaded" flag from the client's game-data region into
 * server+0x438.  Asserts if the flag is zero (data not loaded). */
void FUN_0012caa0(void *server)
{
  char *s = (char *)server;
  void *client;
  void *game_data;
  uint8_t loaded;

  network_game_log("all machines have successfully loaded");
  *(int16_t *)(s + 0x4) = 1;
  *(int32_t *)(s + 0x484) = 0;

  client = network_game_client_get();
  if (client != NULL) {
    game_data =
      network_game_client_get_machine_index(network_game_client_get());
    loaded = *(uint8_t *)((char *)game_data + 0x430);
  } else {
    loaded = 0;
  }

  *(uint8_t *)(s + 0x438) = loaded;
  if (!loaded) {
    display_assert("local game data not loaded",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x4e0, 1);
    system_exit(-1);
  }
}

/* Check whether any team (0 or 1) has zero active clients among the 16 client
 * slots at server+0x22E..+0x44C (stride 0x20).  Returns true when at least one
 * team is empty, false when both teams have members (0x12d040). */
bool get_unique_random_name(void *server)
{
  char *s;
  char *client_ptr;
  int16_t counts[2];
  signed char team;
  int i, j;

  s = (char *)server;
  if (*(char *)(s + 0xc8) == 0)
    return false;

  counts[0] = 0;
  counts[1] = 0;
  client_ptr = s + 0x24c;
  for (i = 0x10; i != 0; i--) {
    if (network_player_is_valid(client_ptr - 0x1e)) {
      team = *client_ptr;
      if (team >= 0 && team < 2)
        counts[team]++;
    }
    client_ptr += 0x20;
  }

  for (j = 0; j < 2; j++) {
    if (counts[j] == 0)
      return true;
  }
  return false;
}

/* Check that every machine slot with a valid team index has at least one
 * active client on that team (0x12d0c0). Iterates the 4 machine slots at
 * server+0x448 (stride 0x10) and for each valid slot, searches the 16 client
 * entries at server+0x22e (stride 0x20) for a matching team byte. Returns
 * false if any valid slot has no matching active client. */
bool get_unique_random_color(void *server)
{
  char *s = (char *)server;
  short *slot = (short *)(s + 0x448);
  int i, j;

  for (i = 0; i < 4; i++) {
    if (*slot >= 0 && *slot < 4) {
      bool found = false;
      char *client_ptr = s + 0x24a;
      for (j = 0x10; j != 0; j--) {
        if (network_player_is_valid(client_ptr - 0x1c)) {
          if ((signed char)*client_ptr == *slot)
            found = true;
        }
        client_ptr += 0x20;
      }
      if (!found)
        return false;
    }
    slot = (short *)((char *)slot + 0x10);
  }
  return true;
}

/* Check whether enough machine slots have valid team indices (0x12d150).
 * Counts slots in [0,4) across the 4 machine entries at server+0x448
 * (stride 0x10). If FUN_0012a170 returns true the threshold is 1,
 * otherwise 2. Returns count >= threshold. */
bool FUN_0012d150(void *server)
{
  char *s = (char *)server;
  int threshold = FUN_0012a170() ? 1 : 2;
  int count = 0;

  if (*(int16_t *)(s + 0x448) >= 0 && *(int16_t *)(s + 0x448) < 4)
    count++;
  if (*(int16_t *)(s + 0x458) >= 0 && *(int16_t *)(s + 0x458) < 4)
    count++;
  if (*(int16_t *)(s + 0x468) >= 0 && *(int16_t *)(s + 0x468) < 4)
    count++;
  if (*(int16_t *)(s + 0x478) >= 0 && *(int16_t *)(s + 0x478) < 4)
    count++;

  return count >= threshold;
}

/* Return the connection handle from a machine struct (0x12d3b0).
 * Returns the first dword at machine+0, or 0 if machine is NULL. */
int network_game_server_adjust_machine_settings(void *machine)
{
  if (machine != NULL)
    return *(int *)machine;
  return 0;
}

/* Get a pointer to the machine entry at the given index (0x12d450).
 * Asserts server is non-null and index < MAXIMUM_NETWORK_MACHINE_COUNT (4).
 * Each machine entry is 0x10 bytes, starting at server+0x43c. */
int FUN_0012d450(int server, int machine_index)
{
  if (!server || machine_index >= 4) {
    display_assert("server && (index<MAXIMUM_NETWORK_MACHINE_COUNT)",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x741, 1);
    system_exit(-1);
  }
  return machine_index * 0x10 + 0x43c + server;
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

  if (!network_game_server_game_is_open((void *)server)) {
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
        if (!network_game_accept_remote_connections() &&
            *(int *)addr_buf != 0x7f000001) {
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
    FUN_0012f430((void *)server, encode_network_game_message(0xb, &data, 2));
    *(unsigned int *)((char *)server + 0x480) = now;
  }
  return true;
}

/* Check if all connected machines have finished precaching (0x12dbb0).
 * Iterates machine slots at server+0x448 (stride 0x10, 4 max).
 * A machine is "valid" if its short at +0 is in [0,3].
 * If a valid machine has bit 3 of byte at +2 clear, returns false (not done).
 * If all valid machines have bit 3 set, asserts that the map is loaded and
 * returns true. */
bool FUN_0012dbb0(int server)
{
  int i;
  char *slot;

  i = 0;
  slot = (char *)server + 0x448;
  while (i < 4) {
    short conn = *(short *)slot;
    if (conn >= 0 && conn < 4) {
      if (((*(uint8_t *)(slot + 2) >> 3) & 1) == 0)
        return false;
    }
    i++;
    slot += 0x10;
  }
  if (!cache_files_precache_map_loaded(main_get_multiplayer_map_name())) {
    display_assert(
      "!all_machines_have_precached || "
      "cache_files_precache_map_loaded(main_get_multiplayer_map_name())",
      "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x8c8, 1);
    system_exit(-1);
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

  if (!game_engine_get_current_stage(s + 0xac, s + 0x2c)) {
    error(2, "network game setup failed; probably due to a missing playlist");
    return false;
  }

  csmemcpy(name_buf, (void *)0x281c38, 20);
  csmemset(name_buf + 20, 0, 44);
  network_game_generate_local_machine_name(name_buf);
  ustrncpy((wchar_t *)(s + 8), (wchar_t *)name_buf, 0xf);

  *(short *)(s + 0x26) = 0;
  *(int *)(s + 0x28) = 0;
  *(char *)(s + 0x115) = 2;
  *(char *)(s + 0x116) = 0x10;
  *(char *)(s + 0x117) = (*(char *)(s + 0xc8) != 0) + 1;

  network_game_server_open_game((void *)server);

  return true;
}

/* Dump network game data fields to the log with a prefix (0x12dd20).
 * Prints machine_count, 4 machine slots (stride 0x44 from game_data+0x154),
 * player_count, 16 player entries (stride 0x20 from game_data+0x226),
 * random seed, and games played. */
void FUN_0012dd20(void *game_data, const char *prefix)
{
  char *s = (char *)game_data;
  char *p;
  int i;

  network_game_log("%snetwork_game_data", prefix);
  network_game_log("%smachine_count %d", prefix, (int)*(int16_t *)(s + 0x112));

  p = s + 0x154;
  for (i = 0; i < 4; i++) {
    network_game_log("\t%smachine %d %x", prefix, i, (int)*(signed char *)p);
    p += 0x44;
  }

  network_game_log("%splayer_count %d", prefix, (int)*(int16_t *)(s + 0x224));

  p = s + 0x243;
  for (i = 0; i < 0x10; i++) {
    network_game_log("%splayer %d", prefix, i);
    network_game_log("%s\tmachine_index %x", prefix,
                     (int)*(signed char *)(p - 1));
    network_game_log("%s\tcontroller_index %x", prefix, (int)*(signed char *)p);
    network_game_log("%s\tteam_index %x", prefix, (int)*(signed char *)(p + 1));
    network_game_log("%s\tplayer_list_index %x", prefix,
                     (int)*(signed char *)(p + 2));
    p += 0x20;
  }

  network_game_log("%snetwork_game_random_seed %x", prefix,
                   *(int *)(s + 0x428));
  network_game_log("%snumber_of_games_played %d", prefix, *(int *)(s + 0x42c));
}

/* Dump the full server state to the network game log for debugging (0x12de20).
 * Prints connection, state, flags, game data, all 4 client machine slots
 * (connection, update sequence, stall time, machine index, flags), and
 * timing fields. */
void FUN_0012de20(void *server)
{
  int i;
  char *slot;
  const char *status;

  network_game_log("*************BEGIN*************");
  network_game_log("\tconnection %x", *(int *)server);
  network_game_log("\tstate %x", (int)*(uint16_t *)((char *)server + 4));
  network_game_log("\tflags %x", (int)*(uint16_t *)((char *)server + 6));
  FUN_0012dd20((char *)server + 8, "\t");
  network_game_log("client_machines:");

  i = 0;
  slot = (char *)server + 0x444;
  do {
    status = "no connection";
    if (*(int *)(slot - 8) != 0) {
      if (FUN_00128660(*(int *)(slot - 8)))
        status = "(active)";
      else
        status = "(dead)";
    }
    network_game_log("\tclient %d", i);
    network_game_log("\t\tconnection %x %s", *(int *)(slot - 8), status);
    network_game_log("\t\tlast_received_update_sequence_number %d",
                     *(int *)(slot - 4));
    network_game_log("\t\tstall_start_time %d", *(int *)slot);
    network_game_log("\t\tmachine_index %x", (int)*(short *)(slot + 4));
    network_game_log("\t\tflags %x", (int)*(uint16_t *)(slot + 6));
    i++;
    slot += 0x10;
  } while (i < 4);

  network_game_log("\tnext_update_number %d", *(int *)((char *)server + 0x47c));
  network_game_log("\ttime_of_last_keep_alive %d",
                   *(int *)((char *)server + 0x480));
  network_game_log("\ttime_of_first_client_loading_completion %d",
                   *(int *)((char *)server + 0x484));
  network_game_log("*************END*************");
}

/* Remove a client machine from the server's game (0x12df50).
 * If the server is in-game (state 1), broadcasts a player-removal message.
 * Finds and removes the machine's entry from game data (server+8), then
 * clears the matching machine slot (connection, stall, flags) and sets the
 * machine_index to -1. Returns true if the slot was found. */
bool FUN_0012df50(void *server, void *machine)
{
  char *s = (char *)server;
  char *m = (char *)machine;
  char *ptr;
  int i;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x22f, 1);
    system_exit(-1);
  }
  if (!machine) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x230, 1);
    system_exit(-1);
  }

  if (*(short *)(s + 4) == 1)
    FUN_0012c1c0((int)server, (int)machine);

  ptr = s + 0x15c;
  for (i = 0; i < 4; i++) {
    if ((short)*(signed char *)ptr == *(short *)(m + 0xc)) {
      if (!FUN_0012b500((int)(s + 8), (int)(s + 0x11c + i * 0x44))) {
        error(
          2, "network_game_server_remove_client_machine_from_game() failed to "
             "remove the offending machine from the server's copy of the game");
      }
      break;
    }
    ptr += 0x44;
  }

  for (i = 0; i < 4; i++) {
    if (s + 0x43c + i * 0x10 == m) {
      if (*(int *)(s + 0x43c + i * 0x10) != 0) {
        if (!FUN_00129130(*(int *)s, *(int *)(s + 0x43c + i * 0x10)))
          network_game_log("server failed to close a client's connection");
      }
      *(int *)(s + 0x440 + i * 0x10) = 0;
      *(int *)(s + 0x43c + i * 0x10) = 0;
      *(int *)(s + 0x444 + i * 0x10) = 0;
      *(uint16_t *)(s + 0x44a + i * 0x10) = 0;
      *(short *)(s + 0x448 + i * 0x10) = -1;
      return true;
    }
  }

  network_game_log(
    "network_game_server_remove_client_machine_from_game() failed to find "
    "the specified machine");
  return false;
}

/* Remove a machine from the server's game by its machine_index (0x12e090).
 * Validates the machine_index byte at player_data+0x40. If valid (0..3),
 * searches the 4 machine slots at server+0x448 (stride 0x10) for a matching
 * index, calls FUN_0012df50 to remove it, then FUN_0012b500 to remove
 * the machine from game data. If the server state is 0 (pre-game),
 * sends updated settings to remaining clients. */
bool FUN_0012e090(void *server, void *player_data)
{
  char *s = (char *)server;
  char *pd = (char *)player_data;
  signed char machine_idx;
  short *ptr;
  bool result;
  int i;

  result = false;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x299, 1);
    system_exit(-1);
  }

  if (*(signed char *)(pd + 0x40) == -1) {
    network_game_log(
      "network_game_server_remove_machine_from_game called with a "
      "machine_index of NONE");
  }

  machine_idx = *(signed char *)(pd + 0x40);
  if (machine_idx < 0 || machine_idx > 3) {
    network_game_log("attempted to remove an invalid machine from the game in "
                     "network_game_server_remove_machine_from_game()");
    network_game_log("machine name = <not implemented>");
    network_game_log("machine index = %x", (int)*(signed char *)(pd + 0x40));
    FUN_0012de20(server);
    return result;
  }

  ptr = (short *)(s + 0x448);
  for (i = 0; i < 4; i++) {
    if (*ptr == (short)machine_idx) {
      result = FUN_0012df50(server, (void *)(s + 0x43c + i * 0x10));
      if (!result) {
        network_game_log(
          "network_game_server_remove_client_machine_from_game() failed in "
          "network_game_server_remove_machine_from_game()");
      }
      break;
    }
    ptr += 8;
    result = false;
  }

  if (i == 4) {
    network_game_log(
      "network_game_server_remove_machine_from_game() failed to find the "
      "specified machine");
  }

  if (*(signed char *)(pd + 0x40) != -1) {
    result = FUN_0012b500((int)(s + 8), (int)player_data);
    if (!result) {
      network_game_log("network_game_remove_machine() failed in "
                       "network_game_server_remove_machine_from_game()");
    }
  }

  if (*(short *)(s + 4) == 0) {
    if (!FUN_0012f5d0(server)) {
      network_game_log(
        "network_game_server_remove_machine_from_game() failed to send "
        "updated game settings to remaining clients");
      return result;
    }
  }

  return result;
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

    if (!network_connection_connected(*(int *)machine))
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
        if (wide_to_ascii(
              (const wchar_t *)(s + 0x11c +
                                (int)*(short *)(flags_ptr - 1) * 0x44),
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
    FUN_0012f430((void *)server,
                 encode_network_game_message(0xa, &countdown, 2));
    *(int *)(s + 0x480) = now;
    return result;
  }

  if (!FUN_0012d150((void *)server) ||
      !get_unique_random_color((void *)server) ||
      get_unique_random_name((void *)server) ||
      *(short *)(s + 0x22c) < (short)*(char *)(s + 0x115)) {
    csmemset(s + 0x488, 0, 0x10);
    i = 0;
  } else {
    i = 1;
    timer_ms = FUN_0012bdb0(s + 0x488);
    if (timer_ms == 0) {
      if (FUN_0012dbb0(server) && *(char *)(s + 0x495) == 0) {
        network_game_server_close_game((void *)server);
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
    void *msg = encode_network_game_message(7, &countdown, 2);
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
    msg = encode_network_game_message(9, &server, 4);
    if (!msg) {
      log_msg = "failed to create a "
                "_message_type_server_graceful_game_exit_pregame message";
    } else {
      goto broadcast;
    }
  } else if (state == 2) {
    msg = encode_network_game_message(0x1f, &server, 4);
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
    network_connection_delete(*(int *)s);
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
  if (!network_game_server_game_is_valid(server)) {
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
        if (network_player_is_valid(client_ptr - 0x1e)) {
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
    msg = encode_network_game_message(0x1e, &data, 4);
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
      if (network_player_is_valid(client_ptr - 0x1e)) {
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
    msg = encode_network_game_message(6, local_buf, 0x434);
    if (msg && FUN_0012f430(server, msg)) {
      *(int16_t *)(s + 4) = 0;
      return true;
    }
    return result;
  }

  /* Playlist ended — send graceful exit */
  {
    int data = 0;
    msg = encode_network_game_message(9, &data, 4);
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
    connection = network_game_server_adjust_machine_settings((void *)machine);
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

/* Send updated game settings to all client machines (0x12f5d0).
 * Gets the game data pointer via FUN_0012d570, copies 0x434 bytes
 * into a local buffer, builds a type-6 message, and broadcasts it.
 * Returns true on success. */
bool FUN_0012f5d0(void *server)
{
  char local_buf[0x434];
  int game_data;
  void *msg;
  bool result;

  result = false;
  if (!server) {
    display_assert(
      "server",
      "c:\\halo\\SOURCE\\networking\\network_server_message_handler.c", 0x1c8,
      1);
    system_exit(-1);
  }

  game_data = FUN_0012d570(server);
  if (game_data == 0) {
    network_game_log(
      "failed to handle a message_server_game_settings_update because their "
      "was no server game");
    return result;
  }

  csmemcpy(local_buf, (void *)game_data, 0x434);
  msg = encode_network_game_message(6, local_buf, 0x434);
  if (!msg) {
    network_game_log(
      "failed to create a message_server_game_settings_update message");
    return false;
  }

  result = FUN_0012f430(server, msg);
  if (!result) {
    network_game_log(
      "failed to send message_server_game_settings_update message to all "
      "machines");
    return false;
  }

  return result;
}
