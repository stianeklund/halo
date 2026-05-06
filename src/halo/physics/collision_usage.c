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

/* 0x14d940 — wraps QueryPerformanceCounter (FUN_001d33e6) */
void collision_log_query_counter(void *counter)
{
  FUN_001d33e6(counter);
}

/* 0x14d950 — records elapsed time for a collision function type.
 * Computes (current_time - start) as 64-bit and accumulates into
 * per-type and per-user counters in the scratch buffer at 0x5a80e0. */
void collision_log_add_time(short collision_function, unsigned int start_lo,
                            int start_hi)
{
  unsigned int current[2];
  short user;
  unsigned int elapsed_lo;
  int elapsed_hi;
  int type_off;
  int user_off;
  unsigned int prev;

  FUN_001d33e6(current);

  user = FUN_0014d840(collision_function);
  if (user == -1)
    return;

  elapsed_lo = current[0] - start_lo;
  elapsed_hi = ((int)current[1] - start_hi) - (unsigned int)(current[0] < start_lo);

  type_off = (int)collision_function * 0x170;
  prev = *(unsigned int *)(0x5a80f0 + type_off);
  *(unsigned int *)(0x5a80f0 + type_off) = prev + elapsed_lo;
  *(int *)(0x5a80f4 + type_off) +=
    elapsed_hi + (unsigned int)((prev + elapsed_lo) < prev);

  user_off = ((int)collision_function * 0x17 + (int)user) * 0x10;
  prev = *(unsigned int *)(0x5a8100 + user_off);
  *(unsigned int *)(0x5a8100 + user_off) = prev + elapsed_lo;
  *(int *)(0x5a8104 + user_off) +=
    elapsed_hi + (unsigned int)((prev + elapsed_lo) < prev);
}

/* 0x14d9d0 — increments call count for a collision function type */
void collision_log_add_call(short collision_function)
{
  short user;

  user = FUN_0014d840(collision_function);
  if (user == -1)
    return;

  *(int *)(0x5a80e8 + (int)collision_function * 0x170) += 1;
  *(int *)(0x5a80f8 + ((int)collision_function * 0x17 + (int)user) * 0x10) += 1;
}
