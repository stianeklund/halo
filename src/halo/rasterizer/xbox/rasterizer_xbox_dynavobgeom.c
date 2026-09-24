/* rasterizer_xbox_dynavobgeom.c */

/* Multitexture parameter overlay copied by __rasterizer_dynamic_screen_geometry_add_multitexture_params_to_base.  PAL source names
 * each copied slot; the 2276 routine confirms the copy direction and offsets. */
typedef struct rasterizer_dynavobgeom_multitex_params {
  byte pad_00[4];
  uint32_t offset;
  byte pad_08;
  uint8_t map_anchor_screen_1;
  uint8_t map_anchor_screen_2;
  byte pad_0b[5];
  uint32_t map_1;
  uint32_t map_2;
  byte pad_18;
  uint8_t map_wrapped_1;
  uint8_t map_wrapped_2;
  byte pad_1b[5];
  uint32_t map_offset_1;
  uint32_t map_offset_2;
  byte pad_28[8];
  uint32_t map_scale_1_i;
  uint32_t map_scale_1_j;
  uint32_t map_scale_2_i;
  uint32_t map_scale_2_j;
  byte pad_40[8];
  uint32_t map_texture_scale_1_i;
  uint32_t map_texture_scale_1_j;
  uint32_t map_texture_scale_2_i;
  uint32_t map_texture_scale_2_j;
  byte pad_58[4];
  uint32_t map_tint_1;
  uint32_t map_tint_2;
  byte pad_64[24];
  uint32_t map_fade_1;
  uint32_t map_fade_2;
  int16_t map0_to_1_blend_function;
  int16_t map1_to_2_blend_function;
} rasterizer_dynavobgeom_multitex_params_t;
/*
 * Layout evidence:
 * PAL source names every copied member.
 * Debug 2276 copy direction matches PAL.
 * +0x04 is offset.
 * +0x10 and +0x14 are maps 1 and 2.
 * +0x20 and +0x24 are map offsets.
 * +0x30 through +0x54 are scale pairs.
 * +0x7c through +0x86 hold fades and blend functions.
 */

/* Debug assertions identify maps and meter parameters.  PAL source, with the
 * same field-access layout, supplies the remaining screen-geometry names. */
typedef struct rasterizer_dynavobgeom_parameters {
  void *meter_parameters;
  real *offset;
  byte map_anchor_screen[3];
  byte pad_0b;
  void *map[3];
  byte map_wrapped[3];
  byte pad_1b;
  real *map_offset[3];
  real map_scale[3][2];
  real map_texture_scale[3][2];
  real *map_tint[3];
  real plasma_fade[4];
  byte doing_plasma_effect;
  byte pad_75[3];
  real *map_fade[3];
  int16_t map0_to_1_blend_function;
  int16_t map1_to_2_blend_function;
  uint16_t framebuffer_blend_function;
  byte point_sampled;
} rasterizer_dynavobgeom_parameters_t;

typedef struct rasterizer_dynavobgeom_meter {
  uint32_t gradient_min_color;
  uint32_t gradient_max_color;
  uint32_t background_color;
  uint32_t flash_color;
  byte flash_color_is_negative;
  byte tint_mode_2;
  byte pad_12[2];
  uint32_t tint_color;
  real gradient;
} rasterizer_dynavobgeom_meter_t;

typedef struct rasterizer_dynavobgeom_vertex {
  real position_x;
  real position_y;
  real texture_coordinate_x;
  real texture_coordinate_y;
  uint32_t color;
} rasterizer_dynavobgeom_vertex_t;

/* 0x15f1f0 */
void __rasterizer_hud_begin(void)
{
  rasterizer_profile_begin(0x1b);
}

/* 0x15f200 */
void __rasterizer_hud_end(void)
{
  rasterizer_profile_end(0x1b);
}

/* 0x15f210 */
void __rasterizer_dynamic_lit_geometry_draw(int param_1)
{
  (void)param_1;
}

/* 0x15f220 */
void __rasterizer_dynamic_screen_geometry_add_multitexture_params_to_base(void *base, void *multitex_params)
{
  rasterizer_dynavobgeom_multitex_params_t *base_params;
  rasterizer_dynavobgeom_multitex_params_t *source_params;

  assert_halt_at(
    "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c", 0xdc,
    base);
  assert_halt_at(
    "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c", 0xdd,
    multitex_params);

  base_params = (rasterizer_dynavobgeom_multitex_params_t *)base;
  source_params = (rasterizer_dynavobgeom_multitex_params_t *)multitex_params;

  base_params->offset = source_params->offset;
  base_params->map_anchor_screen_1 = source_params->map_anchor_screen_1;
  base_params->map_anchor_screen_2 = source_params->map_anchor_screen_2;
  base_params->map_1 = source_params->map_1;
  base_params->map_2 = source_params->map_2;
  base_params->map_wrapped_1 = source_params->map_wrapped_1;
  base_params->map_wrapped_2 = source_params->map_wrapped_2;
  base_params->map_offset_1 = source_params->map_offset_1;
  base_params->map_offset_2 = source_params->map_offset_2;
  base_params->map_scale_1_i = source_params->map_scale_1_i;
  base_params->map_scale_1_j = source_params->map_scale_1_j;
  base_params->map_scale_2_i = source_params->map_scale_2_i;
  base_params->map_scale_2_j = source_params->map_scale_2_j;
  base_params->map_texture_scale_1_i = source_params->map_texture_scale_1_i;
  base_params->map_texture_scale_1_j = source_params->map_texture_scale_1_j;
  base_params->map_texture_scale_2_i = source_params->map_texture_scale_2_i;
  base_params->map_texture_scale_2_j = source_params->map_texture_scale_2_j;
  base_params->map_tint_1 = source_params->map_tint_1;
  base_params->map_tint_2 = source_params->map_tint_2;
  base_params->map_fade_1 = source_params->map_fade_1;
  base_params->map_fade_2 = source_params->map_fade_2;
  base_params->map0_to_1_blend_function =
    source_params->map0_to_1_blend_function;
  base_params->map1_to_2_blend_function =
    source_params->map1_to_2_blend_function;
}

/* 0x15f540 */
void FUN_0015f540(int param_1, int param_2, uint32_t param_3, int param_4)
{
  (void)param_1;
  (void)param_2;
  (void)param_3;
  (void)param_4;
  display_assert("_rasterizer_dynamic_screen_geometry_draw not supported no mo'",
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
                 0xff, 1);
  system_exit(-1);
}

/* 0x15f5e0 */
int IDirect3DDevice8_SetVertexData2f_6(void *device, uint32_t reg, float a, float b)
{
  (void)device;
  D3DDevice_SetVertexData2f(reg, a, b);
  return 0;
}

/* Group and shader views used by __rasterizer_dynamic_unlit_geometry_draw.  Debug assertions confirm the
 * shader type and geometry flags; PAL source and matching 2276 accesses map
 * the remaining transparent-geometry fields. */
typedef struct rasterizer_dynavobgeom_shader {
  byte pad_00[0x24];
  uint16_t shader_type;
} rasterizer_dynavobgeom_shader_t;

typedef struct rasterizer_dynavobgeom_effect_shader {
  byte pad_00[0x28];
  uint8_t flags;
} rasterizer_dynavobgeom_effect_shader_t;

typedef struct rasterizer_dynavobgeom_point3d {
  real x;
  real y;
  real z;
} rasterizer_dynavobgeom_point3d_t;

typedef struct rasterizer_dynavobgeom_block16 {
  uint32_t field_00;
  uint32_t field_04;
  uint32_t field_08;
  uint32_t field_0c;
} rasterizer_dynavobgeom_block16_t;

typedef struct rasterizer_dynavobgeom_group {
  uint32_t geometry_flags;
  uint32_t object_index;
  uint32_t source_object_index;
  uint32_t shader;
  uint16_t shader_permutation_index;
  byte pad_12[2];
  uint16_t effect_type;
  byte pad_16[0x26];
  real model_base_map_scale_i;
  real model_base_map_scale_j;
  uint32_t dynamic_triangle_buffer_index;
  uint32_t triangle_buffer;
  uint32_t first_triangle_index;
  uint32_t triangle_count;
  uint32_t dynamic_vertex_buffer_index;
  uint32_t vertex_buffer;
  uint32_t lightmap;
  uint32_t node_matrices;
  uint16_t node_matrix_count;
  byte pad_66[2];
  uint32_t lighting;
  uint32_t animation;
  real z_sort;
  rasterizer_dynavobgeom_point3d_t centroid;
  rasterizer_dynavobgeom_block16_t plane;
  uint32_t sorted_index;
  uint16_t previous_group_presorted_index;
  uint16_t next_group_presorted_index;
  uint32_t active_camouflage_transparent_source_object_index;
  byte pad_9c;
  uint8_t cortana_hack;
  byte pad_9e[2];
} rasterizer_dynavobgeom_group_t;

/* 0x15f630 */
void __rasterizer_dynamic_unlit_geometry_draw(void *shader, uint32_t param_2, int param_3, int param_4,
                  uint32_t param_5, int param_6, float *centroid,
                  uint32_t geometry_flags)
{
  rasterizer_dynavobgeom_shader_t *shader_base;
  rasterizer_dynavobgeom_effect_shader_t *effect_shader;
  rasterizer_dynavobgeom_group_t *group;
  volatile rasterizer_dynavobgeom_block16_t zeros;
  real delta_x;
  real delta_y;
  real delta_z;

  if (*(void **)0x476ab0 == NULL) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
      0x26, 1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x3256d8 == 0) {
    return;
  }

  shader_base = (rasterizer_dynavobgeom_shader_t *)shader;
  if ((geometry_flags & 0x20) != 0 && shader_base->shader_type != 1) {
    display_assert(
      "!TEST_FLAG(geometry_flags, _rasterizer_geometry_viewspace_bit) || "
      "shader->base.type==_shader_type_effect",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
      0x2a, 1);
    system_exit(-1);
  }
  if (shader_base->shader_type != 1) {
    display_assert(
      "shader->base.type==_shader_type_effect",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
      0x2c, 1);
    system_exit(-1);
  }
  if (centroid == NULL) {
    display_assert(
      "centroid",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
      0x34, 1);
    system_exit(-1);
  }

  delta_x = centroid[0] - *(const real *)0x5a5bc8;
  delta_y = centroid[1] - *(const real *)0x5a5bcc;
  delta_z = centroid[2] - *(const real *)0x5a5bd0;
  group = (rasterizer_dynavobgeom_group_t *)
    rasterizer_transparent_geometry_new_group();
  if (group != NULL) {
    goto group_allocated;
  }
  if (*(uint8_t *)0x47dbf6 == 0) {
    error(2, "### ERROR too many transparent geometry groups");
    *(uint8_t *)0x47dbf6 = 1;
  }
  return;

group_allocated:

  (void)param_3;
  group->dynamic_triangle_buffer_index = (uint32_t)param_4;
  group->geometry_flags = geometry_flags;
  group->dynamic_vertex_buffer_index = param_5;
  group->triangle_count = (uint32_t)param_6;
  group->lightmap = param_2;
  group->object_index = 0;
  group->source_object_index = 0;
  group->shader = (uint32_t)shader;
  group->shader_permutation_index = 0;
  group->effect_type = 0;
  group->triangle_buffer = 0;
  group->first_triangle_index = 0;
  group->vertex_buffer = 0;
  group->z_sort =
    -(*(const real *)0x5a5bd4 * delta_x +
      (*(const real *)0x5a5bd8 * delta_y + *(const real *)0x5a5bdc * delta_z));
  group->centroid = *(rasterizer_dynavobgeom_point3d_t *)centroid;
  zeros.field_00 = 0;
  zeros.field_04 = 0;
  zeros.field_08 = 0;
  zeros.field_0c = 0;
  group->plane = zeros;
  group->model_base_map_scale_j = 1.0f;
  group->model_base_map_scale_i = 1.0f;
  group->previous_group_presorted_index = 0xffff;
  group->next_group_presorted_index = 0xffff;
  group->active_camouflage_transparent_source_object_index = 0;
  group->cortana_hack = 0;

  if (shader_base->shader_type == 1) {
    effect_shader = (rasterizer_dynavobgeom_effect_shader_t *)
      shader_get_and_verify_type(shader, 1);
    if ((effect_shader->flags & 1) != 0) {
      group->z_sort += *(const real *)0x25337c;
    }
  }

  group->node_matrices = 0;
  group->node_matrix_count = 0;
  group->lighting = 0;
  group->animation = 0;

  if (*(uint16_t *)0x3256ba == 2) {
    *(uint32_t *)0x5a5504 += 1;
    *(int *)0x5a5508 += param_6;
    if (*(int *)0x5a550c < param_6) {
      *(int *)0x5a550c = param_6;
    }
    *(int *)0x5a5510 +=
      rasterizer_frame_statistics_count_dynamic_vertices(param_4, 0, param_6);
  }
}

/* 0x15f8e0 */
void __rasterizer_psuedo_dynamic_screen_quad_draw(void *parameters, void *vertices)
{
  rasterizer_dynavobgeom_parameters_t *params;
  rasterizer_dynavobgeom_meter_t *meter;
  rasterizer_dynavobgeom_vertex_t *vertex;
  real *color;
  real *default_color;
  void *map;
  short dx;
  short dy;
  short i;
  real x_offset;
  real y_offset;
  real x_recip;
  real y_recip;
  real alpha;
  real vs_a[20];
  real vs_b[24];

  if (*(int *)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
      0x2f4, true);
    system_exit(-1);
  }
  if (*(byte *)0x3256da == 0) {
    return;
  }
  if (*(uint16_t *)0x5a5bc0 != 0) {
    return;
  }

  params = (rasterizer_dynavobgeom_parameters_t *)parameters;
  if (params == 0) {
    display_assert(
      "parameters",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
      0x2f9, true);
    system_exit(-1);
  }
  if (params->map[0] == 0) {
    display_assert(
      "parameters->map[0]",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
      0x2fb, true);
    system_exit(-1);
  }
  if (params->map[2] != 0 && params->map[1] == 0) {
    display_assert(
      "!parameters->map[2] || parameters->map[1]",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
      0x2fd, true);
    system_exit(-1);
  }
  if (params->map[1] != 0 && params->meter_parameters != 0) {
    display_assert(
      "!parameters->map[1] || !parameters->meter_parameters",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
      0x2ff, true);
    system_exit(-1);
  }

  D3DDevice_SetRenderState_CullMode(0);
  D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
  *(uint32_t *)0x1fb7a4 = 0x10101;
  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(uint32_t *)0x1fb784 = 1;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_ZEnable(0);
  D3DDevice_SetRenderState_ZBias(0);
  rasterizer_set_framebuffer_blend_function((int)params->framebuffer_blend_function);

  dx = (short)(*(short *)0x5a5bfa - *(short *)0x5a5bf6);
  dy = (short)(*(int *)0x5a5bf8 - *(int *)0x5a5bf4);
  x_offset = params->offset != 0 ?
               (params->offset[0] + params->offset[0]) / (real)dx :
               0.0f;
  y_offset =
    params->offset != 0 ? (params->offset[1] * -2.0f) / (real)dy : 0.0f;
  x_recip = 1.0f / (real)dx;
  y_recip = 1.0f / (real)dy;

  vs_a[0] = x_recip + x_recip;
  vs_a[1] = 0.0f;
  vs_a[2] = 0.0f;
  vs_a[3] = x_offset - (x_recip + 1.0f);
  vs_a[4] = 0.0f;
  vs_a[5] = -2.0f * y_recip;
  vs_a[6] = 0.0f;
  vs_a[7] = y_recip + y_offset + 1.0f;
  vs_a[8] = 0.0f;
  vs_a[9] = 0.0f;
  vs_a[10] = 0.0f;
  vs_a[11] = 0.5f;
  vs_a[12] = 0.0f;
  vs_a[13] = 0.0f;
  vs_a[14] = 0.0f;
  vs_a[15] = 1.0f;
  vs_a[16] = params->map_texture_scale[0][0];
  vs_a[17] = params->map_texture_scale[0][1];
  vs_a[18] = 0.0f;
  vs_a[19] = 1.0f;

  vs_b[0] = params->map_texture_scale[1][0];
  vs_b[1] = params->map_texture_scale[1][1];
  vs_b[2] = params->map_texture_scale[2][0];
  vs_b[3] = params->map_texture_scale[2][1];
  vs_b[4] = params->map_anchor_screen[0] != 0 ? 1.0f : 0.0f;
  vs_b[5] = params->map_anchor_screen[0] != 0 ? 0.0f : 1.0f;
  vs_b[6] = params->map_anchor_screen[1] != 0 ? 1.0f : 0.0f;
  vs_b[7] = params->map_anchor_screen[1] != 0 ? 0.0f : 1.0f;
  vs_b[8] = params->map_anchor_screen[2] != 0 ? 1.0f : 0.0f;
  vs_b[9] = params->map_anchor_screen[2] != 0 ? 0.0f : 1.0f;
  color = params->map_offset[0];
  vs_b[10] = color != 0 ? color[0] : 0.0f;
  vs_b[11] = color != 0 ? color[1] : 0.0f;
  color = params->map_offset[1];
  vs_b[12] = color != 0 ? color[0] : 0.0f;
  vs_b[13] = color != 0 ? color[1] : 0.0f;
  color = params->map_offset[2];
  vs_b[14] = color != 0 ? color[0] : 0.0f;
  vs_b[15] = color != 0 ? color[1] : 0.0f;
  vs_b[16] = params->map_scale[0][0];
  vs_b[17] = params->map_scale[0][1];
  vs_b[18] = params->map_scale[1][0];
  vs_b[19] = params->map_scale[1][1];
  vs_b[20] = params->map_scale[2][0];
  vs_b[21] = params->map_scale[2][1];
  vs_b[22] = 0.0f;
  vs_b[23] = 0.0f;

  D3DDevice_SetVertexShaderConstant(-0x44, vs_a, 5);
  D3DDevice_SetVertexShaderConstant(-0x3f, vs_b, 6);
  for (i = 0; i < 3; i++) {
    map = params->map[i];
    if (map == 0) {
      break;
    }
    rasterizer_set_texture_bitmap_data(i, map);
    D3DDevice_SetTextureStageState(i, 0xa, (params->map_wrapped[i] == 0) * 2 + 1);
    D3DDevice_SetTextureStageState(i, 0xb, (params->map_wrapped[i] == 0) * 2 + 1);
    D3DDevice_SetTextureStageState(i, 0xd, (params->point_sampled == 0) + 1);
    D3DDevice_SetTextureStageState(i, 0xe, (params->point_sampled == 0) + 1);
    D3DDevice_SetTextureStageState(i, 0xf, (params->point_sampled == 0) + 1);
  }
  rasterizer_set_vertex_shader_permutation(4, 8, 1);

  meter = (rasterizer_dynavobgeom_meter_t *)params->meter_parameters;
  if (meter != 0) {
    if (meter->tint_mode_2 == 0) {
      display_assert(
        "meter->tint_mode_2",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
        0x376, true);
      system_exit(-1);
    }
    if (meter->gradient != 1.0f) {
      display_assert(
        "meter->gradient==1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_dynavobgeom.c",
        0x377, true);
      system_exit(-1);
    }
    D3DDevice_SetRenderState_Simple(0x40304, 1);
    *(uint32_t *)0x1fb784 = 1;
    D3DDevice_SetRenderState_Simple(0x40344, 0x8001);
    *(uint32_t *)0x1fb790 = 0x8001;
    D3DDevice_SetRenderState_Simple(0x40348, 0x302);
    *(uint32_t *)0x1fb794 = 0x302;
    D3DDevice_SetRenderState_Simple(0x4034c, meter->tint_color);
    *(uint32_t *)0x1fb7c4 = meter->tint_color;
    D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
    *(uint32_t *)0x1fb7c0 = 0x8006;
    D3DDevice_SetTextureStageState(0, 0x15, 4);
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b98 = 1;
    *(uint32_t *)0x5a5b94 = 0x11104;
    *(uint32_t *)0x5a5ae8 = meter->gradient_min_color;
    alpha = meter->gradient * 8.0f;
    if (!(alpha > 1.0f)) {
      alpha = 1.0f;
    }
    *(uint32_t *)0x5a5b08 =
      real_alpha_to_pixel32(1.0f / alpha) | (meter->gradient_max_color & 0xffffff);
    *(uint32_t *)0x5a5ac0 = 0x12081208;
    *(uint32_t *)0x5a5b48 = 0x1120e820;
    *(uint32_t *)0x5a5b28 = 0x20c00;
    *(uint32_t *)0x5a5b74 = 0x20c00;
    *(uint32_t *)0x5a5aec = meter->gradient_min_color;
    *(uint32_t *)0x5a5b0c = meter->gradient_max_color;
    *(uint32_t *)0x5a5ac4 = 0x6c200000;
    *(uint32_t *)0x5a5b2c = 0xc0;
    *(uint32_t *)0x5a5b4c = 0x3c011c02;
    *(uint32_t *)0x5a5b78 = 0xc00;
    *(uint32_t *)0x5a5af0 = meter->gradient_min_color;
    *(uint32_t *)0x5a5b10 = meter->flash_color;
    *(uint32_t *)0x5a5ac8 = 0x820b220;
    *(uint32_t *)0x5a5b30 = 0xc00;
    *(uint32_t *)0x5a5b7c = 0xc00;
    *(uint32_t *)0x5a5b50 = ((meter->flash_color_is_negative != 0) ? 0xe2 : 2) | 0xc201c00;
    *(uint32_t *)0x5a5af4 = meter->background_color;
    *(uint32_t *)0x5a5b14 = meter->tint_color;
    *(uint32_t *)0x5a5acc = 0x12201120;
    *(uint32_t *)0x5a5b34 = 0x4c00;
    *(uint32_t *)0x5a5b54 = 0xc200120;
    *(uint32_t *)0x5a5b80 = 0x4c00;
    *(uint32_t *)0x5a5ae0 = 0xc180000;
  } else {
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b98 = (uint32_t)((((params->map[2] != 0) << 5) |
                                        (params->map[1] != 0))
                                         << 5 |
                                       (params->map[0] != 0));
    default_color = *(real **)0x2ee708;
    color = params->map_tint[0] != 0 ? params->map_tint[0] : default_color;
    vs_a[9] = color[0];
    vs_a[10] = color[1];
    vs_a[11] = color[2];
    color = params->map_tint[1] != 0 ? params->map_tint[1] : default_color;
    vs_a[13] = color[0];
    vs_a[14] = color[1];
    vs_a[15] = color[2];
    color = params->map_tint[2] != 0 ? params->map_tint[2] : default_color;
    vs_a[17] = color[0];
    vs_a[18] = color[1];
    vs_a[19] = color[2];
    vs_a[8] = params->map_fade[0] != 0 ? *params->map_fade[0] : 1.0f;
    vs_a[12] = params->map_fade[1] != 0 ? *params->map_fade[1] : 1.0f;
    vs_a[16] = params->map_fade[2] != 0 ? *params->map_fade[2] : 1.0f;
    *(uint32_t *)0x5a5ae8 = real_argb_color_to_pixel32(&vs_a[8]);
    *(uint32_t *)0x5a5b08 = real_argb_color_to_pixel32(&vs_a[12]);
    *(uint32_t *)0x5a5aec = real_argb_color_to_pixel32(&vs_a[16]);
    *(uint32_t *)0x5a5af8 = real_argb_color_to_pixel32(params->plasma_fade);
    *(uint32_t *)0x5a5afc = real_argb_color_to_pixel32(params->plasma_fade);
    *(uint32_t *)0x5a5b00 = real_argb_color_to_pixel32(params->plasma_fade);
    *(uint32_t *)0x5a5b04 = real_argb_color_to_pixel32(params->plasma_fade);
    *(uint32_t *)0x5a5b74 = 0x89;
    *(uint32_t *)0x5a5b28 = 0x89;
    *(uint32_t *)0x5a5b48 = 0x8010902;
    *(uint32_t *)0x5a5ac0 = 0x18111912;
    *(uint32_t *)0x5a5b4c = 0xa010804;
    *(uint32_t *)0x5a5b78 = 0xac;
    *(uint32_t *)0x5a5ac4 = 0x1a111814;
    *(uint32_t *)0x5a5b2c = 0xac;
    i = 2;
    if (params->map[1] != 0) {
      switch (params->map0_to_1_blend_function) {
      case 0:
        *(uint32_t *)0x5a5b50 = 0xc200920;
        *(uint32_t *)0x5a5ac8 = 0x1c201920;
        *(uint32_t *)0x5a5b30 = 0xc00;
        *(uint32_t *)0x5a5b7c = 0xc00;
        break;
      case 1:
        *(uint32_t *)0x5a5b7c = 0xc0;
        *(uint32_t *)0x5a5b50 = 0xc090000;
        *(uint32_t *)0x5a5ac8 = 0x1c190000;
        *(uint32_t *)0x5a5b30 = 0xc0;
        break;
      case 2:
        *(uint32_t *)0x5a5b50 = 0xc20e920;
        *(uint32_t *)0x5a5ac8 = 0x1c20f920;
        *(uint32_t *)0x5a5b30 = 0xc00;
        *(uint32_t *)0x5a5b7c = 0xc00;
        break;
      case 3:
        *(uint32_t *)0x5a5b50 = 0xc090000;
        *(uint32_t *)0x5a5b7c = 0x100c0;
        *(uint32_t *)0x5a5ac8 = 0x1c190000;
        *(uint32_t *)0x5a5b30 = 0x100c0;
        break;
      case 4:
        *(uint32_t *)0x5a5b7c = 0x20c0;
        *(uint32_t *)0x5a5b50 = 0xc090000;
        *(uint32_t *)0x5a5ac8 = 0x1c190000;
        *(uint32_t *)0x5a5b30 = 0xc0;
        break;
      case 5:
        *(uint32_t *)0x5a5ac8 = 0x820a920;
        *(uint32_t *)0x5a5b50 = 0x1920b820;
        *(uint32_t *)0x5a5acc = 0x1c1c0c0c;
        *(uint32_t *)0x5a5b34 = 0x24c00;
        *(uint32_t *)0x5a5b54 = 0;
        *(uint32_t *)0x5a5b80 = 0;
        *(uint32_t *)0x5a5ad0 = 0x5c5c;
        *(uint32_t *)0x5a5b38 = 0x4d00;
        *(uint32_t *)0x5a5b58 = 0;
        *(uint32_t *)0x5a5b84 = 0;
        *(uint32_t *)0x5a5ad4 = 0;
        *(uint32_t *)0x5a5b3c = 0xc00;
        *(uint32_t *)0x5a5b5c = 0x1ca01da0;
        *(uint32_t *)0x5a5b88 = 0xc00;
        *(uint32_t *)0x5a5b30 = 0xc00;
        *(uint32_t *)0x5a5b7c = 0xc00;
        i = 5;
        break;
      }
      i++;
    }
    if (params->map[2] != 0) {
      switch (params->map1_to_2_blend_function) {
      case 0:
        *(uint32_t *)(0x5a5b48 + i * 4) =
          params->map0_to_1_blend_function == 5 ?
            (((params->doing_plasma_effect != 0) ? 4 : 0x20) | 0xc010a00) :
            0xc200a20;
        *(uint32_t *)(0x5a5b74 + i * 4) = 0xc00;
        *(uint32_t *)(0x5a5ac0 + i * 4) = 0x1c201a20;
        *(uint32_t *)(0x5a5b28 + i * 4) = 0xc00;
        break;
      case 1:
        *(uint32_t *)(0x5a5b48 + i * 4) = 0xc0a0000;
        *(uint32_t *)(0x5a5b74 + i * 4) = 0xc0;
        *(uint32_t *)(0x5a5ac0 + i * 4) = 0x1c1a0000;
        *(uint32_t *)(0x5a5b28 + i * 4) = 0xc0;
        break;
      case 2:
        *(uint32_t *)(0x5a5b48 + i * 4) = 0xc20ea20;
        *(uint32_t *)(0x5a5b74 + i * 4) = 0xc00;
        *(uint32_t *)(0x5a5ac0 + i * 4) = 0x1c20fa20;
        *(uint32_t *)(0x5a5b28 + i * 4) = 0xc00;
        break;
      case 3:
        *(uint32_t *)(0x5a5b48 + i * 4) = 0xc0a0000;
        *(uint32_t *)(0x5a5b74 + i * 4) = 0x100c0;
        *(uint32_t *)(0x5a5ac0 + i * 4) = 0x1c1a0000;
        *(uint32_t *)(0x5a5b28 + i * 4) = 0x100c0;
        break;
      case 4:
        *(uint32_t *)(0x5a5b48 + i * 4) = 0xc0a0000;
        *(uint32_t *)(0x5a5b74 + i * 4) = 0x20c0;
        *(uint32_t *)(0x5a5ac0 + i * 4) = 0x1c1a0000;
        *(uint32_t *)(0x5a5b28 + i * 4) = 0xc0;
        break;
      }
      i++;
    }
    *(uint32_t *)0x5a5b94 = (uint32_t)i | 0x11100;
    *(uint32_t *)0x5a5ae0 = 0xc;
  }
  *(uint32_t *)0x5a5ae4 = 0x1c00;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  D3DDevice_SetRenderState_CullMode(0x901);
  rasterizer_set_vertex_shader_permutation(4, 8, 1);
  D3DDevice_Begin(7);
  vertex = (rasterizer_dynavobgeom_vertex_t *)vertices;
  for (i = 0; i < 4; i++) {
    D3DDevice_SetVertexDataColor(9, vertex->color);
    D3DDevice_SetVertexData2f(4, vertex->texture_coordinate_x,
                              vertex->texture_coordinate_y);
    D3DDevice_SetVertexData2f(0, vertex->position_x, vertex->position_y);
    vertex++;
  }
  D3DDevice_End();
  D3DDevice_SetTextureStageState(0, 0x15, 0);
}
