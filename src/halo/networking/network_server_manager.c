typedef struct network_game_server_game {
  uint8_t field_00[0x10d];
  int8_t minimum_players;
  uint8_t field_10e[0x06];
  uint8_t machines[4][0x44];
  int16_t player_count;
  uint8_t field_226[0x20e];
} network_game_server_game;

typedef struct network_game_server_client_machine {
  int connection;
  uint32_t last_received_update_sequence_number;
  uint32_t stall_start_time;
  int16_t machine_index;
  uint16_t flags;
} network_game_server_client_machine;

enum network_client_machine_flag_bit {
  NETWORK_CLIENT_MACHINE_CONNECTED_BIT = 0,
  NETWORK_CLIENT_MACHINE_VALIDATED_BIT = 1,
  NETWORK_CLIENT_MACHINE_LEVEL_LOADED_BIT = 2,
  NETWORK_CLIENT_MACHINE_PRECACHED_BIT = 3
};
#define NETWORK_CLIENT_MACHINE_FLAG(bit) (1u << (bit))

/* Layout evidence: recovery/evidence/countdown_timer.json. */
typedef struct countdown_timer {
  int32_t time_remaining; /* 0x00: 2276 assert names this field */
  uint32_t field_04;      /* 0x04: PAL calls this last_update_time */
} countdown_timer;
cs(countdown_timer, 0x08);
co(countdown_timer, time_remaining, 0x00);
co(countdown_timer, field_04, 0x04);

typedef struct network_game_server_countdown_state {
  int timer[2];
  int last_countdown_message_time;
  uint8_t active;
  uint8_t paused;
  uint8_t adjusted_time_this_tick;
  uint8_t pad_0f;
} network_game_server_countdown_state;

typedef struct network_game_server {
  int connection;
  uint16_t state;
  uint16_t flags;
  network_game_server_game game;
  network_game_server_client_machine client_machines[4];
  int next_update_number;
  int32_t time_of_last_keep_alive;
  uint32_t time_of_first_client_loading_completion;
  network_game_server_countdown_state countdown_state;
  uint8_t queued_player[0x20];
  uint8_t queued_player_valid;
  uint8_t sent_start_game_message;
  uint8_t pad_4ba[2];
} network_game_server;

/* server->state: 2276 network_game_server_idle switch at 0x12ec01. */
#define NETWORK_GAME_SERVER_STATE_PREGAME 0
#define NETWORK_GAME_SERVER_STATE_INGAME 1
#define NETWORK_GAME_SERVER_STATE_POSTGAME 2

cs(network_game_server_game, 0x434);
co(network_game_server_game, minimum_players, 0x10d);
co(network_game_server_game, machines, 0x114);
co(network_game_server_game, player_count, 0x224);
cs(network_game_server_client_machine, 0x10);
co(network_game_server_client_machine, machine_index, 0x0c);
co(network_game_server_client_machine, flags, 0x0e);
cs(network_game_server_countdown_state, 0x10);
co(network_game_server_countdown_state, last_countdown_message_time, 0x08);
co(network_game_server, game, 0x08);
co(network_game_server, client_machines, 0x43c);
co(network_game_server, countdown_state, 0x488);
co(network_game_server, sent_start_game_message, 0x4b9);
cs(network_game_server, 0x4bc);

/* Decode a network game message from an encoded buffer.
 * Returns true on success; logs an error on failure.
 * 0x12bce0 / network_server_manager.obj */
bool decode_network_game_message(int message_struct, int encoded_message,
                                 short *encoded_message_size, short *packet_type,
                                 short *packet_version, int expected_packet_class)
{
  bool result;

  if ((((message_struct == 0 || encoded_message == 0) || encoded_message_size == (short *)0) ||
       (*encoded_message_size <= 0 || packet_type == (short *)0)) ||
      (*packet_type < 0 || (packet_version == (short *)0 || *packet_version <= 0))) {
    display_assert(
      "message_struct && encoded_message && encoded_message_size && "
      "(*encoded_message_size>0) && packet_type && (*packet_type>=0) && "
      "packet_version && (*packet_version>0)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0x139, 1);
    system_exit(-1);
  }
  result = data_packet_group_decode_packet(
    (int)&s_network_game_messages_group, (void *)message_struct, (char *)encoded_message,
    encoded_message_size, packet_type, packet_version,
    (short)expected_packet_class);
  if (!result) {
    network_event("decode_network_game_message() failed");
  }
  return result;
}

/* Update a countdown timer without reading its value.
 * countdown[0] = time_remaining, countdown[1] = last_tick_time.
 * 0x12bd80 / network_server_manager.obj */
void countdown_timer_update(int *param_1)
{
  int now;
  int old;
  int elapsed;

  now = system_milliseconds();
  old = param_1[1];
  if (now > old) {
    elapsed = now - old;
    if (elapsed < param_1[0]) {
      param_1[0] -= elapsed;
    } else {
      param_1[0] = 0;
    }
  }
  param_1[1] = now;
}

/* Tick a millisecond countdown timer. Subtracts elapsed time from
   time_remaining, clamps to zero, and returns the remaining value.
   countdown[0] = time_remaining, countdown[1] = last_tick_time. */
__declspec(noinline) int countdown_timer_get_time_remaining(void *countdown)
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
  assert_halt_msg_at("timer->time_remaining >= 0",
                     "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                     0x5f, timer[0] >= 0);
  return remaining;
}

/* countdown_timer_increment — 0x12be10
 * Ticks the timer forward, then adds param_2 ms clamped to [0, param_3]. */
void countdown_timer_increment(int *param_1, int param_2, int param_3)
{
  int iVar1;
  int iVar2;
  iVar1 = system_milliseconds();
  if (iVar1 > param_1[1]) {
    iVar2 = (int)((unsigned int)iVar1 - (unsigned int)param_1[1]);
    if (iVar2 < *param_1) {
      *param_1 = (int)((unsigned int)*param_1 - (unsigned int)iVar2);
    } else {
      *param_1 = 0;
    }
  }
  param_1[1] = iVar1;
  if (param_2 < 0) {
    display_assert("adjustment >= 0",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x68, 1);
    system_exit(-1);
  }
  iVar1 = (int)((unsigned int)*param_1 + (unsigned int)param_2);
  if (iVar1 < param_2) {
    *param_1 = param_3;
  } else {
    *param_1 = (int)((unsigned int)*param_1 + (unsigned int)param_2);
    *param_1 = *param_1 <= param_3 ? *param_1 : param_3;
  }
  if (*param_1 < 0) {
    display_assert("timer->time_remaining >= 0",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x75, 1);
    system_exit(-1);
  }
}

/* countdown_timer_decrement — 0x12bea0
 * Ticks the timer forward, then subtracts param_2 ms (floor at 0). */
void countdown_timer_decrement(int *param_1, int param_2)
{
  int iVar1;
  int iVar2;
  iVar1 = system_milliseconds();
  if (iVar1 > param_1[1]) {
    iVar2 = (int)((unsigned int)iVar1 - (unsigned int)param_1[1]);
    if (iVar2 < *param_1) {
      *param_1 = (int)((unsigned int)*param_1 - (unsigned int)iVar2);
    } else {
      *param_1 = 0;
    }
  }
  param_1[1] = iVar1;
  if (param_2 < 0) {
    display_assert("adjustment >= 0",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x7e, 1);
    system_exit(-1);
  }
  if (*param_1 > param_2) {
    iVar2 = (int)((unsigned int)*param_1 - (unsigned int)param_2);
    *param_1 = iVar2;
  } else {
    *param_1 = 0;
  }
  if (*param_1 < 0) {
    display_assert("timer->time_remaining >= 0",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x89, 1);
    system_exit(-1);
  }
}

/* countdown_timer_set_time_remaining — 0x12bf30
 * Resets the timer to param_2 ms and records the current tick time. */
void countdown_timer_set_time_remaining(int *param_1, int param_2)
{
  int iVar1;
  iVar1 = system_milliseconds();
  *param_1 = param_2;
  param_1[1] = iVar1;
  if (param_2 < 0) {
    display_assert("timer->time_remaining >= 0",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x95, 1);
    system_exit(-1);
  }
}

/* network_game_server_set_game_name — 0x12bf70
 * Copies up to 15 wchars from param_2 into server->game_name (server+8),
 * then zeros the trailing terminator at server+0x26. Returns '\0'. */
char network_game_server_set_game_name(int param_1, int param_2)
{
  if (param_1 == 0) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x1dd, 1);
    system_exit(-1);
  }
  if (param_2 == 0) {
    display_assert("name",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x1de, 1);
    system_exit(-1);
  }
  ustrncpy((wchar_t *)(param_1 + 8), (wchar_t *)param_2, 0xf);
  *(short *)(param_1 + 0x26) = 0;
  return '\0';
}

/* network_game_server_get_game_name — 0x12bfe0
 * Returns a pointer to the server's game name buffer (wchar_t at server+8). */
int network_game_server_get_game_name(int param_1)
{
  if (param_1 == 0) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x1e9, 1);
    system_exit(-1);
  }
  return param_1 + 8;
}

/* network_game_server_get_state — 0x12c020
 * Returns server->state (short at server+4). If param_2 is non-NULL,
 * zeroes *param_2 before returning. */
short network_game_server_get_state(int param_1, short *param_2)
{
  if (param_1 == 0) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x1f2, 1);
    system_exit(-1);
  }
  if (param_2 != NULL) {
    *param_2 = 0;
  }
  return *(short *)(param_1 + 4);
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
  *(uint16_t *)((char *)server + 6) |= 1;
  network_server_allow_client_connections(*(int *)server, 1);
  network_event("opening game");
}

/* Close the server's game (0x12c0b0).
 * Clears bit 0 of the flags byte at server+6 (marking the game as closed),
 * then tells the underlying connection to close, and logs "closing game". */
__declspec(noinline) void network_game_server_close_game(void *server)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x208, 1);
    system_exit(-1);
  }
  *(uint16_t *)((char *)server + 6) &= ~1;
  network_server_allow_client_connections(*(int *)server, 0);
  network_event("closing game");
}

/* Check if the server's game is open (0x12c100).
 * Returns bit 0 of the flags byte at server+6, set by
 * network_game_server_open_game and cleared by network_game_server_close_game.
 */
bool network_game_server_game_is_open(void *server)
{
  bool game_is_open;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x214, 1);
    system_exit(-1);
  }
  game_is_open = *(uint8_t *)((char *)server + 6) & 1;
  if (!((1 == game_is_open) || (0 == game_is_open))) {
    display_assert("(TRUE == game_is_open) || (FALSE == game_is_open)",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x217, 1);
    system_exit(-1);
  }
  return game_is_open;
}

/* Check if the server's game is valid (0x12c160).
 * Returns bit 1 of the flags byte at server+6. */
bool network_game_server_game_is_valid(void *server)
{
  bool game_is_valid;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x220, 1);
    system_exit(-1);
  }
  game_is_valid = (*(uint8_t *)((char *)server + 6) >> 1) & 1;
  if (!((1 == game_is_valid) || (0 == game_is_valid))) {
    display_assert("(TRUE == game_is_valid) || (FALSE == game_is_valid)",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x223, 1);
    system_exit(-1);
  }
  return game_is_valid;
}

/* Handle a client player-removal request while in-game (0x12c1c0).
 * Asserts the server is in state 1 (in-game). Iterates the 16 client
 * entries at server+0x22e (stride 0x20). For each active client whose
 * machine_index byte (+0x1c) matches the machine slot's index (+0xc),
 * copies 0x20 bytes of client data, appends a quit time, and broadcasts
 * a type-0x16 message to all machines. */
void network_game_server_remove_players_from_machine_ingame(int server, int client)
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
#if defined(_MSC_VER) && !defined(__clang__)
        memcpy(local_buf, ptr, 0x20);
#else
        csmemcpy(local_buf, ptr, 0x20);
#endif
        quit_time = game_time_get() + 0x21;
        *(int *)(local_buf + 0x20) = quit_time;
        error(2, "sending quit out of game, time = %x", quit_time);
        msg = create_network_game_message(0x16, local_buf, 0x24);
        if (msg) {
          if (!network_game_server_send_message_to_all_machines((void *)server, msg)) {
            network_event(
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
__declspec(noinline) bool network_game_server_start_network_game(void *server)
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
    msg = create_network_game_message(6, local_buf, 0x434);
    if (msg) {
      if (network_game_server_send_message_to_all_machines(server, msg)) {
        msg = create_network_game_message(8, &data, 4);
        if (msg) {
          if (network_game_server_send_message_to_all_machines(server, msg)) {
            network_event(
              "signalling client machines to begin loading for network game");
            *(char *)((char *)server + 0x4b9) = 1;
            goto set_and_return;
          }
        }
      }
    }
    network_event(
      "failed to signal client machines to begin loading for network game");
  }
set_and_return:
  *(int *)((char *)server + 0x47c) = 0;
  return true;
}


/* Signal game over to all clients (0x12c370).
 * Sets server state from in-game (1) to post-game (2) and broadcasts
 * a _message_type_server_game_over message. */
void network_game_server_switch_to_postgame(void *server)
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
    msg = create_network_game_message(0x17, &data, 4);
    if (msg) {
      if (network_game_server_send_message_to_all_machines(server, msg)) {
        network_event("server sent message_game_over to all clients");
        return;
      }
      network_event(
        "failed to signal all client machines to switch to postgame");
      return;
    }
    network_event(
      "failed to create a _message_type_server_game_over message");
  }
}

/* Send a graceful-shutdown message to all clients based on server state
 * (0x12c410). State 0 = pregame: sends message type 9; state 2 = postgame:
 * sends type 0x1f. Other states return false immediately. */
bool network_game_server_graceful_shutdown(void *server)
{
  int data;
  void *msg = NULL;
  bool result = false;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x39f, 1);
    system_exit(-1);
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x1f2, 1);
    system_exit(-1);
  }
  switch (*(uint16_t *)((char *)server + 4)) {
  case 0:
    data = 0;
    msg = create_network_game_message(9, &data, 4);
    if (!msg) {
      network_event(
        "failed to create a message_server_graceful_game_exit_pregame");
    }
    break;
  case 2:
    data = 0;
    msg = create_network_game_message(0x1f, &data, 4);
    if (!msg) {
      network_event(
        "failed to create a message_server_graceful_game_exit_postgame");
    }
    break;
  }

  if (msg) {
    result = network_game_server_send_message_to_all_machines(server, msg);
    if (result == 1) {
      network_event(
        "server closing down; all client machines were properly informed");
    } else {
      network_event(
        "server going down, but failed to properly inform all client machines");
    }
  }
  return result;
}

/* Check if a machine is marked as valid/active on this server (0x12c500).
 * Asserts both server and machine are non-null, then returns bit 1 of the
 * flags byte at machine+0xe (shifted right by 1, masked to a bool). */
__declspec(noinline) bool
network_game_server_client_machine_is_joined_to_game(int server, int machine)
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

/* Accept a client machine into the game and assign it to the next open slot
 * (0x12c560). */
char network_game_server_accept_client_machine_into_game(int server,
                                                         void *machine)
{
  network_game_server *server_data = (network_game_server *)server;
  network_game_server_client_machine *client_machine =
    (network_game_server_client_machine *)machine;
  char result;
  int i;

  result = 0;
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x3da, 1);
    system_exit(-1);
  }
  if (!machine) {
    display_assert("machine",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x3db, 1);
    system_exit(-1);
  }
  for (i = 0; i < 4; i++) {
    if ((char)server_data->game.machines[i][0x40] < 0 ||
        (char)server_data->game.machines[i][0x40] >= 4) {
      char buf[0x44];

      csmemcpy(buf, server_data->game.machines[i], 0x44);
      buf[0x40] = (char)i;
      result = network_game_add_machine((void *)&server_data->game, (void *)buf);
      if (result == 1) {
        transport_address address = {0};

        network_connection_get_address(client_machine->connection, (void *)&address, 0);
        network_event(
          "server added machine @ %s to the game at machine index #%d",
          transport_address_to_string((void *)&address), i);
        client_machine->flags |=
          NETWORK_CLIENT_MACHINE_FLAG(NETWORK_CLIENT_MACHINE_VALIDATED_BIT);
        client_machine->machine_index = (short)i;
      } else {
        network_event(
          "network_game_add_machine() failed in "
          "network_game_server_accept_client_machine_into_game()");
      }
      break;
    }
  }
  if (i == 4) {
    network_event(
      "network_game_server_accept_client_machine_into_game() failed to find "
      "an available opening for the new machine");
  }
  return result;
}

/* Check if a player name/identity already exists in the server's player list
 * (0x12c690). Iterates 0x10 player slots at server+0x22e with stride 0x20.
 * Returns false if a matching player is found, true if unique. */
bool is_name_unique(int server, int player)
{
  int i;
  char *slot;
  i = 0;
  slot = (char *)server + 0x22e;
  do {
    if (network_player_is_valid((void *)slot)) {
      if (!ustrcmp((const wchar_t *)slot, (const wchar_t *)player))
        return false;
    }
    i++;
    slot += 0x20;
  } while (i < 0x10);
  return true;
}

/* get_unique_random_name — 0x12c6d0
 * Pick a random player name that is not already used by any active player.
 * Iterates up to 0x10 player slots at param_1+0x22e (0x20 bytes each).
 * Retries FUN_0012b5e0 until the returned name does not match any active
 * player's name via ustrcmp. Copies the chosen name to param_2 (0xb wide
 * chars) and writes a null short at param_2+0x16. */
void get_unique_random_name(int param_1, int param_2)
{
  wchar_t *name;
  int match_count;

  do {
    int i;

    name = network_game_get_random_player_name();
    match_count = 0;
    for (i = 0; i < 0x10; i++) {
      char *player_slot = (char *)param_1 + 0x22e + i * 0x20;

      if (network_player_is_valid((void *)player_slot) &&
          !ustrcmp((wchar_t *)player_slot, name)) {
        match_count++;
      }
    }
  } while (match_count != 0);

  ustrncpy((wchar_t *)param_2, name, 0xb);
  *(short *)((char *)param_2 + 0x16) = 0;
}

/* get_unique_random_color — 0x12c750
 * Pick a random team index not already occupied by any active player.
 * For the first 10 retries uses FUN_001c19a0, then FUN_001c19c0.
 * Player slots start at param_1+0x246 (0x20 bytes each, 0x10 slots).
 * network_player_is_valid is called with slot-0x18 (slot base pointer).
 * The team value is a short at slot[0] (= player_base+0x18).
 * Stores the chosen team as a short at param_2+0x18. */
void get_unique_random_color(int param_1, int param_2)
{
  int retry_count = 0;
  int random_team;
  char unique;

  do {
    int i;

    random_team = retry_count < 0xa ? FUN_001c19a0() : FUN_001c19c0();
    unique = 1;
    for (i = 0; i < 0x10; i++) {
      if (network_player_is_valid((char *)param_1 + 0x22e + i * 0x20) &&
          *(short *)((char *)param_1 + 0x246 + i * 0x20) == random_team) {
        unique = 0;
        break;
      }
    }
    retry_count++;
  } while (!unique);

  *(short *)((char *)param_2 + 0x18) = (short)random_team;
}

/* Add a player to the game after machine/identity validation (0x12c7e0). */
char network_game_server_add_player_to_game(int server, int machine,
                                            void *player)
{
  char *p = (char *)player;
  char result;
  int next_val;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x46c, 1);
    system_exit(-1);
  }
  if (!machine) {
    display_assert("machine",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x46d, 1);
    system_exit(-1);
  }
  if (!player) {
    display_assert("player",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x46e, 1);
    system_exit(-1);
  }
  if (*(short *)(machine + 0xc) == (short)p[0x1c]) {
    next_val = network_game_server_next_team;
    *(char *)(p + 0x1e) = (char)next_val;
    network_game_server_next_team = (next_val + 1) % 2;
    if (*(short *)p == 0) {
      get_unique_random_name(server, (int)player);
    }
    if (!is_name_unique(server, (int)player)) {
      get_unique_random_name(server, (int)player);
    }
    if (*(short *)(p + 0x18) == -1) {
      get_unique_random_color(server, (int)player);
    }
    result = network_game_add_player((void *)(server + 8), (void *)player);
    if (result == 1) {
      network_event("server added player from machine #%d at controller "
                       "index #%d to the game",
                       (int)*(char *)(p + 0x1c),
                       (int)*(char *)(p + 0x1d));
      return result;
    }
    network_event("network_game_add_player() failed in "
                     "network_game_server_add_player_to_game()");
    return result;
  }
  network_event("client machine tried to add a player with a non-matching "
                   "machine identifier");
  return 0;
}

/* network_game_server_remove_player_from_game — 0x12c920
 * Removes a player from the game if machine IDs match. Asserts server,
 * machine, and player are all non-null. Returns 1 on success. */
char network_game_server_remove_player_from_game(int param_1, int param_2,
                                                 int param_3)
{
  char cVar1;

  if (param_1 == 0) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x4a0, 1);
    system_exit(-1);
  }
  if (param_2 == 0) {
    display_assert("machine",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x4a1, 1);
    system_exit(-1);
  }
  if (param_3 == 0) {
    display_assert("player",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x4a2, 1);
    system_exit(-1);
  }
  if (*(short *)(param_2 + 0xc) == (short)*(char *)(param_3 + 0x1c)) {
    cVar1 = network_game_remove_player((void *)(param_1 + 8), (void *)param_3);
    if (cVar1 == '\x01') {
      network_event("server removed player from machine #%d at controller "
                       "index #%d from the game",
                       (int)*(char *)(param_3 + 0x1c),
                       (int)*(char *)(param_3 + 0x1d));
      return '\x01';
    }
    network_event("network_game_remove_player() failed in "
                     "network_game_server_remove_player_from_game()");
    return cVar1;
  }
  network_event("client machine tried to remove a player with a "
                   "non-matching machine identifier");
  return '\0';
}

/* network_game_server_adjust_machine_settings — 0x12ca00
 * Validates and updates machine description if the machine's ID matches.
 * Asserts server, machine, and machine_description non-null. Returns 1
 * on success, or the error code from network_game_update_machine. */
char network_game_server_adjust_machine_settings(int param_1, int param_2, int param_3)
{
  char cVar1;

  cVar1 = 0;
  if (((param_1 == 0) || (param_2 == 0)) || (param_3 == 0)) {
    display_assert("server && machine && machine_description",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x4bf, 1);
    system_exit(-1);
  }
  if (*(short *)(param_2 + 0xc) == (short)*(char *)(param_3 + 0x40)) {
    cVar1 = network_game_update_machine((void *)(param_1 + 8), (void *)param_3);
    if (cVar1 == '\x01') {
      network_event("server updated machine #%d settings",
                       (int)*(char *)(param_3 + 0x40));
    } else {
      network_event("network_game_update_machine() failed in network_game_server_adjust_machine_settings()");
    }
  } else {
    network_event("client machine tried to update itself with a non-matching "
                     "machine identifier");
  }
  return cVar1;
}

/* Finalize server loading after all machines have loaded (0x12caa0).
 * Sets the server state to 1, clears the timer at +0x484, then copies a
 * "local game data loaded" flag from the client's game-data region into
 * server+0x438.  Asserts if the flag is zero (data not loaded). */
__declspec(noinline) void
network_game_server_all_machines_have_loaded(void *server)
{
  char *s = (char *)server;

  network_event("all machines have successfully loaded");
  *(int16_t *)(s + 0x4) = 1;
  *(int32_t *)(s + 0x484) = 0;
  *(uint8_t *)(s + 0x438) =
    global_network_game_client_get()
      ? *(uint8_t *)((char *)network_game_client_get_game(
                       global_network_game_client_get()) + 0x430)
      : 0;
  if (!*(uint8_t *)(s + 0x438)) {
    display_assert("local game data not loaded",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x4e0, 1);
    system_exit(-1);
  }
}

/* Mark machine as loading-complete and check if all machines are loaded.
 * 0x12cb20 / network_server_manager.obj
 */
void network_game_server_client_machine_game_loading_complete(void *server,
                                                               void *machine)
{
  short sVar1;
  bool bVar2;
  short *psVar4;
  int iVar5;

  bVar2 = 1;
  if (!server) {
    display_assert("server", "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x4ed, 1);
    system_exit(-1);
  }
  if (!machine) {
    display_assert("machine", "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x4ee, 1);
    system_exit(-1);
  }
  *(unsigned char *)((char *)machine + 0xe) |= 4;
  psVar4 = (short *)((char *)server + 0x448);
  iVar5 = 4;
  do {
    sVar1 = *psVar4;
    if (sVar1 >= 0 && sVar1 < 4 &&
        !(*(unsigned char *)((char *)psVar4 + 2) & 4)) {
      network_event("still waiting on machine #%d to finish loading",
                       (int)sVar1);
      bVar2 = 0;
    }
    psVar4 = (short *)((char *)psVar4 + 0x10);
    iVar5--;
  } while (iVar5 != 0);
  if (bVar2 == 1) {
    network_game_server_all_machines_have_loaded(server);
  }
  if (*(int *)((char *)server + 0x484) == 0) {
    *(unsigned int *)((char *)server + 0x484) = system_milliseconds();
  }
}

/* network_game_server_client_machine_is_precached — 0x12cbe0 */
void network_game_server_client_machine_is_precached(int param_1, int param_2,
                                                     int param_3)
{
  char *map_name;
  int iVar2;
  map_name = main_get_multiplayer_map_name();
  iVar2 = csstrcmp(map_name, (const char *)param_3);
  if (iVar2 == 0) {
    *(unsigned char *)(param_2 + 0xe) = *(unsigned char *)(param_2 + 0xe) | 8;
  }
}

#if defined(_MSC_VER) && !defined(__clang__)
extern void *__cdecl memset(void *, int, unsigned int);
extern void *__cdecl memcpy(void *, const void *, unsigned int);
#pragma intrinsic(memset)
#pragma intrinsic(memcpy)
#ifndef qmemcpy
#define qmemcpy memcpy
#endif
#else
#define memset __builtin_memset
#ifndef qmemcpy
#define qmemcpy __builtin_memcpy
#endif
#endif

/* Handle a client update packet (tick sync, player data) from a machine
 * (0x12cc10). */
void network_game_server_handle_client_update_packet(int server, int machine,
                                                     void *message)
{
  short player_count;
  unsigned int tick;
  unsigned int machine_last_tick;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x51e, 1);
    system_exit(-1);
  }
  if (!machine) {
    display_assert("machine",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x51f, 1);
    system_exit(-1);
  }
  if (!message) {
    display_assert("message_packet",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x520, 1);
    system_exit(-1);
  }
  if (*(int *)message & 0x80000000) {
    network_event(
      "client machine #%d is out of sync @ game tick #%ld; switching to "
      "post-game",
      (int)*(short *)((char *)machine + 0xc), (int)game_time_get());
    game_engine_switch_to_postgame();
    return;
  }
  tick = *(unsigned int *)message & 0x7fffffff;
  machine_last_tick = *(unsigned int *)((char *)machine + 4);
  if (tick < machine_last_tick) {
    network_event("received an outdated client update packet; ignoring "
                     "(#%d / #%d)",
                     tick, machine_last_tick);
    return;
  }
  player_count = *(short *)((char *)message + 6);
  if (player_count >= 0 && player_count <= 4) {
    player_action_t actions[4] = { 0 };
    int player_index;

    for (player_index = 0; player_index < player_count; player_index++) {
      actions[player_index] = ((player_action_t *)((char *)message + 8))[player_index];
    }
    update_server_apply_actions(*(short *)((char *)machine + 0xc),
                                (void *)actions);
    *(unsigned int *)((char *)machine + 4) =
      *(unsigned int *)message & 0x7fffffff;
  } else {
    network_event(
      "client update packet from machine #%d had a bad player count; ignoring",
      (int)*(short *)((char *)machine + 0xc));
  }
}

/* network_game_server_switch_machine_from_postgame_to_pregame — 0x12cd60 */
int network_game_server_switch_machine_from_postgame_to_pregame(int param_1,
                                                                int param_2)
{
  if ((param_1 == 0) || (param_2 == 0)) {
    display_assert("server && machine",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x547, 1);
    system_exit(-1);
  }
  network_event("machine #%d has successfully switched to pregame",
                   (int)*(short *)(param_2 + 0xc));
  *(unsigned char *)(param_2 + 0xe) = *(unsigned char *)(param_2 + 0xe) & 0xfb;
  return 1;
}

/* Main server tick function (0x12cdb0).
 * Sends game state updates, handles pending player additions.
 * tick_count @<ax> implicitly forwarded from esi context. */
void network_game_server_update_ticks(int server, unsigned short tick_count)
{
  char input_buf[516]; /* [count:2][pad:2][data:512] contiguous */
  char upkt[0x210];
  int update_number; /* name: PAL 2342 network_server_manager.c:1715 */
  int i;
  short tick_index;
  int16_t target_idx;
  char *s;
  network_game_server *server_data = (network_game_server *)server;
  char *slot;
  void *msg;
  int conn;

  s = (char *)server;
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x553, 1);
    system_exit(-1);
  }

  /* inlined network_game_server_get_state(server, NULL) asserts again (0x12cddb) */
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x1f2, 1);
    system_exit(-1);
  }

  switch (server_data->state) {
  case NETWORK_GAME_SERVER_STATE_INGAME:
    for (tick_index = 0; tick_index < (short)tick_count; tick_index++) {
      update_number = server_data->next_update_number++;
      update_server_next_update();
      update_server_build_server_update(-1, (void *)input_buf, &update_number);
      *(int *)(upkt + 0) = update_number;
      *(int *)(upkt + 4) = get_random_seed();
      *(int *)(upkt + 8) = game_time_get();
      *(unsigned short *)(upkt + 0xe) = *(unsigned short *)input_buf;
      csmemcpy(upkt + 0x10, input_buf + 4,
               (unsigned int)*(unsigned short *)input_buf << 5);
      msg = create_network_game_message(0x14, upkt, 0x210);
      if (msg && !network_game_server_send_message_to_all_machines((void *)server, msg)) {
        network_event(
          "server failed to send game update message to all machines; client "
          "machine may be out of sync");
      }
    }
    if (server_data->queued_player_valid) {
      target_idx = (int16_t)*(int8_t *)(s + 0x4b4);
      for (i = 0, slot = s + 0x448; i < 4; i++, slot += 0x10) {
        if (*(short *)slot == target_idx) {
          conn = (int)(s + 0x43c + i * 0x10);
          if (conn && network_game_server_add_player_to_game(
                        server, conn, (void *)server_data->queued_player)) {
            if (!network_game_server_send_player_joined_info_ingame(server, (void *)server_data->queued_player)) {
              network_event(
                "network_game_server_send_player_joined_info_ingame() "
                "failed in network_game_server_handle_message_client_"
                "add_player_request_ingame()");
            }
            server_data->queued_player_valid = 0;
            return;
          }
          break;
        }
      }
      network_event("server failed to add a network player in-game");
      server_data->queued_player_valid = 0;
    }
    break;
  case NETWORK_GAME_SERVER_STATE_POSTGAME:
    game_engine_update();
    break;
  }
}

/* network_game_server_queue_player_for_addition — 0x12cf60 */
void network_game_server_queue_player_for_addition(int param_1, int param_2)
{
  char cVar1;
  if ((param_1 == 0) || (param_2 == 0)) {
    display_assert("server && player",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x5de, 1);
    system_exit(-1);
  }
  if (*(char *)(param_1 + 0x4b8) == '\0') {
    cVar1 = network_player_is_valid((void *)param_2);
    if (cVar1 != '\0') {
      csmemcpy((void *)(param_1 + 0x498), (void *)param_2, 0x20);
      *(unsigned char *)(param_1 + 0x4b8) = 1;
    }
  }
}

/* network_game_server_begin_game_start_countdown — 0x12cfd0 */
void network_game_server_begin_game_start_countdown(int param_1, int param_2)
{
  if (param_1 == 0) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x5ed, 1);
    system_exit(-1);
  }
  if ((*(char *)(param_1 + 0x494) == '\0') &&
      (*(char *)(param_1 + 0x495) == '\0')) {
    countdown_timer_set_time_remaining((int *)(param_1 + 0x488), param_2);
    *(unsigned char *)(param_1 + 0x496) = 0;
    *(unsigned char *)(param_1 + 0x494) = 1;
    network_event("server game start countdown started");
  }
}

/* Check whether any team (0 or 1) has zero active clients among the 16 client
 * slots at server+0x22E..+0x44C (stride 0x20).  Returns true when at least one
 * team is empty, false when both teams have members (0x12d040). */
__declspec(noinline) bool server_needs_more_teams(void *server)
{
  char *s = (char *)server;
  bool needs_more_teams = false;

  if (*(char *)(s + 0xc8)) {
    int16_t counts[2];
    char *client_ptr;
    signed char team;
    int i, j;

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
      if (counts[j] == 0) {
        needs_more_teams = true;
        break;
      }
    }
  }

  return needs_more_teams;
}

/* Check that every machine slot with a valid team index has at least one
 * active client on that team (0x12d0c0). Iterates the 4 machine slots at
 * server+0x448 (stride 0x10) and for each valid slot, searches the 16 client
 * entries at server+0x22e (stride 0x20) for a matching team byte. Returns
 * false if any valid slot has no matching active client. */
__declspec(noinline) bool server_has_a_player_on_each_machine(void *server)
{
  char *s = (char *)server;
  short *slot;
  int i, j;

  for (i = 0, slot = (short *)(s + 0x448); i < 4;
       i++, slot = (short *)((char *)slot + 0x10)) {
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
  }
  return true;
}

/* Check whether enough machine slots have valid team indices (0x12d150).
 * Counts slots in [0,4) across the 4 machine entries at server+0x448
 * (stride 0x10). If network_game_is_splitscreen_local returns true the
 * threshold is 1, otherwise 2. Returns count >= threshold. */
__declspec(noinline) bool server_has_enough_machines(void *server)
{
  char *s = (char *)server;
  int threshold = network_game_is_splitscreen_local() ? 1 : 2;
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

/* Predicate: server is ready to start the countdown (0x12d1c0).
 * Requires enough machines, a valid color, no pending unique-name conflict,
 * and [server+0x22c] >= sign-extended [server+0x115]. */
bool server_ok_to_countdown(void *server)
{
  char *s = (char *)server;

  if (server_has_enough_machines(server) &&
      server_has_a_player_on_each_machine(server) && !server_needs_more_teams(server) &&
      *(short *)(s + 0x22c) >= (short)*(char *)(s + 0x115)) {
    return true;
  }
  return false;
}

/* Zero out a machine struct (0x44 bytes) and set byte at +0x40 to 0xff.
 * 0x12d210 / network_server_manager.obj
 */
void network_game_server_invalidate_network_machine(void *machine)
{
  if (!machine) {
    display_assert("machine", "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x6c9, 1);
    system_exit(-1);
  }
  csmemset(machine, 0, 0x44);
  *(char *)((char *)machine + 0x40) = (char)0xff;
}

/* Generate a 16-byte join token by writing "message in a bot" (0x12d250). */
void network_game_generate_join_game_token(void *join_token)
{
  char buf[0x14];

  buf[0] = 'm';
  buf[1] = 'e';
  buf[2] = 's';
  buf[3] = 's';
  buf[4] = 'a';
  buf[5] = 'g';
  buf[6] = 'e';
  buf[7] = ' ';
  buf[8] = 'i';
  buf[9] = 'n';
  buf[10] = ' ';
  buf[11] = 'a';
  buf[12] = ' ';
  buf[13] = 'b';
  buf[14] = 'o';
  buf[15] = 't';
  buf[16] = 't';
  buf[17] = 'l';
  buf[18] = 'e';
  if (!join_token) {
    display_assert("join_token",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x6da, 1);
    system_exit(-1);
  }
  csmemset(join_token, 0, 4);
  csmemcpy(join_token, buf, 0x10);
}

/* Get the copied network-machine slot for a client machine (0x12d2f0).
 * Returns server+0x11c+machine_index*0x44 and writes its index byte to *out. */
int network_game_server_get_client_machine(int server, int machine, int *out)
{
  int result;

  if (!server || !machine) {
    display_assert("server && client_machine",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x701, 1);
    system_exit(-1);
  }
  if (*(short *)(machine + 0xc) >= 4) {
    display_assert(
      "client_machine->machine_index<MAXIMUM_NETWORK_MACHINE_COUNT",
      "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x702, 1);
    system_exit(-1);
  }
  if (out)
    *out = -1;
  result = *(short *)(machine + 0xc) * 0x44 + server + 0x11c;
  if (out)
    *out = (int)*(char *)(result + 0x40);
  return result;
}

/* Return the connection object from the server (server[0]).
 * 0x12d380 / network_server_manager.obj
 */
int network_game_server_get_connection(void *server)
{
  if (!server) {
    display_assert("server", "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x712, 1);
    system_exit(-1);
  }
  return *(int *)server;
}

/* Return the connection handle from a machine struct (0x12d3b0).
 * Returns the first dword at machine+0, or 0 if machine is NULL. */
__declspec(noinline) int
network_game_server_get_client_connection(void *machine)
{
  if (machine != NULL)
    return *(int *)machine;
  return 0;
}

/* Return the connection handle for a machine by scanning server+0x43c
 * (0x12d3d0). Finds the slot whose machine_index matches machine+0x40; returns
 * connection or 0. */
int network_game_server_get_machine_connection(int server, int machine)
{
  int result = 0;
  int i;

  if (!server || !machine || *(char *)(machine + 0x40) < 0 ||
      !(*(char *)(machine + 0x40) < 4)) {
    display_assert("server && network_machine_is_valid(machine)",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x72f, 1);
    system_exit(-1);
  }
  for (i = 0; i < 4; i++) {
    if (*(short *)(server + 0x448 + i * 0x10) == *(char *)(machine + 0x40)) {
      result = *(int *)(server + 0x43c + i * 0x10);
      break;
    }
  }
  return result;
}

/* Get a pointer to the machine entry at the given index (0x12d450).
 * Asserts server is non-null and index < MAXIMUM_NETWORK_MACHINE_COUNT (4).
 * Each machine entry is 0x10 bytes, starting at server+0x43c. */
__declspec(noinline) int
network_game_server_get_client_machine_at_index(int server, int machine_index)
{
  if (!server || machine_index >= 4) {
    display_assert("server && (index<MAXIMUM_NETWORK_MACHINE_COUNT)",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x741, 1);
    system_exit(-1);
  }
  return machine_index * 0x10 + 0x43c + server;
}

/* Return a machine entry pointer by scanning for a matching IP address
 * (0x12d4a0). Iterates the 4 connection slots; returns pointer to matching slot
 * or 0. */
int network_game_server_get_client_machine_at_address(int server,
                                                      int ip_address)
{
  network_game_server *server_data = (network_game_server *)server;
  int client_machine = 0;
  int i;

  if (!server || !ip_address) {
    display_assert("server && ip_address",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x74d, 1);
    system_exit(-1);
  }
  for (i = 0; i < 4; i++) {
    if (server_data->client_machines[i].machine_index >= 0 &&
        server_data->client_machines[i].machine_index < 4) {
      int address[6]; /* 24-byte transport address */

      if (!server_data->client_machines[i].connection) {
        display_assert("server->client_machines[i].connection",
                       "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                       0x755, 1);
        system_exit(-1);
      }
      network_connection_get_address(server_data->client_machines[i].connection,
                                     address, 0);
      if (address[0] == ip_address) {
        client_machine = (int)&server_data->client_machines[i];
        break;
      }
    }
  }

  if (i == 4)
    network_event("no machine found @ ip #%lX", ip_address);

  return client_machine;
}

/* Assert server is non-null and return the connection pointer at offset +8
 * (0x12d570). */
__declspec(noinline) int network_game_server_get_game(void *server)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x769, 1);
    system_exit(-1);
  }
  return (int)((char *)server + 8);
}

/* Return the smallest last-update tick across all 4 machine slots that are
 * joined and have a valid update tick. Returns 0xffffffff if none qualify.
 * 0x12d5b0 / network_server_manager.obj */
unsigned int network_game_server_get_oldest_client_update_received(int param_1)
{
  network_game_server *server = (network_game_server *)param_1;
  unsigned int oldest_update;
  int index;

  oldest_update = 0xffffffff;
  for (index = 0; index < 4; index++) {
    network_game_server_client_machine *client_machine =
      &server->client_machines[index];

    if (client_machine->machine_index >= 0 && client_machine->machine_index < 4 &&
        oldest_update > client_machine->last_received_update_sequence_number) {
      oldest_update = client_machine->last_received_update_sequence_number;
    }
  }
  return oldest_update;
}

/* Return true if the server can start counting down (state 0, machines joined).
 * 0x12d640 / network_server_manager.obj
 */
int network_game_server_game_can_start(void *server)
{
  if (!server) {
    display_assert("server", "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x782, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)server + 4) == 0 &&
      *(short *)((char *)server + 0x22c) >= *(char *)((char *)server + 0x115)) {
    return 1;
  }
  return 0;
}

/* Set or clear the countdown pause flag; clear the countdown struct if pausing.
 * 0x12d690 / network_server_manager.obj
 */
void network_game_server_pause_countdown(void *server, char flag)
{
  if (!server) {
    display_assert("server", "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x78c, 1);
    system_exit(-1);
  }
  if (flag == '\x01') {
    csmemset((char *)server + 0x488, 0, 0x10);
  }
  *(char *)((char *)server + 0x495) = flag;
}

/* Set map name, clear preloaded flags on valid machines, notify clients
 * (0x12d6f0). */
void network_game_server_change_map_name(int server, char *map_name)
{
  char *s;

  if (!server || !map_name || !map_name[0]) {
    display_assert("server && map_name && map_name[0]",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x79b, 1);
    system_exit(-1);
  }
  if (*(short *)(server + 4) != 0) {
    display_assert("server->state == _network_game_server_state_pregame",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x79c, 1);
    system_exit(-1);
  }
  s = (char *)server;
  if (*(short *)(s + 0x448) >= 0 && *(short *)(s + 0x448) < 4)
    *(unsigned short *)(s + 0x44a) &= 0xfff7;
  if (*(short *)(s + 0x458) >= 0 && *(short *)(s + 0x458) < 4)
    *(unsigned short *)(s + 0x45a) &= 0xfff7;
  if (*(short *)(s + 0x468) >= 0 && *(short *)(s + 0x468) < 4)
    *(unsigned short *)(s + 0x46a) &= 0xfff7;
  if (*(short *)(s + 0x478) >= 0 && *(short *)(s + 0x478) < 4)
    *(unsigned short *)(s + 0x47a) &= 0xfff7;
  csstrncpy(s + 0x2c, map_name, 0x7f);
  *(char *)(s + 0xab) = 0;
  if (!network_game_server_send_game_data_pregame((void *)server))
    network_event(
      "network_game_server_change_map_name() failed to send updated game "
      "settings to clients");
}

/* Copy game variant data into server and broadcast it to clients.
 * 0x12d7f0 / network_server_manager.obj
 */
void network_game_server_change_game_variant(void *server, void *variant)
{
  char cVar1;
  if (!server || !variant) {
    display_assert("server && variant", "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x7be, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)server + 4) != 0) {
    display_assert("server->state == _network_game_server_state_pregame", "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x7bf, 1);
    system_exit(-1);
  }
  csmemcpy((char *)server + 0xac, variant, 0x68);
  cVar1 = (char)network_game_server_send_game_data_pregame(server);
  if (!cVar1) {
    network_event("network_game_server_change_game_variant() failed to send "
                     "updated game settings to clients");
  }
}

/* Add a new client connection to the server (0x12d880).
 * Finds the first empty machine slot (short == -1 at +0x448 stride 0x10),
 * validates the remote address, stores the connection, and signals accept. */
bool network_game_server_add_new_client(int server, int new_connection)
{
  network_game_server *server_data = (network_game_server *)server;
  bool result = false;
  int i;

  if (!server || !new_connection) {
    display_assert("server && new_connection",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x7d4, 1);
    system_exit(-1);
  }

  if (network_game_server_game_is_open((void *)server)) {
    for (i = 0; i < 4; i++) {
      if (server_data->client_machines[i].machine_index == -1) {
        transport_address client_address = { 0 };

        network_connection_get_address(new_connection, &client_address, 0);
        if (client_address.address.ipv4_address) {
          if (!network_game_should_accept_remote_connections() &&
              client_address.address.ipv4_address != IPV4_LOOPBACK_ADDRESS) {
            network_event(
              "remote system tried to join our server but we are not accepting "
              "remote connections: address= '%s'",
              transport_address_to_string(&client_address));
          } else {
            server_data->client_machines[i].connection = new_connection;
            network_game_invalidate_machine(&server_data->game, i);
            server_data->client_machines[i].machine_index = (short)i;
            server_data->client_machines[i].flags =
              NETWORK_CLIENT_MACHINE_FLAG(NETWORK_CLIENT_MACHINE_CONNECTED_BIT);
            result = network_connection_server_accept_client_connection(
              server_data->connection, new_connection);
            if (result == 1) {
              network_event("new remote connection accepted from %s",
                            transport_address_to_string(&client_address));
            }
          }
        } else {
          network_event(
            "network_connection_get_address() failed to get a valid address "
            "in network_game_server_add_new_client()");
        }
        break;
      }
    }

    if (i == 4) {
      network_event("failed to find an available machine slot in "
                    "network_game_server_add_new_client()");
    }
  } else {
    network_event(
      "network_game_server_add_new_client() failed because the game is closed");
  }

  return result;
}

/* Handle incoming datagrams on the server's public endpoint (0x12d9f0).
 * Loops reading datagrams and dispatching them until none remain. */
bool network_game_server_handle_public_endpoint(int server)
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

  while (result && network_connection_read(*(int *)s, buffer, &size, addr)) {
    result = network_game_server_handle_datagram((void *)server, buffer, size, addr);
    if (!result) {
      network_event("network_game_server_handle_datagram() failed in "
                       "network_game_server_handle_public_endpoint()");
    }
    size = 0x190;
  }

  return result;
}

/* Send a rejection message to an endpoint (0x12da90).
 * endpoint @<ebx>, reason @<ax> (0-6). */
void network_game_server_send_rejection_message(int endpoint, unsigned short reason)
{
  unsigned short farewell_message[1];
  void *msg;

  farewell_message[0] = reason;
  if (!endpoint || reason >= 7) {
    display_assert("endpoint && (reason < NUMBER_OF_SERVER_REJECTION_CODES)",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x878, 1);
    system_exit(-1);
  }
  msg = create_network_game_message(5, farewell_message, 2);
  if (msg) {
    int msg_len = (int)(*(unsigned short *)msg) >> 4;
    int send_result;

    byte_swap_message_header((unsigned short *)msg, 1);
    send_result = send_endpoint((int *)endpoint, (const char *)msg, msg_len);
    if (send_result != msg_len) {
      network_event(
        "error sending rejection message to client; transport error= \'%s\'",
        FUN_00081c80(send_result));
    }
  } else {
    network_event(
      "failed to create a message_server_machine_rejected message in "
      "network_game_server_send_rejection_message");
  }
}

/* Connection refused (game full) callback (0x12db30). Registered as the
 * connection's rejection procedure, called with just the endpoint; reason is
 * hardcoded to 4 (server full), not a parameter (confirmed via disasm:
 * MOV EAX,0x4 before the call, param comes off the stack into EBX). */
void network_game_server_reject_connection_game_is_full(int endpoint)
{
  network_event("client connection refused; game is full");
  network_game_server_send_rejection_message(endpoint, 4);
}

/* Postgame state handler (0x12db60).
 * Every 5 seconds sends a heartbeat message (type 0xb) to all clients. */
bool network_game_server_idle_postgame_tasks(int server)
{
  unsigned int now;
  short data;

  now = system_milliseconds();
  if (now > *(unsigned int *)((char *)server + 0x480) + 5000) {
    data = 0;
    network_game_server_send_message_to_all_machines((void *)server, create_network_game_message(0xb, &data, 2));
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
bool network_game_server_have_all_machines_have_precached(int server)
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
bool network_game_server_setup_game_from_playlist(int server)
{
  char *s = (char *)server;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x961, 1);
    system_exit(-1);
  }

  network_event("setting up a net game");

  /* Reference sinks the failure block to the tail (`test al,al; je <end>`),
   * and initializes the 64-byte buffer inline in the success path
   * (`rep movsl` of 5 dwords from the L"<unknown>" const at 0x281c38, then
   * `rep stosl` of 11 zero dwords) -- a wide-string aggregate initializer
   * on a block-scoped local, not two library calls. */
  if (game_engine_get_current_stage(s + 0xac, s + 0x2c)) {
#if defined(_MSC_VER) && !defined(__clang__)
    wchar_t name_buf[0x20] = L"<unknown>";
#else
    /* clang lowers the aggregate initializer to a memcpy call, and this
     * target has no linkable memcpy; the explicit copy is byte-identical. */
    wchar_t name_buf[0x20];

    csmemcpy(name_buf, (void *)0x281c38, 20);
    csmemset((char *)name_buf + 20, 0, 44);
#endif

    network_game_generate_local_machine_name(name_buf);
    ustrncpy(((network_game_blob_t *)(s + 8))->game_name, name_buf, 0xf);

    ((network_game_blob_t *)(s + 8))->game_name[15] = 0;
    ((network_game_blob_t *)(s + 8))->map_version = 0;
    ((network_game_blob_t *)(s + 8))->minimum_players = 2;
    ((network_game_blob_t *)(s + 8))->maximum_player_count = 0x10;
    ((network_game_blob_t *)(s + 8))->maximum_teams =
        (((network_game_blob_t *)(s + 8))->game_variant.team_play != 0) + 1;

    network_game_server_open_game((void *)server);

    return true;
  }

  error(2, "network game setup failed; probably due to a missing playlist");
  return false;
}

/* Dump network game data fields to the log with a prefix (0x12dd20).
 * Prints machine_count, 4 machine slots (stride 0x44 from game_data+0x154),
 * player_count, 16 player entries (stride 0x20 from game_data+0x226),
 * random seed, and games played. */
void dump_network_game_data(void *game_data, const char *prefix)
{
  network_game_blob_t *game = (network_game_blob_t *)game_data;
  int i;

  network_event("%snetwork_game_data", prefix);
  network_event("%smachine_count %d", prefix, game->machine_count);

  for (i = 0; i < 4; i++) {
    network_event("\t%smachine %d %x", prefix, i, game->machines[i].machine_index);
  }

  network_event("%splayer_count %d", prefix, game->player_count);

  for (i = 0; i < 0x10; i++) {
    network_event("%splayer %d", prefix, i);
    network_event("%s\tmachine_index %x", prefix, game->players[i].machine_index);
    network_event("%s\tcontroller_index %x", prefix,
                  game->players[i].controller_index);
    network_event("%s\tteam_index %x", prefix, game->players[i].team_index);
    network_event("%s\tplayer_list_index %x", prefix,
                  game->players[i].player_index);
  }

  network_event("%snetwork_game_random_seed %x", prefix, game->random_seed);
  network_event("%snumber_of_games_played %d", prefix, game->number_of_games_played);
}

/* Dump the full server state to the network game log for debugging (0x12de20).
 * Prints connection, state, flags, game data, all 4 client machine slots
 * (connection, update sequence, stall time, machine index, flags), and
 * timing fields. */
void network_game_server_dump(void *server)
{
  network_game_server *server_data = (network_game_server *)server;
  int itr;

  network_event("*************BEGIN*************");
  network_event("\tconnection %x", server_data->connection);
  network_event("\tstate %x", (int)server_data->state);
  network_event("\tflags %x", (int)server_data->flags);
  dump_network_game_data(&server_data->game, "\t");
  network_event("client_machines:");

  for (itr = 0; itr < 4; itr++) {
    network_game_server_client_machine *client_machine =
      server_data->client_machines + itr;
    const char *status = "no connection";

    if (client_machine->connection != 0) {
      if (network_connection_active(client_machine->connection))
        status = "(active)";
      else
        status = "(dead)";
    }
    network_event("\tclient %d", itr);
    network_event("\t\tconnection %x %s", client_machine->connection, status);
    network_event("\t\tlast_received_update_sequence_number %d",
                     client_machine->last_received_update_sequence_number);
    network_event("\t\tstall_start_time %d", client_machine->stall_start_time);
    network_event("\t\tmachine_index %x", (int)client_machine->machine_index);
    network_event("\t\tflags %x", (int)client_machine->flags);
  }

  network_event("\tnext_update_number %d", server_data->next_update_number);
  network_event("\ttime_of_last_keep_alive %d",
                   server_data->time_of_last_keep_alive);
  network_event("\ttime_of_first_client_loading_completion %d",
                   (int)server_data->time_of_first_client_loading_completion);
  network_event("*************END*************");
}

/* Remove a client machine from the server's game (0x12df50).
 * If the server is in-game (state 1), broadcasts a player-removal message.
 * Finds and removes the machine's entry from game data (server+8), then
 * clears the matching machine slot (connection, stall, flags) and sets the
 * machine_index to -1. Returns true if the slot was found. */
__declspec(noinline) bool network_game_server_remove_client_machine_from_game(void *server, void *machine)
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
    network_game_server_remove_players_from_machine_ingame((int)server, (int)machine);

  ptr = s + 0x15c;
  for (i = 0; i < 4; i++) {
    if ((short)*(signed char *)ptr == *(short *)(m + 0xc)) {
      if (!network_game_remove_machine((void *)(s + 8),
                                       (void *)(s + 0x11c + i * 0x44))) {
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
        if (!network_server_close_client_connection(*(int *)s, *(int *)(s + 0x43c + i * 0x10)))
          network_event("server failed to close a client's connection");
      }
      *(int *)(s + 0x440 + i * 0x10) = 0;
      *(int *)(s + 0x43c + i * 0x10) = 0;
      *(int *)(s + 0x444 + i * 0x10) = 0;
      *(uint16_t *)(s + 0x44a + i * 0x10) = 0;
      *(short *)(s + 0x448 + i * 0x10) = -1;
      return true;
    }
  }

  network_event(
    "network_game_server_remove_client_machine_from_game() failed to find "
    "the specified machine");
  return false;
}

/* Remove a machine from the server's game by its machine_index (0x12e090).
 * Validates the machine_index byte at player_data+0x40. If valid (0..3),
 * searches the 4 machine slots at server+0x448 (stride 0x10) for a matching
 * index, calls network_game_server_remove_client_machine_from_game to remove it, then FUN_0012b500 to remove
 * the machine from game data. If the server state is 0 (pre-game),
 * sends updated settings to remaining clients. */
bool network_game_server_remove_machine_from_game(void *server, void *player_data)
{
  network_game_server *server_data = (network_game_server *)server;
  char *pd = (char *)player_data;
  signed char machine_idx;
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
    network_event(
      "network_game_server_remove_machine_from_game called with a "
      "machine_index of NONE");
  }

  machine_idx = *(signed char *)(pd + 0x40);
  /* inlined network_machine_is_valid(machine) */
  if (machine_idx >= 0 && machine_idx < 4) {
    for (i = 0; i < 4; i++) {
      if (server_data->client_machines[i].machine_index ==
          *(signed char *)(pd + 0x40)) {
        result = network_game_server_remove_client_machine_from_game(server, &server_data->client_machines[i]);
        if (!result) {
          network_event(
            "network_game_server_remove_client_machine_from_game() failed in "
            "network_game_server_remove_machine_from_game()");
        }
        break;
      }
    }

    if (i == 4) {
      network_event(
        "network_game_server_remove_machine_from_game() failed to find the "
        "specified machine");
    }

    if (*(signed char *)(pd + 0x40) != -1) {
      result = network_game_remove_machine(&server_data->game, player_data);
      if (!result) {
        network_event("network_game_remove_machine() failed in "
                         "network_game_server_remove_machine_from_game()");
      }
    }

    if (server_data->state == NETWORK_GAME_SERVER_STATE_PREGAME) {
      if (!network_game_server_send_game_data_pregame(server)) {
        network_event(
          "network_game_server_remove_machine_from_game() failed to send "
          "updated game settings to remaining clients");
      }
    }
  } else {
    network_event("attempted to remove an invalid machine from the game in "
                     "network_game_server_remove_machine_from_game()");
    network_event("machine name = <not implemented>");
    network_event("machine index = %x", (int)*(signed char *)(pd + 0x40));
    network_game_server_dump(server);
  }

  return result;
}

/* Manage stalled-client timeout detection and reset (0x12e1d0).
 * If stalled==false, clears all 4 slot timestamps (reset mode).
 * If stalled==true, finds the slot with the oldest (minimum) timestamp
 * that is in an active state (short at +0x448/0x458/0x468/0x478 in [0,3]),
 * then either stamps it on first entry or forcibly removes the client if
 * it has been stalled >1999 ms. After handling, clears timestamps for all
 * other slots. The 4 machine slots live at server+0x43c with stride 0x10;
 * fields per slot: +0x0=machine handle, +0x4=?, +0x8=timestamp, +0xc=state. */
void network_game_server_stalled_on_client(void *server, bool stalled)
{
  network_game_server *server_data = (network_game_server *)server;

  if (!server_data) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x59e, 1);
    system_exit(-1);
  }

  if (stalled) {
    unsigned int oldest_update = 0xffffffff;
    int culprit = -1;
    int i;

    for (i = 0; i < 4; i++) {
      if (server_data->client_machines[i].machine_index >= 0 &&
          server_data->client_machines[i].machine_index < 4 &&
          server_data->client_machines[i].last_received_update_sequence_number <
            oldest_update) {
        oldest_update =
          server_data->client_machines[i].last_received_update_sequence_number;
        culprit = i;
      }
    }

    if (culprit == -1) {
      display_assert("culprit != NONE",
                     "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                     0x5b1, 1);
      system_exit(-1);
    }

    if (server_data->client_machines[culprit].stall_start_time) {
      if (system_milliseconds() -
            server_data->client_machines[culprit].stall_start_time >= 2000) {
        char machine_name[32];

        network_event(
          "forcibly removing client system \'%s\' due to timeout in-game",
          wide_to_ascii(
            (const wchar_t *)server_data->game
              .machines[server_data->client_machines[culprit].machine_index],
            machine_name, 0x20)
            ? machine_name
            : "<unknown name>");
        if (!network_game_server_remove_client_machine_from_game(
              server, &server_data->client_machines[culprit])) {
          display_assert(
            "removed", "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
            0x5c1, 1);
          system_exit(-1);
        }
      }
    } else {
      server_data->client_machines[culprit].stall_start_time = system_milliseconds();
    }

    i = 0;
    do {
      if (i != culprit)
        server_data->client_machines[i].stall_start_time = 0;
      i++;
    } while (i < 4);
  } else {
    int i;

    i = 0;
    do {
      server_data->client_machines[i].stall_start_time = 0;
      i++;
    } while (i < 4);
  }
}

/* Update the pre-game countdown state machine (0x12e3a0).
 * Drives the server-side countdown timer based on countdown_event (countdown event
 * type: 0=increment, 1=clamp-and-latch, 2=cancel, 3=reset).
 * If the server is already in the waiting-for-clients path (0x494==0),
 * starts or validates a timer and transitions 0x494 to 1 when conditions are
 * met.  If in the active countdown path (0x494==1), advances or cancels the
 * timer based on the event type.  Returns early if the server is NULL or not
 * in pregame state. */
void network_game_server_update_countdown(void *server, short countdown_event)
{
  network_game_server *server_data = (network_game_server *)server;
  int *timer;
  unsigned int now;
  short client_machine_count;
  short *machine_index;
  int i;
  int countdown;

  if (server_data == 0 || server_data->state != NETWORK_GAME_SERVER_STATE_PREGAME) {
    display_assert(
      "server && server->state == _network_game_server_state_pregame",
      "c:\\halo\\SOURCE\\networking\\network_server_manager.c", 0x66e, 1);
    system_exit(-1);
  }
  if (server_data->countdown_state.paused == 0 &&
      (server_ok_to_countdown(server) || countdown_event == 2)) {
    if (server_data->countdown_state.active == 1) {
      if (!server_data->countdown_state.adjusted_time_this_tick) {
        switch (countdown_event) {
        case 0:
          server_data->countdown_state.adjusted_time_this_tick = 1;
          countdown_timer_increment(server_data->countdown_state.timer, 5000, 30999);
          break;
        case 1:
          server_data->countdown_state.adjusted_time_this_tick = 1;
          timer = server_data->countdown_state.timer;
          if (countdown_timer_get_time_remaining(timer) > 999) {
            countdown_timer_decrement(timer, 5000);
            if (countdown_timer_get_time_remaining(timer) < 999) {
              /* inlined countdown_timer_set_time_remaining(timer, 999) */
              now = system_milliseconds();
              timer[0] = 999;
              timer[1] = now;
            }
          }
          break;
        case 2:
          server_data->countdown_state.active = 0;
          server_data->countdown_state.adjusted_time_this_tick = 1;
          break;
        case 3:
          server_data->countdown_state.adjusted_time_this_tick = 1;
          now = system_milliseconds();
          server_data->countdown_state.timer[0] = 0;
          server_data->countdown_state.timer[1] = now;
          break;
        }
      }
    } else {
      system_milliseconds();
      if (countdown_event == 3) {
        now = system_milliseconds();
        server_data->countdown_state.timer[0] = 0;
        server_data->countdown_state.timer[1] = now;
        server_data->countdown_state.active = 1;
        server_data->countdown_state.adjusted_time_this_tick = 0;
        return;
      }
      if (network_game_should_accept_remote_connections()) {
        /* inlined client-machine count */
        client_machine_count = 0;
        machine_index = &server_data->client_machines[0].machine_index;
        i = 4;
        do {
          if (*(int *)((char *)machine_index - 12) != 0 && *machine_index != -1)
            client_machine_count++;
          machine_index += 8;
          i--;
        } while (i != 0);
        if (client_machine_count <= 1)
          return;
      }
      if ((unsigned char)network_game_is_splitscreen_local())
        countdown = 10999;
      else
        countdown = 30999;
      server_data->countdown_state.active = 1;
      countdown_timer_set_time_remaining(server_data->countdown_state.timer,
                                         countdown);
      server_data->countdown_state.last_countdown_message_time = 0;
      server_data->countdown_state.adjusted_time_this_tick = 0;
    }
  }
}

/* Process all connected client machines (0x12e580).
 * For each of 4 machine slots: checks connection liveness, reads pending
 * messages, handles disconnections and removal. Returns true on success. */
bool network_game_server_handle_client_machines(int server)
{
  network_game_server *server_data = (network_game_server *)server;
  bool success;
  int i;
  char buffer[0x800];
  int size;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x827, 1);
    system_exit(-1);
  }

  success = true;
  for (i = 0; success && i < 4; i++) {
    if (server_data->client_machines[i].machine_index != -1) {
      if (!network_connection_active(server_data->client_machines[i].connection)) {
        if (network_game_server_remove_machine_from_game(
              (void *)server, server_data->game.machines[server_data->client_machines[i].machine_index])) {
          network_event("client machine %x removed from game", server_data->client_machines[i].machine_index);
          network_game_server_dump((void *)server);
        } else {
          network_event("failed to remove client machine %x from game",
                        server_data->client_machines[i].machine_index);
          network_game_server_dump((void *)server);
        }
      } else if (network_connection_idle(server_data->client_machines[i].connection, 0, 0) &&
                 network_connection_connected(server_data->client_machines[i].connection)) {
        size = 0x800;
        while (success &&
               network_connection_read(server_data->client_machines[i].connection, buffer, &size, 0)) {
          if (network_game_server_handle_client_message((void *)server, server_data->client_machines + i,
                                                        buffer, size)) {
            size = 0x800;
          } else {
            network_event("network_game_server_handle_client_message() failed in "
                          "network_game_server_handle_client_machines()");
            if (network_game_server_remove_machine_from_game(
                  (void *)server,
                  server_data->game.machines[server_data->client_machines[i].machine_index])) {
              network_event("client machine removed from game", server_data->client_machines[i].machine_index);
            } else if (!network_game_server_remove_client_machine_from_game(
                         (void *)server, &server_data->client_machines[i])) {
              network_event("failed to remove client machine from game",
                            server_data->client_machines[i].machine_index);
            }
            break;
          }
        }
      } else {
        if (network_game_server_remove_machine_from_game(
              (void *)server, server_data->game.machines[server_data->client_machines[i].machine_index])) {
          network_event("client machine removed from game", server_data->client_machines[i].machine_index);
        } else {
          network_event("failed to remove client machine from game", server_data->client_machines[i].machine_index);
        }
      }
    }
  }
  return success;
}

/* Pregame tick handler (0x12e750).
 * When loading: enforces a 15-second timeout for client map loads.
 * When not loading: boots dead clients, checks if all players are ready,
 * manages the pregame countdown, and starts the game when ready. */
bool network_game_server_idle_pregame_tasks(int server)
{
  network_game_server *server_data = (network_game_server *)server;
  int now;
  bool result;
  int i;
  char name_buf[0x20];
  const char *name;

  now = system_milliseconds();
  result = true;

  if (server_data->sent_start_game_message == 0) {
    int itr;

    for (itr = 0; itr < 4; itr++) {
      network_game_server_client_machine *client_machine =
        server_data->client_machines + itr;

      if (client_machine->connection != 0 &&
          !network_connection_active(client_machine->connection)) {
        network_event("booting dead client machine %d", itr);
        network_game_server_remove_client_machine_from_game((void *)server,
                                                            client_machine);
      }
    }

    if (server_data->countdown_state.active == 1) {
      bool send_countdown_update;
      bool ok_to_countdown;

      send_countdown_update = false;
      ok_to_countdown = server_has_enough_machines((void *)server) &&
                        server_has_a_player_on_each_machine((void *)server) &&
                        !server_needs_more_teams((void *)server) &&
                        server_data->game.player_count >=
                          (short)server_data->game.minimum_players;
      if (!ok_to_countdown) {
        csmemset(&server_data->countdown_state, 0,
                 sizeof(server_data->countdown_state));
        send_countdown_update = true;
      } else if (countdown_timer_get_time_remaining(
                   server_data->countdown_state.timer) == 0 &&
                 network_game_server_have_all_machines_have_precached(server) &&
                 server_data->countdown_state.paused == 0) {
        network_game_server_close_game((void *)server);
        if ((result = network_game_server_start_network_game((void *)server)) != 1)
          network_event("network_game_server_start_network_game() failed");
      } else if (now - server_data->countdown_state.last_countdown_message_time >
                 1000) {
        send_countdown_update = true;
      }

      if (send_countdown_update == 1) {
        struct {
          short seconds_to_start;
        } message_packet;
        void *msg;

        server_data->countdown_state.adjusted_time_this_tick = 0;
        if (ok_to_countdown) {
          int time_remaining = countdown_timer_get_time_remaining(
            server_data->countdown_state.timer);

          message_packet.seconds_to_start = (short)(time_remaining / 1000);
        } else {
          message_packet.seconds_to_start = -1;
        }
        msg = create_network_game_message(7, &message_packet, 2);
        if (msg) {
          if (network_game_server_send_message_to_all_machines((void *)server,
                                                                 msg)) {
            server_data->countdown_state.last_countdown_message_time = now;
          } else {
            network_event(
              "failed to send a message_server_pregame_countdown to all clients");
          }
        }
      }
    } else {
      int keep_alive_deadline = server_data->time_of_last_keep_alive + 5000;

      if (now > keep_alive_deadline) {
        struct {
          short field_00;
        } message_packet;
        void *msg;

        message_packet.field_00 = 0;
        msg = create_network_game_message(0xa, &message_packet, 2);
        network_game_server_send_message_to_all_machines((void *)server, msg);
        server_data->time_of_last_keep_alive = now;
      }
    }
  } else if (server_data->time_of_first_client_loading_completion) {
    if ((unsigned int)(system_milliseconds() -
                       server_data->time_of_first_client_loading_completion) >= 15000) {
      for (i = 0; i < 4; i++) {
        if ((server_data->client_machines[i].flags &
             NETWORK_CLIENT_MACHINE_FLAG(NETWORK_CLIENT_MACHINE_CONNECTED_BIT)) &&
            !(server_data->client_machines[i].flags &
              NETWORK_CLIENT_MACHINE_FLAG(NETWORK_CLIENT_MACHINE_LEVEL_LOADED_BIT))) {
          if (wide_to_ascii(
                (const wchar_t *)server_data->game
                  .machines[server_data->client_machines[i].machine_index],
                name_buf, 0x20)) {
            name = name_buf;
          } else {
            name = "<unknown name>";
          }
          network_event(
            "forcibly removing client system '%s' due to timeout while "
            "loading for game",
            name);
          if (!network_game_server_remove_client_machine_from_game(
                (void *)server, &server_data->client_machines[i])) {
            display_assert("removed",
                           "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                           0x94e, 1);
            system_exit(-1);
          }
        }
      }
      network_game_server_all_machines_have_loaded((void *)server);
    }
  }

  return result;
}

/* Dispose the network game server (0x12ea00).
 * Sends graceful exit messages based on current state (pregame or postgame),
 * handles remaining client machines, disconnects, clears the server struct,
 * and resets the in-use flag. */
void network_game_server_dispose(void *server)
{
  void *msg;
  void *server_addr;

  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x120, 1);
    system_exit(-1);
  }
  server_addr = server;
  switch (*(uint16_t *)((char *)server + 4)) {
  case 0:
    msg = create_network_game_message(9, &server_addr, 4);
    if (msg) {
      goto notify;
    }
    network_event(
      "failed to create a _message_type_server_graceful_game_exit_pregame message");
    break;
  case 2:
    msg = create_network_game_message(0x1f, &server_addr, 4);
    if (msg) {
      goto notify;
    }
    network_event(
      "failed to create a _message_type_server_graceful_game_exit_postgame message");
    break;
  }
  goto after_notify;

notify:
  if (network_game_server_send_message_to_all_machines(server, msg)) {
    network_event("notified all clients that we are going down");
  } else {
    network_event("failed to notify all clients that we are going down");
  }

after_notify:

  if (!network_game_server_handle_client_machines((int)server)) {
    error(2, "network_game_server_handle_client_machines() failed inside "
             "network_game_server_dispose()");
  }
  if (*(int *)server != 0) {
    network_connection_delete(*(int *)server);
  }
  SleepEx(1000, 0);
  FUN_00082b30();
  csmemset(server, 0, 0x4bc);
  if (!network_game_server_memory_do_not_use_directly_in_use) {
    display_assert("network_game_server_memory_do_not_use_directly_in_use",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x171, 1);
    system_exit(-1);
  }
  network_game_server_memory_do_not_use_directly_in_use = 0;
  network_event("network server disposed");
}

/* Main server tick function (0x12eb20).
 * Verifies network connectivity, validates the game, accepts new client
 * connections, handles the public endpoint, processes client machines,
 * and dispatches based on server state (pregame/ingame/postgame). */
#if defined(_MSC_VER) && !defined(__clang__)
/* The reference CALLs network_game_server_game_is_valid (`push esi; call`);
 * cl.exe /Ob2 inlines it here instead (`testb $0x2, 0x6(%esi)`). */
#pragma inline_depth(0)
#endif
bool network_game_server_idle(void *server)
{
  network_game_server *server_data = (network_game_server *)server;
  transport_address client_address;
  int new_client_connection;
  bool success = true;
  uint16_t state;

  if (!transport_network_available() && !(unsigned char)network_game_is_splitscreen_local()) {
    display_error_when_main_menu_loaded(6);
    error(2, "network connection went down!");
    success = false;
  } else if (network_game_server_game_is_valid(server)) {
    new_client_connection = 0;
    success = network_connection_idle(server_data->connection, 0, &new_client_connection);
    if (success == true) {
      if (new_client_connection != 0) {
        success = network_game_server_add_new_client((int)server, new_client_connection);
        if (success == true) {
          network_connection_get_address(new_client_connection, &client_address, 0);
          network_event(
            "new client connected from ip %s (validation pending)",
            transport_address_to_string(&client_address));
        } else {
          network_event("failed to add new client connection to the game");
          network_server_close_client_connection(server_data->connection, new_client_connection);
        }
      }
      success = network_game_server_handle_public_endpoint((int)server);
      if (success) {
        success = network_game_server_handle_client_machines((int)server);
        if (success) {
          state = server_data->state;
          switch (state) {
          case NETWORK_GAME_SERVER_STATE_PREGAME:
            success = network_game_server_idle_pregame_tasks((int)server);
            break;
          case NETWORK_GAME_SERVER_STATE_INGAME:
            break;
          case NETWORK_GAME_SERVER_STATE_POSTGAME:
            success = network_game_server_idle_postgame_tasks((int)server);
            break;
          default:
            network_event("unknown server state");
            success = false;
            break;
          }
        } else {
          network_event("network_game_server_handle_client_machines() failed");
        }
      } else {
        network_event("network_game_server_handle_public_endpoint() failed");
      }
    } else {
      network_event("network_connection_idle() failed");
    }
  } else {
    network_event("the server's game is invalid");
  }

  return success;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* Reset server to pregame state (0x12eca0).
 * If already in postgame, sends pregame reset message, toggles client
 * team assignments, clears per-machine flags, reinitializes game settings,
 * and attempts to start a new game cycle. */
bool network_game_server_reset_to_pregame(void *server)
{
  char *s;
  volatile bool result;
  int i;
  char *flags_ptr;
  void *msg;
  int data;

  s = (char *)server;
  result = false;
  data = 0;
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

  if (*(int16_t *)(s + 4) == 2) {
    /* Postgame -> pregame transition */
    msg = create_network_game_message(0x1e, &data, 4);
    if (msg && network_game_server_send_message_to_all_machines(s, msg)) {
      network_event("server resetting to pregame");

      /* Toggle client team assignments */
      if (*(char *)(s + 0xc8) != 0) {
        for (i = 0; i < 0x10; i++) {
          if (network_player_is_valid(s + 0x22e + i * 0x20)) {
            switch (*(char *)(s + 0x24c + i * 0x20)) {
            case 0:
              *(char *)(s + 0x24c + i * 0x20) = 1;
              break;
            case 1:
              *(char *)(s + 0x24c + i * 0x20) = 0;
              break;
            }
          }
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

      network_game_reset_for_next_round(s + 8, 0);

      if (network_game_server_setup_game_from_playlist((int)s)) {
        char game_settings[0x434]; /* name: PAL 2342 network_server_manager.c:3371 */

        csmemcpy(game_settings, s + 8, 0x434);
        msg = create_network_game_message(6, game_settings, 0x434);
        if (msg && network_game_server_send_message_to_all_machines(s, msg)) {
          *(int16_t *)(s + 4) = 0;
          result = true;
        }
      } else {
        /* Playlist ended -- send graceful exit */
        int shutdown_message = 0; /* name: PAL 2342 network_server_manager.c:3386 */

        msg = create_network_game_message(9, &shutdown_message, 4);
        if (msg && network_game_server_send_message_to_all_machines(s, msg) &&
            network_game_server_handle_client_machines((int)s)) {
          network_event("the playlist has ended - server going down");
        } else {
          network_event("the playlist has ended - server going down, but failed to "
                        "alert client machines");
        }
      }
    } else {
      network_event(
        "failed to signal all client machines to switch to pregame");
    }
  } else {
    result = network_game_server_setup_game_from_playlist((int)s);
    if (*(char *)(s + 0xc8) != 0) {
      for (i = 0; i < 0x10; i++) {
        if (network_player_is_valid(s + 0x22e + i * 0x20)) {
          switch (*(char *)(s + 0x24c + i * 0x20)) {
          case 0:
            *(char *)(s + 0x24c + i * 0x20) = 1;
            break;
          case 1:
            *(char *)(s + 0x24c + i * 0x20) = 0;
            break;
          }
        }
      }
    }
  }

  return result;
}

/* Initialize the global network server (0x12eef0).
 * Creates a connection, sets up game data, initializes machine slots. */
void *network_game_server_create(void)
{
  void *server = (void *)0x5a90e0;
  int i;

  if (network_game_server_memory_do_not_use_directly_in_use) {
    display_assert("!network_game_server_memory_do_not_use_directly_in_use",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0xe0, 1);
    system_exit(-1);
  }
  network_game_server_memory_do_not_use_directly_in_use = 1;
  csmemset((void *)0x5a90e0, 0, 0x4bc);
  network_game_server_connection = network_connection_new(1, 0x141e);
  if (network_game_server_connection) {
    FUN_00082a90();
    network_game_server_state = 0;
    network_game_server_flags = 2;
    csmemset((void *)0x5a90e8, 0, 0x434);
    network_connection_set_connection_rejection_procedure(network_game_server_connection,
                                                          (void *)network_game_server_reject_connection_game_is_full);
    network_game_invalidate((void *)0x5a90e8);
    network_game_server_difficulty = (short)main_get_difficulty();
    network_game_server_reset_counter = -1;
    for (i = 0; i < 4; i++) {
      *(int *)(0x5a951c + i * 0x10) = 0;
      *(int *)(0x5a9520 + i * 0x10) = 0;
      *(int *)(0x5a9524 + i * 0x10) = 0;
      *(unsigned short *)(0x5a9528 + i * 0x10) = 0xffff;
      *(unsigned short *)(0x5a952a + i * 0x10) = 0;
      network_game_invalidate_machine((void *)0x5a90e8, i);
    }
    network_game_server_loading_flag = 0;
    network_game_server_all_loaded_time = 0;
    if (!network_game_server_reset_to_pregame(server)) {
      error(2, "failed to initialize server pregame settings");
      network_game_server_dispose(server);
      server = 0;
    }
  } else {
    error(2, "failed to create the server connection");
    network_game_server_dispose(server);
    server = 0;
  }
  return server;
}

#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth(0)
#endif
bool network_game_server_handle_message_client_game_start_request(int server, int machine, void *message_data, int message_size)
{
  int packet_type;
  int packet_version;
  int countdown_time;

  if (network_game_server_get_state(server, (short *)0) == 0) {
    message_size -= 2;
    packet_type = 0x10;
    packet_version = 1;
    if (decode_network_game_message((int)&countdown_time, (int)((char *)message_data + 2),
                     (short *)&message_size, (short *)&packet_type,
                     (short *)&packet_version, 3)) {
      network_game_server_update_countdown((void *)server, countdown_time);
      return true;
    }
    network_event(
      "server failed to decode a message_client_game_start_request packet");
    return true;
  }
  network_event(
    "failed to handle a message_client_game_start_request because the "
    "server is not in pregame");
  return true;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* Handle map-precached notification from client (0x12f0d0). */
#if defined(_MSC_VER) && !defined(__clang__)
/* The reference makes real CALLs to network_game_server_get_state and
 * network_game_server_client_machine_is_precached; cl.exe /Ob2 inlines both
 * here (16 surplus instructions). Scoped inline_depth(0) matches the
 * original's call shape. */
#pragma inline_depth(0)
#endif
bool network_game_server_handle_message_client_map_is_precached_pregame(int server, int machine, void *message_data, int message_size)
{
  /* The reference stores both packet header fields as DWORDs
   * (`mov DWORD PTR [ebp-0x4],0x13`), and its frame is 0x108 bytes
   * (256 buffer + 2 * 4), so these two locals are int-width. */
  int packet_type;
  int packet_version;
  char decoded_buf[256];

  if (network_game_server_get_state(server, (short *)0) == 0) {
    message_size -= 2;
    packet_type = 0x13;
    packet_version = 1;
    if (decode_network_game_message((int)decoded_buf, (int)((char *)message_data + 2),
                     (short *)&message_size, (short *)&packet_type,
                     (short *)&packet_version, 3)) {
      network_game_server_client_machine_is_precached((int)server, machine,
                                                      (int)decoded_buf);
      return true;
    }
    network_event(
      "server failed to decode a message_type_client_map_is_precached_pregame "
      "packet");
    return true;
  }
  network_event(
    "failed to handle a message_type_client_map_is_precached_pregame "
    "because the server is not in pregame");
  return true;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth(0)
#endif
bool network_game_server_handle_message_client_loaded(int server, int client_machine, void *message, int message_size)
{
  bool result = true;
  int packet_type;
  int packet_version;
  int loaded;

  if (network_game_server_get_state(server, (short *)0) == 0) {
    message_size -= 2;
    packet_type = 0x18;
    packet_version = 1;
    if (decode_network_game_message((int)&loaded, (int)((char *)message + 2),
                     (short *)&message_size, (short *)&packet_type,
                     (short *)&packet_version, 5)) {
      network_game_server_client_machine_game_loading_complete((void *)server,
                                                                (void *)client_machine);
    } else {
      network_event("server failed to decode a message_client_loaded packet");
      result = false;
    }
  } else {
    network_event(
      "failed to handle a message_client_loaded message because the server is "
      "not in pregame");
    result = false;
  }
  return result;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* Handle add-player request ingame (0x12f200). */
#if defined(_MSC_VER) && !defined(__clang__)
/* The reference calls network_game_server_get_state and compares AX; keep
 * VC71 from inlining its server+4 field load into this handler. */
#pragma inline_depth(0)
#endif
bool network_game_server_handle_message_client_add_player_request_ingame(int server, int machine, void *message_data, int message_size)
{
  network_player_record_t player;
  short packet_type;
  short packet_version;

  if (network_game_server_get_state(server, (short *)0) == 1) {
    message_size -= 2;
    packet_type = 0x1a;
    packet_version = 1;
    if (decode_network_game_message((int)&player, (int)((char *)message_data + 2),
                     (short *)&message_size, &packet_type, &packet_version, 5)) {
      network_game_server_queue_player_for_addition(server, (int)&player);
      return true;
    }
    network_event(
      "server failed to decode a message_client_add_player_request_ingame "
      "packet");
    return true;
  }
  network_event(
    "failed to handle a message_client_add_player_request_ingame because "
    "the server is not in game");
  return true;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* Handle remove-player request postgame (0x12f290).
 * The reference makes a real CALL to network_game_server_get_state;
 * cl.exe /Ob2 inlines it here. Scoped inline_depth(0) matches the
 * original's call shape. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth(0)
#endif
bool network_game_server_handle_message_client_remove_player_request_postgame(int server, int machine, void *message_data, int message_size)
{
  network_player_record_t player;
  short packet_type;
  short packet_version;

  if (network_game_server_get_state(server, (short *)0) == 2) {
    message_size -= 2;
    packet_type = 0x20;
    packet_version = 1;
    if (decode_network_game_message((int)&player, (int)((char *)message_data + 2),
                     (short *)&message_size, &packet_type, &packet_version, 7)) {
      if (!network_game_server_remove_player_from_game(server, machine,
                                                       (int)&player))
        network_event("server failed to remove a network player post-game");
      return true;
    }
    network_event(
      "server failed to decode a message_client_remove_player_request_postgame "
      "packet");
    return true;
  }
  network_event(
    "failed to handle a message_client_remove_player_request_postgame "
    "because the server is not in post-game");
  return true;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* Handle client switch-to-pregame request (0x12f330). */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth(0)
#endif
char network_game_server_handle_message_client_switch_to_pregame(int server, int machine, void *message_data, int message_size)
{
  char decoded_buf[4];
  int packet_type;
  int packet_version;
  char result;

  result = 1;
  if (network_game_server_get_state(server, (short *)0) == 2) {
    message_size -= 2;
    packet_type = 0x21;
    packet_version = 1;
    if (decode_network_game_message((int)decoded_buf, (int)((char *)message_data + 2),
                     (short *)&message_size, (short *)&packet_type,
                     (short *)&packet_version,
                     7)) {
      result =
        (char)network_game_server_switch_machine_from_postgame_to_pregame(
          server, machine);
      if (!result) {
        network_event(
          "network_game_server_switch_machine_from_postgame_to_pregame() "
          "failed");
        return result;
      }
      result = 1;
    } else {
      network_event(
        "server failed to decode a message_client_remove_player_request_"
        "postgame packet");
    }
  } else {
    network_event(
      "failed to handle a message_client_switch_to_pregame because the server "
      "is not in post-game");
  }
  return result;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* Fastcall wrapper: write message via network_connection_write (0x12f3d0).
 * dest_address @<ecx>, size @<edx>, reliable @<eax>, stack: connection,
 * message. */
bool network_game_server_write(int dest_address, unsigned short size, int reliable,
                  void *connection, void *message)
{
  return network_connection_write(connection, message, size, dest_address,
                                  reliable);
}

/* Write a message to a machine's network connection (0x12f3f0).
 * Resolves machine→connection via get_machine_connection, then sends reliably.
 */
bool network_game_server_send_message_to_machine(int server, int machine, void *message)
{
  int connection;
  unsigned short msg_size;
  bool result;

  result = 0;
  connection = network_game_server_get_machine_connection(server, machine);
  if (connection) {
    msg_size = *(unsigned short *)message;
    result = network_connection_write((void *)connection, message,
                                      msg_size >> 4, 0, 1);
  }
  return result;
}

/* Broadcast a message to all connected client machines (0x12f430).
 * Iterates 4 machine slots, checks each is valid and alive, then copies
 * and sends the message. Returns false if any write fails. */
__declspec(noinline) bool network_game_server_send_message_to_all_machines(void *server, void *message)
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
    machine = network_game_server_get_client_machine_at_index((int)server, i);
    if (!network_game_server_client_machine_is_joined_to_game((int)server,
                                                              machine))
      continue;
    connection = network_game_server_get_client_connection((void *)machine);
    if (!connection)
      continue;
    if (!network_connection_active(connection))
      continue;
    if (msg_len > 0x600) {
      display_assert(
        "message_length<=sizeof(message_buffer)",
        "c:\\halo\\SOURCE\\networking\\network_server_message_handler.c", 0x19a,
        1);
      system_exit(-1);
    }
    csmemcpy(local_buf, message, msg_len);
    if (!network_connection_write((void *)connection, local_buf, msg_len, 0,
                                  1)) {
      network_event("network_game_server_write() failed in "
                       "network_game_server_send_message_to_all_machines()");
      result = false;
    }
  }

  return result;
}

typedef struct {
  unsigned int w[8];
} player_msg_t;

/* Copy player struct (0x20 bytes) and broadcast a player-joined message
 * (0x12f540). */
bool network_game_server_send_player_joined_info_ingame(int server, void *player)
{
  player_msg_t local_buf;
  int msg;
  bool sent;

  if (!server || !player) {
    display_assert(
      "server && player",
      "c:\\halo\\SOURCE\\networking\\network_server_message_handler.c", 0x1b0,
      1);
    system_exit(-1);
  }
  local_buf = *(player_msg_t *)player;
  msg = (int)create_network_game_message(0x15, &local_buf, 0x20);
  if (msg) {
    sent = network_game_server_send_message_to_all_machines((void *)server, (void *)msg);
    if (!sent)
      network_event(
        "network_game_server_send_message_to_all_machines() failed in "
        "network_game_server_send_player_joined_info_ingame()");
    return sent;
  }
  network_event(
    "failed to create a message_server_add_player_ingame message");
  return false;
}

/* Send updated game settings to all client machines (0x12f5d0).
 * Gets the game data pointer via network_game_server_get_game, copies 0x434
 * bytes into a local buffer, builds a type-6 message, and broadcasts it.
 * Returns true on success. */
bool network_game_server_send_game_data_pregame(void *server)
{
  network_game_blob_t message;
  network_game_blob_t *game_data;
  void *msg;
  bool result;

  result = false;
  if (!server) {
    display_assert(
      "server",
      "c:\\halo\\SOURCE\\networking\\network_server_message_handler.c", 0x1c8,
      true);
    system_exit(-1);
  }

  game_data = (network_game_blob_t *)network_game_server_get_game(server);
  if (game_data != 0) {
    csmemcpy(&message, game_data, sizeof(message));
    msg = create_network_game_message(6, &message, sizeof(message));
    if (msg != NULL) {
      result = network_game_server_send_message_to_all_machines(server, msg);
      if (!result) {
        network_event(
          "failed to send message_server_game_settings_update message to all "
          "machines");
      }
    } else {
      network_event(
        "failed to create a message_server_game_settings_update message");
    }
  } else {
    network_event(
      "failed to handle a message_server_game_settings_update because their "
      "was no server game");
  }

  return result;
}

/* Handle client broadcast game search (0x12f690).
 * client_message @<ecx>, source_address @<eax>, server on stack. */
/* 2276 game-advertise payload. PAL 2342 names these same fields; the 2276
 * stores at +0x34, +0x3a, +0x74, +0xf8..+0x104 establish their offsets. */
typedef struct {
  uint8_t client_nonce[8];
  uint8_t nonce[8];
  uint8_t key_id[8];
  uint8_t key[16];
  uint8_t xnaddr[12];
  uint16_t port;
  uint16_t version;
  uint16_t platform;
  uint16_t game_name[16];
  uint8_t reserved[0x1a];
  uint8_t map[0x84];
  uint16_t engine_type;
  uint16_t machine_count;
  uint16_t player_count;
  uint16_t maximum_player_count;
  uint16_t variant_setting;
  uint16_t flags;
  uint8_t join_game_token[16];
} message_server_game_advertise_t;
cs(message_server_game_advertise_t, 0x114);
co(message_server_game_advertise_t, port, 0x34);
co(message_server_game_advertise_t, game_name, 0x3a);
co(message_server_game_advertise_t, map, 0x74);
co(message_server_game_advertise_t, engine_type, 0xf8);
co(message_server_game_advertise_t, flags, 0x102);
co(message_server_game_advertise_t, join_game_token, 0x104);

char handle_message_client_broadcast_game_search(int server, void *client_message,
                                          void *source_address)
{
  message_server_game_advertise_t advertise_buf;
  union {
    int key[4];
    struct {
      int pad;
      int xnaddr[3];
    } x;
  } scratch;
  int addr_hdr[6];
  char *body;
  network_game_blob_t *game_data;
  int *key_ptr;
  int *xnaddr_ptr;
  void *msg;
  unsigned short msg_len;
  int connection;
  char result;

  if (!server || !source_address || !client_message) {
    display_assert(
      "server && source_address && client_message",
      "c:\\halo\\SOURCE\\networking\\network_server_message_handler.c", 0x21f,
      1);
    system_exit(-1);
  }
  if (*(short *)((char *)client_message + 2) != 1)
    return true;
  game_data = (network_game_blob_t *)network_game_server_get_game((void *)server);
  if (!game_data)
    return true;

#if defined(_MSC_VER) && !defined(__clang__)
  memset(&advertise_buf, 0, 0x114);
#else
  csmemset(&advertise_buf, 0, 0x114);
#endif
  body = (char *)&advertise_buf;

  addr_hdr[0] = -1;
  *(short *)((char *)addr_hdr + 0x10) = 4;
  *(short *)((char *)addr_hdr + 0x12) = 0x141f;

  csmemcpy(advertise_buf.client_nonce, (char *)client_message + 4, 8);
  transport_get_nonce(advertise_buf.nonce, 8);
  *(int64_t *)(body + 0x10) = transport_get_key_id();

  key_ptr = (int *)transport_get_key(scratch.key);
  *(int *)(body + 0x18) = key_ptr[0];
  *(int *)(body + 0x1c) = key_ptr[1];
  *(int *)(body + 0x20) = key_ptr[2];
  *(int *)(body + 0x24) = key_ptr[3];

  xnaddr_ptr = (int *)transport_get_xnaddr(scratch.x.xnaddr);
  *(int *)(body + 0x28) = xnaddr_ptr[0];
  *(int *)(body + 0x2c) = xnaddr_ptr[1];
  *(int *)(body + 0x30) = xnaddr_ptr[2];

  advertise_buf.port = 0x141e;
  advertise_buf.version = 1;
  advertise_buf.platform = 0;

  ustrncpy((wchar_t *)advertise_buf.game_name, (wchar_t *)game_data->map_name, 0xf);
  advertise_buf.engine_type = game_data->game_variant.engine_type;
  csmemcpy(advertise_buf.map, (char *)game_data + 0x20, 0x84);
  advertise_buf.player_count = game_data->player_count;
  advertise_buf.machine_count = game_data->machine_count;
  advertise_buf.maximum_player_count = (short)game_data->maximum_player_count;
  advertise_buf.variant_setting = *(short *)((char *)game_data + 0xe4);

  advertise_buf.flags = 0;
  if (game_data->game_variant.team_play == 1)
    advertise_buf.flags = 4;
  if (game_data->game_variant.engine_type == 3 &&
      *(int *)((char *)game_data + 0x100) == 2)
    advertise_buf.flags |= 8;
  if (network_game_server_game_is_open((void *)server))
    advertise_buf.flags |= 2;

  network_game_generate_join_game_token(advertise_buf.join_game_token);

  msg = create_network_game_message(2, body, 0x114);
  if (msg) {
    msg_len = *(unsigned short *)msg;
    connection = network_game_server_get_connection((void *)server);
    result = network_connection_write((void *)connection, msg, msg_len >> 4,
                                      (int)addr_hdr, 0);
    if (!result)
      network_event("network_game_server_write() failed in "
                       "handle_message_client_broadcast_game_search()");
    return result;
  }
  network_event(
    "failed to create a message_server_game_advertise message");
  return true;
}

/* Handle a ping message from a client - send pong response (0x12f8d0).
 * The reference calls network_game_server_get_connection() out of line
 * (call count 7); cl.exe /Ob2 inlines its trivial NULL-guard here, which
 * duplicates a nested display_assert call (our count 8). */
typedef struct {
  uint32_t timestamp;
  uint16_t port;
  uint8_t pad_06[2];
} message_client_ping_t;
cs(message_client_ping_t, 8);
co(message_client_ping_t, port, 4);

#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth(0)
#endif
char handle_message_client_ping(int server, void *decoded_msg, void *client_message)
{
  unsigned int pong_timestamp;
  transport_address reply_address;
  void *pong_msg;
  int connection;
  char result;

  result = false;
  if (!server || !client_message || !decoded_msg) {
    display_assert(
      "server && source_address && client_message",
      "c:\\halo\\SOURCE\\networking\\network_server_message_handler.c", 0x26a,
      1);
    system_exit(-1);
  }
  pong_timestamp = ((message_client_ping_t *)decoded_msg)->timestamp;
  pong_msg = create_network_game_message(3, &pong_timestamp, 4);
  if (pong_msg) {
    reply_address.address.ipv4_address = *(int *)client_message;
    reply_address.address_length = 4;
    reply_address.port = ((message_client_ping_t *)decoded_msg)->port;
    connection = network_game_server_get_connection((void *)server);
    result = network_connection_write(
      (void *)connection, pong_msg, *(unsigned short *)pong_msg >> 4,
      (int)&reply_address, 0);
    if (!result)
      network_event(
        "network_game_server_write() failed in handle_message_client_ping()");
  } else {
    network_event("failed to create a message_server_pong message");
  }
  return result;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif

/* Handle client join-game request (0x12f990).
 * server=stack, machine=ESI(thunk-provided), message/size=stack. */
#if defined(_MSC_VER) && !defined(__clang__)
/* The reference CALLs network_game_server_get_state and two other same-TU
 * helpers (call count 46 vs our 43); cl.exe /Ob2 inlines them here
 * (`cmpw $0x0, 0x4(%edi)`). */
#pragma inline_depth(0)
#endif
char network_game_server_handle_message_client_join_game_request(int server, void *machine, void *message, int message_size)
{
  char decode_buf[0x50]; /* [0x40]=name(wchar), [0x40..0x4f]=client_token */
  char expected_token[16];
  char addr_buf[0x18]; /* network_connection_get_address may clear 0x18 bytes */
  char host_line[0x20];
  int conn;
  /* Reference keeps this flag in a 1-byte slot (`movb $0x1, -0x1(%ebp)`;
   * `movb -0x1(%ebp),%al`; `testb %al,%al`), not a dword. */
  bool in_hosts;
  int machine_idx_out;
  void *msg;
  void *stream;
  unsigned short msg_len;
  char result;
  short reject_code;
  struct {
    int games;
    short idx;
  } accepted_data;
  short packet_type;
  short packet_ver;

  result = true;
  if (network_game_server_get_state(server, (short *)0) == 0) {
  message_size -= 2;
  packet_type = 0xc;
  packet_ver = 1;
  if (network_game_server_client_machine_is_joined_to_game(
        server, (int)(void *)machine)) {
    network_event("ignoring redundant join request from machine");
    return true;
  }
  if (!decode_network_game_message((int)decode_buf, (int)((char *)message + 2),
                    (short *)&message_size, &packet_type, &packet_ver, 3)) {
    network_event(
      "server failed to decode a message_client_join_game_request packet");
    return false;
  }
  conn = network_game_server_get_client_connection(machine);
  network_connection_get_address(conn, (void *)addr_buf, 0);
  if (network_game_server_get_state(server, (short *)0) == 0 &&
      network_game_server_game_is_open((void *)server)) {
    network_game_generate_join_game_token(expected_token);
    /* client token is at decode_buf[0x40..0x4f] */
    if (csmemcmp(decode_buf + 0x40, expected_token, 0x10) == 0) {
      wide_to_ascii((const wchar_t *)decode_buf, decode_buf, 0x40);
      stream = crt_fopen("d:\\hosts.txt", "r");
      if (stream) {
        csmemset(host_line, 0, 0x20);
        in_hosts = 0;
        while (crt_fgets(host_line, 0x20, stream) != NULL) {
          if (csstrncmp(decode_buf, host_line, csstrlen(decode_buf)) == 0) {
            in_hosts = 1;
            break;
          }
        }
        crt_fclose(stream);
        if (!in_hosts) {
          reject_code = 6;
          network_event(
            "server refused client '%s' because it is not in your hosts file",
            decode_buf);
          msg = create_network_game_message(5, &reject_code, 2);
          if (!msg)
            return false;
          msg_len = *(unsigned short *)msg;
          conn = network_game_server_get_client_connection(machine);
          network_connection_write((void *)conn, msg, msg_len >> 4, 0, 1);
          return false;
        }
      }
      result =
        network_game_server_accept_client_machine_into_game(server, machine);
      if (result) {
        int client_machine;

        machine_idx_out = -1;
        client_machine = network_game_server_get_client_machine(
          server, (int)machine, &machine_idx_out);
        network_game_server_get_game((void *)server);
        if (!client_machine || *(char *)(client_machine + 0x40) < 0 ||
            *(char *)(client_machine + 0x40) >= 4) {
          display_assert(
            "network_machine_is_valid(client_machine)",
            "c:\\halo\\SOURCE\\networking\\network_server_message_handler.c",
            0x2ce, 1);
          system_exit(-1);
        }
        accepted_data.idx = machine_idx_out;
        accepted_data.games = network_game_get_random_seed();
        msg = create_network_game_message(4, &accepted_data, 8);
        if (!msg)
          return false;
        msg_len = *(unsigned short *)msg;
        conn = network_game_server_get_client_connection(machine);
        result =
          network_connection_write((void *)conn, msg, msg_len >> 4, 0, 1);
        if (!result)
          network_event(
            "network_game_server_write() failed in "
            "network_game_server_handle_message_client_join_game_request()");
        else
          network_event("sent _message_type_server_machine_accepted message "
                           "to %d",
                           (int)machine_idx_out);
        if (result == 1) {
          result = network_game_server_send_game_data_pregame((void *)server);
          if (!result)
            network_event(
              "network_game_server_send_game_data_pregame() failed in "
              "network_game_server_handle_message_client_join_game_request()");
          return result;
        }
        return result;
      }
      reject_code = 5;
      network_event(
        "server failed to accept valid client machine '%s' @%s into the game",
        decode_buf, transport_address_to_string((void *)addr_buf));
    } else {
      reject_code = 2;
      network_event(
        "client machine '%s' @%s tried to join game with a bad join token",
        decode_buf, transport_address_to_string((void *)addr_buf));
    }
  } else {
    reject_code = 5;
    network_event(
      "client machine '%s' @%s tried to join game when they should not be",
      decode_buf, transport_address_to_string((void *)addr_buf));
  }
  msg = create_network_game_message(5, &reject_code, 2);
  if (!msg) {
    network_event(
      "failed to create a message_server_machine_rejected message");
    return false;
  }
  msg_len = *(unsigned short *)msg;
  conn = network_game_server_get_client_connection(machine);
  result = network_connection_write((void *)conn, msg, msg_len >> 4, 0, 1);
  if (!result)
    network_event(
      "network_game_server_write() failed while sending a rejection reply");
  result = false;
  }
  return result;
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma inline_depth()
#endif
