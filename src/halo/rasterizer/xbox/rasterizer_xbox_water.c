/*
 * rasterizer_xbox_water.c
 *
 * Xbox water ripple bump-map build pass.
 *
 * Source path (from binary):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_water.c
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  void *       - global_d3d_device (IDirect3DDevice8 pointer)
 *   0x3256d6  char         - water bump-map feature flag (byte, !=0 gate)
 *   0x5a5e18  float        - global map-animation time (seconds)
 *   0x25eeac  float        - "mysterious_horizontal_offset" (.rdata const)
 *   0x5a5ac0  byte[0xf0]   - pixel/combiner shader state block
 *   0x5a5ae0  int          - combiner state word (+0x20 of the block)
 *   0x5a5ae4  int          - combiner state word (+0x24)
 *   0x5a5ae8  int          - ripple 0/1 blend alpha, packed <<24  (+0x28)
 *   0x5a5aec  int          - ripple 2/3 blend alpha, packed <<24  (+0x2c)
 *   0x5a5af0  int          - ripple pair blend alpha, packed <<24 (+0x30)
 *   0x5a5b48..0x5a5b98     - combiner/shader words inside the block
 *   0x5a5b6c  int          - per-layer constant color (ARGB)
 *   0x5a5bc0  short        - render-target selector for the final restore
 *   0x1fb784  int          - shadowed D3D render state 0x40304
 *   0x1fb788  int          - shadowed D3D render state 0x40300
 *   0x1fb7a4  int          - shadowed D3D render state 0x40358
 */

#include "x87_math.h"

#define RASTERIZER_XBOX_WATER_FILE \
  "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_water.c"

#define NUMBER_OF_WATER_RIPPLES 4

/* Water ripple tag block element (tag_block at water_shader+0x124,
 * element size 0x4c, proven by the PUSH 0x4c / REP MOVSD ECX=0x13 pair
 * at 0x179662-0x179676). Only the fields the bump-map pass touches are
 * named; everything else stays explicitly unknown. */
typedef struct s_water_ripple {
  char unknown_00[0x04];          /* +0x00 */
  float contribution_factor;      /* +0x04 */
  char unknown_08[0x20];          /* +0x08 */
  float animation_angle;          /* +0x28 */
  float animation_velocity;       /* +0x2c */
  float map_offset_x;             /* +0x30 */
  float map_offset_y;             /* +0x34 */
  short map_repeats;              /* +0x38 */
  unsigned short animation_frame; /* +0x3a */
  char unknown_3c[0x10];          /* +0x3c */
} s_water_ripple;                 /* sizeof == 0x4c */

/* Inlined from ..\bitmaps\bitmaps_inlines.h:0x123 (the assert string and
 * line number are stamped into the original at three call sites here).
 * FISTP, not a truncating (int) cast: the original rounds with the FPU's
 * current rounding mode. */
static __inline int water_alpha_to_pixel32(float alpha_in) {
  /* The original stores the computed alpha once and reloads it from memory
   * at each of its three uses (two range compares + the scale multiply,
   * FSTP [EBP+0x8] / FLD x3 at 0x1799a6..0x1799e9) — volatile forces the
   * same store-once/reload-each-use shape and is the numerically faithful
   * choice (each reload rounds to float). */
  volatile float alpha = alpha_in;
  volatile float scale = 255.0f;
  int packed;
  if (!(alpha >= 0.0f && alpha <= 1.0f)) {
    display_assert("alpha>=0.0f && alpha<=1.0f", "..\\bitmaps\\bitmaps_inlines.h",
                   0x123, 1);
    system_exit(-1);
  }
  /* SHL is applied to the FISTP slot in memory before the load
   * (SHL dword ptr [EBP-0x4],0x18 at 0x1799f4) — keep it a statement on
   * the local, not part of the return expression. */
  packed = x87_round_to_int(alpha * scale);
  packed <<= 24;
  return packed;
}

/* Each D3D call in the draw loop is followed by a check that folds into a
 * running success flag and reports the literal call text on failure. */
#define VERIFY_D3D_CALL(call_text)  \
  do {                              \
    if (success) {                  \
      success = 1;                  \
    } else {                        \
      FUN_00167ff0(0, (call_text)); \
      success = 0;                  \
    }                               \
  } while (0)

/* 0x1795c0
 *
 * FUN_001795c0 - rasterizer_water_build_bumpmap
 *
 * Renders the animated water ripple bump map for a water shader.
 *
 * Gathers up to four ripple definitions from the shader's ripple tag block
 * (missing entries default to zero with map_repeats == 1), programs one
 * texture stage per ripple, uploads a per-ripple 2x4 texture-transform
 * matrix into vertex shader constants -0x51..-0x4a, builds the combiner
 * state block at 0x5a5ac0 with the ripple blend weights packed into the
 * alpha byte of three constants, then draws one full-screen triangle fan
 * per bump-map layer at successively halved scales.
 *
 * shader: shader tag pointer; shader definition 7 is the water definition.
 */
void FUN_001795c0(void *shader) {
  int ripple;
  int gather_count;
  s_water_ripple *gather;
  int stage_index;
  unsigned short *frame_ptr;
  int layer_index;
  float contribution_sum;
  short stage;
  short layer;
  short layer_count;
  short water_layers;
  int bitmap_index;
  char success;
  float cos_angle;
  float sin_angle;
  float animation_offset;
  float alpha;
  float scale;
  float horizontal_offset;
  void *water;
  int *ripple_block;
  s_water_ripple ripples[NUMBER_OF_WATER_RIPPLES];
  float vs_constants[NUMBER_OF_WATER_RIPPLES * 8];

  if (shader == 0) {
    display_assert("shader", RASTERIZER_XBOX_WATER_FILE, 0x2f, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device", RASTERIZER_XBOX_WATER_FILE, 0x30, 1);
    system_exit(-1);
  }
  if (*(char *)0x3256d6 == 0) {
    return;
  }

  water = FUN_001906b0(shader, 7);

  layer_count = NUMBER_OF_WATER_RIPPLES;
  if (*(short *)((char *)water + 0xd8) <= NUMBER_OF_WATER_RIPPLES) {
    layer_count = *(short *)((char *)water + 0xd8);
  }

  /* Down-counted do-while with a pointer walking the gather array: the
   * original keeps the index (ESI), a separate down-counter ([EBP-0x4],
   * DEC/JNZ at 0x179698) and the element pointer (EBX, +0x4c per step). */
  ripple_block = (int *)((char *)water + 0x124);
  ripple = 0;
  gather = ripples;
  gather_count = NUMBER_OF_WATER_RIPPLES;
  do {
    if (ripple < *ripple_block) {
      *gather =
          *(s_water_ripple *)tag_block_get_element(ripple_block, ripple, 0x4c);
    } else {
      csmemset(gather, 0, 0x4c);
      gather->map_repeats = 1;
    }
    ripple++;
    gather++;
  } while (--gather_count != 0);

  /* Compare against the .rdata 0.0f constant the original reads
   * (FCOMP [0x2533c0] at 0x1796a7..0x1796ed); a 0.0f literal makes VC71
   * load the pool zero first and FUCOMPP instead. Same value either way. */
  if (ripples[0].contribution_factor == *(const float *)0x2533c0 &&
      ripples[1].contribution_factor == *(const float *)0x2533c0) {
    ripples[1].contribution_factor = 1.0f;
  }
  if (ripples[2].contribution_factor == *(const float *)0x2533c0 &&
      ripples[3].contribution_factor == *(const float *)0x2533c0) {
    ripples[3].contribution_factor = 1.0f;
  }

  /* The original keeps three parallel induction variables here: the short
   * loop counter (EDI, CMP DI,0x4 at 0x17978f), an int copy for the
   * stage-state register argument (ESI), and a pointer walking the
   * animation_frame field (EBX, +0x4c per step). */
  stage_index = 0;
  frame_ptr = &ripples[0].animation_frame;
  for (stage = 0; stage < NUMBER_OF_WATER_RIPPLES; stage++) {
    if (stage_index < *(int *)((char *)water + 0x124)) {
      bitmap_index = *(int *)((char *)water + 0xd4);
    } else {
      bitmap_index = -1;
    }
    rasterizer_set_texture(stage, 0, 3, bitmap_index, *frame_ptr);
    D3DDevice_SetTextureStageState(stage_index, 10, 1);
    D3DDevice_SetTextureStageState(stage_index, 11, 1);
    D3DDevice_SetTextureStageState(stage_index, 13, 2);
    D3DDevice_SetTextureStageState(stage_index, 14, 2);
    D3DDevice_SetTextureStageState(stage_index, 15, 2);
    stage_index++;
    frame_ptr = (unsigned short *)((char *)frame_ptr + 0x4c);
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

  /* Per-ripple 2x4 texture transform: rows (repeats, 0, 0, u) and
   * (0, repeats, 0, v), scrolled along the ripple's animation angle. */
  for (ripple = 0; ripple < NUMBER_OF_WATER_RIPPLES; ripple++) {
    cos_angle = x87_fcos(ripples[ripple].animation_angle);
    sin_angle = x87_fsin(ripples[ripple].animation_angle);
    if (!(ripples[ripple].map_repeats > 0)) {
      display_assert("ripples[ripple_index].map_repeats>0",
                     RASTERIZER_XBOX_WATER_FILE, 0x7c, 1);
      system_exit(-1);
    }
    /* Operand order written velocity-first so VC71 emits the original's
     * FLD [0x5a5e18]; FMUL [velocity] (it loads the right operand first);
     * a single multiply, so the product is identical either way. */
    animation_offset = ripples[ripple].animation_velocity * *(float *)0x5a5e18;
    vs_constants[ripple * 8 + 0] = (float)ripples[ripple].map_repeats;
    vs_constants[ripple * 8 + 1] = 0.0f;
    vs_constants[ripple * 8 + 2] = 0.0f;
    vs_constants[ripple * 8 + 3] =
        cos_angle * animation_offset + ripples[ripple].map_offset_x;
    vs_constants[ripple * 8 + 4] = 0.0f;
    vs_constants[ripple * 8 + 5] = (float)ripples[ripple].map_repeats;
    vs_constants[ripple * 8 + 6] = 0.0f;
    vs_constants[ripple * 8 + 7] =
        animation_offset * sin_angle + ripples[ripple].map_offset_y;
  }
  D3DDevice_SetVertexShaderConstant(-0x51, vs_constants, 8);

  success = 1;
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  /* The original computes this sum in ST0 interleaved with the combiner
   * stores (FLD/FADD at 0x1798ac before the block, FCOMP mid-block) —
   * hoisting it into a single-assignment temp lets VC71 hold it FPU-
   * resident across the integer stores the same way. */
  contribution_sum =
      ripples[0].contribution_factor + ripples[1].contribution_factor;
  *(int *)0x5a5b74 = 0xc00;
  *(int *)0x5a5b80 = 0xc00;
  *(int *)0x5a5b98 = 0x8421;
  *(int *)0x5a5b94 = 0x11004;
  *(int *)0x5a5b48 = 0x31481149;
  *(int *)0x5a5b4c = 0x314a114b;
  *(int *)0x5a5b78 = 0xd00;
  *(int *)0x5a5b50 = 0x31cc11cd;
  *(int *)0x5a5b7c = 0x30c00;
  *(int *)0x5a5b54 = 0xcc20a020;
  *(int *)0x5a5ae0 = 0x310c0100;
  *(int *)0x5a5ae4 = 0;

  if (!(contribution_sum > 0.0f)) {
    display_assert(
        "ripples[0].contibution_factor + ripples[1].contibution_factor>0.0f",
        RASTERIZER_XBOX_WATER_FILE, 0x9f, 1);
    system_exit(-1);
  }
  if (!(ripples[2].contribution_factor + ripples[3].contribution_factor >
        0.0f)) {
    display_assert(
        "ripples[2].contibution_factor + ripples[3].contibution_factor>0.0f",
        RASTERIZER_XBOX_WATER_FILE, 0xa0, 1);
    system_exit(-1);
  }

  *(int *)0x5a5ae8 = water_alpha_to_pixel32(
      ripples[0].contribution_factor /
      (ripples[0].contribution_factor + ripples[1].contribution_factor));
  *(int *)0x5a5aec = water_alpha_to_pixel32(
      ripples[2].contribution_factor /
      (ripples[2].contribution_factor + ripples[3].contribution_factor));
  *(int *)0x5a5af0 = water_alpha_to_pixel32(
      (ripples[0].contribution_factor + ripples[1].contribution_factor) /
      (ripples[0].contribution_factor + ripples[1].contribution_factor +
       ripples[2].contribution_factor + ripples[3].contribution_factor));

  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  FUN_00158ae0(0);

  /* Dual induction, as the original: the short loop counter ([EBP-0x4],
   * CMP AX vs layer_count at 0x179d8f) plus a separate int copy that is
   * FILDed for the float conversion ([EBP-0x10] at 0x179b30). */
  layer_index = 0;
  for (layer = 0; layer < layer_count; layer++) {
    water_layers = *(short *)((char *)water + 0xd8);
    if (water_layers <= 1) {
      *(int *)0x5a5b6c = 0x7f7fff;
    } else {
      /* float / int-expression: VC71 emits the original's exact FIDIV
       * dword (0x179b41); operands fit in 24 bits so the quotient is
       * identical to converting the divisor to float first. */
      alpha = ((float)layer_index / (water_layers - 1)) *
              *(float *)((char *)water + 0xdc);
      *(int *)0x5a5b6c = water_alpha_to_pixel32(alpha) | 0x8080ff;
    }
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    FUN_00158140(6, layer, 0, 0, 0);

    scale = 1.0f / (float)(0x80 >> layer);
    horizontal_offset = scale * *(float *)0x25eeac;

    D3DDevice_Begin(7);
    VERIFY_D3D_CALL(
        "IDirect3DDevice8_Begin(global_d3d_device, D3DPT_TRIANGLEFAN)");
    D3DDevice_SetVertexData2s(4, 0, 0);
    VERIFY_D3D_CALL(
        "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 0, 0)");
    D3DDevice_SetVertexData2f(0, scale - 1.0f + horizontal_offset,
                              scale + 1.0f);
    VERIFY_D3D_CALL(
        "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
        "scale - 1.0f + mysterious_horizontal_offset, scale + 1.0f)");
    D3DDevice_SetVertexData2s(4, 1, 0);
    VERIFY_D3D_CALL(
        "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 1, 0)");
    D3DDevice_SetVertexData2f(0, scale + 1.0f + horizontal_offset,
                              scale + 1.0f);
    VERIFY_D3D_CALL(
        "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
        "scale + 1.0f + mysterious_horizontal_offset, scale + 1.0f)");
    D3DDevice_SetVertexData2s(4, 1, 1);
    VERIFY_D3D_CALL(
        "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 1, 1)");
    D3DDevice_SetVertexData2f(0, scale + 1.0f + horizontal_offset,
                              scale - 1.0f);
    VERIFY_D3D_CALL(
        "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
        "scale + 1.0f + mysterious_horizontal_offset, scale - 1.0f)");
    D3DDevice_SetVertexData2s(4, 0, 1);
    VERIFY_D3D_CALL(
        "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 0, 1)");
    D3DDevice_SetVertexData2f(0, scale - 1.0f + horizontal_offset,
                              scale - 1.0f);
    VERIFY_D3D_CALL(
        "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
        "scale - 1.0f + mysterious_horizontal_offset, scale - 1.0f)");
    D3DDevice_End();
    VERIFY_D3D_CALL("IDirect3DDevice8_End(global_d3d_device)");
    layer_index++;
  }

  FUN_00158140(*(unsigned short *)0x5a5bc0, 0, 0, 0, 1);
  FUN_00158ae0(2);
  if (!success) {
    error(2, "### ERROR rasterizer_water_build_bumpmap failed");
  }
}

/* 0x179de0
 *
 * FUN_00179de0 - rasterizer_water_draw_group
 *
 * Draws one water render-group.  Up to three passes are emitted, all sharing
 * the same vertex type / vertex-shader permutation:
 *
 *   - a degenerate "depth only" pass, taken when the water shader's
 *     0x28 flag bit 8 is set and the group is neither of the two 0x12 kinds;
 *   - a reflection pass  (shader 0x28 bit 1);
 *   - an environment/second pass (shader 0x28 bit 2);
 *   - the main water pass, which uploads the ripple scroll matrix into
 *     vertex-shader constants -0x54..-0x52 and picks the per-group tint by
 *     lerping shader+0x70..0x78 and shader+0x80..0x88 with the clamped dot
 *     product of the group's vector at +0x80 against the global at 0x5a5bd4.
 *
 * group: render-group record; +0x0 flags, +0xc shader tag, +0x10 frame index,
 *        +0x80 float3.
 */
void FUN_00179de0(void *group) {
  void *shader;
  int permutation;
  int vertex_type;
  unsigned short water_flag;
  unsigned char alpha_blend_enable;
  unsigned int blend_state;
  short mipmap_count;
  int max_mipmap;
  float *v;
  float scroll;
  float weight;
  float inverse_weight;
  float tint[3];
  float constants[12];
  union {
    float f;
    unsigned int u;
  } stage_value;

  if (group == 0) {
    display_assert("group", RASTERIZER_XBOX_WATER_FILE, 0xee, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device", RASTERIZER_XBOX_WATER_FILE, 0xef, 1);
    system_exit(-1);
  }
  if (*(char *)0x3256d6 == 0) {
    return;
  }

  shader = FUN_001906b0(*(void **)((char *)group + 0xc), 7);
  permutation = shader_get_vertex_shader_permutation(*(void **)((char *)group + 0xc));
  vertex_type = FUN_00184610(group);
  /* 16-bit load/AND, as the original: MOV AX,[ESI+0x28]; AND AX,0x8. */
  water_flag = *(unsigned short *)((char *)shader + 0x28);
  water_flag &= 8;

  if (water_flag != 0 && (*(unsigned char *)group & 0x12) == 0) {
    D3DDevice_SetRenderState_CullMode(0);
    D3DDevice_SetRenderState_Simple(0x40358, 0);
    *(int *)0x1fb7a4 = 0;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(int *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(int *)0x1fb788 = 0;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(0x40354, 0x203);
    *(int *)0x1fb77c = 0x203;
    D3DDevice_SetRenderState_Simple(0x4035c, 1);
    *(int *)0x1fb798 = 1;
    FUN_00178b40(0x14, vertex_type, permutation);
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(int *)0x5a5b94 = 1;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    FUN_00174510(group, 0);
    return;
  }

  /* Alpha blending is suppressed for the 0x10 group kind and for the
   * "bump only" water shaders. */
  alpha_blend_enable = 1;
  if ((*(unsigned char *)group & 0x10) != 0 || water_flag != 0) {
    alpha_blend_enable = 0;
  }

  if (*(char *)0x47e4c8 != 0) {
    FUN_001795c0(*(void **)((char *)group + 0xc));
    *(char *)0x47e4c8 = 0;
  }

  if ((*(unsigned char *)((char *)shader + 0x28) & 1) != 0) {
    rasterizer_set_texture(0, 0, 1, *(int *)((char *)shader + 0x58),
                           *(unsigned short *)((char *)group + 0x10));
    D3DDevice_SetTextureStageState(0, 10, 3);
    D3DDevice_SetTextureStageState(0, 0xb, 3);
    D3DDevice_SetTextureStageState(0, 0xd, 2);
    D3DDevice_SetTextureStageState(0, 0xe, 2);
    D3DDevice_SetTextureStageState(0, 0xf, 2);
    rasterizer_set_texture_direct(1, *(int *)(*(char **)0x476204 + 0x1c), 0);
    D3DDevice_SetTextureStageState(1, 10, 3);
    D3DDevice_SetTextureStageState(1, 0xb, 3);
    D3DDevice_SetTextureStageState(1, 0xc, 3);
    D3DDevice_SetTextureStageState(1, 0xd, 2);
    D3DDevice_SetTextureStageState(1, 0xe, 2);
    D3DDevice_SetTextureStageState(1, 0xf, 2);
    D3DDevice_SetRenderState_CullMode(0);
    D3DDevice_SetRenderState_Simple(0x40358, 0x1000000);
    *(int *)0x1fb7a4 = 0x1000000;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(int *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(int *)0x1fb788 = 0;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(0x40354, 0x203);
    *(int *)0x1fb77c = 0x203;
    blend_state = alpha_blend_enable;
    D3DDevice_SetRenderState_Simple(0x4035c, blend_state);
    *(int *)0x1fb798 = blend_state;
    FUN_00178b40(0x14, vertex_type, permutation);
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(int *)0x5a5b98 = 0x61;
    *(int *)0x5a5b94 = 2;
    *(unsigned int *)0x5a5ae8 = FUN_00159070(*(float *)((char *)shader + 0x6c));
    *(unsigned int *)0x5a5b08 = FUN_00159070(*(float *)((char *)shader + 0x7c));
    *(int *)0x5a5ac0 = 0x29120911;
    *(int *)0x5a5b28 = 0xc00;
    *(int *)0x5a5ac4 = 0x1c180000;
    *(int *)0x5a5b2c = 0xc0;
    *(int *)0x5a5ae0 = 0;
    *(int *)0x5a5ae4 = 0x1c00;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    FUN_00174510(group, 0);
  }

  if ((*(unsigned char *)((char *)shader + 0x28) & 2) != 0) {
    rasterizer_set_texture(0, 0, 1, *(int *)((char *)shader + 0x58),
                           *(unsigned short *)((char *)group + 0x10));
    D3DDevice_SetTextureStageState(0, 10, 3);
    D3DDevice_SetTextureStageState(0, 0xb, 3);
    D3DDevice_SetTextureStageState(0, 0xd, 2);
    D3DDevice_SetTextureStageState(0, 0xe, 2);
    D3DDevice_SetTextureStageState(0, 0xf, 2);
    D3DDevice_SetRenderState_CullMode(0);
    SetRenderStateSmart(0x43, 0x10101);
    SetRenderStateSmart(0x3b, 1);
    SetRenderStateSmart(0x3e, 0);
    SetRenderStateSmart(0x3f, 0x300);
    SetRenderStateSmart(0x4a, 0x8006);
    SetRenderStateSmart(0x3c, 0);
    SetRenderStateSmart(0x7b, 1);
    SetRenderStateSmart(0x39, 0x203);
    SetRenderStateSmart(0x40, alpha_blend_enable);
    FUN_00178b40(0x14, vertex_type, permutation);
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(int *)0x5a5b98 = 1;
    *(int *)0x5a5b94 = 1;
    *(int *)0x5a5ae0 =
        ((((*(unsigned char *)((char *)shader + 0x28) & 4) != 0) ? 0x13 : 0) << 24) | 0x200800;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    FUN_00174510(group, 0);
  }

  mipmap_count = *(short *)((char *)shader + 0xd8);
  max_mipmap = (mipmap_count <= 4) ? mipmap_count : 4;
  FUN_001584f0(0, 6, max_mipmap);
  D3DDevice_SetTextureStageState(0, 10, 1);
  D3DDevice_SetTextureStageState(0, 0xb, 1);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 2);
  rasterizer_set_texture(3, 2, 0, *(int *)((char *)shader + 0xa8),
                         *(unsigned short *)((char *)group + 0x10));
  D3DDevice_SetTextureStageState(3, 10, 3);
  D3DDevice_SetTextureStageState(3, 0xb, 3);
  D3DDevice_SetTextureStageState(3, 0xc, 3);
  D3DDevice_SetTextureStageState(3, 0xd, 2);
  D3DDevice_SetTextureStageState(3, 0xe, 2);
  D3DDevice_SetTextureStageState(3, 0xf, 2);
  D3DDevice_SetRenderState_CullMode(0);
  SetRenderStateSmart(0x43, 0x10101);
  SetRenderStateSmart(0x3b, ~(*(unsigned int *)group >> 4) & 1);
  SetRenderStateSmart(
      0x3e, ((((*(unsigned char *)((char *)shader + 0x28) & 1) != 0) ? 0x303 : 0) + 1));
  SetRenderStateSmart(0x3f, 1);
  SetRenderStateSmart(0x4a, 0x8006);
  SetRenderStateSmart(0x3c, 0);
  SetRenderStateSmart(0x7b, 1);
  SetRenderStateSmart(0x39, 0x203);
  SetRenderStateSmart(0x40, alpha_blend_enable);
  FUN_00178b40(0x17, vertex_type, permutation);

  /* c[-0x54] .. c[-0x52]: ripple scroll matrix.  Must stay one contiguous
   * 48-byte block - SetVertexShaderConstant uploads 3 vec4 from &constants[0]. */
  /* The original copies +0xc4 as an integer move (MOV EDX,[ESI+0xC4] /
   * MOV [EBP-0x44],EDX / MOV [EBP-0x40],EAX at 0x17a41d), not through the
   * FPU — bit-identical either way. */
  *(int *)&constants[0] = *(int *)((char *)shader + 0xc4);
  *(int *)&constants[1] = *(int *)&constants[0];
  /* Association must stay (cos*+0xc0)*time — FMUL [ESI+0xC0] then
   * FMUL [0x5a5e18] at 0x17a470/0x17a476.  As one expression VC71
   * reassociates to (cos*time)*+0xc0, which rounds differently; the
   * sequenced temp pins the original order for both compilers. */
  scroll = x87_fcos(*(float *)((char *)shader + 0xbc)) *
           *(float *)((char *)shader + 0xc0);
  constants[2] = scroll * *(float *)0x5a5e18;
  scroll = x87_fsin(*(float *)((char *)shader + 0xbc)) *
           *(float *)((char *)shader + 0xc0);
  constants[3] = scroll * *(float *)0x5a5e18;
  constants[4] = 0.0f;
  constants[5] = 0.0f;
  constants[6] = 0.0f;
  constants[7] = 0.0f;
  constants[8] = 0.0f;
  constants[9] = 0.0f;
  constants[10] = 0.0f;
  constants[11] = 0.0f;
  D3DDevice_SetVertexShaderConstant(-0x54, &constants[0], 3);

  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(int *)0x5a5b98 = 0x64621;
  *(int *)0x5a5b9c = 0x111;
  *(int *)0x5a5b74 = 0xcd;
  *(int *)0x5a5b48 = 0xb0b0120;
  *(int *)0x5a5b4c = 0xc0c0000;
  if ((*(unsigned char *)((char *)shader + 0x28) & 4) != 0) {
    *(int *)0x5a5b94 = 4;
    *(int *)0x5a5b78 = 0xc0;
    *(int *)0x5a5b50 = 0xc0c0000;
    *(int *)0x5a5b7c = 0xc0;
    *(int *)0x5a5b54 = 0x2d0c0d0b;
    *(int *)0x5a5b80 = 0xc00;
    *(int *)0x5a5ae0 = 0x330c0000;
  } else {
    *(int *)0x5a5b94 = 2;
    *(int *)0x5a5b78 = 0xc0;
    *(int *)0x5a5ae0 = 0x2d0f0b00;
    *(int *)0x5a5ae4 = 0xc0c0000;
  }

  v = (float *)((char *)group + 0x80);
  if (x87_sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) <= 0.0f) {
    *(int *)0x5a5ae8 = 0xffffff;
  } else {
    weight = 0.0f;
    if (-(*(float *)0x5a5bd8 * v[1] + *(float *)0x5a5bdc * v[2] + *(float *)0x5a5bd4 * v[0]) >=
        0.0f) {
      weight = 1.0f;
      if (-(*(float *)0x5a5bd8 * v[1] + *(float *)0x5a5bdc * v[2] +
            *(float *)0x5a5bd4 * v[0]) <= 1.0f) {
        weight = -FUN_00013070((float *)0x5a5bd4, v);
      }
    }
    inverse_weight = 1.0f - weight;
    tint[0] = inverse_weight * *(float *)((char *)shader + 0x80) +
              weight * *(float *)((char *)shader + 0x70);
    tint[1] = inverse_weight * *(float *)((char *)shader + 0x84) +
              weight * *(float *)((char *)shader + 0x74);
    tint[2] = inverse_weight * *(float *)((char *)shader + 0x88) +
              weight * *(float *)((char *)shader + 0x78);
    *(unsigned int *)0x5a5ae8 = FUN_000d1dd0(&tint[0]);
  }

  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  /* The stage-state value carries the float bit pattern of -shader[0xe0],
   * not an integer conversion of it. */
  stage_value.f = -*(float *)((char *)shader + 0xe0);
  D3DDevice_SetTextureStageState(0, 0x10, stage_value.u);
  FUN_00174510(group, 0);
  D3DDevice_SetTextureStageState(0, 0x10, 0);
}
