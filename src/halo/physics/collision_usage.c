/* Build collision prism feature list from a collision prism descriptor.
 * FPU-WARN reviewed: ECX/EDX register assignment for origin y-component;
 * semantic value is correct (origin_y) in both paths — false positive.
 * 0x14c6d0 / collision_usage.obj
 */
void FUN_0014c6d0(int param_1, void *param_2)
{
  float *pfVar1;
  float fVar2;
  short sVar5;
  float local_64[24];
  int i;
  int n;

  if (8 < *(int *)(param_1 + 0x24)) {
    display_assert("prism->point_count<=MAXIMUM_POINTS_PER_COLLISION_PRISM",
                   "c:\\halo\\SOURCE\\physics\\collision_features.c", 0x437, 1);
    system_exit(-1);
  }
  sVar5 = 0;
  if (0 < *(int *)(param_1 + 0x24)) {
    i = 0;
    do {
      pfVar1 = local_64 + i * 3;
      project_point2d((float *)(param_1 + 0x28 + i * 8),
                      (float *)(param_1 + 0xc), *(int16_t *)(param_1 + 0x20),
                      *(uint8_t *)(param_1 + 0x22), pfVar1);
      fVar2 = *(float *)(param_1 + 0x1c);
      n = *(int *)(param_1 + 0x24);
      sVar5 = sVar5 + 1;
      *pfVar1 = fVar2 * *(float *)(param_1 + 0xc) + *pfVar1;
      local_64[i * 3 + 1] =
        fVar2 * *(float *)(param_1 + 0x10) + local_64[i * 3 + 1];
      local_64[i * 3 + 2] =
        fVar2 * *(float *)(param_1 + 0x14) + local_64[i * 3 + 2];
      i = (int)sVar5;
    } while (sVar5 < n);
  }
  n = *(int *)(param_1 + 0x24);
  sVar5 = 0;
  if (0 < n) {
    i = 0;
    do {
      FUN_00189270(1, local_64 + i * 3, local_64 + ((i + 1) % n) * 3, param_2);
      n = *(int *)(param_1 + 0x24);
      sVar5 = sVar5 + 1;
      i = (int)sVar5;
    } while (i < n);
  }
}

/* Validate collision feature counts and dispatch feature computation
 * for each prism, cylinder, and sphere in the feature buffer.
 * 0x14c7b0 / collision_usage.obj
 */
void FUN_0014c7b0(int16_t *param_1)
{
  short sVar2;
  int iVar1;

  if (0x100 < *param_1) {
    display_assert("features->count[_collision_feature_sphere]<=MAXIMUM_"
                   "COLLISION_FEATURES_PER_TEST",
                   "c:\\halo\\SOURCE\\physics\\collision_features.c", 0x454, 1);
    system_exit(-1);
  }
  if (0x100 < param_1[1]) {
    display_assert("features->count[_collision_feature_cylinder]<=MAXIMUM_"
                   "COLLISION_FEATURES_PER_TEST",
                   "c:\\halo\\SOURCE\\physics\\collision_features.c", 0x455, 1);
    system_exit(-1);
  }
  if (0x100 < param_1[2]) {
    display_assert("features->count[_collision_feature_prism]<=MAXIMUM_"
                   "COLLISION_FEATURES_PER_TEST",
                   "c:\\halo\\SOURCE\\physics\\collision_features.c", 0x456, 1);
    system_exit(-1);
  }
  sVar2 = 0;
  if (0 < param_1[2]) {
    do {
      FUN_0014c6d0((int)(param_1 + sVar2 * 0x34 + 0x2204), *(void **)0x2ee6d8);
      sVar2 = sVar2 + 1;
    } while (sVar2 < param_1[2]);
  }
  sVar2 = 0;
  if (0 < param_1[1]) {
    do {
      iVar1 = (int)sVar2;
      FUN_001896d0(
        1, param_1 + iVar1 * 0x14 + 0xe0a, param_1 + iVar1 * 0x14 + 0xe10,
        *(int *)(param_1 + iVar1 * 0x14 + 0xe16), *(void **)0x2ee6d4);
      sVar2 = sVar2 + 1;
    } while (sVar2 < param_1[1]);
  }
  sVar2 = 0;
  if (0 < *param_1) {
    do {
      FUN_00189540(1, param_1 + sVar2 * 0xe + 10,
                   *(int *)(param_1 + sVar2 * 0xe + 0x10), *(void **)0x2ee6d0);
      sVar2 = sVar2 + 1;
    } while (sVar2 < *param_1);
  }
}

/* Retrieve the collision model components for an object.
 * Looks up the object's "obje" tag, checks if it has a "coll" subtag.
 * If yes, fills out[0..3] with: handle, coll_tag, object_node_ptr,
 * node_matrices. Returns 1 if the object has a collision model, 0 otherwise.
 * 0x14c8e0 / collision_usage.obj
 */
int FUN_0014c8e0(int *out, int object_handle)
{
  int *obj;
  int obje_tag;

  obj = (int *)object_get_and_verify_type(object_handle, 0xffffffff);
  obje_tag = (int)tag_get(0x6f626a65, *obj);
  if (*(int *)(obje_tag + 0x7c) != -1) {
    out[0] = object_handle;
    out[1] = (int)tag_get(0x636f6c6c, *(int *)(obje_tag + 0x7c));
    out[2] = (int)(obj + 0x4c);
    out[3] = (int)object_get_node_matrices(object_handle);
    return 1;
  }
  return 0;
}

/* Collision sphere test: for each sphere in the collision bsp3d,
 * transform the test point by the sphere's local frame and find
 * the bsp3d leaf. Returns 1 if any leaf lookup failed, 0 otherwise.
 * 0x14c950 / collision_usage.obj
 */
unsigned int FUN_0014c950(int param_1, void *param_2)
{
  uint8_t bVar1;
  int *piVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  float local_14[3];
  int local_8;

  piVar2 = (int *)(*(int *)(param_1 + 4) + 0x28c);
  iVar6 = 0;
  local_8 = 0;
  if (0 < *(int *)(*(int *)(param_1 + 4) + 0x28c)) {
    do {
      iVar3 = (int)tag_block_get_element(piVar2, iVar6, 0x40);
      if (*(short *)(iVar3 + 0x20) != -1) {
        bVar1 =
          *(uint8_t *)(*(int *)(param_1 + 8) + (int)*(short *)(iVar3 + 0x20));
        if ((uint16_t)bVar1 != 0xffff) {
          iVar4 = *(int *)(iVar3 + 0x34);
          if (0 < iVar4) {
            iVar5 = (int)(short)(uint16_t)bVar1;
            iVar4 = iVar4 - 1;
            if (iVar5 <= iVar4) {
              iVar4 = iVar5;
            }
            piVar2 = (int *)tag_block_get_element((int *)(iVar3 + 0x34),
                                                  (int)(short)iVar4, 0x60);
            if (0 < *piVar2) {
              real_matrix3x3_transform_point(
                (void *)(iVar6 * 0x34 + *(int *)(param_1 + 0xc)),
                (float *)param_2, local_14);
              iVar6 = (int)bsp3d_find_leaf(piVar2, 0, local_14);
              if (iVar6 == -1) {
                return 1;
              }
            }
          }
        }
      }
      local_8 = local_8 + 1;
      piVar2 = (int *)(*(int *)(param_1 + 4) + 0x28c);
      iVar6 = (int)(short)local_8;
    } while (iVar6 < *piVar2);
  }
  return 0;
}

/* Cylinder collision test: for each cylinder, invert the cylinder local
 * frame matrix and transform the test point into cylinder space.
 * 0x14ca30 / collision_usage.obj
 */
unsigned int FUN_0014ca30(int param_1, void *param_2)
{
  uint8_t bVar1;
  unsigned int uVar2;
  int iVar3;
  int iVar4;
  int *piVar5;
  int iVar6;
  short sVar7;
  int iVar8;
  float local_44[13];
  float local_10[3];

  uVar2 = *(unsigned int *)(param_1 + 4) + 0x28c;
  sVar7 = 0;
  if (0 < *(int *)(*(int *)(param_1 + 4) + 0x28c)) {
    iVar8 = 0;
    do {
      iVar3 = (int)tag_block_get_element((void *)uVar2, iVar8, 0x40);
      if (*(short *)(iVar3 + 0x20) != -1) {
        bVar1 =
          *(uint8_t *)(*(int *)(param_1 + 8) + (int)*(short *)(iVar3 + 0x20));
        if ((uint16_t)bVar1 != 0xffff) {
          iVar4 = *(int *)(iVar3 + 0x34);
          if (0 < iVar4) {
            iVar6 = (int)(short)(uint16_t)bVar1;
            iVar4 = iVar4 - 1;
            if (iVar6 <= iVar4) {
              iVar4 = iVar6;
            }
            piVar5 = (int *)tag_block_get_element((int *)(iVar3 + 0x34),
                                                  (int)(short)iVar4, 0x60);
            if (0 < *piVar5) {
              matrix_inverse((float *)(iVar8 * 0x34 + *(int *)(param_1 + 0xc)),
                             local_44);
              matrix_transform_point(local_44, (float *)param_2, local_10);
            }
          }
        }
      }
      sVar7 = sVar7 + 1;
      uVar2 = *(unsigned int *)(param_1 + 4) + 0x28c;
      iVar8 = (int)sVar7;
    } while (iVar8 < *(int *)(*(int *)(param_1 + 4) + 0x28c));
  }
  return uVar2 & 0xffffff00;
}

/* Cylinder BSP vector test: tests a ray against each cylinder collision
 * element. 0x14cb00 / collision_usage.obj
 */
char FUN_0014cb00(int param_1, void *param_2, void *param_3, void *param_4,
                  int16_t *param_5)
{
  short bVar1;
  char cVar2;
  int *piVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  float local_5c[13];
  float local_28[3];
  float local_1c[3];
  int local_10;
  int local_c;
  char local_5;

  local_5 = 0;
  collision_log_add_call(3);
  collision_log_query_counter((void *)0x4761c8);
  *(int *)(param_5 + 4) = 0x7f7fffff;
  piVar3 = (int *)(*(int *)(param_1 + 4) + 0x28c);
  iVar6 = 0;
  local_c = 0;
  if (0 < *(int *)(*(int *)(param_1 + 4) + 0x28c)) {
    do {
      local_10 = (int)tag_block_get_element(piVar3, iVar6, 0x40);
      if (*(short *)(local_10 + 0x20) != -1) {
        bVar1 = (short)(uint16_t)(*(
          uint8_t *)(*(int *)(param_1 + 8) + (int)*(short *)(local_10 + 0x20)));
        if (bVar1 != -1) {
          iVar4 = *(int *)(local_10 + 0x34);
          if (0 < iVar4) {
            iVar5 = (int)bVar1;
            iVar4 = iVar4 - 1;
            if (iVar4 < iVar5) {
              iVar5 = iVar4;
            }
            piVar3 = (int *)tag_block_get_element((int *)(local_10 + 0x34),
                                                  (int)(short)iVar5, 0x60);
            if (0 < *piVar3) {
              matrix_inverse((float *)(iVar6 * 0x34 + *(int *)(param_1 + 0xc)),
                             local_5c);
              matrix_transform_point(local_5c, (float *)param_3, local_28);
              matrix_scale_transform_vector(local_5c, (float *)param_4,
                                            local_1c);
              cVar2 = collision_bsp_test_vector(
                (int)param_2, (int)piVar3, 0, 0, (int)local_28, (int)local_1c,
                *(float *)(param_5 + 4), (float *)(param_5 + 4));
              if (cVar2 != '\0') {
                *param_5 = (int16_t)local_c;
                param_5[1] = *(int16_t *)(local_10 + 0x20);
                param_5[2] = (int16_t)iVar5;
                local_5 = 1;
              }
            }
          }
        }
      }
      local_c = local_c + 1;
      piVar3 = (int *)(*(int *)(param_1 + 4) + 0x28c);
      iVar6 = (int)(short)local_c;
    } while (iVar6 < *piVar3);
  }
  collision_log_add_time(3, *(unsigned int *)0x4761c8, *(int *)0x4761cc);
  return local_5;
}

/* Object model cylinder BSP collision test: for each cylinder model,
 * invert the local frame matrix, transform the test ray, then call the
 * BSP cylinder intersection test. Stores the closest hit index and LOD.
 * No collision_log calls (differs from FUN_0014cb00 which has them).
 * 0x14cc80 / collision_usage.obj
 */
int FUN_0014cc80(int param_1, int param_2, int param_3, float param_4,
                 int16_t *param_5)
{
  uint8_t bVar1;
  char cVar2;
  int *piVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int local_14;
  float local_60[13];
  float local_2c[3];
  float local_20[3];
  int local_c;
  char local_5;

  *(int *)((char *)param_5 + 8) = 0x7f7fffff;
  piVar3 = (int *)(*(int *)(param_1 + 4) + 0x28c);
  iVar6 = 0;
  local_5 = 0;
  local_c = 0;
  if (0 < *(int *)(*(int *)(param_1 + 4) + 0x28c)) {
    do {
      local_14 = (int)tag_block_get_element(piVar3, iVar6, 0x40);
      if (*(int16_t *)(local_14 + 0x20) != -1) {
        bVar1 = *(uint8_t *)(*(int *)(param_1 + 8) +
                             (int)*(int16_t *)(local_14 + 0x20));
        if (bVar1 != 0xff) {
          iVar4 = *(int *)(local_14 + 0x34);
          if (0 < iVar4) {
            iVar5 = (int)(short)(unsigned short)bVar1;
            if (iVar5 < 0) {
              iVar5 = 0;
            } else {
              iVar4 = iVar4 - 1;
              if (iVar4 < iVar5)
                iVar5 = iVar4;
            }
            piVar3 = (int *)tag_block_get_element((int *)(local_14 + 0x34),
                                                  (int)(short)iVar5, 0x60);
            if (0 < *piVar3) {
              matrix_inverse((float *)(iVar6 * 0x34 + *(int *)(param_1 + 0xc)),
                             local_60);
              matrix_transform_point(local_60, (float *)param_2, local_2c);
              matrix_scale_transform_vector(local_60, (float *)param_3,
                                            local_20);
              cVar2 =
                FUN_00149c60(piVar3, local_2c, local_20, local_60[0] * param_4,
                             *(float *)((char *)param_5 + 8),
                             (float *)((char *)param_5 + 8));
              if (cVar2 != '\0') {
                param_5[0] = (int16_t)local_c;
                param_5[1] = *(int16_t *)(local_14 + 0x20);
                param_5[2] = (int16_t)iVar5;
                local_5 = 1;
              }
            }
          }
        }
      }
      local_c = local_c + 1;
      piVar3 = (int *)(*(int *)(param_1 + 4) + 0x28c);
      iVar6 = (int)(short)local_c;
    } while (iVar6 < *piVar3);
  }
  return local_5;
}

/* Iterates collision elements; calls render_debug_collision_bsp for those with
 * valid BSP data. 0x14cf20 / collision_usage.obj
 */
void FUN_0014cf20(int param_1)
{
  unsigned char bVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  short sVar5;
  int iVar6;

  iVar2 = *(int *)(param_1 + 4);
  sVar5 = 0;
  if (0 < *(int *)(iVar2 + 0x28c)) {
    iVar6 = 0;
    do {
      iVar2 = (int)tag_block_get_element((void *)(iVar2 + 0x28c), iVar6, 0x40);
      if ((*(short *)(iVar2 + 0x20) != -1) &&
          (bVar1 = *(unsigned char *)(*(int *)(param_1 + 8) +
                                      (int)*(short *)(iVar2 + 0x20)),
           bVar1 != 0xff)) {
        iVar3 = *(int *)(iVar2 + 0x34);
        if (0 < iVar3) {
          iVar4 = (int)(short)(unsigned short)bVar1;
          iVar3 = iVar3 - 1;
          if (iVar4 <= iVar3) {
            iVar3 = iVar4;
          }
          iVar2 = (int)tag_block_get_element((void *)(iVar2 + 0x34),
                                             (int)(short)iVar3, 0x60);
          if (((0 < *(int *)(iVar2 + 0x3c)) && (0 < *(int *)(iVar2 + 0x48))) &&
              (0 < *(int *)(iVar2 + 0x54))) {
            render_debug_collision_bsp(iVar2,
                                       iVar6 * 0x34 + *(int *)(param_1 + 0xc));
          }
        }
      }
      iVar2 = *(int *)(param_1 + 4);
      sVar5 = sVar5 + 1;
      iVar6 = (int)sVar5;
    } while (iVar6 < *(int *)(iVar2 + 0x28c));
  }
}

/* Comparator: returns -1/0/1 based on *(int *)(a+8) vs *(int *)(b+8).
 * 0x14cfe0 / collision_usage.obj
 */
int FUN_0014cfe0(int param_1, int param_2)
{
  if (*(int *)(param_2 + 8) < *(int *)(param_1 + 8)) {
    return -1;
  }
  return *(int *)(param_1 + 8) < *(int *)(param_2 + 8);
}

void collision_log_initialize(void)
{
  csmemset((void *)0x5a5e40, 0, 0x2298);
  assert_halt(*(int16_t *)0x4761d8 < 0x20);
  *(int16_t *)(0x5a8c80 + *(int16_t *)0x4761d8 * 2) = 0;
  *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 + 1;
}

/* Enable or disable collision logging globally.
 * Sets the byte at 0x325054 to the enable flag.
 * 0x14d070 / collision_usage.obj
 */
void collision_log_enable(char enable)
{
  *(char *)0x325054 = enable;
}

/* Helper shared by begin/continue_period. Asserts no period is active,
 * validates time_period is in [0,3), zeros the scratch buffer at 0x5a80e0,
 * and sets the current period. time_period passed in ESI. */
void collision_log_period_helper(int time_period /* @<esi> */, int begin_flag)
{
  int16_t period = (int16_t)time_period;

  if (*(int16_t *)0x325058 != -1) {
    display_assert("collision_usage_current_period == NONE",
                   "c:\\halo\\SOURCE\\physics\\collision_usage.c", 0xa7, 1);
    system_exit(-1);
  }
  if (period < 0 || period >= 3) {
    display_assert(
      "(time_period >= 0) && (time_period < NUMBER_OF_COLLISION_TIME_PERIODS)",
      "c:\\halo\\SOURCE\\physics\\collision_usage.c", 0xa8, 1);
    system_exit(-1);
  }
  csmemset((void *)0x5a80e0, 0, 0xb88);
  *(int16_t *)0x325058 = period;
}

void collision_log_begin_period(__int16 time_period)
{
  collision_log_period_helper((int)time_period, 1);
}

void collision_log_continue_period(__int16 time_period)
{
  collision_log_period_helper((int)time_period, 0);
}

void collision_log_end_period(void)
{
  int16_t period;
  int i;
  int *src;
  int *dst;

  period = *(int16_t *)0x325058;
  assert_halt(period >= 0 && period < 3);
  *(char *)0x5a80e0 = 1;
  src = (int *)0x5a80e0;
  dst = (int *)(0x5a5e40 + (int)period * 0xb88);
  for (i = 0x2e2; i != 0; i--) {
    *dst++ = *src++;
  }
  *(int16_t *)0x325058 = -1;
}

/*
 * collision_log_format_stat — format one collision stat accumulator into a
 * display string buffer.
 *
 * If the timing flag at 0x4761d4 is set, captures the current performance
 * frequency via QueryPerformanceFrequency and formats "count/ms" where ms =
 * (accumulated_ticks * 1000.0f) / frequency. Otherwise formats just "count".
 *
 * Register args: buf @<edi> (output char buffer), stat @<esi> (pointer to
 * accumulator: int count at [+0], int64 ticks at [+8]).
 *
 * Confirmed: PUSH EDI/%d/%.2f pattern; FILD [ESI+8]; FMUL 1000.0f; FDIVP;
 * FSTP double [ESP]; PUSH [ESI]; PUSH 0x29d438; PUSH EDI; CALL crt_sprintf.
 * Confirmed: else branch: PUSH [ESI]; PUSH 0x25acb8; PUSH EDI; CALL sprintf.
 * 0x14d1b0 / collision_usage.obj
 */
void collision_log_format_stat(char *buf /* @<edi> */, void *stat /* @<esi> */)
{
  int64_t freq;
  double ms;

  if (*(uint8_t *)0x4761d4) {
    QueryPerformanceFrequency(&freq);
    ms = (double)(*(int64_t *)((char *)stat + 8)) * 1000.0f / (double)freq;
    crt_sprintf(buf, "%d/%.2f", *(int *)stat, ms);
  } else {
    crt_sprintf(buf, "%d", *(int *)stat);
  }
}

/* 0x14d840 — returns the current collision user if collision logging is
 * active, or -1 if logging is disabled / not applicable.
 * Validates depth > 0, user in [0,22), collision_function in [0,8),
 * game in progress, not in editor, logging enabled, and period set. */
short FUN_0014d840(short collision_function /* @<edi> */)
{
  short user;

  assert_halt(*(int16_t *)0x4761d8 > 0);

  user = *(int16_t *)(0x5a8c7e + *(int16_t *)0x4761d8 * 2);
  assert_halt(user >= 0 && user < 0x16);
  assert_halt(collision_function >= 0 && collision_function < 8);

  if (!game_in_progress())
    return -1;
  if (game_in_editor())
    return -1;
  if (*(char *)0x325054 == 0)
    return -1;
  if (*(int16_t *)0x325058 == -1)
    return -1;

  assert_halt(*(int16_t *)0x325058 >= 0 && *(int16_t *)0x325058 < 3);

  return user;
}

/* 0x14d940 — wraps QueryPerformanceCounter (QueryPerformanceCounter) */
void collision_log_query_counter(void *counter)
{
  QueryPerformanceCounter(counter);
}

/* 0x14d950 — records elapsed time for a collision function type.
 * Computes (current_time - start) as 64-bit and accumulates into
 * per-type and per-user counters in the scratch buffer at 0x5a80e0. */
void collision_log_add_time(short collision_function, unsigned int start_lo,
                            int start_hi)
{
  unsigned int current[2];
  short user;
  unsigned int elapsed_lo;
  int elapsed_hi;
  int type_off;
  int user_off;
  unsigned int prev;

  QueryPerformanceCounter(current);

  user = FUN_0014d840(collision_function);
  if (user == -1)
    return;

  elapsed_lo = current[0] - start_lo;
  elapsed_hi =
    ((int)current[1] - start_hi) - (unsigned int)(current[0] < start_lo);

  type_off = (int)collision_function * 0x170;
  prev = *(unsigned int *)(0x5a80f0 + type_off);
  *(unsigned int *)(0x5a80f0 + type_off) = prev + elapsed_lo;
  *(int *)(0x5a80f4 + type_off) +=
    elapsed_hi + (unsigned int)((prev + elapsed_lo) < prev);

  user_off = ((int)collision_function * 0x17 + (int)user) * 0x10;
  prev = *(unsigned int *)(0x5a8100 + user_off);
  *(unsigned int *)(0x5a8100 + user_off) = prev + elapsed_lo;
  *(int *)(0x5a8104 + user_off) +=
    elapsed_hi + (unsigned int)((prev + elapsed_lo) < prev);
}

/* 0x14d9d0 — increments call count for a collision function type */
void collision_log_add_call(short collision_function)
{
  short user;

  user = FUN_0014d840(collision_function);
  if (user == -1)
    return;

  *(int *)(0x5a80e8 + (int)collision_function * 0x170) += 1;
  *(int *)(0x5a80f8 + ((int)collision_function * 0x17 + (int)user) * 0x10) += 1;
}

/*
 * FUN_0014da20 — append collision sphere statistics to a display line buffer.
 *
 * If collision logging is active (0x5a5e40 flag), appends a formatted line of
 * sphere/structure-vector/object-vector test counts to the end of display_line.
 *
 * Confirmed: cdecl, 1 arg (char *display_line).
 * Confirmed: PUSH ESI; CALL 0x8df60 (csstrlen); ADD ESP,4 → end-of-string.
 * Confirmed: PUSH end; PUSH format(0x29d548); PUSH 6 globals; CALL 0x1d90f0
 * (crt_sprintf). Confirmed: globals: 0x5a6128, 0x5a66e8+0x5a6858, 0x5a5e48,
 * 0x5a6578, 0x5a5fb8, 0x5a6408.
 */
void FUN_0014da20(char *display_line)
{
  char *end;

  if (!*(uint8_t *)0x5a5e40)
    return;
  end = display_line + csstrlen(display_line);
  crt_sprintf(end, (char *)0x29d548, *(int *)0x5a6128,
              *(int *)0x5a66e8 + *(int *)0x5a6858, *(int *)0x5a5e48,
              *(int *)0x5a6578, *(int *)0x5a5fb8, *(int *)0x5a6408);
}

/*
 * FUN_0014da80 — look up a collision-function attribute from its tag block.
 *
 * Indexes the block at tag_data+0x234 (0x48-byte elements) by
 * collision_fn_index and returns the int16_t at element+0x24, or -1 if index is
 * -1.
 *
 * Confirmed: cdecl, 2 stack args: int tag_data at [EBP+8], int16_t index at
 * [EBP+C]. Confirmed: CMP AX,0xffff → OR EAX,-1 for -1 case. Confirmed: PUSH
 * 0x48; PUSH index; PUSH tag+0x234; CALL 0x19b210 (tag_block_get_element).
 * Confirmed: MOVSX EAX,word ptr [EAX+0x24].
 */
int FUN_0014da80(int tag_data, int16_t collision_fn_index)
{
  void *elem;

  if (collision_fn_index == (int16_t)-1)
    return -1;
  elem = tag_block_get_element((void *)(tag_data + 0x234),
                               (int)collision_fn_index, 0x48);
  return (int)*(int16_t *)((char *)elem + 0x24);
}

/* 0x14dab0 — Tests whether a point (param_1) passes a sphere–BSP collision
 * check. Finds the BSP3D leaf for param_1 via FUN_0018e420; if found, tests the
 * collision BSP sphere. Returns 1 if outside BSP or collision sphere
 * intersects, 0 on pass. Confirmed: cdecl, 2 stack args. _chkstk(0x1010) for
 * 4112-byte buf local. */
char FUN_0014dab0(int param_1, int param_2)
{
  char buf[0x1010];
  if ((int)bsp3d_find_leaf(FUN_0018e420(), 0, (void *)param_1) == -1)
    goto fail;
  if (!(char)collision_bsp_test_sphere(
        (int)global_collision_bsp_get(), 0x100,
        (int)breakable_surfaces_get_bsp_surface_data(), param_1, param_2,
        (int *)buf))
    return 0;
fail:
  return 1;
}

/* 0x14db10 — Walk a linked list of objects (via [obj+0xC4] sibling index),
 * testing each against a sphere (radius at [obj+0x5C], position at [obj+0x50]).
 * Dispatches to FUN_001509c0/FUN_00150ac0 (type-flag branch A) or
 * FUN_0014c8e0/FUN_0014c950 (branch B). Recurses on child index [obj+0xC8].
 * Returns 1 if any match found. */
char FUN_0014db10(int param_1, int param_2, int param_3, int param_4)
{
  int *obj;
  int type_short;
  int cur;
  int child;
  float dx;
  float dy;
  float dz;
  float r;
  int local_a[19];
  int local_b[4];

  cur = param_1;
  do {
    obj = (int *)object_get_and_verify_type(cur, -1);
    if (cur != param_4 && !(*(unsigned char *)((char *)obj + 4) & 1)) {
      type_short = (int)*(short *)((char *)obj + 0x64);
      if (param_2 & (1 << (type_short + 8))) {
        r = *(float *)((char *)obj + 0x5c);
        dx = *(float *)((char *)obj + 0x50) - ((float *)param_3)[0];
        dy = *(float *)((char *)obj + 0x54) - ((float *)param_3)[1];
        dz = *(float *)((char *)obj + 0x58) - ((float *)param_3)[2];
        if (r * r >= dx * dx + dy * dy + dz * dz) {
          if ((1 << type_short) & 2 && param_2 & 0x400000) {
            if (FUN_001509c0(local_a, cur) &&
                FUN_00150ac0(local_a, (int *)param_3)) {
              return 1;
            }
          } else {
            if (FUN_0014c8e0(local_b, cur) &&
                FUN_0014c950((int)local_b, (void *)param_3)) {
              return 1;
            }
          }
          child = *(int *)((char *)obj + 0xc8);
          if (child != -1 && FUN_0014db10(child, param_2, param_3, param_4)) {
            return 1;
          }
        }
      }
    }
    cur = *(int *)((char *)obj + 0xc4);
  } while (cur != -1);
  return 0;
}

/* 0x14ec30 — Collision test against BSP surfaces and nearby objects.
 * Performs a BSP surface collision query with the given search radius,
 * optionally adds BSP collision features, then iterates over objects in
 * nearby clusters (filtered by type mask in flags bits 8-19) and tests
 * each marked object via FUN_0014ea10. Returns true if any collision
 * features were recorded (any of the 3 feature counts is nonzero). */
bool FUN_0014ec30(int flags, float *pos, float search_radius, float dist_b,
                  float dist_a, int param6, void *scratch)
{
  void *scenario;
  void *bsp;
  char *bsp_surface_data;
  char bsp_hit;
  unsigned char test_objects;
  void *cluster_block;
  void *elem;
  int16_t cluster_idx;
  int iter_state;
  int obj_handle;
  int i;

  collision_features_init(scratch);

  if ((flags & 0x20) == 0 && (flags & 0xc0) == 0)
    goto check_result;

  scenario = scenario_get();
  bsp = global_collision_bsp_get();
  test_objects = (unsigned char)((unsigned int)flags >> 7) & 1;
  if (*(char *)0x4761f8 != 0) {
    test_objects = 0;
  }

  collision_log_add_call(2);
  collision_log_query_counter((void *)0x4761e0);

  /* Adjust search radius by the global epsilon at 0x255d90 (0.0625f) */
  search_radius = search_radius + *(float *)0x255d90;

  /* Large local buffer for collision results — _chkstk allocated 0x1018 bytes
   */
  {
    int results_buf[0x406]; /* 0x1018 bytes */

    bsp_surface_data = breakable_surfaces_get_bsp_surface_data();
    bsp_hit = (char)collision_bsp_test_sphere(
      (int)bsp, 0x100, (int)bsp_surface_data, (int)pos, *(int *)&search_radius,
      results_buf);

    if (bsp_hit && (flags & 0x20) != 0) {
      collision_features_add((int)bsp, results_buf, 0, *(int *)&dist_b,
                             *(int *)&dist_a, -1, scratch);
    }

    if (test_objects != 0 && results_buf[0x303] > 0) {
      if ((flags & 0xfff00) == 0) {
        flags = flags | 0xfff00;
      }

      structures_cluster_marker_begin();
      object_reset_markers();

      cluster_block = (void *)((char *)scenario + 0xe0);
      i = 0;
      if (results_buf[0x303] > 0) {
        do {
          elem = tag_block_get_element(
            cluster_block, results_buf[0x304 + i] & 0x7fffffff, 0x10);
          cluster_idx = *(int16_t *)((char *)elem + 8);
          if (structure_cluster_mark(cluster_idx)) {
            obj_handle =
              cluster_partition_object_iter_first(&iter_state, cluster_idx);
            while (obj_handle != -1) {
              if (object_mark(obj_handle)) {
                FUN_0014ea10((unsigned int)flags, obj_handle, pos,
                             search_radius, dist_b, dist_a, param6,
                             (int)scratch);
              }
              obj_handle = cluster_partition_object_iter_next(&iter_state);
            }
          }
          i++;
        } while ((int)(int16_t)i < results_buf[0x303]);
      }

      object_marker_end();
      structure_cluster_marker_end();
    }

    collision_log_add_time(2, *(unsigned int *)0x4761e0, *(int *)0x4761e4);
  }

check_result:
  if (*(int16_t *)scratch == 0 && *((int16_t *)scratch + 1) == 0 &&
      *((int16_t *)scratch + 2) == 0) {
    return 0;
  }
  return 1;
}
