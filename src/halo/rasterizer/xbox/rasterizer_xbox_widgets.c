/*
 * rasterizer_xbox_widgets.c
 *
 * Xbox rasterizer widget helpers.
 *
 * Source path (from binary):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_widgets.c
 * (proven by the __FILE__ string at 0x2ae7f0, referenced by the assert at
 *  0x17ae45)
 *
 * Globals (used by address, not in kb.json):
 *   0x3256fc  char  - occlusion-query feature flag (byte, !=0 gate)
 */

/* 0x17adc0 — rasterizer_widget_get_occlusion_test_result
 *
 * Blocking read of a D3D visibility (occlusion) query result.  Issues
 * GetVisibilityTestResult; while the query is still pending
 * (0x88760828 == D3DERR_ ...PENDING) it spins inside a
 * rasterizer_spin_begin(0x1a)/rasterizer_spin_end() bracket.
 *
 * Name evidence: the trailing error() string at 0x2ae788 is
 * "### ERROR rasterizer_widget_get_occlusion_test_result failed".
 *
 * Shape notes (all from disassembly, not the decompiler):
 *   - The 0xffffffff seed of the result local is stored at 0x17adcd, i.e.
 *     BETWEEN the flag TEST and the early-out JE — it is emitted before the
 *     branch even though it is dead on the early-out path.
 *   - The early-out at 0x17ae7b returns the literal 1 (MOV EAX,1), NOT the
 *     0xffffffff sentinel.  Do not merge the two exits.
 *   - Ghidra declared the assert/error blocks unreachable (it treats
 *     display_assert as noreturn) and dropped both the BL success flag and
 *     the trailing error() call.  Both are present in the binary
 *     (BL set at 0x17ae23 / cleared at 0x17ae2d, tested at 0x17ae60).
 *   - The assert tail at 0x17ae56 is system_exit(-1), not
 *     halt_and_catch_fire.
 */
unsigned int rasterizer_widget_get_occlusion_test_result(unsigned int index)
{
  unsigned int occlusion_test_result; /* [EBP-0x4] — also the return value */
  unsigned int timestamp; /* [EBP-0xc] — out param, unused after */
  int hr; /* ESI */
  char ok; /* BL — running success flag */

  occlusion_test_result = 0xffffffff;
  /* nested-if (not `if (!flag) return 1;`) so that the literal-1 exit is laid
   * out as a trailing block reached by a forward JE, matching the original's
   * `je LAB_0017ae7b` + separate `mov eax,1 / leave / ret` tail. */
  if (*(char *)0x3256fc != 0) {
    hr = D3DDevice_GetVisibilityTestResult(index, &occlusion_test_result,
                                           &timestamp);
    if (hr == (int)0x88760828) {
      rasterizer_spin_begin(0x1a);
      /* the two LEAs and the index push are re-done every iteration in the
       * original (the loop head at 0x17ae02 is the first LEA, not the
       * CALL). */
      do {
        hr = D3DDevice_GetVisibilityTestResult(index, &occlusion_test_result,
                                               &timestamp);
      } while (hr == (int)0x88760828);
      rasterizer_spin_end();
    }

    if (hr < 0) {
      ok = 0;
      FUN_00167ff0(hr, "hr");
    } else {
      ok = 1;
    }

    if ((int)occlusion_test_result < 0) {
      display_assert("(occlusion_test_result&0x80000000)==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "widgets.c",
                     0x23e, 1);
      system_exit(-1);
    }
    if (ok == 0) {
      error(2, "### ERROR rasterizer_widget_get_occlusion_test_result failed");
    }
    return occlusion_test_result;
  }
  return 1;
}

/* 0x17ae90 — FUN_0017ae90
 *
 * Builds a transparent-geometry group for a widget: allocates the next group
 * slot (rasterizer_transparent_geometry_group_new, 0x184330), stores the three
 * caller-supplied handles, zero-initialises the group record and computes the
 * signed plane distance of the supplied centroid against the view plane.
 *
 * Signature (from disassembly — the kb decl was the "void(void)" stub, so
 * Ghidra rendered the four cdecl slots as in_stack_XXXX):
 *   [EBP+0x08] arg_4c    -> group+0x4c
 *   [EBP+0x0c] arg_50    -> group+0x50
 *   [EBP+0x10] centroid  (float *, kept in EDI)
 *   [EBP+0x14] arg_48    -> group+0x48, and the non-zero guard at entry
 * Plain RET (no immediate) => __cdecl.  No register arguments.
 *
 * Globals (used by address, not in kb.json):
 *   0x5a5bc8/cc/d0  float  - view reference point (centroid is measured from
 * it) 0x5a5bd4/d8/dc  float  - view plane normal 0x47e4ca        char   -
 * one-shot "already reported" flag for the error()
 *
 * Group record: stride 0xa0 (see rasterizer_transparent_geometry_group_new).
 * Mixed store widths taken from the disassembly, NOT the decompiler:
 *   dword +0x00 +0x04 +0x08 +0x0c +0x44 +0x48 +0x4c +0x50 +0x54 +0x58 +0x5c
 *         +0x60 +0x68 +0x6c +0x80..+0x8c +0x98
 *   float +0x3c +0x40 (1.0f, materialised once in ECX as 0x3f800000)
 *         +0x70 (plane distance) +0x74 +0x78 +0x7c (centroid copy)
 *   word  +0x10 +0x14 +0x64 +0x94 +0x96      byte +0x9d
 * Do not widen the 16-bit fields to int (LOADW hazard).
 *
 * Shape notes:
 *   - group_new() is called BEFORE the centroid NULL check; the assert tail is
 *     display_assert(...) then system_exit(-1) (CALL at 0x17aec9 resolves to
 *     0x8e2f0 in the pristine XBE; Ghidra's delinked reloc mislabels it
 *     FUN_001029a0 / halt_and_catch_fire).
 *   - d0/d1/d2 stay live on the x87 stack across the whole integer-store block
 *     and are popped by three FSTP ST(0) at the end — they are float locals
 *     enregistered in the FPU, not expression temporaries.
 *   - The dot product is emitted right-associated: FLD[bdc]*d2, FLD[bd8]*d1,
 *     FADDP, FLD[bd4]*d0, FADDP, FCHS.
 */
typedef struct {
  float x;
  float y;
  float z;
} rasterizer_widget_point3d;

/* the 16 bytes at group+0x80; zeroed through a stack copy (EBP-0x10..EBP-0x4)
 * in the original, so it is an aggregate assignment, not four direct stores */
typedef struct {
  unsigned int field_0;
  unsigned int field_4;
  unsigned int field_8;
  unsigned int field_c;
} rasterizer_widget_block16;

void FUN_0017ae90(unsigned int arg_4c, unsigned int arg_50, float *centroid,
                  unsigned int arg_48)
{
  char *group;
  /* volatile is required to reproduce the original frame: the original keeps
   * this 16-byte block in the stack slots EBP-0x10..EBP-0x4 (SUB ESP,0x10),
   * zeroes it with four MOVL $0 and only then copies it to group+0x80 through
   * ECX.  Without volatile VC71 constant-folds the copy into four direct zero
   * stores and drops the stack frame entirely (86.1% -> 91.7%). */
  volatile rasterizer_widget_block16 zeros; /* EBP-0x10 .. EBP-0x4 */
  float d0, d1, d2; /* held in ST(2)/ST(1)/ST(0) by the original */

  if (arg_48 != 0) {
    group = (char *)rasterizer_transparent_geometry_group_new();

    if (centroid == (float *)0) {
      display_assert("centroid",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "widgets.c",
                     0x58, 1);
      system_exit(-1);
    }

    if (group != (char *)0) {
      d0 = centroid[0] - *(const float *)0x5a5bc8;
      d1 = centroid[1] - *(const float *)0x5a5bcc;
      d2 = centroid[2] - *(const float *)0x5a5bd0;

      zeros.field_0 = 0;
      zeros.field_4 = 0;
      zeros.field_8 = 0;
      zeros.field_c = 0;

      *(unsigned int *)(group + 0x48) = arg_48;
      *(unsigned int *)(group + 0x4c) = arg_4c;
      *(unsigned int *)(group + 0x50) = arg_50;
      *(int *)(group + 0x00) = 0;
      *(int *)(group + 0x04) = 0;
      *(int *)(group + 0x08) = 0;
      *(int *)(group + 0x0c) = 0;
      *(short *)(group + 0x10) = 0;
      *(short *)(group + 0x14) = 0;
      *(int *)(group + 0x58) = 0;
      *(int *)(group + 0x5c) = 0;
      *(int *)(group + 0x44) = -1;
      *(int *)(group + 0x54) = -1;

      *(float *)(group + 0x70) =
        -(*(const float *)0x5a5bd4 * d0 +
          (*(const float *)0x5a5bd8 * d1 + *(const float *)0x5a5bdc * d2));

      *(rasterizer_widget_point3d *)(group + 0x74) =
        *(rasterizer_widget_point3d *)centroid;
      *(rasterizer_widget_block16 *)(group + 0x80) =
        *(rasterizer_widget_block16 *)&zeros;

      *(int *)(group + 0x98) = 0;
      *(char *)(group + 0x9d) = 0;
      *(int *)(group + 0x60) = 0;
      *(short *)(group + 0x64) = 0;
      *(int *)(group + 0x68) = 0;
      *(int *)(group + 0x6c) = 0;
      *(float *)(group + 0x40) = 1.0f;
      *(float *)(group + 0x3c) = 1.0f;
      *(short *)(group + 0x94) = -1;
      *(short *)(group + 0x96) = -1;
      return;
    }

    if (*(char *)0x47e4ca == 0) {
      error(2, "### ERROR too many transparent geometry groups");
      *(char *)0x47e4ca = 1;
    }
  }
}

/* 0x17b000 — FUN_0017b000
 *
 * Programs the fixed render state + screen-space projection used by the
 * two supported widget draw types.  Two shapes only: type 5 (blended,
 * alpha-tested, caller-controlled Z) and type 6 (opaque, occlusion-flag
 * driven Z).  Anything else trips the assert at line 0x10a.
 *
 * Both branches end by uploading a 5-vector (0x50 byte) constant block to
 * vertex shader constant -0x44 and re-programming the 0xf0-byte pixel
 * shader state block at 0x5a5ac0.  The constant block is a screen-space
 * orthographic projection built from the viewport rectangle at 0x5a5bf4
 * (rectangle2d: {top, left, bottom, right} as int16):
 *
 *   row0 = { 2/w, 0,    0, -1 - 1/w }
 *   row1 = { 0,  -2/h,  0,  1 + 1/h }
 *   row2 = { 0,   0,    1,  0       }
 *   row3 = { 0,   0,    0,  1       }
 *   row4 = { 0,   0,    0,  1       }
 *
 * Confirmed from disassembly:
 *   - two int16 stack params: [EBP+8] MOVSX (signed widget type),
 *     [EBP+0xC] MOVZX (unsigned flag word; bit0 -> ZEnable,
 *     bit1 -> render state 0x4035c).
 *   - 0x2533c8 = 1.0f, 0x255e94 = -1.0f, 0x25eeac = -2.0f (read from XBE).
 *   - FDIVR (not FDIV): the constant is the dividend.
 *   - both asserts tail into system_exit(-1) (PUSH -1; CALL 0x8e2f0),
 *     not halt_and_catch_fire.
 *
 * Uncertain: the meaning of widget types 5/6 and of the render-state
 * shadow globals at 0x1fb7xx; they are written verbatim.
 */
void FUN_0017b000(int16_t widget_type, uint16_t flags)
{
  float vs_constants[20];
  int type;
  short screen_width;
  short screen_height;
  float x_scale;
  float y_scale;
  unsigned int state_value;

  if (*(void **)0x476ab0 == (void *)0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "widgets.c",
                   0x9a, 1);
    system_exit(-1);
  }

  type = widget_type;
  if (type != 5) {
    if (type != 6) {
      display_assert("### ERROR unsupported widget type",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "widgets.c",
                     0x10a, 1);
      system_exit(-1);
      return;
    }

    /* ---- widget type 6: opaque, occlusion-flag driven ---- */
    D3DDevice_SetRenderState_CullMode(0x901);

    state_value = (*(const unsigned char *)0x3256fd != 0) ? 0x10101u : 0u;
    D3DDevice_SetRenderState_Simple(0x40358, state_value);
    *(unsigned int *)0x1fb7a4 = state_value;

    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(unsigned int *)0x1fb784 = 0;

    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(unsigned int *)0x1fb788 = 0;

    D3DDevice_SetRenderState_ZEnable(1);

    D3DDevice_SetRenderState_Simple(0x40354, 0x203);
    *(unsigned int *)0x1fb77c = 0x203;

    state_value = *(const unsigned char *)0x3256fd;
    D3DDevice_SetRenderState_Simple(0x4035c, state_value);
    *(unsigned int *)0x1fb798 = state_value;

    D3DDevice_SetRenderState_ZBias(*(const unsigned int *)0x32570c);

    FUN_00178b40(0x38, 6, 0);

    screen_width = *(const short *)0x5a5bfa - *(const short *)0x5a5bf6;
    screen_height = (short)(*(const int *)0x5a5bf8 - *(const int *)0x5a5bf4);
    x_scale = 1.0f / (float)screen_width;
    vs_constants[0] = x_scale + x_scale;
    vs_constants[1] = 0.0f;
    vs_constants[2] = 0.0f;
    vs_constants[3] = -1.0f - x_scale;
    vs_constants[4] = 0.0f;
    y_scale = 1.0f / (float)screen_height;
    vs_constants[5] = -2.0f * y_scale;
    vs_constants[6] = 0.0f;
    vs_constants[7] = y_scale + 1.0f;
    vs_constants[8] = 0.0f;
    vs_constants[9] = 0.0f;
    vs_constants[10] = 1.0f;
    vs_constants[11] = 0.0f;
    vs_constants[12] = 0.0f;
    vs_constants[13] = 0.0f;
    vs_constants[14] = 0.0f;
    vs_constants[15] = 1.0f;
    vs_constants[16] = 0.0f;
    vs_constants[17] = 0.0f;
    vs_constants[18] = 0.0f;
    vs_constants[19] = 1.0f;

    D3DDevice_SetVertexShaderConstant(-0x44, vs_constants, 5);

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(unsigned int *)0x5a5b94 = 1;
    *(unsigned int *)0x5a5ae0 = 0x20;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    return;
  }

  /* ---- widget type 5: alpha-blended, caller-controlled Z ---- */
  D3DDevice_SetRenderState_CullMode(0x901);

  D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
  *(unsigned int *)0x1fb7a4 = 0x10101;

  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(unsigned int *)0x1fb784 = 1;

  D3DDevice_SetRenderState_Simple(0x40344, 0x302);
  *(unsigned int *)0x1fb790 = 0x302;

  D3DDevice_SetRenderState_Simple(0x40348, 1);
  *(unsigned int *)0x1fb794 = 1;

  D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
  *(unsigned int *)0x1fb7c0 = 0x8006;

  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(unsigned int *)0x1fb788 = 0;

  D3DDevice_SetRenderState_ZEnable(flags & 1);

  D3DDevice_SetRenderState_Simple(0x40354, 0x203);
  *(unsigned int *)0x1fb77c = 0x203;

  state_value = (flags >> 1) & 1;
  D3DDevice_SetRenderState_Simple(0x4035c, state_value);
  *(unsigned int *)0x1fb798 = state_value;

  D3DDevice_SetRenderState_ZBias(0);

  /* PUSH 0x3f800000 — the raw IEEE bit pattern for 1.0f is forwarded as a
   * dword through the int-typed parameter, exactly as the original does. */
  FUN_0017cfe0(0x3f800000);

  FUN_00178b40(0x38, 6, 0);

  screen_width = *(const short *)0x5a5bfa - *(const short *)0x5a5bf6;
  screen_height = (short)(*(const int *)0x5a5bf8 - *(const int *)0x5a5bf4);
  x_scale = 1.0f / (float)screen_width;
  vs_constants[0] = x_scale + x_scale;
  vs_constants[1] = 0.0f;
  vs_constants[2] = 0.0f;
  vs_constants[3] = -1.0f - x_scale;
  vs_constants[4] = 0.0f;
  y_scale = 1.0f / (float)screen_height;
  vs_constants[5] = -2.0f * y_scale;
  vs_constants[6] = 0.0f;
  vs_constants[7] = y_scale + 1.0f;
  vs_constants[8] = 0.0f;
  vs_constants[9] = 0.0f;
  vs_constants[10] = 1.0f;
  vs_constants[11] = 0.0f;
  vs_constants[12] = 0.0f;
  vs_constants[13] = 0.0f;
  vs_constants[14] = 0.0f;
  vs_constants[15] = 1.0f;
  vs_constants[16] = 0.0f;
  vs_constants[17] = 0.0f;
  vs_constants[18] = 0.0f;
  vs_constants[19] = 1.0f;

  D3DDevice_SetVertexShaderConstant(-0x44, vs_constants, 5);

  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(unsigned int *)0x5a5b98 = 1;
  *(unsigned int *)0x5a5b94 = 3;
  *(unsigned int *)0x5a5b48 = 0x8080000;
  *(unsigned int *)0x5a5b74 = 0xc0;
  *(unsigned int *)0x5a5b4c = 0xc0c0000;
  *(unsigned int *)0x5a5b78 = 0xd0;
  *(unsigned int *)0x5a5b50 = 0x4082415;
  *(unsigned int *)0x5a5b7c = 0x45;
  *(unsigned int *)0x5a5ae0 = 0x50f0004;
  *(unsigned int *)0x5a5ae4 = 0xc0d1400;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
}

/* 0x17b480 — FUN_0017b480
 *
 * Binds a bitmap to a texture stage for widget drawing.  If the bind fails,
 * the stage's addressing and filtering states are forced to a fixed
 * configuration before returning, so the caller cannot sample the failed
 * texture with whatever sampler state was left over.
 *
 * A bitmap_index of -1 selects the "direct" bind path and takes the tag index
 * from the object at global 0x476204 (a pointer; the index lives at +0x6c).
 * Any other value goes through the tag-driven path with bitmap_type 1.
 *
 * Signature (from disassembly — the kb decl was `char FUN_0017b480(int, int,
 * short)` and Ghidra dropped all three cdecl slots, rendering them as
 * in_stack_XXXX):
 *   [EBP+0x08] stage         -> first arg of both bind calls
 *   [EBP+0x0c] bitmap_index  -> the -1 discriminator
 *   [EBP+0x10] frame_index   -> last arg of both bind calls
 * All three are read as full DWORDs (MOV EAX/ECX/EDX, dword ptr) at
 * 0x17b4ac / 0x17b4b5 / 0x17b4b8 / 0x17b4cc, so the kb decl's `short` third
 * parameter was not supported by the binary and has been widened to int.
 * Plain RET, no immediate => __cdecl; only EBX is saved (0x17b4b2/0x17b534).
 *
 * Parameter names are taken from the blocking siblings' kb declarations
 * (rasterizer_set_texture_direct @0x155cf0, rasterizer_set_texture @0x155e80),
 * whose argument lists these two calls match slot for slot.  Both callees were
 * declared `void(void)` in kb.json; the call sites push 3 (ADD ESP,0xc) and 5
 * (ADD ESP,0x14) dwords respectively and consume AL, so both decls were
 * corrected as part of this lift.
 *
 * Shape notes (all from disassembly, not the decompiler):
 *   - The BL flag at 0x17b4e6 is BOTH the branch predicate (TEST BL,BL /
 *     JNZ 0x17b532) and the return value (MOV AL,BL at 0x17b532).  There is a
 *     single exit; do not split it.  Ghidra lost this because it modelled both
 *     bind calls as returning void (extraout_AL is the real return).
 *   - `MOV EAX,[EBP+0xc]` at 0x17b4ac (loaded for the CMP) is reused directly
 *     as the bitmap_index argument at 0x17b4bc — it is not reloaded.  On the
 *     direct path EAX holds two different values in four instructions
 *     (frame_index at 0x17b4cc, then stage at 0x17b4d9); each PUSH was traced
 *     back separately rather than assumed to be one variable.
 *   - The assert tail at 0x17b4a4 is system_exit(-1) (CALL 0x8e2f0), not
 *     halt_and_catch_fire; the merged ADD ESP,0x14 at 0x17b4a9 covers both the
 *     4 display_assert args and the 1 system_exit arg.
 *   - The five stage-state calls all use stage 0 (XOR ECX,ECX), not the
 *     `stage` parameter.  States 0xa/0xb are D3DTSS_ADDRESSU/ADDRESSV and
 *     0xd/0xe/0xf are D3DTSS_MAG/MIN/MIPFILTER; the values (4,4,2,2,2) are
 *     reproduced verbatim.  No D3DTSS_* enum exists in the project headers
 *     yet, and the sibling call sites (interface/progress_bar.c) also use the
 *     raw numbers.
 *
 * Uncertain: the identity of the object at global 0x476204 and the meaning of
 * its +0x6c field (used here as a bitmap tag index, unnamed in the binary).
 */
char FUN_0017b480(int stage, int bitmap_index, int frame_index)
{
  char ok; /* BL — bind result: branch predicate and return value */

  if (*(void **)0x476ab0 == (void *)0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "widgets.c",
                   0x11d, 1);
    system_exit(-1);
  }

  /* Tested as `!= -1` (not `== -1`) so that the tag-driven bind is the
   * fall-through block and the direct bind is the branch target, matching the
   * original's `cmp eax,-1 / je LAB_0017b4cc` layout at 0x17b4af. */
  if (bitmap_index != -1) {
    ok = rasterizer_set_texture_non_blocking(stage, 0, 1, bitmap_index,
                                             frame_index);
  } else {
    ok = rasterizer_set_texture_direct_non_blocking(
      stage, *(const int *)(*(const char **)0x476204 + 0x6c), frame_index);
  }

  if (ok == 0) {
    D3DDevice_SetTextureStageState(0, 0xa, 4); /* D3DTSS_ADDRESSU */
    D3DDevice_SetTextureStageState(0, 0xb, 4); /* D3DTSS_ADDRESSV */
    D3DDevice_SetTextureStageState(0, 0xd, 2); /* D3DTSS_MAGFILTER */
    D3DDevice_SetTextureStageState(0, 0xe, 2); /* D3DTSS_MINFILTER */
    D3DDevice_SetTextureStageState(0, 0xf, 2); /* D3DTSS_MIPFILTER */
  }

  return ok;
}
