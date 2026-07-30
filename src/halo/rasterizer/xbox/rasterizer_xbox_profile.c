/*
 * rasterizer_xbox_profile.c
 *
 * GPU profile event begin/end markers, lifted from cachebeta.xbe.
 * Original TU: c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_profile.c
 *
 * Both functions validate a profile index (0..0x1c, i.e.
 * NUMBER_OF_RASTERIZER_PROFILES = 0x1d), emit "tell Bernie!" warnings via
 * FUN_0016f480 when begin/end pairing or per-frame duplication rules are
 * violated, and register FUN_0016f500 as a D3D push-buffer callback so the
 * GPU timestamps the profile section.
 *
 * Globals (not in kb.json, hardcoded):
 *   0x476ab0  void *    – IDirect3DDevice8 pointer (global_d3d_device)
 *   0x3256ba  short     – rasterizer debug mode selector (3 enables profiling)
 *   0x325704  char      – profile-enable flag (alternate gate)
 *   0x325184  short     – gate: must be 0 for profiling
 *   0x47e458  short     – profile-suppression nesting counter ("local_
 *                        profile_enable"); must be 0 for profiling.  Pushed/
 *                        popped by FUN_0016f8a0, which asserts 0 <= it < 100
 *   0x325180  short     – currently open profile index, -1 = none
 *   0x47e45c  unsigned  – bitmask of profiles completed this frame
 *   0x47e460  short     – sticky profile error bits, drained by FUN_0016f730:
 *                        1 = callback recieved invalid context, 2 = begin
 *                        out-of-synch, 4 = end out-of-synch.  Written word-
 *                        sized (FUN_0016f500, and the clear at 0x16f816);
 *                        FUN_0016f730 tests it with byte loads
 *   0x47e468  short     – "tell Bernie!" warning-emission counter, capped
 *                        at 3 (signed word CMP/JGE against 3)
 *   0x47e450  short     – current frame slot index, 0..15 (signed mod 16)
 *   0x47e454  short     – report frame slot index, indexes the 0x47e008 table
 *   0x47e440  dword     – per-frame accumulator, cleared on frame arm
 *   0x47e444  dword     – per-frame accumulator, cleared on frame arm
 *                        (0x47e440/0x47e444 are one int64: FUN_0016FDD0 reads
 *                        them with a single 64-bit negate, MOV EAX/MOV ECX/
 *                        NEG EAX/ADC ECX,0/NEG ECX)
 *   0x47e448  dword     – int64 with 0x47e44c; the saturated negation of the
 *   0x47e44c  dword     – 0x47e440 accumulator, stored via CDQ so the high
 *                        dword is always a sign-extension of the low
 *   0x47e188  dword[2] x 0x1d – per-profile record (8-byte stride), both
 *                        dwords zeroed on begin and on end
 */

/* 0x16f500 — GPU push-buffer profile callback: timestamps the begin or the
 * end of a profile section.  Registered with D3DDevice_InsertCallback by
 * FUN_0016f910 (type 0, context = index | 0x80000000) and FUN_0016fa40
 * (type 1, context = index), so the single stack argument packs two fields:
 *
 *   bit 31    - begin flag (SHR EBX,0x1f / AND BL,0x1 at 0x16f50d)
 *   bits 0-15 - profile index, validated 0..0x1c against
 *               NUMBER_OF_RASTERIZER_PROFILES = 0x1d.  The original tests the
 *               *16-bit* half (TEST SI,SI / CMP SI,0x1d) precisely so the
 *               begin flag in bit 31 stays outside the range compare.
 *
 * kb.json declared this void(void); the prologue's MOV ESI,[EBP+0x8] plus the
 * plain RET (no immediate) prove one __cdecl stack argument.
 *
 * 0x47e460 is a 16-bit warning word - every access is a word load/store
 * (XOR reg,reg; MOV reg16,[0x47e460]; OR reg,imm; MOV word[0x47e460],reg16):
 *   1 - profile index out of range
 *   2 - begin over an already-open profile
 *   4 - end with no open profile
 * Each of the three warning paths ends in a dead re-read of the word
 * (0x16f559 / 0x16f5de / 0x16f5fd), the residue of a macro that yields the
 * flag word; the volatile qualifier is what keeps the load/modify/store split
 * rather than a folded OR word ptr[...],imm.
 *
 * Per-profile 8-byte (LARGE_INTEGER) slots, 0x1d entries - the same arrays
 * FUN_0016f6c0 zeroes:
 *   0x47e358 - open-section start timestamp, 0 = no section open
 *   0x47e270 - elapsed ticks of the last completed section
 *
 * The begin path indexes with a scaled index (MOVSX ECX,SI then
 * [ECX*0x8 + base]); the end path with a pre-scaled byte offset (MOVSX EAX,SI;
 * SHL EAX,0x3 then [EAX + base]), the same `offset` idiom as FUN_0016f6c0 -
 * hence the two different local spellings below.  The 64-bit subtract is the
 * original's SUB/SBB pair at 0x16f59c/0x16f5a1, now minus start. */
void FUN_0016f500(int profile_and_flag)
{
  /* one 8-byte LARGE_INTEGER at [EBP-0x8]; LEA EAX,[EBP-0x8] is the QPC arg */
  unsigned __int64 now;
  short profile;
  unsigned char is_begin;
  int slot;
  int offset;

  profile = (short)profile_and_flag;
  is_begin = (unsigned char)(((unsigned int)profile_and_flag >> 0x1f) & 1);
  if (profile < 0 || profile >= 0x1d) {
    *(volatile unsigned short *)0x47e460 =
        (unsigned short)(*(volatile unsigned short *)0x47e460 | 1);
    (void)*(volatile unsigned short *)0x47e460; /* dead re-read at 0x16f5fd */
    return;
  }
  QueryPerformanceCounter(&now);
  if (is_begin) {
    slot = profile;
    if (*(unsigned __int64 *)(slot * 8 + 0x47e358) != 0) {
      *(volatile unsigned short *)0x47e460 =
          (unsigned short)(*(volatile unsigned short *)0x47e460 | 2);
      (void)*(volatile unsigned short *)0x47e460; /* dead re-read at 0x16f559 */
    }
    *(unsigned __int64 *)(slot * 8 + 0x47e358) = now;
    return;
  }
  offset = (int)profile * 8;
  /* the open-slot test reads the two halves (MOV ECX,[..358]; OR ECX,[..35c]
   * at 0x16f57f) and the subtract below re-loads them (0x16f58d/0x16f596) */
  if ((*(unsigned int *)(offset + 0x47e358) |
       *(unsigned int *)(offset + 0x47e35c)) != 0) {
    *(unsigned __int64 *)(offset + 0x47e270) =
        now - *(unsigned __int64 *)(offset + 0x47e358);
    *(unsigned __int64 *)(offset + 0x47e358) = 0;
    return;
  }
  *(volatile unsigned short *)0x47e460 =
      (unsigned short)(*(volatile unsigned short *)0x47e460 | 4);
  (void)*(volatile unsigned short *)0x47e460; /* dead re-read at 0x16f5de */
}

/* 0x16f6c0 - profiler frame/state reset ("rasterizer_profile_initialize"
 * shaped).  Zeroes the two per-profile record arrays that follow 0x47e188
 * (0x47e270 and 0x47e358, both dword[2] x 0x1d, 8-byte stride), clears the
 * three 0x80-byte tables at 0x47e008/0x47e088/0x47e108 that immediately
 * precede 0x47e188, and latches the performance-counter frequency into
 * 0x325178 (LARGE_INTEGER).
 *
 * The original returns TRUE (`MOV AL,0x1` at 0x16f72b, immediately before
 * `POP ESI; RET`) - hence the bool return rather than the void the
 * decompiler reports.  No callers are known (no xrefs; probably reached
 * through a function/callback table), so the return value's consumer is
 * unidentified.
 *
 * The zeroing loop's four stores are emitted in the original's order
 * (0x47e358, 0x47e35c, 0x47e270, 0x47e274) with a single shared byte offset,
 * matching the EAX offset / ESI=0 / ECX=0x1d shape of the disassembly.
 * The three csmemset calls share one batched `ADD ESP,0x24` in the original;
 * their order (0x108, 0x088, 0x008) is as MSVC emitted it.
 *
 * NOTE: this kb.json entry carries no object mapping, so maintain.py routes
 * it to rasterizer.c; binary evidence (the 0x47eXXX profile record arrays and
 * the 0x1d NUMBER_OF_RASTERIZER_PROFILES count shared with FUN_0016f910 /
 * FUN_0016fa40) places it in this TU. */
bool FUN_0016f6c0(void)
{
  int count;
  int offset;

  count = 0x1d;
  offset = 0;
  do {
    *(unsigned int *)(offset + 0x47e358) = 0;
    *(unsigned int *)(offset + 0x47e35c) = 0;
    *(unsigned int *)(offset + 0x47e270) = 0;
    *(unsigned int *)(offset + 0x47e274) = 0;
    offset += 8;
    count--;
  } while (count != 0);
  csmemset((void *)0x47e108, 0, 0x80);
  csmemset((void *)0x47e088, 0, 0x80);
  csmemset((void *)0x47e008, 0, 0x80);
  QueryPerformanceFrequency((void *)0x325178);
  return 1;
}

/* 0x16f730 — end-of-frame profile flush / next-frame arm (assert at
 * rasterizer_xbox_profile.c line 0xc2, the same __FILE__ string at 0x2a3ca4
 * the rest of this TU uses).
 *
 * Drains the three sticky error bits in 0x47e460 as "tell Bernie!" warnings
 * (at most three warnings ever, via the counter at 0x47e468), clears the bit
 * word, and — when profiling is enabled — advances the 16-slot frame index
 * at 0x47e450 and inserts the FUN_0016f610 push-buffer marker with context
 * index*2.
 *
 * Frame is PUSH EBP / MOV EBP,ESP / PUSH ECX / PUSH EBX / PUSH ESI ...
 * POP EBX / MOV ESP,EBP / POP EBP / RET (no immediate), so __cdecl void(void);
 * the EBP-4 slot is a dead MSVC temp for the bit test and is not a variable.
 * EBX is the shared zero and ESI the shared constant 3.
 *
 * Widths are from the disassembly, not the decompiler (which renders the
 * clear as a 32-bit store): the three reads of 0x47e460 are
 * `MOV AL,byte ptr [0x47e460]` while the clear at 0x16f816 is
 * `MOV word ptr [0x47e460],BX`.  0x47e468 is word-sized with a signed
 * compare (`CMP word ptr [0x47e468],SI` / JGE) and `INC word ptr`, and
 * 0x47e450 is read `MOVSX EAX,word ptr`, hence `short` throughout.
 *
 * The `~bit & 1` tests are transcribed literally from `NOT AL; AND AL,1;
 * JNZ past`: the warning fires when the original bit IS set.
 *
 * The five zero-stores are emitted between the mod-16 AND and its negative
 * fixup (0x16f828-0x16f860); that is scheduling, the source order below is
 * the disassembly's store order.  `AND EAX,0x8000000f` + `JNS/DEC/OR
 * 0xfffffff0/INC` is MSVC's signed `% 16`, not `& 15`.  The callback context
 * re-reads 0x47e450 after the store (`MOV [0x47e450],AX; MOVSX EAX,AX;
 * SHL EAX,1`).
 *
 * Like the rest of this TU, the kb.json entry carries no object mapping, so
 * maintain.py wants to route it to rasterizer.c; it is kept here on the
 * shared-__FILE__ and shared-globals evidence. */
void FUN_0016f730(void)
{
  unsigned char bit_clear;
  int slot;
  short frame_slot;

  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "profile.c",
                   0xc2, 1);
    system_exit(-1);
  }

  bit_clear = (unsigned char)(~(*(volatile unsigned char *)0x47e460) & 1);
  if (bit_clear == 0 && *(short *)0x47e468 < 3) {
    error(2, "### PROFILE: %s -- tell Bernie!", 0xffffffff,
          "callback recieved invalid context");
    *(short *)0x47e468 = *(short *)0x47e468 + 1;
  }
  bit_clear = (unsigned char)(~(*(volatile unsigned char *)0x47e460 >> 1) & 1);
  if (bit_clear == 0 && *(short *)0x47e468 < 3) {
    error(2, "### PROFILE: %s -- tell Bernie!", 0xffffffff,
          "begin out-of-synch");
    *(short *)0x47e468 = *(short *)0x47e468 + 1;
  }
  bit_clear = (unsigned char)(~(*(volatile unsigned char *)0x47e460 >> 2) & 1);
  if (bit_clear == 0 && *(short *)0x47e468 < 3) {
    error(2, "### PROFILE: %s -- tell Bernie!", 0xffffffff,
          "end out-of-synch");
    *(short *)0x47e468 = *(short *)0x47e468 + 1;
  }
  *(volatile short *)0x47e460 = 0;

  /* The index read is hoisted above the gate compare: that placement is
   * what reproduces the original's scheduling (it costs 2.7pp of match
   * when written inside the branch).  0x47e450 is a plain global that
   * nothing above writes, so the hoist is observationally inert. */
  frame_slot = *(short *)0x47e450;
  if (*(short *)0x3256ba == 3 || *(char *)0x325704 != 0) {
    slot = ((int)frame_slot + 1) % 16;
    *(unsigned int *)0x47e45c = 0;
    *(short *)0x325184 = 0;
    *(short *)0x325180 = -1;
    *(unsigned int *)0x47e440 = 0;
    *(unsigned int *)0x47e444 = 0;
    *(short *)0x47e450 = (short)slot;
    D3DDevice_InsertCallback(0, (void *)FUN_0016f610,
                             (unsigned int)((int)*(short *)0x47e450 << 1));
  }
}

/* 0x16f8a0 — nesting counter for the profile-disable gate 0x47e458
 * (asserts at rasterizer_xbox_profile.c lines 0xf4/0xf9; the assert reason
 * strings name the counter "local_profile_enable").
 *
 * A non-zero argument decrements the counter (asserting it is > 0 first); a
 * zero argument increments it (asserting it is < 100).  Since FUN_0016f910 /
 * FUN_0016fa40 only profile while 0x47e458 == 0, the counter is a
 * suppression depth: the zero-argument call pushes a suppression, the
 * non-zero call pops one.  The parameter name follows that reading
 * (true => allow profiling again); the polarity is Inferred from the gate
 * compare in the neighbouring functions, not from a string.
 *
 * Frame is PUSH EBP / MOV EBP,ESP / ... / POP EBP / RET (no immediate), so
 * __cdecl with a single 1-byte argument read as `MOV AL,[EBP+8]; TEST AL,AL`.
 * 0x47e458 is accessed with word-sized INC/DEC/CMP, hence `short`.
 *
 * Like FUN_0016f6c0 above, this kb.json entry carries no object mapping, so
 * maintain.py wants to route it to rasterizer.c; it is kept here on the same
 * binary evidence (the 0x47e458 gate shared with FUN_0016f910 /
 * FUN_0016fa40 and the shared __FILE__ string at 0x2a3ca4). */
void FUN_0016f8a0(bool enable_profiling)
{
  if (enable_profiling) {
    if (*(short *)0x47e458 <= 0) {
      display_assert("local_profile_enable>0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0xf4, 1);
      system_exit(-1);
    }
    *(short *)0x47e458 = *(short *)0x47e458 - 1;
  } else {
    if (*(short *)0x47e458 >= 100) {
      display_assert("local_profile_enable<100",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0xf9, 1);
      system_exit(-1);
    }
    *(short *)0x47e458 = *(short *)0x47e458 + 1;
  }
}

/* 0x16f910 — profile section begin (asserts at rasterizer_xbox_profile.c
 * lines 0x103/0x109/0x10a) */
void FUN_0016f910(short profile)
{
  int slot;
  unsigned int bit;

  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "profile.c",
                   0x103, 1);
    system_exit(-1);
  }
  if ((*(short *)0x3256ba == 3 || *(char *)0x325704 != 0) &&
      *(short *)0x325184 == 0 && *(short *)0x47e458 == 0) {
    if (profile < 0 || profile >= 0x1d) {
      display_assert("profile>=0 && profile<NUMBER_OF_RASTERIZER_PROFILES",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0x109, 1);
      system_exit(-1);
    }
    if (*(int *)0x476ab0 == 0) {
      display_assert("global_d3d_device",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0x10a, 1);
      system_exit(-1);
    }
    slot = profile;
    bit = 1u << profile;
    /* warn when this profile already ran this frame */
    FUN_0016f480("profile duplication within frame (begin)", profile,
                 (char)((bit & *(unsigned int *)0x47e45c) == 0));
    /* warn when another profile section is still open */
    FUN_0016f480("profile begin/end pairing incorrect (begin)", profile,
                 (char)(*(short *)0x325180 == -1));
    /* type 0 callback; context = profile index with begin flag in bit 31 */
    D3DDevice_InsertCallback(0, (void *)FUN_0016f500,
                             (unsigned int)slot | 0x80000000u);
    *(short *)0x325180 = profile;
    *(unsigned int *)(slot * 8 + 0x47e188) = 0;
    *(unsigned int *)(slot * 8 + 0x47e18c) = 0;
  }
}

/* 0x16fa40 — profile section end (asserts at rasterizer_xbox_profile.c
 * lines 0x126/0x12c/0x12d) */
void FUN_0016fa40(short profile)
{
  int slot;
  /* volatile reproduces the original's [EBP-4] spill of the profile bit:
   * MOV [EBP-4],EAX after the SHL, reloaded for the final mask OR
   * (permuter-verified shape, 87.2% -> 95.5%) */
  volatile unsigned int bit_spill;
  unsigned int bit;
  unsigned int masked;

  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "profile.c",
                   0x126, 1);
    system_exit(-1);
  }
  if ((*(short *)0x3256ba == 3 || *(char *)0x325704 != 0) &&
      *(short *)0x325184 == 0 && *(short *)0x47e458 == 0) {
    if (profile < 0 || profile >= 0x1d) {
      display_assert("profile>=0 && profile<NUMBER_OF_RASTERIZER_PROFILES",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0x12c, 1);
      system_exit(-1);
    }
    if (*(int *)0x476ab0 == 0) {
      display_assert("global_d3d_device",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0x12d, 1);
      system_exit(-1);
    }
    bit_spill = 1u << profile;
    slot = profile;
    bit = bit_spill;
    masked = bit & *(unsigned int *)0x47e45c;
    /* warn when this profile already ended this frame */
    FUN_0016f480("profile duplication within frame (end)", profile,
                 (char)(masked == 0));
    /* warn when the open profile is not the one being ended */
    FUN_0016f480("profile begin/end pairing incorrect (end)", profile,
                 (char)(*(short *)0x325180 == profile));
    /* type 1 callback; context = profile index (no begin flag) */
    D3DDevice_InsertCallback(1, (void *)FUN_0016f500, (unsigned int)slot);
    *(unsigned int *)(slot * 8 + 0x47e188) = 0;
    *(unsigned int *)(slot * 8 + 0x47e18c) = 0;
    *(short *)0x325180 = -1;
    *(unsigned int *)0x47e45c |= bit;
  }
}

/* 0x16fbd0 — profile section elapsed-time query (asserts at
 * rasterizer_xbox_profile.c lines 0x177/0x187).
 *
 * Ghidra typed this `void FUN_0016fbd0(void)`: BOTH the stack parameter
 * (MOV EDI,[EBP+8]; CMP DI,0x1d — 16-bit signed compares throughout) and the
 * ST0 float return were lost, and the accumulator loop was collapsed into an
 * empty countdown with the FADDP/FDIVP dropped.  Both return expressions are
 * reconstructed from the disassembly.
 *
 * Returns seconds: the per-profile performance-counter delta at
 * 0x47e270 (int64, 8-byte stride, 0x1d entries) divided by the
 * QueryPerformanceFrequency LARGE_INTEGER latched into 0x325178 by
 * FUN_0016f6c0.  Both operands are loaded with FILD qword (signed 64-bit),
 * and the dividend is the counter — not the reverse.
 *
 * profile == 0x1d (NUMBER_OF_RASTERIZER_PROFILES) is the "all profiles"
 * request and sums all 0x1d counters.  The accumulator is the same x87
 * register value that the entry FLD/FST of 0.0f (0x2533c0) seeds, which is
 * why the entry constant doubles as the gate-off return value and the
 * assert paths FSTP ST0 to discard it.
 *
 * The two no-data constants differ: 0.0f (0x2533c0) when profiling is
 * disabled, -1.0f (0x255e94) when the requested profile has not completed
 * this frame.
 *
 * The int64 counter is copied through the 8-byte frame slot at EBP-8 before
 * FILD qword, matching the original's two 32-bit MOVs per iteration; hence
 * the explicit `counter_value` local rather than a direct cast of the
 * dereference. */
float FUN_0016fbd0(int16_t profile)
{
  float total;
  /* volatile reproduces the original's 8-byte staging slot at EBP-8: each
   * counter is copied through it with two 32-bit MOVs before FILD qword
   * (SUB ESP,8 in the prologue).  Without it VC71 folds the copy away and
   * FILDs straight from the counter array. */
  volatile int64_t counter_value;
  const int64_t *counter;
  int count;
  int slot;

  total = 0.0f;
  if (*(short *)0x3256ba == 3 || *(char *)0x325704 != 0) {
    if (*(int *)0x476ab0 == 0) {
      display_assert("global_d3d_device",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0x177, 1);
      system_exit(-1);
    }
    if (profile == 0x1d) {
      /* "all profiles": sum every per-profile counter */
      counter = (const int64_t *)0x47e270;
      count = 0x1d;
      do {
        counter_value = *counter;
        total += (float)counter_value;
        counter++;
        count--;
      } while (count != 0);
      return total / (float)*(int64_t *)0x325178;
    }
    if (profile < 0 || profile >= 0x1d) {
      display_assert("profile>=0 && profile<NUMBER_OF_RASTERIZER_PROFILES",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0x187, 1);
      system_exit(-1);
    }
    /* warn when the profile has not completed this frame */
    FUN_0016f480("profile not completed (query)", profile,
                 (char)(*(short *)0x325180 == -1));
    slot = profile;
    if ((1u << (unsigned int)slot) & *(unsigned int *)0x47e45c) {
      counter_value = *(const int64_t *)(slot * 8 + 0x47e270);
      return (float)counter_value / (float)*(int64_t *)0x325178;
    }
    total = -1.0f;
  }
  return total;
}

/* 0x16fcf0 — profile section raw-tick query (asserts at
 * rasterizer_xbox_profile.c lines 0x19f/0x1ae).
 *
 * The integer sibling of FUN_0016fbd0: same gate, same asserts, same
 * "profile not completed" warning, but it returns the raw performance-counter
 * delta from the 0x47e188 record array (int64, 8-byte stride, 0x1d entries)
 * saturated into an int32, rather than seconds from 0x47e270.
 *
 * Ghidra typed this `void FUN_0016fcf0(void)`: BOTH the stack parameter
 * (MOV EDI,[EBP+8]; CMP DI,0x1d — 16-bit signed compares throughout) and the
 * EAX return were lost (§16 void-EAX), and the tail was reduced to bare
 * `return`s.  Four distinct return values are reconstructed from the
 * disassembly:
 *   profiling disabled          -> 0     (XOR EAX,EAX in the prologue)
 *   profile == 0x1d             -> 0     (its own XOR EAX,EAX + epilogue)
 *   profile not completed       -> -1    (OR EAX,0xffffffff at 0x16fdc1)
 *   otherwise                   -> saturated int32 of the int64 counter
 *
 * Unlike FUN_0016fbd0, profile == 0x1d does NOT sum all profiles here — it
 * returns 0 — and the check happens BEFORE the range assert, so 0x1d is the
 * "no profile" sentinel rather than an "all profiles" request.
 *
 * The saturation is a single signed 64-bit compare against 0x7fffffff; MSVC
 * expands it to the TEST EDX,EDX / JL / JG / CMP EAX,0x7fffffff / JBE
 * tri-branch seen in the original.  The int64 is read straight out of the
 * array (no EBP-8 staging slot and no volatile here — the original's frame is
 * PUSH EBP; MOV EBP,ESP; XOR EAX,EAX; PUSH EDI with no SUB ESP at all, so
 * both the counter and the slot index stay in registers).
 *
 * The assert tails are `display_assert(..., 1); system_exit(-1);` (PUSH -1;
 * CALL 0x8e2f0), not halt_and_catch_fire — Ghidra's thunk_FUN_001029a0 is
 * wrong. */
int FUN_0016fcf0(int16_t profile)
{
  int result;
  int slot;
  int64_t counter_value;

  result = 0;
  if (*(short *)0x3256ba == 3 || *(char *)0x325704 != 0) {
    if (*(int *)0x476ab0 == 0) {
      display_assert("global_d3d_device",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0x19f, 1);
      system_exit(-1);
    }
    if (profile == 0x1d) {
      return 0;
    }
    if (profile < 0 || profile >= 0x1d) {
      display_assert("profile>=0 && profile<NUMBER_OF_RASTERIZER_PROFILES",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0x1ae, 1);
      system_exit(-1);
    }
    /* warn when the profile has not completed this frame */
    FUN_0016f480("profile not completed (query)", profile,
                 (char)(*(short *)0x325180 == -1));
    slot = profile;
    if ((1u << (unsigned int)slot) & *(unsigned int *)0x47e45c) {
      counter_value = *(const int64_t *)(slot * 8 + 0x47e188);
      if (counter_value > 0x7fffffff) {
        return 0x7fffffff;
      }
      return (int)counter_value;
    }
    result = -1;
  }
  return result;
}

/* 0x16fdd0 — end-of-frame profile report (assert at
 * rasterizer_xbox_profile.c line 0x1c7).
 *
 * Runs behind the same profile-enabled gate as the rest of the file, then does
 * three things: saturate the negated per-frame accumulator into the int64 at
 * 0x47e448, insert a type-1 FUN_0016f610 push-buffer marker with the odd
 * context `slot * 2 + 1`, and hand the reporting pair (elapsed seconds,
 * saturated ticks) to FUN_0008f810.
 *
 * The negate is a real 64-bit negation of 0x47e440 (MOV EAX,[0x47e440];
 * MOV ECX,[0x47e444]; NEG EAX; ADC ECX,0; NEG ECX), and the saturation reuses
 * the flags NEG ECX just set — JS skips when the result is negative, JG
 * saturates when the high dword is positive, otherwise CMP EAX,0x7fffffff /
 * JBE.  The store is CDQ; MOV [0x47e448],EAX; MOV [0x47e44c],EDX, i.e. the
 * high dword is a sign-extension of the truncated low dword, not an
 * independent value — so a negative accumulator that does not fit in 32 bits
 * is silently truncated rather than clamped.  Ghidra reconstructed this as
 * three separate DAT_ assignments plus an unreachable `0x7ffffffe <` compare;
 * the disassembly is a plain signed int64 compare against 0x7fffffff.
 *
 * The original's frame is PUSH EBP; MOV EBP,ESP; SUB ESP,0x8 for the int64
 * negate staging slot.  [EBP-4] is written on both paths and never read — it
 * is dead staging, not a variable.
 *
 * FUN_0008f810 call site (0x16fe95) — three stack dwords, ADD ESP,0xc:
 * MOVSX ECX,word[0x47e454]; MOV EDX,[0x47e44c]; MOV EAX,[0x47e448];
 * FILD qword[ECX*8+0x47e008]; PUSH EDX; PUSH EAX; PUSH ECX;
 * FMUL dword[0x254cb8]; FILD qword[0x325178]; FDIVP; FSTP dword[ESP].
 * The PUSH ECX slot is overwritten by FSTP [ESP], so ECX is NOT an argument —
 * arg1 is the FPU-computed float and arg2 is the int64 re-read out of
 * 0x47e448 (PUSH high, PUSH low is MSVC's 64-bit push order).  kb.json
 * declared FUN_0008f810 as (int, int); its delinked body reads [EBP+8] and
 * [EBP+0xc] only, so the high dword is passed but unused.  Corrected to
 * (float, int64_t) — the 12-byte cleanup and the float in slot 1 are both
 * unambiguous.
 *
 * Both FILDs are qword: the 0x47e008 sample and the timer frequency at
 * 0x325178 are int64.  FDIVP computes st(1)/st(0), so the sample (scaled by
 * the float at 0x254cb8) is the numerator and the frequency the divisor.
 *
 * The assert tail is `display_assert(..., 1); system_exit(-1);` (PUSH -1;
 * CALL 0x8e2f0), not halt_and_catch_fire — Ghidra's thunk_FUN_001029a0 is
 * wrong. */
void FUN_0016FDD0(void)
{
  int64_t negated;
  int saturated;
  int slot;

  if (*(short *)0x3256ba == 3 || *(char *)0x325704 != 0) {
    if (*(int *)0x476ab0 == 0) {
      display_assert("global_d3d_device",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "profile.c",
                     0x1c7, 1);
      system_exit(-1);
    }
    negated = -*(const int64_t *)0x47e440;
    saturated = (int)negated;
    if (negated > 0x7fffffff) {
      saturated = 0x7fffffff;
    }
    *(int64_t *)0x47e448 = saturated;
    D3DDevice_InsertCallback(1, (void *)FUN_0016f610,
                             (unsigned int)(*(short *)0x47e450 * 2 + 1));
    slot = *(short *)0x47e454;
    FUN_0008f810((float)*(const int64_t *)(slot * 8 + 0x47e008) *
                     *(const float *)0x254cb8 /
                     (float)*(const int64_t *)0x325178,
                 *(const int64_t *)0x47e448);
  }
}
