/* Projection axis remapping table at 0x28cb10.
 * Indexed as [projection_axis * 2 + projection_sign][component].
 * Maps a 3D projection basis + sign to two axis indices for 2D projection. */
static const short g_projection3d_mappings[6][2] = {
  { 2, 1 }, { 1, 2 }, { 0, 2 }, { 2, 0 }, { 1, 0 }, { 0, 1 },
};

/* 0x14ad40 — Zero-initialize a 6-byte collision features header */
void collision_features_init(void *features)
{
  csmemset(features, 0, 6);
}

/* 0x14adb0 — Add a sphere collision feature to the features buffer.
 * Writes a sphere entry (position, material, surface ID). If param_2 > 0,
 * also writes a second sphere entry and a cylinder entry with position_z
 * adjusted by subtracting param_2 (depth/height correction). */
void collision_features_from_point(int param_1, float param_2, int param_3,
                                   int param_4, int param_5,
                                   unsigned char param_6, unsigned char param_7,
                                   short param_8, void *features)
{
  short sVar4;
  char *entry;
  float *pos;
  float fVar2;

  pos = (float *)param_1;

  /* First sphere entry — always written if count < 256 */
  sVar4 = *(short *)features;
  if (sVar4 < 0x100) {
    *(short *)features = (short)(sVar4 + 1);
    entry = (char *)features + (int)sVar4 * 0x1c + 0x8;
    *(int *)(entry + 0x00) = param_4;
    *(int *)(entry + 0x04) = param_5;
    *(unsigned char *)(entry + 0x08) = param_6;
    *(short *)(entry + 0x0a) = param_8;
    *(unsigned char *)(entry + 0x09) = param_7;
    *(float *)(entry + 0x0c) = pos[0];
    *(float *)(entry + 0x10) = pos[1];
    *(float *)(entry + 0x14) = pos[2];
    *(int *)(entry + 0x18) = param_3;
  }

  /* If param_2 > 0.0f, write a second sphere entry and a cylinder entry */
  if (param_2 > 0.0f) {
    fVar2 = pos[2] - param_2;

    /* Second sphere entry with adjusted z */
    sVar4 = *(short *)features;
    if (sVar4 < 0x100) {
      entry = (char *)features + (int)sVar4 * 0x1c + 0x8;
      *(short *)features = (short)(sVar4 + 1);
      *(int *)(entry + 0x00) = param_4;
      *(int *)(entry + 0x04) = param_5;
      *(unsigned char *)(entry + 0x08) = param_6;
      *(unsigned char *)(entry + 0x09) = param_7;
      *(short *)(entry + 0x0a) = param_8;
      *(float *)(entry + 0x0c) = pos[0];
      *(float *)(entry + 0x10) = pos[1];
      *(float *)(entry + 0x14) = fVar2;
      *(int *)(entry + 0x18) = param_3;
    }

    /* Cylinder entry */
    sVar4 = *(short *)((char *)features + 2);
    if (sVar4 < 0x100) {
      *(short *)((char *)features + 2) = (short)(sVar4 + 1);
      entry = (char *)features + (int)sVar4 * 0x28 + 0x1c08;
      *(int *)(entry + 0x00) = param_4;
      *(unsigned char *)(entry + 0x08) = param_6;
      *(int *)(entry + 0x04) = param_5;
      *(unsigned char *)(entry + 0x09) = param_7;
      *(short *)(entry + 0x0a) = param_8;
      *(float *)(entry + 0x0c) = pos[0];
      *(float *)(entry + 0x10) = pos[1];
      *(float *)(entry + 0x14) = fVar2;
      *(int *)(entry + 0x18) = 0;
      *(int *)(entry + 0x1c) = 0;
      *(float *)(entry + 0x20) = param_2;
      *(int *)(entry + 0x24) = param_3;
    }
  }
}

/* 0x14aee0 — Add a cylinder feature for the line segment (param_1 = start,
 * param_2 = direction) and, if param_3 (depth) > 0 and the direction has a
 * non-zero XY component, also two prism features representing the side planes
 * of the line at depth param_3 below the surface.  The two prisms share the
 * same 4 3D vertices (start, end, end_low, start_low) but with opposite plane
 * normals derived from the 2D perpendicular of the direction. */
void collision_features_from_line(float *param_1, float *param_2, float param_3,
                                  int param_4, int param_5, int param_6,
                                  unsigned char param_7, unsigned char param_8,
                                  unsigned short param_9, void *features)
{
  float *pfVar1;
  int *puVar2;
  float fVar3;
  float fVar4;
  unsigned char uVar7;
  unsigned int uVar8;
  int iVar9;
  short sVar10;
  float pts[12]; /* 4 * 3 floats: pts[0..2]=start, pts[3..5]=end,
                    pts[6..8]=end_low, pts[9..11]=start_low */
  float
    perp[2]; /* 2D perpendicular of direction: perp[0]=-dir.y, perp[1]=dir.x */

  sVar10 = *(short *)((char *)features + 2);
  if (sVar10 < 0x100) {
    *(short *)((char *)features + 2) = (short)(sVar10 + 1);
    puVar2 = (int *)((char *)features + 0x1c08 + (int)sVar10 * 0x28);
    *puVar2 = param_5;
    puVar2[1] = param_6;
    *(unsigned char *)(puVar2 + 2) = param_7;
    *(unsigned char *)((int)puVar2 + 9) = param_8;
    *(unsigned short *)((int)puVar2 + 10) = param_9;
    pfVar1 = (float *)(puVar2 + 3);
    pfVar1[0] = param_1[0];
    pfVar1[1] = param_1[1];
    pfVar1[2] = param_1[2];
    pfVar1 = (float *)(puVar2 + 6);
    pfVar1[0] = param_2[0];
    pfVar1[1] = param_2[1];
    pfVar1[2] = param_2[2];
    puVar2[9] = param_4;
  }
  if (param_3 > 0.0f) {
    sVar10 = *(short *)((char *)features + 2);
    if (sVar10 < 0x100) {
      *(short *)((char *)features + 2) = (short)(sVar10 + 1);
      iVar9 = (int)((char *)features + 0x1c08 + (int)sVar10 * 0x28);
      *(int *)(iVar9 + 0) = param_5;
      *(unsigned char *)(iVar9 + 8) = param_7;
      *(int *)(iVar9 + 4) = param_6;
      *(unsigned short *)(iVar9 + 10) = param_9;
      *(unsigned char *)(iVar9 + 9) = param_8;
      fVar3 = param_1[2];
      fVar4 = param_1[1];
      *(float *)(iVar9 + 0xc) = param_1[0];
      *(float *)(iVar9 + 0x10) = fVar4;
      *(float *)(iVar9 + 0x14) = fVar3 - param_3;
      *(float *)(iVar9 + 0x18) = param_2[0];
      *(float *)(iVar9 + 0x1c) = param_2[1];
      *(float *)(iVar9 + 0x20) = param_2[2];
      *(int *)(iVar9 + 0x24) = param_4;
    }
    FUN_0010b600(param_2, perp);
    if (magnitude3d(perp) != 0.0f) {
      /* build 4 vertices; pts[0..2]=start, pts[3..5]=end, pts[6..8]=end_low,
       * pts[9..11]=start_low */
      pts[0] = param_1[0];
      pts[1] = param_1[1];
      pts[2] = param_1[2];
      fVar3 = perp[0] * param_1[0] + perp[1] * param_1[1];
      pts[3] = param_1[0] + param_2[0];
      pts[4] = param_2[1] + param_1[1];
      pts[9] = param_1[0];
      pts[5] = param_1[2] + param_2[2];
      pts[10] = param_1[1];
      sVar10 = *(short *)((char *)features + 4);
      pts[8] = pts[5] - param_3;
      pts[11] = param_1[2] - param_3;
      pts[6] = pts[3];
      pts[7] = pts[4];
      if (sVar10 < 0x100) {
        puVar2 = (int *)((char *)features + 0x4408 + (int)sVar10 * 0x68);
        *(short *)((char *)features + 4) = (short)(sVar10 + 1);
        *puVar2 = param_5;
        puVar2[1] = param_6;
        *(unsigned char *)((int)puVar2 + 9) = param_8;
        *(unsigned short *)((int)puVar2 + 10) = param_9;
        *(unsigned char *)(puVar2 + 2) = param_7;
        pfVar1 = (float *)(puVar2 + 3);
        puVar2[3] = *(int *)&perp[0];
        puVar2[5] = 0;
        ((float *)puVar2)[4] = perp[1];
        puVar2[6] = *(int *)&fVar3;
        puVar2[7] = param_4;
        uVar8 = FUN_00099220(pfVar1);
        *(short *)(puVar2 + 8) = (short)uVar8;
        uVar7 = FUN_00099270(pfVar1, uVar8);
        *(unsigned char *)((int)puVar2 + 0x22) = uVar7;
        sVar10 = 0;
        puVar2[9] = 4;
        iVar9 = 0;
        do {
          FUN_00061df0(
            &pts[iVar9 * 3], (uint32_t) * (unsigned short *)(puVar2 + 8),
            *(unsigned char *)((int)puVar2 + 0x22), puVar2 + iVar9 * 2 + 10);
          sVar10 = (short)(sVar10 + 1);
          iVar9 = (int)sVar10;
        } while (iVar9 < (int)puVar2[9]);
      }
      /* shuffle pts[1] ↔ pts[3] (swap end with start_low) via integer copies */
      {
        int tmp_x, tmp_y, tmp_z;
        tmp_x = *(int *)&pts[3];
        tmp_y = *(int *)&pts[4];
        tmp_z = *(int *)&pts[5];
        *(int *)&pts[3] = *(int *)&pts[9];
        sVar10 = *(short *)((char *)features + 4);
        *(int *)&pts[4] = *(int *)&pts[10];
        *(int *)&pts[5] = *(int *)&pts[11];
        *(int *)&pts[9] = tmp_x;
        *(int *)&pts[10] = tmp_y;
        *(int *)&pts[11] = tmp_z;
      }
      if (sVar10 < 0x100) {
        puVar2 = (int *)((char *)features + 0x4408 + (int)sVar10 * 0x68);
        *(short *)((char *)features + 4) = (short)(sVar10 + 1);
        *puVar2 = param_5;
        puVar2[1] = param_6;
        *(unsigned char *)(puVar2 + 2) = param_7;
        *(unsigned char *)((int)puVar2 + 9) = param_8;
        *(unsigned short *)((int)puVar2 + 10) = param_9;
        pfVar1 = (float *)(puVar2 + 3);
        puVar2[5] = 0;
        *pfVar1 = -perp[0];
        ((float *)puVar2)[4] = -perp[1];
        puVar2[7] = param_4;
        ((float *)puVar2)[6] = -fVar3;
        uVar8 = FUN_00099220(pfVar1);
        *(short *)(puVar2 + 8) = (short)uVar8;
        uVar7 = FUN_00099270(pfVar1, uVar8);
        *(unsigned char *)((int)puVar2 + 0x22) = uVar7;
        sVar10 = 0;
        puVar2[9] = 4;
        iVar9 = 0;
        do {
          FUN_00061df0(
            &pts[iVar9 * 3], (uint32_t) * (unsigned short *)(puVar2 + 8),
            *(unsigned char *)((int)puVar2 + 0x22), puVar2 + iVar9 * 2 + 10);
          sVar10 = (short)(sVar10 + 1);
          iVar9 = (int)sVar10;
        } while (iVar9 < (int)puVar2[9]);
      }
    }
  }
}

/* 0x14b220 — Add a prism collision feature to the features buffer.
 * Copies the plane, surface ID, material flags, and projected 2D points
 * into the next prism slot.  If param_4 > 0 and the plane normal z-component
 * is negative, adjusts the plane distance and projected point z-coordinates
 * by subtracting param_4 (depth correction for embedded surfaces). */
void FUN_0014b220(int point_count, void *points, float *plane, float param_4,
                  int param_5, int param_6, int param_7, int param_8,
                  int param_9, int param_10, void *features)
{
  char *prism;
  float *plane_dst;
  unsigned int uVar3;
  short sVar6;
  int iVar4;
  int table_row;
  int component;

  assert_halt((short)point_count <= 8);

  sVar6 = *(short *)((char *)features + 4);
  if (sVar6 >= 0x100)
    return;

  *(short *)((char *)features + 4) = (short)(sVar6 + 1);
  prism = (char *)features + (int)sVar6 * 0x68 + 0x4408;

  /* store header: param_6, param_9, param_7, param_8, param_10
   * (matches original MSVC instruction-scheduled store order) */
  *(int *)(prism) = param_6;
  *(unsigned char *)(prism + 9) = (unsigned char)param_9;
  *(int *)(prism + 4) = param_7;
  *(unsigned char *)(prism + 8) = (unsigned char)param_8;
  *(unsigned short *)(prism + 0xa) = (unsigned short)param_10;

  /* copy plane (4 floats) into prism+0x0C..prism+0x18 */
  plane_dst = (float *)(prism + 0x0c);
  plane_dst[0] = plane[0];
  plane_dst[1] = plane[1];
  plane_dst[2] = plane[2];
  plane_dst[3] = plane[3];

  /* store surface ID */
  *(int *)(prism + 0x1c) = param_5;

  /* compute projection basis from plane normal */
  uVar3 = FUN_00099220(plane_dst);
  *(short *)(prism + 0x20) = (short)uVar3;
  *(unsigned char *)(prism + 0x22) = FUN_00099270(plane_dst, uVar3);

  /* store point count and project each 3D point to 2D */
  sVar6 = 0;
  iVar4 = (int)(short)point_count;
  *(int *)(prism + 0x24) = iVar4;
  if (0 < iVar4) {
    iVar4 = 0;
    do {
      FUN_00061df0((char *)points + iVar4 * 0xc,
                   (uint32_t) * (unsigned short *)(prism + 0x20),
                   *(unsigned char *)(prism + 0x22), prism + 0x28 + iVar4 * 8);
      sVar6 = (short)(sVar6 + 1);
      iVar4 = (int)sVar6;
    } while (iVar4 < *(int *)(prism + 0x24));
  }

  /* depth correction: if param_4 > 0 and plane normal z < 0, adjust */
  if (param_4 > 0.0f && plane[2] < 0.0f) {
    *(float *)(prism + 0x18) =
      *(float *)(prism + 0x18) - param_4 * *(float *)(prism + 0x14);

    if (*(short *)(prism + 0x20) != 2) {
      table_row =
        (int)(unsigned char)*(prism + 0x22) + (int)*(short *)(prism + 0x20) * 2;
      sVar6 = g_projection3d_mappings[table_row][1];
      component = (int)(unsigned short)(sVar6 == 2);

      assert_halt(g_projection3d_mappings[table_row][component] == 2);

      sVar6 = 0;
      if (0 < *(int *)(prism + 0x24)) {
        iVar4 = 0;
        do {
          iVar4 = component + iVar4 * 2 + 10;
          sVar6 = (short)(sVar6 + 1);
          *(float *)(prism + iVar4 * 4) =
            *(float *)(prism + iVar4 * 4) - param_4;
          iVar4 = (int)sVar6;
        } while (iVar4 < *(int *)(prism + 0x24));
      }
    }
  }
}

/* 0x14b3d0 — Look up a prism collision element by handle, resolve its
 * tag block chain (prisms -> regions -> materials), extract material flags,
 * optionally transform the element, then add it as a collision feature.
 * Called once per prism in the collision results. */
void collision_features_from_vertex(int param_1, int param_2, int param_3,
                                    int param_4, int param_5, int param_6,
                                    void *features)
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

  collision_features_from_point((int)elem, *(float *)&param_4, param_5, param_6,
                                uVar4, *(unsigned char *)(iVar3 + 8),
                                *(unsigned char *)(iVar3 + 9),
                                *(unsigned short *)(iVar3 + 0xa), features);
}

/* 0x14b470 — Look up a cylinder collision element by handle, resolve its
 * tag block chain (edges -> regions -> vertices -> nodes), compute the
 * displacement vector between vertex endpoints, check surface normal
 * consistency via scalar triple product against a threshold, optionally
 * transform position and direction, then add a cylinder collision feature.
 * Called once per cylinder in the collision results. */
void collision_features_from_polygon(int param_1, int object_handle,
                                     int param_3, int param_4, int param_5,
                                     int param_6, void *features)
{
  unsigned int *puVar3;
  unsigned int *puVar4;
  float *pfVar5;
  float *pfVar7;
  void *edge;
  unsigned int uVar1;
  unsigned int uVar2;
  int local_14;
  unsigned char local_5;
  float local_24[3];
  float local_34[3];
  int uVar6;

  edge = tag_block_get_element((void *)(param_1 + 0x48), object_handle, 0x18);
  puVar3 = (unsigned int *)tag_block_get_element(
    (void *)(param_1 + 0x3c), *(int *)((int)edge + 0x10), 0xc);
  puVar4 = (unsigned int *)tag_block_get_element(
    (void *)(param_1 + 0x3c), *(int *)((int)edge + 0x14), 0xc);

  if (*puVar3 != *puVar4) {
    pfVar7 = (float *)tag_block_get_element((void *)(param_1 + 0x54),
                                            *(int *)edge, 0x10);
    pfVar5 = (float *)tag_block_get_element((void *)(param_1 + 0x54),
                                            *(int *)((int)edge + 0x4), 0x10);
    uVar1 = *puVar3;
    uVar2 = *puVar4;
    local_14 = (int)tag_block_get_element((void *)(param_1 + 0xc),
                                          uVar1 & 0x7fffffff, 0x10);
    uVar6 = (int)tag_block_get_element((void *)(param_1 + 0xc),
                                       uVar2 & 0x7fffffff, 0x10);

    local_5 = (*puVar3 & 0x80000000) != 0;
    local_24[0] = pfVar5[0] - pfVar7[0];
    local_24[1] = pfVar5[1] - pfVar7[1];
    local_24[2] = pfVar5[2] - pfVar7[2];

    if ((uVar1 & 0x7fffffff) != (uVar2 & 0x7fffffff)) {
      if ((int)local_5 == (int)((*puVar4 & 0x80000000) != 0)) {
        if (FUN_000993b0((float *)local_14, (float *)uVar6, local_24) <=
            -1.0e-4f)
          return;
      } else {
        if (FUN_000993b0((float *)local_14, (float *)uVar6, local_24) >=
            1.0e-4f)
          return;
      }
    }

    uVar6 = 0xffffffff;
    if (param_6 == -1) {
      uVar6 = *(int *)((int)edge + 0x10);
    }

    if (param_3 == 0) {
      pfVar5 = local_24;
    } else {
      matrix_scale_transform_vector(
        (float *)param_3, local_24,
        local_24); /* dup-args-ok: in-place transform, confirmed LEA [EBP-0x24]
                      pushed twice */
      pfVar5 = local_24;
      matrix_transform_point((float *)param_3, pfVar7, local_34);
      pfVar7 = local_34;
    }

    collision_features_from_line(
      pfVar7, pfVar5, *(float *)&param_4, param_5, param_6, uVar6,
      (char)puVar3[2], *(unsigned char *)((int)puVar3 + 9),
      *(unsigned short *)((int)puVar3 + 0xa), features);
  }
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

  material_elem =
    (int *)tag_block_get_element((void *)(param_1 + 0x3c), param_2, 0xc);
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
    FUN_0010a1c0((float *)param_3, plane,
                 plane); /* dup-args-ok: in-place transform, confirmed LEA
                            [EBP-0x14] pushed twice */
  }

  if (param_6 != -1) {
    material_index = -1;
  } else {
    material_index = param_2;
  }

  FUN_0014b220(point_count_packed, (void *)points, plane, *(float *)&param_4,
               param_5, param_6, material_index,
               (int)*(unsigned char *)((char *)material_elem + 8),
               (int)*(unsigned char *)((char *)material_elem + 9),
               (int)*(unsigned short *)((char *)material_elem + 0xa), features);
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
    collision_features_from_vertex(param_1, collision_results[0x203 + i],
                                   param_3, param_4, param_5, param_6,
                                   features);

  for (i = 0; i < collision_results[0x101]; i++)
    collision_features_from_polygon(param_1, collision_results[0x102 + i],
                                    param_3, param_4, param_5, param_6,
                                    features);

  for (i = 0; i < collision_results[0]; i++)
    FUN_0014b620(param_1, collision_results[1 + i], param_3, param_4, param_5,
                 param_6, features);
}

/* 0x14b890 — Test a sphere collision feature against a line-of-sight point.
 * Computes the displacement from the sphere center (feature+0x0C) to the test
 * point, checks if the squared distance is less than the sphere radius squared
 * (feature+0x18). If inside, normalizes the displacement to produce the outward
 * normal, computes the plane equation (normal[3] = dot(center,normal) +
 * radius), and returns the penetration depth (radius - distance). Returns 1 if
 * the point is inside the sphere, 0 otherwise. */
char collision_features_from_surface(void *feature, void *los_data,
                                     float *t_hit, float *normal)
{
  float *sphere = (float *)feature;
  float *point = (float *)los_data;
  float dx, dy, dz;
  float dist_sq, dist;
  float radius;
  float scale;

  /* displacement from sphere center to test point */
  dx = point[0] - sphere[3]; /* sphere+0x0C */
  dy = point[1] - sphere[4]; /* sphere+0x10 */
  dz = point[2] - sphere[5]; /* sphere+0x14 */

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
char collision_cylinder_test_point(void *feature, void *los_data, float *t_hit,
                                   float *normal)
{
  float *cyl = (float *)feature;
  float *point = (float *)los_data;
  float dx, dy, dz;
  float dot, axis_len_sq;
  float radius;
  float t, t_ax, t_ay, t_az;
  float mag;

  /* displacement from cylinder origin to test point */
  dx = point[0] - cyl[3]; /* cyl+0x0C */
  dy = point[1] - cyl[4]; /* cyl+0x10 */
  dz = point[2] - cyl[5]; /* cyl+0x14 */

  /* project displacement onto cylinder axis */
  dot = dz * cyl[8] + dy * cyl[7] + dx * cyl[6];

  if (dot < 0.0f)
    return 0;

  axis_len_sq = cyl[6] * cyl[6] + cyl[7] * cyl[7] + cyl[8] * cyl[8];

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
  normal[3] =
    cyl[5] * normal[2] + cyl[4] * normal[1] + cyl[3] * normal[0] + radius;

  /* penetration depth: radius - perpendicular distance */
  *t_hit = radius - mag;

  return 1;
}

/* 0x14bae0 — Test a prism collision feature against a line-of-sight point.
 * Projects the point onto the prism base plane, checks it is within the prism
 * height, then projects to 2D and performs a winding-order polygon test on
 * all edges. Returns 1 if the point is inside the prism, 0 otherwise.
 * Writes the prism plane normal and t_hit (distance to the top face). */
char collision_prism_test_point(void *feature, void *los_data, float *t_hit,
                                float *normal)
{
  char *prism_bytes = (char *)feature;
  float vi_x_mem;
  float vi_y_mem;
  float *point = (float *)los_data;
  char *height_ptr;
  float plane_dist_from_point;
  float neg_dist;
  float proj[3];
  float *vi_x_ref;
  float *vi_y_ref;
  float out_2d[2];
  float *edge_ptr;
  int edge_count;
  int counter;
  int j;

  /* signed distance from point to prism base plane:
   * dot(normal, point) - plane_dist
   * normal at offsets 0x0C/0x10/0x14, plane_dist at 0x18 */
  plane_dist_from_point =
    *(float *)(prism_bytes + 0x10) * point[1] +
    *(float *)(prism_bytes + 0x14) * point[2] +
    *(float *)(prism_bytes + 0x0c) * point[0] -
    *(float *)(prism_bytes + 0x18);

  /* point must be at or above the base plane */
  if (plane_dist_from_point < 0.0f)
    return 0;

  /* point must be below the top face (height at 0x1C) */
  if (!(plane_dist_from_point < *(float *)(prism_bytes + 0x1c)))
    return 0;

  /* project point onto the base plane: p' = point - dist * normal */
  neg_dist = -plane_dist_from_point;
  proj[0] = neg_dist * *(float *)(prism_bytes + 0x0c) + point[0];
  proj[1] = neg_dist * *(float *)(prism_bytes + 0x10) + point[1];
  height_ptr = prism_bytes + 0x1c;
  proj[2] = neg_dist * *(float *)(prism_bytes + 0x14) + point[2];

  /* project 3D point to 2D using the prism's dominant axis */
  FUN_00061df0(proj,
               *(uint16_t *)(prism_bytes + 0x20),
               *(uint8_t *)(prism_bytes + 0x22),
               out_2d);

  /* polygon winding test: cross product of each edge with the 2D point
   * must be >= 0 for all edges (CCW winding) */
  edge_count = *(int *)(prism_bytes + 0x24);

  if (edge_count > 0) {
    vi_x_ref = &vi_x_mem;
    vi_y_ref = &vi_y_mem;
    counter = 1;
    edge_ptr = (float *)(prism_bytes + 0x28);
    do {
      /* wrap index: j = (counter >= edge_count) ? 0 : counter
       * uses SETGE/DEC/AND pattern from the original */
      j = ((counter >= edge_count) - 1) & counter;
      vi_x_mem = edge_ptr[0] - out_2d[0];
      vi_y_mem = edge_ptr[1] - out_2d[1];
      if ((*vi_x_ref) *
              (*(float *)(prism_bytes + j * 8 + 0x2c) - out_2d[1]) -
          (*vi_y_ref) *
              (*(float *)(prism_bytes + j * 8 + 0x28) - out_2d[0]) < 0.0f)
        return 0;
      edge_ptr += 2;
      counter++;
    } while (counter - 1 < edge_count);
  }

  /* write hit normal (prism plane normal + 4th component = plane_dist + height) */
  *(int *)&normal[0] = *(int *)(prism_bytes + 0x0c);
  *(int *)&normal[1] = *(int *)(prism_bytes + 0x10);
  *(int *)&normal[2] = *(int *)(prism_bytes + 0x14);
  normal[3] = *(float *)(prism_bytes + 0x18) + *(float *)height_ptr;

  /* t_hit = height - signed distance (depth until top face) */
  *t_hit = *(float *)height_ptr - plane_dist_from_point;

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
        hit = collision_features_from_surface(base + 8 + i * 0x1c, los_data,
                                              &current_t, current_normal);
      else if (type == 1)
        hit = collision_cylinder_test_point(base + 0x1c08 + i * 0x28, los_data,
                                            &current_t, current_normal);
      else if (type == 2)
        hit = collision_prism_test_point(base + 0x4408 + i * 0x68, los_data,
                                         &current_t, current_normal);

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
