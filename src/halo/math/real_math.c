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
