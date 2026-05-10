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

void FUN_001094d0(float *out_matrix, float *position, float *basis_data)
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
 *   [1..9]    3x3 rotation (row-major: row0=forward[1..3], row1=left[4..6], row2=up[7..9])
 *   [10..12]  translation
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
  /* All __m128 declarations hoisted to the top for C89 (MSVC 7.1) compatibility. */
  float *a_rows;  /* &a[1] */
  float *b_rows;  /* &b[1] */
  float *out_rows;  /* &out[1] */
  __m128 row0, row1, row2;
  __m128 xmm3, xmm4, xmm5, xmm6, xmm7;
  __m128 t3, t4, t5, a_trans, scale_v;

  a_rows   = a + 1;
  b_rows   = b + 1;
  out_rows = out + 1;

  /* Load a's 3 rotation rows into XMM registers.
   * Pattern: MOVSS loads first component to lane 0 (lanes 1-3 zeroed),
   * MOVHPS loads 2nd and 3rd components to lanes 2 and 3. Layout: [x,0,y,z].
   *
   * XMM0 = row0 (forward): [a[1], 0, a[2], a[3]]
   * XMM1 = row1 (left):    [a[4], 0, a[5], a[6]]
   * XMM2 = row2 (up):      loaded reversed, then SHUFPS 0x36 to [a[7],0,a[8],a[9]] */

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
  xmm3 = _mm_mul_ps(xmm3, row0);  /* b[1]*row0 */
  xmm4 = _mm_mul_ps(xmm4, row1);  /* b[2]*row1 */
  xmm5 = _mm_mul_ps(xmm5, row2);  /* b[3]*row2 */
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
  xmm6 = _mm_mul_ps(xmm6, row0);  /* b[4]*row0 */
  xmm7 = _mm_mul_ps(xmm7, row1);  /* b[5]*row1 */
  xmm5 = _mm_mul_ps(xmm5, row2);  /* b[6]*row2 */
  xmm6 = _mm_add_ps(xmm6, xmm7);
  xmm6 = _mm_add_ps(xmm6, xmm5);
  /* xmm6 = out_row1 = [out[4], 0, out[5], out[6]] */

  /* Compute output row 2: b[7]*row0 + b[8]*row1 + b[9]*row2. */
  xmm7 = _mm_shuffle_ps(_mm_load_ss(&b_rows[6]), _mm_load_ss(&b_rows[6]), 0);
  xmm4 = _mm_shuffle_ps(_mm_load_ss(&b_rows[7]), _mm_load_ss(&b_rows[7]), 0);
  xmm5 = _mm_shuffle_ps(_mm_load_ss(&b_rows[8]), _mm_load_ss(&b_rows[8]), 0);
  xmm7 = _mm_mul_ps(xmm7, row0);  /* b[7]*row0 */
  xmm4 = _mm_mul_ps(xmm4, row1);  /* b[8]*row1 */
  xmm5 = _mm_mul_ps(xmm5, row2);  /* b[9]*row2 */
  xmm7 = _mm_add_ps(xmm7, xmm4);
  xmm7 = _mm_add_ps(xmm7, xmm5);
  /* xmm7 = out_row2 = [out[7], 0, out[8], out[9]] before shuffle */

  /* Store output row 1: MOVSS → out[4]; MOVHPS → out[5],out[6]. */
  _mm_store_ss(&out_rows[3], xmm6);
  _mm_storeh_pi((__m64 *)&out_rows[4], xmm6);

  /* SHUFPS 0x8f reorders row2 result for MOVHPS/MOVSS output layout.
   * 0x8f = 0b10_00_11_11: result[0]=src[3], result[1]=src[3], result[2]=src[0], result[3]=src[2].
   * Before: [out[7], 0, out[8], out[9]].
   * After:  [out[9], out[9], out[7], out[8]].
   * MOVHPS stores lanes [2,3] → out[7],out[8]; MOVSS stores lane [0] → out[9]. */
  xmm7 = _mm_shuffle_ps(xmm7, xmm7, 0x8f);

  /* Store output row 2: MOVHPS → out[7],out[8]; MOVSS → out[9]. */
  _mm_storeh_pi((__m64 *)&out_rows[6], xmm7);
  _mm_store_ss(&out_rows[8], xmm7);

  /* Translation row: (b_translation * a_rotation) * a_scale + a_translation.
   * b[10..12] each broadcast and multiplied against the 3 rotation rows. */
  t3 = _mm_shuffle_ps(_mm_load_ss(&b[10]), _mm_load_ss(&b[10]), 0);
  t4 = _mm_shuffle_ps(_mm_load_ss(&b[11]), _mm_load_ss(&b[11]), 0);
  t5 = _mm_shuffle_ps(_mm_load_ss(&b[12]), _mm_load_ss(&b[12]), 0);
  t3 = _mm_mul_ps(t3, row0);  /* b[10]*row0 */
  t4 = _mm_mul_ps(t4, row1);  /* b[11]*row1 */
  t5 = _mm_mul_ps(t5, row2);  /* b[12]*row2 */
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
void FUN_0010a150(float *mat0, float *mat1, float *out_vec3)
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
  out_plane[3] = in_plane[3] * matrix[0]
               + matrix[10] * out_plane[0]
               + matrix[11] * out_plane[1]
               + matrix[12] * out_plane[2];
}

void real_math_reset_precision(void)
{
  __control87(0x9001f, 0xfffff);
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

/* Test whether a line segment intersects a sphere. Returns true if
   the segment origin is inside the sphere or if the segment crosses
   the sphere boundary within t in [0,1]. */
bool FUN_0010bc70(float *line_start, float *line_end, float *sphere_center,
                  float sphere_radius)
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

float FUN_0010c600(float *a, float *b)
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
void FUN_0010ca30(float *quaternion)
{
  float mag_sq;

  mag_sq = quaternion[3] * quaternion[3] +
           quaternion[2] * quaternion[2] +
           quaternion[1] * quaternion[1] +
           quaternion[0] * quaternion[0];

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

/* FUN_0010e040 (0x10e040) — Test if two line segments are within a given
 * radius. Computes closest points between segments A (start_a + s*dir_a, s in
 * [0,1]) and B (start_b + t*dir_b, t in [0,1]). Returns true if distance <
 * radius. */
bool FUN_0010e040(float *start_a, float *dir_a, float *start_b, float *dir_b,
                  float radius)
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
    if (FUN_0010bc70(start_b, dir_b, &closest_a_x, radius))
      return 1;
  }
  if (t_oob) {
    if (FUN_0010bc70(start_a, dir_a, &closest_b_x, radius))
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
