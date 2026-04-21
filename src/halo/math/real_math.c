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

/* Transform a 3D vector by the rotation part of a 4x3 matrix, normalizing by
 * the scale factor. If scale != 1.0, the input vector is divided by scale
 * before the rotation is applied.
 * Matrix layout: [scale +0x00][3x3 rotation +0x04..+0x24].
 * out[i] = dot(normalized_vec, column_i of rotation). */
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

static float matrix4x3_round_to_float(float value)
{
  volatile float rounded = value;
  return rounded;
}

/* Multiply two 4x3 matrices: out = b * a.
 * Matrix layout (13 floats each):
 *   [0]       scale
 *   [1..9]    3x3 rotation (row-major)
 *   [10..12]  translation
 *
 * Result:
 *   out_rotation    = b_rotation * a_rotation
 *   out_translation = (b_translation * a_rotation) * a_scale + a_translation
 *   out_scale       = a_scale * b_scale
 *
 * The original uses SSE (MULPS/ADDPS/SHUFPS) for the 3x3 and translation
 * parts, then x87 FLD/FMUL/FSTP for the scale multiply.
 * round_to_float forces intermediate results to 32-bit precision to match
 * the original SSE arithmetic (Clang keeps intermediates in x87 80-bit
 * registers under -mno-sse). */
void matrix4x3_multiply(float *a, float *b, float *out)
{
  float a1 = a[1], a2 = a[2], a3 = a[3];
  float a4 = a[4], a5 = a[5], a6 = a[6];
  float a7 = a[7], a8 = a[8], a9 = a[9];
  float b1 = b[1], b2 = b[2], b3 = b[3];
  float b4 = b[4], b5 = b[5], b6 = b[6];
  float b7 = b[7], b8 = b[8], b9 = b[9];
  float p0, p1, p2, scale, b10, b11, b12;

  p0 = matrix4x3_round_to_float(b1 * a1);
  p1 = matrix4x3_round_to_float(b2 * a4);
  p2 = matrix4x3_round_to_float(b3 * a7);
  out[1] = matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2);
  p0 = matrix4x3_round_to_float(b1 * a2);
  p1 = matrix4x3_round_to_float(b2 * a5);
  p2 = matrix4x3_round_to_float(b3 * a8);
  out[2] = matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2);
  p0 = matrix4x3_round_to_float(b1 * a3);
  p1 = matrix4x3_round_to_float(b2 * a6);
  p2 = matrix4x3_round_to_float(b3 * a9);
  out[3] = matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2);
  p0 = matrix4x3_round_to_float(b4 * a1);
  p1 = matrix4x3_round_to_float(b5 * a4);
  p2 = matrix4x3_round_to_float(b6 * a7);
  out[4] = matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2);
  p0 = matrix4x3_round_to_float(b4 * a2);
  p1 = matrix4x3_round_to_float(b5 * a5);
  p2 = matrix4x3_round_to_float(b6 * a8);
  out[5] = matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2);
  p0 = matrix4x3_round_to_float(b4 * a3);
  p1 = matrix4x3_round_to_float(b5 * a6);
  p2 = matrix4x3_round_to_float(b6 * a9);
  out[6] = matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2);
  p0 = matrix4x3_round_to_float(b7 * a1);
  p1 = matrix4x3_round_to_float(b8 * a4);
  p2 = matrix4x3_round_to_float(b9 * a7);
  out[7] = matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2);
  p0 = matrix4x3_round_to_float(b7 * a2);
  p1 = matrix4x3_round_to_float(b8 * a5);
  p2 = matrix4x3_round_to_float(b9 * a8);
  out[8] = matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2);
  p0 = matrix4x3_round_to_float(b7 * a3);
  p1 = matrix4x3_round_to_float(b8 * a6);
  p2 = matrix4x3_round_to_float(b9 * a9);
  out[9] = matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2);

  b10 = b[10];
  b11 = b[11];
  b12 = b[12];
  scale = a[0];
  p0 = matrix4x3_round_to_float(b10 * a1);
  p1 = matrix4x3_round_to_float(b11 * a4);
  p2 = matrix4x3_round_to_float(b12 * a7);
  out[10] = matrix4x3_round_to_float(
    matrix4x3_round_to_float(
      matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2) *
      scale) +
    a[10]);
  p0 = matrix4x3_round_to_float(b10 * a2);
  p1 = matrix4x3_round_to_float(b11 * a5);
  p2 = matrix4x3_round_to_float(b12 * a8);
  out[11] = matrix4x3_round_to_float(
    matrix4x3_round_to_float(
      matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2) *
      scale) +
    a[11]);
  p0 = matrix4x3_round_to_float(b10 * a3);
  p1 = matrix4x3_round_to_float(b11 * a6);
  p2 = matrix4x3_round_to_float(b12 * a9);
  out[12] = matrix4x3_round_to_float(
    matrix4x3_round_to_float(
      matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2) *
      scale) +
    a[12]);

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
