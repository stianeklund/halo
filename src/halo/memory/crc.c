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
    asm volatile("movl $0x46e400, %%edx\n\t"
                 "movl $0x1190c0, %%eax\n\t"
                 "call *%%eax" ::
                   : "eax", "ecx", "edx", "memory", "cc");
    *(uint8_t *)0x46E800 = 1;
  }

  {
    uint32_t init_value = *checksum;
    int orig_size = size;
    buffer = (uint8_t *)data;
    value = init_value;
    while (size > 0) {
      value = (value >> 8) ^ ((uint32_t *)0x46E400)[(*buffer ^ value) & 0xFF];
      buffer++;
      size--;
    }
    *checksum = value;

    if (orig_size > 0x100) {
      error(2, "CRC: in=%08x out=%08x size=0x%x",
            init_value, value, orig_size);
    }
  }
}
