/* collision_bsp.obj (physics/collision_bsp.c)
 *
 * Declarations for tag_block_get_element, display_assert and system_exit come
 * from the generated decl.h via kb.json.
 */

/* 0x1473b0 - collision_surface_edge_count
 *
 * Counts the edges around one collision-BSP surface by walking its circular
 * edge loop. Same winged-edge traversal as collision_surface_polygon but
 * without gathering geometry.
 *
 * bsp base holds tag_block headers at fixed offsets:
 *   +0x3c surfaces (stride 0xc): surface[+4] = first-edge index
 *   +0x48 edges    (stride 0x18): edge[+0x14] = owning-surface index,
 *                                 edge[+8]/edge[+0xc] = the two half-edge
 *                                 next-edge links
 *
 * `side` = (edge[+0x14] == surface_index) selects this surface's half-edge
 * slot: next-edge index at edge[+8 | +0xc]. The do-while increments the count
 * once per edge and terminates when the next-edge index returns to the
 * surface's first-edge index. The original returns the count in AX only (high
 * half of EAX is leftover garbage from the terminator index), so the faithful
 * return type is short.
 */
short collision_surface_edge_count(int bsp, int surface_index)
{
  short edge_count;
  int first_edge;
  int edge_index;
  int *edge;

  edge_count = 0;
  first_edge = *(int *)((char *)tag_block_get_element((void *)(bsp + 0x3c),
                                                      surface_index, 0xc) +
                        4);
  edge_index = first_edge;
  do {
    edge = (int *)tag_block_get_element((void *)(bsp + 0x48), edge_index, 0x18);
    edge_count = (short)(edge_count + 1);
    edge_index = edge[2 + (edge[5] == surface_index)];
  } while (edge_index != first_edge);
  return edge_count;
}

/* 0x147410 - collision_surface_polygon
 *
 * Walks a collision-BSP surface's circular edge loop and gathers the surface's
 * polygon vertices (xyz float triples, stride 0xc) into out_points. Returns the
 * 16-bit vertex count.
 *
 * bsp base holds three tag_block headers at fixed offsets:
 *   +0x3c surfaces (stride 0xc): surface[+4] = first-edge index
 *   +0x48 edges    (stride 0x18)
 *   +0x54 vertices (stride 0x10): first 0xc bytes = xyz float32
 *
 * Winged-edge orientation: `side` = (edge[+0x14] == surface_index). The edge
 * stores its two half-edge slots interleaved; `side` selects this surface's
 * slot -> vertex index at edge[+0 | +4], next-edge index at edge[+8 | +0xc].
 *
 * The loop is do-while (the first vertex is always copied); it terminates when
 * the next-edge index returns to the surface's first-edge index. The original
 * returns the count in AX only (high half of EAX is leftover garbage from the
 * terminator index), so the faithful return type is short.
 */
short collision_surface_polygon(int bsp, int surface_index, void *out_points)
{
  short point_count;
  int first_edge;
  int edge_index;
  int *edge;
  unsigned int *vertex;
  unsigned int *out;
  int side;

  point_count = 0;
  first_edge = *(int *)((char *)tag_block_get_element((void *)(bsp + 0x3c),
                                                      surface_index, 0xc) +
                        4);
  edge_index = first_edge;
  do {
    edge = (int *)tag_block_get_element((void *)(bsp + 0x48), edge_index, 0x18);
    side = (edge[5] == surface_index);
    vertex = (unsigned int *)tag_block_get_element((void *)(bsp + 0x54),
                                                   edge[side], 0x10);
    if (point_count > 7) {
      display_assert("point_count<MAXIMUM_VERTICES_PER_COLLISION_SURFACE",
                     "c:\\halo\\SOURCE\\physics\\collision_bsp.c", 0xe1, 1);
      system_exit(-1);
    }
    out = (unsigned int *)((char *)out_points + point_count * 0xc);
    out[0] = vertex[0];
    out[1] = vertex[1];
    out[2] = vertex[2];
    edge_index = edge[2 + side];
    point_count = (short)(point_count + 1);
  } while (edge_index != first_edge);
  return point_count;
}

/* render_debug.c -- debug primitive rendering
 * (c:\halo\SOURCE\render\render_debug.c)
 *
 * The debug renderer keeps a per-frame cache of debug primitives (points,
 * lines, boxes, text, ...) submitted through the cache writer at 0x188ec0.
 * Each cache record is 0x38 (56) bytes; the leading short is the primitive
 * type. render_debug (0x18ac50) flushes the cache once per frame: it runs the
 * fixed set of debug sub-renderers, walks the cache dispatching each record to
 * its draw routine, then clears the cache when the game frame advances.
 */

#include "x87_math.h"

/* Per-frame debug primitive cache record (0x38 bytes, array based at 0x4d1220).
 * The payload is a tagged union keyed by `type`; individual offsets are reused
 * per primitive kind, so the fields carry raw-offset names. */
typedef struct debug_primitive {
  short type; /* +0x00 primitive type (0..9)          */
  short pad02; /* +0x02                                */
  float f04; /* +0x04                                */
  float f08; /* +0x08                                */
  float f0c; /* +0x0c                                */
  float f10; /* +0x10                                */
  unsigned short s14; /* +0x14                                */
  unsigned char b16; /* +0x16                                */
  unsigned char pad17; /* +0x17                                */
  float f18; /* +0x18                                */
  float f1c; /* +0x1c                                */
  float f20; /* +0x20                                */
  float f24; /* +0x24                                */
  float f28; /* +0x28                                */
  float f2c; /* +0x2c                                */
  float f30; /* +0x30                                */
  float f34; /* +0x34                                */
} debug_primitive; /* sizeof == 0x38 */

typedef char
  debug_primitive_size_check[sizeof(debug_primitive) == 0x38 ? 1 : -1];

#define debug_primitives ((debug_primitive *)0x4d1220)
#define debug_primitive_count (*(short *)0x4d8224)
#define debug_primitive_frame (*(short *)0x4d8220)

/* Per-frame debug string arena: char[0x400] at 0x4d0e20, ending at 0x4d121f
 * (immediately before the primitive cache at 0x4d1220). Text primitives intern
 * their string here; the cursor at 0x4d8228 counts bytes used (max 0x3ff). */
#define debug_string_pool ((char *)0x4d0e20)
#define debug_string_pool_count (*(short *)0x4d8228)
#define debug_string_overflow_warned (*(char *)0x4d822b)

/* Draw one collision-BSP vertex as a debug point (0x147520, collision_bsp.obj).
 * Fetches collision vertex `vertex_index` (0x10-byte record) from the tag_block
 * at bsp+0x54, optionally transforms it through `matrix` into a local scratch
 * point, then submits it to the cached debug-point drawer (0x189150) with the
 * given scale and color. When matrix is NULL the raw vertex position is drawn
 * directly. */
void render_debug_collision_vertex(int bsp, int vertex_index, float *matrix,
                                   float scale, void *color)
{
  float *point;
  float transformed[3];

  point =
    (float *)tag_block_get_element((void *)(bsp + 0x54), vertex_index, 0x10);
  if (matrix != 0) {
    matrix_transform_point(matrix, point, transformed);
    point = transformed;
  }
  FUN_00189150(1, point, scale, color);
}

/* 0x147570 - render_debug_collision_edge
 *
 * Draws one collision-BSP edge as a debug line. The edge tag_block lives at
 * bsp+0x48 (stride 0x18); the first two dwords of an edge element are its two
 * endpoint vertex indices (v0, v1). Vertices live at bsp+0x54 (stride 0x10),
 * xyz float32 in the leading 0xc bytes.
 *
 * When matrix_or_flag is non-NULL it is a transform matrix pointer: each
 * endpoint is passed through matrix_transform_point into a local vec3 scratch
 * and the transformed points are drawn. When NULL the raw tag_block vertex
 * pointers are drawn directly. The debug color pointer is forwarded unchanged.
 *
 * Endpoint order is not swapped: point_a = vertex[edge v0], point_b =
 * vertex[edge v1]; the draw call is (flag=1, point_a, point_b, color).
 * matrix_transform_point/FUN_00189270 return void in kb.json, so the
 * transformed points are read from the scratch buffers, not a returned ptr.
 */
void render_debug_collision_edge(int bsp, int edge_index, int matrix_or_flag,
                                 void *color)
{
  int *edge;
  float *point_a;
  float *point_b;
  float xformed_a[3];
  float xformed_b[3];

  edge = (int *)tag_block_get_element((void *)(bsp + 0x48), edge_index, 0x18);
  point_a = (float *)tag_block_get_element((void *)(bsp + 0x54), edge[0], 0x10);
  point_b = (float *)tag_block_get_element((void *)(bsp + 0x54), edge[1], 0x10);
  if (matrix_or_flag != 0) {
    matrix_transform_point((float *)matrix_or_flag, point_a, xformed_a);
    matrix_transform_point((float *)matrix_or_flag, point_b, xformed_b);
    point_a = xformed_a;
    point_b = xformed_b;
  }
  FUN_00189270(1, point_a, point_b, color);
}

/* 0x1475f0 - render_debug_collision_surface
 *
 * Walks the circular doubly-linked edge list of one collision-BSP surface and
 * renders each bounding edge via render_debug_collision_edge.
 *
 * bsp+0x3c = surfaces tag_block (stride 0xc); surface element field +4 is the
 * index of the surface's first bounding edge. bsp+0x48 = edges tag_block
 * (stride 0x18). Per edge: field +0x14 is the edge's "side A" (left) surface
 * reference; fields +8 and +0xc are the two edge links.
 *
 * Link selection: if the edge's +0x14 surface equals this surface_index (we own
 * the edge on side A) advance via the +0xc link, otherwise via the +8 link.
 * Encoded exactly as the original: base +8 plus (cond)*4 as a byte offset; kept
 * verbatim because the (cond)*4 codegen matters for VC71 match.
 *
 * Terminator: do-while until the walk wraps back to the first edge. param_3
 * (matrix-or-flag) and param_4 (debug color) are forwarded to the edge draw
 * unchanged.
 */
void render_debug_collision_surface(int bsp, int surface_index,
                                    int matrix_or_flag, void *color)
{
  int surface;
  int edge;
  int first_edge;
  int edge_index;
  int left_surface;

  surface =
    (int)tag_block_get_element((void *)(bsp + 0x3c), surface_index, 0xc);
  first_edge = *(int *)(surface + 4);
  edge_index = first_edge;
  do {
    edge = (int)tag_block_get_element((void *)(bsp + 0x48), edge_index, 0x18);
    left_surface = *(int *)(edge + 0x14);
    render_debug_collision_edge(bsp, edge_index, matrix_or_flag, color);
    edge_index =
      *(int *)(edge + 8 + (unsigned int)(left_surface == surface_index) * 4);
  } while (edge_index != first_edge);
}

/* 0x147660 - render_debug_collision_bsp
 *
 * Draws every edge of a collision BSP for debug visualization. The edge
 * tag_block header lives at bsp+0x48; its element count (bsp+0x48+0 first
 * dword) is the loop bound. Each edge is rendered by
 * render_debug_collision_edge, with param_2 forwarded unchanged
 * (transform-matrix pointer or flag) and the debug color pointer read from the
 * global at 0x2ee6d4.
 *
 * The original is a do-while guarded by an outer `count > 0` test, which is the
 * canonical MSVC codegen for this for-loop.
 */
void render_debug_collision_bsp(int bsp, int matrix_or_flag)
{
  int i;

  for (i = 0; i < *(int *)(bsp + 0x48); i++) {
    render_debug_collision_edge(bsp, i, matrix_or_flag, *(void **)0x2ee6d4);
  }
}

/* 0x1476a0 - collision_edge_length
 *
 * Returns the 3D Euclidean length of a collision-BSP edge. The edge element
 * (bsp+0x48, stride 0x18) holds its two endpoint vertex indices in the first
 * two dwords. Each vertex (bsp+0x54, stride 0x10) begins with an xyz float32
 * triple. Result = sqrt(dx^2 + dy^2 + dz^2).
 *
 * The original x87 codegen (float10/FSQRT) loads the three component
 * differences in x, y, z order (offsets 0, 4, 8), each taken as
 * vertex_b - vertex_a, squares them, and sums; kept inline in that order for
 * VC71 match (confirmed against the delinked reference: flds 0/4/8).
 */
float collision_edge_length(int bsp, int edge_index)
{
  unsigned int *edge;
  float *vertex_a;
  float *vertex_b;

  edge = (unsigned int *)tag_block_get_element((void *)(bsp + 0x48), edge_index,
                                               0x18);
  vertex_a =
    (float *)tag_block_get_element((void *)(bsp + 0x54), edge[0], 0x10);
  vertex_b =
    (float *)tag_block_get_element((void *)(bsp + 0x54), edge[1], 0x10);
  return sqrtf((vertex_b[0] - vertex_a[0]) * (vertex_b[0] - vertex_a[0]) +
               (vertex_b[1] - vertex_a[1]) * (vertex_b[1] - vertex_a[1]) +
               (vertex_b[2] - vertex_a[2]) * (vertex_b[2] - vertex_a[2]));
}

/* 0x147710 - collision_surface_perimeter
 *
 * Sums the edge lengths around a collision-BSP surface's winged-edge loop and
 * returns the total perimeter (float in ST0).
 *
 * Layout mirrors collision_surface_polygon (same three tag_block headers on the
 * bsp base): +0x3c surfaces (stride 0xc, surface[+4] = first-edge index),
 * +0x48 edges (stride 0x18), +0x54 vertices (stride 0x10, xyz float32 at +0).
 *
 * Winged-edge orientation: side = (edge[+0x14] == surface_index). side selects
 * this surface's half-edge slot -> vertex_a at edge[side], vertex_b at
 * edge[!side], next-edge index at edge[2 + side] (byte +0x8 | +0xc). The loop
 * is do-while (at least one edge) and terminates when the next-edge index
 * returns to the surface's first-edge index.
 *
 * The per-edge SQRT sums the squared component differences (vertex_b -
 * vertex_a) in y, z, x order (edge indices [1], [2], [0]) to match the original
 * x87 FADD scheduling; the running total is accumulated as sqrtf(...) + total.
 */
float collision_surface_perimeter(int bsp, int surface_index)
{
  int *edge;
  float *vertex_a;
  float *vertex_b;
  int first_edge;
  int edge_index;
  unsigned char side;
  float total;
  float dx, dy, dz;

  total = 0.0f;
  first_edge = *(int *)((char *)tag_block_get_element((void *)(bsp + 0x3c),
                                                      surface_index, 0xc) +
                        4);
  edge_index = first_edge;
  do {
    edge = (int *)tag_block_get_element((void *)(bsp + 0x48), edge_index, 0x18);
    side = (edge[5] == surface_index);
    vertex_a =
      (float *)tag_block_get_element((void *)(bsp + 0x54), edge[side], 0x10);
    vertex_b =
      (float *)tag_block_get_element((void *)(bsp + 0x54), edge[!side], 0x10);
    dy = vertex_b[1] - vertex_a[1];
    dz = vertex_b[2] - vertex_a[2];
    dx = vertex_b[0] - vertex_a[0];
    total = sqrtf(dy * dy + dz * dz + dx * dx) + total;
    edge_index = edge[2 + side];
  } while (edge_index != first_edge);
  return total;
}

/* 0x1477f0 - collision_surface_area
 *
 * Computes the signed projected area of a collision-BSP surface polygon by
 * fan-triangulating its edge loop from a fixed anchor vertex and summing
 * dot(cross(e0, e1), plane_normal) over each triangle, where the plane normal
 * comes from the surface's plane designator. Returns the accumulated area if
 * positive, else 0.0f (both the >0 branch and the fall-through return 0.0f).
 *
 * bsp base tag_block headers (see collision_surface_polygon):
 *   +0x3c surfaces (stride 0xc): surface[0] = plane designator,
 *                                surface[+4] = first-edge index
 *   +0x48 edges    (stride 0x18): +0x14 = owning surface index;
 *                                 +0/+4 = start/end vertex refs;
 *                                 +8/+0xc = next-edge refs (winged-edge slots)
 *   +0x54 vertices (stride 0x10): first 0xc bytes = xyz float32
 *
 * `side` = (edge[+0x14] == surface_index) selects this surface's half-edge
 * slot each iteration. The anchor vertex and plane normal are fetched once
 * before the loop; each iteration walks the two vertices of the current edge.
 *
 * The plane normal is written contiguously into plane[3] by
 * bsp3d_get_plane_from_designator (out_plane), so it reads back as
 * plane[0..2]. Cross-product and accumulation operand order preserved exactly
 * from the disassembly (x87 FLD/FMUL/FSUBP order); getting any subtraction
 * backwards negates the area.
 */
float collision_surface_area(int bsp, int surface_index)
{
  float plane[3];
  volatile float cross_x; /* volatile = store-once/reload-each-use; the
                             original spills exactly these four to stack
                             slots and keeps pa_xyz, qa_z, cross_z
                             ST-resident */
  volatile float cross_y;
  float cross_z;
  float pa_x, pa_y, pa_z;          /* edge[side] vertex - anchor */
  volatile float qa_x, qa_y;       /* edge[!side] vertex - anchor */
  float qa_z;
  float *anchor;
  float *v0;
  float *v1;
  int *surface;
  int edges_block;
  int verts_block;
  int edge;
  unsigned char side;
  unsigned char is_owner;
  float area;

  area = 0.0f;
  surface =
    (int *)tag_block_get_element((void *)(bsp + 0x3c), surface_index, 0xc);
  edges_block = bsp + 0x48;
  edge = (int)tag_block_get_element((void *)edges_block, surface[1], 0x18);
  side = (*(int *)(edge + 0x14) == surface_index);
  verts_block = bsp + 0x54;
  anchor = (float *)tag_block_get_element((void *)verts_block,
                                          *(int *)(edge + side * 4), 0x10);
  bsp3d_get_plane_from_designator(bsp, (unsigned int)surface[0], plane);
  edge = (int)tag_block_get_element((void *)edges_block,
                                    *(int *)(edge + 8 + side * 4), 0x18);
  is_owner = (*(int *)(edge + 0x14) == surface_index);
  side = is_owner;
  if (*(int *)(edge + 8 + side * 4) != surface[1]) {
    do {
      v1 = (float *)tag_block_get_element((void *)verts_block,
                                          *(int *)(edge + side * 4), 0x10);
      v0 = (float *)tag_block_get_element(
        (void *)verts_block, *(int *)(edge + (!is_owner) * 4), 0x10);
      pa_x = v1[0] - anchor[0];
      pa_y = v1[1] - anchor[1];
      pa_z = v1[2] - anchor[2];
      qa_x = v0[0] - anchor[0];
      qa_y = v0[1] - anchor[1];
      qa_z = v0[2] - anchor[2];
      cross_x = qa_z * pa_y - qa_y * pa_z;
      cross_y = pa_z * qa_x - qa_z * pa_x;
      cross_z = pa_x * qa_y - qa_x * pa_y;
      area = plane[2] * cross_z + plane[1] * cross_y + cross_x * plane[0] +
             area;
      edge = (int)tag_block_get_element((void *)edges_block,
                                        *(int *)(edge + 8 + side * 4), 0x18);
      is_owner = (*(int *)(edge + 0x14) == surface_index);
      side = is_owner;
    } while (*(int *)(edge + 8 + side * 4) != surface[1]);
    if (area > 0.0f) {
      return area;
    }
  }
  return 0.0f;
}

/* 0x147d10 - collision_surface_test_line2d
 *
 * Clips a 2D line (point + direction) against one collision-BSP surface's
 * bounding-edge loop, returning whether the line's entering and leaving
 * parameters bracket a non-empty interval (i.e. the line crosses the surface's
 * interior). Same tag_block geometry as collision_surface_polygon:
 *   bsp+0x3c surfaces (stride 0xc): surface[+4] = first-edge index
 *   bsp+0x48 edges    (stride 0x18)
 *   bsp+0x54 vertices (stride 0x10): first 0xc bytes = xyz float32
 *
 * Winged edge: `side` = (edge[5] == surface_index) tells which half-edge slot
 * belongs to this surface. Both endpoints (edge[0], edge[1]) are always read;
 * the next-edge link is edge[2 + side] and the neighbor-surface index across
 * the edge is edge[!side + 4] (i.e. edge[+0x10] or edge[+0x14]).
 *
 * out_result is a MIXED 6-dword record, NOT six floats:
 *   +0x00 float  enter_t   (max entering parameter; init -FLT_MAX)
 *   +0x04 int    enter_edge (edge index; init -1)
 *   +0x08 int    enter_surface (neighbor surface index; init -1)
 *   +0x0c float  leave_t   (min leaving parameter; init +FLT_MAX)
 *   +0x10 int    leave_edge (init -1)
 *   +0x14 int    leave_surface (init -1)
 * The four index slots are raw dword stores (the current edge index and the
 * neighbor-surface index). Ghidra prints the index stores as (float)..., but
 * the disassembly is a plain MOV: they are int32 fields, not int->float
 * conversions (lift-silent-bugs Check 1). They are written through the
 * int-aliased pointer.
 *
 * Per edge, edge_cross = 2D cross of the edge vector (v1-v0) with the ray
 * direction; pt_cross = 2D cross of (point-v0) with (v1-v0). FPU load/subtract
 * order verified against the delinked reference (cross-product operand order,
 * lift-decompiler-traps Trap 4). When edge_cross==0 (ray parallel to edge) and
 * the point sits on the inner side, both enter/leave are forced to a crossing
 * interval. Otherwise t = pt_cross/edge_cross updates the entering or leaving
 * bound depending on sign(edge_cross) vs side. Returns 1 iff leave_t < enter_t.
 */
int collision_surface_test_line2d(int bsp, int surface_index, int param3,
                                  int param4, float *point, float *direction,
                                  float *out_result)
{
  int first_edge;
  int edge_index;
  int *edge;
  float *v0;
  float *v1;
  unsigned char side;         /* sete to a byte slot in the original */
  volatile float edge_cross;  /* store-once/reload: the original spills it
                                 to the out_result param home slot and
                                 reloads it 3x (==0 test, divide, sign) */
  float pt_cross;
  float ex, ey; /* v1 - v0 (edge vector), kept ST-resident */
  float cx, cy; /* point - v0, kept ST-resident */
  int *out_i;

  out_i = (int *)out_result;

  first_edge = *(int *)((char *)tag_block_get_element((void *)(bsp + 0x3c),
                                                      surface_index, 0xc) +
                        4);
  edge_index = first_edge;

  out_result[0] = -3.4028235e+38f;
  out_i[1] = -1;
  out_i[2] = -1;
  out_result[3] = 3.4028235e+38f;
  out_i[4] = -1;
  out_i[5] = -1;

  do {
    edge = (int *)tag_block_get_element((void *)(bsp + 0x48), edge_index, 0x18);
    side = (edge[5] == surface_index);
    v0 = (float *)tag_block_get_element((void *)(bsp + 0x54), edge[0], 0x10);
    v1 = (float *)tag_block_get_element((void *)(bsp + 0x54), edge[1], 0x10);

    ex = v1[0] - v0[0];
    ey = v1[1] - v0[1];
    cx = point[0] - v0[0];
    cy = point[1] - v0[1];
    edge_cross = ey * direction[0] - ex * direction[1];
    pt_cross = ex * cy - cx * ey;

    if (edge_cross != 0.0f) {
      pt_cross = pt_cross / edge_cross;
      if ((edge_cross < 0.0f) != side) {
        if (pt_cross > out_result[0]) {
          out_result[0] = pt_cross;
          out_i[1] = edge_index;
          out_i[2] = edge[!side + 4];
        }
      } else if (pt_cross < out_result[3]) {
        out_result[3] = pt_cross;
        out_i[4] = edge_index;
        out_i[5] = edge[!side + 4];
      }
    } else if ((pt_cross < 0.0f) != side) {
      out_result[0] = 3.4028235e+38f;
      out_i[1] = edge_index;
      out_i[2] = edge[!side + 4];
      out_result[3] = -3.4028235e+38f;
      out_i[4] = edge_index;
      out_i[5] = edge[!side + 4];
    }

    edge_index = edge[side + 2];
  } while (edge_index != first_edge);

  if (out_result[0] > out_result[3]) {
    return 1;
  }
  return 0;
}
