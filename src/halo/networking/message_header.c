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

/* 0x80d30 - Comparator for qsort over unsigned int arrays (ascending).
 * Returns 1 if *a < *b, -1 if *a > *b, 0 if equal. */
int prime_compare(unsigned int *a, unsigned int *b)
{
  if (*a < *b) {
    return 1;
  }
  return -(int)(*b < *a);
}

/* Global: pointer to key_agreement_packets group definition at 0x2ee588. */
#define key_agreement_group ((void *)0x2ee588)

/* 0x803d0 - Encode a key-agreement packet and wrap it in a message.
 * Encodes the packet data (param_2) into a 128-byte stack buffer using
 * encode_packet_group with message type param_1, then calls create_message
 * to allocate and build the network message.  Sets the encrypted-key-exchange
 * flag (bit 1) in the returned message header and returns the message pointer,
 * or NULL on failure. */
unsigned short *key_agreement_build_message(short type, void *data, int buffer,
                                            unsigned short buffer_size)
{
  unsigned char encoded_buf[0x88];
  int encoded_size;
  unsigned short *msg;
  int i;

  encoded_buf[0] = 0;
  for (i = 0; i < 0x7f; i++) {
    encoded_buf[1 + i] = 0;
  }
  encoded_size = 0x80;

  if (encode_packet_group((group_definition *)key_agreement_group, data,
                          (char *)encoded_buf, &encoded_size, type, 1)) {
    msg = (unsigned short *)create_message(
      3, (int)encoded_buf, (unsigned int)encoded_size, buffer, buffer_size);
    if (msg != (unsigned short *)0) {
      *msg = (*msg & 0xfffe) | 2;
      return msg;
    }
  }
  return (unsigned short *)0;
}

/* 0x80940 - Encrypt a message in-place using TEA + keystream XOR.
 * Encrypts full 8-byte TEA blocks followed by any remainder bytes via
 * key_message_xor_keystream.  Payload byte-count is derived from the
 * 4-bit-shifted length field minus 2 (the header size).
 * Sets the encrypted flag (bit 0) in the message header on success.
 * Asserts if msgptr or key is NULL, or if the resulting flags exceed 2 bits. */
void message_encrypt(unsigned short *msgptr, unsigned int *key)
{
  unsigned short hdr;
  unsigned int blocks;
  unsigned int remain;
  unsigned short *cursor;
  unsigned int key_copy[4];
  unsigned int i;

  assert_halt_msg(msgptr != (unsigned short *)0 && key != (unsigned int *)0,
                  "msgptr && key");

  hdr = *msgptr;
  if ((hdr & 1) == 0) {
    blocks = (unsigned int)(((unsigned short)(hdr >> 4) - 2) >> 3);
    remain = (unsigned int)((unsigned char)((char)((hdr >> 4) - 2)) & 7);
    cursor = msgptr + 1;
    key_copy[0] = key[0];
    key_copy[1] = key[1];
    key_copy[2] = key_copy[0];
    key_copy[3] = key_copy[1];

    if ((short)blocks != 0) {
      i = (unsigned int)(blocks & 0xffff);
      do {
        tea_encrypt((unsigned int *)cursor, (unsigned int *)cursor,
                    (int *)key_copy);
        cursor = cursor + 4;
        i = i - 1;
      } while (i != 0);
      i = 0;
    }
    if ((short)remain != 0) {
      key_message_xor_keystream((int)cursor, (int)(short)remain, (int)key, 8);
    }
    hdr = (unsigned short)(hdr & 3) | 1;
    assert_halt_msg(!(3 < hdr),
                    "(0<=flags) && ((flags)<=MESSAGE_FLAG_BITS_MASK)");
    *msgptr = (*msgptr & 0xfffc) | hdr;
  }
}

/* 0x80a40 - Decrypt a message in-place using TEA + keystream XOR.
 * Mirror of message_encrypt: checks that bit 0 is set (message is encrypted),
 * decrypts full 8-byte TEA blocks, then XORs any remainder bytes.
 * Clears the encrypted flag (bit 0) and retains the key-exchange flag (bit 1).
 */
void message_decrypt(unsigned short *msgptr, unsigned int *key)
{
  unsigned short hdr;
  unsigned int blocks;
  unsigned int remain;
  unsigned short *cursor;
  unsigned int key_copy[4];
  unsigned int i;

  assert_halt_msg(msgptr != (unsigned short *)0 && key != (unsigned int *)0,
                  "msgptr && key");

  hdr = *msgptr;
  if ((hdr & 1) != 0) {
    blocks = (unsigned int)(((unsigned short)(hdr >> 4) - 2) >> 3);
    remain = (unsigned int)((unsigned char)((char)((hdr >> 4) - 2)) & 7);
    cursor = msgptr + 1;
    key_copy[0] = key[0];
    key_copy[1] = key[1];
    key_copy[2] = key_copy[0];
    key_copy[3] = key_copy[1];

    if ((short)blocks != 0) {
      i = (unsigned int)(blocks & 0xffff);
      do {
        tea_decrypt((unsigned int *)cursor, (unsigned int *)cursor,
                    (int *)key_copy);
        cursor = cursor + 4;
        i = i - 1;
      } while (i != 0);
      i = 0;
    }
    if ((short)remain != 0) {
      key_message_xor_keystream((int)cursor, (int)(short)remain, (int)key, 8);
    }
    assert_halt_msg(!(3 < (hdr & 2)),
                    "(0<=flags) && ((flags)<=MESSAGE_FLAG_BITS_MASK)");
    *msgptr = (*msgptr & 0xfffc) | (hdr & 2);
  }
}

/* 0x80d50 - Sieve of Eratosthenes: return an ascending array of primes <=
 * limit. Allocates an array of all odd candidates (3, 5, 7, ...) plus 2, runs
 * the sieve, appends 2, sorts with qsort(prime_compare), then shrinks the
 * allocation to *num_primes elements via debug_realloc.
 * Returns NULL if limit < 2 or if malloc fails; *num_primes is set on all
 * paths. */
unsigned int *sieve_of_eratosthenes(unsigned int limit,
                                    unsigned int *num_primes)
{
  unsigned int count;
  unsigned int *primes;
  unsigned int uVar3;
  unsigned int p;
  unsigned int local_c;
  unsigned int local_8;
  unsigned int uVar5;
  unsigned int *puVar6;

  count = limit >> 1;
  if ((limit & 1) == 0) {
    count = count - 1;
  }
  uVar5 = 0;
  local_8 = 0;

  assert_halt_msg(num_primes != (unsigned int *)0, "num_primes");

  if (limit < 2) {
    *num_primes = 0;
    return (unsigned int *)0;
  }

  uVar3 = count + 1;
  *num_primes = uVar3;

  primes =
    (unsigned int *)debug_malloc(count * 4 + 4, 0, "prime_numbers.c", 0x47);
  if (primes != (unsigned int *)0) {
    p = 3;
    uVar3 = (unsigned int)(int)sqrtf((float)(int)limit);
    if (count == 0) {
      local_8 = 0;
    } else {
      do {
        primes[uVar5] = p;
        uVar5++;
        p += 2;
      } while (uVar5 < count);
      do {
        if (uVar3 < primes[local_8])
          break;
        local_8++;
      } while (local_8 < count);
    }
    if (local_8 != 0) {
      uVar5 = 1;
      puVar6 = primes;
      local_c = local_8;
      do {
        if (*puVar6 != 0) {
          for (p = uVar5; p < count; p++) {
            if (primes[p] != 0 && primes[p] % *puVar6 == 0) {
              primes[p] = 0;
              *num_primes = *num_primes - 1;
            }
          }
        }
        uVar5++;
        puVar6++;
        local_c--;
      } while (local_c != 0);
    }
    primes[count] = 2;
    qsort(primes, count + 1, 4,
          (int (*)(const void *, const void *))prime_compare);
    if (*num_primes < count + 1) {
      primes = (unsigned int *)debug_realloc(primes, (int)(*num_primes << 2),
                                             "prime_numbers.c", 0x75);
    }
  }
  return primes;
}
