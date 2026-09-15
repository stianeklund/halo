/* 0x124900 — Walks the tag_block at offset 0xd0 of the definition, and for
 * each 0x30-byte element walks the nested tag_block at element+0x24 whose
 * elements are 0x68 bytes. Both retrieved elements are discarded: only
 * tag_block_get_element's bounds-check side effects are observed. Both loop
 * counters are 16-bit and are sign-extended before the comparison against the
 * 32-bit block count, which is re-read from memory on every iteration
 * (MOVSX EAX,AX / MOVSX EAX,DI at 0x12494f and 0x12495f).
 * Parameter is the stack slot at [EBP+8]; there are no direct callers in the
 * binary, so the pointee type is unknown. */
void model_build_tangent_matrices(void *definition)
{
  char *block;
  int *sub_block;
  int16_t outer_index;
  int16_t inner_index;

  block = (char *)definition + 0xd0;
  outer_index = 0;
  if (*(int *)block > 0) {
    do {
      sub_block =
        (int *)((char *)tag_block_get_element(block, (int)outer_index, 0x30) +
                0x24);
      inner_index = 0;
      if (*sub_block > 0) {
        do {
          tag_block_get_element(sub_block, (int)inner_index, 0x68);
          inner_index = (int16_t)(inner_index + 1);
        } while ((int)inner_index < *sub_block);
      }
      outer_index = (int16_t)(outer_index + 1);
    } while ((int)outer_index < *(int *)block);
  }
}

/* 0x1249b0 — network_game_client_dispose.
 * Tears down the network game client connection. If the server pointer is
 * non-null, closes its connection handle and clears the in-use flag.
 *
 * noinline (VC71 verification only): the original build emits this as a real
 * call from network_game_client_create (CALL 0x001249b0 at 0x127061). /O2's implied /Ob2
 * otherwise inlines the whole body — including the line-0xb2 assert — into
 * that failure path, adding 14 instructions that the reference does not have.
 * network_game_client_create is its only in-TU caller. */
__declspec(noinline) void network_game_client_dispose(void *server)
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
  network_event("network client disposed");
}

/* 0x124a10 — network_game_client_keep_alive.
 * Forwards the client's connection handle (int at +0x82c) to
 * network_connection_keep_alive. The whole body is
 * MOV EAX,[EBP+8] / MOV ECX,[EAX+0x82c] / PUSH ECX / CALL 0x128d20 /
 * ADD ESP,4 — no null check, no return value. The parameter is the stack
 * slot at [EBP+8]; the pointee type is unknown (no callers in the binary),
 * so it stays void * like the rest of this TU. */
void network_game_client_keep_alive(void *client)
{
  network_connection_keep_alive(*(int *)((char *)client + 0x82c));
}

/* 0x124a30 — Returns the connection state (int16_t at offset 0xca6) and
 * optionally writes elapsed-time percentage into out_param. The time
 * calculation divides (current_ms - stored_ms) * 100 by 120000.
 *
 * noinline (VC71 verification only): small enough for clang -O3 to inline
 * at some call sites (confirmed at network_game_client_handle_message_server_game_advertise, 0x127260), producing an
 * inlined NULL-check + assert_halt in the caller where the reference makes
 * a real CALL. */
__declspec(noinline) int16_t network_game_client_get_state(void *server,
                                                           void *out_param)
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

/* network_game_client_initiate_join_game (0x124aa0)
 *
 * Starts an outbound connection attempt to an advertised game. One combined
 * assert guards the whole body (0x124aa8-0x124ae4, all failing branches jump
 * to the display_assert at 0x124af7): client non-null, client->state
 * (int16_t at +0xca6) == searching (0), game non-null, join_parameters
 * non-null, client->connection (int at +0x82c) non-null, the connection not
 * already connected, and game->platform (int16_t at +0xde) equal to the local
 * platform. The assert text names network_game_get_local_platform(), but the
 * compiled comparison is against the immediate 0 (CMP word ptr [EBX+0xde],0x0
 * at 0x124adc), so the local platform folded to 0 at compile time.
 *
 * On success it marks +0xc90 = 1, clears the transport-server slot at +0x830,
 * stamps the attempt time at +0x834 with system_milliseconds(), copies the
 * 0x22-byte join_parameters block into the client at +0x838, and asks the
 * connection layer to connect. The address argument (the fourth stack slot,
 * [EBP+0x14]) is only loaded after the csmemcpy — EDI holds join_parameters
 * up to that point (MOV EDI,[EBP+0x14] at 0x124b34).
 *
 * Returns the connect result in AL (MOV AL,BL on both exit paths). */
bool network_game_client_initiate_join_game(void *client, void *game,
                                            void *join_parameters,
                                            void *address)
{
  bool connected;

  if (client == NULL || *(int16_t *)((char *)client + 0xca6) != 0 ||
      game == NULL || join_parameters == NULL ||
      *(int *)((char *)client + 0x82c) == 0 ||
      network_connection_connected(*(int *)((char *)client + 0x82c)) ||
      *(int16_t *)((char *)game + 0xde) != 0) {
    display_assert("client && (client->state == "
                   "_network_game_client_state_searching) && game && "
                   "join_parameters && client->connection && "
                   "!network_connection_connected(client->connection) && "
                   "(game->platform == network_game_get_local_platform())",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x157, true);
    system_exit(-1);
  }

  *(int *)((char *)client + 0xc90) = 1;
  *(int *)((char *)client + 0x830) = 0;
  *(int *)((char *)client + 0x834) = (int)system_milliseconds();
  csmemcpy((char *)client + 0x838, join_parameters, 0x22);

  connected = network_connection_connect(*(int *)((char *)client + 0x82c),
                                         (int)address, 0);
  if (connected) {
    *(int16_t *)((char *)client + 0xca6) = 1;
    network_event("attempting to connect to game @ %s",
                     transport_address_to_string(address));
  } else {
    display_error_when_main_menu_loaded(7);
    network_event("failed attempt to initiate a connection to game @ %s",
                     transport_address_to_string(address));
  }
  return connected;
}

/* network_game_client_set_machine (0x124ba0)
 *
 * Copies a 0x44-byte network machine record into the client's embedded machine
 * array slot selected by the client's own 16-bit index at offset 0. The array
 * base (+0x970) and the 0x44 stride are the same ones
 * network_game_client_get_machine (0x124c10) reads back.
 *
 * One combined assert guards the body (0x124ba7-0x124bc2; every failing branch
 * jumps to the display_assert at 0x124bd5): client non-null, the client's
 * 16-bit index at +0 unsigned-below 4 (CMP word ptr [ESI],0x4 / JNC), machine
 * non-null, and the machine's own index byte at +0x40 in [0,4) compared as a
 * SIGNED char (MOV AL,[EDI+0x40] / TEST AL,AL / JL / CMP AL,0x4 / JL) — the
 * assert text spells that last pair as network_machine_is_valid(machine).
 *
 * Returns true unconditionally on the success path (MOV AL,0x1 at 0x124bfe);
 * the assert path does not return. */
bool network_game_client_set_machine(void *client, void *machine)
{
  unsigned short machine_index;

  if (client == NULL || *(unsigned short *)client >= 4 || machine == NULL ||
      *(signed char *)((char *)machine + 0x40) < 0 ||
      *(signed char *)((char *)machine + 0x40) >= 4) {
    display_assert("client && (client->machine_index<"
                   "MAXIMUM_NETWORK_MACHINE_COUNT) && "
                   "network_machine_is_valid(machine)",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x1e1, true);
    system_exit(-1);
  }

  machine_index = *(unsigned short *)client;
  csmemcpy((char *)client + 0x970 + machine_index * 0x44, machine, 0x44);
  return true;
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

/* network_game_client_get_machine_index (0x124c40)
 *
 * Asserts client is non-null and returns the client's 16-bit value at +0.
 */
uint16_t network_game_client_get_machine_index(void *client)
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

/* network_game_client_get_available_games (0x124c80)
 *
 * Asserts client is non-null, then returns the address of the client field at
 * offset 4 (LEA EAX,[ESI+0x4] at 0x124cab). The assert message string at
 * 0x2917a8 is "client" and the recovered assert line is 0x2ac. The pointee
 * type at +4 is not established by this function; the sole caller is
 * server_list_menu_update (0xf0f73), so the field meaning is unknown.
 */
void *network_game_client_get_available_games(void *client)
{
  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x2ac, client);

  return (void *)((char *)client + 4);
}

/* 0x124cc0 — Asserts client is non-null and returns the int16_t field at
 * offset 0xca8. */
int16_t network_game_client_get_error(void *server)
{
  assert_halt(server);
  return *(int16_t *)((char *)server + 0xca8);
}

/* network_game_client_get_seconds_to_game_start (0x124d00)
 *
 * Asserts client is non-null and returns the 16-bit field at +0xca4.
 * Evidence (0x124d00-0x124d34): MOV ESI,[EBP+8]; TEST ESI,ESI; JNZ over the
 * assert block, which pushes ("client", "c:\halo\SOURCE\networking\
 * network_client_manager.c", 0x2bc, true) to display_assert (0x8d9f0) then
 * system_exit(-1) (0x8e2f0). The value is loaded with a bare `MOV AX,word ptr
 * [ESI+0xca4]` — no MOVSX/MOVZX — so the return width is exactly 16 bits and
 * the signedness is NOT established by this function; int16_t is chosen to
 * match the sibling accessor at +0xca8 (network_game_client_get_error) on the same struct.
 * The meaning of the field at +0xca4 is unknown; its three callers
 * (splitscreen_pregame_status_screen_update 0xf1f6c,
 * multiplayer_game_directions 0xf34b5, network_pregame_status_screen_update 0xf17be) are UI
 * update paths.
 */
int16_t network_game_client_get_seconds_to_game_start(void *client)
{
  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x2bc, client);

  return *(int16_t *)((char *)client + 0xca4);
}

/* 0x124d40 — Thin wrapper that tail-calls network_connection_write with the
 * same five arguments. The prologue sets up a frame (PUSH EBP / MOV EBP,ESP)
 * and immediately tears it down (POP EBP / JMP 0x128e00), so every argument
 * passes through to the callee unchanged. In the one observed call site
 * (network_game_client_end_frame), the caller resolves a server handle to a
 * connection pointer via network_game_client_get_connection, then
 * calls this wrapper with the resulting connection pointer, a message buffer,
 * its size, a dest_address, and reliable=0. */
bool network_game_client_write(void *connection, void *message,
                               unsigned short size, int dest_address,
                               int reliable)
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
__declspec(noinline) void network_game_client_game_out_of_sync(void *client)
{
  int16_t player_index;

  if (*(char *)0x46e8b8 == '\0') {
    network_event("local machine is out of sync with the server");
    if (*((char *)client + 0xcac) == '\0') {
      player_index = local_player_get_next(-1);
      while (player_index != -1) {
        display_error(8, player_index, 1, 0);
        player_index = local_player_get_next(player_index);
      }
    }
    *((char *)client + 0xcac) = 1;
  }
}

/* network_game_client_ponged (0x124e90)
 *
 * Handles a received pong reply. Asserts client and source_address are
 * non-null. Ignores the pong unless the client is actively pinging
 * (flag byte at +0x82a set) and the reply's source machine id (int at
 * *source_address) matches the client's expected ping target (int at
 * +0x808). If echo_time (the timestamp echoed back by the remote) is in
 * the future relative to system_milliseconds(), logs and ignores it
 * (clock skew / bad data). Otherwise updates a running average round-trip
 * time at +0x828 (ushort) using the running sample count at +0x826
 * (ushort, incremented every call): new_avg = (old_avg*old_count -
 * echo_time + now) / (old_count+1). */
void network_game_client_ponged(void *client, void *source_address,
                                unsigned int echo_time)
{
  unsigned int now;
  unsigned short count;

  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x307, client && source_address);

  if (*(char *)((char *)client + 0x82a) != 0 &&
      *(int *)((char *)client + 0x808) == *(int *)source_address) {
    now = system_milliseconds();
    if (echo_time <= now) {
      count = *(unsigned short *)((char *)client + 0x826);
      *(unsigned short *)((char *)client + 0x828) =
        (unsigned short)((((unsigned int)*(unsigned short *)((char *)client +
                                                             0x828) *
                             (unsigned int)count -
                           echo_time) +
                          now) /
                         (count + 1));
      *(unsigned short *)((char *)client + 0x826) = count + 1;
    } else {
      network_event("received a pong from the future");
    }
  } else {
    network_event("received a pong from a system we aren't interested in");
  }
}

/* network_game_client_accepted_into_game (0x124f40)
 *
 * Handles message_server_machine_accepted. Asserts client, source_address and
 * message_packet are non-null and that the client is still in state 1
 * (joining, int16 at +0xca6). The message carries the game's random seed
 * (dword at +0) and our assigned machine index (int16 at +4). An index outside
 * [0, 4) is rejected with a log line and nothing else happens.
 *
 * On acceptance the index is stored twice: as the client's own machine index
 * (int16 at +0x0) and as the index byte at +0x40 of the machine record
 * selected by that index in the client's machine array (base +0x970, stride
 * 0x44 from IMUL EAX,EAX,0x44 at 0x124fa8; 0x970 + 0x40 = the 0x9b0 in the
 * store at 0x124fab — same array network_game_client_set_machine fills). The
 * client
 * moves to state 2 (pregame), seeds the shared RNG from the message, then
 * replies with a message_client_settings_request: a 0x44-byte struct whose
 * leading bytes are filled by network_game_generate_local_machine_name and
 * whose byte at +0x40 (EBP-0x4, inside the EBP-0x44 buffer) is the machine
 * index. The encoded message is written to the client's connection (+0x82c)
 * reliably (dest_address 0); the size is the encoded header's uint16 at +0
 * shifted right 4 (unsigned, XOR ECX,ECX / MOV CX,[EAX] / SHR CX,0x4).
 *
 * The four cdecl call sites between 0x124fbb and 0x124fe9 share one deferred
 * ADD ESP,0x1c at 0x124fee; that is MSVC stack-cleanup batching, not a 7-arg
 * call to create_network_game_message.
 */
void network_game_client_accepted_into_game(void *client, void *source_address,
                                            void *message_packet)
{
  char settings_request[0x44]; /* EBP-0x44 */
  int16_t machine_index;
  void *message;

  if (client == NULL || source_address == NULL || message_packet == NULL ||
      *(int16_t *)((char *)client + 0xca6) != 1) {
    display_assert("client && source_address && message_packet && "
                   "(client->state == _network_game_client_state_joining)",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x327, true);
    system_exit(-1);
  }

  machine_index = *(int16_t *)((char *)message_packet + 4);
  if (machine_index < 0 || machine_index >= 4) {
    network_event("received a message_server_machine_accepted message with "
                     "a bad machine_index");
  } else {
    *(int16_t *)client = machine_index;
    *((char *)client + *(int16_t *)((char *)message_packet + 4) * 0x44 +
      0x9b0) = *(char *)((char *)message_packet + 4);
    *(int16_t *)((char *)client + 0xca6) = 2;
    network_game_set_random_seed(*(int *)message_packet);
    network_event("successfully joined a net game; our machine is #%d",
                     (int)*(int16_t *)((char *)message_packet + 4));
    network_game_generate_local_machine_name(settings_request);
    settings_request[0x40] = *(char *)((char *)message_packet + 4);
    message = create_network_game_message(0xf, settings_request, 0x44);
    if (message == NULL) {
      network_event(
        "failed to create a message_client_settings_request message");
      return;
    }
    if (!network_connection_write(
          *(void **)((char *)client + 0x82c), message,
          (unsigned short)(*(unsigned short *)message >> 4), 0, true)) {
      network_event("network_game_client_write() failed while sending a "
                       "message_client_settings_request message");
      return;
    }
  }
}

/* network_game_client_game_settings_updated (0x125050)
 *
 * Applies a message_server_game_settings_update packet to the client. Asserts
 * both pointers, then range-checks the packet's machine count (int16 at +0x112,
 * valid 0..4) and player count (int16 at +0x224, valid 0..0x10); an
 * out-of-range packet is logged and rejected with 0 (XOR AL,AL at 0x125179).
 *
 * When the packet's map name (+0x24) differs from the client's current one
 * (+0x880), the map is logged and precached. The 0x434-byte settings block is
 * then rotated: the client's current block (+0x85c) is saved into the single
 * 0x434-byte stack buffer at [EBP-0x434], the packet overwrites +0x85c, and the
 * last 4 bytes of the saved block ([EBP-4] = saved_settings+0x430) are written
 * to +0xc8c. Ghidra shows that source as a separate `local_8`; it is inside the
 * buffer — SUB ESP,0x434 with the first csmemcpy filling [EBP-0x434, EBP).
 *
 * Both trailing logs take the player count first and the machine count second
 * (PUSH ECX/PUSH EAX at 0x12512d and 0x125147). Returns 1 (MOV AL,1). */
bool network_game_client_game_settings_updated(void *client,
                                               void *message_packet)
{
  char saved_settings[0x434];
  char *map_name;
  int16_t machine_count;

  if (client == NULL || message_packet == NULL) {
    display_assert("client && message_packet",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x375, true);
    system_exit(-1);
  }

  machine_count = *(int16_t *)((char *)message_packet + 0x112);
  if (machine_count < 0 || machine_count > 4 ||
      *(int16_t *)((char *)message_packet + 0x224) < 0 ||
      *(int16_t *)((char *)message_packet + 0x224) > 0x10) {
    network_event("invalid message_server_game_settings_update message "
                     "received player count %d machine count %d",
                     (int)*(int16_t *)((char *)message_packet + 0x224),
                     (int)machine_count);
    return false;
  }

  map_name = (char *)message_packet + 0x24;
  if (csstrcmp(map_name, (char *)client + 0x880) != 0) {
    network_event("precaching map '%s'...", map_name);
    main_set_multiplayer_map_name(map_name);
  }

  csmemcpy(saved_settings, (char *)client + 0x85c, 0x434);
  csmemcpy((char *)client + 0x85c, message_packet, 0x434);
  csmemcpy((char *)client + 0xc8c, saved_settings + 0x430, 4);

  network_event("received updated game settings from the server; there are "
                   "%d players on %d machines in the game",
                   (int)*(int16_t *)((char *)message_packet + 0x224),
                   (int)*(int16_t *)((char *)message_packet + 0x112));
  network_event("player count %d machine count %d",
                   (int)*(int16_t *)((char *)message_packet + 0x224),
                   (int)*(int16_t *)((char *)message_packet + 0x112));
  return true;
}

/* unstrip_player_index (0x125180)
 *
 * Given a 16-bit player index (the low half of a player datum handle, as
 * stored in the client's player records), recovers the full salted handle by
 * walking the player data array and comparing low halves.
 *
 * Confirmed from disassembly:
 *   - 0x125186 MOV EAX,[0x005aa6d4] then PUSH EAX / PUSH &iter → the global is
 *     dereferenced before data_iterator_new (player_data, kb 0x5aa6d4).
 *   - 0x125192 OR EDI,0xffffffff materializes the NONE return before the
 *     first iterator call; 0x1251d2 MOV EAX,EDI is the loop-exhausted path.
 *   - 0x1251aa MOV ESI,[EBP+8] / AND ESI,0xffff is the parameter mask,
 *     hoisted out of the loop (loop-invariant); the JNZ at 0x1251d0 targets
 *     0x1251b3, which re-reads iter.datum_handle ([EBP-0x8] = iter+0x8) on
 *     every iteration.
 *   - On a match (JZ 0x1251c0 → 0x1251d4) EAX still holds the *unmasked*
 *     iter.datum_handle, so the return value is the full handle, not the
 *     index. Confirmed by the caller at 0x1255ac (see below), which uses the
 *     EAX return as a player handle.
 *   - ADD ESP,0xc at 0x1251a3 is MSVC coalescing data_iterator_new's two
 *     stack args with the first data_iterator_next's one arg; both callees are
 *     plain cdecl (the ARG_COUNT audit hazard is that coalescing, not a real
 *     three-argument call).
 */
int unstrip_player_index(int player_index)
{
  data_iter_t iter;
  void *player;

  data_iterator_new(&iter, player_data);
  for (player = data_iterator_next(&iter); player != NULL;
       player = data_iterator_next(&iter)) {
    if ((iter.datum_handle & 0xffff) == (uint32_t)(player_index & 0xffff))
      return (int)iter.datum_handle;
  }

  return NONE;
}

/* network_game_client_game_has_started (0x1251e0)
 *
 * Asserts client && client->state == _network_game_client_state_pregame
 * (line 0x3b0), marks +0xca4 = 0xffff, keeps the connection alive, then loads
 * the game objects (network_game_create_game_objects(client+0x85c)) — on
 * failure logs and returns early.
 *
 * On success, scans the client's 16-entry 0x20-byte player table
 * (base client+i*0x20, i=0..15) for the first entry whose byte at +0xa9e
 * equals the target ushort at client+0. If found, walks the singly-linked
 * chain of matching entries (each entry's +0xabe byte, when equal to the
 * (re-read) target, points to the next entry 0x20 bytes further): for each
 * live entry (network_player_is_valid(entry+0xa82)) it resolves the full
 * player handle (unstrip_player_index(entry+0xaa1)) and binds it to the
 * local player slot (local_player_set_player_index(entry+0xa9f, handle)).
 *
 * Confirmed from disassembly (0x1251e0-0x12537a):
 *  - the second push to local_player_set_player_index reuses EAX's high 16
 *    bits left over from the unstrip_player_index call (MOVSX AX only sets
 *    the low half) — garbage in the original, harmless here since the
 *    parameter is declared unsigned __int16 and only the low 16 bits are
 *    ever read.
 *  - the two calls' stack cleanup is batched into one `ADD ESP,0xc` after
 *    both return, not one push/cleanup pair per call — a coalescing
 *    artifact, not a 3-argument call.
 *
 * Keeps the connection alive again, then encodes and sends an empty (4-byte
 * zeroed) message_client_loaded (type 0x18). On write success: logs, sets
 * state = _network_game_client_state_ingame (3), clears +0xc98/+0xc9c/+0xcad,
 * closes all UI widgets, and starts the game clock/pulse
 * (game_time_start/game_initial_pulse). Every exit path (assert aside)
 * returns `client->state == _network_game_client_state_ingame`. */
char network_game_client_game_has_started(void *client)
{
  char *c;
  int target;
  int i;
  char *entry;
  unsigned short *packet;
  signed char next;

  c = (char *)client;
  if (client == NULL || *(int16_t *)(c + 0xca6) != 2) {
    display_assert(
      "client && (client->state == _network_game_client_state_pregame)",
      "c:\\halo\\SOURCE\\networking\\network_client_manager.c", 0x3b0, 1);
    system_exit(-1);
  }

  *(int16_t *)(c + 0xca4) = -1;
  network_connection_keep_alive(*(int *)(c + 0x82c));

  if (network_game_create_game_objects(c + 0x85c)) {
    target = (int)*(uint16_t *)c;
    i = 0;
    entry = c + 0xa9e;
    do {
      if ((int)*(signed char *)entry == target)
        goto found_player;
      i++;
      entry += 0x20;
    } while (i < 16);
    goto skip_players;

  found_player:
    entry = c + (i << 5);
    if ((int)*(signed char *)(entry + 0xa9e) == target) {
      do {
        if (!network_player_is_valid(entry + 0xa82))
          break;
        local_player_set_player_index(
          (signed char)*(entry + 0xa9f),
          unstrip_player_index(*(signed char *)(entry + 0xaa1)));
        next = *(signed char *)(entry + 0xabe);
        target = (int)*(uint16_t *)c;
        entry += 0x20;
      } while ((int)next == target);
    }

  skip_players:

    network_connection_keep_alive(*(int *)(c + 0x82c));
    client = 0;
    packet = (unsigned short *)create_network_game_message(0x18, &client, 4);
    if (packet != NULL) {
      if (network_connection_write(*(void **)(c + 0x82c), packet, *packet >> 4,
                                   0, true)) {
        network_event("local machine is loaded & ready to play");
        *(int16_t *)(c + 0xca6) = 3;
        *(int *)(c + 0xc98) = 0;
        *(int *)(c + 0xc9c) = 0;
        *(char *)(c + 0xcad) = 0;
        ui_widgets_close_all();
        game_time_start();
        game_initial_pulse();
        return *(int16_t *)(c + 0xca6) == 3;
      }
      network_event("network_game_client_write() failed while sending a "
                       "message_client_loaded message");
    } else {
      network_event("failed to create a message_client_loaded message");
    }
  } else {
    network_event("failed to load the necessary game data");
  }

  return *(int16_t *)(c + 0xca6) == 3;
}

/* network_game_client_handle_game_update (0x125380)
 *
 * Asserts client && message_packet (line 0x40d). If the packet's slot count
 * (int16 at message+0xe) is smaller than the client's current slot count
 * (int16 at client+0xa80), zero-fills the packet's slot array from that
 * point on (message+0x10 + old_count*0x20, for (new_count-old_count)*0x20
 * bytes) and bumps the packet's count up to match — growing the packet's
 * slot table in place before it is copied out below.
 *
 * If the packet's sequence number (uint at message+0) matches the client's
 * expected update counter (client+0xc98) and we are not also hosting
 * (global_network_game_server_get() == NULL), runs three independent sanity checks
 * against game_time_get()/get_random_seed() and logs (never fatal) on:
 *  - a time/update-number mismatch that isn't itself a bug (log only).
 *  - a client/server random seed mismatch when game_time_get() lines up with
 *    the packet's game-time field (message+8) — also calls
 *    network_game_client_game_out_of_sync(client).
 *  - falling behind by a multiple of 30 game ticks (log only).
 * If the sequence number does NOT match the expected counter, logs a single
 * "missed a server update" message and calls
 * network_game_client_game_out_of_sync(client) unconditionally.
 *
 * Regardless of the above, copies the packet into the client action
 * ring-buffer via update_client_handle_server_update(&{count,data}, sequence_number):
 * update_client_handle_server_update does `csmemcpy(slot, data, 0x204)`, i.e. it reads a full
 * contiguous 0x204-byte {int16 count; int16 pad; uint8 data[512];} region
 * starting at its `data` argument — the original's two adjacent locals
 * (count at EBP-0x204, the 512-byte data buffer immediately after at
 * EBP-0x200) form that region only because MSVC happened to place them
 * back-to-back; clang gives no such guarantee for two separate locals, so
 * they are combined into one `update_buf` array here to keep the same
 * memory shape update_client_handle_server_update depends on (buffer-alias hazard, not a
 * cosmetic merge — see lift-decompiler-traps §5).
 *
 * Finally increments client->0xc98 and stamps client->0xc9c with
 * system_milliseconds() (Ghidra's decompile misrenders this call site as
 * `thunk_FUN_001d0581()`; disassembly at 0x1254f8 shows a direct
 * `CALL 0x0008e370`, which is system_milliseconds). Always returns true. */
char network_game_client_handle_game_update(void *client, void *message)
{
  char *c;
  char *m;
  int16_t msg_count;
  int16_t client_count;
  unsigned int seq;
  unsigned char update_buf[0x204];

  c = (char *)client;
  m = (char *)message;
  if (client == NULL || message == NULL) {
    display_assert("client && message_packet",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x40d, 1);
    system_exit(-1);
  }

  msg_count = *(int16_t *)(m + 0xe);
  client_count = *(int16_t *)(c + 0xa80);
  if (msg_count < client_count) {
    csmemset(m + msg_count * 0x20 + 0x10, 0, (client_count - msg_count) * 0x20);
    *(int16_t *)(m + 0xe) = client_count;
  }

  if (*(int *)m == *(int *)(c + 0xc98)) {
    if (global_network_game_server_get() == NULL) {
      if (game_time_get() == *(int *)m && game_time_get() != *(int *)(m + 8)) {
        network_event("not a bug, but update %d time %d our time %d",
                         *(int *)m, *(int *)(m + 8), game_time_get());
      }
      if (game_time_get() == *(int *)(m + 8)) {
        if (*(unsigned int *)(m + 4) != get_random_seed()) {
          network_event(
            "out of sync: client/server random seed mismatch, update= "
            "#%ld, game time= #%ld (%ld) (#%lx/#%lx)",
            *(int *)m, game_time_get(), *(int *)(m + 8), get_random_seed(),
            *(unsigned int *)(m + 4));
          network_game_client_game_out_of_sync(client);
        }
      }
      if (*(unsigned int *)m % 30 == 0) {
        network_event(
          "client is lagging behind the server by #%d game ticks",
          *(int *)m - game_time_get());
      }
    }
  } else {
    network_event(
      "out of sync: missed a server update (expected #%ld, got #%ld)",
      *(int *)(c + 0xc98), *(int *)m);
    network_game_client_game_out_of_sync(client);
  }

  seq = *(unsigned int *)m;
  msg_count = *(int16_t *)(m + 0xe);
  *(uint16_t *)update_buf = (uint16_t)msg_count;
  csmemcpy(update_buf + 4, m + 0x10, (unsigned int)msg_count << 5);
  update_client_handle_server_update(update_buf, seq);

  *(int *)(c + 0xc98) = *(int *)(c + 0xc98) + 1;
  *(int *)(c + 0xc9c) = system_milliseconds();

  return 1;
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

  if (network_player_is_valid(player)) {
    added = network_game_add_player((char *)client + 0x85c, player);
    if (added) {
      if (*(int16_t *)((char *)client + 0xca6) == 3) {
        player =
          (char *)client + 0xa62 + (*(int16_t *)((char *)client + 0xa80) << 5);
        added = network_game_spawn_player(player);
        if (!added) {
          return added;
        }
        player_handle = unstrip_player_index((signed char)player[0x1f]);
        if ((int)(signed char)player[0x1c] == (int)*(uint16_t *)client) {
          local_player_set_player_index(
            (unsigned short)(signed char)player[0x1d], player_handle);
        }
        client = (void *)player_handle;
        update_client_add_player((int)client);
        if (global_network_game_server_get() != NULL) {
          update_server_add_player((int)client);
        }
      }

      network_event(
        "added new player to the game (machine #%d / controller #%d)",
        (int)(signed char)player[0x1c], (int)(signed char)player[0x1d]);
    }
  }

  return added;
}

/* network_game_client_switch_to_postgame (0x125610)
 *
 * Asserts client is non-null, then switches the game engine to the postgame
 * state, sets the client state field (offset 0xca6) to 4 (postgame), and
 * logs "switching to postgame". */
void network_game_client_switch_to_postgame(void *client)
{
  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x48c, 1);
    system_exit(-1);
  }
  game_engine_switch_to_postgame();
  *(int16_t *)((char *)client + 0xca6) = 4;
  network_event("switching to postgame");
}

/* network_game_client_switch_to_pregame (0x125660)
 *
 * Asserts client is non-null, then — only when the client is not already in
 * state 2 (pregame, int16 at +0xca6) — resets the embedded game state at
 * +0x85c for the next round, pings the connection at +0x82c to keep it alive,
 * clears the pregame bookkeeping fields, marks the state as 2, logs, resets
 * the pregame UI, and pings the connection a second time. The stores at
 * +0xc98/+0xc9c/+0xcad/+0xcac all come from the zeroed EBX in the original
 * (XOR EBX,EBX at 0x125668); +0xc90 is the immediate 1. Always returns 1,
 * including on the already-in-pregame path (MOV AL,1 after the join).
 */
char network_game_client_switch_to_pregame(void *client)
{
  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x499, true);
    system_exit(-1);
  }
  if (*(int16_t *)((char *)client + 0xca6) != 2) {
    network_game_reset_for_next_round((char *)client + 0x85c, true);
    network_connection_keep_alive(*(int *)((char *)client + 0x82c));
    *(int *)((char *)client + 0xc98) = 0;
    *(int *)((char *)client + 0xc90) = 1;
    *(int *)((char *)client + 0xc9c) = 0;
    *((char *)client + 0xcad) = 0;
    *(int16_t *)((char *)client + 0xca6) = 2;
    *((char *)client + 0xcac) = 0;
    network_event("switching to pregame");
    network_game_reset_to_pregame_ui();
    network_connection_keep_alive(*(int *)((char *)client + 0x82c));
  }
  return 1;
}

/* 0x125710 — Asserts client is non-null and returns the connection handle
 * (int) stored at offset 0x82c in the client structure. The returned handle
 * is used by the caller (network_game_client_end_frame) as the first argument
 * to network_game_client_write (which forwards it to network_connection_write
 * to send a network message). */
int network_game_client_get_connection(void *client)
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
void network_game_client_get_remote_server_address(void *server, void *out)
{
  assert_halt(server);
  network_connection_get_address(*(int *)((char *)server + 0x82c), out, 0);
}

/* network_game_client_get_game (0x1257a0)
 *
 * Asserts client is non-null and returns client + 0x85c.
 */
void *network_game_client_get_game(void *client)
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
bool network_game_client_server_has_started_game(void *server)
{
  assert_halt(server);
  return *(int *)((char *)server + 0xc98) != 0;
}

/* 0x125820 — Asserts client is non-null and returns the uint32_t field at
 * offset 0xc98 (the raw value that network_game_client_server_has_started_game
 * tests for non-zero). */
uint32_t network_game_client_get_next_update_number(void *server)
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

/* network_game_client_add_player (0x1258a0)
 *
 * Validates client/player_index (display_assert + system_exit(-1) on
 * failure, matching network_game_client_request_remove_player's assert
 * shape). Fetches the local player's profile via
 * player_ui_get_active_player_profile((int16_t)player_index, profile) —
 * the callee's own stack frame proves the profile buffer is 0x30 bytes
 * (SUB ESP,0x70 splits 0x30 profile + 0x20 staging buf + 0x20 record).
 *
 * Builds a 0x20-byte player-identifier record: ustrncpy(record,
 * (wchar_t*)profile, 0xb) copies profile's leading wide name (record+0x00,
 * 11 wchar_t = 0x16 bytes); record+0x16 = 0; record+0x18 = profile+0x18
 * (word, unproven meaning); record+0x1a = 0xffff; record+0x1c =
 * client's first byte; record+0x1d = (unsigned char)player_index;
 * record+0x1e/+0x1f = 0xff/0xff.
 *
 * network_event's controller-index arg is MOVSX from record+0x1d
 * (signed-char widen), matching request_remove_player's own log call.
 *
 * switch(*(int16_t*)(client+0xca6)): case 0/1 log+return false; case 2
 * (pregame, type 0xd) / case 3 (ingame, type 0x1a) each csmemcpy the
 * record into a fresh staging buffer, create_network_game_message, and on
 * NULL packet log-and-return **true** — unlike
 * network_game_client_request_remove_player, which returns false on an
 * encode failure, this function's `ok`-preset BL is never cleared on that
 * path (disasm-confirmed at 0x125983 JZ 0x1259c2 / 0x1259c2 falls straight
 * to `MOV AL,BL` with BL untouched since the 0x1258ad `MOV BL,1` prologue
 * store). On a successful encode, network_connection_write's own bool
 * result becomes the return value. case 4 (postgame) only logs and
 * returns false — no message is sent, unlike remove_player's case 4.
 * default (state > 4, reached via the bounds check before the jump table,
 * not a table entry) logs and returns true.
 */
bool network_game_client_add_player(void *client, uint16_t player_index)
{
  char *c;
  bool result;
  unsigned char profile[0x30];
  unsigned char buf[0x20];
  unsigned char record[0x20];
  unsigned short *packet;

  result = true;
  c = (char *)client;

  if (client == NULL || (int16_t)player_index < 0 ||
      (int16_t)player_index >= 4) {
    display_assert("client && (local_player_index>=0) && "
                   "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x530, true);
    system_exit(-1);
  }

  player_ui_get_active_player_profile((int16_t)player_index, profile);

  record[0x1c] = *(unsigned char *)c;
  record[0x1d] = (unsigned char)player_index;
  ustrncpy((wchar_t *)record, (wchar_t *)profile, 0xb);
  *(uint16_t *)(record + 0x16) = 0;
  *(uint16_t *)(record + 0x18) = *(uint16_t *)(profile + 0x18);
  *(uint16_t *)(record + 0x1a) = 0xffff;
  record[0x1e] = 0xff;
  record[0x1f] = 0xff;

  network_event("requesting a player addition (controller index #%d)",
                   (signed char)record[0x1d]);

  switch (*(uint16_t *)(c + 0xca6)) {
  case 0:
  case 1:
    network_event(
      "can't add players to a game until after a game is joined");
    return false;
  case 2:
    csmemcpy(buf, record, 0x20);
    packet = (unsigned short *)create_network_game_message(0xd, buf, 0x20);
    if (packet != NULL) {
      result = network_connection_write(*(void **)(c + 0x82c), packet,
                                        (unsigned short)(*packet >> 4), 0, true);
      if (!result) {
        network_event("network_game_client_write() failed while sending a "
                         "message_client_add_player_request_pregame message");
      }
    } else {
      network_event(
        "failed to create a message_client_add_player_request_pregame message");
    }
    return result;
  case 3:
    csmemcpy(buf, record, 0x20);
    packet = (unsigned short *)create_network_game_message(0x1a, buf, 0x20);
    if (packet != NULL) {
      result = network_connection_write(*(void **)(c + 0x82c), packet,
                                        (unsigned short)(*packet >> 4), 0, true);
      if (!result) {
        network_event("network_game_client_write() failed while sending a "
                         "message_client_add_player_request_ingame message");
      }
    } else {
      network_event(
        "failed to create a message_client_add_player_request_ingame message");
    }
    return result;
  case 4:
    network_event("client tried to add a new player in post-game");
    return false;
  default:
    network_event("client is in an unknown state");
    break;
  }

  return result;
}

/* network_game_client_update_local_player_data (0x125a90)
 *
 * Name comes from the failure log string emitted by this function itself.
 * Asserts "client && player" (line 0x587), then that the player's
 * machine_index (signed byte at player+0x1c, MOVSX at 0x125ac9) equals the
 * client's machine_index (unsigned word at client+0, MOVZX at 0x125acd)
 * (line 0x588), then network_player_is_valid(player) (line 0x589).
 *
 * Copies the 0x20-byte player-settings block out of the player record
 * (csmemcpy at 0x125b28 into the EBP-0x20 frame slot) and normalizes the
 * byte at +0x1e: 0xff becomes 0 (CMP AL,0xff / MOV byte ptr [EBP-0x2],0x0 at
 * 0x125b33-0x125b37). Encodes it as a message_client_player_settings_request
 * (type 0x10) and writes it to the connection handle at client+0x82c; the
 * size argument is the encoded header word >> 4 (MOV CX,[EAX]; SHR CX,4 at
 * 0x125b57).
 *
 * Return is a bool in AL: 1 only on a successful write (MOV AL,1 at
 * 0x125b73); both the encode-failed path (JZ 0x125b87) and the write-failed
 * path fall through to MOV AL,BL with BL zeroed at entry (XOR BL,BL at
 * 0x125a9b). */
bool network_game_client_update_local_player_data(void *client, void *player)
{
  char player_settings[32];
  unsigned short *encoded;

  if (client == NULL || player == NULL) {
    display_assert("client && player",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x587, true);
    system_exit(-1);
  }
  if (*(signed char *)((char *)player + 0x1c) != *(unsigned short *)client) {
    display_assert("player->machine_index==client->machine_index",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x588, true);
    system_exit(-1);
  }
  if (!network_player_is_valid(player)) {
    display_assert("network_player_is_valid(player)",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x589, true);
    system_exit(-1);
  }
  csmemcpy(player_settings, player, 0x20);
  if (player_settings[0x1e] == -1) {
    player_settings[0x1e] = 0;
  }
  encoded =
    (unsigned short *)create_network_game_message(0x10, player_settings, 0x20);
  if (encoded != NULL) {
    if (network_connection_write(*(void **)((char *)client + 0x82c), encoded,
                                 (unsigned short)(*encoded >> 4), 0, true)) {
      return 1;
    }
    network_event("network_game_client_update_local_player_data() failed "
                     "while sending a message_client_player_settings_request "
                     "message");
  }
  return 0;
}

/* 0x125b90 — network_game_client_request_start_time_change
 *
 * Name comes from the failure log string emitted by this function itself.
 * Asserts client non-null (line 0x5a5) and 0 <= request_type < 4 (line 0x5a6,
 * "NUMBER_OF_GAME_START_REQUESTS"). Only sends while the client state word at
 * +0xca6 equals 2 (pregame); otherwise it just logs. Encodes a 2-byte
 * message_client_game_start_request (type 0x11) holding request_type and
 * writes it to the connection handle at +0x82c. The size argument is the
 * encoded header word >> 4 (MOV CX,[EAX]; SHR CX,4 at 0x125c15). Always
 * returns 1 — every reachable exit is MOV AL,1 (0x125c3d / 0x125c50), and
 * only AL is set, so the return is a bool.
 *
 * The original builds the 2-byte message in the dead incoming parameter slot
 * (MOV word ptr [EBP+0xe],DI) rather than allocating a frame; that is MSVC
 * stack packing of a local, not a parameter.
 */
bool network_game_client_request_start_time_change(void *client,
                                                   short request_type)
{
  short message;
  unsigned short *encoded;

  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x5a5, true);
    system_exit(-1);
  }
  if (request_type < 0 || request_type >= 4) {
    display_assert(
      "(request_type>=0) && (request_type<NUMBER_OF_GAME_START_REQUESTS)",
      "c:\\halo\\SOURCE\\networking\\network_client_manager.c", 0x5a6, true);
    system_exit(-1);
  }
  if (*(int16_t *)((char *)client + 0xca6) == 2) {
    message = request_type;
    encoded = (unsigned short *)create_network_game_message(0x11, &message, 2);
    if (encoded != NULL) {
      if (!network_connection_write(*(void **)((char *)client + 0x82c), encoded,
                                    (unsigned short)(*encoded >> 4), 0, true)) {
        network_event("network_game_client_request_start_time_change() "
                         "failed to send a message_client_game_start_request "
                         "message");
      }
    }
  } else {
    network_event("failed to send a message_client_game_start_request "
                     "because we are not in the pregame state");
  }
  return 1;
}

/* 0x125c60 — network_game_client_countdown_timer_update
 *
 * Two-argument setter for the 16-bit field at client+0xca4 — the same field
 * the accessor network_game_client_get_seconds_to_game_start (0x124d00) reads. Asserts client non-null at line
 * 0x5c3 with reason string "client", then stores the caller's second (16-bit)
 * stack argument.
 *
 * Evidence (0x125c60-0x125ca6): MOV ESI,[EBP+8]; TEST ESI,ESI; JNZ 0x125c99
 * over PUSH 0x1 / PUSH 0x5c3 / PUSH 0x291774 / PUSH 0x2917a8 / CALL
 * display_assert (0x8d9f0); PUSH -0x1 / CALL system_exit (0x8e2f0). The store
 * is `MOV CX,word ptr [EBP+0xc]` followed by `MOV word ptr [ESI+0xca4],CX` —
 * exactly 16 bits wide, and the signedness is NOT established here (int16_t
 * chosen to match network_game_client_get_seconds_to_game_start on the same field). The epilogue is POP ESI /
 * POP EBP / plain RET with no stack adjust, so the convention is cdecl and
 * the caller pops. kb.json previously declared this `void(void)`, which
 * contradicts the two stack slots the code reads; the decl is corrected here.
 *
 * The meaning of the field at +0xca4 remains unproven — the "countdown_timer"
 * in the symbol name is not established by anything this function does, so
 * the value parameter keeps a mechanical name.
 */
void network_game_client_countdown_timer_update(void *client, short value)
{
  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x5c3, client);

  *(int16_t *)((char *)client + 0xca4) = value;
}

/* 0x125cb0 — network_game_client_advertised_game_is_valid
 *
 * Predicate over a single advertised-game record: the record is valid only if
 * the byte flag at +0xe1 is set AND the record was refreshed within the last
 * 6000 ms.
 *
 * Evidence (0x125cb0-0x125cde): MOV ESI,[EBP+8] (one cdecl stack argument;
 * the epilogue is POP ESI / POP EBX / POP EBP / plain RET, so the caller
 * pops). MOV AL,[ESI+0xe1]; TEST AL,AL; MOV BL,0x1; JZ 0x125cd3 — the
 * zero-flag case falls into the XOR AL,AL return. Otherwise CALL 0x0008e370
 * (system_milliseconds); SUB EAX,[ESI+0x2c]; CMP EAX,0x1770; JLE 0x125cd9,
 * which is MOV AL,BL — i.e. true. JLE (not JBE) makes the elapsed comparison
 * SIGNED, so the difference is taken as int, not unsigned. The result is
 * returned in AL, so the return type is bool; kb.json previously declared
 * this void(void), contradicting both the stack slot read and the AL result,
 * and the decl is corrected here.
 *
 * The MOV BL,0x1 / MOV AL,BL pair (EBX is saved solely for it) is the shape
 * of a bool local initialised to true and cleared on the failure path, which
 * is why this is not written as two early returns.
 *
 * The identity of the pointee is NOT proven here. The offsets it touches
 * (+0x2c, +0xe1) are far below the client-globals offsets the rest of this
 * TU uses (+0x82c, +0xca0, +0xca4), so the argument is named after the entity
 * in the symbol name rather than assumed to be the client structure. The
 * meaning of +0xe1 beyond "non-zero means in use" and of +0x2c beyond "a
 * millisecond timestamp compared against system_milliseconds()" is unproven.
 */
bool network_game_client_advertised_game_is_valid(void *advertised_game)
{
  bool valid;

  valid = true;
  if (*(unsigned char *)((char *)advertised_game + 0xe1) == 0 ||
      (int)system_milliseconds() - *(int *)((char *)advertised_game + 0x2c) >
        6000) {
    valid = false;
  }
  return valid;
}

/* add_advertised_game (0x125ce0)
 *
 * message_packet is an @<edi> register argument (see
 * network_game_client_new_advertised_game's evidence comment above — this
 * was the source of a runtime crash, see
 * project_system_link_search_crash_125ce0 memory: caller passed it correctly,
 * but kb.json's decl once lacked
 * @<edi> and clang left EDI unset). advertised_games is a genuine cdecl
 * stack argument: a 9-entry array of the 0xe4-byte record
 * network_game_client_advertised_game_is_valid (0x125cb0) reads (+0xe1 =
 * occupied, +0x2c = last-refresh timestamp).
 *
 * local_open computes whether the advertised game still has room: message+
 * 0x102 bit 1 set AND message+0xfa (player count) < 4.
 *
 * Pass 1: any entry whose +0xe1 byte is 0 (unoccupied) or whose +0x2c
 * timestamp is >6000ms stale gets zeroed (matches
 * network_game_client_advertised_game_is_valid's own validity window).
 *
 * Pass 2: transport_nonce_is_equal(entry+0x24, message+8) against every
 * entry — the +0x24 field is this record's own 8-byte nonce, filled from
 * message+8 the same way in pass 4 below, so this identifies "we already
 * have a record for this exact game" and reuses that slot directly.
 *
 * Pass 3 (no match in pass 2): the first entry with +0xe1 == 0 (unoccupied,
 * including ones pass 1 just cleared) is claimed.
 *
 * Pass 4 (all 9 occupied, no match): only runs if local_open is true —
 * closed/full advertisements do not evict anything. Scans for the first
 * occupied entry whose +0xe0 (open) byte is 0 (a closed game) and reclaims
 * it. display_assert("current->valid", ...) fires if an unoccupied (+0xe1
 * == 0) entry turns up here — pass 3 already established all 9 are
 * occupied, so finding one otherwise is a broken invariant, not a normal
 * path.
 *
 * If neither pass 3 nor pass 4 claims a slot (closed game with all 9 slots
 * full and occupied, or an open game with all 9 full of other open games),
 * logs "not fatal, but we have to many active network games cannot add
 * more to the list" and returns — AL is 0 on this path (disassembly), but
 * kb.json declares this void(void) message_packet/advertised_games, and
 * the sole caller (network_game_client_new_advertised_game) discards the
 * result, so the void return is kept as declared.
 *
 * Fill-in (claimed slot at `entry`): sets +0xe1 = 1 (occupied), copies
 * message+0x18..0x28 to entry+8..0x18 (4 dwords), message+0x10..0x18 to
 * entry+0..8 (2 dwords), message+0x28..0x34 to entry+0x18..0x24 (3 dwords),
 * message+8 (8-byte nonce) to entry+0x24, stamps entry+0x2c with
 * system_milliseconds(), copies message+0x38 (platform) to entry+0xde,
 * copies message+0x3a (a wide machine-name string, or the "<no name>"
 * literal at 0x292468 if empty) into entry+0x30 via ustrncpy(...,15),
 * zeros entry+0x4e/+0x4f, copies message+0xf8/+0xfa/+0xfc/+0xfe/+0x100
 * (5 words) to entry+0xd4/+0xd6/+0xd8/+0xda/+0xdc, stores local_open to
 * entry+0xe0, message+0x102 bit 2 to entry+0xe2, and
 * (entry+0xd4==3 && message+0x102 bit 3) to entry+0xe3. Logs "there is %s
 * %s net game with %d players and %d machines" (open/closed, platform
 * name, entry+0xd8, entry+0xd6) and returns. */
void add_advertised_game(void *message_packet, void *advertised_games)
{
  char *m;
  char *g;
  char *entry;
  char *p;
  bool local_open;
  int i;
  const char *platform_str;
  const char *open_str;

  local_open =
    (*(unsigned char *)((char *)message_packet + 0x102) & 2) != 0 &&
    *(int16_t *)((char *)message_packet + 0xfa) < 4;

  m = (char *)message_packet;
  g = (char *)advertised_games;

  entry = g;
  for (i = 0; i < 9; i++) {
    if (*(unsigned char *)(entry + 0xe1) == 0 ||
        (int)system_milliseconds() - *(int *)(entry + 0x2c) > 6000) {
      csmemset(entry, 0, 0xe4);
    }
    entry += 0xe4;
  }

  entry = NULL;
  p = g;
  for (i = 0; i < 9; i++) {
    if (transport_nonce_is_equal(p + 0x24, m + 8)) {
      entry = p;
      break;
    }
    p += 0xe4;
  }
  if (entry != NULL) {
    goto fill_in;
  }

  for (p = g, i = 0; i < 9; i++) {
    if (*(unsigned char *)(p + 0xe1) == 0) {
      entry = p;
      goto fill_in;
    }
    p += 0xe4;
  }

  if (local_open) {
    p = g + 0xe0;
    for (i = 0; i < 9; i++) {
      if (p[1] == 0) {
        display_assert("current->valid",
                       "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                       0x61f, true);
        system_exit(-1);
      }
      if (*p == 0) {
        entry = p - 0xe0;
        csmemset(entry, 0, 0xe4);
        goto fill_in;
      }
      p += 0xe4;
    }
  }

  error(2,
        "not fatal, but we have to many active network games cannot add more "
        "to the list");
  return;

fill_in:
  *(unsigned char *)(entry + 0xe1) = 1;
  *(unsigned int *)(entry + 8) = *(unsigned int *)(m + 0x18);
  *(unsigned int *)(entry + 0xc) = *(unsigned int *)(m + 0x1c);
  *(unsigned int *)(entry + 0x10) = *(unsigned int *)(m + 0x20);
  *(unsigned int *)(entry + 0x14) = *(unsigned int *)(m + 0x24);
  *(unsigned int *)entry = *(unsigned int *)(m + 0x10);
  *(unsigned int *)(entry + 4) = *(unsigned int *)(m + 0x14);
  *(unsigned int *)(entry + 0x18) = *(unsigned int *)(m + 0x28);
  *(unsigned int *)(entry + 0x1c) = *(unsigned int *)(m + 0x2c);
  *(unsigned int *)(entry + 0x20) = *(unsigned int *)(m + 0x30);
  csmemcpy(entry + 0x24, m + 8, 8);
  *(int *)(entry + 0x2c) = system_milliseconds();
  *(uint16_t *)(entry + 0xde) = *(uint16_t *)(m + 0x38);
  if (*(uint16_t *)(m + 0x3a) == 0) {
    ustrncpy((wchar_t *)(entry + 0x30), (wchar_t *)0x292468, 0xf);
  } else {
    ustrncpy((wchar_t *)(entry + 0x30), (wchar_t *)(m + 0x3a), 0xf);
  }
  *(uint16_t *)(entry + 0x4e) = 0;
  *(uint16_t *)(entry + 0xd4) = *(uint16_t *)(m + 0xf8);
  csmemcpy(entry + 0x50, m + 0x74, 0x84);
  *(uint16_t *)(entry + 0xd6) = *(uint16_t *)(m + 0xfa);
  *(uint16_t *)(entry + 0xd8) = *(uint16_t *)(m + 0xfc);
  *(uint16_t *)(entry + 0xda) = *(uint16_t *)(m + 0xfe);
  *(uint16_t *)(entry + 0xdc) = *(uint16_t *)(m + 0x100);
  *(unsigned char *)(entry + 0xe0) = (unsigned char)local_open;
  *(unsigned char *)(entry + 0xe2) = (*(unsigned char *)(m + 0x102) >> 2) & 1;
  *(unsigned char *)(entry + 0xe3) =
    (*(int16_t *)(entry + 0xd4) == 3 &&
     (*(unsigned char *)(m + 0x102) & 8) != 0) ?
      1 :
      0;

  if (*(uint16_t *)(entry + 0xde) == 0) {
    platform_str = "XBox";
  } else if (*(uint16_t *)(entry + 0xde) == 1) {
    platform_str = "PC";
  } else {
    platform_str = "<unknown platform>";
  }
  open_str = local_open ? "an open" : "a closed";
  network_event("there is %s %s net game with %d players and %d machines",
                   open_str, platform_str, *(uint16_t *)(entry + 0xd8),
                   *(uint16_t *)(entry + 0xd6));
}

/* network_game_client_set_error (0x125fb0)
 *
 * Records a client leave/shutdown reason in the 16-bit field at client+0xca8 —
 * the same field network_game_client_game_shutdown (0x126750) sets to 8 — but
 * only if it is still 0, so the FIRST reason recorded wins. Reason codes at or
 * above 9 are folded to 1 before the store.
 *
 * ABI: both arguments arrive in registers; there is no frame and no stack slot
 * is ever read. `TEST ESI,ESI` at 0x125fb0 uses ESI before any definition, and
 * `MOV EDI,EAX` at 0x125fb3 uses EAX before any definition — so ESI is the
 * client pointer (the assert reason string is "client") and EAX carries the
 * reason. The only xref is CALL 0x125fb0 from 0x126852 inside
 * network_game_client_reset (0x1267c0). The epilogue is POP EDI / plain RET.
 *
 * Evidence (0x125fb0-0x125ff4): TEST ESI,ESI; JNZ 0x125fd7 over PUSH 0x1 /
 * PUSH 0x662 / PUSH 0x291774 / PUSH 0x2917a8 / CALL display_assert (0x8d9f0);
 * PUSH -0x1 / CALL system_exit (0x8e2f0) — assert line 0x662, reason "client",
 * the TU's usual __FILE__ pointer. Then CMP DI,0x9; JC 0x125fe2 over MOV
 * EDI,0x1: JC (not JL) makes the clamp comparison UNSIGNED and 16 bits wide,
 * hence the unsigned short parameter. The clamp is spelled `>= 9` rather than
 * the equivalent `> 8` so the compare immediate reproduces the reference's
 * CMP DI,0x9 / JC pair instead of CMP AX,0x8 / JBE. Finally CMP word ptr
 * [ESI+0xca8],0x0; JNZ 0x125ff3; MOV word ptr [ESI+0xca8],DI — a 16-bit test
 * and a 16-bit store. Signedness of the field itself is not established here;
 * int16_t is chosen to match the sibling writer at 0x126750 on the same offset.
 *
 * The meaning of the individual reason codes is NOT proven — only that 8 is
 * what the host-shutdown path writes and that >= 9 collapses to 1.
 */
void network_game_client_set_error(unsigned short reason, void *client)
{
  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x662, client);

  if (reason >= 9)
    reason = 1;
  if (*(int16_t *)((char *)client + 0xca8) == 0)
    *(int16_t *)((char *)client + 0xca8) = (int16_t)reason;
}

#if defined(_MSC_VER) && !defined(__clang__)
extern void *__cdecl memset(void *, int, unsigned int);
#pragma intrinsic(memset)
#define client_zero_bytes(p, n) memset((p), 0, (n))
#else
#define client_zero_bytes(p, n) csmemset((p), 0, (n))
#endif

/* network_game_client_update_precache_status (0x126000) — network_game_client_send_graceful_exit_pregame
 *
 * Periodically (every 1000ms) encodes and sends a
 * message_client_graceful_game_exit_pregame (type 0x13) containing the
 * multiplayer map name to the server connection. */
void network_game_client_update_precache_status(void *server)
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
      client_zero_bytes(buf, sizeof(buf));
      csstrncpy(buf, map_name, 0x100);
      encoded = (unsigned short *)create_network_game_message(0x13, buf, 0x100);
      if (encoded != NULL) {
        size = *encoded >> 4;
        if (!network_connection_write((void *)*(int *)((char *)server + 0x82c),
                                      encoded, size, 0, 1)) {
          network_event("network_game_client_write() failed while sending a "
                           "message_client_graceful_game_exit_pregame message");
        }
      }
    }
  }
}

/* network_game_client_process_incoming_messages (0x1260c0) — network_game_client_process_incoming_messages
 *
 * Drains all pending messages from the server connection. Loops calling
 * network_connection_read to receive each message, then network_game_client_handle_message to handle it.
 * Returns true if all messages were processed successfully, false if any
 * handler fails. */
bool network_game_client_process_incoming_messages(void *server)
{
  bool result;
  char local_820[2048];
  char local_20[24];
  int local_8;

  result = true;
  do {
    local_8 = 0x800;
    if (!network_connection_read(*(int *)((char *)server + 0x82c), local_820, &local_8,
                      local_20))
      return result;
    result = network_game_client_handle_message(server, local_820, local_8, local_20);
    if (!result)
      network_event("network_game_client_handle_message() failed in "
                       "network_game_client_process_incoming_messages()");
  } while (result);
  return result;
}

/* network_game_client_leave_game (0x126140)
 *
 * Asserts client && client->connection (line 0x179), logs "leaving network
 * game", then dispatches on the client's state (int16 at +0xca6, 0..4) to
 * gracefully tear down the connection:
 *  0: asserts the connection is NOT connected (line 0x180) — nothing else to
 *     do, falls straight through.
 *  1 (joining): frees/clears the transport handle at +0x830 if set, then
 *     disconnects if still connected.
 *  2 (pregame): if connected, sends a
 *     message_client_graceful_game_exit_pregame (type 0x12, empty 4-byte
 *     body), logging on either encode or write failure, then disconnects.
 *  3 (ingame): disconnects if connected.
 *  4 (postgame): if connected, sends a
 *     message_client_graceful_game_exit_postgame (type 0x22, empty 4-byte
 *     body), logging only on write failure (unlike case 2, an encode failure
 *     here is silent), then disconnects.
 *  default: logs "client is in an unknown state".
 * Any non-goto case that falls out of the switch (disconnect not attempted,
 * or disconnect failed) logs an error string first. All paths converge to
 * invalidate the embedded game state (network_game_invalidate on client+0x85c)
 * and reset the state to 0.
 *
 * Return value: the result of the state-1/2/3/4 disconnect check (bool in
 * BL), matching disassembly (0x126140-0x126386) exactly, including that BL is
 * never initialized before the case-0 "already disconnected" fast path or the
 * case-1..4 "not connected" fast paths — those return whatever value the
 * caller's EBX happened to hold, an uninitialized-read quirk in the original
 * binary, not introduced here.
 *
 * kb.json previously recorded this as `void(void)` (an unanalyzed/unreferenced
 * stub guess — Ghidra finds no callers or xrefs to 0x126140, so the ABI is
 * confirmed only from the function's own prologue/epilogue, not caller
 * evidence): the real signature takes one stack param (client) and returns
 * bool. Corrected below. */
bool network_game_client_leave_game(void *client)
{
  /* Disasm shows BL (this function's return) genuinely unset on the "nothing
   * to disconnect" fast paths (case 0, and each case's own not-connected
   * check) — it returns whatever the caller's EBX held. No caller/xref was
   * found to confirm the value is ever read; -Werror forces a deterministic
   * initializer here, so `true` (already in the desired disconnected state)
   * is used as the least-wrong default rather than reproducing the garbage
   * read. */
  bool result = true;
  int16_t state;
  const char *msg;
  unsigned short *packet;
  int scratch;

  if (client == NULL || *(int *)((char *)client + 0x82c) == 0) {
    display_assert("client && client->connection",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x179, 1);
    system_exit(-1);
  }

  network_event("leaving network game");

  state = *(int16_t *)((char *)client + 0xca6);
  switch (state) {
  case 0:
    if (network_connection_connected(*(int *)((char *)client + 0x82c))) {
      display_assert("!network_connection_connected(client->connection)",
                     "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                     0x180, 1);
      system_exit(-1);
    }
    goto done;

  case 1:
    if (*(int *)((char *)client + 0x830) != 0) {
      transport_server_terminate((int *)*(int *)((char *)client + 0x830));
      *(int *)((char *)client + 0x830) = 0;
    }
    if (!network_connection_connected(*(int *)((char *)client + 0x82c)))
      goto done;
    result = network_connection_disconnect(*(int *)((char *)client + 0x82c));
    if (result)
      goto done;
    msg = "network_connection_disconnect() failed "
          "_network_game_client_state_joining";
    break;

  case 2:
    if (!network_connection_connected(*(int *)((char *)client + 0x82c)))
      goto done;
    scratch = 0;
    packet = (unsigned short *)create_network_game_message(0x12, &scratch, 4);
    if (packet == NULL) {
      msg = "failed to create a message_client_graceful_game_exit_pregame "
            "message";
      network_event(msg);
    } else if (!network_connection_write(*(void **)((char *)client + 0x82c),
                                         packet, *packet >> 4, 0, true)) {
      msg = "network_game_client_write() failed while sending a "
            "message_client_graceful_game_exit_pregame message";
      network_event(msg);
    }
    result = network_connection_disconnect(*(int *)((char *)client + 0x82c));
    if (result)
      goto done;
    msg = "network_connection_disconnect() failed "
          "_network_game_client_state_pregame";
    break;

  case 3:
    if (!network_connection_connected(*(int *)((char *)client + 0x82c)))
      goto done;
    result = network_connection_disconnect(*(int *)((char *)client + 0x82c));
    if (result)
      goto done;
    msg = "network_connection_disconnect() failed "
          "_network_game_client_state_ingame";
    break;

  case 4:
    if (!network_connection_connected(*(int *)((char *)client + 0x82c)))
      goto done;
    scratch = 0;
    packet = (unsigned short *)create_network_game_message(0x22, &scratch, 4);
    if (packet != NULL &&
        !network_connection_write(*(void **)((char *)client + 0x82c), packet,
                                  *packet >> 4, 0, true)) {
      network_event("network_game_client_write() failed while sending a "
                       "message_client_graceful_game_exit_postgame message");
    }
    result = network_connection_disconnect(*(int *)((char *)client + 0x82c));
    if (result)
      goto done;
    msg = "network_connection_disconnect() failed "
          "_network_game_client_state_postgame";
    break;

  default:
    msg = "client is in an unknown state";
    break;
  }

  network_event(msg);

done:
  network_game_invalidate((char *)client + 0x85c);
  *(int16_t *)((char *)client + 0xca6) = 0;
  return result;
}

/* network_game_client_request_remove_player (0x1263a0)
 *
 * Asserts client non-null and network_player_is_valid(record) (line 0x208),
 * then asserts the requesting machine owns the player: client's own machine
 * record (base +0x9b0, stride 0x44, indexed by the client's own uint16 at
 * +0) byte 0 must equal record+0x1c (line 0x209). Logs the request
 * ("requesting a player removal (controller index #%d)", record+0x1d, a
 * signed controller index).
 *
 * Dispatches on client state (+0xca6) via a 5-entry jump table (cases 0-4,
 * default for state > 4):
 *  - 0, 1 (pregame states before a game is joined): logs and returns false.
 *  - 2, 3, 4: copies the 0x20-byte record into a local, encodes it as
 *    message_client_remove_player_request_{pregame,ingame,postgame}
 *    (types 0xe, 0x1b, 0x20) — encode failure logs and returns false.
 *  - default: logs "client is in an unknown state" and returns true.
 *
 * Every non-early-return case (2, 3, 4, and the default's *encoded* message)
 * falls through to one shared network_connection_write(connection, message,
 * size, 0, true) at the tail, EXCEPT case 4, which the reference inlines its
 * own copy of that same write (identical operands) rather than falling
 * through — a MSVC jump-table layout artifact, not a behavioral difference. */
char network_game_client_request_remove_player(void *client, void *record)
{
  uint16_t state;
  char result;
  unsigned char buf[0x20];
  unsigned short *packet;

  result = 1;
  if (client == NULL || !network_player_is_valid(record)) {
    display_assert("client && network_player_is_valid(player)",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x208, true);
    system_exit(-1);
  }
  if (*(char *)((char *)client + 0x9b0 + (*(uint16_t *)client) * 0x44) != *(char *)((char *)record + 0x1c)) {
    display_assert("client's can only remove players from their own machines",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x209, true);
    system_exit(-1);
  }

  network_event("requesting a player removal (controller index #%d)",
                   *(signed char *)((char *)record + 0x1d));

  state = *(uint16_t *)((char *)client + 0xca6);
  switch (state) {
  case 0:
  case 1:
    network_event(
      "can't remove players from a game until after a game is joined");
    return 0;
  case 2:
    csmemcpy(buf, record, 0x20);
    packet = (unsigned short *)create_network_game_message(0xe, buf, 0x20);
    if (packet != NULL) {
      goto send_packet;
    }
    network_event(
      "failed to create a message_client_remove_player_request_pregame "
      "mesage");
    return 0;
  case 3:
    csmemcpy(buf, record, 0x20);
    packet = (unsigned short *)create_network_game_message(0x1b, buf, 0x20);
    if (packet == NULL) {
      network_event(
        "failed to create a message_client_remove_player_request_ingame "
        "message");
      return 0;
    }
send_packet:
    return network_connection_write(*(void **)((char *)client + 0x82c), packet,
                                    (unsigned short)(*packet >> 4), 0, true);
  case 4:
    csmemcpy(buf, record, 0x20);
    packet = (unsigned short *)create_network_game_message(0x20, buf, 0x20);
    if (packet == NULL) {
      network_event(
        "failed to create a message_client_remove_player_request_postgame "
        "message");
      return 0;
    }
    result = network_connection_write(*(void **)((char *)client + 0x82c), packet,
                                      (unsigned short)(*packet >> 4), 0, true);
    if (!result) {
      network_event("network_game_client_write() failed while sending a "
                       "message_client_remove_player_request_postgame message");
    }
    break;
  default:
    network_event("client is in an unknown state");
    break;
  }

  return result;
}

/* network_game_client_remove_player (0x126590)
 *
 * Removes the player identified by a remove-player message from the client's
 * network game. Asserts both pointers (line 0x273, reason "client && player"),
 * then scans the 16-entry 0x20-byte client player table based at client+0xa82
 * for a live entry whose two bytes at +0x1c/+0x1d match player+0x1c/+0x1d.
 * No match -> return 0. On a match, the entry byte at +0x1f (client + i*0x20 +
 * 0xaa1) is sign-extended and run through unstrip_player_index to get the real
 * player index, then network_game_remove_player(client+0x85c, player) does the
 * removal. Only when that succeeded AND the flag at client+0xc8c is set does
 * the rest run: a phony index (0 or -1) is reported and returns 0; otherwise
 * the player datum is fetched and, when tick != -1, the quit tick is logged and
 * stored at datum+0xcc. Finally the table is rescanned for any live entry whose
 * signed byte at +0x1c equals the unsigned int16 at client+0; if none remains,
 * network_game_client_all_local_players_have_quit() runs and the exit message
 * is logged. Returns the network_game_remove_player result.
 *
 * Evidence / decompiler corrections (0x126590-0x1266f4):
 *  - Ghidra renders 0x125180 as `FUN_00125180()` with an `extraout_EAX`. The
 *    disassembly has PUSH EAX (0x12660f) and MOV ESI,EAX (0x126620), so it is
 *    a real one-argument call returning the player index.
 *  - The two table scans compare DIFFERENTLY and are intentionally not
 *    symmetric. Scan 1 is an 8-bit CMP CL,[EAX+0x1c] / CMP DL,[EAX+0x1d]
 *    (char vs char). Scan 2 is MOVSX EDX,byte[EDI] against MOVZX ECX,
 *    word[EAX] with a 32-bit CMP: a signed byte widened against an UNSIGNED
 *    16-bit read of client+0.
 *  - Scan 2 re-reads [EBP+8] inside the loop (0x1266a5) because EBX was
 *    reused for the datum pointer at 0x126669, so `client` stays a parameter
 *    deref here rather than being hoisted into a local.
 *  - The "%x quit of of game at tick %d (now %d)" doubled "of" is verbatim
 *    from the original .rdata at 0x292aa8; it is a Bungie typo, not ours.
 *  - Call-site-audit ARG_COUNT flags are all accounted for and no callee decl
 *    is changed: the ADD ESP,0xc at 0x126627 folds unstrip_player_index's one
 *    push with network_game_remove_player's two (so remove_player really does
 *    take 2 args); error()'s cleanup=5 and cleanup=3 are its vararg slots; and
 *    network_event's cleanup=1 is a fmt-only call to a varargs decl.
 *  - No struct is recovered for the client or player records, so every access
 *    stays a raw offset cast. */
char network_game_client_remove_player(void *client, void *player, int tick)
{
  char *entry;
  void *player_datum;
  int index;
  int player_index;
  int now;
  bool result;

  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x273, client && player);

  index = 0;
  entry = (char *)client + 0xa9e;
  while (!network_player_is_valid(entry - 0x1c) ||
         entry[0] != *((char *)player + 0x1c) ||
         entry[1] != *((char *)player + 0x1d)) {
    index++;
    entry += 0x20;
    if (index >= 0x10)
      return 0;
  }

  player_index = unstrip_player_index(
    (int)*(signed char *)((char *)client + (index << 5) + 0xaa1));
  result = network_game_remove_player((char *)client + 0x85c, player);
  if (result && *((char *)client + 0xc8c) != 0) {
    if (player_index == 0 || player_index == -1) {
      error(2,
            "network game tried to delete a player with a phony player index "
            "(#0x%08lX)",
            player_index);
      return 0;
    }
    player_datum = datum_get(*(data_t **)0x5aa6d4, player_index);
    if (tick != -1) {
      now = game_time_get();
      error(2, "%x quit of of game at tick %d (now %d)", player_index, tick,
            now);
      *(int *)((char *)player_datum + 0xcc) = tick;
    }

    index = 0;
    entry = (char *)client + 0xa9e;
    do {
      if (network_player_is_valid(entry - 0x1c) &&
          (int)*(signed char *)entry == (int)*(unsigned short *)client)
        break;
      index++;
      entry += 0x20;
    } while (index < 0x10);
    if (index == 0x10) {
      network_game_client_all_local_players_have_quit();
      network_event("no local players remain in the game, exiting the game "
                       "now");
    }
  }
  return (char)result;
}

/* 0x126700 — network_game_client_new_advertised_game
 *
 * Two-argument cdecl handler. Asserts both arguments non-null at assert line
 * 0x2fc with reason string "client && message_packet", then tail-forwards
 * client+4 to add_advertised_game and returns nothing.
 *
 * Evidence (0x126700-0x126742): PUSH EBP / MOV EBP,ESP / PUSH ESI / MOV
 * ESI,[EBP+0x8] / TEST ESI,ESI / PUSH EDI / MOV EDI,[EBP+0xc] / JZ 0x126713 /
 * TEST EDI,EDI / JNZ 0x126733 — two cdecl stack slots, both tested, and the
 * epilogue is POP EDI / POP ESI / POP EBP / plain RET with no stack adjust,
 * so the caller pops. The failure path is PUSH 0x1 / PUSH 0x2fc / PUSH
 * 0x291774 / PUSH 0x291cd8 / CALL display_assert (0x8d9f0); PUSH -0x1 / CALL
 * system_exit (0x8e2f0). 0x291774 is this TU's usual __FILE__ pointer;
 * 0x291cd8 is the reason string "client && message_packet", which is why the
 * assert is written as one combined condition rather than two.
 *
 * The success path is ADD ESI,0x4 / PUSH ESI / CALL 0x125ce0 / ADD ESP,0x4 —
 * one cdecl stack argument, caller-cleanup. EDI (message_packet) is not
 * reloaded between 0x12670a and the CALL, so it is still live at the call
 * site: add_advertised_game reads it as a register argument (confirmed by its own
 * disassembly — the first real instruction after its prologue is
 * TEST byte ptr [EDI+0x102],0x2, and EDI is read at many further offsets
 * throughout the function body, never reloaded from the stack). kb.json
 * declares add_advertised_game with message_packet as an @<edi> register argument
 * alongside the one cdecl stack argument.
 *
 * The pointee at client+4 is NOT identified here — nothing in this function
 * dereferences it, so the callee argument keeps a mechanical form rather than
 * a named field. kb.json declared this function void(void), contradicting the
 * two stack slots it reads; the decl is corrected here. */
void network_game_client_new_advertised_game(void *client, void *message_packet)
{
  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x2fc, client && message_packet);

  add_advertised_game(message_packet, (char *)client + 4);
}

/* network_game_client_game_shutdown (0x126750)
 *
 * Asserts client is non-null, then — only when the int16 at +0xca8 is still 0 —
 * records shutdown reason 8 there (the field is left alone if a reason was
 * already set). Logs the host-shutdown message and tail-calls
 * network_game_client_all_local_players_have_quit (JMP at 0x1267ba).
 *
 * The cold path contains TWO assert blocks that share one test and one
 * combined `add esp,0x28` cleanup (0x12675b-0x00126747): line 0x3fc and then
 * line 0x662, both with reason "client" and the same __FILE__ pointer
 * (0x2917a8 / 0x291774). The second block is unreachable — system_exit()
 * never returns — so it is an artifact of a second same-condition assert
 * (source line 1634) whose redundant test MSVC folded into the first. It is
 * reproduced here because the reference bytes contain it; under clang the
 * __noreturn attribute deletes it again, which is behaviourally identical. */
void network_game_client_game_shutdown(void *client)
{
  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x3fc, true);
    system_exit(-1);
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x662, true);
    system_exit(-1);
  }
  if (*(int16_t *)((char *)client + 0xca8) == 0)
    *(int16_t *)((char *)client + 0xca8) = 8;
  network_event("the game host is shutting down");
  network_game_client_all_local_players_have_quit();
}

/* network_game_client_reset (0x1267c0)
 *
 * Asserts client is non-null (line 0x4ee), invalidates the client's embedded
 * network-game block (+0x85c) and resets the per-join bookkeeping fields.
 *
 * When `reinitialize` is set and the client still owns a connection handle
 * (+0x82c) that is currently connected, the join-in-progress flag at +0xc90 is
 * raised and network_connection_disconnect re-initializes the connection; success clears bit 0
 * of the flag byte at +0xcaa, failure reports reason 1 through network_game_client_set_error
 * (register ABI: reason@<eax>, client@<esi>; ESI already holds the client at
 * 0x12684d, so the call site is MOV EAX,1 / CALL 0x125fb0) and logs.
 *
 * Field widths are taken from the store instructions, not the decompiler:
 * MOV word at +0 / +0xca6 / +0xca8 / +0xca4, MOV dword at +0xc90 / +0xc94 /
 * +0xc98 / +0xc9c, MOV byte at +0xcad then +0xcac (that store order is the
 * reference order, 0x126884 before 0x12688a). The meanings of the individual
 * fields are unproven; only their widths and reset values are. */
__declspec(noinline) void network_game_client_reset(void *client,
                                                    bool reinitialize)
{
  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x4ee, true);
    system_exit(-1);
  }
  network_game_invalidate((char *)client + 0x85c);
  *(int16_t *)client = -1;
  *(int16_t *)((char *)client + 0xca6) = 0;
  if (reinitialize && *(int *)((char *)client + 0x82c) != 0 &&
      network_connection_connected(*(int *)((char *)client + 0x82c))) {
    *(int *)((char *)client + 0xc90) = 1;
    if (network_connection_disconnect(*(int *)((char *)client + 0x82c))) {
      *(unsigned char *)((char *)client + 0xcaa) =
        *(unsigned char *)((char *)client + 0xcaa) & 0xfe;
    } else {
      network_game_client_set_error(1, client);
      network_event("failed to reinitialize network game client");
    }
  }
  *(unsigned char *)((char *)client + 0xcaa) =
    *(unsigned char *)((char *)client + 0xcaa) & 0xfd;
  *(int16_t *)((char *)client + 0xca8) = 0;
  *(int *)((char *)client + 0xc94) = 0;
  *(int *)((char *)client + 0xc98) = 0;
  *(int *)((char *)client + 0xc9c) = 0;
  *(unsigned char *)((char *)client + 0xcad) = 0;
  *(unsigned char *)((char *)client + 0xcac) = 0;
  *(int16_t *)((char *)client + 0xca4) = -1;
}

/* network_game_client_idle_searching (0x1268a0)
 *
 * Called from the client idle dispatch (network_game_client_idle) when state == 0
 * (searching). All own-string names below are taken verbatim from this
 * function's log calls.
 *
 * Connectivity gate: unless splitscreen-local, checks
 * transport_network_available(); on failure logs "network connection went
 * down!" and calls display_error_when_main_menu_loaded(6), then returns
 * false — identical to network_game_client_idle_joining's own gate immediately below.
 *
 * If global_network_game_server_get() returns non-null (this machine is also
 * hosting), builds a loopback join request and self-joins: a zeroed 0xE4-byte
 * "game" record (transport_get_nonce fills 8 bytes at +0x24; the platform
 * field the callee checks, +0xde, stays 0 from the zero-fill — matching
 * network_game_client_initiate_join_game's own comment that the platform
 * check folds to a literal 0), a 0x22-byte "join_parameters" record (only
 * bytes 2-3 = 0 and bytes 0x12-0x21 = network_game_generate_join_game_token's
 * 16-byte token are ever written — bytes 0-1 and 4-0x11 are genuine
 * uninitialized stack content in the original, faithfully left uninitialized
 * here too), and a transport_address of 127.0.0.1:0x141e (only the first 4
 * address bytes, address_length, and port are set — bytes 4-15 of the
 * 16-byte address field are likewise genuinely uninitialized in the
 * original).
 *
 * Otherwise (still searching): network_connection_idle_server_reliable_endpoint(connection, 5000,
 * NULL), then network_game_client_process_incoming_messages(server) (process incoming messages) — either
 * failing logs and returns false. Then, if more than 2000ms elapsed since
 * the last broadcast (+0xc94), and no server has appeared meanwhile,
 * broadcasts a message_client_broadcast_game_search (type 0, 12 bytes: word
 * 0x141f, word 1, 8-byte nonce) to 255.255.255.255:0x141e and stamps +0xc94.
 * Otherwise (within the 2000ms window), if the "have a ping target" flag at
 * +0x82a is set and more than 1000ms elapsed since the last ping (+0x820),
 * sends a message_client_ping (type 1, 8 bytes: dword now_time, word 0x141f,
 * 2 bytes genuinely uninitialized) to the cached target address at +0x808
 * and stamps +0x820 — a failed ping write still returns true (only the
 * broadcast path treats a write failure as fatal). */
bool network_game_client_idle_searching(void *server)
{
  char *s;
  unsigned int now_time;
  bool ok;
  void *found_server;

  s = (char *)server;
  now_time = system_milliseconds();
  network_connection_keep_alive(*(int *)(s + 0x82c));

  ok = true;
  if (!network_game_is_splitscreen_local()) {
    ok = transport_network_available();
    if (!ok) {
      error(2, "network connection went down!");
      display_error_when_main_menu_loaded(6);
    }
  }
  if (!ok) {
    return ok;
  }

  found_server = global_network_game_server_get();
  if (found_server != NULL) {
    unsigned char game_buf[0xe4];
    unsigned char join_params[0x22];
    transport_address addr;
    unsigned int *p;
    int i;

    p = (unsigned int *)game_buf;
    for (i = 0; i < 0x39; i++) {
      p[i] = 0;
    }
    transport_get_nonce(game_buf + 0x24, 8);
    *(uint16_t *)(join_params + 2) = 0;
    network_game_generate_join_game_token(join_params + 0x12);
    *(unsigned int *)addr.address = 0x7f000001;
    addr.address_length = 4;
    addr.port = 0x141e;

    if (!network_game_client_initiate_join_game(server, game_buf, join_params,
                                                &addr)) {
      display_error_when_main_menu_loaded(7);
      network_event("network_game_client_initiate_join_game() failed");
      return false;
    }
    return ok;
  }

  if (!network_connection_idle(*(int *)(s + 0x82c), 5000, NULL)) {
    display_error_when_main_menu_loaded(7);
    network_event("network_connection_idle_server_reliable_endpoint() failed in "
                     "network_game_client_idle_searching()");
    return false;
  }
  if (!network_game_client_process_incoming_messages(server)) {
    network_event(
      "network_game_client_process_incoming_messages() failed in "
      "network_game_client_idle_searching()");
    return false;
  }

  if (now_time - *(unsigned int *)(s + 0xc94) > 2000) {
    unsigned char msg[0xc];
    unsigned short *packet;
    transport_address dest;

    if (global_network_game_server_get() != NULL) {
      return ok;
    }

    *(uint16_t *)msg = 0x141f;
    *(uint16_t *)(msg + 2) = 1;
    transport_get_nonce(msg + 4, 8);
    *(unsigned int *)dest.address = 0xffffffff;
    dest.address_length = 4;
    dest.port = 0x141e;

    packet = (unsigned short *)create_network_game_message(0, msg, 0xc);
    if (packet == NULL) {
      network_event(
        "failed to create a message_client_broadcast_game_search message");
      return ok;
    }
    if (network_connection_write(*(void **)(s + 0x82c), packet,
                                 (unsigned short)(*packet >> 4), (int)&dest,
                                 false)) {
      network_event("sent out a broadcast game search packet");
      *(unsigned int *)(s + 0xc94) = now_time;
      return ok;
    }
    network_event("network_game_client_write() failed while sending a "
                     "message_client_broadcast_game_search message");
    return false;
  }

  if (*(unsigned char *)(s + 0x82a) == 1 &&
      now_time - *(unsigned int *)(s + 0x820) > 1000) {
    unsigned char ping[8];
    unsigned short *packet;

    *(unsigned int *)ping = now_time;
    *(uint16_t *)(ping + 4) = 0x141f;

    packet = (unsigned short *)create_network_game_message(1, ping, 8);
    if (packet == NULL) {
      network_event("failed to create a message_client_ping message");
      return ok;
    }
    if (network_connection_write(*(void **)(s + 0x82c), packet,
                                 (unsigned short)(*packet >> 4),
                                 (int)(s + 0x808), false)) {
      *(unsigned int *)(s + 0x820) = now_time;
      return ok;
    }
    network_event("network_game_client_write() failed while sending a "
                     "message_client_ping message");
    return ok;
  }

  return ok;
}

/* network_game_client_idle_joining (0x126b60) — network_game_client_idle_joining
 *
 * Called from the client idle dispatch (network_game_client_idle) when state == 1
 * (joining). Verifies network connectivity, sends a join request once,
 * and checks for 120s timeout on the connect-process. Returns false
 * if connection drops, join request fails, or connection times out. */
bool network_game_client_idle_joining(void *server)
{
  char connected;
  unsigned char join_payload[0x50];
  unsigned short *encoded;
  int now_ms;
  int connect_handle;

  connected = 1;
  if (!network_game_is_splitscreen_local()) {
    connected = transport_network_available();
    if (!connected) {
      error(2, "network connection went down!");
      display_error_when_main_menu_loaded(6);
    }
  }

  if (connected) {
    if (network_connection_connected(*(int *)((char *)server + 0x82c))) {
      if ((*(unsigned char *)((char *)server + 0xcaa) & 2) == 0) {
        csmemset(join_payload, 0, 0x50);
        network_game_generate_local_machine_name(join_payload);
        csmemcpy(&join_payload[0x40], (char *)server + 0x84a, 0x10);
        encoded = (unsigned short *)create_network_game_message(
          0xc, join_payload, 0x50);
        if (encoded == NULL) {
          network_event(
            "failed to create a message_client_join_game_request message");
        } else if (network_connection_write(
                     (void *)*(int *)((char *)server + 0x82c), encoded,
                     (unsigned short)(*encoded >> 4), 0, 1)) {
          *(unsigned char *)((char *)server + 0xcaa) =
            *(unsigned char *)((char *)server + 0xcaa) | 2;
        } else {
          network_event("network_game_client_write() failed to send a "
                           "message_client_join_game_request message");
        }
      }
      *(int *)((char *)server + 0x830) = 0;
    } else {
      connect_handle = *(int *)((char *)server + 0x830);
      if (connect_handle != 0) {
        now_ms = (int)system_milliseconds();
        if ((unsigned int)(now_ms - *(int *)((char *)server + 0x834)) >
            120000) {
          network_event(
            "client connection process has timed out; aborting connection "
            "attempt");
          transport_server_terminate(*(int **)((char *)server + 0x830));
          *(int *)((char *)server + 0x830) = 0;
          return 0;
        }
      }
    }

    connected = network_connection_idle(*(int *)((char *)server + 0x82c), 5000, 0);
    if (connected) {
      connected = network_game_client_process_incoming_messages(server);
      if (!connected) {
        network_event(
          "network_game_client_process_incoming_messages() failed in "
          "network_game_client_idle_joining()");
        return 0;
      }
    } else {
      network_event("network_connection_idle_server_reliable_endpoint() failed in "
                       "network_game_client_idle_joining()");
    }
  }
  return connected;
}

/* network_game_client_idle_pregame (0x126ce0) — network_game_client_idle_pregame
 *
 * Called from the client idle dispatch (network_game_client_idle) when state == 2
 * (pregame). Checks network connectivity, processes the connection, and handles
 * incoming messages. Returns false if the connection drops or processing fails.
 */
bool network_game_client_idle_pregame(void *server)
{
  bool connected;

  connected = true;
  if (!network_game_is_splitscreen_local()) {
    connected = transport_network_available();
    if (!connected) {
      error(2, "network connection went down!");
      display_error_when_main_menu_loaded(6);
    }
  }

  if (connected) {
    if (network_connection_active(*(int *)((char *)server + 0x82c)) &&
        network_connection_connected(*(int *)((char *)server + 0x82c))) {
      network_game_client_update_precache_status(server);
      connected = network_connection_idle(*(int *)((char *)server + 0x82c), 15000, 0);
      if (connected) {
        connected = network_game_client_process_incoming_messages(server);
        if (connected)
          return true;
        network_event(
          "network_game_client_process_incoming_messages() failed in "
          "network_game_client_idle_pregame()");
      } else {
        network_event("network_connection_idle_server_reliable_endpoint() failed in "
                         "network_game_client_idle_pregame()");
      }
    } else {
      connected = false;
    }
  }

  if (!network_connection_active(*(int *)((char *)server + 0x82c))) {
    display_error_when_main_menu_loaded(4);
    return false;
  }
  return connected;
}

/* network_game_client_idle_ingame (0x126db0) — network_game_client_idle_ingame
 *
 * Called from the client idle dispatch (network_game_client_idle) when state == 3 (ingame).
 * Verifies the server connection is alive, checks if the connection has gone
 * silent (bit 5 of connection+0x30 via network_connection_going_stale),
 * displays per-player error widgets if newly silent, records the silent flag at
 * server+0xcad, then runs the connection idle tick (15-second timeout) and
 * processes incoming messages. Returns false if the connection drops or any
 * critical step fails.
 */
bool network_game_client_idle_ingame(void *server)
{
  char valid;
  char is_silent;
  char result;
  int connection;

  connection = *(int *)((char *)server + 0x82c);
  if (!network_connection_active(connection) ||
      !network_connection_connected(connection)) {
    error(2, "new idle in game abort hit");
    display_error_when_main_menu_loaded(4);
    return 0;
  }

  if (!network_game_is_splitscreen_local()) {
    valid = 1;
    is_silent =
      network_connection_going_stale(*(int *)((char *)server + 0x82c));
    if (!transport_network_available()) {
      display_error_when_main_menu_loaded(6);
      network_event("network connection went down (idle in game)!");
      valid = 0;
    } else if (is_silent && !*(char *)((char *)server + 0xcad)) {
      __int16 player_idx;
      player_idx = local_player_get_next(-1);
      while (player_idx != (__int16)-1) {
        display_error(9, player_idx, 0, 0);
        player_idx = local_player_get_next(player_idx);
      }
      network_event(
        "network client connection has been silent for a dangerously long "
        "amount of time");
    }
    *(char *)((char *)server + 0xcad) = is_silent;
    if (valid != 1)
      return valid;
  }

  result = network_connection_idle(*(int *)((char *)server + 0x82c), 15000, 0);
  if (result) {
    result = network_game_client_process_incoming_messages(server);
    if (!result) {
      network_event(
        "network_game_client_process_incoming_messages() failed in "
        "network_game_client_idle_ingame()");
      return result;
    }
  } else {
    connection = *(int *)((char *)server + 0x82c);
    if (!network_connection_active(connection) ||
        !network_connection_connected(connection)) {
      error(2, "new2 idle in game abort hit");
      display_error_when_main_menu_loaded(4);
      result = 0;
    }
    network_event(
      "network_connection_idle_server_reliable_endpoint() failed in network_game_client_idle_ingame()");
  }
  return result;
}

/* network_game_client_idle_postgame (0x126f40) — network_game_client_idle_postgame
 *
 * Called from the client idle dispatch (network_game_client_idle) when state == 4
 * (postgame). Checks network connectivity, runs the connection idle with a
 * 15-second timeout, and processes incoming messages. Returns false if the
 * connection drops or processing fails. */
bool network_game_client_idle_postgame(void *server)
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
  result = network_connection_idle(*(int *)((char *)server + 0x82c), 15000, 0);
  if (!result) {
    network_event("network_connection_idle_server_reliable_endpoint() failed in "
                     "network_game_client_idle_postgame()");
    goto tail_check;
  }
  result = network_game_client_process_incoming_messages(server);
  if (result)
    return result;
  network_event("network_game_client_process_incoming_messages() failed in "
                   "network_game_client_idle_postgame()");

tail_check:
  if (!network_connection_active(*(int *)((char *)server + 0x82c))) {
    display_error_when_main_menu_loaded(4);
    return false;
  }
  return result;
}

/* 0x126fe0 — network_game_create_client (name from the failure log string at
 * 0x293180, "network_game_create_client() failed; could not create network
 * connection"; kept as network_game_client_create here so the lift pipeline can resolve it).
 *
 * Allocates the singleton network game client. Asserts the in-use flag at
 * 0x46e8b9 is clear (assert line 0x94), sets it, zeroes the 0xcb0-byte client
 * block at 0x5a95a0, then opens the client connection. The connection handle
 * is stored at 0x5a9dcc, which is client + 0x82c — the same field
 * network_game_client_dispose and network_game_client_keep_alive read; MSVC
 * folded the base+offset into an absolute store (MOV [0x005a9dcc],EAX).
 * On success resets the client and returns the block; on failure logs, tears
 * the client down again, and returns NULL.
 *
 * The connection handle is held in a local: the reference tests EAX before
 * storing it (TEST EAX,EAX at 0x127036, MOV [0x5a9dcc],EAX at 0x127038), so
 * there is no reload of the global for the branch.
 *
 * FUN_001267c0/network_game_client_reset is called with TWO stack arguments
 * here (PUSH 0x0 / PUSH 0x5a95a0 / ADD ESP,0x8 at 0x12703f-0x12704b); its own
 * prologue reads the client from [EBP+8] and a byte flag from [EBP+0xc]
 * (0x1267c5, 0x1267fa). The kb decl was `(void)` and has been widened. */
void *network_game_client_create(void)
{
  int connection;

  if (*(char *)0x46e8b9 != '\0') {
    display_assert("!network_game_client_dont_use_directly_in_use",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x94, 1);
    system_exit(-1);
  }
  *(char *)0x46e8b9 = 1;
  csmemset((void *)0x5a95a0, 0, 0xcb0);
  connection = network_connection_new(2, 0x141f);
  *(int *)0x5a9dcc = connection;
  if (connection != 0) {
    network_game_client_reset((void *)0x5a95a0, 0);
    return (void *)0x5a95a0;
  }
  network_event(
    "network_game_create_client() failed; could not create network connection");
  network_game_client_dispose((void *)0x5a95a0);
  return NULL;
}

/* 0x127070 — Network client idle dispatch: asserts client non-null, switches
 * on the connection state at offset 0xca6, and calls the appropriate
 * state-specific idle handler. Logs and returns false on handler failure. */
bool network_game_client_idle(void *server)
{
  bool result;

  result = 0;
  assert_halt(server);
  switch (*(unsigned short *)((char *)server + 0xca6)) {
  case 0:
    result = network_game_client_idle_searching(server);
    if (!result) {
      network_event("network_game_client_idle_searching() failed");
      return result;
    }
    break;
  case 1:
    result = network_game_client_idle_joining(server);
    if (!result) {
      network_event("network_game_client_idle_joining() failed");
      return result;
    }
    break;
  case 2:
    result = network_game_client_idle_pregame(server);
    if (!result) {
      network_event("network_game_client_idle_pregame() failed");
      return result;
    }
    break;
  case 3:
    result = network_game_client_idle_ingame(server);
    if (!result) {
      network_event("network_game_client_idle_ingame() failed");
      return result;
    }
    break;
  case 4:
    result = network_game_client_idle_postgame(server);
    if (!result) {
      network_event("network_game_client_idle_postgame() failed");
      return result;
    }
    break;
  default:
    assert_halt(!"unknown client state");
  }
  return result;
}

/* 0x1271a0 — Handles a server rejection message: resets the connection state
 * field (int16 at +0xca6) back to searching (0), maps the 16-bit rejection code
 * to its diagnostic string, logs it, and tears the client connection down.
 *
 * The rejection code is read zero-extended (MOVZX EAX,word ptr [EBP+0x10] at
 * 0x1271d8) and pushed as the %d argument, so the parameter is a 16-bit
 * unsigned value in a dword stack slot. Codes above 6 fall through the jump
 * table's bounds check (CMP EAX,6 / JA) and keep the "<unknown>" default that
 * ESI is preloaded with at 0x1271aa.
 *
 * source_address is asserted but never dereferenced here, so its pointee type
 * is unknown. kb.json declared this function void(void), contradicting the
 * three stack slots it reads; the decl is corrected here.
 *
 * network_game_client_reset is called with (client, 1) — PUSH 0x1 at 0x12722c
 * then PUSH EDI at 0x12722e; the combined ADD ESP,0x14 at 0x127234 cleans both
 * this 2-arg call and the preceding 3-arg log call. */
void network_game_client_rejected_by_game(void *client, void *source_address, uint16_t rejection_code)
{
  const char *reason;

  reason = "<unknown>";
  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x35a, client && source_address);

  *(int16_t *)((char *)client + 0xca6) = 0;
  switch (rejection_code) {
  case 0:
    reason = "_rejection_code_version_too_old";
    break;
  case 1:
    reason = "_rejection_code_version_too_new";
    break;
  case 2:
    reason = "_rejection_code_bad_join_token";
    break;
  case 3:
    reason = "_rejection_code_bad_password";
    break;
  case 4:
    reason = "_rejection_code_game_is_full";
    break;
  case 5:
    reason = "_rejection_code_game_is_closed";
    break;
  case 6:
    reason = "_rejection_code_blacklisted_machine";
    break;
  }
  network_event("unable to join game: reason= #%d/%s", rejection_code,
                   reason);
  network_game_client_reset(client, 1);
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_game_advertise — handle message_server_game_advertise (type 2).
 * (client @esi, message, message_size, source_address) -> char.
 * source_address is never read. Ignores the advertisement unless the client
 * is actively looking for new games (get_state() == 0). Decodes the packet
 * (type 2, version 1, flag 1) into a 276-byte stack buffer via
 * decode_network_game_message() (decode_network_game_message), then checks the leading
 * 8 bytes of the decoded buffer against the global transport nonce
 * (transport_nonce_is_equal_to_global) before forwarding to
 * network_game_client_new_advertised_game(). Every path returns 1.
 * Confirmed via disassembly (0x127260-0x127308): PUSH 0x0/PUSH ESI into
 * get_state(); local dword at EBP-0x8 (=1) and EBP-0x4 (=2) are adjacent,
 * forming the decompiler's "local_c[4]" — &EBP-0x4 is the type slot,
 * &EBP-0x8 the version slot, matching sibling decode call sites in this
 * TU. message_size is decremented and reused in place (no separate local).
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_game_advertise(void *client, void *message, int message_size,
                  void *source_address)
{
  char decoded[276];
  int packet_type;
  int packet_version;

  if (network_game_client_get_state(client, (void *)0) == 0) {
    message_size -= 2;
    packet_type = 2;
    packet_version = 1;
    if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                     (short *)&packet_type, (short *)&packet_version, 1)) {
      if (transport_nonce_is_equal_to_global(decoded, 8)) {
        network_game_client_new_advertised_game(client, decoded);
      }
    } else {
      network_event(
        "failed to decode a message_server_game_advertise packet");
    }
  } else {
    network_event(
      "ignoring an advertised game because we are not looking for new games");
  }
  return 1;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_pong — handle message_server_pong (type 3).
 * (client @esi, message, message_size, source_address) -> char.
 * Ignores the pong unless the client is actively listening for them
 * (get_state() == 0). Decodes the packet (type 3, version 1, flag 1) into
 * a 4-byte stack buffer (the echoed send timestamp) and forwards it to
 * network_game_client_ponged() along with source_address. Every path
 * returns 1.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_pong(void *client, void *message, int message_size,
                  void *source_address)
{
  char decoded[4];
  int packet_type;
  int packet_version;

  if (network_game_client_get_state(client, (void *)0) == 0) {
    message_size -= 2;
    packet_type = 3;
    packet_version = 1;
    if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                     (short *)&packet_type, (short *)&packet_version, 1)) {
      network_game_client_ponged(client, source_address,
                                 *(unsigned int *)decoded);
    } else {
      network_event("failed to decode a message_server_pong packet");
    }
  } else {
    network_event(
      "ignoring a pong message because we are not listening for them");
  }
  return 1;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_machine_accepted — handle message_server_machine_accepted (type 4).
 * (client @esi, source_address @edi, message, message_size) -> char.
 * Ignores the message unless source_address matches the server we're
 * addressing (network_game_client_address_matches_server) AND we are
 * actively awaiting acceptance (get_state() == 1). Decodes the packet
 * (type 4, version 1, flag 2) into an 8-byte stack buffer (seed dword +
 * assigned machine index) and forwards it to
 * network_game_client_accepted_into_game(). Returns 1 only on the accepted
 * path; every rejection/failure path returns 0.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_machine_accepted(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[8];
  int packet_type;
  int packet_version;
  char result;

  result = 1;
  if (network_game_client_address_matches_server(client, source_address) &&
      network_game_client_get_state(client, (void *)0) == 1) {
    message_size -= 2;
    packet_type = 4;
    packet_version = 1;
    if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                     (short *)&packet_type, (short *)&packet_version, 2)) {
      network_game_client_accepted_into_game(client, source_address, decoded);
    } else {
      network_event(
        "failed to decode a message_server_machine_accepted packet");
      result = 0;
    }
  } else {
    network_event(
      "ignoring a message_server_machine_accepted message; either a bad "
      "machine or we aren't joining");
    result = 0;
  }
  return result;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_machine_rejected — handle message_server_machine_rejected (type 5).
 * (client @esi, source_address @edi, message, message_size) -> char.
 * Ignores the message unless source_address matches the server AND we are
 * actively awaiting acceptance (get_state() == 1). Decodes the packet
 * (type 5, version 1, flag 2) into a 4-byte stack buffer (rejection code)
 * and forwards it to network_game_client_rejected_by_game(). Returns 1 only on the accepted
 * decode path; every rejection/failure path returns 0.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_machine_rejected(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;
  char result;

  result = 1;
  if (network_game_client_address_matches_server(client, source_address) &&
      network_game_client_get_state(client, (void *)0) == 1) {
    message_size -= 2;
    packet_type = 5;
    packet_version = 1;
    if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                     (short *)&packet_type, (short *)&packet_version, 2)) {
      network_game_client_rejected_by_game(client, source_address, *(unsigned int *)decoded);
    } else {
      network_event(
        "failed to decode a message_server_machine_rejected packet");
      result = 0;
    }
  } else {
    network_event(
      "ignoring a message_server_machine_rejected message; either a bad "
      "machine or we aren't joining");
    result = 0;
  }
  return result;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_game_settings_update — handle message_server_game_settings_update (type 6).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Asserts client and source_address are non-null (original assert strings
 * recovered from network_client_message_handler.c, lines 0x169/0x16a —
 * this handler's asm frame layout doesn't match that TU's other functions,
 * so it stays here). Ignores the message (returns 1, logged) unless
 * source_address matches the server AND we are in the pregame state
 * (get_state() == 2). Decodes the packet (type 6, version 1, flag 2) into
 * a 1076-byte stack buffer and forwards it to
 * network_game_client_game_settings_updated(), whose bool result becomes
 * the return value (logged on failure).
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_game_settings_update(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[1076];
  int packet_type;
  int packet_version;
  char result;

  result = 0;
  assert_halt_at(
    "c:\\halo\\SOURCE\\networking\\network_client_message_handler.c", 0x169,
    client != NULL);
  assert_halt_at(
    "c:\\halo\\SOURCE\\networking\\network_client_message_handler.c", 0x16a,
    source_address != NULL);

  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 2) {
      message_size -= 2;
      packet_type = 6;
      packet_version = 1;
      if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                       (short *)&packet_type, (short *)&packet_version, 2)) {
        result = network_game_client_game_settings_updated(client, decoded);
        if (result == 0) {
          network_event(
            "network_game_client_game_settings_updated() failed");
        }
        return result;
      }
      network_event(
        "failed to decode a message_server_game_settings_update packet");
      return result;
    }
    network_event(
      "failed to handle a message_server_game_settings_update message; "
      "not in pregame state");
    return 1;
  }
  network_event(
    "ignoring a message_server_game_settings_update; came from a bad "
    "machine");
  return 1;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_pregame_countdown — handle message_server_pregame_countdown (type 7).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Asserts client and source_address are non-null (original assert strings
 * recovered from network_client_message_handler.c, lines 0x19b/0x19c).
 * Ignores the message (logged) unless source_address matches the server
 * AND we are in the pregame state (get_state() == 2). Decodes the packet
 * (type 7, version 1, flag 2) into a 4-byte stack buffer (the countdown
 * value) and forwards it to network_game_client_countdown_timer_update().
 * Every path returns 1.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_pregame_countdown(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;

  assert_halt_at(
    "c:\\halo\\SOURCE\\networking\\network_client_message_handler.c", 0x19b,
    client != NULL);
  assert_halt_at(
    "c:\\halo\\SOURCE\\networking\\network_client_message_handler.c", 0x19c,
    source_address != NULL);

  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 2) {
      message_size -= 2;
      packet_type = 7;
      packet_version = 1;
      if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                       (short *)&packet_type, (short *)&packet_version, 2)) {
        network_game_client_countdown_timer_update(client,
                                                   *(unsigned int *)decoded);
      } else {
        network_event(
          "failed to decode a message_server_pregame_countdown packet");
      }
    } else {
      network_event(
        "failed to handle a message_server_pregame_countdown message; not "
        "in pregame state");
    }
  } else {
    network_event(
      "ignoring a message_server_pregame_countdown; came from a bad "
      "machine");
  }
  return 1;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_pregame_keep_alive — handle message_server_pregame_keep_alive (type 10).
 * (client @esi, source_address @edi, message, message_size) -> char.
 * Asserts client and source_address are non-null (original assert strings
 * recovered from network_client_message_handler.c, lines 0x1c4/0x1c5).
 * Ignores the message (logged) unless source_address matches the server
 * AND we are in the pregame state (get_state() == 2). Decodes the packet
 * (type 10, version 1, flag 2) into a 4-byte stack buffer that is not
 * otherwise used — this is a keep-alive ping, the decode call itself is
 * the only side effect. Every path returns 1.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_pregame_keep_alive(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;

  assert_halt_at(
    "c:\\halo\\SOURCE\\networking\\network_client_message_handler.c", 0x1c4,
    client != NULL);
  assert_halt_at(
    "c:\\halo\\SOURCE\\networking\\network_client_message_handler.c", 0x1c5,
    source_address != NULL);

  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 2) {
      message_size -= 2;
      packet_type = 10;
      packet_version = 1;
      if (!decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                        (short *)&packet_type, (short *)&packet_version, 2)) {
        network_event(
          "failed to decode a message_server_pregame_keep_alive packet");
      }
    } else {
      network_event(
        "failed to handle a message_server_pregame_keep_alive message; not "
        "in pregame state");
    }
  } else {
    network_event(
      "ignoring a message_server_pregame_keep_alive; came from a bad "
      "machine");
  }
  return 1;
}
