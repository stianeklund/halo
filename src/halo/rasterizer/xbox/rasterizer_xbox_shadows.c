/*
 * rasterizer_xbox_shadows.c
 *
 * Rasterizer Xbox shadow rendering support.
 *
 * Source path (from binary):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_shadows.c
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  void *  – global_d3d_device (IDirect3DDevice8 pointer)
 *   0x5a5bc0  short   – render-disable / suppression flag (16-bit, ==0 gate)
 *   0x3256ca  char    – shadow feature enable flag (byte, !=0 gate)
 *   0x3256ba  short   – render mode/pass selector (16-bit, ==2 -> bump counter)
 *   0x47e4b0  void *  – stashed shadow parameters pointer
 *   0x47e4b5  bool    – shadow-parameters-active flag (one-shot set)
 *   0x5a54f4  int     – per-frame counter incremented when mode word == 2
 *   0x5a5e18  float   – animation time fed to the map-animation evaluator
 *   0x5a5500  int     – per-frame shadow draw-call counter (mode word == 2)
 *   0x5a54fc  int     – per-frame shadow triangle counter (mode word == 2)
 *   0x5a54f8  int     – per-frame counter accumulated from FUN_0017ed90
 */

/* 0x172590
 *
 * FUN_00172590
 *
 * Begins/sets the per-frame shadow rendering parameters.
 *
 * Asserts the D3D device exists. When rendering is enabled
 * (*(short *)0x5a5bc0 == 0) and the shadow feature flag is set
 * (*(char *)0x3256ca != 0):
 *   1. Asserts the supplied parameters pointer is non-null.
 *   2. Programs model skinning from the parameter block at param+8.
 *   3. Stashes the parameters pointer and marks shadow params active.
 *   4. If the render-mode word (*(short *)0x3256ba) == 2, increments the
 *      per-frame counter at 0x5a54f4.
 *
 * param_1: pointer to the shadow parameter block.
 */
void FUN_00172590(int param_1)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0xef, 1);
    system_exit(-1);
  }
  if (*(short *)0x5a5bc0 == 0 && *(char *)0x3256ca != 0) {
    if (param_1 == 0) {
      display_assert(
        "parameters",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0xf5,
        1);
      system_exit(-1);
    }
    rasterizer_set_model_skinning((void *)(param_1 + 8));
    *(int *)0x47e4b0 = param_1;
    *(char *)0x47e4b5 = 1;
    if (*(short *)0x3256ba == 2) {
      *(int *)0x5a54f4 = *(int *)0x5a54f4 + 1;
    }
  }
}

/* 0x172730
 *
 * FUN_00172730
 *
 * Composites the shadow accumulation buffer with a four-tap diagonal blur.
 *
 * Asserts the D3D device exists.  When both shadow feature flags are set
 * (*(char *)0x3256ca != 0 and *(char *)0x3256f6 != 0):
 *   1. Binds render target 2 into texture stages 0..3 and programs each
 *      stage's address/filter states (0x0a/0x0b = 4, 0x0d/0x0e = 2,
 *      0x0f = 1).
 *   2. Cull CCW (0x901); colour write mask 0x10101, alpha blend off and
 *      alpha test off (with the shadowed copies of those states updated at
 *      0x1fb7a4 / 0x1fb784 / 0x1fb788); Z buffer and Z bias off.
 *   3. Uploads eight vertex-shader constant rows at register -0x51.  Each
 *      row is a texture-coordinate generation vector offset by +/- 1/256
 *      (0x3b800000) in x or y — the four diagonal taps of the blur, each
 *      emitted twice.
 *   4. Zero-fills the 0xf0-byte pixel-shader state block at 0x5a5ac0, pokes
 *      the seven combiner/mask dwords, and installs it.
 *   5. Draws a full-screen quad (D3DPT_QUADLIST, clockwise) spanning
 *      [-129/128, +127/128] in both axes with texcoords (0,0)..(1,1).
 */
void FUN_00172730(void)
{
  /* One contiguous 0x80-byte block: SetVertexShaderConstant uploads all
   * eight vec4 rows starting at &texture_offsets[0]. */
  float texture_offsets[32];
  /* 16-bit loop counter: the original compares CMP DI,4 (signed word) and
   * carries a separate 32-bit copy in ESI for the D3D stage argument.
   * Measured alternatives, all VC71 vs the delinked reference for this
   * function: plain `stage < 4` 85.6%, explicit two-variable EDI/ESI form
   * 85.6% (178 vs 175 insns), `do { } while (stage < 4)` 85.6%, and the
   * biased condition below 91.7%.  The bias keeps the induction value and
   * the trip counter distinct instead of letting VC71 strength-reduce them
   * into one down-counter (movl $4,%edi / decl %edi / jne), which is what
   * the reference does not do. */
  short stage;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x1f, 1);
    system_exit(-1);
  }
  if (*(char *)0x3256ca != 0 && *(char *)0x3256f6 != 0) {
    for (stage = 0; (stage - 1) < (4 - 1); stage++) {
      FUN_001584f0(stage, 2, 0);
      D3DDevice_SetTextureStageState(stage, 10, 4);
      D3DDevice_SetTextureStageState(stage, 0xb, 4);
      D3DDevice_SetTextureStageState(stage, 0xd, 2);
      D3DDevice_SetTextureStageState(stage, 0xe, 2);
      D3DDevice_SetTextureStageState(stage, 0xf, 1);
    }
    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
    *(int *)0x1fb7a4 = 0x10101;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(int *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(int *)0x1fb788 = 0;
    D3DDevice_SetRenderState_ZEnable(0);
    D3DDevice_SetRenderState_ZBias(0);
    FUN_00178b40(0x26, 8, 0);
    /* 1/256 = 0x3b800000 */
    texture_offsets[0] = 1.0f;
    texture_offsets[1] = 0.0f;
    texture_offsets[2] = 0.0f;
    texture_offsets[3] = -0.00390625f;
    texture_offsets[4] = 0.0f;
    texture_offsets[5] = 1.0f;
    texture_offsets[6] = 0.0f;
    texture_offsets[7] = -0.00390625f;
    texture_offsets[8] = 1.0f;
    texture_offsets[9] = 0.0f;
    texture_offsets[10] = 0.0f;
    texture_offsets[11] = 0.00390625f;
    texture_offsets[12] = 0.0f;
    texture_offsets[13] = 1.0f;
    texture_offsets[14] = 0.0f;
    texture_offsets[15] = 0.00390625f;
    texture_offsets[16] = 1.0f;
    texture_offsets[17] = 0.0f;
    texture_offsets[18] = 0.0f;
    texture_offsets[19] = -0.00390625f;
    texture_offsets[20] = 0.0f;
    texture_offsets[21] = 1.0f;
    texture_offsets[22] = 0.0f;
    texture_offsets[23] = 0.00390625f;
    texture_offsets[24] = 1.0f;
    texture_offsets[25] = 0.0f;
    texture_offsets[26] = 0.0f;
    texture_offsets[27] = 0.00390625f;
    texture_offsets[28] = 0.0f;
    texture_offsets[29] = 1.0f;
    texture_offsets[30] = 0.0f;
    texture_offsets[31] = -0.00390625f;
    D3DDevice_SetVertexShaderConstant(-0x51, texture_offsets, 8);
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(int *)0x5a5b98 = 0x8421;
    *(int *)0x5a5b94 = 1;
    *(int *)0x5a5ac0 = 0x8a009a0;
    *(int *)0x5a5b28 = 0x30c00;
    *(int *)0x5a5b48 = 0xaa00ba0;
    *(int *)0x5a5b74 = 0x30c00;
    *(int *)0x5a5ae0 = 0xc20001c;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    FUN_00158140(3, 0, 0, 0, 0);
    D3DDevice_Begin(7);
    D3DDevice_SetVertexData2s(4, 0, 0);
    D3DDevice_SetVertexData2f(0, -1.0078125f, 1.0078125f);
    D3DDevice_SetVertexData2s(4, 1, 0);
    D3DDevice_SetVertexData2f(0, 0.9921875f, 1.0078125f);
    D3DDevice_SetVertexData2s(4, 1, 1);
    D3DDevice_SetVertexData2f(0, 0.9921875f, -0.9921875f);
    D3DDevice_SetVertexData2s(4, 0, 1);
    D3DDevice_SetVertexData2f(0, -1.0078125f, -0.9921875f);
    D3DDevice_End();
  }
}

/* 0x172de0
 *
 * FUN_00172de0
 *
 * Draws one shadow-projected decal batch.
 *
 * Asserts the D3D device exists.  When rendering is enabled
 * (*(short *)0x5a5bc0 == 0) and the shadow feature flag is set
 * (*(char *)0x3256ca != 0), and the shader is of type 4
 * (shader->type at +0x24):
 *   1. Resolves the type-4 shader data block via FUN_001906b0(shader, 4).
 *   2. Selects the cull mode from shader-data flag bit 1 (+0x28):
 *      clear -> 0x901 (D3DCULL_CCW), set -> 0 (D3DCULL_NONE).
 *   3. Programs render state 0x27 from the 16-bit word at the head of the
 *      vertex buffer.
 *   4. Flag bit 2 clear -> enables one pixel-shader texture stage, binds the
 *      base map (+0xb0) at the requested frame, and programs stage 0
 *      address/filter states.  Flag bit 2 set -> no texture stages.
 *   5. Builds three vertex-shader constant vectors at register -0x54:
 *      c0 = (alpha, alpha * fade, 1, 1) from +0xd8 / +0xec, and c1/c2 which
 *      are the texture-coordinate generation rows produced by the map
 *      animation evaluator FUN_00190e10 (seeded with the identity rows
 *      (1,0,0,0) and (0,1,0,0)).
 *   6. Emits the indexed draw via FUN_0015e430.
 *   7. If the render-mode word (*(short *)0x3256ba) == 2, bumps the three
 *      per-frame statistics counters.
 *
 * shader:          shader tag block (type word at +0x24 must be 4).
 * frame_index:     animation frame index passed through to the texture bind.
 * triangle_buffer: index/triangle buffer; triangle count at +0x04.
 * vertex_buffer:   vertex buffer; 16-bit render-state operand at +0x00.
 */
void FUN_00172de0(void *shader, int frame_index, void *triangle_buffer,
                  void *vertex_buffer)
{
  char *shader_data;
  char *parameters;
  /* One contiguous 0x30-byte block: SetVertexShaderConstant uploads all
   * three vec4s starting at &shader_constants[0]. */
  float shader_constants[12];

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x112,
      1);
    system_exit(-1);
  }
  if (*(short *)0x5a5bc0 == 0 && *(char *)0x3256ca != 0) {
    if (shader == 0) {
      display_assert(
        "shader",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x118,
        1);
      system_exit(-1);
    }
    if (*(short *)((char *)shader + 0x24) == 4) {
      shader_data = (char *)FUN_001906b0(shader, 4);
      if (vertex_buffer == 0) {
        display_assert(
          "vertex_buffer",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c",
          0x11e, 1);
        system_exit(-1);
      }
      if (triangle_buffer == 0) {
        display_assert(
          "triangle_buffer",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c",
          0x11f, 1);
        system_exit(-1);
      }
      if (*(void **)0x47e4b0 == 0) {
        display_assert(
          "local_parameters",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c",
          0x120, 1);
        system_exit(-1);
      }
      D3DDevice_SetRenderState_CullMode(
        (*(unsigned char *)(shader_data + 0x28) & 2) != 0 ? 0 : 0x901);
      /* Zero-extended 16-bit operand read from the head of the vertex buffer
       * (XOR EAX,EAX; MOV AX,[EBX]). */
      FUN_00178b40(0x27, (int)*(unsigned short *)vertex_buffer, 0);
      if ((*(unsigned char *)(shader_data + 0x28) & 4) != 0) {
        D3DDevice_SetRenderState_PSTextureModes(0);
      } else {
        D3DDevice_SetRenderState_PSTextureModes(1);
        rasterizer_set_texture(0, 0, 1, *(int *)(shader_data + 0xb0),
                               frame_index);
        D3DDevice_SetTextureStageState(0, 10, 1);
        D3DDevice_SetTextureStageState(0, 0xb, 1);
        D3DDevice_SetTextureStageState(0, 0xd, 2);
        D3DDevice_SetTextureStageState(0, 0xe, 2);
        D3DDevice_SetTextureStageState(0, 0xf, 2);
      }
      shader_constants[0] = *(float *)(shader_data + 0xd8);
      shader_constants[1] =
        *(float *)(shader_data + 0xec) * *(float *)(shader_data + 0xd8);
      shader_constants[2] = 1.0f;
      shader_constants[3] = 1.0f;
      shader_constants[4] = 1.0f;
      shader_constants[5] = 0.0f;
      shader_constants[6] = 0.0f;
      shader_constants[7] = 0.0f;
      shader_constants[8] = 0.0f;
      shader_constants[9] = 1.0f;
      shader_constants[10] = 0.0f;
      shader_constants[11] = 0.0f;
      parameters = *(char **)0x47e4b0;
      FUN_00190e10(
        shader_data + 0xfc, parameters + 0x84,
        *(float *)(parameters + 0xc4) * *(float *)(shader_data + 0x9c),
        *(float *)(parameters + 0xc8) * *(float *)(shader_data + 0xa0), 0.0f,
        0.0f, 0.0f, *(float *)0x5a5e18, &shader_constants[4],
        &shader_constants[8]);
      D3DDevice_SetVertexShaderConstant(-0x54, shader_constants, 3);
      FUN_0015e430(triangle_buffer, 0, *(int *)((char *)triangle_buffer + 4),
                   vertex_buffer);
      if (*(short *)0x3256ba == 2) {
        *(int *)0x5a5500 = *(int *)0x5a5500 + 1;
        *(int *)0x5a54fc =
          *(int *)0x5a54fc + *(int *)((char *)triangle_buffer + 4);
        *(int *)0x5a54f8 =
          *(int *)0x5a54f8 + FUN_0017ed90(triangle_buffer, vertex_buffer);
      }
    }
  }
}