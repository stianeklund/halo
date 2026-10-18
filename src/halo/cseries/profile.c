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

/* profile_timesection_end (0x90180) - timesection arrives in EAX; end_lo/
 * end_hi are the cdecl stack halves of a 64-bit cycle timestamp. Stores the
 * end stamp at +0x8, subtracts the start stamp at +0x0, converts to msec
 * (FILD diff * [0x254cb8] / FILD [0x3361a0]) and adds the same unnarrowed
 * result to the floats at +0x10 and +0x14 (FLD ST0 / FADD / FSTP twice).
 * Field meanings beyond that are unconfirmed. */
void profile_timesection_end(void *timesection /* @<eax> */, uint32_t end_lo,
                             uint32_t end_hi)
{
  char *t = (char *)timesection;
  int64_t diff;
  uint32_t *diff_parts;
  float elapsed;

  *(uint32_t *)(t + 0x8) = end_lo;
  *(uint32_t *)(t + 0xc) = end_hi;
  diff_parts = (uint32_t *)&diff;
  diff_parts[0] = end_lo;
  diff_parts[1] = end_hi;
  diff -= *(int64_t *)t;
  elapsed = (float)diff * *(float *)0x254cb8 / (float)*(int64_t *)0x3361a0;
  *(float *)(t + 0x10) += elapsed;
  *(float *)(t + 0x14) += elapsed;
}

/* compare_profile_sections (0x901d0) - qsort-style comparator over an array
 * of profile_section pointers (each argument is a pointer TO the element,
 * i.e. section**; the disassembly dereferences [EBP+8]/[EBP+0xc] once
 * before touching any field).
 *
 * Section fields used here follow this file's raw-offset convention (see
 * find_profile_section): +0x00 char *name, +0x08 uint8 active. Three more
 * offsets are only observed here, so their meaning is unconfirmed:
 *   +0x20 int64 accumulated value (CMP high signed / low unsigned pair)
 *   +0x5c8 int32 sample count
 *   +0x5e0 int64 sample total
 *
 * Active sections always sort before inactive ones. Ties then dispatch on
 * the int16 sort mode at 0x3365b8 (MOVSX word + SUB/DEC/DEC switch chain):
 *   0 - by name via csstrcmp (tail call, its result is returned verbatim)
 *   1 - by mean sample (FILD qword total / FIDIV dword count; count == 0
 *       yields the double 0.0 constant at 0x2602c0), descending
 *   2 - by the 64-bit value at +0x20, descending
 * Any other mode hits the "unreachable" assert at profile.c:0x34c. */
int compare_profile_sections(void **a, void **b)
{
  char *sa;
  char *sb;
  double avg_a;
  double avg_b;
  int64_t val_a;
  int64_t val_b;

  sa = (char *)*a;
  if (*(uint8_t *)(sa + 8) != 0 && *(uint8_t *)((char *)*b + 8) == 0) {
    return -1;
  }

  sb = (char *)*b;
  if (*(uint8_t *)(sb + 8) != 0 && *(uint8_t *)(sa + 8) == 0) {
    return 1;
  }

  switch (*(int16_t *)0x3365b8) {
  case 0:
    return csstrcmp(*(char **)sa, *(char **)sb);

  case 1:
    avg_a = 0.0;
    if (*(int32_t *)(sa + 0x5c8) != 0) {
      avg_a = (double)*(int64_t *)(sa + 0x5e0) / *(int32_t *)(sa + 0x5c8);
    }
    avg_b = 0.0;
    if (*(int32_t *)(sb + 0x5c8) != 0) {
      avg_b = (double)*(int64_t *)(sb + 0x5e0) / *(int32_t *)(sb + 0x5c8);
    }
    if (avg_a > avg_b) {
      return -1;
    }
    if (avg_a < avg_b) {
      return 1;
    }
    return 0;

  case 2:
    val_a = *(int64_t *)(sa + 0x20);
    val_b = *(int64_t *)(sb + 0x20);
    if (val_a > val_b) {
      return -1;
    }
    if (val_a < val_b) {
      return 1;
    }
    return 0;

  default:
    display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\cseries\\profile.c",
                   0x34c, 1);
    system_exit(-1);
  }

  return 0;
}

/* profile_dump (0x902f0) -- sorts a copy of profile_globals.sections[]
 * (count at 0x3361b0) with compare_profile_sections under sort_mode
 * (stored to 0x3365b8), then writes a header and one row per run of
 * equally-named active sections into buffer. Header/row format tables are
 * the pointer arrays at 0x2ee770/0x2ee778, indexed by format_mode.
 * Section fields +0x5c8..+0x5f0 are raw offsets observed only here and in
 * compare_profile_sections. Constants: 0x2602c0 double (sample-count-zero
 * fallback), 0x268628 double scale, 0x253398 float bias before _ftol2. */
void profile_dump(const char *substring, int16_t sort_mode, int16_t format_mode,
                  int16_t maximum_section_count, char *buffer)
{
  void *sections[0x100];
  char *section;
  char *entry;
  double sample_total;
  int32_t format;
  int32_t value_5d8;
  int32_t value_5c8;
  int32_t sum_5d8;
  int32_t sum_5cc;
  int32_t sum_5e8;
  int16_t dumped;
  int16_t section_index;
  float sum_5e0_seconds;
  float sum_5d8_per_5c8;
  float sum_average_msec;
  float sum_5f0_msec;
  float sum_5d0_msec;

  dumped = 0;
  if (sort_mode < 0 || sort_mode >= 3) {
    display_assert("sort_mode>=0 && sort_mode<NUMBER_OF_PROFILE_SORT_MODES",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x36f, 1);
    system_exit(-1);
  }
  if (format_mode < 0 || format_mode >= 2) {
    display_assert(
      "format_mode>=0 && format_mode<NUMBER_OF_PROFILE_DUMP_FORMAT_MODES",
      "c:\\halo\\SOURCE\\cseries\\profile.c", 0x370, 1);
    system_exit(-1);
  }
  if (maximum_section_count <= 0) {
    display_assert("maximum_section_count>0",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x371, 1);
    system_exit(-1);
  }
  if (buffer == NULL) {
    display_assert("buffer", "c:\\halo\\SOURCE\\cseries\\profile.c", 0x372, 1);
    system_exit(-1);
  }

  *(int16_t *)0x3365b8 = sort_mode;
  csmemcpy(sections, (void *)0x3361b4, *(int16_t *)0x3361b0 * sizeof(void *));
  qsort(sections, *(int16_t *)0x3361b0, sizeof(void *),
        (qsort_compar_proc)compare_profile_sections);

  if (*(int16_t *)0x3361b0 != 0 && *(uint8_t *)((char *)sections[0] + 8) != 0) {
    format = format_mode;
    crt_sprintf(buffer, ((const char **)0x2ee770)[format]);

    for (section_index = 0; section_index < *(int16_t *)0x3361b0;
         section_index++) {
      if (dumped >= maximum_section_count) {
        return;
      }
      section = (char *)sections[section_index];
      sum_5e8 = 0;
      sum_5d8 = 0;
      sum_5e0_seconds = 0.0f;
      sum_5d8_per_5c8 = 0.0f;
      sum_average_msec = 0.0f;
      sum_5f0_msec = 0.0f;
      sum_5cc = 0;
      sum_5d0_msec = 0.0f;

      if (*(uint8_t *)(section + 8) != 0 &&
          (substring == NULL ||
           crt_strstr(*(char **)section, substring) != NULL)) {
        do {
          entry = (char *)sections[section_index];
          sample_total = (double)*(int64_t *)(entry + 0x5e0);
          value_5d8 = *(int32_t *)(entry + 0x5d8);
          sum_5d8 += value_5d8;
          value_5c8 = *(int32_t *)(entry + 0x5c8);
          sum_5e0_seconds += sample_total / (double)*(int64_t *)0x3361a0;
          sum_5d8_per_5c8 += (float)value_5d8 / value_5c8;
          sum_average_msec +=
            (value_5c8 == 0 ? *(double *)0x2602c0 : sample_total / value_5c8) *
            *(double *)0x268628 / (double)*(int64_t *)0x3361a0;
          sum_5e8 += *(int32_t *)(entry + 0x5e8);
          sum_5cc += *(int32_t *)(entry + 0x5cc);
          section_index++;
          sum_5f0_msec += (double)*(int64_t *)(entry + 0x5f0) *
                          *(double *)0x268628 / (double)*(int64_t *)0x3361a0;
          sum_5d0_msec += (double)*(int64_t *)(entry + 0x5d0) *
                          *(double *)0x268628 / (double)*(int64_t *)0x3361a0;
        } while (
          section_index < *(int16_t *)0x3361b0 &&
          csstrcmp(*(char **)sections[section_index], *(char **)section) == 0);

        switch (format) {
        case 0:
          crt_sprintf(buffer + csstrlen(buffer),
                      ((const char **)0x2ee778)[format], *(char **)section,
                      sum_5d8, (double)sum_5e0_seconds, (double)sum_5d8_per_5c8,
                      (double)sum_average_msec, sum_5e8, (double)sum_5f0_msec);
          break;
        case 1:
          crt_sprintf(buffer + csstrlen(buffer),
                      ((const char **)0x2ee778)[format], *(char **)section,
                      (double)sum_5d0_msec, sum_5cc, (double)sum_average_msec,
                      (int)(sum_5d8_per_5c8 + *(float *)0x253398),
                      (double)sum_5f0_msec, sum_5e8, (double)sum_5e0_seconds,
                      sum_5d8);
          break;
        }
        section_index--;
        dumped++;
      }
    }
  }
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

/* FUN_000907c0 (0x907c0) -- shared worker for profile_sections_activate
 * (0x90860) and profile_sections_deactivate (0x90880): walks
 * profile_globals.sections[] (0x3361b4, count at 0x3361b0, both raw
 * addresses per this file's convention -- see find_profile_section) and
 * writes each matching section's "active" byte (offset 8, same field
 * find_profile_section checks as "section->active") to the caller-
 * supplied flag.
 *
 * substring arrives in EDI (unaff_EDI in the decompile -- caller-set,
 * never saved/restored here, matching FUN_000906d0's @<edi> pattern).
 * active is an ordinary cdecl stack arg, read/written as a single byte
 * at [EBP+8]/[section+8].
 *
 * Match rule per section (tested in this order, first hit wins):
 *   - csstrcmp(substring, "*") == 0: every section matches. Computed
 *     once before the loop (BL in the disassembly), not per-section.
 *   - substring[0] == '_': matches when section name starts with
 *     substring+1. The inline compare loop only checks that substring+1
 *     is exhausted (hits '\0'); it never re-checks the name for its own
 *     terminator at that point, so this is a PREFIX test, not exact
 *     equality -- name may run on longer than substring+1.
 *   - otherwise: matches when substring appears anywhere in the section
 *     name (crt_strstr(name, substring) != NULL). */
void FUN_000907c0(char *substring /* @<edi> */, unsigned char active)
{
  int is_wildcard;
  char leading_underscore;
  int16_t i;
  void **sections;

  is_wildcard = (csstrcmp(substring, "*") == 0);
  leading_underscore = (*substring == '_');
  sections = (void **)0x3361b4;

  for (i = 0; i < *(int16_t *)0x3361b0; i++) {
    char *section = (char *)sections[i];
    char *name = *(char **)section;
    int matched;

    if (is_wildcard) {
      matched = 1;
    } else if (leading_underscore) {
      char *p = substring + 1;
      char c = *p;

      matched = 1;
      if (c != '\0') {
        int offset = (int)name - (int)p;

        do {
          if (*p != p[offset]) {
            matched = 0;
            break;
          }
          c = p[1];
          p = p + 1;
        } while (*p != '\0');
      }
    } else {
      matched = (crt_strstr(name, substring) != NULL);
    }

    if (matched) {
      *(unsigned char *)(section + 8) = active;
    }
  }
}

/* profile_sections_activate (0x90860) -- forward to the shared worker
 * FUN_000907c0 with active=1:
 *   MOV EDI,[EBP+8]  -- substring into EDI (the worker's @<edi> arg)
 *   PUSH 1           -- active = 1 (single cdecl stack arg)
 *   CALL 0x907c0 ; ADD ESP,4 */
void profile_sections_activate(const char *substring)
{
  FUN_000907c0((char *)substring, 1);
}

/* profile_sections_deactivate (0x90880) -- HaloScript "profile_sections_
 * deactivate" builtin.  The whole body is a forward to the shared worker
 * FUN_000907c0 with active=0:
 *   MOV EDI,[EBP+8]  -- substring into EDI (the worker's @<edi> arg)
 *   PUSH 0           -- active = 0 (single cdecl stack arg)
 *   CALL 0x907c0 ; ADD ESP,4
 * The sibling profile_sections_activate (0x90860) is the same shape with
 * PUSH 1. */
void profile_sections_deactivate(const char *substring)
{
  FUN_000907c0((char *)substring, 0);
}

/* profile_find_frame_value (0x908a0) -- map a frame-value name to its
 * index via a case-insensitive crt_stricmp chain (0x1dd801); unknown names
 * yield -1 (EDI preset by OR EDI,-1).  Note 'render' maps to 0x15, out of
 * sequence with its neighbours (binary: MOV EDI,0x15 at 0x90b05).  Every
 * exit writes -1 (MOV word ptr [EBX],0xffff) to *section_index_reference
 * and returns the 16-bit value (MOV AX,DI).  Assert line 0x446. */
int16_t profile_find_frame_value(const char *name,
                                 int16_t *section_index_reference)
{
  int16_t value;

  value = -1;
  if (name == NULL || section_index_reference == NULL) {
    display_assert("name && section_index_reference",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x446, 1);
    system_exit(-1);
  }

  if (crt_stricmp(name, "frame") == 0) {
    value = 1;
  } else if (crt_stricmp(name, "load") == 0) {
    value = 2;
  } else if (crt_stricmp(name, "game0") == 0) {
    value = 3;
  } else if (crt_stricmp(name, "game1") == 0) {
    value = 4;
  } else if (crt_stricmp(name, "game2") == 0) {
    value = 5;
  } else if (crt_stricmp(name, "game3") == 0) {
    value = 6;
  } else if (crt_stricmp(name, "game4") == 0) {
    value = 7;
  } else if (crt_stricmp(name, "game5") == 0) {
    value = 8;
  } else if (crt_stricmp(name, "game6") == 0) {
    value = 9;
  } else if (crt_stricmp(name, "game7") == 0) {
    value = 10;
  } else if (crt_stricmp(name, "player0") == 0) {
    value = 11;
  } else if (crt_stricmp(name, "player1") == 0) {
    value = 12;
  } else if (crt_stricmp(name, "player2") == 0) {
    value = 13;
  } else if (crt_stricmp(name, "player3") == 0) {
    value = 14;
  } else if (crt_stricmp(name, "nonplayer") == 0) {
    value = 15;
  } else if (crt_stricmp(name, "render") == 0) {
    value = 21;
  } else if (crt_stricmp(name, "render0") == 0) {
    value = 16;
  } else if (crt_stricmp(name, "render0_1") == 0) {
    value = 17;
  } else if (crt_stricmp(name, "render0_2") == 0) {
    value = 18;
  } else if (crt_stricmp(name, "render0_3") == 0) {
    value = 19;
  } else if (crt_stricmp(name, "render0_3np") == 0) {
    value = 20;
  } else if (crt_stricmp(name, "game_render") == 0) {
    value = 22;
  } else if (crt_stricmp(name, "stall") == 0) {
    value = 23;
  } else if (crt_stricmp(name, "texture") == 0) {
    value = 24;
  } else if (crt_stricmp(name, "idle") == 0) {
    value = 25;
  } else if (crt_stricmp(name, "dt") == 0) {
    value = 26;
  } else if (crt_stricmp(name, "gpu") == 0) {
    value = 27;
  } else if (crt_stricmp(name, "pushbuffer") == 0) {
    value = 28;
  }

  *section_index_reference = -1;
  return value;
}

/* Asserts name and section_index_reference are both non-null (per the
 * assert string), then unconditionally writes -1 (0xffff, the same
 * "not found"/"not started" sentinel used elsewhere in this file, e.g.
 * profile_frame_iterator_new) to *section_index_reference and returns -1.
 * Disassembly shows the null-check-failure path (display_assert +
 * system_exit) and the normal fallthrough path converge on the same
 * write+return -- name is only null-checked here, never dereferenced. */
int16_t profile_find_game_value(const char *name,
                                int16_t *section_index_reference)
{
  if (name == NULL || section_index_reference == NULL) {
    display_assert("name && section_index_reference",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x4c8, 1);
    system_exit(-1);
  }

  *section_index_reference = -1;
  return -1;
}

/* profile_frame_get_value (0x90d10) -- return one float statistic from the
 * ring-buffer entry selected by iterator->current_buffer_index (int16 at
 * iterator+0; ring base 0x3365c8, entry stride 0x1128). Asserts mirror
 * profile_frame_get_messages (lines 0x4d7/0x4d8 here). value_index is a
 * 1-based selector dispatched through the 28-entry jump table at 0x91040;
 * indices 4..10, 12..14 and anything outside 1..28 return 0.0f.
 * Entry field meanings are unconfirmed; offsets only:
 *   +0x10 int16 count of 0x18-stride floats at +0x40
 *   +0x12 int16 count of 0x18-stride floats at +0xe50, flags bytes at +0x14
 *   +0x1114 uint32, converted unsigned (FILD + 2^32 fixup) then scaled.
 * Index 20 adds every value but counts only flagged ones (binary FADD
 * precedes the JZ); kept as-is. */
float profile_frame_get_value(void *iterator, int16_t value_index)
{
  int16_t *it = (int16_t *)iterator;
  char *entry = (char *)(0x3365c8 + (int)it[0] * 0x1128);
  float result = 0.0f;
  int16_t count;
  int16_t found;
  int16_t i;

  if (it[0] < 0 || it[0] >= *(int16_t *)0x3365c2) {
    display_assert("(iterator->current_buffer_index >= 0) && "
                   "(iterator->current_buffer_index < "
                   "profile_globals.current_frame_history_count)",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x4d7, 1);
    system_exit(-1);
  }

  if (it[0] == *(int16_t *)0x3365c4) {
    display_assert("iterator->current_buffer_index != "
                   "profile_globals.current_frame_history_index",
                   "c:\\halo\\SOURCE\\cseries\\profile.c", 0x4d8, 1);
    system_exit(-1);
  }

  switch (value_index) {
  case 1:
    return *(float *)(entry + 0x28);
  case 2:
    return *(float *)(entry + 0x28) - *(float *)(entry + 0xef8);
  case 3:
    if (*(int16_t *)(entry + 0x10) > 0)
      return *(float *)(entry + 0x40);
    break;
  case 11:
    if (*(int16_t *)(entry + 0x12) > 0)
      return *(float *)(entry + 0xe50);
    break;
  case 15:
    count = *(int16_t *)(entry + 0x12);
    for (i = 0; i < count; i++) {
      if (*(entry + 0x14 + i) == 0)
        return *(float *)(entry + 0xe50 + i * 0x18);
    }
    break;
  case 16:
    count = *(int16_t *)(entry + 0x12);
    result = 0.0f;
    found = 0;
    for (i = 0; i < count; i++) {
      if (found >= 1)
        break;
      if (*(entry + 0x14 + i) != 0) {
        result += *(float *)(entry + 0xe50 + i * 0x18);
        found++;
      }
    }
    break;
  case 17:
    count = *(int16_t *)(entry + 0x12);
    result = 0.0f;
    found = 0;
    for (i = 0; i < count; i++) {
      if (found >= 2)
        break;
      if (*(entry + 0x14 + i) != 0) {
        result += *(float *)(entry + 0xe50 + i * 0x18);
        found++;
      }
    }
    break;
  case 18:
    count = *(int16_t *)(entry + 0x12);
    result = 0.0f;
    found = 0;
    for (i = 0; i < count; i++) {
      if (found >= 3)
        break;
      if (*(entry + 0x14 + i) != 0) {
        result += *(float *)(entry + 0xe50 + i * 0x18);
        found++;
      }
    }
    break;
  case 19:
    count = *(int16_t *)(entry + 0x12);
    result = 0.0f;
    found = 0;
    for (i = 0; i < count; i++) {
      if (found >= 4)
        break;
      if (*(entry + 0x14 + i) != 0) {
        result += *(float *)(entry + 0xe50 + i * 0x18);
        found++;
      }
    }
    break;
  case 20:
    count = *(int16_t *)(entry + 0x12);
    result = 0.0f;
    found = 0;
    for (i = 0; i < count; i++) {
      if (found >= 4)
        break;
      result += *(float *)(entry + 0xe50 + i * 0x18);
      if (*(entry + 0x14 + i) != 0)
        found++;
    }
    break;
  case 21:
    return *(float *)(entry + 0xeb0);
  case 22:
    count = *(int16_t *)(entry + 0x10);
    result = *(float *)(entry + 0xeb0);
    for (i = 0; i < count; i++)
      result += *(float *)(entry + 0x40 + i * 0x18);
    break;
  case 23:
    return *(float *)(entry + 0xec8);
  case 24:
    return *(float *)(entry + 0xee0);
  case 25:
    return *(float *)(entry + 0xef8);
  case 26:
    return *(float *)(entry + 0xf00) * 1000.0f;
  case 27:
    return *(float *)(entry + 0x1110);
  case 28:
    result = (float)*(uint32_t *)(entry + 0x1114) * 1.2715658e-06f * 33.333332f;
    break;
  }

  return result;
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
 * is the return value. Only caller: render_debug_profile (unlifted); field
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

/* profile_rasterizer_stalls (0x912c0) - cdecl, no callers found. Reads five
 * stack slots: dword [ebp+8], word [ebp+0xc], dword [ebp+0x10] (FILD + 2^32
 * fixup when negative = unsigned), dword [ebp+0x14] never read, qword
 * [ebp+0x18]. The 0x449c80 block uses the profile_timesection_end layout:
 * start stamp (+0x0) zeroed, end stamp (+0x8) = the qword, and its msec
 * conversion (same scale 0x254cb8 / freq 0x3361a0) added unnarrowed to the
 * floats at +0x10/+0x14. 0x449ee0/0x449ee4/0x449ee8 receive the dword, the
 * word and the msec conversion of the unsigned dword. Parameter meanings
 * beyond these slots are unconfirmed. */
void profile_rasterizer_stalls(int32_t value_08, int16_t value_0c,
                               uint32_t cycles_10, int32_t unused_14,
                               int64_t cycles_18)
{
  float elapsed;

  elapsed = (float)cycles_18 * *(float *)0x254cb8 / (float)unknown_3361a0;
  unknown_449c80 = 0;
  unknown_449c88 = cycles_18;
  unknown_449ee0 = value_08;
  unknown_449ee4 = value_0c;
  unknown_449c90 += elapsed;
  unknown_449c94 += elapsed;
  unknown_449ee8 = (float)cycles_10 * *(float *)0x254cb8 / (float)unknown_3361a0;
  (void)unused_14;
}

/* Read RDTSC into a caller-supplied low/high dword pair (register-argument
 * helper: out pointer arrives in ECX). Auto-lift-assigned name; no callers
 * found in this binary. Mirrors the RDTSC macro's two dword stores. */
void FUN_00091350(uint32_t *out /* @<ecx> */)
{
  uint32_t lo, hi;

  RDTSC(lo, hi);
  out[0] = lo;
  out[1] = hi;
}

/* profile_timesection_end_now (0x91380) - timesection arrives in ECX (read
 * before any write, no stack args, plain RET). Reads RDTSC, stores the end
 * stamp at +0x8/+0xc, subtracts the start stamp at +0x0, converts to msec
 * (FILD diff * [0x254cb8] / FILD [0x3361a0]) and adds the same unnarrowed
 * result to the floats at +0x10 and +0x14. Same field layout as
 * profile_timesection_end; field meanings beyond that are unconfirmed. */
void profile_timesection_end_now(void *timesection /* @<ecx> */)
{
  char *t = (char *)timesection;
  uint32_t lo, hi;
  int64_t diff;
  uint32_t *diff_parts;
  float elapsed;

  RDTSC(lo, hi);
  *(uint32_t *)(t + 0x8) = lo;
  *(uint32_t *)(t + 0xc) = hi;
  diff_parts = (uint32_t *)&diff;
  diff_parts[0] = lo;
  diff_parts[1] = hi;
  diff -= *(int64_t *)t;
  elapsed = (float)diff * *(float *)0x254cb8 / (float)*(int64_t *)0x3361a0;
  *(float *)(t + 0x10) += elapsed;
  *(float *)(t + 0x14) += elapsed;
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

/* FUN_00091ba0 (0x91ba0) -- paired end marker for FUN_00091b70. Same body
 * as profile_timesection_end_now, with the timesection at fixed global
 * 0x449cb0: reads RDTSC, stores the end stamp at 0x449cb8/0x449cbc,
 * subtracts the start stamp at 0x449cb0, converts to msec (FILD diff *
 * [0x254cb8] / FILD [0x3361a0]) and adds the same unnarrowed result to the
 * floats at 0x449cc0 and 0x449cc4. Field meanings are unconfirmed. */
void FUN_00091ba0(void)
{
  uint32_t lo, hi;
  int64_t diff;
  uint32_t *diff_parts;
  float elapsed;

  RDTSC(lo, hi);
  *(uint32_t *)0x449cb8 = lo;
  *(uint32_t *)0x449cbc = hi;
  diff_parts = (uint32_t *)&diff;
  diff_parts[0] = lo;
  diff_parts[1] = hi;
  diff -= *(int64_t *)0x449cb0;
  elapsed = (float)diff * *(float *)0x254cb8 / (float)*(int64_t *)0x3361a0;
  *(float *)0x449cc0 += elapsed;
  *(float *)0x449cc4 += elapsed;
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

/* FUN_00091cf0 (0x91cf0) -- generic in-place selection sort over an array of
 * 16-bit elements, ascending, using a caller-supplied comparator. Sibling of
 * FUN_00091ef0 (0x91ef0, the int/32-bit-keyed generic sort already declared
 * in kb.json): same shape (repeatedly scan for the "largest" element per
 * `compare`, swap it into the current tail slot, shrink the range by one
 * element), but this variant's elements and pointer stride are 2 bytes
 * (LEA ESI,[EAX+0x2] / SUB EDI,0x2 at 0x91d00/0x91d3e) instead of 4, and
 * `end` arrives in EAX (register arg) rather than on the stack. Sole xref is
 * an unconditional call from FUN_00091da0 (0x91de6), not yet lifted.
 *
 * `end` points at the last element to consider (inclusive); `begin` at the
 * first. No sort happens when the range holds 0 or 1 elements (0x91cf9
 * CMP EDI,EAX / JBE). Each outer pass linearly scans (begin, end] for the
 * element `compare` judges greatest, tracking it in `max_elem` (init'd to
 * `begin`), then swaps that element with *end and steps end down by one
 * element; the outer loop continues while end > begin (0x91d41/0x91d43).
 *
 * Compare call-site push order (0x91d1a-0x91d1c): PUSH *max_elem, PUSH
 * *scan, CALL -- cdecl right-to-left, so *scan is the first argument and
 * *max_elem the second: compare(*scan, *max_elem) != 0 means *scan replaces
 * the running max. Both values are loaded with XOR reg,reg + MOV r16
 * (explicit zero-extend, not MOVSX), so the element type is unsigned. */
void FUN_00091cf0(uint16_t *end /* @<eax> */, uint16_t *begin,
                  profile_sort16_compare_proc compare)
{
  uint16_t *scan;
  uint16_t *max_elem;
  uint16_t tmp;

  if (end <= begin) {
    return;
  }

  do {
    scan = begin + 1;
    max_elem = begin;
    if (scan <= end) {
      do {
        if (compare(*scan, *max_elem) != 0) {
          max_elem = scan;
        }
        scan++;
      } while (scan <= end);
    }

    tmp = *end;
    *end = *max_elem;
    *max_elem = tmp;
    end--;
  } while (end > begin);
}

/* FUN_00091d50 (0x91d50) -- generic in-place selection sort over an array of
 * 32-bit elements, ascending, using a caller-supplied comparator. Sibling of
 * FUN_00091cf0 (0x91cf0, the 16-bit-keyed variant just above): identical
 * shape (repeatedly scan for the "largest" element per `compare`, swap it
 * into the current tail slot, shrink the range by one element), but this
 * variant's elements and pointer stride are 4 bytes (LEA ESI,[EAX+0x4] /
 * SUB EDI,0x4 at 0x91d60/0x91d94) instead of 2, loaded with a plain 32-bit
 * MOV (dword ptr) rather than a zero-extending 16-bit MOV; `end` arrives in
 * EAX (register arg), same convention as the 16-bit sibling. Sole xref is
 * an unconditional call from FUN_00091ef0 (0x91f37), not yet lifted.
 *
 * `end` points at the last element to consider (inclusive); `begin` at the
 * first. No sort happens when the range holds 0 or 1 elements (0x91d59
 * CMP EDI,EAX / JBE). Each outer pass linearly scans (begin, end] for the
 * element `compare` judges greatest, tracking it in `max_elem` (init'd to
 * `begin`), then swaps that element with *end and steps end down by one
 * element; the outer loop continues while end > begin (0x91d97/0x91d99).
 *
 * Compare call-site push order (0x91d74-0x91d76): PUSH *max_elem, PUSH
 * *scan, CALL -- cdecl right-to-left, so *scan is the first argument and
 * *max_elem the second: compare(*scan, *max_elem) != 0 means *scan replaces
 * the running max. */
void FUN_00091d50(int32_t *end /* @<eax> */, int32_t *begin,
                  profile_sort32_compare_proc compare)
{
  int32_t *scan;
  int32_t *max_elem;
  int32_t tmp;

  if (end <= begin) {
    return;
  }

  do {
    scan = begin + 1;
    max_elem = begin;
    if (scan <= end) {
      do {
        if (compare(*scan, *max_elem) != 0) {
          max_elem = scan;
        }
        scan++;
      } while (scan <= end);
    }

    tmp = *end;
    *end = *max_elem;
    *max_elem = tmp;
    end--;
  } while (end > begin);
}

/* FUN_00091da0 (0x91da0) -- non-recursive quicksort over an array of 16-bit
 * elements with an explicit 30-entry lo/hi range stack (lo stack at
 * EBP-0x7c, hi stack at EBP-0xf4; frame SUB ESP,0xf4). Ranges of <= 8
 * elements (0x91dda CMP EAX,8 / JA) go to the selection sort FUN_00091cf0
 * (end in EAX, PUSH compare / PUSH lo). Otherwise the middle element is
 * swapped into *lo as pivot and the range is partitioned.
 *
 * `count` is compared unsigned (0x91dac CMP EAX,2 / JC); its stack slot is
 * reused as the range-stack depth (tested signed after DEC, 0x91df5 JS).
 * Element count uses SAR (signed pointer difference / 2). The
 * partition-size comparison at 0x91e80-0x91e8b is done on byte differences:
 * (higuy - lo) - 1 vs (hi - loguy), signed JL.
 *
 * Compare call sites (0x91e36/0x91e53): PUSH *lo, PUSH *elem, CALL -- so
 * compare(*elem, *lo); both loaded zero-extended; result tested as AL.
 * The forward scan continues while compare returns 0 (JZ 0x91e22), the
 * backward scan while it returns nonzero (JNZ 0x91e42). */
void FUN_00091da0(void *base, int count, void *compare)
{
  uint16_t *lo;
  uint16_t *hi;
  uint16_t *loguy;
  uint16_t *higuy;
  uint16_t *lostk[30];
  uint16_t *histk[30];
  uint32_t size;
  uint16_t tmp;
  int stkptr;
  profile_sort16_compare_proc comp;

  if ((uint32_t)count < 2) {
    return;
  }

  comp = (profile_sort16_compare_proc)compare;
  lo = (uint16_t *)base;
  stkptr = 0;
  hi = lo + (count - 1);

recurse:
  size = (uint32_t)(hi - lo) + 1;
  if (size <= 8) {
    FUN_00091cf0(hi, lo, comp);
  } else {
    size >>= 1;
    tmp = lo[size];
    lo[size] = *lo;
    *lo = tmp;

    loguy = lo;
    higuy = hi + 1;
    for (;;) {
      do {
        loguy++;
      } while (loguy <= hi && !comp(*loguy, *lo));
      do {
        higuy--;
      } while (higuy > lo && comp(*higuy, *lo));
      if (higuy < loguy) {
        break;
      }
      tmp = *loguy;
      *loguy = *higuy;
      *higuy = tmp;
    }

    tmp = *lo;
    *lo = *higuy;
    *higuy = tmp;

    if ((char *)higuy - (char *)lo - 1 >= (char *)hi - (char *)loguy) {
      if (lo + 1 < higuy) {
        lostk[stkptr] = lo;
        histk[stkptr] = higuy - 1;
        stkptr++;
      }
      if (loguy < hi) {
        lo = loguy;
        goto recurse;
      }
    } else {
      if (loguy < hi) {
        lostk[stkptr] = loguy;
        histk[stkptr] = hi;
        stkptr++;
      }
      if (lo + 1 < higuy) {
        hi = higuy - 1;
        goto recurse;
      }
    }
  }

  stkptr--;
  if (stkptr >= 0) {
    lo = lostk[stkptr];
    hi = histk[stkptr];
    goto recurse;
  }
}

/* FUN_00091ef0 (0x91ef0) -- non-recursive quicksort over an array of 32-bit
 * elements with an explicit 30-entry lo/hi range stack (lo stack at
 * EBP-0x7c, hi stack at EBP-0xf4; frame SUB ESP,0xf4). 32-bit sibling of
 * FUN_00091da0 (0x91da0) just above: same shape, dword loads/stores and
 * 4-byte stride. Ranges of <= 8 elements (0x91f2b CMP EAX,8 / JA) go to the
 * selection sort FUN_00091d50 (end in EAX, PUSH compare / PUSH lo at
 * 0x91f30-0x91f37). Otherwise the middle element is swapped into *lo as
 * pivot and the range is partitioned.
 *
 * `count` is compared unsigned (0x91efc CMP EAX,2 / JC); its stack slot is
 * reused as the range-stack depth (tested signed after DEC, 0x91f46 JS).
 * Element count uses SAR (signed pointer difference / 4). The
 * partition-size comparison at 0x91fc0-0x91fc9 is done on byte differences:
 * (higuy - lo) - 1 vs (hi - loguy), signed JL.
 *
 * Compare call sites (0x91f7e/0x91f9b): PUSH *lo, PUSH *elem, CALL [EBP+0x10]
 * -- so compare(*elem, *lo); result tested as AL (TEST AL,AL), hence the
 * bool-returning profile_sort32_compare_proc view of the kb-declared `cmp`.
 * The forward scan continues while compare returns 0 (JZ 0x91f70), the
 * backward scan while it returns nonzero (JNZ 0x91f90). */
void FUN_00091ef0(int *keys, int count, int (*cmp)(int, int))
{
  int32_t *lo;
  int32_t *hi;
  int32_t *loguy;
  int32_t *higuy;
  int32_t *lostk[30];
  int32_t *histk[30];
  uint32_t size;
  int32_t tmp;
  int stkptr;
  profile_sort32_compare_proc comp;

  if ((uint32_t)count < 2) {
    return;
  }

  comp = (profile_sort32_compare_proc)cmp;
  lo = (int32_t *)keys;
  stkptr = 0;
  hi = lo + (count - 1);

recurse:
  size = (uint32_t)(hi - lo) + 1;
  if (size <= 8) {
    FUN_00091d50(hi, lo, comp);
  } else {
    size >>= 1;
    tmp = lo[size];
    lo[size] = *lo;
    *lo = tmp;

    loguy = lo;
    higuy = hi + 1;
    for (;;) {
      do {
        loguy++;
      } while (loguy <= hi && !comp(*loguy, *lo));
      do {
        higuy--;
      } while (higuy > lo && comp(*higuy, *lo));
      if (higuy < loguy) {
        break;
      }
      tmp = *loguy;
      *loguy = *higuy;
      *higuy = tmp;
    }

    tmp = *lo;
    *lo = *higuy;
    *higuy = tmp;

    if ((char *)higuy - (char *)lo - 1 >= (char *)hi - (char *)loguy) {
      if (lo + 1 < higuy) {
        lostk[stkptr] = lo;
        histk[stkptr] = higuy - 1;
        stkptr++;
      }
      if (loguy < hi) {
        lo = loguy;
        goto recurse;
      }
    } else {
      if (loguy < hi) {
        lostk[stkptr] = loguy;
        histk[stkptr] = hi;
        stkptr++;
      }
      if (lo + 1 < higuy) {
        hi = higuy - 1;
        goto recurse;
      }
    }
  }

  stkptr--;
  if (stkptr >= 0) {
    lo = lostk[stkptr];
    hi = histk[stkptr];
    goto recurse;
  }
}

/* FUN_00092050 (0x92050) -- one-instruction setter: store the incoming
 * byte argument into stack_walk_load_failed (0x2ee784), a stack_walk_windows
 * global (see globals note at the top of stack_walk_windows.c) written from
 * this profile.obj-resident helper.
 *
 * Disassembly: PUSH EBP / MOV EBP,ESP / MOV AL,[EBP+8] / MOV [0x2ee784],AL /
 * POP EBP / RET -- a byte-width load and store, so the parameter is declared
 * uint8_t to match the reference's AL-sized argument fetch. No xrefs found
 * in the decompiled artifact (xrefs_to reported none); caller is elsewhere
 * in the unlifted binary. */
void FUN_00092050(uint8_t failed)
{
  stack_walk_load_failed = failed;
}

/* -----------------------------------------------------------------------
 * profile_idle_start (0x92060) -- cdecl comparator over two 0x10-byte
 * records, ordering them by the uint32 field at +0x04.  The kb name is the
 * pre-existing auto-generated label and is NOT supported by this body's
 * evidence: the only xref is a DATA reference from load_symbol_table
 * (0x92710) at 0x92cde, i.e. the address is taken as a function pointer,
 * and +0x04 is the value field of that loader's 0x10-byte symbol entries
 * (same field FUN_000921c0 returns on a match).  Renaming is left to a
 * separate recovery pass.
 *
 * Disassembly (0x92060..0x9208e), two stack args at [EBP+8]/[EBP+0xc]:
 *   MOV ECX,[EAX+4] / TEST ECX,ECX / JZ  -> MOV EAX,1   ; a == 0 -> 1
 *   MOV EAX,[EDX+4] / CMP ECX,EAX / JA   -> MOV EAX,1   ; unsigned a > b -> 1
 *   TEST EAX,EAX    / JZ           -> OR EAX,-1         ; b == 0 -> -1
 *   CMP ECX,EAX     / JC (below)   -> OR EAX,-1         ; a < b  -> -1
 *   fallthrough XOR EAX,EAX                             ; a == b -> 0
 * The comparisons are unsigned (JA/JC), so both fields are read as uint32.
 * The b == 0 arm is unreachable in practice (a != 0 and a <= b already
 * imply b != 0) but is emitted by the reference, so it is kept verbatim
 * rather than folded away.  The nesting (rather than a flat guard chain)
 * mirrors the reference's block order: both "return 1" exits are the
 * sunk tail block at 0x92088, so the two forward branches are taken on
 * a == 0 and on a > b.
 * ----------------------------------------------------------------------- */
int profile_idle_start(const void *a, const void *b)
{
  uint32_t value_a;
  uint32_t value_b;

  value_a = *(const uint32_t *)((const char *)a + 4);
  if (value_a != 0) {
    value_b = *(const uint32_t *)((const char *)b + 4);
    if (value_a <= value_b) {
      if (value_b != 0 && value_a >= value_b) {
        return 0;
      }
      return -1;
    }
  }

  return 1;
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

/* -----------------------------------------------------------------------
 * FUN_00092110 (0x92110) -- format a code address as "<name> + <offset> :
 * <name2>" into the static 0x4000-byte text buffer at 0x449f00 (size arg
 * 0x3fff) and return that buffer; "<unknown>" (0x25b724) is copied first and
 * returned unchanged when no entry matches.
 *
 * The address is biased by stack_walk_bias (LEA ESI,[EAX+ECX]). table uses
 * the same 3-word shape read by FUN_000921c0: table[0]=count,
 * table[1]=name-pool base, table[2]=entries base, 0x10-byte entries. Entry
 * +4 is compared as an unsigned start address (JC/JNC/JA). An entry i-1 is
 * selected when entry[i-1]+4 <= address < entry[i]+4, searching i=1..count-1,
 * after a range guard address >= entry[0]+4 and
 * address < entry[count-1]+4 + 0xffff. The snprintf args (first PUSH is the
 * last arg) are: pool + entry[i-1]+8, address - entry[i-1]+4,
 * pool + entry[i-1]+0xc. Field meanings of +8/+0xc beyond "pool offset"
 * are unknown.
 * ----------------------------------------------------------------------- */
char *FUN_00092110(int32_t addr, int32_t *symtab)
{
  uint32_t address;
  int32_t count;
  int32_t entries;
  int32_t index;
  int32_t entry;

  address = (uint32_t)(stack_walk_bias + addr);
  csstrcpy((char *)0x449f00, "<unknown>");
  count = symtab[0];
  if (count > 0) {
    entries = symtab[2];
    if (*(uint32_t *)(entries + 4) <= address &&
        address < *(uint32_t *)(count * 0x10 - 0xc + entries) + 0xffff) {
      for (index = 1; index < count; index++) {
        if (*(uint32_t *)(entries + index * 0x10 - 0xc) <= address &&
            address < *(uint32_t *)(entries + index * 0x10 + 4)) {
          entry = index * 0x10 + entries;
          snprintf((char *)0x449f00, 0x3fff, "%s + %04lX : %s",
                   (char *)(*(int32_t *)(entry - 8) + symtab[1]),
                   address - *(uint32_t *)(entry - 0xc),
                   (char *)(*(int32_t *)(entry - 4) + symtab[1]));
          return (char *)0x449f00;
        }
      }
    }
  }

  return (char *)0x449f00;
}

/* -----------------------------------------------------------------------
 * FUN_000921c0 (0x921c0) -- linear-search a 3-word table for an entry
 * whose name matches `name`, returning the matched entry's value field
 * (or -1 if none matched). table[0]=count, table[1]=name-pool base,
 * table[2]=entries base; this is the same 3-field shape documented for
 * symtab in symbol_table_dispose (0x92090) directly above, but that is
 * a structural resemblance only -- nothing in this function's own
 * disassembly proves it is the identical struct, so the parameter stays
 * generically named rather than reusing "symtab".
 *
 * Each entry is 0x10 bytes; entry+4 is the value returned on a match,
 * entry+8 is a byte offset into the name pool for that entry's name
 * string (entry_name = *(int32_t*)(entries+i*0x10+8) + table[1]).
 * The search starts at entry index 1, skipping entry 0 entirely (no
 * disassembly evidence for why -- likely a reserved/sentinel slot).
 *
 * Disassembly: no early-out on match -- TEST EAX,EAX / JNZ only skips
 * the result-store on a NON-match; the loop always runs to
 * index == table[0], and count/table[2] are re-read from memory on
 * every iteration rather than cached in a register. So if more than
 * one entry compares equal, the LAST matching entry's value wins. */
int32_t FUN_000921c0(const char *name, int32_t *table)
{
  int32_t result;
  int32_t index;
  int32_t offset;

  result = -1;
  index = 1;

  if (1 < table[0]) {
    offset = 0x10;
    do {
      if (csstrcmp(name, (char *)(*(int32_t *)(offset + 8 + table[2]) +
                                  table[1])) == 0) {
        result = *(int32_t *)(offset + 4 + table[2]);
      }
      index = index + 1;
      offset = offset + 0x10;
    } while (index < table[0]);
  }

  return result;
}

/* -----------------------------------------------------------------------
 * FUN_000922a0 (0x922a0) — walk EBP chain and collect return addresses.
 *
 * Custom register ABI: ECX = skip, EDX = frames[]; max and count are stack
 * arguments and the caller cleans them after the call.
 *
 * Same EBP-chain walk as FUN_00092370 (0x92370, stack_walk_windows.obj),
 * minus that function's "capture the caller's own EBP" preamble: this
 * helper starts from whatever g_sw_frame/g_sw_base (0x449ef8/0x449efc) the
 * caller already established.
 *
 * NOTE (uncertain, out of this lift's scope): the validity check here is
 * `frame>=base` treated as VALID (0x922bc `JNC` after `CMP EAX,EDI`),
 * confirmed identically in 0x92370's own disassembly (0x92398 `JNC`). The
 * existing FUN_00092370 lift in stack_walk_windows.c instead treats
 * `frame>=base` as INVALID, which looks inverted relative to both
 * functions' disassembly. This function is written to match the
 * disassembly directly rather than propagate that possible inversion. */
void FUN_000922a0(int skip, int32_t *frames, uint32_t max, uint32_t *count)
{
  uint32_t *frame;
  uint32_t *base;
  uint32_t return_address;
  uint32_t i;

  frame = *(uint32_t **)0x449ef8;
  base = *(uint32_t **)0x449efc;

  if (((uint32_t)frame & 3) != 0 || (uint32_t)frame < (uint32_t)base) {
    frame = NULL;
    *(uint32_t **)0x449ef8 = frame;
  }

  while (skip != 0) {
    skip--;
    if (skip == 0)
      break;
    if (frame != NULL) {
      frame = (uint32_t *)*frame;
      *(uint32_t **)0x449ef8 = frame;
      if (((uint32_t)frame & 3) != 0 || (uint32_t)frame < (uint32_t)base) {
        frame = NULL;
        *(uint32_t **)0x449ef8 = frame;
      }
      base = frame;
      *(uint32_t **)0x449efc = base;
    }
  }

  i = 0;
  if (max != 0) {
    for (;;) {
      return_address = 0;
      if (frame != NULL) {
        return_address = frame[1];
        frame = (uint32_t *)frame[0];
        *(uint32_t **)0x449ef8 = frame;
        if (((uint32_t)frame & 3) != 0 || (uint32_t)frame < (uint32_t)base) {
          frame = NULL;
          *(uint32_t **)0x449ef8 = frame;
        }
        base = frame;
        *(uint32_t **)0x449efc = base;
      }
      frames[i] = (int32_t)return_address;
      if (return_address == 0)
        break;
      i++;
      if (i >= max)
        break;
    }
  }
  *count = i;
}
