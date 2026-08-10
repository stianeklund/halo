/* collision_bsp.obj (physics/collision_bsp.c)
 *
 * Declarations for tag_block_get_element, display_assert and system_exit come
 * from the generated decl.h via kb.json.
 */

/* 0x147380
 *
 * __cdecl adapter over the bsp3d node walker at 0x1470b0. Forwards all seven
 * caller arguments unchanged and injects 0xffffffff as the callee's third
 * argument (`flags`); that constant is the only thing the thunk contributes.
 * The callee's EAX result falls straight through the epilogue (no MOV/XOR
 * after the CALL), so this thunk returns the callee's value.
 *
 * Binary: 8 pushes / ADD ESP,0x20 out, 7 dword params in at EBP+0x08..+0x20,
 * plain RET (caller cleanup on both sides). Parameter types are taken from the
 * callee's declaration; +0x18 is the callee's `float epsilon`.
 */
int FUN_00147380(
  int tag_base, uint32_t node_index, float *verts, int counts, float epsilon,
  void (*callback)(float *, int, unsigned int, unsigned int, void *), void *ctx)
{
  return FUN_001470b0(tag_base, node_index, 0xffffffff, verts, counts, epsilon,
                      callback, ctx);
}

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
  float pa_x, pa_y, pa_z; /* edge[side] vertex - anchor */
  volatile float qa_x, qa_y; /* edge[!side] vertex - anchor */
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
      area =
        plane[2] * cross_z + plane[1] * cross_y + cross_x * plane[0] + area;
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

/* 0x147990 - collision_surface_project_point2d
 *
 * Projects a 3D point onto a collision-BSP surface's 2D plane space by looking
 * up the surface's plane and delegating to project_point2d.
 *
 * The collision_bsp tag block headers are 0xc bytes each and sit at fixed
 * offsets from the bsp base; this function touches two of them:
 *   +0x0c planes   (stride 0x10): 4 float32 plane equation (nx ny nz d)
 *   +0x3c surfaces (stride 0x0c): surface[+0] = plane index
 *
 * surface[+0] carries a plane-flip flag in bit 31 (`AND EDX,0x7fffffff` at
 * 0x1479ac), so the index must be masked before indexing the plane block --
 * without the mask the lookup runs off the end of the block.
 *
 * param3/param4 are passed straight through; the original pushes both as full
 * dwords ([EBP+0x10], [EBP+0x14]) and the callee's int16_t/uint8_t prototype
 * performs the truncation.
 *
 * Returns out_point (MOV EAX,ESI at 0x1479d3, where ESI was reloaded from
 * [EBP+0x1c] at 0x1479ba) -- the same pointer that was passed in, NOT a status
 * code (lift-silent-bugs Check 16, void-EAX/wrong-return).
 *
 * Note on the frame: ADD ESP,0x2c at 0x1479d0 is a single coalesced cdecl
 * cleanup for all three calls (3 + 3 + 5 dwords), not evidence of an 11-arg
 * call (lift-decompiler-traps, cdecl ADD ESP mis-grouping).
 *
 * project_point2d writes 3 floats to out_point, so callers must supply a
 * buffer of at least 12 bytes (projection-output-size, §5).
 */
int collision_surface_project_point2d(int bsp, int surface_index, int param3,
                                      int param4, float *point,
                                      float *out_point)
{
  int *surface;
  float *plane;

  surface =
    (int *)tag_block_get_element((void *)(bsp + 0x3c), surface_index, 0xc);
  plane = (float *)tag_block_get_element((void *)(bsp + 0xc),
                                         *surface & 0x7fffffff, 0x10);
  project_point2d(point, plane, param3, param4, out_point);
  return (int)out_point;
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
  unsigned char side; /* sete to a byte slot in the original */
  volatile float edge_cross; /* store-once/reload: the original spills it
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

/* 0x1486e0
 *
 * Recursive descent through a 2D BSP with a two-sided plane epsilon: visits
 * every node whose splitting line the query point straddles within `epsilon`,
 * and hands each reached leaf to FUN_00147ed0.
 *
 * `state` is the walk context; only four fields are touched here:
 *   +0x000 pointer to the bsp tag base; the 2D-node tag_block sits at +0x30
 *   +0x010 float epsilon (used for BOTH the +eps and -eps bound; the original
 *          re-reads the field for each compare rather than caching it)
 *   +0x220 float query x
 *   +0x224 float query y
 *
 * Node record is 0x14 bytes:
 *   +0x00 float i, +0x04 float j  (2D plane normal)
 *   +0x08 float d                 (plane offset)
 *   +0x0c int front child, +0x10 int back child
 * A negative child index is a leaf: bit 31 is the leaf flag, so the index is
 * masked with 0x7fffffff before it reaches FUN_00147ed0 (which takes its
 * state pointer in EAX, @<eax>).
 *
 * Ordering is load-bearing and is taken from the disassembly, not the
 * decompile:
 *  - The sum is built as `j*y` FIRST (`FLD [ESI+4]; FMUL [EDI+0x224]`) and
 *    `x*i` second (`FLD [EDI+0x220]; FMUL [ESI]`), then FADDP; Ghidra prints
 *    the addends in the opposite order because it normalises commutative adds.
 *  - `FSUB dword ptr [ESI+8]` is sum MINUS the plane offset, not the reverse.
 *  - BOTH comparisons are evaluated before either branch is taken: FCOM
 *    against +epsilon latches the front flag into CL, then FLD/FCHS/FXCH/
 *    FCOMPP against -epsilon latches the back flag into BL. The recursive
 *    call happens after both flags exist, so the flags must be materialised
 *    into byte-wide locals up front rather than folded into the `if`s.
 *
 * The loop is the rotated form MSVC emits for `while`: the entry sign test
 * jumps straight to the leaf handler, and the bottom `MOV ESI,[ESI+0x10];
 * JNS` either re-enters the body or falls through to that same leaf handler.
 */
void FUN_001486e0(void *state, int node_index)
{
  float *node;
  float d;
  unsigned char front; /* CL in the original */
  unsigned char back; /* BL in the original */

  while (node_index >= 0) {
    node = (float *)tag_block_get_element((void *)(*(int *)state + 0x30),
                                          node_index, 0x14);

    d = node[1] * *(float *)((char *)state + 0x224) +
        *(float *)((char *)state + 0x220) * node[0] - node[2];

    front = (unsigned char)(d > *(float *)((char *)state + 0x10));
    back = (unsigned char)(d < -*(float *)((char *)state + 0x10));

    if (front) {
      FUN_001486e0(state, ((int *)node)[3]);
    }
    if (!back) {
      return;
    }
    node_index = ((int *)node)[4];
  }

  FUN_00147ed0(state, node_index & 0x7fffffff);
}

/* 0x148b20 - collision_bsp_test_pill_new
 *
 * Packs the eight caller arguments plus three fixed defaults into a 0x2c-byte
 * bsp3d traversal record on the stack, seeds the caller's distance slot with
 * +FLT_MAX, and tail-calls the recursive bsp3d walker at 0x148440 over the
 * whole parametric span [0.0, 1.0].
 *
 * Binary: PUSH EBP / MOV EBP,ESP / SUB ESP,0x2c, plain RET, eight dword
 * parameter slots at EBP+0x08..+0x24 and a four-argument CALL cleaned with
 * ADD ESP,0x10 - cdecl on both sides. EBP+0x0c is loaded with
 * `MOV CX, word ptr` and stored with `MOV word ptr [EBP-0x28],CX`: that
 * parameter is 16-bit, and record bytes +0x06..+0x07 are never written. The
 * three bytes after the +0x24 byte store are likewise never written; the
 * record is deliberately NOT zero-initialised, so do not add an initialiser.
 *
 * Record field meanings are taken from the reads performed by the callee at
 * 0x148440: [+0x00] is the bsp3d tag base (tag_blocks at +0x00 and +0x0c),
 * [+0x0c]/[+0x10] are the two float[3] vectors dotted against each node plane,
 * [+0x14] is the plane-distance tolerance (the pill radius), [+0x18] is the
 * float* the walker overwrites with the hit distance, [+0x1c] is the float[3]
 * the surface normal is copied into, and [+0x28] is the signed plane index the
 * walker latches (hence the 0xffffffff seed). [+0x04] (the 16-bit parameter),
 * [+0x08], [+0x20] and [+0x24] are not read by 0x148440 itself - they are
 * consumed further down the traversal - so they keep mechanical names.
 *
 * Return: the function performs no MOV/XOR after the CALL, so the walker's AL
 * result falls through the epilogue. The single caller (FUN_0014e940 at
 * 0x14e989) consumes it with TEST AL,AL / JE, so the return type is bool, not
 * void.
 */
typedef struct {
  int bsp3d; /* 0x00 */
  short flags; /* 0x04 */
  short pad_06; /* 0x06 - never written by the builder */
  int field_08; /* 0x08 */
  float *origin; /* 0x0c */
  float *direction; /* 0x10 */
  float radius; /* 0x14 */
  float *out_distance; /* 0x18 */
  float *out_normal; /* 0x1c */
  int field_20; /* 0x20 */
  char field_24; /* 0x24 */
  char pad_25[3]; /* 0x25 - never written by the builder */
  int field_28; /* 0x28 */
} bsp3d_pill_test_data;

/* noinline: the sole caller FUN_0014e940 (0x14e940) reaches this through a real
 * CALL at 0x14e989, so the original build did NOT inline it. Left to its own
 * devices the compiler folds this body into that caller and hoists the 0x2c
 * bsp3d_pill_test_data record into the caller's frame (sub esp,0x3c instead of
 * sub esp,0x10), which is a structural mismatch against the binary. */
bool __declspec(noinline)
collision_bsp_test_pill_new(int bsp3d, short flags, int param3, float *origin,
                            float *direction, float radius,
                            float *out_distance, float *out_normal)
{
  bsp3d_pill_test_data data;

  data.bsp3d = bsp3d;
  data.flags = flags;
  data.field_08 = param3;
  data.origin = origin;
  data.direction = direction;
  data.radius = radius;
  data.out_distance = out_distance;
  data.out_normal = out_normal;
  data.field_20 = -1;
  data.field_24 = 0;
  data.field_28 = -1;

  *out_distance = 3.4028235e+38f;

  return FUN_00148440(&data, 0, 0.0f, 1.0f);
}

/* 0x1493b0 - collision_bsp_test_sphere
 *
 * Packs the six caller arguments plus a zeroed seventh field into a 0x228-byte
 * bsp3d sphere-test context on the stack, clears the four result-list counters,
 * and runs the recursive bsp3d sphere walk from node 0.
 *
 * Binary: PUSH EBP / MOV EBP,ESP / SUB ESP,0x228 (no _chkstk), EBX/ESI/EDI
 * saved, and a single grouped ADD ESP,0x1c at 0x149454 covering ALL FOUR calls
 * (1 + 1 + 2 + 3 = 7 dwords). The enrichment's "cleanup=7 stack args, decl=3"
 * report against the collision_log_add_time site is that cdecl mis-grouping,
 * not a real arg-count mismatch: that call pushes exactly EAX/ECX/EDI at
 * 0x14944a-0x14944c.
 *
 * Only the first 0x1c bytes of the context are written here; the remaining
 * 0x20c bytes are scratch the recursive walk fills, so the aggregate must stay
 * 0x228 bytes or the callee overruns the frame.
 *
 * ESI is loaded with `bsp` ([EBP+0x8]) early and then RELOADED at 0x1493f9
 * with `results` ([EBP+0x1c]); the `MOV [ESI+0xc0c],EBX` style stores are
 * therefore against `results`, not `bsp` (register-aliasing trap).
 *
 * EDI = (bsp == *(int *)0x5064dc) + 6, i.e. log id 7 when the bsp is the
 * structure BSP the scenario installed at 0x5064dc, else 6. The same value is
 * passed to collision_log_add_call and collision_log_add_time.
 *
 * `flags` is stored with `MOV word ptr [EBP-0x224],CX` - 16-bit, so the upper
 * half of that context dword is never written.
 *
 * `results` is indexed with 0x404-byte strides (int indices 0, 0x101, 0x202,
 * 0x303): four parallel lists, each a count int followed by 0x100 entries. No
 * struct is recovered for it, so the raw int-index form is kept. Store order
 * in the binary is +0xc0c, +0x000, +0x404, +0x808 and is preserved here.
 *
 * Return: 1 when either of the first two list counters ended up positive (both
 * compared signed with JG against EBX = 0), else 0. The epilogue is duplicated
 * on both paths, so there is no shared tail.
 */
typedef struct {
  int bsp; /* 0x00 */
  short flags; /* 0x04 - 16-bit store */
  short pad_06; /* 0x06 - never written by the builder */
  int origin; /* 0x08 */
  int direction; /* 0x0c */
  int radius; /* 0x10 */
  int *results; /* 0x14 */
  int field_18; /* 0x18 */
  char scratch[0x228 - 0x1c]; /* 0x1c - filled by the recursive walk */
} bsp3d_sphere_test_data;

int collision_bsp_test_sphere(int bsp, short flags, int origin, int direction,
                              int radius, int *results)
{
  bsp3d_sphere_test_data data;
  short log_id;

  log_id = (short)((bsp == *(int *)0x5064dc) + 6);
  collision_log_add_call(log_id);
  collision_log_query_counter((void *)0x46f098);

  data.bsp = bsp;
  data.flags = flags;
  data.origin = origin;
  data.direction = direction;
  data.radius = radius;
  data.results = results;
  data.field_18 = 0;

  results[0x303] = 0;
  results[0] = 0;
  results[0x101] = 0;
  results[0x202] = 0;

  bsp3d_test_sphere_recursive(&data, 0);

  collision_log_add_time(log_id, *(unsigned int *)0x46f098, *(int *)0x46f09c);

  /* Both tests are JG against EBX = 0, i.e. `> 0`, and each branch carries its
   * own copy of the epilogue - Ghidra's `< 1 && < 1 -> return 0` rendition is
   * the same predicate but compiles to JGE and a shared tail. */
  if (results[0] > 0 || results[0x101] > 0) {
    return 1;
  }
  return 0;
}

/* 0x14dc30 - Point-vs-world collision test. If any of the collision-type
 * flags (0xE0) are set, locate the BSP3D leaf containing `pos`; a leaf of -1
 * (point outside the BSP) reports a hit. When flag bit 7 (0x80) is set and the
 * global at 0x4761f8 is clear, walk the collideable object partition of the
 * leaf's cluster and sphere-test each object list via FUN_0014db10; the first
 * hit returns 1. Otherwise returns 0.
 *
 * Confirmed: cdecl, 3 stack args, char return in AL (XOR AL,AL at 0x14dcc7 /
 * MOV AL,1 at 0x14dcce). No FPU ops anywhere in the function. Entry test is
 * TEST BL,0xE0. The original has no locals (no `sub esp`) - the iterator state
 * is written into the incoming [EBP+8] arg slot, but EBX/EDI/ESI already hold
 * param_1/pos/param_3 before that happens, so using a separate local here is
 * behaviourally identical (same idiom as the sibling loops in
 * collision_usage.c). */
char FUN_0014dc30(int param_1, float *pos, int param_3)
{
  uint32_t leaf;
  char use_water;
  void *elem;
  int16_t cluster_idx;
  int object_handle;
  int iter_state;

  if ((param_1 & 0xe0) != 0) {
    leaf = bsp3d_find_leaf(FUN_0018e420(), 0, pos);

    /* SHR ECX,7 / AND CL,1, then zeroed when the global is set. */
    use_water = (char)(((uint32_t)param_1 >> 7) & 1);
    if (*(char *)0x4761f8 != '\0')
      use_water = 0;

    if (leaf == 0xffffffff)
      return 1;

    if (use_water != 0) {
      /* Ghidra cdecl arg mis-grouping (ADD ESP,0x14 covers these pushes plus
       * the iter_first pushes): block is scenario_get()+0xe0, index is
       * leaf&0x7fffffff, element size 0x10. Cluster index is MOVSX word
       * [elem+8] - int16, sign-extended. */
      elem = tag_block_get_element((char *)scenario_get() + 0xe0,
                                   leaf & 0x7fffffff, 0x10);
      cluster_idx = *(int16_t *)((char *)elem + 8);

      object_handle =
        cluster_partition_object_iter_first(&iter_state, cluster_idx);
      while (object_handle != -1) {
        if (FUN_0014db10(object_handle, param_1, (int)pos, param_3)) {
          return 1;
        }
        object_handle = cluster_partition_object_iter_next(&iter_state);
      }
    }
  }
  return 0;
}

/* 0x14e940
 *
 * Sweeps one pill of radius `radius` along the segment [origin, origin+delta]
 * against the STRUCTURE bsp only (the bsp handed back by
 * global_collision_bsp_get - no object or model collision is consulted here)
 * and fills the caller's 0x50-byte collision-result record.
 *
 * Binary: PUSH EBP / MOV EBP,ESP / SUB ESP,0x10, EBX/ESI/EDI saved, plain RET
 * (cdecl both ways). Six dword parameter slots at EBP+0x08..+0x1c; +0x08 and
 * +0x18 are never referenced by the body, so they keep mechanical names.
 * ESI caches the result pointer from EBP+0x1c at 0x14e94e.
 *
 * Call site at 0x14e989: eight pushes cleaned by a single ADD ESP,0x20, so
 * every push belongs to collision_bsp_test_pill_new. In push order they are
 * &normal, &t, radius, delta, origin, 0, 0, bsp - i.e. reversed into C order:
 * (bsp, 0, 0, origin, delta, radius, &t, &normal). The bsp pointer is the
 * LAST push (0x14e988, straight off the getter's EAX) and is therefore the
 * FIRST argument; the enrichment's "getter swallowed the args" note is the
 * usual cdecl mis-grouping and does not apply.
 *
 * `t` lives in the dead EBP+0x1c parameter slot in the original: the result
 * pointer is already cached in ESI, so MSVC recycled the incoming slot as the
 * out-distance scratch and reads the callee-written float back from it at
 * 0x14e995. Modelled here as an ordinary local, which is behaviourally
 * identical - the result pointer must NOT be re-read after the call.
 *
 * The surface normal is copied into the record at +0x24..+0x2c inside the hit
 * branch and then unconditionally zeroed again by the tail at 0x14e9fe. That
 * double write is in the binary (MSVC 7.1 does not eliminate the dead store
 * across the branch merge); do not collapse it.
 *
 * Return: AL. The hit path sets AL=1 directly (0x14e9d1) and the miss path
 * reloads the byte flag seeded to 0 at 0x14e967, so the frame slot is real
 * even though the true path never stores through it.
 */
bool FUN_0014e940(int param_1, float *origin, float *delta, float radius,
                  int param_5, collision_test_result *result)
{
  float normal[3];
  float t;
  float hit_t;
  bool hit = false;

  result->field_00 = -1;
  result->field_04 = -1;
  result->field_08 = -1;
  result->field_0c = -1;
  result->field_10 = -1;
  result->t = 1.0f;

  if (collision_bsp_test_pill_new((int)global_collision_bsp_get(), 0, 0, origin,
                                  delta, radius, &t, normal)) {
    result->t = t;
    result->normal[0] = normal[0];
    result->normal[1] = normal[1];
    result->normal[2] = normal[2];
    result->field_00 = 2;
    result->field_30 = 3.4028235e+38f;
    result->field_34 = -1;
    result->field_44 = -1;
    result->field_48 = -1;
    result->field_4c = 0;
    result->field_4d = 0;
    result->field_4e = -1;
    hit = true;
  }

  /* One FLD of result->t held on the x87 stack and duplicated per component
   * (0x14e9d8: FLD [ESI+0x14] / FLD ST(0) ...), not three reloads. */
  hit_t = result->t;
  result->position[0] = hit_t * delta[0] + origin[0];
  result->position[1] = hit_t * delta[1] + origin[1];
  result->position[2] = hit_t * delta[2] + origin[2];
  result->normal[0] = 0.0f;
  result->normal[1] = 0.0f;
  result->normal[2] = 0.0f;

  return hit;
}
