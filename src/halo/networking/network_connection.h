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

/* Rejection callback installed by
 * network_connection_set_connection_rejection_procedure and invoked from
 * network_connection_idle's reject path with the raw accepted endpoint. */
typedef void (*connection_rejection_procedure)(int endpoint);

/* The transport connection block.  Layout recovered from access sites in
 * network_connection.c; field names at +0x00/+0x10/+0x14/+0x30 are quoted
 * verbatim by binary assert strings ("connection->reliable_endpoint",
 * "connection->reliable_incoming_queue", "connection->unreliable_incoming_queue",
 * "connection->flags&FLAG(...)"); the four counters at +0x20..+0x2c are named
 * by the traffic-log rodata strings ("datagrams sent\t%ld", "datagrams
 * received\t%ld", "stream messages sent\t%ld", "stream messages received\t%ld").
 * sizeof == 0x38 == the clientside-client debug_malloc size in
 * network_connection_new / network_connection_new_serverside_client. */
typedef struct network_connection {
  int reliable_endpoint;                 /* 0x00 transport endpoint handle */
  int unreliable_endpoint;               /* 0x04 transport endpoint handle */
  unsigned int last_keep_alive_milliseconds; /* 0x08 stamped by network_connection_keep_alive; creation time at new() */
  connection_rejection_procedure rejection_procedure; /* 0x0c */
  int reliable_incoming_queue;           /* 0x10 circular queue handle */
  int unreliable_incoming_queue;         /* 0x14 circular queue handle */
  void *traffic_log_file;                /* 0x18 crt FILE*; never opens in shipping */
  int traffic_log_start_milliseconds;    /* 0x1c signed: tick delta is sign-tested */
  int datagrams_sent;                    /* 0x20 */
  int datagrams_received;                /* 0x24 */
  int stream_messages_sent;              /* 0x28 */
  int stream_messages_received;          /* 0x2c */
  unsigned int flags;                    /* 0x30 FLAG(_connection_*) bits; byte-tested at several sites (keep access width) */
  unsigned short well_known_port;        /* 0x34 */
  unsigned short field_36;               /* 0x36 never accessed; alignment/unknown */
} network_connection;

cs(network_connection, 0x38);
co(network_connection, reliable_endpoint, 0x00);
co(network_connection, unreliable_endpoint, 0x04);
co(network_connection, last_keep_alive_milliseconds, 0x08);
co(network_connection, rejection_procedure, 0x0c);
co(network_connection, reliable_incoming_queue, 0x10);
co(network_connection, unreliable_incoming_queue, 0x14);
co(network_connection, traffic_log_file, 0x18);
co(network_connection, traffic_log_start_milliseconds, 0x1c);
co(network_connection, datagrams_sent, 0x20);
co(network_connection, datagrams_received, 0x24);
co(network_connection, stream_messages_sent, 0x28);
co(network_connection, stream_messages_received, 0x2c);
co(network_connection, flags, 0x30);
co(network_connection, well_known_port, 0x34);

/* Server-role connection: embeds the base block (the binary assert
 * "connection->connection.reliable_endpoint" in network_connection_idle
 * proves the original nested a plain connection at +0x00) and appends the
 * endpoint set (assert "connection->endpoint_set"), the four child
 * serverside-client connection slots walked by network_connection_delete /
 * network_connection_idle, and the accept gate stored by
 * network_server_allow_client_connections.
 * sizeof == 0x50 == the server debug_malloc size in network_connection_new. */
typedef struct network_server_connection {
  network_connection connection;         /* 0x00 */
  int endpoint_set;                      /* 0x38 endpoint-set handle (capacity 5: listen + 4 clients) */
  network_connection *client_connections[4]; /* 0x3c */
  bool allow_client_connections;         /* 0x4c */
  uint8_t pad_4d[3];                     /* 0x4d */
} network_server_connection;

cs(network_server_connection, 0x50);
co(network_server_connection, endpoint_set, 0x38);
co(network_server_connection, client_connections, 0x3c);
co(network_server_connection, allow_client_connections, 0x4c);

#endif /* NETWORK_CONNECTION_H */
