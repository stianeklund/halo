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
 *   0x47e458  short     – gate: must be 0 for profiling
 *   0x325180  short     – currently open profile index, -1 = none
 *   0x47e45c  unsigned  – bitmask of profiles completed this frame
 *   0x47e188  dword[2] x 0x1d – per-profile record (8-byte stride), both
 *                        dwords zeroed on begin and on end
 */

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
