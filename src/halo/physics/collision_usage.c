void collision_log_initialize(void)
{
  csmemset((void *)0x5a5e40, 0, 0x2298);
  assert_halt(*(int16_t *)0x4761d8 < 0x20);
  *(int16_t *)(0x5a8c80 + *(int16_t *)0x4761d8 * 2) = 0;
  *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 + 1;
}

void collision_log_begin_period(__int16 time_period)
{
  /* helper at 0x14d080 takes time_period in ESI register */
  __asm__ volatile("pushl $1\n\t"
                   "call *%0\n\t"
                   "addl $4, %%esp"
                   :
                   : "r"((void *)0x14d080), "S"((int)time_period)
                   : "eax", "ecx", "edx", "memory");
}

void collision_log_continue_period(__int16 time_period)
{
  /* helper at 0x14d080 takes time_period in ESI register */
  __asm__ volatile("pushl $0\n\t"
                   "call *%0\n\t"
                   "addl $4, %%esp"
                   :
                   : "r"((void *)0x14d080), "S"((int)time_period)
                   : "eax", "ecx", "edx", "memory");
}

void collision_log_end_period(void)
{
  int16_t period;
  int i;
  int *src;
  int *dst;

  period = *(int16_t *)0x325058;
  assert_halt(period >= 0 && period < 3);
  *(char *)0x5a80e0 = 1;
  src = (int *)0x5a80e0;
  dst = (int *)(0x5a5e40 + (int)period * 0xb88);
  for (i = 0x2e2; i != 0; i--) {
    *dst++ = *src++;
  }
  *(int16_t *)0x325058 = -1;
}
