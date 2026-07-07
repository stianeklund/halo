/* ========================================================================
 * network_client_message_handler.c
 * Original source: c:\halo\SOURCE\networking\network_client_message_handler.c
 *
 * Client-side handling of server->client network-game messages.  The
 * dispatcher (FUN_00127ea0) validates the framed message, extracts the
 * message-type byte, and forwards to a per-type handler.  Each handler
 * validates the source machine and the client game-state, decodes the
 * packet with decode_network_game_message() (FUN_0012bce0), then applies
 * the requested state change.
 *
 * ABI note (critical): the per-type handlers are NOT plain cdecl.  The
 * network-game-client pointer arrives in ESI, and the source transport
 * address arrives in EAX (most handlers) or EDI (the keep-alive handlers).
 * The message buffer and its byte length are the only true stack args.
 * These register arguments are invisible in the Ghidra pseudocode (shown
 * as unaff_ESI / param_1) and were recovered from the disassembly
 * prologues.  The @<reg> annotations in kb.json encode this.
 *
 * Handlers for the message types owned by network_client_manager.obj
 * (FUN_00127260 game_advertise .. FUN_00127710 pregame_keep_alive) are
 * declared here via decl.h but implemented in that TU.
 * ======================================================================== */
#include "../../common.h"

/* ------------------------------------------------------------------------
 * FUN_00127ea0 — network_game_client_handle_message (dispatcher).
 * cdecl(client, message, message_size, source_address) -> bool.
 * Validates the framed message header, rejects bad flags / message
 * categories, then dispatches on the trailing message-type byte to the
 * matching per-type handler.  Returns the handler's result (1 on
 * success/ignored, 0 on handler failure).
 * Confirmed: arg layout, header math (>>4 size, &3 flags, >>2&3 category),
 *   type byte = message[(short)message_size-1], case->handler map, register
 *   setup for each handler (client=ESI, source_address=EAX/EDI/stack).
 * ---------------------------------------------------------------------- */
bool FUN_00127ea0(void *client, void *message, int message_size,
                  void *source_address)
{
  char result;
  unsigned short header;
  unsigned char category;
  int type_byte;

  result = 1;

  assert_halt_msg(
    client != (void *)0 && message != (void *)0 &&
      (unsigned short)message_size ==
        (unsigned short)(*(unsigned short *)message >> 4) &&
      source_address != (void *)0,
    "client && message && (message_size == GET_MESSAGE_SIZE(*message)) && "
    "source_address");

  header = *(unsigned short *)message;
  category = (unsigned char)((header >> 2) & 3);

  if ((header & 3) != 0) {
    network_game_log("client received client message with invalid flags");
    return 1;
  }

  if (category == 1) {
    if (0x82 < (unsigned short)message_size) {
      network_game_log("client received low-level error message: error= #%d (%s)",
                       (unsigned char)((unsigned short *)message)[0x41],
                       (unsigned short *)message + 1);
      return 1;
    }
    network_game_log("client received a malformed/damaged message from a server");
    /* falls through to `return result` (== 1) */
  } else if (category == 2) {
    network_game_log("client received a bad message type (_message_type_data)");
    return 1;
  } else if (category != 3) {
    network_game_log("client received a message with an unknown message type");
    return 1;
  } else {
    type_byte = (int)((unsigned char *)message)[(short)message_size - 1];
    switch (type_byte) {
    case 2:
      result = FUN_00127260(client, message, message_size, source_address);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_game_advertise() failed");
        return 0;
      }
      break;
    case 3:
      result = FUN_00127310(client, message, message_size, source_address);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_pong() failed");
        return 0;
      }
      break;
    case 4:
      result = FUN_001273a0(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_machine_accepted() failed");
        return 0;
      }
      break;
    case 5:
      result = FUN_00127440(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_machine_rejected() failed");
        return 0;
      }
      break;
    case 6:
      result = FUN_001274E0(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_game_settings_update() "
          "failed");
        return 0;
      }
      break;
    case 7:
      result = FUN_00127610(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_pregame_countdown() failed");
        return 0;
      }
      break;
    case 8:
      result = FUN_001278d0(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_begin_game() failed");
        return 0;
      }
      break;
    case 9:
      result = FUN_001279a0(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log("network_game_client_handle_message_server_graceful_"
                         "game_exit_pregame() failed");
        return 0;
      }
      break;
    case 10:
      result = FUN_00127710(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_pregame_keep_alive() "
          "failed");
        return 0;
      }
      break;
    case 0xb:
      result = FUN_001277f0(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_postgame_keep_alive() "
          "failed");
        return 0;
      }
      break;
    case 0x14:
      result = FUN_00127a50(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_game_update() failed");
        return 0;
      }
      break;
    case 0x15:
      result = FUN_00127b10(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_add_player_ingame() "
          "failed");
        return 0;
      }
      break;
    case 0x16:
      result = FUN_00127bd0(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_remove_player_ingame() "
          "failed");
        return 0;
      }
      break;
    case 0x17:
      result = FUN_00127c90(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_game_over() failed");
        return 0;
      }
      break;
    case 0x1e:
      result = FUN_00127d30(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log(
          "network_game_client_handle_message_server_switch_to_pregame() "
          "failed");
        return 0;
      }
      break;
    case 0x1f:
      result = FUN_00127df0(client, source_address, message, message_size);
      if (result == 0) {
        network_game_log("network_game_client_handle_message_server_graceful_"
                         "game_exit_postgame() failed");
        return 0;
      }
      break;
    default:
      network_game_log("unknown packet type received from system @ address: %s",
                       transport_address_to_string(source_address));
      return 1;
    }
  }

  return result;
}

/* ------------------------------------------------------------------------
 * FUN_001277f0 — handle message_server_postgame_keep_alive (type 0xb).
 * (client @esi, source_address @edi, message, message_size) -> char.
 * Requires the source to match the server and the client to be in postgame
 * (state == 4), then decodes the keep-alive packet.  Every path returns 1.
 * Confirmed: @esi/@edi register args + NULL asserts, state==4, type 0xb,
 *   version 1, decode flags 6.
 * ---------------------------------------------------------------------- */
char FUN_001277f0(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;

  assert_halt_msg(client != (void *)0, "client != NULL");
  assert_halt_msg(source_address != (void *)0, "source_address != NULL");

  if (!network_game_client_address_matches_server(client, source_address)) {
    network_game_log(
      "ignoring a message_server_postgame_keep_alive; came from a bad machine");
    return 1;
  }
  if (network_game_client_get_state(client, (void *)0) == 4) {
    message_size -= 2;
    packet_type = 0xb;
    packet_version = 1;
    if (!FUN_0012bce0((int)decoded, (int)message + 2, (short *)&message_size,
                      (short *)&packet_type, (short *)&packet_version, 6)) {
      network_game_log(
        "failed to decode a message_server_postgame_keep_alive packet");
      return 1;
    }
  } else {
    network_game_log("failed to handle a message_server_postgame_keep_alive "
                     "message; not in postgame state");
    return 1;
  }
  return 1;
}

/* ------------------------------------------------------------------------
 * FUN_001278d0 — handle message_server_begin_game (type 8).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and pregame state (== 2), decodes the packet,
 * then starts the client game.  Returns 1 for a bad machine; 0 for wrong
 * state or decode failure; otherwise the result of game_has_started().
 * ---------------------------------------------------------------------- */
char FUN_001278d0(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;
  char started;

  if (!network_game_client_address_matches_server(client, source_address)) {
    network_game_log(
      "ignoring a message_server_begin_game message; came from a bad machine");
    return 1;
  }
  if (network_game_client_get_state(client, (void *)0) != 2) {
    network_game_log("failed to handle a message_server_begin_game message; we "
                     "are not in pregame");
    return 0;
  }
  message_size -= 2;
  packet_type = 8;
  packet_version = 1;
  if (!FUN_0012bce0((int)decoded, (int)message + 2, (short *)&message_size,
                    (short *)&packet_type, (short *)&packet_version, 2)) {
    network_game_log("failed to decode a message_server_begin_game packet");
    return 0;
  }
  started = network_game_client_game_has_started(client);
  if (!started) {
    network_game_log("network_game_client_game_has_started() failed");
  }
  return started;
}

/* ------------------------------------------------------------------------
 * FUN_001279a0 — handle message_server_graceful_game_exit_pregame (type 9).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and pregame state (== 2), decodes the packet,
 * then shuts the client game down.  Every path returns 1.
 * ---------------------------------------------------------------------- */
char FUN_001279a0(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;

  if (!network_game_client_address_matches_server(client, source_address)) {
    network_game_log("ignoring a message_server_graceful_game_exit_pregame "
                     "message; came from a bad machine");
    return 1;
  }
  if (network_game_client_get_state(client, (void *)0) == 2) {
    message_size -= 2;
    packet_type = 9;
    packet_version = 1;
    if (FUN_0012bce0((int)decoded, (int)message + 2, (short *)&message_size,
                     (short *)&packet_type, (short *)&packet_version, 2)) {
      network_game_client_game_shutdown(client);
      return 1;
    }
    network_game_log(
      "failed to decode a message_server_graceful_game_exit_pregame packet");
    return 1;
  }
  network_game_log("failed to handle a message_server_graceful_game_exit_pregame"
                   " message; we are not in pregame");
  return 1;
}

/* ------------------------------------------------------------------------
 * FUN_00127a50 — handle message_server_game_update (type 0x14).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and in-game state (== 3), decodes the update
 * into a 528-byte scratch buffer, then applies it.  A bad machine returns 1
 * (ignored).  Any in-game failure logs, flags the game out of sync, and
 * returns 0.  On success returns the (non-zero) update result.
 * ---------------------------------------------------------------------- */
char FUN_00127a50(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[528];
  int packet_type;
  int packet_version;
  char result;
  const char *reason;

  if (!network_game_client_address_matches_server(client, source_address)) {
    network_game_log(
      "ignoring a message_server_game_update message; came from a bad machine");
    return 1;
  }
  if (network_game_client_get_state(client, (void *)0) == 3) {
    message_size -= 2;
    packet_type = 0x14;
    packet_version = 1;
    if (FUN_0012bce0((int)decoded, (int)message + 2, (short *)&message_size,
                     (short *)&packet_type, (short *)&packet_version, 4)) {
      result = network_game_client_handle_game_update(client, decoded);
      if (result != 0) {
        return result;
      }
      reason = "network_game_client_handle_game_update() failed";
    } else {
      reason = "failed to decode a message_server_game_update packet";
    }
  } else {
    reason =
      "failed to handle a message_server_game_update message; we are not in game";
  }
  network_game_log(reason);
  network_game_client_game_out_of_sync(client);
  return 0;
}

/* ------------------------------------------------------------------------
 * FUN_00127b10 — handle message_server_add_player_ingame (type 0x15).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and in-game state (== 3), decodes the packet
 * into a 32-byte scratch buffer, then adds the player.  Same return policy
 * as game_update: 1 for a bad machine, 0 for any in-game failure (which
 * also flags the game out of sync), otherwise the non-zero add result.
 * ---------------------------------------------------------------------- */
char FUN_00127b10(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[32];
  int packet_type;
  int packet_version;
  char result;
  const char *reason;

  if (!network_game_client_address_matches_server(client, source_address)) {
    network_game_log("ignoring a message_server_add_player_ingame message; came "
                     "from a bad machine");
    return 1;
  }
  if (network_game_client_get_state(client, (void *)0) == 3) {
    message_size -= 2;
    packet_type = 0x15;
    packet_version = 1;
    if (FUN_0012bce0((int)decoded, (int)message + 2, (short *)&message_size,
                     (short *)&packet_type, (short *)&packet_version, 4)) {
      result = network_game_client_add_player_to_game(client, decoded);
      if (result != 0) {
        return result;
      }
      reason = "network_game_client_add_player_to_game() failed";
    } else {
      reason = "failed to decode a message_server_add_player_ingame packet";
    }
  } else {
    reason = "failed to handle a message_server_add_player_ingame message; we "
             "are not in game";
  }
  network_game_log(reason);
  network_game_client_game_out_of_sync(client);
  return 0;
}

/* ------------------------------------------------------------------------
 * FUN_00127bd0 — handle message_server_remove_player_ingame (type 0x16).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and in-game state (== 3), decodes the packet
 * into a 36-byte scratch buffer, then removes the player.  The remove call
 * takes a third argument read from the decoded message at offset 0x20 (a
 * dword the decompiler drops).  Bad machine -> 1; any in-game failure logs,
 * flags the game out of sync, and returns 0; success returns the non-zero
 * remove result.
 * ---------------------------------------------------------------------- */
char FUN_00127bd0(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[36];
  int packet_type;
  int packet_version;
  char result;
  const char *reason;

  if (!network_game_client_address_matches_server(client, source_address)) {
    network_game_log("ignoring a message_server_remove_player_ingame message; "
                     "came from a bad machine");
    return 1;
  }
  if (network_game_client_get_state(client, (void *)0) == 3) {
    message_size -= 2;
    packet_type = 0x16;
    packet_version = 1;
    if (FUN_0012bce0((int)decoded, (int)message + 2, (short *)&message_size,
                     (short *)&packet_type, (short *)&packet_version, 4)) {
      result = network_game_client_remove_player(client, decoded,
                                                 *(int *)(decoded + 0x20));
      if (result != 0) {
        return result;
      }
      reason = "network_game_client_remove_player() failed";
    } else {
      reason = "failed to decode a message_server_remove_player_ingame packet";
    }
  } else {
    reason = "failed to handle a message_server_remove_player_ingame message; "
             "we are not in game";
  }
  network_game_log(reason);
  network_game_client_game_out_of_sync(client);
  return 0;
}

/* ------------------------------------------------------------------------
 * FUN_00127c90 — handle message_server_game_over (type 0x17).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and in-game state (== 3).  A decode failure is
 * non-critical (logged, then ignored); the client is switched to postgame
 * regardless.  Every path returns 1.
 * ---------------------------------------------------------------------- */
char FUN_00127c90(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;

  if (!network_game_client_address_matches_server(client, source_address)) {
    network_game_log(
      "ignoring a message_server_game_over message; came from a bad machine");
    return 1;
  }
  if (network_game_client_get_state(client, (void *)0) == 3) {
    message_size -= 2;
    packet_type = 0x17;
    packet_version = 1;
    if (!FUN_0012bce0((int)decoded, (int)message + 2, (short *)&message_size,
                      (short *)&packet_type, (short *)&packet_version, 4)) {
      network_game_log(
        "failed to decode a message_server_game_over message (not critical)");
    }
    network_client_switch_to_postgame(client);
    return 1;
  }
  network_game_log(
    "failed to handle a message_server_game_over message; we are not in game");
  return 1;
}

/* ------------------------------------------------------------------------
 * FUN_00127d30 — handle message_server_switch_to_pregame (type 0x1e).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and postgame state (== 4).  A decode failure is
 * logged but non-fatal; the client is switched to pregame regardless, and
 * the switch result is returned.  Bad machine -> 1; wrong state -> 0.
 * ---------------------------------------------------------------------- */
char FUN_00127d30(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;
  char result;

  if (!network_game_client_address_matches_server(client, source_address)) {
    network_game_log("ignoring a message_server_switch_to_pregame message; came "
                     "from a bad machine");
    return 1;
  }
  if (network_game_client_get_state(client, (void *)0) == 4) {
    message_size -= 2;
    packet_type = 0x1e;
    packet_version = 1;
    if (!FUN_0012bce0((int)decoded, (int)message + 2, (short *)&message_size,
                      (short *)&packet_type, (short *)&packet_version, 6)) {
      network_game_log(
        "failed to decode a message_server_switch_to_pregame packet");
    }
    result = network_game_client_switch_to_pregame(client);
    if (result == 0) {
      network_game_log("network_game_client_switch_to_pregame() failed");
    }
    return result;
  }
  network_game_log("failed to handle a message_server_switch_to_pregame "
                   "message; we are not in post-game");
  return 0;
}

/* ------------------------------------------------------------------------
 * FUN_00127df0 — handle message_server_graceful_game_exit_postgame (0x1f).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and postgame state (== 4).  A decode failure is
 * non-critical (logged); the client game is shut down regardless.  Bad
 * machine -> 1; all other paths -> 0 (the shutdown result is not captured).
 * ---------------------------------------------------------------------- */
char FUN_00127df0(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;

  if (!network_game_client_address_matches_server(client, source_address)) {
    network_game_log("ignoring a message_server_graceful_game_exit_postgame "
                     "message; came from a bad machine");
    return 1;
  }
  if (network_game_client_get_state(client, (void *)0) == 4) {
    message_size -= 2;
    packet_type = 0x1f;
    packet_version = 1;
    if (!FUN_0012bce0((int)decoded, (int)message + 2, (short *)&message_size,
                      (short *)&packet_type, (short *)&packet_version, 6)) {
      network_game_log("failed to decode a message_server_graceful_game_exit_"
                       "postgame packet (not critical)");
    }
    network_game_client_game_shutdown(client);
    return 0;
  }
  network_game_log("failed to handle a message_server_graceful_game_exit_"
                   "postgame message; we are not in post-game");
  return 0;
}
