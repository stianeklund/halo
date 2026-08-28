#include "../../common.h"

/* Global: pointer to key_agreement_packets group definition at 0x2ee588. */
#define key_agreement_group ((void *)0x2ee588)

/* 0x80380 - Decode a key-agreement packet: thin wrapper around
 * decode_packet_group (FUN_0011aa40, data_packet_groups.c) bound to the
 * key_agreement_group definition, the decode twin of 0x803d0's
 * encode_packet_group call below. Three params (packet_type,
 * packet_version, expected_packet_class) arrive in EDX/ECX/EAX
 * (binary-proven: PUSH EDX/ECX/EAX at entry save the incoming register
 * values before EAX/ECX/EDX are reloaded from the stack args, and those
 * saved copies are the last three cdecl pushes before the CALL). The
 * function does not touch EAX after the CALL, so FUN_0011aa40's bool
 * return value passes through unmodified (implicit-EAX return, no
 * comparison in the wrapper). */
bool FUN_00080380(void *decoded_packet, char *encoded_packet,
                  short *encoded_packet_size, short *packet_type /* @<edx> */,
                  short *packet_version /* @<ecx> */,
                  short expected_packet_class /* @<eax> */)
{
  return FUN_0011aa40((int)key_agreement_group, decoded_packet, encoded_packet,
                      encoded_packet_size, packet_type, packet_version,
                      expected_packet_class);
}

/* 0x803b0 - Encode a key-agreement packet: thin wrapper around
 * encode_packet_group (FUN_0011aca0, data_packet_groups.c) bound to the
 * key_agreement_group definition, the encode twin of 0x80380's
 * decode_packet_group call above. Two params (data, encoded_packet) arrive
 * on the stack at [EBP+8]/[EBP+0xC]; the other three (encoded_packet_size,
 * packet_type, version) arrive in EDX/ECX/EAX (binary-proven: PUSH
 * EDX/ECX/EAX at entry save the incoming register values before EAX/ECX are
 * reloaded from the stack args and re-pushed for encoded_packet/data, and
 * the saved EDX/ECX/EAX copies are the first three cdecl pushes before the
 * CALL, i.e. the last three params: encoded_packet_size, packet_type,
 * version). The function does not touch EAX after the CALL, so
 * encode_packet_group's bool return value passes through unmodified
 * (implicit-EAX return, no comparison in the wrapper). */
bool FUN_000803b0(void *data, char *encoded_packet,
                  short *encoded_packet_size /* @<edx> */,
                  short packet_type /* @<ecx> */, short version /* @<eax> */)
{
  return encode_packet_group((group_definition *)key_agreement_group, data,
                             encoded_packet, encoded_packet_size, packet_type,
                             version);
}

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

  /* encode_packet_group's size parameter is short * (binary-proven);
   * `encoded_size` stays a dword local because 0x803d0 stores/loads it with
   * dword ops (MOV dword [EBP-4],0x80 at 0x80410). */
  if (encode_packet_group((group_definition *)key_agreement_group, data,
                          (char *)encoded_buf, (short *)&encoded_size, type,
                          1)) {
    msg = (unsigned short *)create_message(
      3, (int)encoded_buf, (unsigned int)encoded_size, buffer, buffer_size);
    if (msg != (unsigned short *)0) {
      *msg = (*msg & 0xfffe) | 2;
      return msg;
    }
  }
  return (unsigned short *)0;
}

/* 0x80470 - Pack prime/g/key values and build a key-agreement message.
 * Copies two dwords (8 bytes) from each of prime, g, key (register args,
 * binary-proven: TEST ESI/EBX/EDI at entry, tested in that order matching
 * the assert text "prime && g && key") into a contiguous 24-byte stack
 * buffer in that order (prime, then g, then key), then forwards buffer and
 * buffer_size unchanged to key_agreement_build_message with message type 0
 * and the packed buffer as data. Return value is discarded (binary-proven:
 * no test of EAX after the CALL). Asserts and halts if prime, g, or key is
 * NULL. */
void FUN_00080470(int buffer, unsigned short buffer_size,
                  unsigned int *prime /* @<esi> */,
                  unsigned int *g /* @<ebx> */, unsigned int *key /* @<edi> */)
{
  unsigned int packed_data[6];

  assert_halt_msg(prime != (unsigned int *)0 && g != (unsigned int *)0 &&
                    key != (unsigned int *)0,
                  "prime && g && key");

  packed_data[0] = prime[0];
  packed_data[1] = prime[1];
  packed_data[2] = g[0];
  packed_data[3] = g[1];
  packed_data[4] = key[0];
  packed_data[5] = key[1];

  key_agreement_build_message(0, packed_data, buffer, buffer_size);
}

/* 0x804e0 - Pack a key value and build a key-agreement message (single-value
 * variant of FUN_00080470: one 8-byte operand instead of the prime/g/key
 * triple). Copies two dwords (8 bytes) from key (register arg, binary-proven:
 * TEST ESI at entry, tested against assert text "key") into an 8-byte stack
 * buffer, then forwards buffer and buffer_size unchanged to
 * key_agreement_build_message with message type 1 and the packed buffer as
 * data. Return value is discarded (binary-proven: no test of EAX after the
 * CALL). Asserts and halts if key is NULL. */
void FUN_000804e0(int buffer, unsigned short buffer_size,
                  unsigned int *key /* @<esi> */)
{
  unsigned int packed_data[2];

  assert_halt_msg(key != (unsigned int *)0, "key");

  packed_data[0] = key[0];
  packed_data[1] = key[1];

  key_agreement_build_message(1, packed_data, buffer, buffer_size);
}

/* ========================================================================
 * message_header.c — Message header, encryption, key agreement, prime sieve
 * Original source: c:\halo\SOURCE\bungie_net\common\message_header.c
 *                  c:\halo\SOURCE\bungie_net\common\message_encryption.c
 *                  c:\halo\SOURCE\bungie_net\common\key_agreement.c
 *                  c:\halo\SOURCE\bungie_net\common\prime_numbers.c
 * ======================================================================== */

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
      *(unsigned char *)(iVar3 + msg) =
        ~(*pbVar1 ^ *(unsigned char *)(iVar3 + msg));
      iVar2 = iVar2 + iVar4;
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
  int uVar2;
  int iVar3;
  unsigned int count;

  uVar1 = v[0];
  uVar2 = v[1];
  iVar3 = 0;
  count = 0x20;

  do {
    iVar3 = iVar3 + (int)0x9E3779B9u; /* -0x61c88647 mod 2^32 */
    uVar1 += (((uVar2 << 4) + key[0]) ^ (uVar2 + (unsigned int)iVar3)) ^
             ((uVar2 >> 5) + key[1]);
    uVar2 += (((uVar1 << 4) + key[2]) ^ (uVar1 + (unsigned int)iVar3)) ^
             ((uVar1 >> 5) + key[3]);
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
  int uVar1;
  unsigned int uVar2;
  int iVar3;
  unsigned int count;

  uVar2 = v[0];
  uVar1 = v[1];
  iVar3 = (int)0xC6EF3720u; /* -0x3910c8e0 mod 2^32 */
  count = 0x20;

  do {
    uVar1 -= (((uVar2 << 4) + key[2]) ^ (uVar2 + (unsigned int)iVar3)) ^
             ((uVar2 >> 5) + key[3]);
    uVar2 -= (((uVar1 << 4) + key[0]) ^ (uVar1 + (unsigned int)iVar3)) ^
             ((uVar1 >> 5) + key[1]);
    iVar3 = iVar3 + 0x61c88647;
    count = count - 1;
  } while (count != 0);

  w[0] = uVar2;
  w[1] = uVar1;
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

  *header = (*header & 0xf) | (length << 4);

  if (type == 0) {
    goto bad_type;
  }
  if (type < 4) {
    goto good_type;
  }
bad_type:
  /* assert_halt_msg(0,...) is noreturn (system_exit); control never falls
   * through into good_type. Layout matches the original's forward branches. */
  assert_halt_msg(0, "(0<(type)) && ((type)<NUMBER_OF_MESSAGE_TYPES)");

good_type:
  *header = ((unsigned short)(type & 3) << 2) | (*header & 0xfff3);
  assert_halt_msg((0 <= (int)flags) && ((int)flags <= 3),
                  "(0<=flags) && ((flags)<=MESSAGE_FLAG_BITS_MASK)");
  *header = (*header & 0xfffc) | (unsigned short)flags;
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
    goto do_malloc;
  }
  if ((int)(unsigned int)buffer_size >= (int)msg_size) {
    goto after_malloc;
  }
  /* assert_halt_msg(0,...) is noreturn (system_exit); control never falls
   * through into do_malloc. The malloc block is hoisted after the size check
   * so buffer==0 forward-jumps to it, matching the original's layout. */
  assert_halt_msg(0, "buffer_size >= message_size");

do_malloc:
  buffer = (int)debug_malloc(
    (int)msg_size, 0, "c:\\halo\\SOURCE\\bungie_net\\common\\message_header.c",
    0x2e);

after_malloc:
  if (buffer != 0) {
    build_message_header((unsigned short *)buffer, payload_len + 2,
                         (unsigned char)type, 0);
    if (payload != 0) {
      csmemcpy((void *)(buffer + 2), (void *)payload,
               (unsigned short)payload_len);
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
  if (*a > *b) {
    return -1;
  }
  return 0;
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

/* 0x80eb0 - Build a sieved prime table for [2, limit], pick one entry at a
 * pseudo-random index via FUN_00081410(0, num_primes-1), then free the whole
 * table with debug_free. Returns the picked prime, or 0 if the sieve
 * produced no table. */
unsigned int FUN_00080eb0(unsigned int limit)
{
  unsigned int *primes;
  unsigned int num_primes;
  unsigned int index;
  unsigned int result;

  result = 0;
  primes = sieve_of_eratosthenes(limit, &num_primes);
  if (primes != (unsigned int *)0) {
    index = (unsigned int)FUN_00081410(0, (int)(num_primes - 1));
    result = primes[index];
    debug_free(primes, "c:\\halo\\SOURCE\\bungie_net\\common\\prime_numbers.c",
               0x89);
  }
  return result;
}

/* A 64-bit value addressable both as a plain qword (the exit range check in
 * FUN_00080fc0 reproduces the original `s.qword <= 0xFFFFFFFF` compare,
 * recovered verbatim from the assert message string) and as four 16-bit
 * limbs (the math64_multiply/math64_add/math64_divide calls below take
 * `uint16_t *`). Shared by FUN_00080f00 and FUN_00080fc0. Local to this TU;
 * not 64bit_math.c's math64_half_t, which has no scalar view. */
typedef union {
  uint64_t qword;
  uint16_t word[4];
} math64_qword_t;

/* 0x80f00 - Compute a pseudo-random 64-bit "key" into *result: start at 1,
 * then for four iterations sieve primes <=0xffff, pick one at a
 * pseudo-random index, multiply it into the accumulator, and free the
 * table -- the same sieve_of_eratosthenes + FUN_00081410 + debug_free
 * sequence FUN_00080eb0 uses, inlined here per iteration rather than
 * calling it. Finally adds 2. assert_halt_msg's recovered text ("result")
 * is the parameter's original name. `result` is declared uint16_t * (like
 * math64_multiply/math64_add) rather than math64_qword_t * so the global
 * decl.h prototype does not depend on this TU-local typedef; acc
 * reinterprets it in place so every write below lands directly in the
 * caller's buffer, matching the original's direct [EBX]/[EBX+4] stores. */
void FUN_00080f00(uint16_t *result)
{
  math64_qword_t *acc;
  unsigned int *primes;
  unsigned int num_primes;
  unsigned int index;
  unsigned int prime_value;
  unsigned int count;
  math64_qword_t prime;
  math64_qword_t two;

  assert_halt_msg(result != (uint16_t *)0, "result");

  acc = (math64_qword_t *)result;
  acc->qword = 1;

  count = 4;
  do {
    prime_value = 0;
    primes = sieve_of_eratosthenes(0xffff, &num_primes);
    if (primes != (unsigned int *)0) {
      index = (unsigned int)FUN_00081410(0, (int)(num_primes - 1));
      prime_value = primes[index];
      debug_free(primes,
                 "c:\\halo\\SOURCE\\bungie_net\\common\\prime_numbers.c", 0x89);
    }
    prime.qword = prime_value;
    math64_multiply(acc->word, prime.word, acc->word);
  } while (--count != 0);

  two.qword = 2;
  math64_add(acc->word, two.word, acc->word);
}

/* 0x80fc0 - 32-bit modular exponentiation: base^exponent mod modulus via
 * binary square-and-multiply, computed into a 64-bit accumulator that is
 * never returned to the caller (void, no output parameter). This TU
 * (public_key_crypt.c) is unfinished RSA-support scaffolding the 2276 beta
 * does not appear to exercise -- consistent with the seeded/broken
 * math64_multiply and operand-swapped math64_divide primitives it calls
 * (see 64bit_math.c), which make the "product" this function computes not
 * an actual modular power.
 *
 * Confirmed (0x80fc0-0x81080):
 *  - Register args, no stack params: exponent@<eax> (aliased into EBX for
 *    the whole body), base@<ecx>, modulus@<edx>. All three are 32-bit
 *    values, zero-extended into a 64-bit slot (dword pair, high dword
 *    forced to 0) before being handed to the 64bit_math.c helpers.
 *  - `TEST EBX,EBX` at entry (0x80fc9) is tested by the `JZ 0x8107c` at
 *    0x80fee -- i.e. exponent==0 skips the loop entirely -- but the flags
 *    survive three unrelated MOV stores (the s/base/modulus initialization)
 *    in between, so those inits run unconditionally before the branch.
 *  - Standard binary modexp: multiply-by-base runs only when the current
 *    exponent bit is set (`TEST BL,1` / `JZ` at 0x80ff4/0x80ff7); square
 *    runs every iteration unconditionally. `SHR EBX,1` (0x8102c) is
 *    scheduled between the square step's arg pushes and its CALL -- pure
 *    instruction scheduling, since nothing reads the shifted exponent
 *    before the bottom-of-loop continuation test.
 *  - Each multiply+divide pair is two back-to-back cdecl calls sharing one
 *    combined cleanup: `ADD ESP,0x1c` = 0x1c/4 = 7 dwords = multiply's 3
 *    args + divide's 4 args, not a single 7-arg call.
 *  - math64_divide's quotient argument is a literal `PUSH 0x0` (NULL) at
 *    both call sites -- only the remainder output is used:
 *    `accumulator = product % modulus` (subject to math64_divide's own
 *    documented numerator/denominator swap).
 *  - Exit range check (0x8104d-0x81052) is the textbook two-step expansion
 *    of a 64-bit-vs-32-bit-constant compare (`accumulator > 0xFFFFFFFF`,
 *    whose constant has high dword 0): `TEST` the accumulator's high dword
 *    / `JA` assert; only reached when that's false does it fall through to
 *    `CMP` the low dword against -1 / `JBE` exit (always true for a 32-bit
 *    value, so the low-dword compare is dead in practice -- an artifact of
 *    the constant's shape, not a second condition). The recovered assert
 *    string at 0x265d88 is literally "s.qword <= 0xFFFFFFFF", which both
 *    confirms this reading and gives the accumulator's original name.
 *  - display_assert args at 0x8106a: __FILE__ 0x265da0 =
 *    "c:\halo\SOURCE\bungie_net\common\public_key_crypt.c", line 0x5f.
 */
void FUN_00080fc0(uint32_t exponent /* @<eax> */, uint32_t base /* @<ecx> */,
                  uint32_t modulus /* @<edx> */)
{
  math64_qword_t s;
  math64_qword_t b;
  math64_qword_t m;
  math64_qword_t product;

  s.qword = 1;
  b.qword = base;
  m.qword = modulus;

  while (exponent != 0) {
    if ((exponent & 1) != 0) {
      math64_multiply(s.word, b.word, product.word);
      math64_divide(product.word, m.word, NULL, s.word);
    }

    exponent >>= 1;
    math64_multiply(b.word, b.word, product.word);
    math64_divide(product.word, m.word, NULL, b.word);
  }

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                 0x5f, s.qword <= 0xFFFFFFFF);
}

/* 0x81090 - validate Diffie-Hellman parameters then compute modexp.
 * Register-arg entry point (no stack params, no prologue): callers set
 * p@esi, x@ebx, g@edi before calling in. All three asserts recovered
 * verbatim from cachebeta.xbe .rdata at the same __FILE__ FUN_00080fc0
 * uses (c:\halo\SOURCE\bungie_net\common\public_key_crypt.c):
 *   0x70 "p>2", 0x71 "x<(p-1)", 0x72 "g<p".
 * After validation, tail-calls FUN_00080fc0 (modexp: base^exponent mod
 * modulus) with x/g/p in FUN_00080fc0's declared register slots
 * (exponent@eax, base@ecx, modulus@edx) -- confirmed by the MOV EDX,ESI /
 * MOV EAX,EBX / MOV ECX,EDI register shuffle immediately before the
 * original JMP 0x80fc0 tail call. */
void FUN_00081090(uint32_t p /* @<esi> */, uint32_t x /* @<ebx> */,
                  uint32_t g /* @<edi> */)
{
  assert_halt_msg_at("p>2",
                     "c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                     0x70, p > 2);
  assert_halt_msg_at("x<(p-1)",
                     "c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                     0x71, x < (p - 1));
  assert_halt_msg_at("g<p",
                     "c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                     0x72, g < p);

  FUN_00080fc0(x, g, p);
}

/* 0x81110 - Validate Diffie-Hellman exponent/modulus then compute modexp.
 * Standard cdecl prologue (PUSH EBP/MOV EBP,ESP, no locals): one stack
 * param g@[EBP+8], plus register args p@esi, x@edi (binary-proven: CMP
 * ESI,0x2 / LEA EAX,[ESI-1] / CMP EDI,EAX operate directly on the incoming
 * register values, no reload from a stack slot). Only two of the three
 * FUN_00081090 checks are present here (no g<p check). Both asserts
 * recovered verbatim from cachebeta.xbe .rdata at the same __FILE__
 * FUN_00080fc0/FUN_00081090 use
 * (c:\halo\SOURCE\bungie_net\common\public_key_crypt.c): 0x85 "p>2" (string
 * bytes read directly from the pristine XBE at VA 0x265de0, confirmed "p>2\0"
 * -- Ghidra's decompile showed it as an unresolved PTR_DAT because it
 * immediately follows "x<(p-1)\0" at 0x265dd8 in .rdata, not because it's a
 * different string), 0x86 "x<(p-1)". After validation, tail-calls FUN_00080fc0
 * (modexp: base^exponent mod modulus) with x/g/p in FUN_00080fc0's declared
 *   register slots (exponent@eax, base@ecx, modulus@edx) -- confirmed by
 *   the MOV ECX,[EBP+8] / MOV EDX,ESI / MOV EAX,EDI register shuffle
 *   immediately before the original JMP 0x80fc0 tail call. */
void FUN_00081110(uint32_t g, uint32_t x /* @<edi> */, uint32_t p /* @<esi> */)
{
  assert_halt_msg_at("p>2",
                     "c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                     0x85, p > 2);
  assert_halt_msg_at("x<(p-1)",
                     "c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                     0x86, x < (p - 1));

  FUN_00080fc0(x, g, p);
}
