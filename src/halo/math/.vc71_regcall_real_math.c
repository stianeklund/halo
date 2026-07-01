/* 0x1acb0 — 2D scale-add: out = base + scale * dir. */
void FUN_0001acb0(float *base, float *dir, float scale, float *out)
{
  out[0] = scale * dir[0] + base[0];
  out[1] = scale * dir[1] + base[1];
}

/* 0x1ace0 — Squared distance between two 2D points. */
float FUN_0001ace0(float *a, float *b)
{
  float dx = b[0] - a[0];
  float dy = b[1] - a[1];
  return dx * dx + dy * dy;
}

/* 0x1ad10 — Distance between two 2D points. */
float FUN_0001ad10(float *a, float *b)
{
  float dx = b[0] - a[0];
  float dy = b[1] - a[1];
  return sqrtf(dx * dx + dy * dy);
}

/* 0x1ad40 — Negate a 2D vector: out = -in. */
void FUN_0001ad40(float *in, float *out)
{
  out[0] = -in[0];
  out[1] = -in[1];
}

#include "x87_math.h"

/* FUN_0001ad60 (0x1ad60) — Euclidean distance between two 3D points.
 * Confirmed: cdecl, 2 pointer args. Pure FPU leaf (FSUB/FMUL/FADD/FSQRT). */
float FUN_0001ad60(float *a, float *b)
{
  float dx = a[0] - b[0];
  float dy = a[1] - b[1];
  float dz = a[2] - b[2];
  return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* 0x109010 — Scale a rectangle (short coords) by a fraction param_4/256,
 * clamping the aspect ratio to the smaller dimension of param_2. */
void FUN_00109010(short *param_1, short *param_2, uint32_t *param_3,
                  short param_4)
{
  int w1 = param_1[2] - param_1[0];
  int h1 = param_1[3] - param_1[1];
  int w2 = param_2[2] - param_2[0];
  int h2 = param_2[3] - param_2[1];
  short sw, sh;
  short x0, y0;

  if (w1 * h2 - h1 * w2 == 0 || w1 * h2 < h1 * w2) {
    h1 = (short)(w1 * h2 / w2);
  } else {
    w1 = (short)(h1 * w2 / h2);
  }

  sw = (short)((uint32_t)(w1 * param_4) >> 8);
  sh = (short)((uint32_t)(h1 * param_4) >> 8);
  x0 = (short)((w1 - sw) / 2) + param_1[0];
  y0 = (short)((h1 - sh) / 2) + param_1[1];

  ((short *)param_3)[0] = x0;
  ((short *)param_3)[1] = y0;
  ((short *)param_3)[2] = x0 + sw;
  ((short *)param_3)[3] = y0 + sh;
}

/* 0x1090e0 — Initialize a 4x3 identity matrix (13 floats). */
void FUN_001090e0(float *out)
{
  ((uint32_t *)out)[0] = 0x3f800000;
  ((uint32_t *)out)[1] = 0x3f800000;
  ((uint32_t *)out)[2] = 0;
  ((uint32_t *)out)[3] = 0;
  ((uint32_t *)out)[4] = 0;
  ((uint32_t *)out)[5] = 0x3f800000;
  ((uint32_t *)out)[6] = 0;
  ((uint32_t *)out)[7] = 0;
  ((uint32_t *)out)[8] = 0;
  ((uint32_t *)out)[9] = 0x3f800000;
  ((uint32_t *)out)[10] = 0;
  ((uint32_t *)out)[11] = 0;
  ((uint32_t *)out)[12] = 0;
}

/* 0x109120 — Transpose the rotation part of a 4x3 matrix in-place.
 * Swaps [2]<->[4], [3]<->[7], [6]<->[8] (0-indexed from float[1]). */
void FUN_00109120(float *m)
{
  uint32_t *p = (uint32_t *)m;
  uint32_t t;

  t = p[2];
  p[2] = p[4];
  p[4] = t;

  t = p[3];
  p[3] = p[7];
  p[7] = t;

  t = p[6];
  p[6] = p[8];
  p[8] = t;
}

/* Compute the inverse of a 4x3 matrix (scale + rotation + translation).
 * Intermediates use double to prevent precision loss from x87 register spills.
 */
void matrix_inverse(float *src, float *dst)
{
  double tx, ty, tz;

  if (*(float *)((char *)src + 0x00) == 0.0f) {
    csmemset(dst, 0, 0x34);
    return;
  }

  tx = -*(float *)((char *)src + 0x28);
  ty = -*(float *)((char *)src + 0x2c);
  tz = -*(float *)((char *)src + 0x30);

  if (*(int *)src == 0x3f800000) {
    *(int *)dst = 0x3f800000;
  } else {
    double inv_scale = 1.0f / *(float *)((char *)src + 0x00);
    *(float *)((char *)dst + 0x00) = (float)inv_scale;
    tx = inv_scale * tx;
    ty = inv_scale * ty;
    tz = inv_scale * tz;
  }

  *(float *)((char *)dst + 0x04) = *(float *)((char *)src + 0x04);
  *(float *)((char *)dst + 0x14) = *(float *)((char *)src + 0x14);
  *(float *)((char *)dst + 0x24) = *(float *)((char *)src + 0x24);

  /* Load both sides before writing either — the original uses FPU+GPR
   * pairs so both values are live simultaneously.  Without this, in-place
   * inversion (src==dst) corrupts the second read. */
  {
    float s2 = *(float *)((char *)src + 0x08);
    float s4 = *(float *)((char *)src + 0x10);
    *(float *)((char *)dst + 0x08) = s4;
    *(float *)((char *)dst + 0x10) = s2;
  }
  {
    float s3 = *(float *)((char *)src + 0x0c);
    float s7 = *(float *)((char *)src + 0x1c);
    *(float *)((char *)dst + 0x0c) = s7;
    *(float *)((char *)dst + 0x1c) = s3;
  }
  {
    float s6 = *(float *)((char *)src + 0x18);
    float s8 = *(float *)((char *)src + 0x20);
    *(float *)((char *)dst + 0x20) = s6;
    *(float *)((char *)dst + 0x18) = s8;
  }

  /* Original MSVC evaluation order: (tx*col + tz*col) + ty*col */
  *(float *)((char *)dst + 0x28) =
    (float)((tx * *(float *)((char *)dst + 0x04) +
             tz * *(float *)((char *)dst + 0x1c)) +
            ty * *(float *)((char *)dst + 0x10));
  *(float *)((char *)dst + 0x2c) =
    (float)((tx * *(float *)((char *)dst + 0x08) +
             tz * *(float *)((char *)dst + 0x20)) +
            ty * *(float *)((char *)dst + 0x14));
  *(float *)((char *)dst + 0x30) =
    (float)((tx * *(float *)((char *)dst + 0x0c) +
             tz * *(float *)((char *)dst + 0x24)) +
            ty * *(float *)((char *)dst + 0x18));
}

/* 0x109240 — Initialize a scaled 4x3 identity matrix. */
void FUN_00109240(float *out, float scale)
{
  *(uint32_t *)&out[0] = *(uint32_t *)&scale;
  ((uint32_t *)out)[1] = 0x3f800000;
  ((uint32_t *)out)[2] = 0;
  ((uint32_t *)out)[3] = 0;
  ((uint32_t *)out)[4] = 0;
  ((uint32_t *)out)[5] = 0x3f800000;
  ((uint32_t *)out)[6] = 0;
  ((uint32_t *)out)[7] = 0;
  ((uint32_t *)out)[8] = 0;
  ((uint32_t *)out)[9] = 0x3f800000;
  ((uint32_t *)out)[10] = 0;
  ((uint32_t *)out)[11] = 0;
  ((uint32_t *)out)[12] = 0;
}

/* 0x109280 — matrix4x3_identity_with_position: build an identity 4x3 matrix
 * with the given translation.
 *
 * Matrix layout (13 floats):
 *   [0]       scale = 1.0
 *   [1..3]    row 0 (forward)  = (1, 0, 0)
 *   [4..6]    row 1 (left)     = (0, 1, 0)
 *   [7..9]    row 2 (up)       = (0, 0, 1)
 *   [10..12]  translation      = position
 *
 * Confirmed: XOR ECX,ECX zeros out [2],[3],[4],[6],[7],[8].
 * Confirmed: MOV EDX,0x3f800000 sets scale and diagonal to 1.0.
 * Confirmed: position copied from param_2 to [EAX+0x28..0x30].
 */
void matrix4x3_identity_with_position(float *out, float *position)
{
  out[0] = 1.0f;
  out[1] = 1.0f;
  out[2] = 0.0f;
  out[3] = 0.0f;
  out[4] = 0.0f;
  out[5] = 1.0f;
  out[6] = 0.0f;
  out[7] = 0.0f;
  out[8] = 0.0f;
  out[9] = 1.0f;
  out[10] = position[0];
  out[11] = position[1];
  out[12] = position[2];
}

void FUN_001092d0(float *out_matrix, float *axis, float sine, float cosine)
{
  float x;
  float y;
  float z;
  float xx;
  float yy;
  float zz;
  float one_minus_cosine;
  float xy_term;
  float xz_term;
  float yz_term;

  x = axis[0];
  y = axis[1];
  z = axis[2];

  xx = x * x;
  yy = y * y;
  zz = z * z;
  one_minus_cosine = 1.0f - cosine;

  out_matrix[0] = 1.0f;
  out_matrix[1] = (1.0f - xx) * cosine + xx;

  xy_term = y * x * one_minus_cosine;
  out_matrix[2] = sine * z + xy_term;
  out_matrix[4] = xy_term - sine * z;
  out_matrix[5] = (1.0f - yy) * cosine + yy;

  xz_term = z * x * one_minus_cosine;
  out_matrix[3] = xz_term - sine * y;
  out_matrix[7] = xz_term + sine * y;
  out_matrix[9] = (1.0f - zz) * cosine + zz;

  yz_term = z * y * one_minus_cosine;
  out_matrix[6] = sine * x + yz_term;
  out_matrix[8] = yz_term - sine * x;

  out_matrix[10] = 0.0f;
  out_matrix[11] = 0.0f;
  out_matrix[12] = 0.0f;
}

/* 0x1093b0 — Convert a quaternion to a 3x3 rotation matrix (inside a 4x3
 * frame). Sets scale=1.0 and translation=0. Original keeps sq0 (s*q[0]) in
 * 80-bit FPU while sq1/sq2 spill to float32. */
void FUN_001093b0(float *out, float *q)
{
  float norm = q[3] * q[3] + q[2] * q[2] + q[1] * q[1] + q[0] * q[0];
  float s = 0.0f;
  float sq1, sq2;
  float xw, yw, zw;
  float xx, xy, xz, yy, zy, zz;

  if (norm != 0.0f) {
    s = 2.0f / norm;
  }

  sq1 = s * q[1];
  sq2 = s * q[2];
  xw = s * q[0] * q[3];
  yw = sq1 * q[3];
  zw = sq2 * q[3];

  xx = s * q[0] * q[0];
  xy = sq1 * q[0];
  xz = sq2 * q[0];
  yy = sq1 * q[1];
  zy = sq2 * q[1];
  zz = sq2 * q[2];

  ((uint32_t *)out)[0] = 0x3f800000;
  ((uint32_t *)out)[10] = 0;
  ((uint32_t *)out)[11] = 0;
  ((uint32_t *)out)[12] = 0;

  out[1] = 1.0f - (yy + zz);
  out[2] = xy - zw;
  out[3] = xz + yw;
  out[4] = xy + zw;
  out[5] = 1.0f - (zz + xx);
  out[6] = zy - xw;
  out[7] = xz - yw;
  out[8] = zy + xw;
  out[9] = 1.0f - (yy + xx);
}

void component_vectors_from_normal3d(float *out_matrix, float *position,
                                     float *basis_data)
{
  typedef void (*quat_to_matrix_fn)(float *out, float *basis);

  ((quat_to_matrix_fn)0x1093b0)(out_matrix, basis_data);
  out_matrix[10] = position[0];
  out_matrix[11] = position[1];
  out_matrix[12] = position[2];
}

/* 0x109500 — Convert a quaternion+scale+position to a full 4x3 matrix.
 * param_2 layout: [quat(4), pos(3), scale(1)] at offsets 0..0x1c. */
void FUN_00109500(float *out, float *qsp)
{
  FUN_001093b0(out, qsp);
  out[0] = qsp[7];
  out[10] = qsp[4];
  out[11] = qsp[5];
  out[12] = qsp[6];
}

/* 0x109540 — matrix4x3_decompose: extract forward, up, and translation
 * vectors from a 4x3 matrix.
 *
 *   out_forward = matrix row 0   (offsets +0x04, +0x08, +0x0c)
 *   out_up      = matrix row 2   (offsets +0x1c, +0x20, +0x24)
 *   out_pos     = translation    (offsets +0x28, +0x2c, +0x30)
 *
 * Confirmed: reads 9 floats from fixed offsets, writes to three
 * separate output vectors. No scale component is returned.
 */
__declspec(noinline) void matrix4x3_decompose(float *matrix, float *out_pos,
                                              float *out_forward, float *out_up)
{
  out_forward[0] = *(float *)((char *)matrix + 0x04);
  out_forward[1] = *(float *)((char *)matrix + 0x08);
  out_forward[2] = *(float *)((char *)matrix + 0x0c);

  out_up[0] = *(float *)((char *)matrix + 0x1c);
  out_up[1] = *(float *)((char *)matrix + 0x20);
  out_up[2] = *(float *)((char *)matrix + 0x24);

  out_pos[0] = *(float *)((char *)matrix + 0x28);
  out_pos[1] = *(float *)((char *)matrix + 0x2c);
  out_pos[2] = *(float *)((char *)matrix + 0x30);
}

/* Transform a 3D point by a 4x3 matrix (scale + rotation + translation).
 * If scale != 1.0, input components are pre-multiplied by scale.
 * out = rotation * (scale * in) + translation.
 * Matrix layout: [scale +0x00][3x3 rotation +0x04..+0x24][translation
 * +0x28..+0x30]. */
void matrix_transform_point(float *matrix, float *in, float *out)
{
  float x = in[0];
  float y = in[1];
  float z = in[2];

  if (*(int *)matrix != 0x3f800000) {
    x = x * *matrix;
    y = y * *matrix;
    z = z * *matrix;
  }

  out[0] = x * *(float *)((char *)matrix + 0x04) +
           y * *(float *)((char *)matrix + 0x10) +
           z * *(float *)((char *)matrix + 0x1c) +
           *(float *)((char *)matrix + 0x28);
  out[1] = x * *(float *)((char *)matrix + 0x08) +
           y * *(float *)((char *)matrix + 0x14) +
           z * *(float *)((char *)matrix + 0x20) +
           *(float *)((char *)matrix + 0x2c);
  out[2] = x * *(float *)((char *)matrix + 0x0c) +
           y * *(float *)((char *)matrix + 0x18) +
           z * *(float *)((char *)matrix + 0x24) +
           *(float *)((char *)matrix + 0x30);
}

/* 0x109610 — Scale-transform a vector by a 4x3 matrix.
 * If matrix scale != 1.0, scales the input vector components first. */
void matrix_scale_transform_vector(float *matrix, float *in, float *out)
{
  float x = in[0];
  float y = in[1];
  float z = in[2];

  if (*(uint32_t *)&matrix[0] != 0x3f800000) {
    x = x * matrix[0];
    y = y * matrix[0];
    z = z * matrix[0];
  }
  out[0] = z * matrix[7] + y * matrix[4] + x * matrix[1];
  out[1] = z * matrix[8] + y * matrix[5] + x * matrix[2];
  out[2] = z * matrix[9] + y * matrix[6] + x * matrix[3];
}

/* Transform a 3D vector by a 3x3 rotation matrix (stored at matrix+0x4).
 * out[i] = dot(in, column_i of matrix).
 * The matrix layout is: [scale(4 bytes)][3x3 floats row-major at +0x4]. */
void matrix_transform_vector(float *matrix, float *in, float *out)
{
  float x = in[0];
  float y = in[1];
  float z = in[2];

  out[0] = x * *(float *)((char *)matrix + 0x04) +
           y * *(float *)((char *)matrix + 0x10) +
           z * *(float *)((char *)matrix + 0x1c);
  out[1] = x * *(float *)((char *)matrix + 0x08) +
           y * *(float *)((char *)matrix + 0x14) +
           z * *(float *)((char *)matrix + 0x20);
  out[2] = x * *(float *)((char *)matrix + 0x0c) +
           y * *(float *)((char *)matrix + 0x18) +
           z * *(float *)((char *)matrix + 0x24);
}

/* real_matrix3x3_transform_point (0x1096e0)
 *
 * Transform a world-space point into a 4x3 matrix's local frame.
 * Subtracts the matrix's translation (+0x28..+0x30), normalizes by
 * scale (+0x00), then applies the 3x3 rotation (+0x04..+0x24).
 * If scale == 0.0, outputs the zero vector.
 *
 * Counterpart of real_matrix3x3_transform_vector (0x109780) which
 * omits the translation subtraction — use _point for positions
 * (world-to-local) and _vector for directions/velocities. */
void real_matrix3x3_transform_point(void *matrix, float *point, float *out)
{
  float *m = (float *)matrix;
  float x, y, z;

  if (*m == *(float *)0x2533c0) {
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    return;
  }

  x = point[0] - *(float *)((char *)matrix + 0x28);
  y = point[1] - *(float *)((char *)matrix + 0x2c);
  z = point[2] - *(float *)((char *)matrix + 0x30);

  if (*(int *)m != 0x3f800000) {
    float inv_scale = *(float *)0x2533c8 / *m;
    x = inv_scale * x;
    y = inv_scale * y;
    z = inv_scale * z;
  }

  out[0] = x * *(float *)((char *)matrix + 0x04) +
           y * *(float *)((char *)matrix + 0x08) +
           z * *(float *)((char *)matrix + 0x0c);
  out[1] = x * *(float *)((char *)matrix + 0x10) +
           y * *(float *)((char *)matrix + 0x14) +
           z * *(float *)((char *)matrix + 0x18);
  out[2] = x * *(float *)((char *)matrix + 0x1c) +
           y * *(float *)((char *)matrix + 0x20) +
           z * *(float *)((char *)matrix + 0x24);
}

/* real_matrix3x3_transform_vector (0x109780)
 *
 * Transform a direction or velocity vector by the rotation part of a
 * 4x3 matrix, normalizing by scale. No translation subtraction —
 * use real_matrix3x3_transform_point (0x1096e0) for positions. */
void real_matrix3x3_transform_vector(void *matrix, vector3_t *vec,
                                     vector3_t *out)
{
  float *m = (float *)matrix;
  float x = vec->x;
  float y = vec->y;
  float z = vec->z;

  if (*(int *)m != 0x3f800000) {
    float inv_scale = *(float *)0x2533c8 / *m;
    x = inv_scale * x;
    y = inv_scale * y;
    z = inv_scale * z;
  }

  out->x = x * *(float *)((char *)matrix + 0x04) +
           y * *(float *)((char *)matrix + 0x08) +
           z * *(float *)((char *)matrix + 0x0c);
  out->y = x * *(float *)((char *)matrix + 0x10) +
           y * *(float *)((char *)matrix + 0x14) +
           z * *(float *)((char *)matrix + 0x18);
  out->z = x * *(float *)((char *)matrix + 0x1c) +
           y * *(float *)((char *)matrix + 0x20) +
           z * *(float *)((char *)matrix + 0x24);
}

/* Transform a 3D point by the rotation part of a 4x3 matrix (no scale
 * normalization, no translation). Computes:
 *   out[i] = dot(point, column_i of 3x3 rotation at matrix+0x04).
 * Matrix layout: [scale +0x00][3x3 rotation +0x04..+0x24][translation
 * +0x28..+0x30]. Only the 3x3 rotation is used. */
void real_matrix4x3_transform_point(void *matrix, void *point, void *out)
{
  float *p = (float *)point;
  float *o = (float *)out;
  float x = p[0];
  float y = p[1];
  float z = p[2];

  o[0] = x * *(float *)((char *)matrix + 0x04) +
         y * *(float *)((char *)matrix + 0x08) +
         z * *(float *)((char *)matrix + 0x0c);
  o[1] = x * *(float *)((char *)matrix + 0x10) +
         y * *(float *)((char *)matrix + 0x14) +
         z * *(float *)((char *)matrix + 0x18);
  o[2] = x * *(float *)((char *)matrix + 0x1c) +
         y * *(float *)((char *)matrix + 0x20) +
         z * *(float *)((char *)matrix + 0x24);
}

/* Multiply two 4x3 matrices: out = b * a.
 * Matrix layout (13 floats each):
 *   [0]       scale
 *   [1..9]    3x3 rotation (row-major: row0=forward[1..3], row1=left[4..6],
 * row2=up[7..9]) [10..12]  translation
 *
 * Result:
 *   out_rotation    = b_rotation * a_rotation
 *   out_translation = (b_translation * a_rotation) * a_scale + a_translation
 *   out_scale       = a_scale * b_scale  (x87 FLD/FMUL/FSTP)
 *
 * The original uses SSE1 (MOVSS+MOVHPS loads, SHUFPS broadcasts, MULPS/ADDPS
 * accumulation, MOVSS+MOVHPS stores). Each row is loaded as [x, 0, y, z] in
 * the XMM register (lane 1 unused). Row 2 is reshuffled via SHUFPS 0x36
 * before computation, and the result row 2 is reshuffled via SHUFPS 0x8f
 * before the store to realign lanes for MOVHPS/MOVSS output.
 * The final scale is computed by x87 to match the original FLD/FMUL/FSTP. */
#ifndef _MSC_VER
#define _MM_MALLOC_H_INCLUDED
#endif
#include <xmmintrin.h>

#ifdef __clang__
__attribute__((target("sse")))
#endif
void matrix4x3_multiply(float *a, float *b, float *out)
{
  /* All __m128 declarations hoisted to the top for C89 (MSVC 7.1)
   * compatibility. */
  float *a_rows; /* &a[1] */
  float *b_rows; /* &b[1] */
  float *out_rows; /* &out[1] */
  __m128 row0, row1, row2;
  __m128 xmm3, xmm4, xmm5, xmm6, xmm7;
  __m128 t3, t4, t5, a_trans, scale_v;

  a_rows = a + 1;
  b_rows = b + 1;
  out_rows = out + 1;

  /* Load a's 3 rotation rows into XMM registers.
   * Pattern: MOVSS loads first component to lane 0 (lanes 1-3 zeroed),
   * MOVHPS loads 2nd and 3rd components to lanes 2 and 3. Layout: [x,0,y,z].
   *
   * XMM0 = row0 (forward): [a[1], 0, a[2], a[3]]
   * XMM1 = row1 (left):    [a[4], 0, a[5], a[6]]
   * XMM2 = row2 (up):      loaded reversed, then SHUFPS 0x36 to
   * [a[7],0,a[8],a[9]] */

  /* XMM0: MOVSS a[1] → lane0; MOVHPS a[2],a[3] → lanes 2,3 */
  row0 = _mm_load_ss(&a_rows[0]);
  row0 = _mm_loadh_pi(row0, (const __m64 *)&a_rows[1]);
  /* row0 = [a[1], 0, a[2], a[3]] */

  /* XMM1: MOVSS a[4] → lane0; MOVHPS a[5],a[6] → lanes 2,3 */
  row1 = _mm_load_ss(&a_rows[3]);
  row1 = _mm_loadh_pi(row1, (const __m64 *)&a_rows[4]);
  /* row1 = [a[4], 0, a[5], a[6]] */

  /* XMM2: MOVSS a[9] → lane0; MOVHPS a[7],a[8] → lanes 2,3; then SHUFPS 0x36.
   * SHUFPS 0x36 = 0b00_11_01_10: result[0]=src[2], result[1]=src[1],
   *                               result[2]=src[3], result[3]=src[0].
   * Before shuffle: [a[9], 0, a[7], a[8]].
   * After  shuffle: [a[7], 0, a[8], a[9]]  (matches row0/row1 layout). */
  row2 = _mm_load_ss(&a_rows[8]);
  row2 = _mm_loadh_pi(row2, (const __m64 *)&a_rows[6]);
  row2 = _mm_shuffle_ps(row2, row2, 0x36);
  /* row2 = [a[7], 0, a[8], a[9]] */

  /* Compute output row 0: b[1]*row0 + b[2]*row1 + b[3]*row2.
   * Each b scalar is broadcast to all 4 lanes via SHUFPS imm=0. */
  xmm3 = _mm_shuffle_ps(_mm_load_ss(&b_rows[0]), _mm_load_ss(&b_rows[0]), 0);
  xmm4 = _mm_shuffle_ps(_mm_load_ss(&b_rows[1]), _mm_load_ss(&b_rows[1]), 0);
  xmm5 = _mm_shuffle_ps(_mm_load_ss(&b_rows[2]), _mm_load_ss(&b_rows[2]), 0);
  xmm3 = _mm_mul_ps(xmm3, row0); /* b[1]*row0 */
  xmm4 = _mm_mul_ps(xmm4, row1); /* b[2]*row1 */
  xmm5 = _mm_mul_ps(xmm5, row2); /* b[3]*row2 */
  xmm3 = _mm_add_ps(xmm3, xmm4);
  xmm3 = _mm_add_ps(xmm3, xmm5);
  /* xmm3 = out_row0 = [out[1], 0, out[2], out[3]] */

  /* Store output row 0: MOVSS → out[1]; MOVHPS → out[2],out[3]. */
  _mm_store_ss(&out_rows[0], xmm3);
  _mm_storeh_pi((__m64 *)&out_rows[1], xmm3);

  /* Compute output row 1: b[4]*row0 + b[5]*row1 + b[6]*row2. */
  xmm6 = _mm_shuffle_ps(_mm_load_ss(&b_rows[3]), _mm_load_ss(&b_rows[3]), 0);
  xmm7 = _mm_shuffle_ps(_mm_load_ss(&b_rows[4]), _mm_load_ss(&b_rows[4]), 0);
  xmm5 = _mm_shuffle_ps(_mm_load_ss(&b_rows[5]), _mm_load_ss(&b_rows[5]), 0);
  xmm6 = _mm_mul_ps(xmm6, row0); /* b[4]*row0 */
  xmm7 = _mm_mul_ps(xmm7, row1); /* b[5]*row1 */
  xmm5 = _mm_mul_ps(xmm5, row2); /* b[6]*row2 */
  xmm6 = _mm_add_ps(xmm6, xmm7);
  xmm6 = _mm_add_ps(xmm6, xmm5);
  /* xmm6 = out_row1 = [out[4], 0, out[5], out[6]] */

  /* Compute output row 2: b[7]*row0 + b[8]*row1 + b[9]*row2. */
  xmm7 = _mm_shuffle_ps(_mm_load_ss(&b_rows[6]), _mm_load_ss(&b_rows[6]), 0);
  xmm4 = _mm_shuffle_ps(_mm_load_ss(&b_rows[7]), _mm_load_ss(&b_rows[7]), 0);
  xmm5 = _mm_shuffle_ps(_mm_load_ss(&b_rows[8]), _mm_load_ss(&b_rows[8]), 0);
  xmm7 = _mm_mul_ps(xmm7, row0); /* b[7]*row0 */
  xmm4 = _mm_mul_ps(xmm4, row1); /* b[8]*row1 */
  xmm5 = _mm_mul_ps(xmm5, row2); /* b[9]*row2 */
  xmm7 = _mm_add_ps(xmm7, xmm4);
  xmm7 = _mm_add_ps(xmm7, xmm5);
  /* xmm7 = out_row2 = [out[7], 0, out[8], out[9]] before shuffle */

  /* Store output row 1: MOVSS → out[4]; MOVHPS → out[5],out[6]. */
  _mm_store_ss(&out_rows[3], xmm6);
  _mm_storeh_pi((__m64 *)&out_rows[4], xmm6);

  /* SHUFPS 0x8f reorders row2 result for MOVHPS/MOVSS output layout.
   * 0x8f = 0b10_00_11_11: result[0]=src[3], result[1]=src[3], result[2]=src[0],
   * result[3]=src[2]. Before: [out[7], 0, out[8], out[9]]. After:  [out[9],
   * out[9], out[7], out[8]]. MOVHPS stores lanes [2,3] → out[7],out[8]; MOVSS
   * stores lane [0] → out[9]. */
  xmm7 = _mm_shuffle_ps(xmm7, xmm7, 0x8f);

  /* Store output row 2: MOVHPS → out[7],out[8]; MOVSS → out[9]. */
  _mm_storeh_pi((__m64 *)&out_rows[6], xmm7);
  _mm_store_ss(&out_rows[8], xmm7);

  /* Translation row: (b_translation * a_rotation) * a_scale + a_translation.
   * b[10..12] each broadcast and multiplied against the 3 rotation rows. */
  t3 = _mm_shuffle_ps(_mm_load_ss(&b[10]), _mm_load_ss(&b[10]), 0);
  t4 = _mm_shuffle_ps(_mm_load_ss(&b[11]), _mm_load_ss(&b[11]), 0);
  t5 = _mm_shuffle_ps(_mm_load_ss(&b[12]), _mm_load_ss(&b[12]), 0);
  t3 = _mm_mul_ps(t3, row0); /* b[10]*row0 */
  t4 = _mm_mul_ps(t4, row1); /* b[11]*row1 */
  t5 = _mm_mul_ps(t5, row2); /* b[12]*row2 */
  t3 = _mm_add_ps(t3, t4);
  t3 = _mm_add_ps(t3, t5);
  /* t3 = b_trans * a_rot = [tx, 0, ty, tz] */

  /* Load a_translation via MOVSS+MOVHPS pattern. */
  a_trans = _mm_load_ss(&a[10]);
  a_trans = _mm_loadh_pi(a_trans, (const __m64 *)&a[11]);
  /* a_trans = [a[10], 0, a[11], a[12]] */

  /* Broadcast a_scale, multiply into t3, then add a_translation. */
  scale_v = _mm_shuffle_ps(_mm_load_ss(&a[0]), _mm_load_ss(&a[0]), 0);
  t3 = _mm_mul_ps(t3, scale_v);
  t3 = _mm_add_ps(t3, a_trans);
  /* t3 = [out[10], 0, out[11], out[12]] */

  /* Store translation: MOVSS → out[10]; MOVHPS → out[11],out[12]. */
  _mm_store_ss(&out[10], t3);
  _mm_storeh_pi((__m64 *)&out[11], t3);

  /* Scale: out[0] = a[0] * b[0] via x87 FLD/FMUL/FSTP (matches original). */
  out[0] = a[0] * b[0];
}

/* 0x1099a0 — Determinant of a 3x3 matrix (row-major, float[9]). */
float FUN_001099a0(float *m)
{
  return (m[8] * m[0] * m[4] + m[2] * m[3] * m[7] + m[6] * m[1] * m[5]) -
         m[0] * m[7] * m[5] - m[8] * m[1] * m[3] - m[6] * m[2] * m[4];
}

/* 0x1099f0 — Transpose a 3x3 matrix. Supports src == dst (in-place). */
void FUN_001099f0(float *src, float *dst)
{
  uint32_t t;
  uint32_t *s = (uint32_t *)src;
  uint32_t *d = (uint32_t *)dst;

  if (src == dst) {
    t = s[3];
    d[3] = s[1];
    d[1] = t;
    t = s[6];
    d[6] = s[2];
    d[2] = t;
    t = s[7];
    d[7] = s[5];
    d[5] = t;
    return;
  }
  d[0] = s[0];
  d[1] = s[3];
  d[2] = s[6];
  d[3] = s[1];
  d[4] = s[4];
  d[5] = s[7];
  d[6] = s[2];
  d[7] = s[5];
  d[8] = s[8];
}

/* 0x109ba0 — Build a 3x3 rotation matrix from an axis and sin/cos values.
 * Rodrigues' rotation formula. */
void FUN_00109ba0(float *out, float *axis, float sine, float cosine)
{
  float xx = axis[0] * axis[0];
  float yy = axis[1] * axis[1];
  float zz = axis[2] * axis[2];
  float one_minus_cos = 1.0f - cosine;
  float xy, xz, yz;

  out[0] = (1.0f - xx) * cosine + xx;

  xy = axis[1] * axis[0] * one_minus_cos;
  out[1] = xy;
  out[3] = xy - sine * axis[2];
  out[1] = sine * axis[2] + out[1];

  out[4] = (1.0f - yy) * cosine + yy;

  xz = axis[2] * axis[0] * one_minus_cos;
  out[2] = xz;
  out[6] = xz + sine * axis[1];
  out[2] = out[2] - sine * axis[1];

  out[8] = (1.0f - zz) * cosine + zz;

  yz = axis[2] * axis[1] * one_minus_cos;
  out[5] = yz;
  out[7] = yz - sine * axis[0];
  out[5] = sine * axis[0] + out[5];
}

/* 0x109c70 — Multiply two 3x3 matrices: out = a * b. Supports aliasing. */
void FUN_00109c70(float *a, float *b, float *out)
{
  int i;
  float *p;
  float local_a[9];
  float local_b[9];

  if (a == out) {
    p = local_a;
    for (i = 9; i != 0; i--) {
      *p = *a;
      a++;
      p++;
    }
    a = local_a;
  }
  if (b == out) {
    p = local_b;
    for (i = 9; i != 0; i--) {
      *p = *b;
      b++;
      p++;
    }
    b = local_b;
  }
  out[0] = b[2] * a[6] + a[3] * b[1] + a[0] * b[0];
  out[1] = a[7] * b[2] + a[1] * b[0] + a[4] * b[1];
  out[2] = a[8] * b[2] + a[2] * b[0] + a[5] * b[1];
  out[3] = b[5] * a[6] + b[3] * a[0] + a[3] * b[4];
  out[4] = a[1] * b[3] + a[4] * b[4] + a[7] * b[5];
  out[5] = a[2] * b[3] + a[5] * b[4] + a[8] * b[5];
  out[6] = a[6] * b[8] + a[3] * b[7] + a[0] * b[6];
  out[7] = a[1] * b[6] + a[4] * b[7] + a[7] * b[8];
  out[8] = a[2] * b[6] + a[5] * b[7] + a[8] * b[8];
}

/* 0x109d90 — Transform a 3D vector by a 3x3 matrix. Supports b==out aliasing.
 */
void FUN_00109d90(float *m, float *v, float *out)
{
  float local[3];

  if (v == out) {
    local[0] = v[0];
    local[1] = v[1];
    local[2] = v[2];
    v = local;
  }
  out[0] = m[0] * v[0] + m[3] * v[1] + m[6] * v[2];
  out[1] = m[4] * v[1] + m[1] * v[0] + m[7] * v[2];
  out[2] = m[5] * v[1] + m[2] * v[0] + m[8] * v[2];
}

/* Build a 4x3 matrix from forward and up direction vectors.
 * Layout: [1.0f scale][forward 3f][left 3f][up 3f][translation 0,0,0].
 * Left is computed as forward x up (cross product). */
void matrix_from_forward_and_up(float *out, float *forward, float *up)
{
  /* scale = 1.0 */
  *(int *)out = 0x3f800000;

  /* row 0: forward */
  *(float *)((char *)out + 0x04) = forward[0];
  *(float *)((char *)out + 0x08) = forward[1];
  *(float *)((char *)out + 0x0c) = forward[2];

  /* row 1: left = forward x up */
  *(float *)((char *)out + 0x10) = forward[2] * up[1] - up[2] * forward[1];
  *(float *)((char *)out + 0x14) = up[2] * forward[0] - forward[2] * up[0];
  *(float *)((char *)out + 0x18) = forward[1] * up[0] - forward[0] * up[1];

  /* row 2: up */
  *(float *)((char *)out + 0x1c) = up[0];
  *(float *)((char *)out + 0x20) = up[1];
  *(float *)((char *)out + 0x24) = up[2];

  /* translation = zero */
  *(float *)((char *)out + 0x28) = 0.0f;
  *(float *)((char *)out + 0x2c) = 0.0f;
  *(float *)((char *)out + 0x30) = 0.0f;
}

/* 0x109e90 — Build a 4x3 rotation matrix from euler angles (yaw, pitch, roll).
 */
void FUN_00109e90(float *out, float yaw, float pitch, float roll)
{
  float cr, sr, sp, cy, sy;
  float cp_f;

  cr = x87_fcos(roll);
  ((uint32_t *)out)[0] = 0x3f800000;
  ((uint32_t *)out)[10] = 0;
  ((uint32_t *)out)[11] = 0;
  ((uint32_t *)out)[12] = 0;
  sr = x87_fsin(roll);
  cp_f = x87_fcos(pitch);
  sp = x87_fsin(pitch);
  cy = x87_fcos(yaw);
  sy = x87_fsin(yaw);
  out[1] = cy * cp_f;
  out[2] = sy * cr - (float)(sp * sr) * cy;
  out[3] = sy * sr + sp * cr * cy;
  out[4] = -(sy * cp_f);
  out[5] = cy * cr + (float)(sp * sr) * sy;
  out[6] = cy * sr - sp * cr * sy;
  out[7] = -sp;
  out[8] = -(cp_f * sr);
  out[9] = cp_f * cr;
}

/* 0x109f40 — Extract yaw/pitch/roll euler angles from a 4x3 matrix's
 * rotation part (the inverse of FUN_00109e90).
 *
 * Matrix rows (row-major, +0x04..+0x24): row0 (forward) m[1..3],
 * row1 m[4..6], row2 (up) m[7..9]. Output euler[0]=yaw, [1]=pitch,
 * [2]=roll.
 *
 * pitch = -asin(m[7])  (m[7] = -sin(pitch) in the forward transform).
 * When cos(pitch) exceeds a small epsilon the normal branch recovers
 * yaw and roll via atan2 of the off-axis rotation terms divided by
 * cos(pitch); otherwise (gimbal lock) roll is forced to 0 and yaw is
 * recovered from a degenerate pair. The +/-1.0 scale constants and the
 * 1/cos divisions are kept explicit to preserve the original codegen. */
void FUN_00109f40(float *matrix, float *euler)
{
  float pitch;
  float cos_pitch;
  float inv_neg; /* -1.0 / cos_pitch  (FLD [0x255e94] / cos) */
  float inv_pos; /*  1.0 / cos_pitch  (FLD [0x2533c8] / cos) */

  pitch = -(float)asin((double)matrix[7]);
  euler[1] = pitch;
  cos_pitch = x87_fcos(pitch);

  if ((double)*(float *)0x28c728 < (double)cos_pitch) {
    inv_neg = *(float *)0x255e94 / cos_pitch;
    inv_pos = *(float *)0x2533c8 / cos_pitch;
    euler[2] = (float)atan2((double)(inv_neg * matrix[8]),
                            (double)(inv_pos * matrix[9]));
    euler[0] = (float)atan2((double)(inv_neg * matrix[4]),
                            (double)(inv_pos * matrix[1]));
    return;
  }

  euler[2] = 0.0f;
  euler[0] = (float)atan2((double)matrix[2], (double)matrix[5]);
}

/* Convert a 4x3 matrix rotation part to a unit quaternion (Shepperd's method).
 * Output quaternion layout: [x, y, z, w]. */
void FUN_00109fc0(float *matrix4x3, float *out_quat4)
{
  static const int16_t nxt[3] = { 1, 2, 0 };
  float *m = (float *)((char *)matrix4x3 + 4);
  float trace = m[0] + m[4] + m[8];
  float s;
  short i;
  int j, k;
  float q[3];

  if (trace > 0.0f) {
    s = sqrtf(trace + 1.0f);
    out_quat4[3] = 0.5f * s;
    s = 0.5f / s;
    out_quat4[0] = (m[7] - m[5]) * s;
    out_quat4[1] = (m[2] - m[6]) * s;
    out_quat4[2] = (m[3] - m[1]) * s;
    return;
  }

  i = 0;
  if (m[4] > m[0])
    i = 1;
  if (m[8] > m[i * 4])
    i = 2;

  j = nxt[i];
  k = nxt[j];

  s = sqrtf(m[i * 3 + i] - (m[k * 3 + k] + m[j * 3 + j]) + 1.0f);
  q[i] = 0.5f * s;
  if (s != 0.0f) {
    s = 0.5f / s;
  }
  q[j] = (m[i * 3 + j] + m[j * 3 + i]) * s;
  q[k] = (m[i * 3 + k] + m[k * 3 + i]) * s;

  out_quat4[0] = q[0];
  out_quat4[1] = q[1];
  out_quat4[2] = q[2];
  out_quat4[3] = (m[k * 3 + j] - m[j * 3 + k]) * s;
}

/* Build a 4x3 matrix from forward, up direction vectors and a position.
 * Fills the rotation via matrix_from_forward_and_up, then sets translation. */
void matrix4x3_from_forward_up_position(void *out, float *position,
                                        float *forward, float *up)
{
  matrix_from_forward_and_up((float *)out, forward, up);
  *(float *)((char *)out + 0x28) = position[0];
  *(float *)((char *)out + 0x2c) = position[1];
  *(float *)((char *)out + 0x30) = position[2];
}

/* Compute the rotation difference between two 4x3 matrices as an axis-angle
 * vector.  Inverts mat1, multiplies by mat0, extracts the rotation quaternion,
 * converts to axis-angle, then writes (axis * angle) into out_vec3. */
void quaternion_to_angle_and_vector(float *mat0, float *mat1, float *out_vec3)
{
  float local_inv[13];
  float local_product[13];
  float local_quat[4];
  float local_angle;

  matrix_inverse(mat1, local_inv);
  matrix4x3_multiply(mat0, local_inv, local_product);
  FUN_00109fc0(local_product, local_quat);
  FUN_0010caf0(local_quat, &local_angle, out_vec3);
  out_vec3[0] = local_angle * out_vec3[0];
  out_vec3[1] = local_angle * out_vec3[1];
  out_vec3[2] = local_angle * out_vec3[2];
}

/* Transform a plane (normal + distance) by a 4x3 matrix.
 * The plane normal (in_plane[0..2]) is rotated by the matrix's 3x3 rotation
 * part, and the distance (in_plane[3]) is recomputed from the original
 * distance scaled by matrix[0] plus the dot product of the transformed normal
 * with the matrix's translation (matrix[10..12]). */
void FUN_0010a1c0(float *matrix, float *in_plane, float *out_plane)
{
  float nx = in_plane[0];
  float ny = in_plane[1];
  float nz = in_plane[2];

  out_plane[0] = nx * matrix[1] + ny * matrix[4] + nz * matrix[7];
  out_plane[1] = nx * matrix[2] + ny * matrix[5] + nz * matrix[8];
  out_plane[2] = nx * matrix[3] + ny * matrix[6] + nz * matrix[9];
  out_plane[3] = in_plane[3] * matrix[0] + matrix[10] * out_plane[0] +
                 matrix[11] * out_plane[1] + matrix[12] * out_plane[2];
}

/* Transform a plane by a matrix. Computes new plane distance and normal. */
void FUN_0010a240(float *matrix, float *plane, int out)
{
  float d;

  if (*matrix == 0.0f) {
    *(int *)(out + 0xc) = 0;
    real_matrix3x3_transform_vector(matrix, (void *)plane, (void *)out);
    return;
  }
  d = plane[3] -
      (matrix[10] * plane[0] + matrix[11] * plane[1] + matrix[12] * plane[2]);
  *(float *)(out + 0xc) = d;
  if (*matrix != 1.0f) {
    *(float *)(out + 0xc) = d / *matrix;
  }
  real_matrix3x3_transform_vector(matrix, (void *)plane, (void *)out);
}

/* 0x10a2c0 — Build a 3x3 basis matrix from forward and up vectors.
 * Row 0 = forward, row 1 = cross(forward, up), row 2 = up. */
void FUN_0010a2c0(float *out, float *forward, float *up)
{
  float f0, f1, f2, u0, u1, u2;

  out[0] = forward[0];
  out[1] = forward[1];
  out[2] = forward[2];
  f0 = forward[0];
  f1 = forward[1];
  f2 = forward[2];
  u0 = up[0];
  u1 = up[1];
  u2 = up[2];
  out[3] = u1 * f2 - f1 * u2;
  out[4] = f0 * u2 - u0 * f2;
  out[5] = f1 * u0 - u1 * f0;
  out[6] = up[0];
  out[7] = up[1];
  out[8] = up[2];
}

/* Convert a 3x3 rotation matrix to a unit quaternion [x,y,z,w].
 * Uses the Shepperd method for numerical stability. */
void FUN_0010a330(float *m, float *quat)
{
  float trace;
  float s;
  int i, j, k;
  float local_10[3];

  trace = m[4] + m[0] + m[8];
  if (trace > 0.0f) {
    s = sqrtf(trace + 1.0f);
    quat[3] = 0.5f * s;
    s = 0.5f / s;
    quat[0] = (m[7] - m[5]) * s;
    quat[1] = (m[2] - m[6]) * s;
    quat[2] = (m[3] - m[1]) * s;
    return;
  }

  i = (m[4] > m[0]) ? 1 : 0;
  if (m[8] > m[i * 4])
    i = 2;

  j = *(short *)((char *)0x31fb8c + i * 2);
  k = *(short *)((char *)0x31fb8c + j * 2);

  s = sqrtf(m[i * 4] - (m[k * 4] + m[j * 4]) + 1.0f);
  local_10[i] = 0.5f * s;
  if (s != 0.0f)
    s = 0.5f / s;
  local_10[j] = (m[i * 3 + j] + m[j * 3 + i]) * s;
  local_10[k] = (m[i * 3 + k] + m[k * 3 + i]) * s;
  quat[0] = local_10[0];
  quat[1] = local_10[1];
  quat[2] = local_10[2];
  quat[3] = (m[k * 3 + j] - m[j * 3 + k]) * s;
}

/* 0x10a480 — Validate a real_point4d (quaternion/plane): all 4 components
 * must be a valid normal3d AND w must not be infinity. */
int FUN_0010a480(int p)
{
  char valid;

  valid = (char)valid_real_normal3d((float *)p);
  if (valid != '\0' && (*(uint32_t *)(p + 0xc) & 0x7f800000) != 0x7f800000) {
    return 1;
  }
  return 0;
}

/* Construct a matrix from a 3D plane (normal + distance). */
void FUN_0010a4c0(int out_matrix, float *plane)
{
  float axis[3];
  float point[3];

  if (!valid_real_normal3d(plane) ||
      (*(unsigned int *)&plane[3] & 0x7f800000) == 0x7f800000) {
    display_assert("valid_real_plane3d(plane)",
                   "c:\\halo\\SOURCE\\math\\matrix_math.c", 0x172, 1);
    system_exit(-1);
  }
  perpendicular3d(plane, axis);
  normalize3d(axis);
  point[0] = plane[3] * plane[0];
  point[1] = plane[1] * plane[3];
  point[2] = plane[2] * plane[3];
  matrix_from_forward_and_up((float *)out_matrix, axis, plane);
  *(float *)(out_matrix + 0x28) = point[0];
  *(float *)(out_matrix + 0x2c) = point[1];
  *(float *)(out_matrix + 0x30) = point[2];
}

/* 0x10a570 — periodic_functions_dispose: free all 12 periodic + 6 transition
 * function tables and clear the initialized flag. */
void FUN_0010a570(void)
{
  void **table;
  int i;

  if (*(char *)0x46e39c != '\0') {
    table = (void **)0x46e3b8;
    for (i = 0xc; i != 0; i--) {
      debug_free(*table, "c:\\halo\\SOURCE\\math\\periodic_functions.c", 0x7a);
      table++;
    }
    table = (void **)0x46e3a0;
    for (i = 6; i != 0; i--) {
      debug_free(*table, "c:\\halo\\SOURCE\\math\\periodic_functions.c", 0x84);
      table++;
    }
    *(char *)0x46e39c = '\0';
  }
}

/* 0x10a5e0 — evaluate one of the built-in periodic functions
 * (function_type 0..11) at the given input.
 *
 * function_type 0 returns 1.0 unconditionally. Otherwise the curve is
 * sampled from a runtime byte table (PERIODIC_FUNCTION_TABLES at 0x46e3b8,
 * indexed by type), each a byte[1024] normalised by 1/255. The input is
 * scaled by 25.6 (0x28c838); the fractional weight is fmod(scaled, 1.0)
 * and the integer index is FISTP(scaled - weight), masked to [0,0x3ff].
 * Both the index and index+1 wrap modulo 1024. The result is the linear
 * interpolation table[idx]*(1-weight) + table[idx+1]*weight.
 *
 * For function_types 6 and 7 (bit mask 0xc0) a discontinuity fix-up is
 * applied: when the first sample is above 0.75 and the next is below 0.25
 * (a wrap from ~1 down through 0) the next sample is bumped by +1.0 before
 * interpolating, and any result exceeding 1.0 is brought back by -1.0 so
 * the output stays in [0,1). If the tables are not yet initialised
 * (flag at 0x46e39c == 0) it returns 0.0. */
float FUN_0010a5e0(int16_t function_type, float input)
{
  unsigned char *table;
  float scaled;
  float weight;
  unsigned int idx;
  float v0;
  float v1;
  float result;

  if (function_type == 0) {
    return *(float *)0x2533c8;
  }

  if (function_type < 0 || function_type > 0xb) {
    display_assert(
      "function_type>=0 && function_type<NUMBER_OF_PERIODIC_FUNCTIONS",
      "c:\\halo\\SOURCE\\math\\periodic_functions.c", 0x9d, 1);
    system_exit(-1);
  }

  if (*(char *)0x46e39c == '\0') {
    return *(float *)0x2533c0;
  }

  scaled = input * *(float *)0x28c838;
  weight = x87_fmod(scaled, *(double *)0x2573d8);
  idx = (unsigned int)x87_round_to_int(scaled - weight) & 0x3ff;

  table = ((unsigned char **)0x46e3b8)[function_type];
  v0 = (float)table[idx] * *(float *)0x261518;
  v1 = (float)table[(idx + 1) & 0x3ff] * *(float *)0x261518;

  if ((1 << function_type & 0xc0) == 0) {
    return (*(float *)0x2533c8 - weight) * v0 + v1 * weight;
  }

  if (*(float *)0x25afcc < v0 && v1 < *(float *)0x25337c) {
    v1 = v1 + *(float *)0x2533c8;
  }
  result = (*(float *)0x2533c8 - weight) * v0 + v1 * weight;
  if (*(float *)0x2533c8 < result) {
    return result - *(float *)0x2533c8;
  }
  return result;
}

/* 0x10a710 — transition_function_evaluate: evaluate one of the built-in
 * transition curves (function_type 0..5) at parameter t in [0,1].
 *
 * t is first clamped to [0,1]. function_type 0 is the identity (returns
 * the clamped t directly). Otherwise the curve is sampled from a runtime
 * byte table (TRANSITION_FUNCTION_TABLES at 0x46e3a0, indexed by type),
 * each entry a byte[1024] of values normalised by 1/255. The clamped t is
 * scaled by 1023.0, the integer sample index is the FISTP round-to-nearest
 * of (scaled - 0.5) == floor(scaled), and the fractional weight is
 * fmod(scaled, 1.0). The result is a linear interpolation between
 * table[idx] and table[idx+1]. When idx hits the last slot (0x3ff) the
 * top sample is returned directly. If the tables are not yet initialised
 * (flag at 0x46e39c == 0) it returns 0.0. */
float transition_function_evaluate(short function_type, float t)
{
  unsigned char *table;
  float scaled;
  float weight;
  int idx;

  if (t < *(float *)0x2533c0) {
    t = 0.0f;
  } else if (*(float *)0x2533c8 < t) {
    t = 1.0f;
  }

  if (function_type == 0) {
    return t;
  }

  if (function_type < 0 || function_type > 5) {
    display_assert(
      "function_type>=0 && function_type<NUMBER_OF_TRANSITION_FUNCTIONS",
      "c:\\halo\\SOURCE\\math\\periodic_functions.c", 0xd8, 1);
    system_exit(-1);
  }

  if (*(char *)0x46e39c == '\0') {
    return *(float *)0x2533c0;
  }

  table = ((unsigned char **)0x46e3a0)[function_type];
  scaled = t * *(float *)0x28c87c;
  weight = x87_fmod(scaled, *(double *)0x2573d8);
  idx = x87_round_to_int(scaled - *(float *)0x253398);

  if ((short)idx == 0x3ff) {
    return (float)table[0x3ff] * *(float *)0x261518;
  }

  return (float)table[idx] * *(float *)0x261518 *
           (*(float *)0x2533c8 - weight) +
         (float)table[idx + 1] * *(float *)0x261518 * weight;
}

/* 0x10b5c0 — real_math_initialize: init random tables and periodic functions.
 */
void real_math_initialize(void)
{
  random_math_initialize();
  periodic_functions_initialize();
}

/* 0x10b5d0 — real_math_dispose: dispose random tables and periodic functions.
 */
void real_math_dispose(void)
{
  random_math_dispose();
  FUN_0010a570();
}

void real_math_reset_precision(void)
{
  __control87(0x9001f, 0xfffff);
}

/* Rotate a 2D float vector 90 degrees counterclockwise (0x10b600).
 * Computes: out[0] = -in[1], out[1] = in[0]
 * This produces the perpendicular (left-normal) of a 2D vector.
 */
void perpendicular2d(float *in, float *out)
{
  out[0] = -in[1];
  out[1] = in[0];
}

/* Compute a perpendicular vector to the input 3D vector.
 * Finds the component with the smallest absolute value and zeros it,
 * then swaps the other two with one negated.  This produces a vector
 * orthogonal to `in` without requiring normalization.
 *
 * If abs(x) is smallest: out = (0, z, -y)
 * If abs(y) is smallest: out = (-z, 0, x)
 * If abs(z) is smallest: out = (y, -x, 0)
 */
void perpendicular3d(float *in, float *out)
{
  float abs_x = fabsf(in[0]);
  float abs_y = fabsf(in[1]);
  float abs_z = fabsf(in[2]);

  if (abs_x <= abs_y && abs_x <= abs_z) {
    out[0] = 0.0f;
    out[1] = in[2];
    out[2] = -in[1];
  } else if (abs_y <= abs_z) {
    out[1] = 0.0f;
    out[0] = -in[2];
    out[2] = in[0];
  } else {
    out[0] = in[1];
    out[1] = -in[0];
    out[2] = 0.0f;
  }
}

/* 0x10b6b0 — Compute a perpendicular 4D vector (swizzle + negate). */
void perpendicular4d(float *in, float *out)
{
  out[0] = in[2];
  out[1] = in[3];
  out[2] = -in[0];
  out[3] = -in[1];
}

/* Rotate vector in-place around axis by angle given as sin/cos (0x10b6e0).
 * Uses the Rodrigues rotation formula:
 *   v' = v*cos + axis*(axis.v)*(1-cos) - (v x axis)*sin */
void rotate_vector3d_by_sincos(float *vector, float *axis, float sin_angle,
                               float cos_angle)
{
  float v0 = vector[0], v1 = vector[1], v2 = vector[2];
  float a0 = axis[0], a1 = axis[1], a2 = axis[2];

  float k = (1.0f - cos_angle) * (v0 * a0 + v1 * a1 + v2 * a2);

  float cx = v1 * a2 - v2 * a1;
  float cy = v2 * a0 - v0 * a2;
  float cz = v0 * a1 - v1 * a0;

  vector[0] = k * a0 + cos_angle * v0 - cx * sin_angle;
  vector[1] = k * a1 + cos_angle * v1 - cy * sin_angle;
  vector[2] = k * a2 + cos_angle * v2 - cz * sin_angle;
  if (vector[0] != vector[0] || vector[1] != vector[1] ||
      vector[2] != vector[2]) {
    error(2,
          "rotate_vector3d_by_sincos: NaN OUTPUT v=(%f,%f,%f) axis=(%f,%f,%f) "
          "sin=%f cos=%f",
          (double)vector[0], (double)vector[1], (double)vector[2], (double)a0,
          (double)a1, (double)a2, (double)sin_angle, (double)cos_angle);
  }
}

/* 0x10b780 — Linearly interpolate between two 3D vectors: out = a*(1-t) + b*t.
 */
void vectors_interpolate(float *a, float *b, float t, float *out)
{
  float one_minus_t = 1.0f - t;
  out[0] = t * b[0] + one_minus_t * a[0];
  out[1] = t * b[1] + one_minus_t * a[1];
  out[2] = t * b[2] + one_minus_t * a[2];
}

/* Linearly interpolate between two vec3 values (0x10b7d0).
 * Computes: out[i] = (1.0f - blend) * a[i] + blend * b[i]
 * blend=0.0 yields a, blend=1.0 yields b.
 * Called as points_interpolate(this_kf_data, next_kf_data, blend, out)
 * in model_animations.c for keyframe interpolation.
 */
void points_interpolate(float *a, float *b, float blend, float *out)
{
  float inv_blend;
  inv_blend = 1.0f - blend;
  out[0] = inv_blend * a[0] + blend * b[0];
  out[1] = inv_blend * a[1] + blend * b[1];
  out[2] = inv_blend * a[2] + blend * b[2];
}

/* Linearly interpolate between two scalar floats (0x10b820).
 * Computes: *out = b * blend + (1.0f - blend) * a
 * blend=0.0 yields a, blend=1.0 yields b.
 * Called as scalars_interpolate(this_kf, next_kf, blend, out) in
 * model_animations.c for scalar keyframe interpolation (scale channel).
 *
 * Confirmed: cdecl, 4 args, void return. Disassembly at 0x10b820:
 *   FLD [0x2533c8]; FSUB [EBP+0x10]; FMUL [EBP+0x8]; FLD [EBP+0xc];
 *   FMUL [EBP+0x10]; FADDP; FSTP [EAX]
 * Where [0x2533c8] holds 1.0f. Equivalent to b*t + (1-t)*a.
 */
void scalars_interpolate(float a, float b, float blend, float *out)
{
  *out = b * blend + (1.0f - blend) * a;
}

/* 0x10b840 — Interpolate two scalars and clamp result to [0, 1]. */
void scalars_interpolate_and_clamp_0_to_1(float a, float b, float t, float *out)
{
  float result = b * t + (1.0f - t) * a;
  if (result < 0.0f) {
    *out = 0.0f;
    return;
  }
  if (1.0f < result) {
    *out = 1.0f;
    return;
  }
  *out = result;
}

/* 0x10b8a0 — Project a vector onto an axis: parallel = dot(v,axis)*axis,
 * perpendicular = v - parallel. Either output can be NULL. */
void FUN_0010b8a0(float *v, float *axis, float *parallel, float *perpendicular)
{
  float dot;
  float local[3];

  dot = v[0] * axis[0] + axis[2] * v[2] + v[1] * axis[1];
  if (parallel == (float *)0) {
    parallel = local;
  }
  parallel[0] = dot * axis[0];
  parallel[1] = dot * axis[1];
  parallel[2] = dot * axis[2];
  if (perpendicular != (float *)0) {
    perpendicular[0] = v[0] - parallel[0];
    perpendicular[1] = v[1] - parallel[1];
    perpendicular[2] = v[2] - parallel[2];
  }
}

/* 0x10bb20 — Rotate a point by a quaternion: q * v * q_conjugate. */
void quaternion_transform_point(float *q, float *v, float *out)
{
  float ww = q[3] * q[3];
  float ww2_minus_1 = (ww + ww) - 1.0f;
  float dot2 = q[1] * v[1] + q[2] * v[2] + q[0] * v[0];
  float w2;
  float q2, v0, v2, q0, q0b, v1, q1, v0b;

  dot2 = dot2 + dot2;
  w2 = q[3] + q[3];
  q2 = q[2];
  v0 = v[0];
  v2 = v[2];
  q0 = q[0];
  q0b = q[0];
  v1 = v[1];
  q1 = q[1];
  v0b = v[0];
  out[0] = ww2_minus_1 * v[0] + dot2 * q[0] + (q[1] * v[2] - q[2] * v[1]) * w2;
  out[1] = (q2 * v0 - v2 * q0) * w2 + ww2_minus_1 * v[1] + dot2 * q[1];
  out[2] = (q0b * v1 - q1 * v0b) * w2 + ww2_minus_1 * v[2] + dot2 * q[2];
}

/* 0x10bbc0 — Build forward/up vectors from euler angles by computing the
 * rotation matrix and then decomposing it. */
void vectors3d_from_euler_angles3d(float *forward, float *up, float *angles)
{
  float matrix[13];
  float local_position[3];

  if (forward == (float *)0) {
    display_assert("forward", "c:\\halo\\SOURCE\\math\\real_math.c", 0x340, 1);
    system_exit(-1);
  }
  if (up == (float *)0) {
    display_assert("up", "c:\\halo\\SOURCE\\math\\real_math.c", 0x341, 1);
    system_exit(-1);
  }
  if (angles == (float *)0) {
    display_assert("angles", "c:\\halo\\SOURCE\\math\\real_math.c", 0x342, 1);
    system_exit(-1);
  }
  FUN_00109e90(matrix, angles[0], angles[1], angles[2]);
  matrix4x3_decompose(matrix, local_position, forward, up);
}

/* Test whether a line segment intersects a sphere. Returns true if
   the segment origin is inside the sphere or if the segment crosses
   the sphere boundary within t in [0,1]. */
bool fast_vector_intersects_sphere(float *line_start, float *line_end,
                                   float *sphere_center, float sphere_radius)
{
  float dx, dy, dz, c;
  float dir_x, dir_y, dir_z, b;
  float a, disc, t_check;

  dx = line_start[0] - sphere_center[0];
  dy = line_start[1] - sphere_center[1];
  dz = line_start[2] - sphere_center[2];
  c = dx * dx + dy * dy + dz * dz - sphere_radius * sphere_radius;

  if (c < 0.0f)
    return true;

  dir_x = line_end[0];
  dir_y = line_end[1];
  dir_z = line_end[2];
  b = dir_x * dx + dir_y * dy + dir_z * dz;

  if (b >= 0.0f)
    return false;

  a = dir_x * dir_x + dir_y * dir_y + dir_z * dir_z;
  disc = b * b - a * c;

  if (disc <= 0.0f)
    return false;

  t_check = -a - b;
  if (t_check < 0.0f)
    return true;

  if (t_check * t_check < disc)
    return true;

  return false;
}

/* 0x10bd70 — Point-in-rectangle test (2D, fully inclusive). */
int FUN_0010bd70(float *point, float *rect)
{
  if (rect[0] <= point[0] && point[0] <= rect[1] && rect[2] <= point[1] &&
      point[1] <= rect[3]) {
    return 1;
  }
  return 0;
}

/* 0x10bdc0 — Point-in-3D-box test (inclusive on both ends). */
int FUN_0010bdc0(float *point, float *box)
{
  if (box[0] <= point[0] && point[0] <= box[1] && box[2] <= point[1] &&
      point[1] <= box[3] && box[4] <= point[2] && point[2] <= box[5]) {
    return 1;
  }
  return 0;
}

/* 0x10be20 — 2D ray vs AABB intersection (slab method).
 * ray_origin=p1, ray_dir=p2, aabb=p3 (xmin,xmax,ymin,ymax).
 * Returns 1 if ray intersects with t in [0,1]. */
char FUN_0010be20(float *ray_origin, float *ray_dir, float *aabb)
{
  float tmin = -3.4028235e+38f;
  float tmax = 3.4028235e+38f;
  float t1, t2;

  if (fabsf(ray_dir[0]) < *(double *)0x2533d0) {
    if (ray_origin[0] < aabb[0])
      return 0;
    if (ray_origin[0] > aabb[1])
      return 0;
  } else {
    t1 = (aabb[0] - ray_origin[0]) * (1.0f / ray_dir[0]);
    t2 = (aabb[1] - ray_origin[0]) * (1.0f / ray_dir[0]);
    if (ray_dir[0] <= 0.0f) {
      if (t2 > -3.4028235e+38f)
        tmin = t2;
      if (t1 < 3.4028235e+38f)
        tmax = t1;
    } else {
      if (t1 > -3.4028235e+38f)
        tmin = t1;
      if (t2 < 3.4028235e+38f)
        tmax = t2;
    }
    if (tmin > tmax)
      return 0;
  }

  if (fabsf(ray_dir[1]) < *(double *)0x2533d0) {
    if (ray_origin[1] < aabb[2])
      return 0;
    if (ray_origin[1] > aabb[3])
      return 0;
  } else {
    t1 = (aabb[2] - ray_origin[1]) * (1.0f / ray_dir[1]);
    t2 = (aabb[3] - ray_origin[1]) * (1.0f / ray_dir[1]);
    if (ray_dir[1] <= 0.0f) {
      if (tmin < t2)
        tmin = t2;
      if (t1 < tmax)
        tmax = t1;
    } else {
      if (tmin < t1)
        tmin = t1;
      if (t2 < tmax)
        tmax = t2;
    }
    if (tmin > tmax)
      return 0;
  }

  if (0.0f <= tmax && tmin <= 1.0f)
    return 1;
  return 0;
}

/* 0x10bff0 — 3D ray vs AABB intersection (slab method).
 * ray_origin=p1, ray_dir=p2, aabb=p3 (xmin,xmax,ymin,ymax,zmin,zmax). */
char FUN_0010bff0(float *ray_origin, float *ray_dir, float *aabb)
{
  float tmin = -3.4028235e+38f;
  float tmax = 3.4028235e+38f;
  float t1, t2;

  if (fabsf(ray_dir[0]) < *(double *)0x2533d0) {
    if (ray_origin[0] < aabb[0])
      return 0;
    if (ray_origin[0] > aabb[1])
      return 0;
  } else {
    t1 = (aabb[0] - ray_origin[0]) * (1.0f / ray_dir[0]);
    t2 = (aabb[1] - ray_origin[0]) * (1.0f / ray_dir[0]);
    if (ray_dir[0] <= 0.0f) {
      if (t2 > -3.4028235e+38f)
        tmin = t2;
      if (t1 < 3.4028235e+38f)
        tmax = t1;
    } else {
      if (t1 > -3.4028235e+38f)
        tmin = t1;
      if (t2 < 3.4028235e+38f)
        tmax = t2;
    }
    if (tmin > tmax)
      return 0;
  }

  if (fabsf(ray_dir[1]) < *(double *)0x2533d0) {
    if (ray_origin[1] < aabb[2])
      return 0;
    if (ray_origin[1] > aabb[3])
      return 0;
  } else {
    t1 = (aabb[2] - ray_origin[1]) * (1.0f / ray_dir[1]);
    t2 = (aabb[3] - ray_origin[1]) * (1.0f / ray_dir[1]);
    if (ray_dir[1] <= 0.0f) {
      if (tmin < t2)
        tmin = t2;
      if (t1 < tmax)
        tmax = t1;
    } else {
      if (tmin < t1)
        tmin = t1;
      if (t2 < tmax)
        tmax = t2;
    }
    if (tmin > tmax)
      return 0;
  }

  if (fabsf(ray_dir[2]) < *(double *)0x2533d0) {
    if (ray_origin[2] < aabb[4])
      return 0;
    if (ray_origin[2] > aabb[5])
      return 0;
  } else {
    t1 = (aabb[4] - ray_origin[2]) * (1.0f / ray_dir[2]);
    t2 = (aabb[5] - ray_origin[2]) * (1.0f / ray_dir[2]);
    if (ray_dir[2] <= 0.0f) {
      if (tmin < t2)
        tmin = t2;
      if (t1 < tmax)
        tmax = t1;
    } else {
      if (tmin < t1)
        tmin = t1;
      if (t2 < tmax)
        tmax = t2;
    }
    if (tmin > tmax)
      return 0;
  }

  if (0.0f <= tmax && tmin <= 1.0f)
    return 1;
  return 0;
}

float angle_between_normals3d(float *a, float *b)
{
  float dot;
  float sine_term;

  if (*(uint32_t *)&a[0] == *(uint32_t *)&b[0] &&
      *(uint32_t *)&a[1] == *(uint32_t *)&b[1] &&
      *(uint32_t *)&a[2] == *(uint32_t *)&b[2]) {
    return 0.0f;
  }

  dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];

  if (dot < -1.0f) {
    dot = -1.0f;
  }

  if (dot > 1.0f) {
    dot = 1.0f;
  }

  sine_term = sqrtf((1.0f - dot) * (1.0f + dot));
  return (float)atan2((double)sine_term, (double)dot);
}

/* 0x10c690 — Update v1 = scale1 * cross(axis, v1) + scale2 * v1.
 * Uses temporaries for aliasing safety. */
void FUN_0010c690(float *v1, float *axis, float scale1, float scale2)
{
  float a = v1[0];
  float ax2 = axis[2];
  float ax0 = axis[0];
  float b = v1[1];
  float ax0b = axis[0];
  float c = v1[0];
  float ax1 = axis[1];

  v1[0] = (axis[1] * v1[2] - v1[1] * axis[2]) * scale1 + scale2 * v1[0];
  v1[1] = (a * ax2 - ax0 * v1[2]) * scale1 + scale2 * v1[1];
  v1[2] = scale2 * v1[2] + (b * ax0b - c * ax1) * scale1;
}

/* 0x10c700 — Rotate two 3D vectors around an axis: rotate v1 toward v2 and v2
 * away from v1 by (scale1, scale2). Uses temporaries to allow aliasing. */
void FUN_0010c700(float *v1, float *v2, float scale1, float scale2)
{
  float a = v1[0];
  float b = v1[1];
  float c = v1[2];

  v1[0] = scale1 * v2[0] + scale2 * v1[0];
  v1[1] = scale1 * v2[1] + scale2 * v1[1];
  v1[2] = scale1 * v2[2] + scale2 * v1[2];
  v2[0] = -a * scale1 + scale2 * v2[0];
  v2[1] = scale2 * v2[1] + -b * scale1;
  v2[2] = -c * scale1 + scale2 * v2[2];
}

/* Normalize a quaternion [x,y,z,w] in place.
 * If the squared magnitude is > 0, divides each component by the magnitude.
 * Otherwise, resets to the identity quaternion (0,0,0,1). */
void sphere_intersects_rectangle3d(float *quaternion)
{
  float mag_sq;

  mag_sq = quaternion[3] * quaternion[3] + quaternion[2] * quaternion[2] +
           quaternion[1] * quaternion[1] + quaternion[0] * quaternion[0];

  if (mag_sq > 0.0f) {
    float scale;

    scale = 1.0f / sqrtf(mag_sq);
    quaternion[0] = scale * quaternion[0];
    quaternion[1] = scale * quaternion[1];
    quaternion[2] = scale * quaternion[2];
    quaternion[3] = scale * quaternion[3];
  } else {
    quaternion[0] = 0.0f;
    quaternion[1] = 0.0f;
    quaternion[2] = 0.0f;
    quaternion[3] = 1.0f;
  }
}

/* 0x10cab0 — Build a quaternion from an axis and angle.
 * out = (axis * sin(angle/2), cos(angle/2)). */
void FUN_0010cab0(float *out, float angle, float *axis)
{
  float s = x87_fsin(angle * 0.5f);
  float c = x87_fcos(angle * 0.5f);

  out[3] = c;
  out[0] = s * axis[0];
  out[1] = s * axis[1];
  out[2] = s * axis[2];
}

/* Convert a unit quaternion [x,y,z,w] to axis-angle representation.
 * Extracts the rotation axis (normalized) and the angle in radians.
 * If the angle exceeds pi, flips to the shorter equivalent rotation. */
void FUN_0010caf0(float *in_quat4, float *out_angle, float *out_axis3)
{
  float w = in_quat4[3];
  float magnitude;
  float angle;

  out_axis3[0] = in_quat4[0];
  out_axis3[1] = in_quat4[1];
  out_axis3[2] = in_quat4[2];

  magnitude = normalize3d(out_axis3);
  angle = (float)(2.0 * atan2((double)magnitude, (double)w));
  *out_angle = angle;

  if (angle > 3.1415927f) {
    out_axis3[0] = -out_axis3[0];
    out_axis3[1] = -out_axis3[1];
    out_axis3[2] = -out_axis3[2];
    *out_angle = 6.2831855f - angle;
  }
}

/* Interpolate two quaternions via slerp and normalize the result. */
void quaternions_interpolate_and_normalize(float *q1, float *q2, float t,
                                           float *out)
{
  FUN_0010ba90(q1, q2, t, out);
  sphere_intersects_rectangle3d(out);
}

/* Interpolate two orientations (quaternion[4] + scale/translation[4]).
 * Quaternion part is slerped, remaining 4 floats are linearly interpolated. */
void orientations_interpolate(float *orient1, float *orient2, float t,
                              float *out)
{
  float inv_t;

  FUN_0010ba90(orient1, orient2, t, out);
  sphere_intersects_rectangle3d(out);
  inv_t = 1.0f - t;
  out[4] = t * orient2[4] + inv_t * orient1[4];
  out[5] = t * orient2[5] + inv_t * orient1[5];
  out[6] = t * orient2[6] + inv_t * orient1[6];
  out[7] = t * orient2[7] + inv_t * orient1[7];
}

/* Convert a 3D direction vector to yaw/pitch angles (radians). */
void vector_to_angles(float *out_angles, float *in_vector)
{
  float x = in_vector[0];
  float y = in_vector[1];
  float z = in_vector[2];

  out_angles[0] = (float)atan2((double)y, (double)x);
  out_angles[1] = (float)atan2((double)z, (double)sqrtf(y * y + x * x));
}

/* Convert yaw/pitch angles to a unit direction vector.
 * angles[0] = yaw, angles[1] = pitch.
 * out[0] = cos(yaw) * cos(pitch)
 * out[1] = sin(yaw) * cos(pitch)
 * out[2] = sin(pitch) */
void angles_to_vector(float *out, float *angles)
{
  float cos_pitch;

  cos_pitch = x87_fcos(angles[1]);
  out[0] = x87_fcos(angles[0]) * cos_pitch;
  out[1] = x87_fsin(angles[0]) * cos_pitch;
  out[2] = x87_fsin(angles[1]);
}

/* Convert an angle to a 2D direction vector stored as (cos, sin, 0) (0x10cc70).
 */
void vector3d_from_angle(float *out, float angle)
{
  float c = x87_fcos(angle);
  out[2] = 0.0f;
  out[0] = c;
  out[1] = x87_fsin(angle);
}

/* 0x10cc90 — Test if a 2D circle intersects a line segment.
 * p1=point, p2=line start, p3=line direction, r=radius². */
int FUN_0010cc90(float *p1, float *p2, float *p3, float r)
{
  float t = ((p1[1] - p2[1]) * p3[1] + (p1[0] - p2[0]) * p3[0]) /
            (p3[1] * p3[1] + p3[0] * p3[0]);
  float dx, dy;
  float clamped_t = 0.0f;

  if (0.0f <= t) {
    clamped_t = t;
    if (1.0f < t) {
      clamped_t = 1.0f;
    }
  }
  dx = -clamped_t * p3[0] + (p1[0] - p2[0]);
  dy = -clamped_t * p3[1] + (p1[1] - p2[1]);
  if (r * r < dy * dy + dx * dx) {
    return 0;
  }
  return 1;
}

/* 0x10cd40 — Squared distance from a 3D point to a line segment.
 * p1=point, p2=segment start, p3=segment direction.
 * Returns dot(closest - point, closest - point). */
float FUN_0010cd40(float *p1, float *p2, float *p3)
{
  float t_unclamped = ((p1[0] - p2[0]) * p3[0] + (p1[1] - p2[1]) * p3[1] +
                       (p1[2] - p2[2]) * p3[2]) /
                      (p3[2] * p3[2] + p3[1] * p3[1] + p3[0] * p3[0]);
  float t;
  float dx, dy, dz;

  if (0.0f <= t_unclamped) {
    if (t_unclamped <= 1.0f) {
      t = t_unclamped;
    } else {
      t = 1.0f;
    }
  } else {
    t = 0.0f;
  }
  t = -t;
  dx = t * p3[0] + (p1[0] - p2[0]);
  dy = t * p3[1] + (p1[1] - p2[1]);
  dz = t * p3[2] + (p1[2] - p2[2]);
  return dy * dy + dx * dx + dz * dz;
}

/* 0x10ce10 — squared distance between two 3D line segments.
 * p1/p2 = segment A (start, dir); p3/p4 = segment B (start, dir). Returns the
 * squared distance between the closest points of the two segments.
 *   Parallel case (|dir_a x dir_b|^2 < epsilon): closest-point parameters s,t
 *     via projection + clamp-midpoint, then fall through to the distance.
 *   Non-parallel: scalar-triple-product params s,t; if either is out of [0,1]
 *     the closest approach is at an endpoint -> clamp it onto its line and take
 *     the min of point-vs-segment squared distances via FUN_0010cd40 (asserting
 *     at least one is finite); else both in [0,1] (asserted) -> fall through.
 * Constants: 0x2533c0=0.0f, 0x2533c8=1.0f, 0x253398=0.5f, 0x253f44=len eps,
 * 0x2533d0=double parallel eps, 0x2548fc=REAL_MAX (FLT_MAX). */
float vector_to_line_distance_squared3d(float *p1, float *p2, float *p3,
                                        float *p4)
{
  float delta_x, delta_y, delta_z;
  float nx, ny, nz;
  float cross_sq;
  float d0_d1, d0_sq, d1_sq;
  float inv;
  float s, t;
  float s_start, s_end, t_start, t_end;
  float clamped_s, clamped_t;
  float d0, d1;
  float closest_a[3];
  float closest_b[3];
  float diff_x, diff_y, diff_z;
  char s_oob, t_oob;

  delta_x = p3[0] - p1[0];
  delta_y = p3[1] - p1[1];
  delta_z = p3[2] - p1[2];

  /* n = dir_a x dir_b */
  nx = p2[1] * p4[2] - p4[1] * p2[2];
  ny = p2[2] * p4[0] - p2[0] * p4[2];
  nz = p4[1] * p2[0] - p2[1] * p4[0];
  cross_sq = nx * nx + ny * ny + nz * nz;

  if (fabsf(cross_sq) < (float)*(double *)0x2533d0) {
    /* parallel: closest-point parameters via projection + clamp-midpoint */
    d0_d1 = p2[1] * p4[1] + p2[2] * p4[2] + p2[0] * p4[0];
    d0_sq = p2[1] * p2[1] + p2[2] * p2[2] + p2[0] * p2[0];
    if (d0_sq <= *(float *)0x253f44) {
      s = *(float *)0x2533c0;
    } else {
      inv = *(float *)0x2533c8 / d0_sq;
      s_start = (delta_x * p2[0] + delta_y * p2[1] + delta_z * p2[2]) * inv;
      s_end = inv * d0_d1 + s_start;
      clamped_s = *(float *)0x2533c0;
      if (*(float *)0x2533c0 <= s_start) {
        clamped_s = s_start;
        if (*(float *)0x2533c8 < s_start)
          clamped_s = *(float *)0x2533c8;
      }
      if (*(float *)0x2533c0 <= s_end) {
        if (s_end <= *(float *)0x2533c8)
          s = (s_end + clamped_s) * *(float *)0x253398;
        else
          s = (*(float *)0x2533c8 + clamped_s) * *(float *)0x253398;
      } else {
        s = (*(float *)0x2533c0 + clamped_s) * *(float *)0x253398;
      }
    }
    d1_sq = p4[2] * p4[2] + p4[1] * p4[1] + p4[0] * p4[0];
    if (d1_sq <= *(float *)0x253f44) {
      t = *(float *)0x2533c0;
    } else {
      inv = *(float *)0x2533c8 / d1_sq;
      t_start = -((delta_z * p4[2] + delta_x * p4[0] + delta_y * p4[1]) * inv);
      t_end = inv * d0_d1 + t_start;
      clamped_t = *(float *)0x2533c0;
      if (*(float *)0x2533c0 <= t_start) {
        clamped_t = t_start;
        if (*(float *)0x2533c8 < t_start)
          clamped_t = *(float *)0x2533c8;
      }
      if (*(float *)0x2533c0 <= t_end) {
        if (t_end <= *(float *)0x2533c8)
          t = (t_end + clamped_t) * *(float *)0x253398;
        else
          t = (*(float *)0x2533c8 + clamped_t) * *(float *)0x253398;
      } else {
        t = (*(float *)0x2533c0 + clamped_t) * *(float *)0x253398;
      }
    }
    /* fall through to closest-point distance */
  } else {
    inv = *(float *)0x2533c8 / cross_sq;
    s = (delta_y * p4[2] - delta_z * p4[1]) * nx * inv +
        (delta_z * p4[0] - delta_x * p4[2]) * ny * inv +
        (delta_x * p4[1] - delta_y * p4[0]) * nz * inv;
    t = (delta_y * p2[2] - delta_z * p2[1]) * nx * inv +
        (delta_z * p2[0] - delta_x * p2[2]) * ny * inv +
        (delta_x * p2[1] - delta_y * p2[0]) * nz * inv;

    s_oob = (s < *(float *)0x2533c0 || *(float *)0x2533c8 < s);
    t_oob = (t < *(float *)0x2533c0 || *(float *)0x2533c8 < t);

    if (s_oob || t_oob) {
      d0 = 3.4028235e+38f; /* REAL_MAX */
      d1 = 3.4028235e+38f;
      if (s_oob) {
        clamped_s = *(float *)0x2533c8;
        if (s < *(float *)0x2533c0)
          clamped_s = *(float *)0x2533c0;
        closest_a[0] = clamped_s * p2[0] + p1[0];
        closest_a[1] = clamped_s * p2[1] + p1[1];
        closest_a[2] = clamped_s * p2[2] + p1[2];
        d0 = FUN_0010cd40(closest_a, p3, p4);
      }
      if (t_oob) {
        clamped_t = *(float *)0x2533c8;
        if (t < *(float *)0x2533c0)
          clamped_t = *(float *)0x2533c0;
        closest_b[0] = clamped_t * p4[0] + p3[0];
        closest_b[1] = clamped_t * p4[1] + p3[1];
        closest_b[2] = clamped_t * p4[2] + p3[2];
        /* 2nd arg is start_a (p1): decompile shows extraout_EDX, but
         * FUN_0010cd40 preserves EDX which still holds p1 from entry. */
        d1 = FUN_0010cd40(closest_b, p1, p2);
      }
      if (*(float *)0x2548fc <= d0 && *(float *)0x2548fc <= d1) {
        display_assert("(d0 < REAL_MAX) || (d1 < REAL_MAX)",
                       "c:\\halo\\SOURCE\\math\\real_math.c", 0x3ae, 1);
        system_exit(-1);
      }
      if (d0 <= d1)
        return d0;
      return d1;
    }
  }

  /* both params in [0, 1] (parallel clamped, or non-parallel in-range):
   * sanity-assert then return the closest-point squared distance */
  if (s < *(float *)0x2533c0 || *(float *)0x2533c8 < s) {
    display_assert("(t0 >= 0.0f) && (t0 <= 1.0f)",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x3da, 1);
    system_exit(-1);
  }
  if (t < *(float *)0x2533c0 || *(float *)0x2533c8 < t) {
    display_assert("(t1 >= 0.0f) && (t1 <= 1.0f)",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x3db, 1);
    system_exit(-1);
  }
  diff_x = (t * p4[0] + p3[0]) - (s * p2[0] + p1[0]);
  diff_y = (t * p4[1] + p3[1]) - (s * p2[1] + p1[1]);
  diff_z = (t * p4[2] + p3[2]) - (s * p2[2] + p1[2]);
  return diff_x * diff_x + diff_y * diff_y + diff_z * diff_z;
}

/* 0x10d380 — Ray-sphere intersection.
 * p1=ray_origin, p2=sphere_radius, p3=sphere_center, p4=ray_direction.
 * Returns 1 if intersect, sets *out_t and *out_normal. */
char FUN_0010d380(float *p1, float p2, float *p3, float *p4, float *out_t,
                  float *out_normal)
{
  float dx, dy, dz;
  float dot;
  float dist_sq, c;
  float a, disc;
  void (*normalize3d_in_place)(float *) = (void (*)(float *))FUN_0010c2e0;

  dx = p3[0] - p1[0];
  dy = p3[1] - p1[1];
  dz = p3[2] - p1[2];
  dot = dx * p4[0] + dy * p4[1] + dz * p4[2];
  if (dot < 0.0f) {
    dist_sq = dy * dy + dx * dx + dz * dz;
    c = dist_sq - p2 * p2;
    if (c <= 0.0f) {
      *out_t = 0.0f;
      a = 1.0f / sqrtf(dist_sq);
      out_normal[0] = dx * a;
      out_normal[1] = dy * a;
      out_normal[2] = dz * a;
      return 1;
    }
    a = p4[2] * p4[2] + p4[1] * p4[1] + p4[0] * p4[0];
    disc = dot * dot - a * c;
    if (0.0f <= disc) {
      dot = -(sqrtf(disc) + dot);
      if (dot < a) {
        *out_t = dot / a;
        vector3d_scale_add(&dx, p4, dot / a, out_normal);
        normalize3d_in_place(out_normal);
        return 1;
      }
    }
  }
  return 0;
}

/* 0x10d4c0 — Ray vs cylinder intersection (cylinder along z-axis).
 * p1=ray_origin, p2=cylinder_height, p3=cylinder_radius, p4=cylinder_center,
 * p5=ray_direction, p6=out_t, p7=out_normal. */
char FUN_0010d4c0(float *p1, float p2, float p3, float *p4, float *p5,
                  float *p6, float *p7)
{
  float dx, dy;
  float bp, c2;
  float a, t;
  char hit;
  float local_10;

  dx = p4[0] - p1[0];
  dy = p4[1] - p1[1];
  bp = dy * p5[1] + dx * p5[0];
  c2 = (dx * dx + dy * dy) - p3 * p3;
  t = 0.0f;
  if (c2 <= 0.0f) {
    a = p5[1] * p5[1] + p5[0] * p5[0];
    c2 = bp * bp - a * c2;
    if (c2 < 0.0f)
      return 0;
    c2 = -(sqrtf(c2) + bp);
    if (c2 <= a)
      return 0;
    t = c2 / a;
  }
  hit = 1;
  c2 = (t * p5[2] + (p4[2] - p1[2])) / (p2 * p2);
  if (0.0f <= c2) {
    if (c2 <= 1.0f) {
      if (0.0f <= bp)
        return 0;
      *p6 = t;
      p7[0] = t * p5[0] + dx;
      p7[1] = t * p5[1] + dy;
      FUN_0010c290(p7);
      p7[2] = 0.0f;
      goto check;
    }
    local_10 = p1[0];
    dy = p2 + p1[2];
    dx = p1[1];
    {
      float local_origin[3];
      local_origin[0] = local_10;
      local_origin[1] = dx;
      local_origin[2] = dy;
      hit = FUN_0010d380(local_origin, p3, p4, p5, p6, p7);
    }
  } else {
    hit = FUN_0010d380(p1, p3, p4, p5, p6, p7);
  }
  if (hit == 0)
    return 0;
check:
  if (p5[0] * p7[0] + p7[1] * p5[1] + p7[2] * p5[2] <= 0.0f) {
    return hit;
  }
  return 0;
}

/* 0x10d680 — Ray vs sphere distance.
 * Returns t along ray where ray hits sphere, or REAL_MAX if no hit.
 * Returns 0 if ray origin is inside sphere. */
float FUN_0010d680(float *ray_origin, float *ray_dir, float *sphere_center,
                   float radius)
{
  float dx = ray_origin[0] - sphere_center[0];
  float dy = ray_origin[1] - sphere_center[1];
  float dz = ray_origin[2] - sphere_center[2];
  float c = (dx * dx + dy * dy + dz * dz) - radius * radius;
  float dr_x, dr_y, dr_z;
  float b, a, disc;

  if (c < 0.0f) {
    return 0.0f;
  }
  dr_x = ray_dir[0];
  dr_y = ray_dir[1];
  dr_z = ray_dir[2];
  b = dr_x * dx + dr_y * dy + dr_z * dz;
  if (b < 0.0f) {
    a = dr_x * dr_x + dr_y * dr_y + dr_z * dr_z;
    disc = b * b - a * c;
    if (disc >= 0.0f) {
      return (-b - sqrtf(disc)) / a;
    }
  }
  return 3.4028235e+38f;
}

/* 0x10d770 — 2D triangle barycentric coordinates.
 * Tests if point p1 is inside triangle (p2, p3, p4) and computes
 * barycentric coords. Returns 1 if inside, 0 if outside.
 * Returns char to match MSVC's MOV AL,0x1 (preserves upper EAX bytes). */
char FUN_0010d770(float *p1, float *p2, float *p3, float *p4, float *out_u,
                  float *out_v)
{
  float det =
    (p1[1] - p2[1]) * (p3[0] - p2[0]) - (p1[0] - p2[0]) * (p3[1] - p2[1]);
  float det2, total, inv_total;

  if (0.0f <= det) {
    det2 =
      (p1[0] - p2[0]) * (p4[1] - p2[1]) - (p1[1] - p2[1]) * (p4[0] - p2[0]);
    if (0.0f <= det2) {
      total =
        (p4[1] - p2[1]) * (p3[0] - p2[0]) - (p4[0] - p2[0]) * (p3[1] - p2[1]);
      if (det2 + det <= total) {
        inv_total = 1.0f / total;
        *out_u = det2 * inv_total;
        *out_v = inv_total * det;
        return 1;
      }
    }
  }
  return 0;
}

/* 0x10d830 — 3D triangle barycentric coordinates: project to dominant
 * axis and compute barycentric (u,v). p1=point, p2,p3,p4=triangle vertices. */
char FUN_0010d830(float *p1, float *p2, float *p3, float *p4, float *out_u,
                  float *out_v)
{
  float v3[3], v1[3], v2[3];
  float n[3];
  float dot_n;
  uint32_t basis;
  uint8_t axis;
  float p1_proj[2];
  float v1_proj[2];
  float v2_proj[2];
  float det, det2, total, inv_total;

  v3[0] = p1[0] - p2[0];
  v3[1] = p1[1] - p2[1];
  v3[2] = p1[2] - p2[2];
  v1[0] = p3[0] - p2[0];
  v1[1] = p3[1] - p2[1];
  v1[2] = p3[2] - p2[2];
  v2[0] = p4[0] - p2[0];
  v2[1] = p4[1] - p2[1];
  v2[2] = p4[2] - p2[2];
  n[0] = v2[2] * v1[1] - v1[2] * v2[1];
  n[1] = v1[2] * v2[0] - v2[2] * v1[0];
  n[2] = v2[1] * v1[0] - v1[1] * v2[0];
  dot_n = n[0] * v3[0] + n[1] * v3[1] + n[2] * v3[2];
  if (dot_n * dot_n <
      (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) * (*(float *)0x253f44)) {
    basis = FUN_00099220(n);
    axis = FUN_00099270(n, basis);
    FUN_00061df0(v1, basis, axis, p1_proj);
    FUN_00061df0(v3, basis, axis, v1_proj);
    det = v1_proj[1] * p1_proj[0] - p1_proj[1] * v1_proj[0];
    if (det >= 0.0f) {
      FUN_00061df0(v2, basis, axis, v2_proj);
      det2 = v2_proj[1] * v1_proj[0] - v1_proj[1] * v2_proj[0];
      if (det2 >= 0.0f) {
        total = v2_proj[1] * p1_proj[0] - p1_proj[1] * v2_proj[0];
        if (det2 + det <= total) {
          inv_total = 1.0f / total;
          *out_u = det2 * inv_total;
          *out_v = inv_total * det;
          return 1;
        }
      }
    }
  }
  return 0;
}

/* 0x10d9e0 — 2D point in cone test.
 * Returns 1 if point in 2D cone (apex=p2, axis=p3, max length=cone_radius,
 * half-angle whose cos=cosine). */
char FUN_0010d9e0(float *p1, float *p2, float *p3, float cone_radius,
                  float cosine)
{
  float dx, dy;
  float dot;
  float rsq, dist_sq;

  if (cosine < 0.0f) {
    display_assert("cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c", 0x55f,
                   1);
    system_exit(-1);
  }
  dx = p1[0] - p2[0];
  dy = p1[1] - p2[1];
  dot = dx * p3[0] + dy * p3[1];
  if (dot < 0.0f) {
    return 0;
  }
  if (dot > cone_radius) {
    return 0;
  }
  rsq = dot * dot;
  dist_sq = (dy * dy + dx * dx) * cosine * cosine;
  if (dist_sq < rsq) {
    return 0;
  }
  return 1;
}

/* 0x10da90 — 3D cone vs sphere test.
 * Tests if sphere(center=p1, radius=cone_radius*cosine) is intersected
 * by the cone with apex=p2, axis=p3, half-angle whose cos=cosine. */
char FUN_0010da90(float *p1, float *p2, float *p3, float cone_radius,
                  float cosine)
{
  float dx, dy, dz;
  float dot;
  float rsq, dist_sq;

  if (cosine < 0.0f) {
    display_assert("cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c", 0x57c,
                   1);
    system_exit(-1);
  }
  dx = p1[0] - p2[0];
  dy = p1[1] - p2[1];
  dz = p1[2] - p2[2];
  dot = dx * p3[0] + dy * p3[1] + dz * p3[2];
  if (dot < 0.0f) {
    return 0;
  }
  if (dot > cone_radius) {
    return 0;
  }
  rsq = dot * dot;
  dist_sq = (dy * dy + dx * dx + dz * dz) * cosine * cosine;
  if (dist_sq < rsq) {
    return 0;
  }
  return 1;
}

/* 0x10db50 — 2D cone vs circle test. */
char FUN_0010db50(float *p1, float *p2, float *p3, float cone_radius,
                  float cosine)
{
  float dx, dy;
  float dist_sq;
  float dot;
  float radius_sq;

  if (cosine < 0.0f) {
    display_assert("cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c", 0x599,
                   1);
    system_exit(-1);
  }
  dx = p1[0] - p2[0];
  dy = p1[1] - p2[1];
  dist_sq = dy * dy + dx * dx;
  radius_sq = cone_radius * cone_radius;
  if (radius_sq < dist_sq) {
    return 0;
  }
  dot = dx * p3[0] + dy * p3[1];
  if (dot < 0.0f) {
    return 0;
  }
  if (dist_sq * cosine * cosine < dot * dot) {
    return 0;
  }
  return 1;
}

/* 0x10dbf0 — 3D cone vs sphere test (variant). */
char FUN_0010dbf0(float *p1, float *p2, float *p3, float cone_radius,
                  float cosine)
{
  float dx, dy, dz;
  float dist_sq;
  float dot;
  float radius_sq;

  if (cosine < 0.0f) {
    display_assert("cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c", 0x5b6,
                   1);
    system_exit(-1);
  }
  dx = p1[0] - p2[0];
  dy = p1[1] - p2[1];
  dz = p1[2] - p2[2];
  dist_sq = dy * dy + dx * dx + dz * dz;
  radius_sq = cone_radius * cone_radius;
  if (radius_sq < dist_sq) {
    return 0;
  }
  dot = dx * p3[0] + dy * p3[1] + dz * p3[2];
  if (dot < 0.0f) {
    return 0;
  }
  if (dist_sq * cosine * cosine < dot * dot) {
    return 0;
  }
  return 1;
}

/* 0x10dcb0 — 2D segment vs pill (capsule) intersection.
 * line_start/line_dir = segment A; pill_center/pill_dir = pill spine segment B;
 * pill_radius = capsule radius. Returns 1 if segment A comes within pill_radius
 * of the pill spine, else 0. 2D analog of the 3D pill-pill test above.
 *   Parallel case (|cross| < epsilon): closest-point parameters s,t via
 *     projection + clamp-midpoint, then one distance check.
 *   Non-parallel case: 2D line-intersection params s,t; if both in [0,1] the
 *     segments cross; otherwise clamp the out-of-range endpoint onto its line
 *     and delegate to FUN_0010cc90 (point vs segment within radius).
 * Constants: 0x2533c0=0.0f, 0x2533c8=1.0f, 0x253398=0.5f, 0x253f44=len epsilon,
 * 0x2533d0=double cross-parallel epsilon. */
char vector_intersects_pill2d(float *line_start, float *line_dir,
                              float *pill_center, float *pill_dir,
                              float pill_radius)
{
  float delta_x, delta_y;
  float cross2d;
  float d0_d1, d0_sq, d1_sq;
  float inv;
  float s, t;
  float s_start, s_end, t_start, t_end;
  float clamped_s, clamped_t;
  float closest_a_x, closest_a_y;
  float closest_b_x, closest_b_y;
  float diff_x, diff_y;
  char s_oob, t_oob;

  delta_x = pill_center[0] - line_start[0];
  delta_y = pill_center[1] - line_start[1];

  /* 2D cross of the two directions */
  cross2d = line_dir[0] * pill_dir[1] - line_dir[1] * pill_dir[0];

  if (fabsf(cross2d) < (float)*(double *)0x2533d0) {
    /* parallel or nearly parallel lines */
    d0_d1 = line_dir[0] * pill_dir[0] + line_dir[1] * pill_dir[1];
    d0_sq = line_dir[0] * line_dir[0] + line_dir[1] * line_dir[1];

    if (d0_sq <= *(float *)0x253f44) {
      s = *(float *)0x2533c0;
    } else {
      inv = *(float *)0x2533c8 / d0_sq;
      s_start = (delta_y * line_dir[1] + delta_x * line_dir[0]) * inv;
      s_end = inv * d0_d1 + s_start;

      /* clamp s_start to [0, 1] (decompile branch shape: default 0, take
       * value if >=0, cap at 1 if >1 — comparison directions preserved) */
      clamped_s = *(float *)0x2533c0;
      if (*(float *)0x2533c0 <= s_start) {
        clamped_s = s_start;
        if (*(float *)0x2533c8 < s_start)
          clamped_s = *(float *)0x2533c8;
      }

      /* clamp s_end to [0, 1], average with clamped s_start */
      if (*(float *)0x2533c0 <= s_end) {
        if (s_end <= *(float *)0x2533c8)
          s = (s_end + clamped_s) * *(float *)0x253398;
        else
          s = (*(float *)0x2533c8 + clamped_s) * *(float *)0x253398;
      } else {
        s = (*(float *)0x2533c0 + clamped_s) * *(float *)0x253398;
      }
    }

    d1_sq = pill_dir[1] * pill_dir[1] + pill_dir[0] * pill_dir[0];

    if (d1_sq <= *(float *)0x253f44) {
      t = *(float *)0x2533c0;
    } else {
      inv = *(float *)0x2533c8 / d1_sq;
      t_start = -((delta_y * pill_dir[1] + delta_x * pill_dir[0]) * inv);
      t_end = inv * d0_d1 + t_start;

      /* clamp t_start to [0, 1] */
      clamped_t = *(float *)0x2533c0;
      if (*(float *)0x2533c0 <= t_start) {
        clamped_t = t_start;
        if (*(float *)0x2533c8 < t_start)
          clamped_t = *(float *)0x2533c8;
      }

      /* clamp t_end to [0, 1], average with clamped t_start */
      if (*(float *)0x2533c0 <= t_end) {
        if (t_end <= *(float *)0x2533c8)
          t = (t_end + clamped_t) * *(float *)0x253398;
        else
          t = (*(float *)0x2533c8 + clamped_t) * *(float *)0x253398;
      } else {
        t = (*(float *)0x2533c0 + clamped_t) * *(float *)0x253398;
      }
    }

    closest_a_x = s * line_dir[0] + line_start[0];
    closest_a_y = s * line_dir[1] + line_start[1];
    closest_b_x = t * pill_dir[0] + pill_center[0];
    closest_b_y = t * pill_dir[1] + pill_center[1];
    diff_x = closest_b_x - closest_a_x;
    diff_y = closest_b_y - closest_a_y;
    if (pill_radius * pill_radius < diff_x * diff_x + diff_y * diff_y)
      return 0;
    return 1;
  }

  /* non-parallel: solve the 2D line intersection */
  inv = *(float *)0x2533c8 / cross2d;
  s = (delta_x * pill_dir[1] - delta_y * pill_dir[0]) * inv;
  t = (delta_x * line_dir[1] - delta_y * line_dir[0]) * inv;

  s_oob = (s < *(float *)0x2533c0 || *(float *)0x2533c8 < s);
  t_oob = (t < *(float *)0x2533c0 || *(float *)0x2533c8 < t);

  if (s_oob) {
    clamped_s = *(float *)0x2533c8;
    if (s < *(float *)0x2533c0)
      clamped_s = *(float *)0x2533c0;
    closest_a_x = clamped_s * line_dir[0] + line_start[0];
    closest_a_y = clamped_s * line_dir[1] + line_start[1];
    if (!t_oob)
      goto point_segment_checks;
  } else if (!t_oob) {
    return 1;
  }

  clamped_t = *(float *)0x2533c8;
  if (t < *(float *)0x2533c0)
    clamped_t = *(float *)0x2533c0;
  closest_b_x = clamped_t * pill_dir[0] + pill_center[0];
  closest_b_y = clamped_t * pill_dir[1] + pill_center[1];

point_segment_checks:
  if ((!s_oob ||
       FUN_0010cc90(&closest_a_x, pill_center, pill_dir, pill_radius) == 0) &&
      (!t_oob ||
       FUN_0010cc90(&closest_b_x, line_start, line_dir, pill_radius) == 0))
    return 0;
  return 1;
}

/* vector_intersects_pill3d (0x10e040) — Test if two line segments are within a
 * given radius. Computes closest points between segments A (start_a + s*dir_a,
 * s in [0,1]) and B (start_b + t*dir_b, t in [0,1]). Returns true if distance <
 * radius. */
bool vector_intersects_pill3d(float *start_a, float *dir_a, float *start_b,
                              float *dir_b, float radius)
{
  float delta_x, delta_y, delta_z;
  float nx, ny, nz;
  float cross_sq;
  float s, t;
  float inv;
  float d0_d1, d0_sq, d1_sq;
  float s_start, s_end, t_start, t_end;
  float clamped_s, clamped_t;
  float closest_a_x, closest_a_y, closest_a_z;
  float closest_b_x, closest_b_y, closest_b_z;
  float diff_x, diff_y, diff_z;
  char s_oob, t_oob;

  delta_x = start_b[0] - start_a[0];
  delta_y = start_b[1] - start_a[1];
  delta_z = start_b[2] - start_a[2];

  /* cross = dir_a × dir_b */
  nx = dir_a[1] * dir_b[2] - dir_b[1] * dir_a[2];
  ny = dir_a[2] * dir_b[0] - dir_a[0] * dir_b[2];
  nz = dir_b[1] * dir_a[0] - dir_a[1] * dir_b[0];

  cross_sq = nx * nx + ny * ny + nz * nz;

  if (fabsf(cross_sq) < (float)*(double *)0x2533d0) {
    /* parallel or nearly parallel lines */
    d0_d1 = dir_a[0] * dir_b[0] + dir_a[1] * dir_b[1] + dir_a[2] * dir_b[2];
    d0_sq = dir_a[0] * dir_a[0] + dir_a[1] * dir_a[1] + dir_a[2] * dir_a[2];

    if (d0_sq <= *(float *)0x253f44) {
      s = 0.0f;
    } else {
      inv = *(float *)0x2533c8 / d0_sq;
      s_start =
        (delta_x * dir_a[0] + delta_y * dir_a[1] + delta_z * dir_a[2]) * inv;
      s_end = inv * d0_d1 + s_start;

      /* clamp s_start to [0, 1] */
      if (s_start < *(float *)0x2533c0)
        clamped_s = *(float *)0x2533c0;
      else if (s_start > *(float *)0x2533c8)
        clamped_s = *(float *)0x2533c8;
      else
        clamped_s = s_start;

      /* clamp s_end to [0, 1] */
      if (s_end < *(float *)0x2533c0)
        s = (*(float *)0x2533c0 + clamped_s) * *(float *)0x253398;
      else if (s_end > *(float *)0x2533c8)
        s = (*(float *)0x2533c8 + clamped_s) * *(float *)0x253398;
      else
        s = (s_end + clamped_s) * *(float *)0x253398;
    }

    d1_sq = dir_b[0] * dir_b[0] + dir_b[1] * dir_b[1] + dir_b[2] * dir_b[2];

    if (d1_sq <= *(float *)0x253f44) {
      t = *(float *)0x2533c0;
    } else {
      inv = *(float *)0x2533c8 / d1_sq;
      t_start =
        -((delta_x * dir_b[0] + delta_y * dir_b[1] + delta_z * dir_b[2]) * inv);
      t_end = inv * d0_d1 + t_start;

      /* clamp t_start to [0, 1] */
      if (t_start < *(float *)0x2533c0)
        clamped_t = *(float *)0x2533c0;
      else if (t_start > *(float *)0x2533c8)
        clamped_t = *(float *)0x2533c8;
      else
        clamped_t = t_start;

      /* clamp t_end to [0, 1] */
      if (t_end < *(float *)0x2533c0)
        t = (*(float *)0x2533c0 + clamped_t) * *(float *)0x253398;
      else if (t_end > *(float *)0x2533c8)
        t = (*(float *)0x2533c8 + clamped_t) * *(float *)0x253398;
      else
        t = (t_end + clamped_t) * *(float *)0x253398;
    }

    goto distance_check;
  }

  /* non-parallel case */
  inv = *(float *)0x2533c8 / cross_sq;
  nx = nx * inv;
  ny = ny * inv;

  /* s = dot(delta × dir_b, n_norm) */
  s = (delta_y * dir_b[2] - delta_z * dir_b[1]) * nx +
      (delta_z * dir_b[0] - delta_x * dir_b[2]) * ny +
      (delta_x * dir_b[1] - delta_y * dir_b[0]) * nz * inv;

  /* t = dot(delta × dir_a, n_norm) */
  t = (delta_y * dir_a[2] - delta_z * dir_a[1]) * nx +
      (delta_z * dir_a[0] - delta_x * dir_a[2]) * ny +
      (delta_x * dir_a[1] - delta_y * dir_a[0]) * nz * inv;

  /* check if s is out of [0, 1] */
  s_oob = (s < *(float *)0x2533c0 || s > *(float *)0x2533c8);
  /* check if t is out of [0, 1] */
  t_oob = (t < *(float *)0x2533c0 || t > *(float *)0x2533c8);

  if (s_oob) {
    clamped_s =
      (s < *(float *)0x2533c0) ? *(float *)0x2533c0 : *(float *)0x2533c8;
    closest_a_x = clamped_s * dir_a[0] + start_a[0];
    closest_a_y = clamped_s * dir_a[1] + start_a[1];
    closest_a_z = clamped_s * dir_a[2] + start_a[2];
    if (!t_oob)
      goto point_segment_checks;
  } else if (!t_oob) {
    goto distance_check;
  }

  clamped_t =
    (t < *(float *)0x2533c0) ? *(float *)0x2533c0 : *(float *)0x2533c8;
  closest_b_x = clamped_t * dir_b[0] + start_b[0];
  closest_b_y = clamped_t * dir_b[1] + start_b[1];
  closest_b_z = clamped_t * dir_b[2] + start_b[2];

point_segment_checks:
  if (s_oob) {
    if (fast_vector_intersects_sphere(start_b, dir_b, &closest_a_x, radius))
      return 1;
  }
  if (t_oob) {
    if (fast_vector_intersects_sphere(start_a, dir_a, &closest_b_x, radius))
      return 1;
  }
  return 0;

distance_check:
  closest_a_x = s * dir_a[0] + start_a[0];
  closest_a_y = s * dir_a[1] + start_a[1];
  closest_a_z = s * dir_a[2] + start_a[2];
  closest_b_x = t * dir_b[0] + start_b[0];
  closest_b_y = t * dir_b[1] + start_b[1];
  closest_b_z = t * dir_b[2] + start_b[2];

  diff_x = closest_b_x - closest_a_x;
  diff_y = closest_b_y - closest_a_y;
  diff_z = closest_b_z - closest_a_z;

  if (radius * radius < diff_x * diff_x + diff_y * diff_y + diff_z * diff_z)
    return 0;
  return 1;
}

/* 0x10e4d0 — 2D ray vs triangle intersection (Liang-Barsky interval clip).
 * Checks whether the parametric ray point + t*dir (t in [0,1]) passes through
 * the 2D triangle (v0,v1,v2).  For each edge the function clips the t-interval
 * using the sign of the edge normal relative to dir, then returns 1 if the
 * surviving window is non-empty.
 *
 * point : 2-element position array [x,y]
 * dir   : 2-element direction array [x,y]
 * v0,v1,v2 : triangle vertices, each 2-element [x,y]
 * Returns 1 if intersection, 0 otherwise. */
char FUN_0010e4d0(float *point, float *dir, float *v0, float *v1, float *v2)
{
    float tmin, tmax;
    float e_x, e_y, p_x, p_y, num, den, t;

    tmin = 0.0f;
    tmax = 1.0f;

    /* Edge 1: v0 -> v1 */
    p_x = point[0] - v0[0];
    p_y = point[1] - v0[1];
    e_x = v1[0] - v0[0];
    e_y = v1[1] - v0[1];
    num = p_x * e_y - e_x * p_y;
    den = e_x * dir[1] - e_y * dir[0];
    if (fabs(den) < *(double *)0x2533d0) {
        if (num > 0.0f)
            return 0;
    } else {
        t = num / den;
        if (den > 0.0f) {
            if (t > 0.0f) {
                tmin = t;
                if (tmin > tmax)
                    return 0;
            }
        } else {
            if (t < 1.0f) {
                tmax = t;
                if (tmin > tmax)
                    return 0;
            }
        }
    }

    /* Edge 2: v1 -> v2 */
    p_x = point[0] - v1[0];
    p_y = point[1] - v1[1];
    e_x = v2[0] - v1[0];
    e_y = v2[1] - v1[1];
    num = p_x * e_y - e_x * p_y;
    den = e_x * dir[1] - e_y * dir[0];
    if (fabs(den) < *(double *)0x2533d0) {
        if (num > 0.0f)
            return 0;
    } else {
        t = num / den;
        if (den > 0.0f) {
            if (t > tmin) {
                tmin = t;
            }
        } else {
            if (t < tmax) {
                tmax = t;
            }
        }
        if (tmin > tmax)
            return 0;
    }

    /* Edge 3: v2 -> v0 */
    p_x = point[0] - v2[0];
    p_y = point[1] - v2[1];
    e_x = v0[0] - v2[0];
    e_y = v0[1] - v2[1];
    num = p_x * e_y - e_x * p_y;
    den = e_x * dir[1] - e_y * dir[0];
    if (fabs(den) < *(double *)0x2533d0) {
        if (num > 0.0f)
            return 0;
    } else {
        t = num / den;
        if (den > 0.0f) {
            if (t > tmin) {
                tmin = t;
            }
        } else {
            if (t < tmax) {
                tmax = t;
            }
        }
        if (tmin > tmax)
            return 0;
    }

    return 1;
}

/* 0x10e6f0 — 3D ray vs triangle intersection (Möller–Trumbore-like).
 * p1=ray_origin, p2=ray_direction, p3,p4,p5=triangle vertices,
 * p6=out_t. Returns 1 if ray hits triangle. */
char FUN_0010e6f0(float *p1, float *p2, float *p3, float *p4, float *p5,
                  float *p6)
{
  float e1[3], e2[3];
  float n[3];
  float det, inv_det;
  float origin_to_v0[3];
  float t, u, v;
  float scratch[3];

  e1[0] = p4[0] - p3[0];
  e1[1] = p4[1] - p3[1];
  e1[2] = p4[2] - p3[2];
  e2[0] = p5[0] - p3[0];
  e2[1] = p5[1] - p3[1];
  e2[2] = p5[2] - p3[2];
  n[0] = e2[2] * e1[1] - e2[1] * e1[2];
  n[1] = e1[2] * e2[0] - e2[2] * e1[0];
  n[2] = e2[1] * e1[0] - e2[0] * e1[1];
  det = n[0] * p2[0] + n[1] * p2[1] + n[2] * p2[2];

  if (fabsf(det) < *(double *)0x2533d0) {
    return 0;
  }

  inv_det = 1.0f / det;
  origin_to_v0[0] = p3[0] - p1[0];
  origin_to_v0[1] = p3[1] - p1[1];
  origin_to_v0[2] = p3[2] - p1[2];
  t =
    (origin_to_v0[0] * n[0] + origin_to_v0[1] * n[1] + origin_to_v0[2] * n[2]) *
    inv_det;
  if (t < 0.0f)
    return 0;
  if (t > 1.0f)
    return 0;

  cross_product3d(origin_to_v0, p2, scratch);
  u = (e2[0] * scratch[0] + scratch[1] * e2[1] + scratch[2] * e2[2]) * inv_det;
  if (u < 0.0f)
    return 0;
  if (u > 1.0f)
    return 0;
  v =
    -((e1[0] * scratch[0] + scratch[1] * e1[1] + scratch[2] * e1[2]) * inv_det);
  if (v < 0.0f)
    return 0;
  if (u + v > 1.0f)
    return 0;
  *p6 = t;
  return 1;
}

/* 0x10e8a0 — 2D point-to-rectangle distance test.
 * Returns 1 if point is within radius of rectangle. */
char FUN_0010e8a0(float *point, float radius, float *rect)
{
  float dx, dy;

  if (point[0] > rect[1]) {
    dx = point[0] - rect[1];
  } else {
    dx = 0.0f;
    if (point[0] < rect[0]) {
      dx = rect[0] - point[0];
    }
  }
  if (point[1] > rect[3]) {
    dy = point[1] - rect[3];
  } else {
    dy = 0.0f;
    if (point[1] < rect[2]) {
      dy = rect[2] - point[1];
    }
  }
  if (radius * radius < dx * dx + dy * dy) {
    return 0;
  }
  return 1;
}

/* 0x10e930 — 3D point-to-AABB distance test.
 * Returns 1 if point is within radius of AABB.
 * Layout: rect[0..1]=x bounds, rect[2..3]=y bounds, rect[4..5]=z bounds. */
char FUN_0010e930(float *point, float radius, float *aabb)
{
  float dx, dy, dz;

  if (point[0] > aabb[1]) {
    dx = point[0] - aabb[1];
  } else {
    dx = 0.0f;
    if (point[0] < aabb[0]) {
      dx = aabb[0] - point[0];
    }
  }
  if (point[1] > aabb[3]) {
    dy = point[1] - aabb[3];
  } else {
    dy = 0.0f;
    if (point[1] < aabb[2]) {
      dy = aabb[2] - point[1];
    }
  }
  if (point[2] > aabb[5]) {
    dz = point[2] - aabb[5];
  } else {
    dz = 0.0f;
    if (point[2] < aabb[4]) {
      dz = aabb[4] - point[2];
    }
  }
  if (radius * radius <= dx * dx + dy * dy + dz * dz) {
    return 0;
  }
  return 1;
}

/* 0x10e9f0 — 2D triangle vs circle test. Tests the 3 edges (cw winding)
 * with FUN_0010cc90 (point-segment intersect). Returns 1 if any edge or
 * point intersects. */
char FUN_0010e9f0(float *circle_center, float radius, float *p3, float *p4,
                  float *p5)
{
  float local_c, local_8;

  char result = 1;
  local_c = p4[0] - p3[0];
  local_8 = p4[1] - p3[1];
  if (0.0f < local_8 * (circle_center[0] - p3[0]) -
               local_c * (circle_center[1] - p3[1])) {
    if (FUN_0010cc90(circle_center, p3, &local_c, radius)) {
      return 1;
    }
    result = 0;
  }
  local_c = p5[0] - p4[0];
  local_8 = p5[1] - p4[1];
  if (local_8 * (circle_center[0] - p4[0]) -
        local_c * (circle_center[1] - p4[1]) <
      0.0f) {
    if (FUN_0010cc90(circle_center, p4, &local_c, radius)) {
      return 1;
    }
    result = 0;
  }
  local_c = p3[0] - p5[0];
  local_8 = p3[1] - p5[1];
  if (0.0f < local_8 * (circle_center[0] - p5[0]) -
               local_c * (circle_center[1] - p5[1])) {
    if (FUN_0010cc90(circle_center, p5, &local_c, radius)) {
      return 1;
    }
    result = 0;
  }
  return result;
}

/* Test if a sphere intersects a 3D triangle. Checks plane distance, then
 * tests each edge if the projected point falls outside that edge's half-plane.
 */
char sphere_intersects_triangle3d(float *center, float radius, float *v0,
                                  float *v1, float *v2)
{
  float d0[3], e01[3], e12[3], n[3];
  float c12[3], c20[3];
  float plane_dot, n_mag_sq;
  char result;

  d0[0] = center[0] - v0[0];
  result = 1;
  d0[1] = center[1] - v0[1];
  d0[2] = center[2] - v0[2];

  e01[0] = v1[0] - v0[0];
  e01[1] = v1[1] - v0[1];
  e01[2] = v1[2] - v0[2];

  e12[0] = v2[0] - v1[0];
  e12[1] = v2[1] - v1[1];
  e12[2] = v2[2] - v1[2];

  n[0] = e12[2] * e01[1] - e12[1] * e01[2];
  n[1] = e01[2] * e12[0] - e12[2] * e01[0];
  n[2] = e12[1] * e01[0] - e01[1] * e12[0];

  plane_dot = n[0] * d0[0] + n[1] * d0[1] + n[2] * d0[2];
  n_mag_sq = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
  if (n_mag_sq * radius * radius < plane_dot * plane_dot)
    return 0;

  if (0.0f < (e01[1] * d0[2] - d0[1] * e01[2]) * n[0] +
               (d0[0] * e01[2] - e01[0] * d0[2]) * n[1] +
               n[2] * (e01[0] * d0[1] - e01[1] * d0[0])) {
    if (fast_vector_intersects_sphere(v0, e01, center, radius))
      return 1;
    result = 0;
  }

  c12[0] = e12[2] * (center[1] - v1[1]) - e12[1] * (center[2] - v1[2]);
  c12[1] = (center[2] - v1[2]) * e12[0] - e12[2] * (center[0] - v1[0]);
  if (0.0f <
      c12[0] * n[0] + c12[1] * n[1] +
        n[2] * (e12[1] * (center[0] - v1[0]) - e12[0] * (center[1] - v1[1]))) {
    if (fast_vector_intersects_sphere(v1, e12, center, radius))
      return 1;
    result = 0;
  }

  c20[0] = v0[0] - v2[0];
  c20[1] = v0[1] - v2[1];
  c20[2] = v0[2] - v2[2];
  e12[0] = c20[2] * (center[1] - v2[1]) - c20[1] * (center[2] - v2[2]);
  e12[1] = (center[2] - v2[2]) * c20[0] - c20[2] * (center[0] - v2[0]);
  if (e12[0] * n[0] + e12[1] * n[1] +
        n[2] * (c20[1] * (center[0] - v2[0]) - c20[0] * (center[1] - v2[1])) <
      0.0f) {
    if (fast_vector_intersects_sphere(v2, c20, center, radius))
      return 1;
    result = 0;
  }
  return result;
}

/* Test if a 2D pill intersects an axis-aligned rectangle.
 * rect = {x0, x1, y0, y1}. Tests each edge the pill center is outside of. */
char pill_intersects_rectangle2d(float *pill_center, float *pill_dir,
                                 float pill_radius, float *rect)
{
  float edge_start[2], edge_dir[2];
  char result;

  result = 1;
  if (pill_center[0] < rect[0]) {
    edge_start[0] = rect[0];
    edge_start[1] = rect[2];
    edge_dir[0] = 0.0f;
    edge_dir[1] = rect[3] - rect[2];
    if (vector_intersects_pill2d(edge_start, edge_dir, pill_center, pill_dir,
                                 pill_radius))
      return 1;
    result = 0;
  }
  if (pill_center[1] < rect[2]) {
    edge_start[0] = rect[0];
    edge_start[1] = rect[2];
    edge_dir[0] = rect[1] - rect[0];
    edge_dir[1] = 0.0f;
    if (vector_intersects_pill2d(edge_start, edge_dir, pill_center, pill_dir,
                                 pill_radius))
      return 1;
    result = 0;
  }
  if (rect[1] < pill_center[0]) {
    edge_start[0] = rect[1];
    edge_start[1] = rect[2];
    edge_dir[0] = 0.0f;
    edge_dir[1] = rect[3] - rect[2];
    if (vector_intersects_pill2d(edge_start, edge_dir, pill_center, pill_dir,
                                 pill_radius))
      return 1;
    result = 0;
  }
  if (rect[3] < pill_center[1]) {
    edge_start[0] = rect[0];
    edge_start[1] = rect[3];
    edge_dir[0] = rect[1] - rect[0];
    edge_dir[1] = 0.0f;
    if (vector_intersects_pill2d(edge_start, edge_dir, pill_center, pill_dir,
                                 pill_radius))
      return 1;
    result = 0;
  }
  return result;
}

/* Test if a 2D pill intersects a triangle (3 vertices, 2D).
 * Tests each edge where the pill center is outside the edge half-plane. */
char pill_intersects_triangle2d(float *pill_center, float *pill_dir,
                                float pill_radius, float *v0, float *v1,
                                float *v2)
{
  float edge_dir[2];
  char result;

  result = 1;
  edge_dir[0] = v1[0] - v0[0];
  edge_dir[1] = v1[1] - v0[1];
  if (0.0f < edge_dir[1] * (pill_center[0] - v0[0]) -
               edge_dir[0] * (pill_center[1] - v0[1])) {
    if (vector_intersects_pill2d(v0, edge_dir, pill_center, pill_dir,
                                 pill_radius))
      return 1;
    result = 0;
  }

  edge_dir[0] = v2[0] - v1[0];
  edge_dir[1] = v2[1] - v1[1];
  if (0.0f < edge_dir[1] * (pill_center[0] - v1[0]) -
               edge_dir[0] * (pill_center[1] - v1[1])) {
    if (vector_intersects_pill2d(v1, edge_dir, pill_center, pill_dir,
                                 pill_radius))
      return 1;
    result = 0;
  }

  edge_dir[0] = v0[0] - v2[0];
  edge_dir[1] = v0[1] - v2[1];
  if (0.0f < edge_dir[1] * (pill_center[0] - v2[0]) -
               edge_dir[0] * (pill_center[1] - v2[1])) {
    if (vector_intersects_pill2d(v2, edge_dir, pill_center, pill_dir,
                                 pill_radius))
      return 1;
    result = 0;
  }
  return result;
}

/* Test if a 3D pill (capsule) intersects a triangle.
 * Projects pill onto triangle plane, tests edges for outside half-planes. */
int pill_intersects_triangle3d(float *pill_start, float *pill_dir,
                               float pill_radius, float *v0, float *v1,
                               float *v2)
{
  float e01[3], e12[3], n[3];
  float e20[3], cp[3];
  float proj[3];
  float t, closest_x, closest_y;
  char outside;

  e01[0] = v1[0] - v0[0];
  outside = 0;
  e01[1] = v1[1] - v0[1];
  e01[2] = v1[2] - v0[2];
  e12[0] = v2[0] - v1[0];
  e12[1] = v2[1] - v1[1];
  e12[2] = v2[2] - v1[2];

  n[0] = e12[2] * e01[1] - e12[1] * e01[2];
  n[1] = e01[2] * e12[0] - e12[2] * e01[0];
  n[2] = e12[1] * e01[0] - e01[1] * e12[0];

  t = (n[0] * (v0[0] - pill_start[0]) + n[1] * (v0[1] - pill_start[1]) +
       n[2] * (v0[2] - pill_start[2])) /
      (n[1] * pill_dir[1] + n[2] * pill_dir[2] + n[0] * pill_dir[0]);

  if (t < 0.0f) {
    closest_x = 0.0f;
  } else if (t > 1.0f) {
    closest_x = 1.0f;
  } else {
    closest_x = t;
  }
  proj[0] = closest_x * pill_dir[0] + pill_start[0];
  proj[1] = closest_x * pill_dir[1] + pill_start[1];
  proj[2] = closest_x * pill_dir[2] + pill_start[2];

  if ((e01[1] * (proj[2] - v0[2]) - (proj[1] - v0[1]) * e01[2]) * n[0] +
        (e01[2] * (proj[0] - v0[0]) - (proj[2] - v0[2]) * e01[0]) * n[1] +
        n[2] * ((proj[1] - v0[1]) * e01[0] - (proj[0] - v0[0]) * e01[1]) <
      0.0f) {
    if (vector_intersects_pill3d(v0, e01, pill_start, pill_dir, pill_radius))
      return 1;
    outside = 1;
  }

  if (outside == 0) {
    if ((e12[1] * (proj[2] - v1[2]) - (proj[1] - v1[1]) * e12[2]) * n[0] +
          (e12[2] * (proj[0] - v1[0]) - (proj[2] - v1[2]) * e12[0]) * n[1] +
          n[2] * ((proj[1] - v1[1]) * e12[0] - e12[1] * (proj[0] - v1[0])) <
        0.0f) {
      if (vector_intersects_pill3d(v1, e12, pill_start, pill_dir, pill_radius))
        return 1;
      outside = 1;
    }
  } else {
    if (vector_intersects_pill3d(v1, e12, pill_start, pill_dir, pill_radius))
      return 1;
  }

  e20[0] = v0[0] - v2[0];
  e20[1] = v0[1] - v2[1];
  e20[2] = v0[2] - v2[2];
  cp[0] = proj[0] - v2[0];
  cp[1] = proj[1] - v2[1];
  cp[2] = proj[2] - v2[2];

  if (0.0f <= (e20[1] * cp[2] - e20[2] * cp[1]) * n[0] +
                (e20[2] * cp[0] - e20[0] * cp[2]) * n[1] +
                n[2] * (e20[0] * cp[1] - e20[1] * cp[0])) {
    if (outside == 0) {
      if (0.0f < t && t < 1.0f)
        return 1;
      closest_y = n[0] * cp[0] + n[1] * cp[1] + n[2] * cp[2];
      if (closest_y * closest_y <=
          (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) * pill_radius * pill_radius)
        return 1;
      return 0;
    }
  } else {
    if (vector_intersects_pill3d(v2, e20, pill_start, pill_dir, pill_radius))
      return 1;
  }
  return 0;
}

/* 0x10f310 — Intersect three planes (homogeneous): solve for the point
 * that lies on all three planes. Returns 0 if planes are parallel/degenerate.
 */
char FUN_0010f310(float *p1, float *p2, float *p3, float *out)
{
  float det;
  float d0;

  det = (p2[2] * p1[1] - p1[2] * p2[1]) * p3[0] +
        (p1[2] * p2[0] - p1[0] * p2[2]) * p3[1] +
        (p1[0] * p2[1] - p2[0] * p1[1]) * p3[2];
  if (fabsf(det) < *(double *)0x2533d0) {
    return 0;
  }

  d0 = p1[3];
  out[0] = (p3[2] * p2[1] - p3[1] * p2[2]) * d0;
  out[1] = (p2[2] * p3[0] - p3[2] * p2[0]) * d0;
  out[2] = (p3[1] * p2[0] - p3[0] * p2[1]) * d0;

  d0 = p2[3];
  out[0] = (p1[2] * p3[1] - p3[2] * p1[1]) * d0 + out[0];
  out[1] = (p3[2] * p1[0] - p1[2] * p3[0]) * d0 + out[1];
  out[2] = (p3[0] * p1[1] - p3[1] * p1[0]) * d0 + out[2];

  d0 = p3[3];
  det = 1.0f / det;
  out[0] = ((p2[2] * p1[1] - p1[2] * p2[1]) * d0 + out[0]) * det;
  out[1] = ((p1[2] * p2[0] - p1[0] * p2[2]) * d0 + out[1]) * det;
  out[2] = ((p1[0] * p2[1] - p2[0] * p1[1]) * d0 + out[2]) * det;
  return 1;
}

/* 0x10f480 — Intersect a line (described by two planes p1, p2) with a third
 * plane to get the point. param_3 receives the line-direction cross,
 * param_4 receives the cross of the input planes. */
char FUN_0010f480(float *p1, float *p2, float *out, float *cross_out)
{
  float cy, cz;
  float det, inv_det;
  float d;

  cross_out[2] = p1[0] * p2[1] - p1[1] * p2[0];
  cy = p2[0] * p1[2] - p1[0] * p2[2];
  cz = p1[1] * p2[2] - p1[2] * p2[1];
  cross_out[0] = cz;
  cross_out[1] = cy;
  det = cross_out[2] * cross_out[2] + cy * cy + cz * cz;
  if (fabsf(det) < *(double *)0x2533d0) {
    return 0;
  }

  d = p1[3];
  out[0] = (cross_out[2] * p2[1] - cy * p2[2]) * d;
  out[1] = (p2[2] * cross_out[0] - cross_out[2] * p2[0]) * d;
  out[2] = (cy * p2[0] - cross_out[0] * p2[1]) * d;

  d = p2[3];
  inv_det = 1.0f / det;
  out[0] =
    ((cross_out[1] * p1[2] - p1[1] * cross_out[2]) * d + out[0]) * inv_det;
  out[1] =
    ((p1[0] * cross_out[2] - p1[2] * cross_out[0]) * d + out[1]) * inv_det;
  out[2] =
    ((cross_out[0] * p1[1] - cross_out[1] * p1[0]) * d + out[2]) * inv_det;
  return 1;
}

/* 0x10f5b0 — accelerate_to_position: advance a 1D value (*pos) and its rate
 * (*vel) toward `target` under acceleration `accel`, capped at speed
 * `max_speed`. Uses a critically-damped approach: when the braking speed
 * sqrt(2*accel*|delta|) is below max_speed it is used instead, so the value
 * decelerates to rest exactly at the target. The result is always clamped to
 * the bounds [wrap_min, wrap_max]. When `wrap_flag` is set the axis is periodic
 * (e.g. an angle): the shortest signed delta is taken and the integrated
 * position is wrapped back into one period before clamping. Writes new *pos and
 * *vel. Returns 1 when within reach this step (snapped to target/bound, *vel
 * zeroed), 0 while still accelerating.
 * Constants: 0x2533c0 = 0.0f, 0x253398 = 0.5f. */
char accelerate_to_position(float *pos, float *vel, float target, float accel,
                            float max_speed, float wrap_min, float wrap_max,
                            char wrap_flag)
{
  float cur_pos;
  float cur_vel;
  float delta;
  float half_range;
  float limit;
  float speed;
  float brake_dist;
  float step;
  float step_clamped;
  float new_pos;
  float out_pos;
  float max_speed_sq;
  char result;

  cur_pos = *pos;
  cur_vel = *vel;
  max_speed_sq = max_speed * max_speed;
  result = 0;
  delta = target - cur_pos;

  if (wrap_flag != '\0') {
    half_range = (wrap_max - wrap_min) * *(float *)0x253398;
    if (delta <= half_range) {
      if (delta < -half_range)
        delta = (half_range + half_range) + delta;
    } else {
      delta = delta - (half_range + half_range);
    }
  }

  limit = accel;
  if (max_speed < accel)
    limit = max_speed;

  if (limit < fabsf(delta - cur_vel)) {
    /* still accelerating toward target */
    brake_dist = (accel + accel) * fabsf(delta);
    speed = max_speed;
    if (brake_dist < max_speed_sq)
      speed = sqrtf(brake_dist);
    if (delta < *(float *)0x2533c0)
      speed = -speed;
    step = speed - cur_vel;
    step_clamped = step;
    if (accel < fabsf(step)) {
      step_clamped = accel;
      if (step < *(float *)0x2533c0)
        step_clamped = -accel;
    }
    cur_vel = cur_vel + step_clamped;
    new_pos = step_clamped * *(float *)0x253398 + cur_vel + cur_pos;
    if (wrap_flag != '\0') {
      if (new_pos < wrap_min)
        new_pos = (wrap_max - wrap_min) + new_pos;
      else if (wrap_max < new_pos)
        new_pos = new_pos - (wrap_max - wrap_min);
    }
    if (new_pos >= wrap_min) {
      out_pos = new_pos;
      if (wrap_max < new_pos)
        out_pos = wrap_max;
      *vel = cur_vel;
      *pos = out_pos;
      return result;
    }
    *vel = cur_vel;
    *pos = wrap_min;
    return result;
  }

  /* within reach: snap to target, zero velocity */
  if (target >= wrap_min) {
    out_pos = target;
    if (wrap_max < target)
      out_pos = wrap_max;
    *vel = *(float *)0x2533c0;
    *pos = out_pos;
    result = 1;
    return result;
  }
  *vel = *(float *)0x2533c0;
  *pos = wrap_min;
  result = 1;
  return result;
}

/* Move point param_1 toward point param_2 by at most max_length (vector clamp).
   Computes delta = param_2 - param_1, asks FUN_000a57b0 to clamp its length to
   max_length; if it clamped (returns nonzero), advances param_1 by the clamped
   delta and returns 0 (not arrived); otherwise snaps param_1 to param_2 and
   returns 1 (arrived). */
char FUN_0010f9b0(float *param_1, float *param_2, float max_length)
{
  float delta[3];

  delta[0] = param_2[0] - param_1[0];
  delta[1] = param_2[1] - param_1[1];
  delta[2] = param_2[2] - param_1[2];
  if (FUN_000a57b0(delta, max_length) != 0) {
    param_1[0] = delta[0] + param_1[0];
    param_1[1] = delta[1] + param_1[1];
    param_1[2] = delta[2] + param_1[2];
    return 0;
  }
  param_1[0] = param_2[0];
  param_1[1] = param_2[1];
  param_1[2] = param_2[2];
  return 1;
}

/* Quantize a float to a byte, finding the largest byte whose dequantized
 * value is <= the input value (lower bound). */
unsigned char quantize_real_to_byte_lower_bound(float min, float max,
                                                float value)
{
  float range;
  unsigned char test;
  float dequant;

  range = max - min;
  test = (unsigned char)(int)((value - min) / range * 255.0f);

  if (!(min - *(float *)0x253f44 < value) || max + *(float *)0x253f44 < value) {
    csprintf((char *)0x5ab100, "%lf is not between %lf and %lf", (double)value,
             (double)min, (double)max);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\math\\real_math.c",
                   0xaf3, 1);
    system_exit(-1);
  }

  for (; test != 0; test--) {
    if (test != 0xff)
      dequant = (float)test * (1.0f / 255.0f) * range + min;
    else
      dequant = max;
    if (dequant <= value)
      break;
  }

  if (test != 0xff)
    dequant = (float)test * (1.0f / 255.0f) * range + min;
  else
    dequant = max;
  if (!(dequant <= value) &&
      !(test == 0 && range * 0.0f + min <= value + *(float *)0x253f44)) {
    display_assert("dequantize_byte_to_real(min, max, test)<=value || "
                   "(test==0 && dequantize_byte_to_real(min, max, test)"
                   "<=value+_real_epsilon)",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0xaf9, 1);
    system_exit(-1);
  }
  return test;
}

/* Quantize a float to a byte, finding the smallest byte whose dequantized
 * value is >= the input value (upper bound). */
unsigned char quantize_real_to_byte_upper_bound(float min, float max,
                                                float value)
{
  unsigned char test;
  float dequant;

  test = (unsigned char)(int)((value - min) / (max - min) * 255.0f);

  if (!(min - *(float *)0x253f44 < value) || max + *(float *)0x253f44 < value) {
    csprintf((char *)0x5ab100, "%lf is not between %lf and %lf", (double)value,
             (double)min, (double)max);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\math\\real_math.c",
                   0xb07, 1);
    system_exit(-1);
  }

  if (test != 0xff) {
    dequant = max;
    if (test != 0xff) {
      dequant = (float)test * (1.0f / 255.0f) * (max - min) + min;
    }
    while (dequant < value && (test = test + 1, test != 0xff)) {
      dequant = (float)test * (1.0f / 255.0f) * (max - min) + min;
    }
  }

  if (test != 0xff)
    dequant = (float)test * (1.0f / 255.0f) * (max - min) + min;
  else
    dequant = max;
  if (dequant < value) {
    if (test == 0xff && value - *(float *)0x253f44 < max)
      return 0xff;
    display_assert(
      "dequantize_byte_to_real(min, max, test)>=value || "
      "(test==UNSIGNED_CHAR_MAX && dequantize_byte_to_real(min, max, test)"
      ">=value-_real_epsilon)",
      "c:\\halo\\SOURCE\\math\\real_math.c", 0xb0d, 1);
    system_exit(-1);
  }
  return test;
}

/* Quantize a 3D rectangle (6 floats: x0,x1,y0,y1,z0,z1) to 6 bytes. */
unsigned char *quantize_real_to_byte_rectangle3d(float *bounds, int *rect,
                                                 unsigned char *out)
{
  if (*rect == 0x7f7fffff) {
    if (rect[1] != (int)0xff7fffff || rect[2] != 0x7f7fffff ||
        rect[3] != (int)0xff7fffff || rect[4] != 0x7f7fffff ||
        rect[5] != (int)0xff7fffff) {
      display_assert("rectangle->x1==REAL_MIN && rectangle->y0==REAL_MAX && "
                     "rectangle->y1==REAL_MIN && rectangle->z0==REAL_MAX && "
                     "rectangle->z1==REAL_MIN",
                     "c:\\halo\\SOURCE\\math\\real_math.c", 0xb1b, 1);
      system_exit(-1);
    }
    csmemset(out, 0, 6);
    return out;
  }
  out[0] =
    quantize_real_to_byte_lower_bound(bounds[0], bounds[1], *(float *)&rect[0]);
  out[1] =
    quantize_real_to_byte_upper_bound(bounds[0], bounds[1], *(float *)&rect[1]);
  out[2] =
    quantize_real_to_byte_lower_bound(bounds[2], bounds[3], *(float *)&rect[2]);
  out[3] =
    quantize_real_to_byte_upper_bound(bounds[2], bounds[3], *(float *)&rect[3]);
  out[4] =
    quantize_real_to_byte_lower_bound(bounds[4], bounds[5], *(float *)&rect[4]);
  out[5] =
    quantize_real_to_byte_upper_bound(bounds[4], bounds[5], *(float *)&rect[5]);
  return out;
}

/* 0x10fe80 — Validate 2D normal: x²+y² close to 1.0 and not NaN/Inf. */
int FUN_0010fe80(float x, float y)
{
  float diff = (y * y + x * x) - 1.0f;
  if ((*(unsigned int *)&diff & 0x7f800000) != 0x7f800000 &&
      fabsf(diff) < *(double *)0x2549d8) {
    return 1;
  }
  return 0;
}

/* Pin a normal vector to the boundary of a cone if it falls outside.
 * Returns 1 if the normal was pinned, 0 if it was already inside the cone. */
char pin_normal_to_cone3d(float *normal, float *direction, float sin_half_angle,
                          float cos_half_angle, float *result)
{
  float axis[3];
  float dot;
  float mag;

  if (!valid_real_normal3d(normal)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "normal", (double)normal[0], (double)normal[1], (double)normal[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\math\\real_math.c",
                   0x203, 1);
    system_exit(-1);
  }
  if (!valid_real_normal3d(direction)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "direction", (double)direction[0], (double)direction[1],
             (double)direction[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\math\\real_math.c",
                   0x204, 1);
    system_exit(-1);
  }

  dot = normal[0] * direction[0] + normal[1] * direction[1] +
        normal[2] * direction[2];

  if (dot >= cos_half_angle) {
    result[0] = normal[0];
    result[1] = normal[1];
    result[2] = normal[2];
    return 0;
  }

  axis[0] = normal[2] * direction[1] - normal[1] * direction[2];
  axis[1] = normal[0] * direction[2] - direction[0] * normal[2];
  axis[2] = direction[0] * normal[1] - normal[0] * direction[1];
  mag = normalize3d(axis);
  if (mag == 0.0f) {
    perpendicular3d(direction, axis);
    normalize3d(axis);
  }

  result[0] = direction[0];
  result[1] = direction[1];
  result[2] = direction[2];
  rotate_vector3d_by_sincos(result, axis, sin_half_angle, cos_half_angle);

  if (!valid_real_normal3d(result)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "result", (double)result[0], (double)result[1], (double)result[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\math\\real_math.c",
                   0x21c, 1);
    system_exit(-1);
  }
  return 1;
}

/* 0x1100c0 — 2D capsule-cone intersection. Tests if a 2D capsule
 * (point=p1, radius=p2, half-axis=p4*p5) is intersected by a cone
 * (apex=p3, axis=p4, length cap, half-angle cos=p7, sin=p6). */
char FUN_001100c0(float *p1, float p2, float *p3, float *p4, float p5,
                  float sine, float cosine)
{
  float dx, dy;
  float dot;
  float far_lim;
  float dist_sq;

  if (sine <= 0.0f || cosine < 0.0f) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_sine_cosine(%f, %f)",
             "sine>0.0f && cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c",
             0x877, 1);
    display_assert("sine>0.0f && cosine>=0.0f",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x877, 1);
    system_exit(-1);
  }
  {
    float diff = (cosine * cosine + sine * sine) - 1.0f;
    if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000 ||
        *(double *)0x2549d8 <= fabsf(diff)) {
      csprintf((char *)0x5ab100,
               "%s, %s: assert_valid_real_sine_cosine(%f, %f)", "sine",
               "cosine", "c:\\halo\\SOURCE\\math\\real_math.c", 0x878, 1);
      display_assert("...", "c:\\halo\\SOURCE\\math\\real_math.c", 0x878, 1);
      system_exit(-1);
    }
  }
  dx = p1[0] - p3[0];
  dy = p1[1] - p3[1];
  dot = dx * p4[0] + dy * p4[1];
  far_lim = -p2;
  if (far_lim < dot) {
    far_lim = p2 + p5;
    if (far_lim >= dot) {
      dist_sq = p2 * p2 + (p2 * sine + p2 * sine + dot) * dot;
      cosine = (dx * dx + dy * dy) * cosine * cosine;
      if (cosine < dist_sq) {
        return 1;
      }
    }
  }
  return 0;
}

/* 0x110210 — 3D capsule-cone intersection (variant of 0x1100c0). */
char FUN_00110210(float *p1, float p2, float *p3, float *p4, float p5,
                  float sine, float cosine)
{
  float dx, dy, dz;
  float dot;
  float far_lim;
  float dist_sq;

  if (sine <= 0.0f || cosine < 0.0f) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_sine_cosine(%f, %f)",
             "sine>0.0f && cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c",
             0x897, 1);
    display_assert("sine>0.0f && cosine>=0.0f",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x897, 1);
    system_exit(-1);
  }
  {
    float diff = (cosine * cosine + sine * sine) - 1.0f;
    if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000 ||
        *(double *)0x2549d8 <= fabsf(diff)) {
      csprintf((char *)0x5ab100,
               "%s, %s: assert_valid_real_sine_cosine(%f, %f)", "sine",
               "cosine", "c:\\halo\\SOURCE\\math\\real_math.c", 0x898, 1);
      display_assert("...", "c:\\halo\\SOURCE\\math\\real_math.c", 0x898, 1);
      system_exit(-1);
    }
  }
  dx = p1[0] - p3[0];
  dy = p1[1] - p3[1];
  dz = p1[2] - p3[2];
  dot = dx * p4[0] + dy * p4[1] + dz * p4[2];
  far_lim = -p2;
  if (far_lim < dot) {
    far_lim = p2 + p5;
    if (far_lim >= dot) {
      dist_sq = p2 * p2 + (p2 * sine + p2 * sine + dot) * dot;
      cosine = (dx * dx + dy * dy + dz * dz) * cosine * cosine;
      if (cosine < dist_sq) {
        return 1;
      }
    }
  }
  return 0;
}

/* 0x110380 — 2D capsule vs cone test (calls FUN_0010db50 for cone test). */
char FUN_00110380(float *p1, float p2, float *p3, float *p4, float p5,
                  float sine, float cosine)
{
  float dot;

  if (sine <= 0.0f || cosine < 0.0f) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_sine_cosine(%f, %f)",
             "sine>0.0f && cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c",
             0x8b7, 1);
    display_assert("sine>0.0f && cosine>=0.0f",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x8b7, 1);
    system_exit(-1);
  }
  {
    float diff = (cosine * cosine + sine * sine) - 1.0f;
    if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000 ||
        *(double *)0x2549d8 <= fabsf(diff)) {
      csprintf((char *)0x5ab100,
               "%s, %s: assert_valid_real_sine_cosine(%f, %f)", "sine",
               "cosine", "c:\\halo\\SOURCE\\math\\real_math.c", 0x8b8, 1);
      display_assert("...", "c:\\halo\\SOURCE\\math\\real_math.c", 0x8b8, 1);
      system_exit(-1);
    }
  }
  dot = (p1[1] - p3[1]) * p4[1] + (p1[0] - p3[0]) * p4[0];
  if (-p2 <= dot) {
    if (dot <= p2 + p5 &&
        FUN_0010db50(p1, p3, p4, p2 + p5 - dot + p2, cosine)) {
      return 1;
    }
  }
  return 0;
}

/* 0x1104e0 — 3D capsule vs cone test (calls FUN_0010dbf0 for cone test). */
char FUN_001104e0(float *p1, float p2, float *p3, float *p4, float p5,
                  float sine, float cosine)
{
  float dot;

  if (sine <= 0.0f || cosine < 0.0f) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_sine_cosine(%f, %f)",
             "sine>0.0f && cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c",
             0x8dd, 1);
    display_assert("sine>0.0f && cosine>=0.0f",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x8dd, 1);
    system_exit(-1);
  }
  {
    float diff = (cosine * cosine + sine * sine) - 1.0f;
    if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000 ||
        *(double *)0x2549d8 <= fabsf(diff)) {
      csprintf((char *)0x5ab100,
               "%s, %s: assert_valid_real_sine_cosine(%f, %f)", "sine",
               "cosine", "c:\\halo\\SOURCE\\math\\real_math.c", 0x8de, 1);
      display_assert("...", "c:\\halo\\SOURCE\\math\\real_math.c", 0x8de, 1);
      system_exit(-1);
    }
  }
  dot =
    (p1[1] - p3[1]) * p4[1] + (p1[2] - p3[2]) * p4[2] + (p1[0] - p3[0]) * p4[0];
  if (-p2 <= dot) {
    if (dot <= p2 + p5 &&
        FUN_0010dbf0(p1, p3, p4, p2 + p5 - dot + p2, cosine)) {
      return 1;
    }
  }
  return 0;
}

/* Initialize a vector tree structure (k-d tree for spatial lookups). */
void FUN_00110730(int *param_1, short param_2, int param_3, int param_4,
                  int param_5)
{
  if (param_1 == (int *)0) {
    display_assert("tree", "c:\\halo\\SOURCE\\math\\vector_tree.c", 0x2b, 1);
    system_exit(-1);
  }
  if (param_4 == 0) {
    display_assert("get_vector", "c:\\halo\\SOURCE\\math\\vector_tree.c", 0x2c,
                   1);
    system_exit(-1);
  }
  if (param_5 == 0) {
    display_assert("compare_component", "c:\\halo\\SOURCE\\math\\vector_tree.c",
                   0x2d, 1);
    system_exit(-1);
  }
  if (param_2 < 1) {
    display_assert("component_count>0", "c:\\halo\\SOURCE\\math\\vector_tree.c",
                   0x2e, 1);
    system_exit(-1);
  }
  array_new(param_1 + 1, 0x10);
  *(short *)(param_1 + 4) = param_2;
  param_1[7] = param_5;
  *param_1 = -1;
  param_1[5] = param_3;
  param_1[6] = param_4;
}

/* Dispose a vector tree (free the backing table). */
void FUN_00110800(int param_1)
{
  FUN_00117cf0((int *)(param_1 + 4));
}

/*
 * 0x1108b0 — vector_tree BST search/insert.
 *
 * Traverses a ternary BST built on an array_new pool. Each node holds a
 * stored value and three child indices (left/equal/right), all initialised to
 * -1. The tree struct layout (int[]):
 *   [0]  root index (-1 = empty)
 *   [1+] node pool (passed to FUN_00117da0/FUN_00117ee0 as &tree[1])
 *   [2]  free-list head  (written into the target slot before FUN_00117da0)
 *   [4]  max component count (short, compared with (short) casts)
 *   [5]  user_data passed to the two callbacks
 *   [6]  fn ptr: key_of(user_data, node_value) -> key
 *   [7]  fn ptr: compare(user_data, vector, key, count) -> <0/0/>0
 *
 * Control flow:
 *   - Traverse until an empty slot (-1): allocate a new node there, return 0.
 *   - compare < 0  -> follow left  child (node+1)
 *   - compare > 0  -> follow right child (node+3)
 *   - compare == 0 -> increment persistent count, re-compare with new count
 *                     until non-zero or count reaches max: return 1 (found).
 *                     If re-compare non-zero, follow equal/mid child (node+2).
 *
 * Returns 1 (found), 0 (inserted or alloc failed). *out_node receives the
 * node pointer on found, or the new node pointer on insert, or 0 on alloc fail.
 *
 * The persistent outer count lives in [EBP+8] (the reused param_1 slot zeroed
 * at entry). The inner duplicate-scan increments a separate EBX scan variable
 * without writing back, so it does not corrupt the outer count.
 */
char FUN_001108b0(int *tree, int vector, int *out_node)
{
    int *slot;
    int *node;
    int key;
    int cmp;
    int count;
    int scan;
    int new_node;

    if (tree == (int *)0) {
        display_assert("tree", "c:\\halo\\SOURCE\\math\\vector_tree.c", 0x4a,
                       1);
        system_exit(-1);
    }
    if (vector == 0) {
        display_assert("vector", "c:\\halo\\SOURCE\\math\\vector_tree.c", 0x4b,
                       1);
        system_exit(-1);
    }
    if (out_node == (int *)0) {
        display_assert("index_reference",
                       "c:\\halo\\SOURCE\\math\\vector_tree.c", 0x4c, 1);
        system_exit(-1);
    }

    count = 0;
    slot = tree; /* initially points at tree[0] = root index field */

    do {
        if (*slot == -1) {
            /* Empty slot: insert here. Write free-list head into slot, then
               allocate a node from the pool. */
            *slot = tree[2];
            new_node = FUN_00117da0(&tree[1]);
            if (new_node != -1) {
                new_node = FUN_00117ee0(&tree[1], new_node, 0x10);
                ((int *)new_node)[1] = -1;
                ((int *)new_node)[2] = -1;
                ((int *)new_node)[3] = -1;
                *out_node = new_node;
                return 0;
            }
            *out_node = 0;
            return 0;
        }

        /* Get pointer to current node's data block. */
        node = (int *)FUN_00117ee0(&tree[1], *slot, 0x10);
        key = (*(int (*)(int, int))tree[6])(tree[5], node[0]);
        cmp = (*(int (*)(int, int, int, int))tree[7])(tree[5], vector, key,
                                                       count);
        if (cmp < 0) {
            slot = node + 1; /* left child */
        } else if (cmp > 0) {
            slot = node + 3; /* right child */
        } else {
            /* Equal: increment persistent count, check against max.
             * Both the outer-max check and inner-loop exhaustion share the
             * same found epilogue (matches original's single LAB_1109a2).
             */
            count++;
            scan = count;
            if ((short)scan < (short)tree[4]) {
                /* Inner duplicate scan: call comparator with increasing scan
                   value until non-zero result or scan reaches max. */
                do {
                    cmp = (*(int (*)(int, int, int, int))tree[7])(
                        tree[5], vector, key, scan);
                    if (cmp != 0) {
                        /* Non-zero: follow equal/mid child. */
                        slot = node + 2;
                        goto next_iter;
                    }
                    scan++;
                } while ((short)scan < (short)tree[4]);
            }
            /* Scan exhausted (or count already at max): found. */
            *out_node = (int)node;
            return 1;
        next_iter: ;
        }
    } while (1);
}

/* 0x110a10 — zlib adler32 checksum (pure integer leaf, no calls).
 * Updates a running Adler-32 sum over `len` bytes of `buf`. The state packs
 * two 16-bit sums: s1 (low half) in the low 16 bits, s2 (high half) in the
 * high 16 bits. NMAX (0x15b0 = 5552) is the most bytes that can be summed
 * before s2 can overflow a 32-bit accumulator; BASE (0xfff1 = 65521) is the
 * largest prime below 65536. The inner loop is MSVC's classic DO16 unroll
 * (16 byte accumulations per iteration) — its exact iVar3..iVar17 partial-sum
 * chain and final additive reduction are load-bearing for the VC71 byte match.
 *
 * Variable mapping vs. disassembly: uVar2 = ECX = s1 (low accumulator),
 * param_1 = EDI = s2 (high accumulator), param_2 = ESI = buf, param_3/uVar1
 * = EBX/EAX = length counters. Note the deliberate signed/unsigned asymmetry
 * in the two range tests: the NMAX clamp is unsigned (JC) while the unroll
 * guard `0xf < (int)uVar1` is signed (JL) — both reproduced exactly. */
unsigned int FUN_00110a10(unsigned int param_1, unsigned char *param_2,
                          unsigned int param_3)
{
  unsigned int uVar1;
  unsigned int uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  int iVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  int iVar12;
  int iVar13;
  int iVar14;
  int iVar15;
  int iVar16;
  int iVar17;
  unsigned int uVar18;

  uVar2 = param_1 & 0xffff;
  param_1 = param_1 >> 0x10;
  if (param_2 == (unsigned char *)0x0) {
    return 1;
  }
  while (param_3 != 0) {
    uVar1 = param_3;
    if (0x15af < param_3) {
      uVar1 = 0x15b0;
    }
    param_3 = param_3 - uVar1;
    if (0xf < (int)uVar1) {
      uVar18 = uVar1 >> 4;
      uVar1 = uVar1 + uVar18 * -0x10;
      do {
        iVar3 = uVar2 + *param_2;
        iVar4 = iVar3 + (unsigned int)param_2[1];
        iVar5 = iVar4 + (unsigned int)param_2[2];
        iVar6 = iVar5 + (unsigned int)param_2[3];
        iVar7 = iVar6 + (unsigned int)param_2[4];
        iVar8 = iVar7 + (unsigned int)param_2[5];
        iVar9 = iVar8 + (unsigned int)param_2[6];
        iVar10 = iVar9 + (unsigned int)param_2[7];
        iVar11 = iVar10 + (unsigned int)param_2[8];
        iVar12 = iVar11 + (unsigned int)param_2[9];
        iVar13 = iVar12 + (unsigned int)param_2[10];
        iVar14 = iVar13 + (unsigned int)param_2[0xb];
        iVar15 = iVar14 + (unsigned int)param_2[0xc];
        iVar16 = iVar15 + (unsigned int)param_2[0xd];
        iVar17 = iVar16 + (unsigned int)param_2[0xe];
        uVar2 = iVar17 + (unsigned int)param_2[0xf];
        param_1 = param_1 + iVar3 + iVar4 + iVar5 + iVar6 + iVar7 + iVar8 +
                  iVar9 + iVar10 + iVar11 + iVar12 + iVar13 + iVar14 + iVar15 +
                  iVar16 + iVar17 + uVar2;
        param_2 = param_2 + 0x10;
        uVar18 = uVar18 - 1;
      } while (uVar18 != 0);
    }
    for (; uVar1 != 0; uVar1 = uVar1 - 1) {
      uVar2 = uVar2 + *param_2;
      param_2 = param_2 + 1;
      param_1 = param_1 + uVar2;
    }
    uVar2 = uVar2 % 0xfff1;
    param_1 = param_1 % 0xfff1;
  }
  return param_1 << 0x10 | uVar2;
}

/* 0x110b40 — zlib compress2: deflate source into dest using a one-shot
 * deflate pass.  Allocates a z_stream on the stack (0x38 bytes = 14 dwords),
 * initialises it with deflateInit_ (FUN_001127b0), runs deflate with
 * Z_FINISH flush, then calls deflateEnd.  On Z_STREAM_END the compressed
 * output length is written back through destLen.
 *
 * Stack layout (stream base = EBP-0x38, 14-dword array):
 *   s[0] = next_in  = source        s[1] = avail_in = sourceLen
 *   s[3] = next_out = dest          s[4] = avail_out = *destLen (in)
 *   s[5] = total_out (read back)    s[8..10] = zalloc/zfree/opaque = 0
 *
 * Return: 0 on success (deflateEnd result), -5 if deflate returned Z_OK but
 * not Z_STREAM_END (output buffer too small), or the raw deflate/init error
 * code.  param_5 is the compression level (e.g. -1 = Z_DEFAULT_COMPRESSION,
 * 9 = Z_BEST_COMPRESSION) passed to deflateInit_. */
int FUN_00110b40(void *dest, int *destLen, int source,
                 unsigned int sourceLen, int level)
{
  unsigned int s[14];
  int r;
  int err;

  s[0]  = (unsigned int)source;
  s[1]  = sourceLen;
  s[3]  = (unsigned int)dest;
  s[4]  = (unsigned int)*destLen;
  s[8]  = 0;
  s[9]  = 0;
  s[10] = 0;
  err = FUN_001127b0((int)s, level, "1.1.3", 0x38);
  if (err != 0) goto compress_init_failed;
  r = FUN_00110ed0((void *)s, 4);
  if (r != 1) goto compress_deflate_failed;
  *destLen = (int)s[5];
  return FUN_00111170((int)s);
compress_deflate_failed:
  FUN_00111170((int)s);
  if (r == 0) return -5;
  return r;
compress_init_failed:
  return err;
}

/* zlib compress: forwards to compress2 (FUN_00110b40) with level=-1
   (Z_DEFAULT_COMPRESSION). */
void FUN_00110be0(unsigned int *param_1, int *param_2, int param_3,
                  unsigned int param_4)
{
  FUN_00110b40(param_1, param_2, param_3, param_4, 0xffffffff);
  return;
}

/* zlib crc32(crc, buf, len): accumulate a CRC-32 over a byte buffer using the
   static 256-entry lookup table at 0x28ce48. The CRC is inverted on entry and
   exit (the standard zlib convention). The bulk loop is unrolled by 8; each
   step chains the running CRC through one table lookup. A trailing per-byte
   loop handles len % 8. Returns 0 if buf is NULL. */
unsigned int FUN_00110c10(unsigned int crc, void *buf, int len)
{
  unsigned char *p = (unsigned char *)buf;
  unsigned int rem = (unsigned int)len;
  unsigned int n8;

  if (p == (unsigned char *)0) {
    return 0;
  }
  crc = ~crc;
  if (7 < rem) {
    n8 = rem >> 3;
    do {
      rem = rem - 8;
      crc =
        *(unsigned int *)(0x28ce48 + ((p[0] ^ crc) & 0xff) * 4) ^ (crc >> 8);
      crc =
        *(unsigned int *)(0x28ce48 + ((p[1] ^ crc) & 0xff) * 4) ^ (crc >> 8);
      crc =
        *(unsigned int *)(0x28ce48 + ((p[2] ^ crc) & 0xff) * 4) ^ (crc >> 8);
      crc =
        *(unsigned int *)(0x28ce48 + ((p[3] ^ crc) & 0xff) * 4) ^ (crc >> 8);
      crc =
        *(unsigned int *)(0x28ce48 + ((p[4] ^ crc) & 0xff) * 4) ^ (crc >> 8);
      crc =
        *(unsigned int *)(0x28ce48 + ((p[5] ^ crc) & 0xff) * 4) ^ (crc >> 8);
      crc =
        *(unsigned int *)(0x28ce48 + ((p[6] ^ crc) & 0xff) * 4) ^ (crc >> 8);
      crc =
        (crc >> 8) ^ *(unsigned int *)(0x28ce48 + ((p[7] ^ crc) & 0xff) * 4);
      p = p + 8;
      n8 = n8 - 1;
    } while (n8 != 0);
  }
  for (; rem != 0; rem = rem - 1) {
    crc = (crc >> 8) ^ *(unsigned int *)(0x28ce48 + ((*p ^ crc) & 0xff) * 4);
    p = p + 1;
  }
  return ~crc;
}

/* zlib read_buf + hash insertion: copy up to (window_size - MIN_LOOKAHEAD)
   bytes from the input into the deflate window via csmemcpy, update the adler
   checksum (FUN_00110a10), then for runs of 3+ bytes rebuild the hash chain:
   roll the rolling hash ins_h (+0x40) over each byte and link head (+0x38) /
   prev (+0x3c) using the window mask (+0x2c). The shift (+0x50) and hash mask
   (+0x4c) drive the rolling hash. Returns Z_OK (0), or Z_STREAM_ERROR (-2) on
   a bad stream. */
int FUN_00110d40(int param_1, int param_2, unsigned int param_3)
{
  int state;
  unsigned int more;
  unsigned int n;
  unsigned int count;
  unsigned int ins_h;

  if (param_1 == 0) goto stream_error_d40;
  state = *(int *)(param_1 + 0x1c);
  if (state == 0) goto stream_error_d40;
  if (param_2 == 0) goto stream_error_d40;
  if (*(int *)(state + 4) != 0x2a) goto stream_error_d40;

  *(int *)(param_1 + 0x30) = (int)FUN_00110a10(
    *(unsigned int *)(param_1 + 0x30), (unsigned char *)param_2, param_3);

  if (2 < param_3) {
    count = param_3;
    more = *(int *)(state + 0x24) - 0x106;
    if (more < param_3) {
      param_2 = param_2 + (param_3 - more);
      param_3 = more;
      count = more;
    }
    csmemcpy(*(void **)(state + 0x30), (void *)param_2, param_3);
    *(unsigned int *)(state + 0x64) = count;
    *(unsigned int *)(state + 0x54) = count;

    ins_h = (unsigned int)**(unsigned char **)(state + 0x30);
    *(unsigned int *)(state + 0x40) = ins_h;
    ins_h = ((ins_h << ((unsigned char)*(int *)(state + 0x50) & 0x1f)) ^
             (unsigned int)(*(unsigned char **)(state + 0x30))[1]) &
            *(unsigned int *)(state + 0x4c);
    *(unsigned int *)(state + 0x40) = ins_h;

    n = 0;
    do {
      ins_h =
        ((unsigned int)*(unsigned char *)(*(int *)(state + 0x30) + 2 + n) ^
         (*(int *)(state + 0x40)
          << ((unsigned char)*(int *)(state + 0x50) & 0x1f))) &
        *(unsigned int *)(state + 0x4c);
      *(unsigned int *)(state + 0x40) = ins_h;
      *(unsigned short *)(*(int *)(state + 0x38) +
                          (*(unsigned int *)(state + 0x2c) & n) * 2) =
        *(unsigned short *)(*(int *)(state + 0x3c) + ins_h * 2);
      *(unsigned short *)(*(int *)(state + 0x3c) +
                          *(unsigned int *)(state + 0x40) * 2) =
        (unsigned short)n;
      n = n + 1;
    } while (n <= param_3 - 3);
  }
  return 0;
stream_error_d40:
  return -2;
}

/* zlib deflateEnd(strm): tear down a deflate stream. Validates the stream and
   its internal state (status must be 0x2a / 0x71 / 0x29a), then frees the
   pending, head, prev, and window buffers (when present) via the stream's
   zfree callback at strm+0x24 with opaque at strm+0x28, finally frees the
   state object itself and clears strm+0x1c. Returns Z_STREAM_ERROR (-2) on a
   bad stream, Z_DATA_ERROR (-3) if state was 0x71, else Z_OK (0). */
int FUN_00111170(int param_1)
{
  void (*zfree)(int, int);
  int state;
  int status;
  int ptr;

  if (param_1 == 0) goto z_stream_error;
  state = *(int *)(param_1 + 0x1c);
  if (state == 0) goto z_stream_error;
  status = *(int *)(state + 4);
  if (status != 0x2a && status != 0x71 && status != 0x29a) goto z_stream_error;

  zfree = (void (*)(int, int)) * (void **)(param_1 + 0x24);

  ptr = *(int *)(state + 8);
  if (ptr != 0) {
    zfree(*(int *)(param_1 + 0x28), ptr);
  }
  ptr = *(int *)(*(int *)(param_1 + 0x1c) + 0x3c);
  if (ptr != 0) {
    zfree(*(int *)(param_1 + 0x28), ptr);
  }
  ptr = *(int *)(*(int *)(param_1 + 0x1c) + 0x38);
  if (ptr != 0) {
    zfree(*(int *)(param_1 + 0x28), ptr);
  }
  ptr = *(int *)(*(int *)(param_1 + 0x1c) + 0x30);
  if (ptr != 0) {
    zfree(*(int *)(param_1 + 0x28), ptr);
  }
  zfree(*(int *)(param_1 + 0x28), *(int *)(param_1 + 0x1c));
  *(int *)(param_1 + 0x1c) = 0;

  return ((status != 0x71) - 1) & 0xfffffffd;
z_stream_error:
  return -2;
}

/* 0x112590 — zlib deflateInit2_: allocate and initialize a deflate stream.
 * param_1=z_stream, param_2=level(-1=>6), param_3=method(must be 8),
 * param_4=windowBits(8..15; negative => raw deflate, |windowBits| with wrap=0),
 * param_5=memLevel(1..9), param_6=strategy(0..2), param_7=version string,
 * param_8=sizeof(z_stream) (must be 0x38). Validates the args and the zlib
 * version's first char; installs the default zalloc/zfree (FUN_00117ad0 /
 * FUN_00117b00) when the caller left them NULL; allocates the deflate_state
 * (0x16c0 bytes) and the window/prev/head/pending buffers via the stream's
 * zalloc callback; derives w_size/w_mask/hash_size/hash_bits/hash_mask/
 * hash_shift/lit_bufsize and the sym buffer pointers; then defers to
 * deflateReset (FUN_00112260). Returns the deflateReset result, or
 * Z_STREAM_ERROR(-2) / Z_MEM_ERROR(-4) / Z_VERSION_ERROR(-6).
 * deflate_state (s, int* index): [2]pending_buf [3]pending_buf_size [6]wrap
 * [9]w_size [0xa]w_bits [0xb]w_mask [0xc]window [0xe]prev [0xf]head
 * [0x11]hash_size [0x12]hash_bits [0x13]hash_mask [0x14]hash_shift [0x1f]level
 * [0x20]strategy [0x5a4]sym_end [0x5a5]lit_bufsize [0x5a7]sym_buf; byte+0x1d=
 * method. z_stream byte offsets: +0x18 msg, +0x1c state, +0x20 zalloc,
 * +0x24 zfree, +0x28 opaque. Version string ptr @0x31fc70, "insufficient
 * memory" msg ptr @0x320e20. */
int FUN_00112590(int param_1, int param_2, int param_3, int param_4,
                 int param_5, int param_6, char *param_7, int param_8)
{
  int *s;
  int wrap;
  int w_size;
  int lit_bufsize;
  int opaque;
  void *(*zalloc)(int, unsigned int, unsigned int);

  if (param_7 == (char *)0 || *param_7 != **(char **)0x31fc70 || param_8 != 0x38)
    return -6;
  if (param_1 == 0)
    return -2;

  *(int *)(param_1 + 0x18) = 0;
  if (*(int *)(param_1 + 0x20) == 0) {
    *(int *)(param_1 + 0x20) = (int)FUN_00117ad0;
    *(int *)(param_1 + 0x28) = 0;
  }
  if (*(int *)(param_1 + 0x24) == 0)
    *(int *)(param_1 + 0x24) = (int)FUN_00117b00;

  if (param_2 == -1)
    param_2 = 6;
  wrap = 0;
  if (param_4 < 0) {
    param_4 = -param_4;
    wrap = 1;
  }
  if (param_5 < 1 || param_5 > 9 || param_3 != 8 || param_4 < 8 || param_4 > 0xf ||
      param_2 < 0 || param_2 > 9 || param_6 < 0 || param_6 > 2)
    return -2;

  zalloc = *(void *(**)(int, unsigned int, unsigned int))(param_1 + 0x20);
  opaque = *(int *)(param_1 + 0x28);
  s = (int *)zalloc(opaque, 1, 0x16c0);
  if (s == (int *)0)
    return -4;
  *(int *)(param_1 + 0x1c) = (int)s;
  s[0xa] = param_4;
  w_size = 1 << param_4;
  s[6] = wrap;
  s[0xb] = w_size - 1;
  s[0x12] = param_5 + 7;
  s[0] = param_1;
  s[0x11] = 1 << (param_5 + 7);
  s[0x13] = s[0x11] - 1;
  s[9] = w_size;
  s[0x14] = (unsigned int)(param_5 + 9) / 3;
  s[0xc] = (int)zalloc(opaque, w_size, 2);
  s[0xe] = (int)zalloc(opaque, s[9], 2);
  s[0xf] = (int)zalloc(opaque, s[0x11], 2);
  lit_bufsize = 1 << (param_5 + 6);
  s[0x5a5] = lit_bufsize;
  s[2] = (int)zalloc(opaque, lit_bufsize, 4);
  s[3] = s[0x5a5] * 4;
  if (s[0xc] != 0 && s[0xe] != 0 && s[0xf] != 0 && s[2] != 0) {
    s[0x5a7] = s[2] + (int)((unsigned int)s[0x5a5] & 0xfffffffe);
    s[0x5a4] = s[2] + s[0x5a5] * 3;
    s[0x1f] = param_2;
    s[0x20] = param_6;
    *((char *)s + 0x1d) = 8;
    return FUN_00112260(param_1);
  }
  *(int *)(param_1 + 0x18) = *(int *)0x320e20; /* "insufficient memory" */
  FUN_00111170(param_1);
  return -4;
}

/* zlib deflateInit_: forwards to deflateInit2_ (FUN_00112590) with method=8,
   windowBits=15, memLevel=8, strategy=0. Returns the deflateInit2_ result
   code (Z_OK=0 on success, negative on error). noinline prevents inlining
   across the original TU boundary (originally a separate zlib .c file). */
__declspec(noinline) int FUN_001127b0(int param_1, int param_2, char *param_3,
                                      int param_4)
{
  return FUN_00112590(param_1, param_2, 8, 0xf, 8, 0, param_3, param_4);
}

/* 0x111420 — zlib lm_init: initialize the deflate longest-match state for the
 * current level (state @<esi>). Sets window_size (s+0x34) = 2*w_size (s+0x24);
 * NIL-terminates and zeroes the hash head table (s+0x3c, hash_size s+0x44
 * entries); loads the per-level tuning from the config table @0x28d280
 * (max_lazy s+0x78, good_length s+0x84, nice_length s+0x88, max_chain s+0x74);
 * and resets match bookkeeping (s+0x40 block_start, s+0x54/0x58/0x60/0x64/0x6c
 * strstart/match_start/lookahead/prev_length/match_available, s+0x70). The
 * config layout matches deflateParams/deflateInit2_. @<esi>-defined => VC71
 * prologue ceiling. */
void FUN_00111420(int s)
{
  int cfg;
  unsigned int max_chain;

  *(int *)(s + 0x34) = *(int *)(s + 0x24) << 1;
  *(unsigned short *)(*(int *)(s + 0x3c) + *(int *)(s + 0x44) * 2 - 2) = 0;
  csmemset(*(void **)(s + 0x3c), 0, *(int *)(s + 0x44) * 2 - 2);
  cfg = *(int *)(s + 0x7c) * 0xc;
  *(unsigned int *)(s + 0x78) = *(unsigned short *)(0x28d282 + cfg);
  *(unsigned int *)(s + 0x84) = *(unsigned short *)(0x28d280 + cfg);
  *(unsigned int *)(s + 0x88) = *(unsigned short *)(0x28d284 + cfg);
  max_chain = *(unsigned short *)(0x28d286 + cfg);
  *(int *)(s + 0x64) = 0;
  *(int *)(s + 0x54) = 0;
  *(int *)(s + 0x6c) = 0;
  *(int *)(s + 0x60) = 0;
  *(int *)(s + 0x40) = 0;
  *(unsigned int *)(s + 0x74) = max_chain;
  *(int *)(s + 0x70) = 2;
  *(int *)(s + 0x58) = 2;
}

/* 0x110e40 — zlib putShortMSB: append a 16-bit value to the deflate pending
 * buffer most-significant-byte first, as two post-increment put_byte stores
 * (pending_buf @ state+0x8, pending count @ state+0x14). @<eax>=state =>
 * VC71 prologue ceiling. */
void FUN_00110e40(unsigned int byte_val /*@<ecx>*/, int state /*@<eax>*/)
{
  *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
      (unsigned char)(byte_val >> 8);
  ++*(int *)(state + 0x14);
  *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
      (unsigned char)byte_val;
  ++*(int *)(state + 0x14);
}

/* 0x110e70 — zlib flush_pending: copy as many pending output bytes as fit into
 * strm->next_out, capped at avail_out. strm fields: state @ +0x1c, next_out
 * @ +0xc, avail_out @ +0x10, total_out @ +0x14; state fields: pending_buf @
 * +0x8, pending_out @ +0x10, pending @ +0x14. Advances next_out/total_out and
 * pending_out by len, drains avail_out/pending by len; when the buffer fully
 * drains (pending==0) resets pending_out to pending_buf. @<eax>=strm => VC71
 * prologue ceiling. (kb decl names the arg "state"; it is the z_streamp.) */
void FUN_00110e70(int strm /*@<eax>*/)
{
  int s;
  unsigned int len;

  s = *(int *)(strm + 0x1c);                /* strm->state */
  len = *(unsigned int *)(s + 0x14);        /* state->pending */
  if (len > *(unsigned int *)(strm + 0x10)) /* > avail_out */
    len = *(unsigned int *)(strm + 0x10);
  if (len == 0)
    return;
  csmemcpy(*(void **)(strm + 0xc),          /* next_out (dst) */
           *(void **)(s + 0x10),            /* pending_out (src) */
           len);
  *(int *)(strm + 0xc) += len;              /* next_out += len */
  *(int *)(s + 0x10) += len;                /* pending_out += len */
  *(int *)(strm + 0x14) += len;             /* total_out += len */
  *(int *)(strm + 0x10) -= len;             /* avail_out -= len */
  *(int *)(s + 0x14) -= len;                /* pending -= len */
  s = *(int *)(strm + 0x1c);                /* reload strm->state */
  if (*(int *)(s + 0x14) == 0)              /* pending == 0 */
    *(int *)(s + 0x10) = *(int *)(s + 8);   /* pending_out = pending_buf */
}

/* 0x110820 — bounded callback iterator (real_math.obj). For each index in
 * [start, count) where count = *(short*)(obj+0x10), invoke the object's
 * callback at obj+0x1c as callback(context=*(int*)(obj+0x14), param_1, passthru,
 * index). Returns 0 the moment any callback returns nonzero, else 1 once every
 * element passes. The loop counter is a 16-bit signed short held in a 32-bit
 * register (MSVC short-loop codegen). ABI: start in EAX, passthru in EBX
 * (forwarded untouched to the callback), obj in ESI, param_1 on the stack =>
 * VC71 prologue ceiling. (Decompiler under-counts the 4-arg callback and misses
 * the EBX passthrough; both recovered from disassembly.) */
int FUN_00110820(int start /*@<eax>*/, int passthru /*@<ebx>*/,
                 void *obj /*@<esi>*/, int param_1)
{
  int (*callback)(int, int, int, int);
  int index;

  index = start;
  while ((short)index < *(short *)((char *)obj + 0x10)) {  /* index < count */
    callback = *(int (**)(int, int, int, int))((char *)obj + 0x1c);
    if (callback(*(int *)((char *)obj + 0x14), param_1, passthru, index) != 0)
      return 0;
    index = index + 1;
  }
  return 1;
}

/* 0x110ed0 — zlib deflate(): the compression driver/state machine. Validates
 * the stream + flush (0..4); errors with Z_STREAM_ERROR(-2)/Z_BUF_ERROR(-5).
 * On INIT_STATE writes the 2-byte zlib header (round-to-multiple-of-31, with
 * PRESET_DICT bit + preset-dictionary adler when strstart!=0) and sets BUSY.
 * Drains pending output (flush_pending). Then, when there is input/lookahead or
 * a non-zero flush outside FINISH, runs configuration_table[level].func(s,flush)
 * and acts on its block_state: need_more/finish_started -> Z_OK (last_flush=-1
 * if output is full); block_done -> _tr_align (PARTIAL) or _tr_stored_block +
 * optional hash clear (FULL), then flush. On Z_FINISH writes the adler32 trailer
 * (unless noheader) and returns Z_STREAM_END once fully flushed. strm fields:
 * next_in[0]/avail_in[1]/next_out[3]/avail_out[4]/msg[6]/state[7]/adler[0xc];
 * state fields: strm[0]/status[1]/pending[5]/noheader[6]/last_flush[8]/w_bits
 * [0xa]/head[0xf]/hash_size[0x11]/strstart[0x19]/lookahead[0x1b]/level[0x1f]. */
int FUN_00110ed0(void *strm, int flush)
{
  int *z;
  int *s;
  int old_flush;
  int bstate;
  unsigned int header;
  int level_flags;
  int (*deflate_func)(int *, int);

  z = (int *)strm;
  if (z == (int *)0 || (s = *(int **)(z + 7)) == (int *)0 ||
      flush > 4 || flush < 0)
    return -2;                                  /* Z_STREAM_ERROR */
  if (z[3] == 0 || (z[0] == 0 && z[1] != 0) ||
      (s[1] == 0x29a && flush != 4)) {          /* FINISH_STATE && !Z_FINISH */
    z[6] = *(int *)0x320e18;                     /* msg = "stream error" */
    return -2;
  }
  if (z[4] == 0) {                              /* avail_out == 0 */
    z[6] = *(int *)0x320e24;                     /* msg = "buffer error" */
    return -5;                                  /* Z_BUF_ERROR */
  }

  old_flush = s[8];                             /* save last_flush */
  s[0] = (int)z;                                /* s->strm = strm */
  s[8] = flush;                                 /* s->last_flush = flush */

  /* Write the zlib header. */
  if (s[1] == 0x2a) {                           /* INIT_STATE */
    header = (unsigned int)(((s[0xa] - 8) << 12) + 0x800); /* (Z_DEFLATED|((w_bits-8)<<4))<<8 */
    level_flags = (s[0x1f] - 1) >> 1;
    if ((unsigned int)level_flags > 3)
      level_flags = 3;
    header |= (unsigned int)(level_flags << 6);
    if (s[0x19] != 0)                           /* strstart != 0 */
      header |= 0x20;                           /* PRESET_DICT */
    header += 31 - header % 31;
    s[1] = 0x71;                                /* BUSY_STATE */
    ((void(*)(void))FUN_00110e40)(header, (int)s);               /* putShortMSB(s, header) */
    if (s[0x19] != 0) {                         /* preset dictionary adler32 */
      ((void(*)(void))FUN_00110e40)((unsigned int)*(unsigned short *)((char *)z + 0x32), (int)s);
      ((void(*)(void))FUN_00110e40)((unsigned int)z[0xc] & 0xffff, (int)s);
    }
    z[0xc] = 1;                                 /* strm->adler = 1 */
  }

  /* Flush as much pending output as possible. */
  if (s[5] != 0) {                              /* s->pending != 0 */
    ((void(__fastcall*)(int))FUN_00110e70)((int)z);                       /* flush_pending(strm) */
    if (z[4] == 0) {                            /* avail_out == 0 */
      s[8] = -1;                                /* last_flush = -1 */
      return 0;                                 /* Z_OK */
    }
  } else if (z[1] == 0 && flush <= old_flush && flush != 4) {
    z[6] = *(int *)0x320e24;                     /* "buffer error" */
    return -5;
  }

  /* No more input is allowed after the first Z_FINISH. */
  if (s[1] == 0x29a && z[1] != 0) {             /* FINISH_STATE && avail_in != 0 */
    z[6] = *(int *)0x320e24;
    return -5;
  }

  /* Start a new block or continue the current one. */
  if (z[1] != 0 || s[0x1b] != 0 || (flush != 0 && s[1] != 0x29a)) {
    deflate_func = *(int (**)(int *, int))(0x28d288 + s[0x1f] * 0xc);
    bstate = deflate_func(s, flush);            /* configuration_table[level].func */
    if (bstate == 2 || bstate == 3)             /* finish_started / finish_done */
      s[1] = 0x29a;                             /* FINISH_STATE */
    if (bstate == 0 || bstate == 2) {           /* need_more / finish_started */
      if (z[4] == 0)                            /* avail_out == 0 */
        s[8] = -1;                              /* last_flush = -1 */
      return 0;                                 /* Z_OK */
    }
    if (bstate == 1) {                          /* block_done */
      if (flush == 1) {                         /* Z_PARTIAL_FLUSH */
        FUN_001176f0((int)s);                   /* _tr_align(s) */
      } else {                                  /* FULL_FLUSH or SYNC_FLUSH */
        FUN_001176a0((int)s, (unsigned char *)0, 0, 0); /* _tr_stored_block(s,0,0,0) */
        if (flush == 3) {                       /* Z_FULL_FLUSH: clear hash */
          *(unsigned short *)(s[0xf] + s[0x11] * 2 - 2) = 0;
          csmemset((void *)s[0xf], 0, s[0x11] * 2 - 2);
        }
      }
      ((void(__fastcall*)(int))FUN_00110e70)((int)z);                     /* flush_pending(strm) */
      if (z[4] == 0) {                          /* avail_out == 0 */
        s[8] = -1;                              /* last_flush = -1 */
        return 0;
      }
    }
  }
  /* Assert(strm->avail_out > 0, "bug2") */
  if (z[4] == 0)
    FUN_00117a80((const char *)0x28d2f8);
  if (flush != 4)                               /* != Z_FINISH */
    return 0;                                   /* Z_OK */
  if (s[6] != 0)                                /* noheader != 0: raw deflate, no trailer */
    return 1;                                   /* Z_STREAM_END */
  /* Write the gzip/zlib adler32 trailer. */
  ((void(*)(void))FUN_00110e40)((unsigned int)*(unsigned short *)((char *)z + 0x32), (int)s); /* adler>>16 */
  ((void(*)(void))FUN_00110e40)((unsigned int)z[0xc] & 0xffff, (int)s);                       /* adler&0xffff */
  ((void(__fastcall*)(int))FUN_00110e70)((int)z);                         /* flush_pending(strm) */
  s[6] = -1;                                    /* noheader = -1 */
  return (s[5] == 0);                           /* pending==0 ? Z_STREAM_END(1) : Z_OK(0) */
}

/* 0x1113c0 — zlib read_buf: copy up to `size` bytes from the stream input
 * (strm->next_in, strm[0]) into `buf`, capping at strm->avail_in (strm[1]).
 * Returns the byte count copied (0 if avail_in==0). When state->wrap (state at
 * strm+0x1c, wrap at +0x18) is zero the running checksum strm->adler (strm+0x30)
 * is folded over the copied bytes via FUN_00110a10 (adler32). Then advances
 * avail_in (-=len), total_in (strm+8 +=len) and next_in (+=len). ABI: strm in
 * EDI, size in ECX, buf pushed on the stack => VC71 prologue ceiling. */
unsigned int FUN_001113c0(int strm /*@<edi>*/, unsigned int size /*@<ecx>*/,
                          void *buf)
{
  int *s;
  unsigned int len;

  s = (int *)strm;
  len = (unsigned int)s[1];          /* avail_in */
  if (len > size)
    len = size;
  if (len == 0)
    return 0;
  s[1] -= len;                       /* avail_in -= len */
  if (*(int *)(s[7] + 0x18) == 0)    /* state->wrap == 0 */
    s[0xc] = (int)FUN_00110a10((unsigned int)s[0xc],
                               (unsigned char *)s[0], len);
  csmemcpy(buf, (void *)s[0], len);  /* next_in -> buf */
  s[2] += len;                       /* total_in += len */
  s[0] += len;                       /* next_in += len */
  return len;
}

/* 0x112260 — zlib deflateReset: reset a deflate stream so a fresh compression
 * can begin on the same allocated state. Validates strm + state (strm+0x1c) +
 * zalloc/zfree (strm+0x20/0x24), else Z_STREAM_ERROR(-2). Clears total_out
 * (strm+0x14), total_in (strm+0x8) and msg (strm+0x18); sets data_type
 * (strm+0x2c)=Z_UNKNOWN(2) and adler (strm+0x30)=1. Resets the deflate_state:
 * pending_out (s+0x10)=pending_buf (s+0x8), pending (s+0x14)=0, normalizes wrap
 * (s+0x18 = |wrap|), status (s+0x4) = wrap ? 0x71 : INIT_STATE(0x2a), last_flush
 * (s+0x20)=0; then _tr_init (FUN_00117250) and lm_init (FUN_00111420 @<esi>=
 * state). Returns Z_OK(0). */
int FUN_00112260(int strm)
{
  int s;

  if (strm == 0 || (s = *(int *)(strm + 0x1c)) == 0 ||
      *(int *)(strm + 0x20) == 0 || *(int *)(strm + 0x24) == 0)
    return -2;
  *(int *)(strm + 0x14) = 0;
  *(int *)(strm + 8) = 0;
  *(int *)(strm + 0x18) = 0;
  *(int *)(strm + 0x2c) = 2;
  *(int *)(s + 0x10) = *(int *)(s + 8);
  *(int *)(s + 0x14) = 0;
  if (*(int *)(s + 0x18) < 0)
    *(int *)(s + 0x18) = 0;
  *(int *)(s + 4) = (-(int)(*(int *)(s + 0x18) != 0) & 0x47) + 0x2a;
  *(int *)(strm + 0x30) = 1;
  *(int *)(s + 0x20) = 0;
  FUN_00117250(s);
  ((void(__fastcall*)(int))FUN_00111420)(s);
  return 0;
}

/* 0x1122e0 — zlib deflateParams: dynamically change the deflate compression
 * level (level; -1 => default 6, valid 0..9) and strategy (0..2) on stream
 * `strm`. If the new level selects a different deflate function than the active
 * one (configuration_table[level].func) and input has been consumed, the
 * current block is flushed first — writing the zlib header if still in
 * INIT_STATE (status 0x2a) — via configuration_table[s->level].func(s, Z_BLOCK).
 * Then level/strategy and the good_length/max_lazy/nice_length/max_chain tuning
 * are updated from the config table. Returns Z_OK(0), Z_STREAM_ERROR(-2) or
 * Z_BUF_ERROR(-5).
 *   z_stream: [0]next_in [1]avail_in [2]total_in [3]next_out [4]avail_out
 *   [6]msg [7]state [0xc]adler ; deflate_state s: [0]strm [1]status [5]pending
 *   [8]last_flush [0xa]w_bits [0x19]wrap/dict [0x1b]high_water flag [0x1d]
 *   max_chain [0x1e]max_lazy [0x1f]level [0x20]strategy [0x21]good_length
 *   [0x22]nice_length. config table @0x28d280: { ush good_length, max_lazy,
 *   nice_length, max_chain; func } (12 B/entry). Callees: FUN_00110e40 =
 *   putShortMSB(@<ecx>=val, @<eax>=s); FUN_00110e70 = flush_pending(@<eax>=strm);
 *   FUN_001176f0 = _tr_align; FUN_00117a80 = trace/assert(msg). */
int FUN_001122e0(int *strm, int level, int strategy)
{
  int *s;
  int err;
  int old_last_flush;
  int level_off;
  unsigned int header;
  int level_flags;
  int bstate;
  int need_assert;
  int (*deflate_func)(int *, int);

  err = 0;
  if (strm == (int *)0 || (s = (int *)strm[7]) == (int *)0)
    return -2;
  if (level == -1)
    level = 6;
  else if (level < 0 || level > 9)
    return -2;
  if (strategy < 0 || strategy > 2)
    return -2;

  level_off = level * 0xc;
  if (*(void **)(0x28d288 + s[0x1f] * 0xc) == *(void **)(0x28d288 + level_off) ||
      strm[2] == 0)
    goto set_params;

  if (strm[3] == 0 || (*strm == 0 && strm[1] != 0) || s[1] == 0x29a) {
    err = -2;
    strm[6] = *(int *)0x320e18; /* "stream error" */
    goto set_params;
  }
  if (strm[4] == 0) {
    strm[6] = *(int *)0x320e24; /* "buffer error" */
    err = -5;
    goto set_params;
  }

  old_last_flush = s[8];
  s[0] = (int)strm;
  s[8] = 1;
  if (s[1] == 0x2a) { /* INIT_STATE: emit the zlib header */
    s[1] = 0x71;      /* BUSY_STATE */
    header = (unsigned int)(((s[0xa] - 8) << 0xc) + 0x800);
    level_flags = (s[0x1f] - 1) >> 1;
    if ((unsigned int)level_flags > 3)
      level_flags = 3;
    header |= (unsigned int)(level_flags << 6);
    if (s[0x19] != 0)
      header |= 0x20; /* PRESET_DICT */
    header += 0x1f - header % 0x1f;
    ((void(*)(void))FUN_00110e40)(header, (int)s);
    if (s[0x19] != 0) {
      ((void(*)(void))FUN_00110e40)((unsigned int)*(unsigned short *)((int)strm + 0x32), (int)s);
      ((void(*)(void))FUN_00110e40)((unsigned int)(strm[0xc] & 0xffff), (int)s);
    }
    strm[0xc] = 1;
  }

  if (s[5] != 0) { /* pending output: flush it */
    ((void(__fastcall*)(int))FUN_00110e70)((int)strm);
    if (strm[4] == 0) {
      s[8] = -1;
      err = 0;
      goto set_params;
    }
  } else if (strm[1] == 0 && old_last_flush >= 1) {
    err = -5;
    strm[6] = *(int *)0x320e24;
    goto set_params;
  }

  if (s[1] == 0x29a) {
    if (strm[1] != 0) {
      strm[6] = *(int *)0x320e24;
      err = -5;
      goto set_params;
    }
    if (s[0x1b] == 0 && s[1] == 0x29a)
      goto need_more;
  } else if (strm[1] == 0) {
    if (s[0x1b] == 0 && s[1] == 0x29a)
      goto need_more;
  }

  deflate_func = *(int (**)(int *, int))(0x28d288 + s[0x1f] * 0xc);
  bstate = deflate_func(s, 1);
  if (bstate == 2 || bstate == 3)
    s[1] = 0x29a;
  if (bstate == 0 || bstate == 2) {
    if (strm[4] == 0)
      s[8] = -1;
    err = 0;
    goto set_params;
  }
  if (bstate != 1)
    goto need_more;
  FUN_001176f0((int)s);
  ((void(__fastcall*)(int))FUN_00110e70)((int)strm);
  if (strm[4] == 0) {
    s[8] = -1;
    err = 0;
    goto set_params;
  }
  need_assert = 0;
  goto after_block;

need_more:
  need_assert = 1;
after_block:
  if (need_assert && strm[4] == 0)
    FUN_00117a80((const char *)0x28d2f8);
  err = 0;

set_params:
  if (s[0x1f] != level) {
    s[0x1f] = level;
    s[0x1e] = (int)*(unsigned short *)(0x28d282 + level_off);
    s[0x21] = (int)*(unsigned short *)(0x28d280 + level_off);
    s[0x22] = (int)*(unsigned short *)(0x28d284 + level_off);
    s[0x1d] = (int)*(unsigned short *)(0x28d286 + level_off);
  }
  s[0x20] = strategy;
  return err;
}

/* zlib gzread: read up to len bytes from gzFile into buf via deflate state;
 * returns byte count or <0 on error. */
int FUN_001127e0(int *param_1, int param_2, int param_3)
{
  size_t sVar1;

  if ((param_1 != (int *)0) && (*(char *)((int)param_1 + 0x5c) == 'w')) {
    if (*(int *)((int)param_1 + 0x10) == 0) {
      *(void **)((int)param_1 + 0xc) = *(void **)((int)param_1 + 0x48);
      sVar1 = _fread(*(void **)((int)param_1 + 0x48), 1, 0x4000,
                           *(void **)((int)param_1 + 0x40));
      if (sVar1 != 0x4000) {
        *(int *)((int)param_1 + 0x38) = -1;
      }
      *(int *)((int)param_1 + 0x10) = 0x4000;
    }
    return FUN_001122e0(param_1, param_2, param_3);
  }
  return -2;
}

/* 0x112db0 — zlib gzwrite: deflate up to len bytes from buf through the
 * gz_stream s (write mode only). Drains the deflate output via fread-fill of
 * the input buffer, updates the running crc32, and returns the number of bytes
 * consumed (len - remaining avail_in). s modelled as unsigned int* (dword
 * fields). */
int FUN_00112db0(void *param_1, int param_2, int param_3)
{
  unsigned int *s;
  unsigned int r;
  int n;

  s = (unsigned int *)param_1;
  if (s == 0 || *(char *)(s + 0x17) != 'w') {
    return -2;
  }
  s[0] = (unsigned int)param_2; /* stream.next_in  = buf */
  s[1] = (unsigned int)param_3; /* stream.avail_in = len */
  n = param_3;
  do {
    if (n == 0) {
      s[0x13] = FUN_00110c10(s[0x13], (void *)param_2, param_3); /* crc32 */
      return param_3 - (int)s[1];
    }
    if (s[4] == 0) {
      s[3] = s[0x12];
      r = _fread((void *)s[0x12], 1, 0x4000, (void *)s[0x10]); /* fread */
      if (r != 0x4000) {
        s[0xe] = 0xffffffff;
        s[0x13] = FUN_00110c10(s[0x13], (void *)param_2, param_3); /* crc32 */
        return param_3 - (int)s[1];
      }
      s[4] = 0x4000;
    }
    n = FUN_00110ed0(s, 0); /* deflate */
    s[0xe] = (unsigned int)n;
    if (n != 0) {
      s[0x13] = FUN_00110c10(s[0x13], (void *)param_2, param_3); /* crc32 */
      return param_3 - (int)s[1];
    }
    n = (int)s[1];
  } while (1);
}

/* zlib gzprintf: vsprintf into a 4 KB stack buffer, then write strlen bytes to
 * the gzFile. */
int FUN_00112e50(void *param_1, const char *param_2, ...)
{
  char local_buf[4096];
  int len;

  vsprintf(local_buf, param_2, (char *)((char **)&param_2 + 1));
  len = csstrlen(local_buf);
  if (len < 1) {
    return 0;
  }
  return FUN_00112db0(param_1, (int)local_buf, len);
}

/* zlib gzgetc: read a single byte from stream param_1 via FUN_00112db0
   (gzread, count=1). Returns the byte (zero-extended) on success, or -1 (EOF)
   if the read did not yield exactly one byte. */
unsigned int FUN_00112eb0(void *param_1, unsigned char param_2)
{
  unsigned char buf;
  int n;

  buf = param_2;
  n = FUN_00112db0(param_1, (int)&buf, 1);
  if (n == 1)
    return (unsigned int)buf;
  return 0xffffffff;
}

/* zlib gzputs: writes the C string param_2 to stream param_1, computing its
 * length first. */
void FUN_00112ee0(void *param_1, const char *param_2)
{
  int iVar1;

  iVar1 = csstrlen(param_2);
  FUN_00112db0(param_1, (int)param_2, iVar1);
  return;
}

/* 0x112cd0 — zlib gzio gz_destroy: tear down a gz stream (gz @<esi>). Frees the
 * inflate/deflate workspace (gz+0x50); ends the deflate (write mode 'w' ->
 * FUN_00111170 deflateEnd) or inflate (read mode 'r' -> FUN_00115430
 * inflateEnd) engine when a state (gz+0x1c) exists; closes the backing FILE*
 * (gz+0x40) via crt_fclose, treating a failed close whose errno != ENOENT(0x1d)
 * as err=-1; if z_err (gz+0x38) is negative it overrides err; frees the input
 * and output buffers (gz+0x44, gz+0x48), the path string (gz+0x54), and finally
 * the gz struct itself — all via debug_free(ptr, gzio.c, line). Returns the
 * accumulated error code, or Z_STREAM_ERROR(-2) for a NULL gz.
 * @<esi>-defined => VC71 cannot reproduce the ESI-receiving prologue. */
int FUN_00112cd0(int gz)
{
  int err;

  err = 0;
  if (gz == 0)
    return -2;
  if (*(int *)(gz + 0x50) != 0)
    debug_free(*(void **)(gz + 0x50),
               "c:\\halo\\SOURCE\\memory\\zlib\\gzio.c", 0x143);
  if (*(int *)(gz + 0x1c) != 0) {
    if (*(char *)(gz + 0x5c) == 'w')
      err = FUN_00111170(gz);
    else if (*(char *)(gz + 0x5c) == 'r')
      err = FUN_00115430(gz);
  }
  if (*(void **)(gz + 0x40) != (void *)0 &&
      crt_fclose(*(void **)(gz + 0x40)) != 0 && *(int *)FUN_001db777() != 0x1d)
    err = -1;
  if (*(int *)(gz + 0x38) < 0)
    err = *(int *)(gz + 0x38);
  if (*(int *)(gz + 0x44) != 0)
    debug_free(*(void **)(gz + 0x44),
               "c:\\halo\\SOURCE\\memory\\zlib\\gzio.c", 0x158);
  if (*(int *)(gz + 0x48) != 0)
    debug_free(*(void **)(gz + 0x48),
               "c:\\halo\\SOURCE\\memory\\zlib\\gzio.c", 0x159);
  if (*(int *)(gz + 0x54) != 0)
    debug_free(*(void **)(gz + 0x54),
               "c:\\halo\\SOURCE\\memory\\zlib\\gzio.c", 0x15a);
  debug_free((void *)gz, "c:\\halo\\SOURCE\\memory\\zlib\\gzio.c", 0x15b);
  return err;
}

/* 0x112850 — zlib gzio get_byte: read the next raw byte from a gzip read-stream
 * (gz @<esi>). Returns -1 when z_eof (gz+0x3c) is already set. When the input
 * buffer is empty (count gz+0x4 == 0), clears errno (FUN_001db777) and refills
 * up to 0x4000 bytes via fread (FUN_001db3f7) from the FILE* (gz+0x40) into the
 * input buffer (gz+0x44); on a zero read sets z_eof (gz+0x3c=1) and, if the
 * FILE error flag (*(FILE+0xc) & 0x20) is set, z_err (gz+0x38=-1), then returns
 * -1; otherwise resets the read pointer (gz+0) to the buffer start. Consumes
 * and returns one byte, advancing gz+0 and decrementing the count.
 * @<esi>-defined: VC71 cannot reproduce the ESI-receiving prologue (structural
 * ceiling). Callees: FUN_001db777=_errno (returns int*), FUN_001db3f7=fread. */
unsigned int FUN_00112850(int gz)
{
  unsigned int n;
  unsigned char b;

  if (*(int *)(gz + 0x3c) != 0)
    return 0xffffffff;
  if (*(int *)(gz + 4) == 0) {
    *(int *)FUN_001db777() = 0;
    n = FUN_001db3f7(*(void **)(gz + 0x44), 1, 0x4000, *(void **)(gz + 0x40));
    *(int *)(gz + 4) = n;
    if (n == 0) {
      *(int *)(gz + 0x3c) = 1;
      if ((*(unsigned char *)(*(int *)(gz + 0x40) + 0xc) & 0x20) != 0)
        *(int *)(gz + 0x38) = -1;
      return 0xffffffff;
    }
    *(int *)gz = *(int *)(gz + 0x44);
  }
  *(int *)(gz + 4) = *(int *)(gz + 4) - 1;
  b = **(unsigned char **)gz;
  *(int *)gz = *(int *)gz + 1;
  return (unsigned int)b;
}

/* 0x112f00 — zlib gz_flush internal: deflate-and-write buffered data for a
 * gzip write-stream (gz @<eax>, gz+0x5c=='w'). Loops draining the deflate
 * output: when the output buffer (gz+0x48, gz+0x10 = bytes left) has data,
 * writes it to the FILE* (gz+0x40) via FUN_001db2b3 (=_fread thunk; the gzio
 * read/write CRT entry), resetting gz+0xc/gz+0x10; then runs one deflate step
 * FUN_00110ed0((z_stream*)gz, flush) storing z_err at gz+0x38, treating a
 * Z_BUF_ERROR(-5) with no pending output as Z_OK. Continues while z_err is
 * Z_OK(0) or Z_STREAM_END(1) and not yet `done` (done set once output drains
 * and z_err != Z_STREAM_END). Returns z_err, mapping Z_STREAM_END(1) to 0
 * (the SBB idiom `-(z_err!=1) & z_err`), or Z_STREAM_ERROR(-2)/IO-error(-1).
 * Note: @<eax>-defined, so VC71 cannot reproduce the reg-receiving prologue —
 * a structural ceiling; correctness verified vs disasm. */
unsigned int FUN_00112f00(int gz, int flush)
{
  int done;
  unsigned int n;

  done = 0;
  if (gz == 0 || *(char *)(gz + 0x5c) != 'w')
    return 0xfffffffe;
  *(int *)(gz + 4) = 0;
  do {
    n = 0x4000 - *(int *)(gz + 0x10);
    if (n != 0) {
      if (_fread(*(void **)(gz + 0x48), 1, n, *(void **)(gz + 0x40)) != n) {
        *(int *)(gz + 0x38) = -1;
        return 0xffffffff;
      }
      *(int *)(gz + 0xc) = *(int *)(gz + 0x48);
      *(int *)(gz + 0x10) = 0x4000;
    }
    if (done)
      break;
    *(int *)(gz + 0x38) = FUN_00110ed0((void *)gz, flush);
    if (n == 0 && *(int *)(gz + 0x38) == -5)
      *(int *)(gz + 0x38) = n;
    if (*(int *)(gz + 0x10) == 0 && *(int *)(gz + 0x38) != 1)
      done = 0;
    else
      done = 1;
  } while (*(int *)(gz + 0x38) == 0 || *(int *)(gz + 0x38) == 1);
  return -(unsigned int)(*(unsigned int *)(gz + 0x38) != 1) &
         *(unsigned int *)(gz + 0x38);
}

/* zlib gzflush: flush a gzip write-stream (+0x5c == 'w'). Calls FUN_00112f00
   (@eax=gz, flush) to deflate+write buffered data; if it succeeds (returns 0),
   flushes the underlying FILE* and returns gz+0x38 (z_err) — but maps z_err==1
   (Z_STREAM_END) to 0, leaving any other error code unchanged. */
unsigned int FUN_00112fc0(int gz, int flush)
{
  unsigned int uVar1;

  uVar1 = FUN_00112f00(gz, flush);
  if (uVar1 == 0) {
    crt_fflush(*(void **)(gz + 0x40));
    uVar1 = *(unsigned int *)(gz + 0x38);
    uVar1 = (uVar1 != 1u ? 0xffffffffu : 0u) & uVar1;
  }
  return uVar1;
}

/* 0x1130a0 — zlib gzio putLong: write a 32-bit value little-endian to a FILE*
 * (value @<eax>, file @<ebx>) as four putc (FUN_001db6c7) calls, LSB first.
 * @<eax>/@<ebx>-defined => VC71 cannot reproduce the reg-receiving prologue
 * (structural ceiling). */
void FUN_001130a0(unsigned int value, void *file)
{
  int i;

  i = 4;
  do {
    _fputc((int)(value & 0xff), file);
    value = value >> 8;
    i = i - 1;
  } while (i != 0);
}

/* 0x1130d0 — zlib gzio get_long: read a little-endian 32-bit value from a gzip
 * read-stream (gz @<eax>) as four get_byte (FUN_00112850) reads, LSB first. If
 * the final (MSB) byte returns EOF (-1), sets z_err (gz+0x38) = Z_DATA_ERROR
 * (-3). Returns b0 | b1<<8 | b2<<16 | b3<<24. @<eax>-defined => VC71 cannot
 * reproduce the EAX-receiving prologue (structural ceiling). */
int FUN_001130d0(int gz)
{
  int b0, b1, b2, b3;

  b0 = ((unsigned int(__fastcall*)(int))FUN_00112850)(gz);
  b1 = ((unsigned int(__fastcall*)(int))FUN_00112850)(gz);
  b2 = ((unsigned int(__fastcall*)(int))FUN_00112850)(gz);
  b3 = ((unsigned int(__fastcall*)(int))FUN_00112850)(gz);
  if (b3 == -1)
    *(int *)(gz + 0x38) = 0xfffffffd;
  return b0 + (b1 << 8) + (b2 << 0x10) + (b3 << 0x18);
}

/* 0x113110 — zlib gzclose: close a gzip stream. For a write-stream (gz+0x5c ==
 * 'w'), flushes the remaining output with Z_FINISH(4) via gz_flush
 * (FUN_00112f00); on success appends the gzip footer — the running CRC32
 * (gz+0x4c) then the uncompressed size (gz+0x8) — as little-endian 32-bit
 * values via putLong (FUN_001130a0) to the FILE* (gz+0x40). Then tears the
 * stream down via gz_destroy (FUN_00112cd0) and returns its result. A NULL gz
 * returns Z_STREAM_ERROR(-2). */
int FUN_00113110(int gz)
{
  if (gz == 0)
    return -2;
  if (*(char *)(gz + 0x5c) == 'w') {
    if (FUN_00112f00(gz, 4) == 0) {
      ((void(*)(void))FUN_001130a0)(*(unsigned int *)(gz + 0x4c), *(void **)(gz + 0x40));
      ((void(*)(void))FUN_001130a0)(*(unsigned int *)(gz + 8), *(void **)(gz + 0x40));
    }
  }
  return FUN_00112cd0(gz);
}

/* zlib gzrewind: reset a gzip read-stream (param_1[0x17]/+0x5c byte == 'r') to
   the start. Clears decode state, reinitializes crc, then either rewinds the
   underlying FILE* (+0x40) when there is no gzip header start offset (+0x60),
   or re-inits inflate (FUN_001153c0) and seeks to the header start. */
int FUN_00113000(int *param_1)
{
  int ret;

  if ((param_1 != (int *)0) && (*((char *)param_1 + 0x5c) == 'r')) {
    param_1[0xe] = 0;
    param_1[0xf] = 0;
    param_1[1] = 0;
    param_1[0] = param_1[0x11];
    param_1[0x13] =
      (int)FUN_00110c10(0, (void *)0, 0); /* crc32(0, Z_NULL, 0) */
    if (param_1[0x18] == 0) {
      _rewind((void *)param_1[0x10]); /* rewind(FILE*) */
      return 0;
    }
    FUN_001153c0((int)param_1); /* inflateReset */
    ret = _fseek((void *)param_1[0x10], param_1[0x18],
                       0); /* fseek(FILE*, off, SEEK_SET) */
    return ret;
  }
  return -1;
}

/* 0x113080 — zlib gz stream accessor: if the stream is non-null and in read
 * mode (mode byte at +0x5c == 'r'), return the field at +0x3c; else 0. */
unsigned int FUN_00113080(int param_1)
{
  if (param_1 != 0 && *(char *)(param_1 + 0x5c) == 'r') {
    return *(unsigned int *)(param_1 + 0x3c);
  }
  return 0;
}

/* 0x113160 — zlib gzerror: return the error string for a gz_stream, and store
 * the z_err code in *errnum.  If the stream pointer is null, stores Z_STREAM_ERROR
 * (-2) and returns the static "stream error" string.  If z_err is 0 returns the
 * empty string.  For z_err == -1 (Z_ERRNO) or when the stream's msg pointer is
 * absent/empty the error text is looked up from the static table at 0x320e10
 * indexed by -z_err.  Any previously allocated message buffer at +0x50 is freed
 * before a new one is allocated to hold path ": " errstr. */
char *FUN_00113160(int gz, int *errnum)
{
  int z_err;
  char *pcVar4;
  int iVar1;
  int iVar2;
  char *buf;

  if (gz == 0) {
    *errnum = -2;
    return *(char **)0x320e18;
  }
  z_err = *(int *)(gz + 0x38);
  *errnum = z_err;
  if (z_err == 0) {
    return (char *)0x25386f;
  }
  pcVar4 = *(char **)(gz + 0x18);
  if ((z_err == -1) || (pcVar4 == (char *)0) || (*pcVar4 == '\0')) {
    pcVar4 = ((char **)0x320e10)[-*(int *)(gz + 0x38)];
  }
  if (*(int *)(gz + 0x50) != 0) {
    debug_free(*(void **)(gz + 0x50), "c:\\halo\\SOURCE\\memory\\zlib\\gzio.c", 0x365);
  }
  iVar1 = csstrlen(*(char **)(gz + 0x54));
  iVar2 = csstrlen(pcVar4);
  buf = (char *)debug_malloc(iVar1 + 3 + iVar2, 0, "c:\\halo\\SOURCE\\memory\\zlib\\gzio.c", 0x366);
  *(void **)(gz + 0x50) = buf;
  csstrcpy(buf, *(char **)(gz + 0x54));
  FUN_0008dc30(*(char **)(gz + 0x50), (char *)0x28d3ec);
  FUN_0008dc30(*(char **)(gz + 0x50), pcVar4);
  return *(char **)(gz + 0x50);
}

/* 0x113480 — zlib gzopen(path, mode): open a gzip file by path. Forwards to the
 * internal open (FUN_00113230) with fd = -1 (path-based open; mode passed in
 * @<eax>). Returns the gzFile handle (or NULL). */
void *FUN_00113480(char *path, char *mode)
{
  return FUN_00113230(path, -1, mode);
}

/* zlib gzdopen(fd, mode): wrap an existing fd; synthesizes a "<fd:%d>" name. */
void *FUN_001134a0(int fd, char *mode)
{
  char name[20];

  if (fd < 0) {
    return (void *)0;
  }
  crt_sprintf(name, "<fd:%d>", fd);
  return FUN_00113230(name, fd, mode);
}

/* zlib gzgetc(file): read a single byte from a gzip read-stream. Reads 1 byte
   into a stack buffer via the gzread helper (FUN_001134e0); returns the byte
   zero-extended, or -1 on EOF/error (read count != 1). */
unsigned int FUN_00113710(int *file)
{
  unsigned char c;

  if (FUN_001134e0(file, &c, 1) == 1) {
    return (unsigned int)c;
  }
  return 0xffffffff;
}

/* zlib gzgets(file, buf, len): read up to len-1 bytes from gzip stream into
   buf, stopping at newline (inclusive) or EOF. Null-terminates the result.
   Returns buf if any bytes were written (or len==1), NULL on EOF with no
   bytes read or on invalid args. Faithful lift of FUN_00113740. */
char *FUN_00113740(int *file, char *buf, int len)
{
  char c;
  char *p;

  if (buf != (char *)0x0 && 0 < len) {
    p = buf;
    while (--len > 0) {
      if (FUN_001134e0(file, p, 1) != 1) break;
      c = *p;
      p = p + 1;
      if (c != '\n') continue;
      break;
    }
    *p = '\0';
    if (buf != p) {
      return buf;
    }
    if (len <= 0) {
      return buf;
    }
  }
  return (char *)0x0;
}

/* gzseek: seek within a gzip stream by 'offset' bytes with SEEK_SET(0) or
 * SEEK_CUR(1). SEEK_END(2) is not supported. For write streams, advances by
 * writing zeros. For read streams in transparent mode, delegates to fseek;
 * otherwise, reads and discards bytes to advance the position.
 * Returns the new stream position, or 0xffffffff on error. */
unsigned int FUN_001137a0(int *file, unsigned int offset, int whence)
{
    unsigned int uVar3;
    int iVar2;
    int *gz;
    void *uVar1;

    gz = file;

    if (gz == (int *)0x0) {
        return 0xffffffff;
    }
    if (whence == 2) {
        return 0xffffffff;
    }
    if (gz[0xe] == -1 || gz[0xe] == -3) {
        return 0xffffffff;
    }

    if (*(char *)((char *)gz + 0x5c) == 'w') {
        /* write mode */
        if (whence == 0) {
            offset = offset - (unsigned int)gz[2];
        }
        if ((int)offset < 0) {
            return 0xffffffff;
        }
        if (gz[0x11] == 0) {
            uVar1 = debug_malloc(0x4000, 0, "c:\\halo\\SOURCE\\memory\\zlib\\gzio.c", 0x2ab);
            gz[0x11] = (int)uVar1;
            csmemset(uVar1, 0, 0x4000);
        }
        while ((int)offset > 0) {
            uVar3 = 0x4000;
            if ((int)offset < 0x4000) {
                uVar3 = offset;
            }
            iVar2 = FUN_00112db0(gz, gz[0x11], (int)uVar3);
            if (iVar2 == 0) break;
            offset = offset - (unsigned int)iVar2;
        }
        return (unsigned int)gz[2];
    } else {
        /* read mode */
        if (whence == 1) {
            offset = offset + (unsigned int)gz[5];
        }
        if ((int)offset < 0) {
            return 0xffffffff;
        }
        if (gz[0x16] != 0) {
            /* transparent mode: use fseek directly */
            gz[1] = 0;
            gz[0] = gz[0x11];
            iVar2 = _fseek((void *)gz[0x10], (int)offset, 0);
            if (-1 < iVar2) {
                gz[5] = (int)offset;
                gz[2] = (int)offset;
                return offset;
            }
        } else {
        /* compressed mode: seek by rewinding then reading forward */
        if (offset >= (unsigned int)gz[5]) {
            offset = offset - (unsigned int)gz[5];
        } else {
            iVar2 = FUN_00113000(gz);
            if (iVar2 < 0) {
                return 0xffffffff;
            }
        }
        if (offset != 0) {
            if (gz[0x12] == 0) {
                uVar1 = debug_malloc(0x4000, 0, "c:\\halo\\SOURCE\\memory\\zlib\\gzio.c", 0x2d5);
                gz[0x12] = (int)uVar1;
            }
            for (; 0 < (int)offset; offset = offset - (unsigned int)iVar2) {
                uVar3 = 0x4000;
                if ((int)offset < 0x4000) {
                    uVar3 = offset;
                }
                iVar2 = FUN_001134e0(gz, (void *)gz[0x12], (int)uVar3);
                if (iVar2 < 1) {
                    return 0xffffffff;
                }
            }
        }
        return (unsigned int)gz[5];
        }
    }
    return 0xffffffff;
}

/* zlib gzseek wrapper: seek file to offset 0 with whence=1 (relative). */
int FUN_00113910(void *file)
{
  return FUN_001137a0(file, 0, 1);
}

typedef void *(*zlib_zalloc_fn)(void *, int, int);
typedef void  (*zlib_zfree_fn)(void *, void *);

/* 0x113930 — zlib inflate_blocks_reset: reset the inflate_blocks_state param_1
 * for a fresh block stream. Optionally writes the pending byte count to the OUT
 * param param_3, releases the current decoder mode (codes/codes-with-tree),
 * then clears window pointers, mode/bytes counters, and re-seeds the running
 * check value via the stream's check function (z_stream at param_2). */
void FUN_00113930(int s, int param_2, int last)
{
  int *param_1 = (int *)s;
  int *param_3 = (int *)last;
  int iVar1;

  if (param_3 != 0) {
    *param_3 = param_1[0xf];
  }
  if (param_1[0] == 4 || param_1[0] == 5) {
    (*(void (**)(int, int))(param_2 + 0x24))(*(int *)(param_2 + 0x28),
                                             param_1[3]);
  }
  if (param_1[0] == 6) {
    FUN_00114f60(param_1[1], param_2);
  }
  param_1[0xd] = param_1[10];
  param_1[0xc] = param_1[10];
  param_1[0] = 0;
  param_1[7] = 0;
  param_1[8] = 0;
  if (param_1[0xe] != 0) {
    iVar1 = ((int (*)(int, int, int))param_1[0xe])(0, 0, 0);
    param_1[0xf] = iVar1;
    *(int *)(param_2 + 0x30) = iVar1;
  }
  if (*(int *)0x320e30 > 0) {
    crt_fprintf((void *)0x331070, "inflate:   blocks reset\n");
  }
  return;
}

/* 0x1139d0 — zlib inflate_blocks_new: allocate and initialise a new
 * inflate_blocks_state for decompression. Allocates the state struct (0x40
 * bytes), the sliding window (wsize bytes), and the codes workspace (0x5a0
 * bytes) via z_stream's zalloc. On any allocation failure, frees already-
 * allocated buffers and returns NULL. On success, stores function pointers,
 * initialises mode=0, then calls inflate_blocks_reset to seed the checksum. */
void *FUN_001139d0(int z, int adler_fn, int wsize)
{
    int *s;

    s = (int *)((zlib_zalloc_fn)(*(int *)(z + 0x20)))(*(void **)(z + 0x28), 1, 0x40);
    if (s == 0) {
        return 0;
    }

    s[9] = (int)((zlib_zalloc_fn)(*(int *)(z + 0x20)))(*(void **)(z + 0x28), 8, 0x5a0);
    if (s[9] == 0) {
        ((zlib_zfree_fn)(*(int *)(z + 0x24)))(*(void **)(z + 0x28), s);
        return 0;
    }

    s[10] = (int)((zlib_zalloc_fn)(*(int *)(z + 0x20)))(*(void **)(z + 0x28), 1, wsize);
    if (s[10] == 0) {
        ((zlib_zfree_fn)(*(int *)(z + 0x24)))(*(void **)(z + 0x28), (void *)s[9]);
        ((zlib_zfree_fn)(*(int *)(z + 0x24)))(*(void **)(z + 0x28), s);
        return 0;
    }

    s[0xb] = s[10] + wsize;
    s[0xe] = adler_fn;
    s[0]   = 0;

    if (*(int *)0x320e30 > 0) {
        crt_fprintf((void *)0x331070, "inflate:   blocks allocated\n");
    }

    FUN_00113930((int)s, z, 0);
    return s;
}

/* 0x111220 — zlib deflateCopy: create an independent copy of a deflate stream.
 * Copies the z_stream header (14 dwords = 0x38 bytes) from source to dest, then
 * allocates a fresh deflate_state (0x16c0 bytes) via dest->zalloc.  The new
 * state is populated by bulk-copying the 0x16c0-byte source state, then
 * re-allocating and re-copying the three variable-length sub-buffers
 * (pending_buf at +0x30, d_buf at +0x38, l_buf at +0x3c) and the sliding
 * window (+0x8).  Internal pointers that track position within pending_buf and
 * within the window are fixed up so the new stream is self-consistent.  On any
 * allocation failure, calls deflateFreeState (FUN_00111170) to clean up and
 * returns Z_MEM_ERROR (-4).  Returns Z_STREAM_ERROR (-2) if either stream
 * pointer is NULL or if the source has no internal state. */
int FUN_00111220(int dest, int source)
{
    int *ds;        /* new deflate_state */
    int *ss;        /* source deflate_state */
    int w_size;     /* [ds+0x1694] window count for size calculations */
    int window_buf; /* new window allocation address */

    /* Validate: both stream pointers and source state must be non-NULL */
    if (source == 0) goto stream_error;
    if (dest   == 0) goto stream_error;
    ss = (int *)(*(int *)(source + 0x1c));
    if (ss == 0) goto stream_error;

    /* Copy z_stream header: 14 dwords (0x38 bytes) from source to dest */
    memcpy((void *)dest, (void *)source, 0xe * sizeof(int));

    /* Allocate a fresh deflate_state (0x16c0 bytes) */
    ds = (int *)((zlib_zalloc_fn)(*(int *)(dest + 0x20)))(*(void **)(dest + 0x28), 1, 0x16c0);
    if (ds == 0) return 0xfffffffc;

    /* Link new state into dest z_stream */
    *(int *)(dest + 0x1c) = (int)ds;

    /* Bulk-copy source deflate_state (0x5b0 dwords = 0x16c0 bytes) */
    memcpy(ds, ss, 0x5b0 * sizeof(int));

    /* Fix strm back-pointer: new state must point to dest, not source */
    ds[0] = dest;

    /* Re-allocate the four variable-length sub-buffers.
     * Results stored at the same byte offsets as in the source state. */
    *(int *)((char *)ds + 0x30) = (int)((zlib_zalloc_fn)(*(int *)(dest + 0x20)))(*(void **)(dest + 0x28), *(int *)((char *)ds + 0x24), 2);
    *(int *)((char *)ds + 0x38) = (int)((zlib_zalloc_fn)(*(int *)(dest + 0x20)))(*(void **)(dest + 0x28), *(int *)((char *)ds + 0x24), 2);
    *(int *)((char *)ds + 0x3c) = (int)((zlib_zalloc_fn)(*(int *)(dest + 0x20)))(*(void **)(dest + 0x28), *(int *)((char *)ds + 0x44), 2);
    w_size = *(int *)((char *)ds + 0x1694);
    *(int *)((char *)ds + 0x8)  = (int)((zlib_zalloc_fn)(*(int *)(dest + 0x20)))(*(void **)(dest + 0x28), w_size, 4);

    if (*(int *)((char *)ds + 0x30) != 0 &&
        *(int *)((char *)ds + 0x38) != 0 &&
        *(int *)((char *)ds + 0x3c) != 0 &&
        *(int *)((char *)ds + 0x8)  != 0) {

        /* Copy pending_buf (w_size * 2 bytes) */
        csmemcpy((void *)*(int *)((char *)ds + 0x30), (void *)*(int *)((char *)ss + 0x30), *(int *)((char *)ds + 0x24) << 1);
        /* Copy d_buf (w_size * 2 bytes) */
        csmemcpy((void *)*(int *)((char *)ds + 0x38), (void *)*(int *)((char *)ss + 0x38), *(int *)((char *)ds + 0x24) << 1);
        /* Copy l_buf (lit_bufsize * 2 bytes) */
        csmemcpy((void *)*(int *)((char *)ds + 0x3c), (void *)*(int *)((char *)ss + 0x3c), *(int *)((char *)ds + 0x44) << 1);
        /* Copy window ([ds+0xc] bytes) */
        csmemcpy((void *)*(int *)((char *)ds + 0x8),  (void *)*(int *)((char *)ss + 0x8),  *(int *)((char *)ds + 0xc));

        /* Fix pending_out (+0x10): translate source-relative offset into new window */
        window_buf = *(int *)((char *)ds + 0x8);
        *(int *)((char *)ds + 0x10) = (*(int *)((char *)ss + 0x10) - *(int *)((char *)ss + 0x8)) + window_buf;

        /* pending_buf_size (+0x1690) = window_buf + w_size*3 */
        *(int *)((char *)ds + 0x1690) = window_buf + w_size + w_size * 2;

        /* window_size (+0x169c) = window_buf + (w_size rounded down to even) */
        *(int *)((char *)ds + 0x169c) = window_buf + (w_size & 0xfffffffe);

        /* Fix intra-state pointers (all point into the deflate_state itself) */
        *(int *)((char *)ds + 0xb1c) = (int)((char *)ds + 0x980);
        *(int *)((char *)ds + 0xb10) = (int)((char *)ds + 0x8c);
        *(int *)((char *)ds + 0xb28) = (int)((char *)ds + 0xa74);

        return 0;
    }

    FUN_00111170(dest);
    return 0xfffffffc;
stream_error:
    return 0xfffffffe;
}

