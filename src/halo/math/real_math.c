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
 *   [1..9]    3x3 rotation (row-major)
 *   [10..12]  translation
 *
 * The original uses SSE1 (MULPS/ADDPS/SHUFPS) for rotation and translation,
 * then x87 for the scale multiply. */
#ifndef _MSC_VER
#include <xmmintrin.h>

__attribute__((target("sse"), noinline))
void matrix4x3_multiply(float *a, float *b, float *out)
{
  float *ra = a + 1;
  float *rb = b + 1;
  float *ro = out + 1;

  __m128 a_row0 = _mm_loadh_pi(_mm_load_ss(&ra[0]), (const __m64 *)&ra[1]);
  __m128 a_row1 = _mm_loadh_pi(_mm_load_ss(&ra[3]), (const __m64 *)&ra[4]);
  /* a_row2_t shuffle 0x36 → [ra[6],0,ra[7],ra[8]]; load directly instead */
  __m128 a_row2 = _mm_loadh_pi(_mm_load_ss(&ra[6]), (const __m64 *)&ra[7]);

  __m128 bc, r;

  bc = _mm_set1_ps(rb[0]);
  r = _mm_mul_ps(bc, a_row0);
  bc = _mm_set1_ps(rb[1]);
  r = _mm_add_ps(r, _mm_mul_ps(bc, a_row1));
  bc = _mm_set1_ps(rb[2]);
  r = _mm_add_ps(r, _mm_mul_ps(bc, a_row2));
  _mm_store_ss(&ro[0], r);
  _mm_storeh_pi((__m64 *)&ro[1], r);

  bc = _mm_set1_ps(rb[3]);
  r = _mm_mul_ps(bc, a_row0);
  bc = _mm_set1_ps(rb[4]);
  r = _mm_add_ps(r, _mm_mul_ps(bc, a_row1));
  bc = _mm_set1_ps(rb[5]);
  r = _mm_add_ps(r, _mm_mul_ps(bc, a_row2));
  _mm_store_ss(&ro[3], r);
  _mm_storeh_pi((__m64 *)&ro[4], r);

  bc = _mm_set1_ps(rb[6]);
  r = _mm_mul_ps(bc, a_row0);
  bc = _mm_set1_ps(rb[7]);
  r = _mm_add_ps(r, _mm_mul_ps(bc, a_row1));
  bc = _mm_set1_ps(rb[8]);
  r = _mm_add_ps(r, _mm_mul_ps(bc, a_row2));
  r = _mm_shuffle_ps(r, r, 0x8F);
  _mm_storeh_pi((__m64 *)&ro[6], r);
  _mm_store_ss(&ro[8], r);

  bc = _mm_set1_ps(rb[9]);
  r = _mm_mul_ps(bc, a_row0);
  bc = _mm_set1_ps(rb[10]);
  r = _mm_add_ps(r, _mm_mul_ps(bc, a_row1));
  bc = _mm_set1_ps(rb[11]);
  r = _mm_add_ps(r, _mm_mul_ps(bc, a_row2));

  __m128 a_trans = _mm_loadh_pi(_mm_load_ss(&ra[9]), (const __m64 *)&ra[10]);
  __m128 scale = _mm_set1_ps(a[0]);
  r = _mm_add_ps(_mm_mul_ps(r, scale), a_trans);
  _mm_store_ss(&ro[9], r);
  _mm_storeh_pi((__m64 *)&ro[10], r);

  out[0] = a[0] * b[0];
}
#else
__declspec(noinline)
void matrix4x3_multiply(float *a, float *b, float *out)
{
  float a1 = a[1], a2 = a[2], a3 = a[3];
  float a4 = a[4], a5 = a[5], a6 = a[6];
  float a7 = a[7], a8 = a[8], a9 = a[9];

  out[1] = b[1]*a1 + b[2]*a4 + b[3]*a7;
  out[2] = b[1]*a2 + b[2]*a5 + b[3]*a8;
  out[3] = b[1]*a3 + b[2]*a6 + b[3]*a9;
  out[4] = b[4]*a1 + b[5]*a4 + b[6]*a7;
  out[5] = b[4]*a2 + b[5]*a5 + b[6]*a8;
  out[6] = b[4]*a3 + b[5]*a6 + b[6]*a9;
  out[7] = b[7]*a1 + b[8]*a4 + b[9]*a7;
  out[8] = b[7]*a2 + b[8]*a5 + b[9]*a8;
  out[9] = b[7]*a3 + b[8]*a6 + b[9]*a9;

  {
    float scale = a[0];
    float t0 = b[10]*a1 + b[11]*a4 + b[12]*a7;
    float t1 = b[10]*a2 + b[11]*a5 + b[12]*a8;
    float t2 = b[10]*a3 + b[11]*a6 + b[12]*a9;
    out[10] = t0 * scale + a[10];
    out[11] = t1 * scale + a[11];
    out[12] = t2 * scale + a[12];
  }

  out[0] = a[0] * b[0];
}
#endif

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
