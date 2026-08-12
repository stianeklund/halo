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
  float dx;
  float dy;
  float dz;

  edge = (unsigned int *)tag_block_get_element((void *)(bsp + 0x48), edge_index,
                                               0x18);
  vertex_a =
    (float *)tag_block_get_element((void *)(bsp + 0x54), edge[0], 0x10);
  vertex_b =
    (float *)tag_block_get_element((void *)(bsp + 0x54), edge[1], 0x10);
  /* All three deltas are formed first (0x1476d5..0x1476e7, in x/y/z order) and
   * only then squared and accumulated, in x, z, y order: 0x1476ec FLD ST(2) /
   * FMULP ST(3) squares dx, 0x1476f0 FLD ST(0) / FMUL ST(1) squares dz, and
   * 0x1476f6 FLD ST(1) / FMUL ST(2) squares dy last. x87 addition is not
   * associative, so the sum must be written in that order to round the same
   * way; the same pattern appears in collision_surface_perimeter and in
   * FUN_0014ea10. */
  dx = vertex_b[0] - vertex_a[0];
  dy = vertex_b[1] - vertex_a[1];
  dz = vertex_b[2] - vertex_a[2];
  return sqrtf(dx * dx + dz * dz + dy * dy);
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

/* 0x1479e0 - collision_surface_test_point2d
 *
 * Point-in-surface test in the surface's 2D projection space. Walks the
 * winged-edge ring of one collision-BSP surface; the point is inside iff it
 * lies on the non-positive side of every bounding edge.
 *
 * Same tag block geometry as collision_surface_polygon:
 *   bsp+0x3c surfaces (stride 0xc): surface[+4] = first-edge index
 *   bsp+0x48 edges    (stride 0x18): edge[0]/edge[1] endpoint vertex indices,
 *                                    edge[2]/edge[3] next-edge links,
 *                                    edge[5] (+0x14) owning surface index
 *   bsp+0x54 vertices (stride 0x10)
 * `side` = (edge[5] == surface_index) selects this surface's half-edge slot,
 * so the ring is traversed consistently: the leading endpoint is edge[side],
 * the trailing one edge[!side], and the link forward is edge[2 + side].
 * The original computes !side as a SECOND `sete` off the same compare
 * (XOR ECX,ECX / TEST BL,BL / SETE CL at 0x147a41), not as 1 - side.
 *
 * Both endpoints are projected to 2D with the caller's projection basis
 * (param3) and axis sign (param4); FUN_00061df0 writes 2 floats, hence the
 * float[2] scratch pairs rather than scalars (Ghidra's local_18/local_14 and
 * local_20/local_1c are NOT in buffer order -- a2d is [EBP-0x14], b2d is
 * [EBP-0x1c]; lift-decompiler-traps buffer-alias confusion).
 *
 * Rejection test, FPU operand order verified instruction-by-instruction at
 * 0x147a83-0x147aa2 (cross-product operand swap, lift-decompiler-traps
 * Trap 4 -- swapping the two products negates the test and inverts
 * inside/outside for every collision surface):
 *   (point.y - b2d.y) * (point.x - a2d.x) - (point.x - b2d.x) * (point.y -
 * a2d.y) FCOMP is against the .rdata pooled 0.0f at 0x2533c0, and TEST AH,0x41
 * / JE takes the C0=C3=0 path (strictly greater) to the XOR AL,AL return, i.e.
 * `> 0.0f` rejects. Returns are MOV AL,1 / XOR AL,AL -- bool in AL.
 *
 * ADD ESP,0x44 at 0x147a88 is one coalesced cdecl cleanup for the four calls
 * that follow the first (0x18 + 0x10 + 0x10 dwords of args plus the two
 * projections), not a 17-argument call (lift-decompiler-traps, cdecl ADD ESP
 * mis-grouping). The hazard scanner's ARG_COUNT warning on 0x147a7e is this.
 */
char collision_surface_test_point2d(int bsp, int surface_index, int param3,
                                    int param4, float *point)
{
  void *edges;
  int side;
  int first_edge;
  int edge_index;
  int *edge;
  void *va;
  void *vb;
  float a2d[2];
  float b2d[2];

  first_edge = *(int *)((char *)tag_block_get_element((void *)(bsp + 0x3c),
                                                      surface_index, 0xc) +
                        4);
  edges = (void *)(bsp + 0x48);
  edge_index = first_edge;
  do {
    edge = (int *)tag_block_get_element(edges, edge_index, 0x18);
    side = (edge[5] == surface_index);
    va = tag_block_get_element((void *)(bsp + 0x54), edge[side], 0x10);
    vb = tag_block_get_element((void *)(bsp + 0x54), edge[!side], 0x10);
    FUN_00061df0(va, (short)param3, (unsigned char)param4, a2d);
    FUN_00061df0(vb, (short)param3, (unsigned char)param4, b2d);
    if ((point[1] - b2d[1]) * (point[0] - a2d[0]) -
          (point[0] - b2d[0]) * (point[1] - a2d[1]) >
        0.0f) {
      return 0;
    }
    edge_index = edge[2 + side];
  } while (edge_index != first_edge);
  return 1;
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

/* 0x148240
 *
 * Walk the edge ring of one collision surface and test an already-projected 2D
 * point against every edge. Returns 1 when the point lies on the inner side of
 * (or exactly on) all edges, 0 at the first edge it falls outside of.
 *
 * Before the ring walk the surface is screened against the breakable-surface
 * bit vector: when the surface carries flag 0x8 and its breakable index
 * (surface+9, a byte) is below `param_1`, the matching bit must be set in
 * `bit_vector` or the surface is rejected outright (0x148260-0x148289).
 *
 * `bsp` arrives in EAX (0x148249 MOV EDI,EAX); the six remaining arguments are
 * ordinary cdecl stack slots -- the call site at 0x1488c9 cleans 0x18. The
 * return is written to AL only (0x148355 MOV AL,0x1 / 0x14835e XOR AL,AL),
 * hence char rather than int.
 *
 * Each edge is projected through FUN_00061df0 with the caller's projection
 * axis and sign, then the 2D cross product decides the side. Operand order is
 * load-order-faithful to 0x148310-0x148330: the four differences are pushed
 * dx, dy, ex, ey and the products are dx*ey then dy*ex. The guard rejects on
 * strictly-greater-than-zero (0x14833c TEST AH,0x41 / JZ to the failure tail),
 * so a NaN cross falls through to the next edge rather than rejecting.
 */
char FUN_00148240(short param_1, unsigned int *bit_vector, int elem_index,
                  short projection, unsigned char sign, float *point2d,
                  void *bsp)
{
  int *surface;
  int *edge;
  float *v0;
  float *v1;
  unsigned char breakable;
  int edge_index;
  /* byte-typed: 0x1482b9 SETZ BL / 0x1482bc MOVZX EDI,BL keeps the side flag
   * in a byte register and widens it only for the edge[] index. */
  unsigned char side;
  float proj0[2];
  float proj1[2];
  float dx;
  float dy;
  float ex;
  float ey;

  surface = (int *)tag_block_get_element((char *)bsp + 0x3c, elem_index, 0xc);
  if ((((unsigned char *)surface)[8] & 8) != 0) {
    breakable = ((unsigned char *)surface)[9];
    if (breakable < param_1 &&
        (bit_vector[breakable >> 5] & (1u << (breakable & 0x1f))) == 0) {
      return 0;
    }
  }

  edge_index = surface[1];

  do {
    edge = (int *)tag_block_get_element((char *)bsp + 0x48, edge_index, 0x18);
    side = (edge[5] == elem_index);
    v0 = (float *)tag_block_get_element((char *)bsp + 0x54, edge[side], 0x10);
    v1 = (float *)tag_block_get_element((char *)bsp + 0x54, edge[!side], 0x10);

    FUN_00061df0(v0, projection, sign, proj0);
    FUN_00061df0(v1, projection, sign, proj1);

    dx = point2d[0] - proj0[0];
    dy = point2d[1] - proj0[1];
    ex = proj1[0] - proj0[0];
    ey = proj1[1] - proj0[1];
    if (dx * ey - dy * ex > 0.0f) {
      return 0;
    }

    edge_index = edge[side + 2];
  } while (edge_index != surface[1]);

  return 1;
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
 *  - Both flag polarities come from PARITY, not zero: 0x14871e is
 *    `TEST AH,0x41; JP` (not JZ). AH&0x41 has even parity for "greater"
 *    (0x00) and for "unordered" (0x41), odd for "less" (0x01) and "equal"
 *    (0x40), so CL = (d <= +eps) and NaN clears it. 0x148734 is
 *    `TEST AH,0x1; JNZ` on C0 alone, so BL = (d >= -eps). Reading the JP as
 *    a JZ inverts BOTH descents: points within epsilon of a splitting plane
 *    then reach no leaf at all and every other point takes the wrong half,
 *    which drops BSP ground collision entirely.
 *
 * The loop is the rotated form MSVC emits for `while`: the entry sign test
 * jumps straight to the leaf handler, and the bottom `MOV ESI,[ESI+0x10];
 * JNS` either re-enters the body or falls through to that same leaf handler.
 */
void FUN_001486e0(void *state, int node_index)
{
  float *node;
  float d;
  /* Named for the descent condition each flag gates, not for a plane side:
   * the child at +0x0c is entered when d <= +eps and the child at +0x10 when
   * d >= -eps, so calling them "front"/"back" would assert a sign convention
   * the code contradicts. CL and BL in the original. */
  unsigned char take_le; /* CL in the original */
  unsigned char take_ge; /* BL in the original */

  while (node_index >= 0) {
    node = (float *)tag_block_get_element((void *)(*(int *)state + 0x30),
                                          node_index, 0x14);

    d = node[1] * *(float *)((char *)state + 0x224) +
        *(float *)((char *)state + 0x220) * node[0] - node[2];

    take_le = (unsigned char)(d <= *(float *)((char *)state + 0x10));
    take_ge = (unsigned char)(d >= -*(float *)((char *)state + 0x10));

    if (take_le) {
      FUN_001486e0(state, ((int *)node)[3]);
    }
    if (!take_ge) {
      return;
    }
    node_index = ((int *)node)[4];
  }

  FUN_00147ed0(state, node_index & 0x7fffffff);
}

/* 0x148780
 *
 * Scan one bsp2d node's surface-reference run for the surface the ray hit, and
 * resolve it down to a leaf surface index. Returns that index, or -1 when the
 * run holds no reference to `surface_index` (0x1488f5 OR EAX,0xffffffff).
 *
 * `node_index` arrives in EAX (0x14878e PUSH EAX straight into the first
 * tag_block_get_element); the eight remaining arguments are cdecl stack slots
 * and the call site at 0x1490e8 cleans exactly 0x20.
 *
 * For each matching reference the surface plane picks a projection axis by
 * dropping the largest-magnitude component (0x1487e4-0x148828): the three
 * FABS values are compared as |p2| vs |p1| then |p2| vs |p0|, so axis 2 wins
 * only when |p2| dominates both. `sign` then XORs the sign of the surviving
 * plane component against bit 31 of the reference word.
 *
 * The byte flag at 0x148860 is stored as a byte and reloaded as a dword at
 * 0x148866 (Ghidra renders the dead upper bytes as CONCAT31). Every consumer
 * -- FUN_00061df0 and FUN_00148240 -- declares the parameter `unsigned char`,
 * so the upper three bytes are provably dead and the CONCAT is not modelled.
 *
 * The ray point is formed multiply-then-add (t * direction + origin), matching
 * the FMUL/FADD order at 0x14886c-0x148885; do not reorder.
 *
 * VC71 ceiling: 93.6% match (158 vs 157 insns), operand-normalized 75.8%. The
 * frame matches exactly (0x20 both sides) and dp-LCS agrees with the official
 * score, so there is no anchor collapse hiding progress. What remains, all
 * checked and not source-reachable:
 *  - The single extra instruction is `MOV EAX,[EBP+0x28]` at entry. node_index
 *    is @<eax>, so the generated thunk hands it to us in the ninth stack slot
 *    and our body has to load it; the original already has it in EAX.
 *  - Register allocation is permuted throughout: axis in ESI (ref EDI), plane
 *    in EDX (ref ECX), result in EDI (ref ESI), point2d address in ECX (ref
 *    EAX). That is most of the operand-score gap and it moves nothing in match.
 *  - 0x14884f NEG/SBB/NEG vs our XOR/TEST/SETNE for `flipped != 0`. Two source
 *    forms tried and both measured worse: inlining the mask lets VC71 fold it
 *    to SHR $31 and drop the 0x80000000 constant (93.6 -> 93.2, IMM-WARN);
 *    forcing the plane compare into a byte local to provoke the reference's
 *    MOVZBL emits MOVB/XORB for the 0-or-1 and wrecks the point3d schedule
 *    (93.6 -> 88.9).
 *  - FMULS/FADDS scheduling around 0x148866-0x14888e, and the epilogue's POP
 *    EDI position. Both are VC71 scheduling, not expressible in source.
 *  - `FCOMPS 0x0` vs the reference's pooled `FCOMP [0x2533c0]` is already
 *    scored equal, so spelling the zero as *(float *)0x2533c0 gains nothing.
 * The permuter was run (6376 iterations, 4 threads): it reported a better
 * in-search score only for candidates that truncate `flipped` to 16 or 8 bits
 * or rewrite `> 0.0f` as `>= 1.0f` - i.e. by breaking the logic - and the run
 * itself printed BASELINE MISMATCH. Nothing to take from it.
 */
int FUN_00148780(void *bsp, short param_2, unsigned int *bit_vector,
                 float *origin, float *direction, int surface_index, float t,
                 char param_8, int node_index)
{
  int *node;
  unsigned int *ref;
  float *plane;
  int i;
  /* short: 0x148828 MOVSX EDX,DI sign-extends the axis from 16 bits before
   * it indexes the plane. */
  short axis;
  unsigned char sign;
  unsigned int flipped;
  int result;
  /* Held on the x87 stack, not in frame slots: 0x1487f5 FCOM ST(1) and
   * 0x1487fe FCOMP ST(2) compare register-to-register, so all three
   * magnitudes are live simultaneously and none is spilled. */
  float abs0;
  float abs1;
  float abs2;
  float point3d[3];
  float point2d[2];

  node = (int *)tag_block_get_element((char *)bsp + 0x18, node_index, 8);
  i = node[1];

  /* A plain `while`, not guard + do-while: the original has ONE -1 epilogue at
   * 0x1488f3, entered both by the 0x1487ac JGE that skips an empty run and by
   * the 0x1488ed JL falling through at the bottom. Splitting the guard out
   * duplicates the epilogue. The bound is recomputed from `node` at both tests
   * (0x14879b and 0x1488de each MOVSX the count and re-add node[1]). */
  while (i < *(short *)((char *)node + 2) + node[1]) {
    ref = (unsigned int *)tag_block_get_element((char *)bsp + 0x24, i, 8);
    if ((ref[0] & 0x7fffffff) == (unsigned int)surface_index) {
      plane =
        (float *)tag_block_get_element((char *)bsp + 0xc, surface_index, 0x10);

      abs0 = (float)fabs((double)plane[0]);
      abs1 = (float)fabs((double)plane[1]);
      abs2 = (float)fabs((double)plane[2]);
      if (abs2 >= abs1 && abs2 >= abs0) {
        axis = 2;
      } else if (abs1 >= abs0) {
        axis = 1;
      } else {
        axis = 0;
      }

      /* 0x148849 AND ECX,0x80000000 / NEG / SBB / NEG materialises the sign
       * bit as an explicit 0-or-1 before the compare, so the mask must stay in
       * a local. Folding it inline as `((ref[0] & 0x80000000u) != 0)` lets
       * VC71 recognise the sign-bit extraction and collapse the whole sequence
       * to a single SHR $31, which drops the 0x80000000 constant the original
       * carries (measured: match 93.6% -> 93.2%, IMM-WARN). */
      flipped = ref[0] & 0x80000000u;
      sign = (plane[axis] > 0.0f) != (flipped != 0);

      point3d[0] = t * direction[0] + origin[0];
      point3d[1] = t * direction[1] + origin[1];
      point3d[2] = t * direction[2] + origin[2];

      FUN_00061df0(point3d, axis, sign, point2d);
      result = (int)FUN_00146d40((char *)bsp + 0x30, point2d, (int)ref[1]);

      if (param_8 == 0) {
        return result;
      }
      if (FUN_00148240(param_2, bit_vector, result, axis, sign, point2d, bsp) !=
          0) {
        return result;
      }
    }
    i = i + 1;
  }

  return -1;
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
                            float *direction, float radius, float *out_distance,
                            float *out_normal)
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


/* 0x149480 - Ray/vector-vs-BSP3D test. Thin logging wrapper around the
 * recursive BSP3D vector walk at 0x148eb0: it builds the 0x28-byte query
 * context on the stack, seeds the caller's result record, clamps the ray
 * parameter and hands the whole thing to the walker.
 *
 * ESI = (bsp == *(int *)0x5064dc) + 4, i.e. log id 5 when the bsp is the
 * structure BSP the scenario installed at 0x5064dc, else 4. The same value is
 * passed to collision_log_add_call and collision_log_add_time.
 *
 * `flags` is stored with `MOV word ptr [EBP-0x20],DX` - 16-bit, so the upper
 * half of that context dword is never written. field_20 is a BYTE store
 * (`MOV byte ptr [EBP-8],DL`), so the three bytes above it stay untouched.
 *
 * The clamp appears twice and the two are NOT the same value:
 *   0x1494ae  *result   = max_t < 0.0f ? 0.0f : max_t   (FSTP [ECX])
 *   0x1494fa  walker t  = clamp(max_t, 0.0f, 1.0f)
 * The first clamp never writes back to the parameter slot - 0x1494fa and
 * 0x14951d both reload the ORIGINAL [EBP+0x20]. Collapsing them into one
 * clamped local silently changes the value handed to the walker.
 *
 * FCOM parity senses: `TEST AH,5; JP` is taken when NOT strictly less-than
 * (fallthrough = `max_t < 0.0f`); `FCOMP 1.0f; TEST AH,0x41; JNZ` is taken on
 * below-or-equal, so the fallthrough arm is the `> 1.0f` case that forces 1.0f.
 *
 * Return: MOV BL,AL across the trailing log call, then MOV AL,BL - the char
 * hit flag produced by the walker.
 */
typedef struct {
  int32_t field_00; /* 0x00 - first caller argument, opaque here */
  int32_t bsp; /* 0x04 */
  int16_t flags; /* 0x08 - 16-bit store */
  int16_t pad_0a; /* 0x0a - never written by the builder */
  int32_t field_0c; /* 0x0c */
  int32_t field_10; /* 0x10 - read by the walker as a point (plane distance) */
  int32_t field_14; /* 0x14 - read by the walker as a point (plane distance) */
  float *result; /* 0x18 */
  int32_t field_1c; /* 0x1c - seeded to -1 */
  char field_20; /* 0x20 - BYTE store, seeded to 0 */
  char pad_21[3]; /* 0x21 - never written */
  int32_t field_24; /* 0x24 - seeded to -1 */
} bsp3d_vector_test_data;

char collision_bsp_test_vector(int param_1, int bsp, short flags, int origin,
                               int direction, int radius, float max_t,
                               float *result)
{
  bsp3d_vector_test_data data;
  short log_id;
  float t;
  char hit;

  log_id = (short)((bsp == *(int *)0x5064dc) + 4);
  collision_log_add_call(log_id);
  collision_log_query_counter((void *)0x46f090);

  data.field_00 = param_1;
  data.bsp = bsp;
  data.flags = flags;
  data.field_0c = origin;
  data.field_10 = direction;
  data.field_14 = radius;
  data.result = result;
  data.field_1c = -1;
  data.field_20 = 0;
  data.field_24 = -1;

  *result = (max_t < 0.0f) ? 0.0f : max_t;
  result[5] = 0.0f;

  if (max_t < 0.0f) {
    t = 0.0f;
  } else if (max_t > 1.0f) {
    t = 1.0f;
  } else {
    t = max_t;
  }

  hit = FUN_00148eb0(&data, 0, 0, t);

  collision_log_add_time(log_id, *(unsigned int *)0x46f090, *(int *)0x46f094);
  return hit;
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

/* 0x14dce0 - Ray-vs-object collision test over one object sibling chain.
 *
 * Walks the chain rooted at `object_handle` through object+0xc4 (next sibling)
 * and recurses into object+0xc8 (first child) for every object that passes the
 * entry filter. `collision_result` is the same 0x50-byte record the rest of
 * collision_usage.c fills; the field map documented there applies verbatim
 * (+0x00 type word, +0x14 hit distance, +0x24..+0x30 plane, +0x34 shader,
 * +0x38 object handle, +0x3c/+0x3e/+0x40 indices, +0x44/+0x48 dwords,
 * +0x4c/+0x4d bytes, +0x4e leaf/surface index).
 *
 * Confirmed (disassembly 0x14dce0..0x14df6d):
 *  - cdecl, 7 stack args at EBP+0x08..+0x20, char return in AL from EBP-0x1.
 *  - Entry filter, in order: handle != exclude_handle; (object+0x4 & 1) == 0;
 *    type_mask & (1 << (zero-extended word object+0x64 + 8));
 *    fast_vector_intersects_sphere(origin, direction, object+0x50,
 *    float object+0x5c). The radius is a push-then-FSTP float arg
 *    (PUSH ECX / FSTP [ESP] at 0x14dd3c-0x14dd43), not the pushed pointer.
 *  - Path select: byte object+0x64 shifted (TEST DL,0x2) AND
 *    type_mask & 0x400000 picks the FUN_001509c0/FUN_00150b60 model path;
 *    otherwise the collision-bsp path via FUN_0014c8e0/FUN_0014cb00.
 *  - Both distance guards are `FLD [ESI+0x14]; FCOMP <candidate>;
 *    TEST AH,0x41; JNZ skip` (0x14ddae and 0x14de51), i.e. the source form is
 *    `collision_result->distance > candidate`, NOT `candidate < ...`. Writing
 *    it the other way emits the `TEST AH,5 / JP` shape instead.
 *  - The -1 sentinel is materialised in ECX by OR ECX,0xffffffff at three
 *    sites (0x14df19/0x14df20/0x14df53) and compared with CMP; semantically a
 *    plain `!= -1`.
 *
 * Frame (SUB ESP,0x484 = 1156 bytes), derived from the EBP displacements:
 *   EBP-0x484 (1056)  FUN_0014cb00 output record; only the first 0x1c bytes
 *                     are read back here, the remaining 0x404 are scratch the
 *                     callee owns (size inferred from the frame arithmetic,
 *                     not from a decompiled callee - see Uncertain).
 *   EBP-0x64  (60)    FUN_001509c0 context
 *   EBP-0x28  (16)    FUN_0014c8e0 context (+0x4 tag data, +0xc surface base)
 *   EBP-0x18  (20)    FUN_00150b60 result
 *   EBP-0x4   (4)     `found`
 * Ghidra split every field of those four buffers into independent locals
 * (local_488/local_486/.../local_46e are all one struct at EBP-0x484).
 *
 * FUN_0014cb00 output fields actually consumed:
 *   +0x00 short surface index (also the FUN_0010a1c0 row selector, *0x34)
 *   +0x02 short  -> +0x3c        +0x04 short  -> +0x40
 *   +0x08 float  hit distance    +0x0c float* plane to transform
 *   +0x10 dword  -> +0x44        +0x14 int    -> +0x48, sign selects negate
 *   +0x18 byte   -> +0x4c        +0x19 byte   -> +0x4d
 *   +0x1a short  -> +0x4e and the FUN_0014da80 index argument
 */
char FUN_0014dce0(int object_handle, unsigned int type_mask, int param_3,
                  int origin, int direction, int exclude_handle,
                  void *collision_result)
{
  char found;
  int32_t model_result[5];
  int32_t bsp_ctx[4];
  int32_t model_ctx[15];
  int32_t bsp_result[264];
  char *obj;
  char *res;
  char *bres;
  int child;
  int idx;

  found = 0;
  do {
    obj = (char *)object_get_and_verify_type(object_handle, -1);

    if (object_handle != exclude_handle &&
        (*(unsigned char *)(obj + 4) & 1) == 0 &&
        (type_mask & (1u << (*(uint16_t *)(obj + 0x64) + 8))) != 0 &&
        fast_vector_intersects_sphere((float *)origin, (float *)direction,
                                      (float *)(obj + 0x50),
                                      *(float *)(obj + 0x5c))) {
      res = (char *)collision_result;

      if (((1 << *(unsigned char *)(obj + 0x64)) & 2) != 0 &&
          (type_mask & 0x400000) != 0) {
        if (FUN_001509c0((int *)model_ctx, object_handle) != 0 &&
            FUN_00150b60(model_ctx, (void *)origin, (void *)direction,
                         model_result) != 0 &&
            *(float *)(res + 0x14) > *(float *)model_result) {
          *(int32_t *)(res + 0x24) = model_result[1];
          *(int32_t *)(res + 0x14) = model_result[0];
          *(int32_t *)(res + 0x28) = model_result[2];
          *(int32_t *)(res + 0x2c) = model_result[3];
          *(int16_t *)res = 3;
          *(int32_t *)(res + 0x30) = model_result[4];
          *(int16_t *)(res + 0x34) = -1;
          *(int32_t *)(res + 0x38) = object_handle;
          *(int16_t *)(res + 0x3c) = -1;
          *(int16_t *)(res + 0x3e) = -1;
          *(int16_t *)(res + 0x40) = -1;
          *(int32_t *)(res + 0x44) = -1;
          *(int32_t *)(res + 0x48) = -1;
          res[0x4c] = 0;
          res[0x4d] = 0;
          *(int16_t *)(res + 0x4e) = -1;
          found = 1;
        }
      } else {
        bres = (char *)bsp_result;
        if ((char)FUN_0014c8e0((int *)bsp_ctx, object_handle) != 0 &&
            FUN_0014cb00((int)bsp_ctx, (void *)param_3, (void *)origin,
                         (void *)direction, (int16_t *)bres) != 0 &&
            *(float *)(res + 0x14) > *(float *)(bres + 8)) {
          /* MOVSX at 0x14de65: the row selector is the signed short at +0x00.
           */
          idx = *(int16_t *)bres;
          *(int16_t *)res = 3;
          *(int32_t *)(res + 0x14) = *(int32_t *)(bres + 8);
          FUN_0010a1c0((float *)(bsp_ctx[3] + idx * 0x34),
                       *(float **)(bres + 0xc), (float *)(res + 0x24));
          if (*(int32_t *)(bres + 0x14) < 0)
            plane_negate((float *)(res + 0x24), (float *)(res + 0x24));
          *(int16_t *)(res + 0x34) =
            (int16_t)FUN_0014da80(bsp_ctx[1], *(int16_t *)(bres + 0x1a));
          *(int16_t *)(res + 0x3c) = *(int16_t *)(bres + 2);
          *(int16_t *)(res + 0x3e) = *(int16_t *)bres;
          *(int16_t *)(res + 0x40) = *(int16_t *)(bres + 4);
          *(int32_t *)(res + 0x44) = *(int32_t *)(bres + 0x10);
          *(int32_t *)(res + 0x48) = *(int32_t *)(bres + 0x14);
          res[0x4c] = bres[0x18];
          *(int32_t *)(res + 0x38) = object_handle;
          res[0x4d] = bres[0x19];
          *(int16_t *)(res + 0x4e) = *(int16_t *)(bres + 0x1a);
          found = 1;
        }
      }

      child = *(int32_t *)(obj + 0xc8);
      if (child != -1 &&
          FUN_0014dce0(child, type_mask, param_3, origin, direction,
                       exclude_handle, collision_result) != 0) {
        found = 1;
      }
    }

    object_handle = *(int32_t *)(obj + 0xc4);
  } while (object_handle != -1);

  return found;
}

/* 0x14e640
 *
 * Casts the BACKWARD half of one ray against a single object's collision bsp
 * and fills the caller's 0x50-byte collision-result record. Structurally the
 * twin of the bsp branch inside FUN_0014dce0 (0x14dce0), so the result field
 * map documented there applies verbatim; the differences are all at the two
 * ends:
 *
 *  - Entry seeds the record as "no hit" (word +0x00 = -1, +0x14 = FLT_MAX)
 *    BEFORE the type guard, so a rejected record still leaves a sane result.
 *  - The walk is run from the far endpoint backwards: the point handed to
 *    FUN_0014cb00 is origin+delta and the direction is -delta, so the hit
 *    parameter comes back measured from the far end. It is converted to a
 *    forward parameter by 1.0f - t (FLD [0x2533c8] = 1.0f, FSUB, at
 *    0x14e6e4-0x14e6f3) before being stored to +0x14.
 *  - The forward parameter is then re-read from +0x14 and used to write the
 *    world-space hit point into +0x18/+0x1c/+0x20.
 *
 * Confirmed (disassembly 0x14e640..0x14e7ca):
 *  - cdecl, 4 stack args at EBP+0x08..+0x14, char return in AL. AL is zeroed
 *    at 0x14e650 (covers the type-guard exit, which jumps past the second
 *    XOR because EBX/EDI are not pushed yet), zeroed again at 0x14e7c3 for
 *    the two call-failure exits, and set to 1 at 0x14e7a0 on success.
 *  - Guard is CMP word [record],3 / JNE - the record type word must be 3.
 *  - The three component sums do NOT share an addend order:
 *      +0x00  FLD [origin]  ; FADD [delta]    -> origin[0] + delta[0]
 *      +0x04  FLD [delta+4] ; FADD [origin+4] -> delta[1]  + origin[1]
 *      +0x08  FLD [delta+8] ; FADD [origin+8] -> delta[2]  + origin[2]
 *    Normalising all three to one form is an FPU-WARN-class change.
 *  - FUN_0014cb00's second argument is the literal 1 (PUSH 1 at 0x14e6ca),
 *    where FUN_0014dce0 forwards its own param_3 into the same slot.
 *  - The plane_negate call at 0x14e721 pushes the SAME pointer twice
 *    (LEA EAX,[ESI+0x24] / PUSH EAX / PUSH EAX) - an in-place negate, not a
 *    decompiler duplicate-argument artifact.
 *  - ADD ESP,8 for the FUN_0014da80 call is deferred to 0x14e79d, interleaved
 *    with the FPU tail; it is not the plane_negate cleanup.
 *  - Ghidra's local_440/local_43c/local_438 labels for the scratch record are
 *    each shifted one field low. The disassembly is authoritative:
 *    [EBP-0x43c] = scratch+0x0c is the plane pointer, [EBP-0x438] =
 *    scratch+0x10 goes to result+0x44, [EBP-0x434] = scratch+0x14 both
 *    selects the negate and goes to result+0x48.
 *
 * Frame (SUB ESP,0x448 = 1096 bytes), in declaration order outward from EBP:
 *   EBP-0x0c  (12)    sum_point, the far endpoint origin+delta
 *   EBP-0x18  (12)    neg_delta, the backward direction
 *   EBP-0x28  (16)    FUN_0014c8e0 context (+0x04 tag data, +0x0c surface base)
 *   EBP-0x448 (1056)  FUN_0014cb00 output record; only the first 0x1c bytes
 *                     are read back, the rest is callee-owned scratch.
 *
 * Uncertain: the 1056-byte size of the scratch record is inferred from the
 * frame arithmetic (0x448 - 0x28), not from a decompiled FUN_0014cb00.
 */
char FUN_0014e640(void *test_record, float *origin, float *delta, void *result)
{
  float sum_point[3];
  float neg_delta[3];
  int32_t bsp_ctx[4];
  int32_t bsp_result[264];
  char *res;
  char *bres;
  int idx;
  float t;

  res = (char *)result;
  bres = (char *)bsp_result;

  *(int16_t *)res = -1;
  *(int32_t *)(res + 0x14) = 0x7f7fffff; /* FLT_MAX bit pattern */

  /* Branch polarity is fixed by the reference: CMP word [record],3 / JNE to
   * the far exit, and JE to the far exit on each call result. Rewriting these
   * as early `if (... != 3) return 0;` returns inverts both branches AND
   * stops MSVC tail-merging the epilogues (132 insns vs 128, 86.9% vs 87.1%),
   * so the nested form below is the one the original was built from. */
  if (*(int16_t *)test_record == 3) {
    sum_point[0] = origin[0] + delta[0];
    sum_point[1] = delta[1] + origin[1];
    sum_point[2] = delta[2] + origin[2];
    neg_delta[0] = -delta[0];
    neg_delta[1] = -delta[1];
    neg_delta[2] = -delta[2];

    if ((char)FUN_0014c8e0(bsp_ctx, *(int32_t *)((char *)test_record + 0x38)) !=
          0 &&
        FUN_0014cb00((int)bsp_ctx, (void *)1, (void *)sum_point,
                     (void *)neg_delta, (int16_t *)bres) != 0) {
      /* MOVSX at 0x14e6dd: the row selector is the signed short at +0x00. */
      idx = *(int16_t *)bres;
      *(float *)(res + 0x14) = 1.0f - *(float *)(bres + 8);
      *(int16_t *)res = 3;
      FUN_0010a1c0((float *)(bsp_ctx[3] + idx * 0x34), *(float **)(bres + 0xc),
                   (float *)(res + 0x24));
      if (*(int32_t *)(bres + 0x14) < 0)
        plane_negate((float *)(res + 0x24), (float *)(res + 0x24));
      *(int16_t *)(res + 0x34) =
        (int16_t)FUN_0014da80(bsp_ctx[1], *(int16_t *)(bres + 0x1a));
      *(int16_t *)(res + 0x3c) = *(int16_t *)(bres + 2);
      *(int32_t *)(res + 0x38) = *(int32_t *)((char *)test_record + 0x38);
      *(int16_t *)(res + 0x3e) = *(int16_t *)bres;
      *(int32_t *)(res + 0x44) = *(int32_t *)(bres + 0x10);
      *(int16_t *)(res + 0x40) = *(int16_t *)(bres + 4);
      *(int32_t *)(res + 0x48) = *(int32_t *)(bres + 0x14);
      res[0x4d] = bres[0x19];
      res[0x4c] = bres[0x18];
      *(int16_t *)(res + 0x4e) = *(int16_t *)(bres + 0x1a);

      t = *(float *)(res + 0x14);
      *(float *)(res + 0x18) = t * delta[0] + origin[0];
      *(float *)(res + 0x1c) = t * delta[1] + origin[1];
      *(float *)(res + 0x20) = t * delta[2] + origin[2];
      return 1;
    }
  }

  return 0;
}

/* 0x14e7d0
 *
 * Casts one ray [point, point+offset_vec] against the STRUCTURE bsp only (the
 * bsp handed back by global_collision_bsp_get) and fills the caller's 0x50-byte
 * collision-result record. The walker at 0x149c60 both reports the nearest hit
 * and accumulates the list of leaves the ray crossed; both are consumed here.
 *
 * FRAME (0x14e7d3: SUB ESP,0x420): the whole frame is ONE contiguous 1056-byte
 * scratch record at EBP-0x420, handed to the walker as its last argument
 * (0x14e7e8: LEA EAX,[EBP-0x420]). Every "local" Ghidra invents for this
 * function is a field inside that record (CLAUDE.md buffer-alias pitfall 5):
 *   +0x00 out distance, +0x04..+0x10 the four dwords copied to result+0x24,
 *   +0x14 -> result+0x44, +0x1a a 16-bit value copied to result+0x34 AND
 *   +0x4e, +0x1c the leaf count, +0x20.. the leaf index array. The record must
 *   stay one object - separate locals do not reproduce the layout and the
 *   callee writes past them.
 *
 * Call at 0x14e80b is cdecl (ADD ESP,0x18 = 6 dwords). Pushed right-to-left:
 * &scratch, 0x7f7fffff, [EBP+0x14], [EBP+0x10], [EBP+0xc], then the getter's
 * EAX - i.e. (bsp, point, offset_vec, p4, FLT_MAX, &scratch). Both float args
 * go out as plain dword pushes (MOV ECX,[EBP+0x14] / PUSH ECX and
 * PUSH 0x7f7fffff), the usual MSVC form for forwarding a float parameter and
 * for a float literal - do not convert either value.
 *
 * `unit_handle` ([EBP+0x18]) is never referenced by the body; it keeps its
 * mechanical name. ESI caches the result pointer from [EBP+0x1c] throughout,
 * and is destroyed (ADD ESI,0xc at 0x14e90c) before the tail call, so the
 * +0x18 pointer is captured first.
 *
 * The +0x24..+0x30 block is copied with integer MOVs (0x14e826..0x14e849), not
 * FLD/FSTP, so it is written here as dword copies through the raw record
 * pointer rather than through collision_test_result's float fields - typing
 * them as floats would emit an x87 copy the original does not have.
 *
 * Both cluster lookups read MOVSX EAX,word ptr [elem+8] - a sign-extending
 * 16-bit load. The -1 path reaches the same store via OR EAX,EAX with EAX
 * already 0xffffffff, which is the same visible value as cluster = -1.
 *
 * result+0x14 is seeded to FLT_MAX at entry, replaced by the walker's distance
 * on a hit, and forced to 1.0f at 0x14e8f7 when the hit flag never got set.
 *
 * Return: MOV AL,BL - the char hit flag.
 */
typedef struct {
  float best_dist; /* 0x00 - out distance written by the walker */
  int32_t field_04; /* 0x04 - copied verbatim to result+0x24 */
  int32_t field_08; /* 0x08 - copied verbatim to result+0x28 */
  int32_t field_0c; /* 0x0c - copied verbatim to result+0x2c */
  int32_t field_10; /* 0x10 - copied verbatim to result+0x30 */
  int32_t field_14; /* 0x14 - copied verbatim to result+0x44 */
  int16_t pad_18; /* 0x18 - never read back */
  int16_t field_1a; /* 0x1a - 16-bit, copied to result+0x34 and +0x4e */
  int32_t count; /* 0x1c - number of leaf indices gathered */
  uint32_t indices[256]; /* 0x20 - leaf indices, [0] first and [count-1] last */
} collision_bsp_test_vector_scratch;

char FUN_0014e7d0(uint32_t collision_flags, float *point, float *offset_vec,
                  float p4, int unit_handle, void *result)
{
  collision_bsp_test_vector_scratch scratch;
  char *out;
  char hit;
  uint32_t index;
  int32_t cluster;
  void *elem;
  float t;

  out = (char *)result;
  hit = 0;

  *(int16_t *)out = -1;
  *(float *)(out + 0x14) = 3.4028235e+38f;

  if (FUN_00149c60((int *)global_collision_bsp_get(), point, offset_vec, p4,
                   3.4028235e+38f, (float *)&scratch)) {
    /* Ghidra fuses these two tests into one comma expression; the distance
     * store at 0x14e81d happens before the 0x20 flag test at 0x14e820 and is
     * not guarded by it. */
    *(float *)(out + 0x14) = scratch.best_dist;
    if ((collision_flags & 0x20) != 0) {
      *(int32_t *)(out + 0x24) = scratch.field_04;
      *(int32_t *)(out + 0x28) = scratch.field_08;
      *(int32_t *)(out + 0x2c) = scratch.field_0c;
      *(int32_t *)(out + 0x30) = scratch.field_10;
      out[0x4c] = 0;
      out[0x4d] = 0;
      *(int16_t *)out = 2;
      *(int16_t *)(out + 0x34) = scratch.field_1a;
      *(int32_t *)(out + 0x44) = scratch.field_14;
      *(int32_t *)(out + 0x48) = -1;
      *(int16_t *)(out + 0x4e) = scratch.field_1a;
      hit = 1;
    }
  }

  if (scratch.count > 0) {
    index = scratch.indices[0];
    *(uint32_t *)(out + 4) = index;
    if (index == 0xffffffff) {
      cluster = -1;
    } else {
      elem = tag_block_get_element((char *)scenario_get() + 0xe0,
                                   index & 0x7fffffff, 0x10);
      cluster = *(int16_t *)((char *)elem + 8);
    }
    *(int16_t *)(out + 8) = (int16_t)cluster;

    index = scratch.indices[scratch.count - 1];
    *(uint32_t *)(out + 0xc) = index;
    if (index == 0xffffffff) {
      cluster = -1;
    } else {
      elem = tag_block_get_element((char *)scenario_get() + 0xe0,
                                   index & 0x7fffffff, 0x10);
      cluster = *(int16_t *)((char *)elem + 8);
    }
    *(int16_t *)(out + 0x10) = (int16_t)cluster;
  }

  if (hit == 0) {
    *(float *)(out + 0x14) = 1.0f;
  }

  /* One FLD of result+0x14 held on the x87 stack and duplicated per component
   * (0x14e8fe: FLD [ESI+0x14] / FLD ST(0) ...), multiply by offset_vec first
   * then add point - do not reorder into point + t * offset_vec. */
  t = *(float *)(out + 0x14);
  *(float *)(out + 0x18) = t * offset_vec[0] + point[0];
  *(float *)(out + 0x1c) = t * offset_vec[1] + point[1];
  *(float *)(out + 0x20) = t * offset_vec[2] + point[2];

  scenario_location_from_point(out + 0xc, out + 0x18);

  return hit;
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

/* 0x14ea10 — Walk the object sibling chain starting at first_handle (next at
 * [obj+0xC4]) and, for every object inside the search sphere, add its
 * collision features to the caller's feature buffer (param_8). Recurses into
 * each object's child chain ([obj+0xC8]).
 *
 * Confirmed (disassembly 0x14ea10..0x14ec0f):
 *  - cdecl, 8 stack args at EBP+0x08..+0x24; SUB ESP,0x60 + PUSH EBX/ESI/EDI.
 *    EBX = param_8, EDI = current handle (live across the whole loop),
 *    ESI = object pointer from 0x13d680.
 *  - Reject filter, in order: handle == exclude_handle; [obj+0x04] bit0;
 *    [obj+0x04] bit24; ([obj+0xB6] & 4) together with a zero type word at
 *    [obj+0x64].
 *  - Sphere reject at 0x14ea62: FLD [obj+0x5C]; FADD radius gives r, then
 *    d = obj_center([obj+0x50..0x58]) - origin (object minus origin — the
 *    FSUB operand order is object first). The FCOMPP / TEST AH,1 branch
 *    rejects when d.y*d.y + d.z*d.z + d.x*d.x > r*r; the accumulation order
 *    is dy, dz, dx (FLD ST2/FMUL ST3 chain) — do not reassociate.
 *  - Dispatch at 0x14eaa5: MOVSX EAX,word[obj+0x64]; type_mask & (1 <<
 * (type+8)) gates a real MSVC switch over types 0..8 (index byte table at
 * 0x14EC1C feeding a jump table at 0x14EC10). Only case 0 and cases 1/6/7/8 do
 *    work; 2..5 fall to the default at 0x14ebcd.
 *  - The child recursion at 0x14ebf2 (8 args, ADD ESP,0x20) sits inside the
 *    sphere test but OUTSIDE the type_mask gate, and passes the CHILD handle
 *    [obj+0xC8] — not the sibling [obj+0xC4] that drives the do/while.
 *  - FUN_0014c8e0 returns its result in AL only; EDI (the handle) is never
 *    reloaded from it. Ghidra's `iVar9 = FUN_0014c8e0(...)` is register
 *    aliasing and would corrupt the sibling walk.
 *
 * Frame (SUB ESP,0x60), derived from the EBP displacements:
 *   EBP-0x60 (60)  FUN_001509c0 model context
 *   EBP-0x24 (16)  FUN_0014c8e0 collision-bsp context
 *   EBP-0x14 (12)  biped camera position (vector3)
 *   EBP-0x08 (4)   biped camera height
 *   EBP-0x04 (4)   shift temp for (1 << type)  [Ghidra: `local_c[1] = 1.4e-45`
 *                  is really `MOV dword ptr [EBP-4],1`, not a float denormal]
 * MSVC reused the dead `first_handle` parameter slot (EBP+0x0C) as the float
 * `height_offset` out-parameter of biped_get_camera_height_and_offset; that
 * reuse is reproduced here so the frame stays 0x60 bytes.
 *
 * Note: collision_features_from_point's param_3 is declared `int` in kb.json
 * but the original stores it with FSTP [ESP+4] — it is a float dword. The
 * value is forwarded bit-exact (never through a numeric cast). Likewise
 * FUN_0014cde0/FUN_00150790 params 4 and 5 are raw float dwords. */
void FUN_0014ea10(unsigned int type_mask, int first_handle, float *origin,
                  float radius, float param_5, float param_6,
                  int exclude_handle, int param_8)
{
  char *obj;
  int cur;
  int child;
  int type;
  float r;
  float dx;
  float dy;
  float dz;
  float camera_height;
  float camera_pos[3];
  int bsp_ctx[4];
  int model_ctx[15];

  cur = first_handle;
  do {
    obj = (char *)object_get_and_verify_type(cur, -1);

    if (cur != exclude_handle && (*(unsigned int *)(obj + 4) & 1) == 0 &&
        (*(unsigned int *)(obj + 4) & 0x1000000) == 0 &&
        ((*(unsigned char *)(obj + 0xb6) & 4) == 0 ||
         *(short *)(obj + 0x64) != 0)) {
      r = *(float *)(obj + 0x5c) + radius;
      dx = *(float *)(obj + 0x50) - origin[0];
      dy = *(float *)(obj + 0x54) - origin[1];
      dz = *(float *)(obj + 0x58) - origin[2];

      if (dx * dx + dz * dz + dy * dy <= r * r) {
        type = (int)*(short *)(obj + 0x64);

        if ((type_mask & (1u << (type + 8))) != 0) {
          switch (type) {
          case 0:
            if (((type_mask & 0x200000) == 0 ||
                 (*(unsigned char *)(obj + 0x424) & 0x10) == 0) &&
                (*(int *)(obj + 0xcc) == -1 || *(short *)(obj + 0x2a0) == -1)) {
              /* &first_handle is the reused EBP+0x0C slot: the handle has
               * already been copied into `cur`, so MSVC repurposed it as
               * the float height_offset out-parameter. */
              biped_get_camera_height_and_offset(cur, (vector3_t *)camera_pos,
                                                 (float *)&first_handle,
                                                 &camera_height);
              camera_pos[2] = camera_pos[2] + *(float *)&first_handle;
              camera_height = camera_height + param_6;
              collision_features_from_point(
                (int)camera_pos, *(float *)&first_handle + param_5,
                *(int *)&camera_height, cur, -1, 0, 0xff, -1, (void *)param_8);
            }
            break;
          case 1:
          case 6:
          case 7:
          case 8:
            if (((1 << type) & 2) != 0 && (type_mask & 0x400000) != 0) {
              if (FUN_001509c0(model_ctx, cur) != 0) {
                FUN_00150790((int)model_ctx, (int)origin, radius,
                             *(int *)&param_5, *(int *)&param_6, param_8);
              }
            } else {
              if ((char)FUN_0014c8e0(bsp_ctx, cur) != 0) {
                FUN_0014cde0((int)bsp_ctx, (int)origin, radius,
                             *(int *)&param_5, *(int *)&param_6, param_8);
              }
            }
            break;
          }
        }

        child = *(int *)(obj + 0xc8);
        if (child != -1) {
          FUN_0014ea10(type_mask, child, origin, radius, param_5, param_6,
                       exclude_handle, param_8);
        }
      }
    }

    cur = *(int *)(obj + 0xc4);
  } while (cur != -1);
}
