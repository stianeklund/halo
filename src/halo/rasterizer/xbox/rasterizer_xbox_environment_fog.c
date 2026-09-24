#include "x87_math.h"

/* c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment_fog.c
 *
 * Planar/atmospheric environment fog and the layered "fog screen".
 *
 * Structure and field names follow the PAL build 2342 reference
 * (halo-pal-2342 source/rasterizer/xbox/rasterizer_xbox_environment_fog.c);
 * every offset below was re-derived from the 2276 disassembly, and every
 * place where 2276 differs from PAL is called out at the function. */

#define MAXIMUM_WINDOWS 4
#define MAXIMUM_ENVIRONMENT_FOG_SCREEN_LAYERS 4
#define MAXIMUM_ENVIRONMENT_FOG_SCREEN_OPAQUE_MODELS 0x80
#define MAXIMUM_ATMOSPHERE_DOMINANT_WARNINGS 0x14

#define _error_silent 2

/* rasterizer_profile_begin/_end sections (rasterizer_profile_begin / rasterizer_profile_end). */
#define _rasterizer_profile_environment_fog 0x11
#define _rasterizer_profile_environment_fog_screen 0x12

/* rasterizer_set_vertex_shader_permutation vertex-shader indices. */
#define _rasterizer_vertex_shader_environment_fog 6
#define _rasterizer_vertex_shader_environment_fog_screen 8
#define _rasterizer_vertex_shader_screen_effect 0x26
#define _rasterizer_vertex_type_dynamic_screen 8

#define _shader_type_environment 3
#define _shader_type_transparent_chicago 4
#define _shader_type_transparent_generic 8

/* D3DTSS_* texture stage states and values (Xbox d3d8types.h). */
#define D3DTSS_ADDRESSU 0xa
#define D3DTSS_ADDRESSV 0xb
#define D3DTSS_MAGFILTER 0xd
#define D3DTSS_MINFILTER 0xe
#define D3DTSS_MIPFILTER 0xf
#define D3DTSS_ALPHAKILL 0x15
#define D3DTADDRESS_WRAP 1
#define D3DTADDRESS_CLAMP 3
#define D3DTEXF_LINEAR 2
#define D3DTALPHAKILL_DISABLE 0
#define D3DTALPHAKILL_ENABLE 4

#define D3DCULL_CCW 0x901
#define D3DBLEND_ONE 1
#define D3DBLEND_SRCALPHA 0x302
#define D3DBLEND_INVSRCALPHA 0x303
#define D3DBLEND_INVDESTALPHA 0x305
#define D3DBLENDOP_ADD 0x8006
#define D3DCMP_EQUAL 0x202
#define D3DCMP_LESSEQUAL 0x203
#define D3DCLEAR_ZBUFFER 0x1
#define D3DCLEAR_TARGET_A 0x80
#define D3DPT_TRIANGLEFAN 7
#define D3DVSDE_POSITION 0
#define D3DVSDE_SPECULAR 4

/* D3DDevice_SetRenderState_Simple NV097 method words; each simple state is
 * mirrored into the D3D render-state cache at 0x1fb698 + state * 4. */
#define NV097_SET_ALPHA_TEST_ENABLE_CMD 0x40300
#define NV097_SET_BLEND_ENABLE_CMD 0x40304
#define NV097_SET_ALPHA_REF_CMD 0x40340
#define NV097_SET_BLEND_FUNC_SFACTOR_CMD 0x40344
#define NV097_SET_BLEND_FUNC_DFACTOR_CMD 0x40348
#define NV097_SET_BLEND_EQUATION_CMD 0x40350
#define NV097_SET_DEPTH_FUNC_CMD 0x40354
#define NV097_SET_DEPTH_MASK_CMD 0x4035c
#define D3D_RENDER_STATE_ZFUNC (*(uint32_t *)0x1fb77c)
#define D3D_RENDER_STATE_ALPHABLENDENABLE (*(uint32_t *)0x1fb784)
#define D3D_RENDER_STATE_ALPHATESTENABLE (*(uint32_t *)0x1fb788)
#define D3D_RENDER_STATE_ALPHAREF (*(uint32_t *)0x1fb78c)
#define D3D_RENDER_STATE_SRCBLEND (*(uint32_t *)0x1fb790)
#define D3D_RENDER_STATE_DESTBLEND (*(uint32_t *)0x1fb794)
#define D3D_RENDER_STATE_ZWRITEENABLE (*(uint32_t *)0x1fb798)
#define D3D_RENDER_STATE_COLORWRITEENABLE (*(uint32_t *)0x1fb7a4)
#define D3D_RENDER_STATE_BLENDOP (*(uint32_t *)0x1fb7c0)

/* fog_screen.flags */
#define _fog_screen_no_environment_multipass_flag 0x1
#define _fog_screen_no_model_multipass_flag 0x2
#define _fog_screen_no_texture_flag 0x4
/* global_window_parameters.fog flag words */
#define _fog_definition_atmosphere_dominant_flag 0x2
#define _fog_runtime_use_fixed_screen_density_flag 0x1
/* shader_transparent_chicago.flags */
#define _shader_transparent_chicago_no_fog_flag 0x4

#define VSH_CONSTANTS__TEXSCALE_OFFSET (-0x54)
#define VSH_CONSTANTS__TEXSCALE_COUNT 3

/* Screen block of a 'fog ' tag. Only accessed offsets are named. */
typedef struct fog_screen {
  word flags; /* 0x00 */
  int16_t layer_count; /* 0x02 */
  real near_distance; /* 0x04 */
  real far_distance; /* 0x08 */
  real near_density; /* 0x0c */
  real far_density; /* 0x10 */
  real start_distance_from_fog_plane; /* 0x14 */
  byte pad_18[4];
  uint32_t color; /* 0x1c pixel32 */
  real rotation_multiplier; /* 0x20 */
  real strafing_multiplier; /* 0x24 */
  real zoom_multiplier; /* 0x28 */
  byte pad_2c[8];
  real map_scale; /* 0x34 */
  byte pad_38[0xc]; /* map tag_reference group/name */
  int map_index; /* 0x44 map tag_reference index */
  real animation_period; /* 0x48 */
  byte pad_4c[4];
  real wind_velocity_lower; /* 0x50 */
  real wind_velocity_upper; /* 0x54 */
  real wind_period_lower; /* 0x58 */
  real wind_period_upper; /* 0x5c */
  real wind_acceleration_weight; /* 0x60 */
  real wind_perpendicular_weight; /* 0x64 */
} fog_screen;
cs(fog_screen, 0x68);
co(fog_screen, layer_count, 0x02);
co(fog_screen, color, 0x1c);
co(fog_screen, map_scale, 0x34);
co(fog_screen, map_index, 0x44);
co(fog_screen, wind_velocity_lower, 0x50);
co(fog_screen, wind_perpendicular_weight, 0x64);

/* 0x5a5bc0. camera (+0x08) and frustum.world_to_view (+0x6c) are the
 * rasterizer_window_begin_parameters sub-blocks; fog starts at +0x1e8. */
typedef struct fog_window_parameters {
  int16_t rasterizer_target; /* 0x000 */
  int16_t window_index; /* 0x002 */
  byte pad_004[4];
  real camera_position[3]; /* 0x008 */
  real camera_forward[3]; /* 0x014 */
  byte pad_020[0x10];
  real camera_vertical_field_of_view; /* 0x030 */
  int16_t viewport_y0; /* 0x034 */
  int16_t viewport_x0; /* 0x036 */
  int16_t viewport_y1; /* 0x038 */
  int16_t viewport_x1; /* 0x03a */
  byte pad_03c[0x30];
  real world_to_view[13]; /* 0x06c real_matrix4x3 */
  byte pad_0a0[0x148];
  word fog_definition_flags; /* 0x1e8 */
  word fog_runtime_flags; /* 0x1ea */
  real fog_atmospheric_color[3]; /* 0x1ec */
  real fog_atmospheric_maximum_density; /* 0x1f8 */
  byte pad_1fc[4];
  real fog_atmospheric_maximum_distance; /* 0x200 */
  byte pad_204[4];
  real_plane3d fog_plane; /* 0x208 */
  real fog_planar_color[3]; /* 0x218 */
  real fog_planar_maximum_density; /* 0x224 */
  real fog_planar_maximum_distance; /* 0x228 */
  real fog_planar_maximum_depth; /* 0x22c */
  fog_screen *fog_screen; /* 0x230 */
  real fog_screen_external_intensity; /* 0x234 */
} fog_window_parameters;
co(fog_window_parameters, camera_position, 0x008);
co(fog_window_parameters, camera_vertical_field_of_view, 0x030);
co(fog_window_parameters, viewport_y0, 0x034);
co(fog_window_parameters, world_to_view, 0x06c);
co(fog_window_parameters, fog_definition_flags, 0x1e8);
co(fog_window_parameters, fog_atmospheric_maximum_distance, 0x200);
co(fog_window_parameters, fog_plane, 0x208);
co(fog_window_parameters, fog_planar_maximum_depth, 0x22c);
co(fog_window_parameters, fog_screen_external_intensity, 0x234);

typedef struct fog_matrix4x3 {
  real scale;
  real forward[3];
  real left[3];
  real up[3];
  real position[3]; /* 0x28 */
} fog_matrix4x3;
cs(fog_matrix4x3, 0x34);

typedef struct fog_screen_layer {
  real u;
  real v;
} fog_screen_layer;

typedef struct fog_screen_wind {
  real_vector2d direction; /* 0x00 */
  real magnitude; /* 0x08 */
  real_vector2d target_direction; /* 0x0c */
  real target_magnitude; /* 0x14 */
  real change_time; /* 0x18 */
  real change_period; /* 0x1c */
} fog_screen_wind;
cs(fog_screen_wind, 0x20);

typedef struct fog_screen_window {
  word animation_index; /* 0x00 */
  word pad_02;
  real rotation; /* 0x04 */
  real base_z; /* 0x08 */
  fog_screen_layer layers[MAXIMUM_ENVIRONMENT_FOG_SCREEN_LAYERS]; /* 0x0c */
  fog_screen_wind wind; /* 0x2c */
} fog_screen_window;
cs(fog_screen_window, 0x4c);
co(fog_screen_window, layers, 0x0c);
co(fog_screen_window, wind, 0x2c);

typedef struct fog_model_begin_parameters {
  byte pad_00[8];
  const real *node_matrices; /* 0x08 skinning.node_matrices */
  int16_t node_matrix_count; /* 0x0c skinning.node_matrix_count */
  byte pad_0e[0xa6];
  real centroid[3]; /* 0xb4 */
  byte pad_c0[4];
  real_vector2d base_map_scale; /* 0xc4 */
} fog_model_begin_parameters;
co(fog_model_begin_parameters, node_matrix_count, 0x0c);
co(fog_model_begin_parameters, centroid, 0xb4);
co(fog_model_begin_parameters, base_map_scale, 0xc4);

typedef struct fog_transparent_geometry_group {
  byte pad_00[0xc];
  void *shader; /* 0x0c */
  int16_t shader_permutation_index; /* 0x10 */
  byte pad_12[0x2a];
  real_vector2d model_base_map_scale; /* 0x3c */
  int dynamic_triangle_buffer_index; /* 0x44 */
  void *triangle_buffer; /* 0x48 */
  int first_triangle_index; /* 0x4c */
  int triangle_count; /* 0x50 */
  int dynamic_vertex_buffer_index; /* 0x54 */
  void *vertex_buffer; /* 0x58 */
  byte pad_5c[4];
  const real *node_matrices; /* 0x60 */
  int16_t node_matrix_count; /* 0x64 */
  word pad_66;
  void *lighting; /* 0x68 */
  void *animation; /* 0x6c */
  byte pad_70[0x30];
} fog_transparent_geometry_group;
cs(fog_transparent_geometry_group, 0xa0);
co(fog_transparent_geometry_group, model_base_map_scale, 0x3c);
co(fog_transparent_geometry_group, vertex_buffer, 0x58);
co(fog_transparent_geometry_group, animation, 0x6c);

typedef struct fog_model_skinning_parameters {
  const real *node_matrices;
  int16_t node_matrix_count;
  word pad_06;
} fog_model_skinning_parameters;
cs(fog_model_skinning_parameters, 0x8);

typedef struct fog_shader_header {
  byte pad_00[0x24];
  int16_t type; /* 0x24 shader.type */
} fog_shader_header;

typedef struct fog_shader_transparent_chicago {
  byte pad_00[0x28];
  byte flags; /* 0x28 */
  byte pad_29[0x73];
  real map_u_scale; /* 0x9c */
  real map_v_scale; /* 0xa0 */
  byte pad_a4[0xc];
  int map_index; /* 0xb0 map tag_reference index */
} fog_shader_transparent_chicago;
co(fog_shader_transparent_chicago, map_u_scale, 0x9c);
co(fog_shader_transparent_chicago, map_index, 0xb0);

/* 0x5a5ac0, the shared 0xf0-byte pixel-shader state block. */
typedef struct fog_pixel_shader_definition {
  uint32_t alpha_inputs[8]; /* 0x00 */
  uint32_t final_combiner_inputs_abcd; /* 0x20 */
  uint32_t final_combiner_inputs_efg; /* 0x24 */
  uint32_t constant_0[8]; /* 0x28 */
  uint32_t constant_1[8]; /* 0x48 */
  uint32_t alpha_outputs[8]; /* 0x68 */
  uint32_t rgb_inputs[8]; /* 0x88 */
  uint32_t compare_mode; /* 0xa8 */
  uint32_t final_combiner_constant_0; /* 0xac */
  uint32_t final_combiner_constant_1; /* 0xb0 */
  uint32_t rgb_outputs[8]; /* 0xb4 */
  uint32_t combiner_count; /* 0xd4 */
  uint32_t texture_modes; /* 0xd8 */
  byte pad_dc[0x14];
} fog_pixel_shader_definition;
cs(fog_pixel_shader_definition, 0xf0);
co(fog_pixel_shader_definition, final_combiner_constant_0, 0xac);
co(fog_pixel_shader_definition, combiner_count, 0xd4);

/* 0x47dcb0, rasterizer_environment_fog_screen_globals. */
typedef struct fog_screen_globals {
  int16_t cached_node_matrix_count; /* 0x000 */
  word pad_002;
  const real *cached_node_matrices; /* 0x004 */
  real previous_camera_matrix[MAXIMUM_WINDOWS][13]; /* 0x008 */
  boolean local_environment_fog_screen_model_flag; /* 0x0d8 */
  boolean local_environment_fog_screen_flag; /* 0x0d9 */
  byte pad_0da[2];
  word local_fog_screen_layer_bitmap_indices
    [MAXIMUM_ENVIRONMENT_FOG_SCREEN_LAYERS]; /* 0x0dc */
  real local_fog_screen_layer_colors[MAXIMUM_ENVIRONMENT_FOG_SCREEN_LAYERS]
                                    [3]; /* 0x0e4 */
  real local_fog_eye_density; /* 0x114 */
  int16_t local_fog_pass; /* 0x118 */
  byte pad_11a[6];
  fog_screen_window windows[MAXIMUM_WINDOWS]; /* 0x120 */
  fog_transparent_geometry_group *opaque_model_submit_parameters; /* 0x250 */
  int opaque_model_count; /* 0x254 */
  boolean fog_screen_active[MAXIMUM_WINDOWS]; /* 0x258 */
  byte pad_25c[4];
  uint32_t last_frame_index[MAXIMUM_WINDOWS][2]; /* 0x260 64-bit */
  int16_t atmosphere_dominant_warning_count; /* 0x280 */
  boolean reported_bad_animation_index; /* 0x282 */
  byte pad_283;
  fog_model_begin_parameters *model; /* 0x284 */
  boolean model_parameters_cached; /* 0x288 */
  byte pad_289[3];
  void *cached_lighting; /* 0x28c */
  void *cached_animation; /* 0x290 */
  boolean reported_too_many_opaque_models; /* 0x294 */
} fog_screen_globals;
co(fog_screen_globals, previous_camera_matrix, 0x008);
co(fog_screen_globals, local_environment_fog_screen_model_flag, 0x0d8);
co(fog_screen_globals, local_fog_screen_layer_bitmap_indices, 0x0dc);
co(fog_screen_globals, local_fog_screen_layer_colors, 0x0e4);
co(fog_screen_globals, local_fog_eye_density, 0x114);
co(fog_screen_globals, local_fog_pass, 0x118);
co(fog_screen_globals, windows, 0x120);
co(fog_screen_globals, opaque_model_submit_parameters, 0x250);
co(fog_screen_globals, fog_screen_active, 0x258);
co(fog_screen_globals, last_frame_index, 0x260);
co(fog_screen_globals, atmosphere_dominant_warning_count, 0x280);
co(fog_screen_globals, model, 0x284);
co(fog_screen_globals, cached_lighting, 0x28c);
co(fog_screen_globals, reported_too_many_opaque_models, 0x294);

#define fog_globals (*(fog_screen_globals *)0x47dcb0)
#define global_window_parameters (*(fog_window_parameters *)0x5a5bc0)
#define global_pixel_shader (*(fog_pixel_shader_definition *)0x5a5ac0)
#define global_d3d_device (*(void **)0x476ab0)
/* global_rasterizer_data: tag_reference indices of the atmospheric (+0x2c)
 * and planar (+0x3c) fog density bitmaps. */
#define global_rasterizer_data (*(char **)0x476204)
#define global_identity4x3 (*(fog_matrix4x3 **)0x31fc60)
#define local_fog_screen_first_time (*(boolean *)0x325172)
/* global_frame_parameters */
#define global_game_time_sec (*(real *)0x5a5e18)
#define global_frame_dt (*(real *)0x5a5e1c)
/* rasterizer_globals.fps_accumulation_frame_index (64-bit) */
#define rasterizer_frame_index ((uint32_t *)0x325668)
/* rasterizer_debug_options (0x3256b8) */
#define debug_statistics_mode (*(int16_t *)0x3256ba)
#define debug_drawing_mode (*(int16_t *)0x3256bc)
#define debug_draw_environment_fog (*(boolean *)0x3256d4)
#define debug_draw_environment_fog_screen (*(boolean *)0x3256d5)
#define debug_draw_water (*(boolean *)0x3256d6)
#define _rasterizer_statistics_mode_enabled 2
/* rasterizer_frame_statistics */
#define stats_environment_fog_dynamic_vertex_count (*(int *)0x5a54ac)
#define stats_environment_fog_dynamic_triangle_count (*(int *)0x5a54b0)
#define stats_environment_fog_dynamic_draw_count (*(int *)0x5a54b4)
#define stats_environment_fog_screen_dynamic_vertex_count (*(int *)0x5a54b8)
#define stats_environment_fog_screen_dynamic_triangle_count (*(int *)0x5a54bc)
#define stats_environment_fog_screen_dynamic_draw_count (*(int *)0x5a54c0)
#define stats_environment_fog_screen_model_count (*(int *)0x5a54c4)
#define stats_environment_fog_screen_static_vertex_count (*(int *)0x5a54c8)
#define stats_environment_fog_screen_static_triangle_count (*(int *)0x5a54cc)
#define stats_environment_fog_screen_static_draw_count (*(int *)0x5a54d0)

/* MSVC CRT pow(): compiles to the _CIpow intrinsic (0x1d9e70). */
double pow(double x, double y);

static const char kFogFile[] =
  "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment_fog.c";

/* PAL real_local_random() is a random_math.h inline here: every use calls
 * random_math_get_local_seed_address + random_math_real (0x9ce80 is not
 * called from this object). */
#define FOG_LOCAL_RANDOM_REAL() \
  random_math_real(random_math_get_local_seed_address())

/* 0x165980 -- dead out-of-line instantiation of the D3D8 inline
 * IDirect3DDevice8_Clear wrapper (no callers). RET 0x14: five stack dwords,
 * the first being the never-read device pointer; Color arrives in EDX and
 * Stencil in EAX (both pushed without a prior write). Push order into
 * D3DDevice_Clear: EAX, [ebp+0x18], EDX, [ebp+0x14], [ebp+0x10], [ebp+0xc]. */
int IDirect3DDevice8_Clear(void *device, uint32_t count, void *rects, uint32_t flags,
                 uint32_t color, float z, uint32_t stencil)
{
  (void)device;
  D3DDevice_Clear(count, rects, flags, color, z, stencil);
  return 0;
}

/* 0x1659a0 rasterizer_environment_fog_screen_initialize */
bool rasterizer_environment_fog_screen_initialize(void)
{
  boolean result = 1;

  fog_globals.opaque_model_submit_parameters =
    (fog_transparent_geometry_group *)debug_malloc(0x5000, false, kFogFile,
                                                   0xf8);
  fog_globals.opaque_model_count = 0;
  if (!fog_globals.opaque_model_submit_parameters) {
    error(_error_silent, "### ERROR failed to allocate opaque model geometry "
                         "buffer for environment fog screen");
    result = 0;
  }
  return result;
}

/* 0x1659f0 rasterizer_environment_fog_screen_window_begin */
void rasterizer_environment_fog_screen_window_begin(void)
{
  fog_globals.opaque_model_count = 0;
}

/* 0x165a00 rasterizer_environment_fog_screen_window_end */
void rasterizer_environment_fog_screen_window_end(void)
{
}

/* 0x165a10 rasterizer_environment_fog_screen_dispose */
void rasterizer_environment_fog_screen_dispose(void)
{
  if (fog_globals.opaque_model_submit_parameters) {
    debug_free(fog_globals.opaque_model_submit_parameters, kFogFile, 0x117);
  }
}

/* 0x165cb0 _rasterizer_environment_fog_draw. The shader permutation index
 * (arg2) is not read. */
void __rasterizer_environment_fog_draw(void *shader, int arg2, int arg3, int arg4, int arg5,
                  void *vertex_buffer)
{
  (void)arg2;
  if (!global_d3d_device) {
    display_assert("global_d3d_device", kFogFile, 0x1a7, 1);
    system_exit(-1);
  }
  if (debug_drawing_mode == 0 && debug_draw_environment_fog) {
    if (!shader) {
      display_assert("shader", kFogFile, 0x1ae, 1);
      system_exit(-1);
    }
    shader_get_and_verify_type(shader, _shader_type_environment);
    if (!vertex_buffer) {
      display_assert("vertex_buffer", kFogFile, 0x1b3, 1);
      system_exit(-1);
    }
    rasterizer_set_vertex_shader_permutation(_rasterizer_vertex_shader_environment_fog,
                 (word)((struct vertex_buffer *)vertex_buffer)->type,
                 shader_get_vertex_shader_permutation(shader));
    rasterizer_draw_dynamic_triangles_static_vertices(
      arg3, arg4, arg5, (const struct vertex_buffer *)vertex_buffer);
    if (debug_statistics_mode == _rasterizer_statistics_mode_enabled) {
      stats_environment_fog_dynamic_draw_count++;
      stats_environment_fog_dynamic_triangle_count += arg5;
      stats_environment_fog_dynamic_vertex_count +=
        rasterizer_frame_statistics_count_dynamic_vertices(arg3, arg4, arg5);
    }
  }
}

/* 0x165dd0 _rasterizer_environment_fog_end */
void __rasterizer_environment_fog_end(void)
{
  rasterizer_profile_end(_rasterizer_profile_environment_fog);
}

/* 0x165de0 _rasterizer_environment_fog_screen_wind_get_vector */
void __rasterizer_environment_fog_screen_wind_get_vector(int16_t index, float scale, float *out)
{
  fog_screen_wind *wind = &fog_globals.windows[index].wind;

  if (!(index >= 0 && index < MAXIMUM_WINDOWS)) {
    display_assert("window_index>=0 && window_index<MAXIMUM_WINDOWS", kFogFile,
                   0x1e4, 1);
    system_exit(-1);
  }
  if (!out) {
    display_assert("wind_vector", kFogFile, 0x1e5, 1);
    system_exit(-1);
  }
  out[0] = wind->magnitude * wind->direction.i * scale;
  out[1] = wind->direction.j * wind->magnitude * scale;
  out[2] = 0.0f;
}

/* 0x165ea0 rasterizer_environment_fog_screen_model_submit. Queues an opaque
 * model group for the fog-screen depth pass (7 cdecl stack args, caller
 * 0x16d9a4 cleans 0x1c). */
void rasterizer_environment_fog_screen_model_submit(
  void *shader, int16_t shader_permutation_index, void *triangle_buffer,
  int dynamic_triangle_buffer_index, int triangle_count, void *vertex_buffer,
  int dynamic_vertex_buffer_index)
{
  fog_transparent_geometry_group *group;
  fog_model_begin_parameters *model;

  if (debug_draw_environment_fog_screen && debug_drawing_mode == 0) {
    if (fog_globals.opaque_model_count <
        MAXIMUM_ENVIRONMENT_FOG_SCREEN_OPAQUE_MODELS) {
      group =
        &fog_globals
           .opaque_model_submit_parameters[fog_globals.opaque_model_count++];
      group->shader = shader;
      group->triangle_buffer = triangle_buffer;
      group->shader_permutation_index = shader_permutation_index;
      group->dynamic_triangle_buffer_index = dynamic_triangle_buffer_index;
      group->vertex_buffer = vertex_buffer;
      group->triangle_count = triangle_count;
      group->first_triangle_index = 0;
      group->dynamic_vertex_buffer_index = dynamic_vertex_buffer_index;
      model = fog_globals.model;
      group->model_base_map_scale = model->base_map_scale;
      if (!fog_globals.model_parameters_cached) {
        fog_globals.cached_node_matrices =
          (const real *)rasterizer_memory_alloc_const(
            (int)model->node_matrices, model->node_matrix_count * 0x34);
        fog_globals.cached_node_matrix_count =
          fog_globals.model->node_matrix_count;
        fog_globals.model_parameters_cached = 1;
      }
      group->node_matrices = fog_globals.cached_node_matrices;
      group->node_matrix_count = fog_globals.cached_node_matrix_count;
      group->lighting = fog_globals.cached_lighting;
      group->animation = fog_globals.cached_animation;
    } else if (!fog_globals.reported_too_many_opaque_models) {
      error(_error_silent,
            "### ERROR too many opaque model groups obscuring fog screen "
            "(max=#%d)",
            MAXIMUM_ENVIRONMENT_FOG_SCREEN_OPAQUE_MODELS);
      fog_globals.reported_too_many_opaque_models = 1;
    }
  }
}

/* 0x165fc0 rasterizer_environment_fog_screen_model_end */
void rasterizer_environment_fog_screen_model_end(void)
{
  fog_globals.model = 0;
}

/* 0x165fd0 set_real_vector4d (no callers in 2276). */
float *set_real_vector4d(float *vector, float i, float j, float k, float l)
{
  vector[0] = i;
  vector[1] = j;
  vector[2] = k;
  vector[3] = l;
  return vector;
}

/* 0x166010 rasterizer_environment_fog_screen_is_active (static in PAL as
 * rasterizer_environment_fog_screen_active). Recomputes the per-window
 * active flag and local_fog_eye_density; last_frame_index is compared but
 * never written in this build (same as PAL). */
bool rasterizer_environment_fog_screen_is_active(void)
{
  int16_t window_index = global_window_parameters.window_index;
  fog_screen *screen;
  real depth;
  real distance;
  real z;
  real base_z;

  if (!(window_index >= 0 && window_index < MAXIMUM_WINDOWS)) {
    return 0;
  }
  fog_globals.fog_screen_active[window_index] = 0;
  if (rasterizer_frame_index[0] !=
        fog_globals.last_frame_index[window_index][0] ||
      rasterizer_frame_index[1] !=
        fog_globals.last_frame_index[window_index][1]) {
    if (!(global_window_parameters.fog_planar_maximum_depth != 0.0f)) {
      display_assert("global_window_parameters.fog.planar_maximum_depth!=0.0f",
                     kFogFile, 0x5c, 1);
      system_exit(-1);
    }
    if (debug_drawing_mode == 0 && debug_draw_environment_fog_screen &&
        global_window_parameters.rasterizer_target == 0 &&
        (screen = global_window_parameters.fog_screen) != 0 &&
        screen->layer_count > 0 && screen->map_index != -1 &&
        screen->far_distance != 0.0f && screen->far_density != 0.0f &&
        (!(global_window_parameters.fog_runtime_flags &
           _fog_runtime_use_fixed_screen_density_flag) ||
         global_window_parameters.fog_screen_external_intensity > 0.0f)) {
      depth = global_window_parameters.fog_planar_maximum_depth;
      distance = global_window_parameters.fog_plane.normal[2] *
                   global_window_parameters.camera_position[2] +
                 global_window_parameters.fog_plane.normal[1] *
                   global_window_parameters.camera_position[1] +
                 global_window_parameters.fog_plane.normal[0] *
                   global_window_parameters.camera_position[0] -
                 global_window_parameters.fog_plane.d;
      z = screen->start_distance_from_fog_plane;
      base_z = -depth;
      if (z == base_z) {
        z = 0.0001f - depth;
      }
      if (global_window_parameters.fog_runtime_flags &
          _fog_runtime_use_fixed_screen_density_flag) {
        fog_globals.local_fog_eye_density =
          global_window_parameters.fog_screen_external_intensity;
      } else {
        real density = (distance - z) / (base_z - z);

        fog_globals.local_fog_eye_density =
          density < 0.0f ? 0.0f : (density > 1.0f ? 1.0f : density);
      }
      if (fog_globals.local_fog_eye_density > 0.0f) {
        fog_globals.fog_screen_active[window_index] = 1;
      }
    }
  }
  return fog_globals.fog_screen_active[window_index];
}

/* 0x166210 rasterizer_environment_fog_screen_wind_update. PAL static; the
 * 2276 compiler passed screen in EBX and wind in ESI (kb.json @<reg>).
 * local_random_boolean (0x165ff0, uncalled) is inlined here. */
void rasterizer_environment_fog_screen_wind_update(void *screen_data,
                                                   void *wind_data)
{
  fog_screen *screen = (fog_screen *)screen_data;
  fog_screen_wind *wind = (fog_screen_wind *)wind_data;
  real_vector2d *target_direction;
  real weight;
  real acceleration_weight;

  if (!screen) {
    display_assert("screen", kFogFile, 0xa3, 1);
    system_exit(-1);
  }
  if (!wind) {
    display_assert("wind", kFogFile, 0xa4, 1);
    system_exit(-1);
  }
  if (screen->wind_velocity_upper > 0.0f) {
    target_direction = &wind->target_direction;
    weight = 1.0f - screen->wind_acceleration_weight;
    wind->direction.i *= weight;
    wind->direction.j *= weight;
    acceleration_weight = screen->wind_acceleration_weight;
    wind->direction.i += acceleration_weight * target_direction->i;
    wind->direction.j += acceleration_weight * target_direction->j;
    if (normalize2d(&wind->direction.i) == 0.0f) {
      wind->direction.i = 1.0f;
      wind->direction.j = 0.0f;
    }
    scalars_interpolate(wind->magnitude, wind->target_magnitude,
                        screen->wind_acceleration_weight, &wind->magnitude);
    if (global_game_time_sec - wind->change_time >= wind->change_period) {
      real_vector2d perpendicular;
      real sign;
      real scale;

      weight = (real)pow(FOG_LOCAL_RANDOM_REAL(),
                         1.0f - screen->wind_perpendicular_weight);
      sign = random_seed_step(random_math_get_local_seed_address()) > 0x8000 ?
               -1.0f :
               1.0f;
      perpendicular2d(&wind->direction.i, &perpendicular.i);
      scale = sign * weight;
      perpendicular.i *= scale;
      perpendicular.j *= scale;
      target_direction->i =
        (1.0f - weight) * wind->direction.i + perpendicular.i;
      target_direction->j =
        (1.0f - weight) * wind->direction.j + perpendicular.j;
      if (normalize2d(&target_direction->i) == 0.0f) {
        target_direction->i = 1.0f;
        target_direction->j = 0.0f;
      }
      wind->target_magnitude = random_real_range(
        (int *)random_math_get_local_seed_address(),
        screen->wind_velocity_lower, screen->wind_velocity_upper);
      wind->change_time = global_game_time_sec;
      wind->change_period =
        random_real_range((int *)random_math_get_local_seed_address(),
                          screen->wind_period_lower, screen->wind_period_upper);
    }
  }
}

/* 0x166400 _rasterizer_environment_fog_begin */
void __rasterizer_environment_fog_begin(void)
{
  real atmospheric_eye_density;
  real planar_eye_density;
  real distance;
  real density;

  if (!(global_window_parameters.fog_atmospheric_maximum_distance > 0.0f)) {
    display_assert(
      "global_window_parameters.fog.atmospheric_maximum_distance>0.0f",
      kFogFile, 0x122, 1);
    system_exit(-1);
  }
  if (!(global_window_parameters.fog_planar_maximum_distance > 0.0f)) {
    display_assert("global_window_parameters.fog.planar_maximum_distance>0.0f",
                   kFogFile, 0x123, 1);
    system_exit(-1);
  }
  if (!(global_window_parameters.fog_planar_maximum_depth > 0.0f)) {
    display_assert("global_window_parameters.fog.planar_maximum_depth>0.0f",
                   kFogFile, 0x124, 1);
    system_exit(-1);
  }
  if (!global_d3d_device) {
    display_assert("global_d3d_device", kFogFile, 0x125, 1);
    system_exit(-1);
  }
  rasterizer_profile_begin(_rasterizer_profile_environment_fog);
  if (debug_drawing_mode == 0 && debug_draw_environment_fog) {
    distance = global_window_parameters.fog_plane.normal[2] *
                 global_window_parameters.camera_position[2] +
               global_window_parameters.fog_plane.normal[1] *
                 global_window_parameters.camera_position[1] +
               global_window_parameters.fog_plane.normal[0] *
                 global_window_parameters.camera_position[0] -
               global_window_parameters.fog_plane.d;
    density =
      distance / global_window_parameters.fog_atmospheric_maximum_distance;
    atmospheric_eye_density =
      density < 0.0f ? 0.0f : (density > 1.0f ? 1.0f : density);
    density = -(distance / global_window_parameters.fog_planar_maximum_depth);
    planar_eye_density =
      density < 0.0f ? 0.0f : (density > 1.0f ? 1.0f : density);
    if (global_window_parameters.fog_definition_flags &
        _fog_definition_atmosphere_dominant_flag) {
      atmospheric_eye_density = 1.0f;
      if (distance < -0.3f && fog_globals.atmosphere_dominant_warning_count <
                                MAXIMUM_ATMOSPHERE_DOMINANT_WARNINGS) {
        error(_error_silent,
              "### WARNING camera went below atmosphere-dominant fog plane");
        fog_globals.atmosphere_dominant_warning_count++;
      }
    }

    rasterizer_set_texture_direct(0, *(int *)(global_rasterizer_data + 0x2c),
                                  0);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
    rasterizer_set_texture_direct(1, *(int *)(global_rasterizer_data + 0x3c),
                                  0);
    D3DDevice_SetTextureStageState(1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetTextureStageState(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetTextureStageState(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

    D3DDevice_SetRenderState_CullMode(D3DCULL_CCW);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGB);
    D3D_RENDER_STATE_COLORWRITEENABLE = NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_ENABLE_CMD, 1);
    D3D_RENDER_STATE_ALPHABLENDENABLE = 1;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_SFACTOR_CMD,
                                    D3DBLEND_ONE);
    D3D_RENDER_STATE_SRCBLEND = D3DBLEND_ONE;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_DFACTOR_CMD,
                                    D3DBLEND_INVSRCALPHA);
    D3D_RENDER_STATE_DESTBLEND = D3DBLEND_INVSRCALPHA;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_EQUATION_CMD,
                                    D3DBLENDOP_ADD);
    D3D_RENDER_STATE_BLENDOP = D3DBLENDOP_ADD;
    D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_TEST_ENABLE_CMD, 1);
    D3D_RENDER_STATE_ALPHATESTENABLE = 1;
    D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_REF_CMD, 0);
    D3D_RENDER_STATE_ALPHAREF = 0;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(NV097_SET_DEPTH_FUNC_CMD, D3DCMP_EQUAL);
    D3D_RENDER_STATE_ZFUNC = D3DCMP_EQUAL;
    D3DDevice_SetRenderState_Simple(NV097_SET_DEPTH_MASK_CMD, 0);
    D3D_RENDER_STATE_ZWRITEENABLE = 0;
    D3DDevice_SetRenderState_ZBias(0);

    csmemset(&global_pixel_shader, 0, sizeof(global_pixel_shader));
    global_pixel_shader.texture_modes = 0x21;
    global_pixel_shader.combiner_count = 0x11002;
    global_pixel_shader.constant_0[0] = real_alpha_intensity_to_pixel32(
      global_window_parameters.fog_atmospheric_maximum_density *
        atmospheric_eye_density,
      global_window_parameters.fog_atmospheric_maximum_density);
    global_pixel_shader.constant_1[0] = real_alpha_intensity_to_pixel32(
      global_window_parameters.fog_planar_maximum_density * planar_eye_density,
      (1.0f - planar_eye_density) *
        global_window_parameters.fog_planar_maximum_density);
    global_pixel_shader.alpha_inputs[0] = 0x2191209;
    global_pixel_shader.alpha_outputs[0] = 0xc00;
    global_pixel_shader.rgb_inputs[0] = 0x11180118;
    global_pixel_shader.rgb_outputs[0] = 0x48;
    global_pixel_shader.constant_0[1] = real_a_rgb_color_to_pixel32(
      atmospheric_eye_density, global_window_parameters.fog_atmospheric_color);
    global_pixel_shader.constant_1[1] =
      real_rgb_color_to_pixel32(global_window_parameters.fog_planar_color);
    global_pixel_shader.alpha_inputs[1] = 0x283c311c;
    global_pixel_shader.alpha_outputs[1] = 0xcd;
    global_pixel_shader.rgb_inputs[1] = 0x108021c;
    global_pixel_shader.rgb_outputs[1] = 0xcd;
    global_pixel_shader.final_combiner_inputs_abcd = 0xc3d000f;
    global_pixel_shader.final_combiner_inputs_efg = 0xd243c00;
    rasterizer_set_pixel_shader(&global_pixel_shader);
  }
}

/* 0x166890 _rasterizer_environment_fog_screen_begin.
 * 2276 differs from PAL: the scroll offset is a short, the per-window
 * wind vector goes through the rasterizer.obj wrapper rasterizer_environment_fog_screen_wind_get_vector, the
 * error dump reinterprets floats directly (no csmemcpy), and the ALPHAKILL
 * reset plus the failure report sit inside the opaque-model branch. */
void __rasterizer_environment_fog_screen_begin(int16_t pass)
{
  fog_screen_window *window;
  fog_screen *screen;
  boolean success;
  boolean clear_z_buffer;
  real vsh_constants__texscale[VSH_CONSTANTS__TEXSCALE_COUNT][4];

  if (!(pass == 0 || pass == 1)) {
    display_assert("pass==0 || pass==1", kFogFile, 0x1f3, 1);
    system_exit(-1);
  }
  if (!global_d3d_device) {
    display_assert("global_d3d_device", kFogFile, 0x1f4, 1);
    system_exit(-1);
  }
  fog_globals.local_fog_pass = pass;
  if (pass == 0) {
    rasterizer_profile_begin(_rasterizer_profile_environment_fog_screen);
  }
  if (!rasterizer_environment_fog_screen_is_active()) {
    return;
  }
  window = &fog_globals.windows[global_window_parameters.window_index];
  screen = global_window_parameters.fog_screen;
  if (!(fog_globals.local_fog_eye_density > 0.0f)) {
    display_assert("local_fog_eye_density>0.0f", kFogFile, 0x202, 1);
    system_exit(-1);
  }

  if (pass == 0) {
    real *previous_camera_matrix =
      fog_globals.previous_camera_matrix[global_window_parameters.window_index];
    fog_matrix4x3 wind_matrix = *global_identity4x3;
    real screen_constants[5][4];
    real matrix[13];
    real texture_transforms[MAXIMUM_ENVIRONMENT_FOG_SCREEN_LAYERS][8];
    real vector[3];
    real layer_spacing;
    real inverse_depth;
    real aspect_ratio;
    real projection_scale;
    real inverse_width;
    real inverse_height;
    real cosine;
    real sine;
    int16_t viewport_height;
    int16_t viewport_width;
    int16_t layer;

    if (local_fog_screen_first_time) {
      int16_t window_index;

      tag_get(TAG_GROUP_BITM, screen->map_index);
      for (window_index = 0; window_index < MAXIMUM_WINDOWS; window_index++) {
        csmemset(&fog_globals.windows[window_index], 0,
                 sizeof(fog_screen_window));
        for (layer = 0; layer < screen->layer_count; layer++) {
          fog_screen_layer *layer_state =
            &fog_globals.windows[window_index].layers[layer];

          layer_state->v = FOG_LOCAL_RANDOM_REAL();
          layer_state->u = FOG_LOCAL_RANDOM_REAL();
        }
        /* copies the first matrix-sized bytes of the camera verbatim */
        csmemcpy(fog_globals.previous_camera_matrix[window_index],
                 global_window_parameters.camera_position, 0x34);
      }
      local_fog_screen_first_time = 0;
    }

    rasterizer_environment_fog_screen_wind_update(screen, &window->wind);
    rasterizer_environment_fog_screen_wind_get_vector((word)global_window_parameters.window_index, global_frame_dt,
                 vector);
    wind_matrix.position[0] = vector[0];
    wind_matrix.position[1] = vector[1];
    matrix_inverse(previous_camera_matrix, matrix);
    matrix4x3_multiply(&wind_matrix.scale, matrix, matrix);
    matrix4x3_multiply(global_window_parameters.world_to_view, matrix, matrix);
    csmemcpy(
      fog_globals.previous_camera_matrix[global_window_parameters.window_index],
      global_window_parameters.world_to_view, 0x34);

    layer_spacing = (screen->far_distance - screen->near_distance) /
                    (real)screen->layer_count;
    inverse_depth = 1.0f / (screen->far_distance - screen->near_distance);
    viewport_height = global_window_parameters.viewport_y1 -
                      global_window_parameters.viewport_y0;
    viewport_width = global_window_parameters.viewport_x1 -
                     global_window_parameters.viewport_x0;
    aspect_ratio = (real)viewport_height / (real)viewport_width;
    projection_scale =
      screen->map_scale * 0.5f /
      (x87_fptan(global_window_parameters.camera_vertical_field_of_view *
                 0.5f) *
       screen->far_distance * aspect_ratio);

    inverse_width = 1.0f / (real)viewport_width;
    screen_constants[0][0] = inverse_width * 2.0f;
    screen_constants[0][1] = 0.0f;
    screen_constants[0][2] = 0.0f;
    screen_constants[0][3] = -1.0f - inverse_width;
    inverse_height = 1.0f / (real)viewport_height;
    screen_constants[1][0] = 0.0f;
    screen_constants[1][1] = -2.0f * inverse_height;
    screen_constants[1][2] = 0.0f;
    screen_constants[1][3] = inverse_height + 1.0f;
    screen_constants[2][0] = 0.0f;
    screen_constants[2][1] = 0.0f;
    screen_constants[2][2] = 0.0f;
    screen_constants[2][3] = 0.5f;
    screen_constants[3][0] = 0.0f;
    screen_constants[3][1] = 0.0f;
    screen_constants[3][2] = 0.0f;
    screen_constants[3][3] = 1.0f;
    screen_constants[4][0] = 0.0f;
    screen_constants[4][1] = 0.0f;
    screen_constants[4][2] = 0.0f;
    screen_constants[4][3] = 1.0f;

    vector[0] = 1.0f;
    vector[1] = 0.0f;
    vector[2] = 0.0f;
    matrix_transform_vector(matrix, vector, vector);
    window->rotation -=
      -x87_fatan2f(vector[1], vector[0]) * screen->rotation_multiplier;
    cosine = x87_fcos(window->rotation);
    sine = x87_fsin(window->rotation);

    if (screen->far_distance - screen->near_distance > 0.01f) {
      int16_t offset;

      window->base_z -= screen->zoom_multiplier / layer_spacing * matrix[12];
      offset = (int16_t)x87_round_to_int((real)floor(window->base_z));
      if (offset > 0) {
        real u = FOG_LOCAL_RANDOM_REAL();
        real v = FOG_LOCAL_RANDOM_REAL();
        int16_t index =
          (int16_t)((word)(window->animation_index - 1) % screen->layer_count);

        window->layers[index].u = v;
        window->layers[index].v = u;
      } else if (offset < 0) {
        real u = FOG_LOCAL_RANDOM_REAL();
        real v = FOG_LOCAL_RANDOM_REAL();
        int16_t index = (int16_t)window->animation_index;

        window->layers[index].u = v;
        window->layers[index].v = u;
      }
      window->animation_index =
        (word)((word)(window->animation_index - offset) % screen->layer_count);
      window->base_z -= (real)offset;
      if (!(window->base_z >= 0.0f && window->base_z < 1.0f)) {
        error(_error_silent,
              "### ERROR fog_screen: base z failure (near=%f, far=%f, z=%f, "
              "offset=%d) -- tell Bernie!",
              (double)screen->near_distance, (double)screen->far_distance,
              (double)window->base_z, (int)offset);
        return;
      }
    }

    {
      real animation_times[MAXIMUM_ENVIRONMENT_FOG_SCREEN_LAYERS];
      real phase[MAXIMUM_ENVIRONMENT_FOG_SCREEN_LAYERS];

      phase[0] = 0.0f;
      phase[1] = 0.7135f;
      phase[2] = 0.3422f;
      phase[3] = 0.5798f;
      for (layer = 0; layer < screen->layer_count; layer++) {
        char *bitmap_group = (char *)tag_get(TAG_GROUP_BITM, screen->map_index);

        if (!(bitmap_group && *(int *)(bitmap_group + 0x60) > 0)) {
          display_assert("bitmap_group && bitmap_group->bitmaps.count>0",
                         kFogFile, 0x28d, 1);
          system_exit(-1);
        }
        if (screen->animation_period > 0.0f) {
          int16_t animation_index;
          real t = (real) * (int *)(bitmap_group + 0x60) * phase[layer] +
                   global_game_time_sec / screen->animation_period;
          real animation_time =
            (real)((t - floor(t)) < 0.0 ?
                     0.0 :
                     ((t - floor(t)) > 1.0 ? 1.0 : (t - floor(t))));

          animation_times[layer] = animation_time;
          animation_index = (int16_t)(x87_round_to_int((real)floor(t)) %
                                      *(int *)(bitmap_group + 0x60));
          if (animation_index < 0) {
            if (!fog_globals.reported_bad_animation_index) {
              error(_error_silent, "### ERROR fog screen animation index is "
                                   "invalid -- tell Bernie!!");
              error(_error_silent, "\tindex=%d", (int)animation_index);
              error(_error_silent, "\ttime=%f[%x]",
                    (double)global_game_time_sec,
                    *(int *)&global_game_time_sec);
              error(_error_silent, "\tperiod=%f[%x]",
                    (double)screen->animation_period,
                    *(int *)&screen->animation_period);
              error(_error_silent, "\tphase=%f[%x]", (double)phase[layer],
                    *(int *)&phase[layer]);
              error(_error_silent, "\tt=%f[%x]", (double)t, *(int *)&t);
              error(_error_silent, "\tanimation_time=%f[%x]",
                    (double)animation_time, *(int *)&animation_times[layer]);
              fog_globals.reported_bad_animation_index = 1;
            }
            animation_index = 0;
          }
          if (!(animation_index >= 0 &&
                animation_index < *(int *)(bitmap_group + 0x60))) {
            display_assert("animation_index>=0 && "
                           "animation_index<bitmap_group->bitmaps.count",
                           kFogFile, 0x2aa, 1);
            system_exit(-1);
          }
          fog_globals.local_fog_screen_layer_bitmap_indices[layer] =
            animation_index;
        } else {
          animation_times[layer] = 0.0f;
          fog_globals.local_fog_screen_layer_bitmap_indices[layer] =
            (word)(layer % *(int *)(bitmap_group + 0x60));
        }
      }

      for (layer = 0; layer < screen->layer_count; layer++) {
        int16_t index = (int16_t)((word)(window->animation_index + layer) %
                                  screen->layer_count);
        real z = ((real)layer + window->base_z) * layer_spacing +
                 screen->near_distance;
        real density =
          (real)pow(
            1.0 - pow(fabs((z - screen->near_distance) * inverse_depth * 2.0f -
                           1.0f),
                      3.0),
            2.0) *
          (fog_globals.local_fog_eye_density * screen->far_density);
        real scale = screen->map_scale / screen->far_distance * z;
        real *color = fog_globals.local_fog_screen_layer_colors[layer];

        vector[0] = 0.0f;
        vector[1] = 0.0f;
        vector[2] = -z;
        matrix_transform_point(matrix, vector, vector);
        window->layers[index].u -= (vector[1] * sine + vector[0] * cosine) *
                                   screen->strafing_multiplier *
                                   projection_scale;
        window->layers[index].v -= (vector[1] * cosine - vector[0] * sine) *
                                   screen->strafing_multiplier *
                                   projection_scale;
        if (screen->animation_period != 0.0f) {
          real inverse_time = 1.0f - animation_times[index];

          color[0] = inverse_time * inverse_time * density;
          color[1] = 2.0f * animation_times[index] * inverse_time * density;
          color[2] = animation_times[index] * animation_times[index] * density;
        } else {
          color[0] = 0.0f;
          color[1] = density;
          color[2] = 0.0f;
        }
        texture_transforms[layer][0] = scale * cosine * 0.5f;
        texture_transforms[layer][1] = scale * sine * aspect_ratio * 0.5f;
        texture_transforms[layer][2] = 0.0f;
        texture_transforms[layer][3] = window->layers[index].u;
        texture_transforms[layer][4] = scale * sine * -0.5f;
        texture_transforms[layer][5] = scale * cosine * aspect_ratio * 0.5f;
        texture_transforms[layer][6] = 0.0f;
        texture_transforms[layer][7] = window->layers[index].v;
      }
    }

    D3DDevice_SetVertexShaderConstant(-0x51, texture_transforms, 8);
    D3DDevice_SetVertexShaderConstant(-0x44, screen_constants, 5);

    if (screen->near_density == screen->far_density) {
      fog_globals.local_environment_fog_screen_flag = 0;
      fog_globals.local_environment_fog_screen_model_flag = 0;
      return;
    }
    fog_globals.local_environment_fog_screen_flag =
      !(global_window_parameters.fog_screen->flags &
        _fog_screen_no_environment_multipass_flag);
    fog_globals.local_environment_fog_screen_model_flag =
      !(global_window_parameters.fog_screen->flags &
        _fog_screen_no_model_multipass_flag) &&
      fog_globals.opaque_model_count > 0;
  }

  if (fog_globals.local_environment_fog_screen_flag ||
      fog_globals.local_environment_fog_screen_model_flag) {
    if (pass == 0) {
      if (screen->flags & _fog_screen_no_environment_multipass_flag) {
        clear_z_buffer = !(screen->flags & _fog_screen_no_model_multipass_flag);
      } else {
        clear_z_buffer =
          ((screen->flags & _fog_screen_no_model_multipass_flag) &&
           fog_globals.opaque_model_count > 0) ||
          (rasterizer_water_get_visibility_for_window() && debug_draw_water);
      }
    } else {
      clear_z_buffer = 0;
    }

    /* the inline IDirect3DDevice8_Clear always returns S_OK */
    D3DDevice_Clear(0, 0,
                    D3DCLEAR_TARGET_A | (clear_z_buffer ? D3DCLEAR_ZBUFFER : 0),
                    0, 1.0f, 0);
    success = 1;
    D3DDevice_SetRenderState_CullMode(D3DCULL_CCW);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_ALPHA);
    D3D_RENDER_STATE_COLORWRITEENABLE = NV097_COLOR_MASK_ALPHA;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_ENABLE_CMD, 0);
    D3D_RENDER_STATE_ALPHABLENDENABLE = 0;
    D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_TEST_ENABLE_CMD, 0);
    D3D_RENDER_STATE_ALPHATESTENABLE = 0;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(
      NV097_SET_DEPTH_FUNC_CMD, pass != 0 ? D3DCMP_EQUAL : D3DCMP_LESSEQUAL);
    D3D_RENDER_STATE_ZFUNC = pass != 0 ? D3DCMP_EQUAL : D3DCMP_LESSEQUAL;
    D3DDevice_SetRenderState_Simple(NV097_SET_DEPTH_MASK_CMD, pass == 0);
    D3D_RENDER_STATE_ZWRITEENABLE = pass == 0;
    D3DDevice_SetRenderState_ZBias(0);

    if (pass == 0) {
      real inverse_depth =
        1.0f / (screen->far_distance - screen->near_distance);

      vsh_constants__texscale[1][0] = 0.0f;
      vsh_constants__texscale[1][1] = 0.0f;
      vsh_constants__texscale[1][2] = 0.0f;
      vsh_constants__texscale[1][3] = 0.0f;
      vsh_constants__texscale[2][0] = 0.0f;
      vsh_constants__texscale[2][1] = 0.0f;
      vsh_constants__texscale[2][2] = 0.0f;
      vsh_constants__texscale[2][3] = 0.0f;
      vsh_constants__texscale[0][0] =
        global_window_parameters.camera_forward[0] * inverse_depth;
      vsh_constants__texscale[0][1] =
        global_window_parameters.camera_forward[1] * inverse_depth;
      vsh_constants__texscale[0][2] =
        global_window_parameters.camera_forward[2] * inverse_depth;
      vsh_constants__texscale[0][3] =
        -((global_window_parameters.camera_position[0] *
             global_window_parameters.camera_forward[0] +
           global_window_parameters.camera_forward[2] *
             global_window_parameters.camera_position[2] +
           global_window_parameters.camera_forward[1] *
             global_window_parameters.camera_position[1] +
           screen->near_distance) *
          inverse_depth);
      D3DDevice_SetVertexShaderConstant(VSH_CONSTANTS__TEXSCALE_OFFSET,
                                        vsh_constants__texscale, 1);
      success = 1;
    }

    csmemset(&global_pixel_shader, 0, sizeof(global_pixel_shader));
    if (global_window_parameters.fog_screen->flags &
        _fog_screen_no_texture_flag) {
      global_pixel_shader.combiner_count = 1;
      global_pixel_shader.final_combiner_inputs_efg = 0x3300;
    } else {
      real alpha;
      real alpha_scale;

      rasterizer_set_texture_direct(0, *(int *)(global_rasterizer_data + 0x3c),
                                    0);
      D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
      D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
      D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
      if (!(screen->far_density != 0.0f)) {
        display_assert("screen->far_density!=0.0f", kFogFile, 0x358, 1);
        system_exit(-1);
      }
      global_pixel_shader.texture_modes = 1;
      global_pixel_shader.combiner_count = 1;
      /* inline real_alpha_to_pixel32 (..\bitmaps\bitmaps_inlines.h) */
      alpha_scale = 255.0f;
      alpha = screen->near_density / screen->far_density;
      if (!(alpha >= 0.0f && alpha <= 1.0f)) {
        display_assert("alpha>=0.0f && alpha<=1.0f",
                       "..\\bitmaps\\bitmaps_inlines.h", 0x123, 1);
        system_exit(-1);
      }
      global_pixel_shader.constant_0[0] =
        (uint32_t)x87_round_to_int(alpha * alpha_scale) << 0x18;
      global_pixel_shader.alpha_inputs[0] = 0x28110820;
      global_pixel_shader.alpha_outputs[0] = 0xc00;
      global_pixel_shader.final_combiner_inputs_efg = 0x3c00;
    }
    rasterizer_set_pixel_shader(&global_pixel_shader);

    if (fog_globals.local_environment_fog_screen_model_flag) {
      int16_t group_index;

      for (group_index = 0; group_index < fog_globals.opaque_model_count;
           group_index++) {
        fog_transparent_geometry_group *group =
          &fog_globals.opaque_model_submit_parameters[group_index];
        fog_shader_header *shader = (fog_shader_header *)group->shader;
        fog_shader_transparent_chicago *chicago;
        fog_model_skinning_parameters skinning;

        if (shader->type == _shader_type_transparent_chicago &&
            !((chicago = (fog_shader_transparent_chicago *)shader_get_and_verify_type(
                 shader, _shader_type_transparent_chicago))
                ->flags &
              _shader_transparent_chicago_no_fog_flag)) {
          D3DDevice_SetRenderState_PSTextureModes(0x21);
          D3DDevice_SetTextureStageState(1, D3DTSS_ALPHAKILL,
                                         D3DTALPHAKILL_ENABLE);
          rasterizer_set_texture(1, 0, 1, chicago->map_index,
                                 group->shader_permutation_index);
          D3DDevice_SetTextureStageState(1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
          D3DDevice_SetTextureStageState(1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
          D3DDevice_SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
          D3DDevice_SetTextureStageState(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
          D3DDevice_SetTextureStageState(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
          vsh_constants__texscale[1][0] =
            chicago->map_u_scale * group->model_base_map_scale.i;
          vsh_constants__texscale[1][1] = 0.0f;
          vsh_constants__texscale[1][2] = 0.0f;
          vsh_constants__texscale[1][3] = 0.0f;
          vsh_constants__texscale[2][0] = 0.0f;
          vsh_constants__texscale[2][1] =
            chicago->map_v_scale * group->model_base_map_scale.j;
          vsh_constants__texscale[2][2] = 0.0f;
          vsh_constants__texscale[2][3] = 0.0f;
          D3DDevice_SetVertexShaderConstant(VSH_CONSTANTS__TEXSCALE_OFFSET + 1,
                                            vsh_constants__texscale[1],
                                            VSH_CONSTANTS__TEXSCALE_COUNT - 1);
          if (success) {
            success = 1;
          } else {
            success = 0;
            rasterizer_error(0, "IDirect3DDevice8_SetVertexShaderConstant("
                                "global_d3d_device, "
                                "VSH_CONSTANTS__TEXSCALE_OFFSET+1, "
                                "vsh_constants__texscale_1, "
                                "VSH_CONSTANTS__TEXSCALE_COUNT-1)");
          }
        } else if (shader->type == _shader_type_transparent_chicago ||
                   shader->type == _shader_type_environment ||
                   shader->type == _shader_type_transparent_generic) {
          D3DDevice_SetRenderState_PSTextureModes(1);
          D3DDevice_SetTextureStageState(1, D3DTSS_ALPHAKILL,
                                         D3DTALPHAKILL_DISABLE);
        }
        rasterizer_set_vertex_shader_permutation(5, rasterizer_transparent_geometry_get_primary_vertex_type(group), 0);
        if (group->node_matrices && group->node_matrix_count) {
          skinning.node_matrices = group->node_matrices;
          skinning.node_matrix_count = group->node_matrix_count;
        } else {
          skinning.node_matrices = &global_identity4x3->scale;
          skinning.node_matrix_count = 1;
        }
        rasterizer_set_model_skinning(&skinning);
        if (success) {
          rasterizer_transparent_geometry_group_draw__internal(group, 0);
          if (debug_statistics_mode == _rasterizer_statistics_mode_enabled) {
            stats_environment_fog_screen_static_draw_count++;
            stats_environment_fog_screen_static_triangle_count +=
              group->triangle_count;
            stats_environment_fog_screen_static_vertex_count +=
              rasterizer_frame_statistics_count_static_vertices(group->triangle_buffer, group->vertex_buffer);
          }
        }
      }
      D3DDevice_SetTextureStageState(1, D3DTSS_ALPHAKILL,
                                     D3DTALPHAKILL_DISABLE);
      if (!success) {
        error(_error_silent,
              "### ERROR rasterizer_environment_fog_screen_begin failed");
      }
    }
  }
}

/* 0x1677d0 _rasterizer_environment_fog_screen_draw. The shader permutation
 * index (arg2) is not read. */
void __rasterizer_environment_fog_screen_draw(void *shader, int arg2, int arg3, int arg4, int arg5,
                  void *arg6)
{
  (void)arg2;
  if (!global_d3d_device) {
    display_assert("global_d3d_device", kFogFile, 0x3dc, 1);
    system_exit(-1);
  }
  if (rasterizer_environment_fog_screen_is_active()) {
    if (!(fog_globals.local_fog_pass == 0 || fog_globals.local_fog_pass == 1)) {
      display_assert("local_fog_pass==0 || local_fog_pass==1", kFogFile, 0x3e0,
                     1);
      system_exit(-1);
    }
    if (fog_globals.local_environment_fog_screen_flag) {
      if (!global_window_parameters.fog_screen) {
        display_assert("global_window_parameters.fog.screen", kFogFile, 0x3e4,
                       1);
        system_exit(-1);
      }
      if (!(global_window_parameters.window_index >= 0 &&
            global_window_parameters.window_index < MAXIMUM_WINDOWS)) {
        display_assert("global_window_parameters.window_index>=0 && "
                       "global_window_parameters.window_index<MAXIMUM_WINDOWS",
                       kFogFile, 0x3e5, 1);
        system_exit(-1);
      }
      rasterizer_set_vertex_shader_permutation(_rasterizer_vertex_shader_environment_fog_screen,
                   (word)((struct vertex_buffer *)arg6)->type,
                   shader_get_vertex_shader_permutation(shader));
      rasterizer_draw_dynamic_triangles_static_vertices(
        arg3, arg4, arg5, (const struct vertex_buffer *)arg6);
      if (debug_statistics_mode == _rasterizer_statistics_mode_enabled) {
        stats_environment_fog_screen_dynamic_draw_count++;
        stats_environment_fog_screen_dynamic_triangle_count += arg5;
        stats_environment_fog_screen_dynamic_vertex_count +=
          rasterizer_frame_statistics_count_dynamic_vertices(arg3, arg4, arg5);
      }
    }
  }
}

/* 0x167920 _rasterizer_environment_fog_screen_end */
void __rasterizer_environment_fog_screen_end(void)
{
  fog_screen *screen;
  fog_screen_window *window;
  boolean fog_screen_drawn;
  int16_t layer;
  int16_t layer_count;

  if (!global_d3d_device) {
    display_assert("global_d3d_device", kFogFile, 0x40c, 1);
    system_exit(-1);
  }
  if (rasterizer_environment_fog_screen_is_active()) {
    screen = global_window_parameters.fog_screen;
    window = &fog_globals.windows[global_window_parameters.window_index];
    fog_screen_drawn = fog_globals.local_environment_fog_screen_flag ||
                       fog_globals.local_environment_fog_screen_model_flag;
    if (!(fog_globals.local_fog_pass == 0 || fog_globals.local_fog_pass == 1)) {
      display_assert("local_fog_pass==0 || local_fog_pass==1", kFogFile, 0x415,
                     1);
      system_exit(-1);
    }
    if (!global_window_parameters.fog_screen) {
      display_assert("global_window_parameters.fog.screen", kFogFile, 0x417, 1);
      system_exit(-1);
    }
    if (!(global_window_parameters.window_index >= 0 &&
          global_window_parameters.window_index < MAXIMUM_WINDOWS)) {
      display_assert("global_window_parameters.window_index>=0 && "
                     "global_window_parameters.window_index<MAXIMUM_WINDOWS",
                     kFogFile, 0x418, 1);
      system_exit(-1);
    }

    for (layer = 0; layer < screen->layer_count; layer++) {
      word animation_index = (word)(window->animation_index + layer);
      int16_t layer_index = (int16_t)(animation_index % screen->layer_count);

      rasterizer_set_texture_direct(
        layer, screen->map_index,
        fog_globals.local_fog_screen_layer_bitmap_indices[layer_index]);
      D3DDevice_SetTextureStageState(layer, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
      D3DDevice_SetTextureStageState(layer, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
      D3DDevice_SetTextureStageState(layer, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(layer, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
      D3DDevice_SetTextureStageState(layer, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
    }

    D3DDevice_SetRenderState_CullMode(D3DCULL_CCW);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    fog_globals.local_fog_pass == 0 &&
                                        fog_screen_drawn ?
                                      NV097_COLOR_MASK_ALPHA :
                                      NV097_COLOR_MASK_RGB);
    D3D_RENDER_STATE_COLORWRITEENABLE =
      fog_globals.local_fog_pass == 0 && fog_screen_drawn ?
        NV097_COLOR_MASK_ALPHA :
        NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_ENABLE_CMD, 1);
    D3D_RENDER_STATE_ALPHABLENDENABLE = 1;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_SFACTOR_CMD,
                                    fog_screen_drawn ? D3DBLEND_INVDESTALPHA :
                                                       D3DBLEND_ONE);
    D3D_RENDER_STATE_SRCBLEND =
      fog_screen_drawn ? D3DBLEND_INVDESTALPHA : D3DBLEND_ONE;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_DFACTOR_CMD,
                                    fog_screen_drawn ? D3DBLEND_ONE :
                                                       D3DBLEND_SRCALPHA);
    D3D_RENDER_STATE_DESTBLEND =
      fog_screen_drawn ? D3DBLEND_ONE : D3DBLEND_SRCALPHA;
    D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_EQUATION_CMD,
                                    D3DBLENDOP_ADD);
    D3D_RENDER_STATE_BLENDOP = D3DBLENDOP_ADD;
    D3DDevice_SetRenderState_Simple(NV097_SET_ALPHA_TEST_ENABLE_CMD, 0);
    D3D_RENDER_STATE_ALPHATESTENABLE = 0;
    D3DDevice_SetRenderState_ZEnable(0);
    D3DDevice_SetRenderState_ZBias(0);

    rasterizer_set_vertex_shader_permutation(_rasterizer_vertex_shader_screen_effect,
                 _rasterizer_vertex_type_dynamic_screen, 0);

    csmemset(&global_pixel_shader, 0, sizeof(global_pixel_shader));
    layer_count = screen->layer_count;
    global_pixel_shader.texture_modes =
      (((((uint32_t)(layer_count > 3) << 5) | (layer_count > 2)) << 5 |
        (layer_count > 1))
       << 5) |
      1;
    global_pixel_shader.combiner_count = 0x11004;
    global_pixel_shader.constant_0[0] =
      real_rgb_color_to_pixel32(fog_globals.local_fog_screen_layer_colors[0]);
    global_pixel_shader.constant_1[0] =
      real_rgb_color_to_pixel32(fog_globals.local_fog_screen_layer_colors[1]);
    global_pixel_shader.rgb_inputs[0] = 0x8010902;
    global_pixel_shader.rgb_outputs[0] = 0x3089;
    global_pixel_shader.constant_0[1] =
      real_rgb_color_to_pixel32(fog_globals.local_fog_screen_layer_colors[2]);
    global_pixel_shader.constant_1[1] =
      real_rgb_color_to_pixel32(fog_globals.local_fog_screen_layer_colors[3]);
    global_pixel_shader.alpha_inputs[1] =
      (0x2800 | (screen->layer_count > 1 ? 0x29 : 0x20)) << 0x10;
    global_pixel_shader.alpha_outputs[1] = 0xc0;
    global_pixel_shader.rgb_inputs[1] = 0xa010b02;
    global_pixel_shader.rgb_outputs[1] = 0x30ab;
    global_pixel_shader.alpha_inputs[2] =
      ((screen->layer_count > 2 ? 0x2a : 0x20) << 8 |
       (screen->layer_count > 3 ? 0x2b : 0x20))
      << 0x10;
    global_pixel_shader.alpha_outputs[2] = 0xd0;
    global_pixel_shader.rgb_inputs[2] =
      ((((screen->layer_count > 2 ? 0x2a : 0x20) << 8 |
         (screen->layer_count > 3 ? 0xb : 0))
          << 8 |
        (screen->layer_count > 2 ? 0xa : 0))
       << 8) |
      0x20;
    global_pixel_shader.rgb_outputs[2] = 0xc00;
    global_pixel_shader.alpha_inputs[3] = 0x1c1d0000;
    global_pixel_shader.alpha_outputs[3] = 0xc0;
    global_pixel_shader.rgb_inputs[3] =
      (((screen->layer_count > 1 ? 0x29 : 0x20) << 0x10 |
        (screen->layer_count > 1 ? 9 : 0))
       << 8) |
      0xc0020;
    global_pixel_shader.rgb_outputs[3] = 0xc00;
    if (screen->color) {
      global_pixel_shader.final_combiner_constant_0 = screen->color;
    } else {
      global_pixel_shader.final_combiner_constant_0 =
        real_rgb_color_to_pixel32(global_window_parameters.fog_planar_color);
    }
    global_pixel_shader.final_combiner_inputs_abcd = 0x8010f00;
    global_pixel_shader.final_combiner_inputs_efg = 0xc011c00;
    rasterizer_set_pixel_shader(&global_pixel_shader);

    D3DDevice_Begin(D3DPT_TRIANGLEFAN);
    D3DDevice_SetVertexData2s(D3DVSDE_SPECULAR, -1, 1);
    D3DDevice_SetVertexData2s(D3DVSDE_POSITION, -1, 1);
    D3DDevice_SetVertexData2s(D3DVSDE_SPECULAR, 1, 1);
    D3DDevice_SetVertexData2s(D3DVSDE_POSITION, 1, 1);
    D3DDevice_SetVertexData2s(D3DVSDE_SPECULAR, 1, -1);
    D3DDevice_SetVertexData2s(D3DVSDE_POSITION, 1, -1);
    D3DDevice_SetVertexData2s(D3DVSDE_SPECULAR, -1, -1);
    D3DDevice_SetVertexData2s(D3DVSDE_POSITION, -1, -1);
    D3DDevice_End();

    if (fog_globals.local_fog_pass == 0 && fog_screen_drawn) {
      D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                      NV097_COLOR_MASK_RGB);
      D3D_RENDER_STATE_COLORWRITEENABLE = NV097_COLOR_MASK_RGB;
      D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_SFACTOR_CMD,
                                      D3DBLEND_INVDESTALPHA);
      D3D_RENDER_STATE_SRCBLEND = D3DBLEND_INVDESTALPHA;
      D3DDevice_SetRenderState_Simple(NV097_SET_BLEND_FUNC_DFACTOR_CMD,
                                      D3DBLEND_ONE);
      D3D_RENDER_STATE_DESTBLEND = D3DBLEND_ONE;

      csmemset(&global_pixel_shader, 0, sizeof(global_pixel_shader));
      global_pixel_shader.combiner_count = 1;
      rasterizer_set_pixel_shader(&global_pixel_shader);

      D3DDevice_Begin(D3DPT_TRIANGLEFAN);
      D3DDevice_SetVertexData2s(D3DVSDE_SPECULAR, -1, 1);
      D3DDevice_SetVertexData2s(D3DVSDE_POSITION, -1, 1);
      D3DDevice_SetVertexData2s(D3DVSDE_SPECULAR, 1, 1);
      D3DDevice_SetVertexData2s(D3DVSDE_POSITION, 1, 1);
      D3DDevice_SetVertexData2s(D3DVSDE_SPECULAR, 1, -1);
      D3DDevice_SetVertexData2s(D3DVSDE_POSITION, 1, -1);
      D3DDevice_SetVertexData2s(D3DVSDE_SPECULAR, -1, -1);
      D3DDevice_SetVertexData2s(D3DVSDE_POSITION, -1, -1);
      D3DDevice_End();
    }
  }
  if (fog_globals.local_fog_pass != 0) {
    rasterizer_profile_end(_rasterizer_profile_environment_fog_screen);
  }
}

/* 0x167ee0 rasterizer_environment_fog_screen_model_begin */
bool rasterizer_environment_fog_screen_model_begin(void *param_1)
{
  fog_model_begin_parameters *parameters =
    (fog_model_begin_parameters *)param_1;
  boolean result = 0;
  fog_screen *screen;
  real camera_relative[3];

  if (debug_draw_environment_fog_screen && debug_drawing_mode == 0 &&
      rasterizer_environment_fog_screen_is_active()) {
    screen = global_window_parameters.fog_screen;
    if (!parameters) {
      display_assert("parameters", kFogFile, 0x499, 1);
      system_exit(-1);
    }
    if (!screen) {
      display_assert("screen", kFogFile, 0x49a, 1);
      system_exit(-1);
    }
    camera_relative[0] =
      parameters->centroid[0] - global_window_parameters.camera_position[0];
    camera_relative[1] =
      parameters->centroid[1] - global_window_parameters.camera_position[1];
    camera_relative[2] =
      parameters->centroid[2] - global_window_parameters.camera_position[2];
    if (global_window_parameters.camera_forward[2] * camera_relative[2] +
          global_window_parameters.camera_forward[1] * camera_relative[1] +
          global_window_parameters.camera_forward[0] * camera_relative[0] <
        screen->far_distance) {
      fog_globals.model = parameters;
      fog_globals.model_parameters_cached = 0;
      result = 1;
      if (debug_statistics_mode == _rasterizer_statistics_mode_enabled) {
        stats_environment_fog_screen_model_count++;
      }
    }
  }
  return result;
}

/* 0x167ff0 rasterizer_error. Variadic in the binary: the caller's format
 * (call_text) is fed to vsprintf with arglist = &call_text + 1
 * (LEA EAX,[EBP+0x10]), i.e. MSVC's va_start(ap, call_text). The kb decl is
 * the protected two-parameter form, so the arglist is formed the same way
 * MSVC's <stdarg.h> does. 0x201c48 is declared void, but its EAX is tested
 * (TEST EAX,EAX / JGE) so it is called through an int-returning __stdcall
 * cast; arguments are (hr, buffer, 0x3ff). */
void rasterizer_error(int a1, const char *call_text)
{
  const char *error_name;
  char formatted[1024];
  char description[1024];

  error_name = "<unknown error>";
  vsprintf(formatted, call_text, (char *)(&call_text + 1));
  if (((int(__stdcall *)(int, int, int))FUN_00201c48)(a1, (int)description,
                                                      0x3ff) < 0) {
    csstrcpy(description, "<can't get description>");
  }
  switch (a1) {
  case (int)0x8007000e:
    error_name = "E_OUTOFMEMORY";
    break;
  case (int)0x80004005:
    error_name = "E_FAIL";
    break;
  case (int)0x80070057:
    error_name = "E_INVALIDARG";
    break;
  case (int)0x8876017c:
    error_name = "D3DERR_OUTOFVIDEOMEMORY";
    break;
  case (int)0x88760818:
    error_name = "D3DERR_WRONGTEXTUREFORMAT";
    break;
  case (int)0x88760819:
    error_name = "D3DERR_UNSUPPORTEDCOLOROPERATION";
    break;
  case (int)0x8876081a:
    error_name = "D3DERR_UNSUPPORTEDCOLORARG";
    break;
  case (int)0x8876081b:
    error_name = "D3DERR_UNSUPPORTEDALPHAOPERATION";
    break;
  case (int)0x8876081c:
    error_name = "D3DERR_UNSUPPORTEDALPHAARG";
    break;
  case (int)0x8876081d:
    error_name = "D3DERR_TOOMANYOPERATIONS";
    break;
  case (int)0x8876081e:
    error_name = "D3DERR_CONFLICTINGTEXTUREFILTER";
    break;
  case (int)0x8876081f:
    error_name = "D3DERR_UNSUPPORTEDFACTORVALUE";
    break;
  case (int)0x88760821:
    error_name = "D3DERR_CONFLICTINGRENDERSTATE";
    break;
  case (int)0x88760822:
    error_name = "D3DERR_UNSUPPORTEDTEXTUREFILTER";
    break;
  case (int)0x88760826:
    error_name = "D3DERR_CONFLICTINGTEXTUREPALETTE";
    break;
  case (int)0x88760827:
    error_name = "D3DERR_DRIVERINTERNALERROR";
    break;
  case (int)0x88760866:
    error_name = "D3DERR_NOTFOUND";
    break;
  case (int)0x88760867:
    error_name = "D3DERR_MOREDATA";
    break;
  case (int)0x88760868:
    error_name = "D3DERR_DEVICELOST";
    break;
  case (int)0x88760869:
    error_name = "D3DERR_DEVICENOTRESET";
    break;
  case (int)0x8876086a:
    error_name = "D3DERR_NOTAVAILABLE";
    break;
  case (int)0x8876086b:
    error_name = "D3DERR_INVALIDDEVICE";
    break;
  case (int)0x8876086c:
    error_name = "D3DERR_INVALIDCALL";
    break;
  }
  error(2, "%s in %s (code=%d, error=%s)", error_name, formatted, a1,
        description);
}
