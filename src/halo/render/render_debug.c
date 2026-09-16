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

/* Payload copy shapes used by the cache writer (0x188ec0). The reference codes
 * every 3- and 4-float payload store as a whole-aggregate assignment -- it
 * materializes the destination with `lea <off>(<rec>), <reg>` and then stores
 * at 0/4/8(/0xc) of that register. Element-wise float stores instead compile to
 * direct-displacement stores off the record base, so the payload copies are
 * written as aggregate assignments here. 1- and 2-dword payloads stay scalar;
 * the reference keeps those in direct-displacement form too. */
typedef struct debug_float3 {
  float v[3];
} debug_float3;

typedef struct debug_float4 {
  float v[4];
} debug_float4;

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
void render_debug_triangle(char flag, float *point0, float *point1, float *point2,
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
  rasterizer_debug_triangle(point0, point1, point2, color);
}

/* Draw a debug quad immediately as two triangles (0x188970). Splits the quad
 * (point0..point3) into triangles (0,1,2) and (0,2,3), each drawn via the
 * immediate triangle drawer. Immediate-mode only (asserts flag != 0). */
void render_debug_quadrilateral(char flag, float *point0, float *point1, float *point2,
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
  render_debug_triangle(flag, point0, point1, point2, color);
  render_debug_triangle(flag, point0, point2, point3, color);
}

/* Draw a filled debug polygon immediately as a triangle fan (0x188a90). Fans
 * from points[0]: for each i in 1..count-2 draws triangle
 * (points[0], points[i], points[i+1]). Each point is three floats.
 * Immediate-mode only. */
void render_debug_polygon(float *points, short count, void *color)
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
      render_debug_triangle(1, points, pi, pi + 3, color);
      i = (short)(i + 1);
    } while (i < count - 1);
  }
}

/* Intern a string into the per-frame debug string arena (0x188b20). Scans the
 * arena for an identical string already present and returns its address;
 * otherwise appends a null-terminated copy and advances the cursor. Returns
 * NULL (with a one-shot overflow warning) when the arena is full. The source
 * string is passed in EDI. */
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

void build_circle_points(float *table, float radius)
{
  x87_wide_t sn;
  x87_wide_t cs;
  short i;
  int idx;

  sn = HALO_DEBUG_TRIG_SIN(debug_circle_angle);
  memcpy(&table[0], &radius, sizeof(radius));
  table[1] = 0.0f;
  idx = 0;
  cs = HALO_DEBUG_TRIG_COS(debug_circle_angle);
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

#undef HALO_DEBUG_TRIG_SIN
#undef HALO_DEBUG_TRIG_COS

/* Build a debug coordinate frame from a direction vector (0x188c60). Given a
 * forward direction (EAX) and an origin position, fills a 13-float frame:
 *   frame[0]      = 1.0
 *   frame[1..3]   = side    = cross(up, forward)
 *   frame[4..6]   = up      = perpendicular3d(forward), normalized
 *   frame[7..9]   = forward = the input direction, normalized
 *   frame[10..12] = position
 * Returns the length of the input direction before normalization. The frame
 * pointer arrives in ECX and the direction in EAX. */
float build_height_matrix(float *frame, float *in_vec, float *position)
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
 * (build_height_matrix, returning the height as len) and a unit circle of the given
 * radius (build_circle_points), then fills the caller's vertex buffers:
 *   buffer1/buffer2 -- the bottom (z=0) and top (z=len) circle rings, 17 points
 *     each, transformed into world space by the frame.
 *   buffer3..buffer6 -- when all four are supplied (type 5 / dome), two half-
 *     rings of 9 points closing the top, built from opposite circle points.
 * Every vertex is transformed in place by the frame (matrix_transform_point).
 * The two ring buffers arrive in ECX/EDX and the center in EAX. */
void build_pill_points(float *buffer1, float *buffer2, float *center,
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

  len = build_height_matrix(frame, height_vec, center);
  build_circle_points(circle, radius);
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
 * The trailing arguments MUST be read with <stdarg.h> va_arg, not by taking
 * the address of `type` and walking past it: the original's raw
 * `[ebp+N]`-relative reads happen to work because MSVC's unoptimized cdecl
 * layout keeps a named parameter at its real incoming stack slot, but clang
 * gives `type` its own local storage, so `&type + 4` does not reliably land on
 * the caller's next pushed argument. Under clang this silently read whatever
 * scratch stack data happened to sit next to `type`'s local copy: harmless for
 * the float/int cases (wrong-looking debug geometry at worst), but for the
 * string cases (8/9) it fed a garbage pointer straight into `render_debug_add_cache_string` ->
 * `csstrncpy`, which faulted -- this was the `debug_sprites true`
 * crash/freeze.
 *
 * case 0's final `offset` field (rec+0x34) matches the original: the push-side
 * call site (render_debug_circle) never actually passes an `offset` argument, so both
 * the original binary and this port read one double past the real argument
 * list there. That field is never read back meaningfully; preserved as-is
 * rather than inventing a value the original never had. */
void render_debug_add_cache_entry(int type, ...)
{
  va_list ap;
  char *rec;
  float *p1;
  float *p2;
  float *color;
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

  rec = (char *)&debug_primitives[count];
  debug_primitive_count = count + 1;
  *(short *)rec = type;

  va_start(ap, type);
  switch (type) {
  case 0:
    p1 = va_arg(ap, float *);
    *(debug_float4 *)(rec + 0x04) = *(debug_float4 *)p1;
    ival = va_arg(ap, int);
    *(short *)(rec + 0x14) = (short)ival;
    ival = va_arg(ap, int);
    *(unsigned char *)(rec + 0x16) = (unsigned char)ival;
    p2 = va_arg(ap, float *);
    *(float *)(rec + 0x18) = p2[0];
    *(float *)(rec + 0x1c) = p2[1];
    dval = va_arg(ap, double);
    *(float *)(rec + 0x20) = (float)dval;
    color = va_arg(ap, float *);
    *(debug_float4 *)(rec + 0x24) = *(debug_float4 *)color;
    dval = va_arg(ap, double); /* past the real args; see comment above */
    *(float *)(rec + 0x34) = (float)dval;
    break;
  case 1:
  case 3:
    p1 = va_arg(ap, float *);
    *(debug_float3 *)(rec + 0x04) = *(debug_float3 *)p1;
    dval = va_arg(ap, double);
    *(float *)(rec + 0x10) = (float)dval;
    color = va_arg(ap, float *);
    *(debug_float4 *)(rec + 0x14) = *(debug_float4 *)color;
    break;
  case 2:
    p1 = va_arg(ap, float *);
    *(debug_float3 *)(rec + 0x04) = *(debug_float3 *)p1;
    p2 = va_arg(ap, float *);
    *(debug_float3 *)(rec + 0x10) = *(debug_float3 *)p2;
    color = va_arg(ap, float *);
    *(debug_float4 *)(rec + 0x1c) = *(debug_float4 *)color;
    break;
  case 4:
    p1 = va_arg(ap, float *);
    *(debug_float3 *)(rec + 0x04) = *(debug_float3 *)p1;
    p2 = va_arg(ap, float *);
    *(debug_float3 *)(rec + 0x10) = *(debug_float3 *)p2;
    dval = va_arg(ap, double);
    *(float *)(rec + 0x1c) = (float)dval;
    color = va_arg(ap, float *);
    *(debug_float4 *)(rec + 0x20) = *(debug_float4 *)color;
    break;
  case 5:
    p1 = va_arg(ap, float *);
    *(debug_float3 *)(rec + 0x04) = *(debug_float3 *)p1;
    p2 = va_arg(ap, float *);
    *(debug_float3 *)(rec + 0x10) = *(debug_float3 *)p2;
    dval = va_arg(ap, double);
    *(float *)(rec + 0x1c) = (float)dval;
    color = va_arg(ap, float *);
    *(debug_float4 *)(rec + 0x20) = *(debug_float4 *)color;
    break;
  case 6:
  case 7:
    p1 = va_arg(ap, float *);
    for (i = 0; i < 6; i++) {
      *(float *)(rec + 0x04 + i * 4) = p1[i];
    }
    color = va_arg(ap, float *);
    *(debug_float4 *)(rec + 0x1c) = *(debug_float4 *)color;
    break;
  case 8:
    interned = render_debug_add_cache_string(va_arg(ap, char *));
    if (interned != 0) {
      *(char **)(rec + 0x04) = interned;
    } else {
      debug_primitive_count = debug_primitive_count - 1;
    }
    break;
  case 9:
    interned = render_debug_add_cache_string(va_arg(ap, char *));
    if (interned == 0) {
      debug_primitive_count = debug_primitive_count - 1;
      break;
    }
    *(char **)(rec + 0x04) = interned;
    p1 = va_arg(ap, float *);
    *(debug_float3 *)(rec + 0x08) = *(debug_float3 *)p1;
    color = va_arg(ap, float *);
    *(debug_float4 *)(rec + 0x14) = *(debug_float4 *)color;
    break;
  default:
    break;
  }
  va_end(ap);
}

/* Draw or cache a debug point marker (0x189150). type 1. With flag set, render
 * a 3D crosshair: three axis-aligned line segments through `position`, each
 * 2*s long where s = scale * the global debug marker size (0x253398). The
 * 18-float vertex buffer is a single contiguous block walked in 6-float
 * (two-endpoint) strides by the line helper. With flag clear, submit a type-1
 * primitive (position, scale, color) to the per-frame cache. */
void render_debug_point(char flag, float *position, float scale, void *color)
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
      rasterizer_debug_line(p, p + 3, (int)color);
      p = p + 6;
      i--;
    } while (i != 0);
    return;
  }

  render_debug_add_cache_entry(1, position, (double)scale, color);
}

/* Draw or cache a debug line (0x189270). type 2. With flag set, render the line
 * segment point_a->point_b immediately; otherwise submit a type-2 primitive
 * (point_a, point_b, color) to the per-frame cache. */
__declspec(noinline) void render_debug_line(char flag, float *point_a, float *point_b, void *color)
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
    rasterizer_debug_line(point_a, point_b, (int)color);
    return;
  }

  render_debug_add_cache_entry(2, point_a, point_b, color);
}

/* Draw a debug vector as a line from a point along a direction (0x189320).
 * The line runs from point to point + scale*vector; the cache flag is forwarded
 * to the line drawer. */
void render_debug_vector(int flag, float *point, float *vector, float scale,
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
  render_debug_line(flag, point, endpoint, color);
}

/* Draw a debug tick as a line centered on a point (0x1893e0). Extends the line
 * from point + scale*dir to point - scale*dir; the cache flag is forwarded to
 * the line drawer. */
void render_debug_tick(int flag, float *point, float *dir, float scale, void *color)
{
  float pts[6];

  pts[0] = scale * dir[0] + point[0];
  pts[1] = scale * dir[1] + point[1];
  pts[2] = scale * dir[2] + point[2];
  scale = -scale;
  pts[3] = scale * dir[0] + point[0];
  pts[4] = scale * dir[1] + point[1];
  pts[5] = scale * dir[2] + point[2];
  render_debug_line(flag, pts, pts + 3, color);
}

/* Draw a debug line between two points, each offset by scale times a shared
 * vector (0x189450). The offset vector is *(float **)0x31fc44 (a camera basis
 * vector); the line runs from point_a + scale*V to point_b + scale*V. */
void render_debug_line_offset(int flag, float *point_a, float *point_b, void *color,
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
  render_debug_line(flag, pts, pts + 3, color);
}

/* Draw a debug coordinate frame as three colored axis lines (0x1894d0). The
 * frame (as built by build_height_matrix) stores its length at matrix[0], the side/up/
 * forward basis vectors at matrix[1..3]/[4..6]/[7..9], and the origin at
 * matrix[10..12]. Draws each basis vector from the origin, scaled by
 * scale*matrix[0], in the standard axis colors. */
void render_debug_matrix(int flag, float *matrix, float scale)
{
  float *origin;

  origin = matrix + 10;
  render_debug_vector(flag, origin, matrix + 1, scale * matrix[0], *(void **)0x2ee6d0);
  render_debug_vector(flag, origin, matrix + 4, scale * matrix[0], *(void **)0x2ee6d4);
  render_debug_vector(flag, origin, matrix + 7, scale * matrix[0], *(void **)0x2ee6d8);
}

/* Draw or cache a debug sphere (0x189540). type 3. With flag clear, caches a
 * type-3 primitive (center, radius, color). With flag set, first culls
 * against the debug frustum (render_frustum_sphere_visible); if visible,
 * builds a 16-segment circle table (build_circle_points) and draws three great
 * circles (in the XY, XZ and YZ planes) as line segments. Each iteration
 * walks two adjacent circle points (cp[-3..-2] current, cp[-1..0] next) and
 * emits one segment per plane into a contiguous six-float endpoint buffer. */
void render_debug_sphere(char flag, void *center, float radius, void *color)
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
    render_debug_add_cache_entry(3, center, (double)radius, color);
    return;
  }
  vis = (short)render_frustum_sphere_visible((void *)0x5065a4, center, radius);
  if (vis == 0) {
    return;
  }
  build_circle_points(circle, radius);
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
    rasterizer_debug_line(&verts[3], &verts[0], (int)color);
    /* XZ-plane great circle */
    verts[3] = c[0] + cp[-2];
    verts[4] = c[1];
    verts[5] = c[2] + cp[-3];
    verts[1] = c[1];
    verts[0] = c[0] + cp[0];
    verts[2] = c[2] + cp[-1];
    rasterizer_debug_line(&verts[3], &verts[0], (int)color);
    /* YZ-plane great circle */
    verts[4] = c[1] + cp[-3];
    verts[3] = c[0];
    verts[5] = c[2] + cp[-2];
    verts[0] = c[0];
    verts[1] = c[1] + cp[-1];
    verts[2] = c[2] + cp[0];
    rasterizer_debug_line(&verts[3], &verts[0], (int)color);
    cp = cp + 2;
    i = (short)(i - 1);
  } while (i != 0);
}

/* Draw or cache a debug cylinder (0x1896d0). type 4. With flag clear, caches a
 * type-4 primitive (base, height vector, radius, color). With flag set, builds
 * the two circle rings via the cylinder builder (build_pill_points) into a pair of
 * 17-vertex buffers, then draws the wireframe: each ring as a 16-segment strip,
 * four vertical edges connecting the rings, and two diameter cross-lines per
 * ring. */
void render_debug_cylinder(char flag, void *center, void *height_vec, float radius,
                  void *color)
{
  float buffers[102]; /* buf1 = [0..50], buf2 = [51..101]; 17 vertices each */
  int i;
  float *p1;
  float *p2;

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
    render_debug_add_cache_entry(4, center, height_vec, (double)radius, color);
    return;
  }
  build_pill_points(buffers, buffers + 51, center, height_vec, radius, 0, 0, 0, 0);

  p1 = buffers;
  p2 = buffers + 51;
  for (i = 16; i > 0; i--) {
    rasterizer_debug_line(p1, p1 + 3, (int)color);
    rasterizer_debug_line(p2, p2 + 3, (int)color);
    p1 += 3;
    p2 += 3;
  }

  p1 = buffers;
  p2 = buffers + 51;
  for (i = 4; i > 0; i--) {
    rasterizer_debug_line(p1, p2, (int)color);
    p1 += 12;
    p2 += 12;
  }

  p1 = buffers;
  p2 = buffers + 51;
  for (i = 2; i > 0; i--) {
    rasterizer_debug_line(p1, p1 + 24, (int)color);
    rasterizer_debug_line(p2, p2 + 24, (int)color);
    p1 += 12;
    p2 += 12;
  }
}

/* Draw or cache a debug capsule/dome (0x189860). type 5. With flag clear,
 * caches a type-5 primitive. With flag set, calls the cylinder builder
 * (build_pill_points) to fill two 17-vertex main rings plus four 9-vertex dome
 * half-rings, then draws: each main ring as a 16-segment strip, four vertical
 * edges connecting the main rings, and the four dome half-rings as 8-segment
 * strips. */
void render_debug_pill(char flag, void *center, void *height_vec, float radius,
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
    render_debug_add_cache_entry(5, center, height_vec, (double)radius, color);
    return;
  }
  build_pill_points(buf1, buf2, center, height_vec, radius, buf3, buf4, buf5, buf6);
  for (i = 0; i < 16; i++) {
    rasterizer_debug_line(&buf2[i * 3], &buf2[i * 3 + 3], (int)color);
    rasterizer_debug_line(&buf1[i * 3], &buf1[i * 3 + 3], (int)color);
  }
  for (i = 0; i < 4; i++) {
    rasterizer_debug_line(&buf2[i * 12], &buf1[i * 12], (int)color);
  }
  for (i = 0; i < 8; i++) {
    rasterizer_debug_line(&buf3[i * 3], &buf3[i * 3 + 3], (int)color);
    rasterizer_debug_line(&buf4[i * 3], &buf4[i * 3 + 3], (int)color);
    rasterizer_debug_line(&buf5[i * 3], &buf5[i * 3 + 3], (int)color);
    rasterizer_debug_line(&buf6[i * 3], &buf6[i * 3 + 3], (int)color);
  }
}

/* Draw or cache a debug box (0x189a20). type 6. With flag clear, caches a
 * type-6 primitive (six bounds floats + color). With flag set, expands the
 * bounds {x0,x1,y0,y1,z0,z1} into the eight box corners and draws the six faces
 * as quads (render_debug_quadrilateral). Corners are held in one contiguous 24-float buffer.
 */
void render_debug_box(char flag, float *bounds, void *color)
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
    render_debug_add_cache_entry(6, bounds, color);
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
  render_debug_quadrilateral(1, &corners[0], &corners[6], &corners[12], &corners[18], color);
  render_debug_quadrilateral(1, &corners[3], &corners[9], &corners[15], &corners[21], color);
  render_debug_quadrilateral(1, &corners[0], &corners[3], &corners[9], &corners[6], color);
  render_debug_quadrilateral(1, &corners[12], &corners[15], &corners[21], &corners[18],
               color);
  render_debug_quadrilateral(1, &corners[0], &corners[3], &corners[21], &corners[18], color);
  render_debug_quadrilateral(1, &corners[6], &corners[9], &corners[15], &corners[12], color);
}

/* Draw a closed debug polyline (0x189ba0). Immediate-mode only: with three or
 * more points draws the closing edge from the last point back to the first,
 * then a line between each pair of consecutive points, forming a line loop.
 * Each point is three floats. */
void render_debug_polygon_edges(float *points, short count, void *color)
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
    render_debug_line(1, points + (count - 1) * 3, points, color);
    if (1 < count) {
      points = points + 3;
      n = (unsigned short)(count - 1);
      do {
        render_debug_line(1, points - 3, points, color);
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
void render_debug_string(char flag, const char *string)
{
  if (string == 0) {
    display_assert("string", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x37d,
                   1);
    system_exit(-1);
  }

  if (flag != 0) {
    interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
    rasterizer_draw_string(0, 0, 0, 0, string);
    return;
  }

  render_debug_add_cache_entry(8, string);
}

/* Draw or cache a debug string at a 3D position (0x189cb0). type 9. With flag
 * clear, caches a type-9 primitive (string, position, color). With flag set,
 * projects the world position to screen space (render_camera_world_to_screen);
 * if on screen, converts the screen coordinates to integer text coordinates
 * (relative to the viewport size at 0x50657c/0x50657e), primes the debug text
 * state, sets the text color, and draws the string. */
void render_debug_string_at_point(char flag, void *position, void *string, int color)
{
  float proj[2];
  short text_pos[4];
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
  visible = render_camera_world_to_screen((void *)0x506550, (void *)0x5065a4,
                                          position, proj);
  if (visible != 0) {
    text_pos[1] = (short)(int)(proj[0] - (float)*(short *)0x50657e);
    text_pos[0] = (short)(int)(proj[1] - (float)*(short *)0x50657c);
    text_pos[2] = 0x7fff;
    text_pos[3] = 0x7fff;
    interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
    draw_string_set_color((void *)color);
    rasterizer_draw_string(text_pos, 0, 0, 0, string);
  }
}

/* Draw the collision-BSP leaf/cluster info text overlay (0x189de0). Gated on
 * the debug flag at 0x506535. Formats the debug point's (0x506550) leaf and
 * cluster indices; probes along the scaled forward vector (0x31fc50) and, on a
 * surface hit, appends the ground point, facing angle and surface index; if the
 * structure vector test (0x198cb0) passes, appends the containing structure
 * material's tag name; then draws the accumulated text. */
void render_debug_camera(void)
{
  char text[0x800];
  char collision_result[0x50];
  float scaled[3];
  float out40[3];
  float dir[3];
  float out34;
  float out30;
  float out2c;
  int out8;
  int out4;
  char location[8];
  float *fwd;
  void *e1;
  void *e2;

  if (*(char *)0x506535 == 0) {
    return;
  }
  scenario_location_from_point(location, (void *)0x506550);
  snprintf(text, 0x800,
           "point(%01.2f,%01.2f,%01.2f) leaf(#%d [%d]) cluster(#%d [%d])",
           *(float *)0x506550, *(float *)0x506554, *(float *)0x506558,
           *(int *)location, *(int *)0x506780, (int)*(short *)(location + 4),
           *(int *)0x506784);
  fwd = *(float **)0x31fc50;
  dir[0] = fwd[0] * *(float *)0x254cb8;
  dir[1] = fwd[1] * *(float *)0x254cb8;
  dir[2] = fwd[2] * *(float *)0x254cb8;
  if (FUN_0014df70(0x21, (float *)0x506550, dir, -1,
                   (short *)collision_result)) {
    snprintf(
      text + csstrlen(text), 0x800 - csstrlen(text),
      "|nground_point(%01.2f,%01.2f,%01.2f) facing(%01.2f) surface(#%d)",
      *(float *)(collision_result + 0x18), *(float *)(collision_result + 0x1c),
      *(float *)(collision_result + 0x20),
      x87_fatan2f(*(float *)0x506560, *(float *)0x50655c) * *(float *)0x2b073c,
      *(int *)(collision_result + 0x44));
  }
  scaled[0] = *(float *)0x50655c * *(float *)0x25acf0;
  scaled[1] = *(float *)0x506560 * *(float *)0x25acf0;
  scaled[2] = *(float *)0x506564 * *(float *)0x25acf0;
  if (structure_test_vector((float *)0x506550, scaled, out40, (int16_t *)&out8,
                            (int16_t *)&out4, (int32_t *)&out2c, &out30,
                            &out34)) {
    e1 =
      tag_block_get_element((char *)scenario_get() + 0x104, (short)out8, 0x20);
    e2 = tag_block_get_element((char *)e1 + 0x14, (short)out4, 0x100);
    snprintf(text + csstrlen(text), 0x800 - csstrlen(text), "|n%s",
             *(void **)((char *)e2 + 4));
  }
  interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
  rasterizer_draw_string(0, 0, 0, 0, text);
}

/* Draw the local player's vehicle-state debug text (0x18a000). Gated on the
 * flag at 0x506534. Resolves the local player's unit, and if it is riding an
 * object (unit+0x42c) shows "riding an elevator"; if the unit is in a vehicle
 * (unit+0xcc / unit+0x2a0) formats and draws the vehicle's speed/slide/turn
 * (vehicle+0x42c/0x430/0x434), plus "stuck!" when vehicle+0x478 is set. */
void render_debug_player(void)
{
  char buffer[0x400];
  int player_idx;
  void *player;
  int unit_handle;
  void *unit;
  void *vehicle;
  char *stuck_str;

  if (*(char *)0x506534 == 0 || *(short *)0x506548 == -1 ||
      local_player_get_player_index(*(short *)0x506548) == -1) {
    return;
  }
  player_idx = local_player_get_player_index(*(short *)0x506548);
  player = datum_get(*(void **)0x5aa6d4, player_idx);
  unit_handle = *(int *)((char *)player + 0x34);
  if (unit_handle == -1) {
    return;
  }
  unit = object_try_and_get_and_verify_type(unit_handle, 1);
  if (unit != 0 && *(int *)((char *)unit + 0x42c) != -1) {
    render_debug_string(1, "riding an elevator");
  }
  if (*(int *)((char *)unit + 0xcc) != -1 &&
      *(short *)((char *)unit + 0x2a0) != -1) {
    vehicle =
      object_try_and_get_and_verify_type(*(int *)((char *)unit + 0xcc), 2);
    if (*(int *)((char *)vehicle + 0x478) != 0) {
      stuck_str = "|nstuck!";
    } else {
      stuck_str = "";
    }
    crt_sprintf(buffer, "speed %5f|nslide %5f|nturn  %5f%s",
                *(float *)((char *)vehicle + 0x42c),
                *(float *)((char *)vehicle + 0x430),
                *(float *)((char *)vehicle + 0x434), stuck_str);
    render_debug_string(1, buffer);
  }
}

/* Draw the collision-BSP portal edges (0x18a110). A debug sub-renderer gated on
 * the flag at 0x506533. Iterates the portal block of the global collision BSP
 * (bsp+0x48); for each portal, looks up its two vertices in the vertex block
 * (bsp+0x54) by the indices stored in the portal element, and draws a line
 * between them. */
void render_debug_structure(void)
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
        render_debug_line(1, v0, v1, *(void **)0x2ee6d4);
        i++;
      } while (i < *portal_block);
    }
  }
}

/* Dump the collision-BSP descent path for the debug point (0x18a190). Gated on
 * the debug flag at 0x506532. Walks the structure BSP from the root: at each
 * node it evaluates the node's plane against the debug point (0x506550),
 * records the plane index, appends a "node plane side" text token ('+' front /
 * '-' back), and descends to the child on the point's side. On reaching a leaf
 * it appends the leaf index, or "solid" for the -1 child. Draws the accumulated
 * path text, and -- while key 0x3e is held -- writes the recorded plane indices
 * to d:\debug_bsp.txt. */
void render_debug_bsp(void)
{
  char text[0x800];
  int plane_stack[0x80];
  void *root;
  short node_count;
  char *cursor;
  void *node;
  float *plane;
  int node_index;
  int child;
  char side;
  int len;
  void *file;
  int i;

  if (*(char *)0x506532 == 0) {
    return;
  }
  node_count = 0;
  root = global_bsp3d_get();
  len = crt_sprintf(text, " node plane|n");
  cursor = text + len;
  node_index = 0;
  for (;;) {
    node = tag_block_get_element(root, node_index, 0xc);
    plane =
      (float *)tag_block_get_element((char *)root + 0xc, *(int *)node, 0x10);
    if (*(float *)0x506554 * plane[1] + *(float *)0x506558 * plane[2] +
          *(float *)0x506550 * plane[0] - plane[3] >=
        *(float *)0x2533c0) {
      side = 1;
    } else {
      side = 0;
    }
    if (node_count >= 0x80) {
      display_assert("plane_count<MAXIMUM_BSP3D_DEPTH",
                     "c:\\halo\\SOURCE\\render\\render_debug.c", 0x652, 1);
      system_exit(-1);
    }
    plane_stack[node_count] = *(int *)node;
    node_count = (short)(node_count + 1);
    len = crt_sprintf(cursor, "%5d %5d %c|n", node_index, *(int *)node,
                      side != 0 ? '+' : '-');
    cursor = cursor + len;
    child = *(int *)((char *)node + 4 + side * 4);
    if (child < 0) {
      break;
    }
    node_index = child;
  }
  if (child == -1) {
    crt_sprintf(cursor, "solid");
  } else {
    crt_sprintf(cursor, " leaf %5d", child & 0x7fffffff);
  }
  interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
  rasterizer_draw_string(0, 0, 0, 0, text);
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

/* Draw the raw controller-input debug overlay (0x18a370). Gated on the flag at
 * 0x506531. Sets three text tab stops (200, 400, 550), fetches the raw input
 * data string, primes the debug text state, and draws the string. */
void render_debug_input(void)
{
  short tab_stops[3];
  char buffer[512];

  if (*(char *)0x506531 != 0) {
    tab_stops[0] = 0xc8;
    tab_stops[1] = 0x190;
    tab_stops[2] = 0x226;
    draw_string_set_tab_stops(tab_stops, 3);
    input_get_raw_data_string(buffer, 0x1ff);
    interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
    rasterizer_draw_string(0, 0, 0, 0, buffer);
  }
}

/* Draw encounter/actor firing-position debug markers (0x18a3e0). Gated on the
 * flag at 0x506530. Iterates the scenario's actor-starting-location block
 * (scenario+0x258): for each entry it resolves the actor palette tag, probes a
 * ray from the entry position (FUN_0014df70) built from the entry's two packed
 * angle bytes (elem+0xe/+0xf), draws a small sphere at the entry (or, on a
 * surface hit, at the hit point) colored by whether the entry is within its
 * encounter's active squad range, and labels it with the stripped actor tag
 * name via the cached text-at-position drawer. */
void render_debug_structure_decals(void)
{
  char collision_result[0x50];
  float dir[3];
  float angles[2];
  int tag_index;
  int angle_tmp;
  void *actor_block;
  int *block;
  void *scenario;
  int i;
  void *elem;
  void *pal_elem;
  void *e;
  void *enc;
  int actor_idx;
  void *point;
  void *color;

  if (*(char *)0x506530 == 0) {
    return;
  }
  scenario = scenario_get();
  block = (int *)((char *)scenario + 0x258);
  if (*block <= 0) {
    return;
  }
  actor_block = (char *)scenario + 0x134;
  i = 0;
  do {
    elem = tag_block_get_element(block, i, 0x10);
    pal_elem =
      tag_block_get_element((char *)global_scenario_get() + 0x3b4,
                            *(unsigned char *)((char *)elem + 0xc), 0x10);
    tag_index = *(int *)((char *)pal_elem + 0xc);
    tag_get(0x64656361, tag_index);
    if (FUN_0018e720((int)elem) == -1) {
      actor_idx = -1;
    } else {
      e = tag_block_get_element((char *)scenario_get() + 0xe0,
                                FUN_0018e720((int)elem) & 0x7fffffff, 0x10);
      actor_idx = *(short *)((char *)e + 8);
    }
    enc = tag_block_get_element(actor_block, actor_idx, 0x68);
    angle_tmp = *(signed char *)((char *)elem + 0xe);
    angles[0] = (float)angle_tmp * *(float *)0x2b1958;
    angle_tmp = *(signed char *)((char *)elem + 0xf);
    angles[1] = (float)angle_tmp * *(float *)0x2b1954;
    angles_to_vector(dir, angles);
    if (FUN_0014df70(0x61, (float *)elem, dir, -1, (short *)collision_result)) {
      if (*(short *)((char *)enc + 0xc) == -1 ||
          (int)*(unsigned short *)((char *)enc + 0xe) <=
            i - *(short *)((char *)enc + 0xc)) {
        point = (char *)collision_result + 0x18;
        color = *(void **)0x2ee6f0;
      } else {
        point = (char *)collision_result + 0x18;
        color = *(void **)0x2ee6e0;
      }
    } else {
      point = elem;
      color = *(void **)0x2ee6d0;
    }
    render_debug_sphere(1, point, 0.1f, color);
    render_debug_string_at_point(0, elem, (void *)tag_name_strip_path(tag_get_name(tag_index)),
                 *(int *)0x2ee6d4);
    i = i + 1;
  } while (i < *block);
}

/* Draw a debug point on a plane (0x18a580). Projects a 2D point onto the plane,
 * lifts it off the plane along the projection axis by +/- offset (sign selects
 * the direction), then draws it as a debug point (render_debug_point). */
void render_debug_point2d(int flag, float *plane, int projection, int sign,
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
  render_debug_point(flag, pos, scale, color);
}

/* Draw one edge of a debug plane as a 3D line segment (0x18a650). Projects
 * two 2D endpoints (point_a, point_b) onto the plane, lifts each off the
 * plane along the projection axis by +/- offset (sign selects the direction),
 * then draws the connecting line. The two projected points share a contiguous
 * six-float buffer: pts[3..5] is endpoint A, pts[0..2] is endpoint B. */
void render_debug_line2d(int flag, float *plane, int projection, int sign,
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
  render_debug_line(flag, pts + 3, pts, color);
}

/* Draw a debug vector on a plane (0x18a770). Computes the 2D endpoint
 * point + scale*vector, then draws a plane-edge line from point to that
 * endpoint (render_debug_line2d, which projects both onto the plane). */
void render_debug_vector2d(int flag, float *plane, int projection, int sign,
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
  render_debug_line2d(flag, plane, projection, sign, point, endpoint, color, offset);
}

/* Draw or cache a debug plane disc (0x18a860). type 0. With flag clear, caches
 * a type-0 primitive. With flag set, builds a 16-segment circle table
 * (build_circle_points) and, for each segment, offsets two adjacent circle points by
 * the center, then hands them to the plane-edge drawer (render_debug_line2d) which
 * projects them onto the plane and draws the connecting 3D line. */
void render_debug_circle(char flag, float *plane, int projection, int sign,
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
    render_debug_add_cache_entry(0, plane, (short)projection, (unsigned char)sign, center,
                 (double)radius, color);
    return;
  }
  build_circle_points(circle, radius);
  cp = circle + 3;
  i = 0x10;
  do {
    pts[2] = center[0] + cp[-3];
    pts[3] = center[1] + cp[-2];
    pts[0] = center[0] + cp[-1];
    pts[1] = center[1] + cp[0];
    render_debug_line2d(1, plane, projection, sign, &pts[2], &pts[0], color, offset);
    cp = cp + 2;
    i = (short)(i - 1);
  } while (i != 0);
}

/* Draw a debug coordinate frame built from a forward/up basis (0x18a990).
 * Builds a 4x3 frame at position from the forward and up vectors, then draws
 * its axes. */
void render_debug_vectors(int flag, float *position, float *forward, float *up,
                  float scale)
{
  float matrix[13];

  matrix4x3_from_forward_up_position(matrix, position, forward, up);
  render_debug_matrix(flag, matrix, scale);
}

/* Draw a debug coordinate frame built from a surface normal (0x18a9d0).
 * Derives an orthonormal basis at position from the normal, then draws its
 * axes. */
void render_debug_quaternion(int flag, float *position, float *basis_data, float scale)
{
  float matrix[13];

  component_vectors_from_normal3d(matrix, position, basis_data);
  render_debug_matrix(flag, matrix, scale);
}

/* Draw a 2D debug box in screen space (0x18aa00). Immediate-mode only (there
 * is no cache path -- flag == 0 asserts). Expands the {x0,x1,y0,y1} bounds
 * into four corners at z = -1, transforms each by the debug screen matrix at
 * 0x5065e8, then draws them as a closed polyline. */
void render_debug_box2d_outline(char flag, float *bounds, void *color)
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
    render_debug_polygon_edges(corners, 4, color);
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
void render_debug_box_outline(char wireframe, float *bounds, void *color)
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

    render_debug_polygon_edges(&v[0], 4, color); /* bottom z-face line loop */
    render_debug_polygon_edges(&v[12], 4, color); /* top z-face line loop    */

    p = &v[0];
    i = 4;
    do {
      render_debug_line(1, p, p + 12, color); /* vertical edge */
      p += 3;
      i--;
    } while (i != 0);
    return;
  }

  render_debug_add_cache_entry(7, bounds, color);
}

/* Flush the per-frame debug primitive cache (0x18ac50). Run the fixed debug
 * sub-renderers, dispatch every cached primitive to its draw routine, then
 * reset the cache once the game frame advances past the cached frame. */
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
  FUN_001be7b0();
  render_debug_recording();
  render_debug_detail_objects();
  FUN_00149ce0();
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
    case 0:
      render_debug_circle(1, &rec->f04, rec->s14, rec->b16, &rec->f18, rec->f20,
                   &rec->f24, rec->f34);
      break;
    case 1:
      render_debug_point(1, &rec->f04, rec->f10, &rec->s14);
      break;
    case 2:
      render_debug_line(1, &rec->f04, &rec->f10, &rec->f1c);
      break;
    case 3:
      render_debug_sphere(1, &rec->f04, rec->f10, &rec->s14);
      break;
    case 4:
      render_debug_cylinder(1, &rec->f04, &rec->f10, rec->f1c, &rec->f20);
      break;
    case 5:
      render_debug_pill(1, &rec->f04, &rec->f10, rec->f1c, &rec->f20);
      break;
    case 6:
      render_debug_box(1, &rec->f04, &rec->f1c);
      break;
    case 7:
      render_debug_box_outline(1, &rec->f04, &rec->f1c);
      break;
    case 8:
      render_debug_string(1, *(const char **)&rec->f04);
      break;
    case 9:
      render_debug_string_at_point(1, &rec->f08, *(void **)&rec->f04, (int)&rec->s14);
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
