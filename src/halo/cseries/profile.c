/* Profile timing infrastructure. Uses RDTSC to measure CPU cycles and
 * converts to milliseconds using the stored CPU frequency at 0x3361a0.
 * All timing data is accumulated into global profiling structures. */

/* Read the x86 timestamp counter (RDTSC) into a low/high dword pair.
   Use GCC-style asm for clang (even targeting MSVC) because MSVC-style
   __asm doesn't properly communicate register clobbers to the optimizer. */
#if defined(_MSC_VER) && !defined(__clang__)
#define RDTSC(lo, hi)       \
  do {                      \
    __asm { push eax }      \
    __asm { push edx }      \
    __asm { rdtsc }         \
    __asm { mov (lo), eax } \
    __asm { mov (hi), edx } \
    __asm { pop edx }       \
    __asm { pop eax }       \
  } while (0)
#else
#define RDTSC(lo, hi)                     \
  do {                                    \
    uint32_t _lo, _hi;                    \
    asm volatile("rdtsc\n\t"              \
                 "movl %%eax, %0\n\t"     \
                 "movl %%edx, %1"         \
                 : "=rm"(_lo), "=rm"(_hi) \
                 :                        \
                 : "eax", "edx");         \
    (lo) = _lo;                           \
    (hi) = _hi;                           \
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

  find_profile_section(section);

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

  if (*(uint8_t *)0x3361aa == 0) {
    find_profile_section(section);

    if (*(int16_t *)(s + 0xa) != *(int16_t *)0x3361a8) {
      display_assert("section->stack_depth==profile_globals.stack_depth",
                     "c:\\halo\\SOURCE\\cseries\\profile.c", 0x267, 1);
      system_exit(-1);
    }

    *(int16_t *)0x3361a8 -= 1;

    {
      int64_t timestamp;
      uint32_t *timestamp_parts;

      timestamp_parts = (uint32_t *)&timestamp;
      RDTSC(timestamp_parts[0], timestamp_parts[1]);
      *(int64_t *)(s + 0x5d0) += timestamp - *(int64_t *)(s + 0x10);
    }

    *(int16_t *)(s + 0xa) = -1;
  } else {
    *(int16_t *)(s + 0xa) = -1;
  }
}

/* Start timing a game tick. Increments the tick counter and records
 * the start timestamp in the tick timing array. */
void profile_tick_start(void)
{
  int idx;
  uint32_t lo, hi;

  if (*(uint8_t *)0x449ef0 != 0)
    FUN_0008f6b0();

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
  char *tick;
  int64_t timestamp;
  uint32_t *timestamp_parts;
  float elapsed;

  if (*(int16_t *)0x448dd8 < 1 || *(int16_t *)0x448dd8 > 0x96) {
    display_assert("(profile_globals.current_frame.game_tick_count > 0) && "
                   "(profile_globals.current_frame.game_tick_count <= "
                   "MAXIMUM_GAME_TICKS_PER_FRAME)",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x140, 1);
    system_exit(-1);
  }

  idx = (int)*(int16_t *)0x448dd8;
  tick = (char *)(0x448de0 + idx * 0x18);
  timestamp_parts = (uint32_t *)&timestamp;
  RDTSC(timestamp_parts[0], timestamp_parts[1]);

  *(uint32_t *)(tick + 0x8) = timestamp_parts[0];
  *(uint32_t *)(tick + 0xc) = timestamp_parts[1];

  timestamp -= *(int64_t *)tick;
  elapsed = (float)timestamp * *(float *)0x254cb8 / (float)*(int64_t *)0x3361a0;

  *(float *)(tick + 0x10) += elapsed;
  *(float *)(tick + 0x14) += elapsed;
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
  int64_t timestamp;
  uint32_t *timestamp_parts;
  float elapsed;

  timestamp_parts = (uint32_t *)&timestamp;
  RDTSC(timestamp_parts[0], timestamp_parts[1]);
  *(uint32_t *)0x449c70 = timestamp_parts[0];
  *(uint32_t *)0x449c74 = timestamp_parts[1];

  timestamp -= *(int64_t *)0x449c68;
  elapsed = (float)timestamp * *(float *)0x254cb8 / (float)*(int64_t *)0x3361a0;

  *(float *)0x449c78 += elapsed;
  *(float *)0x449c7c += elapsed;
}

/* Start timing a render window. Increments window count, stores the
 * window parameter, and records the start timestamp. */
void profile_render_window_start(char window_param)
{
  int idx;
  uint32_t lo, hi;

  if (*(int16_t *)0x448dda < 4) {
    *(int16_t *)0x448dda += 1;
    *(uint8_t *)(0x448ddb + (int)*(int16_t *)0x448dda) =
      (uint8_t)window_param;
  }

  if (!(*(int16_t *)0x448dda > 0 && *(int16_t *)0x448dda <= 4)) {
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
  int64_t timestamp;
  uint32_t *timestamp_parts;
  char *window;
  float elapsed;

  if (*(int16_t *)0x448dda < 1 || *(int16_t *)0x448dda > 4) {
    display_assert(
      "(profile_globals.current_frame.window_count > 0) && "
      "(profile_globals.current_frame.window_count <= MAXIMUM_WINDOWS)",
      "c:\\halo\\SOURCE\\cseries\\profile.c", 0x16a, 1);
    system_exit(-1);
  }

  idx = (int)*(int16_t *)0x448dda;
  window = (char *)(0x449bf0 + idx * 0x18);
  timestamp_parts = (uint32_t *)&timestamp;
  RDTSC(timestamp_parts[0], timestamp_parts[1]);

  *(uint32_t *)(window + 0x8) = timestamp_parts[0];
  *(uint32_t *)(window + 0xc) = timestamp_parts[1];

  timestamp -= *(int64_t *)window;
  elapsed = (float)timestamp * *(float *)0x254cb8 / (float)*(int64_t *)0x3361a0;

  *(float *)(window + 0x10) += elapsed;
  *(float *)(window + 0x14) += elapsed;
}

/* Snapshot the current TSC into a dedicated low/high global pair at
 * 0x449c98/0x449c9c (used to mark a reference timestamp). */
void profile_texture_start(void)
{
  uint32_t lo, hi;
  RDTSC(lo, hi);
  *(uint32_t *)0x449c98 = lo;
  *(uint32_t *)0x449c9c = hi;
}

/* End a custom profiling section. Computes elapsed msec since the
 * reference timestamp at 0x449c98/0x449c9c (set by profile_texture_start)
 * and accumulates into the two custom accumulators at 0x449ca8/0x449cac. */
void profile_texture_end(void)
{
  int64_t timestamp;
  uint32_t *timestamp_parts;
  float elapsed;

  timestamp_parts = (uint32_t *)&timestamp;
  RDTSC(timestamp_parts[0], timestamp_parts[1]);
  *(uint32_t *)0x449ca0 = timestamp_parts[0];
  *(uint32_t *)0x449ca4 = timestamp_parts[1];

  timestamp -= *(int64_t *)0x449c98;
  elapsed = (float)timestamp * *(float *)0x254cb8 / (float)*(int64_t *)0x3361a0;

  *(float *)0x449ca8 += elapsed;
  *(float *)0x449cac += elapsed;
}

/* Start a new profiling frame. Clears the current frame data, records
 * the render count and timing state, and timestamps the frame start. */
void profile_frame_start(void)
{
  uint32_t lo, hi;

  if (*(uint8_t *)0x449ef0 == 0)
    FUN_0008f6b0();

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
  int32_t ring_idx;
  float *child_section;

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
    child_section = (float *)(0x448df8 + (int)i * 0x18);
    if (!(*(float *)0x448df4 >= child_section[4])) {
      display_assert(
        "parent_timesection->self_msec >= child_timesection->elapsed_msec",
        "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1b2, 1);
      system_exit(-1);
      tick_count = *(int16_t *)0x448dd8;
    }
    *(float *)0x448df4 -= child_section[4];
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
  if (!(*(float *)0x448df4 >= *(float *)0x449c78)) {
    display_assert(
      "parent_timesection->self_msec >= child_timesection->elapsed_msec",
      "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1b2, 1);
    system_exit(-1);
  }
  *(float *)0x448df4 -= *(float *)0x449c78;

  /* subtract window child times from render self_msec */
  window_count = *(int16_t *)0x448dda;
  for (i = 0; i < window_count; i++) {
    if (*(float *)0x449c7c < *(float *)(0x449c18 + (int)i * 0x18)) {
      display_assert(
        "parent_timesection->self_msec >= child_timesection->elapsed_msec",
        "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1b2, 1);
      system_exit(-1);
    }
    *(float *)0x449c7c -= *(float *)(0x449c18 + (int)i * 0x18);
  }

  /* subtract custom profile time */
  if (!(*(float *)0x448df4 >= *(float *)0x449cc0)) {
    display_assert(
      "parent_timesection->self_msec >= child_timesection->elapsed_msec",
      "c:\\halo\\SOURCE\\cseries\\profile.c", 0x1b2, 1);
    system_exit(-1);
  }
  *(float *)0x448df4 -= *(float *)0x449cc0;

  /* copy current frame data to ring buffer */
  qmemcpy((void *)(0x3365c8 + (int)*(int16_t *)0x3365c4 * 0x1128),
          (void *)0x448dc8, 0x1128);

  ring_idx = (int)*(int16_t *)0x3365c4 + 1; /* hazard-ok: value increment */
  if (*(int16_t *)0x3365c2 <= (int16_t)ring_idx)
    *(int16_t *)0x3365c2 = (int16_t)ring_idx;

  /* wrap ring index to 0-255 */
  ring_idx %= 0x100;
  *(int16_t *)0x3365c4 = (int16_t)ring_idx;

  /* handle profile output */
  if (*(uint8_t *)0x449cd4 == 0) {
    *(int32_t *)0x3365bc += 1;
    if (*(uint8_t *)0x449ef3 == 0)
      goto check_output;
    if (*(uint8_t *)0x449ef2 != 0)
      goto do_output;
    if (*(int32_t *)0x3365bc > 3 && *(uint8_t *)0x3365c0 != 0) {
      if (*(void **)0x3365b4 != 0) {
        ((void (*)(void *, const void *))0x1da685)(*(void **)0x3365b4, L"\r\n");
        ((void (*)(void *))0x1d8f31)(*(void **)0x3365b4);
      }
      *(uint8_t *)0x3365c0 = 0;
      goto check_output;
    }
  } else {
    *(int32_t *)0x3365bc = 0;
  check_output:
    if (*(uint8_t *)0x449ef2 != 0)
      goto do_output;
  }
  if (*(uint8_t *)0x449ef3 == 0)
    return;
  if (*(int32_t *)0x3365bc > 3)
    return;

do_output: {
  /* Signed modulo, not a mask: the reference expands both wraps with MSVC's
   * signed %-by-power-of-two idiom (AND 0x800000ff / JNS / DEC / OR
   * 0xffffff00 / INC) at 0x91b16 and 0x91b49. `& 0xff` emits a single
   * AND and cannot reproduce it. */
  int idx = ((int)*(int16_t *)0x3365c4 + 0xfd) % 0x100;
  do {
    if ((int16_t)idx < *(int16_t *)0x3365c2)
      ((void (*)(void))0x906d0)();
    idx = ((int)(int16_t)idx + 1) % 0x100;
  } while ((int16_t)idx != *(int16_t *)0x3365c4);
}
}
/* -----------------------------------------------------------------------
 * symbol_table_dispose (0x92090) — free name_pool and entries buffers and
 * zero the symtab struct.
 *
 * symtab layout: int32_t[3] = { count, name_pool_ptr, entries_ptr }
 * Called from the error path of load_symbol_table to clean up a partially
 * filled symtab when loading fails.
 * ----------------------------------------------------------------------- */
void symbol_table_dispose(int32_t *symtab)
{
  if (symtab == NULL) {
    display_assert("symbol_table",
                   "c:\\halo\\SOURCE\\cseries\\stack_walk_windows.c", 0x225, 1);
    system_exit(-1);
  }
  if (symtab[1] != 0) {
    debug_free((void *)symtab[1],
               "c:\\halo\\SOURCE\\cseries\\stack_walk_windows.c", 0x227);
  }
  if (symtab[2] != 0) {
    debug_free((void *)symtab[2],
               "c:\\halo\\SOURCE\\cseries\\stack_walk_windows.c", 0x228);
  }
  symtab[0] = 0;
  symtab[1] = 0;
  symtab[2] = 0;
}
