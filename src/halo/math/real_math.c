/* FUN_0001ad60 (0x1ad60) — Euclidean distance between two 3D points.
 * Confirmed: cdecl, 2 pointer args. Pure FPU leaf (FSUB/FMUL/FADD/FSQRT). */
float FUN_0001ad60(float *a, float *b)
{
  float dx = a[0] - b[0];
  float dy = a[1] - b[1];
  float dz = a[2] - b[2];
  return sqrtf(dx * dx + dy * dy + dz * dz);
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

void component_vectors_from_normal3d(float *out_matrix, float *position,
                                     float *basis_data)
{
  typedef void (*quat_to_matrix_fn)(float *out, float *basis);

  ((quat_to_matrix_fn)0x1093b0)(out_matrix, basis_data);
  out_matrix[10] = position[0];
  out_matrix[11] = position[1];
  out_matrix[12] = position[2];
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
void matrix4x3_decompose(float *matrix, float *out_pos, float *out_forward,
                         float *out_up)
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

/* Interpolate two quaternions via slerp and normalize the result. */
void quaternions_interpolate_and_normalize(float *q1, float *q2, float t, float *out)
{
  FUN_0010ba90(q1, q2, t, out);
  sphere_intersects_rectangle3d(out);
}

/* Interpolate two orientations (quaternion[4] + scale/translation[4]).
 * Quaternion part is slerped, remaining 4 floats are linearly interpolated. */
void orientations_interpolate(float *orient1, float *orient2, float t, float *out)
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

  cos_pitch = cosf(angles[1]);
  out[0] = cosf(angles[0]) * cos_pitch;
  out[1] = sinf(angles[0]) * cos_pitch;
  out[2] = sinf(angles[1]);
}

/* Convert an angle to a 2D direction vector stored as (cos, sin, 0) (0x10cc70).
 */
void vector3d_from_angle(float *out, float angle)
{
  float c = cosf(angle);
  out[2] = 0.0f;
  out[0] = c;
  out[1] = sinf(angle);
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

/* 0x109010 — Scale a rectangle (short coords) by a fraction param_4/256,
 * clamping the aspect ratio to the smaller dimension of param_2. */
void FUN_00109010(short *param_1, short *param_2, uint32_t *param_3, short param_4)
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

/* 0x1093b0 — Convert a quaternion to a 3x3 rotation matrix (inside a 4x3 frame).
 * Sets scale=1.0 and translation=0.
 * Original keeps sq0 (s*q[0]) in 80-bit FPU while sq1/sq2 spill to float32. */
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

/* 0x1099a0 — Determinant of a 3x3 matrix (row-major, float[9]). */
float FUN_001099a0(float *m)
{
  return (m[8] * m[0] * m[4] +
          m[2] * m[3] * m[7] +
          m[6] * m[1] * m[5]) -
         m[0] * m[7] * m[5] -
         m[8] * m[1] * m[3] -
         m[6] * m[2] * m[4];
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

/* 0x109d90 — Transform a 3D vector by a 3x3 matrix. Supports b==out aliasing. */
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

/* 0x109e90 — Build a 4x3 rotation matrix from euler angles (yaw, pitch, roll). */
void FUN_00109e90(float *out, float yaw, float pitch, float roll)
{
  float cr, sr, sp, cy, sy;
  float cp_f;

  cr = cosf(roll);
  ((uint32_t *)out)[0] = 0x3f800000;
  ((uint32_t *)out)[10] = 0;
  ((uint32_t *)out)[11] = 0;
  ((uint32_t *)out)[12] = 0;
  sr = sinf(roll);
  cp_f = cosf(pitch);
  sp = sinf(pitch);
  cy = cosf(yaw);
  sy = sinf(yaw);
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

/* 0x10b5c0 — real_math_initialize: init random tables and periodic functions. */
void real_math_initialize(void)
{
  random_math_initialize();
  FUN_0010ad10();
}

/* 0x10b5d0 — real_math_dispose: dispose random tables and periodic functions. */
void real_math_dispose(void)
{
  random_math_dispose();
  FUN_0010a570();
}

/* 0x10b6b0 — Compute a perpendicular 4D vector (swizzle + negate). */
void perpendicular4d(float *in, float *out)
{
  out[0] = in[2];
  out[1] = in[3];
  out[2] = -in[0];
  out[3] = -in[1];
}

/* 0x10b780 — Linearly interpolate between two 3D vectors: out = a*(1-t) + b*t. */
void vectors_interpolate(float *a, float *b, float t, float *out)
{
  float one_minus_t = 1.0f - t;
  out[0] = t * b[0] + one_minus_t * a[0];
  out[1] = t * b[1] + one_minus_t * a[1];
  out[2] = t * b[2] + one_minus_t * a[2];
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
  out[0] = ww2_minus_1 * v[0] +
           dot2 * q[0] + (q[1] * v[2] - q[2] * v[1]) * w2;
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
    halt_and_catch_fire();
  }
  if (up == (float *)0) {
    display_assert("up", "c:\\halo\\SOURCE\\math\\real_math.c", 0x341, 1);
    halt_and_catch_fire();
  }
  if (angles == (float *)0) {
    display_assert("angles", "c:\\halo\\SOURCE\\math\\real_math.c", 0x342, 1);
    halt_and_catch_fire();
  }
  FUN_00109e90(matrix, angles[0], angles[1], angles[2]);
  matrix4x3_decompose(matrix, local_position, forward, up);
}

/* 0x10bd70 — Point-in-rectangle test (2D, fully inclusive). */
int FUN_0010bd70(float *point, float *rect)
{
  if (rect[0] <= point[0] && point[0] <= rect[1] &&
      rect[2] <= point[1] && point[1] <= rect[3]) {
    return 1;
  }
  return 0;
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

/* 0x10cab0 — Build a quaternion from an axis and angle.
 * out = (axis * sin(angle/2), cos(angle/2)). */
void FUN_0010cab0(float *out, float angle, float *axis)
{
  float s = sinf(angle * 0.5f);
  float c = cosf(angle * 0.5f);

  out[3] = c;
  out[0] = s * axis[0];
  out[1] = s * axis[1];
  out[2] = s * axis[2];
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
  float t_unclamped = ((p1[0] - p2[0]) * p3[0] +
                      (p1[1] - p2[1]) * p3[1] +
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
char FUN_0010d770(float *p1, float *p2, float *p3, float *p4,
                  float *out_u, float *out_v)
{
  float det = (p1[1] - p2[1]) * (p3[0] - p2[0]) -
              (p1[0] - p2[0]) * (p3[1] - p2[1]);
  float det2, total, inv_total;

  if (0.0f <= det) {
    det2 = (p1[0] - p2[0]) * (p4[1] - p2[1]) -
           (p1[1] - p2[1]) * (p4[0] - p2[0]);
    if (0.0f <= det2) {
      total = (p4[1] - p2[1]) * (p3[0] - p2[0]) -
              (p4[0] - p2[0]) * (p3[1] - p2[1]);
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
    display_assert("cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c", 0x57c, 1);
    halt_and_catch_fire();
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
    display_assert("cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c", 0x599, 1);
    halt_and_catch_fire();
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
    display_assert("cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c", 0x5b6, 1);
    halt_and_catch_fire();
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

/* 0x10d830 — 3D triangle barycentric coordinates: project to dominant
 * axis and compute barycentric (u,v). p1=point, p2,p3,p4=triangle vertices. */
char FUN_0010d830(float *p1, float *p2, float *p3, float *p4,
                  float *out_u, float *out_v)
{
  float v3[3], v1[3], v2[3];
  float n[3];
  float dot_n;
  uint32_t basis;
  uint8_t axis;
  float p1_proj[3];
  float v1_proj[3];
  float v2_proj[3];
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
  if (dot_n * dot_n < (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) * (*(float *)0x253f44)) {
    basis = FUN_00099220(n);
    axis = FUN_00099270(n, basis);
    FUN_00061df0(v1, basis, axis, p1_proj);
    FUN_00061df0(v3, basis, axis, v1_proj);
    det = v1[2] * p1_proj[0] - p1_proj[2] * v1_proj[0];
    if (det >= 0.0f) {
      FUN_00061df0(v2, basis, axis, v2_proj);
      det2 = v2_proj[2] * v1_proj[0] - v1[2] * v2_proj[0];
      if (det2 >= 0.0f) {
        total = v2_proj[2] * p1_proj[0] - p1_proj[2] * v2_proj[0];
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

/* 0x10e6f0 — 3D ray vs triangle intersection (Möller–Trumbore-like).
 * p1=ray_origin, p2=ray_direction, p3,p4,p5=triangle vertices,
 * p6=out_t. Returns 1 if ray hits triangle. */
char FUN_0010e6f0(float *p1, float *p2, float *p3, float *p4,
                  float *p5, float *p6)
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
  t = (origin_to_v0[0] * n[0] +
       origin_to_v0[1] * n[1] +
       origin_to_v0[2] * n[2]) * inv_det;
  if (t < 0.0f) return 0;
  if (t > 1.0f) return 0;

  cross_product3d(origin_to_v0, p2, scratch);
  u = (e2[0] * scratch[0] + scratch[1] * e2[1] + scratch[2] * e2[2]) * inv_det;
  if (u < 0.0f) return 0;
  if (u > 1.0f) return 0;
  v = -((e1[0] * scratch[0] + scratch[1] * e1[1] + scratch[2] * e1[2]) * inv_det);
  if (v < 0.0f) return 0;
  if (u + v > 1.0f) return 0;
  *p6 = t;
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
      local_c * (circle_center[1] - p4[1]) < 0.0f) {
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
 * tests each edge if the projected point falls outside that edge's half-plane. */
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
  if (0.0f < c12[0] * n[0] + c12[1] * n[1] +
             n[2] * (e12[1] * (center[0] - v1[0]) -
                     e12[0] * (center[1] - v1[1]))) {
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
      n[2] * (c20[1] * (center[0] - v2[0]) -
              c20[0] * (center[1] - v2[1])) < 0.0f) {
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
        n[2] * ((proj[1] - v1[1]) * e12[0] -
                e12[1] * (proj[0] - v1[0])) < 0.0f) {
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
          (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) * pill_radius *
              pill_radius)
        return 1;
      return 0;
    }
  } else {
    if (vector_intersects_pill3d(v2, e20, pill_start, pill_dir, pill_radius))
      return 1;
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
    if (c2 < 0.0f) return 0;
    c2 = -(sqrtf(c2) + bp);
    if (c2 <= a) return 0;
    t = c2 / a;
  }
  hit = 1;
  c2 = (t * p5[2] + (p4[2] - p1[2])) / (p2 * p2);
  if (0.0f <= c2) {
    if (c2 <= 1.0f) {
      if (0.0f <= bp) return 0;
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
  if (hit == 0) return 0;
check:
  if (p5[0] * p7[0] + p7[1] * p5[1] + p7[2] * p5[2] <= 0.0f) {
    return hit;
  }
  return 0;
}

/* 0x10d380 — Ray-sphere intersection.
 * p1=ray_origin, p2=sphere_radius, p3=sphere_center, p4=ray_direction.
 * Returns 1 if intersect, sets *out_t and *out_normal. */
char FUN_0010d380(float *p1, float p2, float *p3, float *p4,
                  float *out_t, float *out_normal)
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

/* 0x1104e0 — 3D capsule vs cone test (calls FUN_0010dbf0 for cone test). */
char FUN_001104e0(float *p1, float p2, float *p3, float *p4, float p5,
                  float sine, float cosine)
{
  float dot;

  if (sine <= 0.0f || cosine < 0.0f) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_sine_cosine(%f, %f)",
             "sine>0.0f && cosine>=0.0f",
             "c:\\halo\\SOURCE\\math\\real_math.c", 0x8dd, 1);
    display_assert("sine>0.0f && cosine>=0.0f",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x8dd, 1);
    halt_and_catch_fire();
  }
  {
    float diff = (cosine * cosine + sine * sine) - 1.0f;
    if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000 ||
        *(double *)0x2549d8 <= fabsf(diff)) {
      csprintf((char *)0x5ab100, "%s, %s: assert_valid_real_sine_cosine(%f, %f)",
               "sine", "cosine",
               "c:\\halo\\SOURCE\\math\\real_math.c", 0x8de, 1);
      display_assert("...", "c:\\halo\\SOURCE\\math\\real_math.c", 0x8de, 1);
      halt_and_catch_fire();
    }
  }
  dot = (p1[1] - p3[1]) * p4[1] +
        (p1[2] - p3[2]) * p4[2] + (p1[0] - p3[0]) * p4[0];
  if (-p2 <= dot) {
    if (dot <= p2 + p5 && FUN_0010dbf0(p1, p3, p4, p2 + p5 - dot + p2, cosine)) {
      return 1;
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
             "sine>0.0f && cosine>=0.0f",
             "c:\\halo\\SOURCE\\math\\real_math.c", 0x8b7, 1);
    display_assert("sine>0.0f && cosine>=0.0f",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x8b7, 1);
    halt_and_catch_fire();
  }
  {
    float diff = (cosine * cosine + sine * sine) - 1.0f;
    if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000 ||
        *(double *)0x2549d8 <= fabsf(diff)) {
      csprintf((char *)0x5ab100, "%s, %s: assert_valid_real_sine_cosine(%f, %f)",
               "sine", "cosine",
               "c:\\halo\\SOURCE\\math\\real_math.c", 0x8b8, 1);
      display_assert("...", "c:\\halo\\SOURCE\\math\\real_math.c", 0x8b8, 1);
      halt_and_catch_fire();
    }
  }
  dot = (p1[1] - p3[1]) * p4[1] + (p1[0] - p3[0]) * p4[0];
  if (-p2 <= dot) {
    if (dot <= p2 + p5 && FUN_0010db50(p1, p3, p4, p2 + p5 - dot + p2, cosine)) {
      return 1;
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
             "sine>0.0f && cosine>=0.0f",
             "c:\\halo\\SOURCE\\math\\real_math.c", 0x897, 1);
    display_assert("sine>0.0f && cosine>=0.0f",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x897, 1);
    halt_and_catch_fire();
  }
  {
    float diff = (cosine * cosine + sine * sine) - 1.0f;
    if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000 ||
        *(double *)0x2549d8 <= fabsf(diff)) {
      csprintf((char *)0x5ab100, "%s, %s: assert_valid_real_sine_cosine(%f, %f)",
               "sine", "cosine",
               "c:\\halo\\SOURCE\\math\\real_math.c", 0x898, 1);
      display_assert("...", "c:\\halo\\SOURCE\\math\\real_math.c", 0x898, 1);
      halt_and_catch_fire();
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
             "sine>0.0f && cosine>=0.0f",
             "c:\\halo\\SOURCE\\math\\real_math.c", 0x877, 1);
    display_assert("sine>0.0f && cosine>=0.0f",
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x877, 1);
    halt_and_catch_fire();
  }
  {
    float diff = (cosine * cosine + sine * sine) - 1.0f;
    if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000 ||
        *(double *)0x2549d8 <= fabsf(diff)) {
      csprintf((char *)0x5ab100, "%s, %s: assert_valid_real_sine_cosine(%f, %f)",
               "sine", "cosine",
               "c:\\halo\\SOURCE\\math\\real_math.c", 0x878, 1);
      display_assert("...", "c:\\halo\\SOURCE\\math\\real_math.c", 0x878, 1);
      halt_and_catch_fire();
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

/* 0x10bff0 — 3D ray vs AABB intersection (slab method).
 * ray_origin=p1, ray_dir=p2, aabb=p3 (xmin,xmax,ymin,ymax,zmin,zmax). */
char FUN_0010bff0(float *ray_origin, float *ray_dir, float *aabb)
{
  float tmin = -3.4028235e+38f;
  float tmax = 3.4028235e+38f;
  float t1, t2;

  if (fabsf(ray_dir[0]) < *(double *)0x2533d0) {
    if (ray_origin[0] < aabb[0]) return 0;
    if (ray_origin[0] > aabb[1]) return 0;
  } else {
    t1 = (aabb[0] - ray_origin[0]) * (1.0f / ray_dir[0]);
    t2 = (aabb[1] - ray_origin[0]) * (1.0f / ray_dir[0]);
    if (ray_dir[0] <= 0.0f) {
      if (t2 > -3.4028235e+38f) tmin = t2;
      if (t1 < 3.4028235e+38f) tmax = t1;
    } else {
      if (t1 > -3.4028235e+38f) tmin = t1;
      if (t2 < 3.4028235e+38f) tmax = t2;
    }
    if (tmin > tmax) return 0;
  }

  if (fabsf(ray_dir[1]) < *(double *)0x2533d0) {
    if (ray_origin[1] < aabb[2]) return 0;
    if (ray_origin[1] > aabb[3]) return 0;
  } else {
    t1 = (aabb[2] - ray_origin[1]) * (1.0f / ray_dir[1]);
    t2 = (aabb[3] - ray_origin[1]) * (1.0f / ray_dir[1]);
    if (ray_dir[1] <= 0.0f) {
      if (tmin < t2) tmin = t2;
      if (t1 < tmax) tmax = t1;
    } else {
      if (tmin < t1) tmin = t1;
      if (t2 < tmax) tmax = t2;
    }
    if (tmin > tmax) return 0;
  }

  if (fabsf(ray_dir[2]) < *(double *)0x2533d0) {
    if (ray_origin[2] < aabb[4]) return 0;
    if (ray_origin[2] > aabb[5]) return 0;
  } else {
    t1 = (aabb[4] - ray_origin[2]) * (1.0f / ray_dir[2]);
    t2 = (aabb[5] - ray_origin[2]) * (1.0f / ray_dir[2]);
    if (ray_dir[2] <= 0.0f) {
      if (tmin < t2) tmin = t2;
      if (t1 < tmax) tmax = t1;
    } else {
      if (tmin < t1) tmin = t1;
      if (t2 < tmax) tmax = t2;
    }
    if (tmin > tmax) return 0;
  }

  if (0.0f <= tmax && tmin <= 1.0f) return 1;
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
    if (ray_origin[0] < aabb[0]) return 0;
    if (ray_origin[0] > aabb[1]) return 0;
  } else {
    t1 = (aabb[0] - ray_origin[0]) * (1.0f / ray_dir[0]);
    t2 = (aabb[1] - ray_origin[0]) * (1.0f / ray_dir[0]);
    if (ray_dir[0] <= 0.0f) {
      if (t2 > -3.4028235e+38f) tmin = t2;
      if (t1 < 3.4028235e+38f) tmax = t1;
    } else {
      if (t1 > -3.4028235e+38f) tmin = t1;
      if (t2 < 3.4028235e+38f) tmax = t2;
    }
    if (tmin > tmax) return 0;
  }

  if (fabsf(ray_dir[1]) < *(double *)0x2533d0) {
    if (ray_origin[1] < aabb[2]) return 0;
    if (ray_origin[1] > aabb[3]) return 0;
  } else {
    t1 = (aabb[2] - ray_origin[1]) * (1.0f / ray_dir[1]);
    t2 = (aabb[3] - ray_origin[1]) * (1.0f / ray_dir[1]);
    if (ray_dir[1] <= 0.0f) {
      if (tmin < t2) tmin = t2;
      if (t1 < tmax) tmax = t1;
    } else {
      if (tmin < t1) tmin = t1;
      if (t2 < tmax) tmax = t2;
    }
    if (tmin > tmax) return 0;
  }

  if (0.0f <= tmax && tmin <= 1.0f) return 1;
  return 0;
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

/* 0x10bdc0 — Point-in-3D-box test (inclusive on both ends). */
int FUN_0010bdc0(float *point, float *box)
{
  if (box[0] <= point[0] && point[0] <= box[1] &&
      box[2] <= point[1] && point[1] <= box[3] &&
      box[4] <= point[2] && point[2] <= box[5]) {
    return 1;
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
    display_assert("cosine>=0.0f", "c:\\halo\\SOURCE\\math\\real_math.c", 0x55f, 1);
    halt_and_catch_fire();
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

/* 0x10f310 — Intersect three planes (homogeneous): solve for the point
 * that lies on all three planes. Returns 0 if planes are parallel/degenerate. */
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
  out[0] = ((cross_out[1] * p1[2] - p1[1] * cross_out[2]) * d + out[0]) * inv_det;
  out[1] = ((p1[0] * cross_out[2] - p1[2] * cross_out[0]) * d + out[1]) * inv_det;
  out[2] = ((cross_out[0] * p1[1] - cross_out[1] * p1[0]) * d + out[2]) * inv_det;
  return 1;
}

/* 0x10fe80 — Validate 2D normal: x²+y² close to 1.0 and not NaN/Inf. */
int FUN_0010fe80(float x, float y)
{
  float diff = (y * y + x * x) - 1.0f;
  if ((*(unsigned int *)&diff & 0x7f800000) != 0x7f800000 &&
      fabsf(diff) < *(float *)0x2549d8) {
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
    csprintf((char *)0x5ab100,
             "%s: assert_valid_real_normal3d(%f, %f, %f)", "normal",
             (double)normal[0], (double)normal[1], (double)normal[2]);
    display_assert((char *)0x5ab100,
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x203, 1);
    system_exit(-1);
  }
  if (!valid_real_normal3d(direction)) {
    csprintf((char *)0x5ab100,
             "%s: assert_valid_real_normal3d(%f, %f, %f)", "direction",
             (double)direction[0], (double)direction[1], (double)direction[2]);
    display_assert((char *)0x5ab100,
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x204, 1);
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
    csprintf((char *)0x5ab100,
             "%s: assert_valid_real_normal3d(%f, %f, %f)", "result",
             (double)result[0], (double)result[1], (double)result[2]);
    display_assert((char *)0x5ab100,
                   "c:\\halo\\SOURCE\\math\\real_math.c", 0x21c, 1);
    system_exit(-1);
  }
  return 1;
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

/* Transform a plane by a matrix. Computes new plane distance and normal. */
void FUN_0010a240(float *matrix, float *plane, int out)
{
  float d;

  if (*matrix == 0.0f) {
    *(int *)(out + 0xc) = 0;
    real_matrix3x3_transform_vector(matrix, (void *)plane, (void *)out);
    return;
  }
  d = plane[3] - (matrix[10] * plane[0] + matrix[11] * plane[1] +
                   matrix[12] * plane[2]);
  *(float *)(out + 0xc) = d;
  if (*matrix != 1.0f) {
    *(float *)(out + 0xc) = d / *matrix;
  }
  real_matrix3x3_transform_vector(matrix, (void *)plane, (void *)out);
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

/* Initialize a vector tree structure (k-d tree for spatial lookups). */
void FUN_00110730(int *param_1, short param_2, int param_3, int param_4,
                  int param_5)
{
  if (param_1 == (int *)0) {
    display_assert("tree", "c:\\halo\\SOURCE\\math\\vector_tree.c", 0x2b, 1);
    system_exit(-1);
  }
  if (param_4 == 0) {
    display_assert("get_vector", "c:\\halo\\SOURCE\\math\\vector_tree.c",
                   0x2c, 1);
    system_exit(-1);
  }
  if (param_5 == 0) {
    display_assert("compare_component",
                   "c:\\halo\\SOURCE\\math\\vector_tree.c", 0x2d, 1);
    system_exit(-1);
  }
  if (param_2 < 1) {
    display_assert("component_count>0",
                   "c:\\halo\\SOURCE\\math\\vector_tree.c", 0x2e, 1);
    system_exit(-1);
  }
  FUN_00117b20(param_1 + 1, 0x10);
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

