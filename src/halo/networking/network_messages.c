void verify_packet_group_definitions(group_definition *group)
{
  short i;

  for (i = 0; i < group->packet_count; i++) {
    packet_entry *entry = &group->packets[i];

    if (entry->definition != NULL) {
      assert_halt(entry->packet_class >= 0 &&
                  entry->packet_class < group->packet_class_count);
      assert_halt(entry->definition->size <=
                  group->maximum_decoded_packet_size);
      assert_halt((uint32_t)(entry->definition->size + sizeof(packet_header)) <=
                  (uint32_t)group->maximum_encoded_packet_size);
      verify_packet_definition(entry->definition);
    }
  }
}

/* network_messages.c — Network game packet group initialization.
 *
 * Corresponds to network_messages.obj.
 * initialize_network_game_packets at 0x12b640 is a thin wrapper that calls
 * verify_packet_group_definitions (0x11a930) with the global
 * s_network_game_messages_group (0x323510).
 *
 * verify_packet_group_definitions iterates the packet entries in the given
 * group, validates class bounds and size constraints, then calls
 * verify_packet_definition on each non-NULL definition.
 *
 * verify_packet_definition at 0x11b540 validates a single packet_definition:
 * checks non-NULL, size >= 0, version >= 0, name and fields non-NULL, then
 * (if not yet validated) computes total field sizes and confirms they match
 * the declared size. Sets validated = 1 after success.
 *
 * Original source: c:\halo\SOURCE\memory\data_packet_groups.c lines 0x28-0x2a
 * verify_packet_definition source: c:\halo\SOURCE\memory\data_packets.c lines
 * 0x20-0x2b
 */

/* compute_packet_field_sizes at 0x11add0 — not yet ported (data_packets.c) */
#define compute_packet_field_sizes \
  ((void (*)(packet_definition *, short *, short *, short *))0x11add0)

void verify_packet_definition(packet_definition *def)
{
  short computed_size;
  short field_count;

  if (def == NULL) {
    display_assert("packet_definition",
                   "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x20, 1);
    system_exit(-1);
  }
  if (def->size < 0) {
    display_assert("packet_definition->size>=0",
                   "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x21, 1);
    system_exit(-1);
  }
  if (def->version < 0) {
    display_assert("packet_definition->version>=0",
                   "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x22, 1);
    system_exit(-1);
  }
  if (def->name == NULL || def->fields == NULL) {
    display_assert("packet_definition->name && packet_definition->fields",
                   "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x23, 1);
    system_exit(-1);
  }
  if (!def->validated) {
    compute_packet_field_sizes(def, &computed_size, def->fields, &field_count);
    if (computed_size != def->size) {
      display_assert(csprintf(error_string_buffer,
                              "packet '%s' fields added up to #%d bytes but "
                              "should have been #%d bytes.",
                              def->name, (int)computed_size, (int)def->size),
                     "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x2b, 1);
      system_exit(-1);
    }
    def->validated = 1;
  }
}

/* Initialize a hashtable header.
 * Source: c:\halo\SOURCE\memory\hashtable.c lines 0x29-0x2c (41-44).
 * Asserts: table non-NULL, key_size>0, element_size>0, 0<load_factor<=1.
 * Byte layout of *table (all offsets confirmed from disassembly):
 *   +0x00 int16  key_size
 *   +0x02 int16  element_size
 *   +0x04 int16  count = 0
 *   +0x06 int16  sentinel = -1 (0xffff)
 *   +0x08 float  load_factor
 *   +0x10 dword  param_5
 *   +0x14 dword  param_6
 *   +0x18 dword  0
 *   +0x1c       array header (passed to array_new with key_size+element_size)
 */
void hashtable_new(void *table, short key_size, short element_size,
                   float load_factor, int param_5, int param_6)
{
  char *t;

  if (table == NULL) {
    display_assert("table", "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x29, 1);
    system_exit(-1);
  }
  if (key_size < 1) {
    display_assert("key_size>0", "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x2a,
                   1);
    system_exit(-1);
  }
  if (element_size < 1) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\hashtable.c",
                   0x2b, 1);
    system_exit(-1);
  }
  if (!(load_factor > 0.0f && load_factor <= 1.0f)) {
    display_assert("load_factor>0 && load_factor<=1",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x2c, 1);
    system_exit(-1);
  }
  t = (char *)table;
  *(float *)(t + 0x08) = load_factor;
  *(int *)(t + 0x10) = param_5;
  *(int *)(t + 0x14) = param_6;
  *(short *)(t + 0x00) = key_size;
  *(short *)(t + 0x02) = element_size;
  *(short *)(t + 0x04) = 0;
  *(short *)(t + 0x06) = (short)-1;
  array_new((int *)(t + 0x1c), (int)key_size + (int)element_size);
  *(int *)(t + 0x18) = 0;
}

void initialize_network_game_packets(void)
{
  verify_packet_group_definitions(&s_network_game_messages_group);
}

/* Static 0x604-byte output buffer for encode_network_game_message (0x46e8d0).
 * Passed as the pre-allocated destination to create_message(); only one caller
 * exists so this is safe as a module-level static. */
static char s_network_game_message_buffer[0x604];

/* encode_network_game_message — validate, encode and wrap a typed network
 * game message struct into a transmittable message packet (0x12b700).
 *
 * Validates that message_struct_size matches the expected size for the given
 * type, encodes the struct into a 1536-byte stack buffer using the global
 * packet group definition, then wraps the encoded bytes in a message header
 * and returns a pointer to the resulting message, or NULL on failure.
 */
void *encode_network_game_message(int type, void *data,
                                  int16_t message_struct_size)
{
  char encoded_buf[0x600];
  int32_t encoded_size;

  encoded_size = 0x600;

  switch ((int16_t)type) {
  case 0:
    if (message_struct_size == 0xc)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_broadcast_game_search)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xa0, 1);
    system_exit(-1);
  case 1:
    if (message_struct_size == 8)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_client_ping)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xa1, 1);
    system_exit(-1);
  case 2:
    if (message_struct_size == 0x114)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_server_game_advertise)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xa4, 1);
    system_exit(-1);
  case 3:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_server_pong)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xa5, 1);
    system_exit(-1);
  case 4:
    if (message_struct_size == 8)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_machine_accepted)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xa8, 1);
    system_exit(-1);
  case 5:
    if (message_struct_size == 2)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_machine_rejected)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xa9, 1);
    system_exit(-1);
  case 6:
    if (message_struct_size == 0x434)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_game_settings_update)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xaa, 1);
    system_exit(-1);
  case 7:
    if (message_struct_size == 2)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_pregame_countdown)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xab, 1);
    system_exit(-1);
  case 8:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_server_begin_game)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xad, 1);
    system_exit(-1);
  case 9:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_graceful_game_exit_pregame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xae, 1);
    system_exit(-1);
  case 10:
    if (message_struct_size == 2)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_pregame_keep_alive)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xac, 1);
    system_exit(-1);
  case 11:
    if (message_struct_size == 2)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_postgame_keep_alive)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xb1, 1);
    system_exit(-1);
  case 12:
    if (message_struct_size == 0x50)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_join_game_request)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xb4, 1);
    system_exit(-1);
  case 13:
    if (message_struct_size == 0x20)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_add_player_request_pregame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xb5, 1);
    system_exit(-1);
  case 14:
    if (message_struct_size == 0x20)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_client_remove_player_"
                   "request_pregame)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xb6, 1);
    system_exit(-1);
  case 15:
    if (message_struct_size == 0x44)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_settings_request)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xb7, 1);
    system_exit(-1);
  case 16:
    if (message_struct_size == 0x20)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_player_settings_request)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xb8, 1);
    system_exit(-1);
  case 17:
    if (message_struct_size == 2)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_game_start_request)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xb9, 1);
    system_exit(-1);
  case 18:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_graceful_game_exit_pregame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xba, 1);
    system_exit(-1);
  case 19:
    if (message_struct_size == 0x100)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_map_is_precached_pregame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xbb, 1);
    system_exit(-1);
  case 20:
    if (message_struct_size == 0x210)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_server_game_update)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xbe, 1);
    system_exit(-1);
  case 21:
    if (message_struct_size == 0x20)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_add_player_ingame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xbf, 1);
    system_exit(-1);
  case 22:
    if (message_struct_size == 0x24)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_remove_player_ingame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xc0, 1);
    system_exit(-1);
  case 23:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_server_game_over)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xc1, 1);
    system_exit(-1);
  case 24:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_client_loaded)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xc4, 1);
    system_exit(-1);
  case 25:
    if (message_struct_size == 0x88)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_client_game_update)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xc5, 1);
    system_exit(-1);
  case 26:
    if (message_struct_size == 0x20)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_add_player_request_ingame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xc6, 1);
    system_exit(-1);
  case 27:
    if (message_struct_size == 0x20)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_client_remove_player_"
                   "request_ingame)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xc7, 1);
    system_exit(-1);
  case 28:
    if (message_struct_size == 0x10)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_host_crashed_cry_for_help)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xc9, 1);
    system_exit(-1);
  case 29:
    if (message_struct_size == 0x10)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_client_join_new_host)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xca, 1);
    system_exit(-1);
  case 30:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_switch_to_pregame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xcd, 1);
    system_exit(-1);
  case 31:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_server_graceful_game_exit_postgame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xce, 1);
    system_exit(-1);
  case 32:
    if (message_struct_size == 0x20)
      goto size_ok;
    display_assert("message_struct_size==sizeof(message_client_remove_player_"
                   "request_postgame)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xd1, 1);
    system_exit(-1);
  case 33:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_switch_to_pregame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xd2, 1);
    system_exit(-1);
  case 34:
    if (message_struct_size == 4)
      goto size_ok;
    display_assert(
      "message_struct_size==sizeof(message_client_graceful_game_exit_postgame)",
      "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xd3, 1);
    system_exit(-1);
  default:
    display_assert("unknown network game message structure type",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0xd5, 1);
    system_exit(-1);
  }

size_ok:
  if (data == NULL || (int16_t)encoded_size < 1) {
    display_assert("message_struct && encoded_message && encoded_message_size "
                   "&& (*encoded_message_size>0)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0x161,
                   1);
    system_exit(-1);
  }

  if (!encode_packet_group(&s_network_game_messages_group, data, encoded_buf,
                           &encoded_size, type, 1)) {
    network_game_log("encode_network_game_message() failed");
    return NULL;
  }

  {
    void *msg = create_message(3, encoded_buf, encoded_size,
                               s_network_game_message_buffer, 0x604);
    if (msg == NULL) {
      network_game_log("create_message() failed");
    }
    return msg;
  }
}
