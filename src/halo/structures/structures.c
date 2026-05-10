/* FUN_00099220 (0x99220)
 *
 * Determine the dominant axis of a plane normal.  Returns the index
 * (0=x, 1=y, 2=z) of the component with the largest absolute value.
 */
uint32_t FUN_00099220(float *plane)
{
  float ax = plane[0] < 0.0f ? -plane[0] : plane[0];
  float ay = plane[1] < 0.0f ? -plane[1] : plane[1];
  float az = plane[2] < 0.0f ? -plane[2] : plane[2];

  if (ay <= az && ax <= az)
    return 2;
  if (ay < ax)
    return 0;
  return 1;
}

/* FUN_00099270 (0x99270)
 *
 * Return 1 if the plane normal component at the given projection axis
 * is positive, 0 otherwise.
 */
uint8_t FUN_00099270(float *plane, uint32_t basis)
{
  assert_halt((int16_t)basis >= 0 && (int16_t)basis <= 2);
  if (plane[basis] > 0.0f)
    return 1;
  return 0;
}

/* FUN_00106130 (0x106130)
 *
 * Test whether a query point lies within a given radius of a 2D convex
 * polygon.  Points are 2D (x, y pairs, stride 8 bytes).  Uses the
 * cross-product sign to check sidedness; if the point is outside any
 * edge beyond the radius, returns false.
 */
bool FUN_00106130(uint16_t point_count, void *points, void *query_point,
                  float radius)
{
  float *pts = (float *)points;
  float *qp = (float *)query_point;
  int16_t i;
  float radius_sq = radius * radius;

  if ((int16_t)point_count <= 0)
    return true;

  for (i = 0; i < (int16_t)point_count; i++) {
    int idx = (int)i;
    int next = (idx + 1 < (int)(int16_t)point_count) ? idx + 1 : 0;
    float ex, ey, edge_len_sq, dx, dy, cross;

    ex = pts[next * 2] - pts[idx * 2];
    ey = pts[next * 2 + 1] - pts[idx * 2 + 1];
    edge_len_sq = ex * ex + ey * ey;

    if (edge_len_sq == 0.0f)
      continue;

    dx = qp[0] - pts[idx * 2];
    dy = qp[1] - pts[idx * 2 + 1];
    cross = dx * ey - dy * ex;

    if (cross <= 0.0f)
      continue;

    if (cross * cross < edge_len_sq * radius_sq)
      continue;

    return false;
  }
  return true;
}

/* FUN_00106200 (0x106200)
 *
 * 2D point-in-polygon winding test with epsilon tolerance.
 * Tests whether a query point lies inside a 2D polygon by checking the
 * cross-product of each edge against the query point.  Points are stored
 * as float[2] pairs (x, y).  For every edge (vert[i] -> vert[next]),
 * computes:  cross = (point.y - vert_i.y) * (next.x - vert_i.x)
 *                  - (point.x - vert_i.x) * (next.y - vert_i.y)
 * If cross < -epsilon for any edge, the point is outside and returns false.
 * The wrap-around index uses: next = (i+1 >= count) ? 0 : i+1.
 */
bool FUN_00106200(int16_t count, void *points, float *query_point, float epsilon)
{
  int16_t i;
  float neg_epsilon;
  float *qp;
  float *pts;

  i = 0;
  if (count <= 0)
    return true;

  neg_epsilon = -epsilon;
  qp = (float *)query_point;
  pts = (float *)points;

  do {
    int idx = (int)i;
    int next = (idx + 1 >= (int)count) ? 0 : idx + 1;
    float ex, ey, dx, dy;

    ex = pts[next * 2] - pts[idx * 2];
    ey = pts[next * 2 + 1] - pts[idx * 2 + 1];
    dx = qp[0] - pts[idx * 2];
    dy = qp[1] - pts[idx * 2 + 1];

    if (dy * ex - dx * ey < neg_epsilon)
      return false;

    i++;
  } while (i < count);

  return true;
}

/* FUN_0018e420 (0x18e420)
 *
 * Returns the global BSP3D pointer (DAT_005064d8). Asserts with a halt if
 * the pointer has not been initialized (i.e. is NULL). Called by BSP
 * traversal and portal-intersection code to obtain the current structure
 * BSP3D tag data.
 *
 * Confirmed: no parameters (plain MOV EAX,[global]; TEST; RET).
 * Confirmed: assert string "global_bsp3d", file scenario.c, line 0xd5.
 */
void *FUN_0018e420(void)
{
  if (*(void **)0x5064d8 == NULL) {
    display_assert("global_bsp3d", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xd5, true);
    system_exit(-1);
  }
  return *(void **)0x5064d8;
}

/* Remove a value from a reference list linked through a datum array (0x1913c0).
 * Walks the list starting at *head, finds the datum whose +4 field matches
 * value, calls datum_delete to free it, and unlinks it. */
void reference_list_remove(data_t *data, int *head, int value)
{
  int *current_ptr = head;

  while (*current_ptr != -1) {
    char *datum = (char *)datum_get(data, *current_ptr);
    if (*(int *)(datum + 4) == value) {
      datum_delete(data, *current_ptr);
      *current_ptr = *(int *)(datum + 8);
      return;
    }
    current_ptr = (int *)(datum + 8);
  }

  display_assert(
    csprintf((char *)0x5ab100,
             "attempt to remove invalid element %ld from reference list",
             value),
    "..\\objects\\reference_lists.h", 0x6d, 1);
  system_exit(-1);
}

int cluster_partition_iter_next(void *partition, int *state)
{
  if (*state != -1) {
    char *cluster_reference =
      datum_get(*(void **)((char *)partition + 4), *state);
    *state = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

/* Add an object to a cluster partition (0x1917a0).
 * Finds all clusters overlapping position+radius via structure_find_in_cluster,
 * then for each cluster: allocates a per-object cluster reference
 * (partition[2]) linking into *first_cluster_ref, and a per-cluster object
 * reference (partition[1]) linking into the cluster head array (partition[0]).
 */
void cluster_partition_add_object(void *partition, int object_handle,
                                  void *first_cluster_ref, void *position,
                                  uint32_t radius_fp, void *location)
{
  int **part = (int **)partition;
  int *first_ref = (int *)first_cluster_ref;
  short *pos = (short *)position;
  char *loc = (char *)location;
  short local_clusters[64];
  uint16_t cluster_bsp_index;
  union {
    uint32_t u;
    float f;
  } rad;
  int16_t cluster_count;

  assert_halt(partition);
  assert_halt(first_cluster_ref);
  assert_halt(*first_ref == -1);
  assert_halt(position);
  assert_halt(location);

  cluster_bsp_index = *(uint16_t *)(loc + 4);
  rad.u = radius_fp;

  cluster_count = structure_find_in_cluster(
    cluster_bsp_index, (float *)pos, rad.f, 0x40, local_clusters);

  if (cluster_count > 0x40) {
    error(2, "an object or light spanned %d clusters.", (int)cluster_count);
    cluster_count = 0x40;
  }

  {
    int i;
    short *cluster_ptr = local_clusters;
    for (i = 0; i < (int)(uint16_t)cluster_count; i++, cluster_ptr++) {
      short cluster_index = *cluster_ptr;

      {
        data_t *obj_ref_data = (data_t *)part[2];
        int obj_ref_handle = data_new_at_index(obj_ref_data);
        if (obj_ref_handle == -1) {
          error(2, "WARNING: maximum %ss per map (%d) exceeded.", obj_ref_data,
                (int)*(short *)((char *)obj_ref_data + 0x20));
        } else {
          int *obj_ref = (int *)datum_get(obj_ref_data, obj_ref_handle);
          obj_ref[1] = (int)cluster_index;
          obj_ref[2] = *first_ref;
          *first_ref = obj_ref_handle;
        }
      }

      if (cluster_index < 0 ||
          (int)cluster_index >= *(int *)((char *)scenario_get() + 0x134)) {
        display_assert(
          "cluster_index>=0 && "
          "cluster_index<global_structure_bsp_get()->clusters.count",
          "c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0xd5, true);
        system_exit(-1);
      }

      {
        int *cluster_head = &part[0][(int)cluster_index];
        data_t *cluster_ref_data = (data_t *)part[1];
        int cluster_ref_handle = data_new_at_index(cluster_ref_data);
        if (cluster_ref_handle == -1) {
          error(2, "WARNING: maximum %ss per map (%d) exceeded.",
                cluster_ref_data,
                (int)*(short *)((char *)cluster_ref_data + 0x20));
        } else {
          int *cluster_ref =
            (int *)datum_get(cluster_ref_data, cluster_ref_handle);
          cluster_ref[1] = object_handle;
          cluster_ref[2] = *cluster_head;
          *cluster_head = cluster_ref_handle;
        }
      }
    }
  }
}

/* Remove an object from a cluster partition (0x1919a0).
 * Walks the per-object cluster reference chain (*first_cluster_ref),
 * and for each entry: reads the cluster index, removes the matching
 * per-cluster object reference via reference_list_remove, frees
 * the per-object datum, then follows the next link. Clears
 * *first_cluster_ref to -1 when done. */
void cluster_partition_remove_object(void *partition, int object_handle,
                                     void *first_cluster_ref)
{
  int **part = (int **)partition;
  int *first_ref = (int *)first_cluster_ref;
  int cursor = *first_ref;

  while (cursor != -1) {
    data_t *obj_ref_data = (data_t *)part[2];
    int *obj_ref = (int *)datum_get(obj_ref_data, cursor);
    short cluster_index = *(short *)((char *)obj_ref + 4);

    datum_delete(obj_ref_data, cursor);

    if (cluster_index < 0 ||
        (int)cluster_index >= *(int *)((char *)scenario_get() + 0x134)) {
      display_assert("cluster_index>=0 && "
                     "cluster_index<global_structure_bsp_get()->clusters.count",
                     "c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0xd5,
                     true);
      system_exit(-1);
    }

    {
      int *cluster_head = &part[0][(int)cluster_index];
      reference_list_remove((data_t *)part[1], cluster_head, object_handle);
    }

    cursor = obj_ref[2];
  }

  *first_ref = -1;
}

int cluster_partition_iter_first(void *partition, int *state,
                                 int16_t cluster_idx)
{
  if (cluster_idx < 0 ||
      cluster_idx >= *(int *)((char *)scenario_get() + 0x134)) {
    display_assert("cluster_index>=0 && "
                   "cluster_index<global_structure_bsp_get()->clusters.count",
                   "c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0xd5,
                   true);
    system_exit(-1);
  }

  *state = *(int *)(*(int *)partition + cluster_idx * 4);
  if (*state != -1) {
    char *cluster_reference =
      datum_get(*(void **)((char *)partition + 4), *state);
    *state = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

void structures_initialize(void)
{
  structure_detail_objects_initialize();
  structure_runtime_decals_initialize();
}

void structures_initialize_for_new_map(void)
{
  structure_detail_objects_initialize_for_new_map();
  structure_runtime_decals_initialize_for_new_map();
}

void structures_dispose_from_old_map(void)
{
}

void structures_dispose(void)
{
}

/* structures_cluster_marker_begin (0x198400)
 *
 * Asserts that the cluster marker is not already initialized,
 * increments the cluster-marker reference counter, and sets the
 * initialized flag.
 *
 * Confirmed: TEST AL,AL on byte ptr [0x4d92e1].
 * Confirmed: INC dword ptr [0x4d92e4].
 * Confirmed: MOV byte ptr [0x4d92e1], 1.
 */
void structures_cluster_marker_begin(void)
{
  if (*(uint8_t *)0x4d92e1 != 0) {
    display_assert("!structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x103, true);
    system_exit(-1);
  }
  *(uint32_t *)0x4d92e4 += 1;
  *(uint8_t *)0x4d92e1 = 1;
}

bool structure_cluster_unmarked(int16_t cluster_index)
{
  if (*(uint8_t *)0x4d92e1 == 0) {
    display_assert("structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x10e, true);
    system_exit(-1);
  }

  if (cluster_index < 0 || cluster_index > 0x1ff) {
    display_assert("cluster_index>=0 && "
                   "cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x10f, true);
    system_exit(-1);
  }

  return ((int *)0x4d92e8)[cluster_index] != *(int *)0x4d92e4;
}

int structure_cluster_mark(int16_t cluster_index)
{
  if (*(uint8_t *)0x4d92e1 == 0) {
    display_assert("structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x11e, true);
    system_exit(-1);
  }

  if (cluster_index < 0 || cluster_index > 0x1ff) {
    display_assert("cluster_index>=0 && "
                   "cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x11f, true);
    system_exit(-1);
  }

  if (((int *)0x4d92e8)[cluster_index] != *(int *)0x4d92e4) {
    ((int *)0x4d92e8)[cluster_index] = *(int *)0x4d92e4;
    return 1;
  }

  return 0;
}

/* structures_cluster_marker_end (0x198540)
 *
 * Asserts that the cluster-marker session is currently active (initialized),
 * then clears the initialized flag, ending the session begun by
 * structures_cluster_marker_begin.
 *
 * Confirmed: TEST AL,AL on byte ptr [0x4d92e1] at 0x198545.
 * Confirmed: assert string "structure_globals.cluster_marker_initialized",
 *   __FILE__ "c:\halo\SOURCE\structures\structures.c", line 0x130 (304).
 * Confirmed: MOV byte ptr [0x4d92e1], 0 at 0x198569.
 */
void structures_cluster_marker_end(void)
{
  if (*(uint8_t *)0x4d92e1 == 0) {
    display_assert("structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x130, true);
    system_exit(-1);
  }
  *(uint8_t *)0x4d92e1 = 0;
}

bool structure_get_planar_fog(void *scenario, int16_t portal_index, float *position,
                  float radius)
{
  uint8_t projected_vertices[1024];
  uint8_t projected_center[8];
  float projected_hit[3];
  char *portal =
    tag_block_get_element((char *)scenario + 0x154, (int)portal_index, 0x40);
  char *structure_bsp = tag_block_get_element((char *)scenario + 0xb0, 0, 0x60);
  float *portal_plane = tag_block_get_element((int *)(structure_bsp + 0xc),
                                              *(int *)(portal + 4), 0x10);
  float plane_distance = position[0] * portal_plane[0] +
                         position[1] * portal_plane[1] +
                         position[2] * portal_plane[2] - portal_plane[3];

  if (fabsf(plane_distance) < radius) {
    float dx = *(float *)(portal + 8) - position[0];
    float dy = *(float *)(portal + 0xc) - position[1];
    float dz = *(float *)(portal + 0x10) - position[2];
    float expanded_radius = radius + *(float *)(portal + 0x14);

    if (dx * dx + dy * dy + dz * dz < expanded_radius * expanded_radius) {
      int portal_plane_index = *(int *)(portal + 4);
      char *bsp3d = FUN_0018e420();
      uint32_t plane_basis;
      uint8_t plane_axis;
      int *portal_vertices = (int *)(portal + 0x34);
      int16_t vertex = 0;

      portal_plane =
        tag_block_get_element((int *)(bsp3d + 0xc), portal_plane_index, 0x10);
      plane_basis = FUN_00099220(portal_plane);
      plane_axis = FUN_00099270(portal_plane, plane_basis);

      projected_hit[0] = -plane_distance * portal_plane[0] + position[0];
      projected_hit[1] = -plane_distance * portal_plane[1] + position[1];
      projected_hit[2] = -plane_distance * portal_plane[2] + position[2];
      FUN_00061df0(projected_hit, plane_basis, plane_axis, projected_center);

      if (*portal_vertices > 0) {
        do {
          FUN_00061df0(tag_block_get_element(portal_vertices, (int)vertex, 0xc),
                       plane_basis, plane_axis,
                       projected_vertices + (int)vertex * 8);
          vertex += 1;
        } while ((int)vertex < *portal_vertices);
      }

      if (FUN_00106130(
            (uint16_t)*portal_vertices, projected_vertices, projected_center,
            sqrtf(radius * radius - plane_distance * plane_distance))) {
        return true;
      }
    }
  }

  return false;
}

int16_t FUN_001989b0(uint16_t cluster_count, float *position, float radius,
                     int max_count, int16_t *out_indices)
{
  void *scenario = scenario_get();
  int16_t current_cluster = (int16_t)cluster_count;
  char *cluster =
    tag_block_get_element((char *)scenario + 0x134, (int)current_cluster, 0x68);
  int remaining_count = max_count - 1;
  int visited_count = 1;

  if ((int16_t)max_count > 0) {
    *out_indices = current_cluster;
    out_indices += 1;
  }

  structure_cluster_mark(cluster_count);

  if (*(int *)(cluster + 0x5c) > 0) {
    int16_t portal_iter = 0;

    do {
      int16_t *portal_index_ptr =
        tag_block_get_element((int *)(cluster + 0x5c), portal_iter, 2);
      int16_t portal_index = *portal_index_ptr;
      int16_t *portal = tag_block_get_element((char *)scenario + 0x154,
                                              (int)portal_index, 0x40);
      int16_t adjacent_cluster = portal[0];

      if (adjacent_cluster == current_cluster) {
        adjacent_cluster = portal[1];
      }

      if (structure_cluster_unmarked(adjacent_cluster) &&
          structure_get_planar_fog(scenario, portal_index, position, radius)) {
        int recurse_count = FUN_001989b0((uint16_t)adjacent_cluster, position,
                                         radius, remaining_count, out_indices);
        visited_count += recurse_count;
        remaining_count -= recurse_count;
        out_indices += (int16_t)recurse_count;
      }

      portal_iter += 1;
    } while ((int)portal_iter < *(int *)(cluster + 0x5c));
  }

  return (int16_t)visited_count;
}

int16_t structure_find_in_cluster(uint16_t cluster_count, float *position,
                                  float radius, int max_count,
                                  int16_t *intersected_indices)
{
  if (position == NULL) {
    display_assert("position", "c:\\halo\\SOURCE\\structures\\structures.c",
                   0x86, true);
    system_exit(-1);
  }

  if (radius < 0.f) {
    display_assert("radius>=0.f", "c:\\halo\\SOURCE\\structures\\structures.c",
                   0x87, true);
    system_exit(-1);
  }

  if ((int16_t)max_count <= 0) {
    display_assert("maximum_count>0",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x88, true);
    system_exit(-1);
  }

  if (intersected_indices == NULL) {
    display_assert("intersected_indices",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x89, true);
    system_exit(-1);
  }

  if ((int16_t)cluster_count != -1) {
    if (radius > 0.f) {
      int16_t cluster_count_out;

      structures_cluster_marker_begin();
      cluster_count_out = FUN_001989b0(cluster_count, position, radius,
                                       max_count, intersected_indices);
      structures_cluster_marker_end();
      return cluster_count_out;
    }

    if ((int16_t)max_count > 0) {
      *intersected_indices = (int16_t)cluster_count;
      return 1;
    }
  }

  return 0;
}
