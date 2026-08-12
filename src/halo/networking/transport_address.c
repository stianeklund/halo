/* Bungie.net transport-layer address helpers.
 *
 * TU: c:\halo\SOURCE\bungie_net\network\transport_address.c -- confirmed by
 * the __FILE__ string at 0x265ffc, which every assert in this file passes to
 * display_assert.
 */

#define TRANSPORT_ADDRESS_FILE \
  "c:\\halo\\SOURCE\\bungie_net\\network\\transport_address.c"

/* IPv4 address length in bytes. Named from the assert text
 * "IPV4_ADDRESS_LENGTH == a->address_length"; the compared immediate is 4. */
#define IPV4_ADDRESS_LENGTH 4

/* transport_initialized flag (0x335090) -- the same byte the rest of the
 * transport layer tests (see transport_endpoint_set_winsock.c). Spelled as a
 * macro so the assert reproduces the original "transport_initialized" reason
 * string at 0x265fe4. */
#define transport_initialized (*(uint8_t *)0x335090)

/* Compare two transport addresses for equality.
 *
 * Returns true when the address bytes and the port match. The compared span is
 * max(a->address_length, b->address_length); both lengths are asserted equal to
 * IPV4_ADDRESS_LENGTH immediately above, so the max is always 4 at runtime, but
 * the original still emits the comparison (MOV AX,[ESI+0x10] / MOV CX,[EDI+0x10]
 * / CMP AX,CX / JA, i.e. an unsigned 16-bit max zero-extended to 32 bits before
 * the push).
 *
 * Confirmed: cdecl, both parameters on the stack (EBP+8, EBP+0xC); returns in
 * EAX (MOV EAX,1 at 0x81b78 on the equal path, XOR EAX,EAX at 0x81b81
 * otherwise). csmemcmp (0x8da40) is called as csmemcmp(a, b, length) --
 * PUSH EAX / PUSH EDI / PUSH ESI, ADD ESP,0xC.
 * Confirmed: asserts at source lines 0x3b, 0x3c, 0x3d, 0x3f, 0x40 (the gap at
 * 0x3e is a blank line in the original source).
 * Confirmed: the port field is compared as a 16-bit value at +0x12; csmemcmp
 * starts at offset 0 and so never reaches it.
 */
bool transport_address_equivalent(transport_address *a, transport_address *b)
{
  uint16_t length;

  assert_halt_at(TRANSPORT_ADDRESS_FILE, 0x3b, a);
  assert_halt_at(TRANSPORT_ADDRESS_FILE, 0x3c, b);
  assert_halt_at(TRANSPORT_ADDRESS_FILE, 0x3d, transport_initialized);

  assert_halt_at(TRANSPORT_ADDRESS_FILE, 0x3f,
                 IPV4_ADDRESS_LENGTH == a->address_length);
  assert_halt_at(TRANSPORT_ADDRESS_FILE, 0x40,
                 IPV4_ADDRESS_LENGTH == b->address_length);

  length = a->address_length > b->address_length ? a->address_length
                                                 : b->address_length;

  return csmemcmp(a, b, length) == 0 && a->port == b->port;
}
