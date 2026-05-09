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

/* 0x14b470 — Look up a cylinder collision element by handle, resolve its
 * tag block chain (edges -> regions -> vertices -> nodes), compute the
 * displacement vector between vertex endpoints, check surface normal
 * consistency via scalar triple product against a threshold, optionally
 * transform position and direction, then add a cylinder collision feature.
 * Called once per cylinder in the collision results. */
void FUN_0014b470(int param_1, int object_handle, int param_3, int param_4,
                  int param_5, int param_6, void *features)
{
  void *edge;
  int *region_0;
  int *region_1;
  float *vertex_0;
  float *vertex_1;
  int node_0;
  int node_1;
  unsigned int raw_0;
  unsigned int raw_1;
  char sign_0;
  char sign_1;
  float displacement[3];
  float local_34[3];
  float *position;
  float *direction;
  int uVar6;
  float angle;

  edge = tag_block_get_element((void *)(param_1 + 0x48), object_handle, 0x18);
  region_0 = (int *)tag_block_get_element((void *)(param_1 + 0x3c),
                                          *(int *)((int)edge + 0x10), 0xc);
  region_1 = (int *)tag_block_get_element((void *)(param_1 + 0x3c),
                                          *(int *)((int)edge + 0x14), 0xc);

  if (*(unsigned int *)region_0 == *(unsigned int *)region_1)
    return;

  vertex_0 = (float *)tag_block_get_element((void *)(param_1 + 0x54),
                                            *(int *)edge, 0x10);
  vertex_1 = (float *)tag_block_get_element((void *)(param_1 + 0x54),
                                            *(int *)((int)edge + 0x4), 0x10);

  raw_0 = *(unsigned int *)region_0;
  raw_1 = *(unsigned int *)region_1;
  node_0 = (int)tag_block_get_element((void *)(param_1 + 0xc),
                                      raw_0 & 0x7fffffff, 0x10);
  node_1 = (int)tag_block_get_element((void *)(param_1 + 0xc),
                                      raw_1 & 0x7fffffff, 0x10);

  sign_0 = (raw_0 & 0x80000000) != 0;
  sign_1 = (raw_1 & 0x80000000) != 0;

  displacement[0] = vertex_1[0] - vertex_0[0];
  displacement[1] = vertex_1[1] - vertex_0[1];
  displacement[2] = vertex_1[2] - vertex_0[2];

  if ((raw_0 & 0x7fffffff) != (raw_1 & 0x7fffffff)) {
    if (sign_0 == sign_1) {
      angle = FUN_000993b0((float *)node_0, (float *)node_1, displacement);
      if (angle <= -1.0e-4f)
        return;
    } else {
      angle = FUN_000993b0((float *)node_0, (float *)node_1, displacement);
      if (angle >= 1.0e-4f)
        return;
    }
  }

  uVar6 = -1;
  if (param_6 == -1) {
    uVar6 = *(int *)((int)edge + 0x10);
  }

  if (param_3 != 0) {
    matrix_scale_transform_vector((float *)param_3, displacement, displacement);
    direction = displacement;
    matrix_transform_point((float *)param_3, vertex_0, local_34);
    position = local_34;
  } else {
    direction = displacement;
    position = vertex_0;
  }

  FUN_0014aee0(position, direction, *(float *)&param_4, param_5, param_6,
               uVar6, *(unsigned char *)((int)region_0 + 8),
               *(unsigned char *)((int)region_0 + 9),
               *(unsigned short *)((int)region_0 + 0xa), features);
}

/* 0x14b620 — Look up a sphere collision surface by handle, collect its
 * vertex points via the BSP edge walk (FUN_00147410), resolve the surface
 * plane via the material element's plane reference, optionally transform
 * all points and the plane through a matrix, then add the surface as a
 * prism collision feature via FUN_0014b220.
 * Called once per sphere surface in the collision results. */
void FUN_0014b620(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6, void *features)
{
  int *material_elem;
  int point_count_packed;
  int material_index;
  float points[24]; /* up to 8 vertices * 3 floats = 96 bytes */
  float plane[4];

  material_elem = (int *)tag_block_get_element((void *)(param_1 + 0x3c),
                                                param_2, 0xc);
  point_count_packed = FUN_00147410(param_1, param_2, (void *)points);
  FUN_00099640(param_1, (uint32_t)*material_elem, plane);

  if (param_3 != 0) {
    if ((short)point_count_packed > 0) {
      unsigned int n = (unsigned short)(short)point_count_packed;
      float *p = points;
      do {
        matrix_transform_point((float *)param_3, p, p);
        p += 3;
        n--;
      } while (n != 0);
    }
    FUN_0010a1c0((float *)param_3, plane, plane);
  }

  if (param_6 != -1) {
    material_index = -1;
  } else {
    material_index = param_2;
  }

  FUN_0014b220(point_count_packed, (void *)points, plane,
               *(float *)&param_4, param_5, param_6, material_index,
               (int)*(unsigned char *)((char *)material_elem + 8),
               (int)*(unsigned char *)((char *)material_elem + 9),
               (int)*(unsigned short *)((char *)material_elem + 0xa),
               features);
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

/* 0x14b890 — Test a sphere collision feature against a line-of-sight point.
 * Computes the displacement from the sphere center (feature+0x0C) to the test
 * point, checks if the squared distance is less than the sphere radius squared
 * (feature+0x18). If inside, normalizes the displacement to produce the outward
 * normal, computes the plane equation (normal[3] = dot(center,normal) + radius),
 * and returns the penetration depth (radius - distance). Returns 1 if the point
 * is inside the sphere, 0 otherwise. */
char FUN_0014b890(void *feature, void *los_data, float *t_hit, float *normal)
{
  float *sphere = (float *)feature;
  float *point = (float *)los_data;
  float dx, dy, dz;
  float dist_sq, dist;
  float radius;
  float scale;

  /* displacement from sphere center to test point */
  dx = point[0] - sphere[3];   /* sphere+0x0C */
  dy = point[1] - sphere[4];   /* sphere+0x10 */
  dz = point[2] - sphere[5];   /* sphere+0x14 */

  /* squared distance */
  dist_sq = dz * dz + dx * dx + dy * dy;

  radius = sphere[6]; /* sphere+0x18 */

  /* point is outside sphere */
  if (dist_sq >= radius * radius)
    return 0;

  /* compute actual distance */
  dist = sqrtf(dist_sq);

  if (dist > 0.0f) {
    /* normalize displacement to get outward normal */
    scale = 1.0f / dist;
    normal[0] = dx * scale;
    normal[1] = dy * scale;
    normal[2] = dz * scale;
  } else {
    /* degenerate: point is at sphere center, use up vector */
    normal[0] = 0.0f;
    normal[1] = 0.0f;
    normal[2] = 1.0f;
  }

  /* plane distance: dot(center, normal) + radius */
  normal[3] = sphere[5] * normal[2] + sphere[4] * normal[1] +
              sphere[3] * normal[0] + sphere[6];

  /* penetration depth: radius - distance */
  *t_hit = sphere[6] - dist;

  return 1;
}

/* 0x14b960 — Test a cylinder collision feature against a line-of-sight point.
 * Projects the point onto the cylinder axis, checks it is within the cylinder
 * bounds and radius, then computes the perpendicular normal and penetration
 * depth. Returns 1 if the point is inside the cylinder, 0 otherwise. */
char FUN_0014b960(void *feature, void *los_data, float *t_hit, float *normal)
{
  float *cyl = (float *)feature;
  float *point = (float *)los_data;
  float dx, dy, dz;
  float dot, axis_len_sq;
  float radius;
  float t, t_ax, t_ay, t_az;
  float mag;

  /* displacement from cylinder origin to test point */
  dx = point[0] - cyl[3];   /* cyl+0x0C */
  dy = point[1] - cyl[4];   /* cyl+0x10 */
  dz = point[2] - cyl[5];   /* cyl+0x14 */

  /* project displacement onto cylinder axis */
  dot = dz * cyl[8] + dy * cyl[7] + dx * cyl[6]; /* axis at cyl+0x18,0x1C,0x20 */

  /* point is behind cylinder start */
  if (dot < 0.0f)
    return 0;

  /* compute squared length of axis */
  axis_len_sq = cyl[6] * cyl[6] + cyl[7] * cyl[7] + cyl[8] * cyl[8];

  /* point is beyond cylinder end */
  if (dot > axis_len_sq)
    return 0;

  radius = cyl[9]; /* cyl+0x24 */

  /* point is outside cylinder radius:
   * d_len_sq * axis_len_sq - dot^2 must be < radius^2 * axis_len_sq */
  if (radius * radius * axis_len_sq <=
      (dx * dx + dy * dy + dz * dz) * axis_len_sq - dot * dot)
    return 0;

  if (axis_len_sq > 0.0f) {
    /* project out the axial component to get perpendicular direction */
    t = dot / axis_len_sq;
    t_ax = t * cyl[6];
    t_ay = t * cyl[7];
    t_az = t * cyl[8];
    normal[0] = dx - t_ax;
    normal[1] = dy - t_ay;
    normal[2] = dz - t_az;
  } else {
    /* degenerate cylinder: use displacement as normal direction */
    *(int *)&normal[0] = *(int *)&dx;
    *(int *)&normal[1] = *(int *)&dy;
    *(int *)&normal[2] = *(int *)&dz;
  }

  mag = normalize3d(normal);

  if (mag == 0.0f) {
    normal[0] = 0.0f;
    normal[1] = 0.0f;
    normal[2] = 1.0f;
  }

  /* plane distance: dot(origin, normal) + radius */
  normal[3] = cyl[5] * normal[2] + cyl[4] * normal[1] + cyl[3] * normal[0] + radius;

  /* penetration depth: radius - perpendicular distance */
  *t_hit = radius - mag;

  return 1;
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
