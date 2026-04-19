/* Compute the inverse of a 4x3 matrix (scale + rotation + translation).
 * Intermediates use double to prevent precision loss from x87 register spills. */
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

static float matrix4x3_round_to_float(float value)
{
  volatile float rounded = value;
  return rounded;
}

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

  b10 = b[10]; b11 = b[11]; b12 = b[12]; scale = a[0];
  p0 = matrix4x3_round_to_float(b10 * a1);
  p1 = matrix4x3_round_to_float(b11 * a4);
  p2 = matrix4x3_round_to_float(b12 * a7);
  out[10] = matrix4x3_round_to_float(
    matrix4x3_round_to_float(
      matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2) *
      scale) + a[10]);
  p0 = matrix4x3_round_to_float(b10 * a2);
  p1 = matrix4x3_round_to_float(b11 * a5);
  p2 = matrix4x3_round_to_float(b12 * a8);
  out[11] = matrix4x3_round_to_float(
    matrix4x3_round_to_float(
      matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2) *
      scale) + a[11]);
  p0 = matrix4x3_round_to_float(b10 * a3);
  p1 = matrix4x3_round_to_float(b11 * a6);
  p2 = matrix4x3_round_to_float(b12 * a9);
  out[12] = matrix4x3_round_to_float(
    matrix4x3_round_to_float(
      matrix4x3_round_to_float(matrix4x3_round_to_float(p0 + p1) + p2) *
      scale) + a[12]);

  out[0] = a[0] * b[0];
}
