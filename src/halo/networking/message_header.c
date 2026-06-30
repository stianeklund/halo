/* ========================================================================
 * message_header.c — Message header, encryption, key agreement, prime sieve
 * Original source: c:\halo\SOURCE\bungie_net\common\message_header.c
 *                  c:\halo\SOURCE\bungie_net\common\message_encryption.c
 *                  c:\halo\SOURCE\bungie_net\common\key_agreement.c
 *                  c:\halo\SOURCE\bungie_net\common\prime_numbers.c
 * ======================================================================== */
#include "../../common.h"

/* 0x80530 - Peek at packet type from a key-agreement message buffer.
 * Reads the packet-type byte from param_1[param_2 - 1] into *param_3.
 * Returns 1 if the header flags indicate a valid encrypted key exchange
 * packet type (0 or 1); otherwise returns 0. */
int key_agreement_peek_packet_type(unsigned char *msgptr,
                                   unsigned short msg_size,
                                   unsigned char *packet_type)
{
  unsigned char hdr_byte;
  unsigned char type_byte;

  assert_halt_msg(msgptr != (unsigned char *)0 &&
                    packet_type != (unsigned char *)0,
                  "msgptr && packet_type");

  type_byte = msgptr[msg_size - 1];
  hdr_byte = *msgptr;
  *packet_type = type_byte;

  if (((hdr_byte & 2) != 0) && ((*msgptr >> 2 & 3) == 3) &&
      ((type_byte == 0) || (type_byte == 1))) {
    return 1;
  }
  return 0;
}

/* 0x807d0 - XOR a message buffer against a bouncing keystream.
 * Iterates param_2 bytes: at each step the message byte is XOR'd with the
 * keystream byte at index iVar2 (which bounces between 0 and param_4-1),
 * then the result is bit-inverted (~). */
void key_message_xor_keystream(int msg, int len, int keystream, int key_len)
{
  unsigned char *pbVar1;
  int iVar2;
  int iVar3;
  int iVar4;

  iVar3 = 0;
  iVar2 = 0;
  iVar4 = 1;

  if (0 < len) {
    do {
      pbVar1 = (unsigned char *)(iVar2 + keystream);
      iVar2 = iVar2 + iVar4;
      *(unsigned char *)(iVar3 + msg) =
        ~(*pbVar1 ^ *(unsigned char *)(iVar3 + msg));
      iVar3 = iVar3 + 1;
      if ((iVar2 == key_len) || (iVar2 < 0)) {
        iVar4 = -iVar4;
        iVar2 = iVar2 + iVar4;
      }
    } while (iVar3 < len);
  }
}

/* 0x80820 - TEA (Tiny Encryption Algorithm) encrypt.
 * Encrypts the 64-bit block in param_1[0..1] using the 128-bit key in
 * param_3[0..3] and writes the ciphertext to param_2[0..1].
 * Runs 32 Feistel rounds with delta = 0x9E3779B9. */
void tea_encrypt(unsigned int *v, unsigned int *w, int *key)
{
  unsigned int uVar1;
  unsigned int uVar2;
  int iVar3;
  unsigned int count;

  uVar1 = v[0];
  uVar2 = v[1];
  iVar3 = 0;
  count = 0x20;

  do {
    iVar3 = iVar3 + (int)0x9E3779B9u; /* -0x61c88647 mod 2^32 */
    uVar1 = uVar1 + ((uVar2 >> 5) + key[1] ^ uVar2 * 0x10 + key[0] ^
                     (unsigned int)iVar3 + uVar2);
    uVar2 = uVar2 + ((uVar1 >> 5) + key[3] ^ uVar1 * 0x10 + key[2] ^
                     (unsigned int)iVar3 + uVar1);
    count = count - 1;
  } while (count != 0);

  w[0] = uVar1;
  w[1] = uVar2;
}

/* 0x808b0 - TEA decrypt.
 * Decrypts the 64-bit block in param_1[0..1] using the 128-bit key in
 * param_3[0..3] and writes plaintext to param_2[0..1].
 * Starting sum = 0xC6EF3720 = 32 * TEA_DELTA. Runs 32 rounds. */
void tea_decrypt(unsigned int *v, unsigned int *w, int *key)
{
  unsigned int uVar1;
  unsigned int uVar2;
  int iVar3;
  unsigned int count;

  uVar2 = v[0];
  uVar1 = v[1];
  iVar3 = (int)0xC6EF3720u; /* -0x3910c8e0 mod 2^32 */
  count = 0x20;

  do {
    uVar1 = uVar1 - ((uVar2 >> 5) + key[3] ^ uVar2 * 0x10 + key[2] ^
                     (unsigned int)iVar3 + uVar2);
    uVar2 = uVar2 - ((uVar1 >> 5) + key[1] ^ uVar1 * 0x10 + key[0] ^
                     (unsigned int)iVar3 + uVar1);
    iVar3 = iVar3 + 0x61c88647;
    count = count - 1;
  } while (count != 0);

  w[0] = uVar2;
  w[1] = uVar1;
}

/* 0x80b40 - Build (encode) a 2-byte message header in-place.
 * Packs length (bits 15..4), type (bits 3..2), and flags (bits 1..0)
 * into *header. Asserts on NULL header, oversized length, invalid type,
 * and out-of-range flags. */
void build_message_header(unsigned short *header, unsigned short length,
                          unsigned char type, unsigned char flags)
{
  assert_halt_msg(header != (unsigned short *)0, "header != NULL");
  assert_halt_msg((0 <= (int)length) && ((int)length <= 0xfff),
                  "(0<=(length)) && ((length)<=MAXIMUM_MESSAGE_SIZE)");

  *header = ((unsigned char)*header & 0xf) | (length << 4);

  if ((type != 0) && (type < 4)) {
    *header = ((unsigned short)(type & 3) << 2) | (*header & 0xfff3);
    assert_halt_msg((0 <= (int)flags) && ((int)flags <= 3),
                    "(0<=flags) && ((flags)<=MESSAGE_FLAG_BITS_MASK)");
    *header = (*header & 0xfffc) | (unsigned short)flags;
    return;
  }

  assert_halt_msg(0, "(0<(type)) && ((type)<NUMBER_OF_MESSAGE_TYPES)");
}

/* 0x80ca0 - Allocate (or use a provided buffer) and build a complete message.
 * If param_4 == 0, allocates (length+2) bytes via debug_malloc.
 * Writes the header at offset 0 and copies param_3 bytes of payload from
 * param_2 to offset 2. Returns the message buffer pointer (0 on alloc fail). */
int create_message(int type, int payload, unsigned int payload_len, int buffer,
                   unsigned short buffer_size)
{
  short msg_size;

  msg_size = (short)(payload_len + 2);

  if (buffer == 0) {
    buffer = (int)debug_malloc(
      (int)msg_size, 0,
      "c:\\halo\\SOURCE\\bungie_net\\common\\message_header.c", 0x2e);
  } else if ((int)(unsigned int)buffer_size < (int)msg_size) {
    assert_halt_msg(0, "buffer_size >= message_size");
  }

  if (buffer != 0) {
    build_message_header((unsigned short *)buffer, payload_len + 2,
                         (unsigned char)type, 0);
    if (payload != 0) {
      csmemcpy((void *)(buffer + 2), (void *)payload, payload_len & 0xffff);
    }
  }
  return buffer;
}

/* 0x80c20 - Byte-swap a 2-byte message header for network byte order.
 * param_2 == 0: host->network; param_2 == 1: network->host.
 * Both directions are identical (swap both bytes). */
void byte_swap_message_header(unsigned short *header, int byte_order)
{
  unsigned short v;

  assert_halt_msg(header != (unsigned short *)0, "header");

  if (byte_order == 1) {
    v = *header;
    *header = (unsigned short)((v << 8) | (v >> 8));
    return;
  }
  if (byte_order == 0) {
    v = *header;
    *header = (unsigned short)((v << 8) | (v >> 8));
    return;
  }

  assert_halt_msg(0, "!\"bad value for byte order\"");
}

/* 0x80d30 - Comparator for qsort over unsigned int arrays (ascending).
 * Returns 1 if *a < *b, -1 if *a > *b, 0 if equal. */
int prime_compare(unsigned int *a, unsigned int *b)
{
  if (*a < *b) {
    return 1;
  }
  return -(int)(*b < *a);
}
