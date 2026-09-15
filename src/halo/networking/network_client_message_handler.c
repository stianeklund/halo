#include "../../common.h"

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_postgame_keep_alive — handle message_server_postgame_keep_alive (type 0xb).
 * (client @esi, source_address @edi, message, message_size) -> char.
 * Requires the source to match the server and the client to be in postgame
 * (state == 4), then decodes the keep-alive packet.  Every path returns 1.
 * Confirmed: @esi/@edi register args + NULL asserts, state==4, type 0xb,
 *   version 1, decode flags 6.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_postgame_keep_alive(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_version;
  int packet_type;

  assert_halt_msg(client != (void *)0, "client != NULL");
  assert_halt_msg(source_address != (void *)0, "source_address != NULL");

  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 4) {
      message_size -= 2;
      packet_type = 0xb;
      packet_version = 1;
      if (!decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                        (short *)&packet_type, (short *)&packet_version, 6)) {
        network_event(
          "failed to decode a message_server_postgame_keep_alive packet");
      }
    } else {
      network_event("failed to handle a message_server_postgame_keep_alive "
                       "message; not in postgame state");
    }
  } else {
    network_event(
      "ignoring a message_server_postgame_keep_alive; came from a bad machine");
  }
  return 1;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_begin_game — handle message_server_begin_game (type 8).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and pregame state (== 2), decodes the packet,
 * then starts the client game.  Returns 1 for a bad machine; 0 for wrong
 * state or decode failure; otherwise the result of game_has_started().
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_begin_game(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;
  char result;

  result = 0;
  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 2) {
      message_size -= 2;
      packet_type = 8;
      packet_version = 1;
      if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                       (short *)&packet_type, (short *)&packet_version, 2)) {
        result = network_game_client_game_has_started(client);
        if (!result) {
          network_event("network_game_client_game_has_started() failed");
        }
      } else {
        network_event("failed to decode a message_server_begin_game packet");
      }
    } else {
      network_event("failed to handle a message_server_begin_game message; we "
                       "are not in pregame");
    }
  } else {
    network_event(
      "ignoring a message_server_begin_game message; came from a bad machine");
    return 1;
  }
  return result;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_graceful_game_exit_pregame — handle message_server_graceful_game_exit_pregame (type 9).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and pregame state (== 2), decodes the packet,
 * then shuts the client game down.  Every path returns 1.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_graceful_game_exit_pregame(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;

  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 2) {
      message_size -= 2;
      packet_type = 9;
      packet_version = 1;
      if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                       (short *)&packet_type, (short *)&packet_version, 2)) {
        network_game_client_game_shutdown(client);
      } else {
        network_event(
          "failed to decode a message_server_graceful_game_exit_pregame packet");
      }
    } else {
      network_event(
        "failed to handle a message_server_graceful_game_exit_pregame"
        " message; we are not in pregame");
    }
  } else {
    network_event("ignoring a message_server_graceful_game_exit_pregame "
                     "message; came from a bad machine");
  }
  return 1;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_game_update — handle message_server_game_update (type 0x14).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and in-game state (== 3), decodes the update
 * into a 528-byte scratch buffer, then applies it.  A bad machine returns 1
 * (ignored).  Any in-game failure logs, flags the game out of sync, and
 * returns 0.  On success returns the (non-zero) update result.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_game_update(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[528];
  int packet_type;
  int packet_version;
  char result;

  result = 0;
  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 3) {
      message_size -= 2;
      packet_type = 0x14;
      packet_version = 1;
      if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                        (short *)&packet_type, (short *)&packet_version, 4)) {
        result = network_game_client_handle_game_update(client, decoded);
        if (result == 0) {
          network_event("network_game_client_handle_game_update() failed");
          network_game_client_game_out_of_sync(client);
        }
      } else {
        network_event("failed to decode a message_server_game_update packet");
        network_game_client_game_out_of_sync(client);
      }
    } else {
      network_event("failed to handle a message_server_game_update message; we are "
                       "not in game");
      network_game_client_game_out_of_sync(client);
    }
  } else {
    network_event(
      "ignoring a message_server_game_update message; came from a bad machine");
    return 1;
  }
  return result;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_add_player_ingame — handle message_server_add_player_ingame (type 0x15).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and in-game state (== 3), decodes the packet
 * into a 32-byte scratch buffer, then adds the player.  Same return policy
 * as game_update: 1 for a bad machine, 0 for any in-game failure (which
 * also flags the game out of sync), otherwise the non-zero add result.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_add_player_ingame(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[32];
  int packet_type;
  int packet_version;
  char result;

  result = 0;
  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 3) {
      message_size -= 2;
      packet_type = 0x15;
      packet_version = 1;
      if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                       (short *)&packet_type, (short *)&packet_version, 4)) {
        result = network_game_client_add_player_to_game(client, decoded);
        if (result == 0) {
          network_event("network_game_client_add_player_to_game() failed");
          network_game_client_game_out_of_sync(client);
        }
      } else {
        network_event("failed to decode a message_server_add_player_ingame packet");
        network_game_client_game_out_of_sync(client);
      }
    } else {
      network_event("failed to handle a message_server_add_player_ingame message; we "
                       "are not in game");
      network_game_client_game_out_of_sync(client);
    }
  } else {
    network_event(
      "ignoring a message_server_add_player_ingame message; came "
      "from a bad machine");
    return 1;
  }
  return result;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_remove_player_ingame — handle message_server_remove_player_ingame (type 0x16).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and in-game state (== 3), decodes the packet
 * into a 36-byte scratch buffer, then removes the player.  The remove call
 * takes a third argument read from the decoded message at offset 0x20 (a
 * dword the decompiler drops).  Bad machine -> 1; any in-game failure logs,
 * flags the game out of sync, and returns 0; success returns the non-zero
 * remove result.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_remove_player_ingame(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[36];
  int packet_type;
  int packet_version;
  char result;

  result = 0;
  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 3) {
      message_size -= 2;
      packet_type = 0x16;
      packet_version = 1;
      if (decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                       (short *)&packet_type, (short *)&packet_version, 4)) {
        result = network_game_client_remove_player(client, decoded,
                                                   *(int *)(decoded + 0x20));
        if (result == 0) {
          network_event("network_game_client_remove_player() failed");
          network_game_client_game_out_of_sync(client);
        }
      } else {
        network_event("failed to decode a message_server_remove_player_ingame packet");
        network_game_client_game_out_of_sync(client);
      }
    } else {
      network_event("failed to handle a message_server_remove_player_ingame message; "
                       "we are not in game");
      network_game_client_game_out_of_sync(client);
    }
  } else {
    network_event("ignoring a message_server_remove_player_ingame message; "
                     "came from a bad machine");
    return 1;
  }
  return result;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_game_over — handle message_server_game_over (type 0x17).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and in-game state (== 3).  A decode failure is
 * non-critical (logged, then ignored); the client is switched to postgame
 * regardless.  Every path returns 1.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_game_over(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;

  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 3) {
      message_size -= 2;
      packet_type = 0x17;
      packet_version = 1;
      if (!decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                        (short *)&packet_type, (short *)&packet_version, 4)) {
        network_event(
          "failed to decode a message_server_game_over message (not critical)");
      }
      network_game_client_switch_to_postgame(client);
    } else {
      network_event(
        "failed to handle a message_server_game_over message; we are not in game");
    }
  } else {
    network_event(
      "ignoring a message_server_game_over message; came from a bad machine");
  }
  return 1;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_switch_to_pregame — handle message_server_switch_to_pregame (type 0x1e).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and postgame state (== 4).  A decode failure is
 * logged but non-fatal; the client is switched to pregame regardless, and
 * the switch result is returned.  Bad machine -> 1; wrong state -> 0.
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_switch_to_pregame(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;
  char result;

  result = 0;
  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 4) {
      message_size -= 2;
      packet_type = 0x1e;
      packet_version = 1;
      if (!decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                        (short *)&packet_type, (short *)&packet_version, 6)) {
        network_event(
          "failed to decode a message_server_switch_to_pregame packet");
      }
      result = network_game_client_switch_to_pregame(client);
      if (result == 0) {
        network_event("network_game_client_switch_to_pregame() failed");
      }
    } else {
      network_event("failed to handle a message_server_switch_to_pregame "
                       "message; we are not in post-game");
    }
  } else {
    network_event(
      "ignoring a message_server_switch_to_pregame message; came "
      "from a bad machine");
    return 1;
  }
  return result;
}

/* ------------------------------------------------------------------------
 * network_game_client_handle_message_server_graceful_game_exit_postgame — handle message_server_graceful_game_exit_postgame (0x1f).
 * (client @esi, source_address @eax, message, message_size) -> char.
 * Requires a matching source and postgame state (== 4).  A decode failure is
 * non-critical (logged); the client game is shut down regardless.  Bad
 * machine -> 1; all other paths -> 0 (the shutdown result is not captured).
 * ---------------------------------------------------------------------- */
char network_game_client_handle_message_server_graceful_game_exit_postgame(void *client, void *source_address, void *message,
                  int message_size)
{
  char decoded[4];
  int packet_type;
  int packet_version;
  char result;

  result = 0;
  if (network_game_client_address_matches_server(client, source_address)) {
    if (network_game_client_get_state(client, (void *)0) == 4) {
      message_size -= 2;
      packet_type = 0x1f;
      packet_version = 1;
      if (!decode_network_game_message((int)decoded, (int)message + 2, (short *)&message_size,
                        (short *)&packet_type, (short *)&packet_version, 6)) {
        network_event("failed to decode a message_server_graceful_game_exit_"
                         "postgame packet (not critical)");
      }
      network_game_client_game_shutdown(client);
    } else {
      network_event("failed to handle a message_server_graceful_game_exit_"
                       "postgame message; we are not in post-game");
    }
  } else {
    network_event("ignoring a message_server_graceful_game_exit_postgame "
                     "message; came from a bad machine");
    result = 1;
  }
  return result;
}

/* ========================================================================
 * network_client_message_handler.c
 * Original source: c:\halo\SOURCE\networking\network_client_message_handler.c
 *
 * Client-side handling of server->client network-game messages.  The
 * dispatcher (network_game_client_handle_message) validates the framed message, extracts the
 * message-type byte, and forwards to a per-type handler.  Each handler
 * validates the source machine and the client game-state, decodes the
 * packet with decode_network_game_message() (decode_network_game_message), then applies
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
 * (network_game_client_handle_message_server_game_advertise game_advertise .. network_game_client_handle_message_server_pregame_keep_alive pregame_keep_alive) are
 * declared here via decl.h but implemented in that TU.
 * ======================================================================== */

/* ------------------------------------------------------------------------
 * network_game_client_handle_message — network_game_client_handle_message (dispatcher).
 * cdecl(client, message, message_size, source_address) -> bool.
 * Validates the framed message header, rejects bad flags / message
 * categories, then dispatches on the trailing message-type byte to the
 * matching per-type handler.  Returns the handler's result (1 on
 * success/ignored, 0 on handler failure).
 * Confirmed: arg layout, header math (>>4 size, &3 flags, >>2&3 category),
 *   type byte = message[(short)message_size-1], case->handler map, register
 *   setup for each handler (client=ESI, source_address=EAX/EDI/stack).
 * ---------------------------------------------------------------------- */
bool network_game_client_handle_message(void *client, void *message, int message_size,
                  void *source_address)
{
  char result;
  unsigned short header;
  unsigned short category;
  int type_byte;
  char *error_str;

  result = 1;

  assert_halt_msg(
    client != (void *)0 && message != (void *)0 &&
      (short)message_size == (*(unsigned short *)message >> 4) &&
      source_address != (void *)0,
    "client && message && (message_size == GET_MESSAGE_SIZE(*message)) && "
    "source_address");

  header = *(unsigned short *)message;
  category = (unsigned short)(((unsigned char)header >> 2) & 3);

  if ((header & 3) != 0) {
    network_event("client received client message with invalid flags");
    return 1;
  }

  switch (category) {
  case 1:
    if ((unsigned short)message_size >= 0x83) {
      error_str = (char *)message + 2;
      network_event(
        "client received low-level error message: error= #%d (%s)",
        (unsigned char)error_str[0x80],
        error_str);
    } else {
      network_event(
        "client received a malformed/damaged message from a server");
    }
    break;

  case 2:
    network_event("client received a bad message type (_message_type_data)");
    break;

  case 3:
    type_byte = (int)((unsigned char *)message)[(short)message_size - 1];
    switch (type_byte) {
    case 2:
      result = network_game_client_handle_message_server_game_advertise(client, message, message_size, source_address);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_game_advertise() failed");
      }
      break;
    case 3:
      result = network_game_client_handle_message_server_pong(client, message, message_size, source_address);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_pong() failed");
      }
      break;
    case 4:
      result = network_game_client_handle_message_server_machine_accepted(client, source_address, message, message_size);
      if (result == 0) {
        network_event("network_game_client_handle_message_server_machine_"
                         "accepted() failed");
      }
      break;
    case 5:
      result = network_game_client_handle_message_server_machine_rejected(client, source_address, message, message_size);
      if (result == 0) {
        network_event("network_game_client_handle_message_server_machine_"
                         "rejected() failed");
      }
      break;
    case 6:
      result = network_game_client_handle_message_server_game_settings_update(client, source_address, message, message_size);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_game_settings_update() "
          "failed");
      }
      break;
    case 7:
      result = network_game_client_handle_message_server_pregame_countdown(client, source_address, message, message_size);
      if (result == 0) {
        network_event("network_game_client_handle_message_server_pregame_"
                         "countdown() failed");
      }
      break;
    case 8:
      result = network_game_client_handle_message_server_begin_game(client, source_address, message, message_size);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_begin_game() failed");
      }
      break;
    case 9:
      result = network_game_client_handle_message_server_graceful_game_exit_pregame(client, source_address, message, message_size);
      if (result == 0) {
        network_event("network_game_client_handle_message_server_graceful_"
                         "game_exit_pregame() failed");
      }
      break;
    case 10:
      result = network_game_client_handle_message_server_pregame_keep_alive(client, source_address, message, message_size);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_pregame_keep_alive() "
          "failed");
      }
      break;
    case 0xb:
      result = network_game_client_handle_message_server_postgame_keep_alive(client, source_address, message, message_size);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_postgame_keep_alive() "
          "failed");
      }
      break;
    case 0x14:
      result = network_game_client_handle_message_server_game_update(client, source_address, message, message_size);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_game_update() failed");
      }
      break;
    case 0x15:
      result = network_game_client_handle_message_server_add_player_ingame(client, source_address, message, message_size);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_add_player_ingame() "
          "failed");
      }
      break;
    case 0x16:
      result = network_game_client_handle_message_server_remove_player_ingame(client, source_address, message, message_size);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_remove_player_ingame() "
          "failed");
      }
      break;
    case 0x17:
      result = network_game_client_handle_message_server_game_over(client, source_address, message, message_size);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_game_over() failed");
      }
      break;
    case 0x1e:
      result = network_game_client_handle_message_server_switch_to_pregame(client, source_address, message, message_size);
      if (result == 0) {
        network_event(
          "network_game_client_handle_message_server_switch_to_pregame() "
          "failed");
      }
      break;
    case 0x1f:
      result = network_game_client_handle_message_server_graceful_game_exit_postgame(client, source_address, message, message_size);
      if (result == 0) {
        network_event("network_game_client_handle_message_server_graceful_"
                         "game_exit_postgame() failed");
      }
      break;
    default:
      network_event("unknown packet type received from system @ address: %s",
                       transport_address_to_string(source_address));
      break;
    }
    break;

  default:
    network_event("client received a message with an unknown message type");
    break;
  }

  return result;
}
