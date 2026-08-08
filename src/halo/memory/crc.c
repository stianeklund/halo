void crc_new(uint32_t *checksum)
{
  *checksum = 0xFFFFFFFF;
}

/* Generate the standard CRC32 lookup table (polynomial 0xEDB88320).
 * Fills 256 entries at the given table pointer.
 * table pointer passed in EDX (register arg). */
#if defined(MSVC) && !defined(__clang__)
#pragma optimize("y", on)
#endif
void crc_table_init(uint32_t *table /* @<edx> */)
{
  uint32_t crc;
  int i, j, count;

  i = 0;
  count = 256;
  do {
    crc = (uint32_t)i;
    j = 8;
    do {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320;
      else
        crc = crc >> 1;
      j--;
    } while (j != 0);
    *table++ = crc;
    i++;
    count--;
  } while (count != 0);
}
#if defined(MSVC) && !defined(__clang__)
#pragma optimize("y", off)
#endif

void crc_checksum_buffer(uint32_t *checksum, void *data, int size)
{
  uint8_t *buffer;
  uint32_t value;

  if (size < 0) {
    display_assert("buffer_size>=0", "c:\\halo\\SOURCE\\memory\\crc.c", 0x2A,
                   true);
    system_exit(-1);
  }

  /* initialize the CRC lookup table on first use */
  if (*(uint8_t *)0x46E800 == 0) {
#if defined(MSVC) && !defined(__clang__)
    /* VC71 models the original EDX register-argument call this way. */
    ((void(__fastcall *)(int))crc_table_init)(0x46E400);
#else
    crc_table_init((uint32_t *)0x46E400);
#endif
    *(uint8_t *)0x46E800 = 1;
  }

  {
    uint32_t init_value = *checksum;
    // int orig_size = size;
    buffer = (uint8_t *)data;
    value = init_value;
    while (size > 0) {
      value = (value >> 8) ^ ((uint32_t *)0x46E400)[(*buffer ^ value) & 0xFF];
      buffer++;
      size--;
    }
    *checksum = value;

    /* if (orig_size > 0x100) {
      error(2, "CRC: in=%08x out=%08x size=0x%x",
            init_value, value, orig_size);
    }
    */
  }
}
