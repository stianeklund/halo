/* Halo CE Xbox — network connection (networking/network_connection.h).
 * Recovered counterpart of c:\halo\SOURCE\networking\network_connection.h
 * (the original header is listed in the punpckhdq PDB TU corpus).
 */
#ifndef NETWORK_CONNECTION_H
#define NETWORK_CONNECTION_H

/* Mirrors the original source's FLAG(bit) macro, quoted verbatim by binary
 * assert strings ("server_connection->flags&FLAG(_connection_create_server_bit)"). */
#ifndef FLAG
#define FLAG(b) (1 << (b))
#endif

/* Connection flag bits (the flags field at connection+0x30).  The three
 * _connection_create_* names are quoted verbatim by binary assert strings in
 * network_connection.c.  The last two have no binary name (unverified names):
 * closed is set on recv error -3 and tested by network_connection_active;
 * going-stale is tested by network_connection_going_stale. */
enum {
  _connection_create_server_bit = 0,
  _connection_create_clientside_client_bit = 1,
  _connection_create_serverside_client_bit = 2,
  _connection_closed_bit = 4,     /* unverified name */
  _connection_going_stale_bit = 5 /* unverified name */
};

/* Both named by binary assert strings in network_connection.c
 * ("buffer_size <= DATAGRAM_MAXIMUM_SIZE",
 *  "well_known_port > MAXIMUM_RESERVED_NETWORK_PORT"). */
#define DATAGRAM_MAXIMUM_SIZE 400
#define MAXIMUM_RESERVED_NETWORK_PORT 0x3ff

#endif /* NETWORK_CONNECTION_H */
