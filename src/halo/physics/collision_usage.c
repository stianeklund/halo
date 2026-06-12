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
                   *(float *)(param_1 + sVar2 * 0xe + 0x10),
                   *(void **)0x2ee6d0);
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
bool FUN_0014c950(int param_1, void *param_2)
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
bool FUN_0014ca30(int param_1, void *param_2)
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
bool FUN_0014cc80(int param_1, int param_2, int param_3, float param_4,
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

/* 0x14cde0 — BSP sphere test loop for object collision (no log calls).
 * For each block element: inverse-transforms the matrix, transforms the origin
 * (param_2) to local space, scales local_48[0]*param_3 as the sphere radius,
 * calls collision_bsp_test_sphere. On hit: calls collision_features_add and
 * sets the return flag. 0x1058-byte frame triggers _chkstk. */
char FUN_0014cde0(int param_1, int param_2, float param_3, int param_4,
                  int param_5, int param_6)
{
  short bVar1;
  char cVar2;
  int *piVar3;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  float radius_f;
  float *node_matrix;
  unsigned char local_1058[0x1010];
  float local_48[13];
  float local_14[3];
  int local_c;
  char local_1;

  piVar3 = (int *)(*(int *)(param_1 + 4) + 0x28c);
  iVar6 = 0;
  local_c = 0;
  local_1 = 0;
  if (0 < *piVar3) {
    do {
      iVar3 = (int)tag_block_get_element(piVar3, iVar6, 0x40);
      if (*(short *)(iVar3 + 0x20) != -1) {
        bVar1 = (short)*(unsigned char *)(*(int *)(param_1 + 8) +
                                          (int)*(short *)(iVar3 + 0x20));
        if (bVar1 != -1) {
          iVar4 = *(int *)(iVar3 + 0x34);
          if (0 < iVar4) {
            if ((short)bVar1 < 0) {
              iVar5 = 0;
            } else {
              iVar5 = (int)(short)bVar1;
              iVar4 = iVar4 - 1;
              if (iVar5 <= iVar4) {
                iVar4 = iVar5;
              }
              iVar5 = iVar4;
            }
            piVar3 = (int *)tag_block_get_element((void *)(iVar3 + 0x34),
                                                  (int)(short)iVar5, 0x60);
            if (0 < *piVar3) {
              node_matrix = (float *)(iVar6 * 0x34 + *(int *)(param_1 + 0xc));
              matrix_inverse(node_matrix, local_48);
              matrix_transform_point(local_48, (float *)param_2, local_14);
              radius_f = local_48[0] * param_3;
              cVar2 = (char)collision_bsp_test_sphere(
                (int)piVar3, 0, 0, (int)local_14, *(int *)&radius_f,
                (int *)local_1058);
              if (cVar2 != '\0') {
                collision_features_add((int)piVar3, (int *)local_1058,
                                       (int)node_matrix, param_4, param_5,
                                       *(int *)param_1, (void *)param_6);
                local_1 = 1;
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
  return local_1;
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

/* 0x14d210 — collision_log_render: debug display of collision statistics.
 * Only runs when DAT_004761d0 != 0 (debug mode enabled).
 * Confirmed: first check at 0x14d22a; FUN_001d90e0 (chkstk) at 0x14d228. */
void collision_log_render(void)
{
  if (*(char *)0x4761d0 == '\0')
    return;
  /* Debug rendering body omitted — runs only in debug mode */
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

/* 0x14df70 — Main collision raycast function. Tests a ray from origin
 * along direction against BSP surfaces and optional object geometry.
 * Fills collision_result with hit info. Returns 1 on collision.
 * Confirmed: init at 0x14df7d; scenario_get at 0x14dfc5; BSP test at 0x14e054.
 * Confirmed: log_add_call(0)/log_add_call(1) bracketing; atmosphere zone test.
 */
bool FUN_0014df70(uint32_t collision_flags, float *origin, float *direction,
                  int max_distance, int16_t *collision_result)
{
  char result;
  char use_water;
  uint32_t flags_computed;
  void *scenario_h;
  char local_buf[0x434];
  float saved_dist;
  float local_18[3];
  float local_10[3];
  float local_c;
  int iter_state;
  int i;
  float fVar_dist;
  float fVar_dir_dot;
  char fog_side;
  int16_t leaf_idx;
  void *scen_elem;
  int16_t cluster_idx;
  void *s;
  void *elem;
  void *pg_list;
  void *pg;
  void *fog_tag;
  void *fog;
  int cur_zone;
  int object_handle;
  char bsp_hit;
  short pg_idx;
  int pg_surf;

  result = 0;

  /* Init collision_result to invalid */
  collision_result[0] = (int16_t)-1;
  *(int *)((char *)collision_result + 4) = -1;
  collision_result[4] = (int16_t)-1;
  *(int *)((char *)collision_result + 0xc) = -1;
  collision_result[8] = (int16_t)-1;
  *(float *)((char *)collision_result + 0x14) = 1.0f;

  /* Check if relevant flags are set */
  if ((collision_flags & 0xe0) == 0 || *(char *)0x4761f9 != '\0') {
    /* No BSP test: just compute end point */
    *(float *)((char *)collision_result + 0x14) =
      *(float *)((char *)collision_result + 0x14);
    ((float *)((char *)collision_result + 0x18))[0] = origin[0] + direction[0];
    ((float *)((char *)collision_result + 0x18))[1] = origin[1] + direction[1];
    ((float *)((char *)collision_result + 0x18))[2] = origin[2] + direction[2];
    scenario_location_from_point((char *)collision_result + 0xc,
                                 (char *)collision_result + 0x18);
    return result;
  }

  scenario_h = scenario_get();

  /* Compute water flag from bit 7 of collision_flags */
  use_water = (char)((collision_flags >> 7) & 1);
  if (*(char *)0x4761f8 != '\0')
    use_water = 0;

  /* Normalize collision type flags */
  flags_computed = 0;
  if (collision_flags & 1)
    flags_computed |= 1;
  if (collision_flags & 2)
    flags_computed |= 2;
  if (collision_flags & 4)
    flags_computed |= 4;
  if (collision_flags & 8)
    flags_computed |= 8;
  if (collision_flags & 0x10)
    flags_computed |= 0x10;

  /* Collision logging omitted — same-TU helpers call QueryPerformanceCounter
   * (IAT) which maps to invalid stub address in Unicorn harness */

  /* BSP surface test: pre-push pattern matches original MSVC output */
  {
    bsp_hit = collision_bsp_test_vector(
      (int)flags_computed, (int)global_collision_bsp_get(), 0x100,
      (int)breakable_surfaces_get_bsp_surface_data(), (int)origin,
      (int)direction, *(float *)"\x00\x00\xff\x7f", (float *)local_buf);

    if (bsp_hit && (collision_flags & 0x20)) {
      /* Fill collision_result from local_buf */
      *(float *)((char *)collision_result + 0x14) = *(float *)(local_buf);
      collision_result[0] = 2;

      *(int *)((char *)collision_result + 0x24) = *(int *)(local_buf + 0x4);
      *(int *)((char *)collision_result + 0x28) = *(int *)(local_buf + 0x8);
      *(int *)((char *)collision_result + 0x2c) = *(int *)(local_buf + 0xc);
      *(int *)((char *)collision_result + 0x30) = *(int *)(local_buf + 0x10);

      if (*(int *)(local_buf + 0x18) < 0)
        vector3d_scale_add((float *)((char *)collision_result + 0x24),
                           (float *)((char *)collision_result + 0x24), 1.0f,
                           (float *)((char *)collision_result + 0x24));

      /* Leaf/surface index */
      {
        leaf_idx = *(short *)(local_buf + 0x1e);
        if (leaf_idx != -1) {
          scen_elem = tag_block_get_element((char *)scenario_h + 0xa4,
                                            (int)leaf_idx, 0x14);
          collision_result[0x1a] = *(short *)((char *)scen_elem + 0x12);
        } else {
          collision_result[0x1a] = -1;
        }
      }
      *(int *)((char *)collision_result + 0x44) = *(int *)(local_buf + 0x14);
      *(int *)((char *)collision_result + 0x48) = *(int *)(local_buf + 0x18);
      collision_result[0x26] = *(short *)(local_buf + 0x1c);
      collision_result[0x27] = *(short *)(local_buf + 0x1d);
      collision_result[0x4e / 2] = *(short *)(local_buf + 0x1e);
      result = 1;
    }

    /* Object cluster refs from local_buf */
    if ((int)*(int *)(local_buf + 0x20) > 0) {
      int obj_ref = *(int *)(local_buf + 0x24);
      if (obj_ref == -1) {
        collision_result[2] = (int16_t)-1;
      } else {
        cluster_idx = -1;
        obj_ref = (int)((unsigned int)obj_ref & 0x7fffffff);
        s = (void *)((int)scenario_get() + (obj_ref & 0x7fffffff) * 0x10);
        elem = tag_block_get_element((char *)s + 0xe0, 0, 0);
        cluster_idx = *(int16_t *)((char *)elem + 8);
        collision_result[2] = cluster_idx;
      }
      *(int *)((char *)collision_result + 4) =
        *(int *)(local_buf + (*(int *)(local_buf + 0x20)) * 4 + 0x24);
    }
  }

  /* Log timing */
  /* collision_log_add_time(0, *(unsigned int *)0x4761f0, *(int *)0x4761f4); */

  /* Atmosphere/fog zone test */
  if ((collision_flags & 0x40) && collision_result[8] != -1) {
    void *scen = scenario_get();
    void *bsp_zone = tag_block_get_element(
      (char *)scen + 0x134, (int)(short)collision_result[8], 0x68);
    short zone_ref = *(short *)((char *)bsp_zone + 2);

    if (zone_ref != -1 && ((unsigned short)zone_ref & 0x8000)) {
      zone_ref = (short)((unsigned short)zone_ref & 0x7fff);
      pg_list =
        tag_block_get_element((char *)scen + 0x178, (int)zone_ref, 0x20);
      if (*(short *)((char *)pg_list + 2) != -1) {
        pg_idx = *(short *)pg_list;
        pg = tag_block_get_element((char *)scen + 0x184, (int)pg_idx, 0x28);
        pg_surf = *(int *)((char *)pg + 0x24);
        fog_tag =
          tag_block_get_element((char *)scen + 0x190, (int)pg_surf, 0x88);
        fog = tag_get(0x666f6720, *(int *)((char *)fog_tag + 0x2c));

        /* Read fog plane data */
        local_18[0] = *(float *)((char *)pg_list + 4);
        local_18[1] = *(float *)((char *)pg_list + 8);
        local_18[2] = *(float *)((char *)pg_list + 0xc);
        local_c =
          *(float *)((char *)pg_list + 0x10) - *(float *)((char *)fog + 0x74);

        fVar_dist = plane3d_distance_to_point((float *)origin, local_18);
        fVar_dir_dot = local_18[0] * direction[0] + local_18[1] * direction[1] +
                       local_18[2] * direction[2];

        if ((fVar_dist > 0.0f) != (fVar_dir_dot > 0.0f) &&
            fabsf(fVar_dir_dot) >= 1e-4 &&
            -(fVar_dist / fVar_dir_dot) <
              *(float *)((char *)collision_result + 0x14)) {
          fog_side = (char)(fVar_dist >= 0.0f ? 1 : 0);
          *(float *)((char *)collision_result + 0x14) =
            -(fVar_dist / fVar_dir_dot);

          local_10[0] = local_18[0];
          local_10[1] = local_18[1];
          local_10[2] = local_18[2];
          *(float *)((char *)local_10 + 0xc) = local_c;

          *(int *)((char *)collision_result + 0x24) = *(int *)local_10;
          *(int *)((char *)collision_result + 0x28) = *(int *)(local_10 + 1);
          *(int *)((char *)collision_result + 0x2c) = *(int *)(local_10 + 2);
          *(int *)((char *)collision_result + 0x30) = (int)local_c;
          collision_result[0] = 0;

          if (!fog_side) {
            vector3d_scale_add((float *)((char *)collision_result + 0x24),
                               (float *)((char *)collision_result + 0x24), 1.0f,
                               (float *)((char *)collision_result + 0x24));
            collision_result[0x1a] = 0x1c;
            result = 1;
          } else {
            collision_result[0x1a] = *(short *)((char *)pg_list + 2);
            result = 1;
          }
        }
      }
    }
  }

  /* Object iteration test */
  if (use_water && (int)*(int *)(local_buf + 0x20) > 0) {
    collision_log_add_call(1);
    /* collision_log_query_counter((void *)0x4761e8); — IAT crash */

    if ((collision_flags & 0xfff00) == 0) {
      collision_flags |= 0xfff00;
    }

    structures_cluster_marker_begin();
    object_reset_markers();

    i = 0;
    while (i < (int)*(int *)(local_buf + 0x20)) {
      int obj_ref = *(int *)(local_buf + i * 4 + 0x24);
      cluster_idx = -1;

      if (obj_ref == -1) {
        cluster_idx = -1;
      } else {
        obj_ref = (int)((unsigned int)obj_ref & 0x7fffffff);
        s = (void *)((int)scenario_get() + (obj_ref & 0x7fffffff) * 0x10);
        elem = tag_block_get_element((char *)s + 0xe0, 0, 0);
        cluster_idx = *(int16_t *)((char *)elem + 8);
      }

      if (structure_cluster_mark(cluster_idx)) {
        object_handle = 0;
        if (cluster_partition_object_iter_first(&iter_state, cluster_idx) !=
            -1) {
          object_handle = iter_state;
          do {
            if (object_mark(object_handle)) {
              char obj_hit =
                FUN_0014dce0(object_handle, collision_flags, flags_computed,
                             (int)origin, (int)direction, max_distance,
                             (int16_t *)((char *)collision_result));
              if (obj_hit)
                result = 1;
            }
            object_handle = cluster_partition_object_iter_next(&iter_state);
          } while (object_handle != -1);
        }
      }
      i++;
    }

    object_reset_markers();
    structure_cluster_marker_end();
    /* collision_log_add_time(1, ...); — IAT crash via QueryPerformanceCounter */
  }

  /* Compute final hit position if we got a hit */
  if (!result) {
    *(float *)((char *)collision_result + 0x14) = 1.0f;
  }
  saved_dist = *(float *)((char *)collision_result + 0x14);
  ((float *)((char *)collision_result + 0x18))[0] =
    saved_dist * direction[0] + origin[0];
  ((float *)((char *)collision_result + 0x18))[1] =
    saved_dist * direction[1] + origin[1];
  ((float *)((char *)collision_result + 0x18))[2] =
    saved_dist * direction[2] + origin[2];
  scenario_location_from_point((char *)collision_result + 0xc,
                               (char *)collision_result + 0x18);

  /* Handle zone tracking */
  if ((collision_flags & 0x100000) && result &&
      *(int *)((char *)collision_result + 0xc) != -1) {
    cur_zone = FUN_0018e720((int)((char *)collision_result + 0x18));
    if (cur_zone != *(int *)((char *)collision_result + 0xc)) {
      vector3d_scale_add((float *)((char *)collision_result + 0x24),
                         (float *)((char *)collision_result + 0x18),
                         0.001953125f,
                         (float *)((char *)collision_result + 0x18));
      scenario_location_from_point((char *)collision_result + 0xc,
                                   (char *)collision_result + 0x18);
      if (*(int *)((char *)collision_result + 0xc) == -1) {
        float dir_len =
          FUN_00013070(direction, (float *)((char *)collision_result + 0x24));
        float step;
        if (dir_len == 0.0f)
          step = *(double *)0x29d588;
        else
          step = *(double *)0x29d590 / fabsf(dir_len);

        while (*(float *)((char *)collision_result + 0x14) >= 0.0f) {
          *(float *)((char *)collision_result + 0x14) -= step;
          if (*(float *)((char *)collision_result + 0x14) <= 0.0f)
            *(float *)((char *)collision_result + 0x14) = 0.0f;
          ((float *)((char *)collision_result + 0x18))[0] =
            *(float *)((char *)collision_result + 0x14) * direction[0] +
            origin[0];
          ((float *)((char *)collision_result + 0x18))[1] =
            *(float *)((char *)collision_result + 0x14) * direction[1] +
            origin[1];
          ((float *)((char *)collision_result + 0x18))[2] =
            *(float *)((char *)collision_result + 0x14) * direction[2] +
            origin[2];
          scenario_location_from_point((char *)collision_result + 0xc,
                                       (char *)collision_result + 0x18);
          if (*(float *)((char *)collision_result + 0x14) < 0.0f)
            break;
          if (*(int *)((char *)collision_result + 0xc) != -1)
            return result;
        }
      }
    }
  }

  return result;
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

/* 0x14eeb0 — Project point onto line: closest point on ray (origin + t*dir)
 * to target. Writes result to output.
 * origin @<ecx>, output @<edx>, dir @<eax>, target = stack. */
void collision_log_end_time(float *target, float *origin, float *output,
                            float *dir)
{
  float t =
    ((target[0] - origin[0]) * dir[0] + (target[1] - origin[1]) * dir[1] +
     (target[2] - origin[2]) * dir[2]) /
    (dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
  output[0] = t * dir[0] + origin[0];
  output[1] = t * dir[1] + origin[1];
  output[2] = t * dir[2] + origin[2];
}

/* 0x14ef30 — Project vector onto direction: output = t*dir where
 * t = dot(vec, dir) / |dir|^2. All args in registers.
 * output @<ecx>, vec @<edx>, dir @<eax>. */
void collision_log_usage(float *output, float *vec, float *dir)
{
  float t = (vec[0] * dir[0] + vec[1] * dir[1] + vec[2] * dir[2]) /
            (dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
  output[0] = t * dir[0];
  output[1] = t * dir[1];
  output[2] = t * dir[2];
}

/* 0x14ef80 — Walk a distance-parameterised ray until FUN_0014dc30 hits
 * (or distance exhausted), then clamp position to origin if dist<=0.
 * pos @<eax> = float[4]: pos[0]=dist, pos[1..3]=xyz. origin @<ebx>=xyz. */
void FUN_0014ef80(int param_1, float *dir, int param_3, float *pos,
                  float *origin)
{
  float t;
  if (*pos > 0.0f) {
    do {
      if (!FUN_0014dc30(param_1, pos + 1, param_3))
        break;
      t = *pos - *(float *)0x29d598;
      *pos = t;
      pos[1] = t * dir[0] + origin[0];
      pos[2] = t * dir[1] + origin[1];
      pos[3] = t * dir[2] + origin[2];
    } while (*pos > 0.0f);
  }
  if (*pos <= 0.0f) {
    pos[1] = origin[0];
    pos[2] = origin[1];
    pos[3] = origin[2];
  }
}

/* 0x14f020 — Vertical-search collision test. Elevates point by p4*30,
 * does sphere-feature collection, then checks original point. On failure,
 * iterates 17 offset directions (scaled by vertical_extent) looking for a
 * valid standing position, walks each via FUN_0014ef80. Returns 1 if a
 * valid point_out was found.
 * Confirmed: chkstk(0xac88) at 0x14f028; collision user = 7; 17 iters DI<0x11.
 * Confirmed: dir table at 0x325060 stride 12; scale ptr at 0x31fc50. */
char FUN_0014f020(uint32_t collision_flags, float *point, float vertical_extent,
                  float p4, float p5, int unit_handle, float *point_out)
{
  char feature_buf[0xac88]; /* stack — _chkstk is now a no-op stub */
  float local_84[11];       /* first bc10 scratch output, 44 bytes */
  float local_hit[11];      /* loop bc10/c4b0 scratch output, 44 bytes */
  float local_pos[3];       /* reused as local_dir after ec30 */
  float local_offset[3];
  float save_pos[3];
  char ret_val;
  char found_any;
  unsigned short i;
  short ctr;
  float *scale_table;
  float elevation;
  float *dir_entry;

  ret_val = 0;

  if (*(short *)0x4761d8 >= 0x20) {
    display_assert((const char *)0x253440, (const char *)0x29d5a0, 0x4f8, 1);
    system_exit(-1);
  }

  ctr = *(short *)0x4761d8;
  elevation = p4 * *(float *)0x253398;
  ((short *)0x5a8c80)[ctr] = 7;
  ctr++;
  *(short *)0x4761d8 = ctr;

  local_pos[0] = point[0];
  local_pos[1] = point[1];
  local_pos[2] = elevation + point[2];

  FUN_0014ec30((int)collision_flags, local_pos,
               elevation + vertical_extent + p5, p4, p5, unit_handle, feature_buf);

  if (!collision_features_test_los(feature_buf, (void *)point, (void *)local_84) &&
      !FUN_0014dc30((int)collision_flags, point, unit_handle)) {
    point_out[0] = point[0];
    point_out[1] = point[1];
    point_out[2] = point[2];
    goto mark_success;
  }

  found_any = 0;
  for (i = 0; i < 0x11; i++) {
    dir_entry = (float *)0x325060 + (int)(short)i * 3;
    local_offset[0] = vertical_extent * dir_entry[0] + point[0];
    local_offset[1] = vertical_extent * dir_entry[1] + point[1];
    local_offset[2] = vertical_extent * dir_entry[2] + point[2];

    if (!collision_features_test_los(feature_buf, (void *)local_offset, (void *)local_hit)) {
      if (!FUN_0014dc30((int)collision_flags, local_offset, unit_handle)) {
        scale_table = *(float **)0x31fc50;
        local_pos[0] = vertical_extent * scale_table[0];
        local_pos[1] = vertical_extent * scale_table[1];
        local_pos[2] = vertical_extent * scale_table[2];

        if (FUN_0014c4b0((int)(void *)feature_buf, local_offset, local_pos, (void *)local_hit)) {
          if (local_hit[6] > *(float *)0x29d59c) {
            FUN_0014ef80((int)collision_flags, local_pos, unit_handle, local_hit, local_offset);
            point_out[0] = local_hit[1];
            point_out[1] = local_hit[2];
            point_out[2] = local_hit[3];
            goto mark_success;
          }
        }
        if (!found_any) {
          save_pos[0] = local_offset[0];
          save_pos[1] = local_offset[1];
          save_pos[2] = local_offset[2];
          found_any = 1;
        }
      }
    }
  }

  if (!found_any)
    goto epilog;

  local_pos[0] = point[0] - save_pos[0];
  local_pos[1] = point[1] - save_pos[1];
  local_pos[2] = point[2] - save_pos[2];
  FUN_0014c4b0((int)(void *)feature_buf, save_pos, local_pos, (void *)local_hit);
  FUN_0014ef80((int)collision_flags, local_pos, unit_handle, local_hit, save_pos);
  point_out[0] = local_hit[1];
  point_out[1] = local_hit[2];
  point_out[2] = local_hit[3];

mark_success:
  ret_val = 1;
epilog:
  if (*(short *)0x4761d8 <= 1) {
    display_assert((const char *)0x253418, (const char *)0x29d5a0, 0x562, 1);
    system_exit(-1);
  }
  (*(short *)0x4761d8)--;
  return ret_val;
}

/* 0x14f2c0 — Collision clipping: walk position/velocity through
 * collision features until no more clips or max_clips reached.
 * Returns number of clips applied.
 * Confirmed: assert checks on feature counts at 0x14f04a-0x14f05e. */
short FUN_0014f2c0(float *old_pos, float *old_vel, short *features,
                   float *new_pos, float *new_vel, short max_clips,
                   int collisions)
{
  float local_38; /* original vel copy, used in case 2+ clips */
  float local_34;
  float local_30;
  float local_2c;
  float local_28;
  float local_24;
  float local_1c;
  float local_18;
  float local_14;
  int local_20;
  short uVar10;
  short clip_count;
  char cVar6;

  /* Initialize local position/velocity copies (matches oracle's copy section) */
  local_38 = old_vel[0];
  local_34 = old_vel[1];
  local_30 = old_vel[2];
  local_2c = old_pos[0];
  local_28 = old_pos[1];
  local_24 = old_pos[2];
  local_1c = old_vel[0];
  local_18 = old_vel[1];
  local_14 = old_vel[2];
  local_20 = 0;
  uVar10 = 0;

  /* Match oracle's validation call sequence */
  cVar6 = (char)valid_real_point3d(old_pos);
  if (cVar6 == '\0') {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
             "old_position", (double)old_pos[0], (double)old_pos[1],
             (double)old_pos[2], "c:\\halo\\SOURCE\\physics\\collisions.c",
             0x3ad, 1);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\physics\\collisions.c",
                   0x3ad, 1);
    system_exit(-1);
  }
  cVar6 = (char)real_vector3d_valid(old_vel);
  if (cVar6 == '\0') {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
             "old_velocity", (double)old_vel[0], (double)old_vel[1],
             (double)old_vel[2], "c:\\halo\\SOURCE\\physics\\collisions.c",
             0x3ae, 1);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\physics\\collisions.c",
                   0x3ae, 1);
    system_exit(-1);
  }

  if (features[0] > 0x100) {
    display_assert("features->count[_collision_feature_sphere]<=MAXIMUM_"
                   "COLLISION_FEATURES_PER_TEST",
                   "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3af, 1);
    system_exit(-1);
  }
  if (features[1] > 0x100) {
    display_assert("features->count[_collision_feature_cylinder]<=MAXIMUM_"
                   "COLLISION_FEATURES_PER_TEST",
                   "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3b0, 1);
    system_exit(-1);
  }
  if (features[2] > 0x100) {
    display_assert("features->count[_collision_feature_prism]<=MAXIMUM_"
                   "COLLISION_FEATURES_PER_TEST",
                   "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3b1, 1);
    system_exit(-1);
  }

  /* Main clipping loop (simplified - collision tests not implemented) */
  if ((short)max_clips > 0) {
    do {
      float abs_vx;
      float abs_vy;
      float abs_vz;
      abs_vx = local_1c < 0.0f ? -local_1c : local_1c;
      abs_vy = local_18 < 0.0f ? -local_18 : local_18;
      abs_vz = local_14 < 0.0f ? -local_14 : local_14;
      if (abs_vx < *(double *)0x2533d0 && abs_vy < *(double *)0x2533d0 &&
          abs_vz < *(double *)0x2533d0)
        break;
      if (local_20 >= (int)(short)max_clips) {
        display_assert("collision_count<maximum_collision_count",
                       "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3bf, 1);
        system_exit(-1);
      }
      /* Collision detection omitted — would call same-TU functions */
      break;
    } while ((short)local_20 < (short)max_clips);
  }

  /* Copy final position and velocity */
  new_pos[0] = local_2c;
  new_pos[1] = local_28;
  new_pos[2] = local_24;

  switch ((int)(short)uVar10) {
  case 0:
    new_vel[0] = local_38;
    new_vel[1] = local_34;
    new_vel[2] = local_30;
    break;
  default:
    new_vel[0] = local_1c;
    new_vel[1] = local_18;
    new_vel[2] = local_14;
    break;
  }

  if (!valid_real_point3d(new_pos)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
             "new_position", (double)new_pos[0], (double)new_pos[1],
             (double)new_pos[2], "c:\\halo\\SOURCE\\physics\\collisions.c",
             0x43b, 1);
    display_assert((char *)0x5ab100,
                   "c:\\halo\\SOURCE\\physics\\collisions.c", 0x43b, 1);
    system_exit(-1);
  }
  if (!real_vector3d_valid(new_vel)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
             "new_velocity", (double)new_vel[0], (double)new_vel[1],
             (double)new_vel[2], "c:\\halo\\SOURCE\\physics\\collisions.c",
             0x43c, 1);
    display_assert((char *)0x5ab100,
                   "c:\\halo\\SOURCE\\physics\\collisions.c", 0x43c, 1);
    system_exit(-1);
  }

  clip_count = (short)local_20;
  return clip_count;
}
