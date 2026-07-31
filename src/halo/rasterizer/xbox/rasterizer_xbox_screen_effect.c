/*
 * rasterizer_xbox_screen_effect.c
 *
 * Full-screen screen-effect (flash / tint / fade) draw for the Xbox
 * rasterizer, lifted from cachebeta.xbe.
 *
 * Source path (from binary, asserts at 0x312 and 0x36b):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_screen_effect.c
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  void *  - global_d3d_device (IDirect3DDevice8 pointer)
 *   0x3256ff  char    - screen-effect enable flag (byte, != 0 gate)
 *   0x5a5df8  short   - screen flash type selector, 1..6; 0 = no effect
 *                       (read word-sized: CMP word ptr [0x5a5df8],0)
 *   0x5a5dfc  float   - effect intensity, scales every colour channel
 *   0x5a5e00  float   - effect colour component 0 (shared by both ARGB sets)
 *   0x5a5e04  float   - effect colour component 1
 *   0x5a5e08  float   - effect colour component 2
 *   0x5a5e0c  float   - effect colour component 3
 *   0x5a5bf4  int     - viewport rect origin  (lo 16 = x, hi 16 = y)
 *   0x5a5bf8  int     - viewport rect extent  (lo 16 = x, hi 16 = y)
 *   0x5a5ac0  -       - 0xf0-byte pixel-shader state block
 *   0x1fb784  ulong   - cached D3D render-state mirror for 0x40304
 *   0x1fb790  ulong   - cached D3D render-state mirror for 0x40344
 *   0x1fb794  ulong   - cached D3D render-state mirror for 0x40348
 *   0x1fb7a4  ulong   - cached D3D render-state mirror for 0x40358
 *   0x1fb7c0  ulong   - cached D3D render-state mirror for 0x40350
 *   0x1fb7c4  ulong   - cached D3D render-state mirror for 0x4034c
 *   0x2533c8  float   -  1.0f
 *   0x255e94  float   - -1.0f
 *   0x25eeac  float   - texture-coordinate scale constant
 */

/* 0x1700d0
 *
 * FUN_001700d0
 *
 * Componentwise reciprocal of a 2D vector: returns {1/v->i, 1/v->j}.
 *
 * Returns the pair BY VALUE in EAX:EDX. This is the part Ghidra drops
 * entirely -- it renders the function `void FUN_001700d0(void)` -- but the
 * tail is unambiguous: the two quotients are spilled with
 * FSTP [EBP-0x8] @0017013b and FSTP [EBP-0x4] @0017014a, then reloaded as
 * integers into MOV EAX,[EBP-0x8] @00170147 and MOV EDX,[EBP-0x4] @0017014d.
 * That is exactly the MSVC 8-byte POD return convention, and clang with
 * -target i386-pc-win32 emits the same pair of loads for a returned
 * {float,float}, so the struct return is faithful on both compilers.
 *
 * `v` arrives in ESI (@<esi>): TEST ESI,ESI @001700d6 tests it before any
 * write to ESI, and all four call sites in FUN_00170440 do LEA ESI,[EBP-N]
 * immediately before the CALL.
 *
 * Both asserts are the system_exit(-1) flavour (PUSH -0x1; CALL 0x0008e2f0
 * @001700ed and @00170129), not halt_and_catch_fire -- Ghidra renders the
 * tail as thunk_FUN_001029a0, which is the wrong helper here.
 *
 * The divides are 1.0f (constant at 0x2533c8) divided by each component;
 * the zero comparisons are against 0.0f at 0x2533c0.
 */
real_vector2d FUN_001700d0(real_vector2d *v)
{
  real_vector2d out;

  if (v == NULL) {
    display_assert(
      "v",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_screen_effect.c",
      0x1e, 1);
    system_exit(-1);
  }
  if (v->i == 0.0f || v->j == 0.0f) {
    display_assert(
      "v->i!=0.0f && v->j!=0.0f",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_screen_effect.c",
      0x1f, 1);
    system_exit(-1);
  }

  out.i = 1.0f / v->i;
  out.j = 1.0f / v->j;
  return out;
}

/* 0x171bc0
 *
 * FUN_00171bc0
 *
 * Draws the full-screen screen effect for this frame.
 *
 * Asserts the D3D device exists (system_exit flavour).  When the effect is
 * enabled (*(char *)0x3256ff != 0) and a flash type is selected
 * (*(short *)0x5a5df8 != 0):
 *   1. Builds two intensity-scaled ARGB colours and converts both to packed
 *      pixels via FUN_000d1c90 - set A is the raw colour, set B inverts
 *      components 1..3 (1.0f - c).  Component 0 is shared by both sets (the
 *      original stores it with a non-popping FST).
 *   2. Programs cull/colour-mask/one common render state, then a per-flash-type
 *      blend configuration (types 1..6) which also selects the two pixel-shader
 *      state words.  An unsupported type asserts and calls system_exit(-1).
 *   3. Uploads five vertex-shader constant registers (20 floats, one
 *      contiguous buffer) derived from the viewport rect.
 *   4. Programs and binds the pixel-shader state block at 0x5a5ac0.
 *   5. Emits one screen-covering quad (D3DDevice_Begin(7) = D3DPT_QUADLIST).
 *
 * The whole body is bracketed by profile section 0x1c.
 */
void FUN_00171bc0(void)
{
  /* One contiguous 80-byte buffer at [EBP-0x74]: SetVertexShaderConstant
   * uploads all five vec4 registers starting at &vs_const[0]. */
  float vs_const[20];
  float color_b[4]; /* [EBP-0x24] - inverted set */
  float color_a[4]; /* [EBP-0x14] - straight set */
  /* [EBP-0x4].  The original reuses this single slot for the packed pixel of
   * colour set B and, after the switch, for the viewport width; keeping one
   * variable preserves the 0x74-byte frame. */
  unsigned long color_b_pixel_then_width;
  unsigned long color_a_pixel; /* ESI */
  unsigned long ps_state_00; /* -> 0x5a5ac0, XOR-zeroed before the switch */
  unsigned long ps_state_88; /* -> 0x5a5b48, XOR-zeroed before the switch */
  int quad_x;
  int quad_y;
  int height;
  float inv_width;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_screen_effect.c",
      0x312, 1);
    system_exit(-1);
  }
  FUN_0016f910(0x1c);
  if (*(char *)0x3256ff != 0 && *(short *)0x5a5df8 != 0) {
    /* Colour set A: each component scaled by the intensity.  Component 0 is
     * duplicated into set B from the same FPU value (FST, no pop). */
    color_a[0] = *(float *)0x5a5e00 * *(float *)0x5a5dfc;
    color_b[0] = color_a[0];
    color_a[1] = *(float *)0x5a5e04 * *(float *)0x5a5dfc;
    color_a[2] = *(float *)0x5a5e08 * *(float *)0x5a5dfc;
    color_a[3] = *(float *)0x5a5e0c * *(float *)0x5a5dfc;
    /* Colour set B inverts components 1..3: FLD 1.0f ; FSUB [component]. */
    color_b[1] = (*(float *)0x2533c8 - *(float *)0x5a5e04) * *(float *)0x5a5dfc;
    color_b[2] = (*(float *)0x2533c8 - *(float *)0x5a5e08) * *(float *)0x5a5dfc;
    color_b[3] = (*(float *)0x2533c8 - *(float *)0x5a5e0c) * *(float *)0x5a5dfc;
    color_a_pixel = FUN_000d1c90(color_a);
    color_b_pixel_then_width = FUN_000d1c90(color_b);

    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGB);
    *(unsigned long *)0x1fb7a4 = NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(0x40304, 1);
    *(unsigned long *)0x1fb784 = 1;

    ps_state_00 = 0;
    ps_state_88 = 0;
    switch (*(short *)0x5a5df8) {
    case 1:
      D3DDevice_SetRenderState_Simple(0x40344, 1);
      *(unsigned long *)0x1fb790 = 1;
      D3DDevice_SetRenderState_Simple(0x40348, 0x303);
      *(unsigned long *)0x1fb794 = 0x303;
      D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
      *(unsigned long *)0x1fb7c0 = 0x8006;
      ps_state_88 = 0x1200000;
      ps_state_00 = 0x11200000;
      break;
    case 2:
      D3DDevice_SetRenderState_Simple(0x40344, 1);
      *(unsigned long *)0x1fb790 = 1;
      D3DDevice_SetRenderState_Simple(0x40348, 1);
      *(unsigned long *)0x1fb794 = 1;
      D3DDevice_SetRenderState_Simple(0x40350, 0x800b);
      *(unsigned long *)0x1fb7c0 = 0x800b;
      ps_state_88 = 0x1200000;
      ps_state_00 = 0x11200000;
      break;
    case 3:
      D3DDevice_SetRenderState_Simple(0x40344, 0x307);
      *(unsigned long *)0x1fb790 = 0x307;
      D3DDevice_SetRenderState_Simple(0x40348, 0x8002);
      *(unsigned long *)0x1fb794 = 0x8002;
      D3DDevice_SetRenderState_Simple(0x40350, 0x8008);
      *(unsigned long *)0x1fb7c0 = 0x8008;
      D3DDevice_SetRenderState_Simple(0x4034c, color_a_pixel);
      *(unsigned long *)0x1fb7c4 = color_a_pixel;
      ps_state_88 = 0x1201140;
      break;
    case 4:
      D3DDevice_SetRenderState_Simple(0x40344, 0x307);
      *(unsigned long *)0x1fb790 = 0x307;
      D3DDevice_SetRenderState_Simple(0x40348, 0x8002);
      *(unsigned long *)0x1fb794 = 0x8002;
      D3DDevice_SetRenderState_Simple(0x40350, 0x8007);
      *(unsigned long *)0x1fb7c0 = 0x8007;
      D3DDevice_SetRenderState_Simple(0x4034c, color_a_pixel);
      *(unsigned long *)0x1fb7c4 = color_a_pixel;
      ps_state_88 = 0x1201120;
      break;
    case 5:
      D3DDevice_SetRenderState_Simple(0x40344, 0x307);
      *(unsigned long *)0x1fb790 = 0x307;
      SetRenderStateSmart(0x3f, 0x8002);
      SetRenderStateSmart(0x4a, 0x8006);
      SetRenderStateSmart(0x4b, (int)color_a_pixel);
      ps_state_88 = 0x1411120;
      break;
    case 6:
      SetRenderStateSmart(0x3e, 1);
      SetRenderStateSmart(0x3f, 0x8002);
      SetRenderStateSmart(0x4a, 0x8006);
      /* Type 6 blends with the inverted colour: the shared pixel-shader
       * colour word below is overwritten with set B. */
      color_a_pixel = color_b_pixel_then_width;
      SetRenderStateSmart(0x4b, (int)color_b_pixel_then_width);
      ps_state_88 = 0x11200000;
      break;
    default:
      display_assert(
        "### ERROR unsupported screen flash type",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_screen_effect.c",
        0x36b, 1);
      /* system_exit(-1), not halt_and_catch_fire: the pristine XBE calls
       * 0x8e2f0 twice in this function and never 0x1029a0, and the delinked
       * reference emits "PUSH -1" before the tail call here. */
      system_exit(-1);
    }
    SetRenderStateSmart(0x3c, 0);
    SetRenderStateSmart(0x7b, 0);
    D3DDevice_SetRenderState_ZBias(0);
    FUN_00178b40(4, 8, 0);

    /* Viewport width from the 32-bit rect fields, low 16 bits sign-extended;
     * height from the 16-bit .hi (y) fields. */
    color_b_pixel_then_width =
      (unsigned long)(short)(*(int *)0x5a5bf8 - *(int *)0x5a5bf4);
    height = (short)(*(short *)0x5a5bfa - *(short *)0x5a5bf6);

    vs_const[3] = *(float *)0x2533c8 / (float)height;
    vs_const[1] = 0.0f;
    vs_const[2] = 0.0f;
    vs_const[4] = 0.0f;
    vs_const[6] = 0.0f;
    vs_const[8] = 0.0f;
    vs_const[9] = 0.0f;
    vs_const[10] = 0.0f;
    vs_const[11] = 0.5f;
    vs_const[12] = 0.0f;
    vs_const[13] = 0.0f;
    vs_const[14] = 0.0f;
    vs_const[15] = 1.0f;
    vs_const[16] = 0.0f;
    vs_const[17] = 0.0f;
    vs_const[18] = 0.0f;
    vs_const[19] = 1.0f;
    vs_const[0] = vs_const[3] + vs_const[3];
    vs_const[3] = *(float *)0x255e94 - vs_const[3];
    inv_width = *(float *)0x2533c8 / (float)(int)color_b_pixel_then_width;
    vs_const[5] = *(float *)0x25eeac * inv_width;
    vs_const[7] = inv_width + *(float *)0x2533c8;
    D3DDevice_SetVertexShaderConstant(-0x44, vs_const, 5);

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(unsigned long *)0x5a5b94 = 1;
    *(unsigned long *)0x5a5b28 = 0xc00;
    *(unsigned long *)0x5a5b74 = 0xc00;
    *(unsigned long *)0x5a5ae0 = 0xc;
    *(unsigned long *)0x5a5ae4 = 0x1c00;
    *(unsigned long *)0x5a5ac0 = ps_state_00;
    *(unsigned long *)0x5a5ae8 = color_a_pixel;
    *(unsigned long *)0x5a5b48 = ps_state_88;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);

    /* Quad extents: the first coordinate is the sign-extended 16-bit height
     * delta, the second the full 32-bit rect delta - the original uses two
     * different widths here. */
    quad_x = (short)(*(short *)0x5a5bfa - *(short *)0x5a5bf6);
    quad_y = *(int *)0x5a5bf8 - *(int *)0x5a5bf4;
    D3DDevice_Begin(7);
    D3DDevice_SetVertexData2s(0, 0, 0);
    D3DDevice_SetVertexData2s(0, quad_x, 0);
    D3DDevice_SetVertexData2s(0, quad_x, quad_y);
    D3DDevice_SetVertexData2s(0, 0, quad_y);
    D3DDevice_End();
  }
  FUN_0016fa40(0x1c);
}
