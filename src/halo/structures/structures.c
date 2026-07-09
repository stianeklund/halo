/* FUN_00061ca0 (0x61ca0)
 *
 * Per-tick debug path-obstacle-avoidance key handler.  When the debug enable
 * byte (0x3340a9) is set and the developer console is not active, polls debug
 * keys and toggles the mode/enable bytes, optionally (re)builds the obstacle
 * avoidance record and advances it, then draws the current obstacle discs and
 * path steps.  Runs the two draw calls whenever the enable byte is set,
 * regardless of console state.
 *
 * Confirmed from disassembly at 0x61ca0:
 *   - scenario_get() is 0-arg (returns the current scenario tag base; 50+
 *     ported callers).  The 7 pushes Ghidra attributed to scenario_get are
 *     actually FUN_00060ea0's stack args (cdecl arg mis-grouping, §7).
 *   - FUN_00060ea0 is __thiscall + @eax: ECX=0x331f68 (avoidance record),
 *     EAX=0x5ab250 (end point2d), plus 9 cdecl stack args.  ADD ESP,0x24
 *     (36 bytes = 9 args) proves the split is 0 (scenario_get) + 9, not 7+2.
 *   - FUN_000615b0 takes @eax=0x331f68 (the record); its return is discarded.
 *   - FUN_00062960(&0x3334a0 obstacles, 0x5ab240 radius) and
 *     FUN_000609e0(&0x331f68 path) are clean cdecl (ADD ESP,0xc = 2+1).
 */
void FUN_00061ca0(void)
{
  void *scenario;

  if (*(char *)0x3340a9 != 0) {
    if (!console_is_active()) {
      if (input_key_is_down(0x22)) {
        *(char *)0x3340a8 = 1;
        *(char *)0x3340a9 = 0;
      }
      if (input_key_is_down(0x31)) {
        *(char *)0x3340a8 = 0;
        *(char *)0x3340a9 = 0;
      }
      if (input_key_is_down(0x3f)) {
        scenario = scenario_get();
        FUN_00060ea0((void *)0x331f68, (float *)0x5ab250, (void *)0x3334a0,
                     scenario, *(unsigned char *)0x5ab244, *(float *)0x5ab240,
                     (float *)0x5ab260, *(int *)0x5ab25c, *(float *)0x5ab248,
                     *(unsigned char *)0x5ab245, 0);
      }
      if (input_key_is_down(0x26)) {
        FUN_000615b0((void *)0x331f68);
      }
    }
    FUN_00062960((void *)0x3334a0, *(float *)0x5ab240);
    FUN_000609e0((void *)0x331f68);
  }
  return;
}

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

/* 0x1056e0 — Dispose of a sphere geometry object.
 * Asserts the sphere handle and its two allocated arrays (vertices at +0x4,
 * triangle_strip_vertex_indices at +0x8) are non-NULL, then frees the two
 * arrays followed by the sphere structure itself.
 * Source: c:\halo\SOURCE\math\geometry.c (lines 0x75-0x7b). */
void FUN_001056e0(void *handle)
{
  if (handle == 0) {
    display_assert("sphere", "c:\\halo\\SOURCE\\math\\geometry.c", 0x75, 1);
    halt_and_catch_fire();
  }
  if (*(int *)((char *)handle + 4) == 0) {
    display_assert("sphere->vertices", "c:\\halo\\SOURCE\\math\\geometry.c",
                   0x76, 1);
    halt_and_catch_fire();
  }
  if (*(int *)((char *)handle + 8) == 0) {
    display_assert("sphere->triangle_strip_vertex_indices",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x77, 1);
    halt_and_catch_fire();
  }
  debug_free(*(void **)((char *)handle + 4),
             "c:\\halo\\SOURCE\\math\\geometry.c", 0x79);
  debug_free(*(void **)((char *)handle + 8),
             "c:\\halo\\SOURCE\\math\\geometry.c", 0x7a);
  debug_free(handle, "c:\\halo\\SOURCE\\math\\geometry.c", 0x7b);
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
bool FUN_00106200(int16_t count, void *points, float *query_point,
                  float epsilon)
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

/* Clear a cluster partition (0x1915d0).
 * Resets the per-cluster head array (partition[0], 0x800 bytes) to the empty
 * sentinel (-1), then empties both datum pools: the per-object cluster
 * references (partition[2]) first, then the per-cluster object references
 * (partition[1]). Callee order and struct offsets confirmed from disassembly.
 */
void cluster_partition_clear(void *partition)
{
  int **part = (int **)partition;

  csmemset(part[0], -1, 0x800);
  data_delete_all((data_t *)part[2]);
  data_delete_all((data_t *)part[1]);
}

/* Dispose both datum pools of a cluster partition (0x191600).
 * Mirrors cluster_partition_clear's pool layout: the per-object cluster
 * references (partition[2]) are disposed first, then the per-cluster object
 * references (partition[1]). Each pool is a data_t whose signature byte at
 * +0x24 is non-zero only while allocated; disposal is skipped otherwise.
 * Callee (data_make_invalid) and disposal order confirmed from disassembly.
 */
void cluster_partition_dispose(void *partition)
{
  data_t **part = (data_t **)partition;

  if (*((char *)part[2] + 0x24) != '\0') {
    data_make_invalid(part[2]);
  }
  if (*((char *)part[1] + 0x24) != '\0') {
    data_make_invalid(part[1]);
  }
}

/* Null a cluster partition's three references (0x191630).
 * Zeroes the head-array pointer (partition[0]), the per-object cluster
 * references pool (partition[2]), then the per-cluster object references pool
 * (partition[1]) -- same [2]-before-[1] pool order as the clear/dispose
 * helpers. Each store is guarded by a test against 0 (if (f != 0) f = 0);
 * this conditional-store shape is preserved verbatim from the original.
 */
void cluster_partition_null_references(int *partition)
{
  if (partition[0] != 0) {
    partition[0] = 0;
  }
  if (partition[2] != 0) {
    partition[2] = 0;
  }
  if (partition[1] != 0) {
    partition[1] = 0;
  }
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

/* Seed-and-advance a cluster iterator (0x191690).
 * Stores cluster_handle into *out_cluster unconditionally; if the handle is
 * valid (!= -1), fetches the cluster reference datum from the pool at
 * cluster_list+8, overwrites *out_cluster with the next link (ref+8), and
 * returns the reference value (ref+4). Returns -1 for an exhausted handle.
 */
int FUN_00191690(void *cluster_list, int *out_cluster, int cluster_handle)
{
  *out_cluster = cluster_handle;
  if (cluster_handle != -1) {
    char *cluster_reference =
      datum_get(*(void **)((char *)cluster_list + 8), cluster_handle);
    *out_cluster = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

/* Advance a cluster iterator over the pool at offset +8 (0x1916d0).
 * Advance-only counterpart to FUN_00191690 (which seeds *state first). If the
 * current handle (*state) is exhausted (-1) returns -1; otherwise fetches the
 * reference datum from the pool at partition+8, advances *state to the next
 * link (ref+8), and returns the reference value (ref+4).
 */
int FUN_001916d0(int partition, int *state)
{
  if (*state != -1) {
    char *cluster_reference = datum_get(*(void **)(partition + 8), *state);
    *state = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

/* Copy a cluster partition (0x191700).
 * Both operands are 3-pointer structs (see cluster_partition_add_object):
 *   [0] -> cluster head array (one dword per cluster)
 *   [1] -> reference_list (per-cluster object references)
 *   [2] -> reference_list (per-object cluster references)
 * The head array holds scenario->0x134 (cluster tag_block count) dwords, so
 * csmemcpy moves count*4 bytes (the <<2 converts the dword count to bytes).
 * Copy order: element [2] BEFORE [1] -- preserve for codegen match.
 */
void cluster_partition_copy(void **destination, void **source)
{
  void *scenario = scenario_get();

  csmemcpy(destination[0], source[0], *(int *)((char *)scenario + 0x134) << 2);
  reference_list_copy(destination[2], source[2]);
  reference_list_copy(destination[1], source[1]);
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

  cluster_count = structure_find_in_cluster(cluster_bsp_index, (float *)pos,
                                            rad.f, 0x40, local_clusters);

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

/* leaf_map_node_stack_push (FUN_00191ad0, 0x191ad0)
 *
 * Bounds-checked push onto the global leaf-map node stack.  If the stack is
 * already full (count > MAXIMUM_NODE_STACK_COUNT-1 = 0xff), fires the engine
 * assert then system_exit(-1); otherwise stores the node value at
 * node_stack[count] and increments count.
 *
 * Confirmed from disassembly at 0x191ad0:
 *   - node_stack_count : int16 @ 0x4d8e90 (cmpw $0x100 / movswl / incw prove
 *     a 16-bit signed counter, not int32)
 *   - node_stack       : int32[256] @ 0x4d8a90 (0x400 bytes, ends at 0x4d8e90)
 *   - MAXIMUM_NODE_STACK_COUNT = 0x100; guard fires when count >= 0x100.
 *   - display_assert(reason, "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x2a,
 * 1) then system_exit(-1) (thunk_FUN_001029a0 resolves to system_exit
 * @0x8e2f0).
 *   - Single cdecl stack arg (the pushed node value); kb decl was void(void).
 */
void leaf_map_node_stack_push(int32_t node)
{
  if (*(int16_t *)0x4d8e90 >= 0x100) {
    display_assert("leaf_map_globals.node_stack_count<MAXIMUM_NODE_STACK_COUNT",
                   "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x2a, 1);
    system_exit(-1);
  }
  *(int32_t *)(0x4d8a90 + *(int16_t *)0x4d8e90 * 4) = node;
  *(int16_t *)0x4d8e90 = *(int16_t *)0x4d8e90 + 1;
}

/* FUN_00191ba0 (0x191ba0)
 *
 * Clears two tag_block members of a structure by resizing each to zero
 * elements.  The structure base is passed as a single cdecl stack argument;
 * the two 0xC-byte tag_blocks (count/address/definition triple) live at
 * base+0x4 and base+0x10.
 *
 * Confirmed from disassembly at 0x191ba0:
 *   - No FPU ops, no branching; two cdecl CALLs to tag_block_resize
 *     (FUN_001b9a90), each: push 0 (count); push block-ptr; call; ADD ESP,8.
 *   - kb decl was void(void); real signature is void f(void *base).
 */
void FUN_00191ba0(void *base)
{
  tag_block_resize((char *)base + 0x4, 0);
  tag_block_resize((char *)base + 0x10, 0);
}

/* FUN_00191d80 (0x191d80)
 *
 * Fetches an outer tag_block element (block = base+4, index masked to
 * 31 bits, stride 0x18), then scans the nested tag_block located at
 * outer_element+0xc (stride 4). Returns false (0) as soon as any inner
 * element's leading int is non-negative; otherwise returns the low byte
 * of the nested element count.
 *
 * Confirmed from disassembly at 0x191d80:
 *   - two cdecl calls to tag_block_get_element (ADD ESP,0xc each);
 *     push order gives (block, index, element_size).
 *   - the nested block pointer (outer_element+0xc) is held in ESI for
 *     the whole function; *ESI is the element count (int at offset 0).
 *   - inner loop counter is a signed short: INC EDI; MOVSX EAX,DI;
 *     CMP EAX,ECX; JL — kept as `short` so codegen emits the MOVSX.
 *   - both RET sites return AL only: early XOR AL,AL (false), and
 *     fall-through MOV AL,byte ptr [ESI] (low byte of the count).
 */
char FUN_00191d80(int base, unsigned int index)
{
  int *count_block;
  int count;
  int *inner;
  short i;

  count_block = (int *)((int)tag_block_get_element((void *)(base + 4),
                                                   index & 0x7fffffff, 0x18) +
                        0xc);
  count = *count_block;
  i = 0;
  if (0 < count) {
    count = 0;
    do {
      inner = (int *)tag_block_get_element(count_block, count, 4);
      if (-1 < *inner) {
        return 0;
      }
      i = i + 1;
      count = (int)i;
    } while (count < *count_block);
  }
  return *(char *)count_block;
}

/* FUN_00191ff0 (0x191ff0)
 *
 * Tag-block iterator. Fetches the tag-block element at
 * (index & 0x7fffffff) with stride 0x18 from the block at base+4,
 * then walks the nested tag-block at element+0xc (count@0, elements@+4),
 * passing each 4-byte element's first dword to FUN_00191e90(base, *elem).
 *
 * Confirmed from disassembly at 0x191ff0:
 * - outer tag_block_get_element(base+4, index & 0x7fffffff, 0x18)
 * - nested block pointer = element + 0xc
 * - inner tag_block_get_element(nested, i, 4)
 * - loop counter is a 16-bit short widened to int each iteration (movsx)
 * All calls cdecl, args pushed right-to-left. No FPU.
 */
void FUN_00191ff0(int base, unsigned int index)
{
  int *count_block;
  int i_idx;
  int *inner;
  short i;

  count_block = (int *)((int)tag_block_get_element((void *)(base + 4),
                                                   index & 0x7fffffff, 0x18) +
                        0xc);
  i = 0;
  if (0 < *count_block) {
    i_idx = 0;
    do {
      inner = (int *)tag_block_get_element(count_block, i_idx, 4);
      FUN_00191e90(base, *inner);
      i = i + 1;
      i_idx = (int)i;
    } while (i_idx < *count_block);
  }
}

/* FUN_001926a0 (0x1926a0)
 *
 * Copies a tag-block bitset from src to dst (word-granular), then for
 * every bit set in the destination bitset invokes FUN_00191de0 to
 * propagate/traverse the corresponding element.
 *
 * Confirmed from disassembly at 0x1926a0:
 * - descriptor at param_1; element count at *(param_1+4)
 * - if src != dst: csmemcpy(dst, src, ((count+0x1f)>>5)<<2) bytes
 *   (word-granular rounded copy of the bitset)
 * - for each bit index i in [0,count) that is set in the dst bitset:
 *     FUN_00191de0(descriptor, dst, i)
 * - count is re-read from *(param_1+4) every iteration (not cached)
 * - returns byte-bool true (AL=1; CONCAT31 high bytes are incidental)
 * All calls cdecl, args pushed right-to-left. No FPU.
 */
unsigned char FUN_001926a0(int descriptor, int src, int dst)
{
  int count;
  int i;

  if (src != dst) {
    csmemcpy((void *)dst, (void *)src,
             (size_t)(((*(int *)(descriptor + 4) + 0x1f) >> 5) << 2));
  }
  count = *(int *)(descriptor + 4);
  i = 0;
  if (0 < count) {
    do {
      if ((*(unsigned int *)(dst + (i >> 5) * 4) & (1 << (i & 0x1f))) != 0) {
        FUN_00191de0(descriptor, dst, i);
      }
      count = *(int *)(descriptor + 4);
      i = i + 1;
    } while (i < count);
  }
  return 1;
}

/* FUN_00194360 (0x194360)
 * qsort/bsort comparator. Two cdecl stack args are pointers to records.
 * Reads a signed int16 field at offset +0x10 of each record and orders
 * descending by that field: returns +1 when the second record's field is
 * strictly less than the first's (first sorts earlier), -1 otherwise. The
 * equal case also falls into -1, matching the original (cond*2 - 1) shape.
 */
int FUN_00194360(int param_1, int param_2)
{
  return (unsigned int)(*(short *)(param_2 + 0x10) <
                        *(short *)(param_1 + 0x10)) *
           2 +
         -1;
}

/* FUN_00194380 (0x194380)
 * Collision-BSP point query. Fetches the structure's bsp3d root element
 * (tag_block at param_1+0xb0, element 0, stride 0x60), locates the leaf that
 * contains point param_2 via bsp3d_find_leaf(root, node=0, point), then returns
 * the signed int16 field at +0x8 of the matching leaf element (tag_block at
 * param_1+0xe0, stride 0x10). Returns -1 on the 0xffffffff not-found sentinel.
 *
 * NOTE: Ghidra mis-groups the two calls. MSVC evaluates and pushes
 * bsp3d_find_leaf's 2nd arg (0) and 3rd arg (param_2) BEFORE the inner
 * tag_block_get_element call, so the decompiler folded them into
 * tag_block_get_element as a bogus 5-arg call and showed bsp3d_find_leaf with
 * a single arg. Both callees are proven 3-arg cdecl: 0x19b210 reads only
 * [ebp+8]/[ebp+0xc]/[ebp+0x10], and the third call site is a clean
 * 3-push / add esp,0xc. The nested call form reproduces the push interleave.
 * The leaf index has its high bit stripped (& 0x7fffffff) before use.
 */
int FUN_00194380(int param_1, void *param_2)
{
  void *leaf_element;
  unsigned int leaf;

  leaf = bsp3d_find_leaf(
    tag_block_get_element((void *)(param_1 + 0xb0), 0, 0x60), 0, param_2);
  if (leaf != 0xffffffff) {
    leaf_element = tag_block_get_element((void *)(param_1 + 0xe0),
                                         (int)(leaf & 0x7fffffff), 0x10);
    return (int)*(short *)((char *)leaf_element + 8);
  }
  return -1;
}

/* FUN_00195530 (0x195530)
 * Integer greater-than comparator, likely a qsort/bsearch comparison callback.
 * Two cdecl stack int args, bool/AL return. Returns true only when
 * param_1 > param_2 (the equal case returns false). The two-branch shape
 * (early-return false on param_2 > param_1, then param_2 < param_1) is the
 * original MSVC codegen shape (cmp param_2,param_1 / jle / xor al,al / setl al)
 * and is preserved verbatim; collapsing it to a single param_1 > param_2 would
 * change the emitted branch structure. VC71 recomputes the compare for the
 * trailing setl (clean-bool-via-edx idiom) where the original reused the
 * branch's flags directly, a ~3-insn small-function idiom gap (88.9% VC71).
 */
char FUN_00195530(int param_1, int param_2)
{
  if (param_2 > param_1) {
    return 0;
  }
  return param_2 < param_1;
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

bool structure_get_planar_fog(void *scenario, int16_t portal_index,
                              float *position, float radius)
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
/* 0x195550 - gather structure surfaces selected by a per-32-surface bitmask.
 *
 * Walks the scenario structure-BSP surfaces tag_block at scenario+0xf8 (first
 * int = surface element count). mask is an array of uint bitmask words, one word
 * per 32 surfaces; for each set bit the matching surface index is appended to
 * out_indices and its 6-byte (short[3]) tag element is copied into out_surfaces
 * (stride 6 bytes). surface_count bounds the output write index (asserted).
 *
 * A zero mask word skips an entire block of 0x20 surfaces (surface_index +=
 * 0x20 without touching the tag_block). The loop bound (*count) is re-read every
 * outer iteration - preserved from the original, not cached. The element copy is
 * exactly three 16-bit moves (element_size 6); widths are kept at uint16. */
void FUN_00195550(short surface_count, int *out_indices, uint32_t *mask,
                  int out_surfaces)
{
  int *block;
  int scenario;
  short bit;
  short write_index;
  int surface_index;
  uint16_t *dst;
  uint16_t *elem;

  scenario = (int)scenario_get();
  block = (int *)(scenario + 0xf8);
  write_index = 0;
  surface_index = 0;
  if (0 < *(int *)(scenario + 0xf8)) {
    do {
      if (*mask == 0) {
        surface_index = surface_index + 0x20;
      }
      else {
        bit = 0;
        do {
          if (*block <= surface_index) break;
          if ((*mask & 1 << ((uint8_t)bit & 0x1f)) != 0) {
            elem = (uint16_t *)tag_block_get_element(block, surface_index, 6);
            if ((write_index < 0) || (surface_count <= write_index)) {
              display_assert(
                  "surface_index_index>=0 && surface_index_index<surface_count",
                  "c:\\halo\\SOURCE\\structures\\structure_render.c", 0x1a5, true);
              system_exit(-1);
            }
            *out_indices = surface_index;
            out_indices = out_indices + 1;
            dst = (uint16_t *)(out_surfaces + write_index * 6);
            dst[0] = elem[0];
            dst[1] = elem[1];
            dst[2] = elem[2];
            write_index = write_index + 1;
          }
          bit = bit + 1;
          surface_index = surface_index + 1;
        } while (bit < 0x20);
      }
      mask = mask + 1;
    } while (surface_index < *block);
  }
}
