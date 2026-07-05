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
