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

int FUN_00193a80(int param_1, short *param_2, int in_EAX)
{
  int count;
  int mid;
  int elem;
  short elem_val;
  short cmp0;

  count = (in_EAX - param_1) >> 5;
  if (count > 0) {
    cmp0 = *param_2;
    do {
      mid = count / 2;
      elem = mid * 0x20 + param_1;
      elem_val = *(short *)elem;
      if (elem_val < cmp0)
        goto do_lower;
      if (elem_val != cmp0)
        goto do_upper;
      if (*(short *)(elem + 2) < param_2[1])
        goto do_lower;
      if (*(short *)(elem + 2) != param_2[1])
        goto do_upper;
      if (*(short *)(elem + 4) >= param_2[2])
        goto do_upper;
    do_lower:
      param_1 = elem + 0x20;
      count = count - 1 - mid;
      goto do_check;
    do_upper:
      count = mid;
    do_check:;
    } while (count > 0);
  }
  return param_1;
}

int FUN_00193b00(int param_1, short *param_2, int in_EAX)
{
  int count;
  int mid;
  int elem;
  short elem_val;

  count = (in_EAX - param_1) >> 5;
  if (count > 0) {
    do {
      mid = count / 2;
      elem = mid * 0x20 + param_1;
      elem_val = *(short *)elem;
      if ((elem_val <= *param_2) &&
          ((elem_val != *param_2) ||
           ((*(short *)(elem + 2) <= param_2[1]) &&
            ((*(short *)(elem + 2) != param_2[1]) ||
             (*(short *)(elem + 4) <= param_2[2]))))) {
        param_1 = elem + 0x20;
        mid = count - 1 - mid;
      }
      count = mid;
    } while (count > 0);
  }
  return param_1;
}

float FUN_00193b80(float *param_1, float *param_2)
{
  return param_1[0] * param_2[0] + param_1[1] * param_2[1] +
         param_1[2] * param_2[2] + param_1[3] * param_2[3];
}
