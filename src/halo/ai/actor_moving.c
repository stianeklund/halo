
/* 0x2b5d0 — FUN_0002b5d0: initialize trigonometric lookup tables.
 *
 * Confirmed: no arguments, no calls, writes table blocks rooted at
 * 0x6327e0 and 0x6325c0 using constants/tables at 0x25577c..0x25581c.
 * Confirmed: first loop runs 9 iterations (EDX=9), stride 0x1c (7 floats).
 * Confirmed: second stage runs 2 outer iterations × 8 inner iterations,
 * with destination stride 0x38 (14 floats) per inner iteration.
 */
void FUN_0002b5d0(void)
{
  float(*table_a)[7] = (float(*)[7])0x6327e0;
  float(*table_b)[7] = (float(*)[7])0x6325c0;
  float(*basis)[3] = (float(*)[3])0x632780;

  const float *angle_table_9 = (const float *)0x2557cc;
  const float *scale_table_9 = (const float *)0x2557a8;
  const float *length_table_9 = (const float *)0x255784;
  const float k_angle = *(const float *)0x255780;
  const float k_length = *(const float *)0x25577c;
  const float k_base = *(const float *)0x255778;

  const float *outer_angles = (const float *)0x25581c;
  const float *outer_scales = (const float *)0x255814;
  const float *inner_angles = (const float *)0x2557f4;
  const float k_inner_base = *(const float *)0x2557f0;

  for (int i = 0; i < 9; i++) {
    float angle = angle_table_9[i];
    float sin_angle = sinf(angle);
    float cos_angle = cosf(angle);
    float scaled_angle = k_angle * scale_table_9[i];
    float sin_scaled = sinf(scaled_angle);
    float scaled_len = k_length * length_table_9[i];

    table_a[i][0] = k_base;
    table_a[i][1] = 0.0f;
    table_a[i][2] = scaled_len * cos_angle;
    table_a[i][3] = scaled_len * sin_angle;
    table_a[i][4] = cosf(scaled_len);
    table_a[i][5] = sin_scaled * scaled_angle;
    table_a[i][6] = sin_scaled * sin_angle;
  }

  for (int row = 0; row < 2; row++) {
    float sin_outer = sinf(outer_angles[row]);
    float cos_outer = cosf(outer_angles[row]);
    float row_scale = outer_scales[row];

    for (int col = 0; col < 8; col++) {
      float inner = inner_angles[col];
      float cos_inner = cosf(inner);
      float sin_inner = sinf(inner);
      int index = row * 8 + col;

      basis[index][0] = 0.0f;
      basis[index][1] = cos_inner;
      basis[index][2] = sin_inner;

      table_b[index][0] = k_inner_base;
      table_b[index][1] = row_scale * basis[index][0];
      table_b[index][2] = row_scale * basis[index][1];
      table_b[index][3] = row_scale * basis[index][2];
      table_b[index][4] = cos_outer;
      table_b[index][5] = sin_outer * basis[index][1];
      table_b[index][6] = sin_outer * basis[index][2];
    }
  }
}
