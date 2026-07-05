/* Halo CE Xbox — network connection helpers.
 * c:\halo\SOURCE\networking\network_connection.c
 *
 * A network_connection is the per-machine transport endpoint used by the
 * server/client managers.  connection+0x30 is a byte flags field; bit 0
 * (mask 1) is _connection_create_server_bit, set on connections that were
 * created as the game server.  connection+0x4c is the "allow client
 * connections" bool, gating whether the server accepts new client joins.
 */

/* network_server_allow_client_connections (0x1282f0).
 * Sets the server connection's accept-clients flag.  Asserts the connection
 * exists and is a server-role connection, then stores the requested state. */
void network_server_allow_client_connections(int server_connection, bool open)
{
  assert_halt(server_connection);
  assert_halt_msg(
    *(uint8_t *)(server_connection + 0x30) & 1,
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
  if ((*(uint8_t *)(connection + 0x30) & 6) != 0 && *(int *)connection != 0) {
    if (FUN_000831a0(*(int *)connection)) {
      return true;
    }
  }
  return false;
}

/* FUN_001283c0 (0x1283c0).
 * Refreshes up to two per-endpoint network-address out-buffers from the
 * connection.  connection+0x00 and connection+0x04 are the two transport
 * endpoint handles.  For each supplied out-buffer (buf, flag): if the matching
 * endpoint is null, or FUN_00083a60(endpoint, buffer) reports a mismatch
 * (non-zero), the 0x18-byte buffer is reset to zero and its address-type field
 * at +0x10 is set to 4.  Asserts the connection exists.  Current callers pass
 * flag==0, so only the first buffer is refreshed. */
void FUN_001283c0(int connection, void *buf, int flag)
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
    *(uint8_t *)(server_connection + 0x30) & 1,
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
  return ((*(unsigned int *)(connection + 0x30) >> 4) & 1) == 0;
}

/* network_connection_going_stale (0x1286a0).
 * Predicate: asserts the connection exists, then reports whether the
 * "going stale" bit (bit 5, mask 0x20) of the flags dword at +0x30 is SET.
 * A connection is flagged going-stale once bit 5 has been raised (e.g. when
 * traffic has lapsed and the transport is about to be torn down). */
bool network_connection_going_stale(int connection)
{
  assert_halt(connection);
  return (*(unsigned int *)(connection + 0x30) >> 5) & 1;
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
 * is freed.  FUN_001288e0 takes its arguments in registers (event=ECX,
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
  FUN_001288e0(1, 1, connection);
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
  if ((*(uint8_t *)(connection + 0x30) & 1) != 0) {
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
 * FUN_001288e0 takes its args in registers (event=ECX, enable=EAX,
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

  assert_halt_msg((flags & 1) != 0 || (flags & 2) != 0,
                  "(flags&FLAG(_connection_create_server_bit))|| "
                  "(flags&FLAG(_connection_create_clientside_client_bit))");

  if ((flags & 1) == 0) {
    if ((flags & 2) == 0) {
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
    assert_halt_msg(well_known_port > 0x3ff,
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

  if ((flags & 1) != 0) {
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
  FUN_001288e0(0, 1, connection);
  return connection;

fail:
  network_connection_delete(connection);
  return 0;
}
