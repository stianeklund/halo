/* 0x12170 — FUN_00012170: squared magnitude of a 3D vector.
 *
 * Computes vector[0]^2 + vector[1]^2 + vector[2]^2 and returns it.
 *
 * Confirmed: loads three floats from [arg0+0], [arg0+4], and [arg0+8].
 * Confirmed: x87 stack sequence squares each component and accumulates with
 *   FADDP; no globals or calls.
 * Confirmed: returns the accumulated sum in ST0.
 */
float FUN_00012170(float *vector)
{
  return vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2];
}

/* 0x121a0 — FUN_000121a0: squared distance between two 3D points.
 *
 * Computes (b[0]-a[0])^2 + (b[1]-a[1])^2 + (b[2]-a[2])^2 and returns it.
 *
 * Confirmed: loads from [arg1+0/4/8] and subtracts [arg0+0/4/8].
 * Confirmed: x87 sequence squares each component delta and sums with FADDP.
 * Confirmed: returns in ST0; no globals or calls.
 */
float FUN_000121a0(const float *a, const float *b)
{
  float dx = b[0] - a[0];
  float dy = b[1] - a[1];
  float dz = b[2] - a[2];

  return dx * dx + dy * dy + dz * dz;
}

float FUN_000121e0(float min, float max)
{
  int *seed = get_global_random_seed_address();
  return random_real_range(seed, min, max);
}

/* Compute the cross product of two 3D vectors.
 *
 * out = a × b
 *
 * Confirmed: pure x87 arithmetic, no assertions, no globals.
 * Confirmed: FLD [ECX] / FMUL [EAX+0x4] / FLD [ECX+0x4] / FMUL [EAX] / FSUBP
 *   computes out[2] = a[0]*b[1] - a[1]*b[0] (z component).
 * Confirmed: three-component store at [EAX], [EAX+0x4], [EAX+0x8].
 */
void cross_product3d(float *a, float *b, float *out)
{
  /* Load all inputs before any store so b==out (in-place) is safe.
   * The binary (0x178d0) holds all three FPU results on the x87 stack
   * before the first FSTP, giving the same aliasing guarantee. */
  float a0 = a[0], a1 = a[1], a2 = a[2];
  float b0 = b[0], b1 = b[1], b2 = b[2];
  out[0] = a1 * b2 - a2 * b1;
  out[1] = a2 * b0 - a0 * b2;
  out[2] = a0 * b1 - a1 * b0;
}

char *FUN_000210f0(int actor_handle)
{
  int weapon_handle = FUN_0003b270(actor_handle);
  if (weapon_handle != -1) {
    int *obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    return (char *)tag_get(0x77656170, *obj);
  }
  return 0;
}

char *FUN_000211f0(int actor_handle)
{
  char *actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  char *actv = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));
  int weapon_handle = FUN_0003b270(actor_handle);
  if (weapon_handle != -1) {
    int *obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    char *weap = (char *)tag_get(0x77656170, *obj);
    if (weap != 0 && *(int *)(weap + 0x3c8) != -1) {
      actv = (char *)tag_get(0x61637476, *(int *)(weap + 0x3c8));
    }
  }
  return actv;
}

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