/* MSVC CRT pow(): compiles to the _CIpow intrinsic (0x1d9e70 dispatcher,
 * body at 0x1d9e94 uses fyl2x). Not in decl.h; declared locally as in
 * objects.c so the compiler emits the intrinsic. */
double pow(double x, double y);

/* rasterizer_plasma_energy_draw (FUN_0016eef0): emit the plasma-energy
 * transparent shader (shader type 10) for one geometry group. Binds the two
 * noise-map textures (primary at shader+0xe0, secondary at shader+0x128),
 * sets fixed-function / render state, computes the two animation scroll
 * matrices (vs const block A: 6 vec4) and the perpendicular/parallel tint
 * colour rows (block B: 3 vec4) into vertex-shader constants, then installs
 * the plasma pixel shader and draws.
 * Original TU: c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_plasma_energy.c
 */
void FUN_0016eef0(void *group)
{
  char *grp = (char *)group;
  char *p; /* plasma shader params base = FUN_001906b0(..)+0x28 */
  int *params; /* grp+0x6c: {color_table, pow_table} pointer pair */
  float *color_table; /* params[0]: 12-byte-stride colour entries */
  float *pow_table; /* params[1]: exponent-table floats [EBP-0xc] */
  float *tint; /* [EBX] tint rgb triple */
  float alpha_c; /* [EBP-8] default 1.0 */
  float alpha_s; /* [EBP-4] default 0.0 */
  float t; /* DAT_005a5e18 scroll time */
  float adiv; /* t / primary_noise_map_animation_period */
  float bdiv; /* t / secondary_noise_map_animation_period */
  float dupA; /* shader+0xa8 diagonal duplicate (primary) */
  float dupB; /* shader+0xf0 diagonal duplicate (secondary) */
  float vsA[24]; /* [EBP-0x9c] vertex-shader const block A (6 vec4) */
  float vsB[12]; /* [EBP-0x3c] vertex-shader const block B (3 vec4) */
  short idx;

  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "plasma_energy.c",
                   0x15, 1);
    system_exit(-1);
  }
  if (*(char *)0x3256fb == 0) {
    return;
  }

  p = (char *)FUN_001906b0(*(void **)(grp + 0xc), 10) + 0x28;
  tint = *(float **)0x2ee708;
  alpha_c = 1.0f;
  alpha_s = 0.0f;

  params = *(int **)(grp + 0x6c);
  if (params != 0) {
    color_table = *(float **)params;
    if (color_table != 0) {
      idx = *(short *)(p + 0x58);
      if (idx >= 1 && idx <= 4) {
        tint = (float *)((char *)color_table + (idx - 1) * 12);
      }
    }
    pow_table = *(float **)(params + 1);
    if (pow_table != 0) {
      idx = *(short *)(p + 4);
      if (idx >= 1 && idx <= 4) {
        alpha_c =
          (float)pow((double)pow_table[idx - 1], (double)*(float *)(p + 8));
      }
      idx = *(short *)(p + 0xc);
      if (idx >= 1 && idx <= 4) {
        alpha_s =
          (float)pow((double)pow_table[idx - 1], (double)*(float *)(p + 0x14)) *
          *(float *)(p + 0x10);
      }
    }
  }

  /* Bind the two noise-map textures and their fixed-function stage state. */
  rasterizer_set_texture(0, 1, 0, *(int *)(p + 0xb8),
                         *(unsigned short *)(grp + 0x10));
  SetTextureStageStateSmart(0, 0xa, 1);
  SetTextureStageStateSmart(0, 0xb, 1);
  SetTextureStageStateSmart(0, 0xc, 1);
  SetTextureStageStateSmart(0, 0xd, 2);
  SetTextureStageStateSmart(0, 0xe, 2);
  SetTextureStageStateSmart(0, 0xf, 2);
  rasterizer_set_texture(1, 1, 0, *(int *)(p + 0x100),
                         *(unsigned short *)(grp + 0x10));
  SetTextureStageStateSmart(1, 0xa, 1);
  SetTextureStageStateSmart(1, 0xb, 1);
  SetTextureStageStateSmart(1, 0xc, 1);
  SetTextureStageStateSmart(1, 0xd, 2);
  SetTextureStageStateSmart(1, 0xe, 2);
  SetTextureStageStateSmart(1, 0xf, 2);

  D3DDevice_SetRenderState_CullMode(0);
  /* 0x10101 is correct despite the delinked disasm showing $0x101: the
   * imm carries a dir32 reloc to XBE_FILE_HEADER_00010000, so the real
   * value is 0x10000 (XBE base) + 0x101 addend = 0x10101. */
  D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                  NV097_COLOR_MASK_RGB);
  *(uint32_t *)0x1fb7a4 = 0x10101;
  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(uint32_t *)0x1fb784 = 1;
  D3DDevice_SetRenderState_Simple(0x40344, 0x302);
  *(uint32_t *)0x1fb790 = 0x302;
  D3DDevice_SetRenderState_Simple(0x40348, 1);
  *(uint32_t *)0x1fb794 = 1;
  D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
  *(uint32_t *)0x1fb7c0 = 0x8006;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_ZEnable(1);
  D3DDevice_SetRenderState_Simple(0x4035c, 0);
  *(uint32_t *)0x1fb798 = 0;
  D3DDevice_SetRenderState_Simple(0x40354, 0x203);
  *(uint32_t *)0x1fb77c = 0x203;
  D3DDevice_SetRenderState_ZBias(0);

  FUN_00178b40(0xf, FUN_00184610(grp), 0);

  if (!(*(float *)(p + 0x98) != 0.0f)) {
    display_assert("plasma->primary_noise_map_animation_period!=0.0f",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "plasma_energy.c",
                   0x69, 1);
    system_exit(-1);
  }
  if (!(*(float *)(p + 0xe0) != 0.0f)) {
    display_assert("plasma->secondary_noise_map_animation_period!=0.0f",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "plasma_energy.c",
                   0x6a, 1);
    system_exit(-1);
  }

  if (alpha_s < *(float *)0x2a39e0) {
    alpha_s = 0.0f;
  }

  /* Vertex-shader const block A (register -0x51): two scroll matrices.
   * Rows 0-2 scale t/primary_period by shader+0xc4/0xc8/0xcc with the
   * shader+0xd0 duplicate on the diagonal; rows 3-5 use t/secondary_period
   * with shader+0x10c/0x110/0x114 and the shader+0x118 duplicate. alpha_s
   * rides in A[2]. */
  t = *(float *)0x5a5e18;
  adiv = t / *(float *)(p + 0x98);
  bdiv = t / *(float *)(p + 0xe0);
  dupA = *(float *)(p + 0xa8);
  dupB = *(float *)(p + 0xf0);
  vsA[0] = dupA;
  vsA[1] = 0.0f;
  vsA[2] = alpha_s;
  vsA[3] = adiv * *(float *)(p + 0x9c);
  vsA[4] = 0.0f;
  vsA[5] = dupA;
  vsA[6] = 0.0f;
  vsA[7] = adiv * *(float *)(p + 0xa0);
  vsA[8] = 0.0f;
  vsA[9] = 0.0f;
  vsA[10] = dupA;
  vsA[11] = adiv * *(float *)(p + 0xa4);
  vsA[12] = dupB;
  vsA[13] = 0.0f;
  vsA[14] = 0.0f;
  vsA[15] = bdiv * *(float *)(p + 0xe4);
  vsA[16] = 0.0f;
  vsA[17] = dupB;
  vsA[18] = 0.0f;
  vsA[19] = bdiv * *(float *)(p + 0xe8);
  vsA[20] = 0.0f;
  vsA[21] = 0.0f;
  vsA[22] = dupB;
  vsA[23] = bdiv * *(float *)(p + 0xec);

  /* Block B (register -0x54): identity row, (perp-parallel)*colour row, and
   * parallel*colour row.  perp rgba = shader+0x64/0x68/0x6c/0x60,
   * parallel rgba = shader+0x74/0x78/0x7c/0x70; alpha uses alpha_c. */
  vsB[0] = 1.0f;
  vsB[1] = 1.0f;
  vsB[2] = 1.0f;
  vsB[3] = 1.0f;
  vsB[4] = (*(float *)(p + 0x3c) - *(float *)(p + 0x4c)) * tint[0];
  vsB[5] = (*(float *)(p + 0x40) - *(float *)(p + 0x50)) * tint[1];
  vsB[6] = (*(float *)(p + 0x44) - *(float *)(p + 0x54)) * tint[2];
  vsB[7] = (*(float *)(p + 0x38) - *(float *)(p + 0x48)) * alpha_c;
  vsB[8] = *(float *)(p + 0x4c) * tint[0];
  vsB[9] = *(float *)(p + 0x50) * tint[1];
  vsB[10] = *(float *)(p + 0x54) * tint[2];
  vsB[11] = alpha_c * *(float *)(p + 0x48);

  D3DDevice_SetVertexShaderConstant(-0x51, vsA, 6);
  D3DDevice_SetVertexShaderConstant(-0x54, vsB, 3);

  /* Pixel-shader state block for the plasma-energy shader. */
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 0x42;
  *(uint32_t *)0x5a5b94 = 0x104;
  *(uint32_t *)0x5a5ac0 = 0x820a920;
  *(uint32_t *)0x5a5b28 = 0xc00;
  *(uint32_t *)0x5a5b48 = 0x1920b820;
  *(uint32_t *)0x5a5b74 = 0xc00;
  *(uint32_t *)0x5a5ac4 = 0x1c1c0c0c;
  *(uint32_t *)0x5a5b2c = 0x24c00;
  *(uint32_t *)0x5a5b4c = 0;
  *(uint32_t *)0x5a5b78 = 0;
  *(uint32_t *)0x5a5ac8 = 0x5c5c;
  *(uint32_t *)0x5a5b30 = 0x4d00;
  *(uint32_t *)0x5a5b50 = 0;
  *(uint32_t *)0x5a5b7c = 0;
  *(uint32_t *)0x5a5acc = 0x14150000;
  *(uint32_t *)0x5a5b34 = 0x40;
  *(uint32_t *)0x5a5b54 = 0x1c051da0;
  *(uint32_t *)0x5a5b80 = 0xc00;
  *(uint32_t *)0x5a5ae0 = 0xc0f0000;
  *(uint32_t *)0x5a5ae4 = 0x1c1c1400;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  FUN_00174510(grp, 0);
}

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

/* 0x16f610
 *
 * FUN_0016f610 -- performance-timer start/stop marker (16 slots).
 *
 * Confirmed from disassembly:
 *   - One 32-bit stack argument at [EBP+8] (kb.json previously declared
 *     `void (void)`, which was wrong). It packs two fields:
 *       bit0      = phase selector (0 = start, 1 = stop)
 *       bits 1..  = slot index, taken as (int16)(arg >> 1)
 *     The shift is logical (SHR) on the full dword, but the range check is
 *     done on the 16-bit result only (TEST SI,SI / JL, CMP SI,0x10 / JGE),
 *     so the signed bound test is on the truncated int16 -- hence `short`
 *     here, not int. Slot use is MOVSX SI + SHL 3, i.e. stride 8.
 *   - The only callee is QueryPerformanceCounter (import thunk 0x1d33e6),
 *     called as `PUSH LEA [EBP-8]; CALL` with no stack cleanup (stdcall).
 *     The frame is `SUB ESP,8` -- exactly one 64-bit counter, no _chkstk.
 *   - START path stores the counter into start[] (0x47e108) and nothing else.
 *   - STOP path stores the counter into stop[] (0x47e088), records the slot
 *     into the 16-bit last-index global (MOV word ptr [0x47e454],SI), and
 *     stores the 64-bit difference into delta[] (0x47e008). The SUB/SBB pair
 *     proves the direction is stop - start (EDI/EBX hold start.low/high).
 *   - No FPU anywhere.
 *
 * Inferred: the three 0x47e008/0x47e088/0x47e108 regions are contiguous
 * 128-byte arrays of 16 x int64 (delta/stop/start), from the 0x80-byte gaps
 * and the common index stride.
 *
 * The arrays are addressed through constant-address macros rather than local
 * pointer variables so that the base folds into the displacement, matching
 * the original's `0x47e108(,%eax,8)` scaled-index form.
 */
#define PERF_TIMER_DELTA ((int64_t *)0x47e008)
#define PERF_TIMER_STOP ((int64_t *)0x47e088)
#define PERF_TIMER_START ((int64_t *)0x47e108)

void FUN_0016f610(uint32_t marker)
{
  int64_t counter;
  int64_t started;
  short slot;

  slot = (short)(marker >> 1);
  if (slot >= 0 && slot < 0x10) {
    QueryPerformanceCounter(&counter);
    if ((marker & 1) != 0) {
      started = PERF_TIMER_START[slot];
      PERF_TIMER_STOP[slot] = counter;
      *(short *)0x47e454 = slot;
      PERF_TIMER_DELTA[slot] = counter - started;
      return;
    }
    PERF_TIMER_START[slot] = counter;
  }
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
    error(2, "### PROFILE: %s -- tell Bernie!", 0xffffffff, "end out-of-synch");
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

/* 0x16f880
 *
 * FUN_0016f880
 *
 * Re-seeds the xbox rasterizer profile state block from the current render
 * window. Copies the current window index (short @0x5a5bc2 -- the same
 * global the text/decal window filters read) into the profile gate
 * (short @0x325184), and marks "no profile currently open" by storing -1
 * into the open-profile index (short @0x325180).
 *
 * Both stores and the load are 16-bit in the original (MOV AX,[5a5bc2] /
 * MOV [325184],AX / MOV word ptr [325180],0FFFFh), so the globals must be
 * accessed as `short`, not int. The two profile words are 4 bytes apart and
 * are separate fields, not one dword.
 *
 * Consumers: src/halo/rasterizer/xbox/rasterizer_xbox_profile.c gates
 * profiling on `*(short *)0x325184 == 0` and treats 0x325180 == -1 as
 * "no open profile".
 *
 * void FUN_0016f880(void); cdecl, no args, no return value, no callees.
 */
void FUN_0016f880(void)
{
  *(short *)0x325184 = *(short *)0x5a5bc2;
  *(short *)0x325180 = -1;
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

/* Profile-name table: 0x1d (NUMBER_OF_RASTERIZER_PROFILES) pointers into
 * .rdata, verified in the XBE (0x2a3a8c..0x2a3c50 plus 0x25be84 and
 * 0x26de40).  Indexed with a 4-byte stride by the MOV EAX,[ECX*4+0x325188]
 * at 0x16fb96. */
#define NUMBER_OF_RASTERIZER_PROFILES 0x1d
#define rasterizer_profile_names ((const char *const *)0x325188)

/* 0x16fb80
 *
 * Return the display name of a rasterizer profile.
 *
 * Confirmed: one 16-bit stack parameter (MOV SI,word ptr [EBP+8]), a signed
 * range check 0 <= profile < 0x1d, then MOVSX ECX,SI / MOV EAX,[ECX*4 +
 * 0x325188] -- the loaded dword is the return value in EAX.  All four
 * original call sites (0x17f8b3, 0x17f8d2, 0x17fdf1, 0x17fe09, all inside
 * FUN_0017ef00) push exactly one dword and clean up with ADD ESP,4, and each
 * pushes the returned EAX straight into a "|t%s|t%.2f|t%d" style formatting
 * call -- so the element type is `const char *`, the profile's name string.
 * (The `push eax` / `sub esp,8` / `fstp qword [esp]` sequence preceding the
 * call at 0x17f8b3 belongs to that outer formatting call, not to this one --
 * lift-learnings section 7 cdecl mis-grouping; the ADD ESP,4 is the tell.)
 *
 * The assert tail is display_assert(..., halt=1) then system_exit(-1)
 * (CALL 0x8e2f0), not halt_and_catch_fire; __LINE__ is 0x16b = 363.
 *
 * Uncertain: the kb.json name "rasterizer_initialize" does not describe this
 * body -- nothing here initializes anything; it is a profile-name accessor
 * (rasterizer_profile_get_name shape), and its __FILE__ places it in
 * rasterizer_xbox_profile.c alongside FUN_0016f910 / FUN_0016fa40.  The name
 * and the rasterizer.obj mapping are left as-is rather than churned through
 * the ABI baseline; treat both as unverified.
 */
const char *rasterizer_initialize(short profile)
{
  if (profile < 0 || profile >= NUMBER_OF_RASTERIZER_PROFILES) {
    display_assert(
      "profile>=0 && profile<NUMBER_OF_RASTERIZER_PROFILES",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_profile.c", 0x16b,
      1);
    system_exit(-1);
  }
  return rasterizer_profile_names[profile];
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
                   *(const float *)0x254cb8 / (float)*(const int64_t *)0x325178,
                 *(const int64_t *)0x47e448);
  }
}

/* FUN_0016FEB0 (0x16feb0) -- empty no-op.
 *
 * The entire function is a single instruction:
 *
 *   0016feb0:  c3            RET
 *
 * No prologue, no frame, no FPU, no memory access, no callees. Under the
 * cdecl `void (void)` signature in kb.json this is a release-build no-op:
 * a debug/profiling hook whose body compiled out. Called unconditionally
 * from FUN_00158f90 (rasterizer_xbox_decals.c).
 *
 * kb.json assigns 0x16feb0 to rasterizer.obj; its address neighbours here
 * are rasterizer_initialize (0x16fb80) and 0x16fec0.
 */
void FUN_0016FEB0(void)
{
}

/* 0x1703f0 and 0x172650 — two byte-identical dead instantiations of the
 * D3D8 __forceinline wrapper around D3DDevice_SetVertexData2f.
 *
 * Same family as the ported `n` wrapper at 0x15a4f0 (the 4f variant): a
 * __stdcall shim that ignores its device pointer, forwards to the real
 * D3D entry point, and returns S_OK. Neither has any xref -- Ghidra finds
 * no references and dump_caller_regsetup finds no CALL sites -- so these are
 * dead instantiations the compiler emitted and never linked away.
 *
 * ABI (kb.json previously said `void FUN_001703f0(void)`, wrong on every
 * count), all read off 001703f0 / 00172650 which are instruction-identical:
 *   - RET 0xc => __stdcall with THREE stack args at +8/+0xc/+0x10. The
 *     first (+8, the device pointer) is never read, exactly as in the 4f
 *     variant where the device arg is likewise ignored.
 *   - The D3D register index arrives in EDX, not on the stack: PUSH EDX
 *     @001703fb / @0017265b with no prior write to EDX in the function.
 *     This is where the 4f variant differs -- there `reg` is the first
 *     stack arg at [EBP+0xc] -- so the register index cannot simply be
 *     copied from that sibling's signature.
 *   - Returns S_OK in EAX: XOR EAX,EAX @00170401 / @00172661.
 *
 * The C impls below are cdecl, not __stdcall, even though kb.json records
 * the original as __stdcall: knowledge.py strips the convention from any
 * @<reg> declaration when generating decl.h, because the generated thunk
 * presents a cdecl interface to C. patch.py's reverse thunk is what honours
 * the original's callee-cleans contract, returning RET 0xc to the original
 * caller (fixed in this commit -- it previously emitted a plain RET).
 *
 * Argument order into the callee is from the push sequence: PUSH EAX
 * ([EBP+0x10]) then PUSH ECX ([EBP+0xc]) then PUSH EDX, and the last push is
 * the first argument -- so SetVertexData2f(reg, a, b) with a=[EBP+0xc].
 */
int FUN_001703f0(void *device, uint32_t reg, float a, float b)
{
  (void)device;
  D3DDevice_SetVertexData2f(reg, a, b);
  return 0;
}

/* 0x172640
 *
 * FUN_00172640
 *
 * Clear the stashed shadow-parameters pointer at 0x47e4b0, discarding
 * whatever shadow-parameter block the last caller registered.
 *
 * The whole body is a single store: MOV dword ptr [0x0047e4b0],0x0 @00172640
 * followed immediately by RET -- no frame, no arguments, no return value.
 * 0x47e4b0 is the same dword FUN_00172a30 zeroes at the end of its
 * shadow-generate setup above, and that FUN_00172de0 reads before using the
 * block; writing 0 here is the "no parameters registered" state.
 */
void FUN_00172640(void)
{
  *(int *)0x47e4b0 = 0;
}

int FUN_00172650(void *device, uint32_t reg, float a, float b)
{
  (void)device;
  D3DDevice_SetVertexData2f(reg, a, b);
  return 0;
}

/* 0x1726a0
 *
 * FUN_001726a0
 *
 * Handles the "empty shadow" case: called when a shadow was cast but no
 * geometry was submitted for it.
 *
 * Asserts the D3D device exists. When rendering is enabled
 * (*(short *)0x5a5bc0 == 0) and the shadow feature flag is set
 * (*(char *)0x3256ca != 0):
 *   1. If no shadow parameters are active (*(char *)0x47e4b5 == 0), emits the
 *      "empty shadow has been cast" warning.
 *   2. Once per run (latched via *(char *)0x3251fc), performs the fallback
 *      target setup through FUN_00158140 and sets the latch.
 *
 * Note: the guard tests the 16-bit word at 0x5a5bc0 for zero, yet that same
 * zero-extended word is what gets passed as FUN_00158140's first argument.
 * This is what the original code does; preserved verbatim.
 */
void FUN_001726a0(void)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x233,
      1);
    system_exit(-1);
  }
  if (*(short *)0x5a5bc0 == 0 && *(char *)0x3256ca != 0) {
    if (*(char *)0x47e4b5 == 0) {
      error(2, "### WARNING empty shadow has been cast");
    }
    if (*(char *)0x3251fc == 0) {
      /* Zero-extended 16-bit read (XOR EAX,EAX; MOV AX,[0x5a5bc0]). */
      FUN_00158140((int)*(unsigned short *)0x5a5bc0, 0, 0, 0, 1);
      *(char *)0x3251fc = 1;
    }
  }
}

/* 0x172720
 *
 * rasterizer_window_get_fog
 *
 * Closes rasterizer profile section 4 and nothing else.
 *
 * Confirmed: the whole body is PUSH 4 / CALL 0x16fa40 / POP ECX / RET —
 * a cdecl one-argument call with the MSVC POP ECX cleanup.  0x16fa40 is
 * the profile-section end (paired with 0x16f910 begin), so this is the
 * close half of the 0x172520 / 0x172720 profile-4 pair.
 *
 * Uncertain: the kb.json name "rasterizer_window_get_fog" comes from PDB
 * line-containment, not from a string or assert in this function, and it
 * does not describe this body — nothing here reads or returns fog state.
 * The name is left as-is rather than churned, but treat it as unverified.
 */
void rasterizer_window_get_fog(void)
{
  FUN_0016fa40(4);
}

/* 0x172a30
 *
 * FUN_00172a30
 *
 * Shadow-pass begin / shadow-generate setup. Programs the D3D render
 * states, pixel shader, and vertex-shader constants for the shadow
 * generation pass, then stashes the shadow projection matrix, RGB color,
 * and object bounding radius into the module-global shadow parameter block
 * (0x47e46c..).
 *
 * Asserts the D3D device exists. When rendering is enabled
 * (*(short *)0x5a5bc0 == 0) and the shadow feature flag is set
 * (*(char *)0x3256ca != 0):
 *   1. Validates the matrix/color pointers, each RGB component (in [0,1]),
 *      and the object bounding radius (> 0).
 *   2. Sets cull mode, four "simple" render states (each mirrored into a
 *      module global at 0x1fb7a4/784/788/78c), disables Z test and Z bias.
 *   3. Clears and programs the 0xf0-byte pixel-shader state block at
 *      0x5a5ac0, then binds it.
 *   4. Builds five vertex-shader constant registers - a shadow-projection
 *      transform scaled by 1/radius - and uploads them at register -0x44.
 *   5. Stashes the 13-dword matrix, RGB color, and radius into the shadow
 *      parameter block, and clears the associated state bytes.
 *   6. Optionally writes the radius back through out_radius.
 *   7. If the render-mode word (*(short *)0x3256ba) == 2, bumps the
 *      per-frame counter at 0x5a5430.
 *
 * param_1:                unused (present for the cdecl caller ABI).
 * shadow_matrix:          shadow projection matrix (13 dwords / 4x3-ish).
 * shadow_color:           RGB shadow color (3 floats, each in [0,1]).
 * object_bounding_radius: bounding radius (> 0); its reciprocal scales the
 *                         projection transform.
 * out_radius:             optional; receives object_bounding_radius.
 *
 * Returns 1 (AL).
 */
char FUN_00172a30(int param_1, const float *shadow_matrix,
                  const float *shadow_color, float object_bounding_radius,
                  float *out_radius)
{
  float vs_const[20];
  float inv_r;
  const unsigned long *src;
  unsigned long *dst;
  int i;

  (void)param_1;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x93, 1);
    system_exit(-1);
  }
  if (*(short *)0x5a5bc0 == 0 && *(char *)0x3256ca != 0) {
    if (shadow_matrix == 0) {
      display_assert(
        "shadow_matrix",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x99,
        1);
      system_exit(-1);
    }
    if (shadow_color == 0) {
      display_assert(
        "shadow_color",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9a,
        1);
      system_exit(-1);
    }
    if (!(shadow_color[0] >= 0.0f) || !(shadow_color[0] <= 1.0f)) {
      display_assert(
        "shadow_color->red >=0.0f && shadow_color->red <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9b,
        1);
      system_exit(-1);
    }
    if (!(shadow_color[1] >= 0.0f) || !(shadow_color[1] <= 1.0f)) {
      display_assert(
        "shadow_color->green>=0.0f && shadow_color->green<=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9c,
        1);
      system_exit(-1);
    }
    if (!(shadow_color[2] >= 0.0f) || !(shadow_color[2] <= 1.0f)) {
      display_assert(
        "shadow_color->blue >=0.0f && shadow_color->blue <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9d,
        1);
      system_exit(-1);
    }
    if (!(object_bounding_radius > 0.0f)) {
      display_assert(
        "object_bounding_radius>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9e,
        1);
      system_exit(-1);
    }

    /* Render state: cull, four "simple" states (mirrored to module globals),
     * Z test/bias off. Each mirror store is paired with its state by value;
     * MSVC schedules the store into the following call's setup window. */
    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGB);
    *(unsigned long *)0x1fb7a4 = 0x10101;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(unsigned long *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 1);
    *(unsigned long *)0x1fb788 = 1;
    D3DDevice_SetRenderState_Simple(0x40340, 0x7f);
    *(unsigned long *)0x1fb78c = 0x7f;
    D3DDevice_SetRenderState_ZEnable(0);
    D3DDevice_SetRenderState_ZBias(0);

    /* Program and bind the shadow-generation pixel-shader state block. */
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(int *)0x5a5b98 = 1;
    *(int *)0x5a5b94 = 1;
    *(int *)0x5a5ae0 = 0x20;
    *(int *)0x5a5ae4 = 0x1800;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);

    /* Vertex-shader constants: rows 0/1 are the shadow projection scaled by
     * 1/radius; the trailing constants are fixed. All 20 floats form one
     * contiguous buffer that SetVertexShaderConstant uploads (5 registers). */
    inv_r = 1.0f / object_bounding_radius;
    vs_const[8] = 0.0f;
    vs_const[9] = 0.0f;
    vs_const[10] = 0.0f;
    vs_const[11] = 0.5f;
    vs_const[12] = 0.0f;
    vs_const[0] = inv_r * shadow_matrix[1];
    vs_const[1] = inv_r * shadow_matrix[2];
    vs_const[2] = inv_r * shadow_matrix[3];
    vs_const[3] = -((shadow_matrix[10] * shadow_matrix[1] +
                     shadow_matrix[11] * shadow_matrix[2] +
                     shadow_matrix[12] * shadow_matrix[3]) *
                    inv_r);
    vs_const[4] = inv_r * shadow_matrix[4];
    vs_const[5] = inv_r * shadow_matrix[5];
    vs_const[6] = inv_r * shadow_matrix[6];
    vs_const[7] = -((shadow_matrix[10] * shadow_matrix[4] +
                     shadow_matrix[11] * shadow_matrix[5] +
                     shadow_matrix[12] * shadow_matrix[6]) *
                    inv_r);
    vs_const[13] = 0.0f;
    vs_const[14] = 0.0f;
    vs_const[15] = 1.0f;
    vs_const[16] = 0.0f;
    vs_const[17] = 0.0f;
    vs_const[18] = 0.0f;
    vs_const[19] = 0.0f;
    D3DDevice_SetVertexShaderConstant(-0x44, vs_const, 5);

    FUN_00158140(2, 0, (*(unsigned char *)0x3256f7 != 0) ? 0x88888888u : 0u, 1,
                 0);
    FUN_00158ae0(0);

    /* Stash the 13-dword matrix, then the RGB color, then the radius. */
    src = (const unsigned long *)shadow_matrix;
    dst = (unsigned long *)0x47e47c;
    for (i = 0xd; i != 0; i--) {
      *dst = *src;
      src++;
      dst++;
    }
    *(float *)0x47e46c = shadow_color[0];
    *(float *)0x47e470 = shadow_color[1];
    *(float *)0x47e474 = shadow_color[2];
    *(float *)0x47e478 = object_bounding_radius;
    if (out_radius != 0) {
      *out_radius = object_bounding_radius;
    }
    *(int *)0x47e4b0 = 0;
    *(char *)0x47e4b4 = 0;
    *(char *)0x47e4b5 = 0;
    *(char *)0x3251fc = 0;
    if (*(short *)0x3256ba == 2) {
      *(int *)0x5a5430 = *(int *)0x5a5430 + 1;
    }
  }
  return 1;
}

/* 0x1741d0
 *
 * FUN_001741d0 — submit one text quad (4 vertices) to D3D.
 *
 * TU: c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_text.c (proven by the
 * __FILE__ string in the device assert at line 0xd8).
 *
 * Called by rasterizer_text_draw_cached_char / _draw_cached_chars
 * (src/halo/rasterizer/rasterizer_text.c) with an 80-byte quad_verts buffer.
 * Vertex format is 5 floats (20 bytes) per vertex, 4 vertices:
 *   +0x00 position.x   +0x04 position.y
 *   +0x08 texcoord.u   +0x0c texcoord.v
 *   +0x10 color        (raw 0xAARRGGBB dword stored into the float slot)
 * The cursor register (ESI) starts at quad+0xc and advances by 0x14, so each
 * vertex is addressed as v[-3..+1] — that biased-cursor form is preserved.
 *
 * D3D primitive: Begin(7 = D3DPT_QUADLIST). Vertex register 9 = D3DVSDE_DIFFUSE
 * (color), 4 = D3DVSDE_TEXCOORD0, 0 = D3DVSDE_VERTEX.
 *
 * `success` mirrors the original D3D result-check macro: BL is initialized to 1
 * (MOV BL,0x1) and only ever re-set to 1, so all four FUN_00167ff0
 * (report_d3d_call_failed) branches are unreachable at runtime — they are kept
 * to preserve the basic-block layout. Each per-call check compiles to
 * TEST BL,BL / JZ / MOV BL,1 / JMP — reproduced by the
 * `if (success) success = 1; else { success = 0; report; }` shape (same idiom
 * as FUN_0015acc0 in rasterizer_xbox_decals.c); the post-End check is the plain
 * `if (!success)` form. `volatile` is required: a plain local folds to constant
 * 1 and VC71 dead-codes every report branch.
 *
 * The device assert tail is display_assert(..., halt=1) followed by
 * system_exit(-1) — NOT halt_and_catch_fire (PUSH -1; CALL 0x8e2f0).
 */
void FUN_001741d0(float *quad)
{
  float *v;
  int remaining;
  char success;

  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_text.c",
                   0xd8, 1);
    system_exit(-1);
  }
  /* MOV AL,[0x3256da]; TEST AL,AL — then CMP word ptr [0x5a5bc0],0. */
  if (*(char *)0x3256da != 0 && *(short *)0x5a5bc0 == 0) {
    D3DDevice_Begin(7);
    success = 1;
    v = quad + 3;
    remaining = 4;
    do {
      /* MOV EAX,[ESI+4]: the color slot is moved as a raw dword, not a float.
       */
      D3DDevice_SetVertexDataColor(9, *(unsigned int *)&v[1]);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexDataColor(global_d3d_device, "
                     "9, vertices[vertex_index].color)");
      }
      D3DDevice_SetVertexData2f(4, v[-1], v[0]);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexData2f(global_d3d_device, 4, "
                     "vertices[vertex_index].texcoord.u, "
                     "vertices[vertex_index].texcoord.v)");
      }
      D3DDevice_SetVertexData2f(0, v[-3], v[-2]);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0, "IDirect3DDevice8_SetVertexData2f(global_d3d_device, "
                        "VSDE_VERTEX, vertices[vertex_index].position.x, "
                        "vertices[vertex_index].position.y)");
      }
      v = v + 5;
      remaining = remaining - 1;
    } while (remaining != 0);
    D3DDevice_End();
    if (!success) {
      FUN_00167ff0(0, "IDirect3DDevice8_End(global_d3d_device)");
      error(2, "### ERROR rasterizer_text_draw_character failed");
    }
  }
}

/* 0x174510
 *
 * FUN_00174510
 *
 * TU: c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_transparent_geometry.c
 * (proven by the __FILE__ string 0x2a4800 used by all four asserts below).
 *
 * Pure 6-way dispatcher for a transparent-geometry group's index/vertex
 * submission path.  Three group fields select the target:
 *   group+0x48  non-zero -> "lightmap-capable" pair (0x15e0f0 / 0x15e430)
 *   group+0x58  non-zero -> the second entry of whichever pair is selected
 *   has_lightmap (byte)   -> asserted false on every path except the
 *                            0x58-non-zero non-lightmap pair
 * Branch order (0x48, then 0x58, then the has_lightmap byte) is load-bearing:
 * the assert line numbers 0x6d / 0x71 / 0x9c / 0xb4 depend on it.
 *
 * Ghidra dropped every argument on the six dispatch calls; all arg lists were
 * reconstructed from the PUSH sequences + ADD ESP cleanup at the call sites.
 *
 * Confirmed: group+0x44 is read both as a signed dword (the JL sign test at
 * 0x174603) and as a signed word (MOV DI, word ptr [ESI+0x44]; NEG DI); the
 * widths below mirror the disassembly exactly.
 */
void FUN_00174510(void *group, int has_lightmap)
{
  char *g;
  char *vertex_buffer;
  short vertices_per_primitive;
  short primitive_count;

  g = (char *)group;
  if (g == 0) {
    display_assert("group",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "transparent_geometry.c",
                   0x6d, 1);
    system_exit(-1);
  }

  if (*(int *)(g + 0x48) != 0) {
    if ((char)has_lightmap != 0) {
      display_assert("!has_lightmap",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "transparent_geometry.c",
                     0x71, 1);
      system_exit(-1);
    }
    /* if/else (not early-return): the original falls through into the 0x15e430
     * block and places the 0x15e0f0 block out of line at LAB_00174582, so the
     * test emits `je` rather than `jne`. */
    if (*(int *)(g + 0x58) != 0) {
      FUN_0015e430(*(void **)(g + 0x48), *(int *)(g + 0x4c), *(int *)(g + 0x50),
                   *(void **)(g + 0x58));
    } else {
      FUN_0015e0f0(*(void **)(g + 0x48), *(int *)(g + 0x4c), *(int *)(g + 0x50),
                   *(int *)(g + 0x54));
    }
    return;
  }

  /* single load of 0x58: the original keeps it in EAX across the test and
   * derives the fifth 0x15de60 argument with LEA ECX,[EAX+0x14]. */
  vertex_buffer = *(char **)(g + 0x58);
  if (vertex_buffer != 0) {
    if ((char)has_lightmap != 0) {
      FUN_0015de60(*(int *)(g + 0x44), *(int *)(g + 0x4c), *(int *)(g + 0x50),
                   vertex_buffer, vertex_buffer + 0x14);
    } else {
      FUN_0015dc10(*(int *)(g + 0x44), *(int *)(g + 0x4c), *(int *)(g + 0x50),
                   vertex_buffer);
    }
    return;
  }

  if ((char)has_lightmap != 0) {
    display_assert("!has_lightmap",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "transparent_geometry.c",
                   0x9c, 1);
    system_exit(-1);
  }

  if (*(int *)(g + 0x44) >= 0) {
    FUN_0015d8b0(*(int *)(g + 0x44), *(int *)(g + 0x4c), *(int *)(g + 0x50),
                 *(int *)(g + 0x54));
    return;
  }

  /* negative 0x44 encodes an indexed strip/fan: the magnitude is the vertex
   * count per primitive. */
  vertices_per_primitive = (short)-*(short *)(g + 0x44);
  if (vertices_per_primitive == 3 || vertices_per_primitive == 4) {
    primitive_count =
      (short)(*(int *)(g + 0x50) / ((int)vertices_per_primitive - 2));
  } else {
    primitive_count = 1;
    if (*(int *)(g + 0x50) != (int)vertices_per_primitive - 2) {
      display_assert("group->triangle_count==vertices_per_primitive-2",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "transparent_geometry.c",
                     0xb4, 1);
      system_exit(-1);
    }
  }
  rasterizer_draw_dynamic_vertices(0, primitive_count, *(int *)(g + 0x54),
                                   vertices_per_primitive);
}

/* 0x1749b0 — end the frame's overdraw visibility test, fold the returned
 * pixel count into the global accumulator at 0x47e4c4 and, on the last
 * window (when the interface-globals font tag is valid), draw the overdraw
 * ratio "visible_pixels / viewport_area" as text near the lower-right
 * corner.
 *
 * Both D3D entry points are __stdcall and return an HRESULT in EAX; the
 * kb.json `void(void)` decls dropped every argument and the return value
 * (RET 4 / RET 12 in the XBE prove the arg counts).  The result fetch spins
 * while the GPU still reports the test as pending (0x88760828).
 *
 * Globals (from the disassembly at 0x1749b0-0x174b58):
 *   0x325740  two enable bytes, both must be non-zero (MOV EAX; TEST AL,AL;
 *             TEST AH,AH)
 *   0x5a5bc2  short  current window index
 *   0x47e4c4  int    accumulated visible-pixel count (reset on window 0)
 *   0x46bd0c  ptr    interface globals; +0x54 = font tag index
 *   0x5a5bf4 / 0x5a5bf8  int viewport origin / extent (lo 16 = x, hi 16 = y)
 *   0x5a5bfc / 0x5a5c00  int screen rect, copied as two dwords into the
 *             local text rect
 *   0x25fb8c  float  4294967296.0f — the signed-FILD-of-unsigned fixup
 *   0x2ee6e0  ptr    to the draw-string color (the POINTER stored there is
 *             the argument, not its address)
 *
 * The failure string names rasterizer_transparent_geometry_groups_begin even
 * though this ends a visibility test; the literal is copied verbatim. */
void FUN_001749b0(void)
{
  unsigned int enable_flags;
  short rect[4]; /* [EBP-0xc] x0, y0, x1, y1 */
  int area; /* [EBP-0x10] viewport area — the FIDIV divisor */
  unsigned int pixels; /* [EBP-0x14] visibility-test result, seeded to -1 */
  unsigned int timestamp; /* [EBP-0x1c] second out param */
  char text[0x100]; /* [EBP-0x11c] */
  char ok; /* EBX — running success flag */
  int hr;
  int font_tag; /* ESI */
  float value;

  enable_flags = *(unsigned int *)0x325740;
  if ((char)enable_flags == 0 || (char)(enable_flags >> 8) == 0) {
    return;
  }

  pixels = 0xffffffff;
  if (*(short *)0x5a5bc2 == 0) {
    *(int *)0x47e4c4 = 0;
  }

  hr = D3DDevice_EndVisibilityTest(0xfff);
  if (hr < 0) {
    ok = 0;
    FUN_00167ff0(
      hr, "IDirect3DDevice8_EndVisibilityTest(global_d3d_device, index)");
  } else {
    ok = 1;
  }

  /* the two LEAs and the index push are re-done every iteration in the
   * original (the loop head is the first LEA, not the CALL). */
  do {
    hr = D3DDevice_GetVisibilityTestResult(0xfff, &pixels, &timestamp);
  } while (hr == (int)0x88760828);
  if (ok != 0 && hr >= 0) {
    ok = 1;
  } else {
    ok = 0;
    FUN_00167ff0(hr, "hr");
  }

  *(int *)0x47e4c4 = *(int *)0x47e4c4 + (int)pixels;

  if ((int)*(short *)0x5a5bc2 == (int)main_get_window_count() - 1 &&
      (font_tag = *(int *)(*(int *)0x46bd0c + 0x54)) != -1) {
    /* FILD of a count that is logically unsigned: the FADD of 2^32 is the
     * standard fixup for a negative signed load.  Keep the shape. */
    value = (float)*(int *)0x47e4c4;
    *(int *)&rect[0] = *(int *)0x5a5bfc;
    *(int *)&rect[2] = *(int *)0x5a5c00;
    if (*(int *)0x47e4c4 < 0) {
      value = value + *(const float *)0x25fb8c;
    }
    /* width via a dword subtract (low 16 taken afterwards), height via a
     * word subtract of the high halves; FIDIV divides value BY area. */
    area = (int)(short)(*(int *)0x5a5bf8 - *(int *)0x5a5bf4) *
           (int)(short)(*(short *)0x5a5bfa - *(short *)0x5a5bf6);
    crt_sprintf(text, "%.02f", (double)(value / (float)area));

    /* both reads use the pre-update y1/x1 (the original loads them before
     * the in-place ADD word ptr [EBP-0x6],-0x32). */
    rect[1] = (short)(rect[3] - 0xa0);
    rect[0] = (short)(rect[2] - 0x32);
    rect[3] = (short)(rect[3] - 0x32);

    draw_string_set_style_justify_flags(-1, 1, 0);
    draw_string_set_color(*(void **)0x2ee6e0);
    draw_string_set_font_tag(font_tag);
    FUN_00158ae0(0);
    rasterizer_text_draw(rect, (short *)0, (const void *)0, 0, text);
  }

  if (ok == 0) {
    error(2, "### ERROR rasterizer_transparent_geometry_groups_begin failed");
  }
}

/* FUN_00174b60: componentwise 4-float subtract, out = a - b (0x174b60).
 * Four FLD/FSUB m32/FSTP triples at offsets 0/4/8/0xc — the FLD is from
 * param1 and the FSUB against param2, so the direction is a - b (not
 * swapped). Four components, not three: this is a real_vector4d / plane /
 * quaternion-shaped operand, so do NOT reduce it to a 3-vector helper.
 * Each lane is read immediately before being stored, so out may alias a
 * or b; keep the statement order as written. No naming evidence in the
 * binary for a semantic name, so the mechanical name is retained. */
void FUN_00174b60(float *a, float *b, float *out)
{
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
  out[3] = a[3] - b[3];
}

/* FUN_00174b90: componentwise 4-float scale-and-add, out = a + t * b
 * (0x174b90). The 4-component twin of the subtract helper at 0x174b60 above,
 * and like it a real_vector4d / plane / quaternion-shaped operand — do NOT
 * reduce it to a 3-vector helper.
 *
 * Four identical FLD/FMUL m32/FADD m32/FSTP quads at offsets 0/4/8/0xc:
 *   FLD  [EBP+0x10]  ; the scalar, re-loaded fresh for every component
 *   FMUL [ECX + i]   ; ECX = [EBP+0xc]  -> the SCALED vector (param 2)
 *   FADD [EDX + i]   ; EDX = [EBP+0x8]  -> the ADDEND vector (param 1)
 *   FSTP [EAX + i]   ; EAX = [EBP+0x14] -> out
 * Argument roles therefore run addend-first: the FIRST stack arg is the term
 * added un-scaled, the SECOND is the one multiplied by the scalar. Swapping
 * them is silent and near-invisible to a byte-match, hence the explicit note.
 * Multiplication is written first in each statement (t * b[i], not b[i] * t)
 * to keep the FLD-scalar-then-FMUL order.
 *
 * The scalar is re-FLD'd per component rather than parked in ST(0), so the
 * expression is spelled out four times with no loop and no common
 * subexpression. Each lane is read immediately before being stored, so out
 * may alias a or b; keep the statement order as written. No FSUB/FDIV, so no
 * operand-direction hazard. Pure leaf: no CALLs, no branches, no globals. No
 * naming evidence in the binary for a semantic name, so the mechanical name
 * is retained. */
void FUN_00174b90(float *a, float *b, float t, float *out)
{
  out[0] = t * b[0] + a[0];
  out[1] = t * b[1] + a[1];
  out[2] = t * b[2] + a[2];
  out[3] = t * b[3] + a[3];
}

/* 0x174bd0 — allocate and prime the transparent-geometry texcoord stream.
 *
 * Creates the 0x4000-byte static vertex buffer at
 * rasterizer_xbox_transparent_geometry_texcoord_stream (0x47e4bc — the same
 * global the already-ported D3DDevice_SetStreamSource(1, *(void**)0x47e4bc, 2)
 * above binds), locks it, and fills it with a repeating 8-byte texcoord
 * pattern.
 *
 * Return is a BOOL in AL (MOV AL,0x1 at 0x174c93 on success, MOV AL,BL at
 * 0x174cac on the failure tail), not the `int` Ghidra reports — the caller
 * (rasterizer_text.c, `success = FUN_00174bd0()`) tests it as a flag.
 *
 * Ghidra reuses a single `bVar3` for both HRESULT checks; the disassembly
 * instead re-materializes BL independently after each call (MOV BL,1 /
 * XOR BL,BL at 0x174bf5/0x174bff for the create, 0x174c2d/0x174c37 for the
 * lock), so `ok` is written in both arms of both branches here.  The lock's
 * error branch is gated on `TEST BL,BL; JZ` — i.e. on the PREVIOUS step's
 * flag, and reports a literal 0 as the HRESULT (D3DVertexBuffer_Lock is void
 * on Xbox), which is why the report argument is not a real hr.
 *
 * Both D3D entry points are __stdcall with kb.json `void(void)` stub decls.
 * CreateVertexBuffer has a real signature in kb.json; Lock does not, so it is
 * reached through the raw __stdcall cast idiom used by FUN_0015c650 — do not
 * "fix" that by calling it by name, which would drop all five arguments.
 *
 * 0x325652 is a WORD (MOV word ptr [0x325652],0x2 before the lock, then
 * MOV word ptr [...],SI with SI==0 after) — a byte store here is a
 * load-width bug.
 *
 * The fill loop runs 0x400 iterations of 8 bytes = 0x2000 bytes into a
 * 0x4000-byte buffer.  That half-fill is what the binary does; it is NOT a
 * transcription error and must not be "corrected".  csmemcpy's returned
 * pointer is discarded (Ghidra smuggles it into the return value via
 * CONCAT31; the real return is the AL flag). */
char FUN_00174bd0(void)
{
  unsigned char pattern[8]; /* [EBP-0xc] */
  void *vertices; /* [EBP-0x4] — Lock's out pointer, walked by the fill loop */
  char ok; /* BL */
  int hr;
  int i; /* ESI */

  vertices = (void *)0;

  hr = D3DDevice_CreateVertexBuffer(0x4000, 8, 0, 1, (void **)0x47e4bc);
  /* success is the FALL-THROUGH arm: the reference branches with JL to an
   * out-of-line report block (LAB_00174bf9), so the `hr >= 0` arm must be
   * written first. */
  if (hr >= 0) {
    ok = 1;
  } else {
    ok = 0;
    FUN_00167ff0(
      hr,
      "IDirect3DDevice8_CreateVertexBuffer(global_d3d_device, "
      "RASTERIZER_TRANSPARENT_GEOMETRY_TEXCOORD_STREAM_SIZE*(2*sizeof(byte)), "
      "RASTERIZER_STATIC_BUFFER_USAGE, 0, RASTERIZER_STATIC_BUFFER_POOL, "
      "&rasterizer_xbox_transparent_geometry_texcoord_stream)");
  }

  *(short *)0x325652 = 2;
  /* hazard-ok: fnptr-conv — __stdcall verified: no ADD ESP follows the CALL at
   * 0x1ef100 and the five pushes (ECX=stream, 0, 0x4000, &vertices, 0) are
   * cleaned by the callee.  Same idiom as FUN_0015c650. */
  ((void(__stdcall *)(void *, uint32_t, uint32_t, void **, uint32_t))0x1ef100)(
    *(void **)0x47e4bc, 0, 0x4000, &vertices, 0);
  if (ok != 0) {
    ok = 1;
  } else {
    FUN_00167ff0(
      0,
      "IDirect3DVertexBuffer8_Lock("
      "rasterizer_xbox_transparent_geometry_texcoord_stream, 0, "
      "RASTERIZER_TRANSPARENT_GEOMETRY_TEXCOORD_STREAM_SIZE*(2*sizeof(byte)), "
      "(unsigned char**)&vertices, 0)");
    ok = 0;
  }
  *(short *)0x325652 = 0;

  if (ok != 0 && vertices != (void *)0) {
    pattern[0] = 0;
    pattern[1] = 0;
    pattern[2] = 0;
    pattern[3] = 0xff;
    pattern[4] = 0xff;
    pattern[5] = 0xff;
    pattern[6] = 0xff;
    pattern[7] = 0;
    i = 0x400;
    do {
      csmemcpy(vertices, pattern, 8);
      vertices = (void *)((char *)vertices + 8);
      i = i - 1;
    } while (i != 0);
    return 1;
  }

  error(2, "### ERROR failed to allocate texcoord stream");
  ok = 0;
  return ok;
}

/* dispose of rasterizer_xbox_transparent_geometry_texcoord_stream: release the
 * stream-1 texcoord vertex buffer allocated by FUN_00174bd0 and clear the
 * global. The D3DResource_Release return value is discarded (the original
 * never tests EAX after the CALL), and the null-out lives inside the guard
 * because the JZ at 0x174cc7 skips both the CALL and the store (0x174cc0). */
void FUN_00174cc0(void)
{
  if (*(void **)0x47e4bc != (void *)0x0) {
    D3DResource_Release(*(void **)0x47e4bc);
    *(void **)0x47e4bc = (void *)0x0;
  }
}

/* begin the per-frame GPU visibility (occlusion) test -- the Begin counterpart
 * of FUN_001749b0, which ends the test and reports the pixel ratio.
 *
 * The two counters are cleared UNCONDITIONALLY, before any branch: the
 * scheduler interleaved the stores with the CMPs (0x174ce9 / 0x174cef sit
 * between CMP AL,CL and its JZ), so they run even when the test is skipped.
 * Their widths differ and must be preserved -- MOV dword [0x47e4b8],ECX vs
 * MOV byte [0x47e4c0],CL.
 *
 * Globals (from the disassembly at 0x174ce0-0x174d0a):
 *   0x325740  two enable bytes, both must be non-zero (CMP AL,CL; CMP AH,CL)
 *             -- read as one dword, same as FUN_001749b0
 *   0x5a5bc2  short  current window index; -1 is the "no window" sentinel and
 *             suppresses the test (CMP word [0x5a5bc2],-1; JZ to the RET)
 *   0x47e4b8  int    per-frame counter, cleared here
 *   0x47e4c0  char   per-frame flag, cleared here (set to 1 at 0x1... see the
 *                    reader at rasterizer.c:2319)
 *
 * D3DDevice_BeginVisibilityTest is reached by a tail JMP to the import thunk
 * at 0x1e8a40 with nothing pushed, so it genuinely takes no argument here.
 * The tail call also means EAX holds the D3D HRESULT on the taken path and the
 * enable pair on the fall-through path -- two different values, so no caller
 * can be consuming an implicit return; the decl stays void (lift-learnings
 * S16). */
void FUN_00174ce0(void)
{
  unsigned int enable_flags;

  enable_flags = *(unsigned int *)0x325740;
  *(int *)0x47e4b8 = 0;
  *(char *)0x47e4c0 = 0;

  /* the high enable byte is tested in place (CMP AH,CL at 0x174cf7); a
   * `>> 8` here costs an extra SHR that the original does not have. */
  if ((char)enable_flags == 0 || (enable_flags & 0xff00) == 0) {
    return;
  }
  if (*(short *)0x5a5bc2 == -1) {
    return;
  }

  D3DDevice_BeginVisibilityTest();
}

/* rasterizer_transparent_geometry_group_draw: draw one sorted transparent
 * geometry group, dispatching per shader type (generic/chicago/glass/meter/
 * plasma/water), handling extra layers via self-recursion, predicted shader
 * pre-pass, debug tint mode, and secondary (dirty) group passes (0x174d10) */
void rasterizer_transparent_geometry_group_draw(void *group, int dirty)
{
  char *grp = (char *)group;
  char success; /* [EBP-0x1] draw success accumulator */
  char draw_secondary; /* [EBP-0x2d] draw dirty secondary groups after */
  char *sh;
  int vertex_type; /* [EBP-0x38] */
  int permutation; /* [EBP-0x40] */
  int pass; /* [EBP-0x80] two-pass layer loop */
  char has_multi; /* [EBP+0xb] case-1 secondary-map flag */
  short sec_count; /* [EBP+0xa] secondary group count */
  char *sec;
  short si;
  char *rec;
  char *next;

  success = 1;
  draw_secondary = 0;
  if (grp == (char *)0) {
    display_assert("group",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "transparent_geometry.c",
                   0xe8, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "transparent_geometry.c",
                   0xe9, 1);
    system_exit(-1);
  }
  if (*(int *)(grp + 0x98) != 0 && (char)dirty == 0) {
    return;
  }
  if (FUN_00184570(grp) == 0) {
    return;
  }
  FUN_001845b0(grp, 0);
  if (*(short *)(grp + 0x94) != -1) {
    rasterizer_transparent_geometry_group_draw(
      rasterizer_transparent_geometry_group_get(*(short *)(grp + 0x94)), dirty);
  }

  /* rasterizer_debug_transparents: draw with random per-group tint */
  if (*(char *)0x3256c2 != 0) {
    if ((*grp & 2) == 0 && *(int *)(grp + 0xc) != 0 &&
        *(int *)(grp + 0x90) != -1) {
      short vertex_shader_table[12];
      char solid_color; /* [EBP+0xb] debug value forces solid color */
      unsigned int seed;
      float argb[4]; /* [EBP-0x2c] alpha,red,green,blue */
      float blue;
      float minimum;
      float maximum;
      float range_scale;
      float tint;
      float dim;
      float skin_xform[12]; /* [EBP-0xb0] */
      struct {
        void *matrices;
        short node_count;
      } skinning; /* [EBP-0x1c] */
      char text_buffer[96]; /* [EBP-0x550] */

      vertex_shader_table[0] = 6;
      vertex_shader_table[1] = 6;
      vertex_shader_table[2] = 6;
      vertex_shader_table[3] = 6;
      vertex_shader_table[4] = 0xd;
      vertex_shader_table[5] = 0xd;
      vertex_shader_table[6] = 0x41;
      vertex_shader_table[7] = 0x41;
      vertex_shader_table[8] = -1;
      vertex_shader_table[9] = -1;
      vertex_shader_table[10] = -1;
      vertex_shader_table[11] = -1;
      vertex_type = (short)FUN_00184610(grp);
      if (*(short *)0x3256ea >= 1000 || *(short *)0x3256ea < 0) {
        solid_color = 1;
      } else {
        solid_color = 0;
      }
      if ((short)vertex_type < 0 || (short)vertex_type >= 0xc) {
        display_assert(
          "vertex_type>=0 && vertex_type<NUMBER_OF_RASTERIZER_VERTEX_TYPES",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
          "geometry.c",
          0x118, 1);
        system_exit(-1);
      }
      if (vertex_shader_table[(short)vertex_type] == -1) {
        display_assert("vertex_shader_table[vertex_type]!=NONE",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x119, 1);
        system_exit(-1);
      }
      FUN_00178b40(
        (int)(0xffff0000u |
              (unsigned short)vertex_shader_table[(short)vertex_type]),
        vertex_type, 0);
      D3DDevice_SetRenderState_CullMode(0);
      D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                      NV097_COLOR_MASK_RGB);
      *(uint32_t *)0x1fb7a4 = 0x10101;
      D3DDevice_SetRenderState_Simple(0x40304, (unsigned char)solid_color);
      *(uint32_t *)0x1fb784 = (unsigned char)solid_color;
      D3DDevice_SetRenderState_Simple(0x40344, 1);
      *(uint32_t *)0x1fb790 = 1;
      D3DDevice_SetRenderState_Simple(0x40348, 1);
      *(uint32_t *)0x1fb794 = 1;
      D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
      *(uint32_t *)0x1fb7c0 = 0x8006;
      D3DDevice_SetRenderState_Simple(0x40300, 0);
      *(uint32_t *)0x1fb788 = 0;
      D3DDevice_SetRenderState_ZEnable(1);
      D3DDevice_SetRenderState_Simple(0x4035c, 0);
      *(uint32_t *)0x1fb798 = 0;
      D3DDevice_SetRenderState_Simple(0x40354, 0x203);
      *(uint32_t *)0x1fb77c = 0x203;
      D3DDevice_SetRenderState_ZBias(0);
      csmemset((void *)0x5a5ac0, 0, 0xf0);
      seed = (unsigned int)((int)*(short *)0x3256ea + *(int *)(grp + 0x90));
      argb[0] = 1.0f;
      argb[1] = random_math_real(&seed);
      argb[2] = random_math_real(&seed);
      blue = random_math_real(&seed);
      /* normalize color so channels span [0.15, 0.33]; MIN/MAX macros
       * re-evaluate their arguments as in the original */
      minimum = (argb[1] <= ((argb[2] <= blue) ? argb[2] : blue)) ?
                  argb[1] :
                  ((argb[2] <= blue) ? argb[2] : blue);
      maximum = (argb[1] <= ((blue < argb[2]) ? argb[2] : blue)) ?
                  ((blue < argb[2]) ? argb[2] : blue) :
                  argb[1];
      range_scale = *(float *)0x2a52b4 / (maximum - minimum);
      argb[1] = (argb[1] - minimum) * range_scale + *(float *)0x256140;
      argb[2] = (argb[2] - minimum) * range_scale + *(float *)0x256140;
      argb[3] = (blue - minimum) * range_scale + *(float *)0x256140;
      if (solid_color != 0) {
        tint = *(float *)0x325724;
        if (tint < *(float *)0x2533c0) {
          tint = *(float *)0x29d598;
        } else if (tint > *(float *)0x2533c8) {
          tint = 1.0f;
        } else if (tint == *(float *)0x2533c0) {
          tint = *(float *)0x29d598;
        }
        if (*(short *)0x3256ea >= 1000) {
          argb[1] = argb[1] * tint;
          argb[2] = argb[2] * tint;
          argb[3] = tint * argb[3];
        } else {
          argb[1] = tint;
          argb[2] = tint;
          argb[3] = tint;
        }
      }
      if (!(argb[1] >= *(float *)0x2533c0 && argb[1] <= *(float *)0x2533c8)) {
        display_assert("color.red >=0.0f && color.red <=1.0f",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x156, 1);
        system_exit(-1);
      }
      if (!(argb[2] >= *(float *)0x2533c0 && argb[2] <= *(float *)0x2533c8)) {
        display_assert("color.green>=0.0f && color.green<=1.0f",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x157, 1);
        system_exit(-1);
      }
      if (!(argb[3] >= *(float *)0x2533c0 && argb[3] <= *(float *)0x2533c8)) {
        display_assert("color.blue >=0.0f && color.blue <=1.0f",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x158, 1);
        system_exit(-1);
      }
      *(uint32_t *)0x5a5b6c = FUN_000d1dd0(&argb[1]);
      *(uint32_t *)0x5a5b94 = 1;
      *(uint32_t *)0x5a5ae0 = 1;
      rasterizer_set_pixel_shader((void *)0x5a5ac0);
      if (*(void **)(grp + 0x60) != (void *)0 && *(short *)(grp + 0x64) != 0) {
        skinning.matrices = *(void **)(grp + 0x60);
        skinning.node_count = *(short *)(grp + 0x64);
      } else {
        skinning.matrices = *(void **)0x31fc60;
        skinning.node_count = 1;
      }
      rasterizer_set_model_skinning(&skinning);
      skin_xform[0] = 1.0f;
      skin_xform[1] = 0.0f;
      skin_xform[2] = 0.0f;
      skin_xform[3] = 0.0f;
      skin_xform[4] = 0.0f;
      skin_xform[5] = 1.0f;
      skin_xform[6] = 0.0f;
      skin_xform[7] = 0.0f;
      skin_xform[8] = 0.0f;
      skin_xform[9] = 0.0f;
      skin_xform[10] = 1.0f;
      skin_xform[11] = 0.0f;
      if ((*grp & 0x20) != 0) {
        skin_xform[0] = *(float *)0x5a5c64;
        skin_xform[1] = *(float *)0x5a5c70;
        skin_xform[2] = *(float *)0x5a5c7c;
        skin_xform[3] = *(float *)0x5a5bc8;
        skin_xform[4] = *(float *)0x5a5c68;
        skin_xform[5] = *(float *)0x5a5c74;
        skin_xform[6] = *(float *)0x5a5c80;
        skin_xform[7] = *(float *)0x5a5bcc;
        skin_xform[8] = *(float *)0x5a5c6c;
        skin_xform[9] = *(float *)0x5a5c78;
        skin_xform[10] = *(float *)0x5a5c84;
        skin_xform[11] = *(float *)0x5a5bd0;
      }
      D3DDevice_SetVertexShaderConstant(0x58, skin_xform, 3);
      success = 1;
      FUN_00174510(grp, 0);
      if (solid_color == 0) {
        crt_sprintf(text_buffer, "%.03f", (double)*(float *)(grp + 0x70));
        argb[1] = argb[1] * *(float *)0x254644;
        if (argb[1] < *(float *)0x2533c0) {
          argb[1] = 0.0f;
        } else if (argb[1] > *(float *)0x2533c8) {
          argb[1] = 1.0f;
        }
        dim = argb[1] * *(float *)0x254644;
        if (dim < *(float *)0x2533c0) {
          argb[2] = 0.0f;
          argb[3] = 0.0f;
        } else if (dim > *(float *)0x2533c8) {
          argb[2] = 1.0f;
          argb[3] = 1.0f;
        } else {
          argb[2] = dim;
          argb[3] = dim;
        }
        FUN_00189cb0(0, grp + 0x74, text_buffer, (int)argb);
      }
    }
    goto tail;
  }

  /* predicted shaders: pre-set state for run of type-2 groups w/ same tag */
  if (*(short *)(grp + 0x14) == 2 && *(int *)(grp + 8) != *(int *)0x47e4b8 &&
      (char)dirty == 0) {
    char *g2;
    int first_tag;
    struct {
      void *matrices;
      short node_count;
    } skinning2;

    first_tag = *(int *)(grp + 8);
    g2 = grp;
    vertex_type = FUN_00184610(grp);
    FUN_00178b40(0xd, vertex_type, 0);
    SetRenderStateSmart(0x7f, 0);
    SetRenderStateSmart(0x43, 0);
    SetRenderStateSmart(0x3b, 0);
    SetRenderStateSmart(0x3c, 0);
    SetRenderStateSmart(0x7b, 1);
    SetRenderStateSmart(0x40, 1);
    SetRenderStateSmart(0x39, 0x203);
    D3DDevice_SetRenderState_ZBias(0);
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b94 = 1;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    do {
      if (*(int *)(g2 + 8) != first_tag || *(short *)(g2 + 0x14) != 2) {
        break;
      }
      if (shader_ignores_effect(*(void **)(g2 + 0xc)) == 0) {
        if (*(void **)(g2 + 0x60) != (void *)0 && *(short *)(g2 + 0x64) != 0) {
          skinning2.node_count = *(short *)(g2 + 0x64);
          skinning2.matrices = *(void **)(g2 + 0x60);
        } else {
          skinning2.node_count = 1;
          skinning2.matrices = *(void **)0x31fc60;
        }
        rasterizer_set_model_skinning(&skinning2);
        if (*(int *)(grp + 0x68) != 0) {
          rasterizer_set_model_lighting(*(void **)(g2 + 0x68));
        }
        FUN_00174510(g2, 0);
      }
      g2 = (char *)rasterizer_transparent_geometry_next_group(g2);
    } while (g2 != (char *)0);
  }

  if ((*grp & 2) == 0) {
    if (*(short *)0x5a5bc0 == 0 && (char)dirty == 0) {
      sh = *(char **)(grp + 0xc);
      if (*(char *)0x3256fa == 0) {
        if (sh == (char *)0 ||
            (*(short *)(sh + 0x24) != 7 && shader_is_water_decal(sh) == 0)) {
          FUN_001595c0();
        }
      } else if (sh != (char *)0 && *(short *)(sh + 0x24) == 4 &&
                 *(short *)(grp + 0x14) == 1 &&
                 *(int *)(grp + 8) != *(int *)0x47e4b8) {
        FUN_001595c0();
      }
    }
    if ((*grp & 2) == 0 && *(short *)0x5a5bc0 == 0 &&
        *(short *)(grp + 0x14) == 1 && *(int *)(grp + 0xc) != 0 &&
        *(short *)(*(char **)(grp + 0xc) + 0x24) == 4 && (char)dirty == 0) {
      next = (char *)rasterizer_transparent_geometry_next_group(grp);
      if (next == (char *)0 || *(short *)(next + 0x14) != 1 ||
          *(int *)(next + 8) != *(int *)(grp + 8) ||
          *(int *)(next + 0xc) == 0 ||
          *(short *)(*(char **)(next + 0xc) + 0x24) != 4) {
        draw_secondary = 1;
      }
    }
  }

  if (*(int *)(grp + 0xc) == 0) {
    /* group with no shader: invoke user callback stored in the record */
    (*(void (**)(int, int))(grp + 0x48))(*(int *)(grp + 0x4c),
                                         *(int *)(grp + 0x50));
    goto tail;
  }

  permutation = shader_get_vertex_shader_permutation(*(void **)(grp + 0xc));
  vertex_type = FUN_00184610(grp);
  if ((*grp & 2) == 0) {
    struct {
      void *matrices;
      short node_count;
    } skinning3;
    if (*(void **)(grp + 0x60) != (void *)0 && *(short *)(grp + 0x64) != 0) {
      skinning3.node_count = *(short *)(grp + 0x64);
      skinning3.matrices = *(void **)(grp + 0x60);
    } else {
      skinning3.node_count = 1;
      skinning3.matrices = *(void **)0x31fc60;
    }
    rasterizer_set_model_skinning(&skinning3);
    if (*(int *)(grp + 0x68) != 0) {
      rasterizer_set_model_lighting(*(void **)(grp + 0x68));
    }
  }
  if ((*grp & 8) != 0) {
    if (*(short *)0x5a5bc0 == 0) {
      rasterizer_set_frustum_z(0.00390625f, 1024.0f);
    }
    SetRenderStateSmart(0x7b, 0);
    SetRenderStateSmart(0x81, 0);
  } else {
    SetRenderStateSmart(0x7b, 1);
    SetRenderStateSmart(0x40, 0);
    SetRenderStateSmart(0x39, 0x203);
    SetRenderStateSmart(0x81,
                        -(int)(shader_is_decal(*(void **)(grp + 0xc)) != 0) &
                          *(int *)0x32570c);
  }

  pass = 0;
  do {
    if ((char)*grp < 0) {
      if (*(short *)(grp + 0x14) == 1) {
        if ((short)pass > 0) {
          break;
        }
        rasterizer_set_frustum_z(*(float *)0x32569c, *(float *)0x3256a0);
      } else if ((short)pass != 0) {
        FUN_00158ae0(2);
        SetRenderStateSmart(0x7b, 0);
      } else {
        sh = *(char **)(grp + 0xc);
        if (sh != (char *)0 && *(short *)(sh + 0x24) == 1) {
          char *senv = (char *)FUN_001906b0(sh, 1);
          if ((*(unsigned char *)(senv + 0x28) & 4) != 0) {
            goto next_pass;
          }
        }
        FUN_00158ae0(3);
      }
    } else if ((short)pass > 0) {
      break;
    }

    sh = *(char **)(grp + 0xc);
    switch (*(short *)(sh + 0x24)) {
    case 1: {
      /* shader_environment-style multitexture path */
      char *env;
      char env_flags_bit1; /* BL: (shader->flags >> 1) & 1 */
      float skin_xform1[12]; /* [EBP-0x108] */
      float texanim1[16]; /* [EBP-0x1f0] rows 2,3 written by texture anim */
      float fog_consts[8]; /* [EBP-0xa0] */
      float opacity;
      float fog_scale;
      int stage_count;
      int idx;

      env = (char *)FUN_001906b0(sh, 1);
      /* has_multi = secondary map used as a regular multitexture stage.
       * A z-sprite secondary map (env+0x5c == 2) is NOT a multitexture
       * stage — it goes through the dedicated z-sprite final-combiner
       * path instead (original 0x175830: jne keeps 1, i.e. != 2). */
      if (*(int *)(env + 0x58) != -1 && *(short *)(env + 0x5c) != 2) {
        has_multi = 1;
      } else {
        has_multi = 0;
      }
      env_flags_bit1 = (char)((*(unsigned char *)(env + 0x28) >> 1) & 1);
      rasterizer_set_texture_bitmap_data(0, *(void **)(grp + 0x5c));
      SetTextureStageStateSmart(0, 0xa,
                                (*(unsigned char *)(env + 0x2e) & 2) | 1);
      SetTextureStageStateSmart(
        0, 0xb, ((*(unsigned char *)(env + 0x2e) & 4) | 2) >> 1);
      SetTextureStageStateSmart(
        0, 0xd, 2 - (int)((*(unsigned char *)(env + 0x2e) & 1) != 0));
      SetTextureStageStateSmart(
        0, 0xe, 2 - (int)((*(unsigned char *)(env + 0x2e) & 1) != 0));
      SetTextureStageStateSmart(
        0, 0xf, 2 - (int)((*(unsigned char *)(env + 0x2e) & 1) != 0));
      if (*(int *)(env + 0x58) != -1) {
        rasterizer_set_texture(1, 0, 1, *(int *)(env + 0x58),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(1, 0xa,
                                  (*(unsigned char *)(env + 0x5e) & 2) | 1);
        SetTextureStageStateSmart(
          1, 0xb, ((*(unsigned char *)(env + 0x5e) & 4) | 2) >> 1);
        SetTextureStageStateSmart(
          1, 0xd, 2 - (int)((*(unsigned char *)(env + 0x5e) & 1) != 0));
        SetTextureStageStateSmart(
          1, 0xe, 2 - (int)((*(unsigned char *)(env + 0x5e) & 1) != 0));
        SetTextureStageStateSmart(
          1, 0xf, 2 - (int)((*(unsigned char *)(env + 0x5e) & 1) != 0));
      }
      SetRenderStateSmart(0x7f, 0);
      SetRenderStateSmart(0x43, 0x10101);
      SetRenderStateSmart(0x3b, 1);
      SetRenderStateSmart(0x3c, 0);
      FUN_001580b0(*(unsigned short *)(env + 0x2a));
      FUN_00178b40(0x41, vertex_type, permutation);
      skin_xform1[0] = 1.0f;
      skin_xform1[1] = 0.0f;
      skin_xform1[2] = 0.0f;
      skin_xform1[3] = 0.0f;
      skin_xform1[4] = 0.0f;
      skin_xform1[5] = 1.0f;
      skin_xform1[6] = 0.0f;
      skin_xform1[7] = 0.0f;
      skin_xform1[8] = 0.0f;
      skin_xform1[9] = 0.0f;
      skin_xform1[10] = 1.0f;
      skin_xform1[11] = 0.0f;
      texanim1[0] = 1.0f;
      texanim1[1] = 0.0f;
      texanim1[2] = 0.0f;
      texanim1[3] = 0.0f;
      texanim1[4] = 0.0f;
      texanim1[5] = 1.0f;
      texanim1[6] = 0.0f;
      texanim1[7] = 0.0f;
      texanim1[8] = 0.0f;
      texanim1[9] = 0.0f;
      texanim1[10] = 0.0f;
      texanim1[11] = 0.0f;
      texanim1[12] = 0.0f;
      texanim1[13] = 0.0f;
      texanim1[14] = 0.0f;
      texanim1[15] = 0.0f;
      if ((*grp & 0x20) != 0) {
        skin_xform1[0] = *(float *)0x5a5c64;
        skin_xform1[1] = *(float *)0x5a5c70;
        skin_xform1[2] = *(float *)0x5a5c7c;
        skin_xform1[3] = *(float *)0x5a5bc8;
        skin_xform1[4] = *(float *)0x5a5c68;
        skin_xform1[5] = *(float *)0x5a5c74;
        skin_xform1[6] = *(float *)0x5a5c80;
        skin_xform1[7] = *(float *)0x5a5bcc;
        skin_xform1[8] = *(float *)0x5a5c6c;
        skin_xform1[9] = *(float *)0x5a5c78;
        skin_xform1[10] = *(float *)0x5a5c84;
        skin_xform1[11] = *(float *)0x5a5bd0;
      }
      if (has_multi != 0) {
        FUN_00190e10(env + 0x60, *(void **)(grp + 0x6c), *(float *)(grp + 0x3c),
                     *(float *)(grp + 0x40), 0.0f, 0.0f, 0.0f,
                     *(float *)0x5a5e18, &texanim1[8], &texanim1[12]);
      }
      D3DDevice_SetVertexShaderConstant(0x58, skin_xform1, 3);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__INVERSE_OFFSET, "
                     "vsh_constants__inverse, VSH_CONSTANTS__INVERSE_COUNT)");
      }
      D3DDevice_SetVertexShaderConstant(-0x51, texanim1, 4);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(
          0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
             "VSH_CONSTANTS__TEXANIM_OFFSET, vsh_constants__texanim, 4)");
      }
      if (*(char *)0x325718 != 0 && *(short *)(env + 0x5c) == 2 &&
          *(int *)(env + 0x58) != -1 && (char)*grp >= 0) {
        /* z-sprite fog constants */
        opacity = 1.0f;
        if (*(float *)(env + 0x9c) != *(float *)0x2533c0) {
          opacity = *(float *)(env + 0x9c);
        }
        fog_scale = *(float *)0x2a50dc;
        if (*(char *)0x32568c != 0) {
          fog_scale = *(float *)0x2a50e0;
        }
        fog_consts[4] = *(float *)0x5a5bd4;
        fog_consts[5] = *(float *)0x5a5bd8;
        fog_consts[6] = *(float *)0x5a5bdc;
        fog_consts[0] = (*(float *)0x5a5c08 * fog_scale) /
                        (*(float *)0x5a5c08 - *(float *)0x5a5c04);
        fog_consts[1] = -(fog_consts[0] * *(float *)0x5a5c04);
        fog_consts[2] = opacity * *(float *)(env + 0x98);
        fog_consts[3] = *(float *)0x5a5c04 + *(float *)0x25bb10;
        fog_consts[7] = -FUN_00013070((float *)0x5a5bd4, (float *)0x5a5bc8);
        D3DDevice_SetVertexShaderConstant(-0x3f, fog_consts, 2);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(0,
                       "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                       "device, VSH_CONSTANTS__ZSPRITE_OFFSET, "
                       "vsh_constants__zsprite, VSH_CONSTANTS__ZSPRITE_COUNT)");
        }
      }
      csmemset((void *)0x5a5ac0, 0, 0xf0);
      *(uint32_t *)0x5a5ac0 = 0x18201415;
      *(uint32_t *)0x5a5b28 = 0xc4;
      *(uint32_t *)0x5a5ae0 = 0xc;
      *(uint32_t *)0x5a5ae4 = 0x1c00;
      if (env_flags_bit1 != 0) {
        *(uint32_t *)0x5a5b48 = 0x8080000;
        *(uint32_t *)0x5a5b74 = 0xc0;
        *(uint32_t *)0x5a5b4c = 0xc0c0000;
        *(uint32_t *)0x5a5b78 = 0xc0;
        *(uint32_t *)0x5a5b50 = 0x250c0508;
        *(uint32_t *)0x5a5b7c = 0xc00;
        stage_count = 3;
      } else {
        *(uint32_t *)0x5a5b48 = 0x8050000;
        *(uint32_t *)0x5a5b74 = 0xc0;
        stage_count = 1;
      }
      if (has_multi != 0) {
        idx = (short)stage_count * 4;
        *(uint32_t *)(0x5a5ac0 + idx) = 0x1c190000;
        *(uint32_t *)(0x5a5b28 + idx) = 0xc0;
        *(uint32_t *)(0x5a5b48 + idx) = 0xc090000;
        *(uint32_t *)(0x5a5b74 + idx) = 0xc0;
        stage_count = stage_count + 1;
      }
      if (*(char *)0x325718 == 0 || *(short *)(env + 0x5c) != 2 ||
          *(int *)(env + 0x58) == -1 || (char)*grp < 0) {
        *(uint32_t *)0x5a5b98 = ((unsigned int)(has_multi != 0) << 5) | 1;
      } else {
        *(uint32_t *)0x5a5b98 = 0x54421;
        *(uint32_t *)0x5a5ba0 = 0x110000;
        *(uint32_t *)0x5a5b9c = 0;
        SetTextureStageStateSmart(1, 0xa, 1);
        SetTextureStageStateSmart(1, 0xb, 1);
        SetTextureStageStateSmart(1, 0xd, 2);
        SetTextureStageStateSmart(1, 0xe, 2);
        SetTextureStageStateSmart(1, 0xf, 2);
        D3DDevice_SetStreamSource(1, *(void **)0x47e4bc, 2);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(0,
                       "IDirect3DDevice8_SetStreamSource(global_d3d_device, 1, "
                       "rasterizer_xbox_transparent_geometry_texcoord_stream, "
                       "2*sizeof(byte))");
        }
        rasterizer_set_texture(1, 0, 0, *(int *)(env + 0x58),
                               *(unsigned short *)(grp + 0x10));
      }
      if (*(char *)0x3256d4 != 0 && (*grp & 4) == 0) {
        switch (*(short *)(env + 0x2a)) {
        case 0:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5ac0 + idx) = 0x1c140000;
          *(uint32_t *)(0x5a5b28 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        case 1:
        case 5:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5b48 + idx) = 0xc142034;
          *(uint32_t *)(0x5a5b74 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        case 2:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5b48 + idx) = 0xc14a034;
          *(uint32_t *)(0x5a5b74 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        case 3:
        case 4:
        case 6:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5b48 + idx) = 0xc140000;
          *(uint32_t *)(0x5a5b74 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        case 7:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5ac0 + idx) = 0x1c140000;
          *(uint32_t *)(0x5a5b28 + idx) = 0xc00;
          *(uint32_t *)(0x5a5b48 + idx) = 0xc140000;
          *(uint32_t *)(0x5a5b74 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        default:
          display_assert("### ERROR unsupported framebuffer blend function",
                         "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                         "transparent_geometry.c",
                         0x3a2, 1);
          system_exit(-1);
        }
      } else {
        *(int *)0x5a5b94 = (short)stage_count;
      }
      goto set_shader_and_draw;
    }

    case 4:
      /* model effect: only valid for object groups; drawn via decal path */
      if (*(short *)(grp + 0x14) != 1) {
        display_assert(
          "### ERROR unsupported model effect type in transparent group",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
          "geometry.c",
          0x2ab, 1);
        system_exit(-1);
      }
      if (*(char *)0x47e4c0 != 0) {
        return;
      }
      FUN_00159900(grp);
      break;
    case 5: {
      /* shader_transparent_generic */
      char *gen;
      char *layers;
      char *map;
      char *stage;
      char *bitm;
      int frame_index; /* [EBP-0x34] */
      int n;
      int m;
      int j;
      short gtype;
      short first_map_type; /* [EBP-0x14] */
      short first_map_type_table[4]; /* [EBP-0xd8] */
      int op_table[4]; /* [EBP-0x134] */
      int colorop;
      int alphaop;
      float u;
      float v;
      float anim_out[32]; /* [EBP-0x330] 4 stages x 8 floats */
      char sub_group[0xa0]; /* [EBP-0x450] */
      int nstages;
      unsigned int fade_mode_value; /* [EBP-0x6c] */
      float fade_consts[12]; /* [EBP-0x180] */
      float t;
      float c[4]; /* [EBP-0x64] stage argb color */
      float da;
      float dr;
      float dg;
      float db;
      float *pf;
      char ok;
      int bcount;
      int limit;
      int eidx;
      int fvi;
      int k;
      float x;
      unsigned int blendrow;

      gen = (char *)FUN_001906b0(sh, 5);
      frame_index = *(unsigned short *)(grp + 0x10);
      layers = gen + 0x48;
      if (*(int *)layers > 0) {
        n = 0;
        do {
          csmemcpy(sub_group, grp, 0xa0);
          *(int *)(sub_group + 0x90) = -1;
          map = (char *)tag_block_get_element(layers, (short)n, 0x10);
          *(void **)(sub_group + 0xc) =
            tag_get(0x73686472, *(int *)(map + 0xc));
          rasterizer_transparent_geometry_group_draw(sub_group, dirty);
          n = n + 1;
        } while ((int)(short)n < *(int *)layers);
      }
      FUN_00178b40(0x18, vertex_type, permutation);
      SetRenderStateSmart(
        0x7f,
        (int)((-(unsigned int)((*(unsigned char *)(gen + 0x29) & 4) != 0) &
               0xfffff6ff) +
              0x901));
      SetRenderStateSmart(0x43, 0x10101);
      SetRenderStateSmart(0x3b, 1);
      SetRenderStateSmart(0x3c, *(unsigned char *)(gen + 0x29) & 1);
      SetRenderStateSmart(0x3d, 0x7f);
      FUN_001580b0(*(unsigned short *)(gen + 0x2c));
      if ((char)*(char *)(gen + 0x29) < 0 && *(int *)(grp + 0x6c) != 0 &&
          *(int *)(gen + 0x54) > 0) {
        /* numeric-counter driven first map index */
        map = (char *)tag_block_get_element(gen + 0x54, 0, 0x64);
        bitm = (char *)tag_get(0x6269746d, *(int *)(map + 0x28));
        bcount = *(short *)(bitm + 0x60);
        limit = (short)*(unsigned char *)(gen + 0x28);
        eidx = ((bcount != 8) - 1 & 3);
        x = (float)limit *
              *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) + eidx * 4) +
            *(float *)0x253398;
        /* PIN(FLOOR(...)) re-evaluates the floor expression per compare */
        if ((int)floor((double)x) < 0) {
          fvi = 0;
        } else if ((int)floor(
                     (double)((float)limit *
                                *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) +
                                           eidx * 4) +
                              *(float *)0x253398)) > limit) {
          fvi = limit;
        } else {
          fvi = (int)floor(
            (double)((float)limit *
                       *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) +
                                  eidx * 4) +
                     *(float *)0x253398));
        }
        for (k = *(short *)(grp + 0x10); k > 0; k--) {
          fvi = (int)(short)fvi / (int)(short)bcount;
        }
        frame_index = (int)(short)fvi % (int)(short)bcount;
      }
      m = 0;
      do {
        if ((int)(short)m < *(int *)(gen + 0x54)) {
          map = (char *)tag_block_get_element(gen + 0x54, (short)m, 0x64);
          gtype = *(short *)(gen + 0x2a);
          first_map_type_table[0] = 0;
          first_map_type_table[1] = 2;
          first_map_type_table[2] = 2;
          first_map_type_table[3] = 2;
          op_table[0] = 1;
          op_table[1] = 3;
          op_table[2] = 3;
          op_table[3] = 3;
          if ((short)m == 0) {
            first_map_type = first_map_type_table[gtype];
          } else {
            first_map_type = 0;
          }
          if ((*gen & 4) != 0 && gtype != 0) {
            display_assert(
              "!TEST_FLAG(shader_transparent_generic->shader.radiosity.flags, "
              "_shader_radiosity_FILTHY_transparent_lit_bit) || "
              "shader_transparent_generic->generic.type==_shader_transparent_"
              "generic_type_2d_map",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x406, 1);
            system_exit(-1);
          }
          if (gtype < 0 || gtype > 3) {
            display_assert(
              "type>=0 && type<NUMBER_OF_SHADER_TRANSPARENT_GENERIC_TYPES",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x407, 1);
            system_exit(-1);
          }
          rasterizer_set_texture((short)m, first_map_type, 0,
                                 *(int *)(map + 0x28), frame_index);
          if (first_map_type == 0 && (*map & 2) != 0) {
            colorop = 3;
          } else if ((short)m != 0) {
            colorop = 1;
          } else {
            colorop = op_table[gtype];
          }
          if (first_map_type == 0 && (*map & 4) != 0) {
            alphaop = 3;
          } else if ((short)m != 0) {
            alphaop = 1;
          } else {
            alphaop = op_table[gtype];
          }
          SetTextureStageStateSmart((short)m, 0xa, colorop);
          SetTextureStageStateSmart((short)m, 0xb, alphaop);
          SetTextureStageStateSmart((short)m, 0xc,
                                    ((short)m != 0) ? 1 : op_table[gtype]);
          D3DDevice_SetTextureStageState((short)m, 0xd, 2);
          SetTextureStageStateSmart((short)m, 0xe, 2 - (int)((*map & 1) != 0));
          SetTextureStageStateSmart((short)m, 0xf, 2 - (int)((*map & 1) != 0));
        }
        if ((int)(short)m < *(int *)(gen + 0x54) &&
            ((short)m > 0 || *(short *)(gen + 0x2a) == 0)) {
          map = (char *)tag_block_get_element(gen + 0x54, (short)m, 0x64);
          u = *(float *)(map + 4);
          v = *(float *)(map + 8);
          if ((short)m == 0) {
            if ((*(unsigned char *)(gen + 0x29) & 0x40) != 0) {
              u = -(u * *(float *)(grp + 0x70));
              v = -(v * *(float *)(grp + 0x70));
            }
            if ((*(unsigned char *)(gen + 0x29) & 8) == 0) {
              u = u * *(float *)(grp + 0x3c);
              v = v * *(float *)(grp + 0x40);
            }
          } else {
            u = u * *(float *)(grp + 0x3c);
            v = v * *(float *)(grp + 0x40);
          }
          FUN_00190e10(map + 0x2c, *(void **)(grp + 0x6c), u, v,
                       *(float *)(map + 0xc), *(float *)(map + 0x10),
                       *(float *)(map + 0x14), *(float *)0x5a5e18,
                       &anim_out[(short)m * 8], &anim_out[(short)m * 8 + 4]);
        } else if ((int)(short)m < *(int *)(gen + 0x54) &&
                   (*(unsigned char *)(gen + 0x29) & 8) != 0) {
          anim_out[(short)m * 8] = *(float *)0x5a5c64;
          anim_out[(short)m * 8 + 1] = *(float *)0x5a5c68;
          anim_out[(short)m * 8 + 2] = *(float *)0x5a5c6c;
          anim_out[(short)m * 8 + 4] = *(float *)0x5a5c70;
          anim_out[(short)m * 8 + 5] = *(float *)0x5a5c74;
          anim_out[(short)m * 8 + 6] = *(float *)0x5a5c78;
          anim_out[(short)m * 8 + 3] = 0.0f;
          anim_out[(short)m * 8 + 7] = 0.0f;
        } else {
          anim_out[(short)m * 8] = 1.0f;
          anim_out[(short)m * 8 + 1] = 0.0f;
          anim_out[(short)m * 8 + 2] = 0.0f;
          anim_out[(short)m * 8 + 4] = 0.0f;
          anim_out[(short)m * 8 + 5] = 1.0f;
          anim_out[(short)m * 8 + 6] = 0.0f;
          anim_out[(short)m * 8 + 3] = 0.0f;
          anim_out[(short)m * 8 + 7] = 0.0f;
        }
        m = m + 1;
      } while ((short)m < 4);
      D3DDevice_SetVertexShaderConstant(-0x51, anim_out, 8);
      if (success != 0) {
        ok = FUN_0017c2f0(*(void **)(grp + 0xc), (void *)0x5a5ac0);
        if (ok == 0) {
          success = 0;
        } else {
          success = 1;
        }
      } else {
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXANIM_OFFSET, "
                     "vsh_constants__texanim, VSH_CONSTANTS__TEXANIM_COUNT)");
        success = 0;
      }
      if (*(char *)0x3256d4 == 0) {
        goto generic_stage_colors;
      }
      nstages = *(int *)(gen + 0x60);
      if (nstages < 1) {
        nstages = 1;
      }
      if ((*grp & 0x10) != 0 && *(short *)(gen + 0x2c) == 0) {
        /* fog-plane driven fade into an extra combiner stage */
        t = -(plane3d_distance_to_point((float *)0x5a5dc8, (float *)0x5a5bc8) /
              *(float *)0x5a5dec);
        if (t < *(float *)0x2533c0) {
          t = 0.0f;
        } else if (t > *(float *)0x2533c8) {
          t = 1.0f;
        }
        ((uint32_t *)0x5a5ae8)[(short)nstages] = real_a_rgb_color_to_pixel32(
          t * *(float *)0x5a5de4, (float *)0x5a5dd8);
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = 0x310c1101;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        goto generic_stage_colors;
      }
      fade_consts[0] = 0.0f;
      fade_consts[1] = 0.0f;
      fade_consts[2] = 0.0f;
      fade_consts[3] = 0.0f;
      fade_consts[4] = 0.0f;
      fade_consts[5] = 0.0f;
      fade_consts[6] = 0.0f;
      fade_consts[7] = 0.0f;
      fade_consts[8] = 0.0f;
      fade_consts[9] = 0.0f;
      fade_consts[10] = 1.0f;
      fade_consts[11] = 0.0f;
      if (*(short *)(grp + 0x14) == 1) {
        t = *(float *)0x2533c8 - *(float *)(grp + 0x18);
        if (t < *(float *)0x2533c0) {
          fade_consts[10] = 0.0f;
        } else if (t > *(float *)0x2533c8) {
          fade_consts[10] = 1.0f;
        } else {
          fade_consts[10] = t;
        }
      }
      if (*(short *)(gen + 0x30) > 0 && *(int *)(grp + 0x6c) != 0 &&
          *(int *)(*(int *)(grp + 0x6c) + 4) != 0) {
        fade_consts[10] =
          fade_consts[10] * *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) - 4 +
                                       *(short *)(gen + 0x30) * 4);
      }
      D3DDevice_SetVertexShaderConstant(-0x54, fade_consts, 3);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXSCALE_OFFSET, "
                     "vsh_constants__texscale, VSH_CONSTANTS__TEXSCALE_COUNT)");
      }
      if (*(short *)(gen + 0x2e) == 0) {
        fade_mode_value = 0x14;
      } else if (*(short *)(gen + 0x2e) == 1) {
        fade_mode_value = 0x15;
      } else {
        if (*(short *)(gen + 0x2e) != 2) {
          display_assert("### ERROR unsupported framebuffer fade mode",
                         "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                         "transparent_geometry.c",
                         0x4a1, 1);
          system_exit(-1);
        }
        fade_mode_value = 5;
      }
      switch (*(short *)(gen + 0x2c)) {
      case 0:
        *(uint32_t *)(0x5a5ac0 + (short)nstages * 4) =
          (fade_mode_value | 0x1c00) << 0x10;
        *(uint32_t *)(0x5a5b28 + (short)nstages * 4) = 0xc00;
        break;
      case 1:
      case 5:
        blendrow =
          (fade_mode_value ^ 0x20) | fade_mode_value << 0x10 | 0xc002000;
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = blendrow;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        break;
      case 2:
        blendrow =
          (fade_mode_value ^ 0x20) | fade_mode_value << 0x10 | 0xc00a000;
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = blendrow;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        break;
      case 3:
      case 4:
      case 6:
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = (fade_mode_value | 0xc00)
                                                       << 0x10;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        break;
      case 7:
        *(uint32_t *)(0x5a5ac0 + (short)nstages * 4) =
          (fade_mode_value | 0x1c00) << 0x10;
        *(uint32_t *)(0x5a5b28 + (short)nstages * 4) = 0xc00;
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = (fade_mode_value | 0xc00)
                                                       << 0x10;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        break;
      default:
        display_assert("### ERROR unsupported framebuffer blend function",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x4c0, 1);
        system_exit(-1);
      }
    generic_stage_colors:
      j = 0;
      if (*(int *)(gen + 0x60) > 0) {
        do {
          stage = (char *)tag_block_get_element(gen + 0x60, (short)j, 0x70);
          if (*(float *)(stage + 8) == *(float *)0x2533c0) {
            display_assert("stage->constant_color0_animation_period!=0.0f",
                           "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_"
                           "xbox_transparent_geometry.c",
                           0x4d3, 1);
            system_exit(-1);
          }
          if (*(int *)(grp + 0x6c) != 0 && (*stage & 4) != 0) {
            t = **(float **)(*(int *)(grp + 0x6c) + 4);
          } else {
            t = FUN_0010a5e0(*(short *)(stage + 6),
                             *(float *)0x5a5e18 / *(float *)(stage + 8));
          }
          da = *(float *)(stage + 0x1c) - *(float *)(stage + 0xc);
          dr = *(float *)(stage + 0x20) - *(float *)(stage + 0x10);
          dg = *(float *)(stage + 0x24) - *(float *)(stage + 0x14);
          db = *(float *)(stage + 0x28) - *(float *)(stage + 0x18);
          c[0] = t * da + *(float *)(stage + 0xc);
          c[1] = t * dr + *(float *)(stage + 0x10);
          c[2] = dg * t + *(float *)(stage + 0x14);
          c[3] = t * db + *(float *)(stage + 0x18);
#if !defined(_MSC_VER) || defined(__clang__)
          /* The original stores each channel to a 32-bit float (FSTP) and the
           * range asserts below reload the rounded value; clang keeps the
           * channels in x87 registers (FST + FUCOMI) and compares at 80-bit
           * extended precision.  When t == 1.0 the lerp lo + round32(hi-lo)*t
           * can land half a ULP above 1.0 (e.g. needler core stage 6 with the
           * weapon A-out pegged at 1.0), which passes the original's rounded
           * compare but trips the extended-precision one.  Force the same
           * store+reload rounding before comparing. */
          asm volatile("" : "+m"(c[0]), "+m"(c[1]), "+m"(c[2]), "+m"(c[3]));
#endif
          if (!(c[1] >= *(float *)0x2533c0 && c[1] <= *(float *)0x2533c8)) {
            display_assert(
              "constant_color0.red >=0.0f && constant_color0.red <=1.0f",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x4e8, 1);
            system_exit(-1);
          }
          if (!(c[2] >= *(float *)0x2533c0 && c[2] <= *(float *)0x2533c8)) {
            display_assert(
              "constant_color0.green>=0.0f && constant_color0.green<=1.0f",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x4e9, 1);
            system_exit(-1);
          }
          if (!(c[3] >= *(float *)0x2533c0 && c[3] <= *(float *)0x2533c8)) {
            display_assert(
              "constant_color0.blue >=0.0f && constant_color0.blue <=1.0f",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x4ea, 1);
            system_exit(-1);
          }
          if (*(short *)(stage + 4) > 0 && *(short *)(stage + 4) < 5 &&
              *(int **)(grp + 0x6c) != (int *)0 &&
              **(int **)(grp + 0x6c) != 0) {
            pf = (float *)(**(int **)(grp + 0x6c) - 0xc +
                           *(short *)(stage + 4) * 0xc);
            if (!(pf[0] >= *(float *)0x2533c0 && pf[0] <= *(float *)0x2533c8)) {
              display_assert(
                "external_color->red >=0.0f && external_color->red <=1.0f",
                "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                "transparent_geometry.c",
                0x4f5, 1);
              system_exit(-1);
            }
            if (!(pf[1] >= *(float *)0x2533c0 && pf[1] <= *(float *)0x2533c8)) {
              display_assert(
                "external_color->green>=0.0f && external_color->green<=1.0f",
                "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                "transparent_geometry.c",
                0x4f6, 1);
              system_exit(-1);
            }
            if (!(pf[2] >= *(float *)0x2533c0 && pf[2] <= *(float *)0x2533c8)) {
              display_assert(
                "external_color->blue >=0.0f && external_color->blue <=1.0f",
                "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                "transparent_geometry.c",
                0x4f7, 1);
              system_exit(-1);
            }
            c[1] = c[1] * pf[0];
            c[2] = c[2] * pf[1];
            c[3] = c[3] * pf[2];
          }
          ((uint32_t *)0x5a5ae8)[(short)j] = FUN_000d1c90(c);
          j = j + 1;
        } while ((int)(short)j < *(int *)(gen + 0x60));
      }
      goto set_shader_and_draw;
    }
    case 6: {
      /* shader_transparent_chicago */
      char *chi;
      char *layers2;
      char *map2;
      char *bitm2;
      int frame_index2; /* [EBP-0x14] */
      int m2; /* loop counter (param slot reuse in original) */
      short ctype;
      short first_map_type2; /* [EBP-0xc] */
      short first_map_type_table2[4]; /* [EBP-0xd0] */
      int op_table2[4]; /* [EBP-0x144] */
      int colorop2;
      int alphaop2;
      float u2;
      float v2;
      float anim_out2[32]; /* [EBP-0x3b0] */
      char sub_group2[0xa0]; /* [EBP-0x4f0] */
      short nstages2;
      unsigned int fade_mode_value2; /* [EBP-0x70] */
      float fade_consts2[12]; /* [EBP-0x1b0] */
      float t2;
      char ok2;
      int bcount2;
      int limit2;
      int eidx2;
      int fvi2;
      int k2;
      float x2;
      unsigned int blendrow2;

      chi = (char *)FUN_001906b0(sh, 6);
      frame_index2 = *(unsigned short *)(grp + 0x10);
      layers2 = chi + 0x48;
      /* NOTE: original re-reads the layer count each iteration and always
       * fetches element 0 -- faithful reproduction of the binary */
      while (*(int *)layers2 > 0) {
        csmemcpy(sub_group2, grp, 0xa0);
        *(int *)(sub_group2 + 0x90) = -1;
        map2 = (char *)tag_block_get_element(layers2, 0, 0x10);
        *(void **)(sub_group2 + 0xc) =
          tag_get(0x73686472, *(int *)(map2 + 0xc));
        rasterizer_transparent_geometry_group_draw(sub_group2, dirty);
      }
      FUN_00178b40(0x18, vertex_type, permutation);
      SetRenderStateSmart(
        0x7f,
        (int)((-(unsigned int)((*(unsigned char *)(chi + 0x29) & 4) != 0) &
               0xfffff6ff) +
              0x901));
      SetRenderStateSmart(0x43, 0x10101);
      SetRenderStateSmart(0x3b, 1);
      SetRenderStateSmart(0x3c, *(unsigned char *)(chi + 0x29) & 1);
      SetRenderStateSmart(0x3d, 0x7f);
      FUN_001580b0(*(unsigned short *)(chi + 0x2c));
      if ((char)*(char *)(chi + 0x29) < 0 && *(int *)(grp + 0x6c) != 0 &&
          *(int *)(chi + 0x54) > 0) {
        map2 = (char *)tag_block_get_element(chi + 0x54, 0, 0xdc);
        bitm2 = (char *)tag_get(0x6269746d, *(int *)(map2 + 0x78));
        bcount2 = *(short *)(bitm2 + 0x60);
        frame_index2 = (short)bcount2;
        if ((*(unsigned char *)(chi + 0x60) & 2) != 0) {
          frame_index2 =
            numeric_countdown_timer_get(*(unsigned short *)(grp + 0x10));
        } else {
          limit2 = (short)*(unsigned char *)(chi + 0x28);
          eidx2 = ((bcount2 != 8) - 1 & 3);
          x2 = (float)limit2 *
                 *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) + eidx2 * 4) +
               *(float *)0x253398;
          /* PIN(FLOOR(...)) re-evaluates the floor expression per compare */
          if ((int)floor((double)x2) < 0) {
            fvi2 = 0;
          } else if ((int)floor((
                       double)((float)limit2 *
                                 *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) +
                                            eidx2 * 4) +
                               *(float *)0x253398)) > limit2) {
            fvi2 = limit2;
          } else {
            fvi2 = (int)floor(
              (double)((float)limit2 *
                         *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) +
                                    eidx2 * 4) +
                       *(float *)0x253398));
          }
          for (k2 = *(short *)(grp + 0x10); k2 > 0; k2--) {
            fvi2 = (int)(short)fvi2 / (int)(short)frame_index2;
          }
          frame_index2 = (int)(short)fvi2 % (int)(short)frame_index2;
        }
      }
      m2 = 0;
      do {
        if ((int)(short)m2 < *(int *)(chi + 0x54)) {
          map2 = (char *)tag_block_get_element(chi + 0x54, (short)m2, 0xdc);
          ctype = *(short *)(chi + 0x2a);
          first_map_type_table2[0] = 0;
          first_map_type_table2[1] = 2;
          first_map_type_table2[2] = 2;
          first_map_type_table2[3] = 2;
          op_table2[0] = 1;
          op_table2[1] = 3;
          op_table2[2] = 3;
          op_table2[3] = 3;
          if ((short)m2 == 0) {
            first_map_type2 = first_map_type_table2[ctype];
          } else {
            first_map_type2 = 0;
          }
          if ((*chi & 4) != 0 && ctype != 0) {
            display_assert(
              "!TEST_FLAG(shader_transparent_chicago->shader.radiosity.flags, "
              "_shader_radiosity_FILTHY_transparent_lit_bit) || "
              "shader_transparent_chicago->chicago.type==_shader_transparent_"
              "chicago_type_2d_map",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x567, 1);
            system_exit(-1);
          }
          if (ctype < 0 || ctype > 3) {
            display_assert(
              "type>=0 && type<NUMBER_OF_SHADER_TRANSPARENT_CHICAGO_TYPES",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x568, 1);
            system_exit(-1);
          }
          rasterizer_set_texture((short)m2, first_map_type2, 0,
                                 *(int *)(map2 + 0x78), frame_index2);
          if (first_map_type2 == 0 && (*map2 & 4) != 0) {
            colorop2 = 3;
          } else if ((short)m2 != 0) {
            colorop2 = 1;
          } else {
            colorop2 = op_table2[ctype];
          }
          if (first_map_type2 == 0 && (*map2 & 8) != 0) {
            alphaop2 = 3;
          } else if ((short)m2 != 0) {
            alphaop2 = 1;
          } else {
            alphaop2 = op_table2[ctype];
          }
          D3DDevice_SetTextureStageState((short)m2, 0xa, colorop2);
          D3DDevice_SetTextureStageState((short)m2, 0xb, alphaop2);
          D3DDevice_SetTextureStageState(
            (short)m2, 0xc, ((short)m2 != 0) ? 1 : op_table2[ctype]);
          D3DDevice_SetTextureStageState((short)m2, 0xd, 2);
          D3DDevice_SetTextureStageState((short)m2, 0xe,
                                         2 - (int)((*map2 & 1) != 0));
          D3DDevice_SetTextureStageState((short)m2, 0xf,
                                         2 - (int)((*map2 & 1) != 0));
        }
        if ((int)(short)m2 < *(int *)(chi + 0x54) &&
            ((short)m2 > 0 || *(short *)(chi + 0x2a) == 0)) {
          map2 = (char *)tag_block_get_element(chi + 0x54, (short)m2, 0xdc);
          u2 = *(float *)(map2 + 0x54);
          v2 = *(float *)(map2 + 0x58);
          if ((short)m2 == 0) {
            if ((*(unsigned char *)(chi + 0x29) & 0x40) != 0) {
              u2 = -(u2 * *(float *)(grp + 0x70));
              v2 = -(v2 * *(float *)(grp + 0x70));
            }
            if ((*(unsigned char *)(chi + 0x29) & 8) == 0) {
              u2 = u2 * *(float *)(grp + 0x3c);
              v2 = v2 * *(float *)(grp + 0x40);
            }
          } else {
            u2 = u2 * *(float *)(grp + 0x3c);
            v2 = v2 * *(float *)(grp + 0x40);
          }
          FUN_00190e10(map2 + 0xa4, *(void **)(grp + 0x6c), u2, v2,
                       *(float *)(map2 + 0x5c), *(float *)(map2 + 0x60),
                       *(float *)(map2 + 0x64), *(float *)0x5a5e18,
                       &anim_out2[(short)m2 * 8],
                       &anim_out2[(short)m2 * 8 + 4]);
        } else if ((int)(short)m2 < *(int *)(chi + 0x54) &&
                   (*(unsigned char *)(chi + 0x29) & 8) != 0) {
          anim_out2[(short)m2 * 8] = *(float *)0x5a5c64;
          anim_out2[(short)m2 * 8 + 1] = *(float *)0x5a5c68;
          anim_out2[(short)m2 * 8 + 2] = *(float *)0x5a5c6c;
          anim_out2[(short)m2 * 8 + 4] = *(float *)0x5a5c70;
          anim_out2[(short)m2 * 8 + 5] = *(float *)0x5a5c74;
          anim_out2[(short)m2 * 8 + 6] = *(float *)0x5a5c78;
          anim_out2[(short)m2 * 8 + 3] = 0.0f;
          anim_out2[(short)m2 * 8 + 7] = 0.0f;
        } else {
          anim_out2[(short)m2 * 8] = 1.0f;
          anim_out2[(short)m2 * 8 + 1] = 0.0f;
          anim_out2[(short)m2 * 8 + 2] = 0.0f;
          anim_out2[(short)m2 * 8 + 4] = 0.0f;
          anim_out2[(short)m2 * 8 + 5] = 1.0f;
          anim_out2[(short)m2 * 8 + 6] = 0.0f;
          anim_out2[(short)m2 * 8 + 3] = 0.0f;
          anim_out2[(short)m2 * 8 + 7] = 0.0f;
        }
        m2 = m2 + 1;
      } while ((short)m2 < 4);
      D3DDevice_SetVertexShaderConstant(-0x51, anim_out2, 8);
      if (success == 0) {
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXANIM_OFFSET, "
                     "vsh_constants__texanim, VSH_CONSTANTS__TEXANIM_COUNT)");
        success = 0;
      } else {
        ok2 = FUN_0017bca0(*(void **)(grp + 0xc), (void *)0x5a5ac0);
        success = 1;
        if (ok2 == 0) {
          success = 0;
        }
      }
      if (*(char *)0x3256d4 == 0) {
        goto set_shader_and_draw;
      }
      nstages2 = *(short *)(chi + 0x54);
      if ((*grp & 0x10) != 0 && *(short *)(chi + 0x2c) == 0) {
        t2 = -(plane3d_distance_to_point((float *)0x5a5dc8, (float *)0x5a5bc8) /
               *(float *)0x5a5dec);
        if (t2 < *(float *)0x2533c0) {
          t2 = 0.0f;
        } else if (t2 > *(float *)0x2533c8) {
          t2 = 1.0f;
        }
        ((uint32_t *)0x5a5ae8)[nstages2] = real_a_rgb_color_to_pixel32(
          *(float *)0x5a5de4 * t2, (float *)0x5a5dd8);
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = 0x310c1101;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        goto set_shader_and_draw;
      }
      fade_consts2[0] = 0.0f;
      fade_consts2[1] = 0.0f;
      fade_consts2[2] = 0.0f;
      fade_consts2[3] = 0.0f;
      fade_consts2[4] = 0.0f;
      fade_consts2[5] = 0.0f;
      fade_consts2[6] = 0.0f;
      fade_consts2[7] = 0.0f;
      fade_consts2[8] = 0.0f;
      fade_consts2[9] = 0.0f;
      fade_consts2[10] = 1.0f;
      fade_consts2[11] = 0.0f;
      if (*(short *)(grp + 0x14) == 1 &&
          (*(unsigned char *)(chi + 0x60) & 1) == 0) {
        t2 = *(float *)0x2533c8 - *(float *)(grp + 0x18);
        if (t2 < *(float *)0x2533c0) {
          fade_consts2[10] = 0.0f;
        } else if (t2 > *(float *)0x2533c8) {
          fade_consts2[10] = 1.0f;
        } else {
          fade_consts2[10] = t2;
        }
      }
      if (*(short *)(chi + 0x30) > 0 && *(int *)(grp + 0x6c) != 0 &&
          *(int *)(*(int *)(grp + 0x6c) + 4) != 0) {
        fade_consts2[10] =
          fade_consts2[10] * *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) - 4 +
                                        *(short *)(chi + 0x30) * 4);
      }
      D3DDevice_SetVertexShaderConstant(-0x54, fade_consts2, 3);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXSCALE_OFFSET, "
                     "vsh_constants__texscale, VSH_CONSTANTS__TEXSCALE_COUNT)");
      }
      if (*(short *)(chi + 0x2e) == 0) {
        fade_mode_value2 = 0x14;
      } else if (*(short *)(chi + 0x2e) == 1) {
        fade_mode_value2 = 0x15;
      } else {
        if (*(short *)(chi + 0x2e) != 2) {
          display_assert("### ERROR unsupported framebuffer fade mode",
                         "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                         "transparent_geometry.c",
                         0x603, 1);
          system_exit(-1);
        }
        fade_mode_value2 = 5;
      }
      switch (*(short *)(chi + 0x2c)) {
      case 0:
        *(uint32_t *)(0x5a5ac0 + nstages2 * 4) = (fade_mode_value2 | 0x1c00)
                                                 << 0x10;
        *(uint32_t *)(0x5a5b28 + nstages2 * 4) = 0xc00;
        break;
      case 1:
      case 5:
        blendrow2 =
          (fade_mode_value2 ^ 0x20) | fade_mode_value2 << 0x10 | 0xc002000;
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = blendrow2;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        break;
      case 2:
        blendrow2 =
          (fade_mode_value2 ^ 0x20) | fade_mode_value2 << 0x10 | 0xc00a000;
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = blendrow2;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        break;
      case 3:
      case 4:
      case 6:
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = (fade_mode_value2 | 0xc00)
                                                 << 0x10;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        break;
      case 7:
        *(uint32_t *)(0x5a5ac0 + nstages2 * 4) = (fade_mode_value2 | 0x1c00)
                                                 << 0x10;
        *(uint32_t *)(0x5a5b28 + nstages2 * 4) = 0xc00;
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = (fade_mode_value2 | 0xc00)
                                                 << 0x10;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        break;
      default:
        display_assert("### ERROR unsupported framebuffer blend function",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x622, 1);
        system_exit(-1);
      }
      goto set_shader_and_draw;
    }

    case 7:
      FUN_00179de0(grp);
      break;
    case 8: {
      /* shader_transparent_glass */
      char *gls;
      short reflection_type;
      float glass_consts[12]; /* diffuse [EBP-0x280] */
      float refl_consts[12]; /* reflection [EBP-0x2b0] */
      float bump_consts[12]; /* bump/specular [EBP-0x220] */
      float bump_color[3]; /* [EBP-0x7c] */
      float bc;

      gls = (char *)FUN_001906b0(sh, 8);
      reflection_type = *(short *)(gls + 0x8a);
      if (reflection_type == 2) {
        if (*(char *)0x5a5bc4 == 0 || *(short *)0x5a5bc0 != 0) {
          break;
        }
      } else if (reflection_type == 0 &&
                 ((*(unsigned char *)(gls + 0x28) & 8) != 0 ||
                  *(int *)(gls + 0xcc) == -1)) {
        reflection_type = 1;
      }
      if (*(int *)(gls + 0x70) != -1 ||
          *(float *)(gls + 0x54) != *(float *)0x2533c0 ||
          *(float *)(gls + 0x58) != *(float *)0x2533c0 ||
          *(float *)(gls + 0x5c) != *(float *)0x2533c0) {
        /* diffuse pass */
        rasterizer_set_texture(0, 0, 1, *(int *)(gls + 0x70),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(0, 0xa, 1);
        SetTextureStageStateSmart(0, 0xb, 1);
        SetTextureStageStateSmart(0, 0xd, 2);
        SetTextureStageStateSmart(0, 0xe, 2);
        SetTextureStageStateSmart(0, 0xf, 2);
        SetRenderStateSmart(
          0x7f,
          (int)((-(unsigned int)((*(unsigned char *)(gls + 0x28) & 4) != 0) &
                 0xfffff6ff) +
                0x901));
        SetRenderStateSmart(0x43, 0x10101);
        SetRenderStateSmart(0x3b, 1);
        SetRenderStateSmart(0x3e, 0);
        SetRenderStateSmart(0x3f, 0x300);
        SetRenderStateSmart(0x4a, 0x8006);
        SetRenderStateSmart(0x3c, 1);
        SetRenderStateSmart(0x3d, 0);
        FUN_00178b40(0x2e, vertex_type, permutation);
        glass_consts[0] = *(float *)(grp + 0x3c) * *(float *)(gls + 0x60);
        glass_consts[1] = *(float *)(grp + 0x40) * *(float *)(gls + 0x60);
        glass_consts[2] = 1.0f;
        glass_consts[3] = 1.0f;
        glass_consts[4] = 0.0f;
        glass_consts[5] = 0.0f;
        glass_consts[6] = 0.0f;
        glass_consts[7] = 0.0f;
        glass_consts[8] = 0.0f;
        glass_consts[9] = 0.0f;
        glass_consts[10] = 0.0f;
        glass_consts[11] = 0.0f;
        D3DDevice_SetVertexShaderConstant(-0x54, glass_consts, 3);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(
            0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
               "VSH_CONSTANTS__TEXSCALE_OFFSET, vsh_constants__texscale, "
               "VSH_CONSTANTS__TEXSCALE_COUNT)");
        }
        csmemset((void *)0x5a5ac0, 0, 0xf0);
        *(uint32_t *)0x5a5b98 = 1;
        *(uint32_t *)0x5a5b94 = 1;
        *(uint32_t *)0x5a5ae8 = FUN_000d1dd0((float *)(gls + 0x54));
        *(uint32_t *)0x5a5b48 = 0x8010000;
        *(uint32_t *)0x5a5b74 = 0xc0;
        if (*(short *)(grp + 0x14) == 1) {
          *(uint32_t *)0x5a5b08 = FUN_00159070(*(float *)(grp + 0x18));
          *(uint32_t *)0x5a5ac0 = 0x14320000;
          *(uint32_t *)0x5a5b28 = 0x40;
        }
        *(uint32_t *)0x5a5ae0 = 0x140c2000;
        *(uint32_t *)0x5a5ae4 = 0x1400;
        rasterizer_set_pixel_shader((void *)0x5a5ac0);
        FUN_00174510(grp, 0);
      }
      if ((*(float *)(gls + 0x8c) > *(float *)0x2533c0 ||
           *(float *)(gls + 0x9c) > *(float *)0x2533c0) &&
          (*(int *)(gls + 0xb8) != -1 || reflection_type == 2)) {
        /* reflection pass */
        if (reflection_type < 0 || reflection_type > 2) {
          display_assert("reflection_type>=0 && "
                         "reflection_type<NUMBER_OF_SHADER_TRANSPARENT_GLASS_"
                         "REFLECTION_TYPES",
                         "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                         "transparent_geometry.c",
                         0x69e, 1);
          system_exit(-1);
        }
        rasterizer_set_texture(0, 0, 3, *(int *)(gls + 0xcc),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(0, 0xa, 1);
        SetTextureStageStateSmart(0, 0xb, 1);
        SetTextureStageStateSmart(0, 0xd, 2);
        SetTextureStageStateSmart(0, 0xe, 2);
        SetTextureStageStateSmart(0, 0xf, 2);
        rasterizer_set_texture_direct(1, *(int *)(*(int *)0x476204 + 0x1c), 0);
        SetTextureStageStateSmart(1, 0xa, 3);
        SetTextureStageStateSmart(1, 0xb, 3);
        SetTextureStageStateSmart(1, 0xc, 3);
        SetTextureStageStateSmart(1, 0xd, 2);
        SetTextureStageStateSmart(1, 0xe, 1);
        SetTextureStageStateSmart(1, 0xf, 1);
        rasterizer_set_texture_direct(2, *(int *)(*(int *)0x476204 + 0x1c), 0);
        SetTextureStageStateSmart(2, 0xa, 3);
        SetTextureStageStateSmart(2, 0xb, 3);
        SetTextureStageStateSmart(2, 0xc, 3);
        SetTextureStageStateSmart(2, 0xd, 2);
        SetTextureStageStateSmart(2, 0xe, 1);
        SetTextureStageStateSmart(2, 0xf, 1);
        if (reflection_type == 2) {
          FUN_001584f0(3, 1, 0);
          SetTextureStageStateSmart(3, 0xa, 3);
          SetTextureStageStateSmart(3, 0xb, 3);
          SetTextureStageStateSmart(3, 0xd, 2);
          SetTextureStageStateSmart(3, 0xe, 2);
          SetTextureStageStateSmart(3, 0xf, 1);
        } else {
          rasterizer_set_texture(3, 2, 0, *(int *)(gls + 0xb8),
                                 *(unsigned short *)(grp + 0x10));
          SetTextureStageStateSmart(3, 0xa, 3);
          SetTextureStageStateSmart(3, 0xb, 3);
          SetTextureStageStateSmart(3, 0xc, 3);
          SetTextureStageStateSmart(3, 0xd, 2);
          SetTextureStageStateSmart(3, 0xe, 2);
          SetTextureStageStateSmart(3, 0xf, 2);
        }
        SetRenderStateSmart(
          0x7f,
          (int)((-(unsigned int)((*(unsigned char *)(gls + 0x28) & 4) != 0) &
                 0xfffff6ff) +
                0x901));
        SetRenderStateSmart(0x43, 0x10101);
        SetRenderStateSmart(0x3b, 1);
        SetRenderStateSmart(0x3e, 0x302);
        SetRenderStateSmart(0x3f, 1);
        SetRenderStateSmart(0x4a, 0x8006);
        SetRenderStateSmart(0x3c, 0);
        FUN_00178b40(0x2b, vertex_type, reflection_type);
        refl_consts[0] = *(float *)(grp + 0x3c) * *(float *)(gls + 0xbc);
        refl_consts[1] = *(float *)(grp + 0x40) * *(float *)(gls + 0xbc);
        refl_consts[2] = 320.0f;
        refl_consts[3] = 240.0f;
        refl_consts[4] = 0.0f;
        refl_consts[5] = 0.0f;
        refl_consts[6] = 0.0f;
        refl_consts[7] = 0.0f;
        refl_consts[8] = 0.0f;
        refl_consts[9] = 0.0f;
        refl_consts[10] = 0.0f;
        refl_consts[11] = 0.0f;
        D3DDevice_SetVertexShaderConstant(-0x54, refl_consts, 3);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(
            0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
               "VSH_CONSTANTS__TEXSCALE_OFFSET, vsh_constants__texscale, "
               "VSH_CONSTANTS__TEXSCALE_COUNT)");
        }
        csmemset((void *)0x5a5ac0, 0, 0xf0);
        if (reflection_type == 0) {
          *(uint32_t *)0x5a5b98 = 0x62e21;
          *(uint32_t *)0x5a5ba0 = 0;
          *(uint32_t *)0x5a5b9c = 0x111;
        } else if (reflection_type == 1) {
          *(uint32_t *)0x5a5b98 = 0x18c61;
        } else {
          if (reflection_type != 2) {
            display_assert("### ERROR unsupported reflection type",
                           "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_"
                           "xbox_transparent_geometry.c",
                           0x707, 1);
            system_exit(-1);
          }
          *(uint32_t *)0x5a5b98 = 0x8c61;
        }
        *(uint32_t *)0x5a5b94 = 0x11005;
        if (reflection_type != 0 && *(int *)(gls + 0xcc) != -1) {
          *(uint32_t *)0x5a5b48 = 0x49480b0b;
        } else {
          bc = *(float *)0x253398 - *(float *)0x5a5bd4 * *(float *)0x253398;
          if (bc < *(float *)0x2533c0) {
            bump_color[0] = 0.0f;
          } else if (bc > *(float *)0x2533c8) {
            bump_color[0] = 1.0f;
          } else {
            bump_color[0] = bc;
          }
          bc = *(float *)0x253398 - *(float *)0x5a5bd8 * *(float *)0x253398;
          if (bc < *(float *)0x2533c0) {
            bump_color[1] = 0.0f;
          } else if (bc > *(float *)0x2533c8) {
            bump_color[1] = 1.0f;
          } else {
            bump_color[1] = bc;
          }
          bc = *(float *)0x253398 - *(float *)0x5a5bdc * *(float *)0x253398;
          if (bc < *(float *)0x2533c0) {
            bump_color[2] = 0.0f;
          } else if (bc > *(float *)0x2533c8) {
            bump_color[2] = 1.0f;
          } else {
            bump_color[2] = bc;
          }
          *(uint32_t *)0x5a5ae8 = FUN_000d1dd0(bump_color);
          *(uint32_t *)0x5a5b48 = 0x4a410b0b;
        }
        *(uint32_t *)0x5a5b74 = 0x20cd;
        *(uint32_t *)0x5a5b4c = 0xc0c0d0d;
        *(uint32_t *)0x5a5b78 = 0xcd;
        if (*(short *)(grp + 0x14) == 1) {
          *(uint32_t *)0x5a5b0c = FUN_00159070(*(float *)(grp + 0x18));
          *(uint32_t *)0x5a5ac4 = 0x14320000;
          *(uint32_t *)0x5a5b2c = 0x40;
        }
        *(uint32_t *)0x5a5b50 = 0xc0c0d0d;
        *(uint32_t *)0x5a5b7c = 0xd;
        *(uint32_t *)0x5a5af4 = FUN_000d1c90((float *)(gls + 0x8c));
        *(uint32_t *)0x5a5b14 = FUN_000d1c90((float *)(gls + 0x9c));
        *(uint32_t *)0x5a5b34 = 0xc00;
        *(uint32_t *)0x5a5b80 = 0xc00;
        *(uint32_t *)0x5a5b84 = 0xc00;
        *(uint32_t *)0x5a5acc = 0x2c120c11;
        *(uint32_t *)0x5a5b54 = 0x2c020c01;
        *(uint32_t *)0x5a5b58 = 0x2c0d0c0b;
        *(uint32_t *)0x5a5ae0 = 0xc0f0000;
        *(uint32_t *)0x5a5ae4 =
          ((-(unsigned int)((*(unsigned char *)(gls + 0x28) & 8) != 0) &
            0xfffffff4) +
           0x14) *
            0x10000 |
          0x1c002000;
        rasterizer_set_pixel_shader((void *)0x5a5ac0);
        FUN_00174510(grp, 0);
      }
      if (*(int *)(gls + 0x164) != -1 || *(int *)(gls + 0x178) != -1) {
        /* bump/specular pass */
        rasterizer_set_texture(0, 0, 1, *(int *)(gls + 0x164),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(0, 0xa, 1);
        SetTextureStageStateSmart(0, 0xb, 1);
        SetTextureStageStateSmart(0, 0xd, 2);
        SetTextureStageStateSmart(0, 0xe, 2);
        SetTextureStageStateSmart(0, 0xf, 2);
        rasterizer_set_texture(1, 0, 2, *(int *)(gls + 0x178),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(1, 0xa, 1);
        SetTextureStageStateSmart(1, 0xb, 1);
        SetTextureStageStateSmart(1, 0xd, 2);
        SetTextureStageStateSmart(1, 0xe, 2);
        SetTextureStageStateSmart(1, 0xf, 2);
        SetRenderStateSmart(
          0x7f,
          (int)((-(unsigned int)((*(unsigned char *)(gls + 0x28) & 4) != 0) &
                 0xfffff6ff) +
                0x901));
        SetRenderStateSmart(0x43, 0x10101);
        SetRenderStateSmart(0x3b, 1);
        SetRenderStateSmart(0x3e, 0x302);
        SetRenderStateSmart(0x3f, 0x303);
        SetRenderStateSmart(0x4a, 0x8006);
        SetRenderStateSmart(0x3c, 1);
        SetRenderStateSmart(0x3d, 0);
        if (*(int *)(grp + 0x5c) == 0) {
          rasterizer_set_texture(2, 0, 0, -1, 0);
          SetTextureStageStateSmart(2, 0xa, 3);
          SetTextureStageStateSmart(2, 0xb, 3);
          SetTextureStageStateSmart(2, 0xd, 2);
          SetTextureStageStateSmart(2, 0xe, 2);
          SetTextureStageStateSmart(2, 0xf, 2);
        } else {
          rasterizer_set_texture_bitmap_data(2, *(void **)(grp + 0x5c));
          SetTextureStageStateSmart(2, 0xa, 3);
          SetTextureStageStateSmart(2, 0xb, 3);
          SetTextureStageStateSmart(2, 0xd, 2);
          SetTextureStageStateSmart(2, 0xe, 2);
          SetTextureStageStateSmart(2, 0xf, 2);
        }
        FUN_00178b40(0x19, vertex_type, permutation);
        bump_consts[0] = *(float *)(gls + 0x154) * *(float *)(grp + 0x3c);
        bump_consts[1] = *(float *)(gls + 0x154) * *(float *)(grp + 0x40);
        bump_consts[2] = *(float *)(gls + 0x168) * *(float *)(grp + 0x3c);
        bump_consts[3] = *(float *)(gls + 0x168) * *(float *)(grp + 0x40);
        bump_consts[4] = 0.0f;
        bump_consts[5] = 0.0f;
        bump_consts[6] = 0.0f;
        bump_consts[7] = 0.0f;
        bump_consts[8] = 0.0f;
        bump_consts[9] = 0.0f;
        bump_consts[10] = 0.0f;
        bump_consts[11] = 0.0f;
        D3DDevice_SetVertexShaderConstant(-0x54, bump_consts, 3);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(
            0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
               "VSH_CONSTANTS__TEXSCALE_OFFSET, vsh_constants__texscale, "
               "VSH_CONSTANTS__TEXSCALE_COUNT)");
        }
        csmemset((void *)0x5a5ac0, 0, 0xf0);
        *(uint32_t *)0x5a5b98 = 0x421;
        *(uint32_t *)0x5a5b94 = 3;
        *(uint32_t *)0x5a5ac0 = 0x18190000;
        *(uint32_t *)0x5a5b28 = 0xc0;
        *(uint32_t *)0x5a5b48 = 0x8090000;
        *(uint32_t *)0x5a5b74 = 0x100c0;
        *(uint32_t *)0x5a5ac4 = 0x1c140000;
        *(uint32_t *)0x5a5b2c = 0xc0;
        *(uint32_t *)0x5a5b4c = 0xa200420;
        *(uint32_t *)0x5a5b78 = 0xd00;
        *(uint32_t *)0x5a5b50 = 0xc0d0000;
        *(uint32_t *)0x5a5b7c = 0xc0;
        *(uint32_t *)0x5a5ae0 = 0xc;
        *(uint32_t *)0x5a5ae4 = 0x1c00;
        rasterizer_set_pixel_shader((void *)0x5a5ac0);
        FUN_00174510(grp, *(int *)(grp + 0x5c) != 0);
      }
      break;
    }

    case 9: {
      /* shader_transparent_meter */
      char *met;
      float brightness; /* [EBP-0x3c] */
      float power; /* [EBP-0x44] */
      float gradient; /* [EBP-0x4c] */
      float met_alpha; /* [EBP-0x48] */
      float flash; /* rides FPU stack in original */
      float tint[3]; /* [EBP-0x150] */
      float flash_color[3]; /* [EBP-0x28] */
      float inv_flash;
      float x9;
      float t9;
      short src;
      int *ext9;
      uint32_t px1;
      uint32_t px2;
      uint32_t px3;
      uint32_t px4;
      uint32_t px_final;
      float px_final_alpha;
      float *px_final_color;
      float px3_alpha;
      float meter_consts[12]; /* [EBP-0x250] */

      met = (char *)FUN_001906b0(sh, 9);
      brightness = 1.0f;
      power = 1.0f;
      gradient = 1.0f;
      met_alpha = 1.0f;
      flash = 1.0f;
      if (*(int *)(grp + 0x6c) != 0 &&
          *(int *)(*(int *)(grp + 0x6c) + 4) != 0) {
        ext9 = (int *)*(int *)(*(int *)(grp + 0x6c) + 4);
        src = *(short *)(met + 0xd8);
        if (src > 0 && src < 5) {
          brightness = *((float *)ext9 + (src - 1));
        }
        src = *(short *)(met + 0xda);
        if (src > 0 && src < 5) {
          power = *((float *)ext9 + (src - 1));
        }
        src = *(short *)(met + 0xdc);
        if (src > 0 && src < 5) {
          gradient = *((float *)ext9 + (src - 1));
        }
        src = *(short *)(met + 0xde);
        if (src > 0 && src < 5) {
          flash = *((float *)ext9 + (src - 1));
        }
        src = *(short *)(met + 0xe0);
        if (src > 0 && src < 5) {
          met_alpha = *((float *)ext9 + (src - 1));
        }
      }
      if (*(char *)0x3256c3 != 0) {
        /* rasterizer_debug_meters override */
        t9 = FUN_0010a5e0(2, *(float *)0x5a5e18 / *(float *)0x325724);
        if (*(float *)0x325728 >= *(float *)0x2533c0) {
          brightness = *(float *)0x325728;
        } else {
          brightness = t9;
        }
        if (*(float *)0x32572c >= *(float *)0x2533c0) {
          power = *(float *)0x32572c;
        } else {
          power = t9;
        }
        if (*(float *)0x325730 >= *(float *)0x2533c0) {
          gradient = *(float *)0x325730;
        } else {
          gradient = t9;
        }
        if (*(float *)0x325734 >= *(float *)0x2533c0) {
          flash = *(float *)0x325734;
        } else {
          flash = t9;
        }
        if (*(float *)0x325738 >= *(float *)0x2533c0) {
          met_alpha = *(float *)0x325738;
        } else {
          met_alpha = t9;
        }
      }
      tint[0] = power * *(float *)(met + 0xa0);
      tint[1] = power * *(float *)(met + 0xa4);
      tint[2] = power * *(float *)(met + 0xa8);
      x9 = flash * *(float *)0x253f78;
      if (x9 <= *(float *)0x2533c8) {
        x9 = 1.0f;
      }
      inv_flash = *(float *)0x2533c8 / x9;
      if ((*(unsigned char *)(met + 0x28) & 8) != 0) {
        flash_color[0] = brightness * *(float *)(met + 0xac);
        flash_color[1] = brightness * *(float *)(met + 0xb0);
        flash_color[2] = brightness * *(float *)(met + 0xb4);
        px3_alpha = *(float *)(met + 0xbc);
        px1 = real_a_rgb_color_to_pixel32(gradient, (float *)(met + 0x7c));
        px2 = real_a_rgb_color_to_pixel32(inv_flash, (float *)(met + 0x88));
        px3 = real_a_rgb_color_to_pixel32(px3_alpha, (float *)(met + 0x94));
        px4 = real_a_rgb_color_to_pixel32(met_alpha, tint);
        px_final_alpha = *(float *)(met + 0xb8);
        px_final_color = flash_color;
      } else {
        px1 = real_a_rgb_color_to_pixel32(gradient, (float *)(met + 0x7c));
        px2 = real_a_rgb_color_to_pixel32(inv_flash, (float *)(met + 0x88));
        px3 = real_a_rgb_color_to_pixel32(*(float *)0x2533c0,
                                          (float *)(met + 0x94));
        px4 = real_a_rgb_color_to_pixel32(met_alpha, tint);
        px_final_alpha = brightness;
        px_final_color = (float *)(met + 0xac);
      }
      px_final = real_a_rgb_color_to_pixel32(px_final_alpha, px_final_color);
      rasterizer_set_texture(0, 0, 1, *(int *)(met + 0x58),
                             *(unsigned short *)(grp + 0x10));
      SetTextureStageStateSmart(0, 0xa, 1);
      SetTextureStageStateSmart(0, 0xb, 1);
      SetTextureStageStateSmart(
        0, 0xd, 2 - (int)((*(unsigned char *)(met + 0x28) & 0x10) != 0));
      SetTextureStageStateSmart(
        0, 0xe, 2 - (int)((*(unsigned char *)(met + 0x28) & 0x10) != 0));
      SetTextureStageStateSmart(0, 0xf, 2);
      SetRenderStateSmart(
        0x7f,
        (int)((-(unsigned int)((*(unsigned char *)(met + 0x28) & 2) != 0) &
               0xfffff6ff) +
              0x901));
      SetRenderStateSmart(0x43, 0x10101);
      SetRenderStateSmart(0x3b, 1);
      SetRenderStateSmart(
        0x3e, (int)((~((unsigned int)*(unsigned char *)(met + 0x28) >> 2) & 2) |
                    0x8001));
      SetRenderStateSmart(
        0x3f,
        (int)((-(unsigned int)((*(unsigned char *)(met + 0x28) & 8) != 0) &
               0xffff8301) +
              0x8001));
      SetRenderStateSmart(0x4b, px_final);
      SetRenderStateSmart(0x4a, 0x8006);
      SetRenderStateSmart(0x3c, 0);
      FUN_00178b40(0x16, vertex_type, permutation);
      meter_consts[0] = 1.0f;
      meter_consts[1] = 1.0f;
      meter_consts[2] = 1.0f;
      meter_consts[3] = 1.0f;
      meter_consts[4] = *(float *)(grp + 0x3c);
      meter_consts[5] = 0.0f;
      meter_consts[6] = 0.0f;
      meter_consts[7] = 0.0f;
      meter_consts[8] = 0.0f;
      meter_consts[9] = *(float *)(grp + 0x40);
      meter_consts[10] = 0.0f;
      meter_consts[11] = 0.0f;
      D3DDevice_SetVertexShaderConstant(-0x54, meter_consts, 3);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXSCALE_OFFSET, "
                     "vsh_constants__texscale, VSH_CONSTANTS__TEXSCALE_COUNT)");
      }
      csmemset((void *)0x5a5ac0, 0, 0xf0);
      SetTextureStageStateSmart(0, 0x15, 4);
      *(uint32_t *)0x5a5b28 = 0x20c00;
      *(uint32_t *)0x5a5b74 = 0x20c00;
      *(uint32_t *)0x5a5ae8 = px4;
      *(uint32_t *)0x5a5b10 = px4;
      *(uint32_t *)0x5a5b08 = px2;
      *(uint32_t *)0x5a5b0c = px2;
      *(uint32_t *)0x5a5aec = px1;
      *(uint32_t *)0x5a5af0 = px1;
      *(uint32_t *)0x5a5b98 = 1;
      *(uint32_t *)0x5a5b94 = 0x11104;
      *(uint32_t *)0x5a5ac0 = 0x12081208;
      *(uint32_t *)0x5a5b48 = 0x1120e820;
      *(uint32_t *)0x5a5ac4 = 0x6c200000;
      *(uint32_t *)0x5a5b2c = 0xc0;
      *(uint32_t *)0x5a5b4c = 0x3c011c02;
      *(uint32_t *)0x5a5b78 = 0xc00;
      *(uint32_t *)0x5a5ac8 = 0x820b120;
      *(uint32_t *)0x5a5b30 = 0xc00;
      *(uint32_t *)0x5a5af4 = px3;
      *(uint32_t *)0x5a5b34 = 0x4c00;
      *(uint32_t *)0x5a5b80 = 0x4c00;
      *(uint32_t *)0x5a5b7c = 0xc00;
      *(uint32_t *)0x5a5b50 =
        ((-(unsigned int)((*(unsigned char *)(met + 0x28) & 4) != 0) & 0xe0) +
         2) |
        0xc201c00;
      *(uint32_t *)0x5a5b14 = px_final;
      *(uint32_t *)0x5a5acc = 0x12201120;
      *(uint32_t *)0x5a5b54 = 0xc200120;
      *(uint32_t *)0x5a5ae0 = 0xc180000;
      *(uint32_t *)0x5a5ae4 = 0x1c00;
      if (*(char *)0x3256c3 != 0 && *(short *)0x3256ea != 0) {
        csmemset((void *)0x5a5ac0, 0, 0xf0);
        SetTextureStageStateSmart(0, 0x15, 0);
        SetRenderStateSmart(0x3b, 0);
        *(uint32_t *)0x5a5b98 = 1;
        *(uint32_t *)0x5a5b94 = 1;
        *(uint32_t *)0x5a5ae0 = ((*(short *)0x3256ea < 2) - 1 & 0x10) + 8;
      }
      rasterizer_set_pixel_shader((void *)0x5a5ac0);
      FUN_00174510(grp, 0);
      SetTextureStageStateSmart(0, 0x15, 0);
      break;
    }

    case 10:
      FUN_0016eef0(grp);
      break;

    default:
      error(2, "### ERROR unsupported shader type");
      success = 0;
      break;
    }

    goto next_pass;

  set_shader_and_draw:
    /* shared tail for shader types 1/5/6 (0x17744d) */
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    FUN_00174510(grp, 0);

  next_pass:
    pass = pass + 1;
  } while ((short)pass < 2);

  if ((*grp & 8) != 0 && *(short *)0x5a5bc0 == 0) {
    rasterizer_set_frustum_z(*(float *)0x2533c0, *(float *)0x2533c0);
  }
  if ((char)*grp < 0 && *(short *)(grp + 0x14) == 1) {
    rasterizer_set_frustum_z(*(float *)0x2533c0, *(float *)0x2533c0);
  }

tail:
  if ((char)dirty == 0) {
    *(int *)0x47e4b8 = *(int *)(grp + 8);
  }
  if (*(short *)(grp + 0x96) != -1) {
    rasterizer_transparent_geometry_group_draw(
      rasterizer_transparent_geometry_group_get(*(short *)(grp + 0x96)), dirty);
  }
  if (draw_secondary != 0) {
    sec = (char *)rasterizer_secondary_geometry_groups_get(&sec_count);
    if ((char)dirty != 0) {
      display_assert("!dirty",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "transparent_geometry.c",
                     0x8e0, 1);
      system_exit(-1);
    }
    for (si = 0; si < sec_count; si++) {
      rec = sec + (int)si * 0xa0;
      if (*(int *)(rec + 0x98) == *(int *)(grp + 8) &&
          *(short *)(rec + 0x14) == 1) {
        rasterizer_transparent_geometry_group_draw(rec, 1);
        if (*(short *)0x3256ea != 0) {
          *(char *)0x47e4c0 = 1;
        }
      }
    }
  }
  if (success == 0) {
    error(2, "### ERROR rasterizer_transparent_geometry_group_draw failed");
  }
}

void rasterizer_frame_begin(float *elapsed)
{
  char val;

  val = *(char *)0x3256c8;
  if (val < 2) {
    *(char *)0x3256d4 = val;
    *(char *)0x3256d3 = val;
    *(char *)0x3256d2 = val;
    *(char *)0x3256d1 = val;
    *(char *)0x3256d0 = val;
    *(char *)0x3256cf = val;
    *(char *)0x3256ce = val;
    *(char *)0x3256cd = val;
    *(char *)0x3256cc = val;
    *(char *)0x3256ca = val;
    *(char *)0x3256cb = val;
    *(char *)0x3256c9 = val;
    *(char *)0x3256d5 = val;
    *(char *)0x3256c8 = 2;
  }
  if (*(float *)0x325694 == *(float *)0x2533c0)
    *(int *)0x325694 = *(int *)0x2af1ac;
  if (*(float *)0x325698 == *(float *)0x2533c0)
    *(int *)0x325698 = *(int *)0x2af1b0;
  if (*(float *)0x32569c == *(float *)0x2533c0)
    *(int *)0x32569c = *(int *)0x2af1b4;
  if (*(float *)0x3256a0 == *(float *)0x2533c0)
    *(int *)0x3256a0 = *(int *)0x2af1b8;
  ((void (*)(float *))0x157940)(elapsed);
}

int rasterizer_windows_begin(void)
{
  return ((int (*)(void))0x1559d0)();
}

static void sanitize_window_screen_flash(window_parameters_t *parameters)
{
  int32_t *flash_type = (int32_t *)((char *)parameters + 0x238);
  float *flash_scale = (float *)((char *)parameters + 0x23c);
  float *flash_color = (float *)((char *)parameters + 0x240);

  if (*flash_type == 0) {
    return;
  }

  if (!(*flash_scale >= 0.0f && *flash_scale <= 1.0f)) {
    *flash_scale = 0.0f;
    *flash_type = 0;
    return;
  }

  if (!(flash_color[0] >= 0.0f && flash_color[0] <= 1.0f &&
        flash_color[1] >= 0.0f && flash_color[1] <= 1.0f &&
        flash_color[2] >= 0.0f && flash_color[2] <= 1.0f &&
        flash_color[3] >= 0.0f && flash_color[3] <= 1.0f)) {
    *flash_type = 0;
    *flash_scale = 0.0f;
    flash_color[0] = 0.0f;
    flash_color[1] = 0.0f;
    flash_color[2] = 0.0f;
    flash_color[3] = 0.0f;
  }
}

int rasterizer_window_begin(window_parameters_t *a1)
{
  sanitize_window_screen_flash(a1);
  return ((int (*)(window_parameters_t *))0x158df0)(a1);
}

void rasterizer_window_end(void)
{
  ((void (*)(void))0x158f90)();
}

void rasterizer_windows_end(void)
{
  ((void (*)(void))0x155a40)();
}

void rasterizer_frame_end(void)
{
  ((void (*)(void))0x155a70)();
}

void rasterizer_set_vblank_callback(void *cb)
{
  ((void (*)(void *))0x155c10)(cb);
}
