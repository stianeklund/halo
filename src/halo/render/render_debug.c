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

/* Build cylinder/capsule vertex rings for the debug cylinder and cone drawers
 * (0x188d00). Builds an orthonormal frame from height_vec at center
 * (FUN_00188c60, returning the height as len) and a unit circle of the given
 * radius (FUN_00188bf0), then fills the caller's vertex buffers:
 *   buffer1/buffer2 -- the bottom (z=0) and top (z=len) circle rings, 17 points
 *     each, transformed into world space by the frame.
 *   buffer3..buffer6 -- when all four are supplied (type 5 / dome), two half-
 *     rings of 9 points closing the top, built from opposite circle points.
 * Every vertex is transformed in place by the frame (matrix_transform_point).
 * The two ring buffers arrive in ECX/EDX and the center in EAX. */
void FUN_00188d00(float *buffer1, float *buffer2, float *center,
                  float *height_vec, float radius, float *buffer3,
                  float *buffer4, float *buffer5, float *buffer6)
{
  float frame[13];
  float circle[34];
  float len;
  float *b1;
  float *b2;
  float *cp;
  int i;
  int n;

  len = FUN_00188c60(frame, height_vec, center);
  FUN_00188bf0(circle, radius);
  if (buffer2 != 0 && buffer1 != 0) {
    b1 = buffer1;
    b2 = buffer2;
    cp = circle;
    n = 0x11;
    do {
      b2[0] = cp[0];
      b2[1] = cp[1];
      b2[2] = len;
      b1[0] = cp[0];
      b1[1] = cp[1];
      b1[2] = 0.0f;
      matrix_transform_point(frame, b2, b2);
      matrix_transform_point(frame, b1, b1);
      cp = cp + 2;
      b1 = b1 + 3;
      b2 = b2 + 3;
      n = n - 1;
    } while (n != 0);
  }
  if (buffer3 != 0 && buffer4 != 0 && buffer5 != 0 && buffer6 != 0) {
    for (i = 0; i < 9; i++) {
      buffer3[0] = 0.0f;
      buffer3[1] = circle[i * 2];
      buffer3[2] = len + circle[i * 2 + 1];
      buffer4[0] = 0.0f;
      buffer4[1] = circle[i * 2 + 16];
      buffer4[2] = circle[i * 2 + 17];
      buffer5[0] = circle[i * 2];
      buffer5[1] = 0.0f;
      buffer5[2] = len + circle[i * 2 + 1];
      buffer6[0] = circle[i * 2 + 16];
      buffer6[1] = 0.0f;
      buffer6[2] = circle[i * 2 + 17];
      matrix_transform_point(frame, buffer3, buffer3);
      matrix_transform_point(frame, buffer4, buffer4);
      matrix_transform_point(frame, buffer5, buffer5);
      matrix_transform_point(frame, buffer6, buffer6);
      buffer3 = buffer3 + 3;
      buffer4 = buffer4 + 3;
      buffer5 = buffer5 + 3;
      buffer6 = buffer6 + 3;
    }
  }
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

/* Draw a debug vector as a line from a point along a direction (0x189320).
 * The line runs from point to point + scale*vector; the cache flag is forwarded
 * to the line drawer. */
void FUN_00189320(int flag, float *point, float *vector, float scale,
                  void *color)
{
  float endpoint[3];

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
  endpoint[0] = scale * vector[0] + point[0];
  endpoint[1] = scale * vector[1] + point[1];
  endpoint[2] = scale * vector[2] + point[2];
  FUN_00189270(flag, point, endpoint, color);
}

/* Draw a debug tick as a line centered on a point (0x1893e0). Extends the line
 * from point + scale*dir to point - scale*dir; the cache flag is forwarded to
 * the line drawer. */
void FUN_001893e0(int flag, float *point, float *dir, float scale, void *color)
{
  float pts[6];

  pts[0] = scale * dir[0] + point[0];
  pts[1] = scale * dir[1] + point[1];
  pts[2] = scale * dir[2] + point[2];
  scale = -scale;
  pts[3] = scale * dir[0] + point[0];
  pts[4] = scale * dir[1] + point[1];
  pts[5] = scale * dir[2] + point[2];
  FUN_00189270(flag, pts, pts + 3, color);
}

/* Draw a debug line between two points, each offset by scale times a shared
 * vector (0x189450). The offset vector is *(float **)0x31fc44 (a camera basis
 * vector); the line runs from point_a + scale*V to point_b + scale*V. */
void FUN_00189450(int flag, float *point_a, float *point_b, void *color,
                  float scale)
{
  float pts[6];
  float *v;

  v = *(float **)0x31fc44;
  pts[0] = scale * v[0] + point_a[0];
  pts[1] = scale * v[1] + point_a[1];
  pts[2] = scale * v[2] + point_a[2];
  pts[3] = scale * v[0] + point_b[0];
  pts[4] = scale * v[1] + point_b[1];
  pts[5] = scale * v[2] + point_b[2];
  FUN_00189270(flag, pts, pts + 3, color);
}

/* Draw a debug coordinate frame as three colored axis lines (0x1894d0). The
 * frame (as built by FUN_00188c60) stores its length at matrix[0], the side/up/
 * forward basis vectors at matrix[1..3]/[4..6]/[7..9], and the origin at
 * matrix[10..12]. Draws each basis vector from the origin, scaled by
 * scale*matrix[0], in the standard axis colors. */
void FUN_001894d0(int flag, float *matrix, float scale)
{
  float *origin;

  origin = matrix + 10;
  FUN_00189320(flag, origin, matrix + 1, scale * matrix[0], *(void **)0x2ee6d0);
  FUN_00189320(flag, origin, matrix + 4, scale * matrix[0], *(void **)0x2ee6d4);
  FUN_00189320(flag, origin, matrix + 7, scale * matrix[0], *(void **)0x2ee6d8);
}

/* Draw or cache a debug sphere (0x189540). type 3. With flag clear, caches a
 * type-3 primitive (center, radius, color). With flag set, first culls
 * against the debug frustum (render_frustum_sphere_visible); if visible,
 * builds a 16-segment circle table (FUN_00188bf0) and draws three great
 * circles (in the XY, XZ and YZ planes) as line segments. Each iteration
 * walks two adjacent circle points (cp[-3..-2] current, cp[-1..0] next) and
 * emits one segment per plane into a contiguous six-float endpoint buffer. */
void FUN_00189540(char flag, void *center, float radius, void *color)
{
  float circle[34];
  float verts[6]; /* verts[3..5] = endpoint A, verts[0..2] = endpoint B */
  float *c;
  float *cp;
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
    FUN_00188ec0(3, center, (double)radius, color);
    return;
  }
  vis = (short)render_frustum_sphere_visible((void *)0x5065a4, center, radius);
  if (vis == 0) {
    return;
  }
  FUN_00188bf0(circle, radius);
  c = center;
  cp = circle + 3;
  i = 0x10;
  do {
    /* XY-plane great circle */
    verts[3] = c[0] + cp[-3];
    verts[5] = c[2];
    verts[2] = c[2];
    verts[4] = c[1] + cp[-2];
    verts[0] = c[0] + cp[-1];
    verts[1] = c[1] + cp[0];
    FUN_0017eb10(&verts[3], &verts[0], (int)color);
    /* XZ-plane great circle */
    verts[3] = c[0] + cp[-2];
    verts[4] = c[1];
    verts[5] = c[2] + cp[-3];
    verts[1] = c[1];
    verts[0] = c[0] + cp[0];
    verts[2] = c[2] + cp[-1];
    FUN_0017eb10(&verts[3], &verts[0], (int)color);
    /* YZ-plane great circle */
    verts[4] = c[1] + cp[-3];
    verts[3] = c[0];
    verts[5] = c[2] + cp[-2];
    verts[0] = c[0];
    verts[1] = c[1] + cp[-1];
    verts[2] = c[2] + cp[0];
    FUN_0017eb10(&verts[3], &verts[0], (int)color);
    cp = cp + 2;
    i = (short)(i - 1);
  } while (i != 0);
}

/* Draw or cache a debug cylinder (0x1896d0). type 4. With flag clear, caches a
 * type-4 primitive (base, height vector, radius, color). With flag set, builds
 * the two circle rings via the cylinder builder (FUN_00188d00) into a pair of
 * 17-vertex buffers, then draws the wireframe: each ring as a 16-segment strip,
 * four vertical edges connecting the rings, and two diameter cross-lines per
 * ring. */
void FUN_001896d0(char flag, void *center, void *height_vec, float radius,
                  void *color)
{
  float buffers[102]; /* buf1 = [0..50], buf2 = [51..101]; 17 vertices each */
  int i;

  if (center == 0) {
    display_assert("base", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x285,
                   1);
    system_exit(-1);
  }
  if (height_vec == 0) {
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
    FUN_00188ec0(4, center, height_vec, (double)radius, color);
    return;
  }
  FUN_00188d00(buffers, buffers + 51, center, height_vec, radius, 0, 0, 0, 0);
  for (i = 0; i < 16; i++) {
    FUN_0017eb10(&buffers[i * 3], &buffers[i * 3 + 3], (int)color);
    FUN_0017eb10(&buffers[i * 3 + 51], &buffers[i * 3 + 54], (int)color);
  }
  for (i = 0; i < 4; i++) {
    FUN_0017eb10(&buffers[i * 12], &buffers[i * 12 + 51], (int)color);
  }
  for (i = 0; i < 2; i++) {
    FUN_0017eb10(&buffers[i * 12], &buffers[i * 12 + 24], (int)color);
    FUN_0017eb10(&buffers[i * 12 + 51], &buffers[i * 12 + 75], (int)color);
  }
}

/* Draw or cache a debug capsule/dome (0x189860). type 5. With flag clear,
 * caches a type-5 primitive. With flag set, calls the cylinder builder
 * (FUN_00188d00) to fill two 17-vertex main rings plus four 9-vertex dome
 * half-rings, then draws: each main ring as a 16-segment strip, four vertical
 * edges connecting the main rings, and the four dome half-rings as 8-segment
 * strips. */
void FUN_00189860(char flag, void *center, void *height_vec, float radius,
                  void *color)
{
  float buf1[51];
  float buf2[51];
  float buf3[27];
  float buf4[27];
  float buf5[27];
  float buf6[27];
  int i;

  if (center == 0) {
    display_assert("base", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x2b5,
                   1);
    system_exit(-1);
  }
  if (height_vec == 0) {
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
    FUN_00188ec0(5, center, height_vec, (double)radius, color);
    return;
  }
  FUN_00188d00(buf1, buf2, center, height_vec, radius, buf3, buf4, buf5, buf6);
  for (i = 0; i < 16; i++) {
    FUN_0017eb10(&buf2[i * 3], &buf2[i * 3 + 3], (int)color);
    FUN_0017eb10(&buf1[i * 3], &buf1[i * 3 + 3], (int)color);
  }
  for (i = 0; i < 4; i++) {
    FUN_0017eb10(&buf2[i * 12], &buf1[i * 12], (int)color);
  }
  for (i = 0; i < 8; i++) {
    FUN_0017eb10(&buf3[i * 3], &buf3[i * 3 + 3], (int)color);
    FUN_0017eb10(&buf4[i * 3], &buf4[i * 3 + 3], (int)color);
    FUN_0017eb10(&buf5[i * 3], &buf5[i * 3 + 3], (int)color);
    FUN_0017eb10(&buf6[i * 3], &buf6[i * 3 + 3], (int)color);
  }
}

/* Draw or cache a debug box (0x189a20). type 6. With flag clear, caches a
 * type-6 primitive (six bounds floats + color). With flag set, expands the
 * bounds {x0,x1,y0,y1,z0,z1} into the eight box corners and draws the six faces
 * as quads (FUN_00188970). Corners are held in one contiguous 24-float buffer.
 */
void FUN_00189a20(char flag, float *bounds, void *color)
{
  float corners[24];

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
    FUN_00188ec0(6, bounds, color);
    return;
  }
  corners[0] = bounds[0]; /* c0 (x0,y0,z0) */
  corners[1] = bounds[2];
  corners[2] = bounds[4];
  corners[3] = bounds[1]; /* c1 (x1,y0,z0) */
  corners[4] = bounds[2];
  corners[5] = bounds[4];
  corners[6] = bounds[0]; /* c2 (x0,y0,z1) */
  corners[7] = bounds[2];
  corners[8] = bounds[5];
  corners[9] = bounds[1]; /* c3 (x1,y0,z1) */
  corners[10] = bounds[2];
  corners[11] = bounds[5];
  corners[12] = bounds[0]; /* c4 (x0,y1,z1) */
  corners[13] = bounds[3];
  corners[14] = bounds[5];
  corners[15] = bounds[1]; /* c5 (x1,y1,z1) */
  corners[16] = bounds[3];
  corners[17] = bounds[5];
  corners[18] = bounds[0]; /* c6 (x0,y1,z0) */
  corners[19] = bounds[3];
  corners[20] = bounds[4];
  corners[21] = bounds[1]; /* c7 (x1,y1,z0) */
  corners[22] = bounds[3];
  corners[23] = bounds[4];
  FUN_00188970(1, &corners[0], &corners[6], &corners[12], &corners[18], color);
  FUN_00188970(1, &corners[3], &corners[9], &corners[15], &corners[21], color);
  FUN_00188970(1, &corners[0], &corners[3], &corners[9], &corners[6], color);
  FUN_00188970(1, &corners[12], &corners[15], &corners[21], &corners[18],
               color);
  FUN_00188970(1, &corners[0], &corners[3], &corners[21], &corners[18], color);
  FUN_00188970(1, &corners[6], &corners[9], &corners[15], &corners[12], color);
}

/* Draw a closed debug polyline (0x189ba0). Immediate-mode only: with three or
 * more points draws the closing edge from the last point back to the first,
 * then a line between each pair of consecutive points, forming a line loop.
 * Each point is three floats. */
void FUN_00189ba0(float *points, short count, void *color)
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
    FUN_00189270(1, points + (count - 1) * 3, points, color);
    if (1 < count) {
      points = points + 3;
      n = (unsigned short)(count - 1);
      do {
        FUN_00189270(1, points - 3, points, color);
        points = points + 3;
        n = n - 1;
      } while (n != 0);
    }
  }
}

/* Draw or cache a debug string (0x189c40). type 8. With flag set, prime the
 * debug text state (font 1, style -1, color tag 5) and draw the string
 * immediately; otherwise submit a type-8 primitive (the string, interned by
 * the cache writer) to the per-frame cache. */
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

/* Draw the collision-BSP portal edges (0x18a110). A debug sub-renderer gated on
 * the flag at 0x506533. Iterates the portal block of the global collision BSP
 * (bsp+0x48); for each portal, looks up its two vertices in the vertex block
 * (bsp+0x54) by the indices stored in the portal element, and draws a line
 * between them. */
void FUN_0018a110(void)
{
  void *bsp;
  int *portal_block;
  void *vertex_block;
  int i;
  int *portal;
  void *v0;
  void *v1;

  if (*(char *)0x506533 != 0) {
    bsp = global_collision_bsp_get();
    portal_block = (int *)((char *)bsp + 0x48);
    i = 0;
    if (0 < *portal_block) {
      vertex_block = (char *)bsp + 0x54;
      do {
        portal = (int *)tag_block_get_element(portal_block, i, 0x18);
        v0 = tag_block_get_element(vertex_block, portal[0], 0x10);
        v1 = tag_block_get_element(vertex_block, portal[1], 0x10);
        FUN_00189270(1, v0, v1, *(void **)0x2ee6d4);
        i++;
      } while (i < *portal_block);
    }
  }
}

/* Draw the raw controller-input debug overlay (0x18a370). Gated on the flag at
 * 0x506531. Sets three text tab stops (200, 400, 550), fetches the raw input
 * data string, primes the debug text state, and draws the string. */
void FUN_0018a370(void)
{
  short tab_stops[3];
  char buffer[512];

  if (*(char *)0x506531 != 0) {
    tab_stops[0] = 0xc8;
    tab_stops[1] = 0x190;
    tab_stops[2] = 0x226;
    draw_string_set_tab_stops(tab_stops, 3);
    input_get_raw_data_string(buffer, 0x1ff);
    interface_draw_text(1, -1, 0, 0, 5, 0);
    rasterizer_text_draw(0, 0, 0, 0, buffer);
  }
}

/* Draw a debug point on a plane (0x18a580). Projects a 2D point onto the plane,
 * lifts it off the plane along the projection axis by +/- offset (sign selects
 * the direction), then draws it as a debug point (FUN_00189150). */
void FUN_0018a580(int flag, float *plane, int projection, int sign,
                  float *point, float scale, void *color, float offset)
{
  float pos[3];
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
  project_point2d(point, plane, projection, sign, pos);
  d = offset;
  if ((char)sign == 0) {
    d = -offset;
  }
  axis = (short)projection;
  pos[axis] = d + pos[axis];
  FUN_00189150(flag, pos, scale, color);
}

/* Draw one edge of a debug plane as a 3D line segment (0x18a650). Projects
 * two 2D endpoints (point_a, point_b) onto the plane, lifts each off the
 * plane along the projection axis by +/- offset (sign selects the direction),
 * then draws the connecting line. The two projected points share a contiguous
 * six-float buffer: pts[3..5] is endpoint A, pts[0..2] is endpoint B. */
void FUN_0018a650(int flag, float *plane, int projection, int sign,
                  float *point_a, float *point_b, void *color, float offset)
{
  float pts[6];
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
  project_point2d(point_a, plane, projection, sign, pts + 3);
  project_point2d(point_b, plane, projection, sign, pts);
  d = offset;
  if ((char)sign == 0) {
    d = -offset;
  }
  axis = (short)projection;
  pts[axis + 3] = d + pts[axis + 3];
  d = offset;
  if ((char)sign == 0) {
    d = -offset;
  }
  pts[axis] = d + pts[axis];
  FUN_00189270(flag, pts + 3, pts, color);
}

/* Draw a debug vector on a plane (0x18a770). Computes the 2D endpoint
 * point + scale*vector, then draws a plane-edge line from point to that
 * endpoint (FUN_0018a650, which projects both onto the plane). */
void FUN_0018a770(int flag, float *plane, int projection, int sign,
                  float *point, float *vector, float scale, void *color,
                  float offset)
{
  float endpoint[2];

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
  endpoint[0] = scale * vector[0] + point[0];
  endpoint[1] = scale * vector[1] + point[1];
  FUN_0018a650(flag, plane, projection, sign, point, endpoint, color, offset);
}

/* Draw or cache a debug plane disc (0x18a860). type 0. With flag clear, caches
 * a type-0 primitive. With flag set, builds a 16-segment circle table
 * (FUN_00188bf0) and, for each segment, offsets two adjacent circle points by
 * the center, then hands them to the plane-edge drawer (FUN_0018a650) which
 * projects them onto the plane and draws the connecting 3D line. */
void FUN_0018a860(char flag, float *plane, int projection, int sign,
                  float *center, float radius, void *color, float offset)
{
  float circle[34];
  float pts[4]; /* pts[2..3] = current circle point, pts[0..1] = next */
  float *cp;
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
    FUN_00188ec0(0, plane, (short)projection, (unsigned char)sign, center,
                 (double)radius, color);
    return;
  }
  FUN_00188bf0(circle, radius);
  cp = circle + 3;
  i = 0x10;
  do {
    pts[2] = center[0] + cp[-3];
    pts[3] = center[1] + cp[-2];
    pts[0] = center[0] + cp[-1];
    pts[1] = center[1] + cp[0];
    FUN_0018a650(1, plane, projection, sign, &pts[2], &pts[0], color, offset);
    cp = cp + 2;
    i = (short)(i - 1);
  } while (i != 0);
}

/* Draw a debug coordinate frame built from a forward/up basis (0x18a990).
 * Builds a 4x3 frame at position from the forward and up vectors, then draws
 * its axes. */
void FUN_0018a990(int flag, float *position, float *forward, float *up,
                  float scale)
{
  float matrix[13];

  matrix4x3_from_forward_up_position(matrix, position, forward, up);
  FUN_001894d0(flag, matrix, scale);
}

/* Draw a debug coordinate frame built from a surface normal (0x18a9d0).
 * Derives an orthonormal basis at position from the normal, then draws its
 * axes. */
void FUN_0018a9d0(int flag, float *position, float *basis_data, float scale)
{
  float matrix[13];

  component_vectors_from_normal3d(matrix, position, basis_data);
  FUN_001894d0(flag, matrix, scale);
}

/* Draw a 2D debug box in screen space (0x18aa00). Immediate-mode only (there
 * is no cache path -- flag == 0 asserts). Expands the {x0,x1,y0,y1} bounds
 * into four corners at z = -1, transforms each by the debug screen matrix at
 * 0x5065e8, then draws them as a closed polyline. */
void FUN_0018aa00(char flag, float *bounds, void *color)
{
  float corners[12];

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
    corners[0] = bounds[0];
    corners[1] = bounds[2];
    corners[2] = -1.0f;
    corners[3] = bounds[1];
    corners[4] = bounds[2];
    corners[5] = -1.0f;
    corners[6] = bounds[1];
    corners[7] = bounds[3];
    corners[8] = -1.0f;
    corners[9] = bounds[0];
    corners[10] = bounds[3];
    corners[11] = -1.0f;
    matrix_transform_point((float *)0x5065e8, corners, corners);
    matrix_transform_point((float *)0x5065e8, corners + 3, corners + 3);
    matrix_transform_point((float *)0x5065e8, corners + 6, corners + 6);
    matrix_transform_point((float *)0x5065e8, corners + 9, corners + 9);
    FUN_00189ba0(corners, 4, color);
    return;
  }
  display_assert("can't add box2d to debug cache",
                 "c:\\halo\\SOURCE\\render\\render_debug.c", 0x2fd, 1);
  system_exit(-1);
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
      FUN_0018a860(1, &rec->f04, rec->s14, rec->b16, &rec->f18, rec->f20,
                   &rec->f24, rec->f34);
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
      FUN_001896d0(1, &rec->f04, &rec->f10, rec->f1c, &rec->f20);
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
