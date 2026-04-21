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
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

/* 0xfae80 — weapon_get_label: resolve a weapon handle to its label string.
 *
 * If the handle is NONE (-1), returns a pointer to an empty string
 * (global at 0x25386f, which starts with a null byte).
 * Otherwise resolves the object via object_get_and_verify_type with
 * type mask 4 (weapon), reads the tag index from the object header,
 * looks up the 'weap' tag, and returns a pointer to offset 0x30c
 * within the tag definition (the weapon label field).
 *
 * Confirmed: CMP ECX,-1 / MOV EAX,0x25386f / JZ return.
 * Confirmed: PUSH 0x4 / PUSH ECX / CALL object_get_and_verify_type.
 * Confirmed: MOV EAX,[EAX] — reads tag_index from object header.
 * Confirmed: PUSH EAX / PUSH 0x77656170 / CALL tag_get.
 * Confirmed: ADD EAX,0x30c — label field offset.
 */
/* 0x21fb0 — valid_real_normal3d: check whether a 3D vector is a valid
 * unit normal (length within epsilon of 1.0).
 *
 * Computes squared_length = dot(v, v) and returns true if
 * |squared_length - 1.0f| < 0.0009765625 (1/1024).
 *
 * Also rejects NaN/infinity by testing the exponent bits.
 *
 * Confirmed: FLD / FMUL / FADDP computes dot(v, v) on x87 stack.
 * Confirmed: FSUB [0x2533c8] subtracts 1.0f.
 * Confirmed: FABS / FCOMP [0x2549d8] compares against epsilon double.
 */
int valid_real_normal3d(float *v)
{
  float sq_len = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
  float diff = sq_len - 1.0f;

  /* Reject NaN / infinity: exponent bits must not be all 1s */
  if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000) {
    return 0;
  }

  return fabsf(diff) < 0.0009765625f;
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

char *weapon_get_label(int weapon_handle)
{
  if (weapon_handle == -1) {
    return (char *)0x25386f;
  }
  int *obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  return (char *)tag_get(0x77656170, *obj) + 0x30c;
}

/* 0xfb0c0 — weapon_is_flag: test whether a weapon object is a flag.
 *
 * Resolves the object via object_get_and_verify_type with type mask 4,
 * reads the tag index, looks up the 'weap' tag, and tests bit 3 of
 * the dword at offset 0x308 in the tag definition.
 *
 * Confirmed: PUSH 0x4 / PUSH EAX / CALL object_get_and_verify_type.
 * Confirmed: MOV ECX,[EAX] — reads tag_index.
 * Confirmed: PUSH ECX / PUSH 0x77656170 / CALL tag_get.
 * Confirmed: MOV EAX,[EAX+0x308] / SHR EAX,0x3 / AND EAX,0x1.
 */
bool weapon_is_flag(int object_index)
{
  int *obj = (int *)object_get_and_verify_type(object_index, 4);
  uint32_t *tag = (uint32_t *)tag_get(0x77656170, *obj);
  return (tag[0x308 / 4] >> 3) & 1;
}
