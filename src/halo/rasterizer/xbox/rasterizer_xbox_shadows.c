#include "d3d8_states.h"
#include "shader_model.h"

/*
 * rasterizer_xbox_shadows.c
 *
 * Rasterizer Xbox shadow rendering support.
 *
 * Source path (from binary):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_shadows.c
 *
 * Globals: see the block below. Names follow PAL 2342
 * rasterizer_xbox_shadows.c and rasterizer_xbox.c (T2); each is laid onto the
 * 2276 address its use sites read.
 */

/* rasterizer_environment_shadows_globals, shadow_restored,
 * global_pixel_shader, global_window_parameters, rasterizer_debug_options,
 * rasterizer_frame_statistics, global_rasterizer_data, global_d3d_device and
 * D3D__RenderState are kb.json data globals (types in types.h). Real symbols
 * rather than address casts let VC71 schedule loads and stores across them as
 * the original does. */

/* rasterizer_set_stencil_mode argument (PUSH 2 at 0x1735e3). */
#define RASTERIZER_STENCIL_MODE_REJECT 2
/* rasterizer_set_vertex_shader_permutation index (PUSH 0x1d at 0x1733ea). */
#define _shadow_vertex_shader_index 0x1d

#define _rasterizer_statistics_mode_enabled 2

/* PAL 2342 dot_product3d_inline. The original sums x, z, y ((x + z) + y) at all
 * three call sites in _rasterizer_environment_shadow_draw. VC71 here always
 * emits a higher-addressed term first, so no source order reproduces that;
 * this order matches the most of it (position operand first, z in the
 * middle). */
static __inline real dot_product3d_inline(const real_point3d *a,
                                   const real_vector3d *b)
{
  return a->x * b->i + a->z * b->k + a->y * b->j;
}

/* 0x172590
 *
 * FUN_00172590
 *
 * Begins/sets the per-frame shadow rendering parameters.
 *
 * Asserts the D3D device exists. When the window renders to the primary
 * target and environment shadows are enabled:
 *   1. Asserts the supplied parameters pointer is non-null.
 *   2. Programs model skinning from the parameter block at param+8.
 *   3. Stashes the parameters pointer (local_parameters) and sets shadow_used.
 *   4. With statistics enabled, bumps model_shadow_count.
 *
 * param_1: pointer to the shadow parameter block.
 */
void FUN_00172590(int param_1)
{
  if (global_d3d_device == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0xef, 1);
    system_exit(-1);
  }
  if (global_window_parameters.rasterizer_target ==
        _rasterizer_target_render_primary &&
      rasterizer_debug_options.draw_environment_shadows != 0) {
    if (param_1 == 0) {
      display_assert(
        "parameters",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0xf5,
        1);
      system_exit(-1);
    }
    rasterizer_set_model_skinning((void *)(param_1 + 8));
    rasterizer_environment_shadows_globals.local_parameters = (void *)param_1;
    rasterizer_environment_shadows_globals.shadow_used = 1;
    if (rasterizer_debug_options.statistics_mode ==
          _rasterizer_statistics_mode_enabled) {
      rasterizer_frame_statistics.model_shadow_count++;
    }
  }
}

/* 0x172730
 *
 * FUN_00172730
 *
 * Composites the shadow accumulation buffer with a four-tap diagonal blur.
 *
 * Asserts the D3D device exists.  When environment shadows and shadow
 * convolution are both enabled:
 *   1. Binds the shadow_primary target into texture stages 0..3 (border
 *      addressing, linear mag/min, point mip).
 *   2. Cull CCW; RGB colour writes, alpha blend and alpha test off; Z buffer
 *      and Z bias off.
 *   3. Uploads eight vertex-shader constant rows at register -0x51.  Each
 *      row is a texture-coordinate generation vector offset by +/- 1/256
 *      (0x3b800000) in x or y — the four diagonal taps of the blur, each
 *      emitted twice.
 *   4. Zero-fills global_pixel_shader, sets the seven combiner/mask dwords,
 *      and installs it; then renders into shadow_secondary.
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

  if (global_d3d_device == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x1f, 1);
    system_exit(-1);
  }
  if (rasterizer_debug_options.draw_environment_shadows != 0 &&
      rasterizer_debug_options.shadows_convolution != 0) {
    for (stage = 0; (stage - 1) < (4 - 1); stage++) {
      FUN_001584f0(stage, 2, 0);
      D3DDevice_SetTextureStageState(stage, D3DTSS_ADDRESSU,
                                     D3DTADDRESS_BORDER);
      D3DDevice_SetTextureStageState(stage, D3DTSS_ADDRESSV,
                                     D3DTADDRESS_BORDER);
      D3DDevice_SetTextureStageState(stage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(stage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(stage, D3DTSS_MIPFILTER, D3DTEXF_POINT);
    }
    D3DDevice_SetRenderState_CullMode(D3DCULL_CCW);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGB);
    D3D__RenderState[D3DRS_COLORWRITEENABLE] = NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_ENABLE_CMD, 0);
    D3D__RenderState[D3DRS_ALPHABLENDENABLE] = 0;
    D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_TEST_ENABLE_CMD, 0);
    D3D__RenderState[D3DRS_ALPHATESTENABLE] = 0;
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
    csmemset(&global_pixel_shader, 0, sizeof(global_pixel_shader));
    global_pixel_shader.texture_modes = 0x8421;
    global_pixel_shader.combiner_count = 1;
    global_pixel_shader.alpha_inputs[0] = 0x8a009a0;
    global_pixel_shader.alpha_outputs[0] = 0x30c00;
    global_pixel_shader.rgb_inputs[0] = 0xaa00ba0;
    global_pixel_shader.rgb_outputs[0] = 0x30c00;
    global_pixel_shader.final_combiner_inputs_abcd = 0xc20001c;
    rasterizer_set_pixel_shader(&global_pixel_shader);
    rasterizer_set_target(_rasterizer_target_shadow_secondary, 0, 0, 0, 0);
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
 * Asserts the D3D device exists.  When the window renders to the primary
 * target, environment shadows are enabled, and the shader is of type 4
 * (shader->base.type at +0x24, _shader_type_model):
 *   1. Resolves the type-4 shader data block via shader_get_and_verify_type(shader, _shader_type_model).
 *   2. Selects the cull mode from shader-data flag bit 1 (+0x28):
 *      clear -> D3DCULL_CCW, set -> D3DCULL_NONE.
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
 *   6. Emits the indexed draw via rasterizer_draw_static_triangles_static_vertices.
 *   7. With statistics enabled, bumps the three model_shadow_* counters.
 *
 * shader:          shader tag block (base.type at +0x24 must be _shader_type_model = 4).
 * frame_index:     animation frame index passed through to the texture bind.
 * triangle_buffer: index/triangle buffer; triangle count at +0x04.
 * vertex_buffer:   vertex buffer; 16-bit render-state operand at +0x00.
 */
void FUN_00172de0(void *shader, int frame_index, void *triangle_buffer,
                  void *vertex_buffer)
{
  shader_model *shader_data;
  shadow_parameters *parameters;
  /* One contiguous 0x30-byte block: SetVertexShaderConstant uploads all
   * three vec4s starting at &shader_constants[0]. */
  float shader_constants[12];

  if (global_d3d_device == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x112,
      1);
    system_exit(-1);
  }
  if (global_window_parameters.rasterizer_target ==
        _rasterizer_target_render_primary &&
      rasterizer_debug_options.draw_environment_shadows != 0) {
    if (shader == 0) {
      display_assert(
        "shader",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x118,
        1);
      system_exit(-1);
    }
    if (((const shader_model *)shader)->base.type == _shader_type_model) {
      shader_data = (shader_model *)shader_get_and_verify_type(shader, _shader_type_model);
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
      if (rasterizer_environment_shadows_globals.local_parameters == 0) {
        display_assert(
          "local_parameters",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c",
          0x120, 1);
        system_exit(-1);
      }
      D3DDevice_SetRenderState_CullMode(
        (shader_data->field_28 & 2) != 0 ? D3DCULL_NONE
                                                        : D3DCULL_CCW);
      /* Zero-extended 16-bit operand read from the head of the vertex buffer
       * (XOR EAX,EAX; MOV AX,[EBX]). */
      FUN_00178b40(0x27, (int)*(unsigned short *)vertex_buffer, 0);
      if ((shader_data->field_28 & 4) != 0) {
        D3DDevice_SetRenderState_PSTextureModes(0);
      } else {
        D3DDevice_SetRenderState_PSTextureModes(1);
        rasterizer_set_texture(0, 0, 1, shader_data->field_b0,
                               frame_index);
        D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
        D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
        D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
        D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
        D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
      }
      shader_constants[0] = shader_data->field_d8;
      shader_constants[1] =
        shader_data->field_ec * shader_data->field_d8;
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
      parameters = (shadow_parameters *)rasterizer_environment_shadows_globals.local_parameters;
      FUN_00190e10(
        &shader_data->field_fc, &parameters->field_84,
        parameters->field_c4 * shader_data->field_9c,
        parameters->field_c8 * shader_data->field_a0, 0.0f,
        0.0f, 0.0f, global_frame_parameters.game_time_sec, &shader_constants[4],
        &shader_constants[8]);
      D3DDevice_SetVertexShaderConstant(-0x54, shader_constants, 3);
      rasterizer_draw_static_triangles_static_vertices(triangle_buffer, 0, *(int *)((char *)triangle_buffer + 4),
                   vertex_buffer);
      if (rasterizer_debug_options.statistics_mode ==
          _rasterizer_statistics_mode_enabled) {
        rasterizer_frame_statistics.model_shadow_draw_count++;
        rasterizer_frame_statistics.model_shadow_triangle_count +=
          *(int *)((char *)triangle_buffer + 4);
        rasterizer_frame_statistics.model_shadow_vertex_count +=
          FUN_0017ed90(triangle_buffer, vertex_buffer);
      }
    }
  }
}

/*
 * _rasterizer_environment_shadow_draw (0x173090) — draw one batch of
 * stencil-shadow geometry, performing the one-time render-state /
 * pixel-shader / vertex-shader-constant setup on the first batch of a shadow.
 *
 * Name: PAL 2342 __rasterizer_environment_shadow_draw (T2); the same assert
 * line 404 (0x194) of c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_shadows.c.
 *
 * Ghidra reports `void _rasterizer_environment_shadow_draw(void)` and loses every parameter: the six
 * cdecl arguments are read straight off the frame — [EBP+8] shader,
 * [EBP+0xc] bitmap_index (never referenced), [EBP+0x10]/[EBP+0x14]/[EBP+0x18]
 * forwarded to rasterizer_draw_dynamic_triangles_static_vertices and the
 * statistics counter, and [EBP+0x1c] the vertex buffer whose type feeds
 * FUN_00178b40 zero-extended (XOR EAX,EAX; MOV AX,[ESI] @0x1733e4).
 *
 * shadow_setup is the per-shadow latch: everything between its test and the
 * store of 1 at 0x1735dc runs only on the first batch.
 *
 * The vertex-shader constant block is one contiguous 20-float array at
 * EBP-0x58 (SUB ESP,0x58 = 80 bytes of constants plus the two scratch floats
 * below), uploaded as five vec4s at register -0x51.  Three reciprocals of the
 * object bounding radius scale it: 1/radius, 1/(radius*4) and
 * 1/(radius*0.5); VC71 keeps the first two on the x87 stack (FMUL ST2 /
 * FMUL ST1) and spills only the third, which is why only one of the three
 * has a frame slot.
 *
 * The residual FPU-WARN lines (candidate FLD local / FMUL global where the
 * reference is FLD global / FMUL local) are not source-addressable: VC71
 * canonicalises commutative FMUL operands and always loads the local first.
 * Writing `inv_half_range * up.x` instead was measured codegen-identical.
 * They cost operand-normalised score only.
 */
void _rasterizer_environment_shadow_draw(void *shader, short bitmap_index,
                                         int dynamic_triangle_buffer_index,
                                         int first_triangle_index,
                                         int triangle_count,
                                         const vertex_buffer *vertex_buffer)
{
  /* One contiguous 0x50-byte block: SetVertexShaderConstant uploads all five
   * vec4s starting at &shader_constants[0]. */
  float shader_constants[20];
  float dot;             /* [EBP-8], FST (not FSTP) -- reused twice below */
  float inv_half_range;  /* [EBP-4] */
  float inv_range;
  float inv_range_scaled;

  (void)bitmap_index; /* [EBP+0xc] is never referenced by the original */

  if (global_d3d_device == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x194,
      1);
    system_exit(-1);
  }
  if (global_window_parameters.rasterizer_target ==
        _rasterizer_target_render_primary &&
      rasterizer_debug_options.draw_environment_shadows != 0) {
    if (rasterizer_environment_shadows_globals.shadow_setup == 0) {
      if (rasterizer_debug_options.shadows_convolution != 0) {
        FUN_00172730();
      }
      FUN_001584f0(0, (rasterizer_debug_options.shadows_convolution != 0) + 2, 0);
      D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_BORDER);
      D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_BORDER);
      D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

      rasterizer_set_texture_direct(
        1, global_rasterizer_data->linear_corner_fade.tag_index, 0);
      D3DDevice_SetTextureStageState(1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
      D3DDevice_SetTextureStageState(1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
      D3DDevice_SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

      rasterizer_set_texture_direct(
        2, global_rasterizer_data->vector_normalization.tag_index, 0);
      D3DDevice_SetTextureStageState(2, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
      D3DDevice_SetTextureStageState(2, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
      D3DDevice_SetTextureStageState(2, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP);
      D3DDevice_SetTextureStageState(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(2, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(2, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

      /* Each simple render state is the XDK inline: the NV097 method write,
       * then the D3D__RenderState cache store. */
      D3DDevice_SetRenderState_CullMode(D3DCULL_CCW);
      D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                      NV097_COLOR_MASK_RGBA);
      D3D__RenderState[D3DRS_COLORWRITEENABLE] = NV097_COLOR_MASK_RGBA;
      D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_ENABLE_CMD, 1);
      D3D__RenderState[D3DRS_ALPHABLENDENABLE] = 1;
      D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_SFACTOR_CMD,
                                      D3DBLEND_ZERO);
      D3D__RenderState[D3DRS_SRCBLEND] = D3DBLEND_ZERO;
      D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_DFACTOR_CMD,
                                      D3DBLEND_INVSRCCOLOR);
      D3D__RenderState[D3DRS_DESTBLEND] = D3DBLEND_INVSRCCOLOR;
      D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_EQUATION_CMD,
                                      D3DBLENDOP_ADD);
      D3D__RenderState[D3DRS_BLENDOP] = D3DBLENDOP_ADD;
      D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_TEST_ENABLE_CMD, 1);
      D3D__RenderState[D3DRS_ALPHATESTENABLE] = 1;
      D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_REF_CMD, 0);
      D3D__RenderState[D3DRS_ALPHAREF] = 0;
      D3DDevice_SetRenderState_ZEnable(1);
      D3DDevice_SetRenderState_Simple(NV097_SET_DEPTH_FUNC_CMD, D3DCMP_EQUAL);
      D3D__RenderState[D3DRS_ZFUNC] = D3DCMP_EQUAL;
      D3DDevice_SetRenderState_Simple(NV097_SET_DEPTH_MASK_CMD, 0);
      D3D__RenderState[D3DRS_ZWRITEENABLE] = 0;
      D3DDevice_SetRenderState_ZBias(0);

      csmemset(&global_pixel_shader, 0, sizeof(global_pixel_shader));
      global_pixel_shader.texture_modes = 0x21;
      global_pixel_shader.combiner_count = 4;
      global_pixel_shader.constant_0[0] = real_rgb_color_to_pixel32(
        (float *)&rasterizer_environment_shadows_globals.shadow_color);
      global_pixel_shader.constant_1[0] = 0xffffff;
      global_pixel_shader.rgb_inputs[0] = 0x14200000;
      global_pixel_shader.rgb_outputs[0] = 0xc0;
      global_pixel_shader.rgb_inputs[1] = 0x290c0821;
      global_pixel_shader.rgb_outputs[1] = 0xcd;
      global_pixel_shader.rgb_inputs[2] = 0x2c200c2d;
      global_pixel_shader.rgb_outputs[2] = 0xc00;
      global_pixel_shader.rgb_inputs[3] = 0x2c020000;
      global_pixel_shader.rgb_outputs[3] = 0x20d0;
      global_pixel_shader.final_combiner_inputs_abcd = 0x2c;
      global_pixel_shader.final_combiner_inputs_efg = 0xd00;
      if (rasterizer_debug_options.shadows_debug != 0) {
        global_pixel_shader.final_combiner_inputs_abcd = 0xc;
        D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_TEST_ENABLE_CMD, 0);
        D3D__RenderState[D3DRS_ALPHATESTENABLE] = 0;
      }
      rasterizer_set_pixel_shader(&global_pixel_shader);

      /* MSVC evaluates the argument list right to left, which is why the
       * permutation lookup is emitted before the 16-bit vertex-type read. */
      FUN_00178b40(_shadow_vertex_shader_index,
                   (unsigned short)vertex_buffer->type,
                   shader_get_vertex_shader_permutation(shader));

      inv_range =
        1.0f / rasterizer_environment_shadows_globals.object_bounding_radius;
      inv_range_scaled =
        1.0f /
        (rasterizer_environment_shadows_globals.object_bounding_radius * 4.0f);
      inv_half_range =
        1.0f /
        (rasterizer_environment_shadows_globals.object_bounding_radius * 0.5f);

      shader_constants[0] =
        rasterizer_environment_shadows_globals.shadow_matrix.forward.i *
        inv_range * 0.5f;
      shader_constants[1] =
        rasterizer_environment_shadows_globals.shadow_matrix.forward.j *
        inv_range * 0.5f;
      shader_constants[2] =
        rasterizer_environment_shadows_globals.shadow_matrix.forward.k *
        inv_range * 0.5f;
      shader_constants[3] =
        (1.0f -
         dot_product3d_inline(
           &rasterizer_environment_shadows_globals.shadow_matrix.position,
           &rasterizer_environment_shadows_globals.shadow_matrix.forward) *
           inv_range) *
        0.5f;
      shader_constants[4] =
        rasterizer_environment_shadows_globals.shadow_matrix.left.i *
        inv_range * -0.5f;
      shader_constants[5] =
        rasterizer_environment_shadows_globals.shadow_matrix.left.j *
        inv_range * -0.5f;
      shader_constants[6] =
        rasterizer_environment_shadows_globals.shadow_matrix.left.k *
        inv_range * -0.5f;
      shader_constants[7] =
        (dot_product3d_inline(
           &rasterizer_environment_shadows_globals.shadow_matrix.position,
           &rasterizer_environment_shadows_globals.shadow_matrix.left) *
           inv_range +
         1.0f) *
        0.5f;
      shader_constants[8] =
        rasterizer_environment_shadows_globals.shadow_matrix.up.i *
        inv_range_scaled;
      shader_constants[9] =
        rasterizer_environment_shadows_globals.shadow_matrix.up.j *
        inv_range_scaled;
      shader_constants[10] =
        rasterizer_environment_shadows_globals.shadow_matrix.up.k *
        inv_range_scaled;
      shader_constants[16] =
        rasterizer_environment_shadows_globals.shadow_matrix.up.i;
      shader_constants[17] =
        rasterizer_environment_shadows_globals.shadow_matrix.up.j;
      shader_constants[18] =
        rasterizer_environment_shadows_globals.shadow_matrix.up.k;
      shader_constants[19] = 0.0f;
      dot = dot_product3d_inline(
        &rasterizer_environment_shadows_globals.shadow_matrix.position,
        &rasterizer_environment_shadows_globals.shadow_matrix.up);
      shader_constants[11] = -(dot * inv_range_scaled);
      shader_constants[12] =
        -(rasterizer_environment_shadows_globals.shadow_matrix.up.i *
          inv_half_range);
      shader_constants[13] =
        -(rasterizer_environment_shadows_globals.shadow_matrix.up.j *
          inv_half_range);
      shader_constants[14] =
        -(rasterizer_environment_shadows_globals.shadow_matrix.up.k *
          inv_half_range);
      shader_constants[15] = dot * inv_half_range;
      D3DDevice_SetVertexShaderConstant(-0x51, shader_constants, 5);

      if (shadow_restored == 0) {
        /* Zero-extended 16-bit read (XOR EAX,EAX; MOV AX,[0x5a5bc0]). */
        rasterizer_set_target(
          (unsigned short)global_window_parameters.rasterizer_target, 0, 0, 0,
          1);
        shadow_restored = 1;
      }
      rasterizer_environment_shadows_globals.shadow_setup = 1;
    }
    rasterizer_set_stencil_mode(RASTERIZER_STENCIL_MODE_REJECT);
    rasterizer_draw_dynamic_triangles_static_vertices(
      dynamic_triangle_buffer_index, first_triangle_index, triangle_count,
      vertex_buffer);
    if (rasterizer_debug_options.statistics_mode == _rasterizer_statistics_mode_enabled) {
      rasterizer_frame_statistics.shadow_draw_count++;
      rasterizer_frame_statistics.shadow_triangle_count += triangle_count;
      rasterizer_frame_statistics.shadow_vertex_count +=
        rasterizer_frame_statistics_count_static_vertices(
          dynamic_triangle_buffer_index, first_triangle_index, triangle_count);
    }
  }
}

/* 0x1726a0
 *
 * _rasterizer_environment_shadow_end
 *
 * Name: PAL 2342 __rasterizer_environment_shadow_end (T2); the same assert
 * line 563 (0x233) and the same "empty shadow" warning string.
 *
 * Asserts the D3D device exists. When the window renders to the primary
 * target and environment shadows are enabled, warns if the shadow drew no
 * geometry (shadow_used clear), then rebinds the window target once
 * (latched via shadow_restored) with its z-buffer.
 */
void _rasterizer_environment_shadow_end(void)
{
  if (global_d3d_device == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x233,
      1);
    system_exit(-1);
  }
  if (global_window_parameters.rasterizer_target ==
        _rasterizer_target_render_primary &&
      rasterizer_debug_options.draw_environment_shadows != 0) {
    if (rasterizer_environment_shadows_globals.shadow_used == 0) {
      error(2, "### WARNING empty shadow has been cast");
    }
    if (shadow_restored == 0) {
      /* Zero-extended 16-bit read (XOR EAX,EAX; MOV AX,[0x5a5bc0]). */
      rasterizer_set_target(
        (unsigned short)global_window_parameters.rasterizer_target, 0, 0, 0,
        1);
      shadow_restored = 1;
    }
  }
}

/* 0x172a30
 *
 * FUN_00172a30
 *
 * Shadow-pass begin / shadow-generate setup. Programs the D3D render
 * states, pixel shader, and vertex-shader constants for the shadow
 * generation pass, then stashes the shadow projection matrix, RGB color,
 * and object bounding radius into rasterizer_environment_shadows_globals.
 *
 * Asserts the D3D device exists. When the window renders to the primary
 * target and environment shadows are enabled:
 *   1. Validates the matrix/color pointers, each RGB component (in [0,1]),
 *      and the object bounding radius (> 0).
 *   2. Sets cull mode and four simple render states (each mirrored into
 *      D3D__RenderState), disables Z test and Z bias.
 *   3. Clears and programs global_pixel_shader, then binds it.
 *   4. Builds five vertex-shader constant registers - a shadow-projection
 *      transform scaled by 1/radius - and uploads them at register -0x44.
 *   5. Stashes the 13-dword matrix, RGB color, and radius into the shadow
 *      globals, and clears local_parameters, shadow_setup, shadow_used and
 *      shadow_restored.
 *   6. Optionally writes the radius back through out_radius.
 *   7. With statistics enabled, bumps shadow_count.
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
  /* Typed view of the float* shadow_color (ABI unchanged: the callers and
   * kb.json still pass float*); layout proven by the stash below.
   * shadow_matrix stays indexed: it is a real_matrix4x3 (forward = [1..3],
   * left = [4..6], position = [10..12]), but naming the members through a
   * real_matrix4x3 pointer measured 567/692 raw-XBE bytes vs 573/692 indexed. */
  const real_rgb_color *color = (const real_rgb_color *)shadow_color;
  const unsigned long *src;
  unsigned long *dst;
  int i;

  (void)param_1;

  if (global_d3d_device == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x93, 1);
    system_exit(-1);
  }
  if (global_window_parameters.rasterizer_target ==
        _rasterizer_target_render_primary &&
      rasterizer_debug_options.draw_environment_shadows != 0) {
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
    if (!(color->red >= 0.0f) || !(color->red <= 1.0f)) {
      display_assert(
        "shadow_color->red >=0.0f && shadow_color->red <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9b,
        1);
      system_exit(-1);
    }
    if (!(color->green >= 0.0f) || !(color->green <= 1.0f)) {
      display_assert(
        "shadow_color->green>=0.0f && shadow_color->green<=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9c,
        1);
      system_exit(-1);
    }
    if (!(color->blue >= 0.0f) || !(color->blue <= 1.0f)) {
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

    /* Render state: cull, four simple states (mirrored to D3D__RenderState),
     * Z test/bias off. */
    D3DDevice_SetRenderState_CullMode(D3DCULL_CCW);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGB);
    D3D__RenderState[D3DRS_COLORWRITEENABLE] = NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_ENABLE_CMD, 0);
    D3D__RenderState[D3DRS_ALPHABLENDENABLE] = 0;
    D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_TEST_ENABLE_CMD, 1);
    D3D__RenderState[D3DRS_ALPHATESTENABLE] = 1;
    D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_REF_CMD, 0x7f);
    D3D__RenderState[D3DRS_ALPHAREF] = 0x7f;
    D3DDevice_SetRenderState_ZEnable(0);
    D3DDevice_SetRenderState_ZBias(0);

    /* Program and bind the shadow-generation pixel-shader state block. */
    csmemset(&global_pixel_shader, 0, sizeof(global_pixel_shader));
    global_pixel_shader.texture_modes = 1;
    global_pixel_shader.combiner_count = 1;
    global_pixel_shader.final_combiner_inputs_abcd = 0x20;
    global_pixel_shader.final_combiner_inputs_efg = 0x1800;
    rasterizer_set_pixel_shader(&global_pixel_shader);

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

    rasterizer_set_target(_rasterizer_target_shadow_primary, 0,
                          rasterizer_debug_options.shadows_debug != 0
                            ? 0x88888888u
                            : 0u,
                          1, 0);
    rasterizer_set_stencil_mode(0);

    /* Stash the 13-dword matrix, then the RGB color, then the radius. */
    src = (const unsigned long *)shadow_matrix;
    dst = (unsigned long *)&rasterizer_environment_shadows_globals.shadow_matrix;
    for (i = 0xd; i != 0; i--) {
      *dst = *src;
      src++;
      dst++;
    }
    rasterizer_environment_shadows_globals.shadow_color.red = color->red;
    rasterizer_environment_shadows_globals.shadow_color.green = color->green;
    rasterizer_environment_shadows_globals.shadow_color.blue = color->blue;
    rasterizer_environment_shadows_globals.object_bounding_radius = object_bounding_radius;
    if (out_radius != 0) {
      *out_radius = object_bounding_radius;
    }
    rasterizer_environment_shadows_globals.local_parameters = 0;
    rasterizer_environment_shadows_globals.shadow_setup = 0;
    rasterizer_environment_shadows_globals.shadow_used = 0;
    shadow_restored = 0;
    if (rasterizer_debug_options.statistics_mode ==
          _rasterizer_statistics_mode_enabled) {
      rasterizer_frame_statistics.shadow_count++;
    }
  }
  return 1;
}
