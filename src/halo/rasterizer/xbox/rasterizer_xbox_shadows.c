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