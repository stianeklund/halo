void collision_log_initialize(void)
{
  csmemset((void *)0x5a5e40, 0, 0x2298);
  assert_halt(*(int16_t *)0x4761d8 < 0x20);
  *(int16_t *)(0x5a8c80 + *(int16_t *)0x4761d8 * 2) = 0;
  *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 + 1;
}

/* Helper shared by begin/continue_period. Asserts no period is active,
 * validates time_period is in [0,3), zeros the scratch buffer at 0x5a80e0,
 * and sets the current period. time_period passed in ESI. */
void collision_log_period_helper(int time_period /* @<esi> */, int begin_flag)
{
  int16_t period = (int16_t)time_period;

  if (*(int16_t *)0x325058 != -1) {
    display_assert("collision_usage_current_period == NONE",
                   "c:\\halo\\SOURCE\\physics\\collision_usage.c", 0xa7, 1);
    system_exit(-1);
  }
  if (period < 0 || period >= 3) {
    display_assert(
      "(time_period >= 0) && (time_period < NUMBER_OF_COLLISION_TIME_PERIODS)",
      "c:\\halo\\SOURCE\\physics\\collision_usage.c", 0xa8, 1);
    system_exit(-1);
  }
  csmemset((void *)0x5a80e0, 0, 0xb88);
  *(int16_t *)0x325058 = period;
}

void collision_log_begin_period(__int16 time_period)
{
  collision_log_period_helper((int)time_period, 1);
}

void collision_log_continue_period(__int16 time_period)
{
  collision_log_period_helper((int)time_period, 0);
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
