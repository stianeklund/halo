
/* 40-byte and 16-byte blocks copied wholesale in FUN_0016c090. The reference
 * lowers the first as an inline `rep movsd` (ECX=10) and the second as four
 * dword load/store pairs, which is what a struct assignment of each size
 * produces; no field-level evidence exists for either block's contents, so
 * they stay opaque. */
typedef struct {
  uint32_t d[10];
} rasterizer_model_block40_t;

typedef struct {
  uint32_t d[4];
} rasterizer_model_block16_t;

typedef struct {
  uint32_t d[3];
} rasterizer_model_block12_t;

/* ---------- constants
 * Names: PAL 2342 rasterizer_xbox_models.c enums (T2). Values: the 2276
 * operands at the use sites in rasterizer_model_draw_environment_shader. */
enum {
  _shader_type_environment = 3 /* PUSH 0x3 to shader_get_and_verify_type (0x16b344) */
};

enum {
  _rasterizer_geometry_no_fog_bit = 2, /* TEST byte [ECX],0x4 (0x16b87d) */
  _rasterizer_geometry_no_zbuffer_bit = 3 /* TEST CL,0x8 (0x16b364) */
};

enum {
  _shader_environment_alpha_tested_bit = 0, /* AND EDI,EBX=1 (0x16b453) */
  _shader_environment_true_atmospheric_fog_bit =
    2 /* TEST [ESI+0x28],0x4 (0x16b624) */
};

enum {
  _fog_definition_atmosphere_dominant_bit = 1 /* TEST AL,0x2 (0x16b950) */
};

enum {
  _rasterizer_statistics_mode_summary = 1, /* CMP word,0x1; JL (0x16b687) */
  _rasterizer_statistics_mode_enabled = 2 /* CMP word,0x2; JNZ (0x16be74) */
};

enum {
  _rasterizer_vertex_shader_model = 10 /* PUSH 0xa to FUN_00178b40 (0x16b67d) */
};

enum {
  _model_vertex_shader_permutation_planar_fog = 0,
  _model_vertex_shader_permutation_lights,
  _model_vertex_shader_permutation_reflection,
  _model_vertex_shader_permutation_reflection_one_node,
  NUMBER_OF_MODEL_VERTEX_SHADER_PERMUTATIONS
};

enum {
  _shader_detail_mask_none = 0 /* XOR EBX,EBX -> BX (0x16bdb7) */
};

/* Vertex shader constant registers (PUSH -0x54 / -0x51 at 0x16b757/0x16b797).
 * Names follow the PAL 2342 locals uploaded there (T3, mechanical). */
enum {
  k_model_vertex_constants_register = -0x54,
  k_model_vertex_constants_count = 3,
  k_model_specular_constants_register = -0x51,
  k_model_specular_constants_count = 2
};

/* D3D values (Xbox d3d8types.h; same spellings as
 * rasterizer_xbox_environment_fog.c). */
#define D3DTSS_ADDRESSU 0xa
#define D3DTSS_ADDRESSV 0xb
#define D3DTSS_ADDRESSW 0xc
#define D3DTSS_MAGFILTER 0xd
#define D3DTSS_MINFILTER 0xe
#define D3DTSS_MIPFILTER 0xf
#define D3DTADDRESS_WRAP 1
#define D3DTADDRESS_CLAMP 3
#define D3DTEXF_LINEAR 2
#define D3DCULL_CCW 0x901
#define D3DBLEND_SRCALPHA 0x302
#define D3DBLEND_INVSRCALPHA 0x303
#define D3DBLENDOP_ADD 0x8006
#define D3DCMP_LESSEQUAL 0x203
#define D3DCOLORWRITEENABLE_RGB 0x10101 /* RED<<16 | GREEN<<8 | BLUE */

/* D3DDevice_SetRenderState_Simple NV097 method words. Each simple state is
 * mirrored into the D3D render-state cache at 0x1fb698 + state * 4; the slot
 * names pair each method with the PAL 2342 D3DRS_* written with the same
 * value (T2). */
#define NV097_SET_COMBINER_SPECULAR_FOG_CW1_CMD 0x4028c
#define NV097_SET_ALPHA_TEST_ENABLE_CMD 0x40300
#define NV097_SET_BLEND_ENABLE_CMD 0x40304
#define NV097_SET_ALPHA_REF_CMD 0x40340
#define NV097_SET_BLEND_FUNC_SFACTOR_CMD 0x40344
#define NV097_SET_BLEND_FUNC_DFACTOR_CMD 0x40348
#define NV097_SET_BLEND_EQUATION_CMD 0x40350
#define NV097_SET_DEPTH_FUNC_CMD 0x40354
#define NV097_SET_DEPTH_MASK_CMD 0x4035c
#define NV097_SET_COMBINER_COLOR_ICW0_CMD 0x40ac0
#define NV097_SET_COMBINER_COLOR_OCW0_CMD 0x41e40
#define D3D_RENDER_STATE_PSFINALCOMBINERINPUTSEFG (*(uint32_t *)0x1fb6bc)
#define D3D_RENDER_STATE_PSRGBINPUTS0 (*(uint32_t *)0x1fb720)
#define D3D_RENDER_STATE_PSRGBOUTPUTS0 (*(uint32_t *)0x1fb74c)
#define D3D_RENDER_STATE_ZFUNC (*(uint32_t *)0x1fb77c)
#define D3D_RENDER_STATE_ALPHABLENDENABLE (*(uint32_t *)0x1fb784)
#define D3D_RENDER_STATE_ALPHATESTENABLE (*(uint32_t *)0x1fb788)
#define D3D_RENDER_STATE_ALPHAREF (*(uint32_t *)0x1fb78c)
#define D3D_RENDER_STATE_SRCBLEND (*(uint32_t *)0x1fb790)
#define D3D_RENDER_STATE_DESTBLEND (*(uint32_t *)0x1fb794)
#define D3D_RENDER_STATE_ZWRITEENABLE (*(uint32_t *)0x1fb798)
#define D3D_RENDER_STATE_COLORWRITEENABLE (*(uint32_t *)0x1fb7a4)
#define D3D_RENDER_STATE_BLENDOP (*(uint32_t *)0x1fb7c0)

/* Pixel-shader combiner words written around the environment-shader draw.
 * Decoded with the PAL 2342 PS_COMBINERINPUTS / PS_COMBINEROUTPUTS calls that
 * produce the same values (T2). */
#define k_environment_rgb_inputs0_draw 0x0a021819 /* T2, C1, T0.a, T1.a */
#define k_environment_rgb_outputs0_draw 0x20cd /* R0, R1, -, AB dot */
#define k_environment_final_inputs_efg_draw 0x0c111a00 /* R0, C0.a, T2.a */
#define k_environment_rgb_inputs0_restore 0x0a020a01 /* T2, C1, T2, C0 */
#define k_environment_rgb_outputs0_restore 0x30cd /* R0, R1, -, AB|CD dot */
#define k_environment_final_inputs_efg_restore 0x0c111800 /* R0, C0.a, T0.a */

/* rasterizer_debug_options_t (0x3256b8) is declared in types.h. */

/* render_lighting (PAL 2342 render.h, T2): the 0x74-byte block at
 * rasterizer_model_begin_parameters+0x10 (FUN_0016c090 copies 0x74 bytes of
 * it into the rasterizer memory pool). Only accessed fields are named. */
typedef struct {
  byte pad_00[0x40]; /* 0x00 not accessed in this TU */
  int16_t point_light_count; /* 0x40 CMP word [EAX+0x50],0 */
  byte pad_42[0xa]; /* 0x42 not accessed in this TU */
  real_argb_color reflection_tint_color; /* 0x4c FMUL [EAX+0x5c..0x68] */
  byte pad_5c[0x18]; /* 0x5c not accessed in this TU */
} rasterizer_model_lighting_t;
cs(rasterizer_model_lighting_t, 0x74);
co(rasterizer_model_lighting_t, point_light_count, 0x40);
co(rasterizer_model_lighting_t, reflection_tint_color, 0x4c);

/* rasterizer_model_begin_parameters (PAL 2342, T2): the block FUN_0016bed0
 * stores in rasterizer_models_globals.parameters. Size unproven. */
typedef struct {
  uint32_t geometry_flags; /* 0x00 _rasterizer_geometry_*_bit */
  int32_t unique_identifier; /* 0x04 */
  const void *node_matrices; /* 0x08 skinning.node_matrices */
  int16_t node_matrix_count; /* 0x0c skinning.node_matrix_count */
  word pad_0e; /* 0x0e */
  rasterizer_model_lighting_t lighting; /* 0x10 */
  render_animation animation; /* 0x84 */
  int16_t effect_type; /* 0x8c effect.type */
  word pad_8e; /* 0x8e */
  real effect_intensity; /* 0x90 effect.intensity */
  byte pad_94[4]; /* 0x94 */
  int32_t effect_source_object_index; /* 0x98 effect.source_object_index */
  real_point3d effect_centroid; /* 0x9c effect.centroid */
  void *effect_shader; /* 0xa8 effect.shader */
  render_animation effect_animation; /* 0xac effect.animation */
  real_point3d centroid; /* 0xb4 FLD [EAX+0xb4..0xbc] */
  real radius; /* 0xc0 */
  real_vector2d base_map_scale; /* 0xc4 MOV EDX,[EAX+0xc4]/[+0xc8] */
} rasterizer_model_begin_parameters_t;
co(rasterizer_model_begin_parameters_t, node_matrix_count, 0x0c);
co(rasterizer_model_begin_parameters_t, lighting, 0x10);
co(rasterizer_model_begin_parameters_t, animation, 0x84);
co(rasterizer_model_begin_parameters_t, effect_type, 0x8c);
co(rasterizer_model_begin_parameters_t, effect_source_object_index, 0x98);
co(rasterizer_model_begin_parameters_t, effect_shader, 0xa8);
co(rasterizer_model_begin_parameters_t, centroid, 0xb4);
co(rasterizer_model_begin_parameters_t, base_map_scale, 0xc4);

/* rasterizer_models_globals (0x47df48). Names and layout: PAL 2342 struct
 * rasterizer_models_globals (offsetof(parameters) == 0xb0, T2); each offset
 * is a 2276 address this TU reads or writes. */
typedef struct {
  void *transparent_geometry_cached_animation; /* 0x00 [0x47df48] */
  void *transparent_geometry_cached_lighting; /* 0x04 [0x47df4c] */
  int16_t transparent_geometry_cached_node_matrix_count; /* 0x08 [0x47df50] */
  word pad_0a; /* 0x0a */
  void *transparent_geometry_cached_node_matrices; /* 0x0c [0x47df54] */
  byte immediate_transparent_geometry_group[0xa0]; /* 0x10 [0x47df58] */
  rasterizer_model_begin_parameters_t *parameters; /* 0xb0 [0x47dff8] */
  boolean local_parameters_queued_flag; /* 0xb4 [0x47dffc] */
  byte pad_b5[3]; /* 0xb5 */
  int16_t model_effect_type; /* 0xb8 [0x47e000] */
  boolean local_sky_flag; /* 0xba [0x47e002] */
  boolean local_planar_fog_flag; /* 0xbb [0x47e003] */
  boolean local_environment_fog_screen_flag; /* 0xbc [0x47e004] */
  boolean local_do_not_change_z_stencil_states; /* 0xbd [0x47e005] */
  boolean local_reported_too_many_transparent_geometry_groups; /* 0xbe */
} rasterizer_models_globals_t;
co(rasterizer_models_globals_t, immediate_transparent_geometry_group, 0x10);
co(rasterizer_models_globals_t, parameters, 0xb0);
co(rasterizer_models_globals_t, model_effect_type, 0xb8);
co(rasterizer_models_globals_t, local_planar_fog_flag, 0xbb);
co(rasterizer_models_globals_t,
   local_reported_too_many_transparent_geometry_groups, 0xbe);

/* render_fog (PAL 2342 render_cameras.h, T2) at
 * global_window_parameters+0x1e8 (0x5a5da8). */
typedef struct {
  word fog_definition_flags; /* 0x00 MOV AL,[0x5a5da8] */
  word runtime_flags; /* 0x02 */
  real_rgb_color atmospheric_color; /* 0x04 [0x5a5dac..0x5a5db4] */
  real atmospheric_maximum_density; /* 0x10 [0x5a5db8] */
  real atmospheric_minimum_distance; /* 0x14 [0x5a5dbc] */
  real atmospheric_maximum_distance; /* 0x18 [0x5a5dc0] */
  int16_t planar_mode; /* 0x1c word [0x5a5dc4] in FUN_0016bed0 */
  word pad_1e; /* 0x1e */
  real_plane3d plane; /* 0x20 [0x5a5dc8..0x5a5dd4] */
  real_rgb_color planar_color; /* 0x30 [0x5a5dd8..0x5a5de0] */
  real planar_maximum_density; /* 0x3c */
  real planar_maximum_distance; /* 0x40 [0x5a5de8] */
  real planar_maximum_depth; /* 0x44 [0x5a5dec] */
} rasterizer_window_fog_t;
co(rasterizer_window_fog_t, atmospheric_color, 0x04);
co(rasterizer_window_fog_t, atmospheric_minimum_distance, 0x14);
co(rasterizer_window_fog_t, plane, 0x20);
co(rasterizer_window_fog_t, planar_color, 0x30);
co(rasterizer_window_fog_t, planar_maximum_depth, 0x44);

/* rasterizer_window_begin_parameters (0x5a5bc0). Prefix only; the same block
 * rasterizer_xbox_environment_fog.c maps as fog_window_parameters. Names:
 * PAL 2342 rasterizer.h / render_cameras.h (T2). */
typedef struct {
  int16_t rasterizer_target; /* 0x000 */
  int16_t window_index; /* 0x002 */
  byte pad_004[4]; /* 0x004 */
  real_point3d camera_position; /* 0x008 camera.position [0x5a5bc8..d0] */
  real_vector3d camera_forward; /* 0x014 camera.forward [0x5a5bd4..dc] */
  byte pad_020[0x1c8]; /* 0x020 not accessed in this TU */
  rasterizer_window_fog_t fog; /* 0x1e8 */
} rasterizer_window_begin_parameters_t;
co(rasterizer_window_begin_parameters_t, camera_position, 0x008);
co(rasterizer_window_begin_parameters_t, camera_forward, 0x014);
co(rasterizer_window_begin_parameters_t, fog, 0x1e8);

/* rasterizer_models_frame_statistics (PAL 2342, T2). Base 0x5a5400 from
 * offsetof(vertices_by_permutation) == 0x14 against ADD [EAX*4+0x5a5414]. */
typedef struct {
  byte pad_000[0x14]; /* 0x000 */
  uint32_t vertices_by_permutation
    [NUMBER_OF_MODEL_VERTEX_SHADER_PERMUTATIONS]; /* 0x014
                                                   */
  byte pad_024[0xb0]; /* 0x024 not accessed in this TU */
  uint32_t model_count; /* 0x0d4 [0x5a54d4] */
  uint32_t model_vertex_count; /* 0x0d8 [0x5a54d8] */
  uint32_t model_triangle_count; /* 0x0dc [0x5a54dc] */
  uint32_t model_draw_count; /* 0x0e0 [0x5a54e0] */
  int32_t transparent_model_vertex_count; /* 0x0e4 [0x5a54e4] */
  int32_t transparent_model_triangle_count; /* 0x0e8 [0x5a54e8] */
  int32_t transparent_model_maximum_triangle_count; /* 0x0ec [0x5a54ec] */
  int32_t transparent_model_submit_count; /* 0x0f0 [0x5a54f0] */
  byte pad_0f4[0x5c]; /* 0x0f4 not accessed in this TU */
  uint32_t skinning_work; /* 0x150 [0x5a5550] */
  uint32_t lighting_work; /* 0x154 [0x5a5554] */
  uint32_t vertex_shader_work; /* 0x158 [0x5a5558] */
  uint32_t pushbuffer_words; /* 0x15c [0x5a555c] */
  uint32_t skinning_work_accumulated; /* 0x160 [0x5a5560] */
  uint32_t lighting_work_accumulated; /* 0x164 [0x5a5564] */
  uint32_t vertex_shader_work_accumulated; /* 0x168 [0x5a5568] */
} rasterizer_models_frame_statistics_t;
co(rasterizer_models_frame_statistics_t, vertices_by_permutation, 0x014);
co(rasterizer_models_frame_statistics_t, model_count, 0x0d4);
co(rasterizer_models_frame_statistics_t, model_draw_count, 0x0e0);
co(rasterizer_models_frame_statistics_t, transparent_model_submit_count, 0x0f0);
co(rasterizer_models_frame_statistics_t, pushbuffer_words, 0x15c);
co(rasterizer_models_frame_statistics_t, vertex_shader_work_accumulated, 0x168);

/* shader_environment_definition (PAL 2342, T2), as returned by
 * shader_get_and_verify_type(shader, _shader_type_environment). Flattened prefix: PAL's
 * environment.diffuse / .specular / .reflection sub-blocks are inlined and only
 * the fields this TU reads are named. PAL declares flags as a word; 2276 only
 * reads its low byte here. */
typedef struct {
  byte pad_000[0x28]; /* 0x000 shader header */
  byte flags; /* 0x028 _shader_environment_*_bit */
  byte pad_029[0x5f]; /* 0x029 */
  tag_reference base_map; /* 0x088 index at +0x94 */
  byte pad_098[0x18]; /* 0x098 */
  int16_t detail_map_function; /* 0x0b0 MOV AX,[ESI+0xb0] */
  word pad_0b2; /* 0x0b2 */
  real primary_detail_map_scale; /* 0x0b4 MOV EDX,[ESI+0xb4] */
  tag_reference primary_detail_map; /* 0x0b8 index at +0xc4 */
  byte pad_0c8[0x60]; /* 0x0c8 */
  tag_reference bump_map; /* 0x128 index at +0x134 */
  byte pad_138[0x170]; /* 0x138 */
  real_rgb_color view_perpendicular_color; /* 0x2a8 specular */
  real_rgb_color view_parallel_color; /* 0x2b4 specular */
  byte pad_2c0[0x34]; /* 0x2c0 */
  real view_perpendicular_brightness; /* 0x2f4 reflection */
  real view_parallel_brightness; /* 0x2f8 reflection */
  byte pad_2fc[0x28]; /* 0x2fc */
  tag_reference reflection_cube_map; /* 0x324 index at +0x330 */
} shader_environment_definition_t;
co(shader_environment_definition_t, flags, 0x028);
co(shader_environment_definition_t, base_map, 0x088);
co(shader_environment_definition_t, detail_map_function, 0x0b0);
co(shader_environment_definition_t, primary_detail_map_scale, 0x0b4);
co(shader_environment_definition_t, primary_detail_map, 0x0b8);
co(shader_environment_definition_t, bump_map, 0x128);
co(shader_environment_definition_t, view_perpendicular_color, 0x2a8);
co(shader_environment_definition_t, view_parallel_color, 0x2b4);
co(shader_environment_definition_t, view_perpendicular_brightness, 0x2f4);
co(shader_environment_definition_t, reflection_cube_map, 0x324);

#define global_d3d_device (*(void **)0x476ab0)
#define rasterizer_debug_options (*(rasterizer_debug_options_t *)0x3256b8)
#define rasterizer_models_globals (*(rasterizer_models_globals_t *)0x47df48)
#define global_window_parameters \
  (*(rasterizer_window_begin_parameters_t *)0x5a5bc0)
#define rasterizer_frame_statistics \
  (*(rasterizer_models_frame_statistics_t *)0x5a5400)

/* 0x16ab00 — shader-model detail-texture render-state setup.
 *
 * Validates detail_function (0..2, NUMBER_OF_SHADER_MODEL_DETAIL_FUNCTIONS)
 * and detail_mask (0..8, NUMBER_OF_SHADER_MODEL_DETAIL_MASKS) against the
 * asserts at lines 0x6a/0x6b of this TU, then builds a packed render-state
 * word from a 12-entry constant lookup table (indices 0..8 keyed by
 * detail_mask, indices 9..11 keyed by detail_function) and writes a run of
 * fields into the shared 0xf0-byte pixel-shader state block at 0x5a5ac0
 * (documented in rasterizer_xbox_screen_effect.c / rasterizer_xbox_widgets.c).
 * param_3/param_4..param_7 are opaque caller-supplied render-state words —
 * no naming evidence for their meaning beyond how they are stored/consumed
 * here, so they stay as explicit unknowns.
 *
 * If a profile-collection section was requested (DAT_00325173, set by
 * FUN_0016b180 in this TU), the state block is instead finished as a
 * pixel-shader descriptor and handed to rasterizer_set_pixel_shader, and the
 * request flag is cleared. Otherwise the block's fields are pushed to the
 * device one at a time via the ECX/EDX fastcall
 * D3DDevice_SetRenderState_Simple, each followed by a write into its NV2A
 * shadow slot (same interleaved call/shadow-store idiom documented in
 * rasterizer_xbox.c). Finally, in shader-cost accounting mode (DAT_003256ba ==
 * 2) the frame's accumulated pixel-shader cost (DAT_005a555c) is bumped by a
 * fixed cost of 0x2c.
 */
void FUN_0016ab00(int16_t detail_function /* @<ax> */,
                  uint32_t param_3 /* @<ecx> */,
                  int16_t detail_mask /* @<bx> */, uint32_t param_4,
                  uint32_t param_5, uint32_t param_6, uint32_t param_7,
                  char param_8, char param_9)
{
  uint32_t detail_table[12];
  uint32_t combined;
  uint32_t value;

  if (detail_function < 0 || detail_function > 2) {
    display_assert(
      "detail_function>=0 && "
      "detail_function<NUMBER_OF_SHADER_MODEL_DETAIL_FUNCTIONS",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x6a,
      true);
    system_exit(-1);
  }

  if (detail_mask < 0 || detail_mask > 8) {
    display_assert(
      "detail_mask>=0 && detail_mask<NUMBER_OF_SHADER_MODEL_DETAIL_MASKS",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x6b,
      true);
    system_exit(-1);
  }

  detail_table[9] = 0xa0;
  detail_table[11] = 0xa0;
  detail_table[0] = 0x20;
  *(uint32_t *)0x5a5af0 = param_4;
  detail_table[1] = 0x2d;
  detail_table[2] = 0xd;
  detail_table[3] = 0x3c;
  detail_table[4] = 0x1c;
  detail_table[5] = 0x39;
  detail_table[6] = 0x19;
  detail_table[7] = 0x3a;
  detail_table[8] = 0x1a;
  detail_table[10] = 0x20;

  combined = ((detail_table[detail_function + 9] |
               (detail_table[detail_mask] ^ 0x20) << 8)
                << 8 |
              detail_table[detail_mask])
               << 8 |
             9;

  *(uint32_t *)0x5a5aec = param_3;
  *(uint32_t *)0x5a5b54 = combined;

  if (param_8 == 0) {
    detail_table[9] = 0x8090809;
    detail_table[10] = 0x8090000;
    detail_table[11] = 0x8204920;
    *(uint32_t *)0x5a5b60 = detail_table[detail_function + 9];
    *(uint32_t *)0x5a5b8c = 0x800;
    *(uint32_t *)0x5a5b64 = 0x8040b1d;
  } else {
    detail_table[9] = 0xc090c09;
    detail_table[10] = 0xc090000;
    detail_table[11] = 0xc204920;
    *(uint32_t *)0x5a5b64 = detail_table[detail_function + 9];
    *(uint32_t *)0x5a5b60 = 0x8040b1d;
    *(uint32_t *)0x5a5b8c = 0xc00;
  }
  *(uint32_t *)0x5a5b90 = 0xc00;

  if (param_9 == 0) {
    *(uint32_t *)0x5a5afc = param_6;
    *(uint32_t *)0x5a5b1c = param_7;
    *(uint32_t *)0x5a5b6c = param_5;
    *(uint32_t *)0x5a5b70 = param_7;
    *(uint32_t *)0x5a5ae0 = 0x340f010d;
    *(uint32_t *)0x5a5ae4 = 0xc111800;
  } else {
    *(uint32_t *)0x5a5ae0 = 0x330c0300;
    *(uint32_t *)0x5a5ae4 = 0x1800;
  }

  if (*(char *)0x325173 != 0) {
    *(uint32_t *)0x5a5b98 = 0x18421;
    *(uint32_t *)0x5a5b94 = 0x11008;
    *(uint32_t *)0x5a5ae8 = 0xff0000;
    *(uint32_t *)0x5a5b08 = 0xff00;
    *(uint32_t *)0x5a5ac0 = 0xa200000;
    *(uint32_t *)0x5a5b28 = 0xc0;
    *(uint32_t *)0x5a5b48 = 0xa020a01;
    *(uint32_t *)0x5a5b74 = 0x30cd;
    *(uint32_t *)0x5a5ac4 = 0x1c200000;
    *(uint32_t *)0x5a5b2c = 0x90;
    *(uint32_t *)0x5a5b4c = 0x4200c01;
    *(uint32_t *)0x5a5b78 = 0x400;
    *(uint32_t *)0x5a5ac8 = 0xc200d15;
    *(uint32_t *)0x5a5b30 = 0xcd;
    *(uint32_t *)0x5a5b50 = 0x3c201c01;
    *(uint32_t *)0x5a5b7c = 0xc00;
    *(uint32_t *)0x5a5b80 = 0x900;
    *(uint32_t *)0x5a5b58 = 0xb05040c;
    *(uint32_t *)0x5a5b84 = 0xb4;
    *(uint32_t *)0x5a5b5c = 0x22014e1;
    *(uint32_t *)0x5a5b88 = 0xd00;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    *(char *)0x325173 = 0;
    return;
  }

  value = param_3;
  D3DDevice_SetRenderState_Simple(0x40a64, value);
  *(uint32_t *)0x1fb6c4 = value;

  value = *(uint32_t *)0x5a5af0;
  D3DDevice_SetRenderState_Simple(0x40a68, value);
  *(uint32_t *)0x1fb6c8 = value;

  value = *(uint32_t *)0x5a5afc;
  D3DDevice_SetRenderState_Simple(0x40a74, value);
  *(uint32_t *)0x1fb6d4 = value;

  value = *(uint32_t *)0x5a5b1c;
  D3DDevice_SetRenderState_Simple(0x40a94, value);
  *(uint32_t *)0x1fb6f4 = value;

  value = *(uint32_t *)0x5a5b54;
  D3DDevice_SetRenderState_Simple(0x40acc, value);
  *(uint32_t *)0x1fb72c = value;

  value = *(uint32_t *)0x5a5b60;
  D3DDevice_SetRenderState_Simple(0x40ad8, value);
  *(uint32_t *)0x1fb738 = value;

  value = *(uint32_t *)0x5a5b8c;
  D3DDevice_SetRenderState_Simple(0x41e58, value);
  *(uint32_t *)0x1fb764 = value;

  value = *(uint32_t *)0x5a5b64;
  D3DDevice_SetRenderState_Simple(0x40adc, value);
  *(uint32_t *)0x1fb73c = value;

  value = *(uint32_t *)0x5a5b90;
  D3DDevice_SetRenderState_Simple(0x41e5c, value);
  *(uint32_t *)0x1fb768 = value;

  value = *(uint32_t *)0x5a5b6c;
  D3DDevice_SetRenderState_Simple(0x41e20, value);
  *(uint32_t *)0x1fb744 = value;

  value = *(uint32_t *)0x5a5b70;
  D3DDevice_SetRenderState_Simple(0x41e24, value);
  *(uint32_t *)0x1fb748 = value;

  value = *(uint32_t *)0x5a5ae0;
  D3DDevice_SetRenderState_Simple(0x40288, value);
  *(uint32_t *)0x1fb6b8 = value;

  value = *(uint32_t *)0x5a5ae4;
  D3DDevice_SetRenderState_Simple(0x4028c, value);
  *(uint32_t *)0x1fb6bc = value;

  if (*(short *)0x3256ba == 2) {
    *(int *)0x5a555c = *(int *)0x5a555c + 0x2c;
  }
}

/* 0x16b180 — model-rendering profile toggle.
 * Guarded by the profile-collection master switch (DAT_003256c4); when
 * active, records that a models profile section has been requested
 * (DAT_00325173 = 1), latches the requested sub-state (DAT_0047e002), and
 * opens the corresponding profile section via rasterizer_profile_begin (1 = models with
 * the flag set, 2 = models without). */
void FUN_0016b180(bool flag)
{
  if (*(char *)0x3256c4 != 0) {
    *(char *)0x325173 = 1;
    *(char *)0x47e002 = flag;
    if (flag != 0) {
      rasterizer_profile_begin(1);
      return;
    }
    rasterizer_profile_begin(2);
  }
}

/* 0x16b1c0 - end of the model local-parameters block; the counterpart of
 * FUN_0016bed0 (0x16bed0), which stashes the parameter block pointer in
 * DAT_0047dff8 and its second argument's low byte in DAT_0047e005.
 * Guarded by the profile-collection master switch (DAT_003256c4). Asserts
 * that a parameter block is currently open ("local_parameters", line 0x5d3),
 * flushes the pending sub-pass when DAT_0047e004 is set (FUN_00165fc0), then
 * - only when the block's first byte is negative and DAT_0047e005 is clear,
 * mirroring the open path's entry condition - restores the default projection
 * with rasterizer_set_stencil_mode(2) and rasterizer_set_frustum_z(0.0f, 0.0f). Finally
 * clears DAT_0047dff8 to mark the block closed. */
void FUN_0016b1c0(void)
{
  if (*(char *)0x3256c4 != 0) {
    if (*(void **)0x47dff8 == NULL) {
      display_assert(
        "local_parameters",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x5d3,
        true);
      system_exit(-1);
    }

    if (*(char *)0x47e004 != 0) {
      FUN_00165fc0();
    }

    if (*(signed char *)*(void **)0x47dff8 < 0 && *(char *)0x47e005 == 0) {
      rasterizer_set_stencil_mode(2);
      rasterizer_set_frustum_z(0.0f, 0.0f);
    }

    *(void **)0x47dff8 = NULL;
  }
}

/* 0x16b240 — model-rendering profile close; the counterpart of FUN_0016b180.
 * Guarded by the same profile-collection master switch (DAT_003256c4); reads
 * back the sub-state latched by FUN_0016b180 (DAT_0047e002) and closes the
 * matching profile section via rasterizer_profile_end (1 = models with the flag set,
 * 2 = models without). */
void FUN_0016b240(void)
{
  if (*(char *)0x3256c4 != 0) {
    if (*(char *)0x47e002 != 0) {
      rasterizer_profile_end(1);
      return;
    }
    rasterizer_profile_end(2);
  }
}

/* global_rasterizer_model_ambient_reflection_tint (DAT_0047e4d0): 0x10-byte
 * game-state allocation holding the model ambient reflection tint. Name is
 * taken verbatim from the assert message at 0x17c7aa (#cond string). The
 * pointer is only populated once rasterizer_window_set_fog has run, hence the
 * null check.
 *
 * Declared in kb.json (<common> data, 0x47e4d0) and emitted into
 * build/generated/decl.h. Referencing it by name rather than through an
 * absolute-address cast (`*(void **)0x47e4d0`, still used in
 * src/halo/cutscene/cinematics.c, src/halo/rasterizer/rasterizer.c and
 * src/halo/rasterizer/common/rasterizer_common.c) is what makes cl.exe lower
 * the three float stores below as FLD/FSTP instead of GPR moves, matching the
 * reference exactly. */

/* 0x16b270 — stores the model ambient reflection tint into the 0x10-byte
 * block at global_rasterizer_model_ambient_reflection_tint: an int at +0x0
 * followed by three floats at +0x4/+0x8/+0xc. The whole body is skipped when
 * the block has not been allocated yet. The three float stores are raw
 * FLD/FSTP passthroughs of the incoming stack slots — no numeric conversion
 * takes place, so the parameters must stay typed float. param_1's meaning
 * beyond "the dword at +0x0" has no binary evidence here. */
void FUN_0016b270(int param_1, float param_2, float param_3, float param_4)
{
  if (global_rasterizer_model_ambient_reflection_tint != NULL) {
    *(int *)global_rasterizer_model_ambient_reflection_tint = param_1;
    *(float *)((char *)global_rasterizer_model_ambient_reflection_tint + 4) =
      param_2;
    *(float *)((char *)global_rasterizer_model_ambient_reflection_tint + 8) =
      param_3;
    *(float *)((char *)global_rasterizer_model_ambient_reflection_tint + 0xc) =
      param_4;
  }
}

/* 0x16b2b0 — draw one model part with an environment shader.
 *
 * Only caller: FUN_0016c5a0 (0x16c775), seven pushed args, ADD ESP,0x1c.
 * Asserts carry the 2276 line numbers of this TU (0xf5..0xfc, 0x1b4..0x1b7,
 * 0x1de..0x1e6). Structure and names follow PAL 2342 (advisory, T2); every
 * operation, constant and call order was checked against the 2276
 * disassembly, which matches PAL 2342 for this function.
 *
 * Texture stages (rasterizer_set_texture(stage, type, usage, index, frame)):
 *   0: base_map           (0, 1)  wrap U/V, linear mag/min/mip
 *   1: primary_detail_map (0, 2)  wrap U/V, linear mag/min/mip
 *   2: bump_map if alpha tested, else NONE  (0, 1)  wrap U/V, linear
 *   3: reflection_cube_map (2, 0) clamp U/V/W, linear
 * No texture-coordinate-index state is set; the model vertex shader
 * (FUN_00178b40, permutation chosen below) generates the coordinates.
 *
 * The pixel shader comes from FUN_0016ab00 (detail color 0, detail mask
 * none, base color 0xffffffff, no double diffuse). Combiner stage 0 RGB
 * inputs/outputs and the final combiner EFG inputs are overridden for the
 * draw and restored afterwards. */
void rasterizer_model_draw_environment_shader(void *shader,
                                              short shader_permutation_index,
                                              triangle_buffer *triangle_buffer,
                                              int dynamic_triangle_buffer_index,
                                              int triangle_count,
                                              vertex_buffer *vertex_buffer,
                                              int dynamic_vertex_buffer_index)
{
  shader_environment_definition_t *shader_environment;
  real_vector3d camera_to_model;
  real vertex_constants[3][4];
  real specular_constants[2][4];
  real perpendicular_brightness;
  real_rgb_color perpendicular_color;
  real parallel_brightness;
  real_rgb_color parallel_color;
  real planar_fog_fraction;
  real fog_density;
  real value;
  real_argb_color cc0;
  real_rgb_color cc0_error;
  real_rgb_color cc1;
  uint32_t alpha_test_enable;
  uint32_t cc0_pixel;
  uint32_t cc0_error_pixel;
  uint32_t cc1_pixel;
  int bump_map_index;
  short vertex_shader_permutation;
  short vertex_type;

  if (global_d3d_device == NULL) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0xf5,
      true);
    system_exit(-1);
  }

  if (!rasterizer_debug_options.draw_models) {
    return;
  }

  if (rasterizer_models_globals.parameters == NULL) {
    display_assert(
      "local_parameters",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0xfb,
      true);
    system_exit(-1);
  }

  if (shader == NULL) {
    display_assert(
      "shader", "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
      0xfc, true);
    system_exit(-1);
  }

  shader_environment = (shader_environment_definition_t *)shader_get_and_verify_type(
    shader, _shader_type_environment);

  camera_to_model.i = rasterizer_models_globals.parameters->centroid.x -
                      global_window_parameters.camera_position.x;
  camera_to_model.j = rasterizer_models_globals.parameters->centroid.y -
                      global_window_parameters.camera_position.y;
  camera_to_model.k = rasterizer_models_globals.parameters->centroid.z -
                      global_window_parameters.camera_position.z;

  if ((rasterizer_models_globals.parameters->geometry_flags &
       FLAG(_rasterizer_geometry_no_zbuffer_bit)) != 0) {
    D3DDevice_SetRenderState_ZEnable(false);
  } else {
    D3DDevice_SetRenderState_ZEnable(true);
    D3DDevice_SetRenderState_Simple(NV097_SET_DEPTH_MASK_CMD, true);
    D3D_RENDER_STATE_ZWRITEENABLE = true;
    D3DDevice_SetRenderState_Simple(NV097_SET_DEPTH_FUNC_CMD, D3DCMP_LESSEQUAL);
    D3D_RENDER_STATE_ZFUNC = D3DCMP_LESSEQUAL;
  }
  D3DDevice_SetRenderState_ZBias(0);
  D3DDevice_SetRenderState_CullMode(D3DCULL_CCW);
  D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                  NV097_COLOR_MASK_RGB);
  D3D_RENDER_STATE_COLORWRITEENABLE = D3DCOLORWRITEENABLE_RGB;
  D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_ENABLE_CMD, false);
  D3D_RENDER_STATE_ALPHABLENDENABLE = false;
  D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_SFACTOR_CMD,
                                  D3DBLEND_SRCALPHA);
  D3D_RENDER_STATE_SRCBLEND = D3DBLEND_SRCALPHA;
  D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_DFACTOR_CMD,
                                  D3DBLEND_INVSRCALPHA);
  D3D_RENDER_STATE_DESTBLEND = D3DBLEND_INVSRCALPHA;
  D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_EQUATION_CMD, D3DBLENDOP_ADD);
  D3D_RENDER_STATE_BLENDOP = D3DBLENDOP_ADD;
  alpha_test_enable =
    shader_environment->flags & FLAG(_shader_environment_alpha_tested_bit);
  D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_TEST_ENABLE_CMD,
                                  alpha_test_enable);
  D3D_RENDER_STATE_ALPHATESTENABLE = alpha_test_enable;
  D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_REF_CMD, 0x7f);
  D3D_RENDER_STATE_ALPHAREF = 0x7f;

  rasterizer_set_texture(0, 0, 1, shader_environment->base_map.tag_index,
                         shader_permutation_index);
  D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
  D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
  D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
  D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
  D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

  rasterizer_set_texture(1, 0, 2,
                         shader_environment->primary_detail_map.tag_index,
                         shader_permutation_index);
  D3DDevice_SetTextureStageState(1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
  D3DDevice_SetTextureStageState(1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
  D3DDevice_SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
  D3DDevice_SetTextureStageState(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
  D3DDevice_SetTextureStageState(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

  if ((shader_environment->flags &
       FLAG(_shader_environment_alpha_tested_bit)) != 0) {
    bump_map_index = shader_environment->bump_map.tag_index;
  } else {
    bump_map_index = NONE;
  }
  rasterizer_set_texture(2, 0, 1, bump_map_index, shader_permutation_index);
  D3DDevice_SetTextureStageState(2, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
  D3DDevice_SetTextureStageState(2, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
  D3DDevice_SetTextureStageState(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
  D3DDevice_SetTextureStageState(2, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
  D3DDevice_SetTextureStageState(2, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

  rasterizer_set_texture(3, 2, 0,
                         shader_environment->reflection_cube_map.tag_index,
                         shader_permutation_index);
  D3DDevice_SetTextureStageState(3, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
  D3DDevice_SetTextureStageState(3, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
  D3DDevice_SetTextureStageState(3, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP);
  D3DDevice_SetTextureStageState(3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
  D3DDevice_SetTextureStageState(3, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
  D3DDevice_SetTextureStageState(3, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

  if ((shader_environment->flags &
       FLAG(_shader_environment_true_atmospheric_fog_bit)) == 0) {
    if (rasterizer_models_globals.local_planar_fog_flag) {
      vertex_shader_permutation = _model_vertex_shader_permutation_planar_fog;
    } else if (rasterizer_models_globals.parameters->lighting
                 .point_light_count > 0) {
      vertex_shader_permutation = _model_vertex_shader_permutation_lights;
    } else if (shader_environment->reflection_cube_map.tag_index == NONE &&
               rasterizer_models_globals.parameters->node_matrix_count <= 1) {
      vertex_shader_permutation =
        _model_vertex_shader_permutation_reflection_one_node;
    } else {
      vertex_shader_permutation = _model_vertex_shader_permutation_reflection;
    }
  } else {
    vertex_shader_permutation = _model_vertex_shader_permutation_reflection;
  }

  if (vertex_buffer != NULL) {
    vertex_type = vertex_buffer->type;
  } else {
    vertex_type =
      rasterizer_dynamic_vertices_get_type(dynamic_vertex_buffer_index);
  }
  FUN_00178b40(_rasterizer_vertex_shader_model, vertex_type,
               vertex_shader_permutation);

  if (rasterizer_debug_options.statistics_mode >=
      _rasterizer_statistics_mode_summary) {
    rasterizer_frame_statistics
      .vertices_by_permutation[vertex_shader_permutation] +=
      vertex_buffer->count;
  }

  perpendicular_brightness =
    shader_environment->view_perpendicular_brightness *
    rasterizer_models_globals.parameters->lighting.reflection_tint_color.alpha;
  perpendicular_color.red =
    shader_environment->view_perpendicular_color.red *
    rasterizer_models_globals.parameters->lighting.reflection_tint_color.red;
  perpendicular_color.green =
    shader_environment->view_perpendicular_color.green *
    rasterizer_models_globals.parameters->lighting.reflection_tint_color.green;
  perpendicular_color.blue =
    shader_environment->view_perpendicular_color.blue *
    rasterizer_models_globals.parameters->lighting.reflection_tint_color.blue;
  parallel_brightness =
    shader_environment->view_parallel_brightness *
    rasterizer_models_globals.parameters->lighting.reflection_tint_color.alpha;
  parallel_color.red =
    shader_environment->view_parallel_color.red *
    rasterizer_models_globals.parameters->lighting.reflection_tint_color.red;
  parallel_color.green =
    shader_environment->view_parallel_color.green *
    rasterizer_models_globals.parameters->lighting.reflection_tint_color.green;
  parallel_color.blue =
    shader_environment->view_parallel_color.blue *
    rasterizer_models_globals.parameters->lighting.reflection_tint_color.blue;

  vertex_constants[0][0] = shader_environment->primary_detail_map_scale;
  vertex_constants[0][1] = shader_environment->primary_detail_map_scale;
  vertex_constants[0][2] = 1.0f;
  vertex_constants[0][3] = 1.0f;
  vertex_constants[1][0] =
    rasterizer_models_globals.parameters->base_map_scale.i;
  vertex_constants[1][1] = 0.0f;
  vertex_constants[1][2] = 0.0f;
  vertex_constants[1][3] = 0.0f;
  vertex_constants[2][0] = 0.0f;
  vertex_constants[2][1] =
    rasterizer_models_globals.parameters->base_map_scale.j;
  vertex_constants[2][2] = 0.0f;
  vertex_constants[2][3] = 0.0f;
  specular_constants[0][0] = perpendicular_color.red - parallel_color.red;
  specular_constants[0][1] = perpendicular_color.green - parallel_color.green;
  specular_constants[0][2] = perpendicular_color.blue - parallel_color.blue;
  specular_constants[0][3] = perpendicular_brightness - parallel_brightness;
  specular_constants[1][0] = parallel_color.red;
  specular_constants[1][1] = parallel_color.green;
  specular_constants[1][2] = parallel_color.blue;
  specular_constants[1][3] = parallel_brightness;
  D3DDevice_SetVertexShaderConstant(k_model_vertex_constants_register,
                                    vertex_constants,
                                    k_model_vertex_constants_count);
  D3DDevice_SetVertexShaderConstant(k_model_specular_constants_register,
                                    specular_constants,
                                    k_model_specular_constants_count);

  if (!(global_window_parameters.fog.atmospheric_maximum_distance >
        global_window_parameters.fog.atmospheric_minimum_distance)) {
    display_assert(
      "global_window_parameters.fog.atmospheric_maximum_distance>"
      "global_window_parameters.fog.atmospheric_minimum_distance",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1b4,
      true);
    system_exit(-1);
  }
  if (!(global_window_parameters.fog.atmospheric_maximum_distance > 0.0f)) {
    display_assert(
      "global_window_parameters.fog.atmospheric_maximum_distance>0.0f",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1b5,
      true);
    system_exit(-1);
  }
  if (!(global_window_parameters.fog.planar_maximum_distance > 0.0f)) {
    display_assert(
      "global_window_parameters.fog.planar_maximum_distance>0.0f",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1b6,
      true);
    system_exit(-1);
  }
  if (!(global_window_parameters.fog.planar_maximum_depth > 0.0f)) {
    display_assert(
      "global_window_parameters.fog.planar_maximum_depth>0.0f",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1b7,
      true);
    system_exit(-1);
  }

  if (rasterizer_debug_options.fog &&
      (rasterizer_models_globals.parameters->geometry_flags &
       FLAG(_rasterizer_geometry_no_fog_bit)) == 0) {
    /* Sum order follows the 2276 x87 sequence (z, y, then x term). */
    planar_fog_fraction =
      (global_window_parameters.camera_position.z *
         global_window_parameters.fog.plane.normal.k +
       global_window_parameters.camera_position.y *
         global_window_parameters.fog.plane.normal.j +
       global_window_parameters.fog.plane.normal.i *
         global_window_parameters.camera_position.x -
       global_window_parameters.fog.plane.d) /
      global_window_parameters.fog.atmospheric_maximum_distance;
    if (planar_fog_fraction < 0.0f) {
      planar_fog_fraction = 0.0f;
    } else if (planar_fog_fraction > 1.0f) {
      planar_fog_fraction = 1.0f;
    }

    /* Sum order follows the 2276 x87 sequence (j, k, then i term); clang
     * keeps source order, so this fixes the rounding to match 2276. */
    fog_density =
      (camera_to_model.j * global_window_parameters.camera_forward.j +
       camera_to_model.k * global_window_parameters.camera_forward.k +
       global_window_parameters.camera_forward.i * camera_to_model.i -
       global_window_parameters.fog.atmospheric_minimum_distance) /
      (global_window_parameters.fog.atmospheric_maximum_distance -
       global_window_parameters.fog.atmospheric_minimum_distance);
    if (fog_density < 0.0f) {
      fog_density = 0.0f;
    } else if (fog_density > 1.0f) {
      fog_density = 1.0f;
    }
    fog_density =
      fog_density * global_window_parameters.fog.atmospheric_maximum_density;

    if ((global_window_parameters.fog.fog_definition_flags &
         FLAG(_fog_definition_atmosphere_dominant_bit)) != 0) {
      planar_fog_fraction = 1.0f;
    }

    cc0.alpha = 1.0f - fog_density;
    cc0.red =
      global_window_parameters.fog.planar_color.red -
      fog_density *
        (global_window_parameters.fog.planar_color.red * planar_fog_fraction +
         global_window_parameters.fog.atmospheric_color.red *
           (1.0f - planar_fog_fraction));
    cc0.green =
      global_window_parameters.fog.planar_color.green -
      fog_density *
        (global_window_parameters.fog.planar_color.green * planar_fog_fraction +
         global_window_parameters.fog.atmospheric_color.green *
           (1.0f - planar_fog_fraction));
    cc0.blue =
      global_window_parameters.fog.planar_color.blue -
      fog_density *
        (global_window_parameters.fog.planar_color.blue * planar_fog_fraction +
         global_window_parameters.fog.atmospheric_color.blue *
           (1.0f - planar_fog_fraction));

    value = -cc0.red;
    cc0_error.red = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    value = -cc0.green;
    cc0_error.green = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    value = -cc0.blue;
    cc0_error.blue = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    if (cc0.red < 0.0f) {
      cc0.red = 0.0f;
    } else if (cc0.red > 1.0f) {
      cc0.red = 1.0f;
    }
    if (cc0.green < 0.0f) {
      cc0.green = 0.0f;
    } else if (cc0.green > 1.0f) {
      cc0.green = 1.0f;
    }
    if (cc0.blue < 0.0f) {
      cc0.blue = 0.0f;
    } else if (cc0.blue > 1.0f) {
      cc0.blue = 1.0f;
    }
    cc1.red = global_window_parameters.fog.atmospheric_color.red * fog_density;
    cc1.green =
      global_window_parameters.fog.atmospheric_color.green * fog_density;
    cc1.blue =
      global_window_parameters.fog.atmospheric_color.blue * fog_density;

    if (!(cc0.red >= 0.0f && cc0.red <= 1.0f)) {
      display_assert(
        "cc0.red >=0.0f && cc0.red <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1de,
        true);
      system_exit(-1);
    }
    if (!(cc0.green >= 0.0f && cc0.green <= 1.0f)) {
      display_assert(
        "cc0.green>=0.0f && cc0.green<=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1df,
        true);
      system_exit(-1);
    }
    if (!(cc0.blue >= 0.0f && cc0.blue <= 1.0f)) {
      display_assert(
        "cc0.blue >=0.0f && cc0.blue <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1e0,
        true);
      system_exit(-1);
    }
    if (!(cc1.red >= 0.0f && cc1.red <= 1.0f)) {
      display_assert(
        "cc1.red >=0.0f && cc1.red <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1e1,
        true);
      system_exit(-1);
    }
    if (!(cc1.green >= 0.0f && cc1.green <= 1.0f)) {
      display_assert(
        "cc1.green>=0.0f && cc1.green<=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1e2,
        true);
      system_exit(-1);
    }
    if (!(cc1.blue >= 0.0f && cc1.blue <= 1.0f)) {
      display_assert(
        "cc1.blue >=0.0f && cc1.blue <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1e3,
        true);
      system_exit(-1);
    }
    if (!(cc0_error.red >= 0.0f && cc0_error.red <= 1.0f)) {
      display_assert(
        "cc0_error.red >=0.0f && cc0_error.red <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1e4,
        true);
      system_exit(-1);
    }
    if (!(cc0_error.green >= 0.0f && cc0_error.green <= 1.0f)) {
      display_assert(
        "cc0_error.green>=0.0f && cc0_error.green<=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1e5,
        true);
      system_exit(-1);
    }
    if (!(cc0_error.blue >= 0.0f && cc0_error.blue <= 1.0f)) {
      display_assert(
        "cc0_error.blue >=0.0f && cc0_error.blue <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x1e6,
        true);
      system_exit(-1);
    }

    cc0_pixel = real_argb_color_to_pixel32((const real_argb_color *)&cc0.alpha);
    cc0_error_pixel = real_rgb_color_to_pixel32(&cc0_error.red);
    cc1_pixel = real_rgb_color_to_pixel32(&cc1.red);
  } else {
    cc1_pixel = 0xff000000;
    cc0_error_pixel = 0xff000000;
    cc0_pixel = 0xff000000;
  }

  FUN_0016ab00(shader_environment->detail_map_function, 0,
               _shader_detail_mask_none, 0xffffffff, cc0_pixel, cc0_error_pixel,
               cc1_pixel, false,
               (char)((shader_environment->flags >>
                       _shader_environment_true_atmospheric_fog_bit) &
                      1));
  D3DDevice_SetRenderState_CullMode(D3DCULL_CCW);
  D3DDevice_SetRenderState_Simple(NV097_SET_COMBINER_COLOR_ICW0_CMD,
                                  k_environment_rgb_inputs0_draw);
  D3D_RENDER_STATE_PSRGBINPUTS0 = k_environment_rgb_inputs0_draw;
  D3DDevice_SetRenderState_Simple(NV097_SET_COMBINER_COLOR_OCW0_CMD,
                                  k_environment_rgb_outputs0_draw);
  D3D_RENDER_STATE_PSRGBOUTPUTS0 = k_environment_rgb_outputs0_draw;
  D3DDevice_SetRenderState_Simple(NV097_SET_COMBINER_SPECULAR_FOG_CW1_CMD,
                                  k_environment_final_inputs_efg_draw);
  D3D_RENDER_STATE_PSFINALCOMBINERINPUTSEFG =
    k_environment_final_inputs_efg_draw;

  rasterizer_draw(triangle_buffer, dynamic_triangle_buffer_index, 0,
                  triangle_count, vertex_buffer, dynamic_vertex_buffer_index);

  D3DDevice_SetRenderState_Simple(NV097_SET_COMBINER_COLOR_ICW0_CMD,
                                  k_environment_rgb_inputs0_restore);
  D3D_RENDER_STATE_PSRGBINPUTS0 = k_environment_rgb_inputs0_restore;
  D3DDevice_SetRenderState_Simple(NV097_SET_COMBINER_COLOR_OCW0_CMD,
                                  k_environment_rgb_outputs0_restore);
  D3D_RENDER_STATE_PSRGBOUTPUTS0 = k_environment_rgb_outputs0_restore;
  D3DDevice_SetRenderState_Simple(NV097_SET_COMBINER_SPECULAR_FOG_CW1_CMD,
                                  k_environment_final_inputs_efg_restore);
  D3D_RENDER_STATE_PSFINALCOMBINERINPUTSEFG =
    k_environment_final_inputs_efg_restore;

  if (rasterizer_debug_options.statistics_mode ==
      _rasterizer_statistics_mode_enabled) {
    rasterizer_frame_statistics.pushbuffer_words += 24;
    rasterizer_frame_statistics.model_draw_count++;
    rasterizer_frame_statistics.model_triangle_count += triangle_count;
    rasterizer_frame_statistics.model_vertex_count +=
      FUN_0017ed90(triangle_buffer, vertex_buffer);
  }
}

/* 0x16bed0 — per-model profiling dispatch: LOD/skinning-lighting cost
 * accounting plus cull-plane visibility flag for the model's next draw.
 *
 * Guarded by the profile-collection master switch (DAT_003256c4, see
 * FUN_0016b180); returns immediately when profiling is inactive. Asserts
 * param_1 (the model's render parameter block) is non-null.
 *
 * When the block's flags byte is negative (bit 0x80 set) and param_2's low
 * byte is 0, forces a near/far-frustum override before anything else:
 * rasterizer_set_stencil_mode(1) then rasterizer_set_frustum_z(DAT_0032569c, DAT_003256a0).
 *
 * Stashes the parameter block pointer (DAT_0047dff8), clears DAT_0047dffc,
 * and records param_2's low byte into DAT_0047e005.
 *
 * Picks a render/profiling path into DAT_0047e000 (0/1/2) from the block's
 * word field at +0x8c and float field at +0x90 (an LOD-tier / min-distance
 * pair) gated by the sub-flags DAT_003256f9 and DAT_005a5bc0:
 *   - tier==1 with distance>0 and sub-flags satisfied: DAT_0047e000=1
 *     (already-cached path skinning/lighting is skipped).
 *   - tier==2: DAT_0047e000=2 (also skipped).
 *   - otherwise: programs skinning (param_1+8) and lighting (param_1+0x10),
 *     and accumulates the draw-call/triangle-count deltas those two calls
 *     produced into DAT_005a5560/DAT_005a5564; DAT_0047e000=0.
 *
 * Computes a cull-plane visibility flag (DAT_0047e003) from the block's
 * flag bits 0x4/0x40 and, when bit 0x40 is set, a plane test — the dot
 * product of a plane normal (DAT_005a5dc8..d0) against a reference point
 * (DAT_005a5bc8..d0) minus the plane distance (DAT_005a5dd4) — gated by the
 * cull-enable word DAT_005a5dc4.
 *
 * If the profiling sub-state DAT_0047e002 is 0, calls FUN_00167ee0(param_1)
 * (returns bool in AL, per-model getter/predicate — not yet ported) and
 * latches its result into DAT_0047e004; otherwise DAT_0047e004 is cleared.
 *
 * In shader-cost accounting mode (DAT_003256ba == 2) bumps the per-frame
 * counter DAT_005a54d4.
 */
void FUN_0016bed0(void *param_1, int param_2)
{
  int draw_calls_before;
  int draw_calls_delta;
  int tri_before;
  bool profile_getter_result;

  if (*(char *)0x3256c4 == 0) {
    return;
  }

  if (param_1 == 0) {
    display_assert(
      "parameters",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x24b,
      true);
    system_exit(-1);
  }

  if (*(signed char *)param_1 < 0 && (char)param_2 == 0) {
    rasterizer_set_stencil_mode(1);
    rasterizer_set_frustum_z(*(float *)0x32569c, *(float *)0x3256a0);
  }

  *(void **)0x47dff8 = param_1;
  *(char *)0x47dffc = 0;
  *(char *)0x47e005 = (char)param_2;

  if (*(char *)0x3256f9 == 0 || *(short *)0x5a5bc0 != 0 ||
      *(short *)((char *)param_1 + 0x8c) != 1 ||
      *(float *)((char *)param_1 + 0x90) <= *(float *)0x2533c0) {
    if (*(short *)((char *)param_1 + 0x8c) == 2) {
      *(short *)0x47e000 = 2;
    } else {
      draw_calls_before = *(int *)0x5a5550;
      rasterizer_set_model_skinning((char *)param_1 + 8);
      draw_calls_delta = *(int *)0x5a5550 - draw_calls_before;
      tri_before = *(int *)0x5a5554;
      rasterizer_set_model_lighting((char *)param_1 + 0x10);
      *(int *)0x5a5560 = *(int *)0x5a5560 + draw_calls_delta;
      *(int *)0x5a5564 = *(int *)0x5a5564 + (*(int *)0x5a5554 - tri_before);
      *(short *)0x47e000 = 0;
    }
  } else {
    *(short *)0x47e000 = 1;
  }

  if (*(short *)0x5a5dc4 == 0 || (*(int *)param_1 & 4) != 0 ||
      ((*(int *)param_1 & 0x40) != 0 &&
       *(float *)0x2533c0 <= (*(float *)0x5a5dc8 * *(float *)0x5a5bc8 +
                              *(float *)0x5a5dcc * *(float *)0x5a5bcc +
                              *(float *)0x5a5dd0 * *(float *)0x5a5bd0) -
                               *(float *)0x5a5dd4)) {
    *(char *)0x47e003 = 0;
  } else {
    *(char *)0x47e003 = 1;
  }

  if (*(char *)0x47e002 == 0) {
    profile_getter_result = FUN_00167ee0(param_1);
    *(char *)0x47e004 = 1;
    if (profile_getter_result != 0) {
      goto profile_done;
    }
  }
  *(char *)0x47e004 = 0;

profile_done:
  if (*(short *)0x3256ba == 2) {
    *(int *)0x5a54d4 = *(int *)0x5a54d4 + 1;
  }
}

/* 0x16c090 — submit one model part as a transparent geometry group.
 *
 * Runs only while profile collection is active (DAT_003256c4 and its second
 * gate DAT_003256c5); otherwise returns NULL without touching anything.
 *
 * Two shader predicates are computed up front from the shader's base.type
 * (the int16 at +0x24; type 4 is the one that owns the extra data block
 * fetched by shader_get_and_verify_type):
 *   - is_decal: type-4 shader whose byte at +0x28 has bit 0x8 set. When set,
 *     the part is dropped entirely: the caller's out block is reset to
 *     {NULL, NULL, 0xffff} and the function returns NULL.
 *   - use_pool: false only when the model path selector DAT_0047e000 is 1 AND
 *     the shader is not a type-4 shader with a non-zero int16 at +0x28.
 *
 * Then asserts shader / shader_type_is_valid_for_model / centroid /
 * local_parameters (lines 0x515-0x518) and picks the destination group:
 *   - use_pool path with bit 1 of local_parameters->flags set (forced on for
 *     decal shaders, which OR in 3): the shared static group at 0x47df58,
 *     with the pending-index global 0x47dfe8 reset to -1.
 *   - otherwise a freshly allocated group — the secondary pool when
 *     DAT_0047e000 == 1 and the shader is not type 4, else the primary
 *     transparent-geometry pool. Only the primary allocation is handed back to
 *     the caller as the return value; the secondary and static-group paths
 *     return NULL. Allocation failure reports "too many transparent geometry
 *     groups" once (latched in DAT_0047e006) and returns.
 *
 * When the caller supplied an out block it receives the group's presorted
 * index plus pointers to the group's two int16 sort keys at +0x94/+0x96.
 *
 * The group is then filled from local_parameters and the stack arguments; the
 * centroid is taken from the caller unless local_parameters has an effect
 * (int16 at +0x8c non-zero), in which case the effect's source object index
 * (+0x98) and its own centroid (+0x9c..+0xa4) are used instead. The sort key
 * at +0x70 is minus the dot product of the group's centroid, relative to the
 * reference point at 0x5a5bc8..0x5a5bd0, with the direction at
 * 0x5a5bd4..0x5a5bdc.
 *
 * Finally the geometry source fields (+0x60..+0x6c) are wired either straight
 * at local_parameters (static-group path, which draws immediately and requests
 * a profile section via DAT_00325173) or at one-shot copies of the vertex/
 * lighting/skinning blocks staged into the rasterizer memory pool and cached in
 * 0x47df48..0x47df54 behind the DAT_0047dffc latch.
 *
 * In shader-cost accounting mode (DAT_003256ba == 2) the part count, total and
 * peak of param_5, and the triangle cost reported by FUN_0017ed90(param_3,
 * param_6) are accumulated into 0x5a54e4..0x5a54f0.
 *
 * param_3..param_7 are opaque caller-supplied geometry words beyond how they
 * are stored and forwarded here, so they keep mechanical names. */
void *FUN_0016c090(void *shader, short param_2, int param_3, int param_4,
                   int param_5, int param_6, int param_7, float *centroid,
                   void *out)
{
  volatile rasterizer_model_block16_t zero;
  uint32_t flags;
  void *result;
  bool use_pool;
  bool is_decal;
  char *group;
  float *group_centroid;
  float dx;
  float dy;
  float dz;
  int *total_ptr;

  result = NULL;
  if (*(char *)0x3256c4 == 0 || *(char *)0x3256c5 == 0) {
    return result;
  }

  if (shader != NULL && *(short *)((char *)shader + 0x24) == 4 &&
      (*(char *)((char *)shader_get_and_verify_type(shader, 4) + 0x28) & 8) != 0) {
    is_decal = true;
  } else {
    is_decal = false;
  }

  if (*(short *)0x47e000 != 1 ||
      (shader != NULL && *(short *)((char *)shader + 0x24) == 4 &&
       *(short *)((char *)shader_get_and_verify_type(shader, 4) + 0x28) != 0)) {
    use_pool = true;
  } else {
    use_pool = false;
  }

  if (!is_decal) {
    if (shader == NULL) {
      display_assert(
        "shader",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x515,
        true);
      system_exit(-1);
    }

    if (shader_type_is_valid_for_model(*(uint16_t *)((char *)shader + 0x24)) ==
        0) {
      display_assert(
        "shader_type_is_valid_for_model(shader->base.type)",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x516,
        true);
      system_exit(-1);
    }

    if (centroid == NULL) {
      display_assert(
        "centroid",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x517,
        true);
      system_exit(-1);
    }

    if (*(void **)0x47dff8 == NULL) {
      display_assert(
        "local_parameters",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x518,
        true);
      system_exit(-1);
    }

    flags = **(uint32_t **)0x47dff8;
    if (use_pool) {
      if (shader_is_decal(shader) != 0) {
        flags = flags | 3;
      }
      if ((flags & 2) != 0) {
        group = (char *)0x47df58;
        *(uint32_t *)0x47dfe8 = 0xffffffff;
        goto have_group;
      }
    }

    if (*(short *)0x47e000 == 1 && *(short *)((char *)shader + 0x24) != 4) {
      group = (char *)rasterizer_secondary_geometry_group_new();
    } else {
      group = (char *)rasterizer_transparent_geometry_group_new();
      result = group;
    }

    if (out != NULL) {
      *(short *)((char *)out + 8) =
        rasterizer_transparent_geometry_group_to_presorted_index(
          (unsigned int)group);
      *(void **)out = group + 0x94;
      *(void **)((char *)out + 4) = group + 0x96;
    }

    if (group != NULL) {
    have_group:
      *(uint32_t *)group = flags;
      *(uint32_t *)(group + 4) = *(uint32_t *)(*(char **)0x47dff8 + 4);
      zero.d[0] = 0;
      zero.d[3] = (zero.d[2] = (zero.d[1] = 0));

      group_centroid = (float *)(group + 0x74);
      if (*(short *)((*(char **)0x47dff8) + 0x8c) == 0) {
        *(uint32_t *)(group + 8) = 0;
        *(rasterizer_model_block12_t *)group_centroid =
          *(rasterizer_model_block12_t *)centroid;
      } else {
        if (*(uint32_t *)((*(char **)0x47dff8) + 0x98) == 0) {
          display_assert(
            "local_parameters->effect.source_object_index!=0",
            "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
            0x54c, true);
          system_exit(-1);
        }
        *(uint32_t *)(group + 8) = *(uint32_t *)((*(char **)0x47dff8) + 0x98);
        *(rasterizer_model_block12_t *)group_centroid =
          *(rasterizer_model_block12_t *)((*(char **)0x47dff8) + 0x9c);
      }

      *(short *)(group + 0x10) = param_2;
      *(void **)(group + 0xc) = shader;
      *(rasterizer_model_block40_t *)(group + 0x14) =
        *(rasterizer_model_block40_t *)((*(char **)0x47dff8) + 0x8c);
      *(int *)(group + 0x44) = param_4;
      *(int *)(group + 0x48) = param_3;
      *(int *)(group + 0x50) = param_5;
      *(int *)(group + 0x54) = param_7;
      *(int *)(group + 0x58) = param_6;
      *(int *)(group + 0x4c) = 0;
      *(int *)(group + 0x5c) = 0;

      dx = group_centroid[0] - *(float *)0x5a5bc8;
      dy = group_centroid[1] - *(float *)0x5a5bcc;
      dz = group_centroid[2] - *(float *)0x5a5bd0;
      *(rasterizer_model_block16_t *)(group + 0x80) = zero;
      *(float *)(group + 0x70) =
        -(*(float *)0x5a5bdc * dz + *(float *)0x5a5bd8 * dy +
          *(float *)0x5a5bd4 * dx);

      *(uint32_t *)(group + 0x3c) = *(uint32_t *)((*(char **)0x47dff8) + 0xc4);
      *(uint32_t *)(group + 0x40) = *(uint32_t *)((*(char **)0x47dff8) + 0xc8);
      *(short *)(group + 0x94) = -1;
      *(short *)(group + 0x96) = -1;

      if (*(short *)0x47e000 == 1 && *(short *)((char *)shader + 0x24) != 4) {
        *(uint32_t *)(group + 0x98) =
          *(uint32_t *)((*(char **)0x47dff8) + 0x98);
        if (*(uint32_t *)(group + 0x98) == 0) {
          display_assert(
            "group->active_camouflage_transparent_source_object_index",
            "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
            0x56d, true);
          system_exit(-1);
        }
      } else {
        *(uint32_t *)(group + 0x98) = 0;
      }

      *(char *)(group + 0x9d) = *(char *)0x5a5570;

      if ((flags & 2) != 0) {
        {
          char *lp = *(char **)0x47dff8;
          *(uint32_t *)(group + 0x60) = *(uint32_t *)(lp + 8);
          *(short *)(group + 0x64) = *(short *)(lp + 0xc);
          *(char **)(group + 0x68) = lp + 0x10;
          *(char **)(group + 0x6c) = lp + 0x84;
        }
        rasterizer_transparent_geometry_groups_begin();
        rasterizer_transparent_geometry_group_draw(group, 0);
        rasterizer_transparent_geometry_groups_end();
        *(char *)0x325173 = 1;
      } else {
        if (*(char *)0x47dffc == 0) {
          char *lp = *(char **)0x47dff8;
          *(int *)0x47df54 = rasterizer_memory_pool_copy(
            *(int *)(lp + 8), *(short *)(lp + 0xc) * 0x34);
          *(short *)0x47df50 = *(short *)(lp + 0xc);
          *(int *)0x47df4c =
            rasterizer_memory_pool_copy((int)(lp + 0x10), 0x74);
          *(int *)0x47df48 = rasterizer_memory_pool_copy((int)(lp + 0x84), 8);
          *(char *)0x47dffc = 1;
        }
        *(uint32_t *)(group + 0x60) = *(uint32_t *)0x47df54;
        *(short *)(group + 0x64) = *(short *)0x47df50;
        *(uint32_t *)(group + 0x68) = *(uint32_t *)0x47df4c;
        *(uint32_t *)(group + 0x6c) = *(uint32_t *)0x47df48;
      }

      if (*(short *)0x3256ba == 2) {
        *(int *)0x5a54f0 = *(int *)0x5a54f0 + 1;
        total_ptr = (int *)0x5a54e8;
        *total_ptr = *total_ptr + param_5;
        if (param_5 > *(int *)0x5a54ec) {
          *(int *)0x5a54ec = param_5;
        }
        *(int *)0x5a54e4 =
          *(int *)0x5a54e4 + FUN_0017ed90((void *)param_3, (void *)param_6);
      }

    } else if (*(char *)0x47e006 == 0) {
      error(2, "### ERROR too many transparent geometry groups");
      *(char *)0x47e006 = 1;
    }

  } else if (out != NULL) {
    *(short *)((char *)out + 8) = (short)0xffff;
    *(void **)out = NULL;
    *(void **)((char *)out + 4) = NULL;
  }
  return result;
}

/* 0x16c5a0 — opaque model draw (PAL 2342 names it _rasterizer_model_draw).
 *
 * local_parameters (0x47dff8) fields read here, from the 2276 disassembly:
 *   +0x00 geometry flags (bit 2 no-fog, bit 3 no-zbuffer), +0x04 unique id
 *   (self-illumination random-phase seed), +0x0c int16 node matrix count,
 *   +0x50 int16 point light count, +0x5c..+0x68 reflection tint (argb),
 *   +0x84 animation (colors pointer at +0x84), +0xa8 effect shader,
 *   +0xac effect animation (values pointer at +0xb0), +0xb4 centroid,
 *   +0xc4/+0xc8 base map scale.
 * Shader-model flags byte +0x28: bit 0 detail after reflection, bit 1 two
 * sided, bit 2 not alpha tested, bit 3 alpha blended decal, bit 4 true
 * atmospheric fog. Asserts carry the 2276 line numbers of this TU. */
void FUN_0016c5a0(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6, int param_7)
{
  void *shader;
  char *local_parameters;
  char *shader_model;
  void *effect_shader;
  void *group;
  float *animation_values;
  float *external_color;
  float *tint;
  short intensity_exponent_source;
  short color_source;
  short vertex_type;
  short local_model_effect_type;
  bool alpha_blended_decal;
  uint32_t alpha_test_enable;
  uint32_t zbias;
  int vertex_shader_permutation;
  int vertex_shader_work;
  unsigned int seed;
  float camera_distance;
  float reflection_fraction;
  float self_illumination_phase;
  float self_illumination_fraction;
  float planar_fog_fraction;
  float fog_density;
  float one_minus_planar;
  float value;
  float color_delta[3];
  float self_illumination_color[3];
  float diffuse_change_color[3];
  float perpendicular[4];
  float parallel[4];
  float vertex_constants[12];
  float specular_constants[8];
  float cc0[4];
  float cc0_error[3];
  float cc1[3];
  uint32_t cc0_pixel;
  uint32_t cc0_error_pixel;
  uint32_t cc1_pixel;
  uint32_t diffuse_change_pixel;
  uint32_t self_illumination_pixel;

  shader = (void *)param_1;

  if (*(void **)0x476ab0 == NULL) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x29f,
      true);
    system_exit(-1);
  }

  if (*(char *)0x3256c4 == 0) {
    return;
  }

  if (*(void **)0x47dff8 == NULL) {
    display_assert(
      "local_parameters",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x2a3,
      true);
    system_exit(-1);
  }

  if (shader == NULL) {
    display_assert(
      "shader", "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
      0x2a4, true);
    system_exit(-1);
  }

  effect_shader = *(void **)(*(char **)0x47dff8 + 0xa8);
  if (effect_shader != NULL) {
    intensity_exponent_source = -1;
    if (*(short *)((char *)effect_shader + 0x24) == 10) {
      intensity_exponent_source =
        *(short *)((char *)shader_get_and_verify_type(effect_shader, 10) + 0x2c);
    }

    animation_values = *(float **)(*(char **)0x47dff8 + 0xb0);
    if (intensity_exponent_source < 1 || intensity_exponent_source > 4 ||
        animation_values == NULL ||
        animation_values[intensity_exponent_source - 1] != 0.0f) {
      local_parameters = *(char **)0x47dff8;
      group = FUN_0016c090(*(void **)(local_parameters + 0xa8), (short)param_2,
                           param_3, param_4, param_5, param_6, param_7,
                           (float *)(local_parameters + 0xb4), NULL);
      if (group != NULL) {
        *(int *)((char *)group + 0x6c) =
          rasterizer_memory_pool_alloc((int)(*(char **)0x47dff8 + 0xac), 8);
      }
    }
  }

  local_model_effect_type = *(short *)0x47e000;
  if (local_model_effect_type == 1) {
    if (*(short *)((char *)shader + 0x24) != 4) {
      display_assert(
        "shader->base.type==_shader_type_model",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x2d9,
        true);
      system_exit(-1);
    }
    rasterizer_model_transparent_geometry_submit(shader, (short)param_2, param_3, param_4, param_5, param_6,
                 param_7, (float *)(*(char **)0x47dff8 + 0xb4), NULL);
    FUN_001592e0(1);
    return;
  }

  if (local_model_effect_type != 0) {
    error(2, "### ERROR model effect type #%d can't render opaque shader",
          (int)local_model_effect_type);
    return;
  }

  if (*(short *)((char *)shader + 0x24) == 3) {
    rasterizer_model_draw_environment_shader(shader, (short)param_2,
                                             (void *)param_3, param_4, param_5,
                                             (void *)param_6, param_7);
  } else {
    shader_model = (char *)shader_get_and_verify_type(shader, 4);
    alpha_blended_decal =
      ((*(unsigned char *)(shader_model + 0x28) >> 3) & 1) != 0;

    if (*(short *)0x47e000 != 0) {
      display_assert(
        "local_model_effect_type==_render_model_effect_type_none",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x303,
        true);
      system_exit(-1);
    }

    local_parameters = *(char **)0x47dff8;
    camera_distance =
      (*(float *)(local_parameters + 0xb4) - *(float *)0x5a5bc8) *
        *(float *)0x5a5bd4 +
      (*(float *)(local_parameters + 0xb8) - *(float *)0x5a5bcc) *
        *(float *)0x5a5bd8 +
      *(float *)0x5a5bdc *
        (*(float *)(local_parameters + 0xbc) - *(float *)0x5a5bd0);

    if (*(float *)(shader_model + 0x140) != 0.0f) {
      reflection_fraction =
        (camera_distance - *(float *)(shader_model + 0x140)) /
        (*(float *)(shader_model + 0x13c) - *(float *)(shader_model + 0x140));
      if (reflection_fraction < 0.0f) {
        reflection_fraction = 0.0f;
      } else if (reflection_fraction > 1.0f) {
        reflection_fraction = 1.0f;
      }
    } else {
      reflection_fraction = 1.0f;
    }

    if ((*(unsigned char *)local_parameters & 8) != 0) {
      D3DDevice_SetRenderState_ZEnable(0);
      zbias = 0;
    } else {
      D3DDevice_SetRenderState_ZEnable(1);
      D3DDevice_SetRenderState_Simple(0x4035c, !alpha_blended_decal);
      *(uint32_t *)0x1fb798 = !alpha_blended_decal;
      D3DDevice_SetRenderState_Simple(0x40354, 0x203);
      *(uint32_t *)0x1fb77c = 0x203;
      zbias = alpha_blended_decal ? *(uint32_t *)0x32570c : 0;
    }
    D3DDevice_SetRenderState_ZBias(zbias);
    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
    *(uint32_t *)0x1fb7a4 = 0x10101;
    D3DDevice_SetRenderState_Simple(0x40304, alpha_blended_decal);
    *(uint32_t *)0x1fb784 = alpha_blended_decal;
    D3DDevice_SetRenderState_Simple(0x40344, 0x302);
    *(uint32_t *)0x1fb790 = 0x302;
    D3DDevice_SetRenderState_Simple(0x40348, 0x303);
    *(uint32_t *)0x1fb794 = 0x303;
    D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
    *(uint32_t *)0x1fb7c0 = 0x8006;
    if (!alpha_blended_decal &&
        (*(unsigned char *)(shader_model + 0x28) & 4) == 0) {
      alpha_test_enable = 1;
    } else {
      alpha_test_enable = 0;
    }
    D3DDevice_SetRenderState_Simple(0x40300, alpha_test_enable);
    *(uint32_t *)0x1fb788 = alpha_test_enable;
    D3DDevice_SetRenderState_Simple(0x40340, 0x7f);
    *(uint32_t *)0x1fb78c = 0x7f;

    rasterizer_set_texture(0, 0, 1, *(int *)(shader_model + 0xb0),
                           (short)param_2);
    D3DDevice_SetTextureStageState(0, 10, 1);
    D3DDevice_SetTextureStageState(0, 0xb, 1);
    D3DDevice_SetTextureStageState(0, 0xd, 2);
    D3DDevice_SetTextureStageState(0, 0xe, 2);
    D3DDevice_SetTextureStageState(0, 0xf, 2);
    rasterizer_set_texture(1, 0, 2, *(int *)(shader_model + 0xe8),
                           (short)param_2);
    D3DDevice_SetTextureStageState(1, 10, 1);
    D3DDevice_SetTextureStageState(1, 0xb, 1);
    D3DDevice_SetTextureStageState(1, 0xd, 2);
    D3DDevice_SetTextureStageState(1, 0xe, 2);
    D3DDevice_SetTextureStageState(1, 0xf, 2);
    rasterizer_set_texture(2, 0, 1, *(int *)(shader_model + 0xc8),
                           (short)param_2);
    SetTextureStageStateSmart(2, 10, 1);
    SetTextureStageStateSmart(2, 0xb, 1);
    SetTextureStageStateSmart(2, 0xd, 2);
    SetTextureStageStateSmart(2, 0xe, 2);
    SetTextureStageStateSmart(2, 0xf, 2);
    rasterizer_set_texture(3, 2, 0, *(int *)(shader_model + 0x170),
                           (short)param_2);
    SetTextureStageStateSmart(3, 10, 3);
    SetTextureStageStateSmart(3, 0xb, 3);
    SetTextureStageStateSmart(3, 0xc, 3);
    SetTextureStageStateSmart(3, 0xd, 2);
    SetTextureStageStateSmart(3, 0xe, 2);
    SetTextureStageStateSmart(3, 0xf, 2);

    seed = *(unsigned int *)(*(char **)0x47dff8 + 4);
    if ((*(unsigned char *)(shader_model + 0x6c) & 1) != 0) {
      self_illumination_phase = 0.0f;
    } else {
      self_illumination_phase = random_math_real(&seed);
    }

    if (*(float *)(shader_model + 0x74) == 0.0f) {
      display_assert(
        "shader_model->model.self_illumination_animation_period!=0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x360,
        true);
      system_exit(-1);
    }

    FUN_00013090((float *)(shader_model + 0x84), (float *)(shader_model + 0x78),
                 color_delta);
    self_illumination_fraction =
      FUN_0010a5e0(*(int16_t *)(shader_model + 0x72),
                   *(float *)0x5a5e18 / *(float *)(shader_model + 0x74) +
                     self_illumination_phase);
    vector3d_scale_add((real_point3d *)(shader_model + 0x78), (real_vector3d *)color_delta,
                       self_illumination_fraction, (real_point3d *)self_illumination_color);

    if (!(self_illumination_color[0] >= 0.0f &&
          self_illumination_color[0] <= 1.0f)) {
      display_assert(
        "self_illumination_color.red >=0.0f && "
        "self_illumination_color.red <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x36c,
        true);
      system_exit(-1);
    }
    if (!(self_illumination_color[1] >= 0.0f &&
          self_illumination_color[1] <= 1.0f)) {
      display_assert(
        "self_illumination_color.green>=0.0f && "
        "self_illumination_color.green<=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x36d,
        true);
      system_exit(-1);
    }
    if (!(self_illumination_color[2] >= 0.0f &&
          self_illumination_color[2] <= 1.0f)) {
      display_assert(
        "self_illumination_color.blue >=0.0f && "
        "self_illumination_color.blue <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x36e,
        true);
      system_exit(-1);
    }

    color_source = *(short *)(shader_model + 0x70);
    if (color_source > 0 && color_source < 5) {
      external_color = (float *)(*(char **)(*(char **)0x47dff8 + 0x84) +
                                 (color_source - 1) * 0xc);
      if (!(external_color[0] >= 0.0f && external_color[0] <= 1.0f)) {
        display_assert(
          "external_color->red >=0.0f && external_color->red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x377,
          true);
        system_exit(-1);
      }
      if (!(external_color[1] >= 0.0f && external_color[1] <= 1.0f)) {
        display_assert(
          "external_color->green>=0.0f && external_color->green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x378,
          true);
        system_exit(-1);
      }
      if (!(external_color[2] >= 0.0f && external_color[2] <= 1.0f)) {
        display_assert(
          "external_color->blue >=0.0f && external_color->blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x379,
          true);
        system_exit(-1);
      }
      self_illumination_color[0] =
        self_illumination_color[0] * external_color[0];
      self_illumination_color[1] =
        self_illumination_color[1] * external_color[1];
      self_illumination_color[2] =
        self_illumination_color[2] * external_color[2];
    }

    color_source = *(short *)(shader_model + 0x4c);
    if (color_source > 0 && color_source < 5) {
      external_color = (float *)(*(char **)(*(char **)0x47dff8 + 0x84) +
                                 (color_source - 1) * 0xc);
      diffuse_change_color[0] = external_color[0];
      diffuse_change_color[1] = external_color[1];
      diffuse_change_color[2] = external_color[2];
      if (!(diffuse_change_color[0] >= 0.0f &&
            diffuse_change_color[0] <= 1.0f)) {
        display_assert(
          "diffuse_change_color.red >=0.0f && diffuse_change_color.red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x388,
          true);
        system_exit(-1);
      }
      if (!(diffuse_change_color[1] >= 0.0f &&
            diffuse_change_color[1] <= 1.0f)) {
        display_assert(
          "diffuse_change_color.green>=0.0f && "
          "diffuse_change_color.green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x389,
          true);
        system_exit(-1);
      }
      if (!(diffuse_change_color[2] >= 0.0f &&
            diffuse_change_color[2] <= 1.0f)) {
        display_assert(
          "diffuse_change_color.blue >=0.0f && "
          "diffuse_change_color.blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x38a,
          true);
        system_exit(-1);
      }
    } else {
      diffuse_change_color[0] = (*(float **)0x2ee708)[0];
      diffuse_change_color[1] = (*(float **)0x2ee708)[1];
      diffuse_change_color[2] = (*(float **)0x2ee708)[2];
    }

    vertex_shader_work = *(int *)0x5a5558;
    local_parameters = *(char **)0x47dff8;
    if ((*(unsigned char *)(shader_model + 0x28) & 0x10) != 0) {
      vertex_shader_permutation = 2;
    } else if (*(char *)0x47e003 != 0) {
      vertex_shader_permutation = 0;
    } else if (*(short *)(local_parameters + 0x50) > 0) {
      vertex_shader_permutation = 1;
    } else if (*(short *)(local_parameters + 0xc) <= 1 &&
               (*(int *)(shader_model + 0xc8) == -1 ||
                (*(short *)(shader_model + 0xd6) == 0 &&
                 self_illumination_color[0] == 0.0f &&
                 self_illumination_color[1] == 0.0f &&
                 self_illumination_color[2] == 0.0f &&
                 diffuse_change_color[0] == 1.0f &&
                 diffuse_change_color[1] == 1.0f &&
                 diffuse_change_color[2] == 1.0f)) &&
               (*(int *)(shader_model + 0x170) == -1 ||
                !(reflection_fraction > 0.0f))) {
      vertex_shader_permutation = 3;
    } else {
      vertex_shader_permutation = 2;
    }

    if (param_6 != 0) {
      vertex_type = *(short *)param_6;
    } else {
      vertex_type = rasterizer_dynamic_vertices_get_type(param_7);
    }
    FUN_00178b40(10, vertex_type, vertex_shader_permutation);

    if (*(short *)0x3256ba >= 1) {
      *(int *)0x5a5568 =
        *(int *)0x5a5568 + (*(int *)0x5a5558 - vertex_shader_work);
      ((int *)0x5a5414)[(short)vertex_shader_permutation] =
        ((int *)0x5a5414)[(short)vertex_shader_permutation] +
        *(int *)(param_6 + 4);
    }

    local_parameters = *(char **)0x47dff8;
    perpendicular[0] = reflection_fraction * *(float *)(shader_model + 0x144) *
                       *(float *)(local_parameters + 0x5c);
    perpendicular[1] =
      *(float *)(shader_model + 0x148) * *(float *)(local_parameters + 0x60);
    perpendicular[2] =
      *(float *)(shader_model + 0x14c) * *(float *)(local_parameters + 0x64);
    perpendicular[3] =
      *(float *)(shader_model + 0x150) * *(float *)(local_parameters + 0x68);
    parallel[0] = reflection_fraction * *(float *)(shader_model + 0x154) *
                  *(float *)(local_parameters + 0x5c);
    parallel[1] =
      *(float *)(shader_model + 0x158) * *(float *)(local_parameters + 0x60);
    parallel[2] =
      *(float *)(shader_model + 0x15c) * *(float *)(local_parameters + 0x64);
    parallel[3] =
      *(float *)(shader_model + 0x160) * *(float *)(local_parameters + 0x68);

    vertex_constants[0] = *(float *)(shader_model + 0xd8);
    vertex_constants[1] =
      *(float *)(shader_model + 0xd8) * *(float *)(shader_model + 0xec);
    vertex_constants[2] = 1.0f;
    vertex_constants[3] = 1.0f;
    vertex_constants[4] = 1.0f;
    vertex_constants[5] = 0.0f;
    vertex_constants[6] = 0.0f;
    vertex_constants[7] = 0.0f;
    vertex_constants[8] = 0.0f;
    vertex_constants[9] = 1.0f;
    vertex_constants[10] = 0.0f;
    vertex_constants[11] = 0.0f;
    specular_constants[0] = perpendicular[1] - parallel[1];
    specular_constants[1] = perpendicular[2] - parallel[2];
    specular_constants[2] = perpendicular[3] - parallel[3];
    specular_constants[3] = perpendicular[0] - parallel[0];
    specular_constants[4] = parallel[1];
    specular_constants[5] = parallel[2];
    specular_constants[6] = parallel[3];
    specular_constants[7] = parallel[0];

    FUN_00190e10(
      shader_model + 0xfc, local_parameters + 0x84,
      *(float *)(local_parameters + 0xc4) * *(float *)(shader_model + 0x9c),
      *(float *)(local_parameters + 0xc8) * *(float *)(shader_model + 0xa0),
      0.0f, 0.0f, 0.0f, *(float *)0x5a5e18, &vertex_constants[4],
      &vertex_constants[8]);
    vertex_constants[10] = *(float *)(shader_model + 0x38);
    D3DDevice_SetVertexShaderConstant(-0x54, vertex_constants, 3);
    D3DDevice_SetVertexShaderConstant(-0x51, specular_constants, 2);

    tint = *(float **)0x47e4d0;
    if (tint != NULL && (tint[0] > 0.0f || tint[1] > 0.0f || tint[2] > 0.0f ||
                         tint[3] > 0.0f)) {
      specular_constants[0] = 0.0f;
      specular_constants[1] = 0.0f;
      specular_constants[2] = 0.0f;
      specular_constants[3] = 0.0f;
      specular_constants[4] = tint[0];
      specular_constants[5] = tint[1];
      specular_constants[6] = tint[2];
      specular_constants[7] = tint[3];
      D3DDevice_SetVertexShaderConstant(-0x51, specular_constants, 2);
    }

    if (!(*(float *)0x5a5dc0 > *(float *)0x5a5dbc)) {
      display_assert(
        "global_window_parameters.fog.atmospheric_maximum_distance>"
        "global_window_parameters.fog.atmospheric_minimum_distance",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x432,
        true);
      system_exit(-1);
    }
    if (!(*(float *)0x5a5dc0 > 0.0f)) {
      display_assert(
        "global_window_parameters.fog.atmospheric_maximum_distance>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x433,
        true);
      system_exit(-1);
    }
    if (!(*(float *)0x5a5de8 > 0.0f)) {
      display_assert(
        "global_window_parameters.fog.planar_maximum_distance>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x434,
        true);
      system_exit(-1);
    }
    if (!(*(float *)0x5a5dec > 0.0f)) {
      display_assert(
        "global_window_parameters.fog.planar_maximum_depth>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x435,
        true);
      system_exit(-1);
    }

    if (*(char *)0x3256d4 != 0 && (**(unsigned char **)0x47dff8 & 4) == 0) {
      planar_fog_fraction =
        plane3d_distance_to_point((const real_plane3d *)(0x5a5dc8), (const real_point3d *)(0x5a5bc8)) /
        *(float *)0x5a5dc0;
      if (planar_fog_fraction < 0.0f) {
        planar_fog_fraction = 0.0f;
      } else if (planar_fog_fraction > 1.0f) {
        planar_fog_fraction = 1.0f;
      }
      fog_density = (camera_distance - *(float *)0x5a5dbc) /
                    (*(float *)0x5a5dc0 - *(float *)0x5a5dbc);
      if (fog_density < 0.0f) {
        fog_density = 0.0f;
      } else if (fog_density > 1.0f) {
        fog_density = 1.0f;
      }
      fog_density = fog_density * *(float *)0x5a5db8;
      if ((*(unsigned char *)0x5a5da8 & 2) != 0) {
        planar_fog_fraction = 1.0f;
      }

      cc0[0] = 1.0f - fog_density;
      one_minus_planar = 1.0f - planar_fog_fraction;
      cc0[1] = *(float *)0x5a5dd8 - (planar_fog_fraction * *(float *)0x5a5dd8 +
                                     one_minus_planar * *(float *)0x5a5dac) *
                                      fog_density;
      cc0[2] = *(float *)0x5a5ddc - (*(float *)0x5a5db0 * one_minus_planar +
                                     *(float *)0x5a5ddc * planar_fog_fraction) *
                                      fog_density;
      cc0[3] = *(float *)0x5a5de0 - (one_minus_planar * *(float *)0x5a5db4 +
                                     *(float *)0x5a5de0 * planar_fog_fraction) *
                                      fog_density;

      value = -cc0[1];
      cc0_error[0] = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
      value = -cc0[2];
      cc0_error[1] = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
      value = -cc0[3];
      cc0_error[2] = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
      if (cc0[1] < 0.0f) {
        cc0[1] = 0.0f;
      } else if (cc0[1] > 1.0f) {
        cc0[1] = 1.0f;
      }
      if (cc0[2] < 0.0f) {
        cc0[2] = 0.0f;
      } else if (cc0[2] > 1.0f) {
        cc0[2] = 1.0f;
      }
      if (cc0[3] < 0.0f) {
        cc0[3] = 0.0f;
      } else if (cc0[3] > 1.0f) {
        cc0[3] = 1.0f;
      }
      cc1[0] = *(float *)0x5a5dac * fog_density;
      cc1[1] = *(float *)0x5a5db0 * fog_density;
      cc1[2] = *(float *)0x5a5db4 * fog_density;

      if (!(cc0[1] >= 0.0f && cc0[1] <= 1.0f)) {
        display_assert(
          "cc0.red >=0.0f && cc0.red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45b,
          true);
        system_exit(-1);
      }
      if (!(cc0[2] >= 0.0f && cc0[2] <= 1.0f)) {
        display_assert(
          "cc0.green>=0.0f && cc0.green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45c,
          true);
        system_exit(-1);
      }
      if (!(cc0[3] >= 0.0f && cc0[3] <= 1.0f)) {
        display_assert(
          "cc0.blue >=0.0f && cc0.blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45d,
          true);
        system_exit(-1);
      }
      if (!(cc1[0] >= 0.0f && cc1[0] <= 1.0f)) {
        display_assert(
          "cc1.red >=0.0f && cc1.red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45e,
          true);
        system_exit(-1);
      }
      if (!(cc1[1] >= 0.0f && cc1[1] <= 1.0f)) {
        display_assert(
          "cc1.green>=0.0f && cc1.green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45f,
          true);
        system_exit(-1);
      }
      if (!(cc1[2] >= 0.0f && cc1[2] <= 1.0f)) {
        display_assert(
          "cc1.blue >=0.0f && cc1.blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x460,
          true);
        system_exit(-1);
      }
      if (!(cc0_error[0] >= 0.0f && cc0_error[0] <= 1.0f)) {
        display_assert(
          "cc0_error.red >=0.0f && cc0_error.red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x461,
          true);
        system_exit(-1);
      }
      if (!(cc0_error[1] >= 0.0f && cc0_error[1] <= 1.0f)) {
        display_assert(
          "cc0_error.green>=0.0f && cc0_error.green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x462,
          true);
        system_exit(-1);
      }
      if (!(cc0_error[2] >= 0.0f && cc0_error[2] <= 1.0f)) {
        display_assert(
          "cc0_error.blue >=0.0f && cc0_error.blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x463,
          true);
        system_exit(-1);
      }

      cc0_pixel = real_argb_color_to_pixel32((const real_argb_color *)cc0);
      cc0_error_pixel = real_rgb_color_to_pixel32(cc0_error);
      cc1_pixel = real_rgb_color_to_pixel32(cc1);
    } else {
      cc1_pixel = 0xff000000;
      cc0_error_pixel = 0xff000000;
      cc0_pixel = 0xff000000;
    }

    diffuse_change_pixel = real_rgb_color_to_pixel32(diffuse_change_color);
    self_illumination_pixel =
      real_rgb_color_to_pixel32(self_illumination_color);
    FUN_0016ab00(*(int16_t *)(shader_model + 0xd4), self_illumination_pixel,
                 *(int16_t *)(shader_model + 0xd6), diffuse_change_pixel,
                 cc0_pixel, cc0_error_pixel, cc1_pixel,
                 (char)(*(unsigned char *)(shader_model + 0x28) & 1),
                 (char)((*(unsigned char *)(shader_model + 0x28) >> 4) & 1));
    D3DDevice_SetRenderState_CullMode(0x901);
    rasterizer_draw((const triangle_buffer *)param_3, param_4, 0, param_5,
                    (const vertex_buffer *)param_6, param_7);
    if (*(short *)0x3256ba == 2) {
      *(int *)0x5a54dc = *(int *)0x5a54dc + param_5;
      *(int *)0x5a54e0 = *(int *)0x5a54e0 + 1;
      *(int *)0x5a54d8 =
        *(int *)0x5a54d8 + FUN_0017ed90((void *)param_3, (void *)param_6);
    }

    if ((*(unsigned char *)(shader_model + 0x28) & 2) != 0) {
      vertex_constants[0] = *(float *)(shader_model + 0xd8);
      vertex_constants[1] =
        *(float *)(shader_model + 0xd8) * *(float *)(shader_model + 0xec);
      vertex_constants[2] = 1.0f;
      vertex_constants[3] = -1.0f;
      vertex_constants[4] = 1.0f;
      vertex_constants[5] = 0.0f;
      vertex_constants[6] = 0.0f;
      vertex_constants[7] = 0.0f;
      vertex_constants[8] = 0.0f;
      vertex_constants[9] = 1.0f;
      vertex_constants[10] = 0.0f;
      vertex_constants[11] = 0.0f;
      local_parameters = *(char **)0x47dff8;
      FUN_00190e10(
        shader_model + 0xfc, local_parameters + 0x84,
        *(float *)(local_parameters + 0xc4) * *(float *)(shader_model + 0x9c),
        *(float *)(local_parameters + 0xc8) * *(float *)(shader_model + 0xa0),
        0.0f, 0.0f, 0.0f, *(float *)0x5a5e18, &vertex_constants[4],
        &vertex_constants[8]);
      vertex_constants[10] = *(float *)(shader_model + 0x38);
      D3DDevice_SetVertexShaderConstant(-0x54, vertex_constants, 3);
      SetRenderStateSmart(0x7f, 0x900);
      rasterizer_draw((const triangle_buffer *)param_3, param_4, 0, param_5,
                      (const vertex_buffer *)param_6, param_7);
      if (*(short *)0x3256ba == 2) {
        *(int *)0x5a54e0 = *(int *)0x5a54e0 + 1;
        *(int *)0x5a54dc = *(int *)0x5a54dc + param_5;
        *(int *)0x5a54d8 =
          *(int *)0x5a54d8 + FUN_0017ed90((void *)param_3, (void *)param_6);
      }
    }
  }

  if (*(char *)0x47e004 != 0) {
    rasterizer_environment_fog_screen_model_submit(
      shader, (int16_t)param_2, (void *)param_3, param_4, param_5,
      (void *)param_6, param_7);
  }
}
