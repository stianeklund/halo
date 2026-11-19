/* render_debug.c -- debug primitive rendering
 * (c:\halo\SOURCE\render\render_debug.c)
 *
 * The debug renderer keeps a per-frame cache of debug primitives (points,
 * lines, boxes, text, ...) submitted through the cache writer at 0x188ec0.
 * Each cache record is 0x38 (56) bytes; the leading short is the primitive
 * type. render_debug (0x18ac50) flushes the cache once per frame: it runs the
 * fixed set of debug sub-renderers, walks the cache dispatching each record to
 * its draw routine, then clears the cache when the game frame advances.
 */

#include <stdarg.h>
#include <limits.h>

#include "x87_math.h"

enum debug_primitive_type {
  _debug_primitive_circle,
  _debug_primitive_point,
  _debug_primitive_line,
  _debug_primitive_sphere,
  _debug_primitive_cylinder,
  _debug_primitive_pill,
  _debug_primitive_box,
  _debug_primitive_box_outline,
  _debug_primitive_string,
  _debug_primitive_string_at_point
};

typedef struct debug_primitive {
  short type;
  short pad_02;
  union {
    struct {
      real_plane3d plane;
      short projection;
      uint8_t sign;
      uint8_t pad_17;
      real_point2d center;
      float radius;
      real_argb_color color;
      float offset;
    } circle;
    struct {
      real_point3d position;
      float scale;
      real_argb_color color;
    } point;
    struct {
      real_point3d point0;
      real_point3d point1;
      real_argb_color color;
    } line;
    struct {
      real_point3d center;
      float radius;
      real_argb_color color;
    } sphere;
    struct {
      real_point3d base;
      real_vector3d height;
      float radius;
      real_argb_color color;
    } cylinder;
    struct {
      real_rectangle3d bounds;
      real_argb_color color;
    } box;
    struct {
      const char *string;
      real_point3d position;
      real_argb_color color;
    } string;
  } data;
} debug_primitive;

typedef char
  debug_primitive_size_check[sizeof(debug_primitive) == 0x38 ? 1 : -1];

#define k_radians_to_degrees (*(const float *)0x2b073c)
#define k_runtime_decal_yaw_scale (*(const float *)0x2b1958)
#define k_runtime_decal_pitch_scale (*(const float *)0x2b1954)
#define debug_primitives ((debug_primitive *)0x4d1220)
#define debug_primitive_count (*(short *)0x4d8224)
#define debug_primitive_frame (*(short *)0x4d8220)

/* Per-frame debug string arena: char[0x400] at 0x4d0e20, ending at 0x4d121f
 * (immediately before the primitive cache at 0x4d1220). Text primitives intern
 * their string here; the cursor at 0x4d8228 counts bytes used (max 0x3ff). */
#define debug_string_pool ((char *)0x4d0e20)
#define debug_string_pool_count (*(short *)0x4d8228)
#define debug_string_overflow_warned (*(char *)0x4d822b)

#define global_real_argb_red (*(const real_argb_color **)0x2ee6d0)
#define global_real_argb_green (*(const real_argb_color **)0x2ee6d4)
#define global_real_argb_blue (*(const real_argb_color **)0x2ee6d8)
#define global_real_argb_yellow (*(const real_argb_color **)0x2ee6e0)
#define global_real_argb_orange (*(const real_argb_color **)0x2ee6f0)
#define global_up3d (*(real_vector3d **)0x31fc44)
#define player_data (*(data_t **)0x5aa6d4)

#define global_scenario_decal_palette \
  (&((scenario_structure_bsps_t *)global_scenario_get())->decal_palette)

/* structure_bsp_t leaves (structure_leaf_t) and lightmaps
 * (structure_lightmap_t) blocks. */
typedef struct structure_bsp_render_debug {
  char pad_00[0xe0];
  tag_block leaves;
  char pad_ec[0x104 - 0xec];
  tag_block lightmaps;
} structure_bsp_render_debug_t;

#define global_structure_bsp_leaves \
  (&((structure_bsp_render_debug_t *)global_structure_bsp_get())->leaves)
#define structure_bsp_lightmaps(bsp) \
  (&((structure_bsp_render_debug_t *)(bsp))->lightmaps)

typedef struct structure_lightmap {
  char pad_00[0x14];
  tag_block materials;
} structure_lightmap_t;

typedef struct structure_material {
  char pad_00[0x04];
  const char *shader_name;
  char pad_08[0x100 - 0x08];
} structure_material_t;

#define render_leaf_index (*(int32_t *)0x506780)
#define render_cluster_index (*(int32_t *)0x506784)

typedef struct rectangle2d {
  short top;
  short left;
  short bottom;
  short right;
} rectangle2d;

/* Structure-BSP leaf element; only the cluster index is proven. */
typedef struct structure_leaf {
  char pad_00[0x08];
  int16_t cluster_index;
  char pad_0a[0x10 - 0x0a];
} structure_leaf_t;

/* unit_data_t covers only the bytes below 0x424. */
typedef struct render_debug_biped {
  unit_data_t unit;
  char pad_424[0x42c - 0x424];
  int32_t field_42c; /* != NONE: "riding an elevator" */
} render_debug_biped;
#define DEBUG_CIRCLE_SEGMENT_COUNT 16
#define point3d_component(point, axis) (((real *)&(point))[axis])
#define global_down3d (*(real_vector3d **)0x31fc50)

#define debug_permanent_decals (*(char *)0x506530)
#define debug_input (*(char *)0x506531)
#define debug_bsp (*(char *)0x506532)
#define debug_structure (*(char *)0x506533)
#define debug_player (*(char *)0x506534)
#define debug_camera (*(char *)0x506535)
#define render_local_player_index (*(short *)0x506548)
#define render_camera_position (*(real_point3d *)0x506550)
#define render_camera_forward (*(real_vector3d *)0x50655c)

void render_debug_triangle(char flag, const real_point3d *point0,
                           const real_point3d *point1,
                           const real_point3d *point2, const real_argb_color *color)
{
  if (flag == 0) {
    display_assert("immediate", "c:\\halo\\SOURCE\\render\\render_debug.c",
                   0x1e0, 1);
    system_exit(-1);
  }
  if (point0 == 0) {
    display_assert("point0", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x1e1,
                   1);
    system_exit(-1);
  }
  if (point1 == 0) {
    display_assert("point1", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x1e2,
                   1);
    system_exit(-1);
  }
  if (point2 == 0) {
    display_assert("point2", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x1e3,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x1e4,
                   1);
    system_exit(-1);
  }
  rasterizer_debug_triangle(point0, point1, point2, color);
}

void render_debug_quadrilateral(char flag, const real_point3d *point0,
                                const real_point3d *point1,
                                const real_point3d *point2,
                                const real_point3d *point3, const real_argb_color *color)
{
  if (flag == 0) {
    display_assert("immediate", "c:\\halo\\SOURCE\\render\\render_debug.c",
                   0x1f3, 1);
    system_exit(-1);
  }
  if (point0 == 0) {
    display_assert("point0", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x1f4,
                   1);
    system_exit(-1);
  }
  if (point1 == 0) {
    display_assert("point1", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x1f5,
                   1);
    system_exit(-1);
  }
  if (point2 == 0) {
    display_assert("point2", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x1f6,
                   1);
    system_exit(-1);
  }
  if (point3 == 0) {
    display_assert("point3", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x1f7,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x1f8,
                   1);
    system_exit(-1);
  }
  render_debug_triangle(flag, point0, point1, point2, color);
  render_debug_triangle(flag, point0, point2, point3, color);
}

void render_debug_polygon(const real_point3d *points, short count, const real_argb_color *color)
{
  short i;
  const real_point3d *pi;

  if (points == 0) {
    display_assert("points", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x357,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x358,
                   1);
    system_exit(-1);
  }
  i = 1;
  if (1 < count - 1) {
    do {
      pi = points + i;
      render_debug_triangle(1, points, pi, pi + 1, color);
      i = (short)(i + 1);
    } while (i < count - 1);
  }
}

char *render_debug_add_cache_string(const char *str)
{
  char *result;
  short i;
  char *dst;
  short added;

  result = 0;
  i = 0;
  if (debug_string_pool_count > 0) {
    do {
      if (csstrcmp(str, &debug_string_pool[i]) == 0) {
        result = &debug_string_pool[i];
        if (result != 0) {
          goto done;
        }
        break;
      }
      i++;
    } while (i < debug_string_pool_count);
  }

  if (debug_string_pool_count < 0x3ff) {
    dst = &debug_string_pool[debug_string_pool_count];
    csstrncpy(dst, str, 0x3ff - debug_string_pool_count);
    debug_string_pool[0x3ff] = 0;
    added = (short)csstrlen(str);
    debug_string_pool_count = debug_string_pool_count + added + 1;
    if (debug_string_pool_count > 0x3ff) {
      debug_string_pool_count = 0x3ff;
    }
    return dst;
  }

  if (debug_string_overflow_warned == 0) {
    error(2, "render debug cache string overflow");
    debug_string_overflow_warned = 1;
  }
done:
  return result;
}

/* Build the debug circle vertex table (0x188bf0). Fills a 17-point table of
 * (x,y) vertices approximating a circle of the given radius in a plane, shared
 * by the sphere, cylinder and plane debug drawers. Point 0 is seeded at
 * (radius, 0); each following point is the previous one rotated by pi/8
 * (2*pi/16, so 16 segments span the full circle). The final entry (table[16],
 * float offset 0x20) duplicates point 0 to close the ring. `table` arrives in
 * ECX; the step is the standard CCW rotation x' = x*cos - y*sin,
 * y' = x*sin + y*cos, computed in double and narrowed to float on store. */
#define debug_circle_angle (*(double *)0x2b17e8) /* pi/8 = 2*pi/16 */

#if defined(_MSC_VER) && !defined(__clang__)
#define HALO_DEBUG_TRIG_SIN sin
#define HALO_DEBUG_TRIG_COS cos
#else
#define HALO_DEBUG_TRIG_SIN x87_fsin_d
#define HALO_DEBUG_TRIG_COS x87_fcos_d
#endif

void build_circle_points(real_point2d *table, float radius)
{
  x87_wide_t sn;
  x87_wide_t cs;
  short i;
  int idx;

  sn = HALO_DEBUG_TRIG_SIN(debug_circle_angle);
  memcpy(&table[0].x, &radius, sizeof(radius));
  table[0].y = 0.0f;
  idx = 0;
  cs = HALO_DEBUG_TRIG_COS(debug_circle_angle);
  i = 0;
  do {
    i = (short)(i + 1);
    table[idx + 1].x = (float)(cs * table[idx].x - sn * table[idx].y);
    table[idx + 1].y = (float)(sn * table[idx].x + cs * table[idx].y);
    idx = (int)i;
  } while (idx + 1 < DEBUG_CIRCLE_SEGMENT_COUNT);
  table[DEBUG_CIRCLE_SEGMENT_COUNT] = table[0];
}

#undef HALO_DEBUG_TRIG_SIN
#undef HALO_DEBUG_TRIG_COS

float build_height_matrix(real_matrix4x3 *matrix, const real_vector3d *height,
                          const real_point3d *position)
{
  float length;

  matrix->scale = 1.0f;
  matrix->up = *height;
  perpendicular3d(&matrix->up, &matrix->left);
  length = normalize3d(&matrix->up);
  normalize3d(&matrix->left);
  matrix->forward.i =
    matrix->left.j * matrix->up.k - matrix->up.j * matrix->left.k;
  matrix->forward.j =
    matrix->up.i * matrix->left.k - matrix->left.i * matrix->up.k;
  matrix->forward.k =
    matrix->left.i * matrix->up.j - matrix->up.i * matrix->left.j;
  matrix->position = *position;
  return length;
}

void build_pill_points(real_point3d *buffer1, real_point3d *buffer2,
                       const real_point3d *center,
                       const real_vector3d *height, float radius,
                       real_point3d *buffer3, real_point3d *buffer4,
                       real_point3d *buffer5, real_point3d *buffer6)
{
  real_matrix4x3 frame;
  real_point2d circle[DEBUG_CIRCLE_SEGMENT_COUNT + 1];
  float len;
  real_point3d *b1;
  real_point3d *b2;
  const real_point2d *cp;
  int i;
  int n;

  len = build_height_matrix(&frame, height, center);
  build_circle_points(circle, radius);
  if (buffer2 != 0 && buffer1 != 0) {
    b1 = buffer1;
    b2 = buffer2;
    cp = circle;
    n = DEBUG_CIRCLE_SEGMENT_COUNT + 1;
    do {
      b2->x = cp->x;
      b2->y = cp->y;
      b2->z = len;
      b1->x = cp->x;
      b1->y = cp->y;
      b1->z = 0.0f;
      matrix_transform_point(&frame, b2, b2);
      matrix_transform_point(&frame, b1, b1);
      cp = cp + 1;
      b1 = b1 + 1;
      b2 = b2 + 1;
      n = n - 1;
    } while (n != 0);
  }
  if (buffer3 != 0 && buffer4 != 0 && buffer5 != 0 && buffer6 != 0) {
    for (i = 0; i < 9; i++) {
      buffer3->x = 0.0f;
      buffer3->y = circle[i].x;
      buffer3->z = len + circle[i].y;
      buffer4->x = 0.0f;
      buffer4->y = circle[i + 8].x;
      buffer4->z = circle[i + 8].y;
      buffer5->x = circle[i].x;
      buffer5->y = 0.0f;
      buffer5->z = len + circle[i].y;
      buffer6->x = circle[i + 8].x;
      buffer6->y = 0.0f;
      buffer6->z = circle[i + 8].y;
      matrix_transform_point(&frame, buffer3, buffer3);
      matrix_transform_point(&frame, buffer4, buffer4);
      matrix_transform_point(&frame, buffer5, buffer5);
      matrix_transform_point(&frame, buffer6, buffer6);
      buffer3 = buffer3 + 1;
      buffer4 = buffer4 + 1;
      buffer5 = buffer5 + 1;
      buffer6 = buffer6 + 1;
    }
  }
}

/* Cache overflow one-shot warning flag (0x4d822a). */
#define cache_overflow_warned (*(char *)0x4d822a)

void render_debug_add_cache_entry(int type, ...)
{
  va_list ap;
  debug_primitive *rec;
  const real_point3d *point0;
  const real_point3d *point1;
  const real_vector3d *height;
  const real_argb_color *color;
  const real_plane3d *plane;
  const real_point2d *center;
  const real_rectangle3d *bounds;
  char *interned;
  short frame;
  short count;
  int i;
  int ival;
  double dval;

  frame = (short)game_time_get();
  if (debug_primitive_frame != frame) {
    debug_primitive_frame = (short)game_time_get();
    debug_string_pool_count = 0;
    debug_string_pool[0] = 0;
    count = 0;
  } else {
    count = debug_primitive_count;
    if (count >= 0x200) {
      if (cache_overflow_warned == 0) {
        error(2, "render debug cache overflow.");
        cache_overflow_warned = 1;
      }
      return;
    }
  }

  rec = &debug_primitives[count];
  debug_primitive_count = count + 1;
  rec->type = type;

  va_start(ap, type);
  switch (type) {
  case _debug_primitive_circle:
    plane = va_arg(ap, const real_plane3d *);
    rec->data.circle.plane = *plane;
    ival = va_arg(ap, int);
    rec->data.circle.projection = (short)ival;
    ival = va_arg(ap, int);
    rec->data.circle.sign = (uint8_t)ival;
    center = va_arg(ap, const real_point2d *);
    rec->data.circle.center = *center;
    dval = va_arg(ap, double);
    rec->data.circle.radius = (float)dval;
    color = va_arg(ap, const real_argb_color *);
    rec->data.circle.color = *color;
    dval = va_arg(ap, double);
    rec->data.circle.offset = (float)dval;
    break;
  case _debug_primitive_point:
  case _debug_primitive_sphere:
    point0 = va_arg(ap, const real_point3d *);
    rec->data.point.position = *point0;
    dval = va_arg(ap, double);
    rec->data.point.scale = (float)dval;
    color = va_arg(ap, const real_argb_color *);
    rec->data.point.color = *color;
    break;
  case _debug_primitive_line:
    point0 = va_arg(ap, const real_point3d *);
    rec->data.line.point0 = *point0;
    point1 = va_arg(ap, const real_point3d *);
    rec->data.line.point1 = *point1;
    color = va_arg(ap, const real_argb_color *);
    rec->data.line.color = *color;
    break;
  case _debug_primitive_cylinder:
    point0 = va_arg(ap, const real_point3d *);
    rec->data.cylinder.base = *point0;
    height = va_arg(ap, const real_vector3d *);
    rec->data.cylinder.height = *height;
    dval = va_arg(ap, double);
    rec->data.cylinder.radius = (float)dval;
    color = va_arg(ap, const real_argb_color *);
    rec->data.cylinder.color = *color;
    break;
  case _debug_primitive_pill:
    point0 = va_arg(ap, const real_point3d *);
    rec->data.cylinder.base = *point0;
    height = va_arg(ap, const real_vector3d *);
    rec->data.cylinder.height = *height;
    dval = va_arg(ap, double);
    rec->data.cylinder.radius = (float)dval;
    color = va_arg(ap, const real_argb_color *);
    rec->data.cylinder.color = *color;
    break;
  case _debug_primitive_box:
  case _debug_primitive_box_outline:
    bounds = va_arg(ap, const real_rectangle3d *);
    /* keep: an element loop, not a struct copy (register order) */
    for (i = 0; i < 6; i++) {
      ((float *)&rec->data.box.bounds)[i] = ((const float *)bounds)[i];
    }
    color = va_arg(ap, const real_argb_color *);
    rec->data.box.color = *color;
    break;
  case _debug_primitive_string:
    interned = render_debug_add_cache_string(va_arg(ap, char *));
    if (interned != 0) {
      rec->data.string.string = interned;
    } else {
      debug_primitive_count = debug_primitive_count - 1;
    }
    break;
  case _debug_primitive_string_at_point:
    interned = render_debug_add_cache_string(va_arg(ap, char *));
    if (interned == 0) {
      debug_primitive_count = debug_primitive_count - 1;
      break;
    }
    rec->data.string.string = interned;
    point0 = va_arg(ap, const real_point3d *);
    rec->data.string.position = *point0;
    color = va_arg(ap, const real_argb_color *);
    rec->data.string.color = *color;
    break;
  default:
    break;
  }
  va_end(ap);
}

void render_debug_point(char flag, const real_point3d *position, float scale,
                        const real_argb_color *color)
{
  real_point3d points[6];
  const real_point3d *point;
  int i;
  float s;

  if (position == 0) {
    display_assert("point", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x147,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x148,
                   1);
    system_exit(-1);
  }

  if (flag != 0) {
    s = scale * 0.5f;
    points[0].x = position->x - s;
    points[0].y = position->y;
    points[0].z = position->z;
    points[1].x = position->x + s;
    points[1].y = position->y;
    points[1].z = position->z;
    points[2].x = position->x;
    points[2].y = position->y - s;
    points[2].z = position->z;
    points[3].x = position->x;
    points[3].y = position->y + s;
    points[3].z = position->z;
    points[4].x = position->x;
    points[4].y = position->y;
    points[4].z = position->z - s;
    points[5].x = position->x;
    points[5].y = position->y;
    points[5].z = position->z + s;
    point = &points[0];
    i = 3;
    do {
      rasterizer_debug_line(point, point + 1, color);
      point = point + 2;
      i--;
    } while (i != 0);
    return;
  }

  render_debug_add_cache_entry(1, position, (double)scale, color);
}

__declspec(noinline) void render_debug_line(char flag,
                                            const real_point3d *point0,
                                            const real_point3d *point1,
                                            const real_argb_color *color)
{
  if (point0 == 0) {
    display_assert("point0", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x16b,
                   1);
    system_exit(-1);
  }
  if (point1 == 0) {
    display_assert("point1", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x16c,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x16d,
                   1);
    system_exit(-1);
  }

  if (flag != 0) {
    rasterizer_debug_line(point0, point1, color);
    return;
  }

  render_debug_add_cache_entry(2, point0, point1, color);
}

void render_debug_vector(int flag, const real_point3d *point,
                         const real_vector3d *vector, float scale,
                         const real_argb_color *color)
{
  real_point3d endpoint;

  if (point == 0) {
    display_assert("point", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x184,
                   1);
    system_exit(-1);
  }
  if (vector == 0) {
    display_assert("vector", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x185,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x186,
                   1);
    system_exit(-1);
  }
  endpoint.x = scale * vector->i + point->x;
  endpoint.y = scale * vector->j + point->y;
  endpoint.z = scale * vector->k + point->z;
  render_debug_line(flag, point, &endpoint, color);
}

void render_debug_tick(int flag, const real_point3d *point,
                       const real_vector3d *vector, float scale, const real_argb_color *color)
{
  real_point3d points[2];

  points[0].x = scale * vector->i + point->x;
  points[0].y = scale * vector->j + point->y;
  points[0].z = scale * vector->k + point->z;
  scale = -scale;
  points[1].x = scale * vector->i + point->x;
  points[1].y = scale * vector->j + point->y;
  points[1].z = scale * vector->k + point->z;
  render_debug_line(flag, &points[0], &points[1], color);
}

void render_debug_line_offset(int flag, const real_point3d *point0,
                              const real_point3d *point1,
                              const real_argb_color *color, float scale)
{
  real_point3d points[2];
  const real_vector3d *offset;

  offset = global_up3d;
  points[0].x = scale * offset->i + point0->x;
  points[0].y = scale * offset->j + point0->y;
  points[0].z = scale * offset->k + point0->z;
  points[1].x = scale * offset->i + point1->x;
  points[1].y = scale * offset->j + point1->y;
  points[1].z = scale * offset->k + point1->z;
  render_debug_line(flag, &points[0], &points[1], color);
}

void render_debug_matrix(int flag, const real_matrix4x3 *matrix, float scale)
{
  const real_point3d *origin;

  origin = &matrix->position;
  render_debug_vector(flag, origin, &matrix->forward, scale * matrix->scale,
                      global_real_argb_red);
  render_debug_vector(flag, origin, &matrix->left, scale * matrix->scale,
                      global_real_argb_green);
  render_debug_vector(flag, origin, &matrix->up, scale * matrix->scale,
                      global_real_argb_blue);
}

void render_debug_sphere(char flag, const real_point3d *center, float radius,
                         const real_argb_color *color)
{
  real_point2d circle[DEBUG_CIRCLE_SEGMENT_COUNT + 1];
  real_point3d points[2];
  const real_point3d *c;
  const real_point2d *cp;
  short i;
  short vis;

  if (center == 0) {
    display_assert("center", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x206,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x207,
                   1);
    system_exit(-1);
  }
  if (flag == 0) {
    render_debug_add_cache_entry(3, center, (double)radius, color);
    return;
  }
  vis = (short)render_frustum_sphere_visible(&global_frustum, center, radius);
  if (vis == 0) {
    return;
  }
  build_circle_points(circle, radius);
  c = center;
  cp = &circle[1];
  i = DEBUG_CIRCLE_SEGMENT_COUNT;
  do {
    /* XY-plane great circle */
    points[1].x = c->x + cp[-1].x;
    points[1].z = c->z;
    points[0].z = c->z;
    points[1].y = c->y + cp[-1].y;
    points[0].x = c->x + cp->x;
    points[0].y = c->y + cp->y;
    rasterizer_debug_line(&points[1], &points[0], color);
    /* XZ-plane great circle */
    points[1].x = c->x + cp[-1].y;
    points[1].y = c->y;
    points[1].z = c->z + cp[-1].x;
    points[0].y = c->y;
    points[0].x = c->x + cp->y;
    points[0].z = c->z + cp->x;
    rasterizer_debug_line(&points[1], &points[0], color);
    /* YZ-plane great circle */
    points[1].y = c->y + cp[-1].x;
    points[1].x = c->x;
    points[1].z = c->z + cp[-1].y;
    points[0].x = c->x;
    points[0].y = c->y + cp->x;
    points[0].z = c->z + cp->y;
    rasterizer_debug_line(&points[1], &points[0], color);
    cp = cp + 1;
    i = (short)(i - 1);
  } while (i != 0);
}

void render_debug_cylinder(char flag, const real_point3d *center,
                           const real_vector3d *height, float radius,
                           const real_argb_color *color)
{
  real_point3d points[34];
  int i;
  real_point3d *p1;
  real_point3d *p2;

  if (center == 0) {
    display_assert("base", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x285,
                   1);
    system_exit(-1);
  }
  if (height == 0) {
    display_assert("height", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x286,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x287,
                   1);
    system_exit(-1);
  }
  if (flag == 0) {
    render_debug_add_cache_entry(4, center, height, (double)radius, color);
    return;
  }
  build_pill_points(points, points + 17, center, height, radius, 0, 0, 0, 0);

  p1 = points;
  p2 = points + 17;
  for (i = 16; i > 0; i--) {
    rasterizer_debug_line(p1, p1 + 1, color);
    rasterizer_debug_line(p2, p2 + 1, color);
    p1 += 1;
    p2 += 1;
  }

  p1 = points;
  p2 = points + 17;
  for (i = 4; i > 0; i--) {
    rasterizer_debug_line(p1, p2, color);
    p1 += 4;
    p2 += 4;
  }

  p1 = points;
  p2 = points + 17;
  for (i = 2; i > 0; i--) {
    rasterizer_debug_line(p1, p1 + 8, color);
    rasterizer_debug_line(p2, p2 + 8, color);
    p1 += 4;
    p2 += 4;
  }
}

void render_debug_pill(char flag, const real_point3d *center,
                       const real_vector3d *height, float radius,
                       const real_argb_color *color)
{
  real_point3d buf1[17];
  real_point3d buf2[17];
  real_point3d buf3[9];
  real_point3d buf4[9];
  real_point3d buf5[9];
  real_point3d buf6[9];
  int i;

  if (center == 0) {
    display_assert("base", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x2b5,
                   1);
    system_exit(-1);
  }
  if (height == 0) {
    display_assert("height", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x2b6,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x2b7,
                   1);
    system_exit(-1);
  }
  if (flag == 0) {
    render_debug_add_cache_entry(5, center, height, (double)radius, color);
    return;
  }
  build_pill_points(buf1, buf2, center, height, radius, buf3, buf4, buf5, buf6);
  for (i = 0; i < 16; i++) {
    rasterizer_debug_line(&buf2[i], &buf2[i + 1], color);
    rasterizer_debug_line(&buf1[i], &buf1[i + 1], color);
  }
  for (i = 0; i < 4; i++) {
    rasterizer_debug_line(&buf2[i * 4], &buf1[i * 4], color);
  }
  for (i = 0; i < 8; i++) {
    rasterizer_debug_line(&buf3[i], &buf3[i + 1], color);
    rasterizer_debug_line(&buf4[i], &buf4[i + 1], color);
    rasterizer_debug_line(&buf5[i], &buf5[i + 1], color);
    rasterizer_debug_line(&buf6[i], &buf6[i + 1], color);
  }
}

void render_debug_box(char flag, const real_rectangle3d *bounds, const real_argb_color *color)
{
  real_point3d corners[8];

  if (bounds == 0) {
    display_assert("bounds", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x308,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x309,
                   1);
    system_exit(-1);
  }
  if (flag == 0) {
    render_debug_add_cache_entry(6, bounds, color);
    return;
  }
  corners[0].x = bounds->x0; /* c0 (x0,y0,z0) */
  corners[0].y = bounds->y0;
  corners[0].z = bounds->z0;
  corners[1].x = bounds->x1; /* c1 (x1,y0,z0) */
  corners[1].y = bounds->y0;
  corners[1].z = bounds->z0;
  corners[2].x = bounds->x0; /* c2 (x0,y0,z1) */
  corners[2].y = bounds->y0;
  corners[2].z = bounds->z1;
  corners[3].x = bounds->x1; /* c3 (x1,y0,z1) */
  corners[3].y = bounds->y0;
  corners[3].z = bounds->z1;
  corners[4].x = bounds->x0; /* c4 (x0,y1,z1) */
  corners[4].y = bounds->y1;
  corners[4].z = bounds->z1;
  corners[5].x = bounds->x1; /* c5 (x1,y1,z1) */
  corners[5].y = bounds->y1;
  corners[5].z = bounds->z1;
  corners[6].x = bounds->x0; /* c6 (x0,y1,z0) */
  corners[6].y = bounds->y1;
  corners[6].z = bounds->z0;
  corners[7].x = bounds->x1; /* c7 (x1,y1,z0) */
  corners[7].y = bounds->y1;
  corners[7].z = bounds->z0;
  render_debug_quadrilateral(1, &corners[0], &corners[2], &corners[4], &corners[6], color);
  render_debug_quadrilateral(1, &corners[1], &corners[3], &corners[5], &corners[7], color);
  render_debug_quadrilateral(1, &corners[0], &corners[1], &corners[3], &corners[2], color);
  render_debug_quadrilateral(1, &corners[4], &corners[5], &corners[7], &corners[6],
               color);
  render_debug_quadrilateral(1, &corners[0], &corners[1], &corners[7], &corners[6], color);
  render_debug_quadrilateral(1, &corners[2], &corners[3], &corners[5], &corners[4], color);
}

void render_debug_polygon_edges(const real_point3d *points, short count,
                                const real_argb_color *color)
{
  unsigned int n;

  if (points == 0) {
    display_assert("points", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x369,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x36a,
                   1);
    system_exit(-1);
  }
  if (2 < count) {
    render_debug_line(1, points + (count - 1), points, color);
    if (1 < count) {
      points = points + 1;
      n = (unsigned short)(count - 1);
      do {
        render_debug_line(1, points - 1, points, color);
        points = points + 1;
        n = n - 1;
      } while (n != 0);
    }
  }
}

void render_debug_string(char flag, const char *string)
{
  if (string == 0) {
    display_assert("string", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x37d,
                   1);
    system_exit(-1);
  }

  if (flag != 0) {
    interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
    rasterizer_text_draw(0, 0, 0, 0, string);
    return;
  }

  render_debug_add_cache_entry(8, string);
}

void render_debug_string_at_point(char flag, const real_point3d *position,
                                  const char *string,
                                  const real_argb_color *color)
{
  real_point2d screen_point;
  rectangle2d text_bounds;
  char visible;

  if (position == 0) {
    display_assert("point", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x392,
                   1);
    system_exit(-1);
  }
  if (string == 0) {
    display_assert("string", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x393,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x394,
                   1);
    system_exit(-1);
  }
  if (flag == 0) {
    render_debug_add_cache_entry(9, string, position, color);
    return;
  }
  visible = render_camera_world_to_screen(&unknown_global_camera, &global_frustum,
                                          position, &screen_point);
  if (visible != 0) {
    text_bounds.left = (short)(int)(screen_point.x - (float)screen_bounds_left);
    text_bounds.top = (short)(int)(screen_point.y - (float)screen_bounds_top);
    text_bounds.bottom = SHRT_MAX;
    text_bounds.right = SHRT_MAX;
    interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
    draw_string_set_color(color);
    rasterizer_text_draw(&text_bounds, 0, 0, 0, string);
  }
}

void render_debug_camera(void)
{
  char text[0x800];
  collision_test_result collision_result;
  real_vector3d forward;
  real_point3d collision_point;
  real_vector3d down;
  float t;
  float s;
  int32_t surface_index;
  int16_t lightmap_index;
  int16_t material_index;
  location_t location;
  const real_vector3d *down_direction;
  structure_lightmap_t *lightmap;
  structure_material_t *material;

  if (debug_camera == 0) {
    return;
  }
  scenario_location_from_point(&location, &render_camera_position);
  snprintf(text, sizeof(text),
           "point(%01.2f,%01.2f,%01.2f) leaf(#%d [%d]) cluster(#%d [%d])",
           render_camera_position.x, render_camera_position.y, render_camera_position.z,
           location.leaf_index, render_leaf_index, (int)location.cluster_index,
           render_cluster_index);
  down_direction = global_down3d;
  down.i = down_direction->i * 1000.0f;
  down.j = down_direction->j * 1000.0f;
  down.k = down_direction->k * 1000.0f;
  if (collision_test_vector(0x21, &render_camera_position, &down, NONE,
                   &collision_result)) {
    snprintf(
      text + csstrlen(text), sizeof(text) - csstrlen(text),
      "|nground_point(%01.2f,%01.2f,%01.2f) facing(%01.2f) surface(#%d)",
      collision_result.position.x, collision_result.position.y,
      collision_result.position.z,
      x87_fatan2f(render_camera_forward.j, render_camera_forward.i) * k_radians_to_degrees,
      collision_result.surface_index);
  }
  forward.i = render_camera_forward.i * 50.0f;
  forward.j = render_camera_forward.j * 50.0f;
  forward.k = render_camera_forward.k * 50.0f;
  if (structure_test_vector(&render_camera_position, &forward, &collision_point, &lightmap_index,
                            &material_index, &surface_index, &s,
                            &t)) {
    lightmap = tag_block_get_element(structure_bsp_lightmaps(scenario_get()),
                                     lightmap_index,
                                     sizeof(structure_lightmap_t));
    material = tag_block_get_element(&lightmap->materials, material_index,
                                     sizeof(structure_material_t));
    snprintf(text + csstrlen(text), sizeof(text) - csstrlen(text), "|n%s",
             material->shader_name);
  }
  interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
  rasterizer_text_draw(0, 0, 0, 0, text);
}

void render_debug_player(void)
{
  char buffer[0x400];
  int player_idx;
  player_data_t *player;
  int unit_handle;
  render_debug_biped *unit;
  vehicle_data_t *vehicle;
  char *stuck_str;

  if (debug_player == 0 || render_local_player_index == -1 ||
      local_player_get_player_index(render_local_player_index) == -1) {
    return;
  }
  player_idx = local_player_get_player_index(render_local_player_index);
  player = datum_get(player_data, player_idx);
  unit_handle = player->unit_handle;
  if (unit_handle == -1) {
    return;
  }
  unit = object_try_and_get_and_verify_type(unit_handle, 1);
  if (unit != 0 && unit->field_42c != -1) {
    render_debug_string(1, "riding an elevator");
  }
  if (unit->unit.object.parent_object_index.value != -1 &&
      unit->unit.parent_seat_index != -1) {
    vehicle =
      object_try_and_get_and_verify_type(unit->unit.object.parent_object_index.value, 2);
    if (vehicle->stuck_mass_point_flags != 0) {
      stuck_str = "|nstuck!";
    } else {
      stuck_str = "";
    }
    crt_sprintf(buffer, "speed %5f|nslide %5f|nturn  %5f%s", vehicle->speed,
                vehicle->slide, vehicle->turn, stuck_str);
    render_debug_string(1, buffer);
  }
}

void render_debug_structure(void)
{
  collision_bsp_t *bsp;
  tag_block *edges;
  tag_block *vertices;
  int i;
  collision_edge_t *edge;
  collision_vertex_t *start_vertex;
  collision_vertex_t *end_vertex;

  if (debug_structure != 0) {
    bsp = global_collision_bsp_get();
    edges = &bsp->edges;
    i = 0;
    if (0 < edges->count) {
      vertices = &bsp->vertices;
      do {
        edge = tag_block_get_element(edges, i, sizeof(collision_edge_t));
        start_vertex = tag_block_get_element(vertices, edge->start_vertex,
                                             sizeof(collision_vertex_t));
        end_vertex = tag_block_get_element(vertices, edge->end_vertex,
                                           sizeof(collision_vertex_t));
        render_debug_line(1, &start_vertex->point, &end_vertex->point,
                          global_real_argb_green);
        i++;
      } while (i < edges->count);
    }
  }
}

void render_debug_bsp(void)
{
  char text[0x800];
  int plane_stack[0x80];
  collision_bsp_t *root;
  short node_count;
  char *cursor;
  bsp3d_node_t *node;
  real_plane3d *plane;
  int node_index;
  int child;
  char side;
  int len;
  void *file;
  int i;

  if (debug_bsp == 0) {
    return;
  }
  node_count = 0;
  root = global_bsp3d_get();
  len = crt_sprintf(text, " node plane|n");
  cursor = text + len;
  node_index = 0;
  for (;;) {
    node = tag_block_get_element(&root->bsp3d_nodes, node_index,
                                 sizeof(bsp3d_node_t));
    plane = tag_block_get_element(&root->planes, node->plane,
                                  sizeof(real_plane3d));
    if (render_camera_position.y * plane->normal.j +
          render_camera_position.z * plane->normal.k +
          render_camera_position.x * plane->normal.i - plane->d >=
        0.0f) {
      side = 1;
    } else {
      side = 0;
    }
    if (node_count >= 0x80) {
      display_assert("plane_count<MAXIMUM_BSP3D_DEPTH",
                     "c:\\halo\\SOURCE\\render\\render_debug.c", 0x652, 1);
      system_exit(-1);
    }
    plane_stack[node_count] = node->plane;
    node_count = (short)(node_count + 1);
    len = crt_sprintf(cursor, "%5d %5d %c|n", node_index, node->plane,
                      side != 0 ? '+' : '-');
    cursor = cursor + len;
    child = node->children[(int)side];
    if (child < 0) {
      break;
    }
    node_index = child;
  }
  if (child == -1) {
    crt_sprintf(cursor, "solid");
  } else {
    crt_sprintf(cursor, " leaf %5d", child & LONG_MAX);
  }
  interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
  rasterizer_text_draw(0, 0, 0, 0, text);
  if (input_key_is_down(0x3e)) {
    file = crt_fopen("d:\\debug_bsp.txt", "w");
    if (file != 0) {
      crt_fprintf(file, "%d\n", (int)node_count);
      for (i = 0; i < node_count; i = i + 1) {
        crt_fprintf(file, "%d\n", plane_stack[i]);
      }
      crt_fclose(file);
    }
  }
}

void render_debug_input(void)
{
  short tab_stops[3];
  char buffer[512];

  if (debug_input != 0) {
    tab_stops[0] = 0xc8;
    tab_stops[1] = 0x190;
    tab_stops[2] = 0x226;
    draw_string_set_tab_stops(tab_stops, 3);
    input_get_raw_data_string(buffer, 0x1ff);
    interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
    rasterizer_text_draw(0, 0, 0, 0, buffer);
  }
}

void render_debug_structure_decals(void)
{
  collision_test_result collision_result;
  real_vector3d vector;
  real_euler_angles2d angles;
  int decal_definition_index;
  int angle;
  tag_block *clusters;
  tag_block *runtime_decals;
  structure_bsp_t *structure_bsp;
  int decal_index;
  structure_runtime_decal_t *decal;
  tag_reference *palette_entry;
  structure_leaf_t *leaf;
  structure_bsp_cluster_t *cluster;
  int cluster_index;
  const real_point3d *point;
  const real_argb_color *color;

  if (debug_permanent_decals == 0) {
    return;
  }
  structure_bsp = global_structure_bsp_get();
  runtime_decals = &structure_bsp->runtime_decals;
  if (runtime_decals->count <= 0) {
    return;
  }
  clusters = &structure_bsp->clusters;
  decal_index = 0;
  do {
    decal = tag_block_get_element(runtime_decals, decal_index,
                                  sizeof(structure_runtime_decal_t));
    palette_entry = tag_block_get_element(global_scenario_decal_palette,
                                          decal->palette_index,
                                          sizeof(tag_reference));
    decal_definition_index = palette_entry->tag_index;
    tag_get(TAG_GROUP_DECAL, decal_definition_index);
    if (scenario_leaf_index_from_point((int)&decal->position) == NONE) {
      cluster_index = NONE;
    } else {
      leaf = tag_block_get_element(
        global_structure_bsp_leaves,
        scenario_leaf_index_from_point((int)&decal->position) & LONG_MAX,
        sizeof(structure_leaf_t));
      cluster_index = leaf->cluster_index;
    }
    cluster = tag_block_get_element(clusters, cluster_index,
                                    sizeof(structure_bsp_cluster_t));
    angle = decal->yaw;
    angles.yaw = (float)angle * k_runtime_decal_yaw_scale;
    angle = decal->pitch;
    angles.pitch = (float)angle * k_runtime_decal_pitch_scale;
    angles_to_vector(&vector, &angles);
    if (collision_test_vector(0x61, &decal->position, &vector, NONE,
                              &collision_result)) {
      if (cluster->first_decal_index == NONE ||
          (int)cluster->decal_count <= decal_index - cluster->first_decal_index) {
        point = &collision_result.position;
        color = global_real_argb_orange;
      } else {
        point = &collision_result.position;
        color = global_real_argb_yellow;
      }
    } else {
      point = &decal->position;
      color = global_real_argb_red;
    }
    render_debug_sphere(1, point, 0.1f, color);
    render_debug_string_at_point(
      0, &decal->position,
      tag_name_strip_path(tag_get_name(decal_definition_index)),
      global_real_argb_green);
    decal_index = decal_index + 1;
  } while (decal_index < runtime_decals->count);
}

void render_debug_point2d(int flag, const real_plane3d *plane, int projection,
                          int sign, const real_point2d *point, float scale,
                          const real_argb_color *color, float offset)
{
  real_point3d position;
  float d;
  int axis;

  if (plane == 0) {
    display_assert("plane", "c:\\halo\\SOURCE\\render\\render_debug.c", 0xdb,
                   1);
    system_exit(-1);
  }
  if (point == 0) {
    display_assert("point", "c:\\halo\\SOURCE\\render\\render_debug.c", 0xdc,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0xdd,
                   1);
    system_exit(-1);
  }
  project_point2d(point, plane, projection, sign, &position);
  d = offset;
  if ((char)sign == 0) {
    d = -offset;
  }
  axis = (short)projection;
  point3d_component(position, axis) = d + point3d_component(position, axis);
  render_debug_point(flag, &position, scale, color);
}

void render_debug_line2d(int flag, const real_plane3d *plane, int projection,
                         int sign, const real_point2d *point_a,
                         const real_point2d *point_b,
                         const real_argb_color *color, float offset)
{
  real_point3d points[2];
  float d;
  int axis;

  if (plane == 0) {
    display_assert("plane", "c:\\halo\\SOURCE\\render\\render_debug.c", 0xf2,
                   1);
    system_exit(-1);
  }
  if (point_a == 0) {
    display_assert("p0", "c:\\halo\\SOURCE\\render\\render_debug.c", 0xf3, 1);
    system_exit(-1);
  }
  if (point_b == 0) {
    display_assert("p1", "c:\\halo\\SOURCE\\render\\render_debug.c", 0xf4, 1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0xf5,
                   1);
    system_exit(-1);
  }
  project_point2d(point_a, plane, projection, sign, &points[1]);
  project_point2d(point_b, plane, projection, sign, &points[0]);
  d = offset;
  if ((char)sign == 0) {
    d = -offset;
  }
  axis = (short)projection;
  point3d_component(points[1], axis) =
    d + point3d_component(points[1], axis);
  d = offset;
  if ((char)sign == 0) {
    d = -offset;
  }
  point3d_component(points[0], axis) =
    d + point3d_component(points[0], axis);
  render_debug_line(flag, &points[1], &points[0], color);
}

void render_debug_vector2d(int flag, const real_plane3d *plane, int projection,
                           int sign, const real_point2d *point,
                           const real_vector2d *vector, float scale,
                           const real_argb_color *color, float offset)
{
  real_point2d endpoint;

  if (plane == 0) {
    display_assert("plane", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x10d,
                   1);
    system_exit(-1);
  }
  if (point == 0) {
    display_assert("point", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x10e,
                   1);
    system_exit(-1);
  }
  if (vector == 0) {
    display_assert("vector", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x10f,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x110,
                   1);
    system_exit(-1);
  }
  endpoint.x = scale * vector->i + point->x;
  endpoint.y = scale * vector->j + point->y;
  render_debug_line2d(flag, plane, projection, sign, point, &endpoint, color,
                      offset);
}

void render_debug_circle(char flag, const real_plane3d *plane, int projection,
                         int sign, const real_point2d *center, float radius,
                         const real_argb_color *color, float offset)
{
  real_point2d circle[DEBUG_CIRCLE_SEGMENT_COUNT + 1];
  real_point2d points[2];
  const real_point2d *cp;
  short i;

  if (plane == 0) {
    display_assert("plane", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x122,
                   1);
    system_exit(-1);
  }
  if (center == 0) {
    display_assert("center", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x123,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x124,
                   1);
    system_exit(-1);
  }
  if (flag == 0) {
    render_debug_add_cache_entry(0, plane, (short)projection, (unsigned char)sign, center,
                 (double)radius, color);
    return;
  }
  build_circle_points(circle, radius);
  cp = &circle[1];
  i = DEBUG_CIRCLE_SEGMENT_COUNT;
  do {
    points[1].x = center->x + cp[-1].x;
    points[1].y = center->y + cp[-1].y;
    points[0].x = center->x + cp->x;
    points[0].y = center->y + cp->y;
    render_debug_line2d(1, plane, projection, sign, &points[1], &points[0],
                        color, offset);
    cp = cp + 1;
    i = (short)(i - 1);
  } while (i != 0);
}

void render_debug_vectors(int flag, const real_point3d *position,
                          const real_vector3d *forward,
                          const real_vector3d *up, float scale)
{
  real_matrix4x3 matrix;

  matrix4x3_from_forward_up_position(&matrix, position, forward, up);
  render_debug_matrix(flag, &matrix, scale);
}

void render_debug_quaternion(int flag, const real_point3d *position,
                             const real_quaternion *orientation, float scale)
{
  real_matrix4x3 matrix;

  matrix4x3_from_point_and_quaternion(&matrix, position, orientation);
  render_debug_matrix(flag, &matrix, scale);
}

void render_debug_box2d_outline(char flag, const real_rectangle2d *bounds,
                                const real_argb_color *color)
{
  real_point3d corners[4];

  if (bounds == 0) {
    display_assert("bounds", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x2e9,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x2ea,
                   1);
    system_exit(-1);
  }
  if (flag != 0) {
    corners[0].x = bounds->x0;
    corners[0].y = bounds->y0;
    corners[0].z = -1.0f;
    corners[1].x = bounds->x1;
    corners[1].y = bounds->y0;
    corners[1].z = -1.0f;
    corners[2].x = bounds->x1;
    corners[2].y = bounds->y1;
    corners[2].z = -1.0f;
    corners[3].x = bounds->x0;
    corners[3].y = bounds->y1;
    corners[3].z = -1.0f;
    matrix_transform_point(&global_frustum.view_to_world, &corners[0], &corners[0]);
    matrix_transform_point(&global_frustum.view_to_world, &corners[1], &corners[1]);
    matrix_transform_point(&global_frustum.view_to_world, &corners[2], &corners[2]);
    matrix_transform_point(&global_frustum.view_to_world, &corners[3], &corners[3]);
    render_debug_polygon_edges(corners, 4, color);
    return;
  }
  display_assert("can't add box2d to debug cache",
                 "c:\\halo\\SOURCE\\render\\render_debug.c", 0x2fd, 1);
  system_exit(-1);
}

void render_debug_box_outline(char wireframe, const real_rectangle3d *bounds,
                              const real_argb_color *color)
{
  real_point3d corners[8];
  const real_point3d *corner;
  int i;

  if (bounds == 0) {
    display_assert("bounds", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x32c,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x32d,
                   1);
    system_exit(-1);
  }

  if (wireframe != 0) {
    corners[0].x = bounds->x0;
    corners[0].y = bounds->y0;
    corners[0].z = bounds->z0;
    corners[1].x = bounds->x1;
    corners[1].y = bounds->y0;
    corners[1].z = bounds->z0;
    corners[2].x = bounds->x1;
    corners[2].y = bounds->y1;
    corners[2].z = bounds->z0;
    corners[3].x = bounds->x0;
    corners[3].y = bounds->y1;
    corners[3].z = bounds->z0;
    corners[4].x = bounds->x0;
    corners[4].y = bounds->y0;
    corners[4].z = bounds->z1;
    corners[5].x = bounds->x1;
    corners[5].y = bounds->y0;
    corners[5].z = bounds->z1;
    corners[6].x = bounds->x1;
    corners[6].y = bounds->y1;
    corners[6].z = bounds->z1;
    corners[7].x = bounds->x0;
    corners[7].y = bounds->y1;
    corners[7].z = bounds->z1;

    render_debug_polygon_edges(&corners[0], 4, color);
    render_debug_polygon_edges(&corners[4], 4, color);

    corner = &corners[0];
    i = 4;
    do {
      render_debug_line(1, corner, corner + 4, color);
      corner++;
      i--;
    } while (i != 0);
    return;
  }

  render_debug_add_cache_entry(7, bounds, color);
}

void render_debug(void)
{
  short i;
  debug_primitive *rec;

  ai_debug_render();
  ai_profile_render();
  render_debug_object_damage();
  render_debug_scripting();
  render_debug_trigger_volumes();
  texture_cache_debug_render();
  sound_cache_debug_render();
  render_debug_recording();
  render_debug_detail_objects();
  collision_debug_render();
  collision_log_render();
  render_debug_obstacle_path();
  render_debug_fog_planes();
  render_debug_decals();
  render_debug_camera();
  render_debug_player();
  render_debug_structure();
  render_debug_bsp();
  render_debug_input();
  render_debug_structure_decals();
  players_debug_render();

  for (i = 0; i < debug_primitive_count; i++) {
    rec = &debug_primitives[i];
    switch (rec->type) {
    case _debug_primitive_circle:
      render_debug_circle(1, &rec->data.circle.plane,
                          rec->data.circle.projection, rec->data.circle.sign,
                          &rec->data.circle.center,
                          rec->data.circle.radius, &rec->data.circle.color,
                          rec->data.circle.offset);
      break;
    case _debug_primitive_point:
      render_debug_point(1, &rec->data.point.position, rec->data.point.scale,
                         &rec->data.point.color);
      break;
    case _debug_primitive_line:
      render_debug_line(1, &rec->data.line.point0, &rec->data.line.point1,
                        &rec->data.line.color);
      break;
    case _debug_primitive_sphere:
      render_debug_sphere(1, &rec->data.sphere.center, rec->data.sphere.radius,
                          &rec->data.sphere.color);
      break;
    case _debug_primitive_cylinder:
      render_debug_cylinder(1, &rec->data.cylinder.base,
                            &rec->data.cylinder.height,
                            rec->data.cylinder.radius,
                            &rec->data.cylinder.color);
      break;
    case _debug_primitive_pill:
      render_debug_pill(1, &rec->data.cylinder.base, &rec->data.cylinder.height,
                        rec->data.cylinder.radius, &rec->data.cylinder.color);
      break;
    case _debug_primitive_box:
      render_debug_box(1, &rec->data.box.bounds, &rec->data.box.color);
      break;
    case _debug_primitive_box_outline:
      render_debug_box_outline(1, &rec->data.box.bounds, &rec->data.box.color);
      break;
    case _debug_primitive_string:
      render_debug_string(1, rec->data.string.string);
      break;
    case _debug_primitive_string_at_point:
      render_debug_string_at_point(1, &rec->data.string.position,
                                   rec->data.string.string,
                                   &rec->data.string.color);
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\render\\render_debug.c", 0x4f5, 1);
      system_exit(-1);
    }
  }

  if ((int)debug_primitive_frame != (int)(short)game_time_get() - 1) {
    debug_primitive_frame = (short)game_time_get();
    debug_primitive_count = 0;
    debug_string_pool_count = 0;
    debug_string_pool[0] = 0;
  }
}
