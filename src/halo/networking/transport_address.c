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
 * the original still emits the comparison (MOV AX,[ESI+0x10] / MOV
 * CX,[EDI+0x10] / CMP AX,CX / JA, i.e. an unsigned 16-bit max zero-extended to
 * 32 bits before the push).
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

  length = a->address_length > b->address_length ? a->address_length :
                                                   b->address_length;

  return csmemcmp(a, b, length) == 0 && a->port == b->port;
}

/* IPv6 address length in bytes. INFERRED by symmetry with
 * IPV4_ADDRESS_LENGTH -- no assert string names this constant; the only
 * evidence is the CMP AX,0x10 at 0x81c24 selecting the eight-group format
 * string at 0x266094. */
#define IPV6_ADDRESS_LENGTH 16

/* Shared formatting scratch for transport_address_to_string: a file-scope
 * static char[0x100] in the original TU, living at 0x334f90. Spelled as its VA
 * (like transport_initialized above) so the ported function and any
 * still-original code in this TU keep sharing the one buffer -- the function
 * returns its address, so every caller holds a pointer into it. */
#define transport_address_string ((char *)0x334f90)
#define TRANSPORT_ADDRESS_STRING_SIZE 0x100

/* Format a transport address as a printable string, e.g. "1.2.3.4:2302".
 *
 * Confirmed: cdecl, one stack parameter (EBP+8, held in ESI -- the only
 * callee-saved register the frame pushes); no locals, no sub esp.
 * Confirmed: returns the buffer address in EAX on every path
 * (MOV EAX,0x334F90 at both 0x81c1c and 0x81c6d), NOT the snprintf result.
 * Confirmed: asserts at original source lines 0x4a and 0x4b.
 * Confirmed: buffer[0] is cleared with a BYTE store (MOV byte ptr
 * [0x334F90],0), not a word or dword store.
 * Confirmed: the IPv4 path prints the address bytes in REVERSE order -- the
 * pushes at 0x81c14 are [ESI+3],[ESI+2],[ESI+1],[ESI+0],[ESI+0x12] (8 dwords,
 * ADD ESP,0x20) -- while the IPv6 path prints its eight 16-bit groups in
 * FORWARD order, [ESI+0]..[ESI+0xE], then the port (12 dwords, ADD ESP,0x30).
 * Every field is zero-extended (MOVZX) into the varargs.
 * Confirmed: address_length is compared three times -- once against 4 straight
 * from memory for the assert (0x81bb8), then loaded once into AX (0x81be3) and
 * compared against 4 (0x81be7) and 0x10 (0x81c24). The single load is the
 * compiler CSE-ing the two field reads below; the assert's compare is separate
 * because the byte store to the buffer sits between them.
 * Uncertain: the IPv6 branch is unreachable at runtime -- the line-0x4b assert
 * already halted on any length other than 4. It is emitted because the
 * original emits it, and its shape is load-bearing for the tail. */
const char *transport_address_to_string(void *addr_)
{
/* The kb prototype keeps the untyped `void *addr` that this function's ten-odd
 * existing callers pass raw byte buffers to. This alias restores the field
 * syntax inside the body so assert_halt_at stringizes the original reason text
 * "IPV4_ADDRESS_LENGTH == addr->address_length" (0x2660cc) verbatim, rather
 * than leaking a cast into the message. */
#define addr ((const transport_address *)addr_)

  assert_halt_at(TRANSPORT_ADDRESS_FILE, 0x4a, addr);
  assert_halt_at(TRANSPORT_ADDRESS_FILE, 0x4b,
                 IPV4_ADDRESS_LENGTH == addr->address_length);

  transport_address_string[0] = '\0';

  if (addr->address_length == IPV4_ADDRESS_LENGTH) {
    snprintf(transport_address_string, TRANSPORT_ADDRESS_STRING_SIZE,
             "%hd.%hd.%hd.%hd:%hd", addr->address[3], addr->address[2],
             addr->address[1], addr->address[0], addr->port);
    return transport_address_string;
  }

  if (addr->address_length == IPV6_ADDRESS_LENGTH) {
    snprintf(transport_address_string, TRANSPORT_ADDRESS_STRING_SIZE,
             "%4X.%4X.%4X.%4X.%4X.%4X.%4X.%4X:%hd",
             ((const uint16_t *)addr->address)[0],
             ((const uint16_t *)addr->address)[1],
             ((const uint16_t *)addr->address)[2],
             ((const uint16_t *)addr->address)[3],
             ((const uint16_t *)addr->address)[4],
             ((const uint16_t *)addr->address)[5],
             ((const uint16_t *)addr->address)[6],
             ((const uint16_t *)addr->address)[7], addr->port);
  }

  return transport_address_string;

#undef addr
}
