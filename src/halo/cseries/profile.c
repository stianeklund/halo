/* Profile timing infrastructure. Uses RDTSC to measure CPU cycles and
 * converts to milliseconds using the stored CPU frequency at 0x3361a0.
 * All timing data is accumulated into global profiling structures. */

/* Read the x86 timestamp counter (RDTSC) into a low/high dword pair.
   Use GCC-style asm for clang (even targeting MSVC) because MSVC-style
   __asm doesn't properly communicate register clobbers to the optimizer. */
#if defined(_MSC_VER) && !defined(__clang__)
#define RDTSC(lo, hi)    \
  do {                   \
    __asm { push eax }     \
    __asm                \
    {                    \
      push edx           \
    }                    \
    __asm { rdtsc }       \
    __asm                \
    {                    \
      mov(lo), eax       \
    }                    \
    __asm { mov (hi), edx } \
    __asm                \
    {                    \
      pop edx            \
    }                    \
    __asm { pop eax }      \
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

/* Store the frame time in seconds (in EBP+8) computed each frame by
 * main_update_time() into the global read by the profiling HUD. */
void profile_seconds_elapsed(float seconds_elapsed)
{
  *(float *)0x449cc8 = seconds_elapsed;
}

/* Store per-frame profile counters. Called once per frame by
 * main_rasterizer_throttle() (0x101970) after computing the vblank
 * throttle result: frames_delta is the frames-lapsed-since-vblank-target
 * count (clamped to [0, 0x7fff]), synced is whether the frame hit its
 * vblank target on time, and debug_buf is an optional throttle debug
 * label copied into the profile's debug string slot.
 *
 * DAT_00449ccc = int16_t frames_delta.
 * DAT_00449cd4 = uint8_t "lapsed" flag consumed by profile_frame_end()
 *   (0x449cd4 == 0 selects the lapsed-frame accounting path there):
 *   1 if frames_delta > 0, or if frames_delta <= 0 and not synced; else 0.
 * DAT_00449cd5 = char[] debug label buffer, copied from debug_buf via
 *   csstrcpy() when debug_buf is non-NULL. */
void profile_lapsed_frames(int16_t frames_delta, bool synced,
                           const char *debug_buf)
{
  *(int16_t *)0x449ccc = frames_delta;

  if (frames_delta > 0) {
    *(uint8_t *)0x449cd4 = 1;
  } else {
    *(uint8_t *)0x449cd4 = 0;
    if (!synced) {
      *(uint8_t *)0x449cd4 = 1;
    }
  }

  if (debug_buf != NULL) {
    csstrcpy((char *)0x449cd5, debug_buf);
  }
}

/* Store the elapsed-time-based lapsed accounting, called once per frame by
 * main_update_time() (0x101821) with the frame's elapsed milliseconds.
 * DAT_00449cd0 = int32_t msec (raw copy of the argument).
 * DAT_00449cd4 = uint8_t "lapsed" flag consumed by profile_frame_end()
 *   (same flag profile_lapsed_frames() sets from frames_delta/synced):
 *   1 if msec > 0, else 0. */
void profile_lapsed_msec(int msec)
{
  *(int32_t *)0x449cd0 = msec;
  *(uint8_t *)0x449cd4 = (uint8_t)(msec > 0);
}

/* Validate a profile section, registering it on first use. If
 * section->index is still NONE (-1), allocates the next slot in
 * profile_globals.sections[] (0x3361b4) and zero-initializes the
 * section's timing/child data. Otherwise verifies the section's
 * recorded index still points back at this section in the global
 * table (catches use of a stack-local/uninitialized section that
 * was never registered via profile_enter()). */
void find_profile_section(void *section)
{
  char *s = (char *)section;
  int32_t index;

  if (section == NULL) {
    display_assert("section", "c:\\halo\\SOURCE\\cseries\\profile.c", 0x22f, 1);
    system_exit(-1);
  }

  if (*(uint8_t *)(s + 8) == 0) {
    display_assert("section->active", "c:\\halo\\SOURCE\\cseries\\profile.c",
                   0x230, 1);
    system_exit(-1);
  }

  index = *(int32_t *)(s + 4);
  if (index != -1) {
    if (index < 0 || index >= *(int16_t *)0x3361b0 ||
        ((void **)0x3361b4)[index] != section) {
      display_assert("don't call profile_enter_private(), call profile_enter()",
                     "c:\\halo\\SOURCE\\cseries\\profile.c", 0x236, 1);
      system_exit(-1);
    }
  } else {
    if (*(int16_t *)0x3361b0 >= 0x100) {
      display_assert("profile_globals.section_count<MAXIMUM_PROFILE_SECTIONS",
                     "c:\\halo\\SOURCE\\cseries\\profile.c", 0x23a, 1);
      system_exit(-1);
    }

    *(int32_t *)(s + 4) = *(int16_t *)0x3361b0;
    *(int16_t *)0x3361b0 += 1;

    index = *(int32_t *)(s + 4);
    ((void **)0x3361b4)[index] = section;

    csmemset(s + 0x208, 0, 0x3c0);
    csmemset(s + 0x28, 0, 0x1e0);
    *(uint32_t *)(s + 0x18) = 0;
    *(uint32_t *)(s + 0x20) = 0;
    *(uint32_t *)(s + 0x24) = 0;
    *(int16_t *)(s + 0xa) = -1;
    *(uint32_t *)(s + 0x5c8) = 0;
    *(uint32_t *)(s + 0x5d0) = 0;
    *(uint32_t *)(s + 0x5d4) = 0;
    *(uint32_t *)(s + 0x5cc) = 0;
    *(uint32_t *)(s + 0x5e0) = 0;
    *(uint32_t *)(s + 0x5e4) = 0;
    *(uint32_t *)(s + 0x5d8) = 0;
    *(uint32_t *)(s + 0x5f0) = 0;
    *(uint32_t *)(s + 0x5f4) = 0;
    *(uint32_t *)(s + 0x5e8) = 0;
  }
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

/* FUN_00090170 (0x90170) — store a pair of dwords at offsets 0x0/0x4 of a
 * caller-supplied destination. dest arrives in EAX (not a stack arg);
 * value0/value1 are ordinary cdecl stack args copied verbatim, no further
 * processing. No xrefs found in this binary snapshot, so the destination
 * struct and true field semantics are unconfirmed. */
void FUN_00090170(void *dest /* @<eax> */, uint32_t value0, uint32_t value1)
{
  *(uint32_t *)dest = value0;
  *(uint32_t *)((char *)dest + 4) = value1;
}

/* profile_dump_to_file (0x90650) — HaloScript "profile_dump" builtin
 * back end (only caller: FUN_000c1fc0). Renders the profile dump into a
 * local scratch buffer via profile_dump() and appends it to
 * "d:\profile.txt", opened in append/binary mode (mode string at
 * 0x267f84 — same global confirmed as "a+b" by debug_string_to_display
 * in errors.c).
 *
 * has_substring is passed to profile_dump() as a flag: true only when
 * substring is non-NULL and non-empty (csstrlen(substring) != 0).
 * profile_dump()'s other two immediate args (0, 0x100) and its buffer
 * pointer are taken verbatim from the call site; profile_dump itself is
 * unlifted so their exact meaning is unconfirmed.
 *
 * fclose(stream) runs unconditionally in the original, even when fopen
 * failed and stream is NULL — the JZ over the write block still falls
 * through into the fclose call. Preserved as-is. */
void profile_dump_to_file(const char *substring)
{
  void *stream;
  int has_substring;
  char buf[0x2000];

  has_substring = (substring != NULL && csstrlen(substring) != 0) ? 1 : 0;

  stream = crt_fopen("d:\\profile.txt", (const char *)0x267f84);
  if (stream != NULL) {
    profile_dump(substring, has_substring, 0, 0x100, buf);
    crt_fprintf(stream, "%s\r\n", buf);
  }
  crt_fclose(stream);
}

/* FUN_000906d0 (0x906d0) -- dump one profile ring-buffer entry to
 * "d:\framedump.txt". Only caller: profile_frame_end's do_output loop,
 * which passes EDI = 0x3365c8 + (int16_t)idx*0x1128 (base of the ring
 * slot that qmemcpy copies the current-frame struct into, 0x1128 bytes
 * each). Byte offset 0 of that slot doubles as a "dumped" flag for this
 * routine -- csmemset in profile_frame_start zeroes it and nothing else
 * writes it before this function runs. EDI is read/written only through
 * *param_1, never reassigned, so it needs no callee-side save/restore.
 *
 * FUN_0008fb60 (unlifted) formats the entry into a 512-byte scratch
 * buffer. Disassembly ARG_COUNT hazard on the following crt_fprintf call
 * resolved by tracing pushes: PUSH 0x200 at 0x90703 is FUN_0008fb60's own
 * cdecl stack arg -- its cleanup is deferred and merged into the single
 * ADD ESP,0x10 after crt_fprintf (4 dwords = 1 for FUN_0008fb60 + 3 for
 * crt_fprintf's stream/format/buffer), not a 4th crt_fprintf arg. EAX is
 * loaded from EDI right before the call and never reused after it; ESI is
 * loaded with the scratch buffer address and stays untouched until the
 * POP ESI restore at 0x90736 -- both are consumed only by the callee, so
 * FUN_0008fb60 takes the ring-entry pointer in EAX and the destination
 * buffer in ESI. */
void FUN_000906d0(char *param_1 /* @<edi> */)
{
  char buf[0x200];

  if (*(void **)0x3365b4 == 0) {
    *(void **)0x3365b4 = crt_fopen("d:\\framedump.txt", "wb");
    if (*(void **)0x3365b4 == 0) {
      *(uint8_t *)0x3365c0 = 1;
      return;
    }
  }

  if (*param_1 == 0) {
    *param_1 = 1;
    FUN_0008fb60(param_1, buf, 0x200);
    crt_fprintf(*(void **)0x3365b4, "%s\r\n", buf);
    *param_1 = 1;
  }

  *(uint8_t *)0x3365c0 = 1;
}

/* Initialize a profile-frame ring iterator: mark it not-yet-started
 * (index sentinel 0xffff) and record the last completed frame's ring
 * index (current ring write index - 1, mod 256) as the iteration bound.
 * Companion to profile_frame_iterator_next (0x91110). */
void profile_frame_iterator_new(void *iterator)
{
  char *it = (char *)iterator;
  int end_idx;

  if (iterator == NULL) {
    display_assert("iterator", "c:\\halo\\SOURCE\\cseries\\profile.c", 0x58b,
                   1);
    system_exit(-1);
  }

  *(int16_t *)it = 0xffff;

  /* Signed modulo, not a mask: see profile_frame_end's note on the
   * AND 0x800000ff / JNS / DEC / OR 0xffffff00 / INC idiom. */
  end_idx = ((int)*(int16_t *)0x3365c4 + 0xff) % 0x100;
  *(int16_t *)(it + 2) = (int16_t)end_idx;
}

/* Advance a profile-frame ring iterator and optionally fetch the current
 * ring entry's two output dwords (ring base 0x3365c8, entry stride 0x1128;
 * the copied fields sit at entry+0x08/entry+0x0C, i.e. &DAT_003365d0 /
 * &DAT_003365d4 scaled by the entry index).
 *
 * iterator[0] (int16) is written with the index being consumed this call;
 * iterator[1] (int16) holds the walk position and is stepped backward
 * (mod 256, matching profile_frame_iterator_new's end-index computation)
 * after the fetch, then reset to the not-yet-started sentinel 0xffff once
 * it reaches the iterator's recorded end index. Returns true iff a valid
 * entry was consumed this call (index != -1 and < DAT_003365c2, the
 * high-water ring count); false ends iteration. */
bool profile_frame_iterator_next(void *iterator, void *out_record)
{
  int16_t *it = (int16_t *)iterator;
  int16_t idx;
  int new_idx;

  idx = it[1];
  it[0] = idx;

  if (idx == -1 || idx >= *(int16_t *)0x3365c2)
    return false;

  if (out_record != NULL) {
    int32_t *out = (int32_t *)out_record;
    out[0] = *(int32_t *)(0x3365d0 + (int)idx * 0x1128);
    out[1] = *(int32_t *)(0x3365d4 + (int)idx * 0x1128);
  }

  /* Signed modulo, not a mask: see profile_frame_end's note on the
   * AND 0x800000ff / JNS / DEC / OR 0xffffff00 / INC idiom. */
  new_idx = ((int)it[0] + 0xff) % 0x100;
  it[1] = (int16_t)new_idx;
  if ((int16_t)new_idx == *(int16_t *)0x3365c4)
    it[1] = -1;

  return true;
}

/* Validate a profile-frame ring iterator's current entry index before use:
 * asserts iterator is non-NULL, iterator->current_buffer_index (offset 0,
 * same field profile_frame_iterator_next writes/reads) is in
 * [0, profile_globals.current_frame_history_count), and does not equal
 * profile_globals.current_frame_history_index (the ring slot currently
 * being written). Pure validation -- no other side effects. */
void profile_frame_get_messages(void *iterator)
{
  int16_t *it = (int16_t *)iterator;

  if (it == NULL) {
    display_assert("iterator", "c:\\halo\\SOURCE\\cseries\\profile.c", 0x5b7,
                   1);
    system_exit(-1);
  }

  if (it[0] < 0 || it[0] >= *(int16_t *)0x3365c2) {
    display_assert("(iterator->current_buffer_index >= 0) && "
                   "(iterator->current_buffer_index < "
                   "profile_globals.current_frame_history_count)",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x5b8, 1);
    system_exit(-1);
  }

  if (it[0] == *(int16_t *)0x3365c4) {
    display_assert("iterator->current_buffer_index != "
                   "profile_globals.current_frame_history_index",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x5b9, 1);
    system_exit(-1);
  }
}

/* Same two validation asserts as profile_frame_get_messages (identical
 * text, lines 0x5c8/0x5c9 here vs 0x5b8/0x5b9 there), then reads three
 * fields out of the current ring-buffer entry (base 0x3365c8, stride
 * 0x1128 -- same ring profile_frame_end's qmemcpy writes and
 * profile_frame_iterator_next indexes). Entry+0x111c (word) goes to
 * *out_a, entry+0x1120 (dword) goes to *out_b, and entry+0x1118 (dword)
 * is the return value. Only caller: FUN_000df4e0 (unlifted); field
 * meanings beyond their offsets are unconfirmed -- "stalls" is the
 * auto-lift-assigned name, not source/PDB evidence. */
int32_t profile_frame_get_stalls(void *iterator, int16_t *out_a, int32_t *out_b)
{
  int16_t *it = (int16_t *)iterator;
  char *entry = (char *)(0x3365c8 + (int)it[0] * 0x1128);

  if (it[0] < 0 || it[0] >= *(int16_t *)0x3365c2) {
    display_assert("(iterator->current_buffer_index >= 0) && "
                   "(iterator->current_buffer_index < "
                   "profile_globals.current_frame_history_count)",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x5c8, 1);
    system_exit(-1);
  }

  if (it[0] == *(int16_t *)0x3365c4) {
    display_assert("iterator->current_buffer_index != "
                   "profile_globals.current_frame_history_index",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x5c9, 1);
    system_exit(-1);
  }

  *out_a = *(int16_t *)(entry + 0x111c);
  *out_b = *(int32_t *)(entry + 0x1120);
  return *(int32_t *)(entry + 0x1118);
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
    *(uint8_t *)(0x448ddb + (int)*(int16_t *)0x448dda) = (uint8_t)window_param;
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
      FUN_000906d0((char *)(0x3365c8 + (int)(int16_t)idx * 0x1128));
    idx = ((int)(int16_t)idx + 1) % 0x100;
  } while ((int16_t)idx != *(int16_t *)0x3365c4);
}
}

/* FUN_00091b70 (0x91b70) -- snapshot the current TSC into a dedicated
 * low/high global pair at 0x449cb0/0x449cb4. Same shape as
 * profile_texture_start: no paired "end" reader of this pair was found in
 * this TU. Both call sites are raw address casts in main_update_time
 * (0x101821-area, "THROTTLE" sleep bracket) and main_rasterizer_throttle
 * (0x101970, vblank wait bracket) — this is the "throttle start" marker;
 * its paired end marker is FUN_00091ba0 (0x91ba0), not yet lifted. */
void FUN_00091b70(void)
{
  uint32_t lo, hi;
  RDTSC(lo, hi);
  *(uint32_t *)0x449cb0 = lo;
  *(uint32_t *)0x449cb4 = hi;
}

/* FUN_00091c10 (0x91c10) -- zero a 0x110-byte destination record, then
 * conditionally copy fields out of `source` into it: two leading dwords
 * (source[0], source[1]), a name string (csstrncpy into dest+0x8, cap 0xff
 * so the string plus NUL fits the 0x100-byte field), and a trailing dword
 * `value` at dest+0x108. The copy only happens when source is non-NULL AND
 * source[0] != 0 (a "valid id" guard on the first dword) -- `value` is
 * written only inside that same guarded block, matching the reference's
 * single JZ/JZ fallthrough to the RET. No xrefs/strings were found for this
 * TU entry, so field/param semantics beyond the observed shape are unknown;
 * offsets are index math matching the disassembly, not a named struct. */
void FUN_00091c10(int *dest, int *source, char *name, int value)
{
  csmemset(dest, 0, 0x110);
  if (source != NULL && source[0] != 0) {
    dest[0] = source[0];
    dest[1] = source[1];
    if (name != NULL) {
      csstrncpy((char *)(dest + 2), name, 0xff);
    }
    dest[0x42] = value;
  }
}

/* FUN_00091c70 (0x91c70) -- fire a progress record's registered callback when
 * the record is active and either the 125ms throttle window has elapsed or a
 * forced update is requested. Shares the "progress/data record" layout with
 * FUN_00091c10 (0x91c10): data[0] = callback fn ptr, data[1] = callback's
 * first arg, data+0x8 = name string (0x100 bytes), data[0x42] (+0x108) =
 * max/total value, data[0x43] (+0x10c) = last-update timestamp (ms). No
 * named struct -- offsets are index math matching the disassembly, same
 * convention as FUN_00091c10.
 * Asserts data != NULL ("data", progress.c:0x23) then system_exit(-1)
 * (noreturn; the display_assert/system_exit pair matches the decompiler's
 * "Subroutine does not return" note).
 * Callback call-site push order verified against disassembly (00091cd6-cdc):
 * push pct, push user_data, push &data[2], push data[1] -- so cdecl arg
 * order (first pushed last = first param) is
 * (data[1], &data[2], user_data, pct). */
void FUN_00091c70(int *data, void *user_data, int value, char force_update)
{
  int now;
  int pct;

  if (data == NULL) {
    display_assert("data", "c:\\halo\\SOURCE\\cseries\\progress.c", 0x23, 1);
    system_exit(-1);
  }

  if (data[0] != 0 && data[0x42] != 0) {
    now = (int)system_milliseconds();
    if ((uint32_t)(now - data[0x43]) > 0x7d || force_update != 0) {
      pct = (value * 100) / data[0x42];
      ((void (*)(int, char *, void *, int))data[0])(data[1], (char *)(data + 2),
                                                    user_data, pct);
      data[0x43] = now;
    }
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
