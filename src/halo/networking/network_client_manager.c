/* 0x124900 — Walks the tag_block at offset 0xd0 of the definition, and for
 * each 0x30-byte element walks the nested tag_block at element+0x24 whose
 * elements are 0x68 bytes. Both retrieved elements are discarded: only
 * tag_block_get_element's bounds-check side effects are observed. Both loop
 * counters are 16-bit and are sign-extended before the comparison against the
 * 32-bit block count, which is re-read from memory on every iteration
 * (MOVSX EAX,AX / MOVSX EAX,DI at 0x12494f and 0x12495f).
 * Parameter is the stack slot at [EBP+8]; there are no direct callers in the
 * binary, so the pointee type is unknown. */
void FUN_00124900(void *definition)
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

/* 0x1249b0 — network_game_server_dispose.
 * Tears down the network game client connection. If the server pointer is
 * non-null, closes its connection handle and clears the in-use flag.
 *
 * noinline (VC71 verification only): the original build emits this as a real
 * call from FUN_00126fe0 (CALL 0x001249b0 at 0x127061). /O2's implied /Ob2
 * otherwise inlines the whole body — including the line-0xb2 assert — into
 * that failure path, adding 14 instructions that the reference does not have.
 * FUN_00126fe0 is its only in-TU caller. */
__declspec(noinline) void network_game_server_dispose(void *server)
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
    network_game_log("attempting to connect to game @ %s",
                     transport_address_to_string(address));
  } else {
    display_error_when_main_menu_loaded(7);
    network_game_log("failed attempt to initiate a connection to game @ %s",
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

/* FUN_00124C80 (0x124c80)
 *
 * Asserts client is non-null, then returns the address of the client field at
 * offset 4 (LEA EAX,[ESI+0x4] at 0x124cab). The assert message string at
 * 0x2917a8 is "client" and the recovered assert line is 0x2ac. The pointee
 * type at +4 is not established by this function; the sole caller is
 * FUN_000f0f30 (0xf0f73), so the field meaning is unknown.
 */
void *FUN_00124C80(void *client)
{
  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x2ac, client);

  return (void *)((char *)client + 4);
}

/* 0x124cc0 — Asserts client is non-null and returns the int16_t field at
 * offset 0xca8. */
int16_t FUN_00124cc0(void *server)
{
  assert_halt(server);
  return *(int16_t *)((char *)server + 0xca8);
}

/* FUN_00124D00 (0x124d00)
 *
 * Asserts client is non-null and returns the 16-bit field at +0xca4.
 * Evidence (0x124d00-0x124d34): MOV ESI,[EBP+8]; TEST ESI,ESI; JNZ over the
 * assert block, which pushes ("client", "c:\halo\SOURCE\networking\
 * network_client_manager.c", 0x2bc, true) to display_assert (0x8d9f0) then
 * system_exit(-1) (0x8e2f0). The value is loaded with a bare `MOV AX,word ptr
 * [ESI+0xca4]` — no MOVSX/MOVZX — so the return width is exactly 16 bits and
 * the signedness is NOT established by this function; int16_t is chosen to
 * match the sibling accessor at +0xca8 (FUN_00124cc0) on the same struct.
 * The meaning of the field at +0xca4 is unknown; its three callers
 * (network_pregame_status_screen_update 0xf1f6c,
 * game_options_menu_update_pic_desc 0xf34b5, FUN_000f1710 0xf17be) are UI
 * update paths.
 */
int16_t FUN_00124D00(void *client)
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
 * call to encode_network_game_message.
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
    network_game_log("received a message_server_machine_accepted message with "
                     "a bad machine_index");
  } else {
    *(int16_t *)client = machine_index;
    *((char *)client + *(int16_t *)((char *)message_packet + 4) * 0x44 +
      0x9b0) = *(char *)((char *)message_packet + 4);
    *(int16_t *)((char *)client + 0xca6) = 2;
    network_game_set_random_seed(*(int *)message_packet);
    network_game_log("successfully joined a net game; our machine is #%d",
                     (int)*(int16_t *)((char *)message_packet + 4));
    network_game_generate_local_machine_name(settings_request);
    settings_request[0x40] = *(char *)((char *)message_packet + 4);
    message = encode_network_game_message(0xf, settings_request, 0x44);
    if (message == NULL) {
      network_game_log(
        "failed to create a message_client_settings_request message");
      return;
    }
    if (!network_connection_write(
          *(void **)((char *)client + 0x82c), message,
          (unsigned short)(*(unsigned short *)message >> 4), 0, true)) {
      network_game_log("network_game_client_write() failed while sending a "
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
    network_game_log("invalid message_server_game_settings_update message "
                     "received player count %d machine count %d",
                     (int)*(int16_t *)((char *)message_packet + 0x224),
                     (int)machine_count);
    return false;
  }

  map_name = (char *)message_packet + 0x24;
  if (csstrcmp(map_name, (char *)client + 0x880) != 0) {
    network_game_log("precaching map '%s'...", map_name);
    main_set_multiplayer_map_name(map_name);
  }

  csmemcpy(saved_settings, (char *)client + 0x85c, 0x434);
  csmemcpy((char *)client + 0x85c, message_packet, 0x434);
  csmemcpy((char *)client + 0xc8c, saved_settings + 0x430, 4);

  network_game_log("received updated game settings from the server; there are "
                   "%d players on %d machines in the game",
                   (int)*(int16_t *)((char *)message_packet + 0x224),
                   (int)*(int16_t *)((char *)message_packet + 0x112));
  network_game_log("player count %d machine count %d",
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
    network_game_log("switching to pregame");
    network_game_reset_to_pregame_ui();
    network_connection_keep_alive(*(int *)((char *)client + 0x82c));
  }
  return 1;
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
    (unsigned short *)encode_network_game_message(0x10, player_settings, 0x20);
  if (encoded != NULL) {
    if (network_connection_write(*(void **)((char *)client + 0x82c), encoded,
                                 (unsigned short)(*encoded >> 4), 0, true)) {
      return 1;
    }
    network_game_log("network_game_client_update_local_player_data() failed "
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
    encoded = (unsigned short *)encode_network_game_message(0x11, &message, 2);
    if (encoded != NULL) {
      if (!network_connection_write(*(void **)((char *)client + 0x82c), encoded,
                                    (unsigned short)(*encoded >> 4), 0, true)) {
        network_game_log("network_game_client_request_start_time_change() "
                         "failed to send a message_client_game_start_request "
                         "message");
      }
    }
  } else {
    network_game_log("failed to send a message_client_game_start_request "
                     "because we are not in the pregame state");
  }
  return 1;
}

/* 0x125c60 — network_game_client_countdown_timer_update
 *
 * Two-argument setter for the 16-bit field at client+0xca4 — the same field
 * the accessor FUN_00124D00 (0x124d00) reads. Asserts client non-null at line
 * 0x5c3 with reason string "client", then stores the caller's second (16-bit)
 * stack argument.
 *
 * Evidence (0x125c60-0x125ca6): MOV ESI,[EBP+8]; TEST ESI,ESI; JNZ 0x125c99
 * over PUSH 0x1 / PUSH 0x5c3 / PUSH 0x291774 / PUSH 0x2917a8 / CALL
 * display_assert (0x8d9f0); PUSH -0x1 / CALL system_exit (0x8e2f0). The store
 * is `MOV CX,word ptr [EBP+0xc]` followed by `MOV word ptr [ESI+0xca4],CX` —
 * exactly 16 bits wide, and the signedness is NOT established here (int16_t
 * chosen to match FUN_00124D00 on the same field). The epilogue is POP ESI /
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

/* FUN_00125fb0 (0x125fb0)
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
void FUN_00125fb0(unsigned short reason, void *client)
{
  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x662, client);

  if (reason >= 9)
    reason = 1;
  if (*(int16_t *)((char *)client + 0xca8) == 0)
    *(int16_t *)((char *)client + 0xca8) = (int16_t)reason;
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

/* 0x126700 — network_game_client_new_advertised_game
 *
 * Two-argument cdecl handler. Asserts both arguments non-null at assert line
 * 0x2fc with reason string "client && message_packet", then tail-forwards
 * client+4 to FUN_00125ce0 and returns nothing.
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
 * one cdecl argument, caller-cleanup. kb.json declared FUN_00125ce0 as
 * void(void), which contradicts the single push and the ADD ESP,0x4; the
 * callee decl is corrected to take one pointer. EDI is loaded only to serve
 * the TEST EDI,EDI of the assert; message_packet is NOT passed to the callee
 * and is not otherwise read, so its only observable use in this function is
 * the null check.
 *
 * The pointee at client+4 is NOT identified here — nothing in this function
 * dereferences it, so the callee argument keeps a mechanical form rather than
 * a named field. kb.json declared this function void(void), contradicting the
 * two stack slots it reads; the decl is corrected here. */
void network_game_client_new_advertised_game(void *client, void *message_packet)
{
  assert_halt_at("c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                 0x2fc, client && message_packet);

  FUN_00125ce0((char *)client + 4);
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
  network_game_log("the game host is shutting down");
  network_game_client_all_local_players_have_quit();
}

/* network_game_client_reset (0x1267c0)
 *
 * Asserts client is non-null (line 0x4ee), invalidates the client's embedded
 * network-game block (+0x85c) and resets the per-join bookkeeping fields.
 *
 * When `reinitialize` is set and the client still owns a connection handle
 * (+0x82c) that is currently connected, the join-in-progress flag at +0xc90 is
 * raised and FUN_00129980 re-initializes the connection; success clears bit 0
 * of the flag byte at +0xcaa, failure reports reason 1 through FUN_00125fb0
 * (register ABI: reason@<eax>, client@<esi>; ESI already holds the client at
 * 0x12684d, so the call site is MOV EAX,1 / CALL 0x125fb0) and logs.
 *
 * Field widths are taken from the store instructions, not the decompiler:
 * MOV word at +0 / +0xca6 / +0xca8 / +0xca4, MOV dword at +0xc90 / +0xc94 /
 * +0xc98 / +0xc9c, MOV byte at +0xcad then +0xcac (that store order is the
 * reference order, 0x126884 before 0x12688a). The meanings of the individual
 * fields are unproven; only their widths and reset values are. */
void network_game_client_reset(void *client, bool reinitialize)
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
    if (FUN_00129980(*(int *)((char *)client + 0x82c))) {
      *(unsigned char *)((char *)client + 0xcaa) =
        *(unsigned char *)((char *)client + 0xcaa) & 0xfe;
    } else {
      FUN_00125fb0(1, client);
      network_game_log("failed to reinitialize network game client");
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

/* 0x126fe0 — network_game_create_client (name from the failure log string at
 * 0x293180, "network_game_create_client() failed; could not create network
 * connection"; kept as FUN_00126fe0 here so the lift pipeline can resolve it).
 *
 * Allocates the singleton network game client. Asserts the in-use flag at
 * 0x46e8b9 is clear (assert line 0x94), sets it, zeroes the 0xcb0-byte client
 * block at 0x5a95a0, then opens the client connection. The connection handle
 * is stored at 0x5a9dcc, which is client + 0x82c — the same field
 * network_game_server_dispose and network_game_client_keep_alive read; MSVC
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
void *FUN_00126fe0(void)
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
  network_game_log(
    "network_game_create_client() failed; could not create network connection");
  network_game_server_dispose((void *)0x5a95a0);
  return NULL;
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
