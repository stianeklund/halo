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
  float local_18[4]; /* fog plane {normal[0..2], d} — must stay contiguous so
                      * &local_18 is a valid real_plane3d for
                      * plane3d_distance_to_point (matches original EBP-0x1c). */
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
  void *elem;
  void *pg_list;
  void *pg;
  void *fog_tag;
  void *fog;
  int object_handle;
  char bsp_hit;
  short pg_idx;
  short pg_surf;

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

  /* Force bits 0+1 on when neither is set (original: TEST BL,3; JNZ; OR EBX,3
   * at 0x14dfe4). This ensures at least one surface type is always tested. */
  if ((collision_flags & 3) == 0)
    collision_flags |= 3;

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
      (int)direction, 3.4028235e+38f, (float *)local_buf);

    if (bsp_hit && (collision_flags & 0x20)) {
      /* Fill collision_result from the BSP-test result struct (local_buf).
       * The buffer the original passes to the BSP test is at EBP-0x434, so
       * the field offsets within it are:
       *   +0x0  dword  hit distance          -> collision_result+0x14
       *   +0x4  float* -> plane[4] (x,y,z,d)  -> collision_result+0x24..+0x30
       *   +0x8  dword                         -> collision_result+0x44
       *   +0xc  dword  (sign-tested)          -> collision_result+0x48
       *   +0x10 byte                          -> collision_result+0x4c
       *   +0x11 byte                          -> collision_result+0x4d
       *   +0x12 short  leaf/surface index     -> collision_result+0x4e (+
       * scenario+0xa4 lookup) +0x14 dword  obj_ref count +0x18 dword[] obj_ref
       * array The prior lift had every offset shifted (reading +0x4..+0x1e
       * where the data is at +0x0..+0x18), and read the plane components inline
       * instead of through the pointer at +0x4 — corrupting the hit data and
       * driving the obj_ref count/index reads (below) off the end of the
       * meaningful data into garbage. */
      int *hit_plane = *(int **)(local_buf + 0x4);

      *(int *)((char *)collision_result + 0x14) = *(int *)(local_buf);
      collision_result[0] = 2;

      *(int *)((char *)collision_result + 0x24) = hit_plane[0];
      *(int *)((char *)collision_result + 0x28) = hit_plane[1];
      *(int *)((char *)collision_result + 0x2c) = hit_plane[2];
      *(int *)((char *)collision_result + 0x30) = hit_plane[3];

      if (*(int *)(local_buf + 0xc) < 0)
        plane_negate((float *)((char *)collision_result + 0x24),
                     (float *)((char *)collision_result + 0x24));

      /* Leaf/surface index */
      {
        leaf_idx = *(short *)(local_buf + 0x12);
        if (leaf_idx != -1) {
          scen_elem = tag_block_get_element((char *)scenario_h + 0xa4,
                                            (int)leaf_idx, 0x14);
          collision_result[0x1a] = *(short *)((char *)scen_elem + 0x12);
        } else {
          collision_result[0x1a] = -1;
        }
      }
      *(int *)((char *)collision_result + 0x44) = *(int *)(local_buf + 0x8);
      *(int *)((char *)collision_result + 0x48) = *(int *)(local_buf + 0xc);
      *(char *)((char *)collision_result + 0x4c) = *(char *)(local_buf + 0x10);
      *(char *)((char *)collision_result + 0x4d) = *(char *)(local_buf + 0x11);
      collision_result[0x4e / 2] = *(short *)(local_buf + 0x12);
      result = 1;
    }

    /* Object cluster refs from local_buf: local_buf+0x14 = obj_ref count,
     * local_buf+0x18 = obj_ref[]. Resolve the first (index 0) and last
     * (index count-1) refs: store the raw handle to collision_result+4 / +0xc
     * and the cluster index to collision_result+8 / +0x10. The cluster index
     * is field +8 of element [ref & 0x7fffffff] in the scenario structure-bsp
     * block (scenario+0xe0, element_size 0x10).
     *
     * NOTE: the original pushes element_size(0x10) and index(ref&0x7fffffff)
     * BEFORE the inner 0-arg scenario_get() call (cdecl arg mis-grouping,
     * lift-learnings §7); they are tag_block_get_element's args, not address
     * arithmetic on the block base. */
    if (*(int *)(local_buf + 0x14) > 0) {
      int obj_ref_first = *(int *)(local_buf + 0x18);
      int obj_ref_last;
      int16_t cluster_first;
      int16_t cluster_last;

      *(int *)((char *)collision_result + 4) = obj_ref_first;
      if (obj_ref_first == -1) {
        cluster_first = (int16_t)-1;
      } else {
        elem = tag_block_get_element((char *)scenario_get() + 0xe0,
                                     obj_ref_first & 0x7fffffff, 0x10);
        cluster_first = *(int16_t *)((char *)elem + 8);
      }
      *(int16_t *)((char *)collision_result + 8) = cluster_first;

      obj_ref_last =
        *(int *)(local_buf + (*(int *)(local_buf + 0x14)) * 4 + 0x14);
      *(int *)((char *)collision_result + 0xc) = obj_ref_last;
      if (obj_ref_last == -1) {
        cluster_last = (int16_t)-1;
      } else {
        elem = tag_block_get_element((char *)scenario_get() + 0xe0,
                                     obj_ref_last & 0x7fffffff, 0x10);
        cluster_last = *(int16_t *)((char *)elem + 8);
      }
      *(int16_t *)((char *)collision_result + 0x10) = cluster_last;
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
        /* Original reads pg_surf as a SIGNED 16-bit word (movsx eax,word ptr
         * [eax+0x24] @0x14e23b), not a dword. Reading *(int*) here combines the
         * +0x24 surface index with the +0x26 field: when +0x24=0 and +0x26=1 the
         * dword is 0x00010000 (65536), which overflows the count-1 fog block at
         * scen+0x190 and halts at tag_groups.c:3089. See feedback_check_disasm. */
        pg_surf = *(short *)((char *)pg + 0x24);
        fog_tag =
          tag_block_get_element((char *)scen + 0x190, (int)pg_surf, 0x88);
        fog = tag_get(0x666f6720, *(int *)((char *)fog_tag + 0x2c));

        /* Read fog plane data */
        local_18[0] = *(float *)((char *)pg_list + 4);
        local_18[1] = *(float *)((char *)pg_list + 8);
        local_18[2] = *(float *)((char *)pg_list + 0xc);
        local_18[3] = local_c =
          *(float *)((char *)pg_list + 0x10) - *(float *)((char *)fog + 0x74);

        /* Reference (0x14df70): FUN_00099500(&plane, origin) passes the fog
         * plane {normal, d} FIRST and the ray origin SECOND. The prior lift
         * swapped the arguments (ray origin as the plane) and passed only the
         * 3-float normal with no d component, so plane3d_distance_to_point read
         * a garbage plane distance -> a spurious near fog fraction that
         * collapsed the observer/vehicle chase camera onto the vehicle. */
        fVar_dist = plane3d_distance_to_point(local_18, (float *)origin);
        fVar_dir_dot = local_18[0] * direction[0] + local_18[1] * direction[1] +
                       local_18[2] * direction[2];

        if ((fVar_dist > 0.0f) != (fVar_dir_dot > 0.0f) &&
            fabs((double)fVar_dist) < fabs((double)fVar_dir_dot) &&
            fabs((double)fVar_dir_dot) >= *(double *)0x2533d0 &&
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
          /* Reference stores the plane d as a float (*(float*)(param_5+0x18) =
           * local_10); the prior lift truncated it to int ((int)local_c). */
          *(float *)((char *)collision_result + 0x30) = local_c;
          collision_result[0] = 0;

          if (!fog_side) {
            /* Back-facing fog plane: flip the plane so its normal points
             * toward the ray origin. The original calls plane_negate
             * (FUN_000994d0(pfVar2,pfVar2) @0x14e2xx) — the SAME helper the
             * BSP branch uses above (line ~942). A prior lift mis-transcribed
             * this 2-arg plane_negate as vector3d_scale_add(n,n,1.0,n), which
             * computes n + 1.0*n = 2n instead of -n: a unit normal (0,0,1)
             * became (0,0,2), tripping assert_valid_real_normal3d in the
             * vehicle thruster effect (effects.c#1121) on checkpoint load. */
            plane_negate((float *)((char *)collision_result + 0x24),
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
  if (use_water && (int)*(int *)(local_buf + 0x14) > 0) {
    collision_log_add_call(1);
    /* collision_log_query_counter((void *)0x4761e8); — IAT crash */

    if ((collision_flags & 0xfff00) == 0) {
      collision_flags |= 0xfff00;
    }

    structures_cluster_marker_begin();
    object_reset_markers();

    i = 0;
    while (i < (int)*(int *)(local_buf + 0x14)) {
      int obj_ref = *(int *)(local_buf + i * 4 + 0x18);
      cluster_idx = -1;

      if (obj_ref == -1) {
        cluster_idx = -1;
      } else {
        /* Same cdecl arg mis-grouping as the cluster block above: block is
         * scenario_get()+0xe0, index is ref&0x7fffffff, element_size is 0x10.
         */
        elem = tag_block_get_element((char *)scenario_get() + 0xe0,
                                     obj_ref & 0x7fffffff, 0x10);
        cluster_idx = *(int16_t *)((char *)elem + 8);
      }

      if (structure_cluster_mark(cluster_idx)) {
        object_handle =
          cluster_partition_object_iter_first(&iter_state, cluster_idx);
        if (object_handle != -1) {
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

    object_marker_end();
    structure_cluster_marker_end();
    /* collision_log_add_time(1, ...); — IAT crash via QueryPerformanceCounter
     */
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
  /* Zone-tracking refinement (original 0x14e500-0x14e631), gated on
   * (collision_flags & 0x100000) — i.e. the detonation/effects callers
   * (0x1000e9 / 0x100061), never the render path (0xfff80, no 0x100000 bit, so
   * this block is render-safe by construction).  When the hit point's BSP leaf
   * disagrees with the recorded surface ref, nudge the point off the surface by
   * 1/4096 along the plane normal (collision_result+0x24) and re-resolve its
   * scenario location into collision_result+0xc.  If that lands in void (-1),
   * step the hit fraction (collision_result+0x14) back toward the ray origin
   * until a valid leaf is found or the fraction reaches 0.
   *
   * Disasm-verified faithful reconstruction (analyst, 2026-06-22): refine =
   * 1/4096 (0x39800000), step = (1/4096)/|dot(direction, normal)| with a 1/32
   * fallback when the ray is parallel to the surface.  The retry is the
   * original's do/while with the live `frac <= 0` terminator checked AFTER the
   * scenario_location_from_point call (clamp `frac = max(frac-step, 0)` keeps it
   * >= 0, so the loop always terminates).  This REPLACES the prior lift that
   * used a wrong 1/512 refine, a swapped base/dir, and a top-tested
   * `while (frac >= 0)` whose clamp made `if (frac < 0) break` dead — the PoA
   * a10 collision-raycast freeze (removed in 59ac3322, now restored correctly).
   * Only refines collision_result+0xc/+0x14/+0x18; does not touch the hit type
   * (+0), the material (+0x34), or the AL return value. */
  if (collision_flags & 0x100000) {
    if (result && *(int *)((char *)collision_result + 0xc) != -1) {
      int cur_zone;
      cur_zone = FUN_0018e720((int)((char *)collision_result + 0x18));
      if (cur_zone != *(int *)((char *)collision_result + 0xc)) {
        vector3d_scale_add((float *)((char *)collision_result + 0x18),
                           (float *)((char *)collision_result + 0x24),
                           0.000244140625f,
                           (float *)((char *)collision_result + 0x18));
        scenario_location_from_point((char *)collision_result + 0xc,
                                     (char *)collision_result + 0x18);
        if (*(int *)((char *)collision_result + 0xc) == -1) {
          float d;
          float step;
          d = FUN_00013070(direction,
                           (float *)((char *)collision_result + 0x24));
          if (d != 0.0f)
            step = (float)(0.000244140625 / fabs((double)d));
          else
            step = 0.03125f;
          for (;;) {
            float frac;
            frac = *(float *)((char *)collision_result + 0x14) - step;
            if (!(frac > 0.0f))
              frac = 0.0f;
            *(float *)((char *)collision_result + 0x14) = frac;
            ((float *)((char *)collision_result + 0x18))[0] =
              frac * direction[0] + origin[0];
            ((float *)((char *)collision_result + 0x18))[1] =
              frac * direction[1] + origin[1];
            ((float *)((char *)collision_result + 0x18))[2] =
              frac * direction[2] + origin[2];
            scenario_location_from_point((char *)collision_result + 0xc,
                                         (char *)collision_result + 0x18);
            if (*(float *)((char *)collision_result + 0x14) <= 0.0f)
              break;
            if (*(int *)((char *)collision_result + 0xc) != -1)
              return result;
          }
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
  int has_features;
  int i;

  collision_features_init(scratch);
  bsp_hit = 0;

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
  has_features = !(*(int16_t *)scratch == 0 && *((int16_t *)scratch + 1) == 0 &&
                   *((int16_t *)scratch + 2) == 0);

  if (!has_features) {
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
  float local_84[11]; /* first bc10 scratch output, 44 bytes */
  float local_hit[11]; /* loop bc10/c4b0 scratch output, 44 bytes */
  float local_pos[3]; /* reused as local_dir after ec30 */
  float local_offset[3];
  float save_pos[3];
  volatile char ret_val;
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
               elevation + vertical_extent + p5, p4, p5, unit_handle,
               feature_buf);

  if (!collision_features_test_los(feature_buf, (void *)point,
                                   (void *)local_84) &&
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

    if (!collision_features_test_los(feature_buf, (void *)local_offset,
                                     (void *)local_hit)) {
      if (!FUN_0014dc30((int)collision_flags, local_offset, unit_handle)) {
        scale_table = *(float **)0x31fc50;
        local_pos[0] = vertical_extent * scale_table[0];
        local_pos[1] = vertical_extent * scale_table[1];
        local_pos[2] = vertical_extent * scale_table[2];

        if (FUN_0014c4b0((int)(void *)feature_buf, local_offset, local_pos,
                         (void *)local_hit)) {
          if (local_hit[6] > *(float *)0x29d59c) {
            FUN_0014ef80((int)collision_flags, local_pos, unit_handle,
                         local_hit, local_offset);
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
  FUN_0014c4b0((int)(void *)feature_buf, save_pos, local_pos,
               (void *)local_hit);
  FUN_0014ef80((int)collision_flags, local_pos, unit_handle, local_hit,
               save_pos);
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
 * Returns number of clips applied (collision_count).
 * EBX=clip_count (number of accumulated clip planes, max 3).
 * EDI=collision_count (number of collision iterations performed).
 * Collision record layout (0x2c = 44 bytes per record):
 *   +0x00: float time (collision fraction)
 *   +0x04: float collision_position[3] (where the hit occurred)
 *   +0x10: float plane_normal[3] (surface normal at hit)
 *   +0x1c: float plane_d (plane distance constant)
 *   +0x20: int handle_1 (datum handle, -1 = none)
 *   +0x24: int handle_2 (datum handle, -1 = none)
 *   +0x28: short feature_index
 * Variable layout from EBP (confirmed from disassembly):
 *   ebp-0x34..-0x2c = old_vel_copy[3]
 *   ebp-0x28..-0x20 = position[3]
 *   ebp-0x18..-0x10 = velocity[3]
 *   ebp-0x70..-0x68 = collision_position[3] (from record+0x04)
 *   ebp-0x64..-0x5c = clip_point[3]
 *   ebp-0x58..-0x4c = collision_plane[4] (normal[3] + d)
 *   ebp-0x0c..-0x04 = clip_line[3]
 *   ebp-0x40..-0x38 = clip_result[3]
 *   ebp-0x48..-0x44 = clip_indices[3] (short)
 *   ebp-0x78 = clip_staging[0] (short)
 *   ebp-0x76 = saved clip_index (short)
 *   ebp-0x1c = collision_count */
short FUN_0014f2c0(float *old_pos, float *old_vel, short *features,
                   float *new_pos, float *new_vel, short max_clips,
                   int collisions)
{
  /* [ebp-0x34..-0x2c] old_vel_copy: saved velocity, scaled each iteration */
  float old_vel_copy[3];
  /* [ebp-0x28..-0x20] position: current clipped position */
  float position[3];
  /* [ebp-0x18..-0x10] velocity: current clipped velocity */
  float velocity[3];
  /* [ebp-0x70..-0x68] collision_position: from record+0x04 */
  float collision_position[3];
  /* [ebp-0x64..-0x5c] clip_point: intersection point for 3-plane case */
  float clip_point[3];
  /* [ebp-0x58..-0x4c] collision_plane: first clip plane normal[3] + d */
  float collision_plane[4];
  /* [ebp-0x0c..-0x04] clip_line: second clip plane normal or cross product */
  float clip_line[3];
  /* [ebp-0x40..-0x38] clip_result: scratch for line-plane intersection */
  float clip_result[3];
  /* [ebp-0x48..-0x44] clip_indices: collision record indices for each plane */
  short clip_indices[3];
  /* [ebp-0x78..-0x74] clip_staging: staging area for new clip indices.
   * clip_staging[0] = clip_staging[0] (current collision record index).
   * clip_staging[1] = saved clip_indices[0] for multi-clip paths.
   * Copied into clip_indices via csmemcpy(clip_indices, clip_staging,
   * clip_count*2). */
  short clip_staging[2];
  /* [ebp-0x1c] collision_count */
  short collision_count;
  /* EBX: clip_count — number of clip planes accumulated */
  short clip_count;
  /* ESI inside loop: new clip_count during processing */
  short new_clip_count;
  /* scratch variables */
  char cVar6;
  char collision_hit;
  float abs_vx, abs_vy, abs_vz;
  float collision_time, scale;
  float dot, dot2, factor, pos_dot, len_sq;
  float *collision_record;
  float *plane_ptr;
  float *prev_plane_ptr;
  float *up_vec;

  /* Initialize local position/velocity copies */
  old_vel_copy[0] = old_vel[0];
  old_vel_copy[1] = old_vel[1];
  old_vel_copy[2] = old_vel[2];
  position[0] = old_pos[0];
  position[1] = old_pos[1];
  position[2] = old_pos[2];
  velocity[0] = old_vel[0];
  velocity[1] = old_vel[1];
  velocity[2] = old_vel[2];
  collision_count = 0;
  clip_count = 0;

  /* Assert valid initial position (0x3ad) */
  cVar6 = (char)valid_real_point3d(old_pos);
  if (cVar6 == '\0') {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
             "old_position", (double)old_pos[0], (double)old_pos[1],
             (double)old_pos[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\physics\\collisions.c",
                   0x3ad, 1);
    system_exit(-1);
  }
  /* Assert valid initial velocity (0x3ae) */
  cVar6 = (char)real_vector3d_valid(old_vel);
  if (cVar6 == '\0') {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
             "old_velocity", (double)old_vel[0], (double)old_vel[1],
             (double)old_vel[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\physics\\collisions.c",
                   0x3ae, 1);
    system_exit(-1);
  }

  /* Assert feature counts do not exceed maximum (0x3af, 0x3b0, 0x3b1) */
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

  /* ---- Main clipping loop ---- */
  while (collision_count < (short)max_clips) {
    /* Check if velocity is negligible (all components below epsilon) */
    abs_vx = velocity[0] < 0.0f ? -velocity[0] : velocity[0];
    abs_vy = velocity[1] < 0.0f ? -velocity[1] : velocity[1];
    abs_vz = velocity[2] < 0.0f ? -velocity[2] : velocity[2];
    if (abs_vx < *(double *)0x2533d0 && abs_vy < *(double *)0x2533d0 &&
        abs_vz < *(double *)0x2533d0)
      break;

    /* Assert collision_count < max_clips (0x3bf) */
    if ((int)collision_count >= (int)(short)max_clips) {
      display_assert("collision_count<maximum_collision_count",
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3bf, 1);
      system_exit(-1);
    }

    /* Setup collision record pointer: collisions + collision_count * 0x2c */
    collision_record =
      (float *)((char *)collisions + (int)collision_count * 0x2c);

    /* Assert valid clipped position before collision test (0x3c0) */
    cVar6 = (char)valid_real_point3d(position);
    if (cVar6 == '\0') {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
               "&clipped_position", (double)position[0], (double)position[1],
               (double)position[2]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3c0, 1);
      system_exit(-1);
    }
    /* Assert valid clipped velocity before collision test (0x3c1) */
    cVar6 = (char)real_vector3d_valid(velocity);
    if (cVar6 == '\0') {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&clipped_velocity", (double)velocity[0], (double)velocity[1],
               (double)velocity[2]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3c1, 1);
      system_exit(-1);
    }

    /* Call collision test: FUN_0014c4b0(features, position, velocity, record)
     */
    collision_hit =
      FUN_0014c4b0((int)features, position, velocity, (void *)collision_record);

    if (!collision_hit) {
      /* No collision: read endpoint position from record+0x04 and exit loop.
       * FUN_0014c4b0 fills record+0x04 even when no collision. */
      position[0] = collision_record[1];
      position[1] = collision_record[2];
      position[2] = collision_record[3];

      /* Validate the updated position (0x418, "&clipped_position") */
      cVar6 = (char)valid_real_point3d(position);
      if (cVar6 == '\0') {
        csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
                 "&clipped_position", (double)position[0], (double)position[1],
                 (double)position[2]);
        display_assert((char *)0x5ab100,
                       "c:\\halo\\SOURCE\\physics\\collisions.c", 0x418, 1);
        system_exit(-1);
      }
      goto post_loop;
    }

    /* ---- Collision found ---- */

    /* Read time and compute remaining velocity scale */
    collision_time = collision_record[0];
    scale = *(float *)0x2533c8 - collision_time; /* 1.0f - time */

    /* Scale old_vel_copy by remaining fraction */
    old_vel_copy[0] = old_vel_copy[0] * scale;
    old_vel_copy[1] = old_vel_copy[1] * scale;
    old_vel_copy[2] = old_vel_copy[2] * scale;

    /* Increment collision count */
    collision_count++;

    /* Read collision position from record+0x04 */
    collision_position[0] = collision_record[1];
    collision_position[1] = collision_record[2];
    collision_position[2] = collision_record[3];

    /* Assert valid collision position (0x3cc, "&position") */
    cVar6 = (char)valid_real_point3d(collision_position);
    if (cVar6 == '\0') {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
               "&position", (double)collision_position[0],
               (double)collision_position[1], (double)collision_position[2]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3cc, 1);
      system_exit(-1);
    }
    /* Assert valid old_vel_copy after scaling (0x3cd, "&velocity") */
    cVar6 = (char)real_vector3d_valid(old_vel_copy);
    if (cVar6 == '\0') {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&velocity", (double)old_vel_copy[0], (double)old_vel_copy[1],
               (double)old_vel_copy[2]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3cd, 1);
      system_exit(-1);
    }

    /* Read plane normal from record+0x10 and check unit length.
     * ESI points to the collision record; plane is at [esi+0x10..0x1c]. */
    plane_ptr = (float *)((char *)collision_record + 0x10);
    dot = plane_ptr[0] * plane_ptr[0] + plane_ptr[1] * plane_ptr[1] +
          plane_ptr[2] * plane_ptr[2];

    /* Assert |dot - 1.0| < epsilon (plane normal must be unit length) (0x3ce)
     */
    {
      float dot_check;
      dot_check = dot - *(float *)0x2533c8; /* dot - 1.0f */
      if (dot_check < 0.0f)
        dot_check = -dot_check;
      if (dot_check >= *(double *)0x2549d8) {
        csprintf(
          (char *)0x5ab100, "%s: assert_valid_real_plane3d(%f, %f, %f / %f)",
          "&collision->plane", (double)plane_ptr[0], (double)plane_ptr[1],
          (double)plane_ptr[2], (double)plane_ptr[3]);
        display_assert((char *)0x5ab100,
                       "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3ce, 1);
        system_exit(-1);
      }
    }

    /* Assert clip_count < 3 (0x3d1) */
    if (clip_count >= 3) {
      display_assert("clip_count<3", "c:\\halo\\SOURCE\\physics\\collisions.c",
                     0x3d1, 1);
      system_exit(-1);
    }

    /* Store collision record index and read plane into collision_plane.
     * clip_staging[0] = (collision_count - 1) = index into collision records.
     * The plane is read from collisions[clip_staging[0]*0x2c + 0x10]. */
    clip_staging[0] = (short)(collision_count - 1);
    plane_ptr =
      (float *)((char *)collisions + (int)clip_staging[0] * 0x2c + 0x10);
    collision_plane[0] = plane_ptr[0];
    collision_plane[1] = plane_ptr[1];
    collision_plane[2] = plane_ptr[2];
    collision_plane[3] = plane_ptr[3];

    /* === FIRST CLIP: velocity and position update ===
     * Velocity: vel[i] = old_vel_copy[i] + (-dot) * plane[i]
     * where dot = plane . old_vel_copy (NO clamp) */
    new_clip_count = 1;

    dot = collision_plane[0] * old_vel_copy[0] +
          collision_plane[1] * old_vel_copy[1] +
          collision_plane[2] * old_vel_copy[2];
    factor = -dot;

    velocity[0] = old_vel_copy[0] + factor * collision_plane[0];
    velocity[1] = old_vel_copy[1] + factor * collision_plane[1];
    velocity[2] = old_vel_copy[2] + factor * collision_plane[2];

    /* Position: pos[i] = collision_position[i] + (-(plane.col_pos - plane_d)) *
     * plane[i] */
    pos_dot = collision_plane[0] * collision_position[0] +
              collision_plane[1] * collision_position[1] +
              collision_plane[2] * collision_position[2] - collision_plane[3];
    factor = -pos_dot;

    position[0] = collision_position[0] + factor * collision_plane[0];
    position[1] = collision_position[1] + factor * collision_plane[1];
    position[2] = collision_position[2] + factor * collision_plane[2];

    if (clip_count > 0) {
      /* === Old clip_count > 0: check velocity against previous planes === */

      /* Check velocity against previous plane (clip_indices[0]) */
      prev_plane_ptr = (float *)((char *)collisions +
                                 (int)(short)clip_indices[0] * 0x2c + 0x10);
      dot = velocity[0] * prev_plane_ptr[0] + velocity[1] * prev_plane_ptr[1] +
            velocity[2] * prev_plane_ptr[2];

      if (dot < *(float *)0x26a810) {
        /* Velocity violates previous plane: find intersection of two planes */
        cVar6 = FUN_0010f480(plane_ptr, prev_plane_ptr, clip_result, clip_line);
        if (cVar6) {
          /* Validate clip_result (0x3e1, "&clip_line_point") */
          cVar6 = (char)valid_real_point3d(clip_result);
          if (cVar6 == '\0') {
            csprintf((char *)0x5ab100,
                     "%s: assert_valid_real_point3d(%f, %f, %f)",
                     "&clip_line_point", (double)clip_result[0],
                     (double)clip_result[1], (double)clip_result[2]);
            display_assert((char *)0x5ab100,
                           "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3e1, 1);
            system_exit(-1);
          }
          /* Validate clip_line (0x3e2, "&clip_line_vector") */
          cVar6 = (char)real_vector3d_valid(clip_line);
          if (cVar6 == '\0') {
            csprintf((char *)0x5ab100,
                     "%s: assert_valid_real_vector2d(%f, %f, %f)",
                     "&clip_line_vector", (double)clip_line[0],
                     (double)clip_line[1], (double)clip_line[2]);
            display_assert((char *)0x5ab100,
                           "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3e2, 1);
            system_exit(-1);
          }

          /* Project velocity onto intersection line:
           * t = clip_line . old_vel_copy / |clip_line|^2
           * velocity = t * clip_line */
          len_sq = clip_line[0] * clip_line[0] + clip_line[1] * clip_line[1] +
                   clip_line[2] * clip_line[2];
          dot2 =
            (clip_line[0] * old_vel_copy[0] + clip_line[1] * old_vel_copy[1] +
             clip_line[2] * old_vel_copy[2]) /
            len_sq;

          velocity[0] = dot2 * clip_line[0];
          velocity[1] = dot2 * clip_line[1];
          velocity[2] = dot2 * clip_line[2];

          /* Update position via collision_log_end_time:
           * target=collision_position, origin=clip_result,
           * output=position, dir=clip_line */
          collision_log_end_time(collision_position, clip_result, position,
                                 clip_line);

          clip_staging[1] = clip_indices[0];
          new_clip_count = 2;

          if (clip_count > 1) {
            /* Was 2+ clips: check velocity against clip_indices[1] plane */
            prev_plane_ptr =
              (float *)((char *)collisions +
                        (int)(short)clip_indices[1] * 0x2c + 0x10);
            dot = velocity[0] * prev_plane_ptr[0] +
                  velocity[1] * prev_plane_ptr[1] +
                  velocity[2] * prev_plane_ptr[2];

            if (dot < *(float *)0x26a810) {
              /* Velocity violates second previous plane too.
               * Try intersection with third plane. */
              prev_plane_ptr = (float *)((char *)collisions +
                                         (int)clip_staging[0] * 0x2c + 0x10);
              cVar6 = FUN_0010f480(
                prev_plane_ptr,
                (float *)((char *)collisions +
                          (int)(short)clip_indices[1] * 0x2c + 0x10),
                clip_result, clip_line);
              if (cVar6) {
                /* Validate clip_result (0x3fe, "&clip_line_point") */
                cVar6 = (char)valid_real_point3d(clip_result);
                if (cVar6 == '\0') {
                  csprintf((char *)0x5ab100,
                           "%s: assert_valid_real_point3d(%f, %f, %f)",
                           "&clip_line_point", (double)clip_result[0],
                           (double)clip_result[1], (double)clip_result[2]);
                  display_assert((char *)0x5ab100,
                                 "c:\\halo\\SOURCE\\physics\\collisions.c",
                                 0x3fe, 1);
                  system_exit(-1);
                }
                /* Validate clip_line (0x3ff, "&clip_line_vector") */
                cVar6 = (char)real_vector3d_valid(clip_line);
                if (cVar6 == '\0') {
                  csprintf((char *)0x5ab100,
                           "%s: assert_valid_real_vector2d(%f, %f, %f)",
                           "&clip_line_vector", (double)clip_line[0],
                           (double)clip_line[1], (double)clip_line[2]);
                  display_assert((char *)0x5ab100,
                                 "c:\\halo\\SOURCE\\physics\\collisions.c",
                                 0x3ff, 1);
                  system_exit(-1);
                }

                /* Project velocity onto new intersection line */
                len_sq = clip_line[0] * clip_line[0] +
                         clip_line[1] * clip_line[1] +
                         clip_line[2] * clip_line[2];
                dot2 = (clip_line[0] * old_vel_copy[0] +
                        clip_line[1] * old_vel_copy[1] +
                        clip_line[2] * old_vel_copy[2]) /
                       len_sq;

                velocity[0] = dot2 * clip_line[0];
                velocity[1] = dot2 * clip_line[1];
                velocity[2] = dot2 * clip_line[2];

                /* Update position */
                collision_log_end_time(collision_position, clip_result,
                                       position, clip_line);

                clip_staging[1] = clip_indices[1];
                new_clip_count = 2;
              } else {
                /* Intersection failed — try 3-plane intersection */
                cVar6 = FUN_0010f480(collision_plane, clip_line, clip_point,
                                     clip_result);
                if (cVar6) {
                  /* Validate clip_point (0x3ee, "&clip_point") */
                  cVar6 = (char)valid_real_point3d(clip_point);
                  if (cVar6 == '\0') {
                    csprintf((char *)0x5ab100,
                             "%s: assert_valid_real_point3d(%f, %f, %f)",
                             "&clip_point", (double)clip_point[0],
                             (double)clip_point[1], (double)clip_point[2]);
                    display_assert((char *)0x5ab100,
                                   "c:\\halo\\SOURCE\\physics\\collisions.c",
                                   0x3ee, 1);
                    system_exit(-1);
                  }
                }

                /* Cornered: zero velocity, snap to clip_point */
                velocity[0] = 0.0f;
                velocity[1] = 0.0f;
                velocity[2] = 0.0f;
                position[0] = clip_point[0];
                position[1] = clip_point[1];
                position[2] = clip_point[2];
                new_clip_count = 3;
              }
            }
            /* else: velocity satisfies second previous plane, keep
             * new_clip_count=2 */
          }
          /* else: was 1 clip, now 2 — keep new_clip_count=2 */
        } else {
          /* FUN_0010f480 failed — try 3-plane intersection */
          cVar6 =
            FUN_0010f480(collision_plane, clip_line, clip_point, clip_result);
          if (cVar6) {
            /* Validate clip_point (0x3ee, "&clip_point") */
            cVar6 = (char)valid_real_point3d(clip_point);
            if (cVar6 == '\0') {
              csprintf((char *)0x5ab100,
                       "%s: assert_valid_real_point3d(%f, %f, %f)",
                       "&clip_point", (double)clip_point[0],
                       (double)clip_point[1], (double)clip_point[2]);
              display_assert((char *)0x5ab100,
                             "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3ee,
                             1);
              system_exit(-1);
            }
          }

          /* Cornered: zero velocity, snap to clip_point */
          velocity[0] = 0.0f;
          velocity[1] = 0.0f;
          velocity[2] = 0.0f;
          position[0] = clip_point[0];
          position[1] = clip_point[1];
          position[2] = clip_point[2];
          new_clip_count = 3;
        }
      } else {
        /* Velocity satisfies previous plane: single-clip recovery.
         * If old clip_count was 1, just keep as single clip. */
        if (clip_count > 1) {
          /* Was 2+ clips: check velocity against clip_indices[1] plane */
          prev_plane_ptr = (float *)((char *)collisions +
                                     (int)(short)clip_indices[1] * 0x2c + 0x10);
          dot = velocity[0] * prev_plane_ptr[0] +
                velocity[1] * prev_plane_ptr[1] +
                velocity[2] * prev_plane_ptr[2];

          if (dot < *(float *)0x26a810) {
            /* Velocity violates second previous plane.
             * Find intersection using clip_staging[0] plane. */
            prev_plane_ptr = (float *)((char *)collisions +
                                       (int)clip_staging[0] * 0x2c + 0x10);
            cVar6 =
              FUN_0010f480(prev_plane_ptr,
                           (float *)((char *)collisions +
                                     (int)(short)clip_indices[1] * 0x2c + 0x10),
                           clip_result, clip_line);
            if (cVar6) {
              /* Validate clip_result (0x3fe, "&clip_line_point") */
              cVar6 = (char)valid_real_point3d(clip_result);
              if (cVar6 == '\0') {
                csprintf((char *)0x5ab100,
                         "%s: assert_valid_real_point3d(%f, %f, %f)",
                         "&clip_line_point", (double)clip_result[0],
                         (double)clip_result[1], (double)clip_result[2]);
                display_assert((char *)0x5ab100,
                               "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3fe,
                               1);
                system_exit(-1);
              }
              /* Validate clip_line (0x3ff, "&clip_line_vector") */
              cVar6 = (char)real_vector3d_valid(clip_line);
              if (cVar6 == '\0') {
                csprintf((char *)0x5ab100,
                         "%s: assert_valid_real_vector2d(%f, %f, %f)",
                         "&clip_line_vector", (double)clip_line[0],
                         (double)clip_line[1], (double)clip_line[2]);
                display_assert((char *)0x5ab100,
                               "c:\\halo\\SOURCE\\physics\\collisions.c", 0x3ff,
                               1);
                system_exit(-1);
              }

              /* Project velocity onto intersection line */
              len_sq = clip_line[0] * clip_line[0] +
                       clip_line[1] * clip_line[1] +
                       clip_line[2] * clip_line[2];
              dot2 = (clip_line[0] * old_vel_copy[0] +
                      clip_line[1] * old_vel_copy[1] +
                      clip_line[2] * old_vel_copy[2]) /
                     len_sq;

              velocity[0] = dot2 * clip_line[0];
              velocity[1] = dot2 * clip_line[1];
              velocity[2] = dot2 * clip_line[2];

              /* Update position */
              collision_log_end_time(collision_position, clip_result, position,
                                     clip_line);

              clip_staging[1] = clip_indices[1];
              new_clip_count = 2;
            } else {
              /* Intersection failed — cornered */
              cVar6 = FUN_0010f480(collision_plane, clip_line, clip_point,
                                   clip_result);
              if (cVar6) {
                cVar6 = (char)valid_real_point3d(clip_point);
                if (cVar6 == '\0') {
                  csprintf((char *)0x5ab100,
                           "%s: assert_valid_real_point3d(%f, %f, %f)",
                           "&clip_point", (double)clip_point[0],
                           (double)clip_point[1], (double)clip_point[2]);
                  display_assert((char *)0x5ab100,
                                 "c:\\halo\\SOURCE\\physics\\collisions.c",
                                 0x3ee, 1);
                  system_exit(-1);
                }
              }
              velocity[0] = 0.0f;
              velocity[1] = 0.0f;
              velocity[2] = 0.0f;
              position[0] = clip_point[0];
              position[1] = clip_point[1];
              position[2] = clip_point[2];
              new_clip_count = 3;
            }
          }
          /* else: velocity satisfies second previous plane too, keep
           * new_clip_count=1 */
        }
        /* else: was 0 or 1 clip, just single clip, new_clip_count=1 */
      }
    }
    /* else: old clip_count was 0, just first clip, new_clip_count=1 */

    /* Assert valid position after clipping (0x40a, "&clipped_position") */
    cVar6 = (char)valid_real_point3d(position);
    if (cVar6 == '\0') {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
               "&clipped_position", (double)position[0], (double)position[1],
               (double)position[2]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x40a, 1);
      system_exit(-1);
    }
    /* Assert valid velocity after clipping (0x40b, "&clipped_velocity") */
    cVar6 = (char)real_vector3d_valid(velocity);
    if (cVar6 == '\0') {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&clipped_velocity", (double)velocity[0], (double)velocity[1],
               (double)velocity[2]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x40b, 1);
      system_exit(-1);
    }

    /* Copy clip indices from staging area (clip_staging[0] + saved)
     * into clip_indices array. Size = new_clip_count * sizeof(short). */
    csmemcpy(clip_indices, clip_staging, (int)new_clip_count * 2);
    clip_count = new_clip_count;

  } /* end main clipping loop */

post_loop:
  /* Copy final position to output */
  new_pos[0] = position[0];
  new_pos[1] = position[1];
  new_pos[2] = position[2];

  /* Switch on clip_count to compute final output velocity */
  switch ((int)clip_count) {
  case 0:
    /* No clips: output velocity = original velocity */
    new_vel[0] = old_vel[0];
    new_vel[1] = old_vel[1];
    new_vel[2] = old_vel[2];
    break;

  case 1:
    /* Single clip: validate collision_plane, then reflect old_vel.
     * dot = plane . old_vel; factor = -dot (no clamp in original);
     * new_vel = old_vel + factor * plane */
    if (!FUN_0010a480((int)collision_plane)) {
      csprintf((char *)0x5ab100,
               "%s: assert_valid_real_plane3d(%f, %f, %f / %f)", "&clip_plane",
               (double)collision_plane[0], (double)collision_plane[1],
               (double)collision_plane[2], (double)collision_plane[3]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x428, 1);
      system_exit(-1);
    }
    dot = collision_plane[0] * old_vel[0] + collision_plane[1] * old_vel[1] +
          collision_plane[2] * old_vel[2];
    factor = -dot;
    new_vel[0] = old_vel[0] + factor * collision_plane[0];
    new_vel[1] = old_vel[1] + factor * collision_plane[1];
    new_vel[2] = old_vel[2] + factor * collision_plane[2];
    break;

  case 2:
    /* Two clips: validate clip_result and clip_line, then project old_vel
     * onto clip_line via collision_log_usage. */
    if (!valid_real_point3d(clip_result)) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
               "&clip_line_point", (double)clip_result[0],
               (double)clip_result[1], (double)clip_result[2]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x42d, 1);
      system_exit(-1);
    }
    if (!real_vector3d_valid(clip_line)) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&clip_line_vector", (double)clip_line[0], (double)clip_line[1],
               (double)clip_line[2]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x42e, 1);
      system_exit(-1);
    }
    collision_log_usage(new_vel, old_vel, clip_line);
    break;

  case 3:
    /* Three clips: cornered, validate clip_point, zero velocity */
    if (!valid_real_point3d(clip_point)) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
               "&clip_point", (double)clip_point[0], (double)clip_point[1],
               (double)clip_point[2]);
      display_assert((char *)0x5ab100,
                     "c:\\halo\\SOURCE\\physics\\collisions.c", 0x433, 1);
      system_exit(-1);
    }
    new_vel[0] = 0.0f;
    new_vel[1] = 0.0f;
    new_vel[2] = 0.0f;
    break;

  default:
    /* Unreachable (0x438) */
    display_assert("unreachable", "c:\\halo\\SOURCE\\physics\\collisions.c",
                   0x438, 1);
    system_exit(-1);
    break;
  }

  /* Assert valid final position (0x43b, "new_position") */
  if (!valid_real_point3d(new_pos)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
             "new_position", (double)new_pos[0], (double)new_pos[1],
             (double)new_pos[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\physics\\collisions.c",
                   0x43b, 1);
    system_exit(-1);
  }
  /* Assert valid final velocity (0x43c, "new_velocity") */
  if (!real_vector3d_valid(new_vel)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
             "new_velocity", (double)new_vel[0], (double)new_vel[1],
             (double)new_vel[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\physics\\collisions.c",
                   0x43c, 1);
    system_exit(-1);
  }

  /* ---- Collision log post-processing (disasm 0x1501d8..0x150414) ----
   * When clip_count > 1 (2 or 3) and collision_count < max_clips, synthesize
   * an extra "edge" collision record at collisions[collision_count]. The
   * record's plane normal (record+0x10..+0x18) is the edge direction, and
   * record+0x1c is the plane distance d = dot(anchor, edge_normal). The handle
   * fields (+0x20/+0x24) are cleared to -1 and flags zeroed.
   *
   * Structural facts derived from disassembly:
   *  - EDI at the record copy holds clip_count (the post_loop switch
   *    selector), so the source copied is clip_indices[clip_count-1] (the LAST
   *    clip). The copy + count++ + handle-clear happen BEFORE the best search.
   *  - The best search keeps the record with the SMALLEST record[6]
   *    (plane_normal[2]), seeded at 0.0: best updated when best_time>record[6].
   *    best_idx stays -1 if no record[6] is below 0.0.
   *  - Four edge cases (clip_count x best_idx==-1):
   *      (2,-1): up-vector projection perpendicular to clip_line; normalize;
   *              d via clip_result.   [0x150307]
   *      (2,>=0): cross product of clip_line with clip_indices[best_idx]'s
   *              normal; normalize; d via clip_result.   [0x150290]
   *      (3,>=0): up - src_normal[2]*src_normal; normalize; d via clip_point.
   *              [0x15037d]
   *      (3,-1): edge = up (raw copy); NO normalize, NO count adjust; d via
   *              clip_point.   [0x1503ea]
   *  - The normalized paths share a tail: on a zero-length edge,
   *    collision_count is decremented back; otherwise d is stored. */
  if (clip_count > 1 && collision_count < (short)max_clips) {
    float *src_record;
    float *dst_record;
    float *edge; /* dst_record + 0x10 = &dst_record[4] */
    float *anchor;
    float best_time;
    float edge_len;

    short best_idx;
    short idx;

    /* Destination = next slot at collisions[collision_count]. */
    dst_record = (float *)((char *)collisions + (int)collision_count * 0x2c);

    /* Copy the LAST clip record (clip_indices[clip_count-1]) into the new
     * slot: first 16 bytes (time + position). EDI == clip_count here. */
    src_record = (float *)((char *)collisions +
                           (int)(short)clip_indices[clip_count - 1] * 0x2c);
    dst_record[0] = src_record[0]; /* time */
    dst_record[1] = src_record[1]; /* position[0] */
    dst_record[2] = src_record[2]; /* position[1] */
    dst_record[3] = src_record[3]; /* position[2] */

    /* Count the new record now (decremented later if the edge degenerates). */
    collision_count++;

    /* Clear handles / feature index / flags in destination record. */
    *(int *)((char *)dst_record + 0x20) = -1; /* object_handle = -1 */
    *(int *)((char *)dst_record + 0x24) = -1; /* surface_handle = -1 */
    *(char *)((char *)dst_record + 0x28) = 0;
    *(char *)((char *)dst_record + 0x29) = 0;
    *(short *)((char *)dst_record + 0x2a) = -1;

    /* Find the clip record with the SMALLEST record[6] (plane_normal[2]),
     * seeded at 0.0. */
    best_time = *(float *)0x2533c0; /* 0.0f */
    best_idx = -1;
    if (clip_count > 0) {
      for (idx = 0; idx < clip_count; idx++) {
        src_record =
          (float *)((char *)collisions + (int)(short)clip_indices[idx] * 0x2c);
        if (best_time >
            src_record[6]) { /* record[6] = +0x18 = plane_normal[2] */
          best_idx = idx;
          best_time = src_record[6];
        }
      }
    }

    edge = (float *)((char *)dst_record + 0x10);
    up_vec = *(float **)0x31fc44;

    if (clip_count == 2) {
      anchor = clip_result;
      if (best_idx == -1) {
        /* Up-vector projected perpendicular to clip_line:
         * factor = -(clip_line[2] / |clip_line|^2); edge = up +
         * factor*clip_line. */
        len_sq = clip_line[0] * clip_line[0] + clip_line[1] * clip_line[1] +
                 clip_line[2] * clip_line[2];
        factor = -(clip_line[2] / len_sq);
        edge[0] = factor * clip_line[0] + up_vec[0];
        edge[1] = factor * clip_line[1] + up_vec[1];
        edge[2] = factor * clip_line[2] + up_vec[2];
      } else {
        /* Cross product of clip_line with clip_indices[best_idx]'s normal.
         * best_idx==0 -> cross(clip_line, src_normal);
         * best_idx!=0 -> cross(src_normal, clip_line). */
        src_record = (float *)((char *)collisions +
                               (int)(short)clip_indices[best_idx] * 0x2c);
        if (best_idx == 0) {
          edge[0] = clip_line[1] * src_record[6] - clip_line[2] * src_record[5];
          edge[1] = clip_line[2] * src_record[4] - clip_line[0] * src_record[6];
          edge[2] = clip_line[0] * src_record[5] - clip_line[1] * src_record[4];
        } else {
          edge[0] = clip_line[2] * src_record[5] - clip_line[1] * src_record[6];
          edge[1] = clip_line[0] * src_record[6] - clip_line[2] * src_record[4];
          edge[2] = clip_line[1] * src_record[4] - clip_line[0] * src_record[5];
        }
      }
    } else {
      /* clip_count == 3 */
      anchor = clip_point;
      if (best_idx == -1) {
        /* edge = up vector (raw copy); NO normalize, NO count adjust. */
        edge[0] = up_vec[0];
        edge[1] = up_vec[1];
        edge[2] = up_vec[2];
        /* d = dot(clip_point, edge); stored directly (skips normalize tail). */
        *(float *)((char *)dst_record + 0x1c) =
          (anchor[1] * edge[1] + anchor[2] * edge[2]) + anchor[0] * edge[0];
        return collision_count;
      }
      /* edge = up - src_normal[2] * src_normal (src = clip_indices[best_idx]).
       */
      src_record = (float *)((char *)collisions +
                             (int)(short)clip_indices[best_idx] * 0x2c);
      factor = -src_record[6]; /* -plane_normal[2] */
      edge[0] = factor * src_record[4] + up_vec[0];
      edge[1] = factor * src_record[5] + up_vec[1];
      edge[2] = factor * src_record[6] + up_vec[2];
    }

    /* Shared normalize tail: normalize the edge; on zero length remove the
     * synthesized record, otherwise store d = dot(anchor, edge). */
    edge_len = normalize3d(edge);
    if (edge_len == *(float *)0x2533c0) { /* length == 0.0 */
      collision_count--;
    } else {
      *(float *)((char *)dst_record + 0x1c) =
        (anchor[1] * edge[1] + anchor[2] * edge[2]) + anchor[0] * edge[0];
    }
  }

  return collision_count;
}
