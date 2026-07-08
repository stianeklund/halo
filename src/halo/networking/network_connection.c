/* Halo CE Xbox — network connection helpers.
 * c:\halo\SOURCE\networking\network_connection.c
 *
 * A network_connection is the per-machine transport endpoint used by the
 * server/client managers.  connection+0x30 is a byte flags field; bit 0
 * (mask 1) is _connection_create_server_bit, set on connections that were
 * created as the game server.  connection+0x4c is the "allow client
 * connections" bool, gating whether the server accepts new client joins.
 */

#include "network_connection.h"

/* network_server_allow_client_connections (0x1282f0).
 * Sets the server connection's accept-clients flag.  Asserts the connection
 * exists and is a server-role connection, then stores the requested state. */
void network_server_allow_client_connections(int server_connection, bool open)
{
  assert_halt(server_connection);
  assert_halt_msg(
    *(uint8_t *)(server_connection + 0x30) & FLAG(_connection_create_server_bit),
    "server_connection->flags&FLAG(_connection_create_server_bit)");
  *(bool *)(server_connection + 0x4c) = open;
}

/* network_connection_connected (0x128360).
 * Predicate: is this connection's transport endpoint up?  Asserts the
 * connection exists, then requires (a) at least one of the two "connected"
 * state bits (mask 6 = bits 1|2) set in the flags byte at +0x30, and
 * (b) a non-null endpoint handle at +0x00.  When both hold, defers to the
 * endpoint-connected predicate FUN_000831a0 for the final answer. */
bool network_connection_connected(int connection)
{
  assert_halt(connection);
  if ((*(uint8_t *)(connection + 0x30) &
       (FLAG(_connection_create_clientside_client_bit) |
        FLAG(_connection_create_serverside_client_bit))) != 0 && *(int *)connection != 0) {
    if (FUN_000831a0(*(int *)connection)) {
      return true;
    }
  }
  return false;
}

/* network_connection_get_address (0x1283c0).
 * Refreshes up to two per-endpoint network-address out-buffers from the
 * connection.  connection+0x00 and connection+0x04 are the two transport
 * endpoint handles.  For each supplied out-buffer (buf, flag): if the matching
 * endpoint is null, or FUN_00083a60(endpoint, buffer) reports a mismatch
 * (non-zero), the 0x18-byte buffer is reset to zero and its address-type field
 * at +0x10 is set to 4.  Asserts the connection exists.  Current callers pass
 * flag==0, so only the first buffer is refreshed. */
void network_connection_get_address(int connection, void *buf, int flag)
{
  int *conn;

  assert_halt(connection);
  conn = (int *)connection;
  if (buf != (void *)0 &&
      (conn[0] == 0 || FUN_00083a60((int *)conn[0], buf) != 0)) {
    csmemset(buf, 0, 0x18);
    *(short *)((char *)buf + 0x10) = 4;
  }
  if (flag != 0) {
    if (conn[1] != 0 && FUN_00083a60((int *)conn[1], (void *)flag) == 0) {
      return;
    }
    csmemset((void *)flag, 0, 0x18);
    *(short *)(flag + 0x10) = 4;
  }
}

/* network_connection_set_connection_rejection_procedure (0x128580).
 * Trivial setter: asserts the connection exists, then stores the rejection
 * callback pointer at connection+0xc. */
void network_connection_set_connection_rejection_procedure(int connection,
                                                           void *callback)
{
  assert_halt(connection);
  *(void **)(connection + 0xc) = callback;
}

/* network_connection_server_accept_client_connection (0x1285c0).
 * Registers a newly-accepted client connection's transport endpoint into the
 * server connection's endpoint set.  Asserts the server connection exists, is
 * a server-role connection (flags bit 0 = _connection_create_server_bit), and
 * that the client connection pointer is non-null.  Forwards the client's
 * endpoint handle (*client_connection) and the server's endpoint-set handle
 * (server_connection+0x38) to add_endpoint_to_set, then returns true when the
 * add reported success (a zero result; the compare is 16-bit, matching the
 * original NEG AX/SBB/INC idiom). */
bool network_connection_server_accept_client_connection(int server_connection,
                                                        int client_connection)
{
  assert_halt(server_connection);
  assert_halt_msg(
    *(uint8_t *)(server_connection + 0x30) & FLAG(_connection_create_server_bit),
    "server_connection->flags&FLAG(_connection_create_server_bit)");
  assert_halt(client_connection);
  return (short)add_endpoint_to_set(*(int *)client_connection,
                                    *(void **)(server_connection + 0x38)) == 0;
}

/* network_connection_active (0x128660).
 * Predicate: asserts the connection exists, then reports whether the
 * "closed/inactive" bit (bit 4, mask 0x10) of the flags dword at +0x30 is
 * CLEAR.  active == !((flags >> 4) & 1); i.e. the connection is active while
 * bit 4 has not been set. */
bool network_connection_active(int connection)
{
  assert_halt(connection);
  return ((*(unsigned int *)(connection + 0x30) >> _connection_closed_bit) & 1) == 0;
}

/* network_connection_going_stale (0x1286a0).
 * Predicate: asserts the connection exists, then reports whether the
 * "going stale" bit (bit 5, mask 0x20) of the flags dword at +0x30 is SET.
 * A connection is flagged going-stale once bit 5 has been raised (e.g. when
 * traffic has lapsed and the transport is about to be torn down). */
bool network_connection_going_stale(int connection)
{
  assert_halt(connection);
  return (*(unsigned int *)(connection + 0x30) >> _connection_going_stale_bit) & 1;
}

/* network_connection_keep_alive (0x128d20).
 * Stamps the connection's last-keep-alive timestamp: samples the current
 * millisecond clock (system_milliseconds) and stores it into the dword at
 * connection+0x8, recording the moment the most recent keep-alive was
 * sent/observed so the going-stale logic can measure elapsed idle time. */
void network_connection_keep_alive(int connection)
{
  *(unsigned int *)(connection + 8) = system_milliseconds();
}

/* network_connection_delete (0x128d30).
 * Tears a connection and all of its child connections down.  Returns
 * immediately for a null connection.  Otherwise it fires the "connection
 * closing" traffic event (event 1, enable 1) to flush and close the
 * connection's traffic log, destroys the two transport endpoint handles
 * (+0x00, +0x04), deletes the two circular queues (+0x10, +0x14), and — only
 * for server-role connections (flags bit 0 at +0x30) — walks the four
 * child-connection slots at +0x3c: for each live child it removes the child's
 * endpoint from the server's endpoint set (+0x38) and recursively deletes the
 * child, then deletes the endpoint set itself.  Finally the connection block
 * is freed.  network_connection_notify_traffic_event takes its arguments in registers (event=ECX,
 * enable=EAX, connection=ESI), matching the original MOV EAX,1 / MOV ECX,EAX
 * setup with the connection already live in ESI. */
void network_connection_delete(int connection)
{
  int *children;
  int i;
  int child;

  if (connection == 0) {
    return;
  }
  network_connection_notify_traffic_event(1, 1, connection);
  if (*(int *)connection != 0) {
    destroy_endpoint(*(int **)connection);
  }
  if (*(int *)(connection + 4) != 0) {
    destroy_endpoint(*(int **)(connection + 4));
  }
  if (*(int *)(connection + 0x10) != 0) {
    circular_queue_delete(*(int *)(connection + 0x10));
  }
  if (*(int *)(connection + 0x14) != 0) {
    circular_queue_delete(*(int *)(connection + 0x14));
  }
  if ((*(uint8_t *)(connection + 0x30) & FLAG(_connection_create_server_bit)) != 0) {
    children = (int *)(connection + 0x3c);
    if (children != (int *)0) {
      i = 4;
      do {
        child = *children;
        if (child != 0) {
          if (*(int *)(connection + 0x38) != 0) {
            remove_endpoint_from_set(*(int **)child,
                                     *(uint32_t **)(connection + 0x38));
          }
          network_connection_delete(child);
        }
        children = children + 1;
        i = i - 1;
      } while (i != 0);
    }
    if (*(int *)(connection + 0x38) != 0) {
      delete_endpoint_set(*(int *)(connection + 0x38));
    }
  }
  debug_free((void *)connection,
             "c:\\halo\\SOURCE\\networking\\network_connection.c", 0x145);
}

/* network_connection_write (0x128e00).
 * Serializes and transmits a message over a connection.  Validates the message,
 * byte-swaps its header to network order, then dispatches on the connection's
 * flags byte (+0x30): bit 0 selects the loopback/datagram path, otherwise the
 * normal transport-endpoint path (which further splits on the reliable flag).
 *
 *  - Loopback (flags&1): must be unreliable, carry a destination address, and
 *    fit a datagram (size<=400).  Hands the message to the addressed datagram
 *    writer FUN_00084740(endpoint(+0x04), message, size, dest) and fires the
 *    traffic-accounting event.  The writer's result is stored but the call
 *    always reports success (reliable is 0 here).
 *  - Normal reliable (flags&1==0, reliable!=0): size<=0x800 and the connection
 *    must be client- or server-side (flags&6).  Repeatedly calls
 *    send_endpoint(connection->[0x00], message, size), retrying while the
 *    endpoint returns -4 (would-block).  On success sets the success flag,
 *    bumps the sent counter (+0x28) and — only when a debug log FILE* (+0x18)
 *    is open — appends a timing record; on a hard error reports the endpoint
 *    error string.  Returns whether the send succeeded.
 *  - Normal unreliable (flags&1==0, reliable==0): no-op when the endpoint
 *    (+0x04) is null; else size<=400.  With no destination it sends to the
 *    bound peer via send_endpoint, gated by the endpoint-connected predicate
 *    FUN_000831a0; with a destination it uses the addressed writer
 *    FUN_00084740 (result discarded).  Always returns true.
 *
 * network_connection_notify_traffic_event (traffic-event notifier) takes its args in registers
 * (event=ECX, enable=EAX, connection=ESI); every call site passes event=2,
 * enable=size, connection=connection.  The +0x18 debug-log block is dead in
 * shipping builds (no log file is ever opened) but is preserved faithfully:
 * _DAT_00265d40 (2^32) folds the signed tick delta back to unsigned before
 * scaling by _DAT_00294bf0, and the record is emitted with fwprintf/fflush. */
bool network_connection_write(void *connection, void *message,
                              unsigned short size, int dest_address,
                              bool reliable)
{
  int conn;
  int result;
  int send_result;
  const char *err;

  conn = (int)connection;
  result = 0;

  assert_halt(message);
  assert_halt(size);
  assert_halt(connection);
  assert_halt_msg((*(unsigned short *)message >> 4) == size,
                  "(message->message_size >> 4) == buffer_size");

  byte_swap_message_header((unsigned short *)message, 1);

  if ((*(uint8_t *)(conn + 0x30) & FLAG(_connection_create_server_bit)) != 0) {
    /* loopback / datagram path */
    assert_halt_msg(reliable == 0, "!reliable");
    assert_halt_msg(dest_address != 0, "dest_address");
    if (size > DATAGRAM_MAXIMUM_SIZE) {
      error(2, "buffer size was %d max is %d", size, DATAGRAM_MAXIMUM_SIZE);
      assert_halt_msg(size <= DATAGRAM_MAXIMUM_SIZE,
                      "buffer_size <= DATAGRAM_MAXIMUM_SIZE");
    }
    result = FUN_00084740(*(int *)(conn + 4), message, size, dest_address);
    network_connection_notify_traffic_event(2, size, conn);
    goto finish;
  }

  if (reliable != 0) {
    /* normal reliable path */
    assert_halt_msg(size <= 0x800, "message size exceeds maximum allowed size");
    assert_halt_msg((*(uint8_t *)(conn + 0x30) &
                   (FLAG(_connection_create_clientside_client_bit) |
                    FLAG(_connection_create_serverside_client_bit))) != 0,
                    "(flags & clientside) || (flags & serverside)");
    do {
      send_result = send_endpoint(*(int **)conn, (const char *)message, size);
    } while (send_result <= 0 && send_result == -4);
    if (send_result > 0) {
      result = 1;
      if (*(int *)(conn + 0x18) != 0) {
        int ticks;
        int delta;
        double elapsed;

        ticks = FUN_001d0581();
        delta = ticks - *(int *)(conn + 0x1c);
        elapsed = (double)delta;
        if (delta < 0) {
          elapsed += *(double *)0x265d40;
        }
        elapsed = elapsed * *(double *)0x294bf0;
        crt_fprintf(*(void **)(conn + 0x18),
                    (const char *)L"%g\t%ld\t%ld\t%ld\t%ld\n", elapsed, 0, 0,
                    send_result, 0);
        crt_fflush(*(void **)(conn + 0x18));
      }
      *(int *)(conn + 0x28) += 1;
    } else {
      err = FUN_00081c80(send_result);
      error(2, "client call to write_endpoint() returned error '%s'", err);
    }
    goto finish;
  }

  /* normal unreliable path */
  if (*(int *)(conn + 4) == 0) {
    return true;
  }
  assert_halt_msg(size <= DATAGRAM_MAXIMUM_SIZE,
                  "message size exceeds maximum allowed size");
  if (dest_address == 0) {
    if (FUN_000831a0(*(int *)(conn + 4))) {
      send_endpoint(*(int **)(conn + 4), (const char *)message, size);
      network_connection_notify_traffic_event(2, size, conn);
    }
    return true;
  }
  FUN_00084740(*(int *)(conn + 4), message, size, dest_address);
  network_connection_notify_traffic_event(2, size, conn);
  return true;

finish:
  if (reliable == 0) {
    return true;
  }
  return result > 0;
}

/* network_connection_new (0x1296b0).
 * Allocates and initializes a transport connection.  The caller must request
 * either the server role (flags bit 0) or the clientside-client role (flags
 * bit 1); anything else asserts.  Server connections get the larger 0x50-byte
 * block (with the child-connection endpoint set at +0x38 and the accept-clients
 * flag at +0x4c), an unreliable incoming queue of 0x1900 and no reliable queue;
 * clientside clients get the 0x38-byte block, a reliable queue of 0x8000 and an
 * unreliable queue of 0x640.  Both create two transport endpoints (types 0x12
 * and 0x11) via get_next_endpoint_from_set, stamp the creation time (+0x08),
 * flags (+0x30) and well-known port (+0x34), bind/prepare each endpoint through
 * FUN_00083ce0/FUN_00083bd0 (and, for servers, FUN_000843a0 + add the primary
 * endpoint to the set), then allocate the incoming circular queues at +0x10
 * (reliable) and +0x14 (unreliable).  Any failure tears the partial connection
 * down via network_connection_delete and returns 0.  On success it fires the
 * connection-created traffic event (event 0, enable 1) and returns the block.
 * network_connection_notify_traffic_event takes its args in registers (event=ECX, enable=EAX,
 * connection=ESI), matching the original MOV EAX,1 / XOR ECX,ECX setup with the
 * connection live in ESI.  The reliable/unreliable guards compare the full
 * dword size against zero (original: MOV EAX,[size]; CMP EAX,EBX; JZ) — not a
 * byte test — so a 0x8000 reliable size still allocates the client queue.
 * The clientside path deliberately leaves the address scratch fields other than
 * family/port uninitialized (the server-only setup block that zeroes them is
 * skipped), matching the original. */
int network_connection_new(unsigned int flags, unsigned short well_known_port)
{
  int connection;
  int endpoint;
  short status;
  int reliable_size;
  int unreliable_size;
  int address[6];

  assert_halt_msg((flags & FLAG(_connection_create_server_bit)) != 0 ||
                    (flags & FLAG(_connection_create_clientside_client_bit)) != 0,
                  "(flags&FLAG(_connection_create_server_bit))|| "
                  "(flags&FLAG(_connection_create_clientside_client_bit))");

  if ((flags & FLAG(_connection_create_server_bit)) == 0) {
    if ((flags & FLAG(_connection_create_clientside_client_bit)) == 0) {
      return 0;
    }
    connection = (int)debug_malloc(
      0x38, 1, "c:\\halo\\SOURCE\\networking\\network_connection.c", 0xb6);
    if (connection == 0) {
      return 0;
    }
    reliable_size = 0x8000;
    unreliable_size = 0x640;
  } else {
    assert_halt_msg(well_known_port > MAXIMUM_RESERVED_NETWORK_PORT,
                    "well_known_port > MAXIMUM_RESERVED_NETWORK_PORT");
    connection = (int)debug_malloc(
      0x50, 1, "c:\\halo\\SOURCE\\networking\\network_connection.c", 0xa5);
    if (connection == 0) {
      return 0;
    }
    *(uint8_t *)(connection + 0x4c) = 1;
    endpoint = create_endpoint_set(5);
    *(int *)(connection + 0x38) = endpoint;
    if (endpoint == 0) {
      network_connection_delete(connection);
      return 0;
    }
    reliable_size = 0;
    unreliable_size = 0x1900;
  }

  *(int *)(connection + 8) = (int)system_milliseconds();
  *(int *)(connection + 0x30) = (int)flags;
  endpoint = get_next_endpoint_from_set(0x12);
  *(int *)connection = endpoint;
  if (endpoint == 0) {
    goto fail;
  }

  if ((flags & FLAG(_connection_create_server_bit)) != 0) {
    address[1] = 0;
    address[2] = 0;
    address[3] = 0;
    address[0] = 0;
    address[5] = 0;
    *(short *)((char *)&address[4]) = 4;
    *(unsigned short *)((char *)&address[4] + 2) = well_known_port;
    status = FUN_00083ce0((int *)endpoint, address);
    if (status != 0 || (status = FUN_00083bd0(*(int *)connection, 0)) != 0 ||
        (status = FUN_000843a0(*(int *)connection)) != 0 ||
        (status = (short)add_endpoint_to_set(
           *(int *)connection, (void *)*(int *)(connection + 0x38))) != 0) {
      goto fail;
    }
  }

  endpoint = get_next_endpoint_from_set(0x11);
  *(int *)(connection + 4) = endpoint;
  if (endpoint == 0) {
    goto fail;
  }
  address[0] = 0;
  *(short *)((char *)&address[4]) = 4;
  *(unsigned short *)((char *)&address[4] + 2) = well_known_port;
  *(unsigned short *)(connection + 0x34) = well_known_port;
  status = FUN_00083ce0((int *)endpoint, address);
  if (status != 0) {
    goto fail;
  }
  status = FUN_00083bd0(*(int *)(connection + 4), 0);
  if (status != 0) {
    goto fail;
  }
  if (reliable_size != 0) {
    *(int *)(connection + 0x10) =
      (int)circular_queue_new((int)"incoming-reliable", reliable_size);
    if (*(int *)(connection + 0x10) == 0) {
      goto fail;
    }
  }
  if (unreliable_size != 0) {
    *(int *)(connection + 0x14) =
      (int)circular_queue_new((int)"incoming-unreliable", unreliable_size);
    if (*(int *)(connection + 0x14) == 0) {
      goto fail;
    }
  }
  network_connection_notify_traffic_event(0, 1, connection);
  return connection;

fail:
  network_connection_delete(connection);
  return 0;
}

/* network_connection_connect (0x128460).
 * Initiates a transport connect on a connection's endpoints toward a remote
 * address.  Asserts the connection and remote_address are non-null.  Returns
 * false immediately if the connection has neither a reliable (+0x00) nor an
 * unreliable (+0x04) endpoint.  When an unreliable endpoint is present it is
 * connected synchronously via connect_endpoint (FUN_00083e20); a non-zero
 * status logs an error and returns false.  When a reliable endpoint is present
 * it is connected either asynchronously (async_process_ref != 0) via
 * connect_endpoint_async (FUN_000841b0) — tolerating the "in progress" status
 * -0x17 (0xffe9) as success — or synchronously via connect_endpoint.  Both
 * connect helpers return their status in AX (16-bit).  Returns true on success.
 * (The kb.json placeholder decl was void(void); the binary passes three cdecl
 * args (connection, remote_address, async_process_ref) and returns a bool.) */
bool network_connection_connect(int connection, int remote_address,
                                int async_process_ref)
{
  int reliable;
  short status;
  const char *err;

  assert_halt(connection);
  assert_halt(remote_address);

  if (*(int *)connection == 0 && *(int *)(connection + 4) == 0) {
    return false;
  }

  if (*(int *)(connection + 4) != 0) {
    status = FUN_00083e20(*(int *)(connection + 4), remote_address);
    if (status != 0) {
      err = FUN_00081c80(status);
      error(2, "connect_endpoint() on unreliable endpoint returned error '%s'",
            err);
      return false;
    }
  }

  reliable = *(int *)connection;
  if (reliable != 0) {
    if (async_process_ref != 0) {
      status = FUN_000841b0(reliable, remote_address, async_process_ref);
      if (status != 0 && status != -0x17) {
        err = FUN_00081c80(status);
        error(2, "connect_endpoint_async() returned error '%s'", err);
        return false;
      }
      return true;
    }
    status = FUN_00083e20(reliable, remote_address);
    if (status != 0) {
      err = FUN_00081c80(status);
      error(2, "connect_endpoint() on reliable endpoint returned error '%s'",
            err);
      return false;
    }
  }
  return true;
}

/* network_connection_read_unreliable (0x1286e0).
 * Dequeues one datagram message from the connection's unreliable incoming
 * queue (+0x14).  connection arrives in ESI.  Asserts the connection has an
 * unreliable queue and is not a server-side client, and that message/buffer
 * fields are valid (*size > sizeof(message_header)).  Peeks the 2-byte header,
 * byte-swaps it to host order, and derives the payload size (header >> 4).
 * Rejects datagrams larger than 400 bytes or larger than the caller buffer
 * (resetting the queue).  When the whole datagram plus its 4-byte trailing
 * source address is present it reads the payload into buffer, reads the source
 * address, stamps the header word into buffer, asserts encryption is inactive,
 * fills the optional out address (addr+0x10 family=4, addr+0x12=0), writes the
 * payload size back through *size, and returns true.  A partial datagram logs
 * an error, resets the queue, and returns false. */
bool network_connection_read_unreliable(int connection, void *buffer, int *size, void *addr)
{
  unsigned short header;
  int source_addr;
  unsigned short *buf_size;
  unsigned short packet_size;
  unsigned int available;

  buf_size = (unsigned short *)size;

  assert_halt_msg(
    connection != 0 && *(int *)(connection + 0x14) != 0 &&
      (*(uint8_t *)(connection + 0x30) &
       FLAG(_connection_create_serverside_client_bit)) == 0,
    "connection && connection->unreliable_incoming_queue && "
    "!(connection->flags&FLAG(_connection_create_serverside_client_bit))");
  assert_halt(buffer);
  assert_halt(size);
  assert_halt_msg(*buf_size > 2, "*buffer_size>sizeof(message_header)");

  if (!circular_queue_try_read(*(int *)(connection + 0x14), &header, 2, 0)) {
    return false;
  }
  byte_swap_message_header(&header, 0);
  packet_size = header >> 4;

  if (packet_size > DATAGRAM_MAXIMUM_SIZE) {
    error(2, "got an unusually large datagram (#d bytes); resetting unreliable incoming queue",
          packet_size);
    circular_queue_reset(*(int *)(connection + 0x14));
    return false;
  }

  if (packet_size > *buf_size) {
    error(2, "packet in queue is #%d bytes, but we can only handle #%d bytes!; resetting unreliable incoming queue",
          packet_size, *buf_size);
    circular_queue_reset(*(int *)(connection + 0x14));
    return false;
  }

  available = circular_queue_size(*(int *)(connection + 0x14));
  if ((unsigned int)packet_size + 4 <= available &&
      circular_queue_try_read(*(int *)(connection + 0x14), buffer, packet_size, 1) &&
      circular_queue_try_read(*(int *)(connection + 0x14), &source_addr, 4, 1)) {
    *(unsigned short *)buffer = header;
    assert_halt_msg((header & 1) == 0, "encryption should not be active");
    if (addr != (void *)0) {
      *(int *)addr = source_addr;
      *(unsigned short *)((char *)addr + 0x12) = 0;
      *(unsigned short *)((char *)addr + 0x10) = 4;
    }
    *buf_size = packet_size;
    return true;
  }

  available = circular_queue_size(*(int *)(connection + 0x14));
  error(2, "partial datagram in queue (#%d of #%d bytes); resetting queue",
        available, packet_size);
  circular_queue_reset(*(int *)(connection + 0x14));
  return false;
}

/* network_connection_read_reliable (0x1292f0).
 * Sibling of network_connection_read_unreliable for the reliable stream incoming queue (+0x10);
 * connection arrives in EDI.  Peeks the 2-byte header, byte-swaps it, derives
 * payload size (header >> 4).  Rejects messages larger than 0x800 bytes or the
 * caller buffer (resetting the queue).  Returns false without logging when the
 * full payload is not yet buffered.  On success reads the payload into buffer,
 * stamps the header word, asserts encryption inactive, and — when an out
 * address is supplied — queries the reliable endpoint's peer address
 * (FUN_00083a60), zeroing the 0x18-byte address and setting family=4 on a
 * mismatch.  Writes the payload size back through *size, bumps the
 * stream-messages-received counter (+0x2c), and returns true. */
bool network_connection_read_reliable(int connection, void *buffer, int *size, void *addr)
{
  unsigned short header;
  unsigned short *buf_size;
  unsigned short packet_size;
  int available;

  buf_size = (unsigned short *)size;

  assert_halt_msg(connection != 0 && *(int *)(connection + 0x10) != 0,
                  "connection && connection->reliable_incoming_queue");
  assert_halt(buffer);
  assert_halt(size);
  assert_halt_msg(*buf_size > 2, "*buffer_size>sizeof(message_header)");

  if (!circular_queue_try_read(*(int *)(connection + 0x10), &header, 2, 0)) {
    return false;
  }
  byte_swap_message_header(&header, 0);
  packet_size = header >> 4;

  if (packet_size > 0x800) {
    error(2, "got an unusually large message (#d bytes); resetting reliable incoming queue",
          packet_size);
    circular_queue_reset(*(int *)(connection + 0x10));
    return false;
  }

  if (packet_size > *buf_size) {
    error(2, "packet in queue is #%d bytes, but we can only handle #%d bytes!; resetting reliable incoming queue",
          packet_size, *buf_size);
    circular_queue_reset(*(int *)(connection + 0x10));
    return false;
  }

  available = circular_queue_size(*(int *)(connection + 0x10));
  if ((int)(unsigned int)packet_size <= available &&
      circular_queue_try_read(*(int *)(connection + 0x10), buffer, packet_size, 1)) {
    *(unsigned short *)buffer = header;
    assert_halt_msg((header & 1) == 0, "encryption should not be active");
    if (addr != (void *)0) {
      if (FUN_00083a60(*(int **)connection, addr) != 0) {
        csmemset(addr, 0, 0x18);
        *(unsigned short *)((char *)addr + 0x10) = 4;
      }
    }
    *buf_size = packet_size;
    *(int *)(connection + 0x2c) += 1;
    return true;
  }
  return false;
}

/* network_connection_idle_client_reliable_endpoint (0x1294d0).
 * Drains all pending bytes from a client's reliable transport endpoint into
 * the connection's reliable incoming queue (+0x10); connection arrives in ESI.
 * Records a start timestamp, asserts the connection and its reliable endpoint
 * and queue exist, then loops while the endpoint reports readable data
 * (FUN_00083040) and the queue has free space: it clamps the read to 0x800
 * bytes, receives into a local buffer (recv_endpoint), stamps the keep-alive
 * time (+0x08), optionally appends a debug-log timing record (dead in shipping;
 * the log FILE* at +0x18 is never opened), and pushes the received bytes into
 * the queue (FUN_00118ec0).  A non-positive recv result terminates: -4 (would
 * block) is benign; -3 marks the connection closed (flags|0x10); 0 logs
 * "connection lost"; any other negative logs the endpoint error string.  If
 * the whole drain took over 1000 ms it logs a "blocked" warning.  Returns
 * whether the drain completed without a hard failure. */
bool network_connection_idle_client_reliable_endpoint(int connection)
{
  uint8_t buffer[2048];
  int start_time;
  int avail;
  int received;
  int now;
  double elapsed;
  bool ok;
  const char *err;

  start_time = (int)system_milliseconds();
  ok = true;

  assert_halt(connection);
  assert_halt_msg(*(int *)connection != 0, "connection->reliable_endpoint");
  assert_halt_msg(*(int *)(connection + 0x10) != 0,
                  "connection->reliable_incoming_queue");

  avail = (int)circular_queue_free_space(*(int *)(connection + 0x10));

  while (FUN_00083040(*(int *)connection, 0) != 0 && avail > 0) {
    if (avail > 0x7ff) {
      avail = 0x800;
    }
    received = recv_endpoint(*(int **)connection, buffer, avail);
    if (received <= 0) {
      if (received != -4) {
        if (received == -3) {
          *(unsigned int *)(connection + 0x30) |= FLAG(_connection_closed_bit);
        } else if (received == 0) {
          error(2, "client reliable connection lost");
        } else {
          err = FUN_00081c80(received);
          error(2, "error '%s' reading from client reliable endpoint", err);
        }
        ok = false;
      }
      goto finish;
    }
    *(int *)(connection + 8) = (int)system_milliseconds();
    if (*(int *)(connection + 0x18) != 0) {
      now = (int)system_milliseconds();
      elapsed = (double)(now - *(int *)(connection + 0x1c));
      if (now - *(int *)(connection + 0x1c) < 0) {
        elapsed += *(double *)0x265d40;
      }
      elapsed = elapsed * *(double *)0x294bf0;
      crt_fprintf(*(void **)(connection + 0x18),
                  (const char *)L"%g\t%ld\t%ld\t%ld\t%ld\n", elapsed, 0, 0, 0,
                  received);
      crt_fflush(*(void **)(connection + 0x18));
    }
    if (!FUN_00118ec0(*(int *)(connection + 0x10), buffer, received)) {
      error(2, "circular_queue_queue_data() failed");
      ok = false;
    }
    avail = (int)circular_queue_free_space(*(int *)(connection + 0x10));
    if (!ok) {
      goto finish;
    }
  }

finish:
  now = (int)system_milliseconds();
  if ((unsigned int)(now - start_time) > 1000) {
    error(2, "blocked in network_connection_idle_client_reliable_endpoint");
  }
  return ok;
}

/* network_connection_idle (0x129a30).
 * Services a server connection's endpoint set once per call; connection
 * arrives in EBX and *output is cleared, receiving a newly-accepted client
 * connection when one is created.  Polls the endpoint set (poll_endpoint_set);
 * a poll status of -0xd is benign, any other non-zero status logs an error and
 * returns false.  On a clean poll it rewinds the set and walks every ready
 * endpoint: activity on the server's own listening endpoint (== *connection)
 * accepts a new client — when accepting is enabled (+0x4c) and the set is not
 * full (< 5) it accepts the raw endpoint (FUN_00084450), prepares it
 * (FUN_00083bd0) and wraps it in a reliable connection (network_connection_new_serverside_client, endpoint
 * in EDI), storing it into the first free child slot (+0x3c[0..3]); otherwise
 * it rejects, either invoking the rejection callback (+0x0c) then destroying
 * the endpoint, or silently dropping it (FUN_00084940).  Activity on an
 * existing child endpoint drains it via network_connection_idle_client_reliable_endpoint; a failed drain removes
 * the child's endpoint from the set and marks the child closed (flags|0x10).
 * An endpoint matching no known child asserts "rogue endpoint".  Returns the
 * running success flag. */
bool network_connection_idle(int connection, int *output)
{
  short poll_result;
  int endpoint;
  int accepted;
  int new_conn;
  int child;
  int i;
  int *slot;
  bool ok;
  const char *err;

  ok = true;

  assert_halt(connection);
  assert_halt_msg(*(int *)connection != 0,
                  "connection->connection.reliable_endpoint");
  assert_halt_msg(*(int *)(connection + 0x38) != 0, "connection->endpoint_set");
  assert_halt(output);

  *output = 0;
  poll_result = (short)poll_endpoint_set(*(int *)(connection + 0x38), 0);

  if (poll_result == 0) {
    rewind_endpoint_set(*(int *)(connection + 0x38));
    do {
      endpoint = FUN_000829b0(*(int *)(connection + 0x38));
      if (endpoint == 0) {
        return ok;
      }
      if (poll_result != 0) {
        return ok;
      }
      if (FUN_00083040(endpoint, 0) != 0) {
        if (endpoint == *(int *)connection) {
          if (*(uint8_t *)(connection + 0x4c) != 0 &&
              FUN_00082a30(*(int *)(connection + 0x38)) < 5) {
            /* accept a new client */
            accepted = FUN_00084450(endpoint);
            if (accepted == 0 || FUN_00083bd0(accepted, 0) != 0 ||
                (new_conn = (int)network_connection_new_serverside_client(accepted)) == 0) {
              error(2, "accept_endpoint() returned NULL");
            } else {
              slot = (int *)(connection + 0x3c);
              i = 0;
              do {
                if (*slot == 0) {
                  *output = new_conn;
                  *(int *)(connection + 0x3c + i * 4) = new_conn;
                  if (i >= 4) {
                    error(2, "error adding new client");
                  }
                  goto next_endpoint;
                }
                i = i + 1;
                slot = slot + 1;
              } while (i < 4);
              error(2, "error adding new client");
            }
          } else {
            /* reject: not accepting, or set is full */
            if (*(int *)(connection + 0xc) == 0) {
              poll_result = (short)FUN_00084940(endpoint);
            } else {
              accepted = FUN_00084450(endpoint);
              if (accepted != 0) {
                (*(void (*)(int))*(int *)(connection + 0xc))(accepted);
                destroy_endpoint((int *)accepted);
              }
            }
          }
        } else {
          /* activity on an existing client endpoint */
          i = 0;
          slot = (int *)(connection + 0x3c);
          do {
            if (*slot != 0 && *(int *)*slot == endpoint) {
              child = *(int *)(connection + 0x3c + i * 4);
              ok = network_connection_idle_client_reliable_endpoint(child);
              if (!ok) {
                if ((short)remove_endpoint_from_set(
                        *(int **)child,
                        *(uint32_t **)(connection + 0x38)) != 0) {
                  error(2, "failed to remove a client endpoint from the server's endpoint set");
                }
                *(unsigned int *)(child + 0x30) |= FLAG(_connection_closed_bit);
                ok = true;
              }
              if (i < 4) {
                goto next_endpoint;
              }
              break;
            }
            i = i + 1;
            slot = slot + 1;
          } while (i < 4);
          assert_halt_msg(0, "rogue endpoint connected to the server");
        }
      }
    next_endpoint:
      ;
    } while (ok);
  } else if (poll_result != -0xd) {
    err = FUN_00081c80(poll_result);
    error(2, "poll_endpoint_set() returned error '%s'", err);
    return false;
  }
  return ok;
}

/* network_connection_notify_traffic_event (0x1288e0).
 * Records a traffic event against a connection's statistics and optional debug
 * traffic log.  Registers: event in ECX, enable/amount in EAX, connection in
 * ESI.  All work is gated on enable > 0.  event selects an 8-way switch:
 *   0 open the per-connection traffic log — derives a filename from the peer
 *     address (transport_address_to_string, truncated at ':'), opens it
 *     (crt_fopen; always fails in shipping so the log FILE* at +0x18 stays
 *     null), writes a column header, and stamps the log start time (+0x1c);
 *   1 close the log — dumps the datagram/stream counters, header overhead, and
 *     connection lifetime, then crt_fclose;
 *   2/3/4/5 append one timing row (elapsed seconds + the enable amount in the
 *     udp-out / udp-in / tcp-out / tcp-in column); 2 and 3 also bump the
 *     datagrams-sent (+0x20) / datagrams-received (+0x24) counters;
 *   6/7 bump the stream-messages-sent (+0x28) / received (+0x2c) counters;
 *   default asserts "unknown traffic event".
 * The log body is dead in shipping (no log file opens); the counter increments
 * in cases 2/3/6/7 are the only live side effects.  Log format strings are
 * referenced by their original rodata address (the exact pointer the original
 * passes to fwprintf).  The signed tick delta is folded to unsigned via
 * _DAT_00265d40 (2^32) before scaling by _DAT_00294bf0 (seconds per tick). */
void network_connection_notify_traffic_event(int event, int enable, int connection)
{
  uint8_t addr_buf[24];
  char name_buf[256];
  int now;
  int i;
  double elapsed;
  const char *addr_str;

  assert_halt(connection);

  if (enable <= 0) {
    return;
  }

  switch (event) {
  case 0:
    if (FUN_00083a60(*(int **)connection, addr_buf) != 0 &&
        FUN_00083a60(*(int **)(connection + 4), addr_buf) != 0) {
      csmemset(addr_buf, 0, 0x18);
      *(unsigned short *)(addr_buf + 0x10) = 4;
    }
    csmemset(name_buf, 0, sizeof(name_buf));
    addr_str = transport_address_to_string(addr_buf);
    csstrcpy(name_buf, addr_str);
    for (i = 0; name_buf[i] != '\0'; i++) {
      if (name_buf[i] == ':') {
        name_buf[i] = '\0';
        break;
      }
    }
    FUN_0008dc30(name_buf, (const char *)0x294d58);
    *(void **)(connection + 0x18) =
      crt_fopen(name_buf, (const char *)0x265938);
    if (*(void **)(connection + 0x18) != 0) {
      crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294d10);
      crt_fflush(*(void **)(connection + 0x18));
    }
    *(int *)(connection + 0x1c) = (int)system_milliseconds();
    return;

  case 1:
    if (*(void **)(connection + 0x18) == 0) {
      break;
    }
    if (FUN_00083a60(*(int **)connection, addr_buf) != 0) {
      csmemset(addr_buf, 0, 0x18);
      *(unsigned short *)(addr_buf + 0x10) = 4;
    }
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294d08);
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294cf4,
                *(int *)(connection + 0x20));
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294cdc,
                *(int *)(connection + 0x24));
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294cc0,
                *(int *)(connection + 0x28));
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294ca0,
                *(int *)(connection + 0x2c));
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294c6c, 0x1c);
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294c3c, 0x28);
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294bf8);
    now = (int)system_milliseconds();
    elapsed = (double)(now - *(int *)(connection + 0x1c));
    if (now - *(int *)(connection + 0x1c) < 0) {
      elapsed += *(double *)0x265d40;
    }
    elapsed = elapsed * *(double *)0x294bf0;
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294bcc, elapsed);
    addr_str = transport_address_to_string(addr_buf);
    crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294ba4,
                addr_str);
    crt_fclose(*(void **)(connection + 0x18));
    *(void **)(connection + 0x18) = 0;
    return;

  case 2:
    if (*(void **)(connection + 0x18) != 0) {
      now = (int)system_milliseconds();
      elapsed = (double)(now - *(int *)(connection + 0x1c));
      if (now - *(int *)(connection + 0x1c) < 0) {
        elapsed += *(double *)0x265d40;
      }
      elapsed = elapsed * *(double *)0x294bf0;
      crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294b90,
                  elapsed, enable, 0, 0, 0);
      crt_fflush(*(void **)(connection + 0x18));
    }
    *(int *)(connection + 0x20) += 1;
    return;

  case 3:
    if (*(void **)(connection + 0x18) != 0) {
      now = (int)system_milliseconds();
      elapsed = (double)(now - *(int *)(connection + 0x1c));
      if (now - *(int *)(connection + 0x1c) < 0) {
        elapsed += *(double *)0x265d40;
      }
      elapsed = elapsed * *(double *)0x294bf0;
      crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294b90,
                  elapsed, 0, enable, 0, 0);
      crt_fflush(*(void **)(connection + 0x18));
    }
    *(int *)(connection + 0x24) += 1;
    return;

  case 4:
    if (*(void **)(connection + 0x18) != 0) {
      now = (int)system_milliseconds();
      elapsed = (double)(now - *(int *)(connection + 0x1c));
      if (now - *(int *)(connection + 0x1c) < 0) {
        elapsed += *(double *)0x265d40;
      }
      elapsed = elapsed * *(double *)0x294bf0;
      crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294b90,
                  elapsed, 0, 0, enable, 0);
      crt_fflush(*(void **)(connection + 0x18));
      return;
    }
    break;

  case 5:
    if (*(void **)(connection + 0x18) != 0) {
      now = (int)system_milliseconds();
      elapsed = (double)(now - *(int *)(connection + 0x1c));
      if (now - *(int *)(connection + 0x1c) < 0) {
        elapsed += *(double *)0x265d40;
      }
      elapsed = elapsed * *(double *)0x294bf0;
      crt_fprintf(*(void **)(connection + 0x18), (const char *)0x294b90,
                  elapsed, 0, 0, 0, enable);
      crt_fflush(*(void **)(connection + 0x18));
      return;
    }
    break;

  case 6:
    *(int *)(connection + 0x28) += 1;
    return;

  case 7:
    *(int *)(connection + 0x2c) += 1;
    return;

  default:
    assert_halt_msg(0, "!\"unknown traffic event\"");
  }
}

/* network_connection_new_serverside_client (0x129270).
 * Wraps an already-accepted reliable transport endpoint (passed in EDI) in a
 * fresh server-side client connection.  Asserts the endpoint is non-null,
 * allocates a 0x38-byte connection block, marks it a server-side client
 * (flags = 4 at +0x30), stores the endpoint at +0x00, and creates the reliable
 * incoming queue (0x8000 bytes) at +0x10.  On queue-allocation failure it tears
 * the connection down and returns null.  On success it fires the
 * connection-created traffic event (event 0, enable 1, via registers) and
 * returns the new connection block. */
void *network_connection_new_serverside_client(int endpoint)
{
  int *connection;

  assert_halt(endpoint);

  connection = (int *)debug_malloc(
    0x38, 1, "c:\\halo\\SOURCE\\networking\\network_connection.c", 0x347);
  if (connection != (int *)0) {
    connection[0xc] = FLAG(_connection_create_serverside_client_bit);
    connection[0] = endpoint;
    connection[4] = (int)circular_queue_new((int)"incoming-reliable", 0x8000);
    if (connection[4] == 0) {
      network_connection_delete((int)connection);
      return (void *)0;
    }
    network_connection_notify_traffic_event(0, 1, (int)connection);
  }
  return connection;
}
