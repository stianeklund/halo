void crc_checksum_buffer(uint32_t *checksum, void *data, int size)
{
  uint8_t *buffer;
  uint32_t value;

  if (size < 0) {
    display_assert("buffer_size>=0", "c:\\halo\\SOURCE\\memory\\crc.c", 0x2A,
                   true);
    system_exit(-1);
  }

  /* initialize the CRC lookup table on first use;
   * the init function reads EDX as the table base address */
  if (*(uint8_t *)0x46E800 == 0) {
    register int _edx asm("edx") = 0x46E400;
    asm volatile("" : "+r"(_edx));
    ((void (*)(void))0x1190C0)();
    *(uint8_t *)0x46E800 = 1;
  }

  buffer = (uint8_t *)data;
  value = *checksum;
  while (size > 0) {
    value = (value >> 8) ^ ((uint32_t *)0x46E400)[(*buffer ^ value) & 0xFF];
    buffer++;
    size--;
  }

  *checksum = value;
}
