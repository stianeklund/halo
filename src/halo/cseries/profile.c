/* Profile timing infrastructure. Uses RDTSC to measure CPU cycles and
 * converts to milliseconds using the stored CPU frequency at 0x3361a0.
 * All timing data is accumulated into global profiling structures. */

/* Read the x86 timestamp counter (RDTSC) into a low/high dword pair. */
#ifdef _MSC_VER
#define RDTSC(lo, hi)                             \
  do {                                            \
    uint32_t _lo, _hi;                            \
    __asm { rdtsc }                               \
    __asm { mov _lo, eax }                        \
    __asm { mov _hi, edx }                        \
    (lo) = _lo;                                   \
    (hi) = _hi;                                   \
  } while (0)
#else
#define RDTSC(lo, hi)                             \
  do {                                            \
    uint32_t _lo, _hi;                            \
    asm volatile("rdtsc" : "=a"(_lo), "=d"(_hi)); \
    (lo) = _lo;                                   \
    (hi) = _hi;                                   \
  } while (0)
#endif

/* Compute elapsed milliseconds from a 64-bit cycle difference.
 * Formula: (float)(int64_t)cycles * scale / (float)cpu_freq */
static float cycles_to_msec(uint32_t lo, uint32_t hi)
{
  int64_t diff;
  uint32_t *p = (uint32_t *)&diff;
  p[0] = lo;
  p[1] = hi;
  return (float)diff * *(float *)0x254cb8 / (float)*(int64_t *)0x3361a0;
}

/* Enter a profiling section. Records the current timestamp and pushes
 * the section onto the profiling stack. */
void profile_enter_private(void *section)
{
  char *s = (char *)section;
  uint32_t lo, hi;

  ((void (*)(void *))0x8f8e0)(section);

  if (*(int16_t *)(s + 0xa) != -1) {
    display_assert("section->stack_depth==NONE",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x255, 1);
    system_exit(-1);
  }

  *(int16_t *)0x3361a8 += 1;
  *(int16_t *)(s + 0xa) = *(int16_t *)0x3361a8;

  RDTSC(lo, hi);
  *(uint32_t *)(s + 0x10) = lo;
  *(uint32_t *)(s + 0x14) = hi;
  *(int *)(s + 0x5cc) += 1;
}

/* Exit a profiling section. Computes elapsed cycles and accumulates
 * them into the section's 64-bit total. */
void profile_exit_private(void *section)
{
  char *s = (char *)section;

  if (*(uint8_t *)0x3361aa != 0) {
    *(int16_t *)(s + 0xa) = -1;
    return;
  }

  ((void (*)(void *))0x8f8e0)(section);

  if (*(int16_t *)(s + 0xa) != *(int16_t *)0x3361a8) {
    display_assert("section->stack_depth==profile_globals.stack_depth",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x267, 1);
    system_exit(-1);
  }

  *(int16_t *)0x3361a8 -= 1;

  {
    uint32_t lo, hi;
    uint32_t diff_lo, diff_hi;
    uint32_t old_lo;

    RDTSC(lo, hi);
    diff_lo = lo - *(uint32_t *)(s + 0x10);
    diff_hi = hi - *(uint32_t *)(s + 0x14) - (lo < *(uint32_t *)(s + 0x10));

    old_lo = *(uint32_t *)(s + 0x5d0);
    *(uint32_t *)(s + 0x5d0) = old_lo + diff_lo;
    *(uint32_t *)(s + 0x5d4) += diff_hi + (*(uint32_t *)(s + 0x5d0) < old_lo);
  }

  *(int16_t *)(s + 0xa) = -1;
}

/* Start timing a game tick. Increments the tick counter and records
 * the start timestamp in the tick timing array. */
void profile_tick_start(void)
{
  int idx;
  uint32_t lo, hi;

  if (*(uint8_t *)0x449ef0 != 0)
    ((void (*)(void))0x8f6b0)();

  if (*(int16_t *)0x448dd8 < 0x96)
    *(int16_t *)0x448dd8 += 1;

  if (*(int16_t *)0x448dd8 < 1 || *(int16_t *)0x448dd8 > 0x96) {
    display_assert("(profile_globals.current_frame.game_tick_count > 0) && "
                   "(profile_globals.current_frame.game_tick_count <= "
                   "MAXIMUM_GAME_TICKS_PER_FRAME)",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x137, 1);
    system_exit(-1);
  }

  idx = (int)*(int16_t *)0x448dd8;
  RDTSC(lo, hi);
  *(uint32_t *)(0x448de0 + idx * 0x18) = lo;
  *(uint32_t *)(0x448de4 + idx * 0x18) = hi;
}

/* End timing a game tick. Computes elapsed msec and accumulates. */
void profile_tick_end(void)
{
  int idx;
  uint32_t lo, hi, start_lo, start_hi, diff_lo, diff_hi;
  float elapsed;

  if (*(int16_t *)0x448dd8 < 1 || *(int16_t *)0x448dd8 > 0x96) {
    display_assert("(profile_globals.current_frame.game_tick_count > 0) && "
                   "(profile_globals.current_frame.game_tick_count <= "
                   "MAXIMUM_GAME_TICKS_PER_FRAME)",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x140, 1);
    system_exit(-1);
  }

  idx = (int)*(int16_t *)0x448dd8;
  RDTSC(lo, hi);

  start_lo = *(uint32_t *)(0x448de0 + idx * 0x18);
  start_hi = *(uint32_t *)(0x448de4 + idx * 0x18);
  *(uint32_t *)(0x448de8 + idx * 0x18) = lo;
  *(uint32_t *)(0x448dec + idx * 0x18) = hi;

  diff_lo = lo - start_lo;
  diff_hi = hi - start_hi - (lo < start_lo);
  elapsed = cycles_to_msec(diff_lo, diff_hi);

  *(float *)(0x448df0 + idx * 0x18) += elapsed;
  *(float *)(0x448df4 + idx * 0x18) += elapsed;
}

/* Start timing the render phase. Resets window count and records
 * the render start timestamp. */
void profile_render_start(void)
{
  uint32_t lo, hi;
  *(int16_t *)0x448dda = 0;
  RDTSC(lo, hi);
  *(uint32_t *)0x449c68 = lo;
  *(uint32_t *)0x449c6c = hi;
}

/* End timing the render phase. Computes elapsed msec. */
void profile_render_end(void)
{
  uint32_t lo, hi, diff_lo, diff_hi;
  float elapsed;

  RDTSC(lo, hi);
  *(uint32_t *)0x449c70 = lo;
  *(uint32_t *)0x449c74 = hi;

  diff_lo = lo - *(uint32_t *)0x449c68;
  diff_hi = hi - *(uint32_t *)0x449c6c - (lo < *(uint32_t *)0x449c68);
  elapsed = cycles_to_msec(diff_lo, diff_hi);

  *(float *)0x449c78 += elapsed;
  *(float *)0x449c7c += elapsed;
}

/* Start timing a render window. Increments window count, stores the
 * window parameter, and records the start timestamp. */
void profile_render_window_start(char window_param)
{
  int16_t count;
  int idx;
  uint32_t lo, hi;

  count = *(int16_t *)0x448dda;
  if (count < 4) {
    count++;
    *(int16_t *)0x448dda = count;
    *(uint8_t *)(0x448ddb + (int)count) = (uint8_t)window_param;
  }

  if (*(int16_t *)0x448dda < 1 || *(int16_t *)0x448dda > 4) {
    display_assert(
      "(profile_globals.current_frame.window_count > 0) && "
      "(profile_globals.current_frame.window_count <= MAXIMUM_WINDOWS)",
      "c:\\halo\\SOURCE\\cseries\\profile.c", 0x161, 1);
    system_exit(-1);
  }

  idx = (int)*(int16_t *)0x448dda;
  RDTSC(lo, hi);
  *(uint32_t *)(0x449bf0 + idx * 0x18) = lo;
  *(uint32_t *)(0x449bf4 + idx * 0x18) = hi;
}

/* End timing a render window. Computes elapsed msec. */
void profile_render_window_end(void)
{
  int idx;
  uint32_t lo, hi, start_lo, diff_lo, diff_hi;
  float elapsed;

  if (*(int16_t *)0x448dda < 1 || *(int16_t *)0x448dda > 4) {
    display_assert(
      "(profile_globals.current_frame.window_count > 0) && "
      "(profile_globals.current_frame.window_count <= MAXIMUM_WINDOWS)",
      "c:\\halo\\SOURCE\\cseries\\profile.c", 0x16a, 1);
    system_exit(-1);
  }

  idx = (int)*(int16_t *)0x448dda;
  RDTSC(lo, hi);

  start_lo = *(uint32_t *)(0x449bf0 + idx * 0x18);
  *(uint32_t *)(0x449bf8 + idx * 0x18) = lo;
  *(uint32_t *)(0x449bfc + idx * 0x18) = hi;

  diff_lo = lo - start_lo;
  diff_hi = hi - *(uint32_t *)(0x449bf4 + idx * 0x18) - (lo < start_lo);
  elapsed = cycles_to_msec(diff_lo, diff_hi);

  *(float *)(0x449c00 + idx * 0x18) += elapsed;
  *(float *)(0x449c04 + idx * 0x18) += elapsed;
}

/* Snapshot the current TSC into a dedicated low/high global pair at
 * 0x449c98/0x449c9c (used to mark a reference timestamp). */
void FUN_000916e0(void)
{
  uint32_t lo, hi;
  RDTSC(lo, hi);
  *(uint32_t *)0x449c98 = lo;
  *(uint32_t *)0x449c9c = hi;
}

/* Start a new profiling frame. Clears the current frame data, records
 * the render count and timing state, and timestamps the frame start. */
void profile_frame_start(void)
{
  uint32_t lo, hi;

  if (*(uint8_t *)0x449ef0 == 0)
    ((void (*)(void))0x8f6b0)();

  csmemset((void *)0x448dc8, 0, 0x1128);
  *(int *)(0x448dcc) = *(int *)0x506540;
  *(int *)(0x448dd0) = *(int *)0x325678;
  *(int *)(0x448dd4) = *(int *)0x32567c;
  *(int16_t *)0x448dd8 = 0;

  RDTSC(lo, hi);
  *(uint32_t *)0x448de0 = lo;
  *(uint32_t *)0x448de4 = hi;
}

/* End a profiling frame. Computes total frame time, validates and
 * subtracts child section times, copies frame data to the ring buffer,
 * and conditionally outputs profiling information. */
void profile_frame_end(void)
{
  uint32_t lo, hi, diff_lo, diff_hi;
  float frame_elapsed;
  int16_t i;
  int16_t tick_count;
  int16_t window_count;
  uint32_t ring_idx;

  /* compute total frame time */
  RDTSC(lo, hi);
  *(uint32_t *)0x448de8 = lo;
  *(uint32_t *)0x448dec = hi;

  diff_lo = lo - *(uint32_t *)0x448de0;
  diff_hi = hi - *(uint32_t *)0x448de4 - (lo < *(uint32_t *)0x448de0);
  frame_elapsed = cycles_to_msec(diff_lo, diff_hi);
  *(float *)0x448df0 += frame_elapsed;
  *(float *)0x448df4 += frame_elapsed;

  /* validate tick count and subtract child tick times */
  if (*(int16_t *)0x448dd8 < 0 || *(int16_t *)0x448dd8 > 0x96) {
    display_assert("(profile_globals.current_frame.game_tick_count >= 0) && "
                   "(profile_globals.current_frame.game_tick_count <= "
                   "MAXIMUM_GAME_TICKS_PER_FRAME)",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1c0, 1);
    system_exit(-1);
  }

  tick_count = *(int16_t *)0x448dd8;
  for (i = 0; i < tick_count; i++) {
    float child = *(float *)(0x448e08 + (int)i * 0x18);
    if (*(float *)0x448df4 < child) {
      display_assert(
        "parent_timesection->self_msec >= child_timesection->elapsed_msec",
        "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1b2, 1);
      system_exit(-1);
      tick_count = *(int16_t *)0x448dd8;
    }
    *(float *)0x448df4 -= child;
  }

  /* validate window count */
  if (*(int16_t *)0x448dda < 0 || *(int16_t *)0x448dda > 4) {
    display_assert(
      "(profile_globals.current_frame.window_count >= 0) && "
      "(profile_globals.current_frame.window_count <= MAXIMUM_WINDOWS)",
      "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1c6, 1);
    system_exit(-1);
  }

  /* subtract render time from frame self_msec */
  if (*(float *)0x448df4 < *(float *)0x449c78) {
    display_assert(
      "parent_timesection->self_msec >= child_timesection->elapsed_msec",
      "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1b2, 1);
    system_exit(-1);
  }
  *(float *)0x448df4 -= *(float *)0x449c78;

  /* subtract window child times from render self_msec */
  window_count = *(int16_t *)0x448dda;
  for (i = 0; i < window_count; i++) {
    float child = *(float *)(0x449c18 + (int)i * 0x18);
    if (*(float *)0x449c7c < child) {
      display_assert(
        "parent_timesection->self_msec >= child_timesection->elapsed_msec",
        "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1b2, 1);
      system_exit(-1);
    }
    *(float *)0x449c7c -= child;
  }

  /* subtract custom profile time */
  if (*(float *)0x448df4 < *(float *)0x449cc0) {
    display_assert(
      "parent_timesection->self_msec >= child_timesection->elapsed_msec",
      "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1b2, 1);
    system_exit(-1);
  }
  *(float *)0x448df4 -= *(float *)0x449cc0;

  /* copy current frame data to ring buffer */
  qmemcpy((void *)(0x3365c8 + (int)*(int16_t *)0x3365c4 * 0x1128),
          (void *)0x448dc8, 0x1128);

  ring_idx = (uint32_t)((int)*(int16_t *)0x3365c4 + 1);
  if (*(int16_t *)0x3365c2 <= (int16_t)ring_idx)
    *(int16_t *)0x3365c2 = (int16_t)ring_idx;

  /* wrap ring index to 0-255 */
  ring_idx &= 0xff;
  *(int16_t *)0x3365c4 = (int16_t)ring_idx;

  /* handle profile output */
  if (*(uint8_t *)0x449cd4 == 0) {
    *(int16_t *)0x3365bc += 1;
    if (*(uint8_t *)0x449ef3 == 0)
      goto check_output;
    if (*(uint8_t *)0x449ef2 != 0)
      goto do_output;
    if (*(int16_t *)0x3365bc > 3 && *(uint8_t *)0x3365c0 != 0) {
      if (*(void **)0x3365b4 != 0) {
        ((void (*)(void *, const void *))0x1da685)(*(void **)0x3365b4, L"\r\n");
        ((void (*)(void *))0x1d8f31)(*(void **)0x3365b4);
      }
      *(uint8_t *)0x3365c0 = 0;
      goto check_output;
    }
  } else {
    *(int16_t *)0x3365bc = 0;
  check_output:
    if (*(uint8_t *)0x449ef2 != 0)
      goto do_output;
  }
  if (*(uint8_t *)0x449ef3 == 0)
    return;
  if (*(int16_t *)0x3365bc > 3)
    return;

do_output: {
  uint32_t idx = ((int)*(int16_t *)0x3365c4 + 0xfd) & 0xff;
  do {
    if ((int16_t)idx < *(int16_t *)0x3365c2)
      ((void (*)(void))0x906d0)();
    idx = ((int)(int16_t)idx + 1) & 0xff;
  } while ((int16_t)idx != *(int16_t *)0x3365c4);
}
}
