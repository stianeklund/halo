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
