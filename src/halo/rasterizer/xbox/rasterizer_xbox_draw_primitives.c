#include "x87_math.h"

/* c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_draw_primitives.c */

#define RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND 10000
#define NUMBER_OF_RASTERIZER_VERTEX_TYPES 12

/* rasterizer vertex type indices; the non-zero-capacity ones are the only
 * cases the original's jump table distinguishes (0x15e86a..0x15e902). */
#define DEBUG_VERTEX_TYPE 5
#define DYNAMIC_UNLIT_VERTEX_TYPE 6
#define DYNAMIC_LIT_VERTEX_TYPE 7
#define DYNAMIC_SCREEN_VERTEX_TYPE 8
#define MODEL_COMPRESSED_VERTEX_TYPE 9

/* struct rasterizer_triangle is three 16-bit indices. */
#define SIZEOF_RASTERIZER_TRIANGLE 6
#define RASTERIZER_MAXIMUM_DYNAMIC_TRIANGLES 0x8000
#define RASTERIZER_MAXIMUM_DYNAMIC_UNLIT_VERTICES 0x2000
#define RASTERIZER_MAXIMUM_DYNAMIC_DEBUG_VERTICES 0x800
#define RASTERIZER_MAXIMUM_DYNAMIC_MODEL_VERTICES 0x6000

/* D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC as the original pushes it. */
#define RASTERIZER_DYNAMIC_BUFFER_USAGE 0x208
#define RASTERIZER_DYNAMIC_BUFFER_POOL 0
#define D3DFMT_INDEX16 0x65
#define D3DLOCK_READONLY 0x80

typedef struct dynamic_vertex_group {
  int vertex_count;
  int maximum_vertex_count;
  int total_vertex_count;
  void *d3d_vertex_buffer;
  boolean first_lock;
  byte pad11[3];
} dynamic_vertex_group;

typedef struct dynamic_vertex_buffer {
  int16_t type;
  word pad02;
  int vertex_start_index;
  int vertex_count;
  byte *vertices;
} dynamic_vertex_buffer;

typedef struct dynamic_triangle_buffer {
  int triangle_start_index;
  int triangle_count;
  int16_t *triangles;
} dynamic_triangle_buffer;

/* vertex_buffer and triangle_buffer live in src/types.h: they appear in
 * kb.json declarations, so the generated decl.h needs them globally. */

cs(dynamic_vertex_group, 0x14);
cs(dynamic_vertex_buffer, 0x10);
cs(dynamic_triangle_buffer, 0xc);

static const char kDrawPrimitivesFile[] =
  "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_draw_primitives.c";

/* Globals. Names from assert/error strings in this file unless noted. */
#define global_d3d_device (*(void **)0x476ab0)
#define dynamic_vertex_groups ((struct dynamic_vertex_group *)0x476ae8)
#define dynamic_vertex_buffers ((struct dynamic_vertex_buffer *)0x476bd8)
#define dynamic_vertices_buffer_count (*(int *)0x47abd8)
#define dynamic_triangle_buffers ((struct dynamic_triangle_buffer *)0x47abe0)
#define dynamic_triangles_buffer_count (*(int *)0x47dbe0)
#define dynamic_triangles_d3d_index_buffer (*(void **)0x47dbe8)
/* Byte cleared by __rasterizer_dynamic_triangles_lock; role unproven. */
#define unk_47dbec (*(byte *)0x47dbec)
#define aux_dynamic_unlit_vb (*(void **)0x47dbf0)
/* Bit 0 selects aux_dynamic_unlit_vb over the unlit group's buffer; role
 * unproven. */
#define unk_325668 (*(int *)0x325668)
/* T2: zero triggers "tried to lock dynamic vertices without specifying a
 * lock operation"; callers store it before locking (rasterizer.c). */
#define dynamic_vertices_lock_operation (*(int16_t *)0x325652)
/* T3: base pointer the index offsets are added to before
 * D3DDevice_DrawIndexedVertices (every site loads it: MOV reg,[0x1fb494]). */
#define index_data_base (*(uint32_t *)0x1fb494)
/* T3: triangle_buffer->type -> D3DPRIMITIVETYPE (first arg of
 * D3DDevice_DrawIndexedVertices). */
#define triangle_buffer_primitive_types ((const int *)0x2a0098)
/* T3: per-primitive-type {multiplier, addend} pairs giving the index count
 * as multiplier * primitive_count + addend. */
#define primitive_index_count_multipliers ((const int *)0x29f7e8)
#define primitive_index_count_addends ((const int *)0x29f7ec)

static void *
dynamic_vertex_group_get_d3d_vertex_buffer(dynamic_vertex_group *group)
{
  void *d3d_vertex_buffer;

  if (group == 0) {
    display_assert("group", kDrawPrimitivesFile, 0x1f8, 1);
    system_exit(-1);
  }
  if (group == &dynamic_vertex_groups[DYNAMIC_UNLIT_VERTEX_TYPE] &&
      (unk_325668 & 1) != 0) {
    d3d_vertex_buffer = aux_dynamic_unlit_vb;
  } else {
    d3d_vertex_buffer = group->d3d_vertex_buffer;
  }
  return d3d_vertex_buffer;
}

/* 0x15d8b0 */
void rasterizer_draw_dynamic_triangles_dynamic_vertices(
  int dynamic_triangle_buffer_index, int first_triangle_index,
  int triangle_count, int dynamic_vertex_buffer_index)
{
  boolean success;
  dynamic_triangle_buffer *dynamic_triangle_buffer;
  dynamic_vertex_buffer *dynamic_vertex_buffer;
  dynamic_vertex_group *group;
  void *d3d_vertex_buffer;
  int vertex_size;
  int local_triangle_count;

  success = 1;
  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x2e5, 1);
    system_exit(-1);
  }

  while (triangle_count > 0) {
    if (dynamic_triangle_buffer_index == -1 ||
        dynamic_vertex_buffer_index == -1) {
      break;
    }
    if (dynamic_triangles_d3d_index_buffer == 0) {
      display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                     0x2f4, 1);
      system_exit(-1);
    }
    if (dynamic_triangle_buffer_index < 0) {
      display_assert("dynamic_triangle_buffer_index>=0", kDrawPrimitivesFile,
                     0x2f7, 1);
      system_exit(-1);
    }
    if (dynamic_triangle_buffer_index >= dynamic_triangles_buffer_count) {
      display_assert(
        "dynamic_triangle_buffer_index<dynamic_triangles.buffer_count",
        kDrawPrimitivesFile, 0x2f8, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer_index < 0) {
      display_assert("dynamic_vertex_buffer_index>=0", kDrawPrimitivesFile,
                     0x2f9, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer_index >= dynamic_vertices_buffer_count) {
      display_assert(
        "dynamic_vertex_buffer_index<dynamic_vertices.buffer_count",
        kDrawPrimitivesFile, 0x2fa, 1);
      system_exit(-1);
    }

    dynamic_vertex_buffer =
      &dynamic_vertex_buffers[dynamic_vertex_buffer_index];
    dynamic_triangle_buffer =
      &dynamic_triangle_buffers[dynamic_triangle_buffer_index];
    vertex_size =
      rasterizer_geometry_get_vertex_size(dynamic_vertex_buffer->type);
    group = &dynamic_vertex_groups[dynamic_vertex_buffer->type];
    d3d_vertex_buffer = dynamic_vertex_group_get_d3d_vertex_buffer(group);

    if (d3d_vertex_buffer == 0) {
      display_assert("d3d_vertex_buffer", kDrawPrimitivesFile, 0x305, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer->vertex_start_index < 0) {
      display_assert("dynamic_vertex_buffer->vertex_start_index>=0",
                     kDrawPrimitivesFile, 0x308, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer->vertex_start_index >
        group->vertex_count - dynamic_vertex_buffer->vertex_count) {
      display_assert("dynamic_vertex_buffer->vertex_start_index<=group->vertex_"
                     "count - dynamic_vertex_buffer->vertex_count",
                     kDrawPrimitivesFile, 0x309, 1);
      system_exit(-1);
    }
    if (dynamic_triangle_buffer->triangle_start_index < 0) {
      display_assert("dynamic_triangle_buffer->triangle_start_index>=0",
                     kDrawPrimitivesFile, 0x30c, 1);
      system_exit(-1);
    }
    if (triangle_count < 0) {
      display_assert("triangle_count>=0", kDrawPrimitivesFile, 0x30d, 1);
      system_exit(-1);
    }
    if (triangle_count >
        dynamic_triangle_buffer->triangle_count - first_triangle_index) {
      display_assert("triangle_count<=dynamic_triangle_buffer->triangle_count "
                     "- first_triangle_index",
                     kDrawPrimitivesFile, 0x30e, 1);
      system_exit(-1);
    }

    local_triangle_count = triangle_count;
    if (local_triangle_count > RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND) {
      local_triangle_count = RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND;
    }

    D3DDevice_SetStreamSource(0, d3d_vertex_buffer, (uint32_t)vertex_size);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(0, "IDirect3DDevice8_SetStreamSource(global_d3d_device, "
                          "0, d3d_vertex_buffer, vertex_size)");
    }
    D3DDevice_SetIndices(dynamic_triangles_d3d_index_buffer,
                         (uint32_t)dynamic_vertex_buffer->vertex_start_index);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(0, "IDirect3DDevice8_SetIndices(global_d3d_device, "
                          "dynamic_triangles.d3d_index_buffer, "
                          "dynamic_vertex_buffer->vertex_start_index)");
    }
    D3DDevice_DrawIndexedVertices(
      5, (uint32_t)(local_triangle_count * 3),
      (const void *)((const int16_t *)index_data_base +
                     (first_triangle_index +
                      dynamic_triangle_buffer->triangle_start_index) *
                       3));
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_DrawIndexedPrimitive(global_d3d_device, "
           "D3DPT_TRIANGLELIST, 0, dynamic_vertex_buffer->vertex_count, "
           "NUMBER_OF_VERTICES_PER_TRIANGLE*(first_triangle_index + "
           "dynamic_triangle_buffer->triangle_start_index), "
           "local_triangle_count)");
    }

    first_triangle_index += local_triangle_count;
    triangle_count -= local_triangle_count;
  }

  if (!success) {
    error(
      2, "### ERROR rasterizer_draw_dynamic_triangles_dynamic_vertices failed");
  }
}

/* 0x15dc10 */
void rasterizer_draw_dynamic_triangles_static_vertices(
  int dynamic_triangle_buffer_index, int first_triangle_index,
  int triangle_count, const vertex_buffer *vertex_buffer)
{
  boolean success;
  dynamic_triangle_buffer *dynamic_triangle_buffer;
  int vertex_size;
  int local_triangle_count;

  success = 1;
  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x337, 1);
    system_exit(-1);
  }
  while (triangle_count > 0) {
    if (dynamic_triangle_buffer_index == -1 || vertex_buffer == 0 ||
        vertex_buffer->hardware_format == 0) {
      break;
    }
    if (dynamic_triangles_d3d_index_buffer == 0) {
      display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                     0x342, 1);
      system_exit(-1);
    }
    if (dynamic_triangle_buffer_index < 0) {
      display_assert("dynamic_triangle_buffer_index>=0", kDrawPrimitivesFile,
                     0x345, 1);
      system_exit(-1);
    }
    if (dynamic_triangle_buffer_index >= dynamic_triangles_buffer_count) {
      display_assert(
        "dynamic_triangle_buffer_index<dynamic_triangles.buffer_count",
        kDrawPrimitivesFile, 0x346, 1);
      system_exit(-1);
    }
    dynamic_triangle_buffer =
      &dynamic_triangle_buffers[dynamic_triangle_buffer_index];
    vertex_size = rasterizer_geometry_get_vertex_size(vertex_buffer->type);
    if (dynamic_triangle_buffer->triangle_start_index < 0) {
      display_assert("dynamic_triangle_buffer->triangle_start_index>=0",
                     kDrawPrimitivesFile, 0x34d, 1);
      system_exit(-1);
    }
    if (triangle_count < 0) {
      display_assert("triangle_count>=0", kDrawPrimitivesFile, 0x34e, 1);
      system_exit(-1);
    }
    if (triangle_count >
        dynamic_triangle_buffer->triangle_count - first_triangle_index) {
      display_assert("triangle_count<=dynamic_triangle_buffer->triangle_count "
                     "- first_triangle_index",
                     kDrawPrimitivesFile, 0x34f, 1);
      system_exit(-1);
    }
    local_triangle_count = triangle_count;
    if (local_triangle_count > RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND) {
      local_triangle_count = RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND;
    }
    D3DDevice_SetStreamSource(0, vertex_buffer->hardware_format,
                              (uint32_t)vertex_size);
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0,
                       "IDirect3DDevice8_SetStreamSource(global_d3d_device, 0, "
                       "(IDirect3DVertexBuffer8*)vertex_buffer->hardware_"
                       "format, vertex_size)");
    }
    D3DDevice_SetIndices(dynamic_triangles_d3d_index_buffer, 0);
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0, "IDirect3DDevice8_SetIndices(global_d3d_device, "
                          "dynamic_triangles.d3d_index_buffer, 0)");
    }
    D3DDevice_DrawIndexedVertices(
      5, (uint32_t)(local_triangle_count * 3),
      (const void *)((const int16_t *)index_data_base +
                     (first_triangle_index +
                      dynamic_triangle_buffer->triangle_start_index) *
                       3));
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0,
                       "IDirect3DDevice8_DrawIndexedPrimitive(global_d3d_"
                       "device, D3DPT_TRIANGLELIST, 0, vertex_buffer->count, "
                       "NUMBER_OF_VERTICES_PER_TRIANGLE*(first_triangle_index "
                       "+ dynamic_triangle_buffer->triangle_start_index), "
                       "local_triangle_count)");
    }
    first_triangle_index += local_triangle_count;
    triangle_count -= local_triangle_count;
  }
  if (!success) {
    error(2,
          "### ERROR rasterizer_draw_dynamic_triangles_static_vertices failed");
  }
}

/* 0x15de60 */
void rasterizer_draw_dynamic_triangles_static_vertices2(
  int dynamic_triangle_buffer_index, int first_triangle_index,
  int triangle_count, const vertex_buffer *vertex_buffer0,
  const vertex_buffer *vertex_buffer1)
{
  boolean success;
  dynamic_triangle_buffer *dynamic_triangle_buffer;
  int vertex_size0;
  int vertex_size1;
  int local_triangle_count;

  success = 1;
  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x37a, 1);
    system_exit(-1);
  }
  while (triangle_count > 0) {
    if (dynamic_triangle_buffer_index == -1 || vertex_buffer0 == 0 ||
        vertex_buffer0->hardware_format == 0 || vertex_buffer1 == 0 ||
        vertex_buffer1->hardware_format == 0) {
      break;
    }
    if (dynamic_triangles_d3d_index_buffer == 0) {
      display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                     0x387, 1);
      system_exit(-1);
    }
    if (dynamic_triangle_buffer_index < 0) {
      display_assert("dynamic_triangle_buffer_index>=0", kDrawPrimitivesFile,
                     0x38a, 1);
      system_exit(-1);
    }
    if (dynamic_triangle_buffer_index >= dynamic_triangles_buffer_count) {
      display_assert(
        "dynamic_triangle_buffer_index<dynamic_triangles.buffer_count",
        kDrawPrimitivesFile, 0x38b, 1);
      system_exit(-1);
    }
    dynamic_triangle_buffer =
      &dynamic_triangle_buffers[dynamic_triangle_buffer_index];
    vertex_size0 = rasterizer_geometry_get_vertex_size(vertex_buffer0->type);
    vertex_size1 = rasterizer_geometry_get_vertex_size(vertex_buffer1->type);
    if (dynamic_triangle_buffer->triangle_start_index < 0) {
      display_assert("dynamic_triangle_buffer->triangle_start_index>=0",
                     kDrawPrimitivesFile, 0x393, 1);
      system_exit(-1);
    }
    if (triangle_count < 0) {
      display_assert("triangle_count>=0", kDrawPrimitivesFile, 0x394, 1);
      system_exit(-1);
    }
    if (triangle_count >
        dynamic_triangle_buffer->triangle_count - first_triangle_index) {
      display_assert("triangle_count<=dynamic_triangle_buffer->triangle_count "
                     "- first_triangle_index",
                     kDrawPrimitivesFile, 0x395, 1);
      system_exit(-1);
    }
    local_triangle_count = triangle_count;
    if (local_triangle_count > RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND) {
      local_triangle_count = RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND;
    }
    D3DDevice_SetStreamSource(0, vertex_buffer0->hardware_format,
                              (uint32_t)vertex_size0);
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0,
                       "IDirect3DDevice8_SetStreamSource(global_d3d_device, 0, "
                       "(IDirect3DVertexBuffer8*)vertex_buffer0->hardware_"
                       "format, vertex_size0)");
    }
    D3DDevice_SetStreamSource(1, vertex_buffer1->hardware_format,
                              (uint32_t)vertex_size1);
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0,
                       "IDirect3DDevice8_SetStreamSource(global_d3d_device, 1, "
                       "(IDirect3DVertexBuffer8*)vertex_buffer1->hardware_"
                       "format, vertex_size1)");
    }
    D3DDevice_SetIndices(dynamic_triangles_d3d_index_buffer, 0);
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0, "IDirect3DDevice8_SetIndices(global_d3d_device, "
                          "dynamic_triangles.d3d_index_buffer, 0)");
    }
    D3DDevice_DrawIndexedVertices(
      5, (uint32_t)(local_triangle_count * 3),
      (const void *)((const int16_t *)index_data_base +
                     (first_triangle_index +
                      dynamic_triangle_buffer->triangle_start_index) *
                       3));
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0,
                       "IDirect3DDevice8_DrawIndexedPrimitive(global_d3d_"
                       "device, D3DPT_TRIANGLELIST, 0, vertex_buffer0->count, "
                       "NUMBER_OF_VERTICES_PER_TRIANGLE*(first_triangle_index "
                       "+ dynamic_triangle_buffer->triangle_start_index), "
                       "local_triangle_count)");
    }
    first_triangle_index += local_triangle_count;
    triangle_count -= local_triangle_count;
  }
  if (!success) {
    error(
      2, "### ERROR rasterizer_draw_dynamic_triangles_static_vertices2 failed");
  }
}

/* 0x15e0f0 */
void rasterizer_draw_static_triangles_dynamic_vertices(
  const triangle_buffer *triangle_buffer, int first_triangle_index,
  int triangle_count, int dynamic_vertex_buffer_index)
{
  boolean success;
  int local_triangle_vertex_indices_offset;
  dynamic_vertex_buffer *dynamic_vertex_buffer;
  dynamic_vertex_group *group;
  void *d3d_vertex_buffer;
  int vertex_size;
  int local_triangle_count;
  int primitive_type;

  success = 1;
  local_triangle_vertex_indices_offset = 0;
  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x3c3, 1);
    system_exit(-1);
  }
  while (triangle_count > 0) {
    if (triangle_buffer == 0 || triangle_buffer->hardware_format == 0 ||
        dynamic_vertex_buffer_index == -1) {
      break;
    }
    if (dynamic_triangles_d3d_index_buffer == 0) {
      display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                     0x3d1, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer_index < 0) {
      display_assert("dynamic_vertex_buffer_index>=0", kDrawPrimitivesFile,
                     0x3d4, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer_index >= dynamic_vertices_buffer_count) {
      display_assert(
        "dynamic_vertex_buffer_index<dynamic_vertices.buffer_count",
        kDrawPrimitivesFile, 0x3d5, 1);
      system_exit(-1);
    }
    dynamic_vertex_buffer =
      &dynamic_vertex_buffers[dynamic_vertex_buffer_index];
    vertex_size =
      rasterizer_geometry_get_vertex_size(dynamic_vertex_buffer->type);
    group = &dynamic_vertex_groups[dynamic_vertex_buffer->type];
    d3d_vertex_buffer = dynamic_vertex_group_get_d3d_vertex_buffer(group);
    if (d3d_vertex_buffer == 0) {
      display_assert("d3d_vertex_buffer", kDrawPrimitivesFile, 0x3df, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer->vertex_start_index < 0) {
      display_assert("dynamic_vertex_buffer->vertex_start_index>=0",
                     kDrawPrimitivesFile, 0x3e2, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer->vertex_start_index >
        group->vertex_count - dynamic_vertex_buffer->vertex_count) {
      display_assert("dynamic_vertex_buffer->vertex_start_index<=group->vertex_"
                     "count - dynamic_vertex_buffer->vertex_count",
                     kDrawPrimitivesFile, 0x3e3, 1);
      system_exit(-1);
    }
    if (first_triangle_index != 0) {
      display_assert("first_triangle_index==0", kDrawPrimitivesFile, 0x3e6, 1);
      system_exit(-1);
    }
    if (triangle_buffer->type < 0 || triangle_buffer->type >= 2) {
      display_assert("triangle_buffer->type>=0 && "
                     "triangle_buffer->type<NUMBER_OF_TRIANGLE_BUFFER_TYPES",
                     kDrawPrimitivesFile, 0x3e7, 1);
      system_exit(-1);
    }
    local_triangle_count = triangle_count;
    if (local_triangle_count > RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND) {
      local_triangle_count = RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND;
    }
    D3DDevice_SetStreamSource(0, d3d_vertex_buffer, (uint32_t)vertex_size);
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0, "IDirect3DDevice8_SetStreamSource(global_d3d_device, "
                          "0, d3d_vertex_buffer, vertex_size)");
    }
    D3DDevice_SetIndices(triangle_buffer->hardware_format,
                         (uint32_t)dynamic_vertex_buffer->vertex_start_index);
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0, "IDirect3DDevice8_SetIndices(global_d3d_device, "
                          "(IDirect3DIndexBuffer8*)triangle_buffer->hardware_"
                          "format, dynamic_vertex_buffer->vertex_start_index)");
    }
    primitive_type = triangle_buffer_primitive_types[triangle_buffer->type];
    D3DDevice_DrawIndexedVertices(
      (uint32_t)primitive_type,
      (uint32_t)(primitive_index_count_multipliers[primitive_type * 2] *
                   local_triangle_count +
                 primitive_index_count_addends[primitive_type * 2]),
      (const void *)((const int16_t *)index_data_base +
                     local_triangle_vertex_indices_offset));
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_DrawIndexedPrimitive(global_d3d_device, "
           "d3d_primitive_type_table[triangle_buffer->type], 0, "
           "dynamic_vertex_buffer->vertex_count, "
           "local_triangle_vertex_indices_offset, local_triangle_count)");
    }
    triangle_count -= local_triangle_count;
    if (triangle_buffer->type == 0) {
      local_triangle_vertex_indices_offset += local_triangle_count * 3;
    } else if (triangle_buffer->type == 1) {
      local_triangle_vertex_indices_offset += local_triangle_count;
    } else {
      display_assert("### ERROR unsupported triangle buffer type",
                     kDrawPrimitivesFile, 0x406, 1);
      system_exit(-1);
    }
  }
  if (!success) {
    error(2,
          "### ERROR rasterizer_draw_static_triangles_dynamic_vertices failed");
  }
}

/* 0x15e430 */
void rasterizer_draw_static_triangles_static_vertices(
  const triangle_buffer *triangle_buffer, int first_triangle_index,
  int triangle_count, const vertex_buffer *vertex_buffer)
{
  boolean success;
  int local_triangle_vertex_indices_offset;
  int vertex_size;
  int local_triangle_count;
  int primitive_type;

  success = 1;
  local_triangle_vertex_indices_offset = 0;
  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x427, 1);
    system_exit(-1);
  }
  while (triangle_count > 0) {
    if (triangle_buffer == 0 || triangle_buffer->hardware_format == 0 ||
        vertex_buffer == 0 || vertex_buffer->hardware_format == 0) {
      break;
    }
    if (dynamic_triangles_d3d_index_buffer == 0) {
      display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                     0x431, 1);
      system_exit(-1);
    }
    if (first_triangle_index != 0) {
      display_assert("first_triangle_index==0", kDrawPrimitivesFile, 0x436, 1);
      system_exit(-1);
    }
    if (triangle_buffer->type < 0 || triangle_buffer->type >= 2) {
      display_assert("triangle_buffer->type>=0 && "
                     "triangle_buffer->type<NUMBER_OF_TRIANGLE_BUFFER_TYPES",
                     kDrawPrimitivesFile, 0x437, 1);
      system_exit(-1);
    }
    vertex_size = rasterizer_geometry_get_vertex_size(vertex_buffer->type);
    local_triangle_count = triangle_count;
    if (local_triangle_count > RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND) {
      local_triangle_count = RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND;
    }
    D3DDevice_SetStreamSource(0, vertex_buffer->hardware_format,
                              (uint32_t)vertex_size);
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(0,
                       "IDirect3DDevice8_SetStreamSource(global_d3d_device, 0, "
                       "(IDirect3DVertexBuffer8*)vertex_buffer->hardware_"
                       "format, vertex_size)");
    }
    D3DDevice_SetIndices(triangle_buffer->hardware_format, 0);
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetIndices(global_d3d_device, "
           "(IDirect3DIndexBuffer8*)triangle_buffer->hardware_format, 0)");
    }
    primitive_type = triangle_buffer_primitive_types[triangle_buffer->type];
    D3DDevice_DrawIndexedVertices(
      (uint32_t)primitive_type,
      (uint32_t)(primitive_index_count_multipliers[primitive_type * 2] *
                   local_triangle_count +
                 primitive_index_count_addends[primitive_type * 2]),
      (const void *)((const int16_t *)index_data_base +
                     local_triangle_vertex_indices_offset));
    if (success)
      success = 1;
    else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_DrawIndexedPrimitive(global_d3d_device, "
           "d3d_primitive_type_table[triangle_buffer->type], 0, "
           "vertex_buffer->count, local_triangle_vertex_indices_offset, "
           "local_triangle_count)");
    }
    triangle_count -= local_triangle_count;
    if (triangle_buffer->type == 0) {
      local_triangle_vertex_indices_offset += local_triangle_count * 3;
    } else if (triangle_buffer->type == 1) {
      local_triangle_vertex_indices_offset += local_triangle_count;
    } else {
      display_assert("### ERROR unsupported triangle buffer type",
                     kDrawPrimitivesFile, 0x459, 1);
      system_exit(-1);
    }
  }
  if (!success) {
    error(2,
          "### ERROR rasterizer_draw_static_triangles_static_vertices failed");
  }
}

/* 0x15e650 */
void rasterizer_draw(const triangle_buffer *triangle_buffer,
                     int dynamic_triangle_buffer_index,
                     int first_triangle_index, int triangle_count,
                     const vertex_buffer *vertex_buffer,
                     int dynamic_vertex_buffer_index)
{
  if (triangle_buffer == 0) {
    if (dynamic_triangle_buffer_index != -1) {
      display_assert("triangle_buffer || dynamic_triangle_buffer_index!=NONE",
                     kDrawPrimitivesFile, 0x479, 1);
      system_exit(-1);
    }
  } else if (dynamic_triangle_buffer_index != -1) {
    display_assert("!triangle_buffer || dynamic_triangle_buffer_index==NONE",
                   kDrawPrimitivesFile, 0x47a, 1);
    system_exit(-1);
  }
  if (vertex_buffer == 0) {
    if (dynamic_vertex_buffer_index != -1) {
      display_assert("vertex_buffer || dynamic_vertex_buffer_index!=NONE",
                     kDrawPrimitivesFile, 0x47d, 1);
      system_exit(-1);
    }
  } else if (dynamic_vertex_buffer_index != -1) {
    display_assert("!vertex_buffer || dynamic_vertex_buffer_index==NONE",
                   kDrawPrimitivesFile, 0x47e, 1);
    system_exit(-1);
  }
  if (triangle_buffer != 0) {
    if (vertex_buffer != 0) {
      rasterizer_draw_static_triangles_static_vertices(
        triangle_buffer, first_triangle_index, triangle_count, vertex_buffer);
    } else {
      rasterizer_draw_static_triangles_dynamic_vertices(
        triangle_buffer, first_triangle_index, triangle_count,
        dynamic_vertex_buffer_index);
    }
  } else if (vertex_buffer != 0) {
    rasterizer_draw_dynamic_triangles_static_vertices(
      dynamic_triangle_buffer_index, first_triangle_index, triangle_count,
      vertex_buffer);
  } else {
    rasterizer_draw_dynamic_triangles_dynamic_vertices(
      dynamic_triangle_buffer_index, first_triangle_index, triangle_count,
      dynamic_vertex_buffer_index);
  }
}

/* 0x15e770 -- out-of-line copy of the XDK IDirect3DVertexBuffer8_Lock inline.
 * MSVC kept size_to_lock/ppb_data/flags in EDX/ECX/EAX and passed only the
 * first two operands on the stack (RET 8); the forwarded call at 0x15e77e
 * pushes them back in D3DVertexBuffer_Lock's declared order. */
int IDirect3DVertexBuffer8_Lock_1(void *vertex_buffer, uint32_t offset_to_lock,
                 uint32_t size_to_lock, void **ppb_data, uint32_t flags)
{
  D3DVertexBuffer_Lock(vertex_buffer, offset_to_lock, size_to_lock, ppb_data,
                       flags);
  return 0;
}

/* 0x15e7a0 -- out-of-line copy of the XDK IDirect3DIndexBuffer8_Lock inline.
 * The resource pointer arrives in EAX and the out-pointer in EDX; the three
 * stack operands (RET 0xc) are offset/size/flags, of which only the offset is
 * read.  The data pointer lives at +4 in the D3D resource header. */
void D3DIndexBuffer_Lock_0(void *index_buffer, uint32_t offset_to_lock,
                  uint32_t size_to_lock, void **ppb_data, uint32_t flags)
{
  (void)size_to_lock;
  (void)flags;
  *ppb_data = (void *)((char *)((void **)index_buffer)[1] + offset_to_lock);
}

/* 0x15e7d0 -- same shape as 0x15e7a0 but returns D3D_OK in EAX. */
int IDirect3DIndexBuffer8_Lock_0(void *index_buffer, uint32_t offset_to_lock,
                 uint32_t size_to_lock, void **ppb_data, uint32_t flags)
{
  (void)size_to_lock;
  (void)flags;
  *ppb_data = (void *)((char *)((void **)index_buffer)[1] + offset_to_lock);
  return 0;
}

/* 0x15e800 */
boolean rasterizer_dynamic_geometry_initialize(void)
{
  boolean success;
  int result;
  int16_t vertex_type;
  dynamic_vertex_group *group;
  int count;

  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x5d, 1);
    system_exit(-1);
  }
  result = D3DDevice_CreateIndexBuffer(
    SIZEOF_RASTERIZER_TRIANGLE * RASTERIZER_MAXIMUM_DYNAMIC_TRIANGLES,
    RASTERIZER_DYNAMIC_BUFFER_USAGE, D3DFMT_INDEX16,
    RASTERIZER_DYNAMIC_BUFFER_POOL, &dynamic_triangles_d3d_index_buffer);
  if (result >= 0) {
    success = 1;
  } else {
    success = 0;
    rasterizer_error(
      result,
      "IDirect3DDevice8_CreateIndexBuffer(global_d3d_device, sizeof(struct "
      "rasterizer_triangle)*RASTERIZER_MAXIMUM_DYNAMIC_TRIANGLES, "
      "RASTERIZER_DYNAMIC_BUFFER_USAGE, D3DFMT_INDEX16, "
      "RASTERIZER_DYNAMIC_BUFFER_POOL, &dynamic_triangles.d3d_index_buffer)");
  }
  if (dynamic_triangles_d3d_index_buffer == 0) {
    success = 0;
  }
  if (!success) {
    dynamic_triangles_d3d_index_buffer = 0;
    error(2, "### ERROR failed to create dynamic triangle buffer");
  }
  for (vertex_type = 0;
       success && vertex_type < NUMBER_OF_RASTERIZER_VERTEX_TYPES;
       vertex_type++) {
    group = &dynamic_vertex_groups[vertex_type];
    switch (vertex_type) {
    case DYNAMIC_UNLIT_VERTEX_TYPE:
      count = RASTERIZER_MAXIMUM_DYNAMIC_UNLIT_VERTICES;
      break;
    case DYNAMIC_LIT_VERTEX_TYPE:
    case DYNAMIC_SCREEN_VERTEX_TYPE:
      count = 0;
      break;
    case DEBUG_VERTEX_TYPE:
      count = RASTERIZER_MAXIMUM_DYNAMIC_DEBUG_VERTICES;
      break;
    case MODEL_COMPRESSED_VERTEX_TYPE:
      count = RASTERIZER_MAXIMUM_DYNAMIC_MODEL_VERTICES;
      break;
    default:
      count = 0;
      break;
    }
    if (count > 0) {
      result = D3DDevice_CreateVertexBuffer(
        (uint32_t)(rasterizer_geometry_get_vertex_size(vertex_type) * count),
        RASTERIZER_DYNAMIC_BUFFER_USAGE, 0, RASTERIZER_DYNAMIC_BUFFER_POOL,
        &group->d3d_vertex_buffer);
      if (success && result >= 0) {
        success = 1;
      } else {
        success = 0;
        rasterizer_error(
          result, "IDirect3DDevice8_CreateVertexBuffer(global_d3d_device, "
                  "rasterizer_geometry_get_vertex_size(vertex_type)*count, "
                  "RASTERIZER_DYNAMIC_BUFFER_USAGE, 0, "
                  "RASTERIZER_DYNAMIC_BUFFER_POOL, &group->d3d_vertex_buffer)");
      }
      if (group->d3d_vertex_buffer == 0) {
        success = 0;
      }
      if (!success) {
        group->d3d_vertex_buffer = 0;
        error(2, "### ERROR failed to create dynamic vertex buffer");
      }
    } else {
      group->d3d_vertex_buffer = 0;
    }
    group->maximum_vertex_count = count;
    group->total_vertex_count = count;
  }
  if (success) {
    result = D3DDevice_CreateVertexBuffer(
      (uint32_t)(rasterizer_geometry_get_vertex_size(
                   DYNAMIC_UNLIT_VERTEX_TYPE) *
                 RASTERIZER_MAXIMUM_DYNAMIC_UNLIT_VERTICES),
      RASTERIZER_DYNAMIC_BUFFER_USAGE, 0, RASTERIZER_DYNAMIC_BUFFER_POOL,
      &aux_dynamic_unlit_vb);
    if (result >= 0) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        result, "IDirect3DDevice8_CreateVertexBuffer(global_d3d_device, "
                "rasterizer_geometry_get_vertex_size(_rasterizer_vertex_type_"
                "dynamic_unlit)*RASTERIZER_MAXIMUM_DYNAMIC_UNLIT_VERTICES, "
                "RASTERIZER_DYNAMIC_BUFFER_USAGE, 0, "
                "RASTERIZER_DYNAMIC_BUFFER_POOL, &aux_dynamic_unlit_vb)");
    }
    if (aux_dynamic_unlit_vb == 0) {
      success = 0;
    }
    if (!success) {
      aux_dynamic_unlit_vb = 0;
    }
  }
  if (!success) {
    error(2, "### ERROR failed to initialize rasterizer dynamic geometry");
  }
  return success;
}

/* 0x15e9e0 */
void rasterizer_dynamic_geometry_dispose(void)
{
  dynamic_vertex_group *group;
  int count;
  void *resource;

  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x107, 1);
    system_exit(-1);
  }
  group = dynamic_vertex_groups;
  count = NUMBER_OF_RASTERIZER_VERTEX_TYPES;
  do {
    resource = group->d3d_vertex_buffer;
    if (resource != 0) {
      D3DResource_Release(resource);
      group->d3d_vertex_buffer = 0;
    }
    group++;
    count--;
  } while (count != 0);
  resource = aux_dynamic_unlit_vb;
  if (resource != 0) {
    D3DResource_Release(resource);
    aux_dynamic_unlit_vb = 0;
  }
  resource = dynamic_triangles_d3d_index_buffer;
  if (resource != 0) {
    D3DResource_Release(resource);
    dynamic_triangles_d3d_index_buffer = 0;
  }
}

/* 0x15ea70 */
void *__rasterizer_dynamic_triangles_lock(int dynamic_triangle_buffer_index)
{
  dynamic_triangle_buffer *dynamic_triangle_buffer;
  void *triangles;

  triangles = 0;
  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x14b, 1);
    system_exit(-1);
  }
  if (dynamic_triangle_buffer_index != -1) {
    if (dynamic_triangle_buffer_index < 0) {
      display_assert("dynamic_triangle_buffer_index>=0", kDrawPrimitivesFile,
                     0x151, 1);
      system_exit(-1);
    }
    if (dynamic_triangle_buffer_index >= dynamic_triangles_buffer_count) {
      display_assert(
        "dynamic_triangle_buffer_index<dynamic_triangles.buffer_count",
        kDrawPrimitivesFile, 0x152, 1);
      system_exit(-1);
    }
    if (dynamic_triangles_d3d_index_buffer == 0) {
      display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                     0x154, 1);
      system_exit(-1);
    }
    dynamic_triangle_buffer =
      &dynamic_triangle_buffers[dynamic_triangle_buffer_index];
    if (dynamic_triangle_buffer->triangle_count <= 0) {
      display_assert("dynamic_triangle_buffer->triangle_count>0",
                     kDrawPrimitivesFile, 0x158, 1);
      system_exit(-1);
    }
    /* IDirect3DIndexBuffer8_Lock is an XDK inline: the data pointer lives at
     * +4 in D3DIndexBuffer (MOV EDX,[ECX+4] at 0x15eb5e) and the offset is
     * added to that pointer, not to the resource. */
    dynamic_triangle_buffer->triangles =
      (int16_t *)(*(char **)((char *)dynamic_triangles_d3d_index_buffer + 4) +
                  SIZEOF_RASTERIZER_TRIANGLE *
                    dynamic_triangle_buffer->triangle_start_index);
    unk_47dbec = 0;
    triangles = dynamic_triangle_buffer->triangles;
  } else {
    error(2, "### WARNING tried to lock dynamic triangles with index=NONE");
  }
  return triangles;
}

/* 0x15eb90 */
void __rasterizer_dynamic_triangles_unlock(int dynamic_triangle_buffer_index)
{
  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x175, 1);
    system_exit(-1);
  }
  if (dynamic_triangle_buffer_index != -1) {
    if (dynamic_triangle_buffer_index < 0) {
      display_assert("dynamic_triangle_buffer_index>=0", kDrawPrimitivesFile,
                     0x179, 1);
      system_exit(-1);
    }
    if (dynamic_triangle_buffer_index >= dynamic_triangles_buffer_count) {
      display_assert(
        "dynamic_triangle_buffer_index<dynamic_triangles.buffer_count",
        kDrawPrimitivesFile, 0x17a, 1);
      system_exit(-1);
    }
    if (dynamic_triangles_d3d_index_buffer == 0) {
      display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                     0x17c, 1);
      system_exit(-1);
    }
  } else {
    error(2, "### WARNING tried to unlock dynamic triangles with index=NONE");
  }
}

/* 0x15ec50 */
void *__rasterizer_dynamic_vertices_lock(int dynamic_vertex_buffer_index)
{
  dynamic_vertex_buffer *dynamic_vertex_buffer;
  dynamic_vertex_group *group;
  void *d3d_vertex_buffer;
  int vertex_size;
  void *vertices;
  uint32_t flags;

  vertices = 0;
  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x207, 1);
    system_exit(-1);
  }
  if (dynamic_vertices_lock_operation == 0) {
    error(2, "### WARNING: tried to lock dynamic vertices without specifying a "
             "lock operation");
  }
  if (dynamic_vertex_buffer_index != -1) {
    if (dynamic_vertex_buffer_index < 0) {
      display_assert("dynamic_vertex_buffer_index>=0", kDrawPrimitivesFile,
                     0x217, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer_index >= dynamic_vertices_buffer_count) {
      display_assert(
        "dynamic_vertex_buffer_index<dynamic_vertices.buffer_count",
        kDrawPrimitivesFile, 0x218, 1);
      system_exit(-1);
    }
    if (dynamic_triangles_d3d_index_buffer == 0) {
      display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                     0x21b, 1);
      system_exit(-1);
    }
    dynamic_vertex_buffer =
      &dynamic_vertex_buffers[dynamic_vertex_buffer_index];
    vertex_size =
      rasterizer_geometry_get_vertex_size(dynamic_vertex_buffer->type);
    if (dynamic_vertex_buffer->type < 0) {
      display_assert("dynamic_vertex_buffer->type>=0", kDrawPrimitivesFile,
                     0x220, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer->type >= NUMBER_OF_RASTERIZER_VERTEX_TYPES) {
      display_assert(
        "dynamic_vertex_buffer->type<NUMBER_OF_RASTERIZER_VERTEX_TYPES",
        kDrawPrimitivesFile, 0x221, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer->vertex_count <= 0) {
      display_assert("dynamic_vertex_buffer->vertex_count>0",
                     kDrawPrimitivesFile, 0x222, 1);
      system_exit(-1);
    }
    group = &dynamic_vertex_groups[dynamic_vertex_buffer->type];
    d3d_vertex_buffer = dynamic_vertex_group_get_d3d_vertex_buffer(group);
    if (d3d_vertex_buffer == 0) {
      display_assert("d3d_vertex_buffer", kDrawPrimitivesFile, 0x227, 1);
      system_exit(-1);
    }
    flags = group->first_lock ? 0 : D3DLOCK_READONLY;
    D3DVertexBuffer_Lock(
      d3d_vertex_buffer,
      (uint32_t)(vertex_size * dynamic_vertex_buffer->vertex_start_index),
      (uint32_t)(vertex_size * dynamic_vertex_buffer->vertex_count),
      (void **)&dynamic_vertex_buffer->vertices, flags);
    group->first_lock = 0;
    vertices = dynamic_vertex_buffer->vertices;
  } else {
    error(2, "### WARNING tried to lock dynamic vertices with index=NONE");
  }
  return vertices;
}

/* 0x15ee80 */
void __rasterizer_dynamic_vertices_unlock(int dynamic_vertex_buffer_index)
{
  dynamic_vertex_buffer *dynamic_vertex_buffer;
  dynamic_vertex_group *group;
  void *d3d_vertex_buffer;

  if (global_d3d_device == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x246, 1);
    system_exit(-1);
  }
  if (dynamic_vertex_buffer_index != -1) {
    if (dynamic_vertex_buffer_index < 0) {
      display_assert("dynamic_vertex_buffer_index>=0", kDrawPrimitivesFile,
                     0x24c, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer_index >= dynamic_vertices_buffer_count) {
      display_assert(
        "dynamic_vertex_buffer_index<dynamic_vertices.buffer_count",
        kDrawPrimitivesFile, 0x24d, 1);
      system_exit(-1);
    }
    if (dynamic_triangles_d3d_index_buffer == 0) {
      display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                     0x24f, 1);
      system_exit(-1);
    }
    dynamic_vertex_buffer =
      &dynamic_vertex_buffers[dynamic_vertex_buffer_index];
    if (dynamic_vertex_buffer->type < 0 ||
        dynamic_vertex_buffer->type >= NUMBER_OF_RASTERIZER_VERTEX_TYPES) {
      display_assert(
        "buffer->type>=0 && buffer->type<NUMBER_OF_RASTERIZER_VERTEX_TYPES",
        kDrawPrimitivesFile, 0x253, 1);
      system_exit(-1);
    }
    group = &dynamic_vertex_groups[dynamic_vertex_buffer->type];
    d3d_vertex_buffer = dynamic_vertex_group_get_d3d_vertex_buffer(group);
    if (d3d_vertex_buffer == 0) {
      display_assert("d3d_vertex_buffer", kDrawPrimitivesFile, 0x258, 1);
      system_exit(-1);
    }
  } else {
    error(2, "### WARNING tried to unlock dynamic vertices with index=NONE");
  }
}
