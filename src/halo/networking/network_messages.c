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

#define byte_swap_raw ((void (*)(void *, int, int))0x118620)

#define byte_swap_structures ((void (*)(void *, void *, int))0x118be0)

#define encode_state_new ((void (*)(int *, int, int))0x119c50)

#define encode_raw_data ((int (*)(int *, int, short, int))0x119cc0)

#define csstrcpy ((char *(*)(char *, const char *))0x8dff0)

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
bool data_encode_string(int *state, const char *source, short max_length)
{
  short string_length;
  int dest;

  string_length = strnlen(source, (int)max_length);
  dest = *state + state[1];
  if (state[2] < (int)string_length + 1 + state[1]) {
    display_assert("state->offset+string_length+1<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0xb6, 1);
    system_exit(-1);
  }
  if ((state[1] + 1 + (int)string_length <= state[2]) &&
      ((char)state[3] == '\0')) {
    csstrncpy((char *)dest, source, (int)string_length);
    *(char *)((int)string_length + dest) = 0;
    state[1] = state[1] + (int)string_length + 1;
    return (char)state[3] == '\0';
  }
  *(char *)(state + 3) = 1;
  return (char)state[3] == '\0';
}

/* decode_state_new — initialize a decode state struct (0x11a2d0).
 * Source: data_encoding.c line 0xcc. */
void data_decode_new(int *state, void *buffer, int buffer_size)
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
  csmemset(state, 0, 0x10);
  *state = (int)buffer;
  state[2] = buffer_size;
}

/* decode_structures — byte-swap structures in-place in the buffer (0x11a340).
 * Source: data_encoding.c line 0xde. */
int data_decode_structures(int *state, short count, void *bs_definition)
{
  short total_size;
  int result;

  if (((state == NULL) || (*state == 0) || (state[1] < 0)) ||
      (state[2] < state[1])) {
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
  total_size = *(short *)((char *)bs_definition + 4) * count;
  if (((int)total_size + state[1] <= state[2]) && ((char)state[3] == '\0')) {
    result = *state + state[1];
    if (total_size != 0) {
      byte_swap_structures(bs_definition, (void *)result, (int)count);
      state[1] = state[1] + (int)total_size;
    }
    return result;
  }
  *(char *)(state + 3) = 1;
  return 0;
}

/* decode_raw_data — byte-swap raw elements in the buffer (0x11a430).
 * Source: data_encoding.c line 0x100. */
__declspec(noinline) int data_decode_memory(int *state, short count, int element_size)
{
  int byte_count;
  int result;

  if (((state == NULL) || (*state == 0) || (state[1] < 0)) ||
      (state[2] < state[1])) {
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
  case -8:
    byte_count = (int)count << 3;
    break;
  case -4:
    byte_count = (int)count << 2;
    break;
  case -2:
    byte_count = (int)count << 1;
    break;
  default:
    display_assert(NULL, "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x109, 1);
    system_exit(-1);
    byte_count = (int)count;
    break;
  }
  if ((state[1] + byte_count <= state[2]) && ((char)state[3] == '\0')) {
    result = *state + state[1];
    if (element_size != 1) {
      byte_swap_raw((void *)result, (int)count, element_size);
    }
    state[1] = state[1] + byte_count;
    return result;
  }
  *(char *)(state + 3) = 1;
  return 0;
}

/* decode_byte — read a single byte from the decode buffer (0x11a560).
 * Source: data_encoding.c. */
__declspec(noinline) unsigned char data_decode_byte(int *state)
{
  int new_offset;
  unsigned char *ptr;

  if (((state == NULL) || (*state == 0) || (state[1] < 0)) ||
      (state[2] < state[1])) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x100, 1);
    system_exit(-1);
  }
  new_offset = state[1] + 1;
  if ((state[2] < new_offset) || ((char)state[3] != '\0')) {
    *(unsigned char *)(state + 3) = 1;
  } else {
    ptr = (unsigned char *)(*state + state[1]);
    state[1] = new_offset;
    if (ptr != NULL) {
      return *ptr;
    }
  }
  return 0;
}

/* decode_short — read and byte-swap a 16-bit value from the buffer (0x11a5d0).
 * Source: data_encoding.c. */
short data_decode_short(int *state)
{
  short *ptr;

  if (((state == NULL) || (*state == 0) || (state[1] < 0)) ||
      (state[2] < state[1])) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x100, 1);
    system_exit(-1);
  }
  if ((state[2] < state[1] + 2) || ((char)state[3] != '\0')) {
    *(unsigned char *)(state + 3) = 1;
  } else {
    ptr = (short *)(*state + state[1]);
    byte_swap_raw(ptr, 1, -2);
    state[1] = state[1] + 2;
    if (ptr != NULL) {
      return *ptr;
    }
  }
  return 0;
}

/* decode_long — read and byte-swap a 32-bit value from the buffer (0x11a650).
 * Source: data_encoding.c. */
int data_decode_long(int *state)
{
  int *ptr;

  if (((state == NULL) || (*state == 0) || (state[1] < 0)) ||
      (state[2] < state[1])) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<=state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x100, 1);
    system_exit(-1);
  }
  if ((state[2] < state[1] + 4) || ((char)state[3] != '\0')) {
    *(unsigned char *)(state + 3) = 1;
  } else {
    ptr = (int *)(*state + state[1]);
    byte_swap_raw(ptr, 1, -4);
    state[1] = state[1] + 4;
    if (ptr != NULL) {
      return *ptr;
    }
  }
  return 0;
}

/* decode_long_long — read and byte-swap an 8-byte value (0x11a6d0).
 * Source: data_encoding.c. Wrapper around decode_raw_data(state, 1, -8). */
int64_t data_decode_int64(int *state)
{
  int64_t *ptr;

  ptr = (int64_t *)data_decode_memory(state, 1, -8);
  if (ptr != NULL) {
    return *ptr;
  }
  return 0;
}

/* decode_value — width-adaptive read based on maximum_value (0x11a700).
 * Source: data_encoding.c line 0x141. */
__declspec(noinline) unsigned int data_decode_integer(int *state, int maximum_value)
{
  if (maximum_value < 1) {
    display_assert("maximum_value>0",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x141, 1);
    system_exit(-1);
  }
  if (maximum_value < 0x100) {
    return (unsigned int)data_decode_byte(state) & 0xff;
  }
  if (maximum_value < 0x10000) {
    return (unsigned int)(unsigned short)data_decode_short(state);
  }
  return (unsigned int)data_decode_long(state);
}

/* decode_element_array — read count + structures from buffer (0x11a770).
 * Source: data_encoding.c line 0x15c. */
void *data_decode_array(int *state, int element_size_type,
                   unsigned int *element_count_ref, int maximum_element_count,
                   void *bs_definition)
{
  short sVar1;
  unsigned int count;
  void *result;

  if (((state == NULL) || (*state == 0) || (state[1] < 0)) ||
      (state[2] <= state[1])) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x15c, 1);
    system_exit(-1);
  }
  if (element_count_ref == NULL) {
    display_assert("element_count_reference",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x15d, 1);
    system_exit(-1);
  }
  if (maximum_element_count < 1) {
    display_assert("maximum_element_count>0",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x15e, 1);
    system_exit(-1);
  }
  if (bs_definition == NULL) {
    display_assert("bs_definition", "c:\\halo\\SOURCE\\memory\\data_encoding.c",
                   0x15f, 1);
    system_exit(-1);
  }
  switch (element_size_type) {
  case 1:
    count = data_decode_byte(state) & 0xff;
    break;
  case -2:
    sVar1 = data_decode_short(state);
    count = (unsigned int)(int)sVar1;
    break;
  case -4:
    count = (unsigned int)data_decode_long(state);
    break;
  case -8:
    count = (unsigned int)(int)data_decode_int64(state);
    break;
  default:
    display_assert(NULL, "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x172, 1);
    system_exit(-1);
    count = *element_count_ref;
    break;
  }
  if (((char)state[3] == '\0') && ((int)count >= 0) &&
      ((int)count <= maximum_element_count)) {
    *element_count_ref = count;
    result = (void *)data_decode_structures(state, (short)count, bs_definition);
    return result;
  }
  return NULL;
}

/* decode_string_read — scan for NUL-terminated string in buffer (0x11a8e0).
 * Source: data_encoding.c. */
__declspec(noinline) char *data_decode_string(int *state, unsigned short max_length)
{
  int offset;
  short scan_count;
  char *base;

  offset = state[1];
  base = (char *)(*state + offset);
  scan_count = 0;
  if (offset >= state[2])
    goto overflow;
  while (state[1] + (int)scan_count < state[2]) {
    if (base[(int)scan_count] == '\0') {
      state[1] = (int)scan_count + 1 + offset;
      return base;
    }
    scan_count = scan_count + 1;
  }
overflow:
  *(unsigned char *)(state + 3) = 1;
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
      assert_halt(entry->packet_class >= 0 &&
                  entry->packet_class < group->packet_class_count);
      assert_halt(entry->definition->size <=
                  group->maximum_decoded_packet_size);
      assert_halt((uint32_t)(entry->definition->size + sizeof(packet_header)) <=
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
  if (expected_packet_class < 0 ||
      *(short *)(group + 6) <= expected_packet_class) {
    display_assert("expected_packet_class>=0 && "
                   "expected_packet_class<group_definition->packet_class_count",
                   "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x4d, 1);
    system_exit(-1);
  }
  if (*encoded_packet_size == 0) {
    error_msg = "got packet with no header";
  } else {
    header_ptr = (char *)(*encoded_packet_size - 1 + (int)encoded_packet);
    byte_swap_structures(packet_header_bs_def, header_ptr, 1);
    packet_type_byte = *header_ptr;
    if (packet_type_byte < 0 ||
        *(short *)(group + 4) <= (short)packet_type_byte) {
      error_msg = "got packet with bad type";
    } else {
      packets_array = *(int *)(group + 0x10);
      if (*(short *)(packets_array + (int)packet_type_byte * 8) ==
          expected_packet_class) {
        *encoded_packet_size = *encoded_packet_size - 1;
        definition = *(int *)(packets_array + (int)packet_type_byte * 8 + 4);
        if (definition != 0) {
          if (!data_packet_decode(definition, (int)encoded_packet,
                            *encoded_packet_size, (int)decoded_packet,
                            (unsigned short *)packet_version, 0)) {
            error_msg = "got packet which wouldn't decode";
            goto done;
          }
        }
        *packet_type = (short)*header_ptr;
      } else {
        error_msg = "got packet with mismatched class";
      }
    }
  }
done:
  s_last_decode_error = error_msg;
  return error_msg == NULL;
}

/* ========================================================================
 * data_packets.c — Packet field encode/decode
 * ======================================================================== */

/* decode_packet_fields — recursively decode packet fields (0x11b2a0).
 * Source: data_packets.c. */
void _data_packet_decode(int definition, int *decode_state, unsigned short version,
                  unsigned short *output, short *decoded_size_out,
                  short *field_defs, short *field_count_out)
{
  short *cur_field;
  unsigned short *cur_output;
  int raw_ptr;
  unsigned int var_count;
  short local_c[2];
  short local_8[2];
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
      case 7: {
        unsigned short nested_count;
        unsigned short *nested_output;

        nested_count =
          (unsigned short)data_decode_integer(decode_state, (int)cur_field[1]);
        compute_packet_field_sizes((packet_definition *)definition, 0,
                                   cur_field + 5, local_c);
        if ((short)nested_count < 0 || cur_field[1] < (short)nested_count) {
          nested_count = 0;
        }
        *cur_output = nested_count;
        nested_output = cur_output + 1;
        if (0 < (short)nested_count) {
          loop_count = (unsigned int)nested_count;
          do {
            _data_packet_decode(definition, decode_state, version, nested_output,
                         local_8, cur_field + 5, 0);
            nested_output =
              (unsigned short *)((int)nested_output + (int)local_8[0]);
            loop_count = loop_count - 1;
          } while (loop_count != 0);
        }
        cur_field = cur_field + (int)local_c[0] * 5;
        break;
      }
      case 8:
        raw_ptr = data_decode_memory(decode_state, cur_field[1], 1);
        if (raw_ptr != 0) {
          csmemcpy(cur_output, (void *)raw_ptr, (int)cur_field[1]);
        }
        break;
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
  int encode_state[4];
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
  encode_state_new(encode_state, (int)buffer, (int)maximum_buffer_size);
  if (version == -1) {
    version = *(short *)(definition + 10);
  }
  if (0 < *(short *)(definition + 10)) {
    version_byte = (char)version;
    encode_raw_data(encode_state, (int)&version_byte, 1, 1);
  }
  _data_packet_encode((packet_definition *)definition, encode_state, version,
                      data, NULL, (short *)*(int *)(definition + 0xc), NULL);
  *buffer_size_out = (short)encode_state[1];
  return (char)encode_state[3] == '\0';
}

/* decode_packet — decode an encoded packet into a data struct (0x11b750).
 * Source: data_packets.c lines 0x5f-0x61. */
bool data_packet_decode(int definition, int encoded_packet, short encoded_packet_size,
                  int decoded_packet, unsigned short *version_out,
                  short *bytes_consumed_out)
{
  unsigned char version_byte;
  unsigned short version;
  bool result;
  int decode_state[4];

  result = 0;
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
  data_decode_new(decode_state, (void *)encoded_packet, (int)encoded_packet_size);
  if (*(short *)(definition + 10) == 0) {
    version = 0;
  } else {
    version_byte = data_decode_byte(decode_state);
    version = (unsigned short)version_byte;
  }
  if ((short)version <= *(short *)(definition + 10)) {
    _data_packet_decode(definition, decode_state, version,
                 (unsigned short *)decoded_packet, 0,
                 *(short **)(definition + 0xc), 0);
    result = 1;
    if ((char)decode_state[3] == '\0')
      goto done;
  }
  result = 0;
done:
  if (version_out != NULL) {
    *version_out = version;
  }
  if (bytes_consumed_out != NULL) {
    *bytes_consumed_out = (short)decode_state[1];
  }
  return result;
}

/* ========================================================================
 * hashtable.c — Hash table implementation
 * ======================================================================== */

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

/* hashtable_set_user_data — store a user-data/callback value at offset 0x0c
 * of the hashtable header (0x11b950).
 *
 * Original source: c:\halo\SOURCE\memory\hashtable.c
 * Offset 0x0c sits between the load_factor float (0x08) and the param_5 field
 * (0x10) established by hashtable_new.  This is a simple field setter with no
 * validation.
 */
void hashtable_set_user_data(void *table, int user_data)
{
  char *t = (char *)table;
  *(int *)(t + 0x0c) = user_data;
}

/* hashtable_delete — validate and dispose a hashtable (0x11b960).
 *
 * Original source: c:\halo\SOURCE\memory\hashtable.c lines 0x6e (110)–0x74
 * (116).
 *
 * Validates the hashtable (null check, key_size>0, element_size>0,
 * load_factor in (0,1], and — when a capacity slot index is set — that the
 * slot count matches 2^slot_index).  On failure fires display_assert with the
 * hashtable_valid predicate string and halts.
 *
 * On success: disposes the embedded array at t+0x1c via FUN_00117cf0, then
 * frees the optional data block at t+0x18 via debug_free if non-NULL.
 *
 * Struct layout (from hashtable_new, offsets are byte offsets):
 *   t+0x00  short   key_size
 *   t+0x02  short   element_size
 *   t+0x04  short   (zero-init)
 *   t+0x06  short   slot_index (-1 when no capacity allocated)
 *   t+0x08  float   load_factor
 *   t+0x0c  int     user_data (set by hashtable_set_user_data)
 *   t+0x10  int     param_5
 *   t+0x14  int     param_6
 *   t+0x18  int     optional data block pointer (freed here)
 *   t+0x1c  []      embedded array header (disposed here)
 *   t+0x20  int     capacity field inside array header (checked when
 *                   slot_index != -1)
 */
#define hashtable_valid(t) \
  ((t) != (void *)0 && *(t) > 0 && (t)[1] > 0 && \
   *(float *)((t) + 4) > 0.0f && *(float *)((t) + 4) <= 1.0f && \
   ((t)[3] == -1 || (1 << (t)[3]) == *(int *)((t) + 16)))

void hashtable_delete(short *table)
{
  char *t;

  if (!hashtable_valid(table)) {
    display_assert("hashtable_valid(table)",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x6e, 1);
    system_exit(-1);
  }
  t = (char *)table;
  FUN_00117cf0((int *)(t + 0x1c));
  if (*(int *)(t + 0x18) != 0) {
    debug_free(*(void **)(t + 0x18), "c:\\halo\\SOURCE\\memory\\hashtable.c",
               0x74);
  }
}

/* hashtable_hash — default hash function using small primes (0x11ba00).
 * Source: hashtable.c. */
int default_hash_function(unsigned char *key, unsigned int key_size)
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
int hashtable_search(short *table, void *key, unsigned short *slot_index_out)
{
  unsigned short hash_val;
  short slot;
  int probe_count;
  int element_ptr;
  int cmp_result;
  int found;

  probe_count = 0;
  if (*(int *)(table + 8) != 0) {
    hash_val = (unsigned short)(*(int (**)(int, void *))(table + 8))(
      *(int *)(table + 6), key);
  } else {
    hash_val =
      (unsigned short)default_hash_function((unsigned char *)key, (unsigned int)*table);
  }
  slot = (short)((unsigned short)(table[0x10] - 1) & hash_val);
  while (1) {
    if ((*(unsigned int *)(*(int *)(table + 0xc) + ((int)slot >> 5) * 4) &
         (1 << (slot & 0x1f))) == 0) {
      *slot_index_out = (unsigned short)slot;
      return 0;
    }
    if (table[2] <= (short)probe_count)
      break;
    if (*(int *)(table + 10) != 0) {
      element_ptr =
        array_get_element((int *)(table + 0xe), (int)slot, (int)table[1]);
      found = (*(int (**)(int, int, void *))(table + 10))(*(int *)(table + 6),
                                                          element_ptr, key);
    } else {
      element_ptr =
        array_get_element((int *)(table + 0xe), (int)slot, (int)table[1]);
      cmp_result = csmemcmp((void *)element_ptr, key, (int)*table);
      found = cmp_result == 0;
    }
    if (found != 0) {
      *slot_index_out = (unsigned short)slot;
      return 1;
    }
    probe_count = probe_count + 1;
    slot = (short)((int)(slot + 1) & (int)(table[0x10] - 1));
  }
  *slot_index_out = (unsigned short)slot;
  return 0;
}

/* hashtable_find — look up a key, return pointer to value (0x11bb70).
 * Source: hashtable.c line 0x4d. */
int hashtable_get(short *table, void *key)
{
  short *psVar1;
  char found;
  int element_ptr;
  short slot;
  int result = 0;

  if (!hashtable_valid(table)) {
    display_assert("hashtable_valid(table)",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x4d, 1);
    system_exit(-1);
  }
  psVar1 = table;
  if (psVar1[2] != 0) {
    found = (char)hashtable_search(psVar1, key, (unsigned short *)&slot);
    if (found != '\0') {
      element_ptr =
        array_get_element((int *)(psVar1 + 0xe), (int)slot, (int)psVar1[1]);
      return element_ptr + *psVar1;
    }
  }
  return result;
}

/* hashtable_remove — remove a key using backward-shift deletion (0x11bc20).
 * Source: hashtable.c line 0xc3. */
void hashtable_remove(short *table, void *key)
{
  unsigned int *bitmap_word;
  unsigned int bit_mask;
  short *psVar3;
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
  psVar3 = table;
  if (key == NULL) {
    display_assert("key", "c:\\halo\\SOURCE\\memory\\hashtable.c", 0xc4, 1);
    system_exit(-1);
  }
  found = (char)hashtable_search(psVar3, key, (unsigned short *)&removed_slot);
  if (found != '\0') {
    next_slot = (unsigned short)((int)(removed_slot + 1) &
                                 (int)(unsigned short)(psVar3[0x10] - 1));
    cur_pos = (int)(short)next_slot;
    bit_mask = *(unsigned int *)(*(int *)(psVar3 + 0xc) + (cur_pos >> 5) * 4) &
               (1 << (next_slot & 0x1f));
    while (bit_mask != 0) {
      next_element =
        array_get_element((int *)(psVar3 + 0xe), cur_pos, (int)psVar3[1]);
      if (*(int *)(psVar3 + 8) == 0) {
        key_hash = (unsigned short)default_hash_function((unsigned char *)next_element,
                                                (unsigned int)*psVar3);
      } else {
        key_hash = (unsigned short)(*(int (**)(int, int))(psVar3 + 8))(
          *(int *)(psVar3 + 6), next_element);
      }
      key_hash = (unsigned short)(psVar3[0x10] - 1) & key_hash;
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
        array_get_element((int *)(psVar3 + 0xe), cur_pos, (int)psVar3[1]);
      dst_element = array_get_element((int *)(psVar3 + 0xe), (int)removed_slot,
                                      (int)psVar3[1]);
      csmemcpy((void *)dst_element, (void *)src_element, *(int *)(psVar3 + 0xe));
      removed_slot = (short)next_slot;
    }
    no_shift:
      (void)0;
      next_slot = (unsigned short)((int)(next_slot + 1) &
                                   (int)(unsigned short)(psVar3[0x10] - 1));
      cur_pos = (int)(short)next_slot;
      bit_mask = *(unsigned int *)(*(int *)(psVar3 + 0xc) + (cur_pos >> 5) * 4) &
                 (1 << (next_slot & 0x1f));
    }
    bitmap_word =
      (unsigned int *)(*(int *)(psVar3 + 0xc) + ((int)removed_slot >> 5) * 4);
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
    array_get_element((int *)(table + 0xe), (int)slot, (int)table[1]);
  csmemcpy((void *)element_ptr, key, (int)*table);
  bitmap_word = (unsigned int *)(*(int *)(table + 0xc) + ((int)slot >> 5) * 4);
  *bitmap_word = *bitmap_word | (1 << ((unsigned char)slot & 0x1f));
  table[2] = table[2] + 1;
  return *table + element_ptr;
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
  char grew;
  int result;

  if (!hashtable_valid(table)) {
    display_assert("hashtable_valid(table)",
                   "c:\\halo\\SOURCE\\memory\\hashtable.c", 0x5d, 1);
    system_exit(-1);
  }
  if ((table[3] == -1) ||
      ((float)(int)table[2] >=
       (float)*(int *)(table + 0x10) * *(float *)(table + 4))) {
    grew = (char)hashtable_grow(table, (short)((table[3] == -1) + 1));
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

/* lra_cache_is_active — check if cache has active blocks (0x11c1b0). */
int lra_full(int cache)
{
  if (*(int *)(cache + 0x2c) != 0 &&
      *(int *)(*(int *)(cache + 0x2c) + 0xc) != 0) {
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

/* lra_cache_validate_block — validate a block header (0x11c210).
 * Register args: @EBX = cache, @ESI = block header. */
void verify_lra_cache_block(int cache, int block)
{
  unsigned int cache_size;
  int block_size;
  int block_offset;
  int next_offset;

  if ((((*(unsigned int *)(block + 4) & 0xfffffffc) == 0x41626c68) &&
       (block_size = *(int *)(block + 8), block_size >= 0)) &&
      (cache_size = *(unsigned int *)(cache + 0x20),
       block_size < (int)cache_size)) {
    block_offset = block - *(int *)(cache + 0x24);
    if ((block_offset >= 0) && (block_size + block_offset <= (int)cache_size)) {
      if (*(int *)(block + 0xc) == 0) {
        next_offset = 0;
      } else {
        next_offset = *(int *)(block + 0xc) - *(int *)(cache + 0x24);
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
                          (char *)cache, (void *)cache, (void *)block),
                 "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x18e, 1);
  system_exit(-1);
}

/* lra_cache_validate — validate cache struct integrity (0x11c290).
 * Register arg: @EAX = cache. */
void verify_lra_cache(int cache)
{
  if (cache == 0) {
    display_assert("cache", "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x198, 1);
    system_exit(-1);
  }
  if (((*(int *)(cache + 0x38) != 0x6c726163) ||
       (*(int *)(cache + 0x24) == 0)) ||
      (*(int *)(cache + 0x20) < 0)) {
    display_assert(csprintf(error_string_buffer,
                            "lra cache %s @%p appears to be corrupt",
                            (char *)cache, (void *)cache),
                   "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x1a2, 1);
    system_exit(-1);
  }
  if (*(int *)(cache + 0x2c) != 0) {
    verify_lra_cache_block(cache, *(int *)(cache + 0x2c));
  }
}

/* lra_cache_new — allocate and initialize a cache (0x11c310).
 * Source: lra_cache.c lines 0x56-0x7e. */
int lra_new(const char *name, int size, void (*lock_proc)(void *, int),
                 void (*unlock_proc)(void *), void *base_address)
{
  int cache;
  char owns_buffer;

  cache =
    (int)debug_malloc(0x3c, 0, "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x56);
  if (size < 0) {
    display_assert("size>=0", "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x58, 1);
    system_exit(-1);
  }
  if (lock_proc == NULL || unlock_proc == NULL) {
    lock_proc = (void (*)(void *, int))lra_default_new_block_proc;
    unlock_proc = (void (*)(void *))lra_default_purge_block_proc;
  }
  if (cache != 0) {
    owns_buffer = 0;
    if (base_address == NULL) {
      base_address =
        debug_malloc(size, 0, "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x66);
      owns_buffer = 1;
      if (base_address == NULL) {
        debug_free((void *)cache, "c:\\halo\\SOURCE\\memory\\lra_cache.c",
                   0x7e);
        return 0;
      }
    }
    if (((unsigned int)base_address & 3) != 0) {
      display_assert("!((long)base_address&3)",
                     "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x6b, 1);
      system_exit(-1);
    }
    csmemset((void *)cache, 0, 0x3c);
    csstrncpy((char *)cache, name, 0x1f);
    *(char *)(cache + 0x1f) = 0;
    *(int *)(cache + 0x20) = size;
    *(void **)(cache + 0x24) = base_address;
    *(int *)(cache + 0x2c) = 0;
    *(int *)(cache + 0x38) = 0x6c726163;
    *(char *)(cache + 0x28) = owns_buffer;
    *(void (**)(void *, int))(cache + 0x30) = lock_proc;
    *(void (**)(void *))(cache + 0x34) = unlock_proc;
    verify_lra_cache(cache);
  }
  return cache;
}

/* lra_cache_dispose — free a cache and its buffer (0x11c430).
 * Source: lra_cache.c lines 0x8c-0x8d. */
void lra_dispose(int cache)
{
  verify_lra_cache(cache);
  if (*(char *)(cache + 0x28) != '\0') {
    debug_free(*(void **)(cache + 0x24),
               "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x8c);
  }
  debug_free((void *)cache, "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x8d);
}

/* lra_cache_flush — unlock all blocks and clear the list (0x11c480).
 * Source: lra_cache.c. */
void lra_flush(int cache)
{
  int *block;

  verify_lra_cache(cache);
  if (*(int *)(cache + 0x2c) != 0 &&
      (block = *(int **)(cache + 0x24), block != NULL)) {
    do {
      if ((*(unsigned char *)(block + 1) & 2) == 0) {
        (*(void (**)(int))(cache + 0x34))(*block);
        block[1] = (block[1] & ~1) | 2;
      }
      block = (int *)block[3];
    } while (block != NULL);
    *(int *)(cache + 0x2c) = 0;
    return;
  }
  *(int *)(cache + 0x2c) = 0;
}

/* lra_cache_unlock_block — release a specific block (0x11c4d0).
 * Source: lra_cache.c line 0x11a. */
void lra_free(int cache, void *pointer)
{
  int block_header;

  if (pointer == NULL) {
    display_assert("pointer", "c:\\halo\\SOURCE\\memory\\lra_cache.c", 0x11a,
                   1);
    system_exit(-1);
  }
  block_header = (int)pointer - 0x10;
  verify_lra_cache(cache);
  verify_lra_cache_block(cache, block_header);
  if ((*(unsigned char *)(block_header + 4) & 2) == 0) {
    (*(void (**)(int))(cache + 0x34))(*(int *)block_header);
    *(unsigned int *)(block_header + 4) =
      (*(unsigned int *)(block_header + 4) & ~1U) | 2;
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
  case 0:
    CHECK_MSG_SIZE(0xc, "message_struct_size==sizeof(message_client_broadcast_game_search)", 0xa0);
  case 1:
    CHECK_MSG_SIZE(8, "message_struct_size==sizeof(message_client_ping)", 0xa1);
  case 2:
    CHECK_MSG_SIZE(0x114, "message_struct_size==sizeof(message_server_game_advertise)", 0xa4);
  case 3:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_pong)", 0xa5);
  case 4:
    CHECK_MSG_SIZE(8, "message_struct_size==sizeof(message_server_machine_accepted)", 0xa8);
  case 5:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_server_machine_rejected)", 0xa9);
  case 6:
    CHECK_MSG_SIZE(0x434, "message_struct_size==sizeof(message_server_game_settings_update)", 0xaa);
  case 7:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_server_pregame_countdown)", 0xab);
  case 8:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_begin_game)", 0xad);
  case 9:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_graceful_game_exit_pregame)", 0xae);
  case 10:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_server_pregame_keep_alive)", 0xac);
  case 11:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_server_postgame_keep_alive)", 0xb1);
  case 12:
    CHECK_MSG_SIZE(0x50, "message_struct_size==sizeof(message_client_join_game_request)", 0xb4);
  case 13:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_add_player_request_pregame)", 0xb5);
  case 14:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_remove_player_request_pregame)", 0xb6);
  case 15:
    CHECK_MSG_SIZE(0x44, "message_struct_size==sizeof(message_client_settings_request)", 0xb7);
  case 16:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_player_settings_request)", 0xb8);
  case 17:
    CHECK_MSG_SIZE(2, "message_struct_size==sizeof(message_client_game_start_request)", 0xb9);
  case 18:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_client_graceful_game_exit_pregame)", 0xba);
  case 19:
    CHECK_MSG_SIZE(0x100, "message_struct_size==sizeof(message_client_map_is_precached_pregame)", 0xbb);
  case 20:
    CHECK_MSG_SIZE(0x210, "message_struct_size==sizeof(message_server_game_update)", 0xbe);
  case 21:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_server_add_player_ingame)", 0xbf);
  case 22:
    CHECK_MSG_SIZE(0x24, "message_struct_size==sizeof(message_server_remove_player_ingame)", 0xc0);
  case 23:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_game_over)", 0xc1);
  case 24:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_client_loaded)", 0xc4);
  case 25:
    CHECK_MSG_SIZE(0x88, "message_struct_size==sizeof(message_client_game_update)", 0xc5);
  case 26:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_add_player_request_ingame)", 0xc6);
  case 27:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_remove_player_request_ingame)", 0xc7);
  case 28:
    CHECK_MSG_SIZE(0x10, "message_struct_size==sizeof(message_client_host_crashed_cry_for_help)", 0xc9);
  case 29:
    CHECK_MSG_SIZE(0x10, "message_struct_size==sizeof(message_client_join_new_host)", 0xca);
  case 30:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_switch_to_pregame)", 0xcd);
  case 31:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_server_graceful_game_exit_postgame)", 0xce);
  case 32:
    CHECK_MSG_SIZE(0x20, "message_struct_size==sizeof(message_client_remove_player_request_postgame)", 0xd1);
  case 33:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_client_switch_to_pregame)", 0xd2);
  case 34:
    CHECK_MSG_SIZE(4, "message_struct_size==sizeof(message_client_graceful_game_exit_postgame)", 0xd3);
  default:
    display_assert("unknown network game message structure type",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c",
                   0xd5, 1);
    system_exit(-1);
    break;
  }
#undef CHECK_MSG_SIZE
  if (data == NULL || (int16_t)encoded_size < 1) {
    display_assert("message_struct && encoded_message && encoded_message_size "
                   "&& (*encoded_message_size>0)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0x161,
                   1);
    system_exit(-1);
  }

  /* encode_packet_group's size parameter is short * (16-bit accesses at
   * 0x11abbc/0x11ac67); `encoded_size` is a dword slot here, matching the
   * original's own dword store/load of the same variable. */
  if (!encode_packet_group(&s_network_game_messages_group, data, encoded_buf,
                           (short *)&encoded_size, type, 1)) {
    network_event("create_network_game_message() failed");
    return NULL;
  }

  {
    void *msg =
      (void *)create_message(3, (int)encoded_buf, encoded_size,
                             (int)s_network_game_message_buffer, 0x604);
    if (msg == NULL) {
      network_event("create_message() failed");
    }
    return msg;
  }
}
