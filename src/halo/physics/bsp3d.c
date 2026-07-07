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
    node = (uint32_t *)tag_block_get_element(bsp2d_nodes, (int)node_index_u,
                                             0x14);
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
