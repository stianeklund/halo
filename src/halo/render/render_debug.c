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

#include "x87_math.h"

/* Per-frame debug primitive cache record (0x38 bytes, array based at 0x4d1220).
 * The payload is a tagged union keyed by `type`; individual offsets are reused
 * per primitive kind, so the fields carry raw-offset names. */
typedef struct debug_primitive {
  short type; /* +0x00 primitive type (0..9)          */
  short pad02; /* +0x02                                */
  float f04; /* +0x04                                */
  float f08; /* +0x08                                */
  float f0c; /* +0x0c                                */
  float f10; /* +0x10                                */
  unsigned short s14; /* +0x14                                */
  unsigned char b16; /* +0x16                                */
  unsigned char pad17; /* +0x17                                */
  float f18; /* +0x18                                */
  float f1c; /* +0x1c                                */
  float f20; /* +0x20                                */
  float f24; /* +0x24                                */
  float f28; /* +0x28                                */
  float f2c; /* +0x2c                                */
  float f30; /* +0x30                                */
  float f34; /* +0x34                                */
} debug_primitive; /* sizeof == 0x38 */

typedef char
  debug_primitive_size_check[sizeof(debug_primitive) == 0x38 ? 1 : -1];

#define debug_primitives ((debug_primitive *)0x4d1220)
#define debug_primitive_count (*(short *)0x4d8224)
#define debug_primitive_frame (*(short *)0x4d8220)

/* Per-frame debug string arena: char[0x400] at 0x4d0e20, ending at 0x4d121f
 * (immediately before the primitive cache at 0x4d1220). Text primitives intern
 * their string here; the cursor at 0x4d8228 counts bytes used (max 0x3ff). */
#define debug_string_pool ((char *)0x4d0e20)
#define debug_string_pool_count (*(short *)0x4d8228)
#define debug_string_overflow_warned (*(char *)0x4d822b)

/* Draw a debug triangle immediately (0x188890). Immediate-mode only: asserts
 * the caller passed flag != 0 and three non-null vertex pointers plus a color,
 * then hands them to the sprite-triangle rasterizer (0x17eb30). Unlike the
 * cached point and line drawers there is no flag == 0 branch -- callers reach
 * this only on their immediate render path. */
void FUN_00188890(char flag, float *point0, float *point1, float *point2,
                  void *color)
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
  FUN_0017eb30(point0, point1, point2, color);
}

/* Draw a debug quad immediately as two triangles (0x188970). Splits the quad
 * (point0..point3) into triangles (0,1,2) and (0,2,3), each drawn via the
 * immediate triangle drawer. Immediate-mode only (asserts flag != 0). */
void FUN_00188970(char flag, float *point0, float *point1, float *point2,
                  float *point3, void *color)
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
  FUN_00188890(flag, point0, point1, point2, color);
  FUN_00188890(flag, point0, point2, point3, color);
}

/* Draw a filled debug polygon immediately as a triangle fan (0x188a90). Fans
 * from points[0]: for each i in 1..count-2 draws triangle
 * (points[0], points[i], points[i+1]). Each point is three floats.
 * Immediate-mode only. */
void FUN_00188a90(float *points, short count, void *color)
{
  short i;
  float *pi;

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
      pi = points + i * 3;
      FUN_00188890(1, points, pi, pi + 3, color);
      i = (short)(i + 1);
    } while (i < count - 1);
  }
}

/* Intern a string into the per-frame debug string arena (0x188b20). Scans the
 * arena for an identical string already present and returns its address;
 * otherwise appends a null-terminated copy and advances the cursor. Returns
 * NULL (with a one-shot overflow warning) when the arena is full. The source
 * string is passed in EDI. */
char *FUN_00188b20(const char *str)
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

void FUN_00188bf0(float *table, float radius)
{
  double sn;
  double cs;
  short i;
  int idx;

  sn = x87_fsin_d(debug_circle_angle);
  table[0] = radius;
  table[1] = 0.0f;
  idx = 0;
  cs = x87_fcos_d(debug_circle_angle);
  i = 0;
  do {
    i = (short)(i + 1);
    table[idx * 2 + 2] = (float)(cs * table[idx * 2] - sn * table[idx * 2 + 1]);
    table[idx * 2 + 3] = (float)(sn * table[idx * 2] + cs * table[idx * 2 + 1]);
    idx = (int)i;
  } while (idx + 1 < 0x10);
  table[0x20] = table[0];
  table[0x21] = table[1];
}

/* Build a debug coordinate frame from a direction vector (0x188c60). Given a
 * forward direction (EAX) and an origin position, fills a 13-float frame:
 *   frame[0]      = 1.0
 *   frame[1..3]   = side    = cross(up, forward)
 *   frame[4..6]   = up      = perpendicular3d(forward), normalized
 *   frame[7..9]   = forward = the input direction, normalized
 *   frame[10..12] = position
 * Returns the length of the input direction before normalization. The frame
 * pointer arrives in ECX and the direction in EAX. */
float FUN_00188c60(float *frame, float *in_vec, float *position)
{
  float *fwd;
  float *up;
  float length;

  frame[0] = 1.0f;
  fwd = frame + 7;
  fwd[0] = in_vec[0];
  fwd[1] = in_vec[1];
  fwd[2] = in_vec[2];
  up = frame + 4;
  perpendicular3d(fwd, up);
  length = normalize3d(fwd);
  normalize3d(up);
  frame[1] = up[1] * fwd[2] - fwd[1] * up[2];
  frame[2] = fwd[0] * up[2] - up[0] * fwd[2];
  frame[3] = up[0] * fwd[1] - fwd[0] * up[1];
  frame[10] = position[0];
  frame[11] = position[1];
  frame[12] = position[2];
  return length;
}

/* Cache overflow one-shot warning flag (0x4d822a). */
#define cache_overflow_warned (*(char *)0x4d822a)

/* Submit a debug primitive to the per-frame cache (0x188ec0). Variadic tagged
 * builder: `type` (0..9) selects how the trailing arguments are interpreted and
 * packed into a fresh 0x38-byte cache record. Float arguments arrive promoted
 * to double (varargs default promotion) and are narrowed back to float on
 * store. When the game frame advances the cache is reset; when it fills (0x200
 * records) a one-shot overflow warning is emitted and the record is dropped.
 *
 * The trailing arguments are read positionally through `args` (the stack slot
 * just past `type`), matching the original's fixed [ebp+N] accesses. Several
 * layouts share the two trailing copy tails: tail_c5/tail_d8 copy a 3-vector
 * then a 4-vector (shared blocks 0x1890c5/0x1890d8), tail_v4 copies a single
 * 4-vector (shared block 0x189060). */
void FUN_00188ec0(short type, ...)
{
  int *args;
  char *rec;
  float *p1;
  float *p2;
  float *q;
  float *dst3;
  float *dst6;
  float *src6;
  float *dst;
  float *src;
  float tmp;
  char *interned;
  short frame;
  int i;

  frame = (short)game_time_get();
  if (debug_primitive_frame != frame) {
    debug_primitive_frame = (short)game_time_get();
    debug_primitive_count = 0;
    debug_string_pool_count = 0;
    debug_string_pool[0] = 0;
  } else if (debug_primitive_count >= 0x200) {
    if (cache_overflow_warned == 0) {
      error(2, "render debug cache overflow.");
      cache_overflow_warned = 1;
    }
    return;
  }

  rec = (char *)&debug_primitives[debug_primitive_count];
  debug_primitive_count = debug_primitive_count + 1;
  *(short *)rec = type;

  args = (int *)((char *)&type + 4);
  switch (type) {
  case 0:
    p1 = (float *)args[0];
    q = (float *)(rec + 0x04);
    q[0] = p1[0];
    q[1] = p1[1];
    q[2] = p1[2];
    q[3] = p1[3];
    *(short *)(rec + 0x14) = *(short *)&args[1];
    *(char *)(rec + 0x16) = *(char *)&args[2];
    p2 = (float *)args[3];
    *(float *)(rec + 0x18) = p2[0];
    *(float *)(rec + 0x1c) = p2[1];
    *(float *)(rec + 0x20) = (float)*(double *)&args[4];
    p1 = (float *)args[6];
    q = (float *)(rec + 0x24);
    q[0] = p1[0];
    q[1] = p1[1];
    q[2] = p1[2];
    q[3] = p1[3];
    *(float *)(rec + 0x34) = (float)*(double *)&args[7];
    goto done;
  case 1:
  case 3:
    p1 = (float *)args[0];
    q = (float *)(rec + 0x04);
    q[0] = p1[0];
    q[1] = p1[1];
    q[2] = p1[2];
    *(float *)(rec + 0x10) = (float)*(double *)&args[1];
    p2 = (float *)args[3];
    q = (float *)(rec + 0x14);
    q[0] = p2[0];
    q[1] = p2[1];
    q[2] = p2[2];
    q[3] = p2[3];
    goto done;
  case 2:
    p1 = (float *)args[0];
    q = (float *)(rec + 0x04);
    q[0] = p1[0];
    q[1] = p1[1];
    q[2] = p1[2];
    dst3 = (float *)(rec + 0x10);
    dst6 = (float *)(rec + 0x1c);
    goto tail_c5;
  case 4:
    p1 = (float *)args[0];
    q = (float *)(rec + 0x04);
    q[0] = p1[0];
    q[1] = p1[1];
    q[2] = p1[2];
    p2 = (float *)args[1];
    dst3 = (float *)(rec + 0x10);
    dst3[0] = p2[0];
    dst3[1] = p2[1];
    tmp = p2[2];
    *(float *)(rec + 0x1c) = (float)*(double *)&args[2];
    dst6 = (float *)(rec + 0x20);
    src6 = (float *)args[4];
    goto tail_d8;
  case 5:
    p1 = (float *)args[0];
    q = (float *)(rec + 0x04);
    q[0] = p1[0];
    q[1] = p1[1];
    q[2] = p1[2];
    p2 = (float *)args[1];
    q = (float *)(rec + 0x10);
    q[0] = p2[0];
    q[1] = p2[1];
    *(float *)(rec + 0x1c) = (float)*(double *)&args[2];
    q[2] = p2[2];
    dst = (float *)(rec + 0x20);
    src = (float *)args[4];
    goto tail_v4;
  case 6:
  case 7:
    p1 = (float *)args[0];
    dst = (float *)(rec + 0x04);
    for (i = 6; i != 0; i--) {
      *dst = *p1;
      p1++;
      dst++;
    }
    dst = (float *)(rec + 0x1c);
    src = (float *)args[1];
    goto tail_v4;
  case 8:
    interned = FUN_00188b20((char *)args[0]);
    if (interned != 0) {
      *(char **)(rec + 0x04) = interned;
    } else {
      debug_primitive_count = debug_primitive_count - 1;
    }
    goto done;
  case 9:
    interned = FUN_00188b20((char *)args[0]);
    if (interned == 0) {
      debug_primitive_count = debug_primitive_count - 1;
      goto done;
    }
    *(char **)(rec + 0x04) = interned;
    dst3 = (float *)(rec + 0x08);
    dst6 = (float *)(rec + 0x14);
    goto tail_c5;
  default:
    goto done;
  }

tail_c5:
  src = (float *)args[1];
  dst3[0] = src[0];
  dst3[1] = src[1];
  tmp = src[2];
  src6 = (float *)args[2];
tail_d8:
  dst3[2] = tmp;
  dst6[0] = src6[0];
  dst6[1] = src6[1];
  dst6[2] = src6[2];
  dst6[3] = src6[3];
  goto done;
tail_v4:
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
done:
  return;
}

/* Draw or cache a debug point marker (0x189150). type 1. With flag set, render
 * a 3D crosshair: three axis-aligned line segments through `position`, each
 * 2*s long where s = scale * the global debug marker size (0x253398). The
 * 18-float vertex buffer is a single contiguous block walked in 6-float
 * (two-endpoint) strides by the line helper. With flag clear, submit a type-1
 * primitive (position, scale, color) to the per-frame cache. */
void FUN_00189150(char flag, float *position, float scale, void *color)
{
  float v[18];
  float *p;
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
    s = scale * *(float *)0x253398;
    v[0] = position[0] - s;
    v[1] = position[1];
    v[2] = position[2];
    v[3] = position[0] + s;
    v[4] = position[1];
    v[5] = position[2];
    v[6] = position[0];
    v[7] = position[1] - s;
    v[8] = position[2];
    v[9] = position[0];
    v[10] = position[1] + s;
    v[11] = position[2];
    v[12] = position[0];
    v[13] = position[1];
    v[14] = position[2] - s;
    v[15] = position[0];
    v[16] = position[1];
    v[17] = position[2] + s;
    p = &v[0];
    i = 3;
    do {
      FUN_0017eb10(p, p + 3, (int)color);
      p = p + 6;
      i--;
    } while (i != 0);
    return;
  }

  FUN_00188ec0(1, position, (double)scale, color);
}

/* Draw or cache a debug line (0x189270). type 2. With flag set, render the line
 * segment point_a->point_b immediately; otherwise submit a type-2 primitive
 * (point_a, point_b, color) to the per-frame cache. */
void FUN_00189270(char flag, float *point_a, float *point_b, void *color)
{
  if (point_a == 0) {
    display_assert("point0", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x16b,
                   1);
    system_exit(-1);
  }
  if (point_b == 0) {
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
    FUN_0017eb10(point_a, point_b, (int)color);
    return;
  }

  FUN_00188ec0(2, point_a, point_b, color);
}

/* Draw or cache a debug string (0x189c40). type 8. With flag set, prime the
 * debug text state (font 1, style -1, color tag 5) and draw the string
 * immediately; otherwise submit a type-8 primitive (the string, interned by the
 * cache writer) to the per-frame cache. */
void FUN_00189c40(char flag, const char *string)
{
  if (string == 0) {
    display_assert("string", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x37d,
                   1);
    system_exit(-1);
  }

  if (flag != 0) {
    interface_draw_text(1, -1, 0, 0, 5, 0);
    rasterizer_text_draw(0, 0, 0, 0, string);
    return;
  }

  FUN_00188ec0(8, string);
}

/* Render a debug bounding box (0x18ab30). With wireframe set, expand the six
 * min/max bounds {x0,x1,y0,y1,z0,z1} into the eight box corners and draw the
 * two z-faces as line loops plus the four vertical edges; otherwise submit a
 * solid box primitive (type 7) to the cache. */
void FUN_0018ab30(char wireframe, float *bounds, void *color)
{
  float v[24];
  float *p;
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
    /* Eight corners, three floats each, grouped as two z-faces of four. */
    v[0] = bounds[0];
    v[1] = bounds[2];
    v[2] = bounds[4]; /* x0 y0 z0 */
    v[3] = bounds[1];
    v[4] = bounds[2];
    v[5] = bounds[4]; /* x1 y0 z0 */
    v[6] = bounds[1];
    v[7] = bounds[3];
    v[8] = bounds[4]; /* x1 y1 z0 */
    v[9] = bounds[0];
    v[10] = bounds[3];
    v[11] = bounds[4]; /* x0 y1 z0 */
    v[12] = bounds[0];
    v[13] = bounds[2];
    v[14] = bounds[5]; /* x0 y0 z1 */
    v[15] = bounds[1];
    v[16] = bounds[2];
    v[17] = bounds[5]; /* x1 y0 z1 */
    v[18] = bounds[1];
    v[19] = bounds[3];
    v[20] = bounds[5]; /* x1 y1 z1 */
    v[21] = bounds[0];
    v[22] = bounds[3];
    v[23] = bounds[5]; /* x0 y1 z1 */

    FUN_00189ba0(&v[0], 4, color); /* bottom z-face line loop */
    FUN_00189ba0(&v[12], 4, color); /* top z-face line loop    */

    p = &v[0];
    i = 4;
    do {
      FUN_00189270(1, p, p + 12, color); /* vertical edge */
      p += 3;
      i--;
    } while (i != 0);
    return;
  }

  FUN_00188ec0(7, bounds, color);
}

/* Flush the per-frame debug primitive cache (0x18ac50). Run the fixed debug
 * sub-renderers, dispatch every cached primitive to its draw routine, then
 * reset the cache once the game frame advances past the cached frame. */
void FUN_0018ac50(void)
{
  short i;
  debug_primitive *rec;

  FUN_000534d0();
  FUN_00053da0();
  render_debug_object_damage();
  render_debug_scripting();
  render_debug_trigger_volumes();
  texture_cache_debug_render();
  FUN_001be7b0();
  render_debug_recording();
  FUN_00194070();
  FUN_00149ce0();
  collision_log_render();
  FUN_00061ca0();
  render_debug_fog_planes();
  FUN_00099070();
  FUN_00189de0();
  FUN_0018a000();
  FUN_0018a110();
  FUN_0018a190();
  FUN_0018a370();
  FUN_0018a3e0();
  players_debug_render();

  for (i = 0; i < debug_primitive_count; i++) {
    rec = &debug_primitives[i];
    switch (rec->type) {
    case 0:
      FUN_0018a860(1, &rec->f04, rec->s14, rec->b16, &rec->f18,
                   *(int *)&rec->f20, &rec->f24, *(int *)&rec->f34);
      break;
    case 1:
      FUN_00189150(1, &rec->f04, rec->f10, &rec->s14);
      break;
    case 2:
      FUN_00189270(1, &rec->f04, &rec->f10, &rec->f1c);
      break;
    case 3:
      FUN_00189540(1, &rec->f04, rec->f10, &rec->s14);
      break;
    case 4:
      FUN_001896d0(1, &rec->f04, &rec->f10, *(int *)&rec->f1c, &rec->f20);
      break;
    case 5:
      FUN_00189860(1, &rec->f04, &rec->f10, rec->f1c, &rec->f20);
      break;
    case 6:
      FUN_00189a20(1, &rec->f04, &rec->f1c);
      break;
    case 7:
      FUN_0018ab30(1, &rec->f04, &rec->f1c);
      break;
    case 8:
      FUN_00189c40(1, *(const char **)&rec->f04);
      break;
    case 9:
      FUN_00189cb0(1, &rec->f08, *(void **)&rec->f04, (int)&rec->s14);
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\render\\render_debug.c", 0x4f5, 1);
      system_exit(-1);
    }
  }

  if ((int)debug_primitive_frame != (int)(short)game_time_get() - 1) {
    debug_primitive_frame = (short)game_time_get();
    debug_primitive_count = 0;
    *(short *)0x4d8228 = 0;
    *(char *)0x4d0e20 = 0;
  }
}
