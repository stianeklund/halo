void crc_new(uint32_t *checksum)
{
  *checksum = 0xFFFFFFFF;
}

/* Generate the standard CRC32 lookup table (polynomial 0xEDB88320).
 * Fills 256 entries at the given table pointer.
 * table pointer passed in EDX (register arg). */
void crc_table_init(uint32_t *table /* @<edx> */)
{
  uint32_t crc;
  int i, j;

  for (i = 0; i < 256; i++) {
    crc = (uint32_t)i;
    for (j = 0; j < 8; j++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320;
      else
        crc = crc >> 1;
    }
    *table++ = crc;
  }
}

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
    crc_table_init((uint32_t *)0x46E400);
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
