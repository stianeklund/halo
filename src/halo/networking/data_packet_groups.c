/* data_packet_groups.obj — packet-group / packet-field ENCODE side.
 *
 * Two Bungie translation units are represented here (kb.json groups all four
 * addresses into data_packet_groups.obj):
 *
 *   0x11abb0, 0x11aca0 — c:\halo\SOURCE\memory\data_packet_groups.c
 *                        (__FILE__ literal at VA 0x28f1f0)
 *   0x11add0, 0x11afa0 — c:\halo\SOURCE\memory\data_packets.c
 *                        (__FILE__ literal at VA 0x28f498)
 *
 * The DECODE-side counterparts of these functions
 * (verify_packet_group_definitions 0x11a930, FUN_0011aa40 0x11aa40,
 * FUN_0011b2a0 0x11b2a0) live in networking/network_messages.c and are not
 * duplicated here.
 *
 * Packet field descriptor: 5 shorts (stride 0xa), terminated by type == 9.
 * Confirmed from the `ADD ESI,0xa` loop advance and the field offsets used:
 *   field[0] = type            (+0x00)
 *   field[1] = count           (+0x02)
 *   field[2] = minimum_version (+0x04)
 *   field[3] = maximum_version (+0x06)  (0 == no upper bound)
 *   field[4] = size            (+0x08)  cached byte size, written by
 *                                       compute_packet_field_sizes
 * No struct is declared for it: network_messages.c's already-ported
 * FUN_0011b2a0 uses the same `short *` + index convention, and no assert
 * string names the members, so the layout stays index-addressed.
 */

/* byte_swap_structures definition for `packet_header` at 0x3220c0.
 * Confirmed: PUSH 0x3220c0 at 0x11ac55 (same global network_messages.c uses).
 */
#define packet_header_bs_def ((void *)0x3220c0)

/* Last packet-group encode/decode error string, global at 0x46e804.
 * Written by both encode_packet_group and its decode twin FUN_0011aa40. */
#define s_last_decode_error (*(char **)0x46e804)

/* ========================================================================
 * data_packet_groups.c
 * ======================================================================== */

/* data_packet_group_append_packet_header (0x11abb0) — stamp the one-byte
 * packet header (the packet type) at the tail of an already-encoded packet
 * and byte-swap it in place.
 *
 * Returns true when the header fit.  On overflow it records
 * "couldn't append header to encoded packet" in the group error global and
 * returns false.  Return is in AL (XOR ECX,ECX / SETZ CL / MOV AL,CL at both
 * exits, 0x11ac6a and 0x11ac82) — kb.json previously declared this void(void).
 *
 * Asserts: data_packet_groups.c lines 0xac, 0xad, 0xae.  Note line 0xac only
 * emits ONE test (TEST EAX,EAX at 0x11abc1, on encoded_packet); the
 * encoded_packet_size half of the recovered expression string has no
 * corresponding compare in the binary, so only the tested half is reproduced.
 */
bool data_packet_group_append_packet_header(group_definition *group,
                                            char *encoded_packet,
                                            short *encoded_packet_size,
                                            short packet_type)
{
  char *header;
  char *error;

  /* ESI = encoded_packet + *encoded_packet_size, computed at 0x11abbc-0x11abbf
   * BEFORE the asserts run.  Order preserved. */
  header = encoded_packet + *encoded_packet_size;
  error = NULL;
  assert_halt_msg_at("encoded_packet && encoded_packet_size",
                     "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0xac,
                     encoded_packet != NULL);
  assert_halt_msg_at("*encoded_packet_size>=0",
                     "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0xad,
                     *encoded_packet_size >= 0);
  /* +0x4 is `packet_count` in types.h; the assert string proves Bungie's own
   * member name was `packet_type_count`.  Renaming the field is out of scope
   * for this lift, so the literal message is supplied verbatim. */
  assert_halt_msg_at(
    "packet_type>=0 && packet_type<group_definition->packet_type_count",
    "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0xae,
    packet_type >= 0 && packet_type < group->packet_count);
  /* MOVSX ECX,[EDI] / INC ECX / CMP ECX,[EDX+0xc] / JNC — unsigned compare
   * against the full dword at group+0xc. */
  if ((uint32_t)(*encoded_packet_size + sizeof(packet_header)) <
      (uint32_t)group->maximum_encoded_packet_size) {
    *header = (char)packet_type;
    FUN_00118be0(packet_header_bs_def, header, 1);
    *encoded_packet_size = *encoded_packet_size + 1;
  } else {
    error = "couldn't append header to encoded packet";
  }
  s_last_decode_error = error;
  return error == NULL;
}

/* encode_packet_group (0x11aca0) — encode `data` as packet `packet_type` of
 * `group` into encoded_packet, then append the packet header.
 *
 * Asserts: data_packet_groups.c lines 0x84, 0x85, 0x86, 0x8b. */
bool encode_packet_group(group_definition *group, void *data,
                         char *encoded_packet, short *encoded_packet_size,
                         short packet_type, short version)
{
  packet_entry *packet;
  char *error;

  error = NULL;
  assert_halt_msg_at("group_definition",
                     "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x84,
                     group != NULL);
  assert_halt_msg_at(
    "packet_type>=0 && packet_type<group_definition->packet_type_count",
    "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x85,
    packet_type >= 0 && packet_type < group->packet_count);
  assert_halt_msg_at("encoded_packet && encoded_packet_size",
                     "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x86,
                     encoded_packet != NULL && encoded_packet_size != NULL);
  /* LEA ESI,[ECX+EAX*8] — stride 8 == sizeof(packet_entry). */
  packet = &group->packets[packet_type];
  assert_halt_msg_at("packet->definition",
                     "c:\\halo\\SOURCE\\memory\\data_packet_groups.c", 0x8b,
                     packet->definition != NULL);
  /* Arg 6 is a 16-bit read of the dword field at group+0xc
   * (XOR EDX,EDX / MOV DX,word [EDI+0xc] at 0x11ad67). */
  if (FUN_0011b650((int)packet->definition, version, data, encoded_packet,
                   encoded_packet_size,
                   (short)group->maximum_encoded_packet_size)) {
    if (!data_packet_group_append_packet_header(
          group, encoded_packet, encoded_packet_size, packet_type)) {
      error = s_last_decode_error;
    }
  } else {
    error = "couldn't encode packet";
  }
  s_last_decode_error = error;
  return error == NULL;
}

/* ========================================================================
 * data_packets.c
 * ======================================================================== */

/* compute_packet_field_sizes (0x11add0) — walk a packet field list, cache each
 * field's encoded byte size into field[4], and report the total decoded size
 * and the field count (including the terminator).
 *
 * Asserts: data_packets.c lines 0x8e, 0x90, 0xc1.
 *
 * Faithfulness note: the binary does NOT reset the running `field_size` when a
 * field is skipped by the version range check (the JL/JNZ at 0x11ae74 and
 * 0x11ae86 land directly on the common tail at 0x11af20, which stores
 * field_size into field[4] unconditionally).  The previous field's size is
 * therefore reused.  MSVC seeds the register holding field_size with a dead
 * load of the `fields` parameter (MOV EDI,[EBP+0x10] at 0x11adf2), i.e. the
 * first iteration would read an uninitialised value; we initialise to 0
 * instead, which is the only deviation from the original and only observable
 * when the very first field is version-skipped.
 */
void compute_packet_field_sizes(packet_definition *definition,
                                short *packet_size_out, short *fields,
                                short *field_count_out)
{
  short *field;
  short field_size;
  short total_size;
  short nested_size;
  short nested_field_count;

  field = fields;
  total_size = 0;
  field_size = 0;
  if (*field != 9) {
    do {
      /* MOV AX,[ESI] / TEST AX,AX / JL / CMP AX,0xa / JL — 16-bit bounds. */
      if (field[0] < 0 || field[0] >= 10) {
        display_assert(csprintf(error_string_buffer,
                                "unknown field type in packet '%s' (probably "
                                "missing '__pack_pack_end')",
                                definition->name),
                       "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x8e, 1);
        system_exit(-1);
      }
      if (field[1] <= 0) {
        display_assert(
          csprintf(error_string_buffer,
                   "field has negative or zero count in packet '%s'",
                   definition->name),
          "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x90, 1);
        system_exit(-1);
      }
      if (definition->version >= field[2] &&
          (definition->version <= field[3] || field[3] == 0)) {
        switch (field[0]) {
        case 0:
        case 1:
        case 8:
          field_size = field[1];
          break;
        case 2:
          field_size = (short)(field[1] << 1);
          break;
        case 3:
          field_size = (short)(field[1] << 2);
          break;
        case 4:
          field_size = (short)(field[1] << 3);
          break;
        case 5:
          field_size = (short)(field[1] + 1);
          break;
        case 6:
          field_size = (short)(field[1] + 2);
          break;
        case 7:
          compute_packet_field_sizes(definition, &nested_size, field + 5,
                                     &nested_field_count);
          field_size = (short)(field[1] * nested_size + 2);
          field = field + nested_field_count * 5;
          break;
        case 9:
          field_size = 0;
          break;
        default:
          display_assert(0, "c:\\halo\\SOURCE\\memory\\data_packets.c", 0xc1,
                         1);
          system_exit(-1);
          break;
        }
      }
      /* Stored through the (possibly advanced, see case 7) cursor. */
      field[4] = field_size;
      total_size = (short)(total_size + field_size);
      field = field + 5;
    } while (*field != 9);
  }
  if (field_count_out != NULL) {
    /* Byte difference / 10, not element difference / 5: the reference feeds the
     * raw byte delta to the 0x66666667 magic-divide (SAR EDX,2 at 0x11af4e). */
    *field_count_out = (short)(((int)field - (int)fields) / 10 + 1);
  }
  if (packet_size_out != NULL) {
    *packet_size_out = total_size;
  }
}

/* _data_packet_encode (0x11afa0) — encode a packet's fields from `data` into
 * `encode_state`.  Mirror of FUN_0011b2a0 (decode) in network_messages.c.
 *
 * Field types drive the encoder helpers:
 *   1 -> raw bytes            FUN_00119cc0(state, src, count,  1)
 *   2 -> 16-bit array         FUN_00119cc0(state, src, count, -2)
 *   3 -> 32-bit array         FUN_00119cc0(state, src, count, -4)
 *   4 -> 64-bit array         FUN_00119cc0(state, src, count, -8)
 *   5 -> string               FUN_0011a230(state, src, count)
 *   6 -> counted byte block   count prefix + raw bytes
 *   7 -> counted struct array count prefix + recursion per element
 *   8 -> raw bytes            same as 1 (separate MSVC block at 0x11b0ba)
 *   0 -> nothing
 *
 * Version-skipped fields take the second jump table at 0x11b274.  Types
 * 1/2/3/4/8 all merge to FUN_00119cc0(state, NULL, field[1], 1) — one code
 * block reached by five table entries, so the zero-fill really is
 * `field->count` bytes even for the wide element types.  Type 5 writes a
 * single NUL byte; types 6/7 write a zero count prefix only.
 *
 * Asserts: data_packets.c lines 0xfd, 0x119, 0x129, 0x144.
 */
void _data_packet_encode(packet_definition *definition, int *encode_state,
                         short version, void *data, short *encoded_size_out,
                         short *fields, short *field_count_out)
{
  short *field;
  char *data_cursor;
  char *nested_data;
  short element_count;
  short nested_size;
  short nested_field_count;
  unsigned int remaining;
  char empty_string;

  field = fields;
  data_cursor = (char *)data;
  if (*field != 9) {
    do {
      if (version >= field[2] && (version <= field[3] || field[3] == 0)) {
        switch (field[0]) {
        case 0:
          break;
        case 1:
          FUN_00119cc0(encode_state, (int)data_cursor, field[1], 1);
          break;
        case 2:
          FUN_00119cc0(encode_state, (int)data_cursor, field[1], -2);
          break;
        case 3:
          FUN_00119cc0(encode_state, (int)data_cursor, field[1], -4);
          break;
        case 4:
          FUN_00119cc0(encode_state, (int)data_cursor, field[1], -8);
          break;
        case 5:
          FUN_0011a230(encode_state, data_cursor, field[1]);
          break;
        case 6:
          element_count = *(short *)data_cursor;
          assert_halt_msg_at("data_size>=0 && data_size<=field->count",
                             "c:\\halo\\SOURCE\\memory\\data_packets.c", 0xfd,
                             element_count >= 0 && element_count <= field[1]);
          if (element_count < 0 || element_count > field[1]) {
            element_count = 0;
          }
          FUN_00119df0(encode_state, element_count, field[1]);
          FUN_00119cc0(encode_state, (int)(data_cursor + 2), element_count, 1);
          break;
        case 8:
          FUN_00119cc0(encode_state, (int)data_cursor, field[1], 1);
          break;
        case 7:
          element_count = *(short *)data_cursor;
          nested_data = data_cursor + 2;
          compute_packet_field_sizes(definition, NULL, field + 5,
                                     &nested_field_count);
          assert_halt_msg_at("element_count>=0 && element_count<=field->count",
                             "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x119,
                             element_count >= 0 && element_count <= field[1]);
          if (element_count < 0 || element_count > field[1]) {
            element_count = 0;
          }
          FUN_00119df0(encode_state, element_count, field[1]);
          if (element_count > 0) {
            remaining = (unsigned int)(unsigned short)element_count;
            do {
              _data_packet_encode(definition, encode_state, version,
                                  nested_data, &nested_size, field + 5, NULL);
              nested_data = nested_data + nested_size;
              remaining = remaining - 1;
            } while (remaining != 0);
          }
          field = field + nested_field_count * 5;
          break;
        default:
          display_assert(0, "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x129,
                         1);
          system_exit(-1);
          break;
        }
      } else {
        switch (field[0]) {
        case 0:
          break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 8:
          FUN_00119cc0(encode_state, 0, field[1], 1);
          break;
        case 5:
          empty_string = '\0';
          FUN_00119cc0(encode_state, (int)&empty_string, 1, 1);
          break;
        case 6:
        case 7:
          FUN_00119df0(encode_state, 0, field[1]);
          break;
        default:
          display_assert(0, "c:\\halo\\SOURCE\\memory\\data_packets.c", 0x144,
                         1);
          system_exit(-1);
          break;
        }
      }
      data_cursor = data_cursor + field[4];
      field = field + 5;
    } while (*field != 9);
  }
  if (field_count_out != NULL) {
    /* Byte difference / 10 — see compute_packet_field_sizes (SAR EDX,2 at
     * 0x11b22e). */
    *field_count_out = (short)(((int)field - (int)fields) / 10 + 1);
  }
  if (encoded_size_out != NULL) {
    *encoded_size_out = (short)(data_cursor - (char *)data);
  }
}
