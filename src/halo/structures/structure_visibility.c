void debug_pvs(uint8_t param_1)
{
  *(uint8_t *)0x505702 = param_1;
  *(uint8_t *)0x505701 = param_1;
}

void structure_visibility_find_camera(void *param_1)
{
  char *scenario;
  uint32_t leaf;
  char *cluster_elem;
  char *fog_elem;

  scenario = (char *)scenario_get();
  leaf = bsp3d_find_leaf(tag_block_get_element(scenario + 0xb0, 0, 0x60), 0,
                         param_1);

  if (leaf == 0xffffffff) {
    leaf = *(uint32_t *)0x506780;
    /* Faithful to original 0x1965f0: the cached-leaf path (jl LAB_00196637)
     * jumps to the RESET block, not past it. Jumping straight to do_cluster
     * leaves visible_sky_model (0x506789) at a stale 1 while visible_sky_index
     * (0x50678a) gets a -1 cluster sky -> render_sky.c:38 asserts
     * (!visible_sky_model || scenario_get_sky(visible_sky_index)). Only the
     * 0x506780 store is skipped on this path. */
    if ((int)leaf < *(int *)(scenario + 0xe0))
      goto reset_sky;
    leaf = 0xffffffff;
  }

  *(uint32_t *)0x506780 = leaf;
reset_sky:
  *(int *)0x506784 = -1;
  *(short *)0x50678a = -1;
  *(uint8_t *)0x506789 = 0;
  if (leaf == 0xffffffff)
    return;

  /* fall-through (was label do_cluster): also reached from the cached-leaf
   * path via `goto reset_sky` above. */
  cluster_elem = (char *)tag_block_get_element(scenario + 0xe0,
                                               (int)(leaf & 0x7fffffff), 0x10);
  *(int *)0x506784 = (int)*(short *)(cluster_elem + 8);
  fog_elem =
    (char *)tag_block_get_element(scenario + 0x134, *(int *)0x506784, 0x68);
  *(short *)0x50678a = *(short *)fog_elem;
  fog_elem = (char *)FUN_0018e7d0((int)*(short *)0x50678a);
  if (fog_elem != 0 && *(int *)(fog_elem + 0xc) != -1)
    *(uint8_t *)0x506789 = 1;
}

/* FUN_001966b0: scenario visibility cluster sweep.
 *   For each rendered cluster, walk its frustum-visible portals
 *   and mark referenced bitfield entries until a cap (0x4000) is hit.
 *   param_1 = scenario pointer (tag block base at +0x134 = clusters table). */
void FUN_001966b0(int param_1)
{
  short *local_8;
  int local_10;
  int sVar6;
  char *iVar3;
  int iVar4;
  int *piVar1;
  int *piVar5;
  int sVar2;
  int iVar_bit;
  unsigned int uVar7;

  if (*(char *)0x449ef1 != '\0' && *(char *)0x32c368 != '\0') {
    profile_enter_private((void *)0x32c360);
  }
  local_10 = 0;
  if (0 < *(short *)0x5137cc) {
    do {
      if (*(short *)0x5937d0 >= 0x4000)
        break;
      local_8 = (short *)rendered_cluster_get(local_10);
      iVar3 = (char *)tag_block_get_element((char *)param_1 + 0x134,
                                            (int)*local_8, 0x68);
      if (*(unsigned char *)0x505701 == 0 && *(int *)0x506784 != -1) {
        local_8 = local_8 + 10;
      } else {
        local_8 = (short *)0x5065a4;
      }
      iVar3 = iVar3 + 0x34;
      sVar6 = 0;
      if (0 < *(int *)iVar3) {
        do {
          if (*(short *)0x5937d0 >= 0x4000)
            break;
          iVar4 = (int)tag_block_get_element(iVar3, sVar6, 0x24);
          if (render_frustum_cube_visible((render_frustum_t *)local_8,
                                      (real_rectangle3d *)iVar4, 0) != 0) {
            piVar1 = (int *)(iVar4 + 0x18);
            piVar5 = (int *)tag_block_get_element(piVar1, 0, 4);
            sVar2 = 0;
            if (0 < *piVar1) {
              do {
                iVar_bit = *piVar5 >> 5;
                uVar7 = 1u << (*piVar5 & 0x1f);
                if ((uVar7 &
                     *(unsigned int *)((char *)0x5137d0 + iVar_bit * 4)) == 0) {
                  if (*(short *)0x5937d0 >= 0x4000)
                    break;
                  *(unsigned int *)((char *)0x5137d0 + iVar_bit * 4) |= uVar7;
                  *(short *)0x5937d0 = *(short *)0x5937d0 + 1;
                }
                piVar5 = piVar5 + 1;
                sVar2 = sVar2 + 1;
              } while ((short)sVar2 < *piVar1);
            }
          }
          sVar6 = sVar6 + 1;
        } while ((short)sVar6 < *(int *)iVar3);
      }
      local_10 = local_10 + 1;
    } while ((short)local_10 < *(short *)0x5137cc);
  }
  if (*(char *)0x449ef1 != '\0' && *(char *)0x32c368 != '\0') {
    profile_exit_private((void *)0x32c360);
  }
}

/* FUN_00196850: structure visibility SURFACE sweep (sibling of 0x1966b0,
 * which does the cube/portal sweep).
 *   For every rendered cluster, walk cluster->surface_indices (a long buffer
 *   at +0x48, count at +0x44). The buffer is a sequence of runs; each run has
 *   a 3-long header {geometry_index, part_index, surface_count} followed by
 *   surface_count raw surface indices. Every not-yet-marked surface is tested
 *   against the frustum as a triangle of three 0x20-byte vertices and, if
 *   visible, its bit is set in the surface bit-vector at 0x5137d0. The marked
 *   count at 0x5937d0 (int16) caps the sweep at 0x4000.
 *   param_1 = structure_bsp: +0xf8 surfaces (6-byte: three u16 vertex
 *   indices), +0x104 geometries (0x20), +0x134 clusters (0x68). The geometry's
 *   nested part block (+0x14, 0x100-byte elements) carries the vertex base at
 *   part+0xf8.
 *   The plane context (first arg to the frustum test) is the rendered-cluster
 *   record +0x14 unless pvs debug is on or no visible cluster is cached, in
 *   which case the global default at 0x5065a4 is used -- identical selection
 *   to 0x1966b0. Ghidra's decompile drops both this block and the part
 *   element; do not trust it. */
void FUN_00196850(int param_1)
{
  short *rendered_cluster;
  void *plane_ctx;
  char *part;
  char *cluster;
  char *geometry;
  char *vertex_base;
  unsigned short *surface;
  int *surface_index_buffer;
  int cluster_i;
  int i;
  /* volatile long: the original keeps the run-end bound in its stack slot and
   * re-reads it on every inner-loop test rather than caching it in a register
   * (permuter-confirmed, +2.5pp VC71). Semantically identical -- nothing else
   * writes this local. */
  volatile long run_end;
  int surface_index;
  unsigned int bit_mask;

  if (*(char *)0x449ef1 != '\0' && *(char *)0x32c960 != '\0') {
    profile_enter_private((void *)0x32c958);
  }
  cluster_i = 0;
  if (0 < *(short *)0x5137cc) {
    do {
      rendered_cluster = (short *)rendered_cluster_get(cluster_i);
      cluster = (char *)tag_block_get_element((char *)param_1 + 0x134,
                                              (int)*rendered_cluster, 0x68);
      if (*(unsigned char *)0x505701 == 0 && *(int *)0x506784 != -1) {
        plane_ctx = (void *)(rendered_cluster + 10); /* record + 0x14 */
      } else {
        plane_ctx = (void *)0x5065a4;
      }
      surface_index_buffer = *(int **)(cluster + 0x48);
      i = 0;
      if (0 < *(int *)(cluster + 0x44)) {
        do {
          run_end = surface_index_buffer[1]; /* part index (loaded first) */
          geometry = (char *)tag_block_get_element((char *)param_1 + 0x104,
                                                   *surface_index_buffer, 0x20);
          part = (char *)tag_block_get_element(geometry + 0x14, run_end, 0x100);
          run_end = surface_index_buffer[2] + 3 + i;
          i = i + 3;
          surface_index_buffer = surface_index_buffer + 3;
          while (i < run_end && *(short *)0x5937d0 < 0x4000) {
            surface_index = *surface_index_buffer;
            surface_index_buffer = surface_index_buffer + 1;
            if (*(int *)(cluster + 0x44) < (int)((char *)surface_index_buffer -
                                                 *(char **)(cluster + 0x48)) >>
                2) {
              display_assert(
                "surface_index_buffer-(long *) "
                "cluster->surface_indices.address<=cluster->surface_indices."
                "count",
                "c:\\halo\\SOURCE\\structures\\structure_visibility.c", 0x1a0,
                1);
              system_exit(-1);
            }
            bit_mask = 1u << (surface_index & 0x1f);
            if ((*(unsigned int *)((char *)0x5137d0 +
                                   (surface_index >> 5) * 4) &
                 bit_mask) == 0) {
              surface = (unsigned short *)tag_block_get_element(
                (char *)param_1 + 0xf8, surface_index, 6);
              vertex_base = *(char **)(part + 0xf8);
              if (render_frustum_triangle_visible(
                    plane_ctx, (real_point3d *)(vertex_base + (surface[0] << 5)),
                    (real_point3d *)(vertex_base + (surface[1] << 5)),
                    (real_point3d *)(vertex_base + (surface[2] << 5))) != 0) {
                *(unsigned int *)((char *)0x5137d0 +
                                  (surface_index >> 5) * 4) |= bit_mask;
                *(short *)0x5937d0 = *(short *)0x5937d0 + 1;
              }
            }
            i = i + 1;
          }
        } while (i < *(int *)(cluster + 0x44));
      }
      cluster_i = cluster_i + 1;
    } while ((short)cluster_i < *(short *)0x5137cc);
  }
  if (*(char *)0x449ef1 != '\0' && *(char *)0x32c960 != '\0') {
    profile_exit_private((void *)0x32c958);
  }
}

/* 0x196a60 - FUN_00196a60
 *
 * Classifies an axis-aligned box `bounds` against the cull box `cull_bounds`.
 * Both are six floats {x0, x1, y0, y1, z0, z1} (min/max per axis). Returns 0
 * when the boxes are disjoint on any axis, 1 when they overlap but `bounds`
 * leaves the cull box on some side, 2 when `bounds` lies entirely inside.
 *
 * ABI: leaf, no frame; ECX = cull_bounds, EDX = bounds, result in EAX.
 * Compare senses: TEST AH,5 / JNP is "less than"; TEST AH,0x41 / JZ is
 * "greater than" (unordered takes neither exit).
 * Dormant: kb ported=false.
 */
int FUN_00196a60(float *cull_bounds, float *bounds)
{
  if (cull_bounds[1] < bounds[0] || cull_bounds[0] > bounds[1] ||
      cull_bounds[3] < bounds[2] || cull_bounds[2] > bounds[3] ||
      cull_bounds[5] < bounds[4] || cull_bounds[4] > bounds[5]) {
    return 0;
  }
  if (bounds[0] < cull_bounds[0] || bounds[1] > cull_bounds[1] ||
      bounds[2] < cull_bounds[2] || bounds[3] > cull_bounds[3] ||
      bounds[4] < cull_bounds[4] || bounds[5] > cull_bounds[5]) {
    return 1;
  }
  return 2;
}

/* 0x196b10 - classify an axis-aligned bounding box against a plane list.
 * Returns 0 when every corner is behind any one plane, 1 when at least one
 * plane splits the box, and 2 when all corners are in front of every plane. */
int FUN_00196b10(float *bounds, int plane_count, int plane_address)
{
  float copied_bounds[6];
  float *plane;
  unsigned char plane_corner_mask;
  unsigned char combined_corner_mask;
  short plane_index;
  int i;

  for (i = 0; i < 6; i++) {
    copied_bounds[i] = bounds[i];
  }

  combined_corner_mask = 0;
  for (plane_index = 0; plane_index < (short)plane_count; plane_index++) {
    plane = (float *)(plane_address + plane_index * 0x10);
    plane_corner_mask = 0;
    if (copied_bounds[0] * plane[0] + copied_bounds[2] * plane[1] +
          copied_bounds[4] * plane[2] - plane[3] <
        0.0f)
      plane_corner_mask |= 0x01;
    if (copied_bounds[1] * plane[0] + copied_bounds[2] * plane[1] +
          copied_bounds[4] * plane[2] - plane[3] <
        0.0f)
      plane_corner_mask |= 0x02;
    if (copied_bounds[0] * plane[0] + copied_bounds[3] * plane[1] +
          copied_bounds[4] * plane[2] - plane[3] <
        0.0f)
      plane_corner_mask |= 0x04;
    if (copied_bounds[1] * plane[0] + copied_bounds[3] * plane[1] +
          copied_bounds[4] * plane[2] - plane[3] <
        0.0f)
      plane_corner_mask |= 0x08;
    if (copied_bounds[0] * plane[0] + copied_bounds[2] * plane[1] +
          copied_bounds[5] * plane[2] - plane[3] <
        0.0f)
      plane_corner_mask |= 0x10;
    if (copied_bounds[1] * plane[0] + copied_bounds[2] * plane[1] +
          copied_bounds[5] * plane[2] - plane[3] <
        0.0f)
      plane_corner_mask |= 0x20;
    if (copied_bounds[0] * plane[0] + copied_bounds[3] * plane[1] +
          copied_bounds[5] * plane[2] - plane[3] <
        0.0f)
      plane_corner_mask |= 0x40;
    if (copied_bounds[1] * plane[0] + copied_bounds[3] * plane[1] +
          copied_bounds[5] * plane[2] - plane[3] <
        0.0f)
      plane_corner_mask |= 0x80;

    if (plane_corner_mask == 0xff) {
      return 0;
    }
    combined_corner_mask |= plane_corner_mask;
  }

  return combined_corner_mask != 0 ? 1 : 2;
}

/* FUN_00196c90: gather visible objects across all rendered clusters (0x196c90).
 * Walks every rendered cluster (count at 0x5137cc), iterating the caller-
 * supplied per-cluster object list via (iter_first, iter_next); for each object
 * that needs_update() accepts it fetches a bounding sphere via get_bounds(),
 * appends the handle to out_handles while under max_count and (when a cluster
 * is current, 0x506784 != -1) the sphere passes the cluster's frustum planes
 * (record + 0x14), then calls mark() on it. Returns the number appended.
 * The function-pointer params are declared void * in kb.json and cast at each
 * call site; arities/cleanups come from the call-site audit (2/1/3/1 args). */
short FUN_00196c90(int out_handles, short max_count, void *iter_first,
                   void *iter_next, void *get_bounds, void *needs_update,
                   void *mark)
{
  short *rendered_cluster;
  int object_handle;
  short count;
  int cluster_i;
  float center[3];
  int iterator;
  float radius;

  scenario_get();
  count = 0;
  cluster_i = 0;
  if (0 < *(short *)0x5137cc) {
    do {
      rendered_cluster = (short *)rendered_cluster_get(cluster_i);
      /* XOR ECX,ECX / MOV CX,[EBX]: cluster index is zero-extended. */
      object_handle = ((int (*)(void *, int))iter_first)(
        &iterator, (int)(unsigned short)*rendered_cluster);
      while (object_handle != -1) {
        /* TEST AL,AL: the predicate returns a byte. */
        if (((char (*)(int))needs_update)(object_handle) != 0) {
          ((void (*)(int, float *, float *))get_bounds)(object_handle, center,
                                                        &radius);
          /* CMP DI,[EBP+0xc] is a signed 16-bit compare; the bounds call above
           * happens before it, unconditionally. */
          if (count < max_count &&
              (*(int *)0x506784 == -1 ||
               /* TEST AX,AX: only the low 16 bits of the result are tested. */
               (short)render_frustum_sphere_visible(
                 (void *)(rendered_cluster + 10), (real_point3d *)center, radius) != 0)) {
            *(int *)(out_handles + count * 4) = object_handle;
            count = count + 1;
            ((void (*)(int))mark)(object_handle);
          }
        }
        object_handle = ((int (*)(void *))iter_next)(&iterator);
      }
      cluster_i = cluster_i + 1;
    } while ((short)cluster_i < *(short *)0x5137cc);
  }
  return count;
}

/* 0x196d60 - FUN_00196d60
 *
 * Grows the 2D rectangle `rect` {x0, x1, y0, y1} to cover every point of a
 * portal hull: int16 point count at +0, float (x, y) pairs from +4. The count
 * must be 0..0x100 (assert "valid_portal_hull(hull)").
 *
 * ABI: leaf, no frame; ESI = rect, EDI = hull, no stack arguments.
 * Per point (0x196dc0): rect[0] > x -> x; rect[1] < x -> x; rect[2] > y -> y;
 * rect[3] < y -> y (dword copies). The counter is 16-bit (INC EDX /
 * CMP DX,[EDI]) and the count is re-read every iteration.
 * Dormant: kb ported=false.
 */
void FUN_00196d60(float *rect, int16_t *hull)
{
  float *point;
  short i;

  if (rect == 0) {
    display_assert("rectangle",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x4cf, 1);
    system_exit(-1);
  }
  if (hull == 0 || hull[0] < 0 || hull[0] > 0x100) {
    display_assert("valid_portal_hull(hull)",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x4d0, 1);
    system_exit(-1);
  }

  point = (float *)(hull + 2);
  for (i = 0; i < hull[0]; i++) {
    if (rect[0] > point[0]) {
      rect[0] = point[0];
    }
    if (rect[1] < point[0]) {
      rect[1] = point[0];
    }
    if (rect[2] > point[1]) {
      rect[2] = point[1];
    }
    if (rect[3] < point[1]) {
      rect[3] = point[1];
    }
    point += 2;
  }
}

/* 0x196e10 - FUN_00196e10
 *
 * Draws a closed polyline through a point list laid out like a portal hull:
 * int16 point count at +0, float (x, y) pairs from +4. Each point is lifted to
 * (x, y, -1.0) and transformed in place by the matrix at 0x5065e8, then a
 * segment is drawn from it to the previous point via render_debug_line(1, ...).
 * The first "previous" point is the last point of the list (only built when
 * the count is non-zero).
 *
 * ABI: EDI = sound_list (kb name), EBX = env, passed through untouched as the
 * last render_debug_line argument. The stack `distance` argument is never read
 * (no [EBP+8] access in 0x196e10..0x196eaf). The counter is 16-bit
 * (INC ESI / CMP SI,[EDI]) and the count is re-read every iteration. */
void FUN_00196e10(uint16_t *sound_list, void *env, float distance)
{
  float prev[3];
  float cur[3];
  short i;

  if (sound_list[0] != 0) {
    prev[0] = *(float *)(sound_list + (short)sound_list[0] * 4 - 2);
    prev[1] = *(float *)(sound_list + (short)sound_list[0] * 4);
    prev[2] = -1.0f;
    matrix_transform_point((const real_matrix4x3 *)(0x5065e8), (real_point3d *)prev, (real_point3d *)prev);
  }
  for (i = 0; i < (short)sound_list[0]; i++) {
    cur[0] = *(float *)(sound_list + i * 4 + 2);
    cur[1] = *(float *)(sound_list + i * 4 + 4);
    cur[2] = -1.0f;
    matrix_transform_point((const real_matrix4x3 *)(0x5065e8), (real_point3d *)cur, (real_point3d *)cur);
    render_debug_line(1, (const real_point3d *)cur, (const real_point3d *)prev, env);
    prev[0] = cur[0];
    prev[1] = cur[1];
    prev[2] = cur[2];
  }
}

/* 0x197130 - gather visible clusters referenced by a BSP leaf's surfaces.
 *
 * Register ABI (prologue at 0x197130): MOV EBX,[EBP+0x2c] then MOV ESI,EAX; the
 * only register arg is leaf@<eax> (BSP node/leaf value; its sign bit is a
 * node/leaf discriminator, masked off with &0x7fffffff for the leaf index).
 * Stack args: bounds ([EBP+0x8] parent_bounds), param_2 ([EBP+0xc] per-call
 * visited-cluster bitset base), param_3 ([EBP+0x10] int* out cluster array),
 * count ([EBP+0x14] out capacity), center ([EBP+0x18] cull-sphere center,
 * null-checked only), radius ([EBP+0x1c], unused here), cull_bounds
 * ([EBP+0x20]), param_8 ([EBP+0x24]), param_9 ([EBP+0x28]), intersection
 * ([EBP+0x2c], mode: the incoming value is read into EBX and the slot is then
 * reused as the running output accumulator that is returned).
 *
 * Resolves the leaf element (scenario+0xe0, stride 0x10), validates it, derives
 * child bounds via FUN_00196eb0, and (unless intersection==2) culls against the
 * cull bounds via FUN_00196a60/FUN_00196b10 taking the min classification.  If
 * the leaf is at all visible it walks the leaf's surface run (scenario+0xec,
 * stride 8), and for each surface's cluster index sets a bit in the global
 * cluster visibility set at 0x5137d0 gated bitset and, if newly visible and not
 * already recorded in the per-call bitset, appends the cluster to the out array
 * (until count is reached).  Returns the number of clusters appended. */
int FUN_00197130(float *bounds, void *param_2, int *param_3, int count,
                 float *center, float radius, float *cull_bounds, int param_8,
                 int param_9, int intersection, int leaf /* @<eax> */)
{
  void *scenario;
  char *leaf_element;
  int accumulator;
  int cull_result;
  float local_20[6];

  (void)radius;
  accumulator = 0;
  scenario = scenario_get();
  leaf_element = (char *)tag_block_get_element((char *)scenario + 0xe0,
                                               leaf & 0x7fffffff, 0x10);

  if ((short)intersection == 0) {
    display_assert("intersection",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x2f0, true);
    system_exit(-1);
  }
  if (bounds == (float *)0) {
    display_assert("parent_bounds",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x2f1, true);
    system_exit(-1);
  }
  if (center == (float *)0) {
    display_assert("cull_sphere_center",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x2f2, true);
    system_exit(-1);
  }
  if (cull_bounds == (float *)0) {
    display_assert("cull_bounds",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x2f3, true);
    system_exit(-1);
  }
  if (*(short *)(leaf_element + 8) < 0 ||
      *(int *)((char *)scenario + 0x134) <= (int)*(short *)(leaf_element + 8)) {
    display_assert(
      "leaf->cluster_index>=0 && leaf->cluster_index<structure->clusters.count",
      "c:\\halo\\SOURCE\\structures\\structure_visibility.c", 0x2f4, true);
    system_exit(-1);
  }

  FUN_00196eb0(bounds, (unsigned char *)leaf_element, local_20);

  cull_result = (short)intersection;
  if ((short)intersection != 2) {
    int a = FUN_00196a60(cull_bounds, local_20);
    int b = FUN_00196b10(local_20, param_8, param_9);
    cull_result = a;
    if ((short)b < (short)a) {
      cull_result = b;
    }
  }

  if ((short)cull_result != 0) {
    int i;
    int first = *(int *)(leaf_element + 0xc);
    int end = (int)*(short *)(leaf_element + 0xa) + first;
    char *surface_block = (char *)scenario + 0xec;
    for (i = first; i < end; i++) {
      int *elem = (int *)tag_block_get_element(surface_block, i, 8);
      int cluster = *elem;
      int word_off = (cluster >> 5) * 4;
      unsigned int mask = 1u << (cluster & 0x1f);
      if ((mask & *(unsigned int *)((char *)0x5137d0 + word_off)) != 0) {
        unsigned int *per_call = (unsigned int *)((char *)param_2 + word_off);
        if ((mask & *per_call) == 0) {
          if ((short)count <= (short)accumulator) {
            break;
          }
          *per_call |= mask;
          param_3[(short)accumulator] = cluster;
          accumulator = accumulator + 1;
        }
      }
      end = (int)*(short *)(leaf_element + 0xa) + *(int *)(leaf_element + 0xc);
    }
  }

  return accumulator;
}

/* 0x197310 - project a structure surface's vertices to screen and clip.
 *
 * Register ABI (prologue at 0x197310): MOV EBX,EAX / MOV EDI,ECX / MOV ESI,EDX
 *   verts@<eax>  -> float* source vertex array (stride 3 floats)
 *   plane@<ecx>  -> float* plane {nx,ny,nz,d}
 *   ref@<edx>    -> float* reference point; byte at ref+0x24 flips winding
 * Stack args: arg1 (matrix container; transform matrix at arg1+0x10),
 *   count (int16_t vertex count), sign (winding direction, +/-1),
 *   out (short* result: [0]=clipped vertex count, then {float x,float y} pairs
 *   at byte offsets +4,+8,... i.e. 8-byte stride starting at out+4).
 *
 * Computes signed distance of ref from plane, scaled by sign; if the magnitude
 * is below the 0x2674e8 epsilon the surface is coplanar (return 2); if the
 * signed side is <= 0 the surface faces away (return 1).  Otherwise transforms
 * each vertex through the matrix into a 3-float scratch buffer, clips the
 * polygon against 0x2b35c4, perspective-divides each surviving vertex
 * (ooz = k / z, k at 0x255e94) walking forward (sign==1) or backward, and
 * writes the 2D coords to out.  Returns 1 if fewer than 3 vertices survive,
 * else 0.  0x2533c0 == 0.0f threshold. */
short FUN_00197310(void *verts, void *plane, void *ref, void *arg1,
                   int16_t count, int sign, short *out)
{
  float *v = (float *)verts;
  float *p = (float *)plane;
  float *r = (float *)ref;
  float buf[256][3];
  float side;
  float ooz;
  int orig_sign;
  int j;
  short idx;
  short end;
  short oidx;

  scenario_get();
  *out = 0;
  orig_sign = (short)sign;
  side = (r[2] * p[2] + r[1] * p[1] + r[0] * p[0] - p[3]) * (float)orig_sign;
  if (*((char *)ref + 0x24) != '\0') {
    sign = -sign;
  }
  if (fabs(side) < *(double *)0x002674e8) {
    return 2;
  }
  if (side <= *(float *)0x002533c0) {
    return 1;
  }

  if (count > 0) {
    float *mtx = (float *)((char *)arg1 + 0x10);
    for (j = 0; j < count; j++) {
      matrix_transform_point((const real_matrix4x3 *)mtx, (real_point3d *)(v + j * 3), (real_point3d *)&buf[j][0]);
    }
  }

  *out = convex_polygon3d_clip_to_plane(count, &buf[0][0], (float *)0x002b35c4,
                                        0x100, &buf[0][0], (uint32_t *)0,
                                        0.0001f, (void *)0x1);
  if (*out == -1) {
    display_assert("result->vertex_count!=NONE",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x485, true);
    system_exit(-1);
  }

  if (sign == 1) {
    idx = 0;
    end = *out;
  } else {
    idx = (short)(*out - 1);
    end = -1;
  }
  oidx = 0;
  if (idx != end) {
    do {
      int e = (int)idx;
      ooz = *(float *)0x00255e94 / buf[e][2];
      if (ooz <= *(float *)0x002533c0) {
        display_assert("ooz>0.f",
                       "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                       0x497, true);
        system_exit(-1);
      }
      *(float *)(out + oidx * 4 + 2) = ooz * buf[e][0];
      *(float *)(out + oidx * 4 + 4) = ooz * buf[e][1];
      idx = (short)(idx + sign);
      oidx = (short)(oidx + 1);
    } while (idx != end);
  }

  return (short)(*out < 3);
}

/* FUN_001978a0: recursive bsp3d structure-visibility traversal.
 *   Original: c:\halo\SOURCE\structures\structure_visibility.c line ~0x2ab.
 *
 * Walks the structure BSP3D node tree from `node_index`. At each node it
 * subdivides the incoming (parent) bounds across the node's fraction record
 * (FUN_00196eb0 -> child bounds in `bounds`), tests those bounds against the
 * cull bounds (FUN_00196a60) and the frustum planes (FUN_00196b10) unless the
 * caller already reported "fully inside" ((short)intersection == 2), then for
 * each of the node's two child slots that survive the splitting-plane sphere
 * test recurses into subtrees (child >= 0) or dispatches leaves (child < 0,
 * child != -1) via FUN_00197130. Returns the accumulated 16-bit count in AX.
 *
 * 11 cdecl stack args (recursive tail cleans ADD ESP,0x2c = 44 = 11*4).
 * ESI is the running accumulator, EDI the propagated intersection mode.
 *
 * Verified against disasm 0x1978a0-0x197afa. Notes on decompiler traps fixed
 * here:
 *   - The two side flags are independent stack bytes (side[0]/side[1]),
 *     defaulted to 1 and cleared by the plane test; Ghidra modelled them as a
 *     CONCAT into param_2. param_2 is really a float* (parent bounds).
 *   - The value passed to children in slot 7 is the UNCHANGED radius (held in
 *     EBX across the FPU block), not fVar1; the decompiler mis-aliased EBX.
 *   - FUN_00196eb0 is a 3-arg call (bounds, fractions, out); its 3rd arg is the
 *     &local_24 push that tag_block_get_element left on the stack (this is the
 *     ADD ESP,0xc "anomaly"). FUN_00196b10 takes &bounds in @eax. */
unsigned short FUN_001978a0(int node_index, float *parent_bounds, void *param_3,
                            int *param_4, int param_5, float *center,
                            float radius, float *cull_bounds, int param_9,
                            int param_10, int intersection)
{
  int accum;
  char *scenario;
  char *nodes_block;
  unsigned char *fractions;
  int mode;
  int t;
  int *node;
  float *plane;
  float dist;
  unsigned char side[2];
  int count;
  int *child_ptr;
  unsigned char *side_ptr;
  int child;
  float bounds[6];

  accum = 0;
  scenario = (char *)scenario_get();
  nodes_block = (char *)tag_block_get_element(scenario + 0xb0, 0, 0x60);

  if (parent_bounds == 0) {
    display_assert("parent_bounds",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x2ab, true);
    system_exit(-1);
  }
  if (center == 0) {
    display_assert("cull_sphere_center",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x2ac, true);
    system_exit(-1);
  }
  if (cull_bounds == 0) {
    display_assert("cull_bounds",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x2ad, true);
    system_exit(-1);
  }
  /* the original loads intersection into EDI here and keeps that register as
   * the running mode for the rest of the function */
  mode = intersection;
  if ((short)mode == 0) {
    display_assert("intersection",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x2ae, true);
    system_exit(-1);
  }

  fractions =
    (unsigned char *)tag_block_get_element(scenario + 0xbc, node_index, 6);
  FUN_00196eb0(parent_bounds, fractions, bounds);

  if ((short)mode != 2) {
    mode = FUN_00196a60(cull_bounds, bounds);
    if ((short)mode == 0)
      return (unsigned short)accum;
    t = FUN_00196b10(bounds, param_9, param_10);
    if ((short)t == 2)
      param_9 = 0;
    if ((short)mode > (short)t)
      mode = t;
  }

  if ((short)mode != 0) {
    node = (int *)tag_block_get_element(nodes_block, node_index, 0xc);
    plane = (float *)tag_block_get_element(nodes_block + 0xc, *node, 0x10);
    dist = plane[2] * center[2] + plane[1] * center[1] + center[0] * plane[0] -
           plane[3];

    side[0] = 1;
    if (!(dist < radius))
      side[0] = 0;
    side[1] = 1;
    if (!(dist > -radius))
      side[1] = 0;

    child_ptr = node + 1;
    side_ptr = side;
    count = 2;
    do {
      if (*side_ptr != 0) {
        child = *child_ptr;
        /* recurse arm first: original falls through into the self-call and
         * sinks the leaf arm past the join (JS to it) */
        if (child >= 0) {
          accum += FUN_001978a0(child, bounds, param_3, param_4 + (short)accum,
                                param_5 - accum, center, radius, cull_bounds,
                                param_9, param_10, mode);
        } else if (child != -1) {
          /* 0x19713c: callee reads the leaf ref from EAX (strips the sign
           * bit itself via AND 0x7fffffff) — implicit @<eax> arg. */
          accum += FUN_00197130(bounds, param_3, param_4 + (short)accum,
                                param_5 - accum, center, radius, cull_bounds,
                                param_9, param_10, mode, child);
        }
      }
      child_ptr++;
      side_ptr++;
    } while (--count != 0);
  }

  return (unsigned short)accum;
}

/* Recursively flood rendered clusters across BSP portal connections (0x197b00).
 * DFS over the cluster portal graph. Sets a per-cluster "visited" bit (dynamic
 * bit-vector at *0x4d8ed8) on entry and clears it on exit (backtrack). The
 * first time a cluster is reached (permanent-mark set at 0x50678c) it allocates
 * a rendered_cluster record: record[0]=cluster_index plus a 16-byte block
 * copied from *(void**)0x31fc68; a bounded counter at 0x5137cc (<0x80) indexes
 * them. For each portal it looks up the connection (scenario+0x154, 0x40-byte
 * record), picks the neighbor cluster (the other side), and, if the neighbor is
 * visible and sound-carrying, recurses -- either with the same sound list, or a
 * freshly built portal-clipped list (FUN_00108060). The assert file string
 * proves this function lives in structure_visibility.c.
 *
 * FUN_00197570 (@edx records / @esi count / float threshold) and
 * FUN_00196e10 (@edi sound_list / @ebx env / float dist) take register args --
 * verified against callee disassembly (0x197570 reads SI+EDX; 0x196e10 reads
 * [EDI] and pushes EBX without saving them). */
void FUN_00197b00(int16_t cluster_index, uint16_t *sound_list)
{
  uint16_t built_list[1026]; /* local_102c([0]=count) + local_1028(elements @
                                &[2]) -- MUST stay contiguous */
  uint16_t portal_hull[1026]; /* original: ONE hull buffer at EBP-0x824
                                 ([0]=count word, float pairs @ &[2]).
                                 FUN_001974f0 -> FUN_00197310 writes up to
                                 0x100 points (0x804 bytes) through it; the
                                 prior split into `int local_828` + work_b
                                 smashed the clang frame (map-load crash,
                                 read of 0xc0170662 at FUN_00197b00+0x2a9). */
  void *bsp;
  int cluster_index_i;
  char *clusters_block;
  char *connections_block;
  uint16_t *cluster_elem;
  uint32_t *sound_bits;
  uint32_t bit_mask;
  int bit_offset;
  int16_t *rec;
  int i;
  void *sound_env_out;

  bsp = scenario_get();
  cluster_index_i = (int)cluster_index;
  cluster_elem = (uint16_t *)tag_block_get_element((char *)bsp + 0x134,
                                                   cluster_index_i, 0x68);
  sound_bits = structure_bsp_get_cluster_sound_data(bsp, *(int16_t *)0x506784);

  if (sound_list == 0 || (int16_t)*sound_list < 0 ||
      (int16_t)*sound_list > 0x100) {
    display_assert("valid_portal_hull(visible_region)",
                   "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                   0x3ee, 1);
    system_exit(-1);
  }

  bit_mask = 1u << (cluster_index_i & 0x1f);
  bit_offset = (cluster_index_i >> 5) * 4;
  *(uint32_t *)(bit_offset + *(int *)0x4d8ed8) |= bit_mask;

  if ((*(uint32_t *)(bit_offset + 0x50678c) & bit_mask) == 0) {
    char *src;
    uint16_t rc_index;

    if (*(int16_t *)0x5137cc >= 0x80) {
      display_assert("raise MAXIMUM_RENDERED_CLUSTERS",
                     "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                     0x3f5, 1);
      system_exit(-1);
    }
    if ((int16_t)cluster_index < 0 || (int16_t)cluster_index >= 0x200) {
      display_assert(
        "cluster_index>=0 && cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
        "c:\\halo\\SOURCE\\structures\\structure_visibility.c", 0x3f8, 1);
      system_exit(-1);
    }

    rc_index = *(uint16_t *)0x5137cc;
    *(uint16_t *)(0x4d8edc + cluster_index_i * 2) = rc_index;
    *(uint16_t *)0x5137cc = rc_index + 1;
    rec = (int16_t *)rendered_cluster_get(
      *(uint16_t *)(0x4d8edc + cluster_index_i * 2));
    rec[0] = cluster_index;
    src = *(char **)0x31fc68;
    *(uint32_t *)((char *)rec + 4) = *(uint32_t *)(src + 0);
    *(uint32_t *)((char *)rec + 8) = *(uint32_t *)(src + 4);
    *(uint32_t *)((char *)rec + 12) = *(uint32_t *)(src + 8);
    *(uint32_t *)((char *)rec + 16) = *(uint32_t *)(src + 12);
  } else {
    rec = (int16_t *)rendered_cluster_get(
      *(uint16_t *)(0x4d8edc + cluster_index_i * 2));
    if (rec[0] != cluster_index) {
      display_assert("rendered_cluster->cluster_index==cluster_index",
                     "c:\\halo\\SOURCE\\structures\\structure_visibility.c",
                     0x403, 1);
      system_exit(-1);
    }
  }

  *(uint32_t *)(bit_offset + 0x50678c) |= bit_mask;
  /* 0x197ca9: original sets EDI=[ebp+0xc] (visible_region hull param) and
   * ESI=rec+4 (rendered-cluster bounds rect) before CALL. Accumulates the
   * hull's 2D points into the rect (min/max union). */
  FUN_00196d60((float *)((char *)rec + 4), (int16_t *)sound_list);

  if (*(char *)0x505702 != 0) {
    FUN_00196e10(sound_list, *(void **)0x2ee6d0, 0.05f);
  } else if (ai_debug_highlight_cluster(cluster_index, &sound_env_out) != 0) {
    FUN_00196e10(sound_list, sound_env_out, 0.05f);
  }

  clusters_block = (char *)bsp + 0x134;
  connections_block = (char *)bsp + 0x154;
  for (i = 0; i < *(int *)((char *)cluster_elem + 0x5c); i++) {
    int16_t conn_index;
    int16_t *conn;
    int pick;
    int16_t neighbor;
    uint32_t nmask;
    int noff;

    conn_index =
      *(int16_t *)tag_block_get_element((char *)cluster_elem + 0x5c, i, 2);
    conn = (int16_t *)tag_block_get_element(connections_block, (int)conn_index,
                                            0x40);
    pick = (conn[0] == cluster_index) ? 1 : 0;
    neighbor = conn[pick];
    if (neighbor < 0 || (int)neighbor >= *(int *)clusters_block)
      continue;

    nmask = 1u << ((int)neighbor & 0x1f);
    noff = ((int)neighbor >> 5) * 4;
    if ((*(uint32_t *)((char *)(*(int *)0x4d8ed8) + noff) & nmask) != 0)
      continue;
    if ((*(uint32_t *)((char *)sound_bits + noff) & nmask) == 0)
      continue;

    {
      int16_t r = FUN_001974f0(conn_index, (char)pick, (int *)portal_hull);

      if (r == 2) {
        FUN_00197b00(neighbor, sound_list);
      } else if (r == 0) {
        if (*(char *)0x506789 == 0) {
          char c =
            FUN_00197570(*(float **)((char *)conn + 0x38),
                         *(int16_t *)((char *)conn + 0x34), *(float *)0x506590);
          if (c == 0)
            continue;
        }
        /* 0x197dd6: arg3 is the dword loaded from the hull base (count word),
         * arg4 the hull points at base+4 — both from the ONE buffer 1974f0
         * filled. */
        built_list[0] = (uint16_t)FUN_00108060(
          *sound_list, sound_list + 2, *(int *)portal_hull, portal_hull + 2,
          0x100, &built_list[2], 0.0001f);
        if ((int16_t)built_list[0] > 0) {
          FUN_00197b00(neighbor, built_list);
        } else if (built_list[0] == 0xffff) {
          error(2, "portal intersection failed.");
          FUN_00197b00(neighbor, sound_list);
        }
      }
    }
  }

  *(uint32_t *)(bit_offset + *(int *)0x4d8ed8) &= ~bit_mask;
}
