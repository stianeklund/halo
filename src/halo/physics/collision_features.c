/* 0x14ad40 — Zero-initialize a 6-byte collision features header */
void collision_features_init(void *features)
{
  csmemset(features, 0, 6);
}

/* 0x14b3d0 — Look up a prism collision element by handle, resolve its
 * tag block chain (prisms -> regions -> materials), extract material flags,
 * optionally transform the element, then add it as a collision feature.
 * Called once per prism in the collision results. */
void FUN_0014b3d0(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6, void *features)
{
  void *elem;
  int iVar2;
  int iVar3;
  int uVar4;
  float local_10[3];

  elem = tag_block_get_element((void *)(param_1 + 0x54), param_2, 0x10);
  iVar2 = (int)tag_block_get_element((void *)(param_1 + 0x48),
                                     *(int *)((int)elem + 0xc), 0x18);
  iVar3 = (int)tag_block_get_element((void *)(param_1 + 0x3c),
                                     *(int *)(iVar2 + 0x10), 0xc);

  if (param_6 != -1) {
    uVar4 = -1;
  } else {
    uVar4 = *(int *)(iVar2 + 0x10);
  }

  if (param_3 != 0) {
    matrix_transform_point((float *)param_3, (float *)elem, local_10);
    elem = local_10;
  }

  FUN_0014adb0((int)elem, param_4, param_5, param_6, uVar4,
               *(unsigned char *)(iVar3 + 8),
               *(unsigned char *)(iVar3 + 9),
               *(unsigned short *)(iVar3 + 0xa), features);
}

/* 0x14b6f0 — Iterate collision results and add features for each object.
 * Three sections in collision_results: spheres at [0], cylinders at [0x101],
 * prisms at [0x202]. Each section has a count followed by object handles. */
void collision_features_add(int param_1, int *collision_results, int param_3,
                            int param_4, int param_5, int param_6,
                            void *features)
{
  short *counts = (short *)features;
  int i;

  assert_halt(counts[0] >= 0 && counts[0] <= 0x100);
  assert_halt(counts[1] >= 0 && counts[1] <= 0x100);
  assert_halt(counts[2] >= 0 && counts[2] <= 0x100);

  for (i = 0; i < collision_results[0x202]; i++)
    FUN_0014b3d0(param_1, collision_results[0x203 + i], param_3, param_4,
                 param_5, param_6, features);

  for (i = 0; i < collision_results[0x101]; i++)
    FUN_0014b470(param_1, collision_results[0x102 + i], param_3, param_4,
                 param_5, param_6, features);

  for (i = 0; i < collision_results[0]; i++)
    FUN_0014b620(param_1, collision_results[1 + i], param_3, param_4, param_5,
                 param_6, features);
}

/* 0x14bc10 — Test line-of-sight against all collision features.
 * Iterates spheres, cylinders, and prisms finding the farthest intersection.
 * Returns true if any hit found, writing t_hit, normal, and feature ID. */
bool collision_features_test_los(void *features, void *los_data, void *out_hit)
{
  char *base = (char *)features;
  short best_type = -1;
  short best_index = -1;
  float best_t = -3.4028235e+38f;
  float saved_normal[4];
  short type;

  for (type = 0; type < 3; type++) {
    short count = *(short *)(base + type * 2);
    short i;
    for (i = 0; i < count; i++) {
      float current_t;
      float current_normal[4];
      char hit = 0;

      if (type == 0)
        hit = FUN_0014b890(base + 8 + i * 0x1c, los_data, &current_t,
                           current_normal);
      else if (type == 1)
        hit = FUN_0014b960(base + 0x1c08 + i * 0x28, los_data, &current_t,
                           current_normal);
      else if (type == 2)
        hit = FUN_0014bae0(base + 0x4408 + i * 0x68, los_data, &current_t,
                           current_normal);

      if (hit && best_t < current_t) {
        saved_normal[0] = current_normal[0];
        saved_normal[1] = current_normal[1];
        saved_normal[2] = current_normal[2];
        saved_normal[3] = current_normal[3];
        best_type = type;
        best_index = i;
        best_t = current_t;
      }
    }
  }

  if (best_type == -1)
    return 0;

  *(float *)((char *)out_hit) = best_t;
  *(float *)((char *)out_hit + 0x10) = saved_normal[0];
  *(float *)((char *)out_hit + 0x14) = saved_normal[1];
  *(float *)((char *)out_hit + 0x18) = saved_normal[2];
  *(float *)((char *)out_hit + 0x1c) = saved_normal[3];

  {
    char *feature_data;
    if (best_type == 0)
      feature_data = base + 8 + best_index * 0x1c;
    else if (best_type == 1)
      feature_data = base + 0x1c08 + best_index * 0x28;
    else if (best_type == 2)
      feature_data = base + 0x4408 + best_index * 0x68;
    else
      return 1;

    *(int *)((char *)out_hit + 0x20) = *(int *)feature_data;
    *(int *)((char *)out_hit + 0x24) = *(int *)(feature_data + 4);
    *((char *)out_hit + 0x28) = *(feature_data + 8);
    *((char *)out_hit + 0x29) = *(feature_data + 9);
    *(short *)((char *)out_hit + 0x2a) = *(short *)(feature_data + 0xa);
  }

  return 1;
}
