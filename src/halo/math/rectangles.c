/* Fold points[point_index] into a 3D convex hull (0x107520). Raw record
 * offsets as accessed here: vertex stride 0xc (+0 extant, +2 int16
 * point_index, +4 int32 edge_index); edge stride 0x20 (+0 extant, +4/+8 int32
 * vertex_indices, +0xc/+0x10 int32 edge_indices, +0x14/+0x18 int32
 * surface_indices); surface stride 0x1c (+0 extant, +4..+0x10 plane i,j,k,d,
 * +0x14 int32 edge_index). x87 addend order per loop follows the binary. */
bool convex_hull3d_expand(int16_t point_count, float *points,
                          int16_t vertices_capacity, char *vertices,
                          int16_t edges_capacity, char *edges,
                          int16_t surfaces_capacity, char *surfaces,
                          int16_t point_index)
{
  float *new_point;
  bool already_contained;
  int16_t first_boundary_edge_index;
  int16_t surface_index;
  int16_t edge_index;
  int16_t vertex_index;
  int16_t new_vertex_index;
  int16_t previous_new_edge_index;
  int16_t first_new_edge_index;
  char *surface;
  char *edge;
  char *vertex;

  already_contained = 1;
  if (points == (float *)0) {
    display_assert("points", "c:\\halo\\SOURCE\\math\\geometry.c", 0x7b5, 1);
    system_exit(-1);
  }
  if (vertices == (char *)0) {
    display_assert("vertices", "c:\\halo\\SOURCE\\math\\geometry.c", 0x7b6, 1);
    system_exit(-1);
  }
  if (edges == (char *)0) {
    display_assert("edges", "c:\\halo\\SOURCE\\math\\geometry.c", 0x7b7, 1);
    system_exit(-1);
  }
  if (surfaces == (char *)0) {
    display_assert("surfaces", "c:\\halo\\SOURCE\\math\\geometry.c", 0x7b8, 1);
    system_exit(-1);
  }

  if (point_index < 0 || point_index >= point_count) {
    return 0;
  }

  new_point = points + point_index * 3;
  first_boundary_edge_index = -1;
  for (surface_index = 0; surface_index < surfaces_capacity; surface_index++) {
    surface = surfaces + surface_index * 0x1c;
    if (*surface != 0 && (*(float *)(surface + 0xc) * new_point[2] +
                          *(float *)(surface + 8) * new_point[1] +
                          new_point[0] * *(float *)(surface + 4)) -
                             *(float *)(surface + 0x10) >
                           *(float *)0x31fb3c) {
      already_contained = 0;
      break;
    }
  }
  if (already_contained) {
    return 1;
  }

  for (surface_index = 0; surface_index < surfaces_capacity; surface_index++) {
    surface = surfaces + surface_index * 0x1c;
    if (*surface != 0) {
      float distance = (*(float *)(surface + 4) * new_point[0] +
                        *(float *)(surface + 0xc) * new_point[2] +
                        *(float *)(surface + 8) * new_point[1]) -
                       *(float *)(surface + 0x10);
      *surface = !(distance > -*(float *)0x31fb40);
    }
  }

  for (edge_index = 0; edge_index < edges_capacity; edge_index++) {
    edge = edges + edge_index * 0x20;
    if (*edge != 0) {
      char *surface0;
      char *surface1;

      if (*(int32_t *)(edge + 0x14) < 0 ||
          *(int32_t *)(edge + 0x14) >= surfaces_capacity ||
          *(int32_t *)(edge + 0x18) < 0 ||
          *(int32_t *)(edge + 0x18) >= surfaces_capacity) {
        return 0;
      }
      surface0 = surfaces + *(int32_t *)(edge + 0x14) * 0x1c;
      surface1 = surfaces + *(int32_t *)(edge + 0x18) * 0x1c;

      *edge = *surface0 != 0 || *surface1 != 0;
      if (*edge != 0 && *surface0 != *surface1) {
        if (*surface0 == 0) {
          *(int32_t *)(edge + 0x14) = -1;
        } else {
          *(int32_t *)(edge + 0x18) = -1;
        }
        if (first_boundary_edge_index == -1) {
          first_boundary_edge_index = edge_index;
        }
      }
    }
  }
  if (first_boundary_edge_index == -1) {
    display_assert("first_boundary_edge_index!=NONE",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x81c, 1);
    system_exit(-1);
  }

  for (vertex_index = 0; vertex_index < vertices_capacity; vertex_index++) {
    vertex = vertices + vertex_index * 0xc;
    if (*vertex != 0) {
      int16_t first_edge_index = -1;
      int16_t previous_edge_index = -1;
      int16_t vertex_edge_index = (int16_t) * (int32_t *)(vertex + 4);
      char *previous_edge;

      do {
        if (vertex_edge_index < 0 || vertex_edge_index >= edges_capacity) {
          display_assert("edge_index>=0 && edge_index<edge_count",
                         "c:\\halo\\SOURCE\\math\\geometry.c", 0x832, 1);
          system_exit(-1);
        }
        edge = edges + vertex_edge_index * 0x20;
        if (*(int32_t *)(edge + 4) != vertex_index &&
            *(int32_t *)(edge + 8) != vertex_index) {
          display_assert("edge->vertex_indices[0]==vertex_index || "
                         "edge->vertex_indices[1]==vertex_index",
                         "c:\\halo\\SOURCE\\math\\geometry.c", 0x833, 1);
          system_exit(-1);
        }

        if (*edge != 0) {
          if (previous_edge_index == -1) {
            first_edge_index = vertex_edge_index;
          } else {
            previous_edge = edges + previous_edge_index * 0x20;
            *(int32_t *)(previous_edge + 0xc +
                         (*(int32_t *)(previous_edge + 4) == vertex_index) *
                           4) = vertex_edge_index;
          }
          previous_edge_index = vertex_edge_index;
        }
        vertex_edge_index =
          (int16_t) * (int32_t *)(edge + 0xc +
                                  (*(int32_t *)(edge + 4) == vertex_index) * 4);
      } while (vertex_edge_index != *(int32_t *)(vertex + 4));

      if (previous_edge_index == -1) {
        *vertex = 0;
      } else {
        previous_edge = edges + previous_edge_index * 0x20;
        *(int32_t *)(vertex + 4) = first_edge_index;
        *(int32_t *)(previous_edge + 0xc +
                     (*(int32_t *)(previous_edge + 4) == vertex_index) * 4) =
          first_edge_index;
      }
    }
  }

  new_vertex_index = 0;
  first_new_edge_index = -1;
  previous_new_edge_index = -1;
  edge_index = first_boundary_edge_index;
  while (new_vertex_index < vertices_capacity &&
         *(vertices + new_vertex_index * 0xc) != 0) {
    new_vertex_index++;
  }
  if (new_vertex_index >= vertices_capacity) {
    return 0;
  }

  for (;;) {
    char *vertex1;
    char *vertex2;
    char *new_surface;
    char *new_edge;
    int16_t new_surface_index;
    int16_t new_edge_index;

    if (edge_index < 0 || edge_index >= edges_capacity) {
      display_assert("edge_index>=0 && edge_index<edge_count",
                     "c:\\halo\\SOURCE\\math\\geometry.c", 0x877, 1);
      system_exit(-1);
    }
    edge = edges + edge_index * 0x20;
    if (*edge == 0) {
      display_assert("edge->extant", "c:\\halo\\SOURCE\\math\\geometry.c",
                     0x878, 1);
      system_exit(-1);
    }
    if (*(int32_t *)(edge + 4) < 0 ||
        *(int32_t *)(edge + 4) >= vertices_capacity) {
      display_assert(
        "edge->vertex_indices[0]>=0 && edge->vertex_indices[0]<vertex_count",
        "c:\\halo\\SOURCE\\math\\geometry.c", 0x879, 1);
      system_exit(-1);
    }
    if (*(int32_t *)(edge + 8) < 0 ||
        *(int32_t *)(edge + 8) >= vertices_capacity) {
      display_assert(
        "edge->vertex_indices[1]>=0 && edge->vertex_indices[1]<vertex_count",
        "c:\\halo\\SOURCE\\math\\geometry.c", 0x87a, 1);
      system_exit(-1);
    }
    if ((*(int32_t *)(edge + 0x14) == -1) ==
        (*(int32_t *)(edge + 0x18) == -1)) {
      display_assert(
        "(edge->surface_indices[0]==NONE)!=(edge->surface_indices[1]==NONE)",
        "c:\\halo\\SOURCE\\math\\geometry.c", 0x87b, 1);
      system_exit(-1);
    }

    for (new_surface_index = 0; new_surface_index < surfaces_capacity &&
                                *(surfaces + new_surface_index * 0x1c) != 0;
         new_surface_index++) {
    }
    if (new_surface_index >= surfaces_capacity) {
      break;
    }

    for (new_edge_index = 0; new_edge_index < edges_capacity &&
                             *(edges + new_edge_index * 0x20) != 0;
         new_edge_index++) {
    }
    if (new_edge_index >= edges_capacity) {
      break;
    }

    new_surface = surfaces + new_surface_index * 0x1c;
    vertex1 =
      vertices +
      *(int32_t *)(edge + 4 + (*(int32_t *)(edge + 0x14) != -1) * 4) * 0xc;
    vertex2 =
      vertices +
      *(int32_t *)(edge + 4 + (*(int32_t *)(edge + 0x18) != -1) * 4) * 0xc;

    if (*vertex1 == 0) {
      display_assert("vertex1->extant", "c:\\halo\\SOURCE\\math\\geometry.c",
                     0x89e, 1);
      system_exit(-1);
    }
    if (*(int16_t *)(vertex1 + 2) < 0 ||
        *(int16_t *)(vertex1 + 2) >= point_count) {
      display_assert(
        "vertex1->point_index>=0 && vertex1->point_index<point_count",
        "c:\\halo\\SOURCE\\math\\geometry.c", 0x89f, 1);
      system_exit(-1);
    }
    if (*vertex2 == 0) {
      display_assert("vertex2->extant", "c:\\halo\\SOURCE\\math\\geometry.c",
                     0x8a1, 1);
      system_exit(-1);
    }
    if (*(int16_t *)(vertex2 + 2) < 0 ||
        *(int16_t *)(vertex2 + 2) >= point_count) {
      display_assert(
        "vertex2->point_index>=0 && vertex2->point_index<point_count",
        "c:\\halo\\SOURCE\\math\\geometry.c", 0x8a2, 1);
      system_exit(-1);
    }

    *new_surface = 1;
    FUN_001037b0((float *)(new_surface + 4), new_point,
                 points + *(int16_t *)(vertex1 + 2) * 3,
                 points + *(int16_t *)(vertex2 + 2) * 3);
    *(int32_t *)(new_surface + 0x14) = edge_index;

    new_edge = edges + new_edge_index * 0x20;
    *new_edge = 1;
    *(int32_t *)(new_edge + 4) =
      *(int32_t *)(edge + 4 + (*(int32_t *)(edge + 0x18) != -1) * 4);
    *(int32_t *)(new_edge + 8) = new_vertex_index;
    *(int32_t *)(new_edge + 0xc) = previous_new_edge_index;
    *(int32_t *)(new_edge + 0x10) =
      *(int32_t *)(edge + 0xc + (*(int32_t *)(edge + 0x14) != -1) * 4);
    *(int32_t *)(new_edge + 0x14) = new_surface_index;
    *(int32_t *)(new_edge + 0x18) = -1;

    *(int32_t *)(edge + 0xc + (*(int32_t *)(edge + 0x14) != -1) * 4) =
      new_edge_index;
    *(int32_t *)(edge + 0x14 + (*(int32_t *)(edge + 0x14) != -1) * 4) =
      new_surface_index;
    if (previous_new_edge_index == -1) {
      first_new_edge_index = new_edge_index;
    } else {
      *(int32_t *)(edges + previous_new_edge_index * 0x20 + 0x18) =
        new_surface_index;
    }
    previous_new_edge_index = new_edge_index;

    edge_index = (int16_t) * (int32_t *)(new_edge + 0x10);
    if (edge_index == first_boundary_edge_index) {
      vertex = vertices + new_vertex_index * 0xc;
      *(int16_t *)(vertex + 2) = point_index;
      *(int32_t *)(vertex + 4) = first_new_edge_index;
      *vertex = 1;
      edge = edges + first_new_edge_index * 0x20;
      *(int32_t *)(edge + 0xc) = new_edge_index;
      *(int32_t *)(new_edge + 0x18) = *(int32_t *)(edge + 0x14);
      return 1;
    }
  }

  return 0;
}

/* Build a 3D convex hull from a point cloud (0x107c30).
 * Validates the four output-array pointers (null asserts + system_exit(-1),
 * matching FUN_00106f50's own idiom at geometry.c), then calls FUN_00106f50
 * to build the initial tetrahedron/hull, then calls convex_hull3d_expand
 * once per input point to fold each remaining point into the hull. Returns
 * false immediately if either FUN_00106f50 or any convex_hull3d_expand call
 * fails; true once every point has been folded in.
 * point_count is read/compared as a 16-bit value (TEST SI,SI / CMP DI,SI in
 * the binary), so the loop index and count stay int16_t here. */
bool convex_hull3d(int16_t point_count, float *points,
                   int16_t vertices_capacity, char *vertices,
                   int16_t edges_capacity, char *edges,
                   int16_t surfaces_capacity, char *surfaces)
{
  int16_t i;
  bool ok;

  if (points == (float *)0) {
    display_assert("points", "c:\\halo\\SOURCE\\math\\geometry.c", 0x8ec, 1);
    system_exit(-1);
  }
  if (vertices == (char *)0) {
    display_assert("vertices", "c:\\halo\\SOURCE\\math\\geometry.c", 0x8ed, 1);
    system_exit(-1);
  }
  if (edges == (char *)0) {
    display_assert("edges", "c:\\halo\\SOURCE\\math\\geometry.c", 0x8ee, 1);
    system_exit(-1);
  }
  if (surfaces == (char *)0) {
    display_assert("surfaces", "c:\\halo\\SOURCE\\math\\geometry.c", 0x8ef, 1);
    system_exit(-1);
  }

  ok = FUN_00106f50(point_count, points, vertices_capacity, vertices,
                    edges_capacity, edges, surfaces_capacity, surfaces);
  if (!ok) {
    return 0;
  }

  for (i = 0; i < point_count; i++) {
    ok = convex_hull3d_expand(point_count, points, vertices_capacity, vertices,
                              edges_capacity, edges, surfaces_capacity,
                              surfaces, i);
    if (!ok) {
      return 0;
    }
  }

  return 1;
}

/* Test whether a point lies inside every valid face of a 3D convex hull
 * (0x107d40).
 * `surfaces` is a char* blob array, stride 0x1c, matching the raw-offset
 * style already used for convex_hull3d/convex_hull3d_expand's vertices/
 * edges/surfaces params in this file. Per-surface record (bytes proven by
 * this function's own accesses; layout past 0x10 is unproven -- stride is
 * 0x1c but nothing beyond 0x10 is ever read here):
 *   +0x00 flag (uint8_t)   -- surface active/valid when nonzero
 *   +0x04 real normal.x
 *   +0x08 real normal.y
 *   +0x0c real normal.z
 *   +0x10 real d (plane distance)
 * xrefs_to is empty for this address (no callers found in this binary), so
 * the true parameter count/types cannot be confirmed from any call site.
 * The disassembly proves only 3 stack reads, at EBP+0x20 (surface_count,
 * word), EBP+0x24 (surfaces, dword) and EBP+0x28 (point, dword) -- i.e. the
 * 7th/8th/9th cdecl argument slots. EBP+0x08..+0x1c (the first six 4-byte
 * slots) are never read by this function body. Rather than invent a
 * signature for that unread region, it is preserved here as six explicit
 * unused parameters so the three real parameters keep their proven stack
 * offsets; nothing supports a guess at their real types/count.
 * Per-surface test order and grouping match the FPU trace exactly (FP
 * addition is bit-exact commutative, but grouping affects rounding and is
 * preserved): dot = (normal.z*point.z + normal.y*point.y) + normal.x*point.x;
 * a surface fails the point when *(float *)0x31fb40 (0.001f) < (dot - d), at
 * which point the function returns false immediately (short-circuit over the
 * whole array, not just skipping one surface). Comparison uses FCOMP +
 * FNSTSW AX + TEST AH,0x41 (fails only when strictly greater, not on <=, ==,
 * or unordered). surface_count/loop index are compared as signed 16-bit
 * (TEST DI,DI / JLE, CMP CX,DI / JL), matching int16_t.
 * 0x31fb40 has no prior name/declaration anywhere in this codebase; per the
 * precedent in real_math.c's angular_accelerate_to_position (which
 * dereferences fixed rdata addresses like 0x2533c0/0x2533c8/0x255e94
 * in-line rather than inventing a named global), it is dereferenced directly
 * here rather than declared as an extern DAT_ symbol. */
boolean
convex_hull3d_test_point(uint32_t unused_param_1, uint32_t unused_param_2,
                         uint32_t unused_param_3, uint32_t unused_param_4,
                         uint32_t unused_param_5, uint32_t unused_param_6,
                         int16_t surface_count, char *surfaces, real *point)
{
  int16_t i;
  char *surface;
  real dot;

  (void)unused_param_1;
  (void)unused_param_2;
  (void)unused_param_3;
  (void)unused_param_4;
  (void)unused_param_5;
  (void)unused_param_6;

  for (i = 0; i < surface_count; i++) {
    surface = surfaces + i * 0x1c;
    if (*surface != 0) {
      dot = (*(real *)(surface + 0xc) * point[2] +
             *(real *)(surface + 8) * point[1]) +
            *(real *)(surface + 4) * point[0];
      if (*(float *)0x31fb40 < dot - *(real *)(surface + 0x10)) {
        return 0;
      }
    }
  }

  return 1;
}

/* Clip the ray point + t*vector against the hull surfaces (0x107db0). Same
 * six unused leading dwords and 0x1c-byte surface stride as
 * convex_hull3d_test_point. Returns false as soon as the [t_min, t_max]
 * interval empties, or when the ray is parallel (|denom| < double at
 * 0x2533d0) and the point lies outside a surface by more than 0x31fb40.
 * The original stores denom into the surface_count stack slot (count is
 * kept in BX). */
boolean
convex_hull3d_test_vector(uint32_t unused_param_1, uint32_t unused_param_2,
                          uint32_t unused_param_3, uint32_t unused_param_4,
                          uint32_t unused_param_5, uint32_t unused_param_6,
                          int16_t surface_count, char *surfaces, real *point,
                          real *vector, real *t_min_out, real *t_max_out)
{
  int16_t i;
  char *surface;
  real numerator;
  real denom;
  real t;
  real t_min;
  real t_max;

  (void)unused_param_1;
  (void)unused_param_2;
  (void)unused_param_3;
  (void)unused_param_4;
  (void)unused_param_5;
  (void)unused_param_6;

  t_min = -3.4028235e38f; /* -FLT_MAX */
  t_max = 3.4028235e38f; /* +FLT_MAX */
  for (i = 0; i < surface_count; i++) {
    surface = surfaces + i * 0x1c;
    if (*surface != 0) {
      numerator = ((*(real *)(surface + 0xc) * point[2] +
                    *(real *)(surface + 8) * point[1]) +
                   *(real *)(surface + 4) * point[0]) -
                  *(real *)(surface + 0x10);
      denom = (*(real *)(surface + 0xc) * vector[2] +
               *(real *)(surface + 8) * vector[1]) +
              *(real *)(surface + 4) * vector[0];
      if (fabs(denom) < *(double *)0x2533d0) {
        if (numerator > *(float *)0x31fb40) {
          return 0;
        }
      } else {
        t = -(numerator / denom);
        if (denom > *(float *)0x2533c0) {
          if (t_min < t) {
            t_min = t;
          }
        } else {
          if (t_max > t) {
            t_max = t;
          }
        }
        if (t_min > t_max) {
          return 0;
        }
      }
    }
  }

  if (t_min_out != (real *)0) {
    *t_min_out = t_min;
  }
  if (t_max_out != (real *)0) {
    *t_max_out = t_max;
  }
  return 1;
}

/* Return (and cache) the vertex index for the point at parameter `level` of
 * `subdivision` steps along the geosphere edge from vertex a to vertex b
 * (0x107ec0). a is register-passed (@<eax>); sphere/b/level/vertex_index_ptr/
 * outer_hash are stack args.
 * sphere layout (raw offsets, matching calculate_vertex's style):
 *   +0x0 subdivision (int16_t), +0xc vertex_count (int16_t).
 * outer_hash is a fixed 64-entry int16_t hash table (see FUN_001087b0),
 * keyed by vb + va*8; va/vb are always among the 6 base octahedron vertex
 * indices (0-5), so the key never exceeds 45. */
int16_t get_edge_vertex(int16_t a /* @<eax> */, void *sphere, int16_t b,
                        int16_t level, int16_t *vertex_index_ptr,
                        int16_t *outer_hash)
{
  int16_t subdivision;
  int16_t vertex_count;
  int16_t va;
  int16_t vb;
  int16_t swapped;
  int16_t hash_key;
  int16_t base;
  int16_t i;
  int16_t new_vertex;

  if (sphere == (void *)0) {
    display_assert("sphere", "c:\\halo\\SOURCE\\math\\geometry.c", 0x10d, 1);
    system_exit(-1);
  }

  if (a > b) {
    va = b;
    vb = a;
    swapped = 1;
  } else {
    va = a;
    vb = b;
    swapped = 0;
  }

  subdivision = *(int16_t *)sphere;
  vertex_count = *(int16_t *)((char *)sphere + 0xc);

  if (va < 0 || va >= vertex_count) {
    display_assert("va >= 0 && va < sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x10e, 1);
    system_exit(-1);
  }
  if (vb < 0 || vb >= vertex_count) {
    display_assert("vb >= 0 && vb < sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x10f, 1);
    system_exit(-1);
  }
  if (vb == va) {
    display_assert("vb != va", "c:\\halo\\SOURCE\\math\\geometry.c", 0x110, 1);
    system_exit(-1);
  }

  if (level == 0) {
    return a;
  }
  if (level == subdivision) {
    return b;
  }

  hash_key = (int16_t)(vb + va * 8);
  base = outer_hash[hash_key];
  if (base == -1) {
    base = *vertex_index_ptr;
    outer_hash[hash_key] = base;
    for (i = 1; i < subdivision; i++) {
      new_vertex = *vertex_index_ptr;
      *vertex_index_ptr = (int16_t)(*vertex_index_ptr + 1);
      calculate_vertex(i, subdivision, vb, va, sphere, new_vertex);
    }
  }

  if (swapped) {
    return (int16_t)(base + (subdivision - level) - 1);
  }
  return (int16_t)(base + level - 1);
}

/* Intersect convex 2D hull q with convex 2D hull p by clipping q against
 * each edge line of p (0x108060). PAL 2342 geometry.c
 * `convex_hull2d_intersect` (T2); the seven asserts are geometry.c lines
 * 0x3f8-0x3fe and 0x40d with PAL's condition strings. Points are
 * real_point2d pairs (8 bytes). Two 0x200-point ping-pong buffers sit at
 * [EBP-0x200c]; the last edge writes straight into result. Returns the
 * clipped point count, or -1 when a clip overflows maximum_count.
 * Confirmed: plane2d_from_points(&plane, p+index, p+previous) at 0x1081e1;
 * convex_polygon2d_clip_to_plane(count, source, &plane, maximum_count,
 * output, NULL, NULL, epsilon) at 0x108203 (ADD ESP,0x20). epsilon is the
 * raw [EBP+0x20] dword, so every caller passes a float literal. */
short FUN_00108060(int16_t count, void *records, int a3, uint16_t *scratch,
                   int max_count, uint16_t *out_list, float epsilon)
{
  float buffers[2][0x200][2];
  float plane[3];
  float *p;
  float *source;
  float *output;
  int16_t p_count;
  int16_t maximum_count;
  int16_t result_count;
  int16_t index;
  int16_t previous_index;

  p_count = count;
  p = (float *)records;
  result_count = (int16_t)a3;
  source = (float *)scratch;
  maximum_count = (int16_t)max_count;
  if (maximum_count > 0x200) {
    display_assert("maximum_count<=CLIP_BUFFER_SIZE",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x3f8, 1);
    system_exit(-1);
  }
  if (p == (float *)0) {
    display_assert("p", "c:\\halo\\SOURCE\\math\\geometry.c", 0x3f9, 1);
    system_exit(-1);
  }
  if (p_count == 0) {
    display_assert("p_count", "c:\\halo\\SOURCE\\math\\geometry.c", 0x3fa, 1);
    system_exit(-1);
  }
  if (source == (float *)0) {
    display_assert("q", "c:\\halo\\SOURCE\\math\\geometry.c", 0x3fb, 1);
    system_exit(-1);
  }
  if (result_count == 0) {
    display_assert("q_count", "c:\\halo\\SOURCE\\math\\geometry.c", 0x3fc, 1);
    system_exit(-1);
  }
  if (out_list == (uint16_t *)0) {
    display_assert("result", "c:\\halo\\SOURCE\\math\\geometry.c", 0x3fd, 1);
    system_exit(-1);
  }
  if ((void *)p == (void *)out_list || (void *)source == (void *)out_list) {
    display_assert("p!=result && q!=result",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x3fe, 1);
    system_exit(-1);
  }

  for (index = 0; index < p_count && result_count > 0; index++) {
    previous_index = index != 0 ? index - 1 : p_count - 1;
    output =
      index == p_count - 1 ? (float *)out_list : &buffers[index & 1][0][0];
    if (plane2d_from_points(plane, p + index * 2, p + previous_index * 2)) {
      result_count = convex_polygon2d_clip_to_plane(
        result_count, source, plane, maximum_count, output, (uint32_t *)0,
        (uint8_t *)0, epsilon);
      if (result_count == -1) {
        return -1;
      }
    } else {
      if (result_count < 0 || result_count > maximum_count) {
        display_assert("result_count>=0 && result_count<=maximum_count",
                       "c:\\halo\\SOURCE\\math\\geometry.c", 0x40d, 1);
        system_exit(-1);
      }
      csmemcpy(output, source, (int)result_count * 8);
    }
    source = output;
  }

  return result_count;
}

/* Return (and cache) the geosphere vertex at row/col of the triangular
 * row_cache grid used while subdividing face (v1,v2,v3) (0x108270). row is
 * @<eax>, sphere is @<ecx>; v1/v2/v3/col/vertex_index_ptr/outer_hash/
 * row_cache are stack args. row_cache is a (subdivision+1)^2-entry int16_t
 * table, row-major, index (subdivision+1)*row+col (allocated and -1-filled
 * by subdivide_triangle, one per face-subdivision call). */
int16_t get_face_vertex(int16_t row /* @<eax> */, void *sphere /* @<ecx> */,
                        int16_t v1, int16_t v2, int16_t v3, int16_t col,
                        int16_t *vertex_index_ptr, int16_t *outer_hash,
                        int16_t *row_cache)
{
  int16_t subdivision;
  int16_t vertex_count;
  int cache_index;
  int16_t new_vertex;
  int16_t r1;
  int16_t r2;

  subdivision = *(int16_t *)sphere;
  vertex_count = *(int16_t *)((char *)sphere + 0xc);
  cache_index = (subdivision + 1) * row + col;

  if (v1 < 0 || v1 > vertex_count) {
    display_assert("v1 >=0 && v1 <= sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0xde, 1);
    system_exit(-1);
  }
  if (v2 < 0 || v2 > vertex_count) {
    display_assert("v2 >=0 && v2 <= sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0xdf, 1);
    system_exit(-1);
  }
  if (v3 < 0 || v3 > vertex_count) {
    display_assert("v3 >=0 && v3 <= sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0xe0, 1);
    system_exit(-1);
  }

  if (row_cache[cache_index] == -1) {
    if (col == 0) {
      row_cache[cache_index] =
        get_edge_vertex(v1, sphere, v3, row, vertex_index_ptr, outer_hash);
    } else if (row == subdivision) {
      row_cache[cache_index] =
        get_edge_vertex(v3, sphere, v2, col, vertex_index_ptr, outer_hash);
    } else if (col == row) {
      row_cache[cache_index] =
        get_edge_vertex(v1, sphere, v2, row, vertex_index_ptr, outer_hash);
    } else {
      new_vertex = *vertex_index_ptr;
      *vertex_index_ptr = (int16_t)(*vertex_index_ptr + 1);
      r1 = get_edge_vertex(v1, sphere, v3, row, vertex_index_ptr, outer_hash);
      r2 = get_edge_vertex(v1, sphere, v2, row, vertex_index_ptr, outer_hash);
      row_cache[cache_index] = new_vertex;
      calculate_vertex(col, row, r2, r1, sphere, new_vertex);
    }
  }

  return row_cache[cache_index];
}

/* Subdivide triangle (v1,v2,v3) into a triangle strip, writing vertex
 * indices into sphere->index_array and advancing *vertex_index_ptr /
 * *triangle_strip_vertex_indices_index_ptr (0x108400). sphere is @<esi>;
 * all other params are stack args. Allocates a scratch row_cache of
 * (subdivision+1)^2 int16_t entries (-1-filled), used only for this call and
 * freed before return. For each row 1..subdivision, a strip-restart marker
 * (2*row+1) is written first, then for each col 1..row the four triangle
 * corners (top, left, right, and topright when col<row) are resolved via
 * get_face_vertex and written into index_array; sphere->field_10 is
 * incremented once per row (purpose unconfirmed). */
void subdivide_triangle(void *sphere /* @<esi> */, int16_t v1, int16_t v2,
                        int16_t v3, int16_t *vertex_index_ptr,
                        int16_t *triangle_strip_vertex_indices_index_ptr,
                        int16_t *outer_hash)
{
  int16_t subdivision;
  int16_t vertex_count;
  int16_t triangle_count;
  int16_t *index_array;
  int16_t *row_cache;
  int row_cache_count;
  int row;
  int col;
  int marker;
  int strip_index;
  int16_t top_vertex;
  int16_t left_vertex;
  int16_t right_vertex;
  int16_t topright_vertex;

  if (vertex_index_ptr == (int16_t *)0) {
    display_assert("vertex_index", "c:\\halo\\SOURCE\\math\\geometry.c", 0x92,
                   1);
    system_exit(-1);
  }
  if (triangle_strip_vertex_indices_index_ptr == (int16_t *)0) {
    display_assert("triangle_strip_vertex_indices_index",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x93, 1);
    system_exit(-1);
  }
  if (outer_hash == (int16_t *)0) {
    display_assert("vertex_subdivision_indices",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x94, 1);
    system_exit(-1);
  }

  subdivision = *(int16_t *)sphere;
  vertex_count = *(int16_t *)((char *)sphere + 0xc);
  triangle_count = *(int16_t *)((char *)sphere + 0xe);
  index_array = *(int16_t **)((char *)sphere + 8);

  if (v1 < 0 || v1 >= vertex_count) {
    display_assert("v1 >= 0 && v1 < sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x95, 1);
    system_exit(-1);
  }
  if (v2 < 0 || v2 >= vertex_count) {
    display_assert("v2 >= 0 && v2 < sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x96, 1);
    system_exit(-1);
  }
  if (v3 < 0 || v3 >= vertex_count) {
    display_assert("v3 >= 0 && v3 < sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x97, 1);
    system_exit(-1);
  }
  if (*triangle_strip_vertex_indices_index_ptr >= triangle_count * 4) {
    display_assert(
      "*triangle_strip_vertex_indices_index < (NUMBER_OF_VERTICES_PER_TRIANGLE "
      "+ 1) * sphere->triangle_count",
      "c:\\halo\\SOURCE\\math\\geometry.c", 0x98, 1);
    system_exit(-1);
  }

  row_cache_count = (subdivision + 1) * (subdivision + 1);
  row_cache = (int16_t *)debug_malloc(
    row_cache_count * 2, 0, "c:\\halo\\SOURCE\\math\\geometry.c", 0x9a);
  if (row_cache != (int16_t *)0) {
    for (row = 0; row < row_cache_count; row++) {
      row_cache[row] = -1;
    }

    if (subdivision >= 1) {
      marker = 3;
      for (row = 1; row <= subdivision; row++) {
        strip_index = *triangle_strip_vertex_indices_index_ptr;
        index_array[strip_index] = (int16_t)marker;
        *triangle_strip_vertex_indices_index_ptr = (int16_t)(strip_index + 1);
        *(int16_t *)((char *)sphere + 0x10) =
          (int16_t)(*(int16_t *)((char *)sphere + 0x10) + 1);

        if (marker >= 3) {
          for (col = 1; col <= row; col++) {
            top_vertex = get_face_vertex((int16_t)(row - 1), sphere, v1, v2, v3,
                                         (int16_t)(col - 1), vertex_index_ptr,
                                         outer_hash, row_cache);
            left_vertex = get_face_vertex((int16_t)row, sphere, v1, v2, v3,
                                          (int16_t)(col - 1), vertex_index_ptr,
                                          outer_hash, row_cache);
            right_vertex =
              get_face_vertex((int16_t)row, sphere, v1, v2, v3, (int16_t)col,
                              vertex_index_ptr, outer_hash, row_cache);

            if (top_vertex < 0 || top_vertex > vertex_count) {
              display_assert(
                "top_vertex >= 0 && top_vertex <= sphere->vertex_count",
                "c:\\halo\\SOURCE\\math\\geometry.c", 0xb0, 1);
              system_exit(-1);
            }
            if (left_vertex < 0 || left_vertex > vertex_count) {
              display_assert(
                "left_vertex >= 0 && left_vertex <= sphere->vertex_count",
                "c:\\halo\\SOURCE\\math\\geometry.c", 0xb1, 1);
              system_exit(-1);
            }
            if (right_vertex < 0 || right_vertex > vertex_count) {
              display_assert(
                "right_vertex >= 0 && right_vertex <= sphere->vertex_count",
                "c:\\halo\\SOURCE\\math\\geometry.c", 0xb2, 1);
              system_exit(-1);
            }

            if (col == 1) {
              strip_index = *triangle_strip_vertex_indices_index_ptr;
              index_array[strip_index] = left_vertex;
              *triangle_strip_vertex_indices_index_ptr =
                (int16_t)(strip_index + 1);

              strip_index = *triangle_strip_vertex_indices_index_ptr;
              index_array[strip_index] = top_vertex;
              *triangle_strip_vertex_indices_index_ptr =
                (int16_t)(strip_index + 1);
            }

            strip_index = *triangle_strip_vertex_indices_index_ptr;
            index_array[strip_index] = right_vertex;
            *triangle_strip_vertex_indices_index_ptr =
              (int16_t)(strip_index + 1);

            if (col < row) {
              topright_vertex = get_face_vertex(
                (int16_t)(row - 1), sphere, v1, v2, v3, (int16_t)col,
                vertex_index_ptr, outer_hash, row_cache);
              if (topright_vertex < 0 || topright_vertex > vertex_count) {
                display_assert("topright_vertex >= 0 && topright_vertex <= "
                               "sphere->vertex_count",
                               "c:\\halo\\SOURCE\\math\\geometry.c", 0xc2, 1);
                system_exit(-1);
              }
              strip_index = *triangle_strip_vertex_indices_index_ptr;
              index_array[strip_index] = topright_vertex;
              *triangle_strip_vertex_indices_index_ptr =
                (int16_t)(strip_index + 1);
            }
          }
        }
        marker += 2;
      }
    }

    debug_free(row_cache, "c:\\halo\\SOURCE\\math\\geometry.c", 0xc8);
  }
}

/* Allocate and build a geosphere of the given subdivision `type` (0x1087b0).
 * sphere layout (0x14 bytes, raw offsets):
 *   +0x0 subdivision (int16_t), +0x4 vertex_array (float*, stride 3),
 *   +0x8 index_array (int16_t*), +0xc vertex_count (int16_t),
 *   +0xe triangle_count (int16_t), +0x10 field_10 (int16_t, incremented by
 *   subdivide_triangle, purpose unconfirmed).
 * vertex_count = ((type-2)*(type-1)*8)/2 + type*12 - 6 (equals 4*type^2 + 2).
 * triangle_count = type*type*8 (equals 8*type^2).
 * outer_hash is a fixed 64-entry int16_t scratch buffer, freed before
 * return in all cases. On success (all three mallocs and both post-loop
 * invariant checks pass) only outer_hash is freed; vertex_array/index_array
 * are the real output. If any of vertex_array/index_array/outer_hash fails
 * to allocate, the whole build is skipped and whichever of
 * vertex_array/index_array/outer_hash succeeded is freed; the struct
 * pointer is still returned with no fields nulled (dangling array
 * pointers), matching the original exactly. The two post-loop asserts are
 * unconditional system_exit(-1) halts (never return) — they are not part of
 * the failure/free path. */
void *FUN_001087b0(int type)
{
  void *result;
  int16_t triangle_count;
  int16_t vertex_count;
  float *vertex_array;
  int16_t *index_array;
  int16_t *outer_hash;
  int16_t vertex_index;
  int16_t triangle_strip_vertex_indices_index;
  int i;

  result = debug_malloc(0x14, 0, "c:\\halo\\SOURCE\\math\\geometry.c", 0x3a);
  if (result == (void *)0) {
    return result;
  }

  triangle_count = (int16_t)(type * type * 8);
  *(int16_t *)((char *)result + 0xe) = triangle_count;
  *(int16_t *)result = (int16_t)type;
  vertex_count =
    (int16_t)((((type - 2) * (type - 1) * 8) / 2) + (type * 12) - 6);
  *(int16_t *)((char *)result + 0xc) = vertex_count;

  vertex_array = (float *)debug_malloc(
    vertex_count * 12, 0, "c:\\halo\\SOURCE\\math\\geometry.c", 0x42);
  *(float **)((char *)result + 4) = vertex_array;

  index_array = (int16_t *)debug_malloc(
    triangle_count * 8, 0, "c:\\halo\\SOURCE\\math\\geometry.c", 0x43);
  *(int16_t **)((char *)result + 8) = index_array;

  *(int16_t *)((char *)result + 0x10) = 0;

  outer_hash = (int16_t *)debug_malloc(
    0x80, 0, "c:\\halo\\SOURCE\\math\\geometry.c", 0x45);

  if (vertex_array != (float *)0 && index_array != (int16_t *)0 &&
      outer_hash != (int16_t *)0) {
    for (i = 0; i < 64; i++) {
      outer_hash[i] = -1;
    }

    for (i = 0; i < 6; i++) {
      vertex_array[i * 3 + 0] = ((float *)0x28bd88)[i * 3 + 0];
      vertex_array[i * 3 + 1] = ((float *)0x28bd88)[i * 3 + 1];
      vertex_array[i * 3 + 2] = ((float *)0x28bd88)[i * 3 + 2];
    }

    vertex_index = 6;
    triangle_strip_vertex_indices_index = 0;

    for (i = 0; i < 8; i++) {
      subdivide_triangle(result, ((int16_t *)0x28bdd0)[i * 3 + 0],
                         ((int16_t *)0x28bdd0)[i * 3 + 1],
                         ((int16_t *)0x28bdd0)[i * 3 + 2], &vertex_index,
                         &triangle_strip_vertex_indices_index, outer_hash);
    }

    if (triangle_strip_vertex_indices_index >= triangle_count * 4) {
      display_assert(
        "triangle_strip_vertex_indices_index < "
        "(NUMBER_OF_VERTICES_PER_TRIANGLE + 1) * sphere->triangle_count",
        "c:\\halo\\SOURCE\\math\\geometry.c", 0x62, 1);
      system_exit(-1);
    }
    if (vertex_index != vertex_count) {
      display_assert("vertex_index == result->vertex_count",
                     "c:\\halo\\SOURCE\\math\\geometry.c", 0x63, 1);
      system_exit(-1);
    }

    /* Success: only the scratch outer_hash buffer is freed; vertex_array
     * and index_array survive as the real output. */
    debug_free(outer_hash, "c:\\halo\\SOURCE\\math\\geometry.c", 0x6b);
    return result;
  }

  /* One or more allocations failed: free whichever of vertex_array/
   * index_array succeeded, then always attempt to free outer_hash. No
   * field is nulled afterward; the caller receives the struct pointer with
   * dangling array pointers on this path, matching the original exactly. */
  if (vertex_array != (float *)0) {
    debug_free(vertex_array, "c:\\halo\\SOURCE\\math\\geometry.c", 0x67);
  }
  if (index_array != (int16_t *)0) {
    debug_free(index_array, "c:\\halo\\SOURCE\\math\\geometry.c", 0x68);
  }
  if (outer_hash != (int16_t *)0) {
    debug_free(outer_hash, "c:\\halo\\SOURCE\\math\\geometry.c", 0x6b);
  }
  return result;
}

/* Store a 2D rectangle (0x1089a0).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Store order follows the binary: left, top, right, bottom. */
void set_rectangle2d(int16_t *rect, int16_t left, int16_t top, int16_t right,
                     int16_t bottom)
{
  rect[1] = left;
  rect[0] = top;
  rect[3] = right;
  rect[2] = bottom;
}

/* Store a 2D point (0x1089d0).
 * point layout: {x, y} as int16_t[2]. */
void set_point2d(int16_t *point, int16_t x, int16_t y)
{
  point[0] = x;
  point[1] = y;
}

/* Offset a 2D point by (dx, dy) (0x1089f0).
 * point layout: {x, y} as int16_t[2]. */
void offset_point2d(int16_t *point, int16_t dx, int16_t dy)
{
  point[0] += dx;
  point[1] += dy;
}

/* Width of a 2D rectangle (0x108a10).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary reads `right` zero-extended (xor eax,eax; mov ax,[ecx+6]) and
 * `left` sign-extended (movsx ecx,[ecx+2]); both extensions preserved. */
int rect2d_width(const int16_t *rect)
{
  return (int)(uint16_t)rect[3] - (int)rect[1];
}

/* Height of a 2D rectangle (0x108a30).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary reads `bottom` zero-extended (xor eax,eax; mov ax,[ecx+4]) and
 * `top` sign-extended (movsx ecx,[ecx]); both extensions preserved. */
int rect2d_height(const int16_t *rect)
{
  return (int)(uint16_t)rect[2] - (int)rect[0];
}

/* Inset a 2D rectangle by (dx, dy) (0x108a50).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Store order follows the binary: left, right, top, bottom. */
void inset_rectangle2d(int16_t *rect, int16_t dx, int16_t dy)
{
  rect[1] += dx;
  rect[3] -= dx;
  rect[0] += dy;
  rect[2] -= dy;
}

/* Offset a 2D rectangle by (dx, dy) (0x108a70).
 * rect layout: {top, left, bottom, right} as int16_t[4]. */
void rect2d_offset(int16_t *rect, int16_t dx, int16_t dy)
{
  rect[1] += dx;
  rect[3] += dx;
  rect[0] += dy;
  rect[2] += dy;
}

/* Position rectangle1 relative to rectangle0 and write the moved copy of
 * rectangle1 to result (0x108a90).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * mode 0: centered on both axes; mode 1: 1/3 down vertically, centered
 * horizontally. Other modes: non-halting assert (integer_math.c line 0xad),
 * result = unmodified copy of rectangle1.
 * Extent differences are 16-bit subtractions; the offset arithmetic is done
 * in 32 bits with top/left read zero-extended, and only the low 16 bits reach
 * the result. Add order follows the binary: top, right, left, bottom. */
void adjust_rectangle2d(const int16_t *rectangle0, const int16_t *rectangle1,
                        int16_t *result, int16_t mode)
{
  int16_t width0;
  int16_t height0;
  int16_t width1;
  int16_t height1;
  int top0;
  int left0;
  int top1;
  int left1;
  int dx;
  int dy;
  int16_t adjusted[4];

  left0 = (uint16_t)rectangle0[1];
  top0 = (uint16_t)rectangle0[0];
  width0 = rectangle0[3] - left0;
  height0 = rectangle0[2] - top0;
  top1 = (uint16_t)rectangle1[0];
  left1 = (uint16_t)rectangle1[1];
  height1 = rectangle1[2] - top1;
  width1 = rectangle1[3] - left1;
  ((uint32_t *)adjusted)[0] = ((const uint32_t *)rectangle1)[0] & 0xFFFFFFFFu;
  ((uint32_t *)adjusted)[1] = ((const uint32_t *)rectangle1)[1];

  switch (mode) {
  case 0:
    dy = height0 / 2 - height1 / 2 - top1 + top0;
    dx = width0 / 2 - width1 / 2;
    break;
  case 1:
    dy = (height0 - height1) / 3 - top1 + top0;
    dx = (width0 - width1) / 2;
    break;
  default:
    display_assert(csprintf((char *)0x5ab100,
                            "adjust_rectangle2d() can't handle mode #%d",
                            (int)mode),
                   "c:\\halo\\SOURCE\\math\\integer_math.c", 0xad, 0);
    goto store;
  }
  dx = dx - left1 + left0;
  adjusted[0] += (int16_t)dy;
  adjusted[3] += (int16_t)dx;
  adjusted[1] += (int16_t)dx;
  adjusted[2] += (int16_t)dy;

store:
  ((uint32_t *)result)[0] = ((uint32_t *)adjusted)[0];
  ((uint32_t *)result)[1] = ((uint32_t *)adjusted)[1];
}

/* Intersect two 2D rectangles (0x108bc0).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * left/top = max, right/bottom = min; all signed 16-bit compares.
 * Compute order follows the binary: left, right, top, bottom.
 * A non-empty intersection (left < right && top < bottom) is copied to
 * `intersection` as two dwords and AL=1 is returned; otherwise
 * `intersection` is zeroed with csmemset(intersection, 0, 8) and AL=0. */
bool intersect_rectangles2d(short *rectangle0, short *rectangle1,
                            short *intersection)
{
  int16_t left;
  int16_t result[4];

  left = (rectangle0[1] > rectangle1[1]) ? rectangle0[1] : rectangle1[1];
  result[1] = left;
  result[3] = (rectangle0[3] > rectangle1[3]) ? rectangle1[3] : rectangle0[3];
  result[0] = (rectangle0[0] > rectangle1[0]) ? rectangle0[0] : rectangle1[0];
  result[2] = (rectangle0[2] > rectangle1[2]) ? rectangle1[2] : rectangle0[2];
  if (left < result[3] && result[0] < result[2]) {
    ((uint32_t *)intersection)[0] = ((uint32_t *)result)[0];
    ((uint32_t *)intersection)[1] = ((uint32_t *)result)[1];
    return 1;
  }
  csmemset(intersection, 0, 8);
  return 0;
}

/* Compute the bounding-rectangle union ("hull") of two 2D rectangles
 * (0x108c60).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * hull.top/left = min(a,b); hull.bottom/right = max(a,b); all signed 16-bit
 * compares (JG/JLE/CMP on 16-bit regs). Store order follows the binary:
 * left, right, top, bottom. */
void rectangle2d_hull_from_rectangles2d(const int16_t *rect_a,
                                        const int16_t *rect_b, int16_t *hull)
{
  hull[1] = (rect_a[1] <= rect_b[1]) ? rect_a[1] : rect_b[1];
  hull[3] = (rect_a[3] > rect_b[3]) ? rect_a[3] : rect_b[3];
  hull[0] = (rect_a[0] <= rect_b[0]) ? rect_a[0] : rect_b[0];
  hull[2] = (rect_a[2] > rect_b[2]) ? rect_a[2] : rect_b[2];
}

/* Test whether a 2D point lies inside a 2D rectangle (0x108cd0).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * point layout: {x, y} as int16_t[2].
 * Binary compares point[0] against left/right and point[1] against
 * top/bottom, all as signed 16-bit compares (JL/JGE); the range is
 * half-open at right/bottom. Result is returned in AL. */
boolean point2d_in_rectangle2d(const int16_t *rect, const int16_t *point)
{
  return (boolean)(point[0] >= rect[1] && point[0] < rect[3] &&
                   point[1] >= rect[0] && point[1] < rect[2]);
}

/* Test whether `interior` lies entirely inside `rect` (0x108d00).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary compare order is left, right, top, bottom; all signed 16-bit
 * (JL/JG), and the bounds are inclusive on every edge.
 * Returns a full 32-bit 0/1 in EAX (mov eax,1 / xor eax,eax), not a byte
 * boolean, so the return type is int32_t. */
int32_t interior_rectangle2d(const int16_t *rect, const int16_t *interior)
{
  return (int32_t)(interior[1] >= rect[1] && interior[3] <= rect[3] &&
                   interior[0] >= rect[0] && interior[2] <= rect[2]);
}

/* Test whether two 2D rectangles are identical (0x108d40).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary compare order is left, right, top, bottom, all 16-bit equality
 * compares short-circuiting to the common failure exit.
 * Returns a full 32-bit 0/1 in EAX (mov eax,1 / xor eax,eax), not a byte
 * boolean, so the return type is int32_t. */
int32_t equal_rectangle2d(const int16_t *rect_a, const int16_t *rect_b)
{
  return (int32_t)(rect_a[1] == rect_b[1] && rect_a[3] == rect_b[3] &&
                   rect_a[0] == rect_b[0] && rect_a[2] == rect_b[2]);
}

/* Test whether two 2D points are identical (0x108d80).
 * point layout: {x, y} as int16_t[2].
 * Binary compares x then y as 16-bit equality compares, both short-circuiting
 * to the common failure exit.
 * Returns a full 32-bit 0/1 in EAX (mov eax,1 / xor eax,eax), not a byte
 * boolean, so the return type is int32_t. */
int32_t equal_point2d(const int16_t *point_a, const int16_t *point_b)
{
  return (int32_t)(point_a[0] == point_b[0] && point_a[1] == point_b[1]);
}

/* Compute floor(log2(value)) (0x108db0).
 * Returns 0 for value <= 1. */
int16_t FUN_00108db0(unsigned int value)
{
  int result = 0;
  if (value > 0) {
    while (value != 1) {
      value >>= 1;
      result++;
    }
  }
  return (int16_t)result;
}

/* Compute a shift-count-style ceiling(log2(value)) (0x108dd0). No callers in
 * this binary (xrefs_to empty).
 * Binary shape (verbatim from disassembly, not the (buggy, ecx-dropping)
 * Ghidra decompile): ecx=0; if (value==0) skip straight to `return ecx+1`
 * (i.e. returns 1 for value==0). Otherwise eax=value-1; if eax==1 (value==2)
 * skip the loop (returns 1). Otherwise loop: eax>>=1; ecx++; while(eax!=1).
 * NOTE: value==1 makes eax=0 after the decrement, so `eax>>=1` stays 0
 * forever and the loop never reaches eax==1 -- this mirrors an infinite
 * loop present in the original binary for that input and is preserved
 * as-is; not fixed here. */
int32_t ceiling_log2(uint32_t value)
{
  uint32_t eax;
  int32_t ecx = 0;

  if (value != 0) {
    eax = value - 1;
    if (eax != 1) {
      do {
        eax >>= 1;
        ecx++;
      } while (eax != 1);
    }
  }
  return ecx + 1;
}

/* Compute the largest power of 2 <= value (0x108df0). Sibling of
 * ceiling_log2 at 0x108dd0; no callers in this binary (xrefs_to empty).
 * Binary shape (from disassembly): param is read via movzx word ptr
 * [ebp+8] (single ushort arg). eax=1 is set unconditionally before the
 * compare; if value<2, that 1 is returned untouched (JL skips the loop).
 * Otherwise ecx=2, then loop: eax=ecx; ecx=eax+eax; while(ecx<=value)
 * repeat. Both eax and ecx stay full 32-bit registers throughout (no
 * truncation on return), preserved as int32_t/uint32_t locals. */
int32_t floor_power2(uint16_t value)
{
  int32_t edx = (int32_t)value;
  int32_t eax = 1;
  int32_t ecx;

  if (edx >= 2) {
    ecx = 2;
    do {
      eax = ecx;
      ecx = eax * 2;
    } while (ecx <= edx);
  }
  return eax;
}

/* Compute the smallest power of 2 >= value (0x108e20). Sibling of
 * floor_power2 at 0x108df0. Binary shape (from disassembly): param is read
 * via movzx word ptr [ebp+8] (single ushort arg). ecx = value; eax = 1
 * unconditionally; if value <= 1 (JLE), that 1 is returned untouched.
 * Otherwise loop: eax <<= 1; while (eax < value) repeat. */
int32_t ceiling_power2(uint16_t value)
{
  int32_t ecx = (int32_t)value;
  int32_t eax = 1;

  if (ecx > 1) {
    do {
      eax <<= 1;
    } while (eax < ecx);
  }
  return eax;
}

/* Integer square root, digit-by-digit (0x108e40). Sibling of ceiling_power2
 * at 0x108e20; no callers found in xrefs_to scan (local index gap, not
 * evidence of dead code). Ghidra's decompile drops the return value and the
 * final rounding step, so this is lifted verbatim from disassembly:
 *   esi=value (single uint arg, [ebp+8]); eax=result=0; edx=bit=0x40000000.
 *   loop: ecx=bit+result; if ecx<=esi: esi-=ecx, result(eax)=ecx+bit;
 *         bit>>=2; result>>=1; while(bit!=0).
 *   tail: if esi>result, result++.
 * Both bit and result stay full 32-bit registers throughout. */
uint32_t integer_square_root(uint32_t value)
{
  uint32_t result = 0;
  uint32_t bit = 0x40000000;
  uint32_t test;

  do {
    test = bit + result;
    if (test <= value) {
      value -= test;
      result = test + bit;
    }
    bit >>= 2;
    result >>= 1;
  } while (bit != 0);

  if (value > result) {
    result++;
  }

  return result;
}
