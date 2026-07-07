/* FUN_00146d40 (0x146d40): descend a 2D BSP node tree from `node_index` to the
 * leaf containing the 2D `point2d` (x, y).
 *
 * Unlike the 3D variant, the plane lives inline in the node record: a single
 * tag_block with stride 0x14 (5 dwords) holds [0]=plane.x, [1]=plane.y,
 * [2]=plane.d and the two child links at [3] (back / dist<0 side) and [4]
 * (front / dist>=0 side). The plane fields are read as floats and the child
 * links as integers off the same element pointer (matching the fld/fmul then
 * `mov [ecx+eax*4+0xc]` in the disassembly).
 *
 * At each interior node compute the signed 2D plane distance
 * plane.y*y + plane.x*x - plane.d (the y term is emitted first to match the
 * original's x87 scheduling); take child [4] when it is >= 0.0f, else child
 * [3]. Keep descending while the child link is a non-negative interior index;
 * when it goes negative we stop. A link of 0xffffffff means solid/no-leaf and
 * is returned unchanged; any other negative link is a leaf index returned with
 * its high sign bit stripped. The result is the integer leaf index in EAX (the
 * original does `and ecx,0x7fffffff; mov eax,ecx; ret`), not a float.
 */
uint32_t FUN_00146d40(void *bsp2d_nodes, float *point2d, int node_index)
{
  uint32_t *node;
  float *plane;
  float *p = point2d;
  uint32_t node_index_u = (uint32_t)node_index;
  float dist;

  while ((int)node_index_u >= 0) {
    node =
      (uint32_t *)tag_block_get_element(bsp2d_nodes, (int)node_index_u, 0x14);
    plane = (float *)node;
    dist = (plane[1] * p[1] + plane[0] * p[0]) - plane[2];
    node_index_u = node[3 + (0.0f <= dist ? 1 : 0)];
  }

  if (node_index_u != 0xffffffff) {
    return node_index_u & 0x7fffffff;
  }
  return 0xffffffff;
}

/* bsp3d_find_leaf (0x146db0): descend the BSP3D node tree from `root` to the
 * leaf containing `point`.
 *
 * Two tag_blocks live in the bsp3d structure: the nodes block at base+0
 * (stride 0xc = 3 dwords: [0]=plane index, [1]=back child, [2]=front child)
 * and the planes block at base+0xc (stride 0x10 = 4 floats: [0..2]=normal xyz,
 * [3]=plane distance d).
 *
 * At each node we compute the signed distance dot(normal, point) - d. If it is
 * >= 0.0f we take the front child (index 2), otherwise the back child (index
 * 1). The multiply-add is emitted in x,z,y source order to match the original's
 * x87 scheduling. We keep descending while the child link is a non-negative
 * (interior-node) index; when it goes negative we stop. A link of 0xffffffff
 * means solid/no-leaf; any other negative link is a leaf index with its high
 * sign bit stripped.
 */
uint32_t bsp3d_find_leaf(void *bsp3d, int root, void *point)
{
  uint32_t *node;
  float *plane;
  float *p = (float *)point;
  uint32_t node_index = (uint32_t)root;
  float dist;

  do {
    node = (uint32_t *)tag_block_get_element(bsp3d, (int)node_index, 0xc);
    plane =
      (float *)tag_block_get_element((char *)bsp3d + 0xc, (int)node[0], 0x10);
    dist = (plane[0] * p[0] + plane[2] * p[2] + plane[1] * p[1]) - plane[3];
    node_index = node[1 + (0.0f <= dist ? 1 : 0)];
  } while ((int)node_index >= 0);

  if (node_index != 0xffffffff) {
    return node_index & 0x7fffffff;
  }
  return 0xffffffff;
}

/* bsp3d_clip_line_to_leaves (0x146e30): recursively clip the line segment
 * [p0, p1] against the BSP3D node tree rooted at `node_index` and invoke
 * `callback` once per terminal leaf the segment passes through, returning the
 * number of leaves visited (accumulated across the recursion).
 *
 * Node record layout (nodes block, stride 0xc = 3 dwords): [0]=plane index,
 * [1]=back child link, [2]=front child link. Plane record (planes block at
 * base+0xc, stride 0x10 = 4 floats): [0..2]=normal xyz, [3]=plane distance d.
 *
 * Each endpoint is classified against a symmetric epsilon band: below
 * *(float*)0x29ca2c (negative epsilon) => on the back side, above
 * *(float*)0x29ca28 (positive epsilon) => on the front side, otherwise within
 * the band (on the plane). The signed distance dot(n, point) - d is emitted in
 * z,x,y source order to match the original's x87 scheduling.
 *
 * When the segment straddles the plane (one endpoint strictly front, the other
 * strictly back) the split point is computed via
 * t = -((p0.n) - d) / (dir.n) where dir = p1 - p0; the original asserts
 * 0 < t < 1 and bails via system_exit(-1) otherwise. The negation is kept in
 * the FCHS form -(num/den) rather than distributed.
 *
 * The two children are iterated in order back (link[1]) then front (link[2]).
 * A side is descended only when at least one endpoint lies on that side. The
 * sub-segment handed to a child replaces any endpoint lying strictly on the
 * OPPOSITE side (index alt = 1-side) with the split point. A child link that is
 * negative is terminal: 0xffffffff means solid/no-leaf (skipped), any other
 * negative value is a leaf index (high sign bit stripped) reported via the
 * callback; a non-negative link is an interior node index recursed into.
 *
 * DAT_005a8d20 is a global node-visit counter, reset when entering at the root
 * (node_index == 0) and incremented on every call.
 */
int bsp3d_clip_line_to_leaves(void *nodes, int node_index, float *p0, float *p1,
                              void (*callback)(float *, float *, unsigned int,
                                               void *),
                              void *data)
{
  int count;
  uint32_t *node;
  float *plane;
  float d0, d1, t;
  float dir[3];
  float split[3];
  char in0[2];
  char in1[2];
  uint32_t *link;
  uint32_t child;
  int side;
  int alt;
  float *sp0;
  float *sp1;

  count = 0;
  node = (uint32_t *)tag_block_get_element(nodes, node_index, 0xc);
  plane =
    (float *)tag_block_get_element((char *)nodes + 0xc, (int)node[0], 0x10);

  d0 = (plane[2] * p0[2] + plane[0] * p0[0] + plane[1] * p0[1]) - plane[3];
  d1 = (plane[2] * p1[2] + plane[0] * p1[0] + plane[1] * p1[1]) - plane[3];

  if (node_index == 0) {
    *(int *)0x005a8d20 = 0;
  }
  (*(int *)0x005a8d20)++;

  in0[0] = (char)(d0 < *(float *)0x0029ca2c);
  in0[1] = (char)(*(float *)0x0029ca28 < d0);
  in1[0] = (char)(d1 < *(float *)0x0029ca2c);
  in1[1] = (char)(*(float *)0x0029ca28 < d1);

  if ((in0[0] && in1[1]) || (in0[1] && in1[0])) {
    dir[0] = p1[0] - p0[0];
    dir[1] = p1[1] - p0[1];
    dir[2] = p1[2] - p0[2];
    t =
      -(((plane[2] * p0[2] + plane[0] * p0[0] + plane[1] * p0[1]) - plane[3]) /
        (plane[2] * dir[2] + plane[0] * dir[0] + plane[1] * dir[1]));
    if (t <= 0.0f || 1.0f <= t) {
      display_assert("t>0.f && t<1.f", "c:\\halo\\SOURCE\\physics\\bsp3d.c",
                     0x49, 1);
      system_exit(-1);
    }
    split[0] = dir[0] * t + p0[0];
    split[1] = dir[1] * t + p0[1];
    split[2] = dir[2] * t + p0[2];
  }

  link = node;
  for (side = 0; side < 2; side++) {
    link++;
    alt = (side == 0) ? 1 : 0;
    if (in0[side] || in1[side]) {
      sp0 = in0[alt] ? split : p0;
      sp1 = in1[alt] ? split : p1;
      child = *link;
      if ((int)child < 0) {
        if (child != 0xffffffff) {
          if (callback != NULL) {
            callback(sp0, sp1, child & 0x7fffffff, data);
          }
          count++;
        }
      } else {
        count += bsp3d_clip_line_to_leaves(nodes, (int)child, sp0, sp1,
                                           callback, data);
      }
    }
  }

  return count;
}
