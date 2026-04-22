float FUN_00193910(float param_1, float param_2)
{
  const float k_inv_255 = *(const float *)0x261518;
  const float k_scale = *(const float *)0x253f78;
  return (param_2 * k_inv_255 + param_1) * k_scale;
}

void FUN_001939f0(float param_1)
{
  *(float *)0x4d8eac = param_1;
  *(uint8_t *)0x4d8ea4 = 1;
  *(float *)0x4d8ea8 = param_1 - *(float *)0x4d8ea8;
}

float FUN_00193b80(float *param_1, float *param_2)
{
  return param_1[0] * param_2[0] + param_1[1] * param_2[1] +
         param_1[2] * param_2[2] + param_1[3] * param_2[3];
}
