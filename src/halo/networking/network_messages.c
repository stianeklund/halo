#include <stdarg.h>

/* compute_packet_field_sizes (0x11add0) and _data_packet_encode (0x11afa0) are
 * now ported in networking/data_packet_groups.c; their declarations come from
 * kb.json via decl.h.  The raw-address XCALL macros that used to stand in for
 * them were removed so the calls below reach the lifted C. */

/* ========================================================================
 * data_encoding.c — Decode-side encoding state helpers
 * Original source: c:\halo\SOURCE\memory\data_encoding.c
 *
 * Encoding state struct (16 bytes, int[4]):
 *   [0] = buffer pointer
 *   [1] = current offset
 *   [2] = buffer_size
 *   [3] = overflow flag (byte at low byte of word [3])
 * ======================================================================== */

#define byte_swap_raw FUN_00118620
#define byte_swap_structures FUN_00118be0
#define encode_state_new FUN_00119c50
#define encode_raw_data FUN_00119cc0

#define array_get_element FUN_00117ee0

#define array_reset array_new

#define array_dispose FUN_00117cf0

/* packet_header byte-swap definition at 0x3220c0 */
#define packet_header_bs_def ((void *)0x3220c0)

/* hash primes table at 0x3220d4 */
#define hashtable_primes ((short *)0x3220d4)

/* last decode error string global at 0x46e804 */
#define s_last_decode_error (*(char **)0x46e804)

/* decode_string — copy a string from source into the state buffer (0x11a230).
 * Source: data_encoding.c line 0xb6. */
bool data_encode_string(data_encoding_state_t *state, const char *string, short maximum_length)
{
  short string_length;
  int destination;

  string_length = strnlen(string, (int)maximum_length);
  destination = (int)state->buffer + state->offset;
  if (!(state->offset + string_length + 1 <= state->buffer_size)) {
    display_assert("state->offset+string_length+1<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0xb6, 1);
    system_exit(-1);
  }
  if ((state->offset + string_length + 1 <= state->buffer_size) &&
      (state->overflow == '\0')) {
    csstrncpy((char *)destination, string, (int)string_length);
    *(char *)(destination + (int)string_length) = 0;
    state->offset += string_length + 1;
  } else {
    state->overflow = 1;
  }
  return state->overflow == '\0';
}

/* decode_state_new — initialize a decode state struct (0x11a2d0).
 * Source: data_encoding.c line 0xcc. */
void data_decode_new(data_encoding_state_t *state, void *buffer, int buffer_size)
{
  if (buffer == NULL) {
    display_assert("buffer", "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0xcc,
                   1);
    system_exit(-1);
  }
  if (buffer_size < 0) {
    display_assert("buffer_size>=0",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0xcd, 1);
    system_exit(-1);
  }
  csmemset(state, 0, sizeof(data_encoding_state_t));
  state->buffer = buffer;
  state->buffer_size = buffer_size;
}

/* decode_structures — byte-swap structures in-place in the buffer (0x11a340).
 * Source: data_encoding.c line 0xde. */
int data_decode_structures(data_encoding_state_t *state, short count, void *bs_definition)
{
  short memory_size;
  char *structures;

  structures = NULL;
  if (!(state != NULL && state->buffer != NULL && state->offset >= 0 &&
        state->offset <= state->buffer_size)) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0xde, 1);
    system_exit(-1);
  }
  if (count < 0) {
    display_assert("structure_count>=0",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0xdf, 1);
    system_exit(-1);
  }
  if (bs_definition == NULL) {
    display_assert("bs_definition", "c:\\halo\\SOURCE\\memory\\data_encoding.c",
                   0xe0, 1);
    system_exit(-1);
  }
  memory_size = *(short *)((char *)bs_definition + 4) * count;
  if (((int)memory_size + state->offset <= state->buffer_size) && (state->overflow == '\0')) {
    structures = (char *)state->buffer + state->offset;
    if (memory_size != 0) {
      byte_swap_structures(bs_definition, (void *)structures, (int)count);
      state->offset = state->offset + (int)memory_size;
    }
  } else {
    state->overflow = 1;
  }
  return (int)structures;
}

/* decode_raw_data — byte-swap raw elements in the buffer (0x11a430).
 * Source: data_encoding.c line 0x100. */
__declspec(noinline) int data_decode_memory(data_encoding_state_t *state, short count, int element_size)
{
  int byte_count;
  int result;

  result = 0;
  if (!(state != NULL && state->buffer != NULL && state->offset >= 0 &&
        state->offset <= state->buffer_size)) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x100, 1);
    system_exit(-1);
  }
  if (count < 0) {
    display_assert("count>=0", "c:\\halo\\SOURCE\\memory\\data_encoding.c",
                   0x101, 1);
    system_exit(-1);
  }
  switch (element_size) {
  case 1:
    byte_count = (int)count;
    break;
  case -2:
    byte_count = (int)count << 1;
    break;
  case -4:
    byte_count = (int)count << 2;
    break;
  case -8:
    byte_count = (int)count << 3;
    break;
  default:
    display_assert(NULL, "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x109, 1);
    system_exit(-1);
    byte_count = (int)count;
    break;
  }
  if (state->offset + byte_count <= state->buffer_size &&
      state->overflow == '\0') {
    result = (int)state->buffer + state->offset;
    if (element_size != 1) {
      FUN_00118620((void *)result, (int)count, element_size);
    }
    state->offset += byte_count;
    return result;
  }
  state->overflow = 1;
  return result;
}

/* decode_byte — read a single byte from the decode buffer (0x11a560).
 * Source: data_encoding.c. */
__declspec(noinline) unsigned char data_decode_byte(data_encoding_state_t *state)
{
  int new_offset;
  unsigned char *value;

  if (!(state != NULL && state->buffer != NULL && state->offset >= 0 &&
        state->offset <= state->buffer_size)) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x100, 1);
    system_exit(-1);
  }
  new_offset = state->offset + 1;
  if (new_offset > state->buffer_size || state->overflow != '\0') {
    state->overflow = 1;
    value = NULL;
  } else {
    value = (unsigned char *)((int)state->buffer + state->offset);
    state->offset = new_offset;
  }
  return value ? *value : 0;
}

/* decode_short — read and byte-swap a 16-bit value from the buffer (0x11a5d0).
 * Source: data_encoding.c. */
short data_decode_short(data_encoding_state_t *state)
{
  short *value;

  if (!(state != NULL && state->buffer != NULL && state->offset >= 0 &&
        state->offset <= state->buffer_size)) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x100, 1);
    system_exit(-1);
  }
  if ((state->offset + 2 > state->buffer_size) || (state->overflow != '\0')) {
    state->overflow = 1;
    value = NULL;
  } else {
    value = (short *)((int)state->buffer + state->offset);
    FUN_00118620(value, 1, -2);
    state->offset += 2;
  }
  return value ? *value : 0;
}

/* decode_long — read and byte-swap a 32-bit value from the buffer (0x11a650).
 * Source: data_encoding.c. */
int data_decode_long(data_encoding_state_t *state)
{
  int *ptr;

  if (!(state != NULL && state->buffer != NULL && state->offset >= 0 &&
        state->offset <= state->buffer_size)) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x100, 1);
    system_exit(-1);
  }
  if ((state->buffer_size < state->offset + 4) || (state->overflow != '\0')) {
    state->overflow = 1;
  } else {
    ptr = (int *)((int)state->buffer + state->offset);
    FUN_00118620(ptr, 1, -4);
    state->offset += 4;
    if (ptr != NULL) {
      return *ptr;
    }
  }
  return 0;
}

/* decode_long_long — read and byte-swap an 8-byte value (0x11a6d0).
 * Source: data_encoding.c. Wrapper around decode_raw_data(state, 1, -8). */
int64_t data_decode_int64(data_encoding_state_t *state)
{
  int64_t *ptr;

  /* Single-expression form: the null check and the 8-byte load share the
   * returned pointer register. */
  return (ptr = (int64_t *)data_decode_memory(state, 1, -8))
           ? *ptr
           : 0;
}

/* decode_value — width-adaptive read based on maximum_value (0x11a700).
 * Source: data_encoding.c line 0x141. */
__declspec(noinline) unsigned int data_decode_integer(data_encoding_state_t *state, int maximum_value)
{
  if (maximum_value <= 0) {
    display_assert("maximum_value>0",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x141, 1);
    system_exit(-1);
  }
  if (maximum_value <= 0xff) {
    return (unsigned char)data_decode_byte(state);
  }
  if (maximum_value <= 0xffff) {
    return (short)data_decode_short(state);
  }
  return data_decode_long(state);
}

/* decode_element_array — read count + structures from buffer (0x11a770).
 * Source: data_encoding.c line 0x15c. */
void *data_decode_array(data_encoding_state_t *state, int element_size,
                   unsigned int *element_count_reference, int maximum_element_count,
                   void *bs_definition)
{
  int element_count;

  if (state == NULL || state->buffer == NULL || state->offset < 0 ||
      state->offset >= state->buffer_size) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x15c, 1);
    system_exit(-1);
  }
  if (element_count_reference == NULL) {
    display_assert("element_count_reference",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x15d, 1);
    system_exit(-1);
  }
  if (maximum_element_count <= 0) {
    display_assert("maximum_element_count>0",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x15e, 1);
    system_exit(-1);
  }
  if (bs_definition == NULL) {
    display_assert("bs_definition", "c:\\halo\\SOURCE\\memory\\data_encoding.c",
                   0x15f, 1);
    system_exit(-1);
  }
  switch (element_size) {
  case 1:
    element_count = (int)(unsigned char)data_decode_byte(state);
    break;
  case -2:
    element_count = (int)(short)data_decode_short(state);
    break;
  case -4:
    element_count = data_decode_long(state);
    break;
  case -8:
    element_count = (int)data_decode_int64(state);
    break;
  default:
    display_assert(NULL, "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x172, 1);
    system_exit(-1);
    break;
  }
  if (state->overflow == '\0' && element_count >= 0 && element_count <= maximum_element_count) {
    *element_count_reference = (unsigned int)element_count;
    return (void *)data_decode_structures(state, (short)element_count, bs_definition);
  }
  return NULL;
}

/* decode_string_read — scan for NUL-terminated string in buffer (0x11a8e0).
 * Source: data_encoding.c. */
__declspec(noinline) char *data_decode_string(data_encoding_state_t *state, unsigned short max_length)
{
  int offset;
  short scan_count;
  char *base;

  offset = state->offset;
  base = (char *)((int)state->buffer + offset);
  scan_count = 0;
  if (offset >= state->buffer_size)
    goto overflow;
  while (state->offset + (int)scan_count < state->buffer_size) {
    if (base[(int)scan_count] == '\0') {
      state->offset = (int)scan_count + 1 + offset;
      return base;
    }
    scan_count = scan_count + 1;
  }
overflow:
  state->overflow = 1;
  return NULL;
}

/* ========================================================================
 * Already-ported: data_packet_group_initialize (0x11a930)
 * ======================================================================== */

void data_packet_group_initialize(group_definition *group)
{
  short i;

  for (i = 0; i < group->packet_count; i++) {
    packet_entry *entry = &group->packets[i];

    if (entry->definition != NULL) {
      assert_halt_msg_at("packet->packet_class>=0 && packet->packet_class<group_definition->packet_class_count", "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x28, entry->packet_class >= 0 &&
                  entry->packet_class < group->packet_class_count);
      assert_halt_msg_at("packet->definition->size<=group_definition->maximum_decoded_packet_size", "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x29, entry->definition->size <=
                  group->maximum_decoded_packet_size);
      assert_halt_msg_at("packet->definition->size + sizeof(struct packet_header)<=group_definition->maximum_encoded_packet_size", "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x2a, (uint32_t)(entry->definition->size + sizeof(packet_header)) <=
                  (uint32_t)group->maximum_encoded_packet_size);
      data_packet_verify(entry->definition);
    }
  }
}

/* ========================================================================
 * data_packet_groups.c — Packet group decode
 * ======================================================================== */

/* decode_packet_group — decode an encoded packet from a group (0x11aa40).
 * Source: data_packet_groups.c lines 0x49-0x4d. */
bool data_packet_group_decode_packet(int group, void *decoded_packet, char *encoded_packet,
                  short *encoded_packet_size, short *packet_type,
                  short *packet_version, short expected_packet_class)
{
  char *header_ptr;
  int packets_array;
  int definition;
  char packet_type_byte;
  char *error_msg;

  error_msg = NULL;
  if (decoded_packet == NULL) {
    display_assert("decoded_packet",
                   "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x49, 1);
    system_exit(-1);
  }
  if (encoded_packet == NULL || encoded_packet_size == NULL) {
    display_assert("encoded_packet && encoded_packet_size",
                   "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x4a, 1);
    system_exit(-1);
  }
  if (packet_type == NULL || packet_version == NULL) {
    display_assert("packet_type && packet_version",
                   "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x4b, 1);
    system_exit(-1);
  }
  if (!(expected_packet_class >= 0 &&
        expected_packet_class < ((group_definition *)group)->packet_class_count)) {
    display_assert("expected_packet_class>=0 && "
                   "expected_packet_class<group_definition->packet_class_count",
                   "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x4d, 1);
    system_exit(-1);
  }
  if (*encoded_packet_size >= sizeof(char)) {
    header_ptr = (char *)(*encoded_packet_size - 1 + (int)encoded_packet);
    byte_swap_structures(packet_header_bs_def, header_ptr, 1);
    packet_type_byte = *header_ptr;
    if (packet_type_byte >= 0 &&
        (short)packet_type_byte < ((group_definition *)group)->packet_count) {
      packets_array = (int)((group_definition *)group)->packets;
      if (((packet_entry *)packets_array)[(int)packet_type_byte].packet_class ==
          expected_packet_class) {
        *encoded_packet_size = *encoded_packet_size - 1;
        definition = (int)((packet_entry *)packets_array)[(int)packet_type_byte].definition;
        if (definition == 0 ||
            data_packet_decode(definition, (int)encoded_packet,
                               *encoded_packet_size, (int)decoded_packet,
                               (unsigned short *)packet_version, 0)) {
          *packet_type = (short)*header_ptr;
        } else {
          error_msg = "got packet which wouldn't decode";
        }
      } else {
        error_msg = "got packet with mismatched class";
      }
    } else {
      error_msg = "got packet with bad type";
    }
  } else {
    error_msg = "got packet with no header";
  }
  s_last_decode_error = error_msg;
  return error_msg == NULL;
}

/* ========================================================================
 * data_packets.c — Packet field encode/decode
 * ======================================================================== */

/* decode_packet_fields — recursively decode packet fields (0x11b2a0).
 * Source: data_packets.c. */
void _data_packet_decode(int definition, data_encoding_state_t *decode_state, unsigned short version,
                  unsigned short *output, short *decoded_size_out,
                  short *field_defs, short *field_count_out)
{
  short *cur_field;
  unsigned short *cur_output;
  int raw_ptr;
  unsigned int var_count;
  unsigned int loop_count;

  cur_field = field_defs;
  cur_output = output;
  if (*cur_field == 9)
    goto loop_done;
  do {
    if ((short)version >= cur_field[2] &&
        ((short)version <= cur_field[3] || cur_field[3] == 0)) {
      switch (*cur_field) {
      case 1:
        raw_ptr = data_decode_memory(decode_state, cur_field[1], 1);
        if (raw_ptr != 0) {
          csmemcpy(cur_output, (void *)raw_ptr, (int)cur_field[1]);
        }
        break;
      case 2:
        raw_ptr = data_decode_memory(decode_state, cur_field[1], -2);
        if (raw_ptr != 0) {
          csmemcpy(cur_output, (void *)raw_ptr, (int)cur_field[1] << 1);
        }
        break;
      case 3:
        raw_ptr = data_decode_memory(decode_state, cur_field[1], -4);
        if (raw_ptr != 0) {
          csmemcpy(cur_output, (void *)raw_ptr, (int)cur_field[1] << 2);
        }
        break;
      case 4:
        raw_ptr = data_decode_memory(decode_state, cur_field[1], -8);
        if (raw_ptr != 0) {
          csmemcpy(cur_output, (void *)raw_ptr, (int)cur_field[1] << 3);
        }
        break;
      case 5:
        raw_ptr = (int)data_decode_string(decode_state, cur_field[1]);
        if (raw_ptr != 0) {
          csstrcpy((char *)cur_output, (const char *)raw_ptr);
        }
        break;
      case 6:
        var_count = data_decode_integer(decode_state, (int)cur_field[1]);
        *cur_output = (unsigned short)var_count;
        raw_ptr = data_decode_memory(decode_state, (short)var_count, 1);
        if (raw_ptr != 0) {
          csmemcpy(cur_output + 1, (void *)raw_ptr,
                   (int)(short)(unsigned short)var_count);
        }
        break;
      case 8:
        raw_ptr = data_decode_memory(decode_state, cur_field[1], 1);
        if (raw_ptr != 0) {
          csmemcpy(cur_output, (void *)raw_ptr, (int)cur_field[1]);
        }
        break;
      case 7: {
        unsigned short nested_count;
        unsigned short *nested_output;
        short element_size;
        short element_field_count;

        nested_count =
          (unsigned short)data_decode_integer(decode_state, (int)cur_field[1]);
        compute_packet_field_sizes((packet_definition *)definition, 0,
                                   cur_field + 5, &element_field_count);
        if ((short)nested_count < 0 || (short)nested_count > cur_field[1]) {
          nested_count = 0;
        }
        *cur_output = nested_count;
        nested_output = cur_output + 1;
        if (0 < (short)nested_count) {
          loop_count = (unsigned int)nested_count;
          do {
            _data_packet_decode(definition, decode_state, version, nested_output,
                         &element_size, cur_field + 5, 0);
            nested_output =
              (unsigned short *)((int)nested_output + (int)element_size);
            loop_count = loop_count - 1;
          } while (loop_count != 0);
        }
        cur_field = cur_field + (int)element_field_count * 5;
        break;
      }
      }
    } else {
      csmemset(cur_output, 0, (int)cur_field[4]);
    }
    cur_output = (unsigned short *)((int)cur_output + (int)cur_field[4]);
    cur_field = cur_field + 5;
  } while (*cur_field != 9);
loop_done:
  if (field_count_out != NULL) {
    int byte_diff;
    byte_diff = (int)cur_field - (int)field_defs;
    *field_count_out = (short)(byte_diff / 10) + 1;
  }
  if (decoded_size_out != NULL) {
    *decoded_size_out = (short)((int)cur_output - (int)output);
  }
}

/* network_messages.c — Network game packet group initialization.
 *
 * Corresponds to network_messages.obj.
 * initialize_network_game_packets at 0x12b640 is a thin wrapper that calls
 * data_packet_group_initialize (0x11a930) with the global
 * s_network_game_messages_group (0x323510).
 *
 * data_packet_group_initialize iterates the packet entries in the given
 * group, validates class bounds and size constraints, then calls
 * data_packet_verify on each non-NULL definition.
 *
 * data_packet_verify at 0x11b540 validates a single packet_definition:
 * checks non-NULL, size >= 0, version >= 0, name and fields non-NULL, then
 * (if not yet validated) computes total field sizes and confirms they match
 * the declared size. Sets validated = 1 after success.
 *
 * Original source: c:\halo\SOURCE\memory\data_packet_groups.c lines 0x28-0x2a
 * data_packet_verify source: c:\halo\SOURCE\memory\data_packets.c lines
 * 0x20-0x2b
 */

void data_packet_verify(packet_definition *def)
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

/* encode_packet — encode a data struct into a packet buffer (0x11b650).
 * Source: data_packets.c lines 0x3d-0x3f. */
bool data_packet_encode(int definition, short version, void *data, char *buffer,
                  short *buffer_size_out, short maximum_buffer_size)
{
  data_encoding_state_t encode_state;
  char version_byte;

  if (definition == 0) {
    display_assert("packet_definition",
                   "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x3d, 1);
    system_exit(-1);
  }
  if (buffer == 0 || buffer_size_out == NULL) {
    display_assert("buffer && buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x3e, 1);
    system_exit(-1);
  }
  if (maximum_buffer_size < 0) {
    display_assert("maximum_buffer_size>=0",
                   "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x3f, 1);
    system_exit(-1);
  }
  data_packet_verify((packet_definition *)definition);
  encode_state_new((int *)&encode_state, (int)buffer, (int)maximum_buffer_size);
  if (version == -1) {
    version = *(short *)(definition + 10);
  }
  if (0 < *(short *)(definition + 10)) {
    version_byte = (char)version;
    encode_raw_data((int *)&encode_state, (int)&version_byte, 1, 1);
  }
  _data_packet_encode((packet_definition *)definition, (int *)&encode_state, version,
                      data, NULL, (short *)*(int *)(definition + 0xc), NULL);
  *buffer_size_out = (short)encode_state.offset;
  return encode_state.overflow == '\0';
}

/* decode_packet — decode an encoded packet into a data struct (0x11b750).
 * Source: data_packets.c lines 0x5f-0x61. */
bool data_packet_decode(int definition, int encoded_packet, short encoded_packet_size,
                  int decoded_packet, unsigned short *version_out,
                  short *bytes_consumed_out)
{
  bool result;
  unsigned char version_byte;
  unsigned short version;
  data_encoding_state_t decode_state;

  result = false;
  if (encoded_packet == 0) {
    display_assert("encoded_packet", "c:\\halo\\SOURCE\\memory\\data_packets.c",
                   0x5f, 1);
    system_exit(-1);
  }
  if (decoded_packet == 0) {
    display_assert("decoded_packet", "c:\\halo\\SOURCE\\memory\\data_packets.c",
                   0x60, 1);
    system_exit(-1);
  }
  if (encoded_packet_size < 0) {
    display_assert("encoded_packet_size>=0",
                   "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x61, 1);
    system_exit(-1);
  }
  data_packet_verify((packet_definition *)definition);
  data_decode_new(&decode_state, (void *)encoded_packet, (int)encoded_packet_size);
  if (*(short *)(definition + 10) == 0) {
    version = 0;
  } else {
    version_byte = data_decode_byte(&decode_state);
    version = (unsigned short)version_byte;
  }
  if ((short)version <= *(short *)(definition + 10)) {
    _data_packet_decode(definition, &decode_state, version,
                 (unsigned short *)decoded_packet, 0,
                 *(short **)(definition + 0xc), 0);
    if (decode_state.overflow == '\0') {
      result = true;
    }
  }
  if (version_out != NULL) {
    *version_out = version;
  }
  if (bytes_consumed_out != NULL) {
    *bytes_consumed_out = (short)decode_state.offset;
  }
  return result;
}

/* ========================================================================
 * hashtable.c — Hash table implementation
 * ======================================================================== */

/* Initialize a hashtable header.
 * Source: c:\halo\SOURCE\memory\hashtable.c lines 0x29-0x2c (41-44).
 * Asserts: table non-NULL, key_size>0, element_size>0, 0<load_factor<=1.
 */
void hashtable_new(void *table, short key_size, short element_size,
                   float load_factor, int param_5, int param_6)
{
  hashtable_t *ht;

  if (table == NULL) {
    display_assert("table", "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x29, 1);
    system_exit(-1);
  }
  if (key_size <= 0) {
    display_assert("key_size>0", "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x2a,
                   1);
    system_exit(-1);
  }
  if (element_size <= 0) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\hashtable.c",
                   0x2b, 1);
    system_exit(-1);
  }
  if (!(load_factor > 0.0f && load_factor <= 1.0f)) {
    display_assert("load_factor>0 && load_factor<=1",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x2c, 1);
    system_exit(-1);
  }
  ht = (hashtable_t *)table;
  ht->load_factor = load_factor;
  ht->hash_proc = (hashtable_hash_proc_t)param_5;
  ht->compare_proc = (hashtable_compare_proc_t)param_6;
  ht->key_size = key_size;
  ht->element_size = element_size;
  ht->count = 0;
  ht->capacity_bits = (short)-1;
  array_new((int *)&ht->array, (int)key_size + (int)element_size);
  ht->bitmap = NULL;
}

/* hashtable_set_user_data — store a user-data/callback value at offset 0x0c
 * of the hashtable header (0x11b950).
 * Source: c:\halo\SOURCE\memory\hashtable.c
 */
void hashtable_set_user_data(void *table, int user_data)
{
  hashtable_t *ht = (hashtable_t *)table;
  ht->user_data = user_data;
}

#define hashtable_valid(t) \
  ((t) != (void *)0 && ((hashtable_t *)(t))->key_size > 0 && ((hashtable_t *)(t))->element_size > 0 && \
   ((hashtable_t *)(t))->load_factor > 0.0f && ((hashtable_t *)(t))->load_factor <= 1.0f && \
   (((hashtable_t *)(t))->capacity_bits == -1 || (1 << ((hashtable_t *)(t))->capacity_bits) == ((hashtable_t *)(t))->array.count))

void hashtable_dispose(short *table)
{
  hashtable_t *ht;

  if (!hashtable_valid(table)) {
    display_assert("hashtable_valid(table)",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x6e, 1);
    system_exit(-1);
  }
  ht = (hashtable_t *)table;
  array_dispose((int *)&ht->array);
  if (ht->bitmap != NULL) {
    debug_free(ht->bitmap, "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x74);
  }
}

/* hashtable_hash — default hash function using small primes (0x11ba00).
 * Source: hashtable.c. */
short default_hash_function(unsigned char *key, unsigned int key_size)
{
  int hash;
  short prime_index;
  int prime;

  hash = 0;
  prime_index = 0;
  if (key_size > 0) {
    do {
      if (prime_index == 0xf) {
        prime_index = 0;
        key_size = key_size - 0xf;
      }
      prime = hashtable_primes[prime_index];
      prime = (short)(prime * (unsigned short)*key);
      hash += prime;
      prime_index = prime_index + 1;
      key = key + 1;
    } while ((unsigned int)(int)prime_index < key_size);
  }
  return hash;
}

/* hashtable_find_slot — probe for a key in the table (0x11ba50).
 * Source: hashtable.c. Takes table via @ESI register arg. */
boolean hashtable_search(short *table_, void *key, unsigned short *slot_index_out)
{
  hashtable_t *table;
  short hash_val;
  short slot;
  short probe_count;
  int element_ptr;
  int found;

  table = (hashtable_t *)table_;

  probe_count = 0;
  if (table->hash_proc != NULL) {
    hash_val = (short)table->hash_proc(table->user_data, key);
  } else {
    hash_val =
      (short)default_hash_function((unsigned char *)key, (unsigned int)table->key_size);
  }
  slot = (short)((unsigned short)(table->array.count - 1) & (unsigned short)hash_val);
  while ((*(unsigned int *)((char *)table->bitmap + ((int)slot >> 5) * 4) &
          (1 << (slot & 0x1f))) != 0) {
    if (probe_count >= table->count) {
      *slot_index_out = (unsigned short)slot;
      return false;
    }
    if (table->compare_proc != NULL) {
      element_ptr =
        array_get_element((int *)&table->array, (int)slot, (int)table->element_size);
      found = table->compare_proc(table->user_data, (const void *)element_ptr, key);
    } else {
      element_ptr =
        array_get_element((int *)&table->array, (int)slot, (int)table->element_size);
      found = (csmemcmp((void *)element_ptr, key, (int)table->key_size) == 0);
    }
    if (found) {
      *slot_index_out = (unsigned short)slot;
      return true;
    }
    slot = (short)((int)(slot + 1) & (int)(table->array.count - 1));
    probe_count = probe_count + 1;
  }
  *slot_index_out = (unsigned short)slot;
  return false;
}

/* hashtable_find — look up a key, return pointer to value (0x11bb70).
 * Source: hashtable.c line 0x4d. */
int hashtable_get(short *table_, void *key)
{
  hashtable_t *table;
  short element_index;
  int result = 0;

  if (!hashtable_valid(table_)) {
    display_assert("hashtable_valid(table)",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x4d, 1);
    system_exit(-1);
  }
  table = (hashtable_t *)table_;
  if (table->count != 0 &&
      (char)hashtable_search((short *)table, key, (unsigned short *)&element_index)) {
    result =
      array_get_element((int *)&table->array, (int)element_index, (int)table->element_size) +
      table->key_size;
  }
  return result;
}

/* hashtable_remove — remove a key using backward-shift deletion (0x11bc20).
 * Source: hashtable.c line 0xc3. */
void hashtable_remove(short *table, void *key)
{
  unsigned int *bitmap_word;
  unsigned int bit_mask;
  hashtable_t *ht;
  char found;
  unsigned short next_slot;
  int next_element;
  unsigned short key_hash;
  short removed_slot;
  int cur_pos;

  if (!hashtable_valid(table)) {
    display_assert("hashtable_valid(table)",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0xc3, 1);
    system_exit(-1);
  }
  ht = (hashtable_t *)table;
  if (key == NULL) {
    display_assert("key", "c:\\halo\\SOURCE\\memory\\hashtable.c", 0xc4, 1);
    system_exit(-1);
  }
  found = (char)hashtable_search(table, key, (unsigned short *)&removed_slot);
  if (found != '\0') {
    next_slot = (unsigned short)((int)(removed_slot + 1) &
                                 (int)(unsigned short)(ht->array.count - 1));
    cur_pos = (int)(short)next_slot;
    bit_mask = *(unsigned int *)((char *)ht->bitmap + (cur_pos >> 5) * 4) &
               (1 << (next_slot & 0x1f));
    while (bit_mask != 0) {
      next_element =
        array_get_element((int *)&ht->array, cur_pos, (int)ht->element_size);
      if (ht->hash_proc == NULL) {
        key_hash = (unsigned short)default_hash_function((unsigned char *)next_element,
                                                (unsigned int)ht->key_size);
      } else {
        key_hash = (unsigned short)ht->hash_proc(ht->user_data, (const void *)next_element);
      }
      key_hash = (unsigned short)(ht->array.count - 1) & key_hash;
      if ((short)key_hash < (short)next_slot) {
        if ((short)removed_slot < (short)key_hash) {
          goto no_shift;
        }
        if ((short)removed_slot < (short)next_slot) {
          goto do_shift;
        }
      } else if ((short)key_hash > (short)next_slot) {
        if ((short)removed_slot >= (short)key_hash) {
          goto do_shift;
        }
        if ((short)removed_slot < (short)next_slot) {
          goto do_shift;
        }
      }
      goto no_shift;
    do_shift: {
      int src_element;
      int dst_element;
      src_element =
        array_get_element((int *)&ht->array, cur_pos, (int)ht->element_size);
      dst_element = array_get_element((int *)&ht->array, (int)removed_slot,
                                      (int)ht->element_size);
      csmemcpy((void *)dst_element, (void *)src_element, ht->array.element_size);
      removed_slot = (short)next_slot;
    }
    no_shift:
      (void)0;
      next_slot = (unsigned short)((int)(next_slot + 1) &
                                   (int)(unsigned short)(ht->array.count - 1));
      cur_pos = (int)(short)next_slot;
      bit_mask = *(unsigned int *)((char *)ht->bitmap + (cur_pos >> 5) * 4) &
                 (1 << (next_slot & 0x1f));
    }
    bitmap_word =
      (unsigned int *)((char *)ht->bitmap + ((int)removed_slot >> 5) * 4);
    *bitmap_word = *bitmap_word & ~(1 << (removed_slot & 0x1f));
  } else {
    display_assert("removing key not in hashtable",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0xe1, 1);
    system_exit(-1);
  }
}

/* hashtable_put — insert a key into a slot (0x11be10).
 * Source: hashtable.c line 0xf1. Takes table via @EAX register arg. */
int hashtable_capacious_put(short *table, void *key)
{
  unsigned int *bitmap_word;
  char found;
  int element_ptr;
  short slot;

  found = (char)hashtable_search(table, key, (unsigned short *)&slot);
  if (found != '\0') {
    display_assert("putting key already in hashtable",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0xf1, 1);
    system_exit(-1);
    return 0;
  }
  element_ptr =
    array_get_element((int *)&((hashtable_t *)table)->array, (int)slot,
                      (int)((hashtable_t *)table)->element_size);
  csmemcpy((void *)element_ptr, key, (int)((hashtable_t *)table)->key_size);
  bitmap_word =
    (unsigned int *)((char *)((hashtable_t *)table)->bitmap + ((int)slot >> 5) * 4);
  *bitmap_word = *bitmap_word | (1 << ((unsigned char)slot & 0x1f));
  ((hashtable_t *)table)->count = ((hashtable_t *)table)->count + 1;
  return ((hashtable_t *)table)->key_size + element_ptr;
}

/* hashtable_grow — resize the hashtable by adding capacity bits (0x11beb0).
 * Source: hashtable.c lines 0x86-0xb0. */
bool hashtable_grow(short *table, short growth_bits)
{
  short *array_hdr;
  int old_capacity_bits;
  short old_count;
  int old_bitmap;
  int old_array[3];
  int new_capacity;
  short *new_var;
  int bitmap_bytes;
  int new_bitmap;
  int i;
  short idx;
  int element_ptr;
  int dest_ptr;

  old_count = table[2];
  old_bitmap = *(int *)(table + 0xc);
  array_hdr = table + 0xe;
  old_capacity_bits = (int)table[3];
  new_var = table + 0x10;
  old_array[0] = *(int *)array_hdr;
  old_array[1] = *(int *)new_var;
  old_array[2] = *(int *)(table + 0x12);
  if (!hashtable_valid(table)) {
    display_assert("hashtable_valid(table)",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x86, 1);
    system_exit(-1);
  }
  if (growth_bits <= 0) {
    display_assert("growth_bits>0", "c:\\halo\\SOURCE\\memory\\hashtable.c",
                   0x87, 1);
    system_exit(-1);
  }
  if (table[3] + growth_bits >= 16) {
    display_assert("table->capacity_bits+growth_bits<SHORT_BITS",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x88, 1);
    system_exit(-1);
  }
  table[3] += growth_bits;
  new_capacity = (short)(1 << table[3]);
  bitmap_bytes = ((new_capacity + 31) >> 5) * 4;
  table[2] = 0;
  new_bitmap = (int)debug_malloc(bitmap_bytes, 0,
                                 "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x8f);
  *(int *)(table + 0xc) = new_bitmap;
  if (new_bitmap != 0) {
    array_reset((int *)array_hdr, *(int *)array_hdr);
    if (array_resize((int *)array_hdr, new_capacity)) {
      csmemset((void *)*(int *)(table + 0xc), 0, bitmap_bytes);
      if (0 < old_array[1]) {
        idx = 0;
        i = 0;
        do {
          if ((*(unsigned int *)(old_bitmap + (i >> 5) * 4) &
               (1 << (i & 0x1f))) != 0) {
            element_ptr = array_get_element(old_array, i, old_array[0]);
            dest_ptr = hashtable_capacious_put(table, (void *)element_ptr);
            csmemcpy((void *)dest_ptr, (void *)(element_ptr + *table),
                     (int)table[1]);
          }
          idx = idx + 1;
          i = (int)idx;
        } while (i < old_array[1]);
      }
      if (old_bitmap != 0) {
        debug_free((void *)old_bitmap, "c:\\halo\\SOURCE\\memory\\hashtable.c",
                   0xa8);
      }
      array_dispose(old_array);
      return 1;
    }
    debug_free((void *)*(int *)(table + 0xc),
               "c:\\halo\\SOURCE\\memory\\hashtable.c", 0xb0);
  }
  table[3] = (short)old_capacity_bits;
  table[2] = old_count;
  *(int *)(table + 0xc) = old_bitmap;
  *(int *)array_hdr = old_array[0];
  *(int *)new_var = old_array[1];
  *(int *)(table + 0x12) = old_array[2];
  return 0;
}

/* hashtable_insert — validate, grow if needed, then put (0x11c0f0).
 * Source: hashtable.c line 0x5d. */
int hashtable_put(short *table, void *key)
{
  hashtable_t *ht;
  char grew;
  int result;

  if (!hashtable_valid(table)) {
    display_assert("hashtable_valid(table)",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x5d, 1);
    system_exit(-1);
  }
  ht = (hashtable_t *)table;
  if ((ht->capacity_bits == -1) ||
      ((float)(int)ht->count >=
       (float)ht->array.count * ht->load_factor)) {
    grew = (char)hashtable_grow(table, (short)((ht->capacity_bits == -1) + 1));
    if (grew == '\0') {
      return 0;
    }
  }
  result = hashtable_capacious_put(table, key);
  return result;
}

/* ========================================================================
 * lra_cache.c — LRU/LRA cache implementation
 * Original source: c:\halo\SOURCE\memory\lra_cache.c
 *
 * Cache struct (0x3c bytes):
 *   +0x00 char[0x20]  name (null-terminated, max 0x1f chars)
 *   +0x20 int         size (total buffer size)
 *   +0x24 void*       base_address (buffer pointer)
 *   +0x28 byte        owns_buffer
 *   +0x2c void*       head_block
 *   +0x30 void(*)(void*,int)  lock_proc
 *   +0x34 void(*)(void*)      unlock_proc
 *   +0x38 int         magic = 0x6c726163 ("lrac")
 *
 * Block header (0x10 bytes, prepended to user data):
 *   +0x00 int         user_data
 *   +0x04 int         flags (bit 0 = in_use, bit 1 = freed/unlocked)
 *   +0x08 int         size
 *   +0x0c void*       next_block
 * ======================================================================== */

#define LRA_CACHE_MAGIC 0x6c726163 /* 'lrac' */
#define LRA_BLOCK_MAGIC 0x41626c68 /* 'hlbA' */

typedef struct lra_block {
  void *data;                  /* +0x00 */
  unsigned int magic_flags;    /* +0x04 */
  int block_size;              /* +0x08 */
  struct lra_block *next;      /* +0x0c */
} lra_block_t;

typedef struct lra_cache {
  char name[32];               /* +0x00..0x1f */
  int size;                    /* +0x20 */
  lra_block_t *base_address;   /* +0x24 */
  char owns_buffer;            /* +0x28 */
  char pad_29[3];              /* +0x29..0x2b */
  lra_block_t *head;           /* +0x2c */
  void (*lock_proc)(void *, int); /* +0x30 */
  void (*unlock_proc)(void *);    /* +0x34 */
  int magic;                   /* +0x38 */
} lra_cache_t;

/* lra_cache_is_active — check if cache has active blocks (0x11c1b0). */
int lra_full(int cache)
{
  lra_cache_t *c = (lra_cache_t *)cache;
  if (c->head != NULL && c->head->next != NULL) {
    return 1;
  }
  return 0;
}

/* lra_cache_default_lock — default lock callback (0x11c1d0). */
void lra_default_new_block_proc(int *ptr, int user_data)
{
  *ptr = user_data;
}

/* lra_cache_default_unlock — default unlock callback (0x11c1e0). */
void lra_default_purge_block_proc(int *ptr)
{
  *ptr = 0;
}

/* lra_block_delete — if the block is not yet deleted, run the cache's delete
 * callback on it, then clear the locked bit and set the deleted bit (0x11c1f0).
 * Register args: @ECX = cache, @ESI = block header.  No references to the
 * out-of-line copy exist; the same body is inlined into its callers. */
void lra_block_delete(int cache, int block)
{
  lra_cache_t *c = (lra_cache_t *)cache;
  lra_block_t *b = (lra_block_t *)block;

  if ((b->magic_flags & 2) == 0) {
    c->unlock_proc(b->data);
    b->magic_flags = (b->magic_flags & ~1U) | 2;
  }
}

/* lra_cache_validate_block — validate a block header (0x11c210).
 * Register args: @EBX = cache, @ESI = block header. */
void verify_lra_cache_block(int cache, int block)
{
  lra_cache_t *c;
  lra_block_t *b;
  unsigned int cache_size;
  int block_size;
  int block_offset;
  int next_offset;

  c = (lra_cache_t *)cache;
  b = (lra_block_t *)block;
  if ((((b->magic_flags & 0xfffffffc) == LRA_BLOCK_MAGIC) &&
       (block_size = b->block_size, block_size >= 0)) &&
      (cache_size = (unsigned int)c->size,
       block_size < (int)cache_size)) {
    block_offset = (int)b - (int)c->base_address;
    if ((block_offset >= 0) && (block_size + block_offset <= (int)cache_size)) {
      if (b->next == NULL) {
        next_offset = 0;
      } else {
        next_offset = (int)b->next - (int)c->base_address;
        if (next_offset < 0)
          goto corrupt;
      }
      if (next_offset + 0x10U <= cache_size) {
        return;
      }
    }
  }
corrupt:
  display_assert(csprintf(error_string_buffer,
                          "lra cache %s @%p block @%p appears to be corrupt",
                          c->name, (void *)c, (void *)b),
                 "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x18e, 1);
  system_exit(-1);
}

/* lra_cache_validate — validate cache struct integrity (0x11c290).
 * Register arg: @EAX = cache. */
void verify_lra_cache(int cache)
{
  lra_cache_t *c;

  if (cache == 0) {
    display_assert("cache", "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x198, 1);
    system_exit(-1);
  }
  c = (lra_cache_t *)cache;
  if (((c->magic != LRA_CACHE_MAGIC) ||
       (c->base_address == NULL)) ||
      (c->size < 0)) {
    display_assert(csprintf(error_string_buffer,
                            "lra cache %s @%p appears to be corrupt",
                            c->name, (void *)c),
                   "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x1a2, 1);
    system_exit(-1);
  }
  if (c->head != NULL) {
    verify_lra_cache_block(cache, (int)c->head);
  }
}

/* lra_cache_new — allocate and initialize a cache (0x11c310).
 * Source: lra_cache.c lines 0x56-0x7e. */
int lra_new(const char *name, int size, void (*lock_proc)(void *, int),
                 void (*unlock_proc)(void *), void *base_address)
{
  lra_cache_t *c;
  char owns_buffer;

  c = (lra_cache_t *)debug_malloc(sizeof(lra_cache_t), 0,
                                  "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x56);
  if (size < 0) {
    display_assert("size>=0", "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x58, 1);
    system_exit(-1);
  }
  if (lock_proc == NULL || unlock_proc == NULL) {
    lock_proc = (void (*)(void *, int))lra_default_new_block_proc;
    unlock_proc = (void (*)(void *))lra_default_purge_block_proc;
  }
  if (c != NULL) {
    owns_buffer = 0;
    if (base_address == NULL) {
      base_address =
        debug_malloc(size, 0, "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x66);
      owns_buffer = 1;
      if (base_address == NULL) {
        goto cleanup;
      }
    }
    if (((unsigned int)base_address & 3) != 0) {
      display_assert("!((long)base_address&3)",
                     "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x6b, 1);
      system_exit(-1);
    }
    csmemset((void *)c, 0, sizeof(lra_cache_t));
    csstrncpy(c->name, name, 0x1f);
    c->name[0x1f] = 0;
    c->size = size;
    c->base_address = (lra_block_t *)base_address;
    c->head = NULL;
    c->magic = LRA_CACHE_MAGIC;
    c->owns_buffer = owns_buffer;
    c->lock_proc = lock_proc;
    c->unlock_proc = unlock_proc;
    verify_lra_cache((int)c);
  }
  return (int)c;
cleanup:
  debug_free((void *)c, "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x7e);
  return 0;
}

/* lra_cache_dispose — free a cache and its buffer (0x11c430).
 * Source: lra_cache.c lines 0x8c-0x8d. */
void lra_dispose(int cache)
{
  lra_cache_t *c;

  c = (lra_cache_t *)cache;
  verify_lra_cache(cache);
  if (c->owns_buffer != '\0') {
    debug_free((void *)c->base_address,
               "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x8c);
  }
  debug_free((void *)c, "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x8d);
}

/* lra_cache_flush — unlock all blocks and clear the list (0x11c480).
 * Source: lra_cache.c. */
void lra_flush(int cache)
{
  lra_cache_t *c;
  lra_block_t *block;

  c = (lra_cache_t *)cache;
  verify_lra_cache(cache);
  if (c->head != NULL && (block = c->base_address) != NULL) {
    do {
      if ((block->magic_flags & 2) == 0) {
        c->unlock_proc(block->data);
        block->magic_flags = (block->magic_flags & ~1) | 2;
      }
      block = block->next;
    } while (block != NULL);
    c->head = block;
    return;
  }
  c->head = NULL;
}

/* lra_cache_unlock_block — release a specific block (0x11c4d0).
 * Source: lra_cache.c line 0x11a. */
void lra_free(int cache, void *pointer)
{
  lra_cache_t *c;
  lra_block_t *block;

  if (pointer == NULL) {
    display_assert("pointer", "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x11a,
                   1);
    system_exit(-1);
  }
  c = (lra_cache_t *)cache;
  block = (lra_block_t *)((char *)pointer - sizeof(lra_block_t));
  verify_lra_cache(cache);
  verify_lra_cache_block(cache, (int)block);
  if ((block->magic_flags & 2) == 0) {
    c->unlock_proc(block->data);
    block->magic_flags = (block->magic_flags & ~1U) | 2;
  }
}

/* ========================================================================
 * Already-ported: initialize_network_game_packets (0x12b640)
 * ======================================================================== */

void initialize_network_game_packets(void)
{
  data_packet_group_initialize(&s_network_game_messages_group);
}

/* Static 0x604-byte output buffer for create_network_game_message (0x46e8d0).
 * Passed as the pre-allocated destination to create_message(); only one caller
 * exists so this is safe as a module-level static. */
static char s_network_game_message_buffer[0x604];

enum network_game_message_type {
  _network_game_message_client_broadcast_game_search = 0,
  _network_game_message_client_ping = 1,
  _network_game_message_server_game_advertise = 2,
  _network_game_message_server_pong = 3,
  _network_game_message_server_machine_accepted = 4,
  _network_game_message_server_machine_rejected = 5,
  _network_game_message_server_game_settings_update = 6,
  _network_game_message_server_pregame_countdown = 7,
  _network_game_message_server_begin_game = 8,
  _network_game_message_server_graceful_game_exit_pregame = 9,
  _network_game_message_server_pregame_keep_alive = 10,
  _network_game_message_server_postgame_keep_alive = 11,
  _network_game_message_client_join_game_request = 12,
  _network_game_message_client_add_player_request_pregame = 13,
  _network_game_message_client_remove_player_request_pregame = 14,
  _network_game_message_client_settings_request = 15,
  _network_game_message_client_player_settings_request = 16,
  _network_game_message_client_game_start_request = 17,
  _network_game_message_client_graceful_game_exit_pregame = 18,
  _network_game_message_client_map_is_precached_pregame = 19,
  _network_game_message_server_game_update = 20,
  _network_game_message_server_add_player_ingame = 21,
  _network_game_message_server_remove_player_ingame = 22,
  _network_game_message_server_game_over = 23,
  _network_game_message_client_loaded = 24,
  _network_game_message_client_game_update = 25,
  _network_game_message_client_add_player_request_ingame = 26,
  _network_game_message_client_remove_player_request_ingame = 27,
  _network_game_message_client_host_crashed_cry_for_help = 28,
  _network_game_message_client_join_new_host = 29,
  _network_game_message_server_switch_to_pregame = 30,
  _network_game_message_server_graceful_game_exit_postgame = 31,
  _network_game_message_client_remove_player_request_postgame = 32,
  _network_game_message_client_switch_to_pregame = 33,
  _network_game_message_client_graceful_game_exit_postgame = 34,
  NUMBER_OF_NETWORK_GAME_MESSAGE_TYPES = 35
};

/* create_network_game_message — validate, encode and wrap a typed network
 * game message struct into a transmittable message packet (0x12b700).
 *
 * Validates that message_struct_size matches the expected size for the given
 * type, encodes the struct into a 1536-byte stack buffer using the global
 * packet group definition, then wraps the encoded bytes in a message header
 * and returns a pointer to the resulting message, or NULL on failure.
 */
void *create_network_game_message(int type, void *data,
                                  int16_t message_struct_size)
{
  char encoded_buf[0x600];
  int32_t encoded_size;

  encoded_size = 0x600;

#define CHECK_MSG_SIZE(sz, msg_str, line) \
  if (message_struct_size != (sz)) { \
    display_assert((msg_str), "c:\\halo\\SOURCE\\networking\\network_messages.c", (line), 1); \
    system_exit(-1); \
  } break

  switch ((int16_t)type) {
  case _network_game_message_client_broadcast_game_search:
    CHECK_MSG_SIZE(0xc, "message_struct_size==sizeof(message_client_broadcast_game_search)", 0xa0);
  case _network_game_message_client_ping:
    CHECK_MSG_SIZE(8, "message_struct_size==sizeof(message_client_ping)", 0xa1);
  case _network_game_message_server_game_advertise:
    CHECK_MSG_SIZE(0x114, "message_struct_size==sizeof(message_server_game_advertise)", 0xa4);
  case _network_game_message_server_pong:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_pong)", 0xa5);
  case _network_game_message_server_machine_accepted:
    CHECK_MSG_SIZE(8, "message_struct_size==sizeof(message_server_machine_accepted)", 0xa8);
  case _network_game_message_server_machine_rejected:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_server_machine_rejected)", 0xa9);
  case _network_game_message_server_game_settings_update:
    CHECK_MSG_SIZE(0x434, "message_struct_size==sizeof(message_server_game_settings_update)", 0xaa);
  case _network_game_message_server_pregame_countdown:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_server_pregame_countdown)", 0xab);
  case _network_game_message_server_pregame_keep_alive:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_server_pregame_keep_alive)", 0xac);
  case _network_game_message_server_begin_game:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_begin_game)", 0xad);
  case _network_game_message_server_graceful_game_exit_pregame:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_graceful_game_exit_pregame)", 0xae);
  case _network_game_message_server_postgame_keep_alive:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_server_postgame_keep_alive)", 0xb1);
  case _network_game_message_client_join_game_request:
    CHECK_MSG_SIZE(0x50, "message_struct_size==sizeof(message_client_join_game_request)", 0xb4);
  case _network_game_message_client_add_player_request_pregame:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_add_player_request_pregame)", 0xb5);
  case _network_game_message_client_remove_player_request_pregame:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_remove_player_request_pregame)", 0xb6);
  case _network_game_message_client_settings_request:
    CHECK_MSG_SIZE(0x44, "message_struct_size==sizeof(message_client_settings_request)", 0xb7);
  case _network_game_message_client_player_settings_request:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_player_settings_request)", 0xb8);
  case _network_game_message_client_game_start_request:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_client_game_start_request)", 0xb9);
  case _network_game_message_client_graceful_game_exit_pregame:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_client_graceful_game_exit_pregame)", 0xba);
  case _network_game_message_client_map_is_precached_pregame:
    CHECK_MSG_SIZE(0x100, "message_struct_size==sizeof(message_client_map_is_precached_pregame)", 0xbb);
  case _network_game_message_server_game_update:
    CHECK_MSG_SIZE(0x210, "message_struct_size==sizeof(message_server_game_update)", 0xbe);
  case _network_game_message_server_add_player_ingame:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_server_add_player_ingame)", 0xbf);
  case _network_game_message_server_remove_player_ingame:
    CHECK_MSG_SIZE(0x24, "message_struct_size==sizeof(message_server_remove_player_ingame)", 0xc0);
  case _network_game_message_server_game_over:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_game_over)", 0xc1);
  case _network_game_message_client_loaded:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_client_loaded)", 0xc4);
  case _network_game_message_client_game_update:
    CHECK_MSG_SIZE(0x88, "message_struct_size==sizeof(message_client_game_update)", 0xc5);
  case _network_game_message_client_add_player_request_ingame:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_add_player_request_ingame)", 0xc6);
  case _network_game_message_client_remove_player_request_ingame:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_remove_player_request_ingame)", 0xc7);
  case _network_game_message_client_host_crashed_cry_for_help:
    CHECK_MSG_SIZE(0x10, "message_struct_size==sizeof(message_client_host_crashed_cry_for_help)", 0xc9);
  case _network_game_message_client_join_new_host:
    CHECK_MSG_SIZE(0x10, "message_struct_size==sizeof(message_client_join_new_host)", 0xca);
  case _network_game_message_server_switch_to_pregame:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_switch_to_pregame)", 0xcd);
  case _network_game_message_server_graceful_game_exit_postgame:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_graceful_game_exit_postgame)", 0xce);
  case _network_game_message_client_remove_player_request_postgame:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_remove_player_request_postgame)", 0xd1);
  case _network_game_message_client_switch_to_pregame:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_client_switch_to_pregame)", 0xd2);
  case _network_game_message_client_graceful_game_exit_postgame:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_client_graceful_game_exit_postgame)", 0xd3);
  default:
    display_assert("unknown network game message structure type",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c",
                   0xd5, 1);
    system_exit(-1);
    break;
  }
#undef CHECK_MSG_SIZE
  if (!(data != NULL && (short)encoded_size > 0)) {
    display_assert("message_struct && encoded_message && encoded_message_size "
                   "&& (*encoded_message_size>0)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0x161,
                   1);
    system_exit(-1);
  }

  /* encode_packet_group's size parameter is short * (16-bit accesses at
   * 0x11abbc/0x11ac67); `encoded_size` is a dword slot here, matching the
   * original's own dword store/load of the same variable. */
  if (encode_packet_group(&s_network_game_messages_group, data, encoded_buf,
                          (short *)&encoded_size, type, 1)) {
    void *msg =
      (void *)create_message(3, (int)encoded_buf, encoded_size,
                             (int)s_network_game_message_buffer, 0x604);
    if (msg == NULL) {
      network_event("create_message() failed");
    }
    return msg;
  }

  network_event("encode_network_game_message() failed");
  return NULL;
}

void network_event(const char *format, ...)
{
  va_list args;

  if (format == NULL) {
    display_assert("format", "c:\\halo\\SOURCE\\networking\\network_messages.c",
                   0x14b, 1);
    system_exit(-1);
  }

  va_start(args, format);
  crt_vsnprintf(error_string_buffer, 0xff, format, args);
  va_end(args);

  error(3, error_string_buffer);
}

bool encode_network_game_message(void *message_struct, char *encoded_message,
                                 int16_t *encoded_message_size, int16_t type,
                                 int one)
{
  if (message_struct == NULL || encoded_message == NULL ||
      encoded_message_size == NULL || !(*encoded_message_size > 0)) {
    display_assert("message_struct && encoded_message && encoded_message_size "
                   "&& (*encoded_message_size>0)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0x161,
                   1);
    system_exit(-1);
  }

  return encode_packet_group(&s_network_game_messages_group, message_struct,
                             encoded_message, encoded_message_size, type,
                             (short)one);
}
